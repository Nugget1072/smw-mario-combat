// "MC hit reactions" cheat (see mc_hits.h).
//
// The Mario Combat Koopa's hit reactions (mc_koopa.c TakeHit, from DefineSprite_163's script) applied to SMW's own
// enemies: each hit takes Mario Combat's damage for its level from a life of 10 and throws the enemy back (and up) by
// Mario Combat's knockback; it slides to a stop on the ground, bounces off walls, and after a launch it tumbles until
// it lands, bounces once and gets up. The hit timing is the Koopa's hurt clip (154): hurt1 frames 1-14, hurt3 16-31,
// hurt4 33-67, hurt5 69-82 (looping in the air), hurt6 83-106; the clip's end hands the enemy back to its own code.
//
// What the enemy does is SMW's: while it is knocked about, its routine runs with the sprites-locked flag (as during
// SMW's own freezes), so it only draws itself where this module has moved it; it neither thinks nor touches Mario.
// How it is moved depends on how it lives:
// - walkers fly under gravity through SMW's sprite collision, as the MC Koopa does;
// - fliers and swimmers are pushed without gravity (they slow down and carry on from where they stopped);
// - enemies tied to their place (a pipe, dirt, a net, lava, a line, a wall, a cloud, a platform) are only stunned:
//   moving them would take them out of the terrain their own code relies on.
// Sprites SMW lets pass through walls (Bullet Bills, ...) move without collision, as they always do.
// The hit that empties the life bar is SMW's own knock-out (PunchSprite continues as a strong hit).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mc_hits.h"
#include "mc_smw.h"
#include "../funcs.h"
#include "../smw_rtl.h"
#include "../variables.h"

static const double kToSmwSpeed = 16.0 / 3.0 * 50.0 / 60.0;  // MC px per tick -> SMW 1/16 px per frame
enum { kWalk = 1, kFly = 2, kAnchored = 3 };
enum { kHurt1 = 1, kHurt3 = 16, kHurt4 = 33, kHurt5 = 69, kHurt5End = 82, kHurt6 = 83, kMaxTicks = 300 };

typedef struct Hit {
  bool known;        // life is set for the enemy in this slot
  bool knocked;      // the hurt clip is playing
  uint8 id;
  uint8 kind;
  double life, grav, xSpeed, rem_x, rem_y;
  int frame, ticks;  // hurt clip frame; ticks since the hit
  bool onGround;
  uint8 own_xs, own_ys;  // the sprite's own speeds when the knockback began, given back when it ends
} Hit;

static Hit g_h[12];
static int g_tick_acc;
static bool g_tick_now;

void McHits_FrameBegin(void) {
  g_tick_acc += 50;
  g_tick_now = g_tick_acc >= 60;
  if (g_tick_now) g_tick_acc -= 60;
}

void McHits_OnSpriteInit(uint8 k) {
  if (k < 12) memset(&g_h[k], 0, sizeof(g_h[k]));
}

static Hit *Slot(uint8 k) {
  if (k >= 12) return NULL;
  Hit *h = &g_h[k];
  if (h->known && h->id != spr_spriteid[k])  // another sprite in the slot
    memset(h, 0, sizeof(*h));
  return h;
}

bool McHits_IsKnocked(uint8 k) {
  return k < 12 && g_h[k].knocked && g_h[k].id == spr_spriteid[k] && spr_current_status[k] == 8;
}

// The Koopa's six `if(this.hitTest(_root.hero.attack.hitN))` reactions (mc_koopa.c TakeHit).
static void TakeHit(Hit *h, int level, const McHeroInfo *hero) {
  int s = hero->xscale == 100 ? 1 : -1;
  double hx = hero->xspeed;
  switch (level) {
  case 1: h->life -= 1; h->frame = kHurt1; h->xSpeed = hx + s * 1; if (!h->onGround) h->grav -= 2; break;
  case 2: h->life -= 2; h->frame = kHurt1; h->grav = 3; h->xSpeed = hx + s * 1.5; break;
  case 3: h->life -= 3; h->frame = kHurt3; h->grav = 4; h->xSpeed = hx + s * 6; break;
  case 4: h->life -= 4; h->frame = kHurt4; h->grav = 4; h->xSpeed = hx + s * 15; break;
  case 5: h->life -= 1; h->frame = kHurt5; h->grav = 20; h->xSpeed = hx + s * (rand() % 7); break;
  default:
    if (!h->onGround) { h->grav = -20; h->life -= 3; } else { h->grav = 7; h->life -= 1; }
    h->frame = kHurt5;
    h->xSpeed = hx + s * (h->onGround ? 1 : 5);
    break;
  }
}

int McHits_Punch(uint8 k, int level, int kind) {
  Hit *h = Slot(k);
  if (!h || spr_current_status[k] != 8 || kind < kWalk || kind > kAnchored)
    return kMcHitsNo;
  if (!h->known) {
    memset(h, 0, sizeof(*h));
    h->known = true;
    h->id = spr_spriteid[k];
    h->life = 10;  // the MC Koopa's onClipEvent(load) life
  }
  if (!h->knocked) {
    h->kind = (uint8)kind;  // (it can change: a Monty Mole is tied to its dirt until it pops out)
    // A sprite SMW never collides with the level can't fall (nothing would stop it): moved as a flier.
    if (kind == kWalk && (spr_property_bits1686[k] & 0x80))
      h->kind = kFly;
    h->onGround = h->kind == kWalk && (spr_table1588[k] & 4);
    h->grav = h->xSpeed = h->rem_x = h->rem_y = 0;
    h->own_xs = spr_xspeed[k];
    h->own_ys = spr_yspeed[k];
  }
  McHeroInfo hero;
  McSmw_GetHeroInfo(&hero);
  TakeHit(h, level, &hero);
  // Fliers are knocked back and up, never down: one that passes through the level (a Super Koopa) would otherwise be
  // pushed into the ground and stay there.
  if (h->kind == kFly && h->grav < 0)
    h->grav = 0;
  if (h->life <= 0) {
    h->knocked = false;
    h->known = false;
    return kMcHitsFinal;
  }
  h->knocked = true;
  h->ticks = 0;
  spr_xspeed[k] = spr_yspeed[k] = 0;
  return kMcHitsAbsorbed;
}

static void Tick(uint8 k, Hit *h) {
  h->ticks++;
  if (h->kind == kWalk) {
    h->grav -= 0.5;
    if (h->xSpeed > 0) { h->xSpeed -= h->onGround ? 0.5 : 0.05; if (h->xSpeed < 0) h->xSpeed = 0; }
    else if (h->xSpeed < 0) { h->xSpeed += h->onGround ? 0.5 : 0.05; if (h->xSpeed > 0) h->xSpeed = 0; }
    if ((spr_table1588[k] & 3) && h->xSpeed) {  // knocked into a wall: bounce off it
      h->xSpeed = (h->xSpeed + (h->xSpeed > 0 ? 1 : -1)) * -1;
      h->frame = kHurt1;
    }
    if (h->grav < 0 && h->onGround && h->frame >= kHurt5 && h->frame <= kHurt5End) {  // the landing bounce
      h->grav = h->grav / 2 * -1;
      h->frame = kHurt6;
    }
  } else if (h->kind == kFly) {  // no gravity: the push dies away in the air
    h->grav = h->grav > 0 ? fmax(0, h->grav - 0.5) : fmin(0, h->grav + 0.5);
    h->xSpeed = h->xSpeed > 0 ? fmax(0, h->xSpeed - 0.5) : fmin(0, h->xSpeed + 0.5);
  }
  // the hurt clip: back to the enemy's own code at its end (a walker only once it is down on the ground)
  h->frame++;
  if (h->frame == kHurt5End) h->frame = kHurt5;  // hurt5 loops while the tumble lasts
  bool end = h->frame == 15 || h->frame == 32 || h->frame == 68 || h->frame == 107;
  // A walker gets up once it is down on the ground and its slide is spent (the MC Koopa's xSpeed carries it on after
  // the hurt clip: `x += walk + xSpeed`).
  if (end && h->kind == kWalk && (!h->onGround || h->xSpeed) && h->ticks < kMaxTicks)
    h->frame--, end = false;
  if (end || h->ticks >= kMaxTicks)
    h->knocked = false;
}

static void Move(uint8 k, Hit *h) {
  if (h->kind == kAnchored) return;
  double sxs = h->xSpeed * kToSmwSpeed + h->rem_x, sys = -h->grav * kToSmwSpeed + h->rem_y;
  int ix = (int)lround(sxs), iy = (int)lround(sys);
  ix = ix < -127 ? -127 : ix > 127 ? 127 : ix;
  iy = iy < -127 ? -127 : iy > 112 ? 112 : iy;
  h->rem_x = sxs - ix, h->rem_y = sys - iy;
  spr_xspeed[k] = (uint8)(int8)ix;
  spr_yspeed[k] = (uint8)(int8)iy;
  UpdateNormalSpritePosition_X(k);
  UpdateNormalSpritePosition_Y(k);
  if (spr_property_bits1686[k] & 0x80) {  // SMW never collides it with the level (Bullet Bills, ...)
    h->onGround = false;
    return;
  }
  HandleNormalSpriteLevelCollision(k);
  uint8 c = spr_table1588[k];
  h->onGround = (c & 4) != 0;
  if (h->onGround && h->grav < 0) h->grav = 0;   // while(hitTest(x, y)) { grav = 0; y--; }
  if ((c & 8) && h->grav > 0) h->grav = -4;      // while(hitTest(x, y - _height)) { y++; grav = -4; }
  if (c & 3) h->rem_x = 0;
  if (h->onGround) spr_yspeed[k] = 0;
}

bool McHits_Knocked(uint8 k) {
  if (!McHits_IsKnocked(k))
    return false;
  Hit *h = &g_h[k];
  if (!flag_sprites_locked) {
    if (g_tick_now)
      Tick(k, h);
    Move(k, h);
    if (!h->knocked) {  // back to its own code, with its own speeds as they were when the knockback began (a Super
      spr_xspeed[k] = h->own_xs;  // Koopa hit while taking off climbs on: its flight starts when that speed is spent)
      spr_yspeed[k] = h->own_ys;
    }
  }
  return true;  // (drawn this frame by its own routine, even on the frame the clip ends)
}

void McHits_AfterDraw(uint8 k) {
  if (!flag_sprites_locked && spr_current_status[k] == 8)
    SubOffscreen_Bank01_Entry1(k);  // knocked off the screen or into a pit: gone, as SMW's own despawn does
  if (spr_current_status[k] != 8)
    g_h[k].knocked = false;
}

int McHits_DebugInfo(uint8 k, char *buf, int n) {
  if (k >= 12 || !g_h[k].known || g_h[k].id != spr_spriteid[k]) return 0;
  const Hit *h = &g_h[k];
  return snprintf(buf, n, " mc-hits life=%.1f knocked=%d kind=%d frame=%d grav=%.1f xs=%.1f ground=%d", h->life,
                  h->knocked, h->kind, h->frame, h->grav, h->xSpeed, h->onGround);
}
