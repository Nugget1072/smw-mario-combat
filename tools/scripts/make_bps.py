"""Create a BPS patch (source -> target), e.g. smw.sfc -> smw_assets.dat, so a build can ship without ROM data.

Usage: python -I make_bps.py <source> <target> <out.bps>
Greedy encoder: SourceCopy / TargetCopy for matches >= 6 bytes (8-byte hash index), TargetRead for the rest.
"""
import sys, zlib


def enc(n):
    out = bytearray()
    while True:
        x = n & 0x7F
        n >>= 7
        if n == 0:
            out.append(0x80 | x)
            return out
        out.append(x)
        n -= 1


def main():
    src = open(sys.argv[1], "rb").read()
    dst = open(sys.argv[2], "rb").read()
    K = 8
    sidx = {}
    for i in range(len(src) - K + 1):
        sidx.setdefault(src[i:i + K], []).append(i)
    tidx = {}
    out = bytearray(b"BPS1") + enc(len(src)) + enc(len(dst)) + enc(0)
    lit = bytearray()
    srel = trel = 0
    i = 0

    def flush():
        nonlocal lit
        if lit:
            out.extend(enc(((len(lit) - 1) << 2) | 1))
            out.extend(lit)
            lit = bytearray()

    def best_match(index, data, i, limit_end=None):
        bl, bp = 0, -1
        for p in index.get(dst[i:i + K], [])[-16:]:
            n = 0
            m = min(len(dst) - i, len(data) - p)
            if limit_end is not None:
                m = min(m, limit_end - p)
            while n < m and data[p + n] == dst[i + n]:
                n += 1
            if n > bl:
                bl, bp = n, p
        return bl, bp

    while i < len(dst):
        # SourceRead: same offset in source
        n = 0
        while i + n < len(dst) and i + n < len(src) and src[i + n] == dst[i + n]:
            n += 1
        sl, sp = best_match(sidx, src, i) if i + K <= len(dst) else (0, -1)
        tl, tp = best_match(tidx, dst, i, limit_end=i) if i + K <= len(dst) else (0, -1)
        if max(n, sl, tl) >= 6:
            flush()
            if n >= sl and n >= tl:
                out.extend(enc(((n - 1) << 2) | 0)); ln = n
            elif sl >= tl:
                d = sp - srel
                out.extend(enc(((sl - 1) << 2) | 2)); out.extend(enc((abs(d) << 1) | (1 if d < 0 else 0)))
                srel = sp + sl; ln = sl
            else:
                d = tp - trel
                out.extend(enc(((tl - 1) << 2) | 3)); out.extend(enc((abs(d) << 1) | (1 if d < 0 else 0)))
                trel = tp + tl; ln = tl
        else:
            lit.append(dst[i]); ln = 1
        for j in range(i, i + ln):
            if j + K <= len(dst):
                tidx.setdefault(dst[j:j + K], []).append(j)
        i += ln
    flush()
    out.extend(zlib.crc32(src).to_bytes(4, "little"))
    out.extend(zlib.crc32(dst).to_bytes(4, "little"))
    out.extend(zlib.crc32(out).to_bytes(4, "little"))
    open(sys.argv[3], "wb").write(out)
    print(f"{sys.argv[3]}: {len(out)} bytes for {len(dst)} byte target")


main()
