// Settings-menu cheats and the "100% save in file 3" option.
#pragma once
#include "types.h"

// Once per frame, before the game frame runs: applies the active cheats.
void Cheats_Frame(void);
bool Cheats_NoDamage(void);
bool Cheats_InfiniteLife(void);
bool Cheats_MoonJump(void);
bool Cheats_PSpeed(void);
bool Cheats_ReplayLevels(void);
bool Cheats_NoKnockback(void);
bool Cheats_MirrorActive(void);          // the level picture is mirrored right now
void Cheats_FilterInputs(void);          // after PollJoypadInputs: Left <-> Right while mirrored
bool Cheats_OneHit(void);
void Cheats_EnemyDefeated(uint8 k);      // Coins from enemies: pop a coin out of sprite k
bool Cheats_SuperPunch(void);
bool Cheats_DoubleJump(void);
bool Cheats_GroundPound(void);
bool Cheats_WallJump(void);              // SMW Mario only (MC Mario wall jumps on his own)
bool Cheats_Slippery(void);              // in a level with the slippery cheat on (MC Mario's ice is in mc_smw.c)
bool Cheats_FlyingYoshi(void);
bool Cheats_McHits(void);
bool Cheats_LifeSteal(void);
bool Cheats_FirePunch(void);
bool Cheats_InstantHits(void);
bool Cheats_BlockBuster(void);
bool Cheats_InfiniteJumps(void);
bool Cheats_OpenMap(void);
bool Cheats_StompAll(void);
bool Cheats_PunchAll(void);              // MC attacks hurt the enemies stomps and punches can't (mc_smw.c PunchSprite)
bool Cheats_LowGravity(void);
// Knockback cheat: 1-20, how many times as far as normal hits knock Mario Combat Mario (1 = normal).
int Cheats_KnockbackScale(void);
bool Cheats_AirDash(void);
// After SMW's physics in the normal player state (yspeed_before: his y speed before them): SMW Mario's low gravity
// and air dash.
void Cheats_AfterPlayerPhysics(uint8 yspeed_before);
// Air dash, both Marios: -1 / +1 when L / R was just pressed with the cheat on (0 otherwise); the dash's sound and smoke.
int Cheats_AirDashPressed(void);
void Cheats_AirDashEffect(void);
// Chasing / jumping enemies: from SprStatus08 before an enemy's own code. Fast enemies: after it, true when the code
// runs once more (id: the sprite's id before its code ran).
void Cheats_EnemyFrame(uint8 k);
bool Cheats_FastEnemy(uint8 k, uint8 id);
// Pit rescue cheat, from PlayerState00_CheckPlayerPitFall every normal-state frame: remembers the last ground Mario
// stood on; returns true when a pit fall was turned into a hit there (the caller then doesn't kill him).
bool Cheats_PitRescue(bool fell);
// How an ordinary enemy moves (the Triple-enemies kinds): 0 not an ordinary enemy, 1 walks, 2 flies / swims, 3 lives
// in terrain (pipe, dirt, net, lava, wall, line).
int Cheats_EnemyKind(uint8 id);
// Double jump / ground pound for SMW Mario: called in the normal player state before SMW's physics.
void Cheats_PlayerMoves(void);
// Shared by both Marios: the double jump's puff of smoke and jump sound, the ground pound's landing.
void Cheats_DoubleJumpEffect(bool spin);
void Cheats_GroundPoundLanding(void);
// Slow motion: true on the frames the game must not advance (every other frame inside a level).
bool Cheats_SlowMotionSkipFrame(void);
// Triple enemies: called right after the level loader (ParseLevelSpriteList) has set up sprite slot k. The loader's
// slot range for it is slot_hi down to slot_lo + 1.
void Cheats_OnLevelSpriteLoaded(uint8 k, uint8 slot_lo, uint8 slot_hi);
void Cheats_ClearPending(void);
// Pilot audit: print one TRIPLE line per enemy the level loads (id, position, copies made / queued).
void Cheats_SetTripleLog(bool on);
// Always Yoshi: called on every level / room load, before SMW decides whether Mario brings Yoshi along.
void Cheats_OnSublevelLoad(void);
// Turns save file 3 into a 100% completed game (backing up the player's own file 3 first), or puts that backup
// back. Returns false if the save could not be written.
bool Cheats_SetSave100(bool on);
