"""Export Mario Combat enemies (a DefineSprite tree per root) to C data for the SMW port.

Usage:
  python -I mc_enemy_export.py <MarioCombat.swf> <ffdec_images_dir> <out_dir> <preview_dir> --prefix <Name>
         --roots <id>[@scale],<id>[@scale]...
  Koopa:  --prefix Koopa  --roots 163
  Bowser: --prefix Bowser --roots 532,555,558@0.3614654541   (the shockwave template sits at that scale on the stage)

Writes <out_dir>/mc_<name>_data.c and .h (name = prefix in lower case), <preview_dir>/<name>_report.json (sidecar:
images, clips, labels, sounds) and <preview_dir>/<name>_sheet.png (every root frame, its nested animation stepped
through, at 1 SNES px per art px).

Shapes are rasterized at native art resolution exactly like the hero (mc_export.py: 1 art pixel = 1 SNES pixel). The
enemy timelines are simple (children only placed / moved / removed, no AS2 tweening), so every clip frame is baked into
a full display list: the C side only tracks each clip's frame and never replays PlaceObject ops.
"""
import json, math, os, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from swflib import Swf  # noqa: E402
from swfraster import raster_shape  # noqa: E402
from mc_export import mat_mul, px_matrix, STAGE_PER_ART  # noqa: E402

# Instance names the enemy scripts use. The index is McKoopaItem.name; keep existing numbers when adding names.
NAMES = ["", "walk", "hurt_mc", "attack", "hit1", "hit2", "bar", "bound"]
HIDDEN = ("hit1", "hit2", "bound")  # invisible boxes (_visible = false in the scripts)


def arg(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def main():
    swf_path, img_dir, out_dir, prev_dir = sys.argv[1:5]
    prefix = arg("--prefix", "Koopa")
    roots = []
    for r in arg("--roots", "163").split(","):
        rid, _, sc = r.partition("@")
        roots.append((int(rid), float(sc) if sc else 1.0))
    low = prefix.lower()
    swf = Swf(swf_path)
    os.makedirs(prev_dir, exist_ok=True)

    # ---- collect the trees: sprites, shapes and the scales the shapes are drawn at ----
    sprites, shapes, usage, visited = [], [], {}, set()

    def visit(cid, mat):
        if cid in swf.sprites:
            if cid not in sprites:
                sprites.append(cid)
            cur = {}
            for f in range(1, swf.sprites[cid]["frames"] + 1):
                for op in swf.sprites[cid]["ops"].get(f, []):
                    if op["op"] == "remove":
                        cur.pop(op["depth"], None)
                    elif op["op"] == "place":
                        prev = cur.get(op["depth"])
                        ch = op.get("id", prev[0] if prev else None)
                        m = px_matrix(op["matrix"]) if "matrix" in op else (prev[1] if prev and op["move"] else [1, 0, 0, 1, 0, 0])
                        cur[op["depth"]] = (ch, m)
                        key = (ch, tuple(np.round(mat_mul(mat, m), 3)))
                        if ch is not None and key not in visited:
                            visited.add(key)
                            visit(ch, mat_mul(mat, m))
        elif cid in swf.shapes:
            if cid not in shapes:
                shapes.append(cid)
            usage.setdefault(cid, []).append(math.sqrt(abs(mat[0] * mat[3] - mat[1] * mat[2])))

    for rid, sc in roots:
        visit(rid, [sc, 0, 0, sc, 0, 0])

    bitmaps = {}
    for f in os.listdir(img_dir):
        stem = f.split(".")[0]
        if stem.isdigit():
            bitmaps[int(stem)] = np.asarray(Image.open(os.path.join(img_dir, f)).convert("RGBA"), np.float32) / 255

    images, shape_img = [], {}
    for sid in sorted(shapes):
        sh = swf.shapes[sid]
        sc = sorted(usage[sid])[len(usage[sid]) // 2]
        k = sc / STAGE_PER_ART
        for nice in (1.0, 1 / 3):
            if abs(k / nice - 1) < 0.06:
                k = nice
        x0, x1, y0, y1 = [v / 20 for v in sh["bounds"]]
        w = max(1, int(math.ceil((x1 - x0) * k - 1e-6)))
        h = max(1, int(math.ceil((y1 - y0) * k - 1e-6)))
        rgba = raster_shape(sh, bitmaps, (k / 20, 0, 0, k / 20, -x0 * k, -y0 * k), w, h)
        a8 = (rgba * 255 + 0.5).astype(np.uint8)
        Image.fromarray(a8, "RGBA").save(os.path.join(prev_dir, f"{low}_shape_{sid}.png"))
        shape_img[sid] = len(images)
        images.append({"id": sid, "w": w, "h": h, "k": k, "ox": x0, "oy": y0, "rgba": a8, "bounds": (x0, y0, x1, y1)})

    # ---- bake every clip frame: depth -> (char, matrix, cxform, name, birth frame) ----
    clip_idx = {sid: i for i, sid in enumerate(sprites)}
    frames, items, clips, sounds = [], [], [], set()
    for sid in sprites:
        t = swf.sprites[sid]
        clips.append((sid, t["frames"], len(frames)))
        cur = {}
        for f in range(1, t["frames"] + 1):
            snd = 0
            for op in t["ops"].get(f, []):
                if op["op"] == "remove":
                    cur.pop(op["depth"], None)
                elif op["op"] == "sound":
                    snd = op["id"]
                    sounds.add(op["id"])
                elif op["op"] == "place":
                    prev = cur.get(op["depth"])
                    new_char = "id" in op and (not prev or op["id"] != prev[0] or not op["move"])
                    ch = op.get("id", prev[0] if prev else None)
                    m = px_matrix(op["matrix"]) if "matrix" in op else (prev[1] if prev else [1, 0, 0, 1, 0, 0])
                    cx = op.get("cxform", {"mul": [256] * 4, "add": [0] * 4})
                    cx = cx["mul"] + cx["add"] if "cxform" in op else (prev[2] if prev else [256] * 4 + [0] * 4)
                    name = op.get("name", prev[3] if prev else None)
                    birth = f if (new_char or not prev) else prev[4]
                    cur[op["depth"]] = (ch, m, cx, name, birth)
            first = len(items)
            bx0 = by0 = 1e9
            bx1 = by1 = -1e9
            for depth in sorted(cur):
                ch, m, cx, name, birth = cur[depth]
                if ch in shape_img:
                    kind, ref = 0, shape_img[ch]
                    x0, y0, x1, y1 = images[ref]["bounds"]
                elif ch in clip_idx:
                    kind, ref = 1, clip_idx[ch]
                    x0 = y0 = x1 = y1 = None
                else:
                    continue
                items.append((kind, ref, NAMES.index(name) if name in NAMES else 0, f - birth, m, cx))
                if x0 is not None and name not in HIDDEN:
                    for px, py in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
                        X, Y = m[0] * px + m[2] * py + m[4], m[1] * px + m[3] * py + m[5]
                        bx0, by0, bx1, by1 = min(bx0, X), min(by0, Y), max(bx1, X), max(by1, Y)
            if bx0 > bx1:
                bx0 = by0 = bx1 = by1 = 0
            frames.append((first, len(items) - first, snd, (bx0, by0, bx1, by1)))

    labels = [(clip_idx[sid], f, n) for sid in sprites for n, f in swf.sprites[sid]["labels"].items()]

    root_desc = ", ".join(f"{rid}" + (f"@{sc:g}" if sc != 1 else "") for rid, sc in roots)
    L = [f'// Generated by tools/scripts/mc_enemy_export.py from Mario Combat (DefineSprite {root_desc}). Do not edit.',
         f'#include "mc_{low}_data.h"', '']
    for im in images:
        argb = im["rgba"].astype(np.uint32)
        packed = (argb[..., 3] << 24) | (argb[..., 0] << 16) | (argb[..., 1] << 8) | argb[..., 2]
        L.append(f"static const uint32 k{prefix}Img{im['id']}[] = {{{','.join(f'0x{v:08x}' for v in packed.flatten())}}};")
    L.append(f"const McImage kMc{prefix}Images[] = {{")
    for im in images:
        L.append(f"  {{{im['w']}, {im['h']}, {im['k']:.6f}f, {im['ox']:.4f}f, {im['oy']:.4f}f, k{prefix}Img{im['id']}}},  // shape {im['id']}")
    L.append("};")
    L.append(f"const McKoopaClip kMc{prefix}Clips[] = {{")
    for sid, nf, first in clips:
        L.append(f"  {{{sid}, {nf}, {first}}},")
    L.append("};")
    L.append(f"const int kMc{prefix}NumClips = {len(clips)};")
    L.append(f"const McKoopaFrame kMc{prefix}Frames[] = {{")
    for first, n, snd, b in frames:
        L.append(f"  {{{first}, {n}, {snd}, {{{b[0]:.3f}f, {b[1]:.3f}f, {b[2]:.3f}f, {b[3]:.3f}f}}}},")
    L.append("};")
    L.append(f"const McKoopaItem kMc{prefix}Items[] = {{")
    for kind, ref, name, age, m, cx in items:
        L.append(f"  {{{kind}, {ref}, {name}, {age}, {{{m[0]:.6f}f, {m[1]:.6f}f, {m[2]:.6f}f, {m[3]:.6f}f, {m[4]:.4f}f, "
                 f"{m[5]:.4f}f}}, {{{','.join(str(v) for v in cx)}}}}},")
    L.append("};")
    L.append(f"const McKoopaLabel kMc{prefix}Labels[] = {{")
    for c, f, n in labels:
        L.append(f'  {{{c}, {f}, "{n}"}},')
    L.append("};")
    L.append(f"const int kMc{prefix}NumLabels = {len(labels)};")
    open(os.path.join(out_dir, f"mc_{low}_data.c"), "w").write("\n".join(L) + "\n")

    H = [f'// Mario Combat enemy data (DefineSprite {root_desc}), generated by tools/scripts/mc_enemy_export.py. Do not edit.',
         '#pragma once', '#include "mc_enemy_data.h"', '',
         f'extern const McImage kMc{prefix}Images[];', f'extern const McKoopaClip kMc{prefix}Clips[];',
         f'extern const int kMc{prefix}NumClips;', f'extern const McKoopaFrame kMc{prefix}Frames[];',
         f'extern const McKoopaItem kMc{prefix}Items[];', f'extern const McKoopaLabel kMc{prefix}Labels[];',
         f'extern const int kMc{prefix}NumLabels;']
    open(os.path.join(out_dir, f"mc_{low}_data.h"), "w").write("\n".join(H) + "\n")

    json.dump({"prefix": prefix, "roots": roots, "sprites": sprites, "names": NAMES,
               "images": [{k: im[k] for k in ("id", "w", "h", "k")} for im in images],
               "frames": len(frames), "items": len(items), "labels": labels, "sounds": sorted(sounds),
               "root_bounds_frame1": [frames[clips[clip_idx[rid]][2]][3] for rid, _ in roots]},
              open(os.path.join(prev_dir, f"{low}_report.json"), "w"), indent=1)

    # ---- contact sheet: each root frame, its nested animation stepped through (nested-nested clips at frame 1) ----
    def composite(ci, f, force_child_frame=None, depth=0):
        first, n, snd, b = frames[clips[ci][2] + f - 1]
        out = []
        for kind, ref, name, age, m, cx in items[first:first + n]:
            if NAMES[name] in HIDDEN:
                continue
            if kind == 0:
                out.append((ref, m))
            elif depth < 3:
                sub = clips[ref]
                cf = force_child_frame if (force_child_frame and depth == 0) else min(sub[1], age + 1)
                for r2, m2 in composite(ref, min(sub[1], cf), None, depth + 1):
                    out.append((r2, mat_mul(m, m2)))
        return out

    def child_frames(ci, f):  # frame counts of the clips placed on root frame f
        first, n, snd, b = frames[clips[ci][2] + f - 1]
        return max([clips[ref][1] for kind, ref, name, age, m, cx in items[first:first + n] if kind == 1] + [1])

    cell = 128
    cells = []
    for rid, sc in roots:
        ci = clip_idx[rid]
        for f in range(1, clips[ci][1] + 1):
            nf = child_frames(ci, f)
            for cf in range(1, nf + 1, max(1, nf // 6)):
                canvas = Image.new("RGBA", (cell, cell), (60, 60, 90, 255))
                for ref, m in composite(ci, f, cf):
                    m = mat_mul([sc, 0, 0, sc, 0, 0], m)
                    im = images[ref]
                    src = Image.fromarray(im["rgba"], "RGBA")
                    inv = 1 / (im["k"] * STAGE_PER_ART)  # image px -> stage px (1/k) -> SNES px (1/3)
                    a, b2, c, d = m[0] * inv, m[1] * inv, m[2] * inv, m[3] * inv
                    tx = (m[0] * im["ox"] + m[2] * im["oy"] + m[4]) / STAGE_PER_ART + cell / 2
                    ty = (m[1] * im["ox"] + m[3] * im["oy"] + m[5]) / STAGE_PER_ART + cell * 0.8
                    det = a * d - b2 * c
                    if abs(det) < 1e-9:
                        continue
                    ia, ib, ic, id_ = d / det, -b2 / det, -c / det, a / det
                    t = src.transform((cell, cell), Image.AFFINE,
                                      (ia, ic, -(ia * tx + ic * ty), ib, id_, -(ib * tx + id_ * ty)), Image.NEAREST)
                    canvas.alpha_composite(t)
                cells.append(canvas)
    cols = 12
    sheet = Image.new("RGBA", (cell * cols, cell * ((len(cells) + cols - 1) // cols)), (30, 30, 40, 255))
    for i, c in enumerate(cells):
        sheet.paste(c, ((i % cols) * cell, (i // cols) * cell))
    sheet.save(os.path.join(prev_dir, f"{low}_sheet.png"))
    print(f"{prefix}: {len(images)} images, {len(clips)} clips, {len(frames)} frames, {len(items)} items, "
          f"sounds {sorted(sounds)}, {len(cells)} sheet cells")


if __name__ == "__main__":
    main()
