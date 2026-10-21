"""Boss rush fights played step by step by a policy (round 19, the speedrun video): every few frames the policy reads the
pilot's status (the sprites near Mario: id, dx, dy, state) and picks the next moves. A fight that costs Mario life is
tried again from its start (a checkpoint) with the next variation (a seed that changes the policy's timings).
Usage: python -I tas_policy.py <session> [--tries N] [--single BOWSER] [--debug]
(--single: one fight in the session's current room instead of the boss rush's fights; --from N: only boss rush fight N,
from the session's checkpoint fightNN)
"""
import contextlib, io, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tas  # noqa: E402

MAIN = __name__ == '__main__'  # (tas_backtrack.py imports the policies)
name = sys.argv[1] if MAIN else None
tries = int(sys.argv[sys.argv.index('--tries') + 1]) if '--tries' in sys.argv else 60
ROOMS = ['IGGY', 'BIG BOO', 'MORTON', 'REZNOR', 'LEMMY', 'LUDWIG', 'ROY', 'WENDY', 'LARRY', 'BOWSER']


DEBUG = '--debug' in sys.argv  # one try of the current fight, a shot every frame; on a hit the last 60 frames
HIST = []


def step(moves):
    s = tas.load(name)
    if not tas.alive(s):
        tas.launch(name, s)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        tas.send(name, s, moves, 2 if DEBUG else 0)
    m = re.search(r'STATUS (\{.*\})', buf.getvalue())
    st = json.loads(m.group(1)) if m else {}
    if DEBUG:
        out = os.path.join(tas.sdir(name), 'out')
        HIST.append((moves, st, sorted(f for f in os.listdir(out) if f.startswith('s%03d_' % s['step']) and
                                      f.endswith('.bmp'))))
        del HIST[:-30]
    return st


def debug_dump():
    from PIL import Image
    out = os.path.join(tas.sdir(name), 'out')
    shots = [f for _, _, fs in HIST for f in fs][-60:]
    sheet = Image.new('RGB', (256 * 10, 224 * ((len(shots) + 9) // 10)))
    for i, f in enumerate(shots):
        sheet.paste(Image.open(os.path.join(out, f)).convert('RGB'), ((i % 10) * 256, (i // 10) * 224))
    p = os.path.join(tas.sdir(name), 'hit_sheet.png')
    sheet.save(p)
    print('HIT SHEET ' + p)
    for mv, st, _ in HIST[-8:]:
        print(mv, json.dumps({k: st.get(k) for k in ('x', 'y', 'mc_life', 'hero_frame', 'sprites_near', 'ext_near')}))


def fight_no(st):
    m = re.match(r'BOSS (\d+)/', st.get('rush', ''))
    return int(m.group(1)) if m else 0


def checkpoint(cp):
    s = tas.load(name)
    s['checkpoints'][cp] = len(s['moves'])
    tas.save(name, s)


def rewind(cp):
    s = tas.load(name)
    n = s['checkpoints'][cp]
    tas.kill(s)
    s['moves'] = s['moves'][:n]
    s['checkpoints'] = {k: v for k, v in s['checkpoints'].items() if v <= n}
    tas.launch(name, s)
    with contextlib.redirect_stdout(io.StringIO()):
        tas.send(name, s, [])


def sprites(st, ids):
    return [p for p in st.get('sprites_near', []) if p['id'] in ids and p['st'] >= 8]


# --- policies: (status, step number, seed) -> moves ---------------------------------------------------------------

def punch_rounds(st, n, seed):
    every = [3, 2, 4, 5, 1][seed % 5]
    air = ' air' if n % every == every - 1 else ''
    return ['attack boss 40%s' % air, 'wait 4']


def big_boo(st, n, seed):
    boss = sprites(st, {'C5'})
    boos = sorted(sprites(st, {'37'}), key=lambda p: abs(p['dx']) + abs(p['dy']))
    reach = 40 + (seed % 4) * 8
    if boss:
        b = boss[0]
        if b['c2'] == 2 and abs(b['dx'] + 8) < reach and -90 < b['dy'] < 10:
            return ['attack C5 %d air %d' % (reach, 2 + seed % 5), 'wait 2']
    if boos:
        near = boos[0]
        d = 'RIGHT' if near['dx'] > 0 else 'LEFT'
        away = 'LEFT' if d == 'RIGHT' else 'RIGHT'
        if abs(near['dx']) < 36 and abs(near['dy']) < 40:
            return ['hold %s' % away, 'wait 6', 'hold %s' % d, 'wait 1', 'hold', 'wait 1']  # step away, face it again
        return ['hold %s' % d, 'wait 1', 'hold', 'wait 3']  # face the nearest Boo: it stops
    return ['wait 4']


def koopa_kid(st, n, seed):
    """Morton / Roy / Ludwig: stand facing him and punch as he walks into the fist's reach; jump away when he gets too
    close; keep out from under him while he is up on a wall or the ceiling."""
    for p in sprites(st, {'34'}):  # Ludwig's fireballs, flying level at Mario: jump them as they come
        if -40 < p['dy'] < 32 and p['xs'] * p['dx'] < 0 and abs(p['dx']) < 30 + (seed % 4) * 6 and st.get('on_ground'):
            return ['hold B', 'wait %d' % (12 + seed % 3 * 2), 'hold', 'wait 2']
    kids = sprites(st, {'29'})
    if not kids:  # (Ludwig waits off to the right until Mario comes)
        return ['hold RIGHT', 'wait 4', 'hold', 'wait 1']
    k = kids[0]
    dx, dy = k['dx'], k['dy']
    toward = 'RIGHT' if dx > 0 else 'LEFT'
    away = 'LEFT' if toward == 'RIGHT' else 'RIGHT'
    ad = abs(dx)
    lo, hi = 22 + (seed % 3) * 2, 44 + (seed % 4) * 3
    if abs(dy) < 20:
        if ad < lo:
            return ['hold B %s' % away, 'wait 14', 'hold %s' % away, 'wait 8', 'hold', 'wait 2']
        if ad <= hi:
            if st.get('hero_frame') != 1:
                return ['hold', 'wait 1']
            return ['hold Y', 'wait 2', 'hold', 'wait 3']
        return ['hold %s' % toward, 'wait 1', 'hold', 'wait 2']
    if ad < 40:  # he is above: step out from under him
        return ['hold %s' % away, 'wait 4', 'hold', 'wait 1']
    return ['hold %s' % toward, 'wait 1', 'hold', 'wait 2']


REZNOR_ROOM = [False]


def reznor_floor(st):
    """Reznor's bridge (Spr0A9_Reznor_BreakReznorBridge): after the n-th break the tiles at x = 176 - 16i and its mirror
    368 - x (i < n) are gone; the next pair counts as gone too (it breaks within the next 64 frames)."""
    n = st.get('reznor_broken', 0) + 1
    gone = set()
    for i in range(min(n, 12)):
        x = 0xB0 - 16 * i
        gone |= {x, 368 - x}
    l1x = st.get('x', 0) - st.get('screen_x', 0)
    return ''.join('0' if ((l1x + t * 16) // 16 * 16) in gone else '1' for t in range(16))


def move(st, d, frames):
    """Walk `d` (LEFT / RIGHT) for `frames`, but not into a hole in the floor row (status `floor`: 16 tiles across the
    screen): jump it when there is floor beyond, else stay."""
    # (only Reznor's bridge needs it: elsewhere the status floor row can miss solid tiles of other kinds, as the roof
    # of Bowser's castle, and would hold Mario still)
    fl = reznor_floor(st) if REZNOR_ROOM[0] else ''
    if len(fl) == 16 and st.get('on_ground'):
        step_dx = 1 if d == 'RIGHT' else -1
        mid = st.get('screen_x', 128) + 8
        ahead = (mid + step_dx * 14) // 16
        beyond = (mid + step_dx * 40) // 16
        if 0 <= ahead < 16 and fl[ahead] == '0':
            if 0 <= beyond < 16 and fl[beyond] == '1':
                return ['hold B %s' % d, 'wait 16', 'hold %s' % d, 'wait 4', 'hold', 'wait 1']
            return ['hold', 'wait 2']
    return ['hold %s' % d, 'wait %d' % frames, 'hold', 'wait 1']


def dodge(st, radius=22, horizon=40):
    """Moves that get Mario out of the way of an extended sprite (fireball...) whose straight path passes within
    `radius` px of his middle in the next `horizon` frames, or None."""
    for f in st.get('ext_near', []):
        px, py = f['dx'] + 4 - 8, f['dy'] + 4 - 20  # its middle relative to his
        vx, vy = f['xs'] / 16.0, f['ys'] / 16.0
        vv = vx * vx + vy * vy
        if vv < 0.01:
            continue
        t = -(px * vx + py * vy) / vv
        if not 0 <= t <= horizon:
            continue
        mx, my = px + vx * t, py + vy * t
        if (mx * mx + my * my) ** 0.5 > radius:
            continue
        if abs(vy) < 0.4:  # flying level at him: jump over it
            return ['hold B', 'wait 14', 'hold', 'wait 2']
        # coming down at him: run away from where it reaches his height
        lx = px + vx * (-py / vy) if vy > 0 and py < 0 else mx
        run = 'RIGHT' if lx < 0 else 'LEFT'
        return move(st, run, 6)
    return None


def reznor(st, n, seed):
    fl = reznor_floor(st)
    cur = (st.get('screen_x', 128) + 8) // 16
    if st.get('on_ground') and 0 <= cur < 16 and fl[cur] == '0':  # standing where the bridge goes: to safe floor
        safe = [i for i in range(16) if fl[i] == '1']
        if safe:
            to = min(safe, key=lambda i: abs(i - cur))
            return move(st, 'RIGHT' if to > cur else 'LEFT', 6)
    d = dodge(st, 20 + (seed % 3) * 4)
    if d:
        return d
    live = [p for p in sprites(st, {'A9'}) if p['t151c'] == 0]
    if not live:
        return ['wait 4']
    # one level with him (his platform at the floor): a punch from the ground is SMW's hit from below; touching him
    # from the side only bounces Mario back, so keep a fist's length away
    for p in sorted(live, key=lambda p: abs(p['dx'])):
        if -30 < p['dy'] < 24 and abs(p['dx']) < 46:
            toward = 'RIGHT' if p['dx'] > 0 else 'LEFT'
            away = 'LEFT' if toward == 'RIGHT' else 'RIGHT'
            if abs(p['dx']) < 16:
                return move(st, away, 4)
            if st.get('hero_frame') != 1:
                return ['hold', 'wait 1']
            facing = st.get('facing_right')
            pre = [] if facing is None or facing == (toward == 'RIGHT') else ['hold %s' % toward, 'wait 1', 'hold', 'wait 1']
            return pre + ['hold Y', 'wait 2', 'hold', 'wait 3']
    t = max(live, key=lambda p: p['dy'])  # the lowest one
    if -86 < t['dy'] < -20 and abs(t['dx']) < 10 + (seed % 3) * 3 and st.get('on_ground'):
        k = 6 + seed % 5
        return ['hold B', 'wait %d' % k, 'hold B Y', 'wait 2', 'hold B', 'wait 10', 'hold', 'wait 2']
    # wait for them at a spot left of the wheel's bottom, which breaks late: each comes down into the fist's reach
    home = 104 + (seed % 3) * 8
    if abs(st.get('x', 0) - home) > 5:
        return move(st, 'RIGHT' if st.get('x', 0) < home else 'LEFT', 3)
    if st.get('facing_right') is False:
        return ['hold RIGHT', 'wait 1', 'hold', 'wait 1']
    return ['wait 2']


def bowser(st, n, seed):
    """Bowser in his clown car: jump kicks late in the rise (the car flies above Mario's head); out of the way of what
    he drops and throws."""
    d = dodge(st, 22)
    if d:
        return d
    cars = sprites(st, {'A0'})
    if not cars:
        return ['wait 4']
    car = cars[0]
    dx, dy = car['dx'], car['dy']
    toward = 'RIGHT' if dx > 0 else 'LEFT'
    away = 'LEFT' if toward == 'RIGHT' else 'RIGHT'
    # a big ball dropping on him: out from under it
    for p in sprites(st, {'A1'}):
        if -110 < p['dy'] < -10 and abs(p['dx']) < 36:
            return ['hold %s' % ('LEFT' if p['dx'] > 0 else 'RIGHT'), 'wait 8', 'hold', 'wait 1']
    # what walks or rolls at him on the roof: a Mechakoopa is punched over, a big ball jumped (a knocked-out
    # Mechakoopa, status 9, is left alone: punching it again only keeps it down)
    for p in [q for q in sprites(st, {'A2', 'A1'}) if q['id'] == 'A1' or q['st'] == 8]:
        if abs(p['dy']) < 40 and abs(p['dx']) < (40 if p['id'] == 'A2' else 56):
            if p['id'] == 'A1' or abs(p['dx']) < 14:
                return ['hold B %s' % ('RIGHT' if p['dx'] < 0 else 'LEFT'), 'wait 14', 'hold', 'wait 4']
            if st.get('hero_frame') == 1:
                face = 'RIGHT' if p['dx'] > 0 else 'LEFT'
                return ['hold %s' % face, 'wait 1', 'hold Y', 'wait 2', 'hold', 'wait 3']
            return ['hold', 'wait 1']
    if not 7 <= car['t151c'] <= 9:  # not fighting yet (or between phases): keep away from the car
        if abs(dx) < 72:
            return move(st, away, 4)
        return ['wait 4']
    lo, hi = 34 + seed % 3 * 2, 48
    if abs(dx) < lo:
        return move(st, away, 3)
    if abs(dx) > hi:
        return move(st, toward, 3)
    if st.get('on_ground') and -150 < dy < -40:
        k = max(4, min(16, int((-dy - 40) / 7) + seed % 3))  # later for a higher car
        return ['hold B %s' % toward, 'wait 1', 'hold B', 'wait %d' % k, 'hold B Y', 'wait 2', 'hold B', 'wait 8',
                'hold', 'wait 2']
    return ['wait 2']


POLICY = {'BIG BOO': big_boo, 'MORTON': koopa_kid, 'ROY': koopa_kid, 'LUDWIG': koopa_kid, 'REZNOR': reznor,
          'BOWSER': bowser}

SINGLE = sys.argv[sys.argv.index('--single') + 1].upper().replace('_', ' ') if '--single' in sys.argv else None


def fight_over(st, fight):
    if SINGLE:  # one fight outside the boss rush: Bowser rising to die, or the level is left
        return st.get('game_mode') != '14' or any(p['id'] == 'A0' and 4 <= p['t151c'] <= 6
                                                  for p in st.get('sprites_near', []))
    return fight_no(st) != fight or not st.get('in_level')


FROM = int(sys.argv[sys.argv.index('--from') + 1]) if '--from' in sys.argv else 0  # this boss rush fight only


def main():
    st = step(['wait 1'])
    fight = 1 if SINGLE else (FROM or fight_no(st))
    while 0 < fight <= 10:
        room = SINGLE or ROOMS[fight - 1]
        pol = POLICY.get(room, punch_rounds)
        REZNOR_ROOM[0] = room == 'REZNOR'
        cp = 'fight%02d' % fight
        if not FROM:
            checkpoint(cp)  # (with --from the checkpoint fightNN must exist: the fight's start)
        st0 = st
        results = []  # (hits, fight frames, seed) of the tries that won

        def play(t, st):
            """One try with seed t from the fight's start: (won, hits, frames, status)."""
            life, hits, f0 = st.get('mc_life', 10), 0, st.get('frame', 0)
            st = step(['wait %d' % (1 + (t * 5) % 30)])
            for n in range(3000):
                st = step(pol(st, n, t))
                if st.get('player_state') == 9:
                    if DEBUG:
                        debug_dump()
                        sys.exit(0)
                    return False, hits, 0, st
                if st.get('mc_life', 10) < life - 0.01:
                    hits += 1
                    if DEBUG:
                        debug_dump()
                        sys.exit(0)
                life = st.get('mc_life', 10)
                if fight_over(st, fight):
                    return True, hits, st.get('frame', 0) - f0, st
            return False, hits, 0, st

        won = False
        for t in range(tries):
            ok, hits, frames, st = play(t, st0)
            print('fight %d %s try %d: %s, %d hits, %d frames (%s)' % (fight, room, t + 1, 'won' if ok else 'lost', hits,
                                                                         frames, st.get('rush')), flush=True)
            if ok and hits == 0:
                won = True
                break
            if ok:
                results.append((hits, frames, t))
            rewind(cp)
            st = step(['wait 1'])
        if not won and results:
            hits, frames, t = min(results)
            ok, hits, frames, st = play(t, st)  # the best try again (the same moves: the same frames)
            print('fight %d %s: best try %d replayed: %s, %d hits, %d frames' % (fight, room, t + 1, ok, hits, frames),
                  flush=True)
            won = ok
        if not won:
            print('fight %d not won in %d tries' % (fight, tries))
            break
        if SINGLE or FROM:
            break
        for _ in range(80):
            if fight_no(st) != fight and st.get('game_mode') == '14':
                break
            if not st.get('rush'):
                break
            st = step(['wait 10'])
        fight = fight_no(st)
    print('done: %s' % st.get('rush'))


if MAIN:
    main()
