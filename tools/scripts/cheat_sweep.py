"""Cheat combination sweep: every single cheat, every pair, all at once, each power-up lock, and the knockback scale at
20x alone, with each cheat and with all of them.

Each run plays the same Yoshi's Island 1 route with the given cheats on (running with P-speed jumps, punches, a long
jump hold for the moon jump, the goal tape and the score tally) and must end on the overworld map with no crash.
Usage: python -I cheat_sweep.py <root>   (writes work/pilot/cheat_sweep/<name>/ and a summary)
"""
import csv, itertools, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = os.path.abspath(sys.argv[1])  # (the game runs in its own folder: paths must not be relative)
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'cheat_sweep')
os.makedirs(out_root, exist_ok=True)
NAMES = ['life', 'nodmg', 'time', 'lives', 'star', 'moon', 'pspeed', 'switch', 'replay', 'triple', 'noknock',
         'superpunch', 'yoshi', 'slowmo', 'mckoopas', 'mirror', 'onehit', 'coins', 'kingkoopas', 'doublejump', 'pound',
         'walljump', 'slippery', 'magnet', 'flyyoshi', 'mchits', 'lifesteal', 'firepunch', 'instant', 'buster',
         'pitrescue', 'infjumps', 'openmap', 'stompall', 'fastenemies', 'chase', 'jumpenemies', 'lowgrav', 'airdash', 'respawn',
         'punchall']  # kCheat_* order

# The level is entered again after the cheats are set, so the cheats that act on level load (triple enemies, always
# Yoshi) take part.
ROUTE = """loadstate 9
wait 5
{cheats}
warp 105
wait 150
log c.csv
hold RIGHT X
wait 120
hold RIGHT X B
wait 30
hold RIGHT Y
wait 4
hold RIGHT X
wait 60
hold RIGHT B
wait 40
hold B
wait 60
hold
wait 60
shot mid
glide 4790 200
wait 30
hold RIGHT
wait 80
hold
wait 600
shot end
quit
"""

def run(name, cheats, powerup=0, knock=1):
    lines = [f'cheat {i} 1' for i in cheats]
    if powerup:
        lines.append(f'cheat powerup {powerup}')
    if knock > 1:
        lines.append(f'cheat knockback {knock}')
    d = os.path.join(out_root, name)
    os.makedirs(d, exist_ok=True)
    script = os.path.join(d, 'route.txt')
    route = ROUTE.format(cheats='\n'.join(lines) or '# no cheats')
    if NAMES.index('mirror') in cheats:  # mirror mode swaps Left / Right on the pad: the route presses Left instead
        route = route.replace('RIGHT', 'LEFT')
    open(script, 'w').write(route)
    p = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', script, '--out', d], cwd=game,
                       capture_output=True, timeout=300)
    rows = list(csv.DictReader(open(os.path.join(d, 'c.csv')))) if os.path.exists(os.path.join(d, 'c.csv')) else []
    modes = [int(r['mode']) for r in rows]
    end_mode = modes[-1] if modes else -1
    reached_map = 14 in modes[len(modes) // 2:]
    min_py = min((int(r['py']) for r in rows if int(r['mode']) == 20), default=0)
    deaths = sum(1 for a, b in zip(rows, rows[1:]) if a['state'] != '9' and b['state'] == '9')
    ok = p.returncode == 0 and reached_map and os.path.exists(os.path.join(d, 'end.bmp'))
    return dict(name=name, rc=p.returncode, frames=len(rows), reached_map=reached_map, end_mode=end_mode,
                deaths=deaths, min_life=min((float(r['mclife']) for r in rows), default=0), min_py=min_py, ok=ok)

jobs = [('none', [])]
jobs += [(NAMES[i], [i]) for i in range(len(NAMES))]
jobs += [(f'{NAMES[a]}+{NAMES[b]}', [a, b]) for a, b in itertools.combinations(range(len(NAMES)), 2)]
jobs += [('all', list(range(len(NAMES))))]
jobs += [(f'powerup{p}', [], p) for p in (1, 2, 3)]
jobs += [(f'all+powerup{p}', list(range(len(NAMES))), p) for p in (2, 3)]
jobs += [('knock20', [], 0, 20)]
jobs += [(f'knock20+{NAMES[i]}', [i], 0, 20) for i in range(len(NAMES))]
jobs += [('all+knock20', list(range(len(NAMES))), 0, 20)]
with ThreadPoolExecutor(4) as pool:  # four games at a time
    results = list(pool.map(lambda j: run(*j), jobs))
with open(os.path.join(out_root, 'summary.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(results[0].keys()))
    w.writeheader()
    w.writerows(results)
bad = [r for r in results if not r['ok']]
print(f'{len(results)} runs, {len(bad)} not OK')
for r in bad:
    print('  NOT OK:', r)
