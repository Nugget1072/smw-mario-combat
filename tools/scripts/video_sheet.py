"""Contact sheet of an mp4 (round 19, checking the videos): N frames evenly spread (or from FIRST to LAST seconds),
each scaled to 256 wide, 6 across, with its time; also prints the stream's frame count, size and duration.
Usage: python -I video_sheet.py <in.mp4> <out.png> [--n 24] [--from S] [--to S]
"""
import os, re, subprocess, sys, tempfile

import imageio_ffmpeg
from PIL import Image, ImageDraw

src, out = sys.argv[1], sys.argv[2]
arg = lambda k, d: float(sys.argv[sys.argv.index(k) + 1]) if k in sys.argv else d
n = int(arg('--n', 24))
ff = imageio_ffmpeg.get_ffmpeg_exe()
info = subprocess.run([ff, '-i', src, '-map', '0:v', '-c', 'copy', '-f', 'null', '-'], capture_output=True,
                      text=True).stderr
dur = re.search(r'Duration: (\d+):(\d+):([\d.]+)', info)
total = int(dur.group(1)) * 3600 + int(dur.group(2)) * 60 + float(dur.group(3))
frames = re.findall(r'frame=\s*(\d+)', info)
size = re.search(r'Video: .*?, (\d+)x(\d+)', info)
audio = re.search(r'Audio: ([^\n]*)', info)
print('duration %.2f s, %s frames, %sx%s, audio: %s' % (total, frames[-1] if frames else '?', size.group(1),
                                                        size.group(2), audio.group(1) if audio else 'none'))
t0, t1 = arg('--from', 0.0), arg('--to', total - 0.05)
times = [t0 + (t1 - t0) * i / max(1, n - 1) for i in range(n)]
cols = 6
sheet = None
with tempfile.TemporaryDirectory() as td:
    for i, t in enumerate(times):
        p = os.path.join(td, '%03d.png' % i)
        subprocess.run([ff, '-y', '-ss', '%.3f' % t, '-i', src, '-frames:v', '1', p], capture_output=True)
        if not os.path.exists(p):
            continue
        im = Image.open(p).convert('RGB')
        im = im.resize((256, im.height * 256 // im.width), Image.NEAREST)
        if sheet is None:
            sheet = Image.new('RGB', (256 * cols, (im.height + 12) * ((n + cols - 1) // cols)), (0, 0, 0))
        x, y = (i % cols) * 256, (i // cols) * (im.height + 12)
        sheet.paste(im, (x, y))
        ImageDraw.Draw(sheet).text((x + 2, y + im.height), '%.2f s' % t, fill=(255, 255, 255))
sheet.save(out)
print('sheet: ' + out)
