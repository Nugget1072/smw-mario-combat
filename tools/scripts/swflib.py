"""Small SWF (AVM1-era) reader used by the Mario Combat exporters.

Parses: header, DefineShape1-4 (styles + full shape records), DefineSprite timelines
(PlaceObject2/3, RemoveObject/2, FrameLabel, DoAction presence, StartSound), DefineSound headers.
Coordinates are kept in twips internally where noted; public helpers return pixels.
"""
import struct, zlib


class Bits:
    def __init__(self, d, p):
        self.d, self.p, self.b = d, p, 0

    def u(self, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | ((self.d[self.p] >> (7 - self.b)) & 1)
            self.b += 1
            if self.b == 8:
                self.b = 0; self.p += 1
        return v

    def s(self, n):
        v = self.u(n)
        return v - (1 << n) if n and v >> (n - 1) else v

    def align(self):
        if self.b:
            self.b = 0; self.p += 1
        return self.p


def read_rect(d, p):
    b = Bits(d, p); n = b.u(5)
    return [b.s(n) for _ in range(4)], b.align()  # twips: xmin, xmax, ymin, ymax


def read_matrix(d, p):
    """Returns ([a, b, c, d, tx, ty], newpos) with x' = a*x + c*y + tx, y' = b*x + d*y + ty (tx/ty in twips)."""
    b = Bits(d, p)
    a = dd = 1.0; bb = cc = 0.0
    if b.u(1):
        n = b.u(5); a = b.s(n) / 65536; dd = b.s(n) / 65536
    if b.u(1):
        n = b.u(5); bb = b.s(n) / 65536; cc = b.s(n) / 65536
    n = b.u(5); tx = b.s(n); ty = b.s(n)
    return [a, bb, cc, dd, tx, ty], b.align()


def read_cxform(d, p, alpha):
    b = Bits(d, p); has_add, has_mul = b.u(1), b.u(1); n = b.u(4)
    cnt = 4 if alpha else 3
    mul = [256] * 4; add = [0] * 4
    if has_mul:
        mul = [b.s(n) for _ in range(cnt)] + ([256] if not alpha else [])
    if has_add:
        add = [b.s(n) for _ in range(cnt)] + ([0] if not alpha else [])
    return {"mul": mul, "add": add}, b.align()


def tags(d, p, end):
    while p < end:
        cl = struct.unpack_from("<H", d, p)[0]; p += 2
        code, ln = cl >> 6, cl & 0x3F
        if ln == 0x3F:
            ln = struct.unpack_from("<I", d, p)[0]; p += 4
        yield code, p, ln
        p += ln
        if code == 0:
            return


class Swf:
    def __init__(self, path):
        raw = open(path, "rb").read()
        if raw[:3] == b"CWS":
            raw = raw[:8] + zlib.decompress(raw[8:])
        self.d = raw
        self.version = raw[3]
        self.stage, p = read_rect(raw, 8)
        self.fps = raw[p + 1] + raw[p] / 256
        self.body = p + 4
        self.shapes, self.sprites, self.sounds, self.bitmaps = {}, {}, {}, set()
        self.root = None
        self._parse()

    # ---------- shapes ----------
    def _fill_style(self, p, ver):
        d = self.d
        t = d[p]; p += 1
        if t == 0x00:
            if ver >= 3:
                return {"type": "solid", "color": tuple(d[p:p + 4])}, p + 4
            return {"type": "solid", "color": tuple(d[p:p + 3]) + (255,)}, p + 3
        if t in (0x10, 0x12, 0x13):
            m, p = read_matrix(d, p)
            b = d[p]; p += 1
            stops = []
            for _ in range(b & 0x0F):
                r = d[p]; p += 1
                c = tuple(d[p:p + (4 if ver >= 3 else 3)]); p += 4 if ver >= 3 else 3
                stops.append((r, c if len(c) == 4 else c + (255,)))
            if t == 0x13:
                p += 2
            return {"type": "gradient", "kind": t, "matrix": m, "stops": stops}, p
        if t in (0x40, 0x41, 0x42, 0x43):
            bid = struct.unpack_from("<H", d, p)[0]; p += 2
            m, p = read_matrix(d, p)
            return {"type": "bitmap", "bitmap": bid, "matrix": m, "clipped": t in (0x41, 0x43),
                    "smoothed": t in (0x40, 0x41)}, p
        raise ValueError(f"fill type {t:#x}")

    def _fill_styles(self, p, ver):
        d = self.d
        n = d[p]; p += 1
        if n == 0xFF and ver >= 2:
            n = struct.unpack_from("<H", d, p)[0]; p += 2
        out = []
        for _ in range(n):
            fs, p = self._fill_style(p, ver)
            out.append(fs)
        return out, p

    def _line_styles(self, p, ver):
        d = self.d
        n = d[p]; p += 1
        if n == 0xFF:
            n = struct.unpack_from("<H", d, p)[0]; p += 2
        out = []
        for _ in range(n):
            if ver == 4:
                w = struct.unpack_from("<H", d, p)[0]; p += 2
                f1, f2 = d[p], d[p + 1]; p += 2
                join = (f1 >> 4) & 3
                has_fill = (f1 >> 3) & 1
                if join == 2:
                    p += 2
                if has_fill:
                    fs, p = self._fill_style(p, ver)
                    out.append({"width": w, "fill": fs})
                else:
                    out.append({"width": w, "color": tuple(d[p:p + 4])}); p += 4
            else:
                w = struct.unpack_from("<H", d, p)[0]; p += 2
                c = tuple(d[p:p + (4 if ver >= 3 else 3)]); p += 4 if ver >= 3 else 3
                out.append({"width": w, "color": c})
        return out, p

    def _parse_shape(self, code, tp, ln):
        d = self.d
        ver = {2: 1, 22: 2, 32: 3, 83: 4}[code]
        sid = struct.unpack_from("<H", d, tp)[0]
        bounds, p = read_rect(d, tp + 2)
        if ver == 4:
            _, p = read_rect(d, p); p += 1
        fills, p = self._fill_styles(p, ver)
        lines, p = self._line_styles(p, ver)
        groups = [{"fills": fills, "lines": lines, "edges": []}]
        nf, nl = d[p] >> 4, d[p] & 15; p += 1
        b = Bits(d, p)
        x = y = 0
        f0 = f1 = ln_ = 0
        while True:
            if b.u(1) == 0:
                flags = b.u(5)
                if flags == 0:
                    break
                if flags & 1:
                    n = b.u(5); x = b.s(n); y = b.s(n)
                if flags & 2:
                    f0 = b.u(nf)
                if flags & 4:
                    f1 = b.u(nf)
                if flags & 8:
                    ln_ = b.u(nl)
                if flags & 16:
                    q = b.align()
                    fills, q = self._fill_styles(q, ver)
                    lines, q = self._line_styles(q, ver)
                    groups.append({"fills": fills, "lines": lines, "edges": []})
                    nf, nl = d[q] >> 4, d[q] & 15
                    b = Bits(d, q + 1)
                    f0 = f1 = ln_ = 0
            else:
                if b.u(1):  # straight
                    n = b.u(4) + 2
                    if b.u(1):
                        dx = b.s(n); dy = b.s(n)
                    elif b.u(1):
                        dx = 0; dy = b.s(n)
                    else:
                        dx = b.s(n); dy = 0
                    groups[-1]["edges"].append((f0, f1, ln_, [(x, y), (x + dx, y + dy)]))
                    x += dx; y += dy
                else:  # curved: flatten the quadratic into segments
                    n = b.u(4) + 2
                    cx, cy = x + b.s(n), y + b.s(n)
                    ax, ay = cx + b.s(n), cy + b.s(n)
                    pts = [(x, y)]
                    steps = 8
                    for i in range(1, steps + 1):
                        t = i / steps
                        pts.append(((1 - t) ** 2 * x + 2 * (1 - t) * t * cx + t * t * ax,
                                    (1 - t) ** 2 * y + 2 * (1 - t) * t * cy + t * t * ay))
                    groups[-1]["edges"].append((f0, f1, ln_, pts))
                    x, y = ax, ay
        self.shapes[sid] = {"bounds": bounds, "groups": groups, "ver": ver}

    # ---------- timelines ----------
    def _timeline(self, p, end):
        d = self.d
        frame, labels, frames = 1, {}, {1: []}
        for code, tp, ln in tags(d, p, end):
            ops = frames.setdefault(frame, [])
            if code == 1:
                frame += 1
            elif code == 43:
                e = d.index(b"\0", tp); labels[d[tp:e].decode("latin1")] = frame
            elif code == 12:
                ops.append({"op": "action"})
            elif code == 15:
                ops.append({"op": "sound", "id": struct.unpack_from("<H", d, tp)[0]})
            elif code in (26, 70):
                q = tp
                fl = d[q]; q += 1
                fl2 = 0
                if code == 70:
                    fl2 = d[q]; q += 1
                depth = struct.unpack_from("<H", d, q)[0]; q += 2
                op = {"op": "place", "depth": depth, "move": bool(fl & 1)}
                if code == 70 and (fl2 & 8 or (fl2 & 16 and fl & 2)):
                    e = d.index(b"\0", q); op["class"] = d[q:e].decode("latin1"); q = e + 1
                if fl & 2:
                    op["id"] = struct.unpack_from("<H", d, q)[0]; q += 2
                if fl & 4:
                    op["matrix"], q = read_matrix(d, q)
                if fl & 8:
                    op["cxform"], q = read_cxform(d, q, True)
                if fl & 16:
                    op["ratio"] = struct.unpack_from("<H", d, q)[0]; q += 2
                if fl & 32:
                    e = d.index(b"\0", q); op["name"] = d[q:e].decode("latin1"); q = e + 1
                if fl & 64:
                    op["clipdepth"] = struct.unpack_from("<H", d, q)[0]; q += 2
                if code == 70 and fl2 & 1:
                    op["filters"] = True
                if code == 70 and fl2 & 2:
                    op["blend"] = True
                if fl & 128:
                    op["clipactions"] = True
                ops.append(op)
            elif code == 4:
                cid, depth = struct.unpack_from("<HH", d, tp)
                m, q = read_matrix(d, tp + 4)
                ops.append({"op": "place", "depth": depth, "move": False, "id": cid, "matrix": m})
            elif code == 5:
                ops.append({"op": "remove", "depth": struct.unpack_from("<H", d, tp + 2)[0]})
            elif code == 28:
                ops.append({"op": "remove", "depth": struct.unpack_from("<H", d, tp)[0]})
        return {"frames": max(1, frame - 1), "labels": labels, "ops": frames}

    def _parse(self):
        d = self.d
        for code, tp, ln in tags(d, self.body, len(d)):
            if code in (2, 22, 32, 83):
                self._parse_shape(code, tp, ln)
            elif code == 39:
                sid, nf = struct.unpack_from("<HH", d, tp)
                t = self._timeline(tp + 4, tp + ln)
                t["declared_frames"] = nf
                self.sprites[sid] = t
            elif code == 14:
                sid = struct.unpack_from("<H", d, tp)[0]
                fl = d[tp + 2]
                self.sounds[sid] = {"format": fl >> 4, "rate": [5512, 11025, 22050, 44100][(fl >> 2) & 3],
                                    "bits16": bool(fl & 2), "stereo": bool(fl & 1),
                                    "samples": struct.unpack_from("<I", d, tp + 3)[0], "offset": tp + 7, "length": ln - 7}
            elif code in (6, 20, 21, 35, 36, 90):
                self.bitmaps.add(struct.unpack_from("<H", d, tp)[0])
        self.root = self._timeline(self.body, len(d))
