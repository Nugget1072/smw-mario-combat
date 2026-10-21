"""Platforms and moving blocks met from below and from above (round 19): Mario Combat Mario against SMW Mario.
Each run: Yoshi's Island 1 (save state 9), flat ground at x 700, sprites cleared, No damage; one platform sprite.
  bump  the platform is spawned 40 px above Mario standing on the ground; he jumps straight up (B for 10 frames).
        Outcome: `bumped` (his head met it: the rise cut off in one frame), `above` (his feet got over its top: a
        platform solid on top only) or `short` (the jump didn't reach that high), then whether he ended up standing
        on it. MC Mario's jump is higher: SMW Mario's `short` and MC Mario's `above` agree.
  drop  Mario is put 100 px above the platform (pilot `ontop 0 0 100`) and falls: he must land on it (SMW's
        "standing on a sprite", `onspr`) as SMW Mario does.
MC Mario's outcome must be SMW Mario's (his own jump is higher and longer, so only the outcome is compared).
Usage: python -I platform_contact_test.py <root> [ids...]
Writes work/pilot/platform_contact/<id>_<scenario>_<mc|smw>/ and prints one line per sprite.
"""
import csv, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = os.path.abspath(sys.argv[1])
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'platform_contact')
IDS = [a.upper() for a in sys.argv[2:]] or [
    '2F', '49', '55', '56', '57', '58', '59', '5A', '5C', '5E', '5F', '60', '6B', '6C', '6D', '83', '84', '8F', '9C',
    'A3', 'B1', 'B7', 'B8', 'B9', 'BA', 'BB', 'BC', 'C0', 'C1', 'C4', 'C8']
SCEN = {
    'bump': ['spawn %s 0 -40', 'wait 2', 'log a.csv', 'hold B', 'wait 10', 'hold', 'wait 70', 'shot a'],
    'drop': ['spawn %s 0 -120', 'wait 2', 'ontop 0 0 100', 'log a.csv', 'wait 90', 'shot a'],
}


def run(job):
    sid, scen, mc = job
    d = os.path.join(out_root, '%s_%s_%s' % (sid, scen, 'mc' if mc else 'smw'))
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    s = ['loadstate 9', 'wait 5', 'cheat 1 1', 'setting mc %d' % mc, 'glide 700 240', 'wait 50', 'clearsprites']
    s += [l % sid if '%s' in l else l for l in SCEN[scen]] + ['log off', 'quit']
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(s) + '\n')
    subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                   text=True, timeout=300)
    a = os.path.join(d, 'a.csv')
    return job, list(csv.DictReader(open(a))) if os.path.exists(a) else []


def outcome(scen, rows):
    if not rows:
        return 'none'
    on = [r.get('onspr', '0') not in ('0', '') for r in rows]
    if scen == 'drop':
        return 'landed' if any(on) else 'missed'
    # a head bump: the rise cut off in one frame (y speed from -8 or faster to 0 or down: the sprite's bump sets 16);
    # otherwise above (his feet got over the platform's top, its y: wy) or short (the jump didn't reach that high)
    ys = [int(r['pys']) for r in rows]
    if any(a <= -8 and b >= 0 for a, b in zip(ys, ys[1:])):
        return 'bumped'
    above = any(int(r['py']) + 31 <= int(r['wy']) for r in rows if r['wid'] != '-1')
    return ('above' if above else 'short') + ('+on' if any(on) else '')


def same(scen, o_s, o_m):
    # MC Mario's own jump is higher: where SMW Mario's falls short of a platform that doesn't stop it, his may get on
    return o_s == o_m or (scen == 'bump' and o_s.startswith('short') and o_m.startswith('above'))


jobs = [(i, s, mc) for i in IDS for s in SCEN for mc in (0, 1)]
with ThreadPoolExecutor(6) as ex:
    res = dict(ex.map(run, jobs))
bad = 0
for sid in IDS:
    line, ok = [], True
    for s in SCEN:
        o_s, o_m = outcome(s, res[(sid, s, 0)]), outcome(s, res[(sid, s, 1)])
        good = same(s, o_s, o_m)
        ok &= good
        line.append('%s SMW %-10s MC %-10s%s' % (s, o_s, o_m, '' if good else ' <-'))
    bad += not ok
    print('%s  %s  %s' % (sid, '   '.join(line), 'ok' if ok else 'FAIL'), flush=True)
print('sprites %d, failed %d' % (len(IDS), bad))
