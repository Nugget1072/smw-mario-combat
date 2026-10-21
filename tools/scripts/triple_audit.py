"""Triple-enemies audit: enter every level number (000-1FF) with the cheat on, fly Mario through it (pilot `scroll`)
and collect the cheat's TRIPLE log: every enemy the level loads, how many copies it got at once, how many later
(after waiting for a free sprite slot), and why not (no fitting spot / no slot in time / not tripled by design).

Usage: python -I triple_audit.py <root> [first last] [--jobs 6]
Writes work/pilot/triple_audit/<level>/ (script, stdout) and work/pilot/triple_audit/summary.txt + summary.json.
"""
import collections, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
args = [a for a in sys.argv[2:] if not a.startswith('--')]
lo, hi = (int(args[0], 16), int(args[1], 16)) if len(args) >= 2 else (0, 0x1FF)
jobs = int(sys.argv[sys.argv.index('--jobs') + 1]) if '--jobs' in sys.argv else 6
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'triple_audit')
os.makedirs(out_root, exist_ok=True)

SCRIPT = """loadstate 9
wait 5
cheat 1 1
cheat 9 1
triplelog 1
warp {lv}
wait 100
scroll
wait 30
quit
"""

NAMES = {}  # optional names for the report
for line in """00 Koopa (no shell) green|01 Koopa (no shell) red|02 Koopa (no shell) blue|03 Koopa (no shell) yellow
04 Koopa green|05 Koopa red|06 Koopa blue|07 Koopa yellow|08 para-Koopa green|09 para-Koopa green bouncing
0A para-Koopa red vertical|0B para-Koopa red horizontal|0C para-Koopa yellow|0D Bob-omb|0F Goomba
10 para-Goomba bouncing|11 Buzzy Beetle|13 Spiny|14 Spiny egg|15 Cheep-cheep|16 Cheep-cheep vertical
17 Cheep-cheep flying|18 Cheep-cheep jumping|1A Piranha Plant|1C Bullet Bill|1D Hopping Flame|1E Lakitu
1F Magikoopa|22 net Koopa|23 net Koopa|24 net Koopa|25 net Koopa|26 Thwomp|27 Thwimp|28 Big Boo|29 Koopa Kid
2A Piranha (upside down)|2E Spike Top|30 Dry Bones (throws)|31 Bony Beetle|32 Dry Bones|33 Podoboo|37 Boo
38 Eerie|39 Eerie (wave)|3A Urchin|3B Urchin|3C Urchin (wall)|3D Rip Van Fish|3F para-Goomba|40 para-Bomb
46 Diggin' Chuck|47 grey fish|4B pipe Lakitu|4D Monty Mole (ground)|4E Monty Mole (ledge)|4F Jumping Piranha
50 Jumping Piranha (fire)|51 Ninji|6E Dino-Rhino|6F Dino-Torch|70 Pokey|71 Super Koopa (red)|72 Super Koopa (yellow)
73 Super Koopa (feather)|86 Wiggler|90 gas bubble|91 Chargin' Chuck|92 Splittin' Chuck|93 Bouncin' Chuck
94 Whistlin' Chuck|95 Clappin' Chuck|97 Puntin' Chuck|98 Pitchin' Chuck|99 Volcano Lotus|9A Sumo Bro
9B Hammer Bro|9F Banzai Bill|A2 Mechakoopa|A5 Sparky / Fuzzy|A6 Hothead|A8 Blargg|AA Fishbone|AB Rex
AC wooden spike|AD wooden spike|AE Fishin' Boo|B2 falling spike|B4 Grinder|B6 fireball|BD sliding Koopa
BE Swooper|BF Mega Mole|C2 Blurp""".replace('\n', '|').split('|'):
    NAMES[line[:2]] = line[3:]


def run(lv):
    d = os.path.join(out_root, f'{lv:03X}')
    os.makedirs(d, exist_ok=True)
    script = os.path.join(d, 'script.txt')
    open(script, 'w').write(SCRIPT.format(lv=f'{lv:03X}'))
    try:
        p = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', script, '--out', d], cwd=game,
                           capture_output=True, text=True, timeout=180)
        out, rc = p.stdout, p.returncode
    except subprocess.TimeoutExpired as e:
        out, rc = (e.stdout or b'').decode(errors='replace') if isinstance(e.stdout, bytes) else (e.stdout or ''), 'timeout'
    open(os.path.join(d, 'stdout.txt'), 'w').write(out)
    return lv, rc, out


per_id = collections.defaultdict(lambda: collections.Counter())
levels_of = collections.defaultdict(set)
bad = []
with ThreadPoolExecutor(jobs) as ex:
    for lv, rc, out in ex.map(run, range(lo, hi + 1)):
        if rc not in (0,):
            bad.append((f'{lv:03X}', rc))
        for line in out.splitlines():
            m = re.match(r'TRIPLE (\w\w) (-?\d+) (-?\d+) kind (\d)(?: made (\d) queued (\d) nospot (\d) oneslot (\d))?', line)
            if m:
                sid, kind = m.group(1), int(m.group(4))
                c = per_id[sid]
                c['loads'] += 1
                c['kind'] = kind
                levels_of[sid].add(f'{lv:03X}')
                if kind:
                    made, queued, nospot = int(m.group(5)), int(m.group(6)), int(m.group(7))
                    c['copies_now'] += made
                    c['queued'] += queued
                    c['nospot'] += nospot
                    c['oneslot'] += int(m.group(8)) * (2 - made)
                continue
            m = re.match(r'TRIPLELATE (\w\w) (made|dropped) (\d)', line)
            if m:
                per_id[m.group(1)]['late_' + m.group(2)] += int(m.group(3))

KIND = {0: 'not tripled (by design)', 1: 'walker', 2: 'flier / swimmer', 3: 'terrain'}
rows = []
for sid in sorted(per_id, key=lambda s: int(s, 16)):
    c = per_id[sid]
    want = 2 * c['loads'] if c['kind'] else 0
    got = c['copies_now'] + c['late_made']
    pct = 100.0 * got / want if want else 0.0
    rows.append(dict(id=sid, name=NAMES.get(sid, ''), kind=KIND[c['kind']], loads=c['loads'], copies=got, wanted=want,
                     pct=round(pct, 1), no_spot=c['nospot'], waited_dropped=c['late_dropped'], one_slot=c['oneslot'],
                     levels=sorted(levels_of[sid])[:12]))
json.dump(dict(rows=rows, bad_runs=bad), open(os.path.join(out_root, 'summary.json'), 'w'), indent=1)
with open(os.path.join(out_root, 'summary.txt'), 'w') as f:
    total_w = sum(r['wanted'] for r in rows)
    total_g = sum(r['copies'] for r in rows)
    f.write(f'Triple-enemies audit, levels {lo:03X}-{hi:03X}: {total_g} of {total_w} copies made '
            f'({100.0 * total_g / max(1, total_w):.1f}%). Runs that did not end normally: {len(bad)}\n\n')
    f.write('loads = times the level put the enemy in; copies = copies made / wanted (2 per load);\n'
            'no spot = loads with no place nearby where that enemy can be (walls, pits, no matching pipe / dirt);\n'
            'no slot = copies that waited for a free sprite slot and were dropped (SMW\'s per-level sprite limit was\n'
            '          full until the enemy left); 1-slot = copies impossible because SMW gives this sprite a single\n'
            '          reserved slot (Banzai Bill in some sprite-memory settings).\n\n')
    f.write(f"{'id':3} {'enemy':28} {'kind':24} {'loads':>5} {'copies':>7} {'%':>6} {'no spot':>8} {'no slot':>8} {'1-slot':>7}\n")
    for r in rows:
        f.write(f"{r['id']:3} {r['name'][:28]:28} {r['kind']:24} {r['loads']:5} {r['copies']:4}/{r['wanted']:<4} "
                f"{r['pct']:6.1f} {r['no_spot']:8} {r['waited_dropped']:8} {r['one_slot']:7}\n")
    if bad:
        f.write('\nRuns that did not end normally (level, exit code): ' + ', '.join(f'{a}:{b}' for a, b in bad) + '\n')
print(open(os.path.join(out_root, 'summary.txt')).read())
