"""Punch anything cheat (round 18): MC attacks hurt the enemies only a star defeats.
Each run: Yoshi's Island 1 (save state 9), flat ground at x 260, sprites cleared, No damage on; one enemy is spawned
in front of MC Mario (a little above the ground for the fliers and wall / ceiling crawlers, which then hang in the
air for a moment) and he punches it (pilot `attack`, several times). Three runs per enemy:
  off    the cheat off: the enemy must still be there (immune, as in SMW)
  on     the cheat on: it must be knocked out (status 2, falling off screen) by the punches
  hits   the cheat and MC hit reactions on: it takes MC hits (its MC life goes down, knocked flying)
Not spawned here: the Podoboo (33: its set-up hangs away from lava), the line-guided Grinder and Fuzzy (67, 68: no
line to ride) and the floating spike ball (A4: stays in its set-up out of water); they share the code path.
Usage: python -I punch_all_test.py <root> [ids...] [--on]   (--on: only the cheat-on run, for an audit)
Writes work/pilot/punch_all/<id>_<mode>/ and prints one line per enemy.
"""
import os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = os.path.abspath(sys.argv[1])
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'punch_all')
CHEAT_PUNCH_ALL, CHEAT_MC_HITS = 40, 25
ONLY_ON = '--on' in sys.argv  # --on: just the cheat-on run (an audit over every ordinary enemy)
IDS = [a for a in sys.argv[2:] if not a.startswith('--')] or [
    '26', '27', '28', '37', '38', '39', '3A', '3B', '3C', '44', '70', '86', '90', 'A5', 'A6', 'A8', 'AA', 'AE', 'AF',
    'B2', 'B4', 'B6']
MODES = {'off': [], 'on': [CHEAT_PUNCH_ALL], 'hits': [CHEAT_PUNCH_ALL, CHEAT_MC_HITS]}
if ONLY_ON:
    MODES = {'on': MODES['on']}


def run(job):
    sid, mode = job
    d = os.path.join(out_root, '%s_%s' % (sid, mode))
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    s = ['loadstate 9', 'wait 5', 'cheat 1 1'] + ['cheat %d 1' % c for c in MODES[mode]]
    # (a Pokey's position is the top of its column: spawned higher, it lands on the ground)
    s += ['glide 260 240', 'wait 50', 'clearsprites', 'spawn %s 36 %d' % (sid, -72 if sid == '70' else -8),
          'wait %d' % (40 if sid == '70' else 1), 'sprites', 'shot a']
    for n in range(6):
        s += ['attack %s 40' % sid, 'wait 6', 'sprites']
        if n == 0:
            s.append('shot b')
    s += ['wait 20', 'shot c', 'sprites', 'quit']
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(s) + '\n')
    try:
        out = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game,
                             capture_output=True, text=True, timeout=300).stdout
    except subprocess.TimeoutExpired as e:
        out = (e.stdout.decode() if isinstance(e.stdout, bytes) else e.stdout or '') + '\nTIMEOUT\n'
    open(os.path.join(d, 'out.txt'), 'w').write(out)
    states = [int(m) for m in re.findall(r'sprite slot \d+ id %s status (\d+)' % sid, out)]
    attacks = len(re.findall(r'^attack:', out, re.M))
    mc_life = [float(x) for x in re.findall(r'mc-hits life=([\d.]+)', out)]
    return job, states, attacks, min(mc_life) if mc_life else 10


jobs = [(i, m) for i in IDS for m in MODES]
with ThreadPoolExecutor(6) as ex:
    res = dict((j, r) for j, *r in ex.map(run, jobs))
bad = 0
for sid in IDS:
    line, ok = [], True
    for m in MODES:
        states, attacks, life = res[(sid, m)]
        alive_end = bool(states) and states[-1] == 8
        knocked = 2 in states or (bool(states) and not alive_end) or not states[1:]
        if m == 'off':
            good = bool(states) and alive_end
        elif m == 'on':
            good = knocked
        else:
            good = knocked or life < 10
        ok &= good
        line.append('%s %s (%d attacks, states %s%s)' % (m, 'ok' if good else 'FAIL', attacks,
                                                         ''.join(str(x) for x in states[:12]),
                                                         ', MC life %g' % life if m == 'hits' else ''))
    bad += not ok
    print('%s  %s' % (sid, '  '.join(line)), flush=True)
print('enemies %d, failed %d' % (len(IDS), bad))
