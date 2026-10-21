"""The hits Mario takes in a tas.py session's kept moves (round 19): replays the session (on a copy, `<session>_chk`) from a
checkpoint, a few move lines at a time with no frames of its own, and prints every drop in MC life (frame, life
before and after), SMW Mario's power-downs and lost lives (round 20) and every refill; at the end the hits and the frames.
Usage: python -I tas_hits.py <session> <checkpoint> [--to <checkpoint>]
"""
import contextlib, io, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tas  # noqa: E402

name, cp = sys.argv[1], sys.argv[2]
s = tas.load(name)
start = s['checkpoints'][cp]
end = s['checkpoints'][sys.argv[sys.argv.index('--to') + 1]] if '--to' in sys.argv else len(s['moves'])
rest = s['moves'][start:end]
chk = name + '_chk'
c = {'base': s['base'], 'moves': s['moves'][:start], 'checkpoints': {}, 'step': 0}
os.makedirs(tas.sdir(chk), exist_ok=True)
tas.launch(chk, c)


def send(moves):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        tas.send(chk, c, moves, shot=False)
    return json.loads(re.search(r'STATUS (\{.*\})', buf.getvalue()).group(1))


st = send([])
life, hits, f0 = st.get('mc_life'), 0, st.get('frame')
power, lives = st.get('power_up', 0), st.get('lives', 0)
i = 0
while i < len(rest):
    chunk = []
    while i < len(rest) and len(chunk) < 4:
        chunk.append(rest[i])
        i += 1
    st = send(chunk)
    nl = st.get('mc_life')
    if nl < life - 0.01:
        hits += 1
        print('frame %d (move %d): hit, life %.2f -> %.2f' % (st['frame'], start + i, life, nl))
    elif nl > life + 0.01:
        print('frame %d: life back %.2f -> %.2f' % (st['frame'], life, nl))
    life = nl
    pw, dl = st.get('power_up', 0), st.get('lives', 0)
    if not st.get('mc_mario') and pw < power:  # (SMW Mario: a power-down is his hit)
        hits += 1
        print('frame %d (move %d): hit, power %d -> %d' % (st['frame'], start + i, power, pw))
    if dl < lives:
        print('frame %d (move %d): a life lost (%d -> %d)' % (st['frame'], start + i, lives, dl))
    power, lives = pw, dl
tas.kill(c)
print('%d hits in %d frames (%s)' % (hits, st['frame'] - f0, st.get('rush')))
