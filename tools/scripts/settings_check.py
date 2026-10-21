# Round 13 settings check: reads the settings_r13 pilot's WAV dumps (SNES mix *_smw.wav, Mario Combat sounds *_mc.wav)
# and prints, per file, how loud it is and how much its left and right channels differ.
# Expected: st_* stereo (left != right somewhere), mono_* left == right everywhere, muted_* silent.
# Usage: python -I tools/scripts/settings_check.py [work/pilot/settings_r13]
import os, struct, sys, wave

d = sys.argv[1] if len(sys.argv) > 1 else os.path.join('work', 'pilot', 'settings_r13')
for name in ('st', 'mono', 'muted'):
    for part in ('smw', 'mc'):
        p = os.path.join(d, f'{name}_{part}.wav')
        if not os.path.exists(p):
            print(p, 'missing')
            continue
        w = wave.open(p)
        n = w.getnframes()
        s = struct.unpack('<%dh' % (n * 2), w.readframes(n))
        left, right = s[0::2], s[1::2]
        peak = max(max(abs(v) for v in s), 0) if s else 0
        diff = sum(1 for a, b in zip(left, right) if a != b)
        print(f'{name}_{part}: {n} frames, peak {peak}, samples with left != right: {diff}')
