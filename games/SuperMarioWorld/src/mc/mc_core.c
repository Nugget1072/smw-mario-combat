// Mario Combat hero emulation. See mc_core.h and notes/DATA_NOTES.md.
//
// The timeline player follows AVM1 ordering (as in Flash Player / Ruffle):
//  * every tick, clips run newest-first: each queues its onEnterFrame, then advances its playhead
//    (applying PlaceObject/RemoveObject and queueing the frame's DoAction);
//  * afterwards the action queue runs in order;
//  * a newly placed clip runs its first frame immediately;
//  * gotoAndPlay/gotoAndStop jump immediately (_currentframe updates at once) and queue the target
//    frame's DoAction; on a goto, a child survives if the same placement of the same character
//    exists at that depth in the target frame.
// The hero scripts below are ports of DefineSprite_440/frame_1/DoAction.as and
// DefineSprite_432/frame_1/DoAction.as; statement order is kept as in the original.
#include <string.h>
#include <math.h>
#include "mc_core.h"
#include "mc_data.h"

enum { Q_ENTERFRAME = 1, Q_FRAMESCRIPT = 2 };
enum { kSpriteHero = 440, kSpriteAttack = 432, kSpriteHurt = 439, kSpriteBound = 165, kSpriteHitBox = 159,
       kSpriteHitEffect = 259 };

#define KEY(k) ((h->keys & (k)) != 0)

static const McSpriteDef *SpriteDef(const McObj *o) { return &kMcSprites[o->sprite]; }

static int CharType(uint16 ch) { return ch <= kMcMaxCharId ? kMcChars[ch].type : 0; }

static int FindLabel(uint16 sprite_id, const char *label) {
  for (int i = 0; i < kMcNumLabels; i++)
    if (kMcLabels[i].sprite == sprite_id && strcmp(kMcLabels[i].name, label) == 0)
      return kMcLabels[i].frame;
  return 0;
}

static int NameIndex(const char *name) {
  for (int i = 0; i < kMcNumNames; i++)
    if (strcmp(kMcNames[i], name) == 0)
      return i;
  return -1;
}

static const float kIdentity[6] = { 1, 0, 0, 1, 0, 0 };
static const int16 kNoCx[8] = { 256, 256, 256, 256, 0, 0, 0, 0 };

// ---------------------------------------------------------------------------------------------
// Objects
// ---------------------------------------------------------------------------------------------
static McObj *Obj(McHero *h, int i) { return i >= 0 ? &h->objs[i] : NULL; }

static int AllocObj(McHero *h, uint16 ch) {
  for (int i = 0; i < kMcMaxObjs; i++) {
    McObj *o = &h->objs[i];
    if (!o->used) {
      memset(o, 0, sizeof(*o));
      o->used = 1;
      o->ch = ch;
      o->visible = 1;
      o->name = -1;
      o->parent = -1;
      o->is_sprite = CharType(ch) == 2;
      o->sprite = o->is_sprite ? kMcChars[ch].index : -1;
      o->playing = 1;
      o->place_op = -1;
      o->serial = ++h->next_serial;
      memcpy(o->m, kIdentity, sizeof(o->m));
      memcpy(o->cx, kNoCx, sizeof(o->cx));
      return i;
    }
  }
  return -1;
}

static void FreeObj(McHero *h, int i) {
  McObj *o = Obj(h, i);
  if (!o || !o->used)
    return;
  for (int c = 0; c < o->nchild; c++)
    FreeObj(h, o->child[c]);
  o->used = 0;
  o->nchild = 0;
}

static int ChildAt(McHero *h, McObj *o, int depth) {
  for (int c = 0; c < o->nchild; c++)
    if (o->child_depth[c] == depth)
      return o->child[c];
  return -1;
}

static void RemoveChildAt(McHero *h, McObj *o, int depth) {
  for (int c = 0; c < o->nchild; c++) {
    if (o->child_depth[c] == depth) {
      FreeObj(h, o->child[c]);
      memmove(&o->child[c], &o->child[c + 1], (o->nchild - c - 1) * sizeof(o->child[0]));
      memmove(&o->child_depth[c], &o->child_depth[c + 1], (o->nchild - c - 1) * sizeof(o->child_depth[0]));
      o->nchild--;
      return;
    }
  }
}

static void InsertChild(McHero *h, McObj *o, int depth, int ci) {
  if (o->nchild >= kMcMaxChildren)
    return;
  int c = 0;
  while (c < o->nchild && o->child_depth[c] < depth)
    c++;
  memmove(&o->child[c + 1], &o->child[c], (o->nchild - c) * sizeof(o->child[0]));
  memmove(&o->child_depth[c + 1], &o->child_depth[c], (o->nchild - c) * sizeof(o->child_depth[0]));
  o->child[c] = ci;
  o->child_depth[c] = depth;
  o->nchild++;
  h->objs[ci].parent = (int16)(o - h->objs);
}

static int ChildByName(McHero *h, McObj *o, const char *name) {
  int n = NameIndex(name);
  if (!o || n < 0)
    return -1;
  for (int c = 0; c < o->nchild; c++)
    if (h->objs[o->child[c]].name == n)
      return o->child[c];
  return -1;
}

static void Enqueue(McHero *h, int obj, int kind, int frame) {
  if (h->nqueue < kMcMaxQueue) {
    h->queue[h->nqueue].obj = obj;
    h->queue[h->nqueue].kind = kind;
    h->queue[h->nqueue].frame = frame;
    h->nqueue++;
  }
}

// Which frames of which sprites carry a DoAction (all of them are ported in RunFrameScript).
static bool HasFrameScript(uint16 sprite_id, int frame) {
  switch (sprite_id) {
  case kSpriteHero:
    return frame == 1 || frame == 2 || frame == 7 || frame == 8 || frame == 9 || frame == 10 || frame == 16 ||
           frame == 17 || frame == 18 || frame == 19;
  case kSpriteAttack: {
    static const uint16 kFrames[] = { 1, 4, 26, 27, 30, 48, 53, 75, 78, 99, 102, 111, 132, 134, 146, 177, 182, 205, 208,
                                      219, 223, 247, 248, 262, 263, 277, 278, 292, 293, 307, 315, 339, 340, 374, 410 };
    for (size_t i = 0; i < sizeof(kFrames) / sizeof(kFrames[0]); i++)
      if (kFrames[i] == frame)
        return true;
    return false;
  }
  case kSpriteHurt: return frame == 12 || frame == 38;
  case kSpriteBound: case kSpriteHitBox: case kSpriteHitEffect: return frame == 1;
  }
  return false;
}

static void InstantiateFirstFrame(McHero *h, int oi);

// Apply the display-list ops of `frame` (a normal playhead advance into that frame).
static void ApplyFrameOps(McHero *h, int oi, int frame) {
  McObj *o = Obj(h, oi);
  const McSpriteDef *sd = SpriteDef(o);
  int start = kMcFrameStart[sd->first_frame + frame - 1], end = kMcFrameStart[sd->first_frame + frame];
  for (int i = start; i < end; i++) {
    const McOp *op = &kMcOps[i];
    if (op->kind == MC_OP_REMOVE) {
      RemoveChildAt(h, o, op->depth);
    } else if (op->kind == MC_OP_SOUND) {
      if (h->nsounds < kMcMaxSounds)
        h->sounds[h->nsounds++] = op->ch;
    } else if (op->kind == MC_OP_PLACE) {
      int ci = ChildAt(h, o, op->depth);
      if (!(op->flags & MC_PF_MOVE) || (op->flags & MC_PF_CHAR)) {
        // new placement, or character replacement at a depth (keeps old matrix/cxform/name if not given)
        float m[6]; int16 cx[8]; int16 name = -1; uint16 ratio = 0;
        McObj *old = Obj(h, ci);
        memcpy(m, old && (op->flags & MC_PF_MOVE) ? old->m : kIdentity, sizeof(m));
        memcpy(cx, old && (op->flags & MC_PF_MOVE) ? old->cx : kNoCx, sizeof(cx));
        if (old && (op->flags & MC_PF_MOVE)) name = old->name, ratio = old->ratio;
        if (ci >= 0)
          RemoveChildAt(h, o, op->depth);
        int ni = AllocObj(h, op->ch);
        if (ni < 0)
          continue;
        McObj *n = &h->objs[ni];
        memcpy(n->m, (op->flags & MC_PF_MATRIX) ? op->m : m, sizeof(n->m));
        memcpy(n->cx, (op->flags & MC_PF_CXFORM) ? op->cx : cx, sizeof(n->cx));
        n->name = (op->flags & MC_PF_NAME) ? op->name : name;
        n->ratio = (op->flags & MC_PF_RATIO) ? op->ratio : ratio;
        n->place_op = (int16)i;
        InsertChild(h, Obj(h, oi), op->depth, ni);
        o = Obj(h, oi);
        if (n->is_sprite)
          InstantiateFirstFrame(h, ni);
      } else if (ci >= 0) {
        McObj *c = &h->objs[ci];
        if (op->flags & MC_PF_MATRIX) memcpy(c->m, op->m, sizeof(c->m));
        if (op->flags & MC_PF_CXFORM) memcpy(c->cx, op->cx, sizeof(c->cx));
        if (op->flags & MC_PF_RATIO) c->ratio = op->ratio;
        if (op->flags & MC_PF_NAME) c->name = op->name;
      }
    }
  }
  o = Obj(h, oi);
  o->frame = frame;
  if (HasFrameScript(kMcSprites[o->sprite].id, frame))
    Enqueue(h, oi, Q_FRAMESCRIPT, frame);
}

static void InstantiateFirstFrame(McHero *h, int oi) {
  McObj *o = Obj(h, oi);
  o->initialized = 1;
  ApplyFrameOps(h, oi, 1);
}

// The display list a sprite has on `target` frame when played from frame 1.
typedef struct TargetEntry { uint16 depth, ch; int op; float m[6]; int16 cx[8]; int16 name; uint16 ratio; } TargetEntry;

static int BuildTargetList(const McObj *o, int target, TargetEntry *out, int max) {
  const McSpriteDef *sd = &kMcSprites[o->sprite];
  int n = 0;
  for (int f = 1; f <= target; f++) {
    int start = kMcFrameStart[sd->first_frame + f - 1], end = kMcFrameStart[sd->first_frame + f];
    for (int i = start; i < end; i++) {
      const McOp *op = &kMcOps[i];
      int k = 0;
      while (k < n && out[k].depth != op->depth)
        k++;
      if (op->kind == MC_OP_REMOVE) {
        if (k < n)
          memmove(&out[k], &out[k + 1], (n - k - 1) * sizeof(out[0])), n--;
      } else if (op->kind == MC_OP_PLACE) {
        if (!(op->flags & MC_PF_MOVE) || (op->flags & MC_PF_CHAR)) {
          TargetEntry e;
          bool keep = (op->flags & MC_PF_MOVE) && k < n;
          e.depth = op->depth;
          e.ch = op->ch;
          e.op = i;
          memcpy(e.m, (op->flags & MC_PF_MATRIX) ? op->m : keep ? out[k].m : kIdentity, sizeof(e.m));
          memcpy(e.cx, (op->flags & MC_PF_CXFORM) ? op->cx : keep ? out[k].cx : kNoCx, sizeof(e.cx));
          e.name = (op->flags & MC_PF_NAME) ? op->name : keep ? out[k].name : -1;
          e.ratio = (op->flags & MC_PF_RATIO) ? op->ratio : keep ? out[k].ratio : 0;
          if (k < n)
            out[k] = e;
          else if (n < max)
            out[n++] = e;
        } else if (k < n) {
          if (op->flags & MC_PF_MATRIX) memcpy(out[k].m, op->m, sizeof(out[k].m));
          if (op->flags & MC_PF_CXFORM) memcpy(out[k].cx, op->cx, sizeof(out[k].cx));
          if (op->flags & MC_PF_RATIO) out[k].ratio = op->ratio;
          if (op->flags & MC_PF_NAME) out[k].name = op->name;
        }
      }
    }
  }
  return n;
}

static void GotoFrame(McHero *h, int oi, int target, bool play) {
  McObj *o = Obj(h, oi);
  if (!o || !o->used || !o->is_sprite)
    return;
  int nf = SpriteDef(o)->nframes;
  if (target < 1) target = 1;
  if (target > nf) target = nf;
  o->playing = play;
  if (target == o->frame)
    return;
  TargetEntry tl[16];
  int n = BuildTargetList(o, target, tl, 16);
  // remove children that do not survive
  for (int c = o->nchild - 1; c >= 0; c--) {
    int ci = o->child[c];
    int depth = o->child_depth[c];
    bool keep = false;
    for (int k = 0; k < n; k++)
      if (tl[k].depth == depth && tl[k].ch == h->objs[ci].ch && tl[k].op == h->objs[ci].place_op)
        keep = true;
    if (!keep)
      RemoveChildAt(h, o, depth);
  }
  // update survivors / create the rest
  for (int k = 0; k < n; k++) {
    int ci = ChildAt(h, o, tl[k].depth);
    if (ci < 0) {
      ci = AllocObj(h, tl[k].ch);
      if (ci < 0)
        continue;
      InsertChild(h, Obj(h, oi), tl[k].depth, ci);
      McObj *c = &h->objs[ci];
      c->place_op = (int16)tl[k].op;
      memcpy(c->m, tl[k].m, sizeof(c->m));
      memcpy(c->cx, tl[k].cx, sizeof(c->cx));
      c->name = tl[k].name;
      c->ratio = tl[k].ratio;
      if (c->is_sprite)
        InstantiateFirstFrame(h, ci);
    } else {
      McObj *c = &h->objs[ci];
      memcpy(c->m, tl[k].m, sizeof(c->m));
      memcpy(c->cx, tl[k].cx, sizeof(c->cx));
      c->name = tl[k].name;
      c->ratio = tl[k].ratio;
    }
  }
  o = Obj(h, oi);
  o->frame = target;
  // Event sounds (StartSound) on the target frame play on a goto too; every combo label frame has one.
  const McSpriteDef *sd = SpriteDef(o);
  for (int i = kMcFrameStart[sd->first_frame + target - 1]; i < kMcFrameStart[sd->first_frame + target]; i++)
    if (kMcOps[i].kind == MC_OP_SOUND && h->nsounds < kMcMaxSounds)
      h->sounds[h->nsounds++] = kMcOps[i].ch;
  if (HasFrameScript(kMcSprites[o->sprite].id, target))
    Enqueue(h, oi, Q_FRAMESCRIPT, target);
}

static void GotoLabel(McHero *h, int oi, const char *label, bool play) {
  McObj *o = Obj(h, oi);
  if (o && o->used && o->is_sprite)
    GotoFrame(h, oi, FindLabel(kMcSprites[o->sprite].id, label), play);
}

// One playhead step (the timeline part of a tick).
static void AdvanceClip(McHero *h, int oi) {
  McObj *o = Obj(h, oi);
  if (o->has_enter_frame)
    Enqueue(h, oi, Q_ENTERFRAME, 0);
  if (!o->playing)
    return;
  int nf = SpriteDef(o)->nframes;
  if (nf <= 1)
    return;
  if (o->frame >= nf) {
    GotoFrame(h, oi, 1, true);  // looping back to frame 1 rebuilds the display list like a goto
  } else {
    ApplyFrameOps(h, oi, o->frame + 1);
  }
}

// ---------------------------------------------------------------------------------------------
// Hero helpers (the AS2 the scripts call)
// ---------------------------------------------------------------------------------------------
static int HeroFrame(McHero *h) { return h->objs[h->root].frame; }
static int AttackObj(McHero *h) { return ChildByName(h, Obj(h, h->root), "attack"); }
static int HurtObj(McHero *h) { return ChildByName(h, Obj(h, h->root), "hurt_mc"); }
// `this.attack._currentframe`; undefined (no attack clip) compares false with everything, like NaN in SWF7+.
static double AttackFrameOrNaN(McHero *h) {
  int a = AttackObj(h);
  return a >= 0 ? (double)h->objs[a].frame : NAN;
}

static bool Hit(McHero *h, double x, double y) {
  return h->world->hit && h->world->hit(h->world->ctx, x, y);
}

static void HeroGotoStop(McHero *h, const char *label) { GotoLabel(h, h->root, label, false); }
static void HeroGotoPlay(McHero *h, const char *label) { GotoLabel(h, h->root, label, true); }
static void HeroGotoStopFrame(McHero *h, int f) { GotoFrame(h, h->root, f, false); }

// Flash keeps _x/_y in twips.
static double Twips(double v) { return (double)(int64)(v * 20.0 + (v >= 0 ? 0.5 : -0.5)) / 20.0; }
static void SetX(McHero *h, double v) { v = Twips(v); h->tick_dx += v - h->x; h->x = v; }
static void SetY(McHero *h, double v) { v = Twips(v); h->tick_dy += v - h->y; h->y = v; }

static void FlipToKeys(McHero *h) {
  if (h->xscale == 100 && KEY(MC_KEY_LEFT))
    h->xscale = -100;
  else if (h->xscale == -100 && KEY(MC_KEY_RIGHT))
    h->xscale = 100;
}

// DefineSprite_440 onEnterFrame
static void HeroEnterFrame(McHero *h) {
  const McWorld *w = h->world;
  h->grav -= 0.5;
  SetY(h, h->y - h->grav);
  if (!w->external) {
    h->onGround = Hit(h, h->x, h->y + 2);
    h->onLeftWall = Hit(h, h->x - 22, h->y - 20);
    h->onRightWall = Hit(h, h->x + 22, h->y - 20);
    while (Hit(h, h->x, h->y - 1)) {
      h->grav = 0;
      SetY(h, h->y - 1);
    }
    while (Hit(h, h->x - 20, h->y - 20)) {
      SetX(h, h->x + 1);
      if (h->onGround) h->xSpeed = 0;
      else if (h->fighting) h->xSpeed = 0;
    }
    while (Hit(h, h->x + 20, h->y - 20)) {
      SetX(h, h->x - 1);
      if (h->onGround) h->xSpeed = 0;
      else if (h->fighting) h->xSpeed = 0;
    }
    while (Hit(h, h->x, h->y - 80)) {
      h->grav = -2;
      SetY(h, h->y + 1);
    }
  } else {
    w->resolve(w->ctx, h);
  }
  if (h->activated && h->life > 0) {
    if (!h->walling && !h->fighting && !h->hurt) {
      if (KEY(MC_KEY_LEFT)) {
        if (h->onGround) {
          if (h->xSpeed > -h->maxMoveSpeed) {
            h->xSpeed -= 2;
            h->xscale = -100;
            if (!h->skiding) {
              if (h->xSpeed <= -h->maxMoveSpeed) HeroGotoPlay(h, "run");
              else HeroGotoPlay(h, "walk");
            }
          }
        } else if (h->xSpeed > -h->maxAirMoveSpeed) {
          h->xSpeed -= 0.5;
        }
      }
      if (KEY(MC_KEY_RIGHT)) {
        if (h->onGround) {
          if (h->xSpeed < h->maxMoveSpeed) {
            h->xSpeed += 2;
            h->xscale = 100;
            if (!h->skiding) {
              if (h->xSpeed >= h->maxMoveSpeed) HeroGotoPlay(h, "run");
              else HeroGotoPlay(h, "walk");
            }
          }
        } else if (h->xSpeed < h->maxAirMoveSpeed) {
          h->xSpeed += 0.5;
        }
      }
    }
  }
  if (h->xSpeed > 0) {
    h->xSpeed -= h->onGround ? 1 : 0.25;
    SetX(h, h->x + h->xSpeed);
  }
  if (h->xSpeed < 0) {
    h->xSpeed += h->onGround ? 1 : 0.25;
    SetX(h, h->x + h->xSpeed);
  }
  if (w->use_scroll_bounds) {
    while (h->x - 22 < w->lscroll) {
      SetX(h, h->x + 1);
      h->xSpeed = 0;
    }
    while (h->x + 22 > w->rscroll) {
      SetX(h, h->x - 1);
      h->xSpeed = 0;
    }
  }
  if (HeroFrame(h) == 1) {
    if (h->xSpeed != 0) {
      if (h->stopSliding > 0) h->stopSliding--;
      else if (h->stopSliding == 0) h->xSpeed = 0;
    }
  } else {
    h->stopSliding = 10;
  }
  if (h->activated) {
    if (KEY(MC_KEY_JUMP) && h->onGround && !h->hurt) {
      h->grav = h->jump;
      h->jumpAnimThres = 5;
      if (HeroFrame(h) == 18 && AttackFrameOrNaN(h) >= 340)
        HeroGotoStop(h, "jump");
      h->shortHop = 5;
    }
    if (h->shortHop > 0)
      h->shortHop--;
    if (h->shortHop > 0 && h->shortHop < 5 && !h->onGround && !KEY(MC_KEY_JUMP) && !h->hurt)
      h->grav = 7;
    if (h->jumpAnimThres > 0)
      h->jumpAnimThres--;
    if (h->onGround && HeroFrame(h) == 9) {
      if (h->jumpAnimThres == 0)
        HeroGotoStop(h, "idle");
    }
    if (!h->onGround && !h->hurt && !h->fighting) {
      if (!h->walling)
        HeroGotoStop(h, "jump");
    }
  }
  if (!h->onGround && !h->hurt) {
    if (h->onLeftWall && !w->no_wall_pull) SetX(h, h->x - 5);
    if (h->onRightWall && !w->no_wall_pull) SetX(h, h->x + 5);
    if (h->xSpeed < -3 && h->onLeftWall) {
      h->xSpeed = 0;
      h->grav = 2;
      h->walling = true;
      HeroGotoStop(h, "wall");
      h->xscale = 100;
    }
    if (h->xSpeed > 3 && h->onRightWall) {
      h->xSpeed = 0;
      h->grav = 2;
      h->walling = true;
      HeroGotoStop(h, "wall");
      h->xscale = -100;
    }
    if (h->walling) {
      if (!h->onLeftWall && !h->onRightWall) {
        HeroGotoStop(h, "jump");
        h->walling = false;
      }
      if (h->onLeftWall) {
        h->grav += 0.25;
        if (!w->no_wall_pull) SetX(h, h->x - 1);
      }
      if (h->onRightWall) {
        h->grav += 0.25;
        if (!w->no_wall_pull) SetX(h, h->x + 1);
      }
      if (KEY(MC_KEY_JUMP)) {
        h->grav = 10;
        if (h->xscale == 100) h->xSpeed = 10;
        if (h->xscale == -100) h->xSpeed = -10;
      }
    }
  }
  if (h->onGround && h->walling)
    HeroGotoStop(h, "idle");
  if (HeroFrame(h) != 17)
    h->walling = false;
  if (h->onGround && !h->fighting && !h->hurt && h->life > 0) {
    if (h->xSpeed > 3 && KEY(MC_KEY_LEFT)) {
      h->skiding = true;
      HeroGotoStop(h, "skid");
    }
    if (h->xSpeed < -3 && KEY(MC_KEY_RIGHT)) {
      h->skiding = true;
      HeroGotoStop(h, "skid");
    }
    if (h->xSpeed == 0) {
      if (h->skiding) {
        h->skiding = false;
        HeroGotoStopFrame(h, 1);
      }
    }
    if (h->xSpeed > 0 && KEY(MC_KEY_RIGHT)) {
      if (h->skiding) {
        h->skiding = false;
        HeroGotoPlay(h, "walk");
      }
    }
    if (h->xSpeed < 0 && KEY(MC_KEY_LEFT)) {
      if (h->skiding) {
        h->skiding = false;
        HeroGotoPlay(h, "walk");
      }
    }
  }
  if (h->activated) {
    h->fighting = HeroFrame(h) == 18;
    if (h->onGround && !h->pressingAttack) {
      if (!h->hurt) {
        if (KEY(MC_KEY_PUNCH) && !KEY(MC_KEY_LEFT) && !KEY(MC_KEY_RIGHT)) {
          if (HeroFrame(h) == 1)
            HeroGotoStop(h, "attack");
        }
        if (!h->fighting) {
          if (KEY(MC_KEY_PUNCH) && (KEY(MC_KEY_RIGHT) || KEY(MC_KEY_LEFT))) {
            HeroGotoStop(h, "attack");
            GotoLabel(h, AttackObj(h), "attack7", true);
          }
        }
        if (KEY(MC_KEY_PUNCH) && KEY(MC_KEY_DOWN)) {
          HeroGotoStop(h, "attack");
          if (AttackFrameOrNaN(h) < 340)
            GotoLabel(h, AttackObj(h), "attack11", true);
        }
      }
    }
    if (!h->onGround && !h->pressingAttack && HeroFrame(h) == 9) {
      if (KEY(MC_KEY_PUNCH)) {
        h->fighting = true;
        HeroGotoStop(h, "attack");
        GotoLabel(h, AttackObj(h), "attack12", true);
      }
    }
    if (h->onGround && HeroFrame(h) == 18) {
      double af = AttackFrameOrNaN(h);
      if (af > 374 && af < 410)
        HeroGotoStopFrame(h, 1);
    }
    double af = AttackFrameOrNaN(h);
    if (af > 374 && af < 410) {
      if (h->xscale == 100 && KEY(MC_KEY_RIGHT)) {
        if (h->xSpeed < h->maxMoveSpeed) h->xSpeed += 0.5;
      }
      if (h->xscale == -100 && KEY(MC_KEY_LEFT)) {
        if (h->xSpeed > -h->maxMoveSpeed) h->xSpeed -= 0.5;
      }
    }
    h->pressingAttack = KEY(MC_KEY_PUNCH);
  }
  if (!h->onGround && h->fighting) {
    if (h->onLeftWall || h->onRightWall) {
      HeroGotoStop(h, "jump");
      h->xSpeed = 0;
    }
    if (w->hit ? (Hit(h, h->x + 30, h->y - 20) || Hit(h, h->x - 30, h->y - 20)) : (h->onLeftWall || h->onRightWall)) {
      HeroGotoStop(h, "jump");
      h->xSpeed = 0;
    }
  }
  h->hurt = HeroFrame(h) == 19;
  if (w->use_dead_zone && h->y > w->dead_zone_y) {
    SetX(h, w->spawn_x);
    SetY(h, w->spawn_y);
    h->grav = 2;
  }
  if (h->life <= 0) {
    h->activated = false;
    HeroGotoStop(h, "hurt");
    int u = HurtObj(h);
    GotoLabel(h, u, h->onGround ? "die2" : "die1", false);
  }
}

// DefineSprite_432 (attack clip) onEnterFrame: the combo state machine.
static void AttackEnterFrame(McHero *h, int ai) {
#define AF() ((int)h->objs[ai].frame)
#define GO(lbl) do { GotoLabel(h, ai, lbl, true); } while (0)
  if (h->pressingAttack != false)
    return;
  if (!h->objs[ai].used) return;
  bool L = KEY(MC_KEY_LEFT), R = KEY(MC_KEY_RIGHT), P = KEY(MC_KEY_PUNCH);
  if (P && !L && !R) {
    if (AF() > 9 && AF() < 23) { GO("attack4"); FlipToKeys(h); }
    if (AF() > 79 && AF() < 88) { GO("attack3"); FlipToKeys(h); }
    if (AF() > 85 && AF() < 94) { GO("attack5"); FlipToKeys(h); }
    if (AF() > 35 && AF() < 44) { GO("attack5"); FlipToKeys(h); }
    if (AF() > 106 && AF() < 132) { GO("attack6"); FlipToKeys(h); }
    if (AF() > 185 && AF() < 191) { GO("attack3"); FlipToKeys(h); }
    if (AF() > 187 && AF() < 203) { GO("attack5"); FlipToKeys(h); }
    if (AF() > 240 && AF() < 247) { GO("attack4"); FlipToKeys(h); }
    if (AF() > 209 && AF() < 219) { GO("attack4"); FlipToKeys(h); }
  }
  if (P && (L || R)) {
    if (AF() > 185 && AF() < 191) { GO("attack8"); FlipToKeys(h); }
    if (AF() > 187 && AF() < 203) { GO("attack9"); FlipToKeys(h); }
    if (AF() > 77 && AF() < 87) { GO("attack10"); FlipToKeys(h); }
    if (AF() > 209 && AF() < 219) { GO("attack105"); FlipToKeys(h); }
  }
  if (P) {
    if (AF() > 234 && AF() < 244) { GO("attack6"); FlipToKeys(h); }
    if (AF() > 249 && AF() < 259) GO("attack102");
    if (AF() > 264 && AF() < 274) GO("attack103");
    if (AF() > 279 && AF() < 288) GO("attack104");
    if (AF() > 294 && AF() < 304) GO("attack105");
    if (AF() > 7 && AF() < 12) { GO("attack2"); FlipToKeys(h); }
    if (AF() > 30 && AF() < 38) { GO("attack3"); FlipToKeys(h); }
  }
#undef AF
#undef GO
}

static void Push(McHero *h, double v) {  // `if(_parent._xscale == 100) _parent.xSpeed += v; if(== -100) -= v`
  if (h->xscale == 100) h->xSpeed += v;
  if (h->xscale == -100) h->xSpeed -= v;
}
static void Set7(McHero *h) {
  if (h->xscale == 100) h->xSpeed = 7;
  if (h->xscale == -100) h->xSpeed = -7;
}

static void RunFrameScript(McHero *h, int oi, int frame) {
  McObj *o = Obj(h, oi);
  switch (o->ch) {
  case kSpriteHero:
    switch (frame) {
    case 1: o->has_enter_frame = 1; break;  // this.onEnterFrame = function() {...}
    case 2: case 10: o->playing = 1; break;
    case 7: case 16: HeroGotoStopFrame(h, 1); break;
    case 8: case 9: case 17: case 19: o->playing = 0; break;
    case 18: h->fighting = true; o->playing = 0; break;
    }
    break;
  case kSpriteAttack:
    switch (frame) {
    case 1: o->has_enter_frame = 1; break;
    case 4: case 30: case 78: case 102: case 111: case 208: Push(h, 5); break;
    case 53: case 248: case 315: Push(h, 7); break;
    case 146: case 182: case 223: Push(h, 10); break;
    case 134: Push(h, -5); break;
    case 263: case 278: case 293: Set7(h); break;
    case 27: FlipToKeys(h); break;
    case 340:
      if (h->xSpeed < 0) h->xSpeed -= 7;
      // `if(xSpeed > 0)` reads the attack clip's own (undefined) xSpeed: never true. Kept as in the original.
      break;
    case 26: case 48: case 75: case 99: case 132: case 177: case 205: case 219: case 247: case 262: case 277:
    case 292: case 307: case 339: case 374:
      HeroGotoStopFrame(h, 1);
      break;
    case 410: HeroGotoStop(h, "jump"); break;
    }
    break;
  case kSpriteHurt:
    if (frame == 12 || frame == 38)
      HeroGotoStopFrame(h, 1);
    break;
  case kSpriteBound:
  case kSpriteHitBox:
    o->visible = 0;
    break;
  case kSpriteHitEffect:
    o->playing = 0;
    break;
  }
}

static void RunQueue(McHero *h) {
  int initial = h->in_tick ? h->nqueue : -1;
  h->in_tick = false;
  for (int qi = 0; qi < h->nqueue; qi++) {
    if (qi == initial && h->on_log_point)
      h->on_log_point(h), initial = -1;
    int oi = h->queue[qi].obj;
    McObj *o = Obj(h, oi);
    if (!o || !o->used)
      continue;
    if (h->queue[qi].kind == Q_ENTERFRAME) {
      if (o->ch == kSpriteHero) HeroEnterFrame(h);
      else if (o->ch == kSpriteAttack) AttackEnterFrame(h, oi);
    } else {
      RunFrameScript(h, oi, h->queue[qi].frame);
    }
  }
  if (initial >= 0 && h->on_log_point)
    h->on_log_point(h);
  h->nqueue = 0;
}

// ---------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------
void McHero_Init(McHero *h, const McWorld *world, double x, double y) {
  memset(h, 0, sizeof(*h));
  h->world = world;
  // onClipEvent(load) of the hero placement (frame_486/PlaceObject2_440_*)
  h->xSpeed = 0;
  h->maxMoveSpeed = 10;
  h->maxAirMoveSpeed = 10;
  h->grav = 0;
  h->jump = 14;
  h->jumpAnimThres = 0;
  h->stopSliding = 10;
  h->shortHop = 0;
  h->pressingAttack = false;
  h->life = 10;
  h->activated = true;  // root frame 485 sets this every frame once the level runs
  h->xscale = 100;
  h->x = Twips(x);
  h->y = Twips(y);
  h->root = AllocObj(h, kSpriteHero);
  InstantiateFirstFrame(h, h->root);
  RunQueue(h);
  // Frame 1 has no stop(): on level load the timeline plays frames 1-7 once and frame 7's gotoAndStop(1)
  // leaves it stopped on idle. Start in that settled state.
  h->objs[h->root].playing = 0;
}

static int CollectClips(McHero *h, int oi, int *out, int n) {
  McObj *o = Obj(h, oi);
  if (!o->is_sprite)
    return n;
  out[n++] = oi;
  for (int c = 0; c < o->nchild; c++)
    n = CollectClips(h, o->child[c], out, n);
  return n;
}

void McHero_Tick(McHero *h, uint8 keys) {
  h->keys = keys;
  h->tick_dx = h->tick_dy = 0;
  h->tick++;
  int list[kMcMaxObjs], n = CollectClips(h, h->root, list, 0);
  // newest first
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && h->objs[list[j]].serial > h->objs[list[j - 1]].serial; j--) {
      int t = list[j]; list[j] = list[j - 1]; list[j - 1] = t;
    }
  uint32 serials[kMcMaxObjs];
  for (int i = 0; i < n; i++)
    serials[i] = h->objs[list[i]].serial;
  for (int i = 0; i < n; i++) {
    McObj *o = &h->objs[list[i]];
    if (o->used && o->serial == serials[i])  // skip clips removed earlier in this pass
      AdvanceClip(h, list[i]);
  }
  h->in_tick = true;
  RunQueue(h);
}

static void MatMul(const float *p, const float *c, float *out) {
  float r[6];
  r[0] = p[0] * c[0] + p[2] * c[1];
  r[1] = p[1] * c[0] + p[3] * c[1];
  r[2] = p[0] * c[2] + p[2] * c[3];
  r[3] = p[1] * c[2] + p[3] * c[3];
  r[4] = p[0] * c[4] + p[2] * c[5] + p[4];
  r[5] = p[1] * c[4] + p[3] * c[5] + p[5];
  memcpy(out, r, sizeof(r));
}

static void CxMul(const int16 *p, const int16 *c, int16 *out) {
  int16 r[8];
  for (int i = 0; i < 4; i++) {
    r[i] = (int16)(p[i] * c[i] / 256);
    r[4 + i] = (int16)(p[i] * c[4 + i] / 256 + p[4 + i]);
  }
  memcpy(out, r, sizeof(r));
}

static void HeroMatrix(McHero *h, float *m) {
  m[0] = h->xscale / 100.0f; m[1] = 0; m[2] = 0; m[3] = 1; m[4] = (float)h->x; m[5] = (float)h->y;
}

static int DrawRec(McHero *h, int oi, const float *pm, const int16 *pcx, McDrawItem *out, int n, int max) {
  McObj *o = Obj(h, oi);
  if (!o->visible)
    return n;
  float m[6]; int16 cx[8];
  MatMul(pm, o->m, m);
  CxMul(pcx, o->cx, cx);
  if (o->is_sprite) {
    for (int c = 0; c < o->nchild; c++)
      n = DrawRec(h, o->child[c], m, cx, out, n, max);
  } else if (CharType(o->ch) == 1 && n < max) {
    const McImage *im = &kMcImages[kMcChars[o->ch].index];
    float s[6] = { 1.0f / im->k, 0, 0, 1.0f / im->k, im->ox, im->oy };
    out[n].image = kMcChars[o->ch].index;
    MatMul(m, s, out[n].m);
    memcpy(out[n].cx, cx, sizeof(cx));
    n++;
  }
  return n;
}

int McHero_GetDrawList(McHero *h, McDrawItem *out, int max) {
  float m[6];
  HeroMatrix(h, m);
  McObj *root = Obj(h, h->root);
  int n = 0;
  for (int c = 0; c < root->nchild; c++)
    n = DrawRec(h, root->child[c], m, kNoCx, out, n, max);
  return n;
}

static const McShapeBounds *ShapeBounds(uint16 id) {
  for (int i = 0; i < kMcNumImages; i++)
    if (kMcShapeBounds[i].id == id)
      return &kMcShapeBounds[i];
  return NULL;
}

// Global AABB of a clip's shapes (MovieClip.getBounds(_root)).
static bool ClipBounds(McHero *h, int oi, const float *pm, double *b) {
  McObj *o = Obj(h, oi);
  float m[6];
  MatMul(pm, o->m, m);
  bool any = false;
  if (o->is_sprite) {
    for (int c = 0; c < o->nchild; c++)
      any |= ClipBounds(h, o->child[c], m, b);
  } else {
    const McShapeBounds *sb = ShapeBounds(o->ch);
    if (sb) {
      float xs[2] = { sb->xmin, sb->xmax }, ys[2] = { sb->ymin, sb->ymax };
      for (int i = 0; i < 4; i++) {
        double X = m[0] * xs[i & 1] + m[2] * ys[i >> 1] + m[4], Y = m[1] * xs[i & 1] + m[3] * ys[i >> 1] + m[5];
        if (X < b[0]) b[0] = X;
        if (Y < b[1]) b[1] = Y;
        if (X > b[2]) b[2] = X;
        if (Y > b[3]) b[3] = Y;
      }
      any = true;
    }
  }
  return any;
}

int McHero_GetHitBoxes(McHero *h, McHitBox *out, int max) {
  int a = AttackObj(h), n = 0;
  if (a < 0)
    return 0;
  float hm[6], am[6];
  HeroMatrix(h, hm);
  MatMul(hm, h->objs[a].m, am);
  McObj *ao = &h->objs[a];
  for (int c = 0; c < ao->nchild && n < max; c++) {
    McObj *ch = &h->objs[ao->child[c]];
    if (ch->name < 0)
      continue;
    const char *nm = kMcNames[ch->name];
    if (nm[0] == 'h' && nm[1] == 'i' && nm[2] == 't' && nm[3] >= '1' && nm[3] <= '6' && nm[4] == 0) {
      double b[4] = { 1e30, 1e30, -1e30, -1e30 };
      if (ClipBounds(h, ao->child[c], am, b)) {
        out[n].level = nm[3] - '0';
        out[n].x0 = b[0]; out[n].y0 = b[1]; out[n].x1 = b[2]; out[n].y1 = b[3];
        n++;
      }
    }
  }
  return n;
}

void McHero_GetBound(McHero *h, double *x0, double *y0, double *x1, double *y1) {
  float hm[6];
  HeroMatrix(h, hm);
  double b[4] = { 1e30, 1e30, -1e30, -1e30 };
  int bi = ChildByName(h, Obj(h, h->root), "bound");
  if (bi < 0 || !ClipBounds(h, bi, hm, b)) {
    b[0] = h->x - 31; b[1] = h->y - 87.5; b[2] = h->x + 31; b[3] = h->y;
  }
  *x0 = b[0]; *y0 = b[1]; *x1 = b[2]; *y1 = b[3];
}

int McHero_HeroFrame(McHero *h) { return HeroFrame(h); }

int McHero_ChildFrame(McHero *h, int depth) {
  int c = ChildAt(h, Obj(h, h->root), depth);
  return c >= 0 && h->objs[c].is_sprite ? h->objs[c].frame : 0;
}

int McHero_AttackFrame(McHero *h) { int a = AttackObj(h); return a >= 0 ? h->objs[a].frame : 0; }
int McHero_HurtFrame(McHero *h) { int u = HurtObj(h); return u >= 0 ? h->objs[u].frame : 0; }

void McHero_TakeHit(McHero *h, int from_dir, bool heavy, double damage) {
  // DefineSprite_163: enemy attack.hit1 / hit2 touching hero.bound
  if (heavy) {
    h->xSpeed = from_dir >= 0 ? 10 : -10;
    h->grav = 10;
    h->hurt = true;
    HeroGotoStop(h, "hurt");
    GotoLabel(h, HurtObj(h), "hurt2", true);
  } else {
    h->xSpeed = from_dir >= 0 ? 6 : -6;
    h->hurt = true;
    HeroGotoStop(h, "hurt");
  }
  h->life -= damage;
  RunQueue(h);
}

void McHero_ShowHitEffect(McHero *h, int level) {
  // DefineSprite_532: hitN -> hero.attack.hitEffect.gotoAndStop(N+1) (hit6 reuses 5), hit1 also replays its spark
  static const uint8 kFrame[7] = { 0, 2, 3, 4, 5, 6, 5 };
  int a = AttackObj(h);
  if (a < 0 || level < 1 || level > 6)
    return;
  int e = ChildByName(h, Obj(h, a), "hitEffect");
  GotoFrame(h, e, kFrame[level], false);
  if (level == 1) {
    int s = ChildByName(h, Obj(h, e), "hit1");
    GotoFrame(h, s, 1, true);
  }
  RunQueue(h);
}

void McHero_GotoLabel(McHero *h, const char *label, bool play) {
  GotoLabel(h, h->root, label, play);
  RunQueue(h);
}

// ---------------------------------------------------------------------------------------------
// Cheat and tool support (not in the original game; the ported scripts above are unchanged)
// ---------------------------------------------------------------------------------------------
// One extra playhead step of the playing attack clip, as its timeline takes it: the next frame's display list and
// frame script (impulses, sounds, the move's end) run; its onEnterFrame (the combo checks) does not.
bool McHero_AttackStep(McHero *h) {
  int a = AttackObj(h);
  if (a < 0 || !h->objs[a].playing || h->objs[a].frame >= SpriteDef(Obj(h, a))->nframes)
    return false;
  ApplyFrameOps(h, a, h->objs[a].frame + 1);
  RunQueue(h);
  return true;
}

// A goto on the attack clip (tools: which frames carry hit boxes).
void McHero_AttackGotoFrame(McHero *h, int frame) {
  int a = AttackObj(h);
  if (a >= 0) {
    GotoFrame(h, a, frame, false);
    RunQueue(h);
  }
}
