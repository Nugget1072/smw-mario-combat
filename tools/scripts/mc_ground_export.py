"""Export a Mario Combat level's `ground` clip as exact polygon edges for the C test harness.

Usage: python -I mc_ground_export.py <MarioCombat.swf> <root_frame> <out.bin> [out_preview.png]
Binary format: int32 count, then count x (int32 fill_key, float64 x0, y0, x1, y1) in global stage px.
A point is inside the ground if, for any fill_key, the even-odd crossing count of its edges is odd
(the same rule `hitTest(x, y, true)` uses on the shape fills).
"""
import os, struct, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from swflib import Swf  # noqa: E402
from swfraster import fill_segments  # noqa: E402
from mc_export import mat_mul, px_matrix  # noqa: E402


def display_at(tl, frame):
    cur = {}
    for f in range(1, frame + 1):
        for op in tl["ops"].get(f, []):
            if op["op"] == "remove":
                cur.pop(op["depth"], None)
            elif op["op"] == "place":
                prev = cur.get(op["depth"])
                if "id" in op and (not op["move"] or prev is None or op.get("id") != prev.get("id")):
                    newp = {"id": op["id"], "m": px_matrix(op.get("matrix")) if "matrix" in op else (prev["m"] if prev and op["move"] else [1, 0, 0, 1, 0, 0]), "name": op.get("name", prev.get("name") if prev and op["move"] else None)}
                    cur[op["depth"]] = newp
                elif prev is not None:
                    if "matrix" in op:
                        prev["m"] = px_matrix(op["matrix"])
                    if "name" in op:
                        prev["name"] = op["name"]
    return cur


def main():
    swf = Swf(sys.argv[1]); frame = int(sys.argv[2]); out = sys.argv[3]
    disp = display_at(swf.root, frame)
    ground = [v for v in disp.values() if v.get("name") == "ground"]
    assert ground, "no ground instance on that frame"
    g = ground[0]
    recs = []
    key = [0]

    def walk(cid, m):
        if cid in swf.sprites:
            for d, c in sorted(display_at(swf.sprites[cid], 1).items()):
                walk(c["id"], mat_mul(m, c["m"]))
        elif cid in swf.shapes:
            for gi, fi, segs in fill_segments(swf.shapes[cid]):
                key[0] += 1
                s = segs / 20.0
                X0 = m[0] * s[:, 0] + m[2] * s[:, 1] + m[4]; Y0 = m[1] * s[:, 0] + m[3] * s[:, 1] + m[5]
                X1 = m[0] * s[:, 2] + m[2] * s[:, 3] + m[4]; Y1 = m[1] * s[:, 2] + m[3] * s[:, 3] + m[5]
                for r in zip(X0, Y0, X1, Y1):
                    recs.append((key[0],) + tuple(float(v) for v in r))
    walk(g["id"], g["m"])
    with open(out, "wb") as f:
        f.write(struct.pack("<i", len(recs)))
        for r in recs:
            f.write(struct.pack("<idddd", *r))
    a = np.array([r[1:] for r in recs])
    print(f"ground char {g['id']} at {g['m'][4:]} -> {len(recs)} edges, {key[0]} fills, "
          f"bbox x {a[:, [0, 2]].min():.1f}..{a[:, [0, 2]].max():.1f} y {a[:, [1, 3]].min():.1f}..{a[:, [1, 3]].max():.1f}")


if __name__ == "__main__":
    main()
