// "King Koopas" cheat (see mc_bowser.h).
//
// Bowser's onEnterFrame (DefineSprite_532/frame_1/DoAction.as) is ported in BowserEnterFrame in the original
// statement order, with the frame scripts of the clips on his timeline (hurt 527, jump 528, teleport 531) and of his
// own frame 21. The fire breath is the root `flame` template's enterFrame (frame_496/PlaceObject2_555_11) and the
// shockwave the `groundHitOrig` copies (frame_496/PlaceObject2_558_15). Units are MC stage px (3 per SNES px) and
// MC's 50 Hz ticks. Each tick follows the AVM1 order of the hero port (mc_core.c): the clip on his timeline queues
// its onEnterFrame and advances, then Bowser does, then the queued actions run in that order.
// What changes inside SMW:
// - The `_root.ground.hitTest` push-outs are SMW's sprite level collision, fed with the script's own motion.
// - `_root.hero` is MC Mario (his bound and attack boxes) or SMW Mario (his body box).
// - MC's stage edges: falling below the level (or into lava) is MC's `_y > 440` pit, and the teleport puts him back
//   in the middle of the screen, as MC's (320, 250) is the middle of its arena. A defeated Bowser falls through the
//   floor as in MC and is removed off-screen (MC fades to the next level instead).
// - SMW Mario's weapons hit him like the hero's attacks of the matching strength (a fireball is a light hit, a stomp
//   or the cape a medium one, a shell or a cape dive a strong one) instead of knocking a Koopa out at once.
// - Score: an SMW 1-up when he is defeated (he is a boss: 50 life, half of MC's 100 at the user's request), not MC's HUD score per hit.
// - `this._height` (the ceiling probe) is his body without the health bar above it.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mc_bowser.h"
#include "mc_level.h"
#include "mc_bowser_data.h"
#include "mc_overlay.h"
#include "mc_audio.h"
#include "mc_smw.h"
#include "../config.h"
#include "../funcs.h"
#include "../smw_rtl.h"
#include "../variables.h"
#include "../cheats.h"

// Bowser's timeline labels
enum { kFrameIdle = 1, kFrameBreath = 3, kFrameFire = 4, kFrameHurt = 22, kFrameJump = 23, kFramePant = 24,
       kFrameTeleport = 25 };
static const double kScale = 3.0;                            // MC stage px per SNES px
static const double kToSmwSpeed = 16.0 / 3.0 * 50.0 / 60.0;  // MC px per tick -> SMW 1/16 px per frame
static const double kWaveScale = 0.3614654541015625;         // groundHitOrig's placement scale (copies keep it)
static const double kPi = 3.141592653589793;
// Bowser's full life. MC's script loads 100; the user asked for half (round 8). The fire-rate steps MC ties to life
// 70 / 50 / 20 and the health bar's `bar._xscale = life` are scaled to this maximum, so the fight keeps MC's pacing.
static const double kMaxLife = 50;

typedef struct Bowser {
  bool active;
  uint32 seen;                  // frame counter of the last frame it was processed (detects new sprites)
  // AS2 variables (onClipEvent(load) of _root.bowser)
  double x, y, grav, xSpeed, life, moveSpeed;
  int attackSpeed, attackSpeedDefult, stopSliding, attack1, attack1Orig, jump, jumpOrig, jumpPower, jumpSpeed;
  int pant, pantOrig;
  bool onGround, dead, hurt;
  int xscale;
  // timeline: the root frame, and the clip placed on it (hurt_mc, jump, pant, teleport)
  int root;
  bool root_playing;
  int sub_frame;
  bool sub_playing, sub_enter;  // sub_enter: the jump clip's onEnterFrame (defined on its frame 35)
  uint32 sub_inst;              // instance number of that clip (a goto to another frame makes a new one)
  bool bar_hidden;              // the health bar's `_visible = false` once life < 0
  // port state
  bool scored, in_pit;
  double move_dx;               // this tick's `_X += xSpeed` steps
  double rem_x, rem_y;          // sub-pixel remainder of the frame speeds
} Bowser;

typedef struct Flame {  // a copy of the `flame` template
  bool on;
  double x, y, Angle;
  int twist, life, age, xscale;
  bool goLeft, goRight;
} Flame;

typedef struct Wave {   // a copy of `groundHitOrig`
  bool on;
  double x, y;
  int alpha, frame;
  bool effect;
} Wave;

static Bowser g_b[12];
static Flame g_flames[32];
static Wave g_waves[12];
static uint32 g_frame;  // sprite-processing frames (not counting pauses, when SMW does not run its sprites)
static int g_tick_acc;  // 50 Hz clock inside 60 Hz frames
static bool g_tick_now;
static int g_clip_bar = -1, g_clip_hurt, g_clip_jump, g_clip_tele, g_clip_flame, g_clip_wave;

static int ClipIndex(int id) {
  for (int i = 0; i < kMcBowserNumClips; i++)
    if (kMcBowserClips[i].id == id) return i;
  return 0;
}

static void Setup(void) {
  if (g_clip_bar >= 0) return;
  g_clip_bar = ClipIndex(510), g_clip_hurt = ClipIndex(527), g_clip_jump = ClipIndex(528);
  g_clip_tele = ClipIndex(531), g_clip_flame = ClipIndex(555), g_clip_wave = ClipIndex(558);
}

bool McBowser_Enabled(void) {
  if (McLevel_Active())  // the Mario Combat level: Bowser on his bridge, whatever the cheats
    return McLevel_Bowser();
  return g_config.cheat[kCheat_KingKoopas];
}

bool McBowser_IsActive(uint8 k) {
  return k < 12 && g_b[k].active && g_b[k].seen + 1 >= g_frame && spr_spriteid[k] <= 7 && spr_current_status[k];
}
bool McBowser_IsDead(uint8 k) { return McBowser_IsActive(k) && g_b[k].dead; }

void McBowser_SpritesBegin(void) { g_frame++; }
void McBowser_OnSpriteInit(uint8 k) { if (k < 12) g_b[k].active = false; }  // a new sprite in this slot

void McBowser_FrameBegin(void) {
  g_tick_acc += 50;
  g_tick_now = g_tick_acc >= 60;
  if (g_tick_now) g_tick_acc -= 60;
  if (misc_game_mode != 0x14) {  // left the level (or a room): his fire and shockwaves go with it
    memset(g_flames, 0, sizeof(g_flames));
    memset(g_waves, 0, sizeof(g_waves));
  }
}

static int CamX(void) { return (int16)mirror_current_layer1_xpos; }
static int CamY(void) { return (int16)mirror_current_layer1_ypos; }

static bool OnScreenAt(double lx, double ly, int margin) {  // level px
  double sx = lx - CamX(), sy = ly - CamY();
  return sx > -margin && sx < 256 + margin && sy > -margin && sy < 224 + margin;
}

static const McKoopaFrame *ClipFrame(int clip, int f) {
  const McKoopaClip *c = &kMcBowserClips[clip];
  if (f < 1) f = 1;
  if (f > c->nframes) f = c->nframes;
  return &kMcBowserFrames[c->first_frame + f - 1];
}

// The clip placed on a root frame besides the health bar (hurt_mc, jump, pant, teleport, walk), or -1.
static int SubClipOf(int root) {
  const McKoopaFrame *rf = ClipFrame(0, root);
  for (int i = 0; i < rf->nitems; i++) {
    const McKoopaItem *it = &kMcBowserItems[rf->first_item + i];
    if (it->kind == 1 && it->ref != g_clip_bar) return it->ref;
  }
  return -1;
}

// ---- level probes: `_root.ground.hitTest(x, y, true)` in stage px ----
static int TileAt(double sx, double sy) {
  return McSmw_LevelMap16At((int)floor(sx / kScale), (int)floor(sy / kScale));
}
static bool Ground(double sx, double sy) {  // anything Bowser can stand on
  int t = TileAt(sx, sy);
  return t >= 0x100 && t < 0x1D8;
}
static bool Solid(double sx, double sy) {   // walls and ceilings (solid on every side)
  int t = TileAt(sx, sy);
  return t >= 0x111 && t < 0x16E;
}

// ---- timeline (gotoAndStop / gotoAndPlay) ----
static void RootGoto(Bowser *b, int f, bool play) {
  b->root_playing = play;
  if (b->root == f) return;  // the same frame: the clip on it keeps running
  b->root = f;
  // every frame's clip is another character, so going to a frame with a clip makes a new instance at its frame 1
  b->sub_frame = 1;
  b->sub_playing = true;
  b->sub_enter = false;
  b->sub_inst++;
}

// ---- boxes ----
typedef struct Box { double x0, y0, x1, y1; } Box;

static void Compose(const float *a, const float *b, float *c) {  // c = a * b (b applied first)
  float r[6] = {
    a[0] * b[0] + a[2] * b[1], a[1] * b[0] + a[3] * b[1],
    a[0] * b[2] + a[2] * b[3], a[1] * b[2] + a[3] * b[3],
    a[0] * b[4] + a[2] * b[5] + a[4], a[1] * b[4] + a[3] * b[5] + a[5],
  };
  memcpy(c, r, sizeof(r));
}

static void UnionBounds(const float *m, const float *bnd, float *bb) {
  float xs[4] = { bnd[0], bnd[2], bnd[0], bnd[2] }, ys[4] = { bnd[1], bnd[1], bnd[3], bnd[3] };
  for (int i = 0; i < 4; i++) {
    float X = m[0] * xs[i] + m[2] * ys[i] + m[4], Y = m[1] * xs[i] + m[3] * ys[i] + m[5];
    bb[0] = fminf(bb[0], X), bb[2] = fmaxf(bb[2], X), bb[1] = fminf(bb[1], Y), bb[3] = fmaxf(bb[3], Y);
  }
}

static int SubFrameOf(const Bowser *b, const McKoopaItem *it, int depth) {
  if (depth == 0 && b && it->ref == SubClipOf(b->root)) return b->sub_frame;
  return it->age % kMcBowserClips[it->ref].nframes + 1;
}

// Bounds of a clip frame's visible art through m (the health bar and the hidden hit boxes left out).
static void ClipBounds(const Bowser *b, int clip, int frame, const float *m, float *bb, int depth) {
  const McKoopaFrame *f = ClipFrame(clip, frame);
  if (f->bounds[2] > f->bounds[0]) UnionBounds(m, f->bounds, bb);
  if (depth >= 3) return;
  for (int i = 0; i < f->nitems; i++) {
    const McKoopaItem *it = &kMcBowserItems[f->first_item + i];
    if (it->kind != 1 || it->ref == g_clip_bar || it->name == kMcNameBound) continue;
    float c[6];
    Compose(m, it->m, c);
    ClipBounds(b, it->ref, SubFrameOf(b, it, depth), c, bb, depth + 1);
  }
}

static Box BodyBox(const Bowser *b) {  // this.getBounds(_root), level px
  float m[6] = { (float)(b->xscale / 100.0), 0, 0, 1, 0, 0 };
  float bb[4] = { 1e9f, 1e9f, -1e9f, -1e9f };
  ClipBounds(b, 0, b->root, m, bb, 0);
  if (bb[2] <= bb[0]) bb[0] = -55, bb[1] = -132, bb[2] = 55, bb[3] = 0;  // frame 1's bounds
  Box r = { (b->x + bb[0]) / kScale, (b->y + bb[1]) / kScale, (b->x + bb[2]) / kScale, (b->y + bb[3]) / kScale };
  return r;
}

static bool Overlap(Box a, double x0, double y0, double x1, double y1) {
  return a.x0 < x1 && x0 < a.x1 && a.y0 < y1 && y0 < a.y1;
}

bool McBowser_Clipping(uint8 k, int *x, int *y, int *w, int *h) {
  if (!McBowser_IsActive(k)) return false;
  Box r = BodyBox(&g_b[k]);
  *x = (int)floor(r.x0), *y = (int)floor(r.y0);
  *w = (int)ceil(r.x1 - r.x0), *h = (int)ceil(r.y1 - r.y0);
  if (*w > 255) *w = 255;
  if (*h > 255) *h = 255;
  return true;
}

// ---- the hero gets hit (MC Mario's life and knockback, SMW Mario's power-down, or Mario loses Yoshi) ----
static void HurtHero(int dir) {
  uint8 saved = spr_current_slotid;
  spr_current_slotid = 0xFF;  // not a sprite's contact: the knockback comes from dir
  McSmw_SetNextHitDir(dir);
  McSmw_EnemyHurtsPlayer();
  McSmw_SetNextHitDir(0);
  spr_current_slotid = saved;
}

// ---- fire breath and shockwaves ----
// A row of Bowsers (YI2 has eight Koopas side by side) would start the same 2-second sound together and take the
// MC voices from Mario's punches: the same sound is started at most once every 20 frames.
static void PlayEffectSound(int id) {
  static uint32 last[2];
  int i = id == 554 ? 0 : 1;
  if (!g_config.mc_sounds || (last[i] && g_frame - last[i] < 20)) return;
  last[i] = g_frame;
  McAudio_Play(id);
}

static void SpawnFlame(const Bowser *b) {  // the `flame` template: duplicateMovieClip at (bowser.x, bowser.y - 70)
  for (int i = 0; i < (int)(sizeof(g_flames) / sizeof(g_flames[0])); i++) {
    Flame *f = &g_flames[i];
    if (f->on) continue;
    memset(f, 0, sizeof(*f));
    f->on = true;
    f->x = b->x, f->y = b->y - 70;
    // onClipEvent(load): life 300; _rotation = 270 (read back as -90); Angle = _rotation - 90; twist = random(150)
    f->life = 300;
    f->Angle = -90 - 90;
    f->twist = rand() % 150;
    if (b->xscale == -100) f->goLeft = true;
    else if (b->xscale == 100) f->goRight = true;
    f->xscale = 100;
    const McKoopaFrame *fr = ClipFrame(g_clip_flame, 1);
    if (fr->sound && OnScreenAt(f->x / kScale, f->y / kScale, 32))
      PlayEffectSound(fr->sound);
    return;
  }
}

static void SpawnWave(double x, double y) {  // groundHitOrig.duplicateMovieClip at Bowser's feet
  for (int i = 0; i < (int)(sizeof(g_waves) / sizeof(g_waves[0])); i++) {
    Wave *w = &g_waves[i];
    if (w->on) continue;
    w->on = true;
    w->x = x, w->y = y;
    w->alpha = 100, w->frame = 1, w->effect = true;
    const McKoopaFrame *fr = ClipFrame(g_clip_wave, 1);
    if (fr->sound && OnScreenAt(x / kScale, y / kScale, 48))
      PlayEffectSound(fr->sound);
    return;
  }
}

static void FlameMatrix(const Flame *f, float *m) {  // _x, _y, _rotation = Angle, _xscale (stage px)
  double r = f->Angle * kPi / 180, sx = f->xscale / 100.0;
  m[0] = (float)(sx * cos(r)), m[1] = (float)(sx * sin(r)), m[2] = (float)-sin(r), m[3] = (float)cos(r);
  m[4] = (float)f->x, m[5] = (float)f->y;
}

static Box FlameBound(const Flame *f) {  // this.bound, level px
  float m[6], bb[4] = { 1e9f, 1e9f, -1e9f, -1e9f };
  FlameMatrix(f, m);
  const McKoopaFrame *fr = ClipFrame(g_clip_flame, 1);
  for (int i = 0; i < fr->nitems; i++) {
    const McKoopaItem *it = &kMcBowserItems[fr->first_item + i];
    if (it->name != kMcNameBound) continue;
    float c[6];
    Compose(m, it->m, c);
    UnionBounds(c, ClipFrame(it->ref, 1)->bounds, bb);
  }
  Box r = { bb[0] / kScale, bb[1] / kScale, bb[2] / kScale, bb[3] / kScale };
  return r;
}

// `this.hitTest(x, y, true)` on a shockwave: the point on its drawn shape (alpha > 0), stage px
static bool WaveHit(const Wave *w, double px, double py) {
  const McKoopaFrame *fr = ClipFrame(g_clip_wave, w->frame);
  for (int i = 0; i < fr->nitems; i++) {
    const McKoopaItem *it = &kMcBowserItems[fr->first_item + i];
    if (it->kind != 0) continue;
    const McImage *im = &kMcBowserImages[it->ref];
    float wm[6] = { (float)kWaveScale, 0, 0, (float)kWaveScale, (float)w->x, (float)w->y }, c[6];
    Compose(wm, it->m, c);
    // image px -> stage px: (u, v) -> c * (ox + u / k, oy + v / k)
    double a = c[0] / im->k, b = c[1] / im->k, cc = c[2] / im->k, d = c[3] / im->k;
    double e = c[0] * im->ox + c[2] * im->oy + c[4], f = c[1] * im->ox + c[3] * im->oy + c[5];
    double det = a * d - cc * b;
    if (fabs(det) < 1e-9) continue;
    double u = (d * (px - e) - cc * (py - f)) / det, v = (-b * (px - e) + a * (py - f)) / det;
    int iu = (int)floor(u), iv = (int)floor(v);
    if (iu < 0 || iv < 0 || iu >= im->w || iv >= im->h) continue;
    if (im->px[iv * im->w + iu] >> 24) return true;
  }
  return false;
}

static void TickFlames(const McHeroInfo *hero) {
  for (int i = 0; i < (int)(sizeof(g_flames) / sizeof(g_flames[0])); i++) {
    Flame *f = &g_flames[i];
    if (!f->on) continue;
    f->age++;
    double xSpeed = cos(f->Angle * kPi / 180) * 5, ySpeed = sin(f->Angle * kPi / 180) * 5;
    if (f->goLeft) {
      f->xscale = -100;
      f->x += xSpeed;
      f->y += ySpeed;
    }
    if (f->goRight) {
      f->xscale = 100;
      f->x -= xSpeed;
      f->y -= ySpeed;
    }
    f->twist++;
    // (_rotation = Angle: FlameMatrix)
    if (f->twist < 50)
      f->Angle += 1;  // turnSpeed
    else if (f->twist < 150)
      f->Angle -= 1;
    if (f->twist >= 150)
      f->twist = -50;
    if (f->life > 0)
      f->life--;
    if (f->life <= 0) {
      f->on = false;
      continue;
    }
    Box bd = FlameBound(f);
    if (hero->alive && Overlap(bd, hero->x0, hero->y0, hero->x1, hero->y1)) {
      // hero.grav = 5; hero.xSpeed -= 5 facing right (+= 5 facing left); life -= 0.25; hurt
      HurtHero(hero->xscale == 100 ? -1 : 1);
      f->on = false;
    }
  }
}

static void TickWaves(const McHeroInfo *hero) {
  for (int i = 0; i < (int)(sizeof(g_waves) / sizeof(g_waves[0])); i++) {
    Wave *w = &g_waves[i];
    if (!w->on) continue;
    if (w->frame < kMcBowserClips[g_clip_wave].nframes) w->frame++;  // plays to its frame 21: stop()
    w->alpha -= 5;
    if (w->alpha <= 0) {
      w->on = false;
      continue;
    }
    if (w->effect && hero->alive && WaveHit(w, hero->feet_x * kScale, hero->feet_y * kScale)) {
      // hero.xSpeed = +-10, life -= _alpha / 90. (MC compares hero._x with the bowser clip itself, which is always
      // false, so it always pushes right; here the push is away from the shockwave.)
      HurtHero(hero->feet_x * kScale < w->x ? -1 : 1);
      w->effect = false;
    }
  }
}

// ---- Bowser ----
static void InitBowser(uint8 k) {
  Bowser *b = &g_b[k];
  memset(b, 0, sizeof(*b));
  b->active = true;
  // onClipEvent(load) of _root.bowser
  b->attackSpeed = 100;
  b->attackSpeedDefult = 100;
  b->xscale = 100;
  b->grav = 0;
  b->xSpeed = 0;
  b->onGround = true;
  b->dead = false;
  b->life = kMaxLife;  // MC: 100
  b->moveSpeed = 3;
  b->stopSliding = 10;
  b->attack1 = 500;
  b->attack1Orig = b->attack1;
  b->jump = 5;
  b->jumpOrig = b->jump;
  b->jumpPower = 10;
  b->jumpSpeed = 6;
  b->pant = 300;
  b->pantOrig = b->pant;
  b->x = ((int16)GetSprXPos(k) + 8) * kScale;
  b->y = ((int16)GetSprYPos(k) + 16) * kScale;
  b->root = kFrameIdle;
  b->sub_inst = 1;
  spr_property_bits1656[k] &= ~0x10;  // the cape can't turn him into a shell
  spr_property_bits1686[k] |= 0x01;   // Yoshi can't eat him
}

// Below the level (or in lava): MC's `_y > 440` (the bottom of its arena)
static bool InPit(const Bowser *b) {
  return b->y > 0 && McSmw_LevelMap16At(16, (int)floor(b->y / kScale)) < 0;
}

// teleport frame 9: `_parent._x = 320; _parent._y = 250` (the middle of MC's arena): the middle of the screen, raised
// out of any terrain there
static void TeleportPlace(uint8 k, Bowser *b) {
  double lx = CamX() + 128, ly = CamY() + 84;
  for (int i = 0; i < 10 && (Solid(lx * kScale, (ly - 2) * kScale) || Solid(lx * kScale, (ly - 24) * kScale)); i++)
    ly -= 16;
  SetSprXPos(k, (uint16)(int)(lx - 8));
  SetSprYPos(k, (uint16)(int)(ly - 16));
  b->x = lx * kScale, b->y = ly * kScale;
  b->in_pit = false;
  b->rem_x = b->rem_y = 0;
}

// The jump clip's onEnterFrame (defined by its frame 35): landing on its last frame makes a shockwave and counts a
// jump; landing at any frame starts the jump again.
static void JumpEnterFrame(Bowser *b) {
  if (Ground(b->x, b->y + 5) || b->onGround) {
    if (b->sub_frame == 35) {
      SpawnWave(b->x, b->y);
      b->jump = b->jump - 1;
    }
    b->sub_frame = 1;  // this.gotoAndPlay(1)
    b->sub_playing = true;
  }
}

static void SubFrameScript(uint8 k, Bowser *b, int clip, int f) {
  if (clip == g_clip_hurt) {
    if (f == 16) RootGoto(b, kFrameIdle, false);  // _parent.gotoAndStop(1)
  } else if (clip == g_clip_jump) {
    if (f == 2) {
      b->grav = b->jumpPower;
      b->grav *= 2;
    } else if (f == 11) {
      b->grav /= 2;
    } else if (f == 24) {
      b->grav -= 20;
    } else if (f == 35) {
      b->sub_playing = false;  // stop(); onEnterFrame = ...
      b->sub_enter = true;
    }
  } else if (clip == g_clip_tele) {
    if (f == 9) TeleportPlace(k, b);
    else if (f == 18) RootGoto(b, kFrameIdle, false);
  }
}

// Bowser's onEnterFrame, minus the hit tests on the hero's attack boxes (McBowser_Punch and the SMW weapon hooks)
// and the ground push-outs (SMW collision, Move).
static void BowserEnterFrame(uint8 k, Bowser *b, const McHeroInfo *hero) {
  b->grav -= 0.5;
  // (_Y -= grav, onGround, the push-outs: Move. `while(hitTest(_X - 20, _Y - 20)) { _X += moveSpeed; xSpeed = 0; }`
  // zeroes xSpeed against a wall on the left only.)
  if (!b->dead && (spr_table1588[k] & 2))
    b->xSpeed = 0;
  if (b->root < 22) {
    if (b->xSpeed != 0) {
      if (b->stopSliding > 0)
        b->stopSliding--;
      else if (b->stopSliding == 0)
        b->xSpeed = 0;
    }
  } else {
    b->stopSliding = 10;
  }
  b->hurt = b->root == kFrameHurt;
  // (the six `this.hitTest(_root.hero.attack.hitN)` blocks: TakeHit)
  if (!b->dead) {
    if (b->hurt) {
      // hurt_mc has no "hurt1" label, so its gotoAndPlay("hurt1") only keeps it playing
      if (Solid(b->x - 23, b->y - 20)) {
        b->xSpeed = (b->xSpeed + 1) * -1;
        RootGoto(b, kFrameHurt, false);
        b->sub_playing = true;
      }
      if (Solid(b->x + 23, b->y - 20)) {
        b->xSpeed = (b->xSpeed + 1) * -1;
        b->sub_playing = true;
      }
      // (the hurt5 landing bounce needs hurt_mc frames 69-82; Bowser's has 16)
    }
  }
  b->move_dx = 0;
  if (b->xSpeed > 0) {
    if (b->onGround)
      b->xSpeed -= 1;
    else
      b->xSpeed -= 0.25;
    b->move_dx += b->xSpeed;
  }
  if (b->xSpeed < 0) {
    if (b->onGround)
      b->xSpeed += 1;
    else
      b->xSpeed += 0.25;
    b->move_dx += b->xSpeed;
  }
  if (b->attack1 > 0) {
    if (!b->hurt && b->root < 22) {
      if (b->attackSpeed > 0)
        b->attackSpeed--;
      if (b->attackSpeed == 0)
        b->attackSpeed = b->attackSpeedDefult;
      if (b->attackSpeed < b->attackSpeedDefult / 2.0)
        RootGoto(b, kFrameBreath, false);
      if (b->attackSpeed > b->attackSpeedDefult / 2.0)
        RootGoto(b, kFrameFire, true);
    }
    if (b->life > kMaxLife * 0.7)  // MC: life > 70, > 50, > 20 of 100
      b->attackSpeedDefult = 100;
    else if (b->life > kMaxLife * 0.5)
      b->attackSpeedDefult = 50;
    else if (b->life > kMaxLife * 0.2)
      b->attackSpeedDefult = 20;
    else if (b->life > 0)
      b->attackSpeedDefult = 10;
    b->attack1--;
  }
  if (b->attack1 == 0) {
    if (b->root <= 22)
      RootGoto(b, kFrameJump, false);
    if (b->root == kFrameJump && b->jump > 0)
      b->xSpeed = b->xscale == 100 ? b->jumpSpeed : -b->jumpSpeed;
    if (b->jump == 0 && b->root < 25)
      RootGoto(b, kFramePant, false);
  }
  if (b->root == kFramePant) {
    if (b->pant > 0)
      b->pant--;
    if (b->pant == 0) {
      b->jump = b->jumpOrig;
      b->jumpOrig++;
      b->pant = b->pantOrig;
      b->attack1 = b->attack1Orig;
      RootGoto(b, kFrameIdle, false);
    }
  }
  if (!b->dead) {
    if (InPit(b) || b->in_pit) {  // _Y = 445; gotoAndStop(25)
      b->in_pit = true;
      RootGoto(b, kFrameTeleport, false);
    }
    if (b->life <= 0)
      b->dead = true;
  }
  // (_y > 855: MC's next-level fade)
  if (b->x > hero->feet_x * kScale)
    b->xscale = -100;
  if (b->x < hero->feet_x * kScale)
    b->xscale = 100;
}

// One MC tick in AVM1 order: the clip on his timeline (newer than him) queues its onEnterFrame and advances, then he
// does; the queued actions then run in that order (an action of a clip a goto has replaced is dropped).
static void Tick(uint8 k, Bowser *b, const McHeroInfo *hero) {
  enum { kSubEnter, kSubScript, kRootEnter, kRootScript };
  struct { int kind, frame, clip; uint32 inst; } q[4];
  int n = 0;
  int sub = SubClipOf(b->root);
  if (sub >= 0) {
    if (b->sub_enter && sub == g_clip_jump)
      q[n].kind = kSubEnter, q[n].frame = 0, q[n].clip = sub, q[n++].inst = b->sub_inst;
    if (b->sub_playing) {
      int nf = kMcBowserClips[sub].nframes;
      b->sub_frame = b->sub_frame >= nf ? 1 : b->sub_frame + 1;
      q[n].kind = kSubScript, q[n].frame = b->sub_frame, q[n].clip = sub, q[n++].inst = b->sub_inst;
    }
  }
  q[n].kind = kRootEnter, q[n].frame = 0, q[n].clip = 0, q[n++].inst = 0;
  if (b->root_playing) {
    RootGoto(b, b->root >= kMcBowserClips[0].nframes ? 1 : b->root + 1, true);
    q[n].kind = kRootScript, q[n].frame = b->root, q[n].clip = 0, q[n++].inst = 0;
  }
  for (int i = 0; i < n; i++) {
    switch (q[i].kind) {
    case kSubEnter:
      if (q[i].inst == b->sub_inst) JumpEnterFrame(b);
      break;
    case kSubScript:
      if (q[i].inst == b->sub_inst) SubFrameScript(k, b, q[i].clip, q[i].frame);
      break;
    case kRootEnter:
      BowserEnterFrame(k, b, hero);
      break;
    case kRootScript:
      if (q[i].frame == 21) RootGoto(b, kFrameIdle, false);  // frame 21: gotoAndStop(1)
      break;
    }
  }
  // the `flame` template's enterFrame: a new flame whenever bowser.attackSpeed == 1
  if (b->attackSpeed == 1)
    SpawnFlame(b);
}

// Hit level reactions (the script's six `if(this.hitTest(_root.hero.attack.hitN))` blocks). s = the hero's facing
// (_root.hero._xscale), hx = _root.hero.xSpeed.
static void TakeHit(Bowser *b, int level, int s, double hx) {
  switch (level) {
  case 1:
    b->life -= 1;
    RootGoto(b, kFrameHurt, false);
    b->xSpeed = hx + s * 1;
    break;
  case 2:
    b->life -= 2;
    RootGoto(b, kFrameHurt, false);
    b->grav = 3;
    b->xSpeed = hx + s * 1.5;
    break;
  case 3:
    b->life -= 3;
    RootGoto(b, kFrameHurt, false);
    b->grav = 4;
    b->xSpeed = hx + s * 2.5;
    break;
  case 4:
    b->life -= 4;
    b->grav = 4;
    RootGoto(b, kFrameHurt, false);
    b->xSpeed = hx + s * 4.5;
    break;
  case 5:
    b->life -= 1;
    b->grav = 7;
    RootGoto(b, kFrameHurt, false);
    b->xSpeed = hx + s * (rand() % 7);
    break;
  default:
    if (!b->onGround) {
      b->grav = -20;
      b->life -= 3;
    } else {
      b->grav = 7;
      b->life -= 1;
    }
    RootGoto(b, kFrameHurt, false);
    b->xSpeed = hx + s * (b->onGround ? 1 : 5);
    break;
  }
  if (!b->onGround)
    b->grav -= 2;
  // (hurt_mc.gotoAndPlay("hurt1" ... "hurt5"): hurt_mc has no labels, it just plays)
  b->sub_playing = true;
}

static void Defeated(uint8 k, Bowser *b) {  // the hit that brought life to 0
  if (b->scored) return;
  b->scored = true;
  b->dead = true;
  GivePoints_02ACEF(k, 0x0D);  // a 1-up: he is a boss
  Cheats_EnemyDefeated(k);
}

static bool Hit(uint8 k, int level, int s, double hx) {
  if (!McBowser_IsActive(k) || g_b[k].dead) return false;
  Bowser *b = &g_b[k];
  TakeHit(b, level, s, hx);
  if (b->life <= 0) Defeated(k, b);
  return true;
}

bool McBowser_Punch(uint8 k, int level) {
  McHeroInfo hero;
  McSmw_GetHeroInfo(&hero);
  if (!Hit(k, level, hero.xscale == 100 ? 1 : -1, hero.xspeed)) return false;
  if (Cheats_SuperPunch() && !g_b[k].dead) {  // the super punch cheat defeats any ordinary enemy at once
    g_b[k].life = 0;
    Defeated(k, &g_b[k]);
  }
  return true;
}

bool McBowser_FireballHit(uint8 k, int dir) {
  if (!McBowser_IsActive(k)) return false;
  if (!g_b[k].dead) {
    Hit(k, 1, dir, 0);
    io_sound_ch1 = 3;
  }
  return true;
}

bool McBowser_CapeHit(uint8 k, uint8 r14) {
  if (!McBowser_IsActive(k)) return false;
  if (!g_b[k].dead) {
    int dir = (int16)GetSprXPos(k) >= (int16)player_xpos ? 1 : -1;
    Hit(k, r14 == 53 ? 4 : 2, dir, 0);  // a cape dive's ground pound is strong; a cape swing or a bounced block medium
    SpawnContactEffectFromSide(k);
    spr_decrementing_table154c[k] = kMcCapeInvincible;  // (SMW gives 8: a spinning cape would hit every 8 frames)
  }
  return true;
}

// SMW knocked the Koopa out (a kicked or thrown shell, a carried item...): a strong hit instead.
static void KnockOutToHit(uint8 k, Bowser *b) {
  int dir = (int8)spr_xspeed[k] >= 0 ? 1 : -1;
  spr_current_status[k] = 8;
  spr_decrementing_table1564[k] = 16;  // no other sprite contact for a moment (one hit per shell)
  if (!b->dead) Hit(k, 4, dir, 0);
}

// SMW Mario (or MC Mario) touching him: MC's Bowser has no contact damage. Landing on him with a spin jump (or on
// Yoshi) is a stomp, a strong hit; a plain jump only bounces off. The star or a slide hits him strongly. MC Mario has
// no stomp (as in MC, he falls past him: a bounce every 8 frames held him on Bowser's head under his flames).
static void PlayerContact(uint8 k, Bowser *b, const McHeroInfo *hero) {
  if (b->dead || player_current_state || spr_decrementing_table154c[k]) return;
  Box body = BodyBox(b);
  if (!Overlap(body, hero->x0, hero->y0, hero->x1, hero->y1)) return;
  int dir = (int16)GetSprXPos(k) + 8 >= (int16)player_xpos + 8 ? 1 : -1;
  if (timer_star_power || player_sliding_on_ground) {  // the star, or sliding down a slope into him
    Hit(k, 4, dir, 0);
    spr_decrementing_table154c[k] = 24;
    io_sound_ch1 = 3;
    return;
  }
  if (hero->falling && hero->y1 < body.y0 + (body.y1 - body.y0) * 0.5 && !hero->mc) {
    // only a spin jump (or Yoshi) hurts him: a plain jump bounces off his spiked shell, as off SMW's spiked enemies
    // when spinning (no damage either way to Mario: MC's Bowser has none on contact)
    spr_decrementing_table154c[k] = 8;
    BoostMarioSpeed();
    if (!(player_spin_jump_flag || player_riding_yoshi_flag)) {
      io_sound_ch1 = 1;  // SMW's bonk (a hit that does nothing)
      return;
    }
    Hit(k, 4, dir, 0);
    io_sound_ch1 = 2;  // SMW's spin-jump contact sound (no points: only defeating him scores)
    SpawnContactEffectFromAbove(k);
  }
}

// ---- SMW side: speeds through SMW's collision ----
static void SetOffscreenFlags(uint8 k) {  // what GetDrawInfo would set (SubOffscreen and the contact stars read them)
  spr_xoffscreen_flag[k] = (uint16)(GetSprXPos(k) - mirror_current_layer1_xpos) >= 256;
  spr_yoffscreen_flag[k] = (uint16)(GetSprYPos(k) + 16 - mirror_current_layer1_ypos) >= 256;
}

static void Move(uint8 k, Bowser *b) {
  if (b->in_pit) {  // _Y = 445 every tick until the teleport moves him
    spr_xspeed[k] = spr_yspeed[k] = 0;
    b->rem_x = b->rem_y = 0;
    return;
  }
  double sxs = b->move_dx * kToSmwSpeed + b->rem_x, sys = -b->grav * kToSmwSpeed + b->rem_y;
  int ix = (int)lround(sxs), iy = (int)lround(sys);
  ix = ix < -127 ? -127 : ix > 127 ? 127 : ix;
  iy = iy < -127 ? -127 : iy > 112 ? 112 : iy;
  b->rem_x = sxs - ix, b->rem_y = sys - iy;
  // `while(hitTest(_X, _Y - _height)) _Y++`: no rising into a ceiling
  if (!b->dead && iy < 0) {
    Box body = BodyBox(b);
    double top = body.y0 + iy / 16.0;
    if (Solid(b->x, top * kScale) || Solid(b->x - 12, top * kScale) || Solid(b->x + 12, top * kScale))
      iy = 0, b->rem_y = 0;
  }
  spr_xspeed[k] = (uint8)(int8)ix;
  spr_yspeed[k] = (uint8)(int8)iy;
  UpdateNormalSpritePosition_X(k);
  UpdateNormalSpritePosition_Y(k);
  if (!b->dead) {
    HandleNormalSpriteLevelCollision(k);
    if (spr_current_status[k] == 5) {  // lava: SMW would sink a Koopa; MC's pit teleport instead
      spr_current_status[k] = 8;
      b->in_pit = true;
    }
    uint8 c = spr_table1588[k];
    b->onGround = (c & 4) != 0;
    if (b->onGround && b->grav < 0) b->grav = 0;  // while(hitTest(x, y)) { grav = 0; y--; }
    if (c & 3) b->rem_x = 0;
    if (b->onGround) spr_yspeed[k] = 0;
  } else {
    b->onGround = false;  // defeated: falls through everything, as in MC
  }
  b->x = ((int16)GetSprXPos(k) + 8) * kScale;
  b->y = ((int16)GetSprYPos(k) + 16) * kScale;
}

// ---- drawing ----
static void ComposeCx(const int16 *p, const int16 *c, int16 *out) {
  for (int i = 0; i < 4; i++) {
    out[i] = (int16)(p[i] * c[i] / 256);
    out[4 + i] = (int16)(p[i] * c[4 + i] / 256 + p[4 + i]);
  }
}

static void DrawClip(const Bowser *b, int clip, int frame, const float *m, const int16 *cx, uint16 z, int depth) {
  const McKoopaFrame *f = ClipFrame(clip, frame);
  for (int i = 0; i < f->nitems; i++) {
    const McKoopaItem *it = &kMcBowserItems[f->first_item + i];
    if (it->name == kMcNameBound || it->name == kMcNameHit1 || it->name == kMcNameHit2) continue;  // invisible
    float im6[6];
    memcpy(im6, it->m, sizeof(im6));
    if (b && it->kind == 1 && it->ref == g_clip_bar) {
      if (b->bar_hidden) continue;
      // the bar clip's onClipEvent(enterFrame): _xscale = +-100 with its parent, so it never reads mirrored
      if (b->xscale < 0) im6[0] = -im6[0], im6[1] = -im6[1];
    }
    if (b && it->name == kMcNameBar) {  // bar._xscale = life / 100 * 100 (full at kMaxLife)
      float s = (float)(b->life / kMaxLife);
      im6[0] *= s, im6[1] *= s;
    }
    float c[6];
    Compose(m, im6, c);
    int16 ccx[8];
    ComposeCx(cx, it->cx, ccx);
    if (it->kind == 0) {
      const McImage *im = &kMcBowserImages[it->ref];
      float s[6] = { c[0] / im->k, c[1] / im->k, c[2] / im->k, c[3] / im->k,
                     c[0] * im->ox + c[2] * im->oy + c[4], c[1] * im->ox + c[3] * im->oy + c[5] };
      McOverlay_DrawImage(im, s, ccx, z);
    } else if (depth < 4) {
      DrawClip(b, it->ref, SubFrameOf(b, it, depth), c, ccx, z, depth + 1);
    }
  }
}

static const int16 kNoCx[8] = { 256, 256, 256, 256, 0, 0, 0, 0 };

static void ScreenMatrix(double sx_stage, double sy_stage, double xs, double ys, float *m) {  // stage px -> screen
  m[0] = (float)(xs / kScale), m[1] = 0, m[2] = 0, m[3] = (float)(ys / kScale);
  m[4] = (float)(sx_stage / kScale - CamX()), m[5] = (float)(sy_stage / kScale - CamY());
}

static void Draw(uint8 k, Bowser *b) {
  if (!OnScreenAt(b->x / kScale, b->y / kScale, 64)) return;
  float m[6];
  ScreenMatrix(b->x, b->y, b->xscale / 100.0, 1, m);
  uint16 z = McOverlay_SpriteZ((spr_table15f6[k] & 0x20) ? 3 : 2);
  DrawClip(b, 0, b->root, m, kNoCx, z, 0);
}

static void DrawEffects(void) {
  uint16 z = McOverlay_SpriteZ(2);
  for (int i = 0; i < (int)(sizeof(g_waves) / sizeof(g_waves[0])); i++) {
    const Wave *w = &g_waves[i];
    if (!w->on || !OnScreenAt(w->x / kScale, w->y / kScale, 64)) continue;
    float m[6];
    ScreenMatrix(w->x, w->y, kWaveScale, kWaveScale, m);
    int16 cx[8] = { 256, 256, 256, (int16)(w->alpha * 256 / 100), 0, 0, 0, 0 };  // _alpha
    DrawClip(NULL, g_clip_wave, w->frame, m, cx, z, 0);
  }
  for (int i = 0; i < (int)(sizeof(g_flames) / sizeof(g_flames[0])); i++) {
    const Flame *f = &g_flames[i];
    if (!f->on || !OnScreenAt(f->x / kScale, f->y / kScale, 32)) continue;
    float fm[6], sm[6], m[6];
    FlameMatrix(f, fm);
    fm[4] = fm[5] = 0;
    ScreenMatrix(f->x, f->y, 1, 1, sm);
    Compose(sm, fm, m);
    // the flame clip runs on its own: its nested animation by the flame's age
    const McKoopaFrame *fr = ClipFrame(g_clip_flame, 1);
    for (int j = 0; j < fr->nitems; j++) {
      const McKoopaItem *it = &kMcBowserItems[fr->first_item + j];
      if (it->kind != 1 || it->name == kMcNameBound) continue;
      float c[6];
      Compose(m, it->m, c);
      DrawClip(NULL, it->ref, f->age % kMcBowserClips[it->ref].nframes + 1, c, it->cx, z, 1);
    }
  }
}

bool McBowser_Main(uint8 k) {
  if (!McBowser_Enabled() || spr_spriteid[k] > 7) return false;
  Setup();
  Bowser *b = &g_b[k];
  if (!b->active || b->seen + 1 < g_frame)  // a new Koopa in this slot (or one not seen for a while)
    InitBowser(k);
  b->seen = g_frame;
  if (!flag_sprites_locked) {
    McHeroInfo hero;
    McSmw_GetHeroInfo(&hero);
    CheckNormalSpriteToNormalSpriteCollision(k);  // kicked shells (they knock a Koopa out: a strong hit instead)
    if (spr_current_status[k] == 2)
      KnockOutToHit(k, b);
    if (spr_current_status[k] != 8) {
      b->active = false;
      return true;
    }
    if (g_tick_now)
      Tick(k, b, &hero);
    if (b->life < 0) b->bar_hidden = true;  // the bar clip: `_visible = false` once life < 0
    Move(k, b);
    if (!b->dead && InPit(b))
      b->in_pit = true;  // held there (not erased by SubOffscreen) until his next tick starts the teleport
    PlayerContact(k, b, &hero);
    SetOffscreenFlags(k);
    // (not in the MC level: there he stays when Mario walks away, never erased and loaded again at full life)
    if (!b->in_pit && b->root != kFrameTeleport && !McLevel_Bowser())
      SubOffscreen_Bank01_Entry1(k);
    if (b->dead && ((int16)GetSprYPos(k) > CamY() + 240 || InPit(b))) {
      spr_current_status[k] = 0;  // fell out of the level
      b->active = false;
      return true;
    }
  }
  if (spr_current_status[k])
    Draw(k, b);
  return true;
}

// SMW knocked the Koopa out in another sprite's routine: back to the normal state, as a strong hit (or the fall of a
// defeated Bowser).
bool McBowser_Dead(uint8 k) {
  if (!McBowser_Enabled() || spr_spriteid[k] > 7 || !McBowser_IsActive(k)) return false;
  KnockOutToHit(k, &g_b[k]);
  return McBowser_Main(k);
}

void McBowser_AfterSprites(void) {
  if (misc_game_mode != 0x14) return;
  Setup();
  if (!flag_sprites_locked && g_tick_now) {
    McHeroInfo hero;
    McSmw_GetHeroInfo(&hero);
    TickFlames(&hero);
    TickWaves(&hero);
  }
  DrawEffects();
}

int McBowser_DebugInfo(uint8 k, char *buf, int n) {
  if (!McBowser_IsActive(k)) return 0;
  const Bowser *b = &g_b[k];
  Box bx = BodyBox(b);
  int flames = 0, waves = 0;
  for (int i = 0; i < (int)(sizeof(g_flames) / sizeof(g_flames[0])); i++) flames += g_flames[i].on;
  for (int i = 0; i < (int)(sizeof(g_waves) / sizeof(g_waves[0])); i++) waves += g_waves[i].on;
  return snprintf(buf, n, " bowser life=%.1f root=%d sub=%d dead=%d grav=%.1f xs=%.1f atk=%d/%d jump=%d pant=%d "
                  "body=%.0f,%.0f-%.0f,%.0f flames=%d waves=%d",
                  b->life, b->root, b->sub_frame, b->dead, b->grav, b->xSpeed, b->attack1, b->attackSpeed, b->jump,
                  b->pant, bx.x0, bx.y0, bx.x1, bx.y1, flames, waves);
}
