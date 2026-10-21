"""Render one Mario Combat story area (a root frame) as one picture at SNES scale (round 20, the MC level port).
Usage: python -I mc_level_render.py <MarioCombat.swf> <ffdec_images_dir> <root_frame> <out.png> [--scale S] [--all]
       [--frame SPRITE_ID:FRAME ...] [--hide ID ...] [--only ID[:FRAME]] [--layers DIR]
Every visible character on the root frame is drawn, nested sprites at their first frame, in depth order. Characters
the game hides or moves itself are left out (the `ground` collision shape, the scroll / spawn / exit markers, the hero,
the camera, the HUD, the enemies and Bowser): --all draws them too. S = output pixels per stage pixel (default 1/3:
one Mario Combat art pixel = one SNES pixel). Also writes <out>_ground.png (the `ground` clip's solid area as a mask,
same frame of reference) and <out>.json: the picture's origin in stage px, the scale, and every named or enemy
instance's position (stage px). --hide leaves those characters out too (round 20: the screen-locked BG clips);
--only draws just that character (at its own origin, its given frame) instead of the root frame. --layers also writes
each drawn top-level item (a root depth) alone, cropped: DIR/d<depth>.png, and DIR/layers.json (depth, id, name, its
box in picture px).
"""
import json, math, os, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from swflib import Swf  # noqa: E402
from swfraster import raster_shape  # noqa: E402
from mc_export import mat_mul  # noqa: E402
from mc_ground_export import display_at  # noqa: E402

HIDDEN_IDS = {125, 478, 484, 495, 502, 445, 460, 462, 466, 440, 447, 458, 163, 532, 555, 558}
ENEMY_IDS = {163: 'koopa', 445: 'exit', 532: 'bowser', 476: 'door', 555: 'flame', 558: 'shockwave'}


def main():
    swf_path, img_dir, frame, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    scale = float(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else 1 / 3
    draw_all = '--all' in sys.argv
    sprite_frame = {}  # --frame ID:F (repeatable): that nested sprite drawn at frame F instead of 1
    for i, a in enumerate(sys.argv):
        if a == '--frame':
            k, v = sys.argv[i + 1].split(':')
            sprite_frame[int(k)] = int(v)
    hide = {int(sys.argv[i + 1]) for i, a in enumerate(sys.argv) if a == '--hide'}
    only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else None
    if only and ':' in only:
        sprite_frame[int(only.split(':')[0])] = int(only.split(':')[1])
    swf = Swf(swf_path)
    bitmaps = {}
    for f in os.listdir(img_dir):
        stem = f.split('.')[0]
        if stem.isdigit():
            bitmaps[int(stem)] = np.asarray(Image.open(os.path.join(img_dir, f)).convert('RGBA'), np.float32) / 255
    disp = display_at(swf.root, frame) if not only else {1: {'id': int(only.split(':')[0]), 'm': (1, 0, 0, 1, 0, 0)}}

    items, ground_items, marks, hidden_items = [], [], [], []  # (shape id, global matrix)
    layers = []  # (depth, id, name, its shapes)

    def walk(cid, m, out_list):
        if cid in swf.sprites:
            for d, c in sorted(display_at(swf.sprites[cid], sprite_frame.get(cid, 1)).items()):
                walk(c['id'], mat_mul(m, c['m']), out_list)
        elif cid in swf.shapes:
            out_list.append((cid, m))

    for depth, p in sorted(disp.items()):
        cid, m, name = p['id'], p['m'], p.get('name')
        if cid in ENEMY_IDS or name:
            marks.append({'depth': depth, 'id': cid, 'kind': ENEMY_IDS.get(cid, name), 'name': name, 'x': m[4], 'y': m[5],
                          'sx': m[0], 'sy': m[3]})
        if name == 'ground':
            walk(cid, m, ground_items)
        if cid in hide:  # (still in the picture's bounds: the same origin as without --hide)
            walk(cid, m, hidden_items)
            continue
        if cid in HIDDEN_IDS and not draw_all and not only:
            continue
        n0 = len(items)
        walk(cid, m, items)
        layers.append((depth, cid, name, items[n0:]))

    def bounds(lst):
        xs, ys = [], []
        for sid, m in lst:
            x0, x1, y0, y1 = [v / 20 for v in swf.shapes[sid]['bounds']]
            for x, y in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
                xs.append(m[0] * x + m[2] * y + m[4]); ys.append(m[1] * x + m[3] * y + m[5])
        return min(xs), min(ys), max(xs), max(ys)

    bx0, by0, bx1, by1 = bounds(items + ground_items + hidden_items)
    W, H = int(math.ceil((bx1 - bx0) * scale)) + 1, int(math.ceil((by1 - by0) * scale)) + 1

    def opaque(sh):  # every fill solid white: hitTest(x, y, true) ignores colour and alpha
        return {**sh, 'groups': [{**g, 'fills': [{'type': 'solid', 'color': (255, 255, 255, 255)} for _ in g['fills']]}
                                 for g in sh['groups']]}

    def render(lst, solid=False):
        canvas = np.zeros((H, W, 4), np.float32)
        for sid, m in lst:
            sx0, sy0, sx1, sy1 = bounds([(sid, m)])
            ox, oy = int(math.floor((sx0 - bx0) * scale)), int(math.floor((sy0 - by0) * scale))
            w = min(W - ox, int(math.ceil((sx1 - bx0) * scale)) - ox + 1)
            h = min(H - oy, int(math.ceil((sy1 - by0) * scale)) - oy + 1)
            if w <= 0 or h <= 0:
                continue
            k = scale / 20
            xf = (m[0] * k, m[1] * k, m[2] * k, m[3] * k, (m[4] - bx0) * scale - ox, (m[5] - by0) * scale - oy)
            sh = swf.shapes[sid]
            rgba = raster_shape(opaque(sh) if solid else sh, bitmaps, xf, w, h)
            dst = canvas[oy:oy + h, ox:ox + w]
            a = rgba[..., 3:4]
            dst[..., :3] = rgba[..., :3] * a + dst[..., :3] * (1 - a)
            dst[..., 3:4] = a + dst[..., 3:4] * (1 - a)
        return canvas

    pic = render(items)
    if '--layers' in sys.argv:
        ldir = sys.argv[sys.argv.index('--layers') + 1]
        os.makedirs(ldir, exist_ok=True)
        index = []
        for depth, cid, name, lst in layers:
            if not lst:
                continue
            im = render(lst)
            ys, xs = np.where(im[..., 3] > 0.01)
            if not len(xs):
                continue
            x0, y0, x1, y1 = int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1
            Image.fromarray((np.clip(im[y0:y1, x0:x1], 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(
                os.path.join(ldir, 'd%d.png' % depth))
            index.append({'depth': depth, 'id': cid, 'name': name, 'box': [x0, y0, x1, y1]})
        json.dump(index, open(os.path.join(ldir, 'layers.json'), 'w'), indent=1)
    Image.fromarray((np.clip(pic, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(out)
    gmask = render(ground_items, True)[..., 3] > 0.5
    Image.fromarray((gmask * 255).astype(np.uint8), 'L').save(out.rsplit('.', 1)[0] + '_ground.png')
    info = {'frame': frame, 'origin_stage_px': [bx0, by0], 'scale': scale, 'size': [W, H], 'marks': marks}
    json.dump(info, open(out.rsplit('.', 1)[0] + '.json', 'w'), indent=1)
    print('frame %d: %dx%d px, origin (%.1f, %.1f) stage px, %d shapes, %d marks' % (frame, W, H, bx0, by0,
                                                                                   len(items), len(marks)))


if __name__ == '__main__':
    main()
