"""A fight played by a policy with backtracking (round 19, the speedrun videos): from a tas.py checkpoint the policy
(tas_policy.py's, or the pipe fights' here) picks the next moves from the pilot's status, step by step. A step that
costs Mario life (or his life) goes back a few steps (more after each failure in a row) and plays on with other
random timings; after --patience failures in a row a hit is let through (never one that leaves him a hit from death).
The fight is won when the boss rush HUD moves on (or the level is left), or, with --bowser, when Bowser rises to die;
the winning moves are kept in the session (checkpoint won_<checkpoint>).
Seeded: the same arguments play the same fight.
Usage: python -I tas_backtrack.py <session> <checkpoint> <policy> [--seed S] [--patience N] [--bowser]
  policies: pipe_kid (Lemmy, Wendy), koopa_kid, big_boo, reznor, bowser, punch_rounds
"""
import contextlib, io, json, os, random, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tas  # noqa: E402
import tas_policy as tp  # noqa: E402

name, cp, pol_name = sys.argv[1], sys.argv[2], sys.argv[3]
arg = lambda k, d: int(sys.argv[sys.argv.index(k) + 1]) if k in sys.argv else d
seed, patience = arg('--seed', 1), arg('--patience', 40)
BOWSER = '--bowser' in sys.argv
MCL = '--mcl' in sys.argv  # an area of the Mario Combat level (policy mc_runner): won when the next area starts
STALL = arg('--stall', 600)  # frames with no boss punch landed before going back
WATCH = arg('--watch', 0)  # play WATCH steps of the policy alone (no backtracking), a shot every 3 frames: a sheet
rng = random.Random(seed)
tp.name = name

# --- the pipe rooms (Lemmy, Wendy): seven pipes across a fixed screen ----------------------------------------------
PIPES = [(16, 303), (48, 319), (80, 319), (112, 303), (144, 287), (176, 303), (208, 319)]  # (left x, top y), 32 wide
if '--wendy' in sys.argv:  # Wendy's pipes are all one height
    PIPES = [(16 + 32 * i, 319) for i in range(7)]
REAL = {}  # the real Koopa Kid's slot: the one kid in the room before the first round


def pipe_top(x):
    for px, top in PIPES:
        if px <= x < px + 32:
            return top
    return 0  # a wall


def ball_path(b, mx, my, frames):
    """Where a reflecting fireball (1 px a frame on both axes) will be: bounces off the side walls, the ceiling and the
    pipe tops. Positions in level pixels (its top left)."""
    x, y = mx + b['dx'], my + b['dy']
    vx, vy = (1 if b['xs'] > 0 else -1 if b['xs'] < 0 else 0), (1 if b['ys'] > 0 else -1 if b['ys'] < 0 else 0)
    out = []
    for _ in range(frames):
        nx, ny = x + vx, y + vy
        if nx < 16 or nx + 16 > 240:
            vx, nx = -vx, x - vx
        if ny < 192 + 24 or ny + 16 > pipe_top(nx + 8):
            vy, ny = -vy, y - vy
        x, y = nx, ny
        out.append((x, y))
    return out


def threat(st, mx, my, frames=40, pad=3):
    """The first frame a fireball overlaps Mario's box (16 x 24 at his feet) if he stood at (mx, my), else None."""
    first = None
    for b in tp.sprites(st, {'B6'}):
        for t, (x, y) in enumerate(ball_path(b, st['x'], st['y'], frames)):
            if x < mx + 16 + pad and mx < x + 16 + pad and y < my + 32 + pad and my + 8 < y + 16 + pad:
                first = t if first is None else min(first, t)
                break
    return first


def walk_to(st, tx):
    x = st['x']
    d = 'RIGHT' if tx > x else 'LEFT'
    ahead = pipe_top(x + 8 + (20 if d == 'RIGHT' else -20))
    if st.get('on_ground') and ahead and ahead < pipe_top(x + 8) - 2:  # a taller pipe ahead: hop up
        return ['hold B %s' % d, 'wait 10', 'hold %s' % d, 'wait 4', 'hold', 'wait 1']
    return ['hold %s' % d, 'wait %d' % max(1, min(6, abs(tx - x) // 2)), 'hold', 'wait 1']


def pipe_kid(st, n, s):
    x, y = st['x'], st['y']
    kids = [p for p in st.get('sprites_near', []) if p['id'] == '29' and p['st'] == 8]
    if kids and not REAL:
        REAL['slot'] = kids[0]['slot'] if len(kids) == 1 else None
    t = threat(st, x, y)
    if t is not None and t < 26:
        # out of the way: the spot nearby with no fireball coming, or a jump
        opts = []
        for dxs in (-24, 24, -40, 40, -12, 12):
            if pipe_top(x + 8 + dxs) and abs(pipe_top(x + 8 + dxs) - pipe_top(x + 8)) <= 16:
                tt = threat(st, x + dxs, pipe_top(x + 8 + dxs) - 32)
                opts.append((tt is None, tt or 99, -abs(dxs), dxs))
        opts.sort(reverse=True)
        if opts and opts[0][0]:
            return walk_to(st, x + opts[0][3])
        return ['hold B', 'wait %d' % rng.choice([6, 10, 14]), 'hold', 'wait 2']
    real = [k for k in kids if k['slot'] == REAL.get('slot')] or kids
    if not real:
        return ['wait 2']
    k = real[0]
    kx = x + k['dx']
    out = k['dy'] < 32  # risen out of his pipe (he sits at about dy 11-27 from a pipe beside his)
    side = -1 if x < kx else 1
    spot = kx + side * (26 + s % 3 * 4)
    if not pipe_top(spot + 8):
        spot = kx - side * (26 + s % 3 * 4)
    if out and 14 <= abs(k['dx']) <= 44 and abs(pipe_top(x + 8) - pipe_top(kx + 8)) <= 16 and st.get('on_ground'):
        toward = 'RIGHT' if k['dx'] > 0 else 'LEFT'
        if st.get('hero_frame') != 1:
            return ['hold', 'wait 1']
        pre = [] if st.get('facing_right') == (toward == 'RIGHT') else ['hold %s' % toward, 'wait 1', 'hold', 'wait 1']
        return pre + ['hold Y', 'wait 2', 'hold', 'wait %d' % (2 + s % 3)]
    if abs(x - spot) > 5:
        return walk_to(st, spot)
    return ['wait 2']


def ludwig(st, n, s):
    """Ludwig breathes fireballs level at Mario: jump them, kicking at him when the jump takes Mario to him; punch
    from the ground between fireballs; jump his spinning shell."""
    kids = tp.sprites(st, {'29'})
    if not kids:
        return ['hold RIGHT', 'wait 4', 'hold', 'wait 1']  # he waits off to the right until Mario comes
    k = kids[0]
    toward = 'RIGHT' if k['dx'] > 0 else 'LEFT'
    ad = abs(k['dx'])
    ground = st.get('on_ground')
    balls = [p for p in tp.sprites(st, {'34'}) if -40 < p['dy'] < 32 and p['xs'] * p['dx'] < 0]
    near = min((abs(p['dx']) for p in balls), default=999)
    if ground and near < 20 + (s % 5) * 6:
        if ad < 40 + (s % 5) * 8:
            k_ = 1 + s % 8
            return ['hold B %s' % toward, 'wait %d' % k_, 'hold B Y %s' % toward, 'wait 2', 'hold B', 'wait 8',
                    'hold', 'wait 2']
        return ['hold B', 'wait %d' % (12 + s % 3 * 2), 'hold', 'wait 2']
    if abs(k['dy']) < 24 and ad < 18 and ground:  # too close (or his shell coming through): jump away over him
        return ['hold B %s' % ('LEFT' if toward == 'RIGHT' else 'RIGHT'), 'wait 14', 'hold', 'wait 2']
    if ad <= 40 + s % 3 * 4 and abs(k['dy']) < 24 and ground and near > 40:
        if st.get('hero_frame') != 1:
            return ['hold', 'wait 1']
        pre = [] if st.get('facing_right') == (toward == 'RIGHT') else ['hold %s' % toward, 'wait 1', 'hold', 'wait 1']
        return pre + ['hold Y', 'wait 2', 'hold', 'wait 3']
    if ad > 36:
        return ['hold %s' % toward, 'wait 2', 'hold', 'wait 1']
    return ['wait 2']


def ludwig_stomp(st, n, s):
    """Ludwig from above: jump onto his head (SMW's stomp) from a little way off, between his fireballs."""
    kids = tp.sprites(st, {'29'})
    if not kids:
        return ['hold RIGHT', 'wait 4', 'hold', 'wait 1']
    k = kids[0]
    toward = 'RIGHT' if k['dx'] > 0 else 'LEFT'
    ad = abs(k['dx'])
    if not st.get('on_ground'):  # drift over him and drop (no jump button: no wall jumps off the windows)
        return ['hold %s' % toward if ad > 4 + s % 3 * 2 else 'hold', 'wait 2']
    balls = [p for p in tp.sprites(st, {'34'}) if -40 < p['dy'] < 32 and p['xs'] * p['dx'] < 0]
    near = min((abs(p['dx']) for p in balls), default=999)
    if ad < 56 + (s % 4) * 8 or near < 24 + (s % 3) * 6:
        return ['hold B %s' % toward, 'wait %d' % (6 + s % 6), 'hold %s' % toward, 'wait 2']
    return ['hold %s' % toward, 'wait 2', 'hold', 'wait 1']


# --- the Mario Combat level (round 20): waypoints per area (level px: x, feet y), read off mc_level_export.py's
# previews; the runner heads for the next one, jumps walls and gaps, wall-jumps up shafts, punches Koopas
MC_WAYPOINTS = {
    1: [(330, 272), (544, 256), (656, 288), (784, 256), (920, 304), (1272, 304)],
    2: [(288, 400), (340, 176), (580, 176), (632, 240), (760, 368), (1180, 368), (1208, 336), (1224, 304), (1270, 288),
        (1208, 256), (1176, 240), (1150, 240), (1230, 80), (1288, 80)],
    # (area 3 starts 2 columns earlier since round 20's art fixes: x + 32 against the first runs)
    3: [(252, 320), (360, 336), (488, 320), (592, 272), (812, 272), (1132, 272), (1372, 224), (1440, 176)],
    4: [(300, 400), (470, 320), (718, 304)],
    5: [],
}


MC_GRID = None
MC_PREV = {}  # the runner's last x in the air (no x progress against a wall: clinging)


def mc_snap(area, x, y):
    """A waypoint's feet y: the top of the ground in its column nearest to the given y (mc_level_data.c.json)."""
    global MC_GRID
    if MC_GRID is None:
        p = os.path.join(tas.ROOT, 'games', 'SuperMarioWorld', 'src', 'mc', 'mc_level_data.c.json')
        MC_GRID = {a['area']: a['solid'] for a in json.load(open(p))}
    g = MC_GRID.get(area)
    if not g:
        return y
    c = max(0, min(len(g[0]) - 1, x // 16))
    tops = [r * 16 for r in range(1, len(g)) if g[r][c] == '1' and g[r - 1][c] != '1']
    return min(tops, key=lambda t: abs(t - y)) if tops else y


def mc_wall_behind(area, x, fy, side):
    """A wall within 5 tiles on that side of Mario, at his middle's height or up to 3 rows above it (a shaft to kick back`n    and forth in; SMW's wall jump never grabs the wall it last kicked off)."""
    mc_snap(area, x, fy)  # (loads the grid)
    g = MC_GRID.get(area)
    if not g:
        return False
    r = max(0, min(len(g) - 1, (fy - 16) // 16))
    c0 = (x + 8) // 16
    for k in range(1, 6):
        c = c0 - k if side == 'LEFT' else c0 + k
        if 0 <= c < len(g[0]) and any(g[rr][c] == '1' for rr in range(max(0, r - 3), r + 1)):
            return True
    return False


def mc_runner(st, n, s):
    moves = mc_runner_moves(st, n, s)
    # SMW Mario runs with X held (and jumps further running), but walks to near targets level or below (area 3's
    # small pillars over the lava); a running hop across a gap down to one (_run)
    if not st.get('mc_mario') and (st.get('_dist', 999) > 96 or st.get('_up') or st.get('_run')):
        moves = [('hold X ' + m[5:]) if m.startswith('hold ') and ('LEFT' in m or 'RIGHT' in m) else m for m in moves]
    return moves


def mc_runner_moves(st, n, s):
    m = st.get('mcl') or {}
    area = m.get('area', 0)
    x, fy = st.get('x', 0), st.get('y', 0) + 32
    ground = st.get('on_ground') or st.get('on_solid_sprite')
    mc = st.get('mc_mario')
    if m.get('prompt'):
        return ['hold UP', 'wait 2', 'hold', 'wait 4']
    if area == 4 and m.get('lift', 0) in (1, 2):  # riding the lift: stand still
        return ['hold', 'wait 6']
    if area == 5 and not mc:  # SMW Mario vs MC Bowser: spin jumps onto him (a strong stomp), steered over him
        bw = [p for p in st.get('sprites_near', []) if p['id'] == '04' and p['st'] == 8]
        if bw:
            f = min(bw, key=lambda p: abs(p['dx']))
            toward = 'RIGHT' if f['dx'] > 0 else 'LEFT'
            if not ground:
                return ['hold A %s' % toward if abs(f['dx']) > 4 + s % 3 * 2 else 'hold A', 'wait 2']
            if abs(f['dx']) < 48 + s % 4 * 10:
                return ['hold A %s' % toward, 'wait %d' % (2 + s % 3)]
            return ['hold %s' % toward, 'wait %d' % (2 + s % 3)]
    # an enemy (an MC Koopa: SMW sprite 04; Bowser too) within reach: face it and punch (SMW Mario: jump on it)
    foes = [p for p in st.get('sprites_near', []) if p['id'] == '04' and p['st'] == 8 and -48 < p['dy'] < 32]
    if foes:
        f = min(foes, key=lambda p: abs(p['dx']))
        toward = 'RIGHT' if f['dx'] > 0 else 'LEFT'
        reach = 40 + s % 3 * 6
        if abs(f['dx']) < reach:
            if not mc:
                return ['hold B %s' % toward, 'wait %d' % (8 + s % 6), 'hold %s' % toward, 'wait 6', 'hold', 'wait 2']
            if abs(f['dx']) < 12 and ground:
                away = 'LEFT' if toward == 'RIGHT' else 'RIGHT'
                return ['hold %s' % away, 'wait 4', 'hold', 'wait 1']
            pre = [] if st.get('facing_right') == (toward == 'RIGHT') else ['hold %s' % toward, 'wait 1', 'hold', 'wait 1']
            return pre + ['hold Y', 'wait 1', 'hold', 'wait %d' % (3 + s % 4)]
        if area == 5:  # Bowser's bridge: go to him
            return ['hold %s' % toward, 'wait 3', 'hold', 'wait 1']
    wps = MC_WAYPOINTS.get(area, [])
    i = st.get('_wp', 0)
    while i < len(wps) and abs(x + 8 - wps[i][0]) < 16 and abs(fy - mc_snap(area, *wps[i])) < 10 and ground:
        i += 1  # reached: the next one
    st['_wp_next'] = i
    if i >= len(wps):
        return ['hold', 'wait 4']
    wx, wy = wps[i]
    wy = mc_snap(area, wx, wy)
    d = 'RIGHT' if wx > x + 8 else 'LEFT'
    away = 'LEFT' if d == 'RIGHT' else 'RIGHT'
    blocked = st.get('blocked', 0) & (1 if d == 'RIGHT' else 2)
    up = wy < fy - 8
    dist = abs(wx - x - 8)
    st['_dist'], st['_up'] = dist, up
    if not ground:
        # the target well above, or a little above with a wall behind Mario (a shaft): up by wall jumps
        climb = fy - wy > 40 or (fy - wy > 4 and mc_wall_behind(area, x, fy, away))
        if climb:
            # up a shaft: from wall to wall. Against the wall it heads for (no x progress, or blocked): a wall jump
            # (a fresh press: let go of jump first) away from it, then on toward the other wall.
            h = MC_PREV.get('hold', d)
            other = 'LEFT' if h == 'RIGHT' else 'RIGHT'
            if not mc_wall_behind(area, x, fy, h) and mc_wall_behind(area, x, fy, other):
                h, other = other, h  # (no wall that way at this height: out of the shaft, as over area 2's lava)
                MC_PREV['hold'] = h
            rising = st.get('y', 0) < MC_PREV.get('y', -1)
            MC_PREV['y'] = st.get('y', 0)
            if MC_PREV.get('x') == x or st.get('hero_frame') == 17 or st.get('blocked', 0) & (1 if h == 'RIGHT' else 2):
                if rising and not mc:  # (SMW's wall jump grabs the wall only while falling: keep pushing into it)
                    MC_PREV['x'] = x
                    return ['hold %s' % h, 'wait 2']
                MC_PREV['x'], MC_PREV['hold'] = None, other
                return ['hold %s' % h, 'wait 1', 'hold B %s' % h, 'wait 2', 'hold B %s' % other, 'wait %d' % (6 + s % 6)]
            MC_PREV['x'] = x
            return ['hold B %s' % h, 'wait 2']
        MC_PREV['x'], MC_PREV['hold'] = x, d
        return ['hold B %s' % d if up else 'hold %s' % d, 'wait 3']
    stuck = MC_PREV.get('gx') == x and MC_PREV.get('gwalk')  # walked last step and didn't move: a wall (MC's own)
    MC_PREV['gx'], MC_PREV['gwalk'] = x, False
    if blocked or stuck or (up and dist < 72 + s % 3 * 16):
        if dist < 40 and fy - wy <= 40:  # a step close by: a short hop
            return ['hold B %s' % d, 'wait %d' % (5 + s % 5), 'hold %s' % d, 'wait %d' % (4 + s % 6), 'hold', 'wait 4']
        return ['hold B %s' % d, 'wait %d' % (10 + s % 8), 'hold B %s' % d, 'wait 6', 'hold %s' % d, 'wait 4']
    if dist < 6:
        if wy > fy + 8:  # right over it, higher up: walk on and drop
            return ['hold RIGHT', 'wait 16', 'hold', 'wait 2']
        return ['hold', 'wait 2']
    # a gap ahead in the floor row (status `floor`: the row under Mario's feet): jump it
    fl = st.get('floor', '')
    col = (st.get('screen_x', 0) + 8) // 16 + (1 if d == 'RIGHT' else -1)
    if len(fl) == 16 and 0 <= col < 16 and fl[col] == '0' and wy <= fy + 8:
        return ['hold B %s' % d, 'wait %d' % (10 + s % 8), 'hold %s' % d, 'wait 6']
    if not mc and len(fl) == 16 and 0 <= col < 16 and fl[col] == '0' and dist > 40:
        # SMW Mario, the target lower down past a gap (area 2's pit before the floor): a running hop across
        st['_run'] = True
        return ['hold B %s' % d, 'wait %d' % (3 + s % 3), 'hold %s' % d, 'wait 8']
    MC_PREV['gwalk'] = True
    return ['hold %s' % d, 'wait %d' % (2 + s % 4)]


POLICIES = {'pipe_kid': pipe_kid, 'ludwig': ludwig, 'ludwig_stomp': ludwig_stomp, 'mc_runner': mc_runner, 'koopa_kid': tp.koopa_kid, 'big_boo': tp.big_boo, 'reznor': tp.reznor,
            'bowser': tp.bowser, 'punch_rounds': tp.punch_rounds}
policy = POLICIES[pol_name]
tp.REZNOR_ROOM[0] = pol_name == 'reznor'


def rewind_to(idx):
    s = tas.load(name)
    tas.kill(s)
    s['moves'] = s['moves'][:idx]
    s['checkpoints'] = {k: v for k, v in s['checkpoints'].items() if v <= idx}
    tas.launch(name, s)
    with contextlib.redirect_stdout(io.StringIO()):
        tas.send(name, s, [])
    return json.load(open(os.path.join(tas.sdir(name), 'out', 'status.json')))


def do(moves):
    s = tas.load(name)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        tas.send(name, s, moves)
    m = re.search(r'STATUS (\{.*\})', buf.getvalue())
    return json.loads(m.group(1)) if m else {}


def moves_len():
    return len(tas.load(name)['moves'])


def progress(st):  # what the stall rule waits for: a boss punch, or the MC level runner's next waypoint
    return st.get('boss_punches', 0) * 100 + st.get('_wp', 0)


def over(st, st0):
    if MCL:  # the Mario Combat level: the next area, or Bowser down
        m, m0 = st.get('mcl') or {}, st0.get('mcl') or {}
        # (not just out of the level: a death's restart of the area fades out too)
        return m.get('area', 0) > m0.get('area', 0) or m.get('won', 0) > 0
    if BOWSER:
        return st.get('game_mode') != '14' or any(p['id'] == 'A0' and 4 <= p['t151c'] <= 6
                                                  for p in st.get('sprites_near', []))
    return tp.fight_no(st) != tp.fight_no(st0) or not st.get('in_level')


st0 = rewind_to(tas.load(name)['checkpoints'][cp])
for _ in range(100):  # (a fight's checkpoint can come while the room is still being entered)
    if st0.get('in_level') and st0.get('game_mode') == '14':
        break
    st0 = do(['wait 4'])
st0['_wp'] = arg('--wp', 0)  # (the MC level runner's first waypoint)
st = st0
if WATCH:
    from PIL import Image
    shots = []
    for n in range(WATCH):
        mv = policy(st, n, seed)
        s_ = tas.load(name)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            tas.send(name, s_, mv, 3)
        old = st
        st = json.loads(re.search(r'STATUS (\{.*\})', buf.getvalue()).group(1))
        st['_wp'] = old.get('_wp_next', old.get('_wp', 0))
        o = os.path.join(tas.sdir(name), 'out')
        shots += sorted(os.path.join(o, f) for f in os.listdir(o) if f.startswith('s%03d_' % s_['step']) and f.endswith('.bmp'))
        print(n, mv, st.get('mc_life'), st.get('boss_punches'), st.get('_wp'), st.get('x'), st.get('y'), st.get('on_ground'),
              [(p['id'], p['dx'], p['dy'], p['c2']) for p in st.get('sprites_near', [])])
    shots = shots[-80:]
    sheet = Image.new('RGB', (256 * 10, 224 * ((len(shots) + 9) // 10)))
    for i, f in enumerate(shots):
        sheet.paste(Image.open(f).convert('RGB'), ((i % 10) * 256, (i // 10) * 224))
    sheet.save(os.path.join(tas.sdir(name), 'watch.png'))
    sys.exit(0)
hist = [(moves_len(), st)]  # after each kept step: (moves so far, status)
fails, hits, fail_frame, total_fails, stalls = 0, 0, 0, 0, 0
for n in range(20000):
    s = (seed * 7919 + n * 31 + fails * 104729) % 1000
    riding = MCL and (st.get('mcl') or {}).get('lift') in (1, 2, 5)  # (no random moves on the MC level's lift ride)
    if fails and not riding and rng.random() < min(0.5, 0.08 * fails):
        d = rng.choice(['LEFT', 'RIGHT'])
        moves = rng.choice([['hold %s' % d, 'wait %d' % rng.choice([2, 4, 8])], ['wait %d' % rng.choice([2, 4, 8])],
                            ['hold B %s' % d, 'wait 8', 'hold', 'wait 4']])
    else:
        moves = policy(st, n, s)
    new = do(moves)
    # (lives since the last step too: 1-UPs taken on the way hide a death from the start's count)
    died = new.get('player_state') == 9 or new.get('lives', 9) < min(st0.get('lives', 9), st.get('lives', 9))
    hurt = new.get('mc_life', 10) < st.get('mc_life', 10) - 0.01 or (
        not new.get('mc_mario') and new.get('power_up', 0) < st.get('power_up', 0))  # (SMW Mario: a power-down)
    if died or (hurt and (fails < patience or new.get('mc_life', 10) <= 2.6)):
        fails += 1
        total_fails += 1
        fail_frame = max(fail_frame, new.get('frame', 0))
        back = min(len(hist) - 1, 3 + fails // 3)
        del hist[len(hist) - back:]
        wp = hist[-1][1].get('_wp', 0)
        st = rewind_to(hist[-1][0])
        st['_wp'] = wp
        hist[-1] = (moves_len(), st)
        continue
    new['_wp'] = st.get('_wp_next', st.get('_wp', 0))
    pn = progress(new)
    since = next((i for i in range(len(hist)) if progress(hist[i][1]) == pn), len(hist) - 1)
    if not over(new, st0) and new.get('frame', 0) - hist[since][1].get('frame', 0) > STALL:
        # no punch landed for too long: from a random step since the last one, with other timings
        fails += 1
        total_fails += 1
        stalls += 1
        keep = rng.randrange(since, len(hist))
        del hist[keep + 1:]
        wp = hist[-1][1].get('_wp', 0)
        st = rewind_to(hist[-1][0])
        st['_wp'] = wp
        hist[-1] = (moves_len(), st)
        continue
    if hurt:
        hits += 1
        print('step %d: HIT let through after %d failures (life %s)' % (n, fails, new.get('mc_life')), flush=True)
    if new.get('frame', 0) > fail_frame + 120:
        fails = 0  # well past where it failed
    st = new
    hist.append((moves_len(), st))
    if n % 20 == 0:
        print('step %d: frame %s life %s punches %s wp %s x %s y %s %s (failures %d, stalls %d)' % (
            n, st.get('frame'), st.get('mc_life'), st.get('boss_punches'), st.get('_wp'), st.get('x'), st.get('y'), st.get('rush'),
                                                                        total_fails, stalls), flush=True)
    if over(st, st0):
        s_ = tas.load(name)
        s_['checkpoints']['won_' + cp] = len(s_['moves'])
        tas.save(name, s_)
        print('WON with %d hits, %d failures (checkpoint won_%s) %s' % (hits, total_fails, cp, st.get('rush')), flush=True)
        tas.kill(s_)  # (the game: a caller waiting for this process tree goes on)
        tas.save(name, s_)
        sys.exit(0)
print('out of steps')
s_ = tas.load(name)
tas.kill(s_)
tas.save(name, s_)
sys.exit(1)
