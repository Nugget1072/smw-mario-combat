// In-game settings menu, FPS counter and short notices, drawn in Super Mario World's own menu font (GFX28).
#pragma once
#include <stdbool.h>
#include <SDL.h>
#include "types.h"

void Menu_Init(void);                 // after the game assets are loaded (decompresses the font from GFX28)
bool Menu_IsOpen(void);
void Menu_Open(void);
void Menu_Close(void);
// Input while the menu is open. Return true when consumed.
bool Menu_HandleKey(SDL_Keycode key, SDL_Keymod mod, bool pressed, bool repeat);
bool Menu_HandlePadButton(int sdl_button, bool pressed);
void Menu_HandlePadAxis(int axis, int value);
void Menu_Update(uint32 now_ms);      // key repeat for held pad directions, capture timeout
// Draw the menu / FPS counter / notice onto the finished frame (ARGB, render_scale = pixels per SNES pixel).
void Menu_DrawOverlay(uint8 *pixels, int pitch, int width, int height, int render_scale);
void Menu_FrameShown(uint32 now_ms);  // FPS counting
void Menu_Notice(const char *text);   // e.g. "MARIO COMBAT MARIO"

// Provided by main.c
void Main_SetFullscreen(int mode);    // 0 windowed, 1 fullscreen (desktop), 2 fullscreen (exclusive)
void Main_SetWindowScale(int scale);
int Main_MaxWindowScale(void);
void Main_ApplyVideo(void);           // aspect ratio, filtering, vsync
void Main_ApplyVolume(void);
void Main_Quit(void);
// Mono / background-mute settings on a finished S16 stereo mix (the audio callback, and the pilot's WAV dumps).
void Main_PostProcessAudio(int16 *buf, int frames);
void Main_SetFocusForTest(bool focused);  // pilot `focus 0/1`: as if the window lost / got the focus
