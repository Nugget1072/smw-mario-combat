"""Compare jump arcs: Mario Combat reference trace vs the SMW port's pilot log.

Usage: python -I compare_jump.py <ref trace.csv> <ref_start_tick> <port log.csv> <port_start_frame> <out.png>
Both arcs are converted to height above the take-off point in SMW pixels (MC px / 3) over real time (ms).
"""
import csv, sys
from PIL import Image, ImageDraw

ref_path, ref_t0, port_path, port_f0, out = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4]), sys.argv[5]
ref = [r for r in csv.DictReader(open(ref_path)) if int(r["tick"]) >= ref_t0]
y0 = float(ref[0]["y"])
ra = []
for r in ref:
    t = (int(r["tick"]) - ref_t0) * 20.0
    h = (y0 - float(r["y"])) / 3
    ra.append((t, h))
    if max(x[1] for x in ra) > 2 and r["onGround"] == "true" and h <= 0.5:
        break
port = [r for r in csv.DictReader(open(port_path)) if int(r["frame"]) >= port_f0]
py0 = int(port[0]["py"]) + int(port[0]["psuby"]) / 256
pa = []
for r in port:
    t = (int(r["frame"]) - port_f0) * 1000 / 60
    h = py0 - (int(r["py"]) + int(r["psuby"]) / 256)
    pa.append((t, h))
    if max(x[1] for x in pa) > 2 and h <= 0.5:
        break
ref_apex = max(h for _, h in ra); port_apex = max(h for _, h in pa)
ref_air = ra[-1][0]; port_air = pa[-1][0]
print(f"apex: ref {ref_apex:.2f} px  port {port_apex:.2f} px   ({100 * (port_apex / ref_apex - 1):+.1f}%)")
print(f"airtime: ref {ref_air:.0f} ms  port {port_air:.0f} ms   ({port_air - ref_air:+.0f} ms)")
# max abs height error at matching times (linear interpolation of the reference)
def at(arr, t):
    for (t1, h1), (t2, h2) in zip(arr, arr[1:]):
        if t1 <= t <= t2:
            return h1 + (h2 - h1) * (t - t1) / (t2 - t1) if t2 > t1 else h1
    return None
errs = [abs(h - at(ra, t)) for t, h in pa if at(ra, t) is not None]
print(f"max |height error| over the arc: {max(errs):.2f} px, mean {sum(errs) / len(errs):.2f} px")
best = None
for shift in range(-60, 61, 2):  # ms; the port moves one frame after its physics step, like SMW's own Mario
    e = [abs(h - at(ra, t - shift)) for t, h in pa if at(ra, t - shift) is not None]
    if e and (best is None or sum(e) / len(e) < best[1]):
        best = (shift, sum(e) / len(e), max(e))
print(f"after best time alignment ({best[0]:+d} ms): max error {best[2]:.2f} px, mean {best[1]:.2f} px")
W, H, S = 800, 300, 3
img = Image.new("RGB", (W, H), (20, 20, 30)); d = ImageDraw.Draw(img)
def pts(arr):
    return [(10 + t * (W - 20) / max(ref_air, port_air), H - 20 - h * S) for t, h in arr]
d.line(pts(ra), fill=(255, 80, 80), width=3); d.line(pts(pa), fill=(80, 200, 255), width=1)
d.text((10, 5), "red: Mario Combat (Flash 50 fps)  blue: SMW port (60 fps)", fill=(255, 255, 255))
img.save(out)
