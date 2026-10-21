// Data tables exported from Mario Combat by tools/scripts/mc_export.py (see mc_data.c).
#pragma once
#include "../types.h"

typedef struct McImage {
  uint16 w, h;
  float k;        // image pixels per stage pixel (1 = vector art traced at art scale, 1/3 = 3x pre-scaled bitmap)
  float ox, oy;   // shape-local stage px of image pixel (0,0)
  const uint32 *px;  // ARGB, straight alpha
} McImage;

enum { MC_OP_PLACE = 0, MC_OP_REMOVE = 1, MC_OP_SOUND = 2 };
enum { MC_PF_MOVE = 1, MC_PF_CHAR = 2, MC_PF_MATRIX = 4, MC_PF_CXFORM = 8, MC_PF_RATIO = 16, MC_PF_NAME = 32 };

typedef struct McOp {
  uint8 kind, flags;
  uint16 depth;
  uint16 ch;      // character id (or sound id for MC_OP_SOUND)
  int16 name;     // index into kMcNames or -1
  uint16 ratio;
  float m[6];     // a, b, c, d, tx, ty (tx/ty in stage px)
  int16 cx[8];    // colour transform: mul r,g,b,a (256 = 1.0), add r,g,b,a
} McOp;

typedef struct McSpriteDef { uint16 id, nframes, first_frame; } McSpriteDef;  // ops of frame f: kMcFrameStart[first_frame+f-1 .. +f)
typedef struct McLabel { uint16 sprite, frame; const char *name; } McLabel;
typedef struct McCharDef { uint8 type; uint16 index; } McCharDef;  // type 1 = shape (image index), 2 = sprite (sprite index)
typedef struct McShapeBounds { uint16 id; float xmin, xmax, ymin, ymax; } McShapeBounds;

extern const McImage kMcImages[];
extern const int kMcNumImages;
extern const McOp kMcOps[];
extern const uint16 kMcFrameStart[];
extern const McSpriteDef kMcSprites[];
extern const int kMcNumSprites;
extern const McLabel kMcLabels[];
extern const int kMcNumLabels;
extern const char *const kMcNames[];
extern const int kMcNumNames;
extern const int kMcMaxCharId;
extern const McCharDef kMcChars[];
extern const McShapeBounds kMcShapeBounds[];
