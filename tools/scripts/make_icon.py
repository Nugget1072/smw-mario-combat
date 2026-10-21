"""The game's Windows icon (round 20): Mario Combat Mario in his fighting stance (MC's idle frame, shape 166, 20 x 28
pixel art from mc_export.py's output), on a transparent background. No ROM data (the exe is shipped).
Usage: python -I make_icon.py <shape_166.png> <out.ico> <preview.png>

Pixel art stays crisp: every size is a whole-number nearest-neighbour scale of the sprite. Where the whole sprite would
come out small (or doesn't fit: 16-24 px), the icon is his upper body (cap, face, fist) at the next scale up instead,
so the little sizes Windows shows in lists, the taskbar and the title bar still read as Mario.
Writes the .ico (16, 20, 24, 32, 40, 48, 64, 128, 256) and a preview of every size.
"""
import sys
from PIL import Image

SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]


def icon(sprite, n):
    w, h = sprite.size
    s = max(1, n // h)
    if h * s <= n and h * s >= 0.8 * n:
        part = sprite                                  # the whole sprite fills the icon
    else:
        if h * s <= n and w * (s + 1) <= n:
            s += 1                                     # small whole sprite: the upper body one scale up
        rows, cols = n // s, min(w, n // s)
        # (the columns round his head and fist: centred on the opaque pixels of those rows)
        top = sprite.crop((0, 0, w, rows))
        l, _, r, _ = top.getbbox()
        x0 = max(0, min(w - cols, (l + r - cols) // 2))
        part = sprite.crop((x0, 0, x0 + cols, rows))
    big = part.resize((part.width * s, part.height * s), Image.NEAREST)
    out = Image.new('RGBA', (n, n), (0, 0, 0, 0))
    out.alpha_composite(big, ((n - big.width) // 2, n - big.height if part is sprite else 0))
    return out


def main():
    src, out_ico, preview = sys.argv[1:4]
    sprite = Image.open(src).convert('RGBA')
    sprite = sprite.crop(sprite.getbbox())
    images = {n: icon(sprite, n) for n in SIZES}
    images[256].save(out_ico, format='ICO', sizes=[(n, n) for n in SIZES], append_images=[images[n] for n in SIZES[:-1]])
    W = sum(SIZES) + 10 * len(SIZES)
    for name, bg in (('', (200, 200, 200, 255)), ('_dark', (32, 32, 32, 255))):
        pv = Image.new('RGBA', (W, 256 + 20), bg)
        x = 0
        for n in SIZES:
            pv.alpha_composite(images[n], (x, 256 - n))
            x += n + 10
        pv.save(preview.replace('.png', name + '.png'))
    print('icon %s: %s' % (out_ico, SIZES))


if __name__ == '__main__':
    main()
