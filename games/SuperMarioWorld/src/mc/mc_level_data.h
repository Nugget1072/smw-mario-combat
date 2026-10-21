// Mario Combat's story areas as SMW level data (tables in mc_level_data.c, made by tools/scripts/mc_level_export.py).
#ifndef SMW_MC_MC_LEVEL_DATA_H_
#define SMW_MC_MC_LEVEL_DATA_H_
#include "../types.h"
#include "mc_data.h"

enum { kMcLevelAreaCount = 5, kMcLevelRows = 27 };
enum { kMcObj_Spawn, kMcObj_Koopa, kMcObj_Exit, kMcObj_Bowser };
// How an area is left: an SMW door / pipe at MC's exit marker, MC's lift (area 4), or no exit (Bowser's bridge).
enum { kMcExit_None, kMcExit_Door, kMcExit_Pipe, kMcExit_Lift };

typedef struct McLevelObj {
  uint8 kind;
  int16 x, y;  // level pixels (MC's registration point: the hero's spawn point, a Koopa's feet, the marker's middle)
} McLevelObj;

typedef struct McLevelArea {
  uint16 base_level;          // the vanilla room loaded under the area (tileset, palette, background, music)
  uint16 width;               // in tiles (whole screens)
  const uint16 *tiles;        // map16 numbers, width x kMcLevelRows, row by row
  const McLevelObj *objs;
  uint8 n_objs;
  uint8 exit;                 // kMcExit_*
  int16 exit_x, exit_y;       // the exit (door / pipe / lift marker): level px of its tile's top left
  int16 lift_x0, lift_x1, lift_y, lift_top;  // the lift (area 4): its tile columns' px span, its top row's px y now
                                             // and where it stops
  int16 cam_x0, cam_x1;       // MC's scroll limits: the camera shows no further (level px)
  int16 wall_x0, wall_x1;     // MC's invisible walls: Mario's middle stays between (level px)
} McLevelArea;

extern const McLevelArea kMcLevelAreas[kMcLevelAreaCount];
extern const McImage kMcLevelArrow;  // MC's bobbing "press" arrow (shape 443)

#endif  // SMW_MC_MC_LEVEL_DATA_H_
