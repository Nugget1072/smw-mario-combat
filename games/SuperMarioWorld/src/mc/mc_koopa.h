// "Mario Combat Koopas" cheat: SMW's Koopa Troopas (sprites 00-07) become Mario Combat's enemy Koopa
// (DefineSprite 163): its art and animations, its walking / jumping / attacking script and its hit reactions, moved
// through SMW's sprite collision. The SMW sprite slot stays a Koopa for the rest of SMW (loading, despawning, shells,
// fireballs, Yoshi), only its normal-state routine, its death fall and its drawing are replaced.
#pragma once
#include <stdbool.h>
#include "../types.h"

bool McKoopa_Enabled(void);                 // the cheat is on (and the sprite is a Koopa: see McKoopa_Main)
// SprStatus08 / SprStatus02_Dead hooks: return true when the Mario Combat Koopa handled the sprite this frame.
bool McKoopa_Main(uint8 k);
bool McKoopa_Dead(uint8 k);
bool McKoopa_IsActive(uint8 k);
bool McKoopa_IsDead(uint8 k);
// A Mario Combat Mario punch / kick of MC hit level 1-6 (from the glue's attack boxes). Returns true if it counted.
bool McKoopa_Punch(uint8 k, int level);
// SMW Mario's cape (or a bounced block, r14 != 53) / cape dive (r14 == 53): an MC hit with invincibility after, instead of
// SMW's knock-out. Returns true if this sprite is an MC Koopa (SMW's own reaction skipped).
bool McKoopa_CapeHit(uint8 k, uint8 r14);
void McKoopa_FrameBegin(void);              // once per frame (MC's 50 Hz clock inside SMW's 60)
void McKoopa_SpritesBegin(void);            // ProcessNormalSprites start: counts the frames sprites run
void McKoopa_OnSpriteInit(uint8 k);         // SprStatus01_Init: a new sprite in slot k
int McKoopa_DebugInfo(uint8 k, char *buf, int n);  // pilot `sprites`: life / animation state, or 0 if not one
