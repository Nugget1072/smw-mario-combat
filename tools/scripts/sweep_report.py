"""Summarise sprite_sweep.py results: MC Mario vs SMW Mario per sprite, flagging the differences.
Usage: python -I sweep_report.py <results json> [more json...]  -> prints a table; differences marked with '*'.
Letters: H hurt, D died, B bounced (rising for several frames after landing), C carried; sN = the sprite's status after.
"""
import json, sys

rows = {}
for path in sys.argv[1:]:
    rows.update(json.load(open(path)))


def flags(t):
    if t is None:
        return 'none'
    return ('H' if t['hurt'] else '-') + ('D' if t['died'] else '-') + ('B' if t['bounce'] else '-') + \
        ('C' if t.get('carry') else '-') + f" s{t['wstatus']:>2} id{int(t['wid']):02X}"


diffs = 0
for sid in sorted(rows, key=lambda s: int(s, 16)):
    v = rows[sid]
    mc, smw = v['mc'], v['smw']
    marks = []
    for t in ('t1', 't2'):
        a, b = mc.get(t), smw.get(t)
        if a and b and (a['hurt'], a['died'], a['bounce']) != (b['hurt'], b['died'], b['bounce']):
            marks.append(t)
    if mc['rc'] != 0 or smw['rc'] != 0:
        marks.append(f"rc {mc['rc']}/{smw['rc']}")
    diffs += bool(marks)
    print(f"{sid} {'*' if marks else ' '} t1 {flags(mc['t1'])} | {flags(smw['t1'])}   t2 {flags(mc['t2'])} | "
          f"{flags(smw['t2'])}   t3 {flags(mc['t3'])}  {' '.join(marks)}")
print(f'{len(rows)} sprites, {diffs} with a difference')
