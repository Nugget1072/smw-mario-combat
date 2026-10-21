"""Level select test, part 2: starting a level in awkward moments, and what the game does afterwards.
Each scenario is one pilot run (state 9 = in Yoshi's Island 1, state 8 = in Yoshi's House) with checks on the pilot's
`whereami` / `progress` / `peek` / `levelselect` lines. Prints PASS / FAIL per scenario with the reason.
Usage: python -I levelselect_scenarios.py <root> [scenario names...]
Writes work/pilot/levelselect_scen/<name>/ (script, stdout, shots).
"""
import os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
only = sys.argv[2:]
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'levelselect_scen')
TO_MAP = ['kill', 'wait 400']  # from YI1 (state 9): lose a life, back on the map, Mario standing on YI1's tile

W = re.compile(r'whereami: mode (\w+) level (\w+) sublevels (\d+) session (\d) map (\d+) pos (\d+),(\d+) ow_level (\w+) '
               r'state (\d+) lives (\d+) power (\d+) yoshi (\d+) time (\d+) x (-?\d+)')
KEYS = ['mode', 'level', 'sub', 'session', 'map', 'px', 'py', 'ow', 'state', 'lives', 'power', 'yoshi', 'time', 'x']


def parse(out):
    ev = []
    for line in out.splitlines():
        m = W.search(line)
        if m:
            ev.append(('w', dict(zip(KEYS, m.groups()))))
        elif line.startswith('progress:'):
            ev.append(('p', line.split()[1]))
        elif line.startswith('levelselect:'):
            ev.append(('ls', line[len('levelselect:'):].split('(frame')[0].strip()))
        elif line.startswith('peek '):
            ev.append(('peek', line.split(':', 1)[1].split()))
    return ev


def get(ev, kind):
    return [v for k, v in ev if k == kind]


SCEN = {}


def scen(name, lines):
    def deco(check):
        SCEN[name] = (lines, check)
        return check
    return deco


def expect(cond, msg, fails):
    if not cond:
        fails.append(msg)


@scen('dying', ['loadstate 9', 'wait 5', 'whereami', 'kill', 'wait 10', 'whereami', 'levelselect 106', 'wait 240',
                'whereami', 'shot s1', 'hold RIGHT', 'wait 60', 'hold', 'whereami', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[1]['state'] == '9', 'Mario was not dying when the level was chosen (state %s)' % w[1]['state'], f)
    expect(w[2]['mode'] == '14' and w[2]['level'] == '106', 'YI2 not started: %s' % w[2], f)
    expect(w[2]['state'] == '0', 'Mario still in state %s in the new level' % w[2]['state'], f)
    expect(w[2]['lives'] == w[0]['lives'], 'a life was taken (%s -> %s)' % (w[0]['lives'], w[2]['lives']), f)
    expect(int(w[3]['x']) > int(w[2]['x']), 'Mario does not move in the new level', f)


@scen('goal_walk', ['loadstate 9', 'wait 5'] + TO_MAP + ['whereami', 'progress', 'levelselect 105', 'wait 240',
                    'glide 4805 200', 'wait 20', 'hold RIGHT', 'wait 40', 'hold', 'wait 20', 'peek 1493', 'whereami',
                    'levelselect 106', 'wait 240', 'whereami', 'peek 1493', 'shot s1', 'kill', 'wait 450', 'whereami',
                    'progress', 'shot s2', 'quit'])
def _(ev, f):
    w, p, pk = get(ev, 'w'), get(ev, 'p'), get(ev, 'peek')
    expect(int(pk[0][0], 16) > 0, 'the goal walk had not started (timer_end_level 0)', f)
    expect(w[2]['mode'] == '14' and w[2]['level'] == '106', 'YI2 not started: %s' % w[2], f)
    expect(int(pk[1][0], 16) == 0, 'the goal walk carried over into YI2', f)
    expect(w[3]['mode'] == '0E' and p[1] == p[0], 'map not as before (mode %s, %s -> %s)' % (w[3]['mode'], p[0], p[1]), f)

@scen('message_box', ['loadstate 8', 'wait 10', 'hold B', 'wait 8', 'hold', 'wait 50', 'peek 1426', 'shot s0',
                      'levelselect 105', 'wait 240', 'whereami', 'peek 1426', 'shot s1', 'hold RIGHT', 'wait 60',
                      'hold', 'whereami', 'quit'])
def _(ev, f):
    w, pk = get(ev, 'w'), get(ev, 'peek')
    expect(int(pk[0][0], 16) != 0, 'no message box was open', f)
    expect(w[0]['mode'] == '14' and w[0]['level'] == '105', 'YI1 not started: %s' % w[0], f)
    expect(int(pk[1][0], 16) == 0, 'the message box is still open in YI1', f)
    expect(int(w[1]['x']) > int(w[0]['x']), 'Mario does not move', f)


@scen('riding_yoshi', ['loadstate 9', 'wait 5', 'yoshi', 'wait 30', 'whereami', 'levelselect 106', 'wait 240',
                       'whereami', 'shot s1', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['yoshi'] != '0', 'Mario was not on Yoshi', f)
    expect(w[1]['level'] == '106' and w[1]['yoshi'] != '0', 'Yoshi did not come along: %s' % w[1], f)


@scen('star_pswitch', ['loadstate 9', 'wait 5', 'poke 1490 FF', 'poke 14AD FF', 'wait 5', 'levelselect 106', 'wait 240',
                       'whereami', 'peek 1490', 'peek 14AD', 'shot s1', 'quit'])
def _(ev, f):
    w, pk = get(ev, 'w'), get(ev, 'peek')
    expect(w[0]['level'] == '106', 'YI2 not started', f)
    expect(int(pk[0][0], 16) == 0 and int(pk[1][0], 16) == 0, 'star / P-switch timers carried over: %s %s' % (pk[0], pk[1]), f)


@scen('in_a_room', ['loadstate 9', 'wait 5', 'warp 03', 'wait 150', 'glide 1752 120', 'wait 40', 'hold DOWN', 'wait 30',
                    'hold', 'wait 120', 'whereami', 'levelselect 105', 'wait 240', 'whereami', 'shot s1', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(int(w[0]['sub']) >= 2, 'not in a room (sublevels %s)' % w[0]['sub'], f)
    expect(w[1]['level'] == '105' and w[1]['sub'] == '0' and int(w[1]['x']) < 64,
           'YI1 not started from its beginning: %s' % w[1], f)


@scen('room_bank_1', ['loadstate 9', 'wait 5'] + TO_MAP + ['levelselect 103', 'wait 240', 'glide 1752 120', 'wait 40',
                                                           'hold DOWN', 'wait 30', 'hold', 'wait 120', 'whereami',
                                                           'shot s1', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(int(w[0]['sub']) >= 1 and int(w[0]['level'], 16) >= 0x100, "YI3's pipe room is not a 1xx room: %s" % w[0], f)


@scen('room_bank_0', ['loadstate 9', 'wait 5'] + TO_MAP + ['levelselect 001', 'wait 240', 'glide %d 200' % (17 * 256 + 100),
                                                           'wait 30', 'takeexit', 'wait 200', 'whereami', 'shot s1',
                                                           'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['level'] == '0D8', "Vanilla Secret 2's exit (screen 17 decimal -> D8) led to %s, not 0D8 (bank 0)" % w[0]['level'], f)


@scen('pause', ['loadstate 9', 'wait 5', 'hold START', 'wait 2', 'hold', 'wait 10', 'peek 13D4', 'levelselect 106',
                'wait 240', 'whereami', 'peek 13D4', 'hold RIGHT', 'wait 60', 'hold', 'whereami', 'shot s1', 'quit'])
def _(ev, f):
    w, pk = get(ev, 'w'), get(ev, 'peek')
    expect(int(pk[0][0], 16) != 0, 'the game was not paused', f)
    expect(w[0]['level'] == '106' and int(pk[1][0], 16) == 0, 'YI2 not started or still paused', f)
    expect(int(w[1]['x']) > int(w[0]['x']), 'Mario does not move', f)


@scen('title_screen', ['wait 120', 'levelselect 105', 'wait 60', 'whereami', 'quit'])
def _(ev, f):
    ls, w = get(ev, 'ls'), get(ev, 'w')
    expect('START OR LOAD A GAME FIRST' in ls[0], 'not refused on the title screen: %s' % ls[0], f)
    expect(w[0]['session'] == '0' and w[0]['mode'] != '14', 'something started: %s' % w[0], f)


@scen('map_walking', ['loadstate 9', 'wait 5'] + TO_MAP + ['hold RIGHT', 'wait 4', 'peek 13D9', 'levelselect 105', 'hold',
                                                           'wait 240', 'whereami', 'quit'])
def _(ev, f):
    ls, w, pk = get(ev, 'ls'), get(ev, 'w'), get(ev, 'peek')
    walking = int(pk[0][0], 16) != 3
    if walking:
        expect('WAIT UNTIL MARIO STANDS' in ls[0], 'not refused while Mario walks: %s' % ls[0], f)
        expect(w[0]['mode'] == '0E' and w[0]['session'] == '0', 'something started: %s' % w[0], f)
    else:
        expect(w[0]['level'] == '105', 'Mario was standing but YI1 did not start', f)


@scen('chain_goal', ['loadstate 9', 'wait 5'] + TO_MAP + ['whereami', 'progress', 'levelselect 106', 'wait 240',
                     'levelselect 015', 'wait 240', 'levelselect 105', 'wait 240', 'whereami', 'glide 4805 200',
                     'wait 20', 'hold RIGHT', 'wait 40', 'hold', 'wait 900', 'whereami', 'progress', 'peek 1EA2 96',
                     'shot s1', 'quit'])
def _(ev, f):
    w, p = get(ev, 'w'), get(ev, 'p')
    expect(w[1]['level'] == '105', 'third level not YI1', f)
    expect(w[2]['mode'] == '0E', 'not back on the map after the goal (mode %s)' % w[2]['mode'], f)
    expect(p[1] == p[0], 'the goal changed the save buffer (%s -> %s)' % (p[0], p[1]), f)
    expect((w[2]['map'], w[2]['px'], w[2]['py']) == (w[0]['map'], w[0]['px'], w[0]['py']), 'map position moved', f)


@scen('midway', ['loadstate 9', 'wait 5'] + TO_MAP + ['poke 1ECB C0', 'progress', 'levelselect 105', 'wait 240',
                 'whereami', 'poke 13CE 01', 'kill', 'wait 450', 'whereami', 'progress', 'peek 1ECB', 'quit'])
def _(ev, f):
    w, p, pk = get(ev, 'w'), get(ev, 'p'), get(ev, 'peek')
    expect(w[0]['level'] == '105' and int(w[0]['x']) < 64, 'YI1 did not start at its beginning: %s' % w[0], f)
    expect(w[1]['mode'] == '0E' and p[1] == p[0], 'map not as before after a midway death (%s -> %s)' % (p[0], p[1]), f)


@scen('game_over', ['loadstate 9', 'wait 5'] + TO_MAP + ['poke DB4 00', 'levelselect 106', 'wait 240', 'whereami', 'kill',
                    'wait 1200', 'whereami', 'shot s1', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['level'] == '106', 'YI2 not started', f)
    expect(w[1]['session'] == '0', 'practice session still open after the game over (mode %s)' % w[1]['mode'], f)


@scen('smw_mario_from_level', ['loadstate 9', 'wait 5', 'toggle', 'wait 20', 'levelselect 106', 'wait 240', 'status',
                               'whereami', 'hold RIGHT', 'wait 60', 'hold', 'whereami', 'shot s1', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['level'] == '106' and int(w[1]['x']) > int(w[0]['x']), 'YI2 not started / Mario stuck', f)


@scen('cheats_triple_yoshi', ['loadstate 9', 'wait 5', 'cheat 9 1', 'cheat 12 1'] + TO_MAP +
      ['levelselect 106', 'wait 240', 'whereami', 'shot s1', 'cheat 9 0', 'cheat 12 0', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['level'] == '106' and w[0]['yoshi'] != '0', 'YI2 with Always Yoshi: %s' % w[0], f)


@scen('same_level', ['loadstate 9', 'wait 5', 'glide 1500 200', 'wait 30', 'whereami', 'levelselect 105', 'wait 240',
                     'whereami', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(int(w[0]['x']) > 1000 and w[1]['level'] == '105' and int(w[1]['x']) < 64,
           'YI1 did not restart from its beginning: %s -> %s' % (w[0]['x'], w[1]['x']), f)


@scen('bonus_game_to_map', ['loadstate 9', 'wait 5'] + TO_MAP + ['whereami', 'progress', 'levelselect 000', 'wait 240',
                            'whereami'] + ['poke %X 01' % (0xC2 + k) for k in range(1, 10)] +
      ['wait 1500', 'whereami', 'progress', 'shot s1', 'quit'])
def _(ev, f):
    w, p = get(ev, 'w'), get(ev, 'p')
    expect(w[1]['level'] == '000', 'the bonus game did not start: %s' % w[1], f)
    expect(w[2]['mode'] == '0E', 'the bonus game did not end on the map (mode %s)' % w[2]['mode'], f)
    if w[2]['mode'] == '0E':
        expect(p[1] == p[0] and w[2]['session'] == '0', 'map not as before after the bonus game', f)


@scen('wings_to_map', ['loadstate 9', 'wait 5'] + TO_MAP + ['whereami', 'progress', 'levelselect 0C8', 'wait 240',
                       'whereami', 'hold RIGHT', 'wait 1800', 'hold', 'wait 600', 'whereami', 'progress', 'shot s1',
                       'quit'])
def _(ev, f):
    w, p = get(ev, 'w'), get(ev, 'p')
    expect(w[1]['level'] == '0C8', 'the wings area did not start: %s' % w[1], f)
    expect(w[2]['mode'] == '0E', 'the wings area did not end on the map (mode %s)' % w[2]['mode'], f)
    if w[2]['mode'] == '0E':
        expect(p[1] == p[0] and w[2]['session'] == '0', 'map not as before after the wings area', f)


@scen('menu_driven', ['loadstate 9', 'wait 10', 'menu', 'wait 2'] + ['menukey Down'] * 6 +
      # (the hub: MARIO COMBAT, BOSS RUSH, then the groups: Down three times is DONUT PLAINS)
      ['menukey Return', 'wait 2', 'shot m0_hub', 'menukey Down', 'menukey Down', 'menukey Down', 'menukey Return',
       'wait 2',
       'shot m1_donut',
       'menukey Down', 'menukey Down', 'menukey Return', 'wait 240', 'whereami', 'shot m2_started', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['mode'] == '14' and w[0]['level'] == '013', 'Donut Secret House not started from the menu: %s' % w[0], f)


@scen('menu_title_refused', ['wait 120', 'menu', 'wait 2'] + ['menukey Down'] * 6 +
      ['menukey Return', 'menukey Down', 'menukey Down', 'menukey Return', 'menukey Down', 'menukey Return', 'wait 5',
       'shot m0_notice', 'menukey Escape', 'menukey Escape', 'menukey Escape', 'wait 30', 'whereami', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['session'] == '0' and w[0]['mode'] != '14', 'a level started from the title: %s' % w[0], f)


# (round 20) MARIO COMBAT on the title screen: refused the same way
@scen('mclevel_title_refused', ['wait 120', 'menu', 'wait 2'] + ['menukey Down'] * 6 +
      ['menukey Return', 'menukey Return', 'wait 5', 'shot m0_notice', 'menukey Escape', 'menukey Escape',
       'menukey Escape', 'wait 30', 'whereami', 'mclevelinfo', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['session'] == '0' and w[0]['mode'] != '14', 'the MC level started from the title: %s' % w[0], f)


# (round 16) BOSS RUSH > START on the title screen: refused the same way
@scen('rush_title_refused', ['wait 120', 'menu', 'wait 2'] + ['menukey Down'] * 6 +
      ['menukey Return', 'menukey Down', 'menukey Return', 'menukey Return', 'wait 5', 'shot m0_notice',
       'menukey Escape', 'menukey Escape', 'menukey Escape', 'wait 30', 'whereami', 'rushinfo', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['session'] == '0' and w[0]['mode'] != '14', 'a boss rush started from the title: %s' % w[0], f)


@scen('during_fade', ['loadstate 9', 'wait 5', 'warp 106', 'wait 2', 'peek 100', 'levelselect 105', 'wait 240',
                      'whereami', 'quit'])
def _(ev, f):
    ls, w, pk = get(ev, 'ls'), get(ev, 'w'), get(ev, 'peek')
    expect(int(pk[0][0], 16) != 0x14, 'not in a fade when choosing (mode %s)' % pk[0][0], f)
    expect('WAIT A MOMENT' in ls[0], 'not refused during the fade: %s' % ls[0], f)
    expect(w[0]['mode'] == '14' and w[0]['level'] == '106' and w[0]['session'] == '0',
           'the warp did not finish normally: %s' % w[0], f)


@scen('start_select_exit', ['loadstate 9', 'wait 5'] + TO_MAP + ['poke 1ECC 80', 'whereami', 'progress',
                            'levelselect 106', 'wait 240', 'whereami', 'hold START', 'wait 2', 'hold', 'wait 20',
                            'hold SELECT', 'wait 2', 'hold', 'wait 400', 'whereami', 'progress', 'shot s1', 'quit'])
def _(ev, f):
    w, p = get(ev, 'w'), get(ev, 'p')
    expect(w[1]['level'] == '106', 'YI2 not started', f)
    expect(w[2]['mode'] == '0E' and p[1] == p[0] and w[2]['session'] == '0',
           'Start+Select did not bring back the map as it was (mode %s, %s -> %s)' % (w[2]['mode'], p[0], p[1]), f)


@scen('in_pipe', ['loadstate 9', 'wait 5', 'warp 03', 'wait 150', 'glide 1752 120', 'wait 40', 'hold DOWN', 'wait 6',
                  'whereami', 'levelselect 105', 'hold', 'wait 240', 'whereami', 'hold RIGHT', 'wait 60', 'hold',
                  'whereami', 'shot s1', 'quit'])
def _(ev, f):
    w = get(ev, 'w')
    expect(w[0]['state'] != '0', 'Mario was not in the pipe (state 0)', f)
    expect(w[1]['level'] == '105' and w[1]['state'] == '0' and int(w[2]['x']) > int(w[1]['x']),
           'YI1 not started normally: %s / %s' % (w[1], w[2]), f)


_fight = []
for _i in range(12):  # MC Mario with Super punch and Infinite MC life on Iggy's tilting platform (a fixed sequence that
    # beats him from this exact frame on: keep the lines before it unchanged)
    _fight += ['hold RIGHT', 'wait 15', 'hold Y', 'wait 4', 'hold', 'wait 10', 'hold RIGHT B', 'wait 16', 'hold',
               'wait 30', 'hold LEFT', 'wait 15', 'hold Y', 'wait 4', 'hold', 'wait 20']


@scen('castle_boss', ['cheat 0 1', 'cheat 11 1', 'loadstate 9', 'wait 5'] + TO_MAP + ['whereami', 'progress',
      'levelselect 101', 'wait 240', 'warp F6', 'wait 200', 'whereami', 'shot s0'] + _fight +
      ['wait 600', 'whereami', 'hold B', 'wait 4', 'hold', 'wait 300', 'hold A', 'wait 4', 'hold', 'wait 300', 'hold B',
       'wait 4', 'hold', 'wait 600', 'whereami', 'progress', 'peek 1EC7', 'shot s1', 'cheat 11 0', 'cheat 0 0', 'quit'])
def _(ev, f):
    w, p, pk = get(ev, 'w'), get(ev, 'p'), get(ev, 'peek')
    expect(w[1]['level'] == '1F6', "not in Iggy's room: %s" % w[1], f)
    expect(w[2]['mode'] in ('1A', '1B'), 'Iggy not beaten (no castle cutscene; mode %s)' % w[2]['mode'], f)
    expect(w[3]['mode'] == '0E' and p[1] == p[0] and w[3]['session'] == '0', 'map not as before after the castle', f)
    expect(int(pk[-1][0], 16) == 0, "Iggy's castle marked beaten (%s)" % pk[-1][0], f)


def run(name):
    lines, check = SCEN[name]
    d = os.path.join(out_root, name)
    os.makedirs(d, exist_ok=True)
    for x in os.listdir(d):
        os.remove(os.path.join(d, x))
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(([] if lines[0].startswith('cheat') else ['cheat 0 0', 'cheat 3 0']) + lines) + '\n')
    try:
        r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                           text=True, timeout=300)
        rc, out = r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        rc, out = 'timeout', ''
    open(os.path.join(d, 'stdout.txt'), 'w').write(out)
    fails = []
    if rc != 0:
        fails.append('exit code %s' % rc)
    try:
        check(parse(out), fails)
    except (IndexError, KeyError) as e:
        fails.append('missing output (%s)' % e)
    return name, fails


names = [n for n in SCEN if not only or n in only]
with ThreadPoolExecutor(6) as ex:
    for name, fails in ex.map(run, names):
        print('%-24s %s' % (name, 'PASS' if not fails else 'FAIL: ' + '; '.join(fails)))
