"""Air attacks against blocks: MC Mario jumps from the ground pressed against a block column (holding toward it) and
presses the punch button (Y) N frames after the take-off (N = 2..MAX), and the same in open air as the reference.
- `column_jump`: a full jump (B held): he rises along the column, attacks, and may land on top of it.
- `column_hop`: a short hop (B tapped for 3 frames): he stays beside the column, clings to it in MC's wall pose (17)
  and slides down it; the press comes before, while and after the cling.
- `head_bump`: standing under a turn block (x 3824, y 320), a straight jump bumps it from below; the press comes
  before and after the bump (reference: a straight jump in open air).
A press in the air must start an attack (hero frame 18) within 4 frames, and the attack must not give way to the jump
or wall pose in the air while the same attack in open air goes on (ending on a landing is MC's own end).
Site: Yoshi's Island 1 (save state 9), the left cement column of the block U (x 1904-1919, top y 320); open air from
x 260 (the flat start).
Usage: python -I air_attack_wall_test.py <root> [--max N]
Writes work/pilot/air_attack_wall/<site>_<n>/ and prints one line per run plus a summary.
"""
import csv, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
MAX = int(sys.argv[sys.argv.index('--max') + 1]) if '--max' in sys.argv else 40
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'air_attack_wall')
COLUMN = ['glide 1872 240', 'wait 50', 'hold RIGHT', 'wait 20']   # on the ground, pressed against the column
OPEN = ['glide 260 240', 'wait 50', 'hold RIGHT', 'wait 20']  # (flat ground, nothing within reach)
UNDER = ['glide 3790 240', 'wait 50', 'glide 3826 352', 'wait 30']  # standing under the turn block at x 3824, y 320
SITES = {  # name: (setup, frames B is held for the jump, open-air reference, direction held)
    'column_jump': (COLUMN, 99, 'open_jump', 'RIGHT'),
    'column_hop': (COLUMN, 3, 'open_hop', 'RIGHT'),
    'head_bump': (UNDER, 99, 'open_up', ''),
    'open_jump': (OPEN, 99, None, 'RIGHT'),
    'open_hop': (OPEN, 3, None, 'RIGHT'),
    'open_up': (OPEN, 99, None, ''),
}


def run(job):
    site, n = job
    setup, jump, _, held = SITES[site]
    d = os.path.join(out_root, '%s_%02d' % (site, n))
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    s = ['loadstate 9', 'wait 5', 'cheat 1 1'] + setup + ['log a.csv']
    for f in range(n + 50):  # one line per frame: the direction held, B for the jump, Y pressed at frame n for 2 frames
        s += ['hold ' + held + (' B' if f < jump else '') + (' Y' if n <= f < n + 2 else ''), 'wait 1']
    s += ['shot a', 'hold', 'wait 10', 'log off', 'quit']
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(s) + '\n')
    subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                   text=True, timeout=300)
    rows = list(csv.DictReader(open(os.path.join(d, 'a.csv'))))
    window = rows[n:n + 6]
    started = any(r['mcframe'] == '18' for r in window)
    pose = rows[n]['mcframe'] if n < len(rows) else '?'
    ground = rows[n]['mcground'] if n < len(rows) else '?'
    clung = any(r['mcframe'] == '17' for r in rows[:n + 1])
    # cut: the attack gave way to the jump / wall pose while still in the air
    cut, seen, length = False, False, 0
    for r in rows[n:]:
        if r['mcframe'] == '18':
            seen = True
            length += 1
        elif seen:
            cut = r['mcframe'] in ('9', '17') and r['mcground'] == '0'
            break
    return job, started, cut, pose, ground, length, clung


jobs = [(s, n) for s in SITES for n in range(2, MAX + 1, 2)]
res = {}
with ThreadPoolExecutor(6) as ex:
    for job, *r in ex.map(run, jobs):
        res[job] = r
bad = clings = 0
for (site, n), (started, cut, pose, ground, length, clung) in sorted(res.items()):
    refname = SITES[site][2]
    ref = res[(refname, n)][4] if refname else None
    shorter = cut and ref is not None and length < ref - 2
    ok = (started or ground == '1') and not shorter
    bad += not ok
    clings += clung and site.startswith('column')
    print('%-12s press %2d  pose %-3s ground %s  clung %-5s attack %-5s frames %2d (open air %s) %s' % (
        site, n, pose, ground, clung, started, length, ref, 'ok' if ok else 'FAIL'), flush=True)
print('runs %d, failed %d; %d column runs clung to the column before the press' % (len(jobs), bad, clings))
