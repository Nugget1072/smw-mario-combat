"""Turn-based play of SMW (as Mario Combat Mario) for agents and testers.

The game runs hidden in pilot "follow" mode: it is frozen until this script appends the next moves to its command
file, then runs exactly those frames and reports back a screenshot and a status JSON. Every session runs a private copy
of the game (work/play/game), so the project can be rebuilt meanwhile (run `setup` again to pick up a new build).

Usage (from the project root, with tools\\Python3.14.8\\python.exe -I):
  play.py setup                                  copy the current build into work/play/game
  play.py levels                                 the level list (tools/play_levels.json)
  play.py start <session> <level> [--smw]        start a level (3-digit level number, e.g. 105) from its beginning
  play.py do <session> "<move>" ["<move>" ...] [--every N]
        moves: "hold <buttons>" (B A Y X L R UP DOWN LEFT RIGHT START SELECT; "hold" alone releases all) and
        "wait <frames>" (60 frames = 1 second; at most 900 frames per call). The held buttons stay held through the
        waits that follow. --every N also takes a screenshot every N frames (a strip image).
  play.py checkpoint <session> <name>           remember this moment (replayable)
  play.py rewind <session> <name|start>         go back to a checkpoint (the moves since are dropped)
  play.py stop <session>
Output: the status (JSON) and the screenshot path(s). Sessions live in work/play/sessions/<session>/.
Buttons: B = jump, A = spin jump, Y = punch (Mario Combat), X = run / grab / fireball / cape spin.
"""
import json, os, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME_SRC = os.path.join(ROOT, 'games', 'SuperMarioWorld')
# The game copy sessions launch: setup makes a new folder each time (a running session's exe can't be replaced), and
# work/play/game_current.txt names the newest one.
CURRENT = os.path.join(ROOT, 'work', 'play', 'game_current.txt')
GAME = os.path.join(ROOT, 'work', 'play', open(CURRENT).read().strip() if os.path.exists(CURRENT) else 'game')
SESS = os.path.join(ROOT, 'work', 'play', 'sessions')
LEVELS = os.path.join(ROOT, 'tools', 'play_levels.json')
BUTTONS = {'B', 'A', 'Y', 'X', 'L', 'R', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'START', 'SELECT'}
MAX_FRAMES = 900


def die(msg):
    print('ERROR: ' + msg)
    sys.exit(1)


def setup():
    global GAME
    GAME = os.path.join(ROOT, 'work', 'play', 'game_%d' % int(time.time()))
    os.makedirs(os.path.join(GAME, 'saves'), exist_ok=True)
    for f in ('smw.exe', 'smw.ini', 'smw_assets.dat'):
        shutil.copy2(os.path.join(GAME_SRC, f), os.path.join(GAME, f))
    for f in ('save8.sav', 'save9.sav', 'smw.srm'):
        src = os.path.join(GAME_SRC, 'saves', f)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(GAME, 'saves', f))
    open(CURRENT, 'w').write(os.path.basename(GAME))
    print('game copied to ' + GAME)


def sdir(name):
    if not name.replace('_', '').replace('-', '').isalnum():
        die('session names: letters, digits, - and _ only')
    return os.path.join(SESS, name)


def load(name):
    p = os.path.join(sdir(name), 'session.json')
    if not os.path.exists(p):
        die('no session %s (use start)' % name)
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


def launch(name, s):
    """(Re)start the game: the base commands, every move so far (replayed at full speed), then follow mode."""
    d = sdir(name)
    out = os.path.join(d, 'out')
    os.makedirs(out, exist_ok=True)
    for f in ('ack.txt', 'status.json'):
        if os.path.exists(os.path.join(out, f)):
            os.remove(os.path.join(out, f))
    lines = s['base'] + s['moves'] + ['follow 40']
    open(os.path.join(d, 'script.txt'), 'w').write('\n'.join(lines) + '\n')
    s['ack'] = 0
    err = open(os.path.join(d, 'game_stderr.txt'), 'w')
    p = subprocess.Popen([os.path.join(GAME, 'smw.exe'), '--pilot', os.path.join(d, 'script.txt'), '--out', out],
                         cwd=GAME, stdout=subprocess.DEVNULL, stderr=err,
                         creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    s['pid'] = p.pid


def send(name, s, moves, every=0):
    """Append moves (+ screenshots, status, ack) and wait until the game has run them."""
    d = sdir(name)
    s['step'] = s.get('step', 0) + 1
    step = s['step']
    shots, extra, since = [], [], 0
    for m in moves:
        extra.append(m)
        if every and m.startswith('wait'):
            n = int(m.split()[1])
            # split long waits so a shot lands every `every` frames. A shot runs one frame itself, so it takes the
            # last frame of its stretch: the moves run the same number of frames with or without --every.
            extra.pop()
            while n > 0:
                k = min(n, every - since)
                n -= k
                since += k
                if since >= every:
                    if k > 1:
                        extra.append('wait %d' % (k - 1))
                    nm = 's%03d_%02d' % (step, len(shots))
                    extra.append('shot ' + nm)
                    shots.append(nm)
                    since = 0
                else:
                    extra.append('wait %d' % k)
    final = 's%03d_end' % step
    s['ack'] += 1
    ack = s['ack']
    tail = ['shot ' + final, 'status', 'ack %d' % ack]
    with open(os.path.join(d, 'script.txt'), 'a') as f:
        f.write('\n'.join(extra + tail) + '\n')
    # the replay log: exactly the frames the game ran (a screenshot takes one frame, like `wait 1`)
    s['moves'] += ['wait 1' if m.startswith('shot') else m for m in extra + tail if not m.startswith(('status', 'ack'))]
    t0 = time.time()
    ackf = os.path.join(d, 'out', 'ack.txt')
    while True:
        if os.path.exists(ackf):
            try:
                if int(open(ackf).read().split()[0]) >= ack:
                    break
            except (ValueError, IndexError, OSError):
                pass
        if not alive(s):
            err = open(os.path.join(d, 'game_stderr.txt')).read()[-1500:]
            die('the game stopped (crash?). stderr tail:\n' + err)
        if time.time() - t0 > 180:
            die('no answer from the game within 180 s')
        time.sleep(0.05)
    save(name, s)
    report(name, shots + [final])


def report(name, shots):
    d = sdir(name)
    out = os.path.join(d, 'out')
    st = {}
    for _ in range(40):  # the game replaces status.json atomically, but a read can still meet the swap: retry
        try:
            st = json.load(open(os.path.join(out, 'status.json')))
            break
        except (OSError, ValueError):
            time.sleep(0.05)
    print('STATUS ' + json.dumps(st))
    paths = [os.path.join(out, x + '.bmp') for x in shots]
    paths = [p for p in paths if os.path.exists(p)]
    try:
        from PIL import Image
        imgs = [Image.open(p).convert('RGB') for p in paths]
        if len(imgs) > 1:
            cols = min(4, len(imgs))
            rows = (len(imgs) + cols - 1) // cols
            strip = Image.new('RGB', (256 * cols, 224 * rows))
            for i, im in enumerate(imgs):
                strip.paste(im, ((i % cols) * 256, (i // cols) * 224))
            sp = os.path.join(out, shots[-1] + '_strip.png')
            strip.save(sp)
            print('SHOTS (left to right, top to bottom; the last one is now): ' + sp)
        png = os.path.join(out, shots[-1] + '.png')
        imgs[-1].save(png)
        print('NOW: ' + png)
    except Exception as e:  # no PIL: the BMP paths
        print('SHOTS: ' + ' '.join(paths) + ' (%s)' % e)


def parse_moves(args):
    moves, every, total = [], 0, 0
    i = 0
    while i < len(args):
        a = args[i].strip()
        if a == '--every':
            every = int(args[i + 1])
            i += 2
            continue
        for part in a.split(';'):
            part = part.strip()
            if not part:
                continue
            w = part.split()
            if w[0] == 'hold':
                bad = [b for b in w[1:] if b.upper() not in BUTTONS]
                if bad:
                    die('unknown buttons %s (use %s)' % (bad, ' '.join(sorted(BUTTONS))))
                moves.append('hold ' + ' '.join(b.upper() for b in w[1:]))
            elif w[0] == 'wait' and len(w) == 2 and w[1].isdigit() and int(w[1]) > 0:
                moves.append('wait ' + w[1])
                total += int(w[1])
            else:
                die('moves are "hold <buttons>" and "wait <frames>" only, got: ' + part)
        i += 1
    if total > MAX_FRAMES:
        die('at most %d frames per call (got %d)' % (MAX_FRAMES, total))
    if every and every < 4:
        die('--every must be at least 4')
    return moves, every


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd = sys.argv[1]
    if cmd == 'setup':
        setup()
    elif cmd == 'levels':
        for lv in json.load(open(LEVELS)):
            print('%s  %s' % (lv['level'], lv['name']))
    elif cmd == 'start':
        name, level = sys.argv[2], sys.argv[3].upper()
        if not os.path.exists(os.path.join(GAME, 'smw.exe')):
            setup()
        d = sdir(name)
        if os.path.exists(os.path.join(d, 'session.json')):
            kill(load(name))
        for _ in range(50):  # a killed game may hold its files for a moment
            shutil.rmtree(d, ignore_errors=True)
            if not os.path.exists(d):
                break
            time.sleep(0.1)
        os.makedirs(d, exist_ok=True)
        base = ['loadstate 9', 'wait 5', 'cheat 3 1']  # Yoshi's Island 1 snapshot; infinite lives only
        if '--smw' in sys.argv:
            base += ['toggle', 'wait 5']
        base += ['warp %s' % level, 'wait 90']
        s = {'level': level, 'base': base, 'moves': [], 'checkpoints': {}, 'step': 0}
        launch(name, s)
        send(name, s, [])
    elif cmd == 'do':
        name = sys.argv[2]
        s = load(name)
        if not alive(s):
            launch(name, s)  # the game had quit (idle too long): replay to where we were
        moves, every = parse_moves(sys.argv[3:])
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
        if cp != 'start' and cp not in s['checkpoints']:
            die('no checkpoint %s (have: %s)' % (cp, ', '.join(s['checkpoints']) or 'none'))
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
        print('stopped')
    else:
        print(__doc__)


if __name__ == '__main__':
    main()
