"""Air dash into pipes: MC Mario, big SMW Mario and small SMW Mario jump next to a pipe and air-dash into its side,
at every height of the jump (the dash pressed 1..N frames after the take-off), from the left and from the right,
with the Air dash cheat on. Then they walk on over the pipe. A run fails if Mario dies, if he is in a pipe state
(5 / 6) during the dash, or if he can't carry on.
Usage: python -I dash_pipe_test.py <root> [--delays N]
Writes work/pilot/dash_pipe/<who>_<pipe>_<side>_<delay>/ and prints one line per run plus a summary.
"""
import csv, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
delays = int(sys.argv[sys.argv.index('--delays') + 1]) if '--delays' in sys.argv else 24
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'dash_pipe')
# Yoshi's Island 2 (room 106, warped to from save state 9; pilot `findtiles 133 13F`): the green pipe x 4368-4399,
# top y 336, and the grey pipe x 4320-4351, top y 352; the ground is at y 384.
PIPES = {'green': (4368, 32), 'grey': (4320, 32)}


def run(job):
    who, pipe_name, delay, side = job
    pipe_x, pipe_w = PIPES[pipe_name]
    d = os.path.join(out_root, '%s_%s_%s_%02d' % (who, pipe_name, side, delay))
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    x0 = pipe_x - 70 if side == 'left' else pipe_x + pipe_w + 54
    dash, walk = ('R', 'RIGHT') if side == 'left' else ('L', 'LEFT')
    s = ['loadstate 9', 'wait 5', 'cheat 3 1', 'cheat 1 1', 'cheat 38 1'] + (['toggle', 'wait 5'] if who != 'mc' else [])
    s += ['warp 106', 'wait 110'] + (['power 0'] if who == 'smw_small' else [])
    s += ['setpos %d 352' % x0, 'wait 30', 'log d.csv', 'hold B', 'wait %d' % delay,
          'hold B %s' % dash, 'wait 1', 'hold B', 'wait 6', 'shot a', 'hold', 'wait 50', 'shot b', 'whereami',
          # then he walks on (over the pipe), still alive and out of any pipe
          'hold %s B' % walk, 'wait 60', 'log off', 'whereami', 'shot c', 'hold', 'wait 2']
    s += (['toggle'] if who != 'mc' else []) + ['quit']
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(s) + '\n')
    r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                       text=True, timeout=300)
    rows = list(csv.DictReader(open(os.path.join(d, 'd.csv')))) if os.path.exists(os.path.join(d, 'd.csv')) else []
    dash_rows = rows[:delay + 40]  # the dash and its stop, before he walks on
    states = set(r['state'] for r in dash_rows)
    w = re.findall(r'whereami: mode (\w+) level (\w+) .* state (\d+) ', r.stdout)
    died = any(r['state'] == '9' for r in rows) or any(st == '9' for _, _, st in w)
    pipe = bool(states & {'5', '6'})
    carries_on = len(w) >= 2 and w[1][2] == '0' and w[1][0] == '14'
    xs = [int(r['px']) for r in dash_rows]
    stop = (max(xs) if side == 'left' else min(xs)) if xs else 0
    # (the body's edge against the pipe: SMW Mario's x + 13 on the left side, x + 2 on the right)
    ok = not died and not pipe and carries_on
    return job, ok, died, pipe, carries_on, stop


jobs = [(who, pn, dl, side) for who in ('mc', 'smw', 'smw_small') for pn in PIPES for side in ('left', 'right')
        for dl in range(1, delays + 1)]
fails = 0
with ThreadPoolExecutor(4) as ex:
    for (who, pn, dl, side), ok, died, pipe, on, stop in ex.map(run, jobs):
        fails += not ok
        x0, w = PIPES[pn]
        print('%-3s %-5s %-5s delay %2d %s died=%d pipe_state=%d carries_on=%d stop_x=%d (pipe %d-%d)' % (
            who, pn, side, dl, 'ok  ' if ok else 'FAIL', died, pipe, on, stop, x0, x0 + w - 1), flush=True)
print('runs %d, failed %d' % (len(jobs), fails))
