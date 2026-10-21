// Mario Combat's own pictures of its story areas (round 20), drawn instead of SMW's tile layers in the MC level
// (McLevel_ArtLine). Generated data: tools/scripts/mc_level_art.py -> mc_level_art.c.
#ifndef SMW_MC_MC_LEVEL_ART_H_
#define SMW_MC_MC_LEVEL_ART_H_
#include "../types.h"
#include "mc_data.h"

typedef struct McLevelPicture {
  uint16 w, h;            // level px (the area's width, 27 rows)
  const uint32 *pal;      // 256 ARGB colours (0: transparent)
  const uint32 *rows;     // h + 1 offsets into rle
  const uint8 *rle;       // each row: (count 1-255, colour) pairs
  const uint32 *stone;    // 16 x 16 ARGB of the area's stone, level-aligned: stone[(ly & 15) * 16 + (lx & 15)]
  const uint32 *sky;      // RGB per row where nothing is drawn (areas 1 and 5); NULL: MC's castle wall behind (its BG
                          // clip, locked to the screen)
} McLevelPicture;

extern const McLevelPicture kMcLevelPictures[5];
extern const uint32 *const kMcLevelBrown;  // MC's brown block, 16 x 16 ARGB
extern const uint32 *const kMcLevelLava;   // MC's lava surface (sprite 141): 6 frames of 16 x 16 ARGB, 5 MC frames each
extern const McImage kMcLevelWall;          // the castle wall, screen-wide; repeats every kMcLevelWallPeriod rows
extern const float kMcLevelWallPeriod;

#endif  // SMW_MC_MC_LEVEL_ART_H_
