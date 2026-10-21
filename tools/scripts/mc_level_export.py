"""Mario Combat's story areas -> SMW level data (round 20, the MC level in the level select).
Usage: python -I mc_level_export.py <MarioCombat.swf> <ffdec_images_dir> <render_dir> <out_c_file> <preview_dir>
<render_dir> holds mc_level_render.py's output for the five areas (area_<frame>.png, _ground.png, .json).

Per area: MC's ground (its `ground` clip, the shape the hero's hitTest uses) at SNES scale (1 px = 3 MC stage px) is
cut into 16x16 cells (a cell is solid when at least half of it is ground), at the grid offset that fits the ground's
edges best, and cropped to an SMW horizontal level: 27 rows (432 px) and whole screens from MC's left scroll limit.
Pockets of air the hero can't reach (MC draws its walls as outlines) are filled. Cells take a kind from MC's own
picture of the area: stone, brown block (MC's light brown blocks), lava (MC's red lava, from the bottom up), spikes
(listed per area). Stone becomes SMW's castle stone blocks, each block outlined with the 3x3 set 15D-165 (as the
castles' own blocks); brown blocks the used block 132; lava 004 / 005; spikes 15A (down; 159 up: the castle tileset's own, which hurt). The exits: a door (1F / 20)
or a pipe (137 / 138 over 135 / 136) where MC's exit marker is; area 4's lift (a row of brown blocks in the floor,
moved by mc_level.c) and the ledge with the two caged Koopas over it; area 5's bridge (solid tiles 130 between the
pillars, air under it; MC's bridge is drawn on them). The sidecar JSON also has every cell's kind as a letter
(mc_level_art.py draws by it). The objects (Koopas, the exit markers, the hero's spawn point, Bowser) are converted to level
pixels. Writes the C tables (mc_level_data.c, with MC's "press" arrow as an ARGB image) and per area a preview: the
tile kinds over MC's picture.
"""
import json, math, os, sys
import numpy as np
from PIL import Image, ImageDraw
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from swflib import Swf  # noqa: E402
from swfraster import raster_shape  # noqa: E402

ROWS = 27
LIFT_ROW, LIFT_TOP_ROW, LEDGE_ROW = 19, 10, 5  # (the top 2 rows are under SMW's status bar)  # area 4: the lift's row in the floor, where it stops, the cages' ledge
AREAS = [
    dict(frame=486, base=0x007, exit='door', note='castle wall over lava; the castle door at the end'),
    # (helpers: tiles SMW Mario needs, his jump being lower than MC Mario's: brown block stairs from the floor's end up to
    # the floating platform, and a step from it towards the ledge on the left)
    dict(frame=488, base=0x007, exit='door', shift=2, helpers=[(75, 22, 'brown'), (75, 21, 'brown'), (76, 22, 'brown'), (76, 21, 'brown'),
                                                                 (76, 20, 'brown'), (76, 19, 'brown'), (75, 16, 'brown')], note='inside: the climb to the red door'),
    # (left_extra: the level starts 2 columns before the grid's first, to show the first pillar as MC's camera does (it
    # shows a little left of lScroll); chains: MC's chain-hung slabs (picture x0, x1, top), one-way ledges here)
    dict(frame=490, base=0x007, exit='pipe', spikes=[(455, 196, 540, 214, 'down')], left_extra=2,
         chains=[(313, 437, 236), (900, 1000, 264)], note='the hall; the pipe at the end'),
    dict(frame=492, base=0x007, exit='lift', floor_top=565, note='the stairs and the lift; two Koopas drop in at the top'),
    # (air: the right pillar's top row, one above the bridge: flush with it, both ends can be fallen off as in MC)
    dict(frame=496, base=0x007, exit=None, bridge=True, air=[(30, 15), (31, 15), (32, 15), (33, 15)],
         note="Bowser's bridge"),
]

STONE, BROWN, LAVA, SPIKE_D, SPIKE_U, DOOR_T, DOOR_B, PIPE_TL, PIPE_TR, PIPE_L, PIPE_R, BRIDGE, CHAIN = range(1, 14)
SOLID = {STONE, BROWN, BRIDGE}
LETTER = {0: '0', STONE: 'S', BROWN: 'B', BRIDGE: 'R', LAVA: 'L', SPIKE_D: 'v', SPIKE_U: '^', DOOR_T: 'D', DOOR_B: 'D',
          PIPE_TL: 'P', PIPE_TR: 'P', PIPE_L: 'P', PIPE_R: 'P', CHAIN: 'C'}  # (the sidecar's kinds: mc_level_art.py)


def best_offset(mask):
    """The grid offset (0-15, 0-15) that gives the least ground cut by cell edges."""
    best = None
    for oy in range(16):
        for ox in range(16):
            m = mask[oy:, ox:]
            h, w = m.shape[0] // 16 * 16, m.shape[1] // 16 * 16
            cells = m[:h, :w].reshape(h // 16, 16, w // 16, 16).mean(axis=(1, 3))
            err = np.minimum(cells, 1 - cells).sum()
            if best is None or err < best[0]:
                best = (err, ox, oy)
    return best[1], best[2]


def fill_pockets(kinds, open_cells):
    """Air not connected (4-way, through air) to any of open_cells (around the objects): stone."""
    R, W = kinds.shape
    seen = np.zeros((R, W), bool)
    q = deque()
    for (r, c) in open_cells:
        if 0 <= r < R and 0 <= c < W and kinds[r, c] not in SOLID:
            seen[r, c] = True
            q.append((r, c))
    while q:
        r, c = q.popleft()
        for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            rr, cc = r + dr, c + dc
            # (not through lava, nor along the cropped top row: those link pockets the hero can't get into)
            if 0 < rr < R and 0 <= cc < W and not seen[rr, cc] and kinds[rr, cc] not in SOLID and kinds[rr, cc] != LAVA:
                seen[rr, cc] = True
                q.append((rr, cc))
    for r in range(R):
        for c in range(W):
            if r > 0 and kinds[r, c] not in SOLID and kinds[r, c] != LAVA and not seen[r, c]:
                kinds[r, c] = STONE


def export_area(n, a, rdir):
    f = a['frame']
    info = json.load(open(os.path.join(rdir, 'area_%d.json' % f)))
    pic = np.asarray(Image.open(os.path.join(rdir, 'area_%d.png' % f)).convert('RGBA'), np.float32)
    mask = np.asarray(Image.open(os.path.join(rdir, 'area_%d_ground.png' % f))) > 127
    ox0, oy0 = info['origin_stage_px']
    marks = info['marks']

    def px(m):  # a mark's position in picture px
        return (m['x'] - ox0) / 3, (m['y'] - oy0) / 3

    lsc = [px(m) for m in marks if m['name'] == 'lScroll']
    rsc = [px(m) for m in marks if m['name'] == 'rScroll']
    left = lsc[0][0] if lsc else 0
    right = rsc[0][0] if rsc else mask.shape[1]
    gx, gy = best_offset(mask)
    col0 = int((left - gx) // 16) - a.get('left_extra', 0)
    x0 = gx + col0 * 16
    width = int(math.ceil((right - x0) / 16))
    width = int(math.ceil(width / 16) * 16)
    if 'floor_top' in a:  # the given picture y lands at the lift's row
        y0 = gy + int(round((a['floor_top'] - gy) / 16)) * 16 - LIFT_ROW * 16
    else:  # the lowest ground's bottom at the level's bottom
        ys = np.where(mask.any(1))[0]
        row_end = int(math.ceil((ys.max() + 1 - gy) / 16))
        y0 = gy + (row_end - ROWS) * 16
    kinds = np.zeros((ROWS, width), np.uint8)
    for r in range(ROWS):
        for c in range(width):
            X, Y = x0 + c * 16, y0 + r * 16
            if X < 0 or Y < 0 or X + 16 > mask.shape[1] or Y + 16 > mask.shape[0]:
                continue
            cm, cp = mask[Y:Y + 16, X:X + 16], pic[Y:Y + 16, X:X + 16]
            rgb = cp[..., :3][cp[..., 3] > 128]
            if cm.mean() >= 0.5:
                rg, gb = rgb[:, 0] - rgb[:, 1], rgb[:, 1] - rgb[:, 2]  # MC's brown blocks: R-G and G-B about equal (lava glow:
                brown = len(rgb) and np.mean((rg > 5) & (gb > 5) & (np.abs(rg - gb) < 8)) > 0.5  # R-G much bigger)
                kinds[r, c] = BROWN if brown else STONE
            elif len(rgb) > 128 and np.mean((rgb[:, 0] > 110) & (rgb[:, 1] < 70) & (rgb[:, 2] < 70)) > 0.5:
                kinds[r, c] = LAVA
    # lava only from the bottom row up (MC's red door is no lava)
    for c in range(width):
        r = ROWS - 1
        if kinds[r, c] != LAVA:
            kinds[:, c][kinds[:, c] == LAVA] = 0
            continue
        while r >= 0 and kinds[r, c] == LAVA:
            r -= 1
        top = r + 1
        kinds[:top, c][kinds[:top, c] == LAVA] = 0
    for sx0, sy0, sx1, sy1, d in a.get('spikes', []):
        for r in range(ROWS):
            for c in range(width):
                X, Y = x0 + c * 16 + 8, y0 + r * 16 + 8
                if sx0 <= X < sx1 and sy0 - 8 <= Y < sy1 + 8 and kinds[r, c] == 0:
                    kinds[r, c] = SPIKE_D if d == 'down' else SPIKE_U

    def lvl(p):  # picture px -> level px
        return p[0] - x0, p[1] - y0

    objs = []
    for m in marks:
        k = m['kind']
        if k in ('koopa', 'exit', 'bowser') or m['name'] == 'spawnPoint':
            x, y = lvl(px(m))
            objs.append({'kind': 'spawn' if m['name'] == 'spawnPoint' else k, 'x': round(x), 'y': round(y)})
    # air the hero can reach: around every object
    open_cells = [(int(o['y'] // 16) + dr, int(o['x'] // 16)) for o in objs for dr in (-1, 0)]
    if not a.get('bridge'):
        fill_pockets(kinds, open_cells)
    extra = {'exit_x': 0, 'exit_y': 0, 'lift': (0, 0, 0, 0)}
    # MC's scroll limits (its camera never shows past them; its hero stays 22 stage px inside: DefineSprite_440's
    # `while(_x - 22 < lScroll._x)`), level px. Area 3's camera may show its first pillar left of lScroll
    # (left_extra: as MC's window does)
    L, R = left - x0, right - x0
    extra['bounds'] = (0 if a.get('left_extra') else int(round(L)), int(round(R)), int(round(L + 22 / 3)),
                       int(round(R - 22 / 3)))
    exits = [o for o in objs if o['kind'] == 'exit']

    def ground_row(c, y):
        r = max(0, int(y // 16))
        while r < ROWS and kinds[r, c] not in SOLID:
            r += 1
        return r

    if a['exit'] == 'door' and exits:
        e = exits[0]
        c = int(e['x'] // 16)
        r = ground_row(c, e['y'])
        kinds[r - 2, c], kinds[r - 1, c] = DOOR_T, DOOR_B
        extra['exit_x'], extra['exit_y'] = c * 16, (r - 2) * 16
    elif a['exit'] == 'pipe' and exits:
        e = exits[0]
        # MC's pipe is part of its ground: the solid cells under the marker, down to the ledge it stands on
        c = int(round((e['x'] - 16) / 16))
        top = ground_row(c, e['y'])
        r = top
        while r < ROWS and kinds[r, c] in SOLID and kinds[r, c + 2 if c + 2 < width else c] not in SOLID:
            r += 1
        r = max(r, top + 2)
        kinds[top, c], kinds[top, c + 1] = PIPE_TL, PIPE_TR
        for rr in range(top + 1, r):
            kinds[rr, c], kinds[rr, c + 1] = PIPE_L, PIPE_R
        extra['exit_x'], extra['exit_y'] = c * 16, top * 16
    elif a['exit'] == 'lift':
        # MC's lift (sprite 492: 747 x 48 stage px at the floor's top, x 632-880 in the picture): 16 brown blocks
        lx0 = int(round((632 - x0) / 16))
        for c in range(lx0, lx0 + 16):
            kinds[LIFT_ROW, c] = BROWN
        extra['lift'] = (lx0 * 16, (lx0 + 16) * 16, LIFT_ROW * 16, LIFT_TOP_ROW * 16)
        if exits:
            extra['exit_x'], extra['exit_y'] = int(exits[0]['x']) - 8, LIFT_ROW * 16 - 32
        # the caged Koopas (MC: y 65 in the picture, far above): on a ledge at LEDGE_ROW, each in a stone cage
        top_k = [o for o in objs if o['kind'] == 'koopa' and o['y'] < 0]
        cx0 = int(round((565 - x0) / 16)); cx1 = int(round((980 - x0) / 16))
        for c in range(cx0, cx1):
            kinds[LEDGE_ROW, c] = STONE
        for o in top_k:
            c = int(o['x'] // 16)
            o['y'] = LEDGE_ROW * 16
            for rr in range(LEDGE_ROW - 3, LEDGE_ROW):
                kinds[rr, c - 1] = kinds[rr, c + 1] = STONE
            kinds[LEDGE_ROW - 3, c] = STONE
    if a.get('shift'):
        # down by k rows, out from under SMW's status bar (the top 32 px of the screen at the level's top): the bottom
        # rows go (floor and lava), the lava under the pits stays in the last row
        k = a['shift']
        lava = kinds[ROWS - 1].copy()
        kinds[k:] = kinds[:ROWS - k].copy()
        kinds[:k] = 0
        for c in range(width):
            if lava[c] == LAVA and kinds[ROWS - 1, c] not in SOLID:
                kinds[ROWS - 1, c] = LAVA
        y0 -= 16 * k
        for o in objs:
            o['y'] += 16 * k
        extra['exit_y'] += 16 * k
    for c, r, k in a.get('helpers', []):
        kinds[r, c] = {'brown': BROWN, 'stone': STONE}[k]
    for px0, px1, top in a.get('chains', []):  # (MC's thin slabs: the cells under at least half of one, its top's row)
        r = int(round((top - y0) / 16))
        for c in range(width):
            X = x0 + c * 16
            if min(X + 16, px1) - max(X, px0) >= 8:
                kinds[r, c] = CHAIN
                if kinds[r + 1, c] == STONE and (r + 2 >= ROWS or kinds[r + 2, c] != STONE):
                    kinds[r + 1, c] = 0  # (a stone cell from the slab's lower half)
    if a.get('bridge'):
        # Bowser's bridge: the ground's top row between MC's pillars is a bridge of brown blocks, with air under it;
        # the pillars (where MC draws stone) stay
        for c in range(width):
            rs = [r for r in range(ROWS) if kinds[r, c] in SOLID]
            if not rs:
                continue
            top = rs[0]
            X = x0 + c * 16
            colp = pic[y0 + top * 16 + 20:y0 + top * 16 + 60, X:X + 16]
            rgb = colp[..., :3][colp[..., 3] > 128] if colp.size else np.zeros((0, 3))
            stone = len(rgb) > 200 and np.mean(rgb[:, 1] > rgb[:, 2] + 8) > 0.5
            if not stone:
                kinds[top, c] = BRIDGE
                for r in rs[1:]:
                    kinds[r, c] = 0
    for c, r in a.get('air', []):
        kinds[r, c] = 0
    return dict(n=n, cfg=a, kinds=kinds, width=width, x0=x0, y0=y0, objs=objs, pic=pic, **extra)


def autotile(kinds):
    """Map16 numbers: stone outlined per block (3x3 set), brown 132, lava 004 / 005, spikes, door, pipe."""
    R, W = kinds.shape
    t = np.full((R, W), 0x25, np.uint16)
    st = kinds == STONE
    fixed = {BROWN: 0x132, BRIDGE: 0x130, CHAIN: 0x100, SPIKE_D: 0x15A, SPIKE_U: 0x159,  # (the castle tileset's spikes: they hurt, 00F120)
             DOOR_T: 0x01F, DOOR_B: 0x020, PIPE_TL: 0x137,
             PIPE_TR: 0x138, PIPE_L: 0x135, PIPE_R: 0x136}
    for r in range(R):
        for c in range(W):
            k = kinds[r, c]
            if k == STONE:
                up = r > 0 and st[r - 1, c]
                dn = r + 1 >= R or st[r + 1, c]
                lf = c == 0 or st[r, c - 1]
                rt = c + 1 >= W or st[r, c + 1]
                row = 0 if not up else (2 if not dn else 1)
                col = 0 if not lf else (2 if not rt else 1)
                if not lf and not rt:
                    col = 1
                t[r, c] = (0x15D, 0x15E, 0x15F, 0x160, 0x161, 0x162, 0x163, 0x164, 0x165)[row * 3 + col]
            elif k == LAVA:
                t[r, c] = 0x004 if r == 0 or kinds[r - 1, c] != LAVA else 0x005
            elif k in fixed:
                t[r, c] = fixed[k]
    return t


def arrow_image(swf_path, img_dir):
    """MC's "press" arrow (shape 443, white, pointing down) at art scale: ARGB words."""
    swf = Swf(swf_path)
    sh = swf.shapes[443]
    x0, x1, y0, y1 = [v / 20 for v in sh['bounds']]
    k = 1 / 3
    w, h = max(1, math.ceil((x1 - x0) * k)), max(1, math.ceil((y1 - y0) * k))
    rgba = raster_shape(sh, {}, (k / 20, 0, 0, k / 20, -x0 * k, -y0 * k), w, h)
    a8 = (np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint32)
    argb = (a8[..., 3] << 24) | (a8[..., 0] << 16) | (a8[..., 1] << 8) | a8[..., 2]
    return w, h, argb.flatten()


def write_c(areas, arrow, out_c):
    L = ["// Mario Combat's story areas as SMW level data: generated by tools/scripts/mc_level_export.py (do not edit).",
         '#include "mc_level_data.h"', '']
    for a in areas:
        n = a['n'] + 1
        tiles = autotile(a['kinds'])
        L.append('static const uint16 kArea%dTiles[%d] = {  // %d columns x %d rows, row by row' % (
            n, tiles.size, a['width'], ROWS))
        flat = tiles.flatten()
        for i in range(0, len(flat), 16):
            L.append('  ' + ', '.join('0x%03x' % v for v in flat[i:i + 16]) + ',')
        L.append('};')
        L.append('static const McLevelObj kArea%dObjs[] = {' % n)
        for o in a['objs']:
            L.append('  { kMcObj_%s, %d, %d },' % (o['kind'].capitalize(), o['x'], o['y']))
        L.append('};')
        L.append('')
    L.append('const McLevelArea kMcLevelAreas[kMcLevelAreaCount] = {')
    for a in areas:
        n = a['n'] + 1
        c = a['cfg']
        L.append('  // %d: %s' % (n, c['note']))
        L.append('  { 0x%03X, %d, kArea%dTiles, kArea%dObjs, %d, kMcExit_%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d },' % (
            c['base'], a['width'], n, n, len(a['objs']), (c['exit'] or 'none').capitalize(), a['exit_x'], a['exit_y'],
            *a['lift'], *a['bounds']))
    L.append('};')
    L.append('')
    w, h, px = arrow
    L.append('static const uint32 kArrowPx[%d] = {' % len(px))
    for i in range(0, len(px), 8):
        L.append('  ' + ', '.join('0x%08x' % v for v in px[i:i + 8]) + ',')
    L.append('};')
    L.append('const McImage kMcLevelArrow = { %d, %d, 1.0f, 0, 0, kArrowPx };' % (w, h))
    open(out_c, 'w', newline='\n').write('\n'.join(L) + '\n')


def preview(a, path):
    pic = Image.fromarray(a['pic'].astype(np.uint8), 'RGBA')
    W = a['width'] * 16
    im = Image.new('RGBA', (W, ROWS * 16), (0, 0, 0, 255))
    im.paste(pic, (-a['x0'], -a['y0']), pic)
    ov = Image.new('RGBA', im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    col = {STONE: (80, 220, 80, 90), BROWN: (230, 160, 60, 110), BRIDGE: (230, 200, 60, 110), CHAIN: (120, 200, 255, 160), LAVA: (255, 40, 40, 110), SPIKE_D: (255, 255, 0, 160),
           SPIKE_U: (255, 255, 0, 160), DOOR_T: (200, 0, 200, 200), DOOR_B: (200, 0, 200, 200),
           PIPE_TL: (0, 200, 200, 200), PIPE_TR: (0, 200, 200, 200), PIPE_L: (0, 160, 160, 200),
           PIPE_R: (0, 160, 160, 200)}
    for r in range(ROWS):
        for c in range(a['width']):
            k = a['kinds'][r, c]
            if k:
                d.rectangle((c * 16, r * 16, c * 16 + 15, r * 16 + 15), fill=col[k], outline=(255, 255, 255, 60))
    for o in a['objs']:
        d.ellipse((o['x'] - 4, o['y'] - 4, o['x'] + 4, o['y'] + 4), outline=(0, 255, 255, 255))
        d.text((o['x'] + 5, o['y'] - 6), o['kind'], fill=(0, 255, 255, 255))
    for s in range(0, W, 256):
        d.line((s, 0, s, ROWS * 16), fill=(255, 255, 255, 120))
    for x in range(0, W, 64):  # level px ticks (for waypoints)
        d.text((x + 1, 1), str(x), fill=(255, 255, 0, 255))
    for y in range(64, ROWS * 16, 64):
        d.text((1, y + 1), str(y), fill=(255, 255, 0, 255))
        d.line((0, y, 6, y), fill=(255, 255, 0, 255))
    Image.alpha_composite(im, ov).convert('RGB').save(path)


def main():
    swf_path, img_dir, rdir, out_c, pdir = sys.argv[1:6]
    os.makedirs(pdir, exist_ok=True)
    areas = []
    for n, a in enumerate(AREAS):
        ar = export_area(n, a, rdir)
        areas.append(ar)
        print('area %d (frame %d): %d x %d tiles, picture origin %d,%d; %d objects; exit %s at %d,%d' % (
            n + 1, a['frame'], ar['width'], ROWS, ar['x0'], ar['y0'], len(ar['objs']), a['exit'], ar['exit_x'],
            ar['exit_y']))
        preview(ar, os.path.join(pdir, 'area%d_tiles.png' % (n + 1)))
    write_c(areas, arrow_image(swf_path, img_dir), out_c)
    json.dump([{'area': a['n'] + 1, 'x0': a['x0'], 'y0': a['y0'], 'width': a['width'],
                'solid': [''.join('1' if a['kinds'][r, c] in SOLID else 'L' if a['kinds'][r, c] == LAVA else '0'
                                  for c in range(a['width'])) for r in range(ROWS)],
                'kinds': [''.join(LETTER[int(a['kinds'][r, c])] for c in range(a['width'])) for r in range(ROWS)]}
               for a in areas],
              open(out_c + '.json', 'w'), indent=1)


if __name__ == '__main__':
    main()
