"""Find a way through a stretch of level automatically (round 19, the speedrun videos): from a tas.py checkpoint, try
a number of random short move sequences (runs, running jumps of several lengths, hops, punches, waits), keep the one
that gets furthest right with no hit and no death (standing on the ground counts extra), checkpoint, and go on until
Mario's x reaches the target or the room changes. Seeded: the same arguments find the same way.
Usage: python -I tas_explore.py <session> <checkpoint> <target x> [--tries N] [--seed S] [--steps N]
"""
import contextlib, io, json, os, random, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tas  # noqa: E402

name, cp, target = sys.argv[1], sys.argv[2], int(sys.argv[3])
arg = lambda k, d: int(sys.argv[sys.argv.index(k) + 1]) if k in sys.argv else d
tries, seed, steps = arg('--tries', 12), arg('--seed', 1), arg('--steps', 60)
rng = random.Random(seed)


def macro():
    k = rng.randrange(8)
    n = rng.choice([6, 10, 16, 24, 32])
    j = rng.choice([2, 4, 8, 12, 16])
    if k <= 1:
        return ['hold RIGHT', 'wait %d' % n]
    if k == 2:
        return ['hold RIGHT X', 'wait %d' % n]
    if k == 3:
        return ['hold RIGHT B', 'wait %d' % j, 'hold RIGHT', 'wait %d' % n]
    if k == 4:
        return ['hold B', 'wait %d' % j, 'hold', 'wait %d' % n]
    if k == 5:
        return ['hold Y', 'wait 2', 'hold', 'wait %d' % max(6, n // 2)]
    if k == 6:
        return ['hold', 'wait %d' % n]
    return ['hold LEFT', 'wait %d' % min(n, 10)]


def rewind_to(c):
    s = tas.load(name)
    n = s['checkpoints'][c]
    tas.kill(s)
    s['moves'] = s['moves'][:n]
    s['checkpoints'] = {k: v for k, v in s['checkpoints'].items() if v <= n}
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


def checkpoint(c):
    s = tas.load(name)
    s['checkpoints'][c] = len(s['moves'])
    tas.save(name, s)


cur = cp
for stepno in range(steps):
    st0 = rewind_to(cur)
    best = None
    for t in range(tries):
        moves = sum((macro() for _ in range(rng.randrange(1, 4))), []) + ['hold', 'wait 4']
        if t:
            rewind_to(cur)
        st = do(moves)
        hurt = st.get('mc_life', 10) < st0.get('mc_life', 10) - 0.01 or st.get('hurt_timer', 0) > 0
        dead = st.get('player_state') == 9 or st.get('lives', 9) < st0.get('lives', 9)
        newroom = st.get('game_mode') != '14' or abs(st.get('x', 0) - st0.get('x', 0)) > 400
        if hurt or dead:
            continue
        score = (1 if newroom else 0, st.get('x', 0) + (20 if st.get('on_ground') else 0))
        if best is None or score > best[0]:
            best = (score, moves, st)
    if best is None:
        print('step %d: every try was hit or died' % stepno, flush=True)
        sys.exit(1)
    rewind_to(cur)
    st = do(best[1])
    cur = 'ex%03d' % stepno
    checkpoint(cur)
    print('step %d -> x %s y %s ground %s mode %s  %s' % (stepno, st.get('x'), st.get('y'), st.get('on_ground'),
                                                           st.get('game_mode'), '; '.join(best[1])), flush=True)
    if best[0][0] or st.get('x', 0) >= target:
        print('REACHED (checkpoint %s)' % cur)
        sys.exit(0)
print('out of steps')
