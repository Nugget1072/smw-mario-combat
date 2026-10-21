"""Compare run-up from standstill: Mario Combat reference vs the SMW port (distance in SMW px over real time).

Usage: python -I compare_run.py <ref trace.csv> <ref_start_tick> <port log.csv> <port_start_frame> [duration_ms]
"""
import csv, sys

ref_path, rt0, port_path, pf0 = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4])
dur = float(sys.argv[5]) if len(sys.argv) > 5 else 800
ref = [r for r in csv.DictReader(open(ref_path)) if int(r["tick"]) >= rt0]
x0 = float(ref[0]["x"])
ra = [((int(r["tick"]) - rt0) * 20.0, (float(r["x"]) - x0) / 3) for r in ref]
port = [r for r in csv.DictReader(open(port_path)) if int(r["frame"]) >= pf0]
px0 = int(port[0]["px"]) + int(port[0]["psubx"]) / 256
pa = [((int(r["frame"]) - pf0) * 1000 / 60, int(r["px"]) + int(r["psubx"]) / 256 - px0) for r in port]


def at(arr, t):
    for (t1, a), (t2, b) in zip(arr, arr[1:]):
        if t1 <= t <= t2:
            return a + (b - a) * (t - t1) / (t2 - t1)
    return None


for t in range(100, int(dur) + 1, 100):
    r, p = at(ra, t), at(pa, t)
    print(f"t={t:4d} ms  ref {r:7.2f} px  port {p:7.2f} px  diff {p - r:+.2f}")
best = None
for shift in range(-60, 61, 2):
    e = [abs(p - at(ra, t - shift)) for t, p in pa if t <= dur and at(ra, t - shift) is not None]
    if e and (best is None or sum(e) / len(e) < best[1]):
        best = (shift, sum(e) / len(e), max(e))
print(f"after best time alignment ({best[0]:+d} ms): max error {best[2]:.2f} px, mean {best[1]:.2f} px")
# top speed over the last 300 ms
tr = [(t, x) for t, x in ra if dur - 300 <= t <= dur]
tp = [(t, x) for t, x in pa if dur - 300 <= t <= dur]
print(f"top speed: ref {(tr[-1][1] - tr[0][1]) / (tr[-1][0] - tr[0][0]) * 1000:.1f} px/s  "
      f"port {(tp[-1][1] - tp[0][1]) / (tp[-1][0] - tp[0][0]) * 1000:.1f} px/s")
