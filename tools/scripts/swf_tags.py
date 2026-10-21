"""Minimal SWF tag walker: lists DefineSprite timelines with frame labels and placed children.

Usage: python -I swf_tags.py <uncompressed FWS swf> <out.json> [sprite_id ...]
Output JSON: {sprite_id: {"frames": n, "labels": {label: frame(1-based)}, "places": [[frame, depth, char_id, name, matrix], ...]}}
"""
import json, struct, sys

TAG_NAMES = {0: "End", 1: "ShowFrame", 2: "DefineShape", 4: "PlaceObject", 5: "RemoveObject", 6: "DefineBits",
             9: "SetBackgroundColor", 12: "DoAction", 20: "DefineBitsLossless", 21: "DefineBitsJPEG2",
             22: "DefineShape2", 26: "PlaceObject2", 28: "RemoveObject2", 32: "DefineShape3", 35: "DefineBitsJPEG3",
             36: "DefineBitsLossless2", 39: "DefineSprite", 43: "FrameLabel", 70: "PlaceObject3", 83: "DefineShape4"}


class Bits:
    def __init__(self, data, pos):
        self.d, self.p, self.b = data, pos, 0

    def u(self, n):
        v = 0
        for _ in range(n):
            byte = self.d[self.p]
            v = (v << 1) | ((byte >> (7 - self.b)) & 1)
            self.b += 1
            if self.b == 8:
                self.b, self.p = 0, self.p + 1
        return v

    def s(self, n):
        v = self.u(n)
        return v - (1 << n) if n and v >> (n - 1) else v

    def align(self):
        if self.b:
            self.b, self.p = 0, self.p + 1
        return self.p


def read_matrix(d, p):
    b = Bits(d, p)
    sx = sy = 1.0
    r0 = r1 = 0.0
    if b.u(1):
        n = b.u(5); sx = b.s(n) / 65536; sy = b.s(n) / 65536
    if b.u(1):
        n = b.u(5); r0 = b.s(n) / 65536; r1 = b.s(n) / 65536
    n = b.u(5)
    tx = b.s(n) / 20; ty = b.s(n) / 20
    return [sx, r0, r1, sy, tx, ty], b.align()


def tags(d, p, end):
    while p < end:
        code_len = struct.unpack_from("<H", d, p)[0]; p += 2
        code, ln = code_len >> 6, code_len & 0x3F
        if ln == 0x3F:
            ln = struct.unpack_from("<I", d, p)[0]; p += 4
        yield code, p, ln
        p += ln
        if code == 0:
            return


def parse_place2(d, p, ln):
    flags = d[p]; depth = struct.unpack_from("<H", d, p + 1)[0]; q = p + 3
    cid = name = mat = None
    if flags & 0x02:
        cid = struct.unpack_from("<H", d, q)[0]; q += 2
    if flags & 0x04:
        mat, q = read_matrix(d, q)
    if flags & 0x08:  # color transform: skip by parsing
        b = Bits(d, q); add, mul = b.u(1), b.u(1); n = b.u(4)
        for _ in range((4 if mul else 0) + (4 if add else 0)):
            b.s(n)
        q = b.align()
    if flags & 0x10:
        q += 2
    if flags & 0x20:
        e = d.index(b"\0", q); name = d[q:e].decode("latin1"); q = e + 1
    return {"move": bool(flags & 1), "depth": depth, "id": cid, "name": name, "matrix": mat}


def walk_timeline(d, p, end):
    frame, labels, places, actions = 1, {}, [], []
    for code, tp, ln in tags(d, p, end):
        if code == 1:
            frame += 1
        elif code == 43:
            e = d.index(b"\0", tp); labels[d[tp:e].decode("latin1")] = frame
        elif code in (26, 70):
            pl = parse_place2(d, tp if code == 26 else tp + 1, ln) if code == 26 else None
            if pl:
                pl["frame"] = frame; places.append(pl)
        elif code == 12:
            actions.append(frame)
        elif code == 28:
            places.append({"frame": frame, "remove_depth": struct.unpack_from("<H", d, tp)[0]})
    return {"frames": frame - 1, "labels": labels, "places": places, "action_frames": actions}


def main():
    d = open(sys.argv[1], "rb").read()
    nbits = d[8] >> 3
    p = 8 + (5 + 4 * nbits + 7) // 8 + 4
    want = {int(x) for x in sys.argv[3:]}
    out, counts = {}, {}
    for code, tp, ln in tags(d, p, len(d)):
        counts[TAG_NAMES.get(code, code)] = counts.get(TAG_NAMES.get(code, code), 0) + 1
        if code == 39:
            sid, nframes = struct.unpack_from("<HH", d, tp)
            if not want or sid in want:
                t = walk_timeline(d, tp + 4, tp + ln)
                t["declared_frames"] = nframes
                out[sid] = t
    out["_root"] = walk_timeline(d, p, len(d)) if not want else {}
    out["_tag_counts"] = counts
    json.dump(out, open(sys.argv[2], "w"), indent=1)
    print(json.dumps(counts))


if __name__ == "__main__":
    main()
