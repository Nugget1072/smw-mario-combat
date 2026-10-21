// Mario Combat's own move/impact sounds, mixed on top of the SNES audio output.
#pragma once
#include "../types.h"

typedef struct McSound { uint16 id; uint32 len; const int16 *pcm; } McSound;  // 22050 Hz mono
extern const McSound kMcSounds[];
extern const int kMcNumSounds;

void McAudio_Play(int sound_id);   // game thread
void McAudio_Mix(int16 *out, int frames, int channels, int freq, int volume);  // audio thread, volume 0-100

// Mario Combat's music (round 20: Bowser's, in the MC level): 4-bit IMA ADPCM, decoded as it plays, looping. It stands
// in for SMW's music, so it follows the game's volume, not SoundVolume.
typedef struct McMusic { uint32 samples, rate; const uint8 *adpcm; } McMusic;
extern const McMusic kMcBowserMusic;
void McAudio_StartMusic(const McMusic *m);  // game thread: from the start (a no-op if it is already playing)
void McAudio_StopMusic(int fade_frames);     // game thread: faded out over that many 60 Hz frames (0: at once)
bool McAudio_MusicPlaying(void);
void McAudio_MixMusic(int16 *out, int frames, int channels, int freq, int volume);  // audio thread, volume 0-100
