"""Search one stretch of a run (round 19, the speedrun videos): from a tas.py checkpoint, try a move template over a
grid of parameters and keep the first try that gets through: Mario reaches the target (x at least X, or a new room /
game mode) with no hit and no death, standing on the ground at the end. The kept moves stay in the session.
Usage: python -I tas_search.py <session> <checkpoint> "<template>" <param>=<a>:<b>:<step> ... [--x X] [--mode MM]
       [--room] [--frames-ok]
  template: tas.py moves with {param} fields, e.g. "wait {w}; hold RIGHT X B; wait {j}; hold RIGHT X; wait 30"
  --x X       success when Mario's x >= X (default: any progress is not enough, give a target)
  --left X    success when Mario's x <= X
  --room      success when the room changes (x jumps back / the level number changes)
  --dead ID N success needs at least N sprites of that id knocked out ($151C set, e.g. Reznors A9)
  --air-ok    Mario need not be on the ground at the end
Prints each try and the kept one.
"""
import contextlib, io, itertools, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tas  # noqa: E402

name, cp, tmpl = sys.argv[1], sys.argv[2], sys.argv[3]
params, opts = {}, {}
i = 4
while i < len(sys.argv):
    a = sys.argv[i]
    if a.startswith('--'):
        if a in ('--room', '--air-ok'):
            opts[a] = True
            i += 1
        elif a in ('--dead', '--spr'):
            # --dead <id> <n>: at least n sprites of that id knocked out ($151C set: Reznors);
            # --spr <id> <state>: a sprite of that id in that state (9 stunned...) or gone from near Mario
            opts[a] = (sys.argv[i + 1].upper(), int(sys.argv[i + 2]))
            i += 3
        else:
            opts[a] = int(sys.argv[i + 1])
            i += 2
        continue
    k, rng = a.split('=')
    lo, hi, st = (int(x) for x in rng.split(':'))
    params[k] = list(range(lo, hi + 1, st))
    i += 1


def run(moves):
    s = tas.load(name)
    n = s['checkpoints'][cp]
    tas.kill(s)
    s['moves'] = s['moves'][:n]
    s['checkpoints'] = {k: v for k, v in s['checkpoints'].items() if v <= n}
    tas.launch(name, s)
    with contextlib.redirect_stdout(io.StringIO()):
        tas.send(name, s, [])
    s = tas.load(name)
    st0 = tas_status(name)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        tas.send(name, s, moves)
    m = re.search(r'STATUS (\{.*\})', buf.getvalue())
    return st0, json.loads(m.group(1)) if m else {}


def tas_status(n):
    try:
        return json.load(open(os.path.join(tas.sdir(n), 'out', 'status.json')))
    except (OSError, ValueError):
        return {}


keys = list(params)
for combo in itertools.product(*(params[k] for k in keys)):
    vals = dict(zip(keys, combo))
    moves = [m.strip() for m in tmpl.format(**vals).split(';') if m.strip()]
    st0, st = run(moves)
    hurt = st.get('mc_life', 10) < st0.get('mc_life', 10) - 0.01 or st.get('hurt_timer', 0) > 0
    dead = st.get('player_state') == 9 or st.get('lives', 0) < st0.get('lives', 0)
    reach = True
    if '--x' in opts:
        reach = st.get('x', 0) >= opts['--x']
    if '--left' in opts:
        reach = st.get('x', 9999) <= opts['--left']
    if '--room' in opts:
        reach = st.get('frame', 0) and (abs(st.get('x', 0) - st0.get('x', 0)) > 600 or st.get('game_mode') != '14' or
                                        st.get('y', 0) - st0.get('y', 0) > 300)
    if '--ymin' in opts:  # ends this low or lower (on the floor, not on a platform)
        reach = reach and st.get('y', 0) >= opts['--ymin']
    if '--spr' in opts:
        sid, want = opts['--spr']
        same = [p for p in st.get('sprites_near', []) if p['id'] == sid]
        reach = reach and (not same or any(p['st'] == want for p in same))
    if '--dead' in opts:
        sid, need = opts['--dead']
        reach = reach and sum(1 for p in st.get('sprites_near', []) if p['id'] == sid and p['t151c']) >= need
    ok = reach and not hurt and not dead and (st.get('on_ground', False) or '--air-ok' in opts)
    print('%s -> x %s y %s life %s hurt %s dead %s ground %s %s' % (vals, st.get('x'), st.get('y'), st.get('mc_life'),
                                                                   hurt, dead, st.get('on_ground'), 'OK' if ok else ''),
          flush=True)
    if ok:
        print('KEPT %s' % vals)
        sys.exit(0)
print('NOTHING WORKED')
sys.exit(1)
