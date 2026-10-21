// Mario Combat hero inside Super Mario World: glue between mc_core (50 Hz, MC stage pixels) and the SMW
// player engine (60 Hz, SMW pixels). One SMW pixel = 3 MC stage pixels (MC's art is SNES-size art drawn 3x).
#pragma once
#include <stdbool.h>
#include "../types.h"

bool McSmw_Enabled(void);          // Mario Combat Mario is the active character right now
// Switch between Mario Combat Mario and SMW Mario (applied at the next safe point in a level).
void McSmw_ToggleCharacter(void);
bool McSmw_CharacterIsMc(void);    // the character after any pending switch
// Replaces HandlePlayerPhysics/CheckPowerUpSpecificPlayerAttacks/HandlePlayerPhysics_InAir/SetPlayerPose in
// normal player state. Returns false when SMW's own physics must run this frame (Yoshi, climbing, swimming...).
bool McSmw_PlayerPhysics(void);
// Called once per level frame from PlayerDraw instead of writing Mario's OAM tiles. obj_priority = OAM priority bits.
void McSmw_Draw(int obj_priority, bool visible);
// Called at the start of every frame (main loop).
void McSmw_FrameBegin(void);
// Called once per level frame right after the player state routine: keeps the MC clock running in states that
// do not use MC physics (SMW moves the player, MC animates).
void McSmw_AfterPlayerState(void);
void McSmw_OnPlayerHurt(void);
// SMW's cape on MC Mario (PlayerDraw_CapeOnly): whether to draw it, and where relative to SMW Mario's anchor.
bool McSmw_CapeVisible(void);
void McSmw_OnPowerUpCollected(void);  // mushroom / flower / feather / star picked up: life bar back to full
// Riding Yoshi, climbing or cape-flying: PlayerDraw shows SMW's own poses (MC has no such art); MC keeps the HUD.
bool McSmw_UseSmwSprite(void);
// The player as Mario Combat's enemy script sees `_root.hero` (level px; MC stage px per tick for xspeed).
typedef struct McHeroInfo {
  double feet_x, feet_y;           // _x, _y
  double x0, y0, x1, y1;           // hero.bound (MC Mario) or SMW Mario's body box
  double xspeed;                   // hero.xSpeed (SMW Mario: his speed in MC units)
  int xscale;                      // 100 facing right, -100 left
  bool alive, falling, mc;         // life > 0; moving down (for stomps); Mario Combat Mario or SMW Mario
} McHeroInfo;
void McSmw_GetHeroInfo(McHeroInfo *hi);
// The next DamagePlayer_Hurt comes from an MC enemy's heavy punch (hit2): knocked back further and up.
void McSmw_SetNextHitHeavy(bool heavy);
// An MC enemy's attack reaches the player, by SMW's rule for enemy fire: the star protects, riding Yoshi Mario loses
// Yoshi, otherwise DamagePlayer_Hurt (MC Mario's life and knockback, or SMW Mario's power-down).
void McSmw_EnemyHurtsPlayer(void);
// The next DamagePlayer_Hurt knocks MC Mario this way (-1 left, +1 right; 0 = away from the sprite / backwards).
void McSmw_SetNextHitDir(int dir);
double McSmw_GetLife(void);  // MC Mario's life bar (0-10)
// While a punch is being applied: the GivePoints level (stomp-chain style) to use instead of the routine's own, or -1.
int McSmw_PointsOverride(void);
int McSmw_GroundJumps(void);  // how many times MC Mario has jumped off the ground (it counts up on each take-off)
// MC Mario's punch on Morton / Roy / Ludwig as a stomp (smw_01.c, next to their stomp code): 0 not now, 1 hurt,
// 2 bounced off (in his shell, or on a wall or the ceiling).
int McSmw_PunchMortonRoyLudwig(uint8 k);
// SMW's SolidSpriteBlock (flying ? blocks, platforms, message boxes...): the sprite's box as the routine sees it with
// the player x it pushes him out to on either side, and a push out of its side (player_left: the player is left of
// the sprite). SMW sets no blocked flag for either.
void McSmw_SolidSpriteBox(uint8 k, uint16 stop_right_of, uint16 stop_left_of);
void McSmw_SolidSpritePush(bool player_left);
// A platform solid on top only, with its own routine (the brown chained platform): its contact box, and sy, the y the
// player stands on as on a SolidSpriteBlock sprite at that y (his feet at sy + 1, caught while they are above sy + 8).
void McSmw_TopPlatformBox(uint8 k, int x0, int y0, int w, int h, int sy);
// The same for a platform of the level's own (no sprite: the Mario Combat level's lift), recorded each frame it is there.
void McSmw_LevelPlatformBox(int x0, int y0, int w, int h, int sy);
// Pilot: the top of the platform sprite k last recorded (the x middle, the sy above), false if none this frame.
bool McSmw_DebugPlatformTop(uint8 k, int *x, int *sy);
// A sprite just knocked the player sideways (player_xspeed: Reznor's and the Koopa Kids' rebounds, a Chargin' Chuck's
// stomp knockback, a spiky enemy's): MC Mario takes that speed on his next frame (MC sets his speed every frame).
void McSmw_SideLaunch(void);
// Layer-1 map16 tile number at a level pixel (-1 outside the level).
int McSmw_LevelMap16At(int x, int y);
void McSmw_CapeOffset(int *dx, int *dy);
// Life-bar mode: replaces DamagePlayer_Hurt (returns true when handled).
bool McSmw_HandleHurt(void);
void McSmw_OnPlayerDeath(void);
// Masks Y out of SMW's merged run/grab bit so Y is only Mario Combat's punch (X still grabs and runs).
void McSmw_FilterInputs(void);
// Debug/pilot info
void McSmw_DebugState(char *buf, int size);
void McSmw_DebugFindTiles(int lo, int hi);
void McSmw_DebugSetPos(int x, int y);
void McSmw_DebugTileAt(int x, int y);
void McSmw_DebugAttackFrames(void);
void McSmw_OnTeleported(void);
// The level select starts a level: the hero starts fresh there (full Mario Combat life), as from the map.
void McSmw_OnLevelSelect(void);
int McSmw_DebugHeroFrame(void);  // MC hero timeline frame (1 idle, 9 jump, 18 attack, 19 hurt...)
int McSmw_DebugBossPunches(void);  // punches that have hurt a boss so far
