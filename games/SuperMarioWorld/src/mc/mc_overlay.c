#include <string.h>
#include <math.h>
#include "mc_overlay.h"

static struct {
  bool active;
  int ymin, ymax;
  uint32 rgb[kMcOverlayH][kMcOverlayW];
  uint8 a[kMcOverlayH][kMcOverlayW];
  uint16 z[kMcOverlayH][kMcOverlayW];
} g_ov;

void McOverlay_BeginFrame(void) {
  if (g_ov.ymax >= g_ov.ymin) {
    for (int y = g_ov.ymin; y <= g_ov.ymax; y++)
      memset(g_ov.a[y], 0, sizeof(g_ov.a[y]));
  }
  g_ov.ymin = kMcOverlayH;
  g_ov.ymax = -1;
  g_ov.active = false;
}

bool McOverlay_Active(void) { return g_ov.active; }

uint16 McOverlay_SpriteZ(int obj_priority) {
  // Same encoding as ppu.c sprites (SPRITE_PRIO_TO_PRIO with an OBJ palette 0-3), and the highest
  // palette byte so the hero wins over other sprites of the same priority like Mario's OAM slots do.
  return (uint16)((((obj_priority & 3) * 4 + 2) * 16 + 6) << 8 | 0xff);
}

static void Put(int x, int y, uint32 argb, uint16 z) {
  if ((unsigned)x >= kMcOverlayW || (unsigned)y >= kMcOverlayH)
    return;
  uint8 a = argb >> 24;
  if (!a)
    return;
  if (g_ov.a[y][x] && a < 255) {  // blend translucent effects over what is already there
    uint32 d = g_ov.rgb[y][x];
    uint32 r = ((argb >> 16 & 255) * a + (d >> 16 & 255) * (255 - a)) / 255;
    uint32 g = ((argb >> 8 & 255) * a + (d >> 8 & 255) * (255 - a)) / 255;
    uint32 b = ((argb & 255) * a + (d & 255) * (255 - a)) / 255;
    g_ov.rgb[y][x] = r << 16 | g << 8 | b;
    if (a > g_ov.a[y][x]) g_ov.a[y][x] = a;
  } else {
    g_ov.rgb[y][x] = argb & 0xffffff;
    g_ov.a[y][x] = a;
  }
  g_ov.z[y][x] = z;
  if (y < g_ov.ymin) g_ov.ymin = y;
  if (y > g_ov.ymax) g_ov.ymax = y;
  g_ov.active = true;
}

static uint32 ApplyCx(uint32 p, const int16 *cx) {
  if (!cx || (cx[0] == 256 && cx[1] == 256 && cx[2] == 256 && cx[3] == 256 && !cx[4] && !cx[5] && !cx[6] && !cx[7]))
    return p;
  int c[4] = { p >> 16 & 255, p >> 8 & 255, p & 255, p >> 24 };
  for (int i = 0; i < 4; i++) {
    int v = c[i] * cx[i] / 256 + cx[4 + i];
    c[i] = v < 0 ? 0 : v > 255 ? 255 : v;
  }
  return (uint32)c[3] << 24 | c[0] << 16 | c[1] << 8 | c[2];
}

static uint32 (*g_recolor)(uint32 argb);

void McOverlay_SetRecolor(uint32 (*fn)(uint32 argb)) { g_recolor = fn; }

static uint32 Colour(uint32 p, const int16 *cx) {
  p = ApplyCx(p, cx);
  return g_recolor ? g_recolor(p) : p;
}

void McOverlay_DrawImage(const McImage *im, const float *m, const int16 *cx, uint16 z) {
  float a = m[0], b = m[1], c = m[2], d = m[3];
  // Pixel art drawn at (almost) unit scale: snap to an exact integer blit so no pixel column is lost.
  bool unit = fabsf(b) < 0.02f && fabsf(c) < 0.02f && fabsf(fabsf(a) - 1) < 0.07f && fabsf(fabsf(d) - 1) < 0.07f;
  if (unit) {
    int sx = a > 0 ? 1 : -1, sy = d > 0 ? 1 : -1;
    // (a scale near 1 drawn at 1: placed so the image's bottom edge and horizontal centre stay where the scaled image
    // would have them: MC Bowser's panting squashes him with an offset that keeps his feet on the ground)
    float ex = m[4] + (a - sx) * im->w * 0.5f, ey = m[5] + (d - sy) * im->h;
    int ox = (int)floorf(ex + 0.5f) - (sx < 0 ? 1 : 0), oy = (int)floorf(ey + 0.5f) - (sy < 0 ? 1 : 0);
    for (int y = 0; y < im->h; y++)
      for (int x = 0; x < im->w; x++) {
        uint32 p = im->px[y * im->w + x];
        if (p >> 24)
          Put(ox + sx * x, oy + sy * y, Colour(p, cx), z);
      }
    return;
  }
  // General affine: inverse-map every screen pixel of the bounding box (nearest neighbour).
  float det = a * d - b * c;
  if (fabsf(det) < 1e-6f)
    return;
  float ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
  float xs[4] = { 0, (float)im->w, 0, (float)im->w }, ys[4] = { 0, 0, (float)im->h, (float)im->h };
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (int i = 0; i < 4; i++) {
    float X = a * xs[i] + c * ys[i] + m[4], Y = b * xs[i] + d * ys[i] + m[5];
    x0 = fminf(x0, X); x1 = fmaxf(x1, X); y0 = fminf(y0, Y); y1 = fmaxf(y1, Y);
  }
  for (int Y = (int)floorf(y0); Y <= (int)ceilf(y1); Y++) {
    for (int X = (int)floorf(x0); X <= (int)ceilf(x1); X++) {
      float px = X + 0.5f - m[4], py = Y + 0.5f - m[5];
      int u = (int)floorf(ia * px + ic * py), v = (int)floorf(ib * px + id * py);
      if ((unsigned)u < im->w && (unsigned)v < im->h) {
        uint32 p = im->px[v * im->w + u];
        if (p >> 24)
          Put(X, Y, Colour(p, cx), z);
      }
    }
  }
}

void McOverlay_DrawRect(int x0, int y0, int x1, int y1, uint32 argb, uint16 z) {
  for (int x = x0; x <= x1; x++) Put(x, y0, argb, z), Put(x, y1, argb, z);
  for (int y = y0; y <= y1; y++) Put(x0, y, argb, z), Put(x1, y, argb, z);
}

// Sprite-vs-sprite order on the SNES is by OAM index (the first sprite found on a line wins), not by priority bits,
// which only decide against the BG layers. So the hero's opaque pixels are put into the PPU's sprite line buffer
// when the sprite scan reaches Mario's own OAM slot: sprites in earlier slots stay in front of him, later ones (the
// Yoshi's House fire, most enemies) go behind, and the PPU then sorts him against the BGs with his priority bits.
static int g_hero_oam = -1;  // sprite number of Mario's first OAM slot, or -1

void McOverlay_SetHeroOamIndex(int sprite) { g_hero_oam = sprite; }
int McOverlay_HeroOamIndex(void) { return g_ov.active ? g_hero_oam : -1; }

bool McOverlay_FillObjLine(int line, uint16 *obj) {
  if (!g_ov.active || line < g_ov.ymin || line > g_ov.ymax || (unsigned)line >= kMcOverlayH)
    return false;
  bool any = false;
  for (int x = 0; x < kMcOverlayW; x++) {
    uint16 z = g_ov.z[line][x];
    if (g_ov.a[line][x] == 255 && z != 0xffff && (obj[x] & 0xff) == 0)
      obj[x] = z, any = true;
  }
  return any;
}

void McOverlay_ApplyLine(int line, uint32 *dst, const uint16 *zbuf, const uint16 *subz, int brightness, bool mirror) {
  if (!g_ov.active || line < g_ov.ymin || line > g_ov.ymax || (unsigned)line >= kMcOverlayH)
    return;
  const uint8 *al = g_ov.a[line];
  for (int x = 0; x < kMcOverlayW; x++) {
    int sx = mirror ? kMcOverlayW - 1 - x : x;  // mirror-mode cheat: the PPU flipped this line's sprites / BG1 / BG2
    uint8 a = al[sx];
    if (!a)
      continue;
    uint16 z = g_ov.z[line][sx];
    // HUD (z 0xffff): always on top. Opaque hero pixels: only where the PPU kept them (they went through its sprite
    // and BG sorting). Translucent effects: over anything of a lower z.
    // (subz: the sprites are only on the sub screen, added onto the main screen by color math: kept there instead)
    if (z != 0xffff && (a == 255 ? zbuf[x] != z && !(subz && subz[x] == z) : z < zbuf[x]))
      continue;
    uint32 s = g_ov.rgb[line][sx];
    uint32 r = (s >> 16 & 255) * brightness / 15, g = (s >> 8 & 255) * brightness / 15, b = (s & 255) * brightness / 15;
    if (a < 255) {
      uint32 d = dst[x];
      r = (r * a + (d >> 16 & 255) * (255 - a)) / 255;
      g = (g * a + (d >> 8 & 255) * (255 - a)) / 255;
      b = (b * a + (d & 255) * (255 - a)) / 255;
    }
    dst[x] = r << 16 | g << 8 | b;
  }
}
