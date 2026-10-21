"""Mario Combat's own art for the MC level in SMW (round 20): generates src/mc/mc_level_art.c.
Usage: python -I mc_level_art.py <MarioCombat.swf> <ffdec_images_dir> <level_json> <out_c_file> <work_dir>
<level_json> is mc_level_export.py's sidecar (mc_level_data.c.json: each area's picture offset x0 / y0, width, and
every cell's kind as a letter: S stone, B brown block, R bridge, L lava, v spikes, D door, P pipe, 0 air).

The game hides SMW's tile layers in the MC level. What shows is drawn per screen line (McLevel_ArtLine):
1. the area's ground, from its tiles at runtime, in MC's style: MC's stone weave framed by MC's 8 px bevel (rounded
   outer corners, inner corners), MC's brown blocks (outlined round each group); so the ground is always exactly where the collision is, also
   where tiles come and go (area 4's ledge, cages and lift);
2. over the rest, this picture of the area (level px, 27 rows): MC's sky and clouds (areas 1, 5) as MC draws them;
   MC's props (Bowser statues, pillars, chains, the castle, the red door) taken out of MC's pictures (MC's ground,
   lava and spikes art removed), each snapped onto the tile ground it stands on (or hangs from), the door onto its
   door tiles; MC's lava (its colour; the game draws its surface, sprite 141, and its glow), spikes, pipe and bridge drawn on their own
   tiles. 255 colours with alpha, run-length coded by row;
3. under it: the sky's colour per row (areas 1, 5), else MC's castle wall locked to the screen (BG 489, screen-wide).
Writes <work_dir>/layers_<frame>/ (mc_level_render.py --layers), <work_dir>/area<n>_art.png previews (as the game
draws them, the wall repeated) and <out_c_file>.json.
"""
import json, os, subprocess, sys
from collections import deque
import numpy as np
from PIL import Image

FRAMES = [486, 488, 490, 492, 496]
ROWS = 432
HIDE = ['469', '489', '14']  # the BG clips (screen-locked), area 2's grey under its BG
BACKDROP = {1: {120, 123}, 5: {498, 500, 559}}  # sky, clouds, fog: as MC draws them, behind everything
LAVA_IDS = {141, 142}  # MC's lava surface tiles and body: drawn on the lava tiles instead
DOOR_ID = 476
STAGE_W = 705  # BG 469 / 489's width in stage px (centred on the screen)
SOLID = 'SBR'


def render(swf, imgs, args, out):
    here = os.path.dirname(os.path.abspath(__file__))
    cmd = [sys.executable, '-I', os.path.join(here, 'mc_level_render.py'), swf, imgs, args[0], out] + args[1:]
    subprocess.run(cmd, check=True, capture_output=True)
    return np.asarray(Image.open(out).convert('RGBA'))


def over(dst, src, x, y):
    """src (RGBA uint8) alpha-composited onto dst (float RGBA 0-1) at (x, y), clipped."""
    H, W = dst.shape[:2]
    h, w = src.shape[:2]
    x0, y0, x1, y1 = max(0, x), max(0, y), min(W, x + w), min(H, y + h)
    if x1 <= x0 or y1 <= y0:
        return
    s = src[y0 - y:y1 - y, x0 - x:x1 - x].astype(np.float32) / 255
    d = dst[y0:y1, x0:x1]
    a = s[..., 3:4]
    d[..., :3] = s[..., :3] * a + d[..., :3] * (1 - a)
    d[..., 3:4] = a + d[..., 3:4] * (1 - a)


def dilate(m, k):
    out = m.copy()
    for _ in range(k):
        o = out.copy()
        o[1:] |= out[:-1]; o[:-1] |= out[1:]; o[:, 1:] |= out[:, :-1]; o[:, :-1] |= out[:, 1:]
        out = o
    return out


def components(alpha):
    """8-connected components of alpha > 0: lists of (ys, xs)."""
    H, W = alpha.shape
    seen = np.zeros((H, W), bool)
    comps = []
    for y0, x0 in zip(*np.nonzero(alpha)):
        if seen[y0, x0]:
            continue
        q = deque([(y0, x0)])
        seen[y0, x0] = True
        ys, xs = [], []
        while q:
            y, x = q.popleft()
            ys.append(y); xs.append(x)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < H and 0 <= xx < W and alpha[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True
                        q.append((yy, xx))
        comps.append((np.array(ys), np.array(xs)))
    return comps


def stone_tile(can, solid):
    """A 16 x 16 tile of the area's stone, level-aligned (tile[ly & 15][lx & 15] continues the picture's pattern):
    the opaque cell inside the ground whose picture repeats best 16 px to the right and below, the most contrasted of
    those (MC's stone weave, not its smoother stair stone)."""
    R, W = len(solid), len(solid[0])
    best = None
    for r in range(1, R - 1):
        for c in range(1, W - 1):
            if solid[r][c] != 'S' or solid[r][c + 1] != 'S' or solid[r + 1][c] != 'S':
                continue
            X, Y = c * 16, r * 16
            a = can[Y:Y + 16, X:X + 16].astype(np.float32)
            if a[..., 3].min() < 255 or can[Y:Y + 32, X:X + 32, 3].min() < 255:
                continue
            err = (np.abs(can[Y:Y + 16, X + 16:X + 32].astype(np.float32) - a).mean() +
                   np.abs(can[Y + 16:Y + 32, X:X + 16].astype(np.float32) - a).mean())
            rgb = a[..., :3]
            if (rgb[..., 0] > rgb[..., 1] + 12).mean() > 0.2:  # (lava over it, not stone)
                continue
            key = (err > 8, -float(rgb.std()))
            if best is None or key < best[0]:
                best = (key, X, Y)
    X, Y = best[1], best[2]
    tile = np.zeros((16, 16, 4), np.uint8)
    for j in range(16):
        for i in range(16):
            tile[j, i] = can[Y + (j - Y) % 16, X + (i - X) % 16]
    return tile


def brown_block(pic488):
    """MC's brown block as a 16 x 16 tile. MC's (area 2's row at 1468-1536, 412-427) is pixel art: a 14 x 14 bevelled
    block, a 1 px dark seam after it (15 px apart), a 1 px dark outline round each group. Here: the seam / outline
    colour on the tile's top row and left column, the block widened to 15 x 15 by one more of its flat middle row and
    column; the game draws the outline on the right and bottom where no brown block follows."""
    blk = pic488[413:427, 1472:1486]  # (14 x 14)
    blk = np.concatenate([blk[:7], blk[7:8], blk[7:]], 0)
    blk = np.concatenate([blk[:, :7], blk[:, 7:8], blk[:, 7:]], 1)
    tile = np.zeros((16, 16, 4), np.uint8)
    tile[...] = pic488[412, 1471]  # (the outline: 104,88,72)
    tile[1:, 1:] = blk
    return tile


def lava_frames(swf, imgs, wdir, body):
    """MC's lava surface (sprite 141: six shapes, five MC frames each, a 30-frame loop) as six 16 x 16 tiles: the frame
    (16 x 15 at SNES scale) over a last row of the lava's colour."""
    frames = []
    for k in range(6):
        im = render(swf, imgs, ['1', '--only', '141:%d' % (1 + 5 * k)], os.path.join(wdir, 'lava_%d.png' % k))
        tile = np.zeros((16, 16, 4), np.uint8)
        tile[15, :, :3], tile[15, :, 3] = body, 255
        h, w = min(15, im.shape[0]), min(16, im.shape[1])
        tile[:h, :w] = im[:h, :w]
        frames.append(tile)
    return frames


def pieces(wdir, pic490, pic496_bridge):
    """MC's art for the tile-anchored things: the lava surface (sprite 141) and colour (142), a spike, the pipe, the
    bridge's repeating strip."""
    lay = json.load(open(os.path.join(wdir, 'layers_486', 'layers.json')))
    d141 = next(e['depth'] for e in lay if e['id'] == 141)
    d142 = next(e['depth'] for e in lay if e['id'] == 142)
    surf = np.asarray(Image.open(os.path.join(wdir, 'layers_486', 'd%d.png' % d141)).convert('RGBA'))[:16, :16]
    body = np.asarray(Image.open(os.path.join(wdir, 'layers_486', 'd%d.png' % d142)).convert('RGBA'))
    bc = np.median(body[body[..., 3] == 255][:, :3], axis=0).astype(np.uint8)
    spike = pic490[200:216, 456:464].copy()  # one ceiling spike, under the ceiling's line (MC: 9 px apart)
    pipe = pic490[181:226, 1436:1474].copy()  # rim 37 px wide, body 30
    slab = pic490[263:274, 900:1001].copy()  # MC's chain-hung slab (its top row: 264, MC's ground's)
    keep = [x for x in range(pipe.shape[1]) if not 16 <= x < 22]  # (the middle of its shading: 32 px, the tiles')
    pipe = pipe[:, keep]
    # the bridge: its plank-and-brick strip repeats; the period from the strip itself
    b = pic496_bridge
    ys = np.where((b[..., 3] > 128).sum(1) > b.shape[1] * 0.6)[0]
    top = ys.min()
    strip = b[top:top + 16, 90:400]
    errs = [(np.abs(strip[:, p:p + 120].astype(np.float32) - strip[:, :120].astype(np.float32)).mean(), p)
            for p in range(10, 40)]
    period = min(errs)[1]
    return surf, bc, spike, pipe, strip[:, :period].copy(), slab


def numbers(L, name, ctype, vals, per=24, fmt='%d'):
    L.append('static const %s %s[%d] = {' % (ctype, name, len(vals)))
    for i in range(0, len(vals), per):
        L.append('  ' + ','.join(fmt % v for v in vals[i:i + per]) + ',')
    L.append('};')


def argb_words(rgba):
    a = rgba.astype(np.uint32)
    return [int(v) for v in ((a[..., 3] << 24) | (a[..., 0] << 16) | (a[..., 1] << 8) | a[..., 2]).flatten()]


def quantize(can):
    a = can.copy()
    a[a[..., 3] < 8] = 0
    a[a[..., 3] >= 248, 3] = 255
    q = Image.fromarray(a, 'RGBA').quantize(255, method=Image.Quantize.FASTOCTREE)
    pal = np.array(q.getpalette(rawmode='RGBA')[:255 * 4], np.uint32).reshape(-1, 4)
    idx = np.asarray(q).astype(np.int32) + 1
    idx[a[..., 3] == 0] = 0
    argb = (pal[:, 3] << 24) | (pal[:, 0] << 16) | (pal[:, 1] << 8) | pal[:, 2]
    return [0] + [int(v) for v in argb] + [0] * (256 - 1 - len(argb)), idx


def rle(idx):
    data, rows = [], [0]
    for row in idx:
        i, n = 0, len(row)
        while i < n:
            j = i
            while j < n and j - i < 255 and row[j] == row[i]:
                j += 1
            data += [j - i, int(row[i])]
            i = j
        rows.append(len(data))
    return data, rows


def build_area(n, a, f, wdir, art, gmask_pic, parts, log):
    W = a['width'] * 16
    K = a['kinds']
    x0, y0 = a['x0'], a['y0']
    PH, PW = art.shape[:2]
    lay = json.load(open(os.path.join(wdir, 'layers_%d' % f, 'layers.json')))
    back = np.zeros((PH, PW, 4), np.float32)
    scene = np.zeros((PH, PW, 4), np.float32)
    door = np.zeros((PH, PW, 4), np.float32)
    for e in lay:
        im = np.asarray(Image.open(os.path.join(wdir, 'layers_%d' % f, 'd%d.png' % e['depth'])).convert('RGBA'))
        if e['id'] in LAVA_IDS:
            continue
        dst = back if e['id'] in BACKDROP.get(n, ()) else door if e['id'] == DOOR_ID else scene
        over(dst, im, e['box'][0], e['box'][1])
    # MC's ground (its hitTest shape, its outline round it) and lava out of the props. Light neutral greys in the ground
    # are remembered (keep): a prop that reaches them gets them back below (the pillars, which run up behind MC's slab)
    rgb = scene[..., :3]
    red = (rgb[..., 0] > 0.35) & (rgb[..., 0] > 2 * rgb[..., 1]) & (rgb[..., 0] > 2 * rgb[..., 2])
    scene[red] = 0
    g = dilate(gmask_pic, 3)
    orig = scene.copy()
    keep = g & (scene[..., 3] > 0.5) & (np.abs(rgb[..., 0] - rgb[..., 1]) < 0.05) & \
        (np.abs(rgb[..., 1] - rgb[..., 2]) < 0.05) & (rgb.mean(2) >= 0.27)
    scene[g] = 0

    def to_level(pic):
        out = np.zeros((ROWS, W) + pic.shape[2:], pic.dtype)
        sx0, sy0 = max(0, x0), max(0, y0)
        sx1, sy1 = min(PW, x0 + W), min(PH, y0 + ROWS)
        if sx1 > sx0 and sy1 > sy0:
            out[sy0 - y0:sy1 - y0, sx0 - x0:sx1 - x0] = pic[sy0:sy1, sx0:sx1]
        return out

    back_l, scene_l, door_l, gm = to_level(back), to_level(scene), to_level(door), to_level(gmask_pic)
    orig_l, keep_l = to_level(orig), to_level(keep)

    def kind(r, c):
        return K[r][c] if 0 <= r < 27 and 0 <= c < a['width'] else '0'

    def surfaces(c, upward):  # tile tops (ground surfaces) or bottoms (ceilings) in column c, level px
        out = []
        for r in range(27):
            if kind(r, c) in SOLID:
                if not upward and kind(r - 1, c) not in SOLID:
                    out.append(r * 16)
                if upward and kind(r + 1, c) not in SOLID:
                    out.append((r + 1) * 16)
        return out

    # MC's ground art outside its hitTest shape (walls MC draws as outlines, filled here; area 3's ceiling): in big
    # pieces, the pixels of the ground's own colours (its exact pixel-art palette) go too
    art_l = to_level(art.astype(np.float32) / 255)
    q = (np.clip(art_l[..., :3], 0, 1) * 255 + 0.5).astype(np.int32)
    key = q[..., 0] << 16 | q[..., 1] << 8 | q[..., 2]
    vals, counts = np.unique(key[gm & (art_l[..., 3] > 0.99)], return_counts=True)
    ground_cols = set(vals[counts >= 50].tolist())
    for ys, xs in components(scene_l[..., 3] > 0.06):
        on_chain = any(kind(y // 16 + d, x // 16) == 'C' for y, x in zip(ys[::5], xs[::5]) for d in (0, 1))
        if on_chain:  # MC's chains and slab: the slab goes (drawn on its ledge tiles), the chains stay where MC hangs them
            gone = np.array([any(kind(y // 16, (x + d) // 16) == 'C' for d in (-8, 0, 8)) for y, x in zip(ys, xs)])
            scene_l[ys[gone], xs[gone]] = 0
        elif len(ys) >= 1500:  # (not the chains: the stone's own greys)
            sq = (np.clip(scene_l[ys, xs, :3], 0, 1) * 255 + 0.5).astype(np.int32)
            gone = np.isin(sq[:, 0] << 16 | sq[:, 1] << 8 | sq[:, 2], list(ground_cols))
            scene_l[ys[gone], xs[gone]] = 0
    pic = np.zeros((ROWS, W, 4), np.float32)
    pic[:] = back_l
    props = 0
    for ys, xs in components((scene_l[..., 3] > 0.06) | keep_l):
        if len(ys) < 25 or not (scene_l[ys, xs, 3] > 0.06).any():  # (no prop: MC's ground greys alone, a brown
            continue  # block's highlight; a pillar's cap over MC's slab is one)
        scene_l[ys, xs] = np.where(keep_l[ys, xs][:, None], orig_l[ys, xs], scene_l[ys, xs])
        cells = {kind(y // 16, x // 16) for y, x in zip(ys[::7], xs[::7])}
        if cells & set('vPLR'):  # MC's spikes, pipe, lava, bridge: drawn on their tiles instead
            continue
        if any(kind(y // 16 + 1, x // 16) == 'L' for y, x in zip(ys[::5], xs[::5])):  # (MC's flames over its lava)
            continue
        cols = {}
        for y, x in zip(ys, xs):
            lo, hi = cols.get(x, (y, y))
            cols[x] = (min(lo, y), max(hi, y))
        stand = sum(gm[min(ROWS - 1, hi + 1):min(ROWS, hi + 5), x].any() for x, (lo, hi) in cols.items())
        hang = sum(gm[max(0, lo - 4):max(0, lo), x].any() for x, (lo, hi) in cols.items())
        dys = []
        for x, (lo, hi) in cols.items():
            if stand >= hang and stand:
                cand = [s - (hi + 1) for s in surfaces(x // 16, False)]
            elif hang:
                cand = [s - lo for s in surfaces(x // 16, True)]
            else:  # (on MC's own small slab, not ground here: down onto the ground just below)
                cand = [s - (hi + 1) for s in surfaces(x // 16, False) if 0 <= s - (hi + 1) <= 16]
            cand = [d for d in cand if abs(d) <= 16]
            if cand:
                dys.append(min(cand, key=abs))
        dy = int(np.median(dys)) if dys else 0
        if any(kind((y + 1) // 16, x // 16) == 'C' for y, x in zip(ys, xs)):  # (chains: down to the slab, as MC's)
            dy = 0
        piece = np.zeros((ys.max() - ys.min() + 1, xs.max() - xs.min() + 1, 4), np.float32)
        piece[ys - ys.min(), xs - xs.min()] = scene_l[ys, xs]
        over(pic, (piece * 255 + 0.5).astype(np.uint8), int(xs.min()), int(ys.min()) + dy)
        props += 1
        log.append('area %d prop at %d,%d %dx%d: %s, moved %+d' % (n, xs.min(), ys.min(), piece.shape[1], piece.shape[0],
                                                                  'stands' if stand >= hang and stand else 'hangs' if hang else 'free', dy))
    # the red door: centred on its door tiles, standing on their bottom
    dcols = [(r, c) for r in range(27) for c in range(a['width']) if kind(r, c) == 'D']
    if dcols and door_l[..., 3].max() > 0:
        ys, xs = np.nonzero(door_l[..., 3] > 0.02)
        c = dcols[0][1]
        bottom = (max(r for r, cc in dcols if cc == c) + 1) * 16
        piece = door_l[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        dx = c * 16 + 8 - (xs.min() + xs.max() + 1) // 2
        over(pic, (piece * 255 + 0.5).astype(np.uint8), int(xs.min() + dx), bottom - piece.shape[0])
    surf, bc, spike, pipe, bridge, slab = parts
    for r in range(27):
        for c in range(a['width']):
            k = kind(r, c)
            X, Y = c * 16, r * 16
            if k == 'L':
                pic[Y:Y + 16, X:X + 16, :3] = bc / 255
                pic[Y:Y + 16, X:X + 16, 3] = 1
                if kind(r - 1, c) != 'L':  # (its surface: drawn by the game, MC's animation)
                    pic[Y:Y + 16, X:X + 16] = 0
            elif k == 'v':
                over(pic, spike, X, Y)
                over(pic, spike, X + 8, Y)
            elif k == 'R':
                seg = np.zeros((bridge.shape[0], 16, 4), np.uint8)
                for i in range(16):
                    seg[:, i] = bridge[:, (X + i) % bridge.shape[1]]
                over(pic, seg, X, Y)
    for r in range(27):  # MC's chain-hung slabs on their ledge tiles: its two ends, its middle repeated between
        cs = [c for c in range(a['width']) if kind(r, c) == 'C']
        runs = []
        for c in cs:
            if runs and runs[-1][1] == c - 1:
                runs[-1][1] = c
            else:
                runs.append([c, c])
        for c0, c1 in runs:
            w = (c1 - c0 + 1) * 16
            mid = slab[:, 40:56]
            body = np.concatenate([slab[:, :8]] + [mid] * ((w - 16) // 16 + 1), 1)[:, :w - 8]
            img = np.concatenate([body, slab[:, -8:]], 1)
            over(pic, img, c0 * 16, r * 16 - 1)  # (its outline row just above the tile's top)
    pcells = [(r, c) for r in range(27) for c in range(a['width']) if kind(r, c) == 'P']
    if pcells:
        top = min(r for r, c in pcells)
        c0 = min(c for r, c in pcells)
        h = (max(r for r, c in pcells) - top + 1) * 16
        img = pipe
        while img.shape[0] < h:  # (taller: one more body row)
            img = np.concatenate([img[:20], img[19:20], img[20:]])
        over(pic, img[:h], c0 * 16, top * 16)
    return pic, back_l, props


def main():
    swf, imgs, level_json, out_c, wdir = sys.argv[1:6]
    os.makedirs(wdir, exist_ok=True)
    level = json.load(open(level_json))
    hide = sum((['--hide', h] for h in HIDE), [])
    arts = [render(swf, imgs, [str(f)] + hide + ['--layers', os.path.join(wdir, 'layers_%d' % f)],
                   os.path.join(wdir, 'art_%d.png' % f)) for f in FRAMES]
    gmasks = [np.asarray(Image.open(os.path.join(wdir, 'art_%d_ground.png' % f))) > 127 for f in FRAMES]
    brown = brown_block(arts[1])
    lay5 = json.load(open(os.path.join(wdir, 'layers_496', 'layers.json')))
    d504 = next(e['depth'] for e in lay5 if e['id'] == 504)
    parts = pieces(wdir, arts[2], np.asarray(Image.open(os.path.join(wdir, 'layers_496', 'd%d.png' % d504)).convert('RGBA')))
    scale = 256 / STAGE_W
    wall = render(swf, imgs, ['1', '--only', '489:1', '--scale', '%.6f' % scale], os.path.join(wdir, 'wall.png'))
    period = 400 * scale
    wall = wall[:int(period) + 2, :256]
    L = ["// Mario Combat's own art for its story areas: generated by tools/scripts/mc_level_art.py (do not edit).",
         '#include "mc_level_art.h"', '']
    side, log = [], []
    for a, f, art, gm in zip(level, FRAMES, arts, gmasks):
        n = a['area']
        W = a['width'] * 16
        full = np.zeros((ROWS, W, 4), np.uint8)  # (the whole picture, for the stone sample)
        sx0, sy0 = max(0, a['x0']), max(0, a['y0'])
        sx1, sy1 = min(art.shape[1], a['x0'] + W), min(art.shape[0], a['y0'] + ROWS)
        full[sy0 - a['y0']:sy1 - a['y0'], sx0 - a['x0']:sx1 - a['x0']] = art[sy0:sy1, sx0:sx1]
        stone = stone_tile(full, a['kinds'])
        pic, back, props = build_area(n, a, f, wdir, art, gm, parts, log)
        can = (np.clip(pic, 0, 1) * 255 + 0.5).astype(np.uint8)
        sky = None
        if n in BACKDROP:  # the sky past the picture's ends: each row's colour at its first opaque backdrop pixel
            sky = np.zeros((ROWS, 3), np.uint8)
            op = back[..., 3] > 0.99
            first = None
            for y in range(ROWS):
                xs = np.where(op[y])[0]
                if len(xs):
                    sky[y] = (back[y, xs[0], :3] * 255).astype(np.uint8)
                    first = y if first is None else first
                elif y:
                    sky[y] = sky[y - 1]
            sky[:first] = sky[first]
        pal, idx = quantize(can)
        data, rows = rle(idx)
        numbers(L, 'kArea%dPal' % n, 'uint32', pal, 8, '0x%08x')
        numbers(L, 'kArea%dRows' % n, 'uint32', rows, 16)
        numbers(L, 'kArea%dRle' % n, 'uint8', data, 32)
        numbers(L, 'kArea%dStone' % n, 'uint32', argb_words(stone), 8, '0x%08x')
        if sky is not None:
            numbers(L, 'kArea%dSky' % n, 'uint32', [int(c[0]) << 16 | int(c[1]) << 8 | int(c[2]) for c in sky], 8, '0x%06x')
        L.append('')
        side.append({'area': n, 'w': W, 'h': ROWS, 'runs': len(data) // 2, 'props': props, 'sky': sky is not None})
        # preview: the picture over the sky or the wall (the ground itself is drawn by the game from the tiles)
        pv = np.zeros((ROWS, W, 3), np.float32)
        if sky is not None:
            pv[:] = sky[:, None, :]
        else:
            for y in range(ROWS):
                pv[y] = np.tile(wall[int(y % period), :, :3], (W // 256 + 1, 1))[:W]
        al = can[..., 3:4] / 255
        pv = can[..., :3] * al + pv * (1 - al)
        Image.fromarray(pv.astype(np.uint8)).save(os.path.join(wdir, 'area%d_art.png' % n))
    lava = lava_frames(swf, imgs, wdir, parts[1])
    numbers(L, 'kBrown', 'uint32', argb_words(brown), 8, '0x%08x')
    numbers(L, 'kLavaPx', 'uint32', sum((argb_words(fr) for fr in lava), []), 8, '0x%08x')
    numbers(L, 'kWallPx', 'uint32', argb_words(wall), 8, '0x%08x')
    L.append('')
    L.append('const McLevelPicture kMcLevelPictures[%d] = {' % len(level))
    for s in side:
        n = s['area']
        L.append('  { %d, %d, kArea%dPal, kArea%dRows, kArea%dRle, kArea%dStone, %s },' % (
            s['w'], s['h'], n, n, n, n, 'kArea%dSky' % n if s['sky'] else 'NULL'))
    L.append('};')
    L.append('const uint32 *const kMcLevelBrown = kBrown;')
    L.append('const uint32 *const kMcLevelLava = kLavaPx;')
    L.append('const McImage kMcLevelWall = { %d, %d, 1.0f, 0, 0, kWallPx };' % (wall.shape[1], wall.shape[0]))
    L.append('const float kMcLevelWallPeriod = %.4ff;' % period)
    open(out_c, 'w', newline='\n').write('\n'.join(L) + '\n')
    json.dump({'areas': side, 'wall': [wall.shape[1], wall.shape[0]], 'wall_period': period, 'props': log,
               'bytes': sum(s['runs'] * 2 for s in side)}, open(out_c + '.json', 'w'), indent=1)
    print(json.dumps(side))


if __name__ == '__main__':
    main()
