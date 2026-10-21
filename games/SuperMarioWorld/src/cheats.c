// Settings-menu cheats and the "100% save in file 3" option (see cheats.h).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cheats.h"
#include "mc/mc_level.h"
#include "config.h"
#include "funcs.h"
#include "variables.h"
#include "common_rtl.h"
#include "smw_rtl.h"
#include "mc/mc_smw.h"
#include "mc/mc_bowser.h"

extern uint8 *g_sram;
extern int g_sram_size;

bool Cheats_NoDamage(void) { return g_config.cheat[kCheat_NoDamage]; }
bool Cheats_InfiniteLife(void) { return g_config.cheat[kCheat_InfiniteLife]; }
bool Cheats_MoonJump(void) { return g_config.cheat[kCheat_MoonJump]; }
bool Cheats_PSpeed(void) { return g_config.cheat[kCheat_PSpeed]; }
bool Cheats_ReplayLevels(void) { return g_config.cheat[kCheat_ReplayLevels]; }
bool Cheats_NoKnockback(void) { return g_config.cheat[kCheat_NoKnockback] && McSmw_Enabled(); }
bool Cheats_SuperPunch(void) { return g_config.cheat[kCheat_SuperPunch]; }

// Mirror mode: inside levels. The PPU flips sprites, BG1 and BG2 and keeps BG3 (status bar, message boxes) the right
// way round (g_ppu_mirror_from_line), so all text stays readable; the map and the menus are not mirrored.
bool Cheats_MirrorActive(void) { return g_config.cheat[kCheat_Mirror] && misc_game_mode == 0x14; }

void Cheats_FilterInputs(void) {  // after the joypads are read: Left and Right trade places in a mirrored level
  if (!Cheats_MirrorActive())
    return;
  uint8 *v[2] = { &io_controller_hold1, &io_controller_press1 };
  for (int i = 0; i < 2; i++)
    *v[i] = (*v[i] & ~3) | (*v[i] & 1) << 1 | (*v[i] & 2) >> 1;
  uint8 *lr[2] = { &io_controller_hold2, &io_controller_press2 };  // L and R too (the air dash's directions)
  for (int i = 0; i < 2; i++)
    *lr[i] = (*lr[i] & ~0x30) | (*lr[i] & 0x10) << 1 | (*lr[i] & 0x20) >> 1;
}

bool Cheats_OneHit(void) { return g_config.cheat[kCheat_OneHit]; }
bool Cheats_DoubleJump(void) { return g_config.cheat[kCheat_DoubleJump]; }
bool Cheats_GroundPound(void) { return g_config.cheat[kCheat_GroundPound]; }
// (the Mario Combat level is built for MC Mario's wall jumps: SMW Mario gets them there too)
bool Cheats_WallJump(void) { return (g_config.cheat[kCheat_WallJump] || McLevel_Active()) && !McSmw_Enabled(); }
bool Cheats_Slippery(void) { return g_config.cheat[kCheat_Slippery] && misc_game_mode == 0x14; }
bool Cheats_FlyingYoshi(void) { return g_config.cheat[kCheat_FlyingYoshi]; }
bool Cheats_McHits(void) { return g_config.cheat[kCheat_McHits]; }
bool Cheats_LifeSteal(void) { return g_config.cheat[kCheat_LifeSteal]; }
bool Cheats_FirePunch(void) { return g_config.cheat[kCheat_FirePunch]; }
bool Cheats_InstantHits(void) { return g_config.cheat[kCheat_InstantHits]; }
bool Cheats_BlockBuster(void) { return g_config.cheat[kCheat_BlockBuster]; }
bool Cheats_InfiniteJumps(void) { return g_config.cheat[kCheat_InfiniteJumps]; }
bool Cheats_OpenMap(void) { return g_config.cheat[kCheat_OpenMap]; }
bool Cheats_StompAll(void) { return g_config.cheat[kCheat_StompAll]; }
bool Cheats_PunchAll(void) { return g_config.cheat[kCheat_PunchAll]; }
bool Cheats_LowGravity(void) { return g_config.cheat[kCheat_LowGravity]; }
int Cheats_KnockbackScale(void) { return IntMax(1, IntMin(20, g_config.cheat_knockback)); }
bool Cheats_AirDash(void) { return g_config.cheat[kCheat_AirDash]; }

// Low gravity for SMW Mario: after SMW's physics in the normal player state, with the y speed from before
// them. Half of what gravity added this frame is taken back (an odd step's half is kept for the next frame), and the top
// falling speed is three quarters of SMW's. A jump's take-off and a cape's float (speed going down) are SMW's own.
static void LowGravitySmw(uint8 yspeed_before) {
  static int frac;
  int before = (int8)yspeed_before, now = (int8)player_yspeed;
  if (!Cheats_LowGravity() || !player_in_air_flag || player_cape_flying_phase || now <= before)
    return;
  int add = now - before + frac;  // gravity's step this frame (3 or 6), plus the half-step left over
  frac = add & 1;
  now = before + add / 2;
  if (now > 0x30)
    now = IntMax(before, 0x30);
  player_yspeed = (uint8)(int8)now;
}

// ---------------------------------------------------------------------------------------------
// Double jump and ground pound (SMW Mario here; MC Mario's are in mc_smw.c, through the MC hero's own variables)
// ---------------------------------------------------------------------------------------------
static void SpawnSmokeAtFeetDx(uint8 type, uint8 timer, int dx) {  // SMW's smoke sprites (3 = the small puff of a
  uint8 j = 3;                                                       // wall kick, 1 = the cloud of an enemy poofed)
  while (smoke_spr_spriteid[j]) {
    if ((--j & 0x80) != 0) { j = 0; break; }
  }
  smoke_spr_spriteid[j] = type;
  smoke_spr_xpos_lo[j] = (uint8)(player_xpos + dx);
  smoke_spr_ypos_lo[j] = (uint8)(player_ypos + (player_current_power_up ? 24 : 20));
  smoke_spr_timer[j] = timer;
}

static void SpawnSmokeAtFeet(uint8 type, uint8 timer) { SpawnSmokeAtFeetDx(type, timer, 0); }

void Cheats_DoubleJumpEffect(bool spin) {
  if (spin)
    io_sound_ch3 = 4;  // SMW's spin-jump sound
  else
    io_sound_ch2 = 1;  // SMW's jump sound
  SpawnSmokeAtFeet(3, 19);
}

// The ground pound's landing, for both Marios: SMW's cape-dive pound (the screen shakes, grounded enemies are knocked
// out) and a cloud of SMW's smoke on either side of Mario's feet.
void Cheats_GroundPoundLanding(void) {
  RunPlayerBlockCode_TriggerCapeDiveGroundPound();
  io_sound_ch3 = 9;
  SpawnSmokeAtFeetDx(1, 27, -10);
  SpawnSmokeAtFeetDx(1, 27, 10);
}

static bool g_air_jump_used, g_pounding;
static int g_pound_windup;  // frames left of the ground pound's wind-up (the spin in mid-air before the drop)

// Air dash, shared by both Marios: L dashes left, R dashes right (in a mirrored level the two swap with Left / Right,
// Cheats_FilterInputs), once per stay in the air. 0 when no new dash was pressed, else -1 / +1.
int Cheats_AirDashPressed(void) {
  if (!Cheats_AirDash())
    return 0;
  uint8 lr = io_controller_press2 & 0x30;
  return lr == 0x20 ? -1 : lr == 0x10 ? 1 : 0;
}

// The dash's start: the cape's swish and a puff of smoke where Mario was.
void Cheats_AirDashEffect(void) {
  io_sound_ch3 = 4;
  SpawnSmokeAtFeetDx(3, 19, 0);
}

// SMW Mario's dash: kDashFrames straight across at kDashSpeed (6.5 px a frame, about 5 blocks in all) without falling;
// it ends early at a wall, on landing and on a jump. Then he goes on at no more than SMW's top running speed (kRunSpeed):
// SMW's own physics would keep the dash's speed in the air and only slowly brake it on the ground, and SMW's wall check
// is made for its own speeds.
enum { kDashFrames = 12, kDashSpeed = 0x68, kRunSpeed = 0x2F };
static int g_dash_frames, g_dash_dir;
static bool g_dash_used;

static bool TileSolid(int x, int y);

static void EndDash(void) {
  if (g_dash_frames > 0 && abs((int8)player_xspeed) > kRunSpeed)
    player_xspeed = (uint8)(int8)((int8)player_xspeed > 0 ? kRunSpeed : -kRunSpeed);
  g_dash_frames = 0;
}

// The pixels SMW Mario can move this frame before his side reaches a wall (up to max): his side probes are SMW's
// (GetPlayerLevelCollisionMap16ID: x + 14 on the right, x + 2 on the left, at y + 0x1A and y + 0x16 small / y + 0x0F
// big). SMW's side check only pushes him out of a wall a little each frame: at the dash's speed his body could end up
// in a wall's corner (the top of a pipe), where SMW takes him for crushed.
static int DashRoomAhead(int dir, int max) {
  int x = (int16)player_xpos, y = (int16)player_ypos;
  int edge = dir > 0 ? x + 14 : x + 2;
  int y0 = y + (player_current_power_up && !player_ducking_flag ? 0x0F : 0x16), y1 = y + 0x1A;
  for (int d = 1; d <= max; d++)
    if (TileSolid(edge + dir * d, y0) || TileSolid(edge + dir * d, y1))
      return d;  // (the side ends 1 px into the wall, where SMW Mario rests against one)
  return -1;
}

static void AirDashSmw(void) {
  if (g_dash_frames <= 0)
    return;
  if (player_blocked_flags & (g_dash_dir > 0 ? 1 : 2)) {  // into a wall: the dash is over
    EndDash();
    return;
  }
  int room = DashRoomAhead(g_dash_dir, kDashSpeed / 16 + 1);
  if (room >= 0) {  // a wall ahead: the dash takes him up to it and is over; SMW's own speed and wall check go on
    player_xpos += g_dash_dir * (room - 1);
    player_xspeed = (uint8)(int8)(g_dash_dir * kRunSpeed);
    player_yspeed = 0;
    g_dash_frames = 0;
    return;
  }
  player_xspeed = (uint8)(int8)(g_dash_dir * kDashSpeed);
  player_yspeed = 0;
  if (--g_dash_frames == 0)
    player_xspeed = (uint8)(int8)(g_dash_dir * kRunSpeed);
}

void Cheats_AfterPlayerPhysics(uint8 yspeed_before) {
  LowGravitySmw(yspeed_before);
  AirDashSmw();
}

bool Cheats_PoundWindup(void) { return g_pounding && g_pound_windup > 0; }

static bool CanUseAirMove(void) {
  return !player_current_state && !timer_end_level && !player_riding_yoshi_flag && !player_climbing_flag &&
         !player_swimming_flag && !player_cape_flying_phase && !flag_sprites_locked;
}

// SMW Mario's ground pound, drawn with SMW's own poses: a wind-up of kPoundWindup frames hanging in the air in his
// spin-jump spin (the spin-jump flag: SMW turns him round, as in a spin jump), then the drop at SMW's top falling speed
// in his ducking pose, then the landing (Cheats_GroundPoundLanding).
enum { kPoundWindup = 16 };

void Cheats_PlayerMoves(void) {
  bool grounded = !player_in_air_flag && (player_blocked_flags & 4);
  if (grounded || !CanUseAirMove()) {
    g_air_jump_used = false;
    g_dash_used = false;
    EndDash();
    if (g_pounding && grounded && CanUseAirMove())
      Cheats_GroundPoundLanding();
    if (g_pounding)
      player_spin_jump_flag = 0;
    g_pounding = false;
    g_pound_windup = 0;
    return;
  }
  if (g_pounding) {
    if (g_pound_windup > 0) {  // the wind-up: hanging in the air, spinning
      player_xspeed = 0;
      player_yspeed = 0;
      player_spin_jump_flag = 1;
      if (--g_pound_windup == 0) {
        player_spin_jump_flag = 0;
        io_sound_ch3 = 4;  // the spin sound as Mario turns into the drop
      }
      return;
    }
    if ((int8)player_yspeed < 0) {  // bounced off an enemy or a note block: the pound is over
      g_pounding = false;
    } else {
      player_xspeed = 0;
      player_yspeed = 0x40;  // SMW's top falling speed, from the start
      player_ducking_flag = 1;
      return;
    }
  }
  if (Cheats_GroundPound() && (io_controller_press1 & 4) && !player_carrying_something_flag1) {
    g_pounding = true;
    g_pound_windup = kPoundWindup;
    g_dash_frames = 0;
    player_xspeed = 0;
    player_yspeed = 0;
    player_spin_jump_flag = 1;
    return;
  }
  int dash = Cheats_AirDashPressed();
  if (dash && !g_dash_used) {
    g_dash_used = true;
    g_dash_frames = kDashFrames;
    g_dash_dir = dash;
    player_facing_direction = dash > 0;
    Cheats_AirDashEffect();
  }
  bool b = (io_controller_press1 & 0x80) != 0, a = (io_controller_press2 & 0x80) != 0;
  // Double jump: once per jump. Infinite jumps: every press in the air.
  bool again = Cheats_InfiniteJumps() || (Cheats_DoubleJump() && !g_air_jump_used);
  if (again && (a || b)) {
    g_air_jump_used = true;
    EndDash();  // a jump ends a dash
    bool spin = a && !player_carrying_something_flag1;
    player_spin_jump_flag = spin;
    player_yspeed = HandlePlayerPhysics_JumpSpeedForRun(spin);
    player_in_air_flag = 11;
    Cheats_DoubleJumpEffect(spin);
  }
}

// SMW's block coin (the spinning coin that jumps out of a coin block) at (x, y), with the coin counted.
static void PopCoin(uint16 x, uint16 y) {
  uint8 v = 3;
  while (block_coin_spr_slotid[v]) {
    if ((--v & 0x80) != 0) { v = 0; break; }
  }
  block_coin_spr_slotid[v] = 1;
  GiveCoins_OneCoin();
  block_coin_spr_xpos_lo[v] = (uint8)x, block_coin_spr_xpos_hi[v] = (uint8)(x >> 8);
  block_coin_spr_ypos_lo[v] = (uint8)y, block_coin_spr_ypos_hi[v] = (uint8)(y >> 8);
  block_coin_spr_layer_index[v] = 0;
  block_coin_spr_yspeed[v] = (uint8)-48;
}

// Coins from enemies: a coin pops out of each enemy as it is defeated, like from a coin block (SMW's block coin).
void Cheats_EnemyDefeated(uint8 k) {
  if (!g_config.cheat[kCheat_EnemyCoins])
    return;
  PopCoin(GetSprXPos(k), GetSprYPos(k) - 16);
}

// Coin magnet: the nearest coin tile within 64 px (4 tiles) of Mario's middle is taken, one per frame. A coin jumps out as a
// block coin (counted); a Yoshi coin is collected through SMW's own block code (its sound, the count of five).
static void CoinMagnet(void) {
  if (player_current_state || flag_sprites_locked || timer_blue_pswitch)
    return;
  int cx = (int16)player_xpos + 8, cy = (int16)player_ypos + (player_current_power_up ? 16 : 24);
  int best = -1, bx = 0, by = 0, bd = 64 * 64 + 1;
  for (int ty = (cy - 64) & ~15; ty <= cy + 64; ty += 16)
    for (int tx = (cx - 64) & ~15; tx <= cx + 64; tx += 16) {
      int t = McSmw_LevelMap16At(tx, ty);
      if (t < 0x2B || t > 0x2E)  // page-0 map16 2B coin, 2C, 2D / 2E the halves of a Yoshi coin
        continue;
      int d = (tx + 8 - cx) * (tx + 8 - cx) + (ty + 8 - cy) * (ty + 8 - cy);
      if (d < bd) bd = d, best = t, bx = tx, by = ty;
    }
  if (best < 0)
    return;
  blocks_xpos = (uint16)bx;
  blocks_ypos = (uint16)by;
  misc_current_layer_being_processed = 0;
  if (best >= 0x2D) {
    RunPlayerBlockCode_00F309((uint8)best);
  } else {
    blocks_map16_to_generate = 1;  // the empty tile
    GenerateTile();
    PopCoin((uint16)bx, (uint16)(by - 8));
  }
}

static uint8 TripleKind(uint8 id);
static bool IsEnemyForCoins(uint8 id) {  // the Triple-enemies list plus the single special enemies
  return TripleKind(id) != 0 || id == 0x1E || id == 0x1F || id == 0x9B;  // Lakitu, Magikoopa, Hammer Bro
}

// Watches the sprite slots for an enemy going from its normal state (8) to defeated: 2 knocked out, 3 squashed,
// 4 spin-jumped into smoke, or 9 stunned (a flipped Goomba, a Koopa stomped out of its shell), whatever defeated it.
// Only from state 8, so a stunned Goomba or a shell that is knocked out later doesn't pay a second coin.
static void WatchDefeats(void) {
  static uint8 prev[12];
  for (uint8 k = 0; k < 12; k++) {
    uint8 s = spr_current_status[k];
    // (King Koopas' Bowser is knocked out by SMW for a moment when a shell hits him: he pays his coin when defeated)
    if (prev[k] == 8 && ((s >= 2 && s <= 4) || s == 9) && IsEnemyForCoins(spr_spriteid[k]) && !McBowser_IsActive(k))
      Cheats_EnemyDefeated(k);
    prev[k] = s;
  }
}

// Respawning enemies: SMW marks each sprite of the level's list as loaded ($1938) and clears the mark when the sprite
// walks off the screen alive, so it comes back when its spot scrolls into view again; a defeated sprite keeps the mark.
// With the cheat a defeated (or eaten, or carried-off) ordinary enemy loses the mark too, once no slot holds it any
// more (a stomped Koopa's shell still counts as the Koopa). Bosses, Bowser (King Koopas) and items are not tracked.
static bool g_resp_tracked[12];
static uint8 g_resp_index[12];

static void ResetRespawns(void) { memset(g_resp_tracked, 0, sizeof(g_resp_tracked)); }

static void WatchRespawns(void) {
  for (uint8 k = 0; k < 12; k++) {
    uint8 idx = g_resp_index[k];
    if (g_resp_tracked[k] && !(spr_current_status[k] && spr_load_status_table_index[k] == idx)) {
      bool held = false;  // another slot still has this list entry
      for (uint8 j = 0; j < 12; j++)
        held |= j != k && spr_current_status[j] && spr_load_status_table_index[j] == idx;
      if (!held)
        sprites_load_status[idx] = 0;
      g_resp_tracked[k] = false;
    }
    if (!g_resp_tracked[k] && spr_current_status[k] >= 8 && spr_load_status_table_index[k] != 0xFF &&
        IsEnemyForCoins(spr_spriteid[k]) && !McBowser_IsActive(k)) {
      g_resp_tracked[k] = true;
      g_resp_index[k] = spr_load_status_table_index[k];
    }
  }
}

bool Cheats_SlowMotionSkipFrame(void) {
  static bool skip;
  skip = g_config.cheat[kCheat_SlowMotion] && misc_game_mode == 0x14 && !skip;
  return skip;
}

// ---------------------------------------------------------------------------------------------
// Triple enemies: each ordinary enemy the level loads brings two copies, placed near it where the same kind of enemy
// can be (beside a walker on the same floor, around a flier in free air, in the same terrain for enemies that live in
// it). A copy that finds no free sprite slot waits a few seconds for one.
// ---------------------------------------------------------------------------------------------
enum {
  kTripleNo,       // not tripled: bosses, Koopa Kids, Magikoopa, Lakitu, items, platforms, generators, scripted sprites
  kTripleWalk,     // walks on the ground: copies beside it on the same floor (or in the air if it starts in the air)
  kTripleFly,      // flies or swims: copies in free air around it, also above / below
  kTripleTerrain,  // lives in terrain (pipe, dirt, net, lava, wall): copies only where the tiles match its own
};

static uint8 TripleKind(uint8 id) {
  if (id <= 0x07 || id == 0x0D) return kTripleWalk;   // Koopas, Bob-omb
  if (id <= 0x0C) return kTripleFly;                  // para-Koopas
  switch (id) {
  case 0x0F: case 0x10: case 0x11: case 0x13: case 0x1D:   // Goomba, para-Goomba, Buzzy, Spiny, Hopping Flame
  case 0x27: case 0x2E: case 0x30: case 0x31: case 0x32:   // Thwimp, Spike Top, Dry Bones, Bony Beetle
  case 0x46: case 0x51: case 0x6E: case 0x6F: case 0x70:   // Diggin' Chuck, Ninji, Dino-Rhino, Dino-Torch, Pokey
  case 0x73: case 0x86: case 0x99: case 0x9A:              // Super Koopa (feather), Wiggler, Volcano Lotus, Sumo Bro
  case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x97: case 0x98:  // Chargin' Chucks
  case 0xA2: case 0xAB: case 0xB4: case 0xBD: case 0xBF:   // Mechakoopa, Rex, Grinder, sliding Koopa, Mega Mole
    return kTripleWalk;
  case 0x14: case 0x15: case 0x16: case 0x17: case 0x18:   // falling Spiny egg, Cheep-cheeps
  case 0x1C: case 0x26: case 0x28: case 0x37: case 0x38:   // Bullet Bill, Thwomp, Big Boo, Boo, Eerie
  case 0x39: case 0x3A: case 0x3B: case 0x3D: case 0x3F:   // wave Eerie, Urchins, Rip Van Fish, para-Goomba
  case 0x40: case 0x47: case 0x71: case 0x72: case 0x90:   // para-Bomb, grey fish, Super Koopas, gas bubble
  case 0x9F: case 0xAA: case 0xAE: case 0xB6: case 0xBE:   // Banzai Bill, Fishbone, Fishin' Boo, fireball, Swooper
  case 0xC2: case 0xC3: case 0x9D: case 0xA4: case 0xAF:   // Blurp, Porcu-Puffer, enemy in a bubble, spike ball, Boo block
    return kTripleFly;
  case 0x65: case 0x66: case 0x67: case 0x68:              // line-guided chainsaws, Grinder, Fuzzy
  case 0x1A: case 0x2A: case 0x4F: case 0x50:              // Piranha Plants (pipes)
  case 0x22: case 0x23: case 0x24: case 0x25:              // net Koopas
  case 0x33: case 0xA8:                                    // Podoboo, Blargg (lava)
  case 0x3C: case 0xA5: case 0xA6:                         // wall-following Urchin, Sparky / Fuzzy, Hothead
  case 0x4B: case 0x4D: case 0x4E:                         // pipe Lakitu, Monty Moles (ground / ledge)
  case 0xAC: case 0xAD: case 0xB2:                         // wooden spikes, falling spike
    return kTripleTerrain;
  }
  return kTripleNo;
}

int Cheats_EnemyKind(uint8 id) { return TripleKind(id); }

// ---------------------------------------------------------------------------------------------
// Fast, chasing and jumping enemies (ordinary enemies only: bosses, Koopa Kids and scripted sprites are left alone)
// ---------------------------------------------------------------------------------------------
// The plain walkers: they walk the floor in the direction they face and turn with SMW's own turn (Koopas, Bob-omb,
// Goombas, Buzzy Beetle, Spiny, Dry Bones, Bony Beetle, Dino-Rhino / Torch, Wiggler, Mechakoopa, Rex, Mega Mole).
// Chucks, Sumo Bros and the like have their own aim at Mario and are not included.
static bool IsPlainWalker(uint8 id) {
  if (id <= 0x07)
    return true;
  switch (id) {
  case 0x0D: case 0x0F: case 0x10: case 0x11: case 0x13: case 0x30: case 0x31: case 0x32:
  case 0x6E: case 0x6F: case 0x86: case 0xA2: case 0xAB: case 0xBF:
    return true;
  }
  return false;
}

// Jumping enemies: each time Mario takes off from the ground, every plain walker that is on the ground within the next
// few frames jumps once (one that is just landing from a bounce or a step still joins in).
enum { kJumpWindow = 8 };
static bool g_mario_grounded;
static int g_mc_jumps_seen;
static uint8 g_jump_serial, g_jump_window;  // take-offs so far; frames left to join the current one
static uint8 g_enemy_jump_serial[12];       // the take-off each enemy slot last jumped for

static void WatchMarioJump(void) {
  bool took_off;
  if (McSmw_Enabled()) {  // MC Mario moves himself: his own count of ground jumps
    took_off = McSmw_GroundJumps() != g_mc_jumps_seen;
    g_mc_jumps_seen = McSmw_GroundJumps();
  } else {  // SMW Mario: on the ground last frame, in the air and rising now (his jump sets both at once)
    bool grounded = !player_in_air_flag && !player_current_state && !player_swimming_flag && !player_climbing_flag;
    took_off = g_mario_grounded && player_in_air_flag && (int8)player_yspeed < 0 && !player_current_state;
    g_mario_grounded = grounded;
  }
  if (took_off) {
    g_jump_serial++;
    g_jump_window = kJumpWindow;
  } else if (g_jump_window) {
    g_jump_window--;
  }
}

// From SprStatus08, before an enemy's own code runs in its normal state.
void Cheats_EnemyFrame(uint8 k) {
  if (flag_sprites_locked || misc_game_mode != 0x14 || !IsPlainWalker(spr_spriteid[k]))
    return;
  if (!(spr_table1588[k] & 4))  // only on the ground
    return;
  if (g_config.cheat[kCheat_ChaseEnemies]) {
    // Turned toward Mario with SMW's own turn (at most every 8 frames); not while he is right above it.
    int dx = ((int16)player_xpos + 8) - ((int16)GetSprXPos(k) + 8);
    bool faces_left = spr_table157c[k] & 1;
    if ((dx < -12 && !faces_left) || (dx > 12 && faces_left))
      ChangeNormalSpriteDirection(k);
  }
  if (g_config.cheat[kCheat_JumpEnemies] && g_jump_window && k < 12 && g_enemy_jump_serial[k] != g_jump_serial) {
    g_enemy_jump_serial[k] = g_jump_serial;
    spr_yspeed[k] = (uint8)-0x38;  // about two tiles high under SMW's sprite gravity
  }
}

// From SprStatus08, after an enemy's own code ran: true when it should run once more this frame (Fast enemies).
bool Cheats_FastEnemy(uint8 k, uint8 id) {
  return g_config.cheat[kCheat_FastEnemies] && !flag_sprites_locked && misc_game_mode == 0x14 &&
         spr_current_status[k] == 8 && spr_spriteid[k] == id && TripleKind(id) != kTripleNo;
}

static int TripleSpacing(uint8 id) {  // distance between copies: the sprite's width
  switch (id) {
  case 0x28: case 0x90: case 0x9F: return 64;              // Big Boo, gas bubble, Banzai Bill
  case 0x26: case 0x3A: case 0x3B: case 0x3C: case 0x6E: case 0x70: case 0x86: case 0x91: case 0x92: case 0x93:
  case 0x94: case 0x95: case 0x97: case 0x98: case 0x9A: case 0xA8: case 0xAB: case 0xBF: case 0x46:
    return 32;
  }
  return 20;
}

static bool TileBlocks(int x, int y) {  // solid, slope or ledge tile (map16 page 1 up to the slopes' end)
  int t = McSmw_LevelMap16At(x, y);
  return t < 0 || (t >= 0x100 && t < 0x1D8);
}

static bool TileSolid(int x, int y) {  // solid on every side (ledge tops and slopes are fine to overlap)
  int t = McSmw_LevelMap16At(x, y);
  return t < 0 || (t >= 0x111 && t < 0x16E);
}

static bool HasRoom(int x, int y) {  // a 16x16 sprite at (x, y) is clear of solid tiles
  return !TileSolid(x + 2, y + 2) && !TileSolid(x + 13, y + 2) && !TileSolid(x + 2, y + 13) && !TileSolid(x + 13, y + 13);
}

static bool HasFloor(int x, int y) { return TileBlocks(x + 4, y + 16) || TileBlocks(x + 11, y + 16); }

// Enemies that need the exact same tiles: plants and Lakitu in pipes, line-guided sprites on their line.
static bool NeedsExactTerrain(uint8 id) {
  return id == 0x1A || id == 0x2A || id == 0x4F || id == 0x50 || id == 0x4B || (id >= 0x65 && id <= 0x68);
}

// Pipe dwellers (Piranha Plants, pipe Lakitu) wait in the pipe on their phase timer ($1540) before coming out.
static bool SharesPipe(uint8 id) { return id == 0x1A || id == 0x2A || id == 0x4F || id == 0x50 || id == 0x4B; }

static int TileClass(int t) {  // what a terrain enemy cares about: solid, ledge / slope, liquid / special, air
  if (t < 0) return -1;
  if (t >= 0x111 && t < 0x16E) return 0x1000;           // solid (dirt, blocks, walls)
  if (t >= 0x100) return 0x1001;                        // ledge top or slope
  if (t < 0x10) return t;                               // water, lava and the other special page-0 tiles
  return 0;                                             // air, coins, decoration
}

static bool SameTerrain(uint8 id, int x, int y, int nx, int ny) {  // the tiles at, above and below both spots match
  bool exact = NeedsExactTerrain(id);
  for (int dy = -16; dy <= 16; dy += 16) {
    int a = McSmw_LevelMap16At(x + 8, y + 8 + dy), b = McSmw_LevelMap16At(nx + 8, ny + 8 + dy);
    if (exact ? a != b : TileClass(a) != TileClass(b))
      return false;
  }
  return true;
}

static bool g_triple_log;
void Cheats_SetTripleLog(bool on) { g_triple_log = on; }

enum { kPendingMax = 12, kPendingFrames = 600 };
static struct { uint8 slot, id, kind, need, status; int16 x, y, frames; } g_pending[kPendingMax];

static uint8 SpawnOne(uint8 orig, uint8 id, uint8 status, int x, int y, uint8 slot_hi) {  // into the highest free slot
  uint8 j = slot_hi;
  while (spr_current_status[j])
    j--;
  spr_current_status[j] = status;  // 1 = run the init routine; 9 = a shell placed as a shell
  spr_spriteid[j] = id;
  SetSprXYPos(j, (uint16)x, (uint16)y);
  spr_load_status_table_index[j] = 0xFF;  // not tied to the level's sprite list: never reloaded on its own
  InitializeNormalSpriteRAMTables(j);
  spr_table15f6[j] = spr_table15f6[orig];
  spr_xoffscreen_flag[j] = 1;
  spr_decrementing_table1fe2[j] = 4;
  return j;
}

// Finds up to `need` spots for copies of `id` loaded at (x, y) and spawns them while a slot is free; returns the
// number made. *no_spot is set when there was no fitting spot at all (as opposed to no free slot).
static int SpawnCopies(uint8 orig, uint8 id, uint8 status, int kind, int x, int y, int need, uint8 slot_lo, uint8 slot_hi,
                       int keep_free, bool *no_spot) {
  int s = TripleSpacing(id);
  bool on_floor = HasFloor(x, y), embedded = !HasRoom(x, y);
  int cand[128][2], n = 0;
  if (kind == kTripleTerrain) {
    // Whole tiles in every direction, nearest first: copies follow a line, a net or a wall up and down too.
    int min_d = 16 * ((s + 15) / 16);
    for (int r = min_d; r <= 80 && n < 128; r += 16)
      for (int dy = -r; dy <= r && n < 128; dy += 16)
        for (int dx = -r; dx <= r && n < 128; dx += 16)
          if (abs(dx) == r || abs(dy) == r)
            cand[n][0] = dx, cand[n++][1] = dy;
  } else {
    for (int m = 1; m <= 5; m++) {  // beside it, further and further out
      int d = s * m + (m > 1 ? 4 * m : 0);
      cand[n][0] = d, cand[n++][1] = 0;
      cand[n][0] = -d, cand[n++][1] = 0;
    }
  }
  if (kind == kTripleFly) {  // and above / below
    int v[][2] = { { 0, -s }, { 0, s }, { s, -s }, { -s, -s }, { 0, -2 * s }, { 0, 2 * s } };
    for (int i = 0; i < 6 && n < 24; i++)
      cand[n][0] = v[i][0], cand[n++][1] = v[i][1];
  }
  int free_slots = 0;
  for (uint8 j = slot_hi; j != slot_lo; j--)
    free_slots += spr_current_status[j] == 0;
  int made = 0, spots = 0;
  for (int c = 0; c < n && made < need; c++) {
    int nx = x + cand[c][0], ny = y + cand[c][1];
    bool ok;
    if (kind == kTripleTerrain || embedded)
      ok = SameTerrain(id, x, y, nx, ny);  // a mole's dirt wall, a pipe opening, a net, lava ...
    else if (kind == kTripleWalk)
      ok = HasRoom(nx, ny) && (!on_floor || HasFloor(nx, ny));  // not in a wall, not over a pit
    else
      ok = HasRoom(nx, ny);
    if (!ok)
      continue;
    spots++;
    if (free_slots <= keep_free)
      break;
    free_slots--;
    SpawnOne(orig, id, status, nx, ny, slot_hi);
    made++;
  }
  // Pipe dwellers with no other pipe in reach share their own pipe: the copies wait longer before they first come out
  // (their phase timer), so they come out one after another.
  if (SharesPipe(id) && made < need && spots == 0) {
    spots++;
    while (made < need && free_slots > keep_free) {
      free_slots--;
      uint8 j = SpawnOne(orig, id, status, x, y, slot_hi);
      spr_decrementing_table1540[j] = (uint8)(72 * (made + 1));
      made++;
    }
  }
  *no_spot = spots == 0;
  return made;
}

// Copies only take slots while one stays free, so keys and P-switches further on still find room (SMW only retries a
// sprite that found no slot when it scrolls in again). A copy that finds no slot waits in g_pending.
enum { kKeepFree = 1 };
static uint8 g_slot_lo, g_slot_hi;

void Cheats_OnLevelSpriteLoaded(uint8 k, uint8 slot_lo, uint8 slot_hi) {
  if (!g_config.cheat[kCheat_TripleEnemies] || misc_game_mode < 0x11)
    return;
  uint8 id = spr_spriteid[k];
  int kind = spr_current_status[k] == 1 || spr_current_status[k] == 9 ? TripleKind(id) : kTripleNo;
  int x = (int16)GetSprXPos(k), y = (int16)GetSprYPos(k);
  if (kind == kTripleNo) {
    if (g_triple_log) printf("TRIPLE %02X %d %d kind 0\n", id, x, y);
    return;
  }
  g_slot_lo = slot_lo, g_slot_hi = slot_hi;
  bool no_spot;
  uint8 status = spr_current_status[k];
  int made = SpawnCopies(k, id, status, kind, x, y, 2, slot_lo, slot_hi, kKeepFree, &no_spot);
  int queued = 0;
  // A sprite with its own reserved one-slot range (Banzai Bill and other big sprites in some sprite-memory settings)
  // can never have a copy: SMW keeps the other slots' OAM for smaller sprites.
  bool one_slot = (uint8)(slot_hi - slot_lo) <= 1;
  if (made < 2 && !no_spot && !one_slot) {
    for (int p = 0; p < kPendingMax; p++)
      if (!g_pending[p].need) {
        g_pending[p].slot = k, g_pending[p].id = id, g_pending[p].kind = (uint8)kind;
        g_pending[p].status = status, g_pending[p].need = (uint8)(2 - made), g_pending[p].x = (int16)x, g_pending[p].y = (int16)y;
        g_pending[p].frames = kPendingFrames;
        queued = 2 - made;
        break;
      }
  }
  if (g_triple_log)
    printf("TRIPLE %02X %d %d kind %d made %d queued %d nospot %d oneslot %d\n", id, x, y, kind, made, queued,
           no_spot && made < 2, one_slot);
}

static void Cheats_PendingCopies(void) {
  for (int p = 0; p < kPendingMax; p++) {
    if (!g_pending[p].need)
      continue;
    uint8 k = g_pending[p].slot;
    if (spr_current_status[k] < 8 || spr_spriteid[k] != g_pending[p].id || --g_pending[p].frames <= 0) {
      if (g_triple_log) printf("TRIPLELATE %02X dropped %d\n", g_pending[p].id, g_pending[p].need);
      g_pending[p].need = 0;  // the enemy is gone, or no slot came free in time
      continue;
    }
    bool no_spot;
    // walkers and fliers: around where the enemy is now; terrain enemies: where it was placed (its pipe, dirt ...)
    int x = g_pending[p].x, y = g_pending[p].y;
    if (g_pending[p].kind != kTripleTerrain)
      x = (int16)GetSprXPos(k), y = (int16)GetSprYPos(k);
    int made = SpawnCopies(k, g_pending[p].id, g_pending[p].status, g_pending[p].kind, x, y, g_pending[p].need,
                           g_slot_lo, g_slot_hi, kKeepFree, &no_spot);
    if (made && g_triple_log) printf("TRIPLELATE %02X made %d\n", g_pending[p].id, made);
    g_pending[p].need -= (uint8)made;
  }
}

void Cheats_ClearPending(void) { memset(g_pending, 0, sizeof(g_pending)); }

// Always Yoshi: Mario enters every level and room on Yoshi (not where SMW itself leaves Yoshi outside).
static bool g_ice_forced;  // Slippery levels: flag_ice_level is ours until the next room load
static uint8 g_ice_saved;  // the room's own value, put back when the cheat goes off

// Pit rescue: the last ground tile Mario stood on in this room (not a sprite platform: it may move over the pit).
static uint16 g_safe_x, g_safe_y;
static bool g_safe_ok;

bool Cheats_PitRescue(bool fell) {
  if (!g_config.cheat[kCheat_PitRescue])
    return false;
  if (!fell) {
    if ((player_blocked_flags & 4) && !player_in_air_flag && !misc_player_on_solid_sprite && !player_current_state)
      g_safe_x = player_xpos, g_safe_y = player_ypos, g_safe_ok = true;
    return false;
  }
  if (!g_safe_ok)
    return false;  // no ground yet in this room: the fall is a fall
  player_xpos = g_safe_x;
  player_ypos = g_safe_y;
  player_xspeed = player_yspeed = 0;
  McSmw_OnTeleported();
  DamagePlayer_Hurt();  // the cost: a hit (Mario Combat's life bar, SMW Mario's power-down; Yoshi runs off)
  return true;
}

void Cheats_OnSublevelLoad(void) {
  ResetRespawns();       // respawning enemies: the new room has its own sprite list
  g_safe_ok = false;     // pit rescue: no safe ground known in the new room yet
  g_ice_forced = false;  // the room just set its own ice flag
  if (!g_config.cheat[kCheat_AlwaysYoshi] || misc_game_mode < 0x11 || flag_prevent_yoshi_carry_over ||
      yoshi_carry_over_levels_flag)
    return;
  yoshi_carry_over_levels_flag = 1;
  if (!yoshi_current_yoshi_color)
    yoshi_current_yoshi_color = 10;  // green
}

void Cheats_Frame(void) {
  const bool *c = g_config.cheat;
  if (c[kCheat_InfiniteLives] && misc_game_mode >= 0x0B && misc_game_mode <= 0x0E) {
    // fade-out after a death and the overworld load, before the map's life counter is copied and drawn
    if (player_current_life_count < 98) player_current_life_count = 98;
    for (int i = 0; i < 2; i++)
      if (players_lives[i] < 98) players_lives[i] = 98;
  }
  if (misc_game_mode < 0x11 || misc_game_mode > 0x14)
    Cheats_ClearPending();  // left the level: copies still waiting for a slot are dropped
  if (misc_game_mode != 0x14) {
    g_mario_grounded = false;
    g_jump_window = 0;
    g_mc_jumps_seen = McSmw_GroundJumps();
    ResetRespawns();
    return;
  }
  WatchMarioJump();
  if (c[kCheat_Respawn])
    WatchRespawns();
  else
    ResetRespawns();
  if (c[kCheat_TripleEnemies])
    Cheats_PendingCopies();
  if (c[kCheat_EnemyCoins])
    WatchDefeats();
  if (Cheats_NoKnockback())
    timer_stun_player = 0;  // the stun of a Koopa Kid's ground pound
  if (c[kCheat_InfiniteTime])
    misc_status_bar_tilemap[55] = 40;  // the 40-frame divider before the timer ticks (UpdateStatusBarCounters)
  if (c[kCheat_InfiniteLives]) {  // 98 shows as 99 lives
    if (player_current_life_count < 98) player_current_life_count = 98;
    for (int i = 0; i < 2; i++)
      if (players_lives[i] < 98) players_lives[i] = 98;
  }
  if (c[kCheat_Invincible] && player_current_state == 0 && timer_star_power < 0x40)
    timer_star_power = 0xFF;  // kept above 0x1E, where SMW would restart the level music
  if (c[kCheat_PSpeed])
    player_pmeter = 0x70;
  if (c[kCheat_Switches])
    memset(flag_activated_switches, 1, 4);
  if (g_config.cheat_powerup && player_current_state == 0 && !timer_player_palette_cycle)
    player_current_power_up = g_config.cheat_powerup;
  // Moon jump for SMW Mario (Mario Combat Mario's is in mc_smw.c): jump held in the air keeps rising.
  if (c[kCheat_MoonJump] && player_current_state == 0 && player_in_air_flag &&
      ((io_controller_hold1 | io_controller_hold2) & 0x80) && (int8)player_yspeed > -40)
    player_yspeed = (uint8)-40;
  // Slippery levels: SMW's ice-level flag (RAM 0x86) on in every room; the room's own value comes back with the cheat off.
  if (c[kCheat_Slippery]) {
    if (!g_ice_forced)
      g_ice_saved = flag_ice_level, g_ice_forced = true;
    flag_ice_level = 1;
  } else if (g_ice_forced) {
    flag_ice_level = g_ice_saved;
    g_ice_forced = false;
  }
  if (c[kCheat_CoinMagnet])
    CoinMagnet();
}

// ---------------------------------------------------------------------------------------------
// 100% save in file 3
// ---------------------------------------------------------------------------------------------
enum { kFileSize = 0x8D, kFile3 = 0x11E, kCopy2 = 429, kSlot = kFileSize + 2 };
static const char kBackupPath[] = "saves/file3_backup.srm";

static void WriteFile3(const uint8 *data /* kFileSize bytes, or NULL to erase */) {
  for (int copy = 0; copy < 2; copy++) {
    uint8 *p = g_sram + kFile3 + copy * kCopy2;
    if (!data) {
      memset(p, 0, kSlot);
      continue;
    }
    uint16 sum = 0;
    for (int i = 0; i < kFileSize; i++)
      sum += p[i] = data[i];
    uint16 chk = (uint16)(0x5a5a - sum);
    p[kFileSize] = (uint8)chk;
    p[kFileSize + 1] = (uint8)(chk >> 8);
  }
}

bool Cheats_SetSave100(bool on) {
  if (!g_sram || g_sram_size < kFile3 + kCopy2 + kSlot)
    return false;
  if (on) {
    FILE *f = fopen(kBackupPath, "rb");
    if (f) {
      fclose(f);  // a backup of the player's own file 3 already exists: keep it
    } else {
      f = fopen(kBackupPath, "wb");
      if (!f)
        return false;
      fwrite(g_sram + kFile3, 1, kSlot, f);
      fwrite(g_sram + kFile3 + kCopy2, 1, kSlot, f);
      fclose(f);
    }
    uint8 data[kFileSize];
    BuildCompletedSaveData(data);
    WriteFile3(data);
  } else {
    FILE *f = fopen(kBackupPath, "rb");
    if (f) {
      uint8 b[2 * kSlot];
      size_t n = fread(b, 1, sizeof(b), f);
      fclose(f);
      if (n != sizeof(b))
        return false;
      memcpy(g_sram + kFile3, b, kSlot);
      memcpy(g_sram + kFile3 + kCopy2, b + kSlot, kSlot);
      remove(kBackupPath);
    } else {
      WriteFile3(NULL);  // no backup: file 3 was empty before
    }
  }
  RtlWriteSram();
  return true;
}
