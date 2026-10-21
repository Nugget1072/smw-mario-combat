"""Turn a pilot recording into an mp4: the frames of `record <name>` (<dir>/<name>.bgr, 256 x H, BGR) and the sound of
`audio <name>` (<dir>/<name>_smw.wav, the SNES mix, and <name>_mc.wav, Mario Combat's sounds, as the game mixes them
for its speakers). Nothing is added or cut: every frame at the SNES's 60.0988 frames a second, scaled up 4x with
square pixels (nearest neighbour, so the pixel art stays sharp), H.264 + AAC.
Usage: python -I make_video.py <dir> <name> <height> <out.mp4>
(ffmpeg: the one bundled with imageio_ffmpeg in tools\\Python3.14.8.)
"""
import os, subprocess, sys
import imageio_ffmpeg

d, name, h, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
ff = imageio_ffmpeg.get_ffmpeg_exe()
cmd = [ff, '-y',
       '-f', 'rawvideo', '-pixel_format', 'bgr24', '-video_size', '256x%d' % h, '-framerate', '60.0988',
       '-i', os.path.join(d, name + '.bgr'),
       '-i', os.path.join(d, name + '_smw.wav'), '-i', os.path.join(d, name + '_mc.wav'),
       '-filter_complex', '[0:v]scale=iw*4:ih*4:flags=neighbor,format=yuv420p[v];'
                          '[1:a][2:a]amix=inputs=2:duration=first:normalize=0[a]',
       '-map', '[v]', '-map', '[a]',
       '-c:v', 'libx264', '-preset', 'slow', '-crf', '16', '-r', '60.0988',
       '-c:a', 'aac', '-b:a', '192k', '-movflags', '+faststart', out]
print(' '.join(cmd))
subprocess.run(cmd, check=True)
