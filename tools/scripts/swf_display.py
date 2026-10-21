"""Print the per-frame display list of SWF sprites (from timelines.json made by swf_tags.py).

Usage: python -I swf_display.py <timelines.json> <sprite_id> [max_frames]
Each line: frame: depth=id(name)@(tx,ty sx,sy) ...
"""
import json, sys

d = json.load(open(sys.argv[1]))
sid = sys.argv[2]
maxf = int(sys.argv[3]) if len(sys.argv) > 3 else 10**9
t = d[sid]
by_frame = {}
for p in t["places"]:
    by_frame.setdefault(p["frame"], []).append(p)
disp = {}
print(f"sprite {sid}: {t['frames']} frames, labels {t['labels']}, actions at {t['action_frames']}")
inv = {v: k for k, v in t["labels"].items()}
for f in range(1, min(t["frames"], maxf) + 1):
    for p in by_frame.get(f, []):
        if "remove_depth" in p:
            disp.pop(p["remove_depth"], None)
            continue
        cur = disp.get(p["depth"], {}) if p["move"] else {}
        if p["id"] is not None:
            cur = {"id": p["id"], "name": p["name"], "matrix": cur.get("matrix")} if not p["move"] or cur.get("id") != p["id"] else cur
        if p["matrix"]:
            cur["matrix"] = p["matrix"]
        if p["name"]:
            cur["name"] = p["name"]
        disp[p["depth"]] = cur
    parts = []
    for dep in sorted(disp):
        c = disp[dep]
        m = c.get("matrix") or [1, 0, 0, 1, 0, 0]
        parts.append(f"{dep}={c.get('id')}({c.get('name') or ''})@({m[4]:.1f},{m[5]:.1f} s{m[0]:.2f},{m[3]:.2f})")
    print(f"{f:3d}{' [' + inv[f] + ']' if f in inv else ''}: " + " ".join(parts))
