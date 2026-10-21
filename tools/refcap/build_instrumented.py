"""Build the instrumented Mario Combat projector used for reference captures.

1. FFDec compiles tools/refcap/driver_frame1.as over the root frame-1 DoAction of a copy of the SWF.
2. The result is re-appended to the original projector stub with the 0xFA123456 trailer.
Usage: python -I build_instrumented.py <MarioCombat.exe> <ffdec.jar> <work_dir>
Output: <work_dir>/MarioCombat_refcap.exe (the original exe is never modified)
"""
import os, shutil, struct, subprocess, sys

exe, ffdec, work = sys.argv[1], sys.argv[2], sys.argv[3]
here = os.path.dirname(os.path.abspath(__file__))
data = open(exe, "rb").read()
magic, length = struct.unpack_from("<II", data, len(data) - 8)
assert magic == 0xFA123456
stub, swf = data[:len(data) - 8 - length], data[len(data) - 8 - length:len(data) - 8]
os.makedirs(work, exist_ok=True)
src_swf = os.path.join(work, "orig.swf")
open(src_swf, "wb").write(swf)
scripts = os.path.join(work, "import", "scripts", "frame_1")
os.makedirs(scripts, exist_ok=True)
shutil.copy(os.path.join(here, "driver_frame1.as"), os.path.join(scripts, "DoAction.as"))
out_swf = os.path.join(work, "refcap.swf")
r = subprocess.run(["java", "-jar", ffdec, "-importScript", src_swf, out_swf, os.path.join(work, "import")],
                   capture_output=True, text=True)
print(r.stdout[-2000:], r.stderr[-2000:])
new = open(out_swf, "rb").read()
assert new != swf, "script import did not change the SWF"
out_exe = os.path.join(work, "MarioCombat_refcap.exe")
open(out_exe, "wb").write(stub + new + struct.pack("<II", 0xFA123456, len(new)))
print("wrote", out_exe, len(new))
