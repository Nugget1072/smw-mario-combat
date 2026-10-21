"""The boss rush played by a bot on a tas.py session (round 19): for each fight, rounds of the pilot's `attack boss`
(walk up to the boss and punch; every k-th round a jump kick) until the boss rush HUD moves on to the next fight. A
round that costs Mario life (or his life) sends the fight back to its start (a checkpoint) to be tried again with
another timing: a wait before the first round and another jump-kick rhythm. The kept moves are the run.
Usage: python -I tas_bot.py <session> [first fight] [--tries N]
"""
import contextlib, io, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tas  # noqa: E402

name = sys.argv[1]
first = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 1
tries = int(sys.argv[sys.argv.index('--tries') + 1]) if '--tries' in sys.argv else 40
BOWSER = 10


def step(moves):
    s = tas.load(name)
    if not tas.alive(s):
        tas.launch(name, s)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        tas.send(name, s, moves)
    m = re.search(r'STATUS (\{.*\})', buf.getvalue())
    return json.loads(m.group(1)) if m else {}


def fight_no(st):
    m = re.match(r'BOSS (\d+)/', st.get('rush', ''))
    return int(m.group(1)) if m else 0


def quiet(fn, *a):
    with contextlib.redirect_stdout(io.StringIO()):
        fn(*a)


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
    quiet(tas.send, name, s, [])


st = step(['wait 1'])
fight = fight_no(st)
while 0 < fight <= 10:
    cp = 'fight%02d' % fight
    checkpoint(cp)
    life0 = st.get('mc_life', 10)
    won = False
    for t in range(tries):
        pre = (t * 7) % 40
        every = [3, 2, 4, 5, 1][t % 5] if fight != BOWSER else 1
        delay = '' if fight != BOWSER else ' %d' % (6 + (t % 4) * 2)
        st = step(['wait %d' % (pre + 1)])
        bad = False
        for r in range(400):
            air = ' air' + delay if (r % every == every - 1) else ''
            st = step(['attack boss 40%s' % air, 'wait 4'])
            if st.get('player_state') == 9 or st.get('mc_life', 10) < life0:
                bad = True
                break
            if fight_no(st) != fight or not st.get('in_level'):
                won = True
                break
        if won:
            print('fight %d won on try %d (rush %s, life %.1f)' % (fight, t + 1, st.get('rush'), st.get('mc_life', 0)),
                  flush=True)
            break
        print('fight %d try %d: %s' % (fight, t + 1, 'hit' if bad else 'too long'), flush=True)
        rewind(cp)
        st = step(['wait 1'])
    if not won:
        print('fight %d not won in %d tries; stopping' % (fight, tries))
        break
    # wait for the next fight to begin (the HUD shows it once the room is loaded)
    for _ in range(60):
        if fight_no(st) != fight and st.get('game_mode') == '14' and st.get('in_level'):
            break
        st = step(['wait 10'])
    fight = fight_no(st)
    if 'CLEAR' in st.get('rush', '') or not fight:
        break
print('done: rush %s' % st.get('rush'))
