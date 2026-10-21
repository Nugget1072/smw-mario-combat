"""Level catalogue for the level select: which of SMW's 512 level numbers are the map's levels, which rooms they reach
(screen exits, secondary exits), and which hold a level nobody can reach (unused). Input: the output of a pilot run
with `levelscan` (on the overworld, e.g. `loadstate 9`, `kill`, `wait 400`, `levelscan`) saved to a text file.

Rules (vanilla SMW, no Lunar Magic):
- map levels: translevels 01-24 are levels 001-024, 25-5F are 101-13B; the bank (0x000 / 0x100) of every room
  reached from a level is the bank of that level (LoadLevel: `ow_players_map != 0`).
- a screen exit goes to bank | exit; if the level's exits are secondary (the flag in the exit object), to
  bank | secondary-exit table[bank | exit] (kLevelInfo_05F800).
- rooms the game loads by itself: the 1-UP bonus game and Yoshi's wings area (kLoadLevel_BonusLevelSublevelsLo).
- what is left: "empty" when its data is the filler shared by many slots (or almost nothing), else "unused" (the
  "same data as" list shows copies; a copy of a used room is still an unused slot).
Usage: python -I level_catalogue.py <levelscan.txt> [more translevels to treat as map levels, hex]
"""
import collections, re, sys

text = open(sys.argv[1]).read()
LV = re.compile(r'level ([0-9A-F]{3}) l1 (\d+) (\w+) l2 (\d+) (\w+) spr (\d+) (\w+) hdr (\w\w) (\w\w) (\w\w) (\w\w) '
                r'tileset (-?\d+) sec (\w\w) (\w\w) (\w\w) exits(.*)')
lv = {}
for m in LV.finditer(text):
    n = int(m.group(1), 16)
    exits = [tuple(e.split(':')) for e in m.group(16).split()]
    lv[n] = dict(l1=int(m.group(2)), h1=m.group(3), l2=int(m.group(4)), h2=m.group(5), spr=int(m.group(6)),
                 hs=m.group(7), tileset=int(m.group(12)), sec=int(m.group(13), 16),
                 exits=[(int(s), int(e, 16), int(hb), int(sc)) for s, e, hb, sc in exits])
maplevels = [int(t.split(':')[0], 16) for t in re.search(r'maplevels(.*)', text).group(1).split()]
maplevels += [int(a, 16) for a in sys.argv[2:]]


def tl2lv(t):
    return t if t < 0x25 else t + 0xDC


reach = {}  # level -> how it is reached (first found)
queue = collections.deque()
for t in maplevels:
    n = tl2lv(t)
    reach[n] = 'map %02X' % t
    queue.append(n)
while queue:
    n = queue.popleft()
    bank = n & 0x100
    d = lv.get(n)
    if not d:
        continue
    for s, e, hb, sc in d['exits']:
        dest = bank | (lv[bank | e]['sec'] if sc else e)
        if dest not in reach:
            reach[dest] = 'exit of %03X' % n
            queue.append(dest)

# rooms the game loads by itself (not through exits): the 1-UP bonus game and Yoshi's wings area
# (kLoadLevel_BonusLevelSublevelsLo 00 / C8, in either bank), the intro (misc_intro_level_flag E9 -> C5) and the title
# screen's level (EB -> C7)
SPECIAL = {0x000: 'bonus game', 0x100: 'bonus game', 0x0C8: 'wings area', 0x1C8: 'wings area', 0x0C5: 'intro',
           0x0C7: 'title screen'}
counts = collections.Counter(d['h1'] for d in lv.values())
test_level = counts.most_common(1)[0][0]  # the filler test level (99 bytes of objects) in most empty slots
key = lambda n: (lv[n]['h1'], lv[n]['h2'], lv[n]['hs'])  # a level's whole content: layer 1, layer 2, sprites
rows = []
for n in range(0x200):
    d = lv[n]
    if n in reach:
        kind = 'map' if reach[n].startswith('map') else 'room'
    elif n in SPECIAL:
        kind = 'special'
    elif d['h1'] == test_level:
        kind = 'test level'
    elif d['l1'] <= 6 and d['spr'] == 0:
        kind = 'empty'
    else:
        kind = 'unused'
    rows.append((n, kind, reach.get(n, ''), d))
for kind in ('map', 'room', 'special', 'test level', 'unused', 'empty'):
    sel = [r for r in rows if r[1] == kind]
    print('%s: %d' % (kind, len(sel)))
    if kind == 'special':
        print('  ' + ', '.join('%03X %s' % (n, SPECIAL[n]) for n, _, _, _ in sel))
    if kind == 'unused':
        for n, _, how, d in sel:
            same = [m for m in range(0x200) if m != n and key(m) == key(n)]
            used = [m for m in same if m in reach or m in SPECIAL]
            print('  %03X l1 %4d l2 %4d sprites %2d tileset %2d exits %s; same content: %s%s' %
                  (n, d['l1'], d['l2'], d['spr'], d['tileset'],
                   ['%02X%s' % (e[1], 's' if e[3] else '') for e in d['exits']],
                   ' '.join('%03X' % m for m in same) or '-', ' (a copy of used %s)' % ' '.join('%03X' % m for m in used) if used else ''))