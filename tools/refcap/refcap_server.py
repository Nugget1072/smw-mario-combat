"""Reference capture: run the instrumented Mario Combat projector with a scripted input plan,
log the hero's per-tick state, and grab real screenshots of the game window at chosen ticks.

Usage: python -I refcap_server.py <instrumented.exe> <plan.json> <out_dir>
plan.json: {"level": 486, "settle": 40, "zoom": 0,
            "steps": [[t, "39", ""], [t2, "39+38", "shot_name"], ..., [tEnd, "END", ""]]}
  t = tick offset after `settle` ticks of the hero being active; keys are Flash key codes
  (37 Left, 38 Up/Jump, 39 Right, 40 Down, 65 A/Punch). "=" keeps the current keys.
Writes <out_dir>/trace.csv and <out_dir>/<shot_name>.png (client area of the projector window).
"""
import ctypes, ctypes.wintypes as wt, json, os, socket, subprocess, sys, time
from PIL import ImageGrab

FIELDS = ["tick", "x", "y", "xSpeed", "grav", "frame", "xscale", "onGround", "onLeftWall", "onRightWall", "walling",
          "fighting", "hurt", "skiding", "shortHop", "stopSliding", "jumpAnimThres", "pressingAttack", "life",
          "child", "childFrame", "attackFrame", "hurtFrame", "vcamScale", "keys"]
POLICY = (b'<?xml version="1.0"?><cross-domain-policy><allow-access-from domain="*" to-ports="*"/>'
          b'</cross-domain-policy>\0')
user32 = ctypes.windll.user32
ctypes.windll.shcore.SetProcessDpiAwareness(2)  # real pixel coordinates for window rects and grabs


def find_window(pid):
    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        p = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True
    user32.EnumWindows(cb, 0)
    return found[0] if found else None


def client_rect(hwnd):
    r = wt.RECT(); user32.GetClientRect(hwnd, ctypes.byref(r))
    pt = wt.POINT(0, 0); user32.ClientToScreen(hwnd, ctypes.byref(pt))
    return (pt.x, pt.y, pt.x + r.right, pt.y + r.bottom)


def main():
    exe, plan_path, out = sys.argv[1], sys.argv[2], sys.argv[3]
    plan = json.load(open(plan_path))
    os.makedirs(out, exist_ok=True)
    srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 8642)); srv.listen(4); srv.settimeout(60)
    proc = subprocess.Popen([exe], cwd=os.path.dirname(os.path.abspath(exe)))
    hwnd = None
    rows, shots = [], []
    try:
        while True:
            conn, _ = srv.accept()
            first = conn.recv(4096)
            if first.startswith(b"<policy-file-request/>"):
                conn.sendall(POLICY); conn.close(); continue
            break
        print(first.split(b"\0")[0].decode("latin1"))
        steps =";".join(f"{t},{k},{s}" for t, k, s in plan["steps"])
        conn.sendall(f"PLAN|{plan['level']}|{plan['settle']}|{plan.get('zoom', 0)}|{steps}".encode() + b"\0")
        for _ in range(100):
            hwnd = find_window(proc.pid)
            if hwnd:
                break
            time.sleep(0.05)
        if hwnd:  # keep the capture window on top in the top-left corner while the run lasts
            user32.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x0001 | 0x0010)
        conn.settimeout(120)
        buf = first[first.find(b"\0") + 1:] if b"\0" in first else b""
        done = False
        while not done:
            while b"\0" not in buf:
                chunk = conn.recv(65536)
                if not chunk:
                    done = True; break
                buf += chunk
            while b"\0" in buf:
                msg, buf = buf.split(b"\0", 1)
                m = msg.decode("latin1")
                if m.startswith("S|"):
                    rows.append(m[2:])
                elif m.startswith("SHOT|"):
                    _, name, tick = m.split("|")
                    time.sleep(0.08)
                    img = ImageGrab.grab(bbox=client_rect(hwnd), all_screens=True)
                    img.save(os.path.join(out, f"{name}.png"))
                    shots.append({"name": name, "tick": int(tick)})
                elif m.startswith("END|"):
                    done = True
    finally:
        proc.kill()
        srv.close()
    with open(os.path.join(out, "trace.csv"), "w") as f:
        f.write(",".join(FIELDS) + "\n")
        f.write("\n".join(rows) + "\n")
    json.dump({"plan": plan, "shots": shots, "ticks": len(rows)}, open(os.path.join(out, "run.json"), "w"), indent=1)
    print(f"{len(rows)} ticks, {len(shots)} shots -> {out}")


main()
