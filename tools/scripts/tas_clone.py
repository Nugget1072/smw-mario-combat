"""Copy a tas.py session (round 19): its moves and checkpoints into a new session (no game running), with a checkpoint
`now` at its current end, so a running search's state can be looked at (tas_backtrack.py --watch) without touching it.
Usage: python -I tas_clone.py <session> <new session>
"""
import json, os, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tas  # noqa: E402

src, dst = sys.argv[1], sys.argv[2]
for _ in range(50):
    try:
        s = json.load(open(os.path.join(tas.sdir(src), 'session.json')))
        break
    except (OSError, ValueError):
        time.sleep(0.1)
else:
    tas.die('could not read session ' + src)
os.makedirs(tas.sdir(dst), exist_ok=True)
s['pid'] = None
s['checkpoints']['now'] = len(s['moves'])
tas.save(dst, s)
print('%s: %d moves, checkpoint now' % (dst, len(s['moves'])))
