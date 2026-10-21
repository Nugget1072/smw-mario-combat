"""Transition probe: what a room inherits from the room before it.

For each boss room: a RAM dump and screenshots when the room has just started (game mode 14), two ways:
  baseline  warped into from Yoshi's Island 1 (save state 9): the room comes after an ordinary level, as in normal play
  rush      reached as the next fight of a boss rush (EVERY BOSS; each earlier fight won with `poke 13C6 1` after
            150 frames of its own play, so the earlier rooms' code has run)
Then the RAM bytes that differ, named from variables.h, with the bytes that differ between two baseline runs (frame
counters, random numbers...) left out.
Usage: python -I transition_probe.py <root> [--names]
Writes work/pilot/transition_probe/{base,base2,rush}/ (*.ram, shots) and work/pilot/transition_probe/diff_<room>.txt.
"""
import os, re, subprocess, sys

root = sys.argv[1]
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'transition_probe')
ROOMS = ['1F6', '0E4', '0E5', '0DF', '1F2', '0D9', '0D5', '0CC', '0E2', '0D3', '1DE', '1EB', '1C7']


def names():
    """address -> variable name (scalars and arrays from variables.h)"""
    m = {}
    for line in open(os.path.join(game, 'src', 'variables.h')):
        r = re.match(r'#define (\w+) \(?\*?\(\(?(uint8|uint16|int8|int16)\*?\)\(g_ram\+0x([0-9A-Fa-f]+)\)\)?', line)
        if r:
            a = int(r.group(3), 16)
            size = 2 if '16' in r.group(2) else 1
            arr = '*)(g_ram' in line and line.count('*') == 1
            for i in range(12 if arr else size):
                m.setdefault(a + i, r.group(1) + ('[%d]' % i if arr else '' if i == 0 else '+1'))
    return m


def run(kind, script):
    d = os.path.join(out_root, kind)
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(script + ['quit']) + '\n')
    r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                       text=True, timeout=1800)
    open(os.path.join(d, 'out.txt'), 'w').write(r.stdout)
    return d


def at_start(tag):
    return ['waitmode 14', 'ramdump %s' % tag, 'wait 30', 'shot %s_a' % tag, 'wait 90', 'shot %s_b' % tag,
            'wait 120', 'shot %s_c' % tag]


base = ['loadstate 9', 'wait 5', 'cheat 1 1']
script_base = []
for r in ROOMS:
    script_base += base + ['warp %s' % r, 'waitmode 0F 60', 'waitmode 13 300'] + at_start(r)
script_rush = ['loadstate 9', 'wait 5', 'cheat 1 1', 'kill', 'wait 400', 'bossrush all', 'waitmode 13']
for r in ROOMS:
    script_rush += at_start(r) + ['poke 13C6 1', 'waitmode 0F 400', 'waitmode 13 300']

dirs = {k: run(k, s) for k, s in (('base', script_base), ('base2', script_base), ('rush', script_rush))}
nm = names()
for r in ROOMS:
    ram = {k: open(os.path.join(d, '%s.ram' % r), 'rb').read() for k, d in dirs.items()
           if os.path.exists(os.path.join(d, '%s.ram' % r))}
    if len(ram) < 3:
        print(r, 'missing dumps', list(ram))
        continue
    noise = {a for a in range(0x2000) if ram['base'][a] != ram['base2'][a]}
    diff = [a for a in range(0x2000) if ram['base'][a] != ram['rush'][a] and a not in noise]
    lines = ['%04X %-48s base %02X rush %02X' % (a, nm.get(a, '?'), ram['base'][a], ram['rush'][a]) for a in diff]
    open(os.path.join(out_root, 'diff_%s.txt' % r), 'w').write('\n'.join(lines) + '\n')
    print(r, '%d bytes differ (%d noisy left out)' % (len(diff), len(noise)))
