// True-colour sprite overlay merged into the SNES PPU output per scanline.
// Mario Combat's art has more colours than an SNES palette, so the hero is drawn here with a z value equal to the
// OBJ priority SMW gives the player. ppu.c composes each line and then lets overlay pixels win wherever their z is
// at least the PPU pixel's z, so the hero still goes behind pipes and foreground layers exactly like OAM Mario.
#pragma once
#include "../types.h"
#include "mc_data.h"

enum { kMcOverlayW = 256, kMcOverlayH = 240 };

void McOverlay_BeginFrame(void);
// m maps image pixels -> screen pixels. z = PPU z-buffer value of a sprite pixel at the wanted priority.
void McOverlay_DrawImage(const McImage *im, const float *m, const int16 *cx, uint16 z);
void McOverlay_DrawRect(int x0, int y0, int x1, int y1, uint32 argb, uint16 z);
// Optional per-pixel colour transform for the following DrawImage calls (NULL = none).
void McOverlay_SetRecolor(uint32 (*fn)(uint32 argb));
// Called by the PPU after a line is composed. dst points at screen x = 0, zbuf at the matching z entries.
// mirror: the line was flipped left <-> right by the PPU (mirror-mode cheat): read the overlay row mirrored.
// subz: the sub screen's z entries when the sprites are only on the sub screen (else NULL).
void McOverlay_ApplyLine(int line, uint32 *dst, const uint16 *zbuf, const uint16 *subz, int brightness, bool mirror);
bool McOverlay_Active(void);
// Sprite ordering: the hero is inserted into the PPU sprite line at Mario's OAM slot (see mc_overlay.c).
void McOverlay_SetHeroOamIndex(int sprite);   // 0..127, -1 = after all sprites
int McOverlay_HeroOamIndex(void);
bool McOverlay_FillObjLine(int line, uint16 *obj);  // obj = sprite z buffer at screen x 0; returns true if drawn
uint16 McOverlay_SpriteZ(int obj_priority);
