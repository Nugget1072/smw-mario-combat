"""Dump DefineShape bounds and fill styles (bitmap fills with matrices, solid colours).

Usage: python -I swf_shapes.py <FWS swf> <out.json>
Output: {shape_id: {"bounds": [xmin, xmax, ymin, ymax] (px), "fills": [{"type", "bitmap", "matrix"|"color"}]}}
"""
import json, struct, sys
sys.path.insert(0, __import__("os").path.dirname(__file__))
from swf_tags import Bits, read_matrix, tags  # noqa: E402  (swf_tags.main is guarded below)


def read_rect(d, p):
    b = Bits(d, p); n = b.u(5)
    v = [b.s(n) / 20 for _ in range(4)]
    return v, b.align()


def read_fills(d, p, ver):
    n = d[p]; p += 1
    if n == 0xFF and ver >= 2:
        n = struct.unpack_from("<H", d, p)[0]; p += 2
    fills = []
    for _ in range(n):
        t = d[p]; p += 1
        if t == 0x00:
            if ver >= 3:
                fills.append({"type": "solid", "color": list(d[p:p + 4])}); p += 4
            else:
                fills.append({"type": "solid", "color": list(d[p:p + 3]) + [255]}); p += 3
        elif t in (0x10, 0x12, 0x13):
            m, p = read_matrix(d, p)
            cnt = d[p] & 0x0F; p += 1
            p += cnt * (5 if ver < 3 else 6)
            if t == 0x13:
                p += 2
            fills.append({"type": "gradient", "matrix": m})
        elif t in (0x40, 0x41, 0x42, 0x43):
            bid = struct.unpack_from("<H", d, p)[0]; p += 2
            m, p = read_matrix(d, p)
            fills.append({"type": "bitmap", "bitmap": bid, "matrix": m, "smoothed": t in (0x40, 0x41), "clipped": t in (0x41, 0x43)})
        else:
            fills.append({"type": f"unknown_{t:#x}"}); break
    return fills, p


def main():
    d = open(sys.argv[1], "rb").read()
    nbits = d[8] >> 3
    p = 8 + (5 + 4 * nbits + 7) // 8 + 4
    out = {}
    vers = {2: 1, 22: 2, 32: 3, 83: 4}
    for code, tp, ln in tags(d, p, len(d)):
        if code in vers:
            ver = vers[code]
            sid = struct.unpack_from("<H", d, tp)[0]
            bounds, q = read_rect(d, tp + 2)
            if ver == 4:
                _, q = read_rect(d, q); q += 1
            try:
                fills, q = read_fills(d, q, min(ver, 3))
            except Exception as e:  # keep going on odd shapes
                fills = [{"type": "error", "msg": str(e)}]
            out[sid] = {"bounds": bounds, "fills": fills}
    json.dump(out, open(sys.argv[2], "w"), indent=1)
    print(len(out), "shapes")


if __name__ == "__main__":
    main()
