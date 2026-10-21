// Mario Combat hero inside SMW. See mc_smw.h.
//
// Timing: Mario Combat runs at 50 fps, SMW at 60. The MC logic runs exactly 5 ticks every 6 SMW frames, so
// every timer, combo window and animation lasts as long in real time as in the original. The displacement of each
// tick (MC px per tick) becomes an SMW speed (1/16 px per frame) = d / 3 * 50/60 * 16, so positions over time match.
//
// Collision: SMW keeps its own block collision (slopes, ledges, ?-blocks, coins, pipes, doors). The MC probes and
// push-out side effects are reproduced from SMW's results (McWorld.external), and MC's wall probes at x +-22, y-20
// test SMW map16 solidity directly.
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "mc_smw.h"
#include "mc_level.h"
#include "mc_core.h"
#include "mc_data.h"
#include "mc_overlay.h"
#include "mc_audio.h"
#include "mc_hud.h"
#include "mc_koopa.h"
#include "mc_bowser.h"
#include "mc_hits.h"
#include "mc_logo_data.h"
#include "../consts.h"
#include "../funcs.h"
#include "../smw_rtl.h"
#include "../variables.h"
#include "../config.h"
#include "../util.h"
#include "assets/smw_assets.h"
#include "../cheats.h"
#include "../feedback.h"

#define SCALE 3.0  // MC stage px per SMW px

uint16 GetLevelLayoutPtr_Vertical(int i);
uint16 GetLevelLayoutPtr_Horizontal(int i);

static McHero g_mc;
static McWorld g_world;
static bool g_need_init = true;
static int g_phase;              // 0..5: ticks happen on 5 of every 6 frames
static double g_vx, g_vy;        // displacement of the last tick, MC px
static double g_rx, g_ry;        // sub-1/16 px remainders carried between frames
static uint8 g_blocked_accum;    // SMW blocked flags seen since the last tick
static int g_last_iy;            // the y speed given to SMW last frame (to see solid sprites bump it)
static bool g_jump_lock;         // jump held through a head bump: ignored until released
static bool g_air_jump_used, g_pounding;  // double-jump / ground-pound cheats (AirMoves)
static bool g_was_puppet;        // SMW drove the player last level frame
static int g_cape_shift_x, g_cape_shift_y;  // SMW cape anchor shift for MC Mario's shape (McSmw_CapeOffset)
static bool g_puppet;            // SMW drives the motion (cutscenes, pipes, Yoshi, swimming...)
static bool g_dead;
static bool g_air_attack_press;  // this tick: the punch newly pressed in the air (see Resolve)
static bool g_bowser_half;       // one punch landed on Bowser: the next one is the hit (PunchBoss)
static bool g_held_rise;         // rising from a speed SMW gave with jump held (McSmw_PlayerPhysics)
static bool g_ticked_this_frame;
enum { kTickQ = 4 };
static struct { double start, dx, dy; } g_tq[kTickQ];  // ticks whose displacement is still being spread
static int g_ntq;
static double g_frame_time;      // in SMW frames; frame f covers [f, f+1)
static double g_next_tick_time;  // start time of the next MC tick, in SMW frames
static double g_fdx, g_fdy;      // this frame's share of tick displacements, MC px
static double g_life = 10;       // MC hero life (onClipEvent(load): life = 10)
static uint8 g_last_power;

static int g_last_prio;          // last PlayerDraw parameters, to re-render while the level is frozen
static bool g_last_visible, g_hud_shown;
static bool g_active = true;      // playing as MC Mario (true) or the original SMW Mario (false)
static bool g_from_map = true;    // came from the title/overworld: the next level starts as MC Mario
static bool g_toggle_request;

// SMW-style running on top of MC's own walk/run: Y or X held fills the P-meter and scales the ground speed.
static double g_run_mult = 1;     // current multiplier of MC's horizontal motion
static bool g_smw_air;            // a P-speed cape take-off: SMW's physics fly this jump
static int g_smw_air_frames;
enum { kPMeterFull = 0x70 };
static const double kRunMult = 1.12, kSprintMult = 1.30;

// Attack hit boxes stay live for a few frames, relative to the feet, so a hit whose MC frame fell between two SMW
// frames or one tick before the enemy arrived still lands.
enum { kLingerFrames = 4, kMaxLinger = 8 };
static struct { int ttl, level; double x0, y0, x1, y1; } g_linger[kMaxLinger];

static void PlaySounds(void);
static void Render(int obj_priority, bool visible);
static void UpdateCapePose(bool grounded);

bool McSmw_Enabled(void) { return g_config.mc_enable && g_active; }
bool McSmw_CharacterIsMc(void) { return g_config.mc_enable && (g_active != g_toggle_request); }

void McSmw_ToggleCharacter(void) {
  if (g_config.mc_enable && misc_game_mode == 0x14)
    g_toggle_request = !g_toggle_request;
}

static double FeetX(void) { return (int16)player_xpos + 8 + player_sub_xpos / 256.0; }
static double FeetY(void) { return (int16)player_ypos + 32 + player_sub_ypos / 256.0; }

// Layer-1 map16 solidity at a level pixel, mirroring GetPlayerLevelCollisionMap16ID_Entry2 without side effects.
static int Map16At(int x, int y) {  // -1 outside the level
  if (x < 0 || y < 0)
    return -1;
  uint16 bx = (uint16)x, by = (uint16)y, r0w;
  uint16 level_height = g_lunar_magic ? lm_level_height : 0x1b0;
  if (misc_level_layout_flags & 1) {
    if ((bx >> 8) >= 2 || (by >> 8) >= misc_screens_in_lvl)
      return -1;
    r0w = GetLevelLayoutPtr_Vertical(by >> 8) + ((by & 0xF0) | ((uint8)bx >> 4) | (bx >> 8) << 8);
  } else {
    if (by >= level_height || (bx >> 8) >= misc_screens_in_lvl)
      return -1;
    r0w = GetLevelLayoutPtr_Horizontal(bx >> 8) + ((by & 0xF0) | ((uint8)bx >> 4) | (by >> 8) << 8);
  }
  return g_ram[r0w] | g_ram[0x10000 + r0w] << 8;
}

// What a tile acts like for the player, as SMW's ModifyMap16IDForSpecialBlocks makes it: the switch-palace switch
// (page-0 tiles EC-FB) is a solid used block (132); with the blue P-switch running, coins (2B) are used blocks and used
// blocks (132) coins, and the "coin" tile 29 is empty (24); with the silver P-switch, 12F is a coin.
static int ActsLike(int t) {
  if (t < 0) return t;
  int lo = t & 0xFF;
  if (t < 0x100) {
    if (lo == 0x29 && timer_blue_pswitch) return 0x24;
    if (lo == 0x2B && timer_blue_pswitch) return 0x132;
    if ((uint8)(lo + 20) < 0x10) return 0x132;
    return t;
  }
  if (lo == 0x32 && timer_blue_pswitch) return 0x2B;
  if (lo == 0x2F && timer_silver_pswitch) return 0x2B;
  return t;
}

int McSmw_LevelMap16At(int x, int y) { return ActsLike(Map16At(x, y)); }

// Layer 2 is solid for the player too in some levels (flag 0x80 of $5B: Valley of Bowser 2's moving pillars, the
// layer-2 platforms of the castles...): HandlePlayerLevelCollision runs the block code a second time with Mario's
// position moved by $26/$28 into layer 2's tile map (the level data's second half), and skips layer 1 when flag 0x40
// is set. MC's own tile tests do the same, or MC Mario walks and jumps into layer-2 walls faster than SMW pushes him
// out (1 px per frame) until SMW takes him for crushed.
static bool LayerActive(int l) {
  return l ? (misc_level_layout_flags & 0x80) != 0 : (misc_level_layout_flags & 0x40) == 0;
}
static int LayerOX(int l) { return l ? (int16)misc_second_level_layer_xpos : 0; }
static int LayerOY(int l) { return l ? (int16)misc_second_level_layer_ypos : 0; }

static int Map16AtLayer(int l, int x, int y) {  // x, y in level (layer 1) pixels
  if (!l)
    return Map16At(x, y);
  x += LayerOX(1), y += LayerOY(1);
  if (x < 0 || y < 0)
    return -1;
  uint16 bx = (uint16)x, by = (uint16)y, r0w;
  uint16 level_height = g_lunar_magic ? lm_level_height : 0x1b0;
  if (misc_level_layout_flags & 2) {
    if ((bx >> 8) >= 2 || (by >> 8) >= 0xE)
      return -1;
    r0w = GetLevelLayoutPtr_Vertical((by >> 8) + 14) + ((by & 0xF0) | ((uint8)bx >> 4) | (bx >> 8) << 8);
  } else {
    if (by >= level_height || (bx >> 8) >= 0x10)
      return -1;
    r0w = GetLevelLayoutPtr_Horizontal((bx >> 8) + 16) + ((by & 0xF0) | ((uint8)bx >> 4) | (by >> 8) << 8);
  }
  return g_ram[r0w] | g_ram[0x10000 + r0w] << 8;
}

static bool SolidAtLayer(int l, int x, int y) {
  if (!LayerActive(l))
    return false;
  int t = ActsLike(Map16AtLayer(l, x, y));
  return t >= 0x111 && t < 0x16E;  // map16 0x111-0x16D: solid on every side
}

static bool FloorTopAtLayer(int l, int x, int y) {  // the top row of a tile the player stands on (0x100-0x110 solid on top only)
  if (!LayerActive(l))
    return false;
  int t = ActsLike(Map16AtLayer(l, x, y));
  return t >= 0x100 && t < 0x16E;
}

static bool SolidAt(int x, int y) { return SolidAtLayer(0, x, y) || SolidAtLayer(1, x, y); }

// Solid sprites (flying ? blocks, used or not, Reznor's platforms, the line-guided and floating platforms, message
// boxes...): SMW's SolidSpriteBlock pushes the player out of their sides but sets no blocked flag, so MC Mario saw no
// wall there: no cling, no wall jump. Each call records the sprite's box where the routine sees it (some sprites move
// themselves onto their platform for the call) and the player x it pushes him out to on either side; a push out of a
// side counts as that side blocked, and the wall probes see the sprite while Mario rests at one of those x.
// (slot 12: a platform of the level's own, the Mario Combat level's lift: McSmw_LevelPlatformBox)
static struct { int x0, y0, x1, y1, sy, stop_l, stop_r; uint8 frame; bool on, top_only; } g_solid_box[13];
static uint8 g_sprite_push;  // sides (1 right, 2 left) a solid sprite pushed the player out of since the last frame

void McSmw_SolidSpriteBox(uint8 k, uint16 stop_right_of, uint16 stop_left_of) {
  if (k >= 12)
    return;
  CollInfo ci;
  StandardSpriteToSpriteCollisionChecks_GetSpriteClippingA(k, &ci);
  g_solid_box[k].x0 = (int16)(ci.r10 << 8 | ci.r4), g_solid_box[k].x1 = g_solid_box[k].x0 + ci.r6;
  g_solid_box[k].y0 = (int16)(ci.r11 << 8 | ci.r5), g_solid_box[k].y1 = g_solid_box[k].y0 + ci.r7;
  g_solid_box[k].sy = (int16)GetSprYPos(k);
  g_solid_box[k].stop_l = (int16)stop_left_of, g_solid_box[k].stop_r = (int16)stop_right_of;
  g_solid_box[k].frame = counter_global_frames;
  g_solid_box[k].on = true;
  g_solid_box[k].top_only = (spr_property_bits190f[k] & 1) != 0;  // (SolidSpriteBlock pushes nothing out sideways)
}

void McSmw_TopPlatformBox(uint8 k, int x0, int y0, int w, int h, int sy) {
  if (k >= 13)
    return;
  g_solid_box[k].x0 = (int16)x0, g_solid_box[k].x1 = (int16)x0 + w;
  g_solid_box[k].y0 = (int16)y0, g_solid_box[k].y1 = (int16)y0 + h;
  g_solid_box[k].sy = (int16)sy;
  g_solid_box[k].stop_l = g_solid_box[k].stop_r = 0;
  g_solid_box[k].frame = counter_global_frames;
  g_solid_box[k].on = g_solid_box[k].top_only = true;
}

void McSmw_LevelPlatformBox(int x0, int y0, int w, int h, int sy) {
  McSmw_TopPlatformBox(12, x0, y0, w, h, sy);
}

bool McSmw_DebugPlatformTop(uint8 k, int *x, int *sy) {
  if (k >= 12 || !g_solid_box[k].on || (uint8)(counter_global_frames - g_solid_box[k].frame) > 1)
    return false;
  *x = (g_solid_box[k].x0 + g_solid_box[k].x1) / 2, *sy = g_solid_box[k].sy;
  return true;
}

void McSmw_SolidSpritePush(bool player_left) {
  if (McSmw_Enabled())
    g_sprite_push |= player_left ? 1 : 2;
}

static bool g_side_launch;  // McSmw_SideLaunch: SMW gave the player a sideways speed since the last frame
void McSmw_SideLaunch(void) { g_side_launch = true; }

// A solid sprite's side right beside the player (side 1: to his right, 2: to his left), at a height where SMW pushes
// him out sideways: his body box overlaps the sprite's, and he is neither on top of it nor under it (SolidSpriteBlock).
static bool SolidSpriteWall(int side) {
  int px = (int16)player_xpos, py = (int16)player_ypos;
  CollInfo ci;
  StandardSpriteToSpriteCollisionChecks_GetMarioClipping(&ci);
  int my0 = (int16)(ci.r9 << 8 | ci.r1), my1 = my0 + ci.r3;
  int under = (player_ducking_flag || !player_current_power_up) ? 8 : 0;
  if (player_riding_yoshi_flag)
    under += 8;
  for (int k = 0; k < 12; k++) {
    if (!g_solid_box[k].on || g_solid_box[k].top_only || spr_current_status[k] != 8 ||
        (uint8)(counter_global_frames - g_solid_box[k].frame) > 1 || my1 <= g_solid_box[k].y0 || my0 >= g_solid_box[k].y1 || py < g_solid_box[k].sy - 24 ||
        py >= g_solid_box[k].sy - under)
      continue;
    if (side == 1 ? px >= g_solid_box[k].stop_l - 1 && px <= g_solid_box[k].stop_l + 2
                  : px <= g_solid_box[k].stop_r + 1 && px >= g_solid_box[k].stop_r - 2)
      return true;
  }
  return false;
}

// The MC probes/push-outs (DefineSprite_440 onEnterFrame, first block) from SMW's collision results.
static void Resolve(void *ctx, McHero *h) {
  uint8 bl = g_blocked_accum;
  double fx = FeetX(), fy = FeetY();
  // `onGround = hitTest(x, y+2)` after `y -= grav`: rising faster than the 2 px probe leaves the ground.
  // (Player state 0A, the walk to a castle / ghost house door: SMW walks Mario along the floor without its collision
  // code, so no blocked flags; he is on the ground all the way, and walks in MC's walk rather than falling in the air.)
  bool ground = ((bl & 4) != 0 || misc_player_on_solid_sprite || player_current_state == 0x0A) && h->grav <= 2;
  h->onGround = ground;
  // The screen edge of an auto-scrolling level stops the player too (flag 0x80 with the side bit), but it is no wall
  // to cling to or kick off (MC Mario slid slowly down it in Butter Bridge 1): there only blocks count.
  uint8 side = (bl & 0x80) ? 0 : bl;
  int lx = (int)floor(fx - 22 / SCALE), rx = (int)floor(fx + 22 / SCALE), wy = (int)floor(fy - 20 / SCALE);
  h->onLeftWall = (side & 2) || SolidAt(lx, wy) || SolidSpriteWall(2);
  h->onRightWall = (side & 1) || SolidAt(rx, wy) || SolidSpriteWall(1);
  // MC cuts an attack in the air short when the hero touches a wall (`if(!onGround && fighting) if(walls) jump`).
  // In SMW's levels that stops the air kick at every block, so mid-air bricks are hard to break: an attack in the air
  // plays out instead (ClampIntoWalls still stops him at the wall). See also Hit().
  // The same on the tick an air attack starts (the punch pressed in the air): otherwise the core, falling beside a
  // block, starts the air kick and at once cuts it back to the jump pose on that tick, or, clinging to the block in its
  // wall pose, never starts it (an air attack starts from the jump pose only). With no wall seen the cling ends and
  // the kick starts, beside a brick as in the open air.
  if (!ground && (h->fighting || g_air_attack_press))
    h->onLeftWall = h->onRightWall = false;
  // MC clings to a wall as soon as he moves into it in the air (`grav = 2; walling`), which kills a jump's rise: next to
  // a 3-tile ledge he could never jump up onto it, as SMW Mario does by holding toward it. While still rising the walls
  // are hidden from the core (ClampIntoWalls stops him at them), so the jump carries him up along the wall; the cling
  // and the wall jump work from the top of the jump on.
  if (!ground && h->grav > 0 && !h->walling)
    h->onLeftWall = h->onRightWall = false;
  if (ground && h->grav < 0)   // while(hitTest(x, y-1)) { grav = 0; y--; }
    h->grav = 0;
  if (bl & 3) {                // while(hitTest(x -+ 20, y-20)) { x+-; if(onGround) xSpeed = 0; else if(fighting) xSpeed = 0; }
    if (h->onGround) h->xSpeed = 0;
    else if (h->fighting) h->xSpeed = 0;
  }
  if (bl & 8)                  // while(hitTest(x, y-80)) { grav = -2; y++; }
    h->grav = -2;
}

static bool Hit(void *ctx, double x, double y) {
  // In SMW mode the core only probes here for its "attack in the air meets a wall" cut-off (x +- 30): an attack in
  // the air plays out against walls (see Resolve).
  if (g_mc.fighting && !g_mc.onGround)
    return false;
  return SolidAt((int)floor(x / SCALE), (int)floor(y / SCALE));
}

static void InitHeroEx(bool fresh_level);
// A level entered from the map starts with a full life bar; a room change (pipe, door) keeps it, as SMW keeps Mario's
// power-up.
static bool g_level_fresh = true;
static void InitHero(void) {
  InitHeroEx(g_level_fresh);
  g_level_fresh = false;
}

void McSmw_OnLevelSelect(void) {
  g_level_fresh = true;
  g_need_init = true;
}

static void InitHeroEx(bool fresh_level) {
  memset(&g_world, 0, sizeof(g_world));
  g_world.external = true;
  g_world.resolve = Resolve;
  g_world.hit = Hit;  // only the airborne-fighting probes at x +- 30 use it in external mode
  g_world.no_wall_pull = true;  // SMW keeps the player against walls (see McSmw_PlayerPhysics)
  memset(g_linger, 0, sizeof(g_linger));
  McHero_Init(&g_mc, &g_world, FeetX() * SCALE, FeetY() * SCALE);
  g_mc.xscale = player_facing_direction ? 100 : -100;
  g_phase = 0;
  g_vx = g_vy = g_rx = g_ry = 0;
  g_ntq = 0;
  g_frame_time = g_next_tick_time = 0;
  g_fdx = g_fdy = 0;
  g_blocked_accum = 0;
  g_last_iy = 0;
  g_jump_lock = false;
  g_air_jump_used = g_pounding = false;
  g_was_puppet = false;
  g_run_mult = 1;
  g_smw_air = false;
  g_need_init = false;
  g_dead = false;
  g_bowser_half = false;
  g_held_rise = false;
  if (g_config.mc_life_bar && fresh_level) {
    g_life = 10;
    // MC Mario is always full-size: use Big Mario's hitbox (and block breaking) from the start.
    if (player_current_power_up == 0)
      player_current_power_up = 1;
  }
  g_last_power = player_current_power_up;
}

static bool g_next_hit_heavy;  // McSmw_SetNextHitHeavy
static int g_next_hit_dir;     // McSmw_SetNextHitDir

// Knockback cheat (1x-20x): after MC's own hit (McHero_TakeHit: xSpeed +-6, or +-10 and grav 10 for a heavy hit) the
// push is made N times as far. Distance grows with the square of speed (ground friction, gravity), so both speeds
// are scaled by the square root of N: N times as far along the ground, N times as high and as far through the air.
static void ScaleKnockback(bool heavy) {
  int n = Cheats_KnockbackScale();
  if (n <= 1)
    return;
  double s = sqrt((double)n);
  g_mc.xSpeed *= s;
  if (heavy)  // a light hit leaves grav alone (it may be his own jump)
    g_mc.grav *= s;
}

// Life-bar mode replacement of DamagePlayer_Hurt: same "can be hurt" conditions as SMW, then MC's hurt
// (knockback away from the enemy, hurt animation) and life loss instead of powering down.
bool McSmw_HandleHurt(void) {
  if (!McSmw_Enabled() || !g_config.mc_life_bar || misc_game_mode != 0x14)
    return false;
  if (player_current_state || timer_end_level || timer_star_power || timer_player_hurt)
    return true;
  counter_pink_berry_cloud_coins = 0;
  if (player_wall_walk_status)
    RunPlayerBlockCode_00EB48(player_wall_walk_status & 1);
  if (!Cheats_InfiniteLife())  // settings-menu cheat: still knocked back, no life lost
    g_life -= g_config.mc_damage_per_hit;
  if (g_life <= 0.001) {
    g_life = 0;
    DamagePlayer_Kill();
    return true;
  }
  timer_player_hurt = 0x7F;  // the invulnerability blink SMW gives after a power-down
  int dir = player_facing_direction ? -1 : 1;
  uint8 k = spr_current_slotid;
  if (k < 12 && spr_current_status[k] >= 8)  // pushed away from the sprite that hit us
    dir = (int16)player_xpos + 8 >= (int16)GetSprXPos(k) + 8 ? 1 : -1;
  if (g_next_hit_dir)  // an MC enemy's fire or shockwave says which way
    dir = g_next_hit_dir;
  if (Cheats_NoKnockback()) {  // settings-menu cheat: the hit costs life and blinks, but Mario keeps going
    io_sound_ch1 = 4;          // SMW's power-down sound in place of MC's hurt clip
    return true;
  }
  if (!g_need_init) {
    McHero_TakeHit(&g_mc, dir, g_next_hit_heavy, 0);
    ScaleKnockback(g_next_hit_heavy);
    PlaySounds();
  }
  g_next_hit_heavy = false;
  return true;
}

void McSmw_EnemyHurtsPlayer(void) {
  if (timer_star_power)
    return;
  if (player_riding_yoshi_flag) {  // SMW's enemy fire: Mario is thrown off Yoshi, who runs away
    if (!Cheats_NoDamage() && !timer_player_hurt && !player_current_state)
      CheckMarioToExtendedSpriteColl_LoseYoshi();
    return;
  }
  DamagePlayer_Hurt();
}

void McSmw_SetNextHitHeavy(bool heavy) { g_next_hit_heavy = heavy; }
void McSmw_SetNextHitDir(int dir) { g_next_hit_dir = dir; }
double McSmw_GetLife(void) { return g_life; }

void McSmw_GetHeroInfo(McHeroInfo *hi) {
  hi->feet_x = FeetX();
  hi->feet_y = FeetY();
  hi->alive = player_current_state != 9 && !g_dead;
  hi->mc = McSmw_Enabled() && !g_need_init && !McSmw_UseSmwSprite();
  if (hi->mc) {
    double ox = hi->feet_x - g_mc.x / SCALE, oy = hi->feet_y - g_mc.y / SCALE, x0, y0, x1, y1;
    McHero_GetBound(&g_mc, &x0, &y0, &x1, &y1);
    hi->x0 = x0 / SCALE + ox, hi->y0 = y0 / SCALE + oy, hi->x1 = x1 / SCALE + ox, hi->y1 = y1 / SCALE + oy;
    hi->xspeed = g_mc.xSpeed;
    hi->xscale = g_mc.xscale;
    hi->falling = !g_mc.onGround && (int8)player_yspeed > 0;
  } else {  // SMW Mario's body (big / small / ducking), his speed in MC stage px per tick
    int top = (player_current_power_up && !player_ducking_flag) ? 8 : 18;
    if (player_riding_yoshi_flag) top -= 8;
    hi->x0 = (int16)player_xpos + 2, hi->x1 = (int16)player_xpos + 14;
    // riding Yoshi: Mario sits 16 px higher and Yoshi's body below him is part of what enemies touch
    hi->y0 = (int16)player_ypos + top, hi->y1 = (int16)player_ypos + (player_riding_yoshi_flag ? 48 : 32);
    hi->xspeed = (int8)player_xspeed * (SCALE * 1.2 / 16);
    hi->xscale = player_facing_direction ? 100 : -100;
    hi->falling = player_in_air_flag && (int8)player_yspeed > 0;
  }
}

static void DrawHud(void) {
  const McHud *hud = &kMcHud;
  int ox = 4, oy = 34;
  int cut = hud->bar_x0 + (int)((hud->bar_x1 - hud->bar_x0) * g_life / 10.0 + 0.5);
  McImage im = { hud->w, hud->h, 1, 0, 0, NULL };
  static uint32 buf[256 * 64];
  for (int y = 0; y < hud->h; y++)
    for (int x = 0; x < hud->w; x++) {
      uint32 p = hud->px[y * hud->w + x];
      buf[y * hud->w + x] = hud->is_bar[y * hud->w + x] && x >= cut ? 0 : p;
    }
  im.px = buf;
  float m[6] = { 1, 0, 0, 1, (float)ox, (float)oy };
  if (Cheats_MirrorActive()) {
    // Mirror mode: the overlay is merged into the flipped lines mirrored (McOverlay_ApplyLine), so the bar is drawn
    // mirrored here and reads the right way round.
    m[0] = -1, m[4] = (float)(256 - ox);
  }
  McOverlay_DrawImage(&im, m, NULL, 0xffff);
}

static uint8 RawPad1(void) {  // the unmerged B/Y/Select/Start/dpad byte of the active controller (PollJoypadInputs)
  uint8 v0 = (io_controllers_plugged_in & 0x80) ? player_current_character : io_controllers_plugged_in;
  return *(&io_controller_hold1_copyp1 + v0);
}

// Standing in front of a door (SMW's door tiles 1F / 20, 9C in ghost houses, and the P-switch doors 27 / 28 while the
// blue P-switch runs): Up is for the door there, not a jump ("Up to jump" setting), so a press a few pixels off the
// door's middle doesn't bounce Mario away from it.
static bool InFrontOfDoor(void) {
  if (!g_mc.onGround)
    return false;
  int fx = (int)floor(FeetX()), y = (int)floor(FeetY()) - 8;
  for (int dx = -8; dx <= 8; dx += 8) {
    int t = Map16At(fx + dx, y);
    if (t == 0x1F || t == 0x20 || (t == 0x9C && misc_level_tileset_setting == 1) ||
        ((t == 0x27 || t == 0x28) && timer_blue_pswitch))
      return true;
  }
  return false;
}

static uint8 Keys(void) {
  uint8 k = 0, h1 = io_controller_hold1, raw1 = RawPad1(), h2 = io_controller_hold2;
  if (h1 & 0x02) k |= MC_KEY_LEFT;
  if (h1 & 0x01) k |= MC_KEY_RIGHT;
  if (h1 & 0x04) k |= MC_KEY_DOWN;
  if ((h1 & 0x80) || (g_config.mc_up_jumps && (h1 & 0x08) && !InFrontOfDoor())) k |= MC_KEY_JUMP;  // B or A (or Up)
  if (raw1 & 0x40) k |= MC_KEY_PUNCH;                                           // Y
  // MC's wall kick fires while jump is merely held against a wall in the air, so the jump button still held from the
  // take-off kicked Mario away from every wall he jumped along (players and testers read it as being pushed sideways).
  // After the short-hop window (where holding jump sets the height) a jump held since the take-off doesn't count;
  // a new press in the air does, so wall jumps work as before.
  static bool held_since_takeoff;
  bool jump_down = (k & MC_KEY_JUMP) != 0;
  if (g_mc.onGround)
    held_since_takeoff = jump_down;
  else if (!jump_down)
    held_since_takeoff = false;
  if (!g_mc.onGround && held_since_takeoff && g_mc.shortHop == 0)
    k &= ~MC_KEY_JUMP;
  // MC jumps again on landing while jump is held. After a head bump that would bonk the block (and reopen a message
  // box) every few frames, so a bump uses up the held jump, as in SMW: release it to jump again.
  if (g_jump_lock) {
    if (k & MC_KEY_JUMP) k &= ~MC_KEY_JUMP;
    else g_jump_lock = false;
  }
  (void)h2;
  return k;
}

void McSmw_FilterInputs(void) {
  if (!McSmw_Enabled() || misc_game_mode != 0x14)
    return;
  // SMW merges Y and X into bit 6 of $15 (held) and $16 (pressed). A Y press is MC's punch, so only X presses
  // fire / cape-spin. Held Y stays in $15: holding the punch button grabs and carries items, as in SMW.
  if (!(io_controller_press2 & 0x40))
    io_controller_press1 &= ~0x40;
}

static bool RunHeld(void) {  // Y (punch) or X (grab/run) held
  return (RawPad1() & 0x40) || (io_controller_hold2 & 0x40);
}

// SMW's P-meter ($13E4) from MC's motion: +2 per frame running at full speed on the ground, -1 otherwise.
static void UpdateRun(bool grounded) {
  bool full_speed = fabs(g_mc.xSpeed) >= g_mc.maxMoveSpeed - 1.5;  // MC's top speed alternates 9 / 10
  if (grounded) {
    if (RunHeld() && full_speed && !g_mc.fighting && !g_mc.hurt)
      player_pmeter = (uint8)IntMin(kPMeterFull, player_pmeter + 2);
    else if (player_pmeter)
      player_pmeter--;
  }
  double target = 1;
  if (grounded && RunHeld() && fabs(g_mc.xSpeed) > 1 && !g_mc.hurt)
    target = player_pmeter >= kPMeterFull ? kSprintMult : kRunMult;
  else if (!grounded)
    target = g_run_mult;  // keep the momentum through a jump
  g_run_mult += IntMax(-1, IntMin(1, (target > g_run_mult) - (target < g_run_mult))) * 0.02;
  if (fabs(g_run_mult - target) < 0.02)
    g_run_mult = target;
  if (Cheats_PSpeed())
    player_pmeter = kPMeterFull;
}

static void PlaySounds(void) {
  if (g_config.mc_sounds)
    for (int i = 0; i < g_mc.nsounds; i++)
      McAudio_Play(g_mc.sounds[i]);
  g_mc.nsounds = 0;
}

// How much bigger the SMW hit area is than MC's hit1..hit6 boxes (SMW px). MC's boxes are tuned for MC's enemies,
// which stop and wait to attack; SMW enemies walk straight into the player, so a strict 1-tick box lets them touch
// Mario first. The padding covers the punching art, and the boxes linger a few frames (kLingerFrames).
enum { kHitPad = 4, kHitReach = 10 };

// Strong attacks (MC hit level 4+: the combo finishers / uppercut, the low sweep, the air spin kick) knock out at once.
// Normal hits (jab, straight, hook, dash punch: levels 1-3) count like a stomp on enemies SMW makes you stomp more
// than once: Rex shrinks first, a Chargin' Chuck takes three, a Dino-Rhino becomes a Dino-Torch.
enum { kStrongHitLevel = 4 };
static uint8 g_spr_hit_cd[12];  // frames before a sprite can be hit again (the boxes linger over several frames)
static int g_boss_punches;      // punches that hurt a boss (pilot `bosspunches`)
static int g_punch_cx, g_punch_cy;  // the middle of the punch box being checked (level px)
int McSmw_DebugBossPunches(void) { return g_boss_punches; }

// Hit chains score like SMW's stomp chains (CheckPlayerToNormalSpriteColl_01AB46): the n-th hit gives GivePoints level
// n (200, 400, 800, 1000, 2000, 4000, 8000, then 1-ups from the 8th) with the stomp sound of that step. A chain goes on
// while the same enemy is hit again within kChainFrames (juggling a shell...), or while Mario hasn't landed since the
// last hit (several enemies in one jump), as landing ends a stomp chain.
enum { kChainFrames = 120 };
static const uint8 kStompSounds[7] = { 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19 };  // kStompSoundTable_Bank01
static struct { int count, timer, slot, id; bool landed; } g_chain;
static int g_points_override = -1;
static int g_ground_jumps;  // MC Mario's jumps from the ground so far

int McSmw_PointsOverride(void) { return g_points_override; }
int McSmw_GroundJumps(void) { return g_ground_jumps; }

static int ChainLevel(uint8 i) {  // the chain step this hit would be
  bool same = i == g_chain.slot && spr_spriteid[i] == g_chain.id;
  bool goes_on = g_chain.count > 0 && g_chain.timer < kChainFrames && (same || !g_chain.landed);
  return (goes_on ? g_chain.count : 0) + 1;
}

static void ChainCommit(uint8 i, int level) {
  g_chain.count = level;
  g_chain.timer = 0;
  g_chain.slot = i;
  g_chain.id = spr_spriteid[i];
  g_chain.landed = false;
  if (level >= 2 && level < 8)
    io_sound_ch1 = kStompSounds[level - 1];  // the rising stomp sounds (the first hit keeps its own sound)
}

static bool IsChuck(uint8 id) { return (id >= 0x91 && id <= 0x98 && id != 0x96) || id == 0x46; }

static void KnockOut(uint8 i, uint8 points) {  // the star / stomp-kill reaction: flips and falls off screen
  SpawnContactEffectFromSide(i);
  GivePoints(i, points);
  spr_current_status[i] = 2;
  spr_yspeed[i] = -48;
  spr_xspeed[i] = g_mc.xscale > 0 ? 16 : -16;
}

// Normal hit on a multi-stomp enemy: SMW's own stomp reaction for that sprite, without bouncing Mario. Returns -1 if
// the sprite is not one of those, 0 if the hit did not count (a Chuck still reeling), 1 if it was applied.
static int StompLikeHit(uint8 i) {
  uint8 id = spr_spriteid[i];
  if (id == 0xAB && spr_table00c2[i] == 0) {  // Rex: first hit squashes him into the small, fast Rex
    Spr0AB_Rex_RexPoints(i);
    SpawnContactEffectFromSide(i);
    spr_table00c2[i] = 1;
    spr_decrementing_table1fe2[i] = 12;
    spr_property_bits1662[i] = 0;
    // Knocked away from the fist: the squashed Rex turns and runs off instead of walking on into Mario (a stomp
    // leaves Mario above it; a punch leaves him in its path).
    spr_table157c[i] = (int16)GetSprXPos(i) + 8 < (int16)player_xpos + 8;
    spr_decrementing_table154c[i] = 16;
    return 1;
  }
  if (IsChuck(id)) {  // Spr091_CharginChuck_ProcessPlayerInteraction's stomp
    if (spr_table00c2[i] == 3)
      return 0;
    spr_decrementing_table1564[i] = 5;
    io_sound_ch1 = 2;
    SpawnContactEffectFromSide(i);
    spr_decrementing_table163e[i] = 0;
    if (++spr_table1528[i] >= 3) {
      KnockOut(i, 3);
      io_sound_ch1 = 3;
      return 1;
    }
    io_sound_ch3 = 40;
    spr_table00c2[i] = 3;
    spr_decrementing_table1540[i] = 3;
    spr_table1570[i] = 0;
    return 1;
  }
  if (id == 0x86) {  // Wiggler: a stomp only makes it angry (Spr086_Wiggler's stomp: flash, points, flower falls off)
    if (spr_table15d0[i] | spr_table151c[i])
      return 0;  // already angry: SMW ignores further stomps too
    io_sound_ch1 = 3;
    SpawnContactEffectFromSide(i);
    GivePoints(i, 0);
    spr_decrementing_table1540[i] = 64;
    ++spr_table151c[i];
    Spr086_Wiggler_02F2D7(i);
    return 1;
  }
  if (id == 0x6E) {  // Dino-Rhino -> Dino-Torch (CheckPlayerToNormalSpriteColl's stomp transform)
    io_sound_ch1 = 2;
    SpawnContactEffectFromSide(i);
    CheckPlayerToNormalSpriteColl_01AB46(i);
    spr_table00c2[i] = 2;
    spr_decrementing_table1540[i] = 0xFF;
    spr_spriteid[i] = 0x6F;
    uint8 r15 = spr_table15f6[i] & 0xE;
    InitializeNormalSpriteRAMTables_YXPPCCCTAndPropertyTables(i);
    spr_table15f6[i] = r15 | (spr_table15f6[i] & 0xF1);
    spr_yspeed[i] = 0;
    return 1;
  }
  return -1;
}

// A punch/kick knocks out an SMW sprite. SMW's cape reaction (CheckPlayerAttackToNormalSpriteColl_029404) is used so
// every enemy reacts by SMW's rules; enemies that ignore the cape but can be stomped (Banzai Bill, ...) are knocked
// out the way a stomp would.
// Super punch cheat: enemies SMW makes immune to the cape, fire and stomps that it knocks out anyway. Bosses, Yoshi,
// platforms and items are not in the list.
static bool SuperPunchTarget(uint8 id) {
  static const uint8 kIds[] = {
    0x26, 0x27, 0x28, 0x33, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C,  // Thwomp, Thwimp, Big Boo, Podoboo, Boos, Urchins
    0x44, 0xA4, 0xA5, 0xA6, 0xA8, 0xAE, 0xB2, 0xB4,              // Torpedo Ted, spike ball, Sparky, Hothead, Blargg...
  };
  for (size_t j = 0; j < sizeof(kIds); j++)
    if (kIds[j] == id) return true;
  return false;
}

// Punch anything cheat: the enemies a stomp, the cape and a punch can't hurt (only a star can), which MC attacks then
// hurt as any other enemy: Thwomp, Thwimp, Big Boo, Podoboo, Boo, Eeries, Urchins, Torpedo Ted, the line-guided Grinder
// and Fuzzy, Wiggler, gas bubble, spike ball, Sparky / Fuzzy, Hothead, Blargg, Fishbone, Fishin' Boo, Boo block,
// falling spike, Grinder, bouncing fireball (the Pokey loses the segment hit: PunchPokey). Bosses, items, platforms and
// stage hazards (wooden spikes, chainsaws) are not in it.
static bool PunchAllTarget(uint8 id) {
  static const uint8 kIds[] = {
    0x26, 0x27, 0x28, 0x33, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x44, 0x67, 0x68,
    0x86, 0x90, 0xA4, 0xA5, 0xA6, 0xA8, 0xAA, 0xAE, 0xAF, 0xB2, 0xB4, 0xB6,
  };
  for (size_t j = 0; j < sizeof(kIds); j++)
    if (kIds[j] == id) return true;
  return false;
}

// Punch anything cheat, the Pokey: the punch knocks off the segment it lands on, as a kicked shell does
// (Spr070_Pokey_02B7AC with the fist in the shell's place): that segment flies off (Spr070_Pokey_02B82E's falling
// segment), the ones above drop down, and the last one gone ends the Pokey.
static bool PunchPokey(uint8 i) {
  if (spr_table1534[i] || spr_decrementing_table1558[i] || !spr_table00c2[i])
    return false;  // a segment already flying off, or one just knocked off
  int fist_y = g_punch_cy - 8;  // (the shell's y is its top)
  uint8 before = spr_table00c2[i];
  uint8 r13 = Spr070_Pokey_RemovePokeySegment(i, (uint8)(fist_y - spr_ypos_lo[i]));
  if (spr_table00c2[i] == before)
    return false;  // (no segment at that height)
  uint8 j = FindFreeNormalSpriteSlot_HighPriority();
  if ((j & 0x80) == 0) {
    spr_current_status[j] = 2;
    spr_spriteid[j] = 0x70;
    SetSprXPos(j, GetSprXPos(i));
    InitializeNormalSpriteRAMTables(j);
    SetSprYPos(j, (uint16)fist_y);
    spr_xspeed[j] = g_mc.xscale > 0 ? 0x10 : (uint8)-0x10;  // (half a kicked shell's speed, as SMW gives it)
    spr_yspeed[j] = (uint8)-32;
    spr_table00c2[j] = r13 & spr_table00c2[i];
    spr_table1534[j] = 1;
    GivePoints_Entry2(j, 1);
  }
  CheckNormalSpriteToNormalSpriteColl_PlayKickSfx();
  return true;
}

// "MC hit reactions" cheat: how the enemy in slot i takes MC hits (Cheats_EnemyKind: 1 walks, 2 flies, 3 tied to its
// place), or 0 when the punch is left as it is: not an ordinary enemy (bosses, items, platforms), the Wiggler (only a
// star defeats it), or one a punch can't defeat (Thwomps, Boos, ...: immune as before, unless the punch anything cheat
// is on).
static int McHitsKind(uint8 i) {
  uint8 id = spr_spriteid[i];
  int kind = Cheats_EnemyKind(id);
  if (id == 0x1E || id == 0x1F || id == 0x9B)  // Lakitu (his cloud), Magikoopa (teleports), Hammer Bro (platform)
    kind = 3;
  if ((id == 0x4D || id == 0x4E) && spr_table00c2[i] >= 2)  // a Monty Mole out of the ground walks (states 2-3)
    kind = 1;
  bool punch_all = Cheats_PunchAll() && PunchAllTarget(id);  // (they take MC hits too: knocked flying)
  if (!kind || (id == 0x86 && !punch_all))
    return 0;
  bool defeatable = IsChuck(id) || id == 0xAB || id == 0x6E || id == 0x6F ||
                    ((spr_property_bits167a[i] & 2) == 0 && (spr_property_bits166e[i] & 0x20) == 0) ||
                    (spr_property_bits1656[i] & 0x10) || punch_all;
  return defeatable ? kind : 0;
}

// Bosses: a punch that lands is one of SMW's own hits for that boss, with the boss's own hurt and invulnerable times:
// the stomp for the Koopa Kids, the hit from below for Reznor, a thrown block for the Big Boo boss and a Mechakoopa on
// the head for Bowser. Any hit level counts as one.
static bool IsBoss(uint8 i) {
  uint8 id = spr_spriteid[i];
  return id == 0x29 || id == 0xA9 || id == 0xC5 || id == 0xA0;
}

// Bowser can be hit while Spr0A0_ActivateBowserBattle_03B078 checks his Mechakoopas: in a fighting phase, the clown car
// facing forward and upright.
static bool BowserCanBeHit(uint8 i) {
  return (uint8)misc_m7_angle == 32 && spr_table151c[i] >= 7 && !misc_m7_rotation && !bowserbattle_var14b5;
}

// The box SMW checks a hit against. Morton / Roy / Ludwig and Iggy / Larry, drawn on the mode 7 layer, have their own
// (Spr029_KoopaKid_MortonRoyLudwig_01D40B; Iggy / Larry's in screen space, Spr029_KoopaKid_IggyLarry_01FC62). Bowser's is
// his clown car and his head (clippings 36 and 37 of Spr0A0_ActivateBowserBattle_03B0F3). Other sprites: their clipping.
static bool PunchTouches(uint8 i, int x0, int y0, int w, int h) {
  CollInfo ci;
  uint8 id = spr_spriteid[i];
  if (id == 0x29 && spr_table00c2[i] >= 3 && spr_table00c2[i] <= 4) {
    PointU16 pt = Spr029_KoopaKid_IggyLarry_01FD50(i);
    int sx = x0 - ((int16)player_xpos - (int16)player_on_screen_pos_x);
    int sy = y0 - ((int16)player_ypos - (int16)player_on_screen_pos_y);
    int bx = (uint8)(pt.x - 8), by = (uint8)(pt.y + 96);
    return sx < bx + 15 && bx < sx + w && sy < by + 12 && by < sy + h;
  }
  SetHiLo(&ci.r8, &ci.r0, (uint16)x0);
  ci.r2 = (uint8)IntMin(w, 255);
  SetHiLo(&ci.r9, &ci.r1, (uint16)y0);
  ci.r3 = (uint8)IntMin(h, 255);
  if (id == 0x29 && spr_table00c2[i] <= 2) {
    Spr029_KoopaKid_MortonRoyLudwig_01D40B(i, &ci);
    return StandardSpriteToSpriteCollisionChecks_CheckContact(&ci);
  }
  if (id == 0xA0 && !McBowser_IsActive(i)) {
    uint8 clip = spr_property_bits1662[i];
    bool touch = false;
    for (uint8 c = 36; c <= 37 && !touch; c++) {
      spr_property_bits1662[i] = c;
      StandardSpriteToSpriteCollisionChecks_GetSpriteClippingA(i, &ci);
      touch = StandardSpriteToSpriteCollisionChecks_CheckContact(&ci);
    }
    spr_property_bits1662[i] = clip;
    return touch;
  }
  StandardSpriteToSpriteCollisionChecks_GetSpriteClippingA(i, &ci);
  return StandardSpriteToSpriteCollisionChecks_CheckContact(&ci);
}

// 1: the boss took the hit; 0: not now (hurt, invulnerable, out of reach of hits); -1: not a boss.
static int PunchBoss(uint8 i) {
  uint8 id = spr_spriteid[i];
  if (id == 0x29) {
    uint8 kid = spr_table00c2[i];
    if (kid <= 2) {  // Morton, Roy, Ludwig
      int r = McSmw_PunchMortonRoyLudwig(i);
      if (r == 2)
        g_spr_hit_cd[i] = 10;  // bounced off: once per punch
      return r == 1;
    }
    if (kid <= 4) {  // Iggy, Larry (Spr029_KoopaKid_IggyLarry_01FC62): into his shell, sliding down the tilted platform
      if (player_current_state || spr_table160e[i] || spr_decrementing_table1558[i])
        return 0;
      PointU16 pt = Spr029_KoopaKid_IggyLarry_01FD50(i);
      spr_decrementing_table1558[i] = 8;
      io_sound_ch3 = 40;
      uint8 x = spr_xpos_lo[i], y = spr_ypos_lo[i];
      spr_xpos_lo[i] = pt.x - 8;
      spr_ypos_lo[i] = pt.y - 16;
      spr_xoffscreen_flag[i] = 0;
      SpawnContactEffectFromAbove(i);
      spr_ypos_lo[i] = y;
      spr_xpos_lo[i] = x;
      if (!spr_decrementing_table154c[i])
        spr_decrementing_table154c[i] = 24;
      return 1;
    }
    // Lemmy, Wendy (Spr029_KoopaKid_WendyLemmy_CheckMarioStomp): only while out of a pipe; a decoy just ducks
    if (spr_table151c[i] != 2)
      return 0;
    SpawnContactEffectFromAbove(i);
    GivePoints(i, 2);
    io_sound_ch1 = 2;
    if (!spr_table1570[i]) {
      io_sound_ch3 = 40;
      if (spr_table1534[i] == 2)
        DespawnNonBossSprites();
    }
    spr_table151c[i] = 4;
    spr_decrementing_table1540[i] = spr_table1570[i] ? 31 : 80;
    return 1;
  }
  if (id == 0xA9) {  // Reznor (Spr0A9_Reznor): the hit from below; his own code knocks him off when $1564 reaches 12
    if (spr_table151c[i] || spr_decrementing_table154c[i] || spr_decrementing_table1564[i])
      return 0;
    spr_decrementing_table154c[i] = 8;
    spr_property_bits1662[i] = 41;
    spr_decrementing_table1564[i] = 15;
    io_sound_ch1 = 1;
    return 1;
  }
  if (id == 0xC5) {  // Big Boo boss (Spr0C5_BigBooBoss_0381E4): a thrown block, only while he is visible
    if (spr_table00c2[i] != 2)
      return 0;
    spr_table00c2[i] = 3;
    spr_decrementing_table1540[i] = 64;
    io_sound_ch3 = 40;
    return 1;
  }
  if (id == 0xA0) {  // Bowser (Spr0A0_ActivateBowserBattle_03B0F3): a Mechakoopa on his head
    if (!BowserCanBeHit(i))
      return 0;
    // Two punches for one Mechakoopa: the first only shakes him (the kick sound and SMW's contact star where the fist
    // lands), the second is the hit. Mechakoopas thrown at him count as in SMW.
    g_bowser_half = !g_bowser_half;
    if (g_bowser_half) {
      uint8 x = spr_xpos_lo[i], y = spr_ypos_lo[i];
      spr_xpos_lo[i] = (uint8)(g_punch_cx - 8), spr_ypos_lo[i] = (uint8)(g_punch_cy - 8);
      SpawnContactEffectFromSide(i);
      spr_xpos_lo[i] = x, spr_ypos_lo[i] = y;
      return 1;
    }
    bowserbattle_var14b5 = 76;
    bowserbattle_var14b3 = 0;
    bowserbattle_var14b4 = spr_table151c[i];
    io_sound_ch3 = 40;
    if (spr_table151c[i] == 9 && spr_table187b[i] == 1)
      DespawnNonBossSprites();
    return 1;
  }
  return -1;
}

// Enemies a stomp leaves lying stunned for Mario to pick up (Goombas, Bob-ombs, Buzzy Beetles, Mechakoopas, Koopas,
// which leave their shell): a punch has that stomp's result (CheckPlayerToNormalSpriteColl's stunned branch) instead of
// knocking them out.
static bool StompStuns(uint8 i) {
  return spr_current_status[i] == 8 && (spr_property_bits1656[i] & 0x10) && !(spr_property_bits1656[i] & 0x20) &&
         !(spr_property_bits1686[i] & 0x40) && !(spr_property_bits1662[i] & 0x80) && !spr_table187b[i];
}

static void Stun(uint8 i) {
  SpawnContactEffectFromSide(i);
  GivePoints(i, 0);
  spr_table1626[i] = 0;
  CheckPlayerToNormalSpriteColl_SetStunnedTimer(i);
  spr_yspeed[i] = (uint8)-24;  // knocked over, away from the fist
  spr_xspeed[i] = g_mc.xscale > 0 ? 8 : (uint8)-8;
}

// A punch on a stunned enemy kicks it as SMW Mario's touch does (CheckPlayerToNormalSpriteColl_01AA42), in the punch's
// direction. Keys and P-switches are not kicked.
static bool KickStunned(uint8 i) {
  uint8 id = spr_spriteid[i];
  if (spr_current_status[i] != 9 || id == 0x80 || id == 0x3E)
    return false;
  if (id != 0x0D && id != 0x2D && id != 0xA2) {
    if (id == 0x0F)
      spr_yspeed[i] = (uint8)-16;
    else
      GivePoints(i, 0);
  }
  CheckNormalSpriteToNormalSpriteColl_PlayKickSfx();
  spr_table00c2[i] = spr_decrementing_table1540[i];
  spr_current_status[i] = 10;
  spr_decrementing_table154c[i] = 16;
  spr_xspeed[i] = g_mc.xscale > 0 ? 0x2E : 0xD2;  // kSprStatus0B_Carried_ShellXSpeed
  return true;
}

static bool PunchSprite(uint8 i, int level) {
  if (McKoopa_IsActive(i))  // a Mario Combat Koopa takes MC's own damage and knockback for each hit level
    return McKoopa_Punch(i, level);
  if (McBowser_IsActive(i))  // so does Mario Combat's Bowser (King Koopas cheat)
    return McBowser_Punch(i, level);
  int boss = PunchBoss(i);
  if (boss >= 0) {
    g_boss_punches += boss == 1;
    return boss == 1;
  }
  if (KickStunned(i))
    return true;
  // An enemy in a bubble: any touch pops the bubble (Spr09D_BubbleWithSprite: $1534 to 6, the enemy comes out), so a
  // punch does too.
  if (spr_spriteid[i] == 0x9D && spr_current_status[i] == 8) {
    if (spr_table1534[i] < 7)
      return false;  // already popping
    spr_table1534[i] = 6;
    SpawnContactEffectFromSide(i);
    return true;
  }
  if (Cheats_PunchAll() && spr_spriteid[i] == 0x70 && spr_current_status[i] == 8)
    return PunchPokey(i);
  if (Cheats_SuperPunch())
    level = IntMax(level, kStrongHitLevel);
  else if (Cheats_McHits() && spr_current_status[i] == 8) {
    // MC hit reactions: Mario Combat's damage and knockback; the hit that empties the life is SMW's knock-out below
    int kind = McHitsKind(i), r = kind ? McHits_Punch(i, level, kind) : kMcHitsNo;
    if (r == kMcHitsAbsorbed)
      return true;
    if (r == kMcHitsFinal)
      level = IntMax(level, kStrongHitLevel);
  }
  // Punch anything cheat: an enemy only a star defeats is knocked out by a punch as the star knocks it out (the
  // Wiggler too, instead of only angering it).
  if (Cheats_PunchAll() && spr_current_status[i] == 8 && PunchAllTarget(spr_spriteid[i])) {
    KnockOut(i, 1);
    io_sound_ch1 = 3;
    return true;
  }
  if (level < kStrongHitLevel || spr_spriteid[i] == 0x86) {  // nothing but a star defeats a Wiggler
    int r = StompLikeHit(i);
    if (r >= 0)
      return r == 1;
  }
  if (StompStuns(i)) {
    Stun(i);
    return true;
  }
  if (spr_spriteid[i] == 0x83 || spr_spriteid[i] == 0x84) {  // flying ? blocks: hit like a head bump from below
    if (spr_table00c2[i])
      return false;  // already used
    SolidSpriteBlock_Entry2(i);
    return true;
  }
  if (IsChuck(spr_spriteid[i])) {  // strong hit or third hit: out
    KnockOut(i, 3);
    io_sound_ch1 = 3;
    return true;
  }
  if ((spr_property_bits167a[i] & 2) == 0 && (spr_property_bits166e[i] & 0x20) == 0) {
    CheckPlayerAttackToNormalSpriteColl_029404(i, 1);
    return true;
  }
  if (spr_property_bits1656[i] & 0x10) {  // "can be jumped on"
    spr_decrementing_table154c[i] = 8;
    SpawnContactEffectFromSide(i);
    GivePoints(i, 0);
    spr_current_status[i] = 2;  // knocked out, falls off screen
    spr_yspeed[i] = -48;
    spr_xspeed[i] = g_mc.xscale > 0 ? 16 : -16;
    return true;
  }
  if (Cheats_SuperPunch() && SuperPunchTarget(spr_spriteid[i])) {
    KnockOut(i, 1);
    io_sound_ch1 = 3;
    return true;
  }
  return false;  // invincible (bosses, Thwomps, ...)
}

// A punch/kick hits the blocks its box covers once, through SMW's own block code: turn blocks shatter as under a big
// Mario's spin jump; ?-, coin, power-up, note and other "hittable" blocks react as the cape (or a head bump) makes
// them, so their items come out the normal way. One block per hit, the one nearest Mario (like a head bump).
static int g_block_cooldown;  // frames until the next block hit (one per move; combo hits are further apart)

static void PunchBlocks(double x0, double y0, double x1, double y1, int dir) {
  if (g_block_cooldown)
    return;
  int cols = ((int)floor(x1) >> 4) - ((int)floor(x0) >> 4) + 1;
  for (int ty = (int)floor(y0) & ~15; ty <= (int)floor(y1); ty += 16) {
    for (int c = 0; c < cols; c++) {
      int tx = dir > 0 ? ((int)floor(x0) & ~15) + c * 16 : ((int)floor(x1) & ~15) - c * 16;
      int t = ActsLike(Map16At(tx, ty));  // (as the tile acts: P-switch coins / blocks swap)
      if ((t >> 8) != 1)
        continue;
      uint8 lo = (uint8)t;
      // Block buster cheat: grey (cement, 130) and used (132) blocks shatter like bricks, except the floor Mario
      // stands on (a low sweep would dig him a hole).
      if (Cheats_BlockBuster() && (lo == 0x30 || lo == 0x32) && ty < (int)floor(FeetY()) - 2) {
        blocks_xpos = (uint16)tx;
        blocks_ypos = (uint16)ty;
        misc_current_layer_being_processed = 0;
        blocks_map16_to_generate = 2;  // empty tile
        GenerateTile();
        SpawnBrickPieces(0);
        g_block_cooldown = 10;
        return;
      }
      // CheckIfBlockWasHit's hittable blocks: 0x111-0x12D (?, coin, power-up, turn, note, on/off ... blocks) and the
      // single yellow / green ! blocks with an item, 0x16A / 0x16B (their own path, in the tilesets that have them).
      if ((lo < 0x11 || lo >= 0x11 + 0x1D) && lo != 0x6A && lo != 0x6B)
        continue;
      blocks_xpos = (uint16)tx;
      blocks_ypos = (uint16)ty;
      misc_current_layer_being_processed = 0;
      blocks_currently_processed_map16_tile_lo = lo;
      if (lo == 0x1E && player_current_power_up)
        CheckIfBlockWasHit_Entry3(0x21, 3);  // the spin-jump break (0x00F160 path for tile 0x11E)
      else
        CheckIfBlockWasHit(lo, 0);
      g_block_cooldown = 10;
      return;
    }
  }
}

static void AttackSprites(void) {
  McHitBox hb[6];
  int n = McHero_GetHitBoxes(&g_mc, hb, 6);
  double fx = FeetX(), fy = FeetY();
  double ox = fx - g_mc.x / SCALE, oy = fy - g_mc.y / SCALE;
  int dir = g_mc.xscale > 0 ? 1 : -1;
  if (g_block_cooldown)
    g_block_cooldown--;
  for (int i = 0; i < 12; i++)
    if (g_spr_hit_cd[i]) g_spr_hit_cd[i]--;
  if (g_chain.timer < kChainFrames)
    g_chain.timer++;
  if ((player_blocked_flags & 4) && g_mc.onGround)
    g_chain.landed = true;
  // Fire punch cheat: a punch / kick (its first hit boxes) also throws SMW's fireball, at most one per 20 frames; SMW
  // has two fireball slots, so a third waits for one to go.
  static int fire_cd;
  if (fire_cd)
    fire_cd--;
  if (n > 0 && !fire_cd && !g_mc.hurt && Cheats_FirePunch()) {
    SpawnPlayerFireball();
    fire_cd = 20;
  }
  // New boxes from this frame's attack frame (feet-relative, padded, extended forward)
  for (int b = 0; b < n; b++) {
    double x0 = hb[b].x0 / SCALE + ox - fx - kHitPad, x1 = hb[b].x1 / SCALE + ox - fx + kHitPad;
    double y0 = hb[b].y0 / SCALE + oy - fy - kHitPad, y1 = hb[b].y1 / SCALE + oy - fy + kHitPad;
    if (dir > 0) x1 += kHitReach; else x0 -= kHitReach;
    int slot = 0;
    for (int k = 0; k < kMaxLinger; k++)
      if (g_linger[k].ttl < g_linger[slot].ttl) slot = k;
    g_linger[slot].ttl = kLingerFrames;
    g_linger[slot].level = hb[b].level;
    g_linger[slot].x0 = x0; g_linger[slot].x1 = x1; g_linger[slot].y0 = y0; g_linger[slot].y1 = y1;
    if (!g_mc.hurt)  // blocks: the MC box itself (padded, no extra reach), once per hit
      PunchBlocks(fx + x0 + kHitPad - 2 + (dir < 0 ? kHitReach : 0), fy + y0 + kHitPad - 2,
                  fx + x1 - kHitPad + 2 - (dir > 0 ? kHitReach : 0), fy + y1 - kHitPad + 2, dir);
  }
  // A hurt or knocked-back hero does not keep hitting with old boxes.
  if (g_mc.hurt || McHero_HeroFrame(&g_mc) != 18) {
    for (int k = 0; k < kMaxLinger; k++) g_linger[k].ttl = 0;
    return;
  }
  for (int k = 0; k < kMaxLinger; k++) {
    if (g_linger[k].ttl <= 0)
      continue;
    g_linger[k].ttl--;
    int x0 = (int)floor(fx + g_linger[k].x0), y0 = (int)floor(fy + g_linger[k].y0);
    int w = (int)ceil(g_linger[k].x1 - g_linger[k].x0), hgt = (int)ceil(g_linger[k].y1 - g_linger[k].y0);
    for (uint8 i = 11; (i & 0x80) == 0; --i) {
      if (spr_current_status[i] == 11 || spr_current_status[i] < 8)
        continue;
      // ($154C is not checked: sprites set it every frame they touch Mario, which would make an enemy pressed against
      // an invincible / no-damage Mario unpunchable. g_spr_hit_cd keeps it to one hit per punch.)
      // (Morton / Roy / Ludwig set $1FE2 whenever they touch Mario: McSmw_PunchMortonRoyLudwig)
      bool kid_mrl = spr_spriteid[i] == 0x29 && spr_table00c2[i] <= 2;
      if ((kid_mrl ? 0 : spr_decrementing_table1fe2[i]) | spr_table15d0[i] | g_spr_hit_cd[i])
        continue;
      // A Koopa just out of its shell can't be touched for a moment ($1564 from SprStatus09_Stunned): the punch that
      // stunned the shell does not also hit it.
      if (spr_spriteid[i] <= 3 && spr_decrementing_table1564[i])
        continue;
      // Behind / in front of a net or a layer: only where SMW's own contact check compares them
      // (CheckPlayerToNormalSpriteColl_01A80F: not with $0D9B bit 6, the mode-7 boss rooms, where Morton, Roy and
      // Ludwig sit on the other layer and every punch missed them).
      if ((misc_nmito_use_flag & 0x40) == 0 && player_current_layer_priority != spr_table1632[i])
        continue;
      spr_current_slotid = i;
      if (!PunchTouches(i, x0, y0, w, hgt))
        continue;
      g_punch_cx = x0 + w / 2, g_punch_cy = y0 + hgt / 2;
      // (a boss gives its own points and is not a step of the chain)
      bool boss = IsBoss(i) && !McBowser_IsActive(i);
      int chain = ChainLevel(i);
      g_points_override = boss ? -1 : IntMin(chain, 8);  // points of this step of the chain (8 = 1-up)
      bool hit = PunchSprite(i, g_linger[k].level);
      g_points_override = -1;
      if (hit) {
        // One hit per punch even though its box lingers (kLingerFrames). With instant hits the moves come every few
        // frames, so the wait is just longer than the linger: every move of the combo lands.
        g_spr_hit_cd[i] = Cheats_InstantHits() ? kLingerFrames + 1 : 10;
        // A flying ? block is not an enemy. A Mario Combat Koopa takes ~10 life of hits (Bowser 100): only knocking
        // it out counts as a step of the chain (each of its hits would otherwise run a chain up to 1-ups on one enemy).
        // (Likewise an enemy the MC hit reactions cheat leaves standing.)
        bool mc_koopa_hit = (McKoopa_IsActive(i) && !McKoopa_IsDead(i)) || McBowser_IsActive(i) || McHits_IsKnocked(i);
        bool enemy = spr_spriteid[i] != 0x83 && spr_spriteid[i] != 0x84;
        if (enemy && !mc_koopa_hit && !boss)
          ChainCommit(i, chain);
        // Life steal cheat: every hit that lands on an enemy gives MC Mario half a point of life back.
        if (enemy && Cheats_LifeSteal() && g_config.mc_life_bar && g_life > 0)
          g_life = fmin(10, g_life + 0.5);
        McHero_ShowHitEffect(&g_mc, g_linger[k].level);  // the spark clip's first frame carries the impact sound
        PlaySounds();
        // RUMBLE setting: a short buzz for each hit that lands, stronger for the strong hit levels (4-6)
        if (g_linger[k].level >= 4)
          Main_Rumble(0.45f, 0.8f, 120);
        else
          Main_Rumble(0.15f, 0.5f, 70);
      }
    }
  }
}

// Stop at solid blocks instead of moving into them: SMW pushes the player out of a wall only 1 px per frame, so a
// faster sideways move sinks Mario into blocks and SMW then takes him for crushed. Body edges as SMW's big Mario.
// Pressing against a wall tile: SMW's side contact (HandlePlayerLevelCollision) runs the tile's side code, which hurts
// on Munchers and castle spikes and hits blocks that react from the side. The clamp below stops MC Mario before SMW's
// own side check sees the tile, so it is run here for the tile he is pressing against.
static void TouchWallTile(int x, int top, int mid, int low, uint8 side /* 1 right, 2 left */) {
  if (player_blocked_flags & side)
    return;  // SMW's own side check already saw this wall last frame
  int ys[3] = { low, mid, top };
  for (int i = 0; i < 3; i++) {
    for (int l = 0; l < 2; l++) {
      if (!SolidAtLayer(l, x, ys[i]))
        continue;
      blocks_xpos = (uint16)((x + LayerOX(l)) & ~15);  // in that layer's tile map, as SMW's own side check
      blocks_ypos = (uint16)((ys[i] + LayerOY(l)) & ~15);
      misc_current_layer_being_processed = l;
      // (the tile as it acts, as SMW's own side check passes it: a coin a blue P-switch made a block is 0x02B but acts
      // as the used block 0x132; its raw number read as a hittable block turned it into a P-switch block, then a
      // yellow block for good)
      RunPlayerBlockCode_CheckIfPlayerTouchingHurtBlock((uint8)ActsLike(Map16AtLayer(l, x, ys[i])), side, true);
      return;
    }
  }
}

// The left edge (x0) of the first solid tile (any layer) at column x and one of the heights ys, -1 if none.
static int WallTileLeft(int x, const int *ys, int n) {
  for (int l = 0; l < 2; l++)
    for (int i = 0; i < n; i++)
      if (SolidAtLayer(l, x, ys[i]))
        return ((x + LayerOX(l)) & ~15) - LayerOX(l);
  return -1;
}

// Auto-scrolling levels and fixed-camera rooms (no horizontal scroll setting): SMW keeps the player between screen x 8
// and 232 by setting his speed to the scroll speed there (HandlePlayerLevelColl_00E9C8), and crushes him past screen
// x 240. MC sets the speed every frame, so the move itself stops at those edges (they are no walls: see Resolve).
// Scrolling levels: SMW pushes the player back once he is less than 8 px into the screen, but kills him if his
// on-screen x is ever past an edge (0xF0-0xFF). SMW's own speeds never get there in one frame; a big knockback (the
// Knockback cheat: up to 8 px a frame) can at the start of a level, so the move stops 1 px inside the screen.
static int ClampIntoScreen(int ix) {
  if (flag_side_exits)  // walking off the screen's side is the level's exit
    return ix;
  double px = (int16)player_xpos + player_sub_xpos / 256.0, l1x = (int16)misc_layer1_pos[0].x;
  if (flag_layer1_horiz_scroll_level_setting) {
    if (ix < 0 && px + ix / 16.0 < l1x + 1)
      return IntMin(0, (int)ceil((l1x + 1 - px) * 16));
    if (ix > 0 && px + ix / 16.0 > l1x + 239)
      return IntMax(0, (int)floor((l1x + 239 - px) * 16));
    return ix;
  }
  if (ix > 0 && px + ix / 16.0 > l1x + 232)
    return IntMax(0, (int)floor((l1x + 232 - px) * 16));
  if (ix < 0 && px + ix / 16.0 < l1x + 8)
    return IntMin(0, (int)ceil((l1x + 8 - px) * 16));
  return ix;
}

static int ClampIntoWalls(int ix) {
  ix = ClampIntoScreen(ix);
  if (!ix)
    return 0;
  double fx = FeetX(), nfx = fx + ix / 16.0;
  int top = (int16)player_ypos + 8, mid = (int16)player_ypos + 18, low = (int16)player_ypos + 28;
  int ys[3] = { top, mid, low };
  if (ix > 0) {
    int edge = (int)floor(nfx + 6), x0 = WallTileLeft(edge, ys, 3);
    if (x0 >= 0) {
      // 1 px into the tile, where SMW Mario rests against a wall: SMW's side check (and with it side pipes, throw
      // blocks, the blocked flags) only sees a wall the body touches. SMW pushes that pixel back out each frame.
      double stop = x0 - 5.01;  // tile's left edge
      g_blocked_accum |= 1;
      TouchWallTile(edge, top, mid, low, 1);
      return IntMax(0, (int)floor((stop - fx) * 16));
    }
  } else {
    int edge = (int)floor(nfx - 6), x0 = WallTileLeft(edge, ys, 3);
    if (x0 >= 0) {
      double stop = x0 + 16 + 5.01;  // tile's right edge (1 px in, as above)
      g_blocked_accum |= 2;
      TouchWallTile(edge, top, mid, low, 2);
      return IntMin(0, (int)ceil((stop - fx) * 16));
    }
  }
  return ix;
}

// Land on floors instead of passing them: MC has no terminal fall speed, and SMW only catches the player on a
// "solid on top" tile (ground, ledges, clouds) while the feet are in the top few pixels of it. A long fall moves
// more than that per frame, so stop the frame's fall just inside the first floor top it would cross.
static int ClampIntoFloor(int iy) {
  if (iy <= 0)
    return iy;
  double fy = FeetY(), nfy = fy + iy / 16.0;
  int fx = (int)floor(FeetX()), res = iy;
  for (int l = 0; l < 2; l++) {  // the nearest floor top of either layer (each on its own tile grid)
    if (!LayerActive(l))
      continue;
    int oy = LayerOY(l);
    for (int top = (((int)floor(fy - 2) + oy) & ~15) + 16 - oy; top + 2 <= (int)floor(nfy); top += 16) {
      bool hit = false;
      for (int dx = -5; dx <= 5 && !hit; dx += 5) {
        int bx = fx + dx;
        hit = bx >= 0 && FloorTopAtLayer(l, bx, top) && !FloorTopAtLayer(l, bx, top - 1);
      }
      if (hit) {
        res = IntMin(res, IntMax(0, (int)floor((top + 2 - fy) * 16)));
        break;
      }
    }
  }
  // Solid sprites' tops (Reznor's platforms, the line-guided and floating platforms, flying ? blocks...): SolidSpriteBlock
  // stands the player on one only while his feet are inside its box and less than 8 px below its y, a band of about 5 px
  // that a fast fall skips (and he is then pushed out sideways). Stop the fall inside that band.
  int px = (int16)player_xpos;
  for (int k = 0; k < 13; k++) {
    if (!g_solid_box[k].on || (k < 12 && spr_current_status[k] != 8) ||
        (uint8)(counter_global_frames - g_solid_box[k].frame) > 1 || px + 2 >= g_solid_box[k].x1 || px + 14 <= g_solid_box[k].x0)
      continue;
    int land = IntMax(g_solid_box[k].sy, g_solid_box[k].y0) + 4;
    if (land < g_solid_box[k].sy + 8 && fy <= land && nfy > land)
      res = IntMin(res, IntMax(0, (int)floor((land - fy) * 16)));
  }
  return res;
}

// Title screen: under SMW's logo a "+" and Mario Combat's own logo (its main menu's fiery lettering, exported by
// tools/scripts/mc_export_logo.py), so it reads SUPER MARIO WORLD + Mario Combat. Through the overlay at the HUD's z,
// so it fades in and out with the screen's brightness. Only while the logo is up alone (game modes 04-07: the fade-in
// and the demo); the file menu (08 on) takes that place.
enum { kTitlePlusSize = 13, kTitlePlusY = 103, kTitleLogoY = 117 };

static void DrawTitleLogo(void) {
  // The "+": arms 5 px thick, a black outline and a grey shade on its lower-right edges, as SMW's logo letters have.
  static uint32 px[kTitlePlusSize * kTitlePlusSize];
  static const McImage plus = { kTitlePlusSize, kTitlePlusSize, 1.0f, 0, 0, px };
  if (!px[kTitlePlusSize * 6 + 6]) {
    enum { N = kTitlePlusSize, A = 4, B = 8 };  // the arms span A..B across
    for (int y = 0; y < N; y++)
      for (int x = 0; x < N; x++) {
        bool in = (x >= A && x <= B) || (y >= A && y <= B);
        bool inner = (x >= A + 1 && x <= B - 1 && y >= 1 && y <= N - 2) || (y >= A + 1 && y <= B - 1 && x >= 1 && x <= N - 2);
        bool shade = inner && ((x == B - 1 && (y <= A || y >= B)) || (y == B - 1 && (x <= A || x >= B)) ||
                               (x == N - 2 && y > A && y < B) || (y == N - 2 && x > A && x < B));
        px[y * N + x] = !in ? 0 : !inner ? 0xff000000 : shade ? 0xffa8a8a8 : 0xfff8f8f8;
      }
  }
  float m[6] = { 1, 0, 0, 1, (float)((256 - kTitlePlusSize) / 2), (float)kTitlePlusY };
  McOverlay_DrawImage(&plus, m, NULL, 0xffff);
  float l[6] = { 1, 0, 0, 1, (float)((256 - kMcTitleLogo.w) / 2), (float)kTitleLogoY };
  McOverlay_DrawImage(&kMcTitleLogo, l, NULL, 0xffff);
}

void McSmw_FrameBegin(void) {
  g_ticked_this_frame = false;
  McKoopa_FrameBegin();
  McHits_FrameBegin();
  McBowser_FrameBegin();
  if (misc_game_mode != 0x14) {
    // Title, overworld, fades: no hero. Coming from the map/title, the next level starts as MC Mario.
    if (misc_game_mode <= 0x0E)
      g_from_map = true;
    g_need_init = true;
    g_toggle_request = false;
    McOverlay_BeginFrame();
    if (misc_game_mode >= 0x04 && misc_game_mode <= 0x07)
      DrawTitleLogo();
    return;
  }
  if (g_from_map) {
    g_from_map = false;
    g_active = true;
    g_need_init = true;
    g_level_fresh = true;
  }
  // Character switch, applied in the normal player state so no SMW sequence is cut in half.
  if (g_toggle_request && g_config.mc_enable && player_current_state == 0 && !flag_sprites_locked &&
      !misc_display_message) {
    g_toggle_request = false;
    g_active = !g_active;
    if (g_active) {
      InitHeroEx(false);  // keep the life bar
    } else {
      McOverlay_BeginFrame();
      player_current_pose = 0;
      player_xspeed = player_yspeed = 0;
    }
  }
  if (!McSmw_Enabled()) {
    McOverlay_BeginFrame();
    McLevel_Draw();  // (the Mario Combat level's lift and arrow: SMW Mario's frames have no MC hero drawing them)
    return;
  }
  // Message boxes freeze the level (PlayerDraw stops); drop the life bar from the kept overlay meanwhile.
  if (!g_need_init && misc_display_message && g_hud_shown)
    Render(g_last_prio, g_last_visible);
}

// Double-jump / ground-pound cheats for MC Mario, through the hero script's own variables before its tick: a second
// jump is the script's take-off (grav = jump; jumpAnimThres = 5; shortHop = 5, so releasing early still cuts it
// short). The pound: a wind-up of kPoundWindupTicks hanging in the air (grav held at 0, as the somersault plays, see
// DrawPoundPose), then the drop straight down (xSpeed 0, grav held at -20) until he lands.
static uint8 g_prev_keys;
enum { kPoundWindupTicks = 13 };  // about 16 SMW frames, as SMW Mario's wind-up
static int g_pound_ticks;          // ticks since the pound began

// Air dash cheat: L / R (latched each frame in McSmw_PlayerPhysics, as a frame may have no tick) dashes left / right
// once per stay in the air: kDashTicks straight across at kDashSpeed MC px a tick (6.7 SMW px a frame, about the
// distance of SMW Mario's dash) with grav held at 0, ending early at a wall; then he falls on at his running speed.
enum { kDashTicks = 10 };
static const double kDashSpeed = 24;
static int g_dash_ticks, g_dash_dir, g_dash_request;
static bool g_dash_used;

static void AirMoves(uint8 keys) {
  uint8 pressed = keys & ~g_prev_keys;
  g_prev_keys = keys;
  if (g_mc.onGround || g_mc.hurt || g_mc.life <= 0 || g_mc.walling || timer_end_level) {
    g_air_jump_used = false;
    g_dash_used = false;
    g_dash_ticks = g_dash_request = 0;
    if (!g_mc.onGround) g_pounding = false;  // (a pound that reaches the ground lands in TickOnce)
    return;
  }
  if (g_pounding) {
    g_mc.xSpeed = 0;
    if (++g_pound_ticks <= kPoundWindupTicks) {
      g_mc.grav = 0.5;  // (the script's grav -= 0.5 makes it 0: he hangs in the air)
      if (g_pound_ticks == kPoundWindupTicks)
        io_sound_ch3 = 4;  // the spin sound as he turns into the drop
    } else if (g_mc.grav > -20) {
      g_mc.grav = -20;
    }
    return;
  }
  if (Cheats_GroundPound() && (pressed & MC_KEY_DOWN) && !g_mc.fighting) {
    g_pounding = true;
    g_pound_ticks = 0;
    g_dash_ticks = 0;
    g_mc.xSpeed = 0;
    g_mc.grav = 0.5;
    return;
  }
  int dash = g_dash_request;
  g_dash_request = 0;
  if (dash && !g_dash_used && !g_mc.fighting) {
    g_dash_used = true;
    g_dash_ticks = kDashTicks;
    g_dash_dir = dash;
    Cheats_AirDashEffect();
  }
  // Double jump: once per jump. Infinite jumps: every press in the air.
  bool again = Cheats_InfiniteJumps() || (Cheats_DoubleJump() && !g_air_jump_used);
  if (again && (pressed & MC_KEY_JUMP) && !g_mc.fighting) {
    g_air_jump_used = true;
    g_dash_ticks = 0;  // a jump ends a dash
    g_mc.grav = g_mc.jump;
    g_mc.jumpAnimThres = 5;
    g_mc.shortHop = 5;
    Cheats_DoubleJumpEffect(false);
  }
  if (g_dash_ticks > 0) {
    if (g_mc.fighting || (g_dash_dir > 0 ? g_mc.onRightWall : g_mc.onLeftWall)) {
      g_dash_ticks = 0;  // an attack or a wall ends it
    } else {
      g_mc.xscale = g_dash_dir * 100;
      g_mc.xSpeed = g_dash_dir * kDashSpeed;
      g_mc.grav = 0.5;  // (the script's grav -= 0.5 makes it 0)
      if (--g_dash_ticks == 0)
        g_mc.xSpeed = g_dash_dir * g_mc.maxMoveSpeed;
    }
  }
}

// The attack clip's moves (DefineSprite 432 labels attack1 ... attack12; the clip has 410 frames) and, probed once
// from the clip itself with a scratch hero, which of its frames carry hit boxes (and the strongest hit level there).
static const int kMoveStart[] = { 1, 27, 49, 76, 100, 133, 178, 206, 220, 248, 263, 278, 293, 308, 340, 375, 411 };
enum { kNumMoves = sizeof(kMoveStart) / sizeof(kMoveStart[0]) - 1, kAttackFrames = 410 };
static uint8 g_hit_level[kAttackFrames + 2];
static bool g_hits_probed;

static void ProbeAttackFrames(void) {
  if (g_hits_probed)
    return;
  g_hits_probed = true;
  static McHero tmp;
  for (int f = 1; f <= kAttackFrames; f++) {
    McHero_Init(&tmp, &g_world, 0, 0);
    McHero_GotoLabel(&tmp, "attack", false);
    McHero_AttackGotoFrame(&tmp, f);
    McHitBox hb[6];
    int n = McHero_GetHitBoxes(&tmp, hb, 6);
    for (int i = 0; i < n; i++)
      if (hb[i].level > g_hit_level[f]) g_hit_level[f] = (uint8)hb[i].level;
  }
}

static int MoveOf(int frame) {
  for (int m = 0; m < kNumMoves; m++)
    if (frame >= kMoveStart[m] && frame < kMoveStart[m + 1]) return m;
  return -1;
}

static int FirstHitFrame(int m) {
  ProbeAttackFrames();
  for (int f = kMoveStart[m]; f < kMoveStart[m + 1]; f++)
    if (g_hit_level[f]) return f;
  return 0;
}

// Instant hits cheat. Every move's hit box comes out on its first frame: right after a move starts (the core's own
// combo logic or start of an attack), the attack clip is stepped through its wind-up in the same tick, frame scripts
// and all (the lunges and sounds still happen), up to its first hit-box frame. And a new punch press once the move has
// hit jumps the clip to the move's next combo window, where the core's combo logic (DefineSprite_432's onEnterFrame,
// windows copied here) starts the next move on that same press: combos chain as fast as the button is pressed.
static void InstantHitsStartup(void) {
  if (McHero_HeroFrame(&g_mc) != 18)
    return;
  int af = McHero_AttackFrame(&g_mc), m = MoveOf(af), hit = m >= 0 ? FirstHitFrame(m) : 0;
  for (int guard = 0; hit && af < hit && guard < 32; guard++) {
    if (!McHero_AttackStep(&g_mc) || McHero_HeroFrame(&g_mc) != 18)
      return;
    af = McHero_AttackFrame(&g_mc);
    if (MoveOf(af) != m)
      return;
  }
}

static bool InWindow(int f, bool dir) {  // the combo windows of DefineSprite_432's onEnterFrame (`AF() > lo && AF() < hi`)
  static const int16 kPunch[][2] = { { 9, 23 }, { 79, 88 }, { 85, 94 }, { 35, 44 }, { 106, 132 }, { 185, 191 },
                                     { 187, 203 }, { 240, 247 }, { 209, 219 } };          // Punch alone
  static const int16 kDir[][2] = { { 185, 191 }, { 187, 203 }, { 77, 87 }, { 209, 219 } };  // Punch + Left / Right
  static const int16 kAny[][2] = { { 234, 244 }, { 249, 259 }, { 264, 274 }, { 279, 288 }, { 294, 304 }, { 7, 12 },
                                   { 30, 38 } };                                            // Punch either way
  for (size_t i = 0; i < sizeof(kAny) / sizeof(kAny[0]); i++)
    if (f > kAny[i][0] && f < kAny[i][1]) return true;
  if (dir) {
    for (size_t i = 0; i < sizeof(kDir) / sizeof(kDir[0]); i++)
      if (f > kDir[i][0] && f < kDir[i][1]) return true;
  } else {
    for (size_t i = 0; i < sizeof(kPunch) / sizeof(kPunch[0]); i++)
      if (f > kPunch[i][0] && f < kPunch[i][1]) return true;
  }
  return false;
}

static void InstantHitsCombo(uint8 keys) {
  if (McHero_HeroFrame(&g_mc) != 18 || !(keys & MC_KEY_PUNCH) || g_mc.pressingAttack || g_mc.hurt)
    return;
  int af = McHero_AttackFrame(&g_mc), m = MoveOf(af), hit = m >= 0 ? FirstHitFrame(m) : 0;
  if (!hit || af < hit)
    return;  // (the move hasn't hit yet: its wind-up is skipped anyway)
  bool dir = (keys & (MC_KEY_LEFT | MC_KEY_RIGHT)) != 0;
  int w = 0;
  for (int f = af + 1; f < kMoveStart[m + 1] - 1; f++)
    if (InWindow(f, dir)) { w = f; break; }
  // The tick about to run advances the clip one frame and then runs the combo check: stop one frame before the window.
  while (w && McHero_AttackFrame(&g_mc) < w - 1 && McHero_HeroFrame(&g_mc) == 18)
    if (!McHero_AttackStep(&g_mc)) break;
  // A move with no combo left for these keys (a finisher, the low sweep, the air kick): its recovery is cut, the
  // clip runs on to its end (its end script sends the hero back to idle / the jump pose), and the tick about to run
  // starts a new attack on this press as the core always does from idle.
  for (int guard = 0; !w && guard < 64 && McHero_HeroFrame(&g_mc) == 18; guard++)
    if (!McHero_AttackStep(&g_mc)) break;
}

static void TickOnce(bool puppet) {
  uint8 keys;
  if (puppet) {
    // SMW moves the player; MC only animates from what SMW does.
    int8 xs = (int8)player_xspeed;
    keys = xs > 4 ? MC_KEY_RIGHT : xs < -4 ? MC_KEY_LEFT : 0;
  } else {
    keys = Keys();
    AirMoves(keys);
  }
  g_mc.x = FeetX() * SCALE;
  g_mc.y = FeetY() * SCALE;
  double xs0 = g_mc.xSpeed;
  bool was_on_ground = g_mc.onGround;
  // (SMW's ice levels and the Slippery levels cheat, which sets SMW's ice flag)
  bool ice = !puppet && (flag_ice_level || Cheats_Slippery()) && misc_game_mode == 0x14 && g_mc.onGround &&
             !g_mc.fighting && !g_mc.hurt;
  bool instant = !puppet && Cheats_InstantHits();
  if (instant)
    InstantHitsCombo(keys);
  g_air_attack_press = !puppet && (keys & MC_KEY_PUNCH) && !g_mc.pressingAttack && !g_mc.onGround && !g_mc.hurt;
  McHero_Tick(&g_mc, keys);
  g_air_attack_press = false;
  if (instant) {
    InstantHitsStartup();
    // Each move's lunge adds to his speed; chained this fast they would stack and send him sliding far past the enemy.
    // While attacking, his speed stays within his top speed (maxMoveSpeed), as one lunge from a standstill does.
    if (g_mc.fighting && fabs(g_mc.xSpeed) > g_mc.maxMoveSpeed)
      g_mc.xSpeed = g_mc.xSpeed > 0 ? g_mc.maxMoveSpeed : -g_mc.maxMoveSpeed;
  }
  // Ice: on the ground MC Mario's speed changes at a fifth of its own rate (slow starts, long slides), as SMW's ice
  // physics do for SMW Mario. Not against a wall (the script stopped him there).
  if (ice && g_mc.onGround && !g_mc.onLeftWall && !g_mc.onRightWall) {
    g_mc.xSpeed = xs0 + (g_mc.xSpeed - xs0) * 0.2;
    // The script's ground friction takes 1 off the speed and then moves him by what is left: a speed under 1 (which
    // only this slower change leaves) would come out on the other side of 0 and move him backwards, every tick, while
    // he stands still. With no direction held, a slide that slow stops.
    if (!(keys & (MC_KEY_LEFT | MC_KEY_RIGHT)) && fabs(g_mc.xSpeed) < 1)
      g_mc.xSpeed = 0;
  }
  // Low-gravity cheat: in the air MC Mario's gravity (grav -= 0.5 each tick) is halved. Not in a wall slide or a ground
  // pound (they set grav themselves).
  if (!puppet && Cheats_LowGravity() && !g_mc.onGround && !g_mc.walling && !g_pounding && !g_dash_ticks)
    g_mc.grav += 0.25;
  g_blocked_accum = 0;
  PlaySounds();
  if (!puppet) {
    g_vx = g_mc.tick_dx;
    g_vy = g_mc.tick_dy;
    if (g_mc.grav == g_mc.jump && g_mc.shortHop == 4) {  // jumped this tick (grav = jump; shortHop = 5; shortHop--)
      player_in_air_flag = 11;
      if (was_on_ground)
        g_ground_jumps++;  // (Jumping enemies cheat)
      // SMW's levels are built for SMW's running jumps: SMW Mario takes off faster the faster he runs (take-off speed
      // 80 standing up to 97 at full run, HandlePlayerPhysics' jump table). MC's jump is one height; it gets the same
      // ratio for his run speed, so 5-tile cliffs a running SMW Mario clears are clearable. A standing jump is MC's own.
      g_mc.grav *= -(int8)HandlePlayerPhysics_JumpSpeedForRun(false) / 80.0;
    }
    if (g_pounding && g_mc.onGround) {
      g_pounding = false;
      Cheats_GroundPoundLanding();
    }
  }
}

// MC ticks last 1.2 SMW frames. Each tick's displacement is spread over the frames it overlaps in time, so the
// SMW position follows the MC position-over-time exactly (no zero-order-hold error, no extra latency).
// Runs the MC clock; called once per level frame in any player state.
static void RunClock(bool puppet) {
  g_ticked_this_frame = true;
  g_blocked_accum |= player_blocked_flags | g_sprite_push;
  g_sprite_push = 0;
  double f0 = g_frame_time, f1 = f0 + 1;
  while (g_next_tick_time < f1) {  // compute every tick that starts before this frame ends
    TickOnce(puppet);
    if (g_ntq < kTickQ) {
      g_tq[g_ntq].start = g_next_tick_time;
      g_tq[g_ntq].dx = puppet ? 0 : g_vx;
      g_tq[g_ntq].dy = puppet ? 0 : g_vy;
      g_ntq++;
    }
    g_next_tick_time += 1.2;
  }
  g_fdx = g_fdy = 0;
  for (int i = 0; i < g_ntq; i++) {
    double a = fmax(f0, g_tq[i].start), b = fmin(f1, g_tq[i].start + 1.2);
    if (b > a) {
      g_fdx += g_tq[i].dx * (b - a) / 1.2;
      g_fdy += g_tq[i].dy * (b - a) / 1.2;
    }
  }
  int k = 0;
  for (int i = 0; i < g_ntq; i++)
    if (g_tq[i].start + 1.2 > f1)
      g_tq[k++] = g_tq[i];
  g_ntq = k;
  g_frame_time = f1;
  if (g_frame_time > 6000) {  // keep the doubles small: shift the time origin every 100 s
    g_frame_time -= 6000, g_next_tick_time -= 6000;
    for (int i = 0; i < g_ntq; i++)
      g_tq[i].start -= 6000;
  }
  g_phase = (g_phase + 1) % 6;
  if (puppet)  // SMW moves (and turns) the player
    g_mc.xscale = player_facing_direction ? 100 : -100;
  else
    player_facing_direction = g_mc.xscale > 0;
}

void McSmw_AfterPlayerState(void) {
  if (!McSmw_Enabled() || misc_game_mode != 0x14 || g_ticked_this_frame)
    return;
  if (g_need_init)
    InitHero();
  g_puppet = true;  // swimming/climbing (PlayerPhysics is not called) or a non-normal player state
  g_was_puppet = player_current_state == 0;
  RunClock(true);
}

bool McSmw_PlayerPhysics(void) {
  if (!McSmw_Enabled() || misc_game_mode != 0x14)
    return false;
  if (g_need_init)
    InitHero();
  // Cape take-off: a jump at full P-speed with the cape is SMW's own (in-air mode 12, then flight), so SMW's physics
  // run that jump until Mario is back on the ground.
  if (!g_smw_air && player_current_power_up == 2 && player_pmeter >= kPMeterFull && (player_blocked_flags & 4) &&
      !player_in_air_flag && ((io_controller_press1 | io_controller_press2) & 0x80) && !g_mc.hurt && !g_mc.fighting) {
    g_smw_air = true;
    g_smw_air_frames = 0;
  }
  if (g_smw_air && ++g_smw_air_frames > 4 && (player_blocked_flags & 4) && !player_in_air_flag &&
      !player_cape_flying_phase)
    g_smw_air = false;
  g_puppet = player_riding_yoshi_flag || player_swimming_flag || player_climbing_flag || player_cape_flying_phase ||
             player_wall_walk_status || timer_end_level || g_smw_air;
  if (g_puppet) {
    g_last_iy = 0;
    g_side_launch = g_held_rise = false;
    g_was_puppet = true;
    RunClock(true);
    return false;  // SMW's own physics for these states
  }
  // Launched by SMW since the last frame: sprite code gave the player an upward speed after MC's (a stomp bounce,
  // springboard, note block, wall springboard, Wiggler, Lakitu's cloud...). MC's motion would overwrite it on this
  // frame and drop Mario back onto the enemy, so it becomes MC's jump speed, as when leaving an SMW-driven state.
  // Converted to reach the height SMW's launch reaches, not by speed alone (MC's gravity is weaker): SMW rises
  // v^2 / (2a) / 16 px under gravity a = 3 (jump held) or 6, MC rises g0^2 / SCALE SMW px -> g0 = v * sqrt(3 / 32a).
  bool jump_held = ((io_controller_hold1 | io_controller_hold2) & 0x80) != 0;
  if (!g_was_puppet && (int8)player_yspeed < 0 && (int8)player_yspeed < g_last_iy - 4) {
    g_mc.grav = -(int8)player_yspeed * (jump_held ? 0.177 : 0.125);
    g_held_rise = jump_held;
    for (int i = 0; i < g_ntq; i++)
      g_tq[i].dy = 0;
    g_ry = 0;
    g_mc.shortHop = 0;
  }
  // A rise SMW gave with jump held (above, or leaving an SMW-driven state below): letting go doubles SMW's gravity,
  // which halves the height still to come; in MC's terms the speed times 1/sqrt(2). Once, while still rising.
  if (g_held_rise && (g_mc.grav <= 0 || g_mc.onGround)) {
    g_held_rise = false;
  } else if (g_held_rise && !jump_held) {
    g_mc.grav *= 0.7071;
    g_held_rise = false;
  }
  // Knocked sideways by SMW since the last frame (Reznor's rebound off his side, a Koopa Kid's or a Chargin' Chuck's
  // after a stomp...): MC's motion would overwrite that speed and walk Mario on into the sprite, so it becomes MC's
  // own speed (SMW 1/16 px per frame -> MC px per tick, as when leaving an SMW-driven state).
  if (g_side_launch && !g_was_puppet) {
    g_mc.xSpeed = (int8)player_xspeed * (SCALE * 1.2 / 16);
    for (int i = 0; i < g_ntq; i++)
      g_tq[i].dx = 0;
    g_rx = 0;
  }
  g_side_launch = false;
  if (g_was_puppet) {
    // Leaving an SMW-driven state (Yoshi dismount, jumping off a vine or net, out of the water...): keep the speed
    // SMW gave the player, or MC would stop him dead (and a dismounted Mario would land straight back on Yoshi).
    // SMW 1/16 px per frame -> MC px per tick: * 3 MC px per SMW px * 1.2 frames per tick / 16.
    // An upward speed is converted to reach the height SMW's would (as for a launch above): MC's gravity is weaker, and
    // the same speed sent him almost three times as high off a rope, vine or net.
    g_was_puppet = false;
    g_mc.xSpeed = (int8)player_xspeed * (SCALE * 1.2 / 16);
    if ((int8)player_yspeed < 0) {
      g_mc.grav = -(int8)player_yspeed * (jump_held ? 0.177 : 0.125);
      g_held_rise = jump_held;
    }
    g_ntq = 0;
    g_rx = g_ry = 0;
    g_run_mult = 1;
  }
  int dash = Cheats_AirDashPressed();  // air dash cheat: the press waits for the next tick (AirMoves)
  if (dash)
    g_dash_request = dash;
  // Moon-jump cheat: jump held in the air keeps MC Mario rising.
  if (Cheats_MoonJump() && !g_mc.onGround && ((io_controller_hold1 | io_controller_hold2) & 0x80) && g_mc.grav < 6)
    g_mc.grav = 6;
  RunClock(false);
  UpdateRun((player_blocked_flags & 4) && g_mc.onGround);
  // Speed for this frame: this frame's share of MC displacement (MC px) -> SMW 1/16 px per frame. Running (Y/X held)
  // scales MC's horizontal motion up to SMW-like run and P-speed.
  double sx = g_fdx * 16.0 / SCALE * g_run_mult + g_rx;
  double sy = g_fdy * 16.0 / SCALE + g_ry;
  int ix = (int)lround(sx), iy = (int)lround(sy);
  g_rx = sx - ix;
  g_ry = sy - iy;
  ix = IntMax(-127, IntMin(127, ix));
  iy = IntMax(-127, IntMin(127, iy));
  // Landed: drop the rest of the fall still queued from earlier ticks (it would push Mario on into the ground).
  if (player_blocked_flags & 4) {
    for (int i = 0; i < g_ntq; i++)
      if (g_tq[i].dy > 0) g_tq[i].dy = 0;
    if (g_ry > 0) g_ry = 0;
  }
  // Keep SMW's slope-stick speed while standing (the collision code sets it when on a slope).
  if ((player_blocked_flags & 4) && g_mc.grav <= 0 && iy < (int8)player_yspeed)
    iy = (int8)player_yspeed;
  // Head bump: SMW just hit the block above (and triggered it). Cancel the rest of the upward motion right away,
  // like MC's `while(hitTest(x, y-80)) { grav = -2; y++; }`, so the block is hit once per jump, not every frame.
  // Solid sprites (message boxes, ...) report the bump by setting the rising player's y speed to 16 instead.
  bool sprite_bump = g_last_iy < 0 && (int8)player_yspeed >= 0;
  if ((player_blocked_flags & 8) || sprite_bump) {
    for (int i = 0; i < g_ntq; i++)
      if (g_tq[i].dy < 0) g_tq[i].dy = 0;
    if (g_mc.grav > -2) g_mc.grav = -2;
    g_mc.shortHop = 0;
    g_jump_lock = true;
    g_ry = 0;
    if (iy < 0) iy = 0;
  }
  iy = ClampIntoFloor(iy);
  ix = ClampIntoWalls(ix);
  if ((ix == 0 && g_rx != 0 && (g_blocked_accum & 3)))
    g_rx = 0;
  player_xspeed = (uint8)(int8)ix;
  player_sub_xspeed = 0;
  player_yspeed = (uint8)(int8)iy;
  g_last_iy = iy;
  // SMW's air physics (replaced here) keep the "in air" flag set while airborne. The tilting-platform boss rooms
  // pin a grounded player to the platform, so a jump that leaves it clear would drag Mario along (and wrap him
  // off-screen above the room).
  if (!player_in_air_flag && !(player_blocked_flags & 4) && !misc_player_on_solid_sprite) {
    if (iy < 0)
      player_in_air_flag = 11;
    else if ((iy > 0 || g_dash_ticks) && !g_mc.onGround)
      player_in_air_flag = 36;  // (an air dash moves level: SMW's side pipes only take a standing player)
  } else if (!player_in_air_flag && iy < 0 && g_mc.grav > 2) {
    player_in_air_flag = 11;  // taking off this frame
  }
  player_ducking_flag = 0;
  player_spin_jump_flag = 0;
  // Fireballs / cape spin stay on X (SMW's run button); Y is the punch.
  CheckPowerUpSpecificPlayerAttacks();
  AttackSprites();
  UpdateCapePose((player_blocked_flags & 4) && g_mc.onGround);
  // The camera follows MC Mario up walls (from a cling until he lands), as MC's follows its hero. SMW scrolls up in the
  // air only for a spring, a vine, cape flight and the like: its own wall jump raises this same flag (lm.c).
  static bool s_wall_camera;
  if (g_mc.walling && !camera_bounce_off_spring_flag)
    camera_bounce_off_spring_flag = 1, s_wall_camera = true;
  else if (s_wall_camera && g_mc.onGround)
    camera_bounce_off_spring_flag = 0, s_wall_camera = false;
  else if (s_wall_camera && !camera_bounce_off_spring_flag)
    camera_bounce_off_spring_flag = 1;  // (kept through the kick: SMW's landing code may clear it in between)
  return true;
}

enum { kYoshiSeatDy = -4 };  // SMW px: MC jump pose's feet sit this much above SMW's riding feet line

// Power-up colours. SMW recolours Mario by swapping his 10-colour palette (normal / fire / Luigi / the star cycle,
// `pointer_player_palette`). Each MC pixel is matched to the nearest colour of SMW's normal Mario palette and shifted
// by the difference to the same slot of the current palette, so MC's shading survives (normal Mario stays MC-exact).
static uint16 g_pal_cur[10];
static bool g_pal_identity = true;
static struct { uint32 key, val; } g_rc_cache[4096];

static uint32 Bgr555ToRgb(uint16 c) {
  uint32 r = c & 31, g = c >> 5 & 31, b = c >> 10 & 31;
  return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

static void PreparePalette(void) {
  const uint16 *cur = GetPlayerPalette();
  if (memcmp(cur, g_pal_cur, sizeof(g_pal_cur))) {
    memcpy(g_pal_cur, cur, sizeof(g_pal_cur));
    memset(g_rc_cache, 0, sizeof(g_rc_cache));
  }
  g_pal_identity = memcmp(cur, kPlayerPalettes, sizeof(g_pal_cur)) == 0;
}

static uint32 RecolorPixel(uint32 argb) {
  uint32 rgb = argb & 0xffffff, key = rgb | 0x1000000;
  uint32 h = (rgb * 2654435761u) >> 20;
  for (int probe = 0; probe < 8; probe++) {
    uint32 i = (h + probe) & 4095;
    if (g_rc_cache[i].key == key)
      return (argb & 0xff000000) | g_rc_cache[i].val;
    if (!g_rc_cache[i].key) {
      int pr = rgb >> 16 & 255, pg = rgb >> 8 & 255, pb = rgb & 255;
      int best = -1, bd = 1 << 30;
      for (int k = 0; k < 10; k++) {
        uint32 c = Bgr555ToRgb(kPlayerPalettes[k]);
        int dr = pr - (int)(c >> 16 & 255), dg = pg - (int)(c >> 8 & 255), db = pb - (int)(c & 255);
        int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
        if (d < bd) bd = d, best = k;
      }
      uint32 out = rgb;
      if (pr <= 0x30 && pg <= 0x30 && pb <= 0x30) {
        // MC's outline, moustache, eyes and darkest shading: SMW Mario's black in every palette.
        out = 0;
      } else if (best >= 0 && bd < 9 * 48 * 48) {  // effects far from Mario's colours keep theirs
        // The slot's colour in the current palette, scaled by how much brighter/darker this pixel is than the slot's
        // normal colour: MC's shading survives and keeps the target hue. (An additive RGB delta turned dark reds
        // teal on the grey slot of the fire palettes.)
        uint32 c0 = Bgr555ToRgb(kPlayerPalettes[best]), c1 = Bgr555ToRgb(g_pal_cur[best]);
        double l0 = 0.299 * (c0 >> 16 & 255) + 0.587 * (c0 >> 8 & 255) + 0.114 * (c0 & 255);
        double lp = 0.299 * pr + 0.587 * pg + 0.114 * pb;
        double s = lp / (l0 < 8 ? 8 : l0);
        int r = (int)((c1 >> 16 & 255) * s + 0.5), g = (int)((c1 >> 8 & 255) * s + 0.5), b = (int)((c1 & 255) * s + 0.5);
        r = r > 255 ? 255 : r; g = g > 255 ? 255 : g; b = b > 255 ? 255 : b;
        out = (uint32)r << 16 | (uint32)g << 8 | (uint32)b;
      }
      g_rc_cache[i].key = key;
      g_rc_cache[i].val = out;
      return (argb & 0xff000000) | out;
    }
  }
  return argb;
}

// The ground pound in Mario Combat's own art: the curled-up tumble of his hurt animation (shape 437, a frame of the
// hurt clip), turned about its middle. During the wind-up it somersaults once (forward, the way he faces); in the
// drop it stays curled with the seat of his trousers down (kPoundDropAngle). One art pixel per screen pixel, as all
// of Mario Combat Mario's art; the middle of the ball sits half his height above his feet.
enum { kPoundShape = 437 };
static const double kPoundDropAngle = 135;  // degrees clockwise, facing right: tucked, seat down, head up

static void DrawPoundPose(double fx, double fy, uint16 z) {
  if (kPoundShape > kMcMaxCharId || kMcChars[kPoundShape].type != 1)
    return;  // (not a shape in this export)
  const McImage *im = &kMcImages[kMcChars[kPoundShape].index];
  int f = g_mc.xscale > 0 ? 1 : -1;
  double t = (double)g_pound_ticks / kPoundWindupTicks;
  double deg = t < 1 ? 360.0 * t + kPoundDropAngle * t : kPoundDropAngle;  // ends the somersault on the drop angle
  double a = deg * 3.14159265358979 / 180 * f, c = cos(a), s = sin(a);
  double cx = im->w / 2.0, cy = im->h / 2.0, ax = fx, ay = fy - 14;
  float m[6];
  m[0] = (float)(c * f), m[1] = (float)(s * f), m[2] = (float)-s, m[3] = (float)c;
  m[4] = (float)(ax - (m[0] * cx + m[2] * cy));
  m[5] = (float)(ay - (m[1] * cx + m[3] * cy));
  McOverlay_DrawImage(im, m, NULL, z);
}

static void Render(int obj_priority, bool visible) {
  g_last_prio = obj_priority;
  g_last_visible = visible;
  McOverlay_BeginFrame();
  g_hud_shown = g_config.mc_life_bar && !misc_display_message;  // keep message boxes readable
  if (g_hud_shown)
    DrawHud();
  if (!visible)
    return;
  // Riding Yoshi: MC has no sitting frame. Its jump pose (knees drawn up) is the closest, raised onto the saddle.
  double seat_dy = 0;
  if (player_riding_yoshi_flag && !g_mc.hurt) {
    if (McHero_HeroFrame(&g_mc) != 9)
      McHero_GotoLabel(&g_mc, "jump", false);
    seat_dy = kYoshiSeatDy;
  }
  McDrawItem items[kMcMaxDraw];
  int n = McHero_GetDrawList(&g_mc, items, kMcMaxDraw);
  double fx = floor(FeetX()) - (int16)mirror_current_layer1_xpos;
  double fy = floor(FeetY()) - (int16)mirror_current_layer1_ypos + (int8)player_relative_yposition_during_screen_shake +
              seat_dy;
  uint16 z = McOverlay_SpriteZ(obj_priority);
  PreparePalette();
  McOverlay_SetRecolor(g_pal_identity ? NULL : RecolorPixel);
  if (g_pounding) {  // the ground pound cheat: Mario Combat's curled tumble, somersaulting, then dropping
    DrawPoundPose(fx, fy, z);
    n = 0;
  }
  for (int i = 0; i < n; i++) {
    float m[6];
    m[0] = items[i].m[0] / SCALE; m[1] = items[i].m[1] / SCALE;
    m[2] = items[i].m[2] / SCALE; m[3] = items[i].m[3] / SCALE;
    m[4] = (float)((items[i].m[4] - g_mc.x) / SCALE + fx);
    m[5] = (float)((items[i].m[5] - g_mc.y) / SCALE + fy);
    McOverlay_DrawImage(&kMcImages[items[i].image], m, items[i].cx, z);
  }
  McOverlay_SetRecolor(NULL);
  McLevel_Draw();  // the Mario Combat level's lift and arrow
  if (g_config.mc_debug_hitboxes) {  // the effective (padded, lingering) attack boxes
    for (int k = 0; k < kMaxLinger; k++)
      if (g_linger[k].ttl > 0)
        McOverlay_DrawRect((int)(fx + g_linger[k].x0), (int)(fy + g_linger[k].y0), (int)(fx + g_linger[k].x1),
                           (int)(fy + g_linger[k].y1), 0xffff2020, McOverlay_SpriteZ(3));
  }
}

void McSmw_Draw(int obj_priority, bool visible) {
  if (g_need_init)
    InitHero();
  if (g_config.mc_life_bar) {
    if (player_current_power_up > g_last_power)  // power-ups heal fully in life-bar mode (also McSmw_OnPowerUpCollected)
      g_life = 10;
    g_last_power = player_current_power_up;
  }
  Render(obj_priority, visible);
}

void McSmw_OnPlayerHurt(void) {
  if (!McSmw_Enabled() || g_need_init || Cheats_NoKnockback())
    return;
  // MC enemies: hero.xSpeed = +-6 away from the enemy, gotoAndStop("hurt"), life -= 0.5 (life is SMW's power-up here)
  McHero_TakeHit(&g_mc, player_facing_direction ? -1 : 1, false, 0);
  ScaleKnockback(false);
}

void McSmw_OnPlayerDeath(void) {
  if (!McSmw_Enabled() || g_need_init || g_dead)
    return;
  g_dead = true;
  g_mc.life = 0;  // next tick: activated = false, gotoAndStop("hurt"), hurt_mc die1/die2
}

// Debug (pilot): print the level positions of map16 tiles page 1, low byte lo..hi (e.g. 0x30 = cement block).
void McSmw_DebugFindTiles(int lo, int hi) {
  // lo/hi: map16 numbers; values below 0x100 mean page 1 (0x1xx), as before. Horizontal and vertical levels.
  // (0x8000 flag: page-0 tiles as given, e.g. coins 0x2B, from the pilot's findtiles0)
  bool page0 = (lo & 0x8000) != 0;
  lo &= 0x7FFF, hi &= 0x7FFF;
  if (lo < 0x100 && !page0) lo |= 0x100;
  if (hi < 0x100 && !page0) hi |= 0x100;
  bool vertical = misc_level_layout_flags & 1;
  int w = vertical ? 512 : misc_screens_in_lvl * 256, h = vertical ? misc_screens_in_lvl * 256 : 0x1b0;
  int last_x = -100, last_y = -100;
  for (int x = 0; x < w; x += 16)
    for (int y = 0; y < h; y += 16) {
      int t = Map16At(x, y);
      bool m = t >= lo && t <= hi;
      if (m && !(x == last_x && y == last_y + 16)) {
        int run = 1;  // matching tiles straight below (a wall's height)
        while (y + run * 16 < h) {
          int t2 = Map16At(x, y + run * 16);
          if (t2 < lo || t2 > hi) break;
          run++;
        }
        printf("tile %03X at x=%d y=%d tileset %d run %d\n", t, x, y, misc_level_tileset_setting, run);
      }
      if (m) last_x = x, last_y = y;
    }
  fflush(stdout);
}

int McSmw_DebugHeroFrame(void) { return McHero_HeroFrame(&g_mc); }

bool McSmw_CapeVisible(void) { return !g_dead && !g_need_init; }

bool McSmw_UseSmwSprite(void) {
  // Cape flight (dive, pull-up, glide) too: SMW tilts Mario's whole body with the cape there, which an upright MC pose
  // can't match. The take-off rise and ordinary jumps keep MC Mario with SMW's cape (PlayerDraw_CapeOnly).
  // The P-balloon (Mario blown up round), running up a wall from a triangle block (Mario turned sideways) and the
  // cape spin (Mario turning on the spot) are SMW-only poses as well.
  return McSmw_Enabled() && misc_game_mode == 0x14 && !g_need_init &&
         (player_riding_yoshi_flag || player_climbing_flag || player_cape_flying_phase || timer_inflate_from_pballoon ||
          player_wall_walk_status || (timer_active_cape_spin && player_current_power_up == 2));
}

void McSmw_OnPowerUpCollected(void) {
  if (McSmw_Enabled() && g_config.mc_life_bar && !g_dead)
    g_life = 10;
}

void McSmw_CapeOffset(int *dx, int *dy) {
  // SMW's cape tables hang the cape from SMW Mario's shoulders; MC Mario's art stands on the same feet and is about
  // as tall, so only a small shift toward his back is needed (more while he leans into a punch).
  int back = player_facing_direction ? -1 : 1;
  *dx = back * g_cape_shift_x;
  *dy = g_cape_shift_y;
}

// SMW pose for the cape layout (MC Mario has his own body art): standing, walking/running frames, or the jump pose,
// plus the cape flap animation from SetPlayerPose (UpdatePlayerCapeImage), driven by MC's speeds.
static void UpdateCapePose(bool grounded) {
  if (player_current_power_up != 2)
    return;
  int frame = McHero_HeroFrame(&g_mc);
  g_cape_shift_x = frame == 18 ? 2 : 0;  // attacks lean forward
  g_cape_shift_y = 0;
  if (timer_active_cape_spin) {
    // Cape spin (X, set by CheckPowerUpSpecificPlayerAttacks): SMW's own spin branch of the pose code turns Mario,
    // swings the cape and places the cape's hit box (InitializeCapeSwingOrNetPunch). PlayerDraw shows SMW's spinning
    // sprite for these frames (McSmw_UseSmwSprite); MC's facing comes back next frame.
    SetPlayerPose_00CF0A();
    return;
  }
  if (!grounded) {
    player_current_pose = 0x0B;
  } else if (player_xspeed) {
    player_current_pose = (g_phase / 2) % 3;  // SMW's big-Mario walk poses 0-2
  } else {
    player_current_pose = 0;
  }
  UpdatePlayerCapeImage();
}

void McSmw_DebugAttackFrames(void) {  // each move of the attack clip: its frames and the frames with hit boxes
  ProbeAttackFrames();
  for (int m = 0; m < kNumMoves; m++) {
    printf("move %d frames %d-%d hits:", m, kMoveStart[m], kMoveStart[m + 1] - 1);
    for (int f = kMoveStart[m]; f < kMoveStart[m + 1]; f++)
      if (g_hit_level[f]) printf(" %d(L%d)", f, g_hit_level[f]);
    printf("\n");
  }
  fflush(stdout);
}

void McSmw_DebugTileAt(int x, int y) {
  printf("map16 at %d,%d = %03X", x, y, Map16At(x, y));
  if (misc_level_layout_flags & 0x80)  // layer 2 takes part in collision: its tile there too, and the offset
    printf("  layer2 %03X (offset %d,%d, layout %02X)", Map16AtLayer(1, x, y), LayerOX(1), LayerOY(1),
           misc_level_layout_flags);
  printf("\n");
  fflush(stdout);
}

void McSmw_OnTeleported(void) {  // the player was moved (pit rescue cheat): MC Mario starts again from there
  if (McSmw_Enabled())
    g_need_init = true;
}

void McSmw_DebugSetPos(int x, int y) {
  player_xpos = (uint16)x;
  player_ypos = (uint16)y;
  player_xspeed = player_yspeed = 0;
  g_need_init = true;
}

void McSmw_DebugState(char *buf, int size) {
  // first attack hit box in level px, and the nearest live normal sprite (slot, x, y)
  McHitBox hb[6];
  int nb = McHero_GetHitBoxes(&g_mc, hb, 6);
  double ox = FeetX() - g_mc.x / SCALE, oy = FeetY() - g_mc.y / SCALE;
  int best = -1, bd = 1 << 30;
  for (int i = 0; i < 12; i++) {
    if (spr_current_status[i] < 8) continue;
    int d = abs((int16)GetSprXPos(i) - (int16)player_xpos) + abs((int16)GetSprYPos(i) - (int16)player_ypos);
    if (d < bd) bd = d, best = i;
  }
  snprintf(buf, size, "%.2f,%.2f,%g,%g,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.1f,%d,%.0f,%.0f,%.0f,%.0f,%d,%d,%d,%d,%d",
           g_mc.x, g_mc.y, g_mc.xSpeed, g_mc.grav, McHero_HeroFrame(&g_mc), g_mc.xscale, McHero_AttackFrame(&g_mc),
           McHero_HurtFrame(&g_mc), g_mc.onGround, g_mc.walling, g_puppet, g_mc.keys, g_mc.shortHop, g_phase, g_vx,
           g_vy, g_life, nb, nb ? hb[0].x0 / SCALE + ox : 0, nb ? hb[0].y0 / SCALE + oy : 0,
           nb ? hb[0].x1 / SCALE + ox : 0, nb ? hb[0].y1 / SCALE + oy : 0, best,
           best >= 0 ? (int16)GetSprXPos(best) : 0, best >= 0 ? (int16)GetSprYPos(best) : 0,
           best >= 0 ? spr_current_status[best] : 0, nb ? hb[0].level : 0);
}
