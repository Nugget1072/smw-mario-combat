// "King Koopas" cheat: SMW's Koopa Troopas (sprites 00-07) become Mario Combat's final boss, Bowser
// (DefineSprite 532): his art, his fire breath (DefineSprite 555), the shockwave of his jumps (DefineSprite 558), his
// health bar and his script, moved through SMW's sprite collision. As with the MC Koopas cheat, the SMW slot stays a
// Koopa for the rest of SMW (loading, despawning); his normal-state routine, his drawing and the ways SMW would
// knock a Koopa out are replaced. Never on together with the MC Koopas cheat (the menu switches the other one off).
#pragma once
#include <stdbool.h>
#include "../types.h"

bool McBowser_Enabled(void);
// SprStatus08 / SprStatus02_Dead hooks: return true when Bowser handled the sprite this frame.
bool McBowser_Main(uint8 k);
bool McBowser_Dead(uint8 k);
bool McBowser_IsActive(uint8 k);
bool McBowser_IsDead(uint8 k);
// A Mario Combat Mario punch / kick of MC hit level 1-6 (the glue's attack boxes). Returns true if it counted.
bool McBowser_Punch(uint8 k, int level);
// SMW Mario's weapons: a fireball (dir = its direction), the cape / a bounced block / a cape ground pound
// (CheckPlayerAttackToNormalSpriteColl's r14). Return true when the sprite is Bowser (the hit is handled).
bool McBowser_FireballHit(uint8 k, int dir);
bool McBowser_CapeHit(uint8 k, uint8 r14);
// Frames an MC enemy can't be hit again after SMW Mario's cape (or a bounced block) hit it: about MC's hurt animation
// (SMW's own 8 let a spinning cape hit every 8 frames). MC Mario's attacks are not affected.
enum { kMcCapeInvincible = 32 };
// His body box (level px) for SMW's sprite clipping (fireballs, cape, punches). False if the sprite is not Bowser.
bool McBowser_Clipping(uint8 k, int *x, int *y, int *w, int *h);
void McBowser_FrameBegin(void);    // once per frame (MC's 50 Hz clock inside SMW's 60)
void McBowser_SpritesBegin(void);  // ProcessNormalSprites start: counts the frames sprites run
void McBowser_OnSpriteInit(uint8 k);  // SprStatus01_Init: a new sprite in slot k
void McBowser_AfterSprites(void);  // after the sprite loop: his flames and shockwaves move, hit Mario and draw
int McBowser_DebugInfo(uint8 k, char *buf, int n);  // pilot `sprites`
