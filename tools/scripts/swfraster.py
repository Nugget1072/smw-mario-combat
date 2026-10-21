"""Scanline rasterizer for swflib shapes (fills only; point-sampled at pixel centres, even-odd per fill).

raster_shape(shape, bitmaps, xf, w, h) -> float32 RGBA array (h, w, 4) in 0..1, straight alpha.
xf = (a, b, c, d, tx, ty) maps shape twips -> output pixels: X = a*x + c*y + tx, Y = b*x + d*y + ty.
fill_segments(shape) -> list of (group, fill_index, segments Nx4 twips) for exact point tests.
"""
import numpy as np


def _segments_for_fill(edges, f):
    segs = []
    for f0, f1, _ln, pts in edges:
        if (f0 == f) != (f1 == f):
            for i in range(len(pts) - 1):
                segs.append((pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]))
    return np.array(segs, dtype=np.float64).reshape(-1, 4)


def fill_segments(shape):
    out = []
    for gi, g in enumerate(shape["groups"]):
        for fi in range(1, len(g["fills"]) + 1):
            s = _segments_for_fill(g["edges"], fi)
            if len(s):
                out.append((gi, fi, s))
    return out


def _coverage(segs_px, w, h):
    """Even-odd coverage mask of pixel centres for a set of segments already in output pixel space."""
    mask = np.zeros((h, w), bool)
    if not len(segs_px):
        return mask
    x0, y0, x1, y1 = segs_px.T
    ylo, yhi = np.minimum(y0, y1), np.maximum(y0, y1)
    nonh = y0 != y1
    cx = np.arange(w) + 0.5
    for row in range(h):
        y = row + 0.5
        sel = nonh & (ylo <= y) & (y < yhi)
        if not sel.any():
            continue
        t = (y - y0[sel]) / (y1[sel] - y0[sel])
        xs = np.sort(x0[sel] + t * (x1[sel] - x0[sel]))
        # parity of crossings to the left of each pixel centre
        cnt = np.searchsorted(xs, cx, side="right")
        mask[row] = (cnt & 1).astype(bool)
    return mask


def raster_shape(shape, bitmaps, xf, w, h):
    a, b, c, d, tx, ty = xf
    img = np.zeros((h, w, 4), np.float32)
    det = a * d - b * c
    ia, ib, ic, id_ = d / det, -b / det, -c / det, a / det  # inverse linear part
    yy, xx = np.mgrid[0:h, 0:w]
    px, py = xx + 0.5 - tx, yy + 0.5 - ty
    sx, sy = ia * px + ic * py, ib * px + id_ * py  # pixel centre -> shape twips
    for g in shape["groups"]:
        for fi, fs in enumerate(g["fills"], start=1):
            segs = _segments_for_fill(g["edges"], fi)
            if not len(segs):
                continue
            P = np.empty_like(segs)
            P[:, 0] = a * segs[:, 0] + c * segs[:, 1] + tx
            P[:, 1] = b * segs[:, 0] + d * segs[:, 1] + ty
            P[:, 2] = a * segs[:, 2] + c * segs[:, 3] + tx
            P[:, 3] = b * segs[:, 2] + d * segs[:, 3] + ty
            m = _coverage(P, w, h)
            if not m.any():
                continue
            if fs["type"] == "solid":
                col = np.array(fs["color"], np.float32) / 255
                src = np.broadcast_to(col, (h, w, 4))
            elif fs["type"] == "bitmap" and fs["bitmap"] in bitmaps:
                bm = bitmaps[fs["bitmap"]]
                fa, fb, fc, fd, ftx, fty = fs["matrix"]
                fdet = fa * fd - fb * fc
                qx, qy = sx - ftx, sy - fty
                u = (fd * qx - fc * qy) / fdet
                v = (-fb * qx + fa * qy) / fdet
                bh, bw = bm.shape[:2]
                ui, vi = np.floor(u).astype(int), np.floor(v).astype(int)
                if fs["clipped"]:
                    ui, vi = np.clip(ui, 0, bw - 1), np.clip(vi, 0, bh - 1)
                else:
                    ui, vi = ui % bw, vi % bh
                src = bm[vi, ui]
            elif fs["type"] == "gradient":
                # gradient square is -16384..16384 twips in gradient space; fill matrix maps it to shape twips
                ga, gb, gc, gd, gtx, gty = fs["matrix"]
                gdet = ga * gd - gb * gc
                qx, qy = sx - gtx, sy - gty
                gx = (gd * qx - gc * qy) / gdet
                gy = (-gb * qx + ga * qy) / gdet
                if fs["kind"] == 0x10:
                    t = (gx + 16384) / 32768
                else:
                    t = np.sqrt(gx * gx + gy * gy) / 16384
                t = np.clip(t * 255, 0, 255)
                ratios = np.array([s[0] for s in fs["stops"]], np.float32)
                cols = np.array([s[1] for s in fs["stops"]], np.float32) / 255
                src = np.stack([np.interp(t, ratios, cols[:, ch]) for ch in range(4)], axis=-1).astype(np.float32)
            else:
                src = np.broadcast_to(np.array([1, 0, 1, 1], np.float32), (h, w, 4))  # missing bitmap: magenta marker
            sa = src[..., 3:4] * m[..., None]
            img[..., :3] = src[..., :3] * sa + img[..., :3] * (1 - sa)
            img[..., 3:4] = sa + img[..., 3:4] * (1 - sa)
    rgb = np.where(img[..., 3:4] > 0, img[..., :3] / np.maximum(img[..., 3:4], 1e-6), 0)
    return np.concatenate([np.clip(rgb, 0, 1), img[..., 3:4]], axis=2)
