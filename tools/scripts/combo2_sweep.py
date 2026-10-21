"""tas_combo2.py's settings swept (round 19, the combo video): each setting played from the checkpoint on its own copy of
the session (sessions ca, cb, cc, cd: copies of `combo`), four at a time; prints the best combo per setting.
Usage: python -I combo2_sweep.py <checkpoint> [--frames N]
"""
import itertools, os, queue, re, subprocess, sys, threading

here = os.path.dirname(os.path.abspath(__file__))
cp = sys.argv[1]
frames = sys.argv[sys.argv.index('--frames') + 1] if '--frames' in sys.argv else '400'
jobs = list(itertools.product([14, 18, 22, 26], [-1, 6, 9], [0, 8, 12]))
free = queue.Queue()
for s in ('ca', 'cb', 'cc', 'cd'):
    free.put(s)
res, lock = [], threading.Lock()


def run(job):
    reach, mid, close = job
    sess = free.get()
    try:
        out = subprocess.run([sys.executable, '-I', os.path.join(here, 'tas_combo2.py'), sess, cp, '--frames', frames,
                              '--reach', str(reach), '--mid', str(mid), '--close', str(close), '--target', '99'],
                             capture_output=True, text=True).stdout
    finally:
        free.put(sess)
    m = re.search(r'best combo (\d+)', out)
    with lock:
        res.append((int(m.group(1)) if m else -1, job))
        print('reach %d mid %d close %d: combo %s' % (job + (m.group(1) if m else '?',)), flush=True)


ts = []
for j in jobs:
    t = threading.Thread(target=run, args=(j,))
    t.start()
    ts.append(t)
    while sum(t.is_alive() for t in ts) >= 4:
        ts[0].join(0.5)
        ts = [t for t in ts if t.is_alive()] + [t for t in ts if not t.is_alive()][:0]
for t in ts:
    t.join()
for best, job in sorted(res, reverse=True)[:8]:
    print('BEST reach %d mid %d close %d: combo %d' % (job + (best,)))
