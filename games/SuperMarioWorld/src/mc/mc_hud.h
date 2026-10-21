// Mario Combat's life bar HUD (DefineSprite 454), exported by tools/scripts/mc_hud_export.py.
#pragma once
#include "../types.h"

typedef struct McHud {
  uint16 w, h;
  uint16 bar_x0, bar_x1;   // horizontal extent of the red bar (masked by life / 10 like MC's mask_mc)
  const uint32 *px;        // ARGB
  const uint8 *is_bar;     // 1 where the pixel belongs to the red bar
} McHud;
extern const McHud kMcHud;
