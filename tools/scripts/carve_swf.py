"""Carve the SWF out of a Flash Player projector exe.

Projector layout: <player exe><swf bytes><u32 magic 0xFA123456><u32 swf length>.
Writes the SWF as found (CWS/FWS/ZWS) and a decompressed FWS copy next to it.
Usage: python -I carve_swf.py <projector.exe> <out_dir>
"""
import json, os, struct, sys, zlib

exe, out = sys.argv[1], sys.argv[2]
data = open(exe, "rb").read()
magic, length = struct.unpack_from("<II", data, len(data) - 8)
assert magic == 0xFA123456, hex(magic)
start = len(data) - 8 - length
swf = data[start:start + length]
sig, ver, decl = swf[:3], swf[3], struct.unpack_from("<I", swf, 4)[0]
os.makedirs(out, exist_ok=True)
open(os.path.join(out, "MarioCombat.swf"), "wb").write(swf)
if sig == b"CWS":
    body = zlib.decompress(swf[8:])
    fws = b"FWS" + bytes([ver]) + struct.pack("<I", 8 + len(body)) + body
else:
    fws = swf
open(os.path.join(out, "MarioCombat_fws.swf"), "wb").write(fws)

# Header: RECT (bit-packed), then frame rate (8.8 fixed) and frame count.
nbits = fws[8] >> 3
rect_bits = 5 + 4 * nbits
rect_bytes = (rect_bits + 7) // 8
bits = int.from_bytes(fws[8:8 + rect_bytes], "big")
vals = []
for i in range(4):
    shift = rect_bytes * 8 - 5 - (i + 1) * nbits
    v = (bits >> shift) & ((1 << nbits) - 1)
    if v >> (nbits - 1):
        v -= 1 << nbits
    vals.append(v / 20)
p = 8 + rect_bytes
fps = fws[p + 1] + fws[p] / 256
frames = struct.unpack_from("<H", fws, p + 2)[0]
info = {"projector": exe, "swf_offset": start, "swf_length": length, "signature": sig.decode(),
        "swf_version": ver, "uncompressed_length": decl, "stage_px": vals, "fps": fps, "frames": frames,
        "player_exe_length": start}
json.dump(info, open(os.path.join(out, "swf_info.json"), "w"), indent=2)
print(json.dumps(info, indent=2))
