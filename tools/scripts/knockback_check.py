# Knockback scale check (round 12): reads the knockback_r12 pilot's logs (one per scale and hit kind, from the hit
# until Mario has stopped) and prints how far and how high each hit carried Mario Combat Mario, and the ratio to 1x.
# Usage: python -I tools/scripts/knockback_check.py [work/pilot/knockback_r12]
import csv, os, sys

d = sys.argv[1] if len(sys.argv) > 1 else os.path.join('work', 'pilot', 'knockback_r12')
res = {}
for kind in ('l', 'h'):
    for n in (1, 2, 5, 10, 20):
        p = os.path.join(d, f'kb_{n}_{kind}.csv')
        if not os.path.exists(p):
            continue
        rows = list(csv.DictReader(open(p)))
        x0, y0 = int(rows[0]['px']), int(rows[0]['py'])
        xs = [int(r['px']) for r in rows]
        ys = [int(r['py']) for r in rows]
        stop = next((i for i in range(1, len(rows)) if all(xs[j] == xs[i] for j in range(i, min(i + 20, len(rows))))),
                    len(rows) - 1)
        dist, rise = xs[stop] - x0, y0 - min(ys)
        hurt = sum(r['mchurt'] != '0' for r in rows)  # frames in MC's hurt clip
        res[(kind, n)] = (dist, rise, stop, hurt, min(float(r['mclife']) for r in rows))
for kind, name in (('l', 'light (SMW enemy)'), ('h', 'heavy (MC strong attack)')):
    print(name)
    b = res.get((kind, 1))
    for n in (1, 2, 5, 10, 20):
        if (kind, n) not in res:
            continue
        dist, rise, stop, hurt, life = res[(kind, n)]
        ratio = dist / b[0] if b and b[0] else 0
        hr = rise / b[1] if b and b[1] else 0
        print(f'  {n:2d}x: {dist:5d} px right ({ratio:5.1f}x), {rise:4d} px up ({hr:5.1f}x), stopped after {stop:3d} '
              f'frames, hurt clip {hurt} frames, life {life}')
