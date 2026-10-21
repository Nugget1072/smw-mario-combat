"""Punched enemies that SMW Mario can carry after a stomp: each sprite id is spawned in Yoshi's House, punched once by
Mario Combat Mario (status should become 9, stunned, as after a stomp), then punched again (status 10, kicked).
Also run on ids a stomp does not stun, which must react as before (knocked out: status 2, or unchanged).
Usage: python -I stun_check.py <root> [ids...]   (hex ids; default: a list of walkers)
Writes work/pilot/stun_check/<id>/ (script, screenshots) and prints one line per id.
"""
import os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
ids = [int(a, 16) for a in sys.argv[2:]] or [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0D, 0x0F,
                                             0x10, 0x11, 0x13, 0x30, 0x31, 0x46, 0x6F, 0xA2, 0xAB, 0xBF]
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'stun_check')
LINE = re.compile(r'sprite slot (\d+) id ([0-9A-F]{2}) status (\d+) x=(-?\d+) y=(-?\d+).*\(frame (\d+)\)')


def run(sid):
    d = os.path.join(out_root, '%02X' % sid)
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    s = ['loadstate 8', 'wait 5', 'cheat 0 0', 'cheat 1 1', 'spawn %02X 40 0' % sid, 'wait 30',
         'sprites', 'attack %02X 30' % sid, 'wait 12', 'shot a', 'sprites', 'wait 20',
         'attack %02X 30' % sid, 'wait 6', 'shot b', 'sprites', 'wait 30', 'shot c',
         'sprites', 'quit']
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(s) + '\n')
    r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                       text=True, timeout=300)
    frames, res = [], {}  # one `sprites` table per frame, in the order of the script's stages
    for line in r.stdout.splitlines():
        m = LINE.match(line)
        if not m:
            continue
        if m.group(6) not in frames:
            frames.append(m.group(6))
        stage = ['before', 'punch1', 'punch2', 'end', '?'][min(frames.index(m.group(6)), 4)]
        res.setdefault(stage, []).append('%s:%s' % (m.group(2), m.group(3)))
    return sid, r.returncode, res


with ThreadPoolExecutor(4) as ex:
    for sid, rc, res in ex.map(run, ids):
        print('%02X rc=%s' % (sid, rc), ' | '.join('%s %s' % (k, ' '.join(v)) for k, v in res.items()))
