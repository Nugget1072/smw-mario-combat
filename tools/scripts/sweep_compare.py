"""Compares two sprite_sweep.py runs (e.g. the plain sweep and one with --cheat): per sprite id, Mario (mc / smw) and
test (t1 touch, t2 stomp, t3 punch), the exit code, hurt, death, bounce and the sprite's final status / id.
Prints crashes and new deaths first, then every other changed field.
Usage: python -I sweep_compare.py <base dir> <other dir>   (dirs under work/pilot, e.g. sprite_sweep sprite_sweep_c34)
"""
import glob, json, os, sys

def load(d):
    r = {}
    for p in glob.glob(os.path.join(d, 'results_*_*.json')):
        if os.path.basename(p).count('_') == 2:
            r.update(json.load(open(p)))
    return r

a, b = load(sys.argv[1]), load(sys.argv[2])
crash, deaths, changed = [], [], []
for sid in sorted(set(a) & set(b)):
    for who in ('mc', 'smw'):
        ra, rb = a[sid].get(who, {}), b[sid].get(who, {})
        if ra.get('rc') != rb.get('rc'):
            crash.append(f'{sid} {who}: rc {ra.get("rc")} -> {rb.get("rc")}')
        for t in ('t1', 't2', 't3'):
            ta, tb = ra.get(t) or {}, rb.get(t) or {}
            if tb.get('died') and not ta.get('died'):
                deaths.append(f'{sid} {who} {t}: new death')
            diff = [f'{k} {ta.get(k)}->{tb.get(k)}' for k in ('hurt', 'died', 'bounce', 'wid', 'wstatus')
                    if ta.get(k) != tb.get(k)]
            if diff:
                changed.append(f'{sid} {who} {t}: ' + ', '.join(diff))
print(f'{len(set(a) & set(b))} sprites compared')
print('exit code changes:', len(crash))
for c in crash:
    print('  ', c)
print('new deaths:', len(deaths))
for c in deaths:
    print('  ', c)
print('other changes:', len(changed))
for c in changed:
    print('  ', c)
