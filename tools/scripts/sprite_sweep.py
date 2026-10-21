"""Interaction audit: every normal sprite (0x00-0xC8) against Mario Combat Mario and, as the reference, SMW Mario.

For each sprite id and character, one pilot run in Yoshi's House (flat floor, walls both sides) with three tests,
each logged to its own CSV:
  t1 touch : the sprite is spawned 40 px to Mario's right; Mario stands still for 70 frames
  t2 stomp : Mario hovers ~80 px above the spot, the sprite is spawned on the floor below him, he drops onto it
  t3 punch : (MC only) the sprite is spawned 28 px to his right and he jabs it
Compared per sprite: hurt (MC life loss / SMW power-down or death), bounce after landing on it, the sprite's status
and id afterwards, crashes. Usage: python -I sprite_sweep.py <root> [first_id last_id] [reanalyse]
(sweep_report.py prints the MC-vs-SMW table.)
"""
import csv, json, os, subprocess, sys

root = sys.argv[1]
argv = sys.argv[2:]
extra_cheat = None  # --cheat N[,M...]: run every test with those cheats on (results in sprite_sweep_cN[_M...])
if '--cheat' in argv:
    i = argv.index('--cheat')
    extra_cheat = [int(c) for c in argv[i + 1].split(',')]
    del argv[i:i + 2]
nums = [a for a in argv if a != 'reanalyse']
lo, hi = (int(nums[0], 16), int(nums[1], 16)) if len(nums) >= 2 else (0, 0xC8)
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'sprite_sweep' + ('_c' + '_'.join(map(str, extra_cheat)) if extra_cheat is not None else ''))
os.makedirs(out_root, exist_ok=True)

SCRIPT = """loadstate 8
wait 5
{toggle}
cheat 0 0
log t1.csv
spawn {id:02X} 40 0
wait 70
shot t1
log off
clearsprites
poke 1497 00
poke 19 01
glide 120 318
log t2.csv
spawn {id:02X} 0 34
wait 70
shot t2
log off
clearsprites
poke 1497 00
poke 19 01
glide 120 300
wait 30
log t3.csv
spawn {id:02X} 28 0
wait 1
hold Y
wait 2
hold
wait 50
shot t3
log off
quit
"""


def read(path):
    try:
        return list(csv.DictReader(open(path)))
    except OSError:
        return []


def analyse(rows, mc):
    if not rows:
        return None
    life0 = float(rows[0]['mclife'])
    # a change into the hurt (1) / death (9) state during the test: a power-down still running from the previous
    # test is not counted
    hurt = any(b['state'] in ('1', '9') and a['state'] != b['state'] for a, b in zip(rows, rows[1:])) \
        or (mc and min(float(r['mclife']) for r in rows) < life0) \
        or (not mc and min(int(r['power']) for r in rows) < int(rows[0]['power']))
    died = any(r['state'] == '9' for r in rows)
    # bounce: after falling (pys > 0) Mario starts rising again above the floor (py < 352 - 4) without B pressed
    bounce = False  # launched upward while falling, and really rising at least 4 px over the next frames
    for i in range(len(rows) - 1):
        b = rows[i + 1]
        fell = any(int(r['pys']) > 0 for r in rows[max(0, i - 12):i + 1])  # a springboard holds Mario still first
        if fell and int(rows[i]['pys']) >= 0 and int(b['pys']) < -8 and int(b['py']) < 348:
            if min(int(r['py']) for r in rows[i + 1:i + 14]) <= int(b['py']) - 4:
                bounce = True
    last = rows[-1]
    return dict(hurt=hurt, died=died, bounce=bounce, wid=last['wid'], wstatus=last['wstatus'], wc2=last['wc2'],
                min_py=min(int(r['py']) for r in rows), mode=last['mode'], state=last['state'],
                carry=any(r['carry'] != '0' for r in rows))


reanalyse = 'reanalyse' in sys.argv  # only re-read the CSVs of an earlier run (keeps its exit codes)
results_path = os.path.join(out_root, f'results_{lo:02X}_{hi:02X}.json')
old = json.load(open(results_path)) if reanalyse and os.path.exists(results_path) else {}
results = {}
for sid in range(lo, hi + 1):
    res = {}
    for who in ('mc', 'smw'):
        d = os.path.join(out_root, f'{sid:02X}_{who}')
        if reanalyse:
            rc = old.get(f'{sid:02X}', {}).get(who, {}).get('rc', 0)
            res[who] = dict(rc=rc, **{t: analyse(read(os.path.join(d, f'{t}.csv')), who == 'mc') for t in ('t1', 't2', 't3')})
            continue
        os.makedirs(d, exist_ok=True)
        script = os.path.join(d, 'script.txt')
        text = SCRIPT.format(id=sid, toggle='toggle\nwait 10' if who == 'smw' else '# mc')
        if extra_cheat is not None:
            text = text.replace('cheat 0 0\n', 'cheat 0 0\n' + ''.join('cheat %d 1\n' % c for c in extra_cheat))
        open(script, 'w').write(text)
        try:
            p = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', script, '--out', d], cwd=game,
                               capture_output=True, timeout=120)
            rc = p.returncode
        except subprocess.TimeoutExpired:
            rc = 'timeout'
        res[who] = dict(rc=rc, **{t: analyse(read(os.path.join(d, f'{t}.csv')), who == 'mc') for t in ('t1', 't2', 't3')})
    results[f'{sid:02X}'] = res
    print(f'{sid:02X}', json.dumps(res), flush=True)
json.dump(results, open(results_path, 'w'), indent=1)
