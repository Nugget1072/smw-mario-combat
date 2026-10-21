// "Mario Combat Koopas" cheat (see mc_koopa.h).
//
// The enemy's onEnterFrame (DefineSprite_163/frame_1/DoAction.as) is ported in KoopaTick in the original statement
// order. Units are MC stage px (3 per SNES px) and MC's 50 Hz ticks, as in the hero port. What changes inside SMW:
// - The `_root.ground.hitTest` push-outs are SMW's sprite level collision (slopes, ledges, walls, water), fed with
//   speeds computed from the script's own motion each frame.
// - `_root.hero` is MC Mario (his bound and attack hit boxes) or SMW Mario (his body box).
// - The hero's hits come in through the glue's attack boxes (McKoopa_Punch), like on every other SMW enemy.
// - Score: SMW points when the Koopa is defeated (a hit chain or stomp chain step), not MC's small HUD score.
// - A dead Koopa falls through the floor as in MC; it is removed when it leaves the screen.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mc_koopa.h"
#include "mc_bowser.h"
#include "mc_level.h"
#include "mc_koopa_data.h"
#include "mc_overlay.h"
#include "mc_audio.h"
#include "mc_smw.h"
#include "../config.h"
#include "../funcs.h"
#include "../smw_rtl.h"
#include "../variables.h"
#include "../cheats.h"

enum { kRootWalk = 1, kRootHurt = 2, kRootAttack = 3 };
static const double kScale = 3.0;                  // MC stage px per SNES px
static const double kToSmwSpeed = 16.0 / 3.0 * 50.0 / 60.0;  // MC px per tick -> SMW 1/16 px per frame

typedef struct Koopa {
  bool active;
  uint32 seen;                       // McKoopa frame counter of the last frame it was processed (detects new sprites)
  // AS2 variables (names from the script / its onClipEvent(load))
  double x, y, grav, xSpeed, life, moveSpeed, eyeSight, jumpHeight;
  bool onGround, hurt, dead, deathBounce, scored;
  int xscale;
  // timeline state: the root frame and the frame / playing flag of the clip on it (walk, hurt_mc, attack)
  int root;
  int sub_frame;
  bool sub_playing;
  int dead_child;                    // frames of the clip inside hurt_mc's "dead" frame
  int walk_dx;                       // this tick's walking step (-1, 0, +1) * moveSpeed
  double rem_x, rem_y;               // sub-pixel remainder of the frame speeds
} Koopa;

static Koopa g_k[12];
static uint32 g_frame;  // sprite-processing frames (not counting pauses, when SMW does not run its sprites)
static int g_tick_acc;  // 50 Hz clock inside 60 Hz frames
static bool g_tick_now;
static int g_clip_walk = -1, g_clip_hurt, g_clip_dead, g_clip_attack, g_clip_hit;

static int ClipIndex(int id) {
  for (int i = 0; i < kMcKoopaNumClips; i++)
    if (kMcKoopaClips[i].id == id) return i;
  return 0;
}

static int Label(int clip, const char *name) {
  for (int i = 0; i < kMcKoopaNumLabels; i++)
    if (kMcKoopaLabels[i].clip == clip && !strcmp(kMcKoopaLabels[i].name, name)) return kMcKoopaLabels[i].frame;
  return 1;
}

static void Setup(void) {
  if (g_clip_walk >= 0) return;
  g_clip_walk = ClipIndex(149), g_clip_hurt = ClipIndex(154), g_clip_dead = ClipIndex(153);
  g_clip_attack = ClipIndex(162), g_clip_hit = ClipIndex(159);
}

// (King Koopas wins if an ini has both on; the menu never allows that)
bool McKoopa_Enabled(void) {
  if (McLevel_Active())  // the Mario Combat level: its Koopas are MC's (Bowser's bridge: him), whatever the cheats
    return McLevel_McKoopas();
  return g_config.cheat[kCheat_McKoopas] && !g_config.cheat[kCheat_KingKoopas];
}


// (the slot must still hold a Koopa: a fireball turns it into a coin, Yoshi swallows it ...)
bool McKoopa_IsActive(uint8 k) {
  return k < 12 && g_k[k].active && g_k[k].seen + 1 >= g_frame && spr_spriteid[k] <= 7 && spr_current_status[k];
}
bool McKoopa_IsDead(uint8 k) { return McKoopa_IsActive(k) && g_k[k].dead; }

void McKoopa_FrameBegin(void) {
  g_tick_acc += 50;
  g_tick_now = g_tick_acc >= 60;
  if (g_tick_now) g_tick_acc -= 60;
}

void McKoopa_SpritesBegin(void) { g_frame++; }
void McKoopa_OnSpriteInit(uint8 k) { if (k < 12) g_k[k].active = false; }  // a new sprite in this slot

// On screen (with a margin for the art): SMW's own off-screen flags are set by its sprite drawing, which these Koopas
// don't use.
static bool OnScreen(uint8 k) {
  int sx = (int16)GetSprXPos(k) - (int16)mirror_current_layer1_xpos, sy = (int16)GetSprYPos(k) - (int16)mirror_current_layer1_ypos;
  return sx > -48 && sx < 304 && sy > -48 && sy < 272;
}

static const McKoopaFrame *ClipFrame(int clip, int f) {
  const McKoopaClip *c = &kMcKoopaClips[clip];
  if (f < 1) f = 1;
  if (f > c->nframes) f = c->nframes;
  return &kMcKoopaFrames[c->first_frame + f - 1];
}

static int SubClip(const Koopa *kp) {
  return kp->root == kRootWalk ? g_clip_walk : kp->root == kRootHurt ? g_clip_hurt : g_clip_attack;
}

// ---- timeline (gotoAndStop / gotoAndPlay on the root and on the clip it shows) ----
static void RootGoto(Koopa *kp, int f) {
  if (kp->root == f) return;  // same frame: the clip on it keeps running
  kp->root = f;
  kp->sub_frame = 1;          // a new instance of that frame's clip, playing from frame 1
  kp->sub_playing = true;
  kp->dead_child = 0;
}

static void HurtGoto(Koopa *kp, const char *label, bool play) {
  kp->sub_frame = Label(g_clip_hurt, label);
  kp->sub_playing = play;
}

// ---- boxes (level px), from the baked frame bounds ----
typedef struct Box { double x0, y0, x1, y1; } Box;

static Box Xform(const float *m, const float *b, const Koopa *kp) {  // clip bounds b through m, at the Koopa
  double xs[4] = { b[0], b[2], b[0], b[2] }, ys[4] = { b[1], b[1], b[3], b[3] };
  Box r = { 1e9, 1e9, -1e9, -1e9 };
  for (int i = 0; i < 4; i++) {
    double X = m[0] * xs[i] + m[2] * ys[i] + m[4], Y = m[1] * xs[i] + m[3] * ys[i] + m[5];
    X *= kp->xscale / 100.0;
    r.x0 = fmin(r.x0, X), r.x1 = fmax(r.x1, X), r.y0 = fmin(r.y0, Y), r.y1 = fmax(r.y1, Y);
  }
  r.x0 = (kp->x + r.x0) / kScale, r.x1 = (kp->x + r.x1) / kScale;
  r.y0 = (kp->y + r.y0) / kScale, r.y1 = (kp->y + r.y1) / kScale;
  return r;
}

static const McKoopaItem *RootItem(const Koopa *kp) {  // the clip placed on the current root frame
  const McKoopaFrame *rf = ClipFrame(0, kp->root);
  for (int i = 0; i < rf->nitems; i++)
    if (kMcKoopaItems[rf->first_item + i].kind == 1) return &kMcKoopaItems[rf->first_item + i];
  return NULL;
}

static Box BodyBox(const Koopa *kp) {  // this.getBounds() (the hit boxes left out)
  const McKoopaItem *it = RootItem(kp);
  Box none = { kp->x / kScale - 6, kp->y / kScale - 24, kp->x / kScale + 6, kp->y / kScale };
  if (!it) return none;
  const McKoopaFrame *f = ClipFrame(SubClip(kp), kp->sub_frame);
  if (f->bounds[2] <= f->bounds[0]) return none;
  float b[4] = { f->bounds[0], f->bounds[1], f->bounds[2], f->bounds[3] };
  // compose root item matrix with the bounds
  return Xform(it->m, b, kp);
}

static bool AttackBox(const Koopa *kp, int which /* 4 hit1, 5 hit2 */, Box *out) {  // this.attack.hitN, if placed
  if (kp->root != kRootAttack) return false;
  const McKoopaItem *root = RootItem(kp);
  const McKoopaFrame *f = ClipFrame(g_clip_attack, kp->sub_frame);
  for (int i = 0; i < f->nitems; i++) {
    const McKoopaItem *it = &kMcKoopaItems[f->first_item + i];
    if (it->name != which) continue;
    const float *hb = ClipFrame(g_clip_hit, 1)->bounds;
    // bounds through the hit clip's matrix, then the attack clip's matrix on the root
    double xs[4] = { hb[0], hb[2], hb[0], hb[2] }, ys[4] = { hb[1], hb[1], hb[3], hb[3] };
    float bb[4] = { 1e9f, 1e9f, -1e9f, -1e9f };
    for (int c = 0; c < 4; c++) {
      float X = (float)(it->m[0] * xs[c] + it->m[2] * ys[c] + it->m[4]);
      float Y = (float)(it->m[1] * xs[c] + it->m[3] * ys[c] + it->m[5]);
      bb[0] = fminf(bb[0], X), bb[2] = fmaxf(bb[2], X), bb[1] = fminf(bb[1], Y), bb[3] = fmaxf(bb[3], Y);
    }
    *out = Xform(root->m, bb, kp);
    return true;
  }
  return false;
}

static bool Overlap(Box a, double x0, double y0, double x1, double y1) {
  return a.x0 < x1 && x0 < a.x1 && a.y0 < y1 && y0 < a.y1;
}

// `_root.ground.hitTest(x, y, true)` for the script's own probes (ledge / wall ahead), in stage px
static bool Ground(double sx, double sy) {
  int t = McSmw_LevelMap16At((int)floor(sx / kScale), (int)floor(sy / kScale));
  return t < 0 ? false : (t >= 0x100 && t < 0x1D8);
}

// The ledge probe `hitTest(x +- 20, y + 100)`: MC's ground is one filled shape, while SMW only puts solid tiles on the
// surface (the dirt under a grass top is a page-0 fill tile). So: is there a floor anywhere from the feet down to
// that point?
static bool GroundBelow(double sx, double sy, double depth) {
  for (double d = 0; d <= depth; d += 8)
    if (Ground(sx, sy + d)) return true;
  return false;
}

static void InitKoopa(uint8 k) {
  Koopa *kp = &g_k[k];
  memset(kp, 0, sizeof(*kp));
  kp->active = true;
  // onClipEvent(load) of every Koopa on MC's stage: life 10, moveSpeed 1.5, eyeSight 500, jumpHeight 8
  kp->life = 10, kp->moveSpeed = 1.5, kp->eyeSight = 500, kp->jumpHeight = 8;
  kp->x = ((int16)GetSprXPos(k) + 8) * kScale;
  kp->y = ((int16)GetSprYPos(k) + 16) * kScale;
  kp->xscale = -100, kp->deathBounce = true;
  kp->root = kRootWalk;
  kp->sub_frame = 1;
  kp->sub_playing = true;
  // SMW's cape and Yoshi treat it as an enemy that can't be turned into a shell
  spr_property_bits1656[k] &= ~0x10;
}

static void KoopaDie(Koopa *kp) {  // life <= 0
  kp->dead = true;
  if (kp->root == kRootWalk) {
    RootGoto(kp, kRootHurt);
    HurtGoto(kp, "dead", false);
  }
}

// One MC tick of the enemy script (DefineSprite_163 onEnterFrame), minus the hit tests on the hero's attack boxes
// (McKoopa_Punch) and the ground push-outs (SMW collision).
static void KoopaTick(uint8 k, Koopa *kp, const McHeroInfo *hero) {
  kp->grav -= 0.5;
  // (this._y -= grav: SMW moves the sprite)
  double xdist = hero->feet_x * kScale - kp->x, ydist = hero->feet_y * kScale - kp->y;
  if (xdist > 0) kp->xscale = 100;
  if (xdist < 0) kp->xscale = -100;
  Box body = BodyBox(kp);
  bool touching = Overlap(body, hero->x0, hero->y0, hero->x1, hero->y1);
  kp->walk_dx = 0;
  if (!kp->dead && !kp->hurt && !touching && ydist < kp->eyeSight && -ydist > -kp->eyeSight) {
    if (xdist > -kp->eyeSight && kp->xscale == -100) kp->walk_dx = -1;
    if (xdist < kp->eyeSight && kp->xscale == 100) kp->walk_dx = 1;
  }
  if (kp->xSpeed > 0) {
    kp->xSpeed -= kp->onGround ? 0.5 : 0.05;
    if (kp->xSpeed < 0) kp->xSpeed = 0;
  } else if (kp->xSpeed < 0) {
    kp->xSpeed += kp->onGround ? 0.5 : 0.05;
    if (kp->xSpeed > 0) kp->xSpeed = 0;
  }
  if (!kp->dead && !kp->hurt && kp->onGround) {
    // (both wall checks test _xscale == 100 in the original)
    if (Ground(kp->x + 21, kp->y - 3) && kp->xscale == 100) kp->grav = kp->jumpHeight;
    if (Ground(kp->x - 21, kp->y - 3) && kp->xscale == 100) kp->grav = kp->jumpHeight;
    if (!GroundBelow(kp->x + 20, kp->y, 100) && kp->xscale == 100) { kp->grav = kp->jumpHeight * 2; kp->xSpeed = 4; }
    if (!GroundBelow(kp->x - 20, kp->y, 100) && kp->xscale == -100) { kp->grav = kp->jumpHeight * 2; kp->xSpeed = -4; }
  }
  kp->hurt = kp->root == kRootHurt;
  if (!kp->dead && kp->hurt) {
    if ((spr_table1588[k] & 3) && kp->xSpeed) {  // knocked into a wall: bounce off it
      kp->xSpeed = (kp->xSpeed + (kp->xSpeed > 0 ? 1 : -1)) * -1;
      HurtGoto(kp, "hurt1", true);
    }
    if (kp->grav < 0 && kp->onGround && kp->sub_frame >= 69 && kp->sub_frame <= 82) {
      kp->grav = kp->grav / 2 * -1;
      HurtGoto(kp, "hurt6", true);
    }
  }
  if (hero->alive) {
    if (kp->onGround && kp->root == kRootWalk && touching && !kp->dead) {
      RootGoto(kp, kRootAttack);
      kp->xSpeed = 0;
    }
    Box hb;
    int dir = kp->xscale == 100 ? 1 : -1;
    if (AttackBox(kp, 4, &hb) && Overlap(hb, hero->x0, hero->y0, hero->x1, hero->y1)) {
      spr_current_slotid = k;
      McSmw_SetNextHitHeavy(false);
      McSmw_EnemyHurtsPlayer();  // hero.xSpeed = +-6, hurt, life -= 0.5 (SMW Mario: a power-down)
      (void)dir;
    }
    if (AttackBox(kp, 5, &hb) && Overlap(hb, hero->x0, hero->y0, hero->x1, hero->y1)) {
      kp->grav = 4;
      spr_current_slotid = k;
      McSmw_SetNextHitHeavy(true);
      McSmw_EnemyHurtsPlayer();  // hero.xSpeed = +-10, hero.grav = 10, hurt2
      McSmw_SetNextHitHeavy(false);
    }
  }
  if (kp->life <= 0)  // if(life <= 0) { dead = true; if(_currentframe == 1) { hurt; hurt_mc.gotoAndStop("dead") } }
    KoopaDie(kp);
  if (kp->dead && kp->deathBounce) {
    kp->grav = 7;
    kp->deathBounce = false;
  }
}

static void AdvanceTimeline(Koopa *kp, uint8 k) {
  if (kp->root == kRootHurt && kp->sub_frame == Label(g_clip_hurt, "dead")) {
    kp->dead_child++;
    return;
  }
  // walk: walk.play() on the ground away from the hero, gotoAndStop(1) touching him, gotoAndStop(10) in the air
  if (kp->root == kRootWalk && !kp->dead) {
    if (!kp->onGround) { kp->sub_frame = 10; return; }
    if (kp->walk_dx == 0) { kp->sub_frame = 1; return; }
  }
  if (!kp->sub_playing)
    return;
  int clip = SubClip(kp);
  int nf = kMcKoopaClips[clip].nframes;
  kp->sub_frame = kp->sub_frame >= nf ? 1 : kp->sub_frame + 1;
  const McKoopaFrame *f = ClipFrame(clip, kp->sub_frame);
  if (f->sound && g_config.mc_sounds && OnScreen(k))
    McAudio_Play(f->sound);
  // frame scripts
  if (clip == g_clip_hurt) {
    int sf = kp->sub_frame;
    if (sf == 15 || sf == 32 || sf == 68 || sf == 107) RootGoto(kp, kRootWalk);  // _parent.gotoAndStop(1)
    else if (sf == 82) HurtGoto(kp, "hurt5", true);                              // gotoAndPlay("hurt5")
    else if (sf == Label(g_clip_hurt, "dead")) kp->sub_playing = false;
  } else if (clip == g_clip_attack && kp->sub_frame == 45) {
    RootGoto(kp, kRootWalk);
  }
}

// ---- drawing ----
static void DrawClip(int clip, int frame, int dead_child, const float *m, int xscale, uint16 z, int depth) {
  const McKoopaFrame *f = ClipFrame(clip, frame);
  for (int i = 0; i < f->nitems; i++) {
    const McKoopaItem *it = &kMcKoopaItems[f->first_item + i];
    if (it->name == 4 || it->name == 5) continue;  // hit boxes are invisible
    float c[6] = {  // m * it->m
      m[0] * it->m[0] + m[2] * it->m[1], m[1] * it->m[0] + m[3] * it->m[1],
      m[0] * it->m[2] + m[2] * it->m[3], m[1] * it->m[2] + m[3] * it->m[3],
      m[0] * it->m[4] + m[2] * it->m[5] + m[4], m[1] * it->m[4] + m[3] * it->m[5] + m[5],
    };
    if (it->kind == 0) {
      const McImage *im = &kMcKoopaImages[it->ref];
      // image px -> clip px: x = ox + u / k
      float s[6] = { c[0] / im->k, c[1] / im->k, c[2] / im->k, c[3] / im->k,
                     c[0] * im->ox + c[2] * im->oy + c[4], c[1] * im->ox + c[3] * im->oy + c[5] };
      McOverlay_DrawImage(im, s, it->cx, z);
    } else if (depth < 3) {
      int sub = it->ref == g_clip_dead ? dead_child % kMcKoopaClips[it->ref].nframes + 1 : it->age + 1;
      DrawClip(it->ref, sub, 0, c, xscale, z, depth + 1);
    }
  }
}

static void Draw(uint8 k, Koopa *kp) {
  if (!OnScreen(k)) return;
  const McKoopaItem *root = RootItem(kp);
  if (!root) return;
  float sx = (float)(kp->xscale / 100.0 / kScale), sy = (float)(1.0 / kScale);
  float m[6] = {  // stage px around the Koopa's origin -> screen px, mirrored when facing left
    sx * root->m[0], sy * root->m[1], sx * root->m[2], sy * root->m[3],
    (float)(sx * root->m[4] + kp->x / kScale - (int16)mirror_current_layer1_xpos),
    (float)(sy * root->m[5] + kp->y / kScale - (int16)mirror_current_layer1_ypos),
  };
  uint16 z = McOverlay_SpriteZ((spr_table15f6[k] & 0x20) ? 3 : 2);
  DrawClip(SubClip(kp), kp->sub_frame, kp->dead_child, m, kp->xscale, z, 0);
}

// ---- SMW side: speeds through SMW's collision, contact with the player ----
static void Move(uint8 k, Koopa *kp, bool collide) {
  double vx = kp->walk_dx * kp->moveSpeed + kp->xSpeed;
  double sxs = vx * kToSmwSpeed + kp->rem_x, sys = -kp->grav * kToSmwSpeed + kp->rem_y;
  int ix = (int)lround(sxs), iy = (int)lround(sys);
  ix = ix < -127 ? -127 : ix > 127 ? 127 : ix;
  iy = iy < -127 ? -127 : iy > 112 ? 112 : iy;
  kp->rem_x = sxs - ix, kp->rem_y = sys - iy;
  spr_xspeed[k] = (uint8)(int8)ix;
  spr_yspeed[k] = (uint8)(int8)iy;
  UpdateNormalSpritePosition_X(k);
  UpdateNormalSpritePosition_Y(k);
  if (collide) {
    HandleNormalSpriteLevelCollision(k);
    if (spr_current_status[k] == 5) {  // lava: SMW would sink an SMW Koopa; this one dies MC's way and falls in
      spr_current_status[k] = 8;
      kp->life = 0;
      kp->scored = true;
      KoopaDie(kp);
    }
    uint8 c = spr_table1588[k];
    kp->onGround = (c & 4) != 0;
    if (kp->onGround && kp->grav < 0) kp->grav = 0;      // while(hitTest(x, y)) { grav = 0; y--; }
    if ((c & 8) && kp->grav > 0) kp->grav = -4;           // while(hitTest(x, y - _height)) { y++; grav = -4; }
    if (c & 3) kp->rem_x = 0;                             // pushed back out of the wall
    if (kp->onGround) spr_yspeed[k] = 0;
  } else {
    kp->onGround = false;  // dead: falls through everything, as in MC
  }
  kp->x = ((int16)GetSprXPos(k) + 8) * kScale;
  kp->y = ((int16)GetSprYPos(k) + 16) * kScale;
}

// Hit level reactions (the script's six `if(this.hitTest(_root.hero.attack.hitN))` blocks).
static void TakeHit(Koopa *kp, int level, const McHeroInfo *hero) {
  int s = hero->xscale == 100 ? 1 : -1;
  double hx = hero->xspeed;
  switch (level) {
  case 1: kp->life -= 1; RootGoto(kp, kRootHurt); HurtGoto(kp, "hurt1", true); kp->xSpeed = hx + s * 1;
    if (!kp->onGround) kp->grav -= 2;
    break;
  case 2: kp->life -= 2; RootGoto(kp, kRootHurt); HurtGoto(kp, "hurt1", true); kp->grav = 3; kp->xSpeed = hx + s * 1.5; break;
  case 3: kp->life -= 3; RootGoto(kp, kRootHurt); HurtGoto(kp, "hurt3", true); kp->grav = 4; kp->xSpeed = hx + s * 6; break;
  case 4: kp->life -= 4; kp->grav = 4; RootGoto(kp, kRootHurt); HurtGoto(kp, "hurt4", true); kp->xSpeed = hx + s * 15; break;
  case 5: kp->life -= 1; kp->grav = 20; RootGoto(kp, kRootHurt); HurtGoto(kp, "hurt5", true); kp->xSpeed = hx + s * (rand() % 7); break;
  default:
    if (!kp->onGround) { kp->grav = -20; kp->life -= 3; } else { kp->grav = 7; kp->life -= 1; }
    RootGoto(kp, kRootHurt); HurtGoto(kp, "hurt5", true);
    kp->xSpeed = hx + s * (kp->onGround ? 1 : 5);
    break;
  }
  kp->hurt = true;
}

static void Defeated(uint8 k, Koopa *kp) {  // the hit that brought life to 0: SMW points, once
  KoopaDie(kp);
  if (!kp->scored) {
    kp->scored = true;
    GivePoints(k, 1);  // 200, or the step of a hit chain (McSmw_PointsOverride)
    Cheats_EnemyDefeated(k);
  }
}

bool McKoopa_Punch(uint8 k, int level) {
  if (!McKoopa_IsActive(k) || g_k[k].dead) return false;
  Koopa *kp = &g_k[k];
  McHeroInfo hero;
  McSmw_GetHeroInfo(&hero);
  TakeHit(kp, level, &hero);
  if (Cheats_SuperPunch()) kp->life = 0;
  if (kp->life <= 0) Defeated(k, kp);
  return true;
}

// SMW Mario's cape (or a block bounced into it) hits it as an MC hit, not SMW's one-swing knock-out, with a moment
// of invincibility after (kMcCapeInvincible): a spinning cape doesn't defeat it in a blink.
bool McKoopa_CapeHit(uint8 k, uint8 r14) {
  if (!McKoopa_Enabled() || spr_spriteid[k] > 7 || !g_k[k].active) return false;
  Koopa *kp = &g_k[k];
  if (!kp->dead) {
    McHeroInfo hero;
    McSmw_GetHeroInfo(&hero);
    TakeHit(kp, r14 == 53 ? 4 : 2, &hero);
    SpawnContactEffectFromSide(k);
    io_sound_ch1 = 3;
    spr_decrementing_table154c[k] = kMcCapeInvincible;
    if (kp->life <= 0) Defeated(k, kp);
  }
  return true;
}

static void PlayerContact(uint8 k, Koopa *kp, const McHeroInfo *hero) {
  if (kp->dead || player_current_state || spr_decrementing_table154c[k]) return;
  Box body = BodyBox(kp);
  if (!Overlap(body, hero->x0, hero->y0, hero->x1, hero->y1)) return;
  if (timer_star_power || player_sliding_on_ground) {  // star / sliding down a slope: knocked out as SMW does
    spr_current_status[k] = 2;
    spr_yspeed[k] = (uint8)-48;
    GivePoints(k, 1);
    io_sound_ch1 = 3;
    kp->scored = true;  // (the coins cheat sees the knock-out itself)
    return;  // McKoopa_Dead shows Mario Combat's tumble while it falls
  }
  bool from_above = hero->falling && hero->y1 < body.y0 + (body.y1 - body.y0) * 0.5;
  if (from_above && (player_spin_jump_flag || player_riding_yoshi_flag)) {
    // SMW's spin jump / Yoshi stomp (CheckPlayerToNormalSpriteColl_01AA42): gone in a puff of smoke
    SpawnContactEffectFromAbove(k);
    player_yspeed = (uint8)-8;
    if (player_riding_yoshi_flag)
      BoostMarioSpeed();
    SprStatus02_Dead_SetNorSprStatus04(k);
    SpawnSpinJumpStars(k);
    CheckPlayerToNormalSpriteColl_01AB46(k);
    io_sound_ch1 = 8;
    kp->dead = kp->scored = true;  // (the coins cheat sees the smoke puff itself)
    return;
  }
  // Stomp: falling onto the upper part of the Koopa counts as a hit (level 2), and Mario bounces like on any enemy.
  if (from_above) {
    TakeHit(kp, 2, hero);
    spr_decrementing_table154c[k] = 8;
    CheckPlayerToNormalSpriteColl_01AB46(k);  // SMW's stomp sound, points and stomp chain
    BoostMarioSpeed();
    if (kp->life <= 0 && !kp->scored) { KoopaDie(kp); kp->scored = true; Cheats_EnemyDefeated(k); }
  }
}

bool McKoopa_Main(uint8 k) {
  if (!McKoopa_Enabled() || spr_spriteid[k] > 7) return false;
  Setup();
  Koopa *kp = &g_k[k];
  if (!kp->active || kp->seen + 1 < g_frame)  // a new Koopa in this slot (or one not seen for a while)
    InitKoopa(k);
  kp->seen = g_frame;
  if (!flag_sprites_locked) {
    McHeroInfo hero;
    McSmw_GetHeroInfo(&hero);
    CheckNormalSpriteToNormalSpriteCollision(k);  // kicked shells, other enemies
    if (spr_current_status[k] != 8) { kp->active = false; return true; }
    if (g_tick_now) {
      KoopaTick(k, kp, &hero);
      AdvanceTimeline(kp, k);
    }
    Move(k, kp, !kp->dead);
    PlayerContact(k, kp, &hero);
    // SMW's drawing (GetDrawInfo) sets the off-screen flags SubOffscreen reads; this Koopa is drawn by the overlay
    spr_xoffscreen_flag[k] = (uint16)(GetSprXPos(k) - mirror_current_layer1_xpos) >= 256;
    spr_yoffscreen_flag[k] = (uint16)(GetSprYPos(k) + 16 - mirror_current_layer1_ypos) >= 256;
    SubOffscreen_Bank01_Entry1(k);
    if (kp->dead && (int16)GetSprYPos(k) > (int16)mirror_current_layer1_ypos + 240) {
      spr_current_status[k] = 0;  // fell out of the level
      kp->active = false;
      return true;
    }
  }
  if (spr_current_status[k])
    Draw(k, kp);
  return true;
}

// Knocked out by SMW (a shell, a fireball, the cape, a star): MC's tumbling hurt animation while SMW's death fall
// runs.
bool McKoopa_Dead(uint8 k) {
  if (!McKoopa_Enabled() || spr_spriteid[k] > 7 || !g_k[k].active) return false;
  Koopa *kp = &g_k[k];
  kp->seen = g_frame;
  if (!kp->dead) {
    kp->dead = true;
    RootGoto(kp, kRootHurt);
    HurtGoto(kp, "hurt5", true);
  }
  if (!flag_sprites_locked) {
    HandleNormalSpriteGravity(k);
    if (g_tick_now) AdvanceTimeline(kp, k);
    kp->x = ((int16)GetSprXPos(k) + 8) * kScale;
    kp->y = ((int16)GetSprYPos(k) + 16) * kScale;
  }
  spr_xoffscreen_flag[k] = (uint16)(GetSprXPos(k) - mirror_current_layer1_xpos) >= 256;
  spr_yoffscreen_flag[k] = (uint16)(GetSprYPos(k) + 16 - mirror_current_layer1_ypos) >= 256;
  SubOffscreen_Bank01_Entry1(k);
  if (spr_current_status[k]) Draw(k, kp);
  return true;
}

int McKoopa_DebugInfo(uint8 k, char *buf, int n) {
  if (!McKoopa_IsActive(k)) return 0;
  const Koopa *kp = &g_k[k];
  Box b = BodyBox(kp);
  McHeroInfo h;
  McSmw_GetHeroInfo(&h);
  return snprintf(buf, n, " mc-koopa life=%.1f root=%d sub=%d dead=%d grav=%.1f xs=%.1f body=%.0f,%.0f-%.0f,%.0f hero=%.0f,%.0f-%.0f,%.0f",
                  kp->life, kp->root, kp->sub_frame, kp->dead, kp->grav, kp->xSpeed, b.x0, b.y0, b.x1, b.y1, h.x0, h.y0,
                  h.x1, h.y1);
}
