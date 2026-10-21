"""Decompress an SMW GFX file (LZ2, as DecompressTo in smw_00.c) from smw_assets.dat and render it as tiles.

Usage: python -I dump_smw_gfx.py <smw_assets.dat> <gfx_index_hex> <bpp 2|4> <out.png>
Assets: asset 0 is kGraphicsPtrs (an index/memblk array). Reads the asset table like main.c LoadAssets.
"""
import struct, sys
from PIL import Image, ImageDraw


def load_assets(path):
    d = open(path, "rb").read()
    n = struct.unpack_from("<I", d, 80)[0]
    off = 88 + n * 4 + struct.unpack_from("<I", d, 84)[0]
    assets = []
    for i in range(n):
        size = struct.unpack_from("<I", d, 88 + i * 4)[0]
        off = (off + 3) & ~3
        assets.append(d[off:off + size])
        off += size
    return assets


def memblk_index(blk, i):
    """Port of FindIndexInMemblk (src/util.c)."""
    end = len(blk) - 2
    f = struct.unpack_from("<H", blk, end)[0]
    mx = f & 0xFFF
    if f & 0x4000:
        end -= 2
        next_mx = struct.unpack_from("<H", blk, end)[0]
        if next_mx >= 256:
            end -= mx * 2 + 2
            i = struct.unpack_from("<H", blk, end + i * 2)[0]
        else:
            end -= mx + 1
            i = blk[end + i]
        mx = next_mx
    if f & 0x8000:
        left = mx * 2 if i == 0 else mx * 2 + struct.unpack_from("<H", blk, i * 2 - 2)[0]
        right = end if i == mx else mx * 2 + struct.unpack_from("<H", blk, i * 2)[0]
    else:
        left = mx * 4 if i == 0 else mx * 4 + struct.unpack_from("<I", blk, i * 4 - 4)[0]
        right = end if i == mx else mx * 4 + struct.unpack_from("<I", blk, i * 4)[0]
    return blk[left:right]


def lz2(src):
    out = bytearray(); p = 0
    while True:
        tag = src[p]; p += 1
        if tag == 0xFF:
            return bytes(out)
        if tag & 0xE0 == 0xE0:
            cmd = (tag << 3) & 0xE0; ln = ((tag & 3) << 8 | src[p]) + 1; p += 1
        else:
            cmd = tag & 0xE0; ln = (tag & 0x1F) + 1
        if cmd == 0:
            out += src[p:p + ln]; p += ln
        elif cmd & 0x80:
            o = src[p] << 8 | src[p + 1]; p += 2
            for i in range(ln):
                out.append(out[o + i])
        elif cmd & 0x40:
            if cmd & 0x20:
                v = src[p]; p += 1
                for i in range(ln):
                    out.append((v + i) & 255)
            else:
                a, b = src[p], src[p + 1]; p += 2
                for i in range(ln):
                    out.append(a if i % 2 == 0 else b)
        else:
            v = src[p]; p += 1
            out += bytes([v]) * ln


def main():
    assets = load_assets(sys.argv[1])
    idx = int(sys.argv[2], 16); bpp = int(sys.argv[3])
    raw = lz2(memblk_index(assets[0], idx))
    tb = 8 * bpp
    ntiles = len(raw) // tb
    cols = 16
    img = Image.new("RGB", (cols * 9 * 3, ((ntiles + cols - 1) // cols) * 9 * 3), (40, 40, 60))
    pal = [(0, 0, 0), (255, 255, 255), (160, 160, 160), (90, 90, 90)] + [(i * 16, i * 16, i * 16) for i in range(4, 16)]
    px = img.load()
    for t in range(ntiles):
        tx, ty = (t % cols) * 9, (t // cols) * 9
        for y in range(8):
            for x in range(8):
                v = 0
                for pl in range(bpp):
                    byte = raw[t * tb + (pl // 2) * 16 + y * 2 + (pl & 1)]
                    v |= ((byte >> (7 - x)) & 1) << pl
                for sy in range(3):
                    for sx in range(3):
                        px[(tx + x) * 3 + sx, (ty + y) * 3 + sy] = pal[v]
    d = ImageDraw.Draw(img)
    for t in range(0, ntiles, cols):
        d.text((0, (t // cols) * 27), f"{t:02X}", fill=(255, 0, 0))
    img.save(sys.argv[4])
    open(sys.argv[4] + ".bin", "wb").write(raw)
    print(len(raw), "bytes", ntiles, "tiles")


main()
