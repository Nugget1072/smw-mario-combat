#include <SDL.h>
#include "mc_audio.h"
#include <string.h>

enum { kVoices = 8, kRate = 22050 };
// MC's samples are normalised to full scale: the hit sound peaks at 32767 with ~20000 RMS, while SMW's own kick/stomp
// effects peak around 10500 (RMS ~3400) in the SNES mix (measured with tools/pilots/audio_levels.txt). -14 dB brings
// a punch's overlapping swing + hit sounds to ~12000 peak / ~4600 RMS, level with SMW's effects; SoundVolume (0-100)
// scales on top of this.
enum { kMcGainNum = 1, kMcGainDen = 5 };

static struct { const McSound *snd; uint32 pos_fx; } g_voice[kVoices];  // pos in 16.16 fixed point
static SDL_SpinLock g_lock;

void McAudio_Play(int sound_id) {
  const McSound *s = NULL;
  for (int i = 0; i < kMcNumSounds; i++)
    if (kMcSounds[i].id == sound_id)
      s = &kMcSounds[i];
  if (!s)
    return;
  SDL_AtomicLock(&g_lock);
  int v = 0;
  for (int i = 0; i < kVoices; i++) {  // a free voice, else the one closest to finishing
    if (!g_voice[i].snd) { v = i; break; }
    if (g_voice[i].pos_fx > g_voice[v].pos_fx) v = i;
  }
  g_voice[v].snd = s;
  g_voice[v].pos_fx = 0;
  SDL_AtomicUnlock(&g_lock);
}

void McAudio_Mix(int16 *out, int frames, int channels, int freq, int volume) {
  if (freq <= 0 || volume <= 0)
    return;
  uint32 step = (uint32)(((uint64)kRate << 16) / freq);
  SDL_AtomicLock(&g_lock);
  for (int v = 0; v < kVoices; v++) {
    const McSound *s = g_voice[v].snd;
    if (!s)
      continue;
    uint32 p = g_voice[v].pos_fx;
    for (int f = 0; f < frames; f++) {
      uint32 i = p >> 16;
      if (i + 1 >= s->len) { s = NULL; break; }
      int32 a = s->pcm[i], b = s->pcm[i + 1], frac = p & 0xffff;
      int32 smp = (a + (int32)(((int64)(b - a) * frac) >> 16)) * volume * kMcGainNum / (100 * kMcGainDen);
      for (int c = 0; c < channels; c++) {
        int32 m = out[f * channels + c] + smp;
        out[f * channels + c] = (int16)(m < -32768 ? -32768 : m > 32767 ? 32767 : m);
      }
      p += step;
    }
    g_voice[v].snd = s;
    g_voice[v].pos_fx = p;
  }
  SDL_AtomicUnlock(&g_lock);
}

// ---- Music (one stream) ----
// MC's music is mixed at -12 dB: sound 560 has ~4300 RMS, SMW's castle music ~1000 in the SNES mix (measured on a
// recording of the MC level).
enum { kMusicGainDen = 4 };
static const int16 kImaSteps[89] = {
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
  130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060,
  1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
  7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 };
static const int8 kImaIndex[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static struct {
  const McMusic *m;
  uint32 pos;          // samples decoded so far (cur is sample pos - 1, next is sample pos)
  int pred, idx;       // the decoder's state
  int16 cur, next;
  uint32 frac;         // between cur and next, 16.16
  int fade, fade_len;  // output frames left of a fade out (0: none), its length
} g_music;

static int16 MusicDecode(void) {
  if (g_music.pos >= g_music.m->samples) {  // (looping: the stream starts again from its initial state)
    g_music.pos = 0, g_music.pred = 0, g_music.idx = 0;
  }
  uint8 b = g_music.m->adpcm[g_music.pos >> 1];
  int code = g_music.pos & 1 ? b >> 4 : b & 15, step = kImaSteps[g_music.idx], d = step >> 3;
  if (code & 4) d += step;
  if (code & 2) d += step >> 1;
  if (code & 1) d += step >> 2;
  int p = code & 8 ? g_music.pred - d : g_music.pred + d;
  g_music.pred = p < -32768 ? -32768 : p > 32767 ? 32767 : p;
  int i = g_music.idx + kImaIndex[code & 7];
  g_music.idx = i < 0 ? 0 : i > 88 ? 88 : i;
  g_music.pos++;
  return (int16)g_music.pred;
}

void McAudio_StartMusic(const McMusic *m) {
  SDL_AtomicLock(&g_lock);
  if (g_music.m != m || g_music.fade) {
    memset(&g_music, 0, sizeof(g_music));
    g_music.m = m;
    g_music.cur = MusicDecode();
    g_music.next = MusicDecode();
  }
  SDL_AtomicUnlock(&g_lock);
}

void McAudio_StopMusic(int fade_frames) {
  SDL_AtomicLock(&g_lock);
  if (fade_frames <= 0)
    g_music.m = NULL;
  else if (g_music.m && !g_music.fade)
    g_music.fade = g_music.fade_len = -fade_frames;  // (made into output frames by the next mix: its rate)
  SDL_AtomicUnlock(&g_lock);
}

bool McAudio_MusicPlaying(void) { return g_music.m != NULL; }

void McAudio_MixMusic(int16 *out, int frames, int channels, int freq, int volume) {
  if (freq <= 0)
    return;
  SDL_AtomicLock(&g_lock);
  if (g_music.m) {
    if (g_music.fade < 0)
      g_music.fade = g_music.fade_len = -g_music.fade * freq / 60;
    uint32 step = (uint32)(((uint64)g_music.m->rate << 16) / freq);
    for (int f = 0; f < frames && g_music.m; f++) {
      int32 smp = g_music.cur + (int32)(((int64)(g_music.next - g_music.cur) * g_music.frac) >> 16);
      smp = smp * volume / (100 * kMusicGainDen);
      if (g_music.fade) {
        smp = (int32)((int64)smp * g_music.fade / g_music.fade_len);
        if (--g_music.fade == 0)
          g_music.m = NULL;
      }
      for (int c = 0; c < channels; c++) {
        int32 v = out[f * channels + c] + smp;
        out[f * channels + c] = (int16)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
      }
      for (g_music.frac += step; g_music.frac >= 0x10000 && g_music.m; g_music.frac -= 0x10000) {
        g_music.cur = g_music.next;
        g_music.next = MusicDecode();
      }
    }
  }
  SDL_AtomicUnlock(&g_lock);
}