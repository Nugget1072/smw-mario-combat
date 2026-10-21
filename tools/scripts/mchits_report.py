"""Summarise mchits_sweep.py: per sprite, whether the MC hit reactions took over its hits, how many hits it took, how
it ended (cheat on vs off), and anything suspicious:
  CRASH    the game exited abnormally or timed out
  FLOOR    the sprite is inside the floor or walls of Yoshi's House while in its normal state (y > 372, x < 0, x > 400)
  STUCK    still knocked about after the final 150-frame wait
  LOST     the sprite vanished while alive (normal state, life left) when it is still there with the cheat off
  DIED     Mario died (with the cheat off he did not)
Usage: python -I mchits_report.py work\\pilot\\mchits_sweep\\results_*.json
"""
import json, sys

rows = {}
for p in sys.argv[1:]:
    rows.update(json.load(open(p)))


def mine(snap, sid):
    return [s for s in snap['sprites'] if s['id'] == sid]


flagged = 0
print('id  taken  hits  life_end  end_on        end_off       flags')
for sid in sorted(rows):
    on, off = rows[sid]['on'], rows[sid]['off']
    flags = []
    if on['rc'] != 0 or off['rc'] != 0:
        flags.append('CRASH(%s/%s)' % (on['rc'], off['rc']))
    lives = [s['life'] for snap in on['snaps'] for s in mine(snap, sid) if s['life'] is not None]
    taken = bool(lives)
    hits = sum(1 for a, b in zip([10.0] + lives, lives) if b < a)
    def odd(s):
        return s['status'] == 8 and (s['y'] > 372 or s['x'] < 0 or s['x'] > 400)
    # (only where the cheat-off run doesn't show the same: some sprites sit in the floor of Yoshi's House anyway)
    off_odd = any(odd(s) for snap in off['snaps'] for s in mine(snap, sid))
    for snap in on['snaps']:
        hit = [s for s in mine(snap, sid) if s['life'] is not None and odd(s)]
        if hit and not off_odd:
            flags.append('FLOOR(%d,%d@%d)' % (hit[0]['x'], hit[0]['y'], snap['frame']))
            break
    if on['snaps'] and any(s['knocked'] for s in mine(on['snaps'][-1], sid)):
        flags.append('STUCK')
    # vanished while alive: a snapshot with the sprite knocked/alive in state 8, the next one without it, while the
    # cheat-off run still has it at that point
    for a, b in zip(on['snaps'], on['snaps'][1:]):
        alive = [s for s in mine(a, sid) if s['status'] == 8 and s['life'] is not None and s['life'] > 0]
        if alive and not mine(b, sid):
            idx = on['snaps'].index(b)
            if idx < len(off['snaps']) and mine(off['snaps'][idx], sid):
                flags.append('LOST@%d' % b['frame'])
            break
    if on['mc_died'] and not off['mc_died']:
        flags.append('DIED')

    def end(r):
        last = [s for snap in r['snaps'] for s in mine(snap, sid)]
        return ('%d@%d,%d' % (last[-1]['status'], last[-1]['x'], last[-1]['y'])) if last else 'gone'
    if flags:
        flagged += 1
    print('%s  %-5s  %4d  %8s  %-12s  %-12s  %s' % (sid, 'yes' if taken else '-', hits,
                                                   ('%.1f' % lives[-1]) if lives else '-', end(on), end(off),
                                                   ' '.join(flags)))
print('%d sprites, %d flagged, %d taken over by the cheat' % (
    len(rows), flagged, sum(1 for sid in rows if any(s['life'] is not None for snap in rows[sid]['on']['snaps']
                                                     for s in mine(snap, sid)))))
