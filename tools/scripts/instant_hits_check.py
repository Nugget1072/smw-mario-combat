"""Check a pilot log (moves.csv from tools/pilots/instant_hits.txt) for the Instant hits cheat: every attack move
(attack-clip label segment) shows its hit box on the first frame it appears, and lists the move sequence.
Usage: python -I instant_hits_check.py <moves.csv>
"""
import csv, sys

STARTS = [1, 27, 49, 76, 100, 133, 178, 206, 220, 248, 263, 278, 293, 308, 340, 375, 411]
NAMES = ['attack1', 'attack2', 'attack3', 'attack4', 'attack5', 'attack6', 'attack7', 'attack8', 'attack9',
         'attack10', 'attack102', 'attack103', 'attack104', 'attack105', 'attack11', 'attack12']


def move(af):
    for m in range(len(STARTS) - 1):
        if STARTS[m] <= af < STARTS[m + 1]:
            return m
    return -1


rows = list(csv.DictReader(open(sys.argv[1])))
prev, seq, bad = -1, [], 0
for i, r in enumerate(rows):
    af, nh = int(r['mcatt']), int(r['nhit'])
    m = move(af) if r['mcframe'] == '18' else -1
    if m >= 0 and (m != prev or (i and int(rows[i - 1]['mcatt']) > af)):  # a move starts (or restarts)
        ok = nh > 0
        bad += not ok
        seq.append('%s@%d%s' % (NAMES[m], i, '' if ok else '(NO HIT on first frame: f%d)' % af))
    prev = m
print(' '.join(seq))
print('%d moves, %d without a hit box on their first frame' % (len(seq), bad))
