"""Building a run move by move (round 19: the speedrun videos), on the follow-mode pilot as play.py does, but with its own
start (a base script, no cheats) and the pilot's scripted helpers allowed as moves.

  tas.py start <session> <base.txt>      start the game on the base script (its lines run first; a line "#REC" marks
                                         where a recording starts)
  tas.py do <session> "<move>; <move>" ...  [--every N]
        moves: hold <buttons> | wait <n> | attack <id|boss> <reach> [air] [delay] | waitmode <hex> [max] |
               waitnear <dx> [id] | menu | menukey <key>
  tas.py checkpoint <session> <name> / rewind <session> <name|start> / stop <session>
  tas.py script <session> <out.txt>      the whole run as one pilot script (base + moves)
  tas.py record <session> <dir> <name>   the run played once more from the start with `record <name>` and
                                         `audio <name>` from the #REC line on (then make_video.py)
Every `do` prints STATUS (the pilot's status.json) and NOW (the screen after the moves). Rewinding replays the kept
moves from the start (deterministic: the same frames as the first time).
Sessions live in work/tas/<session>/.
"""
import json, os, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = os.path.join(ROOT, 'games', 'SuperMarioWorld')
SESS = os.path.join(ROOT, 'work', 'tas')
ALLOWED = ('hold', 'wait', 'attack', 'waitmode', 'waitnear', 'menu', 'menukey', 'sprites')  # (sprites: prints only)


def die(msg):
    print('ERROR: ' + msg)
    sys.exit(1)


def sdir(name):
    return os.path.join(SESS, name)


def load(name):
    p = os.path.join(sdir(name), 'session.json')
    if not os.path.exists(p):
        die('no session ' + name)
    return json.load(open(p))


def save(name, s):
    json.dump(s, open(os.path.join(sdir(name), 'session.json'), 'w'), indent=1)


def alive(s):
    pid = s.get('pid')
    if not pid:
        return False
    r = subprocess.run(['tasklist', '/FI', 'PID eq %d' % pid, '/NH'], capture_output=True, text=True)
    return str(pid) in r.stdout


def kill(s):
    if s.get('pid') and alive(s):
        subprocess.run(['taskkill', '/PID', str(s['pid']), '/F'], capture_output=True)
    s['pid'] = None


def kill_orphans(name):
    """Any game still running this session's script (a launch whose process id was lost): two games writing the same
    status and ack files make every reading wrong."""
    script = os.path.join(sdir(name), 'script.txt')
    ps = ("Get-CimInstance Win32_Process -Filter \"Name='smw.exe'\" | Where-Object { $_.CommandLine -like '*%s*' } | "
          "ForEach-Object { Stop-Process -Id $_.ProcessId -Force }" % script.replace('\\', '\\\\').replace("'", "''"))
    subprocess.run(['powershell', '-NoProfile', '-Command', ps.replace('\\\\', '\\')], capture_output=True)


def launch(name, s):
    kill_orphans(name)
    d = sdir(name)
    out = os.path.join(d, 'out')
    os.makedirs(out, exist_ok=True)
    for f in ('ack.txt', 'status.json'):
        if os.path.exists(os.path.join(out, f)):
            os.remove(os.path.join(out, f))
    lines = [l for l in s['base'] if l != '#REC'] + s['moves'] + ['follow 60']
    open(os.path.join(d, 'script.txt'), 'w').write('\n'.join(lines) + '\n')
    s['ack'] = 0
    err = open(os.path.join(d, 'game_stderr.txt'), 'w')
    p = subprocess.Popen([os.path.join(GAME, 'smw.exe'), '--pilot', os.path.join(d, 'script.txt'), '--out', out],
                         cwd=GAME, stdout=open(os.path.join(d, 'game_stdout.txt'), 'w'), stderr=err,
                         creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    s['pid'] = p.pid


def send(name, s, moves, every=0, shot=True):
    d = sdir(name)
    s['step'] = s.get('step', 0) + 1
    step = s['step']
    extra, shots = [], []
    for m in moves:
        if every and m.startswith('wait '):
            n = int(m.split()[1])
            while n > 0:
                k = min(n, every)
                n -= k
                if k > 1:
                    extra.append('wait %d' % (k - 1))
                nm = 's%03d_%03d' % (step, len(shots))
                extra.append('shot ' + nm)  # (a shot runs one frame)
                shots.append(nm)
        else:
            extra.append(m)
    final = 's%03d_end' % step
    s['ack'] += 1
    tail = (['shot ' + final] if shot else []) + ['status', 'ack %d' % s['ack']]  # (shot=False: no frame of its own)
    with open(os.path.join(d, 'script.txt'), 'a') as f:
        f.write('\n'.join(extra + tail) + '\n')
    s['moves'] += ['wait 1' if m.startswith('shot') else m for m in extra + tail if not m.startswith(('status', 'ack'))]
    ackf = os.path.join(d, 'out', 'ack.txt')
    t0 = time.time()
    while True:
        try:
            if int(open(ackf).read().split()[0]) >= s['ack']:
                break
        except (OSError, ValueError, IndexError):
            pass
        if not alive(s):
            die('the game stopped. stderr: ' + open(os.path.join(d, 'game_stderr.txt')).read()[-800:])
        if time.time() - t0 > 900:
            die('no answer within 900 s')
        time.sleep(0.03)
    save(name, s)
    out = os.path.join(d, 'out')
    st = {}
    for _ in range(40):
        try:
            st = json.load(open(os.path.join(out, 'status.json')))
            break
        except (OSError, ValueError):
            time.sleep(0.05)
    print('STATUS ' + json.dumps(st))
    from PIL import Image
    paths = [os.path.join(out, x + '.bmp') for x in shots + [final]]
    imgs = [Image.open(p).convert('RGB') for p in paths if os.path.exists(p)]
    if len(imgs) > 1:
        cols = min(6, len(imgs))
        rows = (len(imgs) + cols - 1) // cols
        strip = Image.new('RGB', (256 * cols, 224 * rows))
        for i, im in enumerate(imgs):
            strip.paste(im, ((i % cols) * 256, (i // cols) * 224))
        sp = os.path.join(out, final + '_strip.png')
        strip.save(sp)
        print('STRIP: ' + sp)
    if imgs:
        png = os.path.join(out, final + '.png')
        imgs[-1].save(png)
        print('NOW: ' + png)


def parse(args):
    moves, every = [], 0
    i = 0
    while i < len(args):
        if args[i] == '--every':
            every = int(args[i + 1])
            i += 2
            continue
        for part in args[i].split(';'):
            part = part.strip()
            if not part:
                continue
            if part.split()[0] not in ALLOWED:
                die('not a move: ' + part)
            moves.append(part)
        i += 1
    return moves, every


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'start':
        name, base = sys.argv[2], sys.argv[3]
        d = sdir(name)
        if os.path.exists(os.path.join(d, 'session.json')):
            kill(load(name))
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d, exist_ok=True)
        lines = [l.rstrip() for l in open(base) if l.strip() and (not l.startswith('#') or l.startswith('#REC'))]
        s = {'base': lines, 'moves': [], 'checkpoints': {}, 'step': 0}
        launch(name, s)
        send(name, s, [])
    elif cmd == 'do':
        name = sys.argv[2]
        s = load(name)
        if not alive(s):
            launch(name, s)
        moves, every = parse(sys.argv[3:])
        send(name, s, moves, every)
    elif cmd == 'checkpoint':
        name, cp = sys.argv[2], sys.argv[3]
        s = load(name)
        s['checkpoints'][cp] = len(s['moves'])
        save(name, s)
        print('checkpoint %s at move %d' % (cp, len(s['moves'])))
    elif cmd == 'rewind':
        name, cp = sys.argv[2], sys.argv[3]
        s = load(name)
        n = 0 if cp == 'start' else s['checkpoints'][cp]
        kill(s)
        s['moves'] = s['moves'][:n]
        s['checkpoints'] = {k: v for k, v in s['checkpoints'].items() if v <= n}
        launch(name, s)
        send(name, s, [])
    elif cmd == 'stop':
        s = load(sys.argv[2])
        kill(s)
        save(sys.argv[2], s)
    elif cmd == 'script':
        s = load(sys.argv[2])
        open(sys.argv[3], 'w').write('\n'.join([l for l in s['base'] if l != '#REC'] + s['moves'] + ['quit']) + '\n')
    elif cmd == 'record':
        name, out, vid = sys.argv[2], os.path.abspath(sys.argv[3]), sys.argv[4]
        s = load(name)
        base = [('record %s\naudio %s' % (vid, vid)) if l == '#REC' else l for l in s['base']]
        lines = base + s['moves'] + ['record off', 'audio off', 'quit']
        os.makedirs(out, exist_ok=True)
        p = os.path.join(out, vid + '_script.txt')
        open(p, 'w').write('\n'.join(lines) + '\n')
        r = subprocess.run([os.path.join(GAME, 'smw.exe'), '--pilot', p, '--out', out], cwd=GAME,
                           capture_output=True, text=True)
        print(r.stdout[-400:])
    else:
        print(__doc__)


if __name__ == '__main__':
    main()
