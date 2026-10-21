// Mario Combat hero emulation: a small AVM1 timeline player plus a line-by-line port of the hero's
// ActionScript (DefineSprite 440 onEnterFrame, attack clip 432 onEnterFrame, and their frame scripts).
// Units are Mario Combat stage pixels and 50 Hz ticks, exactly as in the original game.
#pragma once
#include <stdbool.h>
#include "../types.h"

enum {
  MC_KEY_LEFT = 1, MC_KEY_RIGHT = 2, MC_KEY_JUMP = 4, MC_KEY_DOWN = 8, MC_KEY_PUNCH = 16,
};

enum { kMcMaxObjs = 64, kMcMaxChildren = 8, kMcMaxQueue = 64, kMcMaxDraw = 32, kMcMaxSounds = 8 };

typedef struct McObj {
  uint8 used, is_sprite, visible, playing, initialized;
  uint16 ch;           // character id
  int16 sprite;        // index into kMcSprites (sprites only)
  uint16 frame;        // current frame, 1-based (0 = not yet run)
  int16 name;          // index into kMcNames, -1 = none
  uint16 ratio;
  int16 place_op;      // index of the PlaceObject op that created it (goto survival rule)
  uint8 has_enter_frame;  // clip has an onEnterFrame handler (hero, attack clip)
  float m[6];
  int16 cx[8];
  int16 parent;
  uint8 nchild;
  uint16 child_depth[kMcMaxChildren];
  int16 child[kMcMaxChildren];
  uint32 serial;       // instantiation order (newest runs first, like Flash's AVM1 execution list)
} McObj;

typedef struct McHero McHero;

// Collision. In MC mode the core calls hit() for every probe exactly like the original
// `_root.ground.hitTest(x, y, true)`. In external mode the host (SMW) moves the player itself and
// resolve() reports what the probes/push-outs would have found.
typedef struct McWorld {
  bool (*hit)(void *ctx, double x, double y);
  void (*resolve)(void *ctx, McHero *h);  // external mode only; set h->onGround/onLeftWall/onRightWall and push-out effects
  void *ctx;
  bool external;
  bool use_scroll_bounds;  // MC level camera bounds (lScroll/rScroll); not used inside SMW
  double lscroll, rscroll;
  bool use_dead_zone;
  double dead_zone_y, spawn_x, spawn_y;
  // Hosts with their own wall collision (SMW) keep the hero against walls themselves: skip MC's airborne
  // "x -= 5 / x += 5" and walling "x -+ 1" pulls into the wall, which would push the player into solid blocks.
  bool no_wall_pull;
} McWorld;

typedef struct McDrawItem { int16 image; float m[6]; int16 cx[8]; } McDrawItem;  // m maps image px -> stage px
typedef struct McHitBox { uint8 level; double x0, y0, x1, y1; } McHitBox;           // hit1..hit6 global AABBs

struct McHero {
  // AS2 variables of the hero clip (names kept from the original script)
  double x, y, xSpeed, grav;
  double maxMoveSpeed, maxAirMoveSpeed, jump, life;
  int jumpAnimThres, stopSliding, shortHop;
  bool pressingAttack, activated, onGround, onLeftWall, onRightWall, skiding, hurt, walling, fighting;
  int xscale;  // 100 or -100
  uint8 keys;  // MC_KEY_* held this tick
  // timeline state
  McObj objs[kMcMaxObjs];
  int16 root;  // the hero clip (sprite 440)
  uint32 next_serial;
  struct { int16 obj; uint8 kind; uint16 frame; } queue[kMcMaxQueue];
  int nqueue;
  // per-tick outputs
  double tick_dx, tick_dy;   // total displacement applied to x/y this tick
  int sounds[kMcMaxSounds]; int nsounds;
  const McWorld *world;
  uint32 tick;
  // Test hook: called once per tick after the actions queued before the queue started have run and
  // before actions queued *during* the run (goto targets, new clips). That is where an older clip's
  // onEnterFrame (like the reference-capture driver) observes the state in the original.
  void (*on_log_point)(McHero *h);
  bool in_tick;
};

void McHero_Init(McHero *h, const McWorld *world, double x, double y);
void McHero_Tick(McHero *h, uint8 keys);
int McHero_GetDrawList(McHero *h, McDrawItem *out, int max);
int McHero_GetHitBoxes(McHero *h, McHitBox *out, int max);
void McHero_GetBound(McHero *h, double *x0, double *y0, double *x1, double *y1);
int McHero_HeroFrame(McHero *h);
int McHero_ChildFrame(McHero *h, int depth);
int McHero_AttackFrame(McHero *h);
int McHero_HurtFrame(McHero *h);
// Damage the way MC enemies do it (DefineSprite 163): knockback, hurt animation, life loss.
void McHero_TakeHit(McHero *h, int from_dir, bool heavy, double damage);
void McHero_ShowHitEffect(McHero *h, int level);
void McHero_GotoLabel(McHero *h, const char *label, bool play);
// Cheat / tool support (not in the original game): one extra playhead step of the attack clip (frame scripts run, the
// combo checks don't), and a goto on it.
bool McHero_AttackStep(McHero *h);
void McHero_AttackGotoFrame(McHero *h, int frame);
