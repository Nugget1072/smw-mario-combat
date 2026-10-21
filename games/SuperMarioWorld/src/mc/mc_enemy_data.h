// Types of the Mario Combat enemy data (Koopa: DefineSprite 163, Bowser: 532 with its flame 555 and shockwave 558),
// exported by tools/scripts/mc_enemy_export.py. Every clip frame is baked into a full display list.
#pragma once
#include "mc_data.h"

typedef struct McKoopaClip { uint16 id, nframes, first_frame; } McKoopaClip;  // frames: k*Frames[first_frame..]
// One clip frame: its display items, the sound it starts (0 = none) and the bounds of its visible shapes (clip-local
// stage px; hit1 / hit2 / bound boxes left out), for hitTest-style boxes.
typedef struct McKoopaFrame { uint16 first_item, nitems, sound; float bounds[4]; } McKoopaFrame;
// kind 0 = image (ref = k*Images index), 1 = nested clip (ref = k*Clips index). name: 0 none, 1 walk, 2 hurt_mc,
// 3 attack, 4 hit1, 5 hit2, 6 bar, 7 bound. age = frames since this placement started (a nested clip's own frame - 1).
typedef struct McKoopaItem { uint8 kind; uint16 ref; uint8 name; uint16 age; float m[6]; int16 cx[8]; } McKoopaItem;
typedef struct McKoopaLabel { uint16 clip, frame; const char *name; } McKoopaLabel;
enum { kMcNameNone, kMcNameWalk, kMcNameHurt, kMcNameAttack, kMcNameHit1, kMcNameHit2, kMcNameBar, kMcNameBound };
