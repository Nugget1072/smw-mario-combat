// Standalone verification harness for mc_core: replays a reference trace recorded from the original
// Mario Combat (Flash Player 32, tools/refcap) against the exact level ground geometry and reports
// every tick where the emulation differs.
// Build: gcc -O2 -o mc_test.exe src/mc/mc_test.c src/mc/mc_core.c src/mc/mc_data.c -I src
// Usage: mc_test <ground.bin> <trace.csv> [lscroll rscroll deadzone_y spawn_x spawn_y] [-v]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mc_core.h"

typedef struct { int key; double x0, y0, x1, y1; } Edge;
static Edge *g_edges;
static int g_nedges, g_maxkey;

static bool GroundHit(void *ctx, double x, double y) {
  // even-odd per fill, ray to +x; half-open in y like the rasterizer
  static int *cnt;
  if (!cnt) cnt = calloc(g_maxkey + 1, sizeof(int));
  memset(cnt, 0, (g_maxkey + 1) * sizeof(int));
  for (int i = 0; i < g_nedges; i++) {
    Edge *e = &g_edges[i];
    double ylo = e->y0 < e->y1 ? e->y0 : e->y1, yhi = e->y0 < e->y1 ? e->y1 : e->y0;
    if (e->y0 == e->y1 || y < ylo || y >= yhi)
      continue;
    double xi = e->x0 + (y - e->y0) / (e->y1 - e->y0) * (e->x1 - e->x0);
    if (xi > x)
      cnt[e->key] ^= 1;
  }
  for (int k = 0; k <= g_maxkey; k++)
    if (cnt[k])
      return true;
  return false;
}

static int ParseKeys(const char *s) {
  int k = 0;
  while (*s) {
    int v = atoi(s);
    if (v == 37) k |= MC_KEY_LEFT;
    if (v == 39) k |= MC_KEY_RIGHT;
    if (v == 38) k |= MC_KEY_JUMP;
    if (v == 40) k |= MC_KEY_DOWN;
    if (v == 65) k |= MC_KEY_PUNCH;
    while (*s && *s != '+') s++;
    if (*s) s++;
  }
  return k;
}

enum { F_TICK, F_X, F_Y, F_XSPEED, F_GRAV, F_FRAME, F_XSCALE, F_ONGROUND, F_ONLW, F_ONRW, F_WALLING, F_FIGHTING,
       F_HURT, F_SKIDING, F_SHORTHOP, F_STOPSLIDING, F_JAT, F_PRESSING, F_LIFE, F_CHILD, F_CHILDFRAME, F_ATTACKFRAME,
       F_HURTFRAME, F_VCAM, F_KEYS, F_COUNT };

static int SplitCsv(char *line, char **f) {
  int n = 0;
  f[n++] = line;
  for (char *p = line; *p; p++) {
    if (*p == ',') { *p = 0; if (n < F_COUNT) f[n++] = p + 1; }
    if (*p == '\n' || *p == '\r') *p = 0;
  }
  return n;
}

static double Num(const char *s) { return (!strcmp(s, "undefined") || !*s) ? 0 : !strcmp(s, "true") ? 1 : !strcmp(s, "false") ? 0 : atof(s); }

// Oracle collision: the reference row says what the probes found and where the push-outs left the hero.
static struct { double x, y; bool ground, lw, rw; double xpush; } g_oracle;

static void OracleResolve(void *ctx, McHero *h) {
  h->onGround = g_oracle.ground;
  h->onLeftWall = g_oracle.lw;
  h->onRightWall = g_oracle.rw;
  double d = g_oracle.y - h->y;  // floor push-outs move up in whole pixels (grav = 0), ceilings down (grav = -2)
  const McWorld *w = h->world;
  if (w->use_dead_zone && h->y > w->dead_zone_y && fabs(g_oracle.y - w->spawn_y) < 1e-6)
    d = 0;  // fell into the dead zone: the respawn at the end of the tick explains the jump, not a push-out
  if (d < -1e-6 && d > -400) {  // MC pushes the hero up onto a step in one tick, however tall
    h->grav = 0;
    h->tick_dy += d; h->y += d;
  } else if (d > 1e-6 && d < 40) {
    h->grav = -2;
    h->tick_dy += d; h->y += d;
  }
  if (g_oracle.xpush != 0) {
    h->tick_dx += g_oracle.xpush; h->x += g_oracle.xpush;
    if (h->onGround) h->xSpeed = 0;
    else if (h->fighting) h->xSpeed = 0;
  }
}

// State as the reference driver sees it (see McHero.on_log_point).
static struct { double x, y, xs, g; int fr, sc, att, hurt, gr, wl; } g_snap;
static void Snap(McHero *h) {
  g_snap.x = h->x; g_snap.y = h->y; g_snap.xs = h->xSpeed; g_snap.g = h->grav;
  g_snap.fr = McHero_HeroFrame(h); g_snap.sc = h->xscale; g_snap.att = McHero_AttackFrame(h);
  g_snap.hurt = McHero_HurtFrame(h); g_snap.gr = h->onGround; g_snap.wl = h->walling;
}

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage\n"); return 2; }
  FILE *gf = fopen(argv[1], "rb");
  fread(&g_nedges, 4, 1, gf);
  g_edges = malloc(sizeof(Edge) * g_nedges);
  for (int i = 0; i < g_nedges; i++) {
    int key; double v[4];
    fread(&key, 4, 1, gf); fread(v, 8, 4, gf);
    g_edges[i] = (Edge){ key, v[0], v[1], v[2], v[3] };
    if (key > g_maxkey) g_maxkey = key;
  }
  fclose(gf);
  bool verbose = false, oracle = false;
  int nargs = argc;
  for (int i = 3; i < nargs; i++) {
    if (!strcmp(argv[i], "-v")) verbose = true, argc--;
    else if (!strcmp(argv[i], "--oracle")) oracle = true, argc--;
  }
  McWorld world = { 0 };
  world.hit = GroundHit;
  world.external = oracle;
  world.resolve = OracleResolve;
  if (argc >= 8) {
    world.use_scroll_bounds = true;
    world.lscroll = atof(argv[3]); world.rscroll = atof(argv[4]);
    world.use_dead_zone = true;
    world.dead_zone_y = atof(argv[5]); world.spawn_x = atof(argv[6]); world.spawn_y = atof(argv[7]);
  }
  FILE *tf = fopen(argv[2], "r");
  char line[1024], *f[F_COUNT];
  fgets(line, sizeof(line), tf);  // header
  static McHero hero;
  int row = 0, mism = 0, first_bad = -1, checked = 0;
  while (fgets(line, sizeof(line), tf)) {
    if (SplitCsv(line, f) < F_COUNT - 1) continue;
    if (row == 0) {
      McHero_Init(&hero, &world, Num(f[F_X]), Num(f[F_Y]));
      hero.xSpeed = Num(f[F_XSPEED]); hero.grav = Num(f[F_GRAV]);
      hero.onGround = Num(f[F_ONGROUND]) != 0;
      hero.stopSliding = (int)Num(f[F_STOPSLIDING]);
      hero.on_log_point = Snap;
      row++;
      continue;
    }
    int keys = ParseKeys(f[F_KEYS]);
    g_oracle.x = Num(f[F_X]); g_oracle.y = Num(f[F_Y]);
    g_oracle.ground = Num(f[F_ONGROUND]) != 0; g_oracle.lw = Num(f[F_ONLW]) != 0; g_oracle.rw = Num(f[F_ONRW]) != 0;
    g_oracle.xpush = 0;
    static McHero save;
    save = hero;
    McHero_Tick(&hero, keys);
    if (oracle && (fabs(g_oracle.x - g_snap.x) > 1e-6 || fabs(Num(f[F_XSPEED]) - g_snap.xs) > 1e-6)) {
      // a wall push-out happened in the original: find the whole-pixel push that reproduces this tick
      static McHero best;
      bool found = false;
      for (int p = 1; p <= 30 && !found; p++) {
        for (int s = -1; s <= 1 && !found; s += 2) {
          hero = save;
          g_oracle.xpush = p * s;
          McHero_Tick(&hero, keys);
          if (fabs(g_oracle.x - g_snap.x) < 1e-6 && fabs(Num(f[F_XSPEED]) - g_snap.xs) < 1e-6)
            found = true, best = hero;
        }
      }
      if (found) {
        hero = best;  // g_snap still holds this candidate's log-point state
      } else {
        hero = save; g_oracle.xpush = 0; McHero_Tick(&hero, keys);
      }
    }
    // Level enemies hit the hero after its own logic (DefineSprite_163); replay those hits from the reference.
    int ref_hurt = (int)Num(f[F_HURTFRAME]);
    if (oracle && (int)Num(f[F_FRAME]) == 19 && (ref_hurt == 1 || ref_hurt == 13) &&
        (McHero_HeroFrame(&hero) != 19 || McHero_HurtFrame(&hero) != ref_hurt)) {
      double xs = Num(f[F_XSPEED]);
      McHero_TakeHit(&hero, xs >= 0 ? 1 : -1, ref_hurt == 13, Num(f[F_LIFE]) < hero.life ? hero.life - Num(f[F_LIFE]) : 0);
      if (ref_hurt == 13) hero.grav = Num(f[F_GRAV]);  // enemy sets grav = 10 after the hero already ran this tick
      Snap(&hero);  // enemies run before the driver, so their effect is visible in the same row
    }
    checked++;
    char got[256], want[256];
    snprintf(got, sizeof(got), "x=%.2f y=%.2f xs=%g g=%g fr=%d sc=%d att=%d hurt=%d gr=%d wl=%d", g_snap.x, g_snap.y,
             g_snap.xs, g_snap.g, g_snap.fr, g_snap.sc, g_snap.att, g_snap.hurt, g_snap.gr, g_snap.wl);
    snprintf(want, sizeof(want), "x=%.2f y=%.2f xs=%g g=%g fr=%d sc=%d att=%d hurt=%d gr=%d wl=%d", Num(f[F_X]), Num(f[F_Y]),
             Num(f[F_XSPEED]), Num(f[F_GRAV]), (int)Num(f[F_FRAME]), (int)Num(f[F_XSCALE]), (int)Num(f[F_ATTACKFRAME]),
             (int)Num(f[F_HURTFRAME]), (int)Num(f[F_ONGROUND]), (int)Num(f[F_WALLING]));
    bool bad = strcmp(got, want) != 0;
    if (bad) {
      mism++;
      if (first_bad < 0) first_bad = atoi(f[F_TICK]);
    }
    if (verbose || (bad && mism <= 25))
      printf("%s tick %s keys %-6s\n   emu  %s\n   ref  %s\n", bad ? "DIFF" : "ok  ", f[F_TICK], f[F_KEYS], got, want);
    if (bad && !verbose && mism <= 25) {
      // resync to the reference so later checks are independent
      hero.x = Num(f[F_X]); hero.y = Num(f[F_Y]); hero.xSpeed = Num(f[F_XSPEED]); hero.grav = Num(f[F_GRAV]);
    }
    row++;
  }
  printf("RESULT %s: %d/%d ticks match (first mismatch at tick %d)\n", mism ? "FAIL" : "PASS", checked - mism, checked,
         first_bad);
  return mism ? 1 : 0;
}
