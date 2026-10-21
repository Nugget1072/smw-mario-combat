"""MC punches on bosses: Mario Combat Mario is warped into each boss room (from inside Yoshi's Island 1, save state 9)
and walks up to the boss and punches it again and again (pilot `attack`), with No damage and Infinite lives on. The
boss is beaten when SMW starts its end-of-level cutscene ($13C6 boss end cutscene / $1493 end-level timer) or, for
Bowser, when the ending starts (game mode 1D and up).
Each round's punch count (pilot `bosspunches`: punches that hurt the boss) tells punches from stomps; --ground punches
from the ground only (no jump kick that could come down as a stomp).
Usage: python -I boss_punch_test.py <root> [names...] [--rounds N] [--ground]
Writes work/pilot/boss_punch/<name>/ (script, screenshots every few rounds, out.txt) and prints one line per boss.
"""
import os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

root = sys.argv[1]
rounds = int(sys.argv[sys.argv.index('--rounds') + 1]) if '--rounds' in sys.argv else 60
ground_only = '--ground' in sys.argv
game = os.path.join(root, 'games', 'SuperMarioWorld')
out_root = os.path.join(root, 'work', 'pilot', 'boss_punch')

# name: (room, sprite id) -- rooms read from the sprite data (pilot `bossrooms`) and the map levels' exits
BOSSES = {
    'iggy': ('1F6', '29'), 'morton': ('0E5', '29'), 'bigboo': ('0E4', 'C5'), 'reznor_vanilla': ('0DF', 'A9'),
    'lemmy': ('1F2', '29'), 'ludwig': ('0D9', '29'), 'reznor_forest': ('0D5', 'A9'), 'roy': ('0CC', '29'),
    'reznor_choco': ('0E2', 'A9'), 'wendy': ('0D3', '29'), 'reznor_valley': ('1DE', 'A9'), 'larry': ('1EB', '29'),
    'bowser': ('1C7', 'A0'),
}
args = [a for a in sys.argv[2:] if a in BOSSES]


def run(name):
    room, sid = BOSSES[name]
    d = os.path.join(out_root, name)
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    s = ['loadstate 9', 'wait 5', 'cheat 1 1', 'cheat 3 1', 'warp %s' % room, 'wait 200', 'shot r000', 'sprites']
    for n in range(1, rounds + 1):
        # every third try a jump kick (bosses up on walls...); Bowser's clown car only a jump reaches: kick late in the rise
        # (--ground: punches from the ground only, no jump that could land as a stomp)
        air = ' air 8' if name == 'bowser' else ' air' if n % 3 == 0 and not ground_only else ''
        s += ['attack %s 40%s' % (sid, air), 'wait 14', 'sprites', 'peek 13C6', 'peek 14B5', 'whereami',
              'bosspunches']
        if n % 6 == 0:
            s += ['shot r%03d' % n]
    s += ['wait 300', 'shot end', 'peek 13C6', 'whereami', 'quit']
    p = os.path.join(d, 'script.txt')
    open(p, 'w').write('\n'.join(s) + '\n')
    try:
        r = subprocess.run([os.path.join(game, 'smw.exe'), '--pilot', p, '--out', d], cwd=game, capture_output=True,
                           text=True, timeout=900)
        rc, out = r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        rc, out = 'timeout', ''
    open(os.path.join(d, 'out.txt'), 'w').write(out)
    lines = out.splitlines()
    hits = len([l for l in lines if l.startswith('attack:')])
    modes = sorted(set(re.findall(r'whereami: mode (\w\w)', out)))
    died = bool(re.search(r'whereami: .* state 9 ', out))
    cut = any(l.startswith('peek 013C6') and not l.endswith(' 00') for l in lines)
    # beaten: the boss's end-of-level cutscene, the castle cutscene or the map with Mario alive, or the ending
    beaten = not died and (cut or any(m in ('0E', '1A', '1B') or int(m, 16) >= 0x1D for m in modes))
    bowser_hits = len([l for l in lines if l.startswith('peek 014B5') and not l.endswith(' 00')])
    # punches that hurt the boss (the boss's own hit, as a stomp / hit from below / Mechakoopa gives it), and the round
    # when he was beaten
    punches = max([int(x) for x in re.findall(r'bosspunches (\d+)', out)] or [0])
    return name, rc, hits, beaten, modes, bowser_hits, punches


names = args or list(BOSSES)
with ThreadPoolExecutor(4) as ex:
    for name, rc, hits, beaten, modes, bh, punches in ex.map(run, names):
        extra = ' bowser hurt in %d checks' % bh if name == 'bowser' else ''
        print('%-15s rc=%s attacks=%d punches that hurt %d beaten=%s modes=%s%s' % (
            name, rc, hits, punches, beaten, ' '.join(modes), extra), flush=True)
