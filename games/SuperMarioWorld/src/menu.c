// In-game settings menu (see menu.h).
//
// Look: Super Mario World's own bold menu font (GFX28, the font of the file-select screen and status bar),
// decompressed at start-up from the player's game data, in a black message-box panel with a white border.
// Navigation: arrows / D-pad / left stick, Enter or (A) to choose, Esc or (B) to go back.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "menu.h"
#include "feedback.h"
#include "levelselect.h"
#include "config.h"
#include "funcs.h"
#include "smw_rtl.h"
#include "variables.h"
#include "util.h"
#include "assets/smw_assets.h"
#include "mc/mc_smw.h"
#include "cheats.h"
#include "mc/mc_level.h"
#include "pilot.h"

MemBlk FindInAssetArray(int asset, int idx);

enum {
  kColOutline = 0xff000000,
  kColWhite = 0xfff8f8f8,
  kColYellow = 0xfff8d830,
  kColGold = 0xfff89818,
  kColGray = 0xff989898,
  kColPanel = 0xff000000,
  kColBorder = 0xfff8f8f8,
  kColBorder2 = 0xff5878b8,
};

// ---------------------------------------------------------------------------------------------
// Font: GFX28 tiles 0x00-0x09 digits, 0x0A-0x23 A-Z, 0x24 '.', 0x25 ',', 0x26 'x' (times), 0x27 '-', 0x28 '!'.
// Colour 3 is the letter, colour 1 its black outline. Symbols SMW lacks are drawn below in the same style.
// ---------------------------------------------------------------------------------------------
static uint8 g_font[128][8][8];   // 0 = clear, 1 = outline, 2 = letter
static bool g_font_ok;
// Status-bar icons from the same graphics: the coin (cursor) and the bonus star (marks a cheat that is on).
enum { kIcon_Coin, kIcon_Star, kIcon_Count };
static const uint8 kIconTiles[kIcon_Count] = { 0x2E, 0x64 };
static uint8 g_icon[kIcon_Count][8][8];

static const struct { char ch; const char *rows[6]; } kExtraGlyphs[] = {
  { '?', { " #### ", "##  ##", "   ## ", "  ##  ", "      ", "  ##  " } },
  { ':', { "      ", "  ##  ", "  ##  ", "      ", "  ##  ", "  ##  " } },
  { '/', { "    ##", "   ## ", "  ##  ", " ##   ", "##    ", "      " } },
  { '+', { "      ", "  ##  ", "######", "######", "  ##  ", "      " } },
  { '%', { "##   #", "##  # ", "   #  ", "  #   ", " #  ##", "#   ##" } },
  { '(', { "  ##  ", " ##   ", "##    ", "##    ", " ##   ", "  ##  " } },
  { ')', { "  ##  ", "   ## ", "    ##", "    ##", "   ## ", "  ##  " } },
  { '<', { "   ## ", "  ### ", " #### ", " #### ", "  ### ", "   ## " } },
  { '>', { " ##   ", " ###  ", " #### ", " #### ", " ###  ", " ##   " } },
  { '=', { "      ", "######", "######", "      ", "######", "######" } },
  { '\'', { "  ##  ", "  ##  ", " ##   ", "      ", "      ", "      " } },
  { '[', { " #### ", " ##   ", " ##   ", " ##   ", " ##   ", " #### " } },
  { ']', { " #### ", "   ## ", "   ## ", "   ## ", "   ## ", " #### " } },
};

static void BuildExtraGlyph(char ch, const char *const *rows) {
  uint8 (*g)[8] = g_font[(uint8)ch];
  memset(g, 0, 64);
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < 6; x++)
      if (rows[y][x] == '#')
        g[y + 1][x + 1] = 2;
  for (int y = 0; y < 8; y++)  // outline: every empty pixel next to the letter
    for (int x = 0; x < 8; x++) {
      if (g[y][x]) continue;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          int yy = y + dy, xx = x + dx;
          if (yy >= 0 && yy < 8 && xx >= 0 && xx < 8 && g[yy][xx] == 2) g[y][x] = 1;
        }
    }
}

// Power-up item icons (POWER-UP lock row): SMW's own 16x16 mushroom / feather / fire flower from sprite graphics GFX00
// (3bpp), in the sprite palette rows SMW gives them (mushroom 12, feather 10, flower 13) taken from the game's object
// palettes the way the level loader fills them (colour 1 white, colours 2-7 from kGlobalPalettes_Objects).
enum { kItemMushroom, kItemFeather, kItemFlower, kItemStar, kItemShell, kItemCount };
static uint32 g_item_icon[kItemCount][16][16];  // ARGB, 0 = clear
static bool g_items_ok;

// The CHEATS hub shows one per page: mushroom (Mario), feather (moves), Koopa shell (enemies, GFX01), star (world).
static void LoadItemIcons(void) {
  static uint8 gfx[2][0x8000];
  for (int f = 0; f < 2; f++) {
    MemBlk blk = FindInAssetArray(0, f);  // kGraphicsPtrs(0x00 / 0x01) = GFX00 / GFX01
    if (!blk.ptr || !blk.size || !kGlobalPalettes_Objects)
      return;
    DecompressTo(blk.ptr, gfx[f]);
  }
  static const uint8 kTiles[kItemCount] = { 0x24, 0x0E, 0x26, 0x48, 0x0C };  // top-left tile of each 16x16 (+1, +0x10, +0x11)
  static const uint8 kRow[kItemCount] = { 12, 10, 13, 10, 13 };
  static const uint8 kFile[kItemCount] = { 0, 0, 0, 0, 1 };
  for (int i = 0; i < kItemCount; i++)
    for (int q = 0; q < 4; q++) {
      int t = kTiles[i] + (q & 1) + (q >> 1) * 0x10;
      const uint8 *p = gfx[kFile[i]] + t * 24;
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
          int v = ((p[y * 2] >> (7 - x)) & 1) | (((p[y * 2 + 1] >> (7 - x)) & 1) << 1) | (((p[16 + y] >> (7 - x)) & 1) << 2);
          uint32 c = 0;
          if (v == 1) {
            c = 0xfff8f8f8;
          } else if (v >= 2) {
            uint16 bgr = kGlobalPalettes_Objects[(kRow[i] - 4) * 6 + (v - 2)];
            uint32 r = bgr & 31, g = bgr >> 5 & 31, b = bgr >> 10 & 31;
            c = 0xff000000 | (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
          }
          g_item_icon[i][(q >> 1) * 8 + y][(q & 1) * 8 + x] = c;
        }
    }
  g_items_ok = true;
}

// Message-box font (GFX2A): the white letters of SMW's message boxes, lowercase included. The description of the
// selected option is written with it, as SMW writes its info boxes. It has no 8, 9, ':', '/' or '+': those are drawn
// below in the same style (2-pixel strokes, rows 0-6).
static uint8 g_msg_font[128][8][8];  // 1 = letter pixel
static bool g_msg_ok;

static const struct { char ch; const char *rows[7]; } kMsgExtraGlyphs[] = {
  { '8', { "  ####  ", " ##  ## ", " ##  ## ", "  ####  ", " ##  ## ", " ##  ## ", "  ####  " } },
  { '9', { "  ####  ", " ##  ## ", " ##  ## ", "  ##### ", "     ## ", " ##  ## ", "  ####  " } },
  { ':', { "        ", "   ##   ", "   ##   ", "        ", "   ##   ", "   ##   ", "        " } },
  { '/', { "     ## ", "     ## ", "    ##  ", "   ##   ", "  ##    ", " ##     ", " ##     " } },
  { '+', { "        ", "   ##   ", "   ##   ", " ###### ", "   ##   ", "   ##   ", "        " } },
};

static void LoadMessageFont(void) {
  static uint8 buf[0x2000];
  MemBlk blk = FindInAssetArray(0, 0x2A);  // kGraphicsPtrs(0x2A) = GFX2A
  if (!blk.ptr || !blk.size)
    return;
  DecompressTo(blk.ptr, buf);
  static const char kPunct[] = "!.-,?#()'";
  static const uint8 kPunctTiles[] = { 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x5A, 0x5B, 0x5C, 0x5D };
  for (int ch = 1; ch < 128; ch++) {
    int t = -1;
    const char *q = strchr(kPunct, ch);
    if (ch >= 'A' && ch <= 'Z') t = ch - 'A';
    else if (ch >= 'a' && ch <= 'z') t = 0x40 + ch - 'a';
    else if (ch >= '1' && ch <= '7') t = 0x64 + ch - '1';
    else if (ch == '0') t = 0x6B;
    else if (q) t = kPunctTiles[q - kPunct];
    if (t < 0) continue;
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++) {
        int v = ((buf[t * 16 + y * 2] >> (7 - x)) & 1) | (((buf[t * 16 + y * 2 + 1] >> (7 - x)) & 1) << 1);
        g_msg_font[ch][y][x] = v == 3;
      }
  }
  for (size_t i = 0; i < sizeof(kMsgExtraGlyphs) / sizeof(kMsgExtraGlyphs[0]); i++)
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 8; x++)
        g_msg_font[(uint8)kMsgExtraGlyphs[i].ch][y][x] = kMsgExtraGlyphs[i].rows[y][x] == '#';
  g_msg_ok = true;
}

void Menu_Init(void) {
  static uint8 buf[0x10000];
  MemBlk blk = FindInAssetArray(0, 0x28);  // kGraphicsPtrs(0x28)
  if (blk.ptr && blk.size) {
    DecompressTo(blk.ptr, buf);
    for (int i = 0; i < kIcon_Count; i++)  // status-bar icons, colours kept apart (1 outline, 2 shade, 3 main)
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
          const uint8 *t = buf + kIconTiles[i] * 16 + y * 2;
          g_icon[i][y][x] = ((t[0] >> (7 - x)) & 1) | (((t[1] >> (7 - x)) & 1) << 1);
        }
    for (int t = 0; t <= 0x28; t++) {
      int ch = t < 10 ? '0' + t : t < 0x24 ? 'A' + t - 10 : ".,x-!"[t - 0x24];
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
          int v = ((buf[t * 16 + y * 2] >> (7 - x)) & 1) | (((buf[t * 16 + y * 2 + 1] >> (7 - x)) & 1) << 1);
          g_font[ch][y][x] = v == 3 || v == 2 ? 2 : v == 1 ? 1 : 0;
        }
    }
    g_font_ok = true;
  }
  LoadItemIcons();
  LoadMessageFont();
  for (size_t i = 0; i < sizeof(kExtraGlyphs) / sizeof(kExtraGlyphs[0]); i++)
    BuildExtraGlyph(kExtraGlyphs[i].ch, kExtraGlyphs[i].rows);
}

// ---------------------------------------------------------------------------------------------
// Drawing on the finished frame
// ---------------------------------------------------------------------------------------------
static struct { uint8 *px; int pitch, w, h, s; } g_fb;  // s = output pixels per SNES pixel

static void Put(int x, int y, uint32 c) {
  if ((unsigned)x >= 256 || (unsigned)y >= (unsigned)(g_fb.h / g_fb.s))
    return;
  for (int yy = 0; yy < g_fb.s; yy++) {
    uint32 *row = (uint32 *)(g_fb.px + (y * g_fb.s + yy) * g_fb.pitch) + x * g_fb.s;
    for (int xx = 0; xx < g_fb.s; xx++)
      row[xx] = c;
  }
}

static void FillRect(int x0, int y0, int w, int h, uint32 c) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++)
      Put(x, y, c);
}

static void DimFrame(void) {
  for (int y = 0; y < g_fb.h; y++) {
    uint32 *row = (uint32 *)(g_fb.px + y * g_fb.pitch);
    for (int x = 0; x < g_fb.w; x++) {
      uint32 p = row[x];
      row[x] = 0xff000000 | ((p >> 2) & 0x3f3f3f);
    }
  }
}

static void DrawChar(int x, int y, char ch, uint32 col) {
  if (ch >= 'a' && ch <= 'z' && ch != 'x') ch -= 32;  // 'x' is SMW's times sign ("3x")
  if (ch == ' ') return;
  uint8 (*g)[8] = g_font[(uint8)ch];
  bool any = false;
  for (int i = 0; i < 64; i++) any |= ((uint8 *)g)[i] != 0;
  if (!any) g = g_font['?'];
  for (int yy = 0; yy < 8; yy++)
    for (int xx = 0; xx < 8; xx++)
      if (g[yy][xx])
        Put(x + xx, y + yy, g[yy][xx] == 2 ? col : kColOutline);
}

static void DrawIcon(int x, int y, int icon, uint32 col) {
  if (!g_font_ok) {  // no game graphics: plain stand-ins
    DrawChar(x, y, icon == kIcon_Coin ? '>' : '+', col);
    return;
  }
  for (int yy = 0; yy < 8; yy++)
    for (int xx = 0; xx < 8; xx++) {
      uint8 v = g_icon[icon][yy][xx];
      if (v) Put(x + xx, y + yy, v == 1 ? kColOutline : col);
    }
}

static void DrawItemIcon(int x, int y, int item) {  // 16x16, top-left at (x, y)
  for (int yy = 0; yy < 16; yy++)
    for (int xx = 0; xx < 16; xx++)
      if (g_item_icon[item][yy][xx]) Put(x + xx, y + yy, g_item_icon[item][yy][xx]);
}

static void DrawText(int x, int y, const char *s, uint32 col) {
  for (; *s; s++, x += 8)
    DrawChar(x, y, *s, col);
}

static int TextW(const char *s) { return (int)strlen(s) * 8; }

static void DrawMsgText(int x, int y, const char *s, int n, uint32 col) {  // the message-box font, n characters
  if (!g_msg_ok) {  // no game graphics: the menu font
    char b[64];
    snprintf(b, sizeof(b), "%.*s", n, s);
    DrawText(x, y, b, col);
    return;
  }
  for (int i = 0; i < n && s[i]; i++, x += 8) {
    uint8 (*g)[8] = g_msg_font[(uint8)s[i] & 127];
    for (int yy = 0; yy < 8; yy++)
      for (int xx = 0; xx < 8; xx++)
        if (g[yy][xx]) Put(x + xx, y + yy, col);
  }
}

static void DrawPanel(int x, int y, int w, int h) {
  FillRect(x + 1, y, w - 2, h, kColBorder);            // rounded white frame
  FillRect(x, y + 1, w, h - 2, kColBorder);
  FillRect(x + 1, y + 1, w - 2, h - 2, kColBorder2);   // blue inner line
  FillRect(x + 2, y + 2, w - 4, h - 4, kColPanel);     // black message-box body
}

// ---------------------------------------------------------------------------------------------
// Menu model
// ---------------------------------------------------------------------------------------------
enum { P_MAIN, P_VIDEO, P_SOUND, P_GAMEPLAY, P_CONTROLS, P_KEYBOARD, P_GAMEPAD, P_RESET, P_QUIT, P_CHEATS,
       P_CHEATS_MARIO, P_CHEATS_WORLD, P_CHEATS_MOVES, P_CHEATS_ENEMIES, P_LEVELS, P_LEVEL_LIST, P_BOSS_RUSH };
enum { K_LINK, K_OPTION, K_BIND, K_ACTION, K_LGROUP /* a level select group (id) */, K_LEVEL /* a level (entry) */ };
enum {
  O_DISPLAY, O_WINDOW, O_ASPECT, O_FILTER, O_VSYNC, O_FPS,
  O_VOLUME, O_MCSOUNDS, O_MCVOLUME,
  O_CHARACTER, O_UPJUMP, O_LIFEBAR, O_DAMAGE, O_HITBOX, O_SAVE100,
  O_POWERUP, O_KNOCKBACK,
  O_MONO, O_BACKGROUND, O_RUMBLE, O_DEADZONE,
  O_RUSH_BOSSES, O_RUSH_DEATH,
  O_CHEAT0,  // O_CHEAT0 + kCheat_*
};
enum { A_RESUME, A_BACK, A_RESET_KEYS, A_RESET_PAD, A_RESET_ALL_YES, A_QUIT_YES, A_CHEATS_OFF, A_RUSH_START,
       A_MC_LEVEL };

typedef struct Item { const char *label; uint8 kind; uint8 id; } Item;

static const Item kMain[] = {
  { "RESUME", K_ACTION, A_RESUME }, { "VIDEO", K_LINK, P_VIDEO }, { "SOUND", K_LINK, P_SOUND },
  { "GAMEPLAY", K_LINK, P_GAMEPLAY }, { "CONTROLS", K_LINK, P_CONTROLS }, { "CHEATS", K_LINK, P_CHEATS },
  { "LEVEL SELECT", K_LINK, P_LEVELS },
  { "RESET ALL SETTINGS", K_LINK, P_RESET }, { "QUIT GAME", K_LINK, P_QUIT },
};
static const Item kCheats[] = {
  { "MARIO CHEATS", K_LINK, P_CHEATS_MARIO }, { "MOVE CHEATS", K_LINK, P_CHEATS_MOVES },
  { "ENEMY CHEATS", K_LINK, P_CHEATS_ENEMIES }, { "WORLD CHEATS", K_LINK, P_CHEATS_WORLD },
  { "ALL CHEATS OFF", K_ACTION, A_CHEATS_OFF }, { "BACK", K_ACTION, A_BACK },
};
static const Item kCheatsMario[] = {  // protection and power-ups
  { "INFINITE MC LIFE", K_OPTION, O_CHEAT0 + kCheat_InfiniteLife },
  { "NO DAMAGE", K_OPTION, O_CHEAT0 + kCheat_NoDamage },
  { "INVINCIBLE (STAR)", K_OPTION, O_CHEAT0 + kCheat_Invincible },
  { "NO KNOCKBACK (MC)", K_OPTION, O_CHEAT0 + kCheat_NoKnockback },
  { "KNOCKBACK (MC)", K_OPTION, O_KNOCKBACK },  // 1X (normal) to 20X
  { "ONE-HIT MODE", K_OPTION, O_CHEAT0 + kCheat_OneHit },
  { "POWER-UP", K_OPTION, O_POWERUP },  // locked power-up (value: OFF / MUSHROOM / FEATHER / FIRE FLOWER)
  { "ALWAYS YOSHI", K_OPTION, O_CHEAT0 + kCheat_AlwaysYoshi },
  { "FLYING YOSHI", K_OPTION, O_CHEAT0 + kCheat_FlyingYoshi },
  { "LIFE STEAL (MC)", K_OPTION, O_CHEAT0 + kCheat_LifeSteal },
  { "BACK", K_ACTION, A_BACK },
};
static const Item kCheatsMoves[] = {  // what Mario can do
  { "SUPER PUNCH (MC)", K_OPTION, O_CHEAT0 + kCheat_SuperPunch },
  { "DOUBLE JUMP", K_OPTION, O_CHEAT0 + kCheat_DoubleJump },
  { "INFINITE JUMPS", K_OPTION, O_CHEAT0 + kCheat_InfiniteJumps },
  { "GROUND POUND", K_OPTION, O_CHEAT0 + kCheat_GroundPound },
  { "WALL JUMP (SMW)", K_OPTION, O_CHEAT0 + kCheat_WallJump },
  { "MOON JUMP", K_OPTION, O_CHEAT0 + kCheat_MoonJump },
  { "AIR DASH (L / R)", K_OPTION, O_CHEAT0 + kCheat_AirDash },
  { "ALWAYS P-SPEED", K_OPTION, O_CHEAT0 + kCheat_PSpeed },
  { "FIRE PUNCH (MC)", K_OPTION, O_CHEAT0 + kCheat_FirePunch },
  { "INSTANT HITS (MC)", K_OPTION, O_CHEAT0 + kCheat_InstantHits },
  { "BLOCK BUSTER (MC)", K_OPTION, O_CHEAT0 + kCheat_BlockBuster },
  { "BACK", K_ACTION, A_BACK },
};
static const Item kCheatsEnemies[] = {
  { "TRIPLE ENEMIES", K_OPTION, O_CHEAT0 + kCheat_TripleEnemies },
  { "MC KOOPAS", K_OPTION, O_CHEAT0 + kCheat_McKoopas },
  { "KING KOOPAS", K_OPTION, O_CHEAT0 + kCheat_KingKoopas },
  { "MC HIT REACTIONS", K_OPTION, O_CHEAT0 + kCheat_McHits },
  { "COINS FROM ENEMIES", K_OPTION, O_CHEAT0 + kCheat_EnemyCoins },
  { "STOMP ANYTHING", K_OPTION, O_CHEAT0 + kCheat_StompAll },
  { "PUNCH ANYTHING (MC)", K_OPTION, O_CHEAT0 + kCheat_PunchAll },
  { "FAST ENEMIES", K_OPTION, O_CHEAT0 + kCheat_FastEnemies },
  { "CHASING ENEMIES", K_OPTION, O_CHEAT0 + kCheat_ChaseEnemies },
  { "JUMPING ENEMIES", K_OPTION, O_CHEAT0 + kCheat_JumpEnemies },
  { "RESPAWNING ENEMIES", K_OPTION, O_CHEAT0 + kCheat_Respawn },
  { "BACK", K_ACTION, A_BACK },
};
static const Item kCheatsWorld[] = {
  { "INFINITE TIME", K_OPTION, O_CHEAT0 + kCheat_InfiniteTime },
  { "INFINITE LIVES", K_OPTION, O_CHEAT0 + kCheat_InfiniteLives },
  { "ALL SWITCH BLOCKS", K_OPTION, O_CHEAT0 + kCheat_Switches },
  { "REPLAY CASTLES", K_OPTION, O_CHEAT0 + kCheat_ReplayLevels },
  { "SLOW MOTION", K_OPTION, O_CHEAT0 + kCheat_SlowMotion },
  { "MIRROR MODE", K_OPTION, O_CHEAT0 + kCheat_Mirror },
  { "SLIPPERY LEVELS", K_OPTION, O_CHEAT0 + kCheat_Slippery },
  { "PIT RESCUE", K_OPTION, O_CHEAT0 + kCheat_PitRescue },
  { "COIN MAGNET", K_OPTION, O_CHEAT0 + kCheat_CoinMagnet },
  { "OPEN MAP", K_OPTION, O_CHEAT0 + kCheat_OpenMap },
  { "LOW GRAVITY", K_OPTION, O_CHEAT0 + kCheat_LowGravity },
  { "BACK", K_ACTION, A_BACK },
};
static const Item kVideo[] = {
  { "DISPLAY", K_OPTION, O_DISPLAY }, { "WINDOW SIZE", K_OPTION, O_WINDOW }, { "ASPECT RATIO", K_OPTION, O_ASPECT },
  { "PIXELS", K_OPTION, O_FILTER }, { "VSYNC", K_OPTION, O_VSYNC }, { "SHOW FPS", K_OPTION, O_FPS },
  { "BACK", K_ACTION, A_BACK },
};
static const Item kSound[] = {
  { "MASTER VOLUME", K_OPTION, O_VOLUME }, { "MC SOUNDS", K_OPTION, O_MCSOUNDS },
  { "MC SOUND VOLUME", K_OPTION, O_MCVOLUME }, { "OUTPUT", K_OPTION, O_MONO }, { "BACK", K_ACTION, A_BACK },
};
static const Item kGameplay[] = {
  { "MARIO", K_OPTION, O_CHARACTER }, { "UP TO JUMP", K_OPTION, O_UPJUMP }, { "LIFE BAR", K_OPTION, O_LIFEBAR },
  { "DAMAGE PER HIT", K_OPTION, O_DAMAGE }, { "SHOW HIT BOXES", K_OPTION, O_HITBOX },
  { "FILE 3 = 100% SAVE", K_OPTION, O_SAVE100 }, { "IN BACKGROUND", K_OPTION, O_BACKGROUND },
  { "BACK", K_ACTION, A_BACK },
};
static const Item kControls[] = {
  { "KEYBOARD", K_LINK, P_KEYBOARD }, { "GAMEPAD", K_LINK, P_GAMEPAD }, { "RUMBLE", K_OPTION, O_RUMBLE },
  { "STICK DEADZONE", K_OPTION, O_DEADZONE }, { "BACK", K_ACTION, A_BACK },
};
static const char *const kBindLabels[kBind_Count] = {
  "UP", "DOWN", "LEFT", "RIGHT", "SELECT", "START", "JUMP (A)", "JUMP (B)", "RUN/GRAB (X)", "PUNCH (Y)",
  "L", "R", "SWITCH MARIO", "SETTINGS", "FPS COUNTER", "FULLSCREEN",
};
static Item g_bind_items[kBind_Count + 2];
enum { kMaxLevelItems = 48 };
static Item g_level_items[kMaxLevelItems];  // the LEVEL SELECT pages' rows (built by PageItems)
static const Item kBossRush[] = {
  { "START BOSS RUSH", K_ACTION, A_RUSH_START }, { "BOSSES", K_OPTION, O_RUSH_BOSSES },
  { "ON DEATH", K_OPTION, O_RUSH_DEATH }, { "BACK", K_ACTION, A_BACK },
};
static const Item kReset[] = { { "NO", K_ACTION, A_BACK }, { "YES, RESET ALL", K_ACTION, A_RESET_ALL_YES } };
static const Item kQuit[] = { { "NO", K_ACTION, A_BACK }, { "YES, QUIT", K_ACTION, A_QUIT_YES } };

static struct {
  bool open, dirty;
  int page, cursor[24], scroll;
  int level_group;          // the group P_LEVEL_LIST shows
  int stack[8], depth;
  int capture;              // bind index being captured, or -1
  uint32 capture_start;
  int pad_dir, pad_axis_x, pad_axis_y;
  uint32 pad_next_repeat;
  char notice[48];
  uint32 notice_until;
  uint32 fps_t0;
  int fps_frames, fps;
  uint32 now;
} g_m = { .capture = -1 };

static const char *PageTitle(int p) {
  static const char *const t[] = { "SETTINGS", "VIDEO", "SOUND", "GAMEPLAY", "CONTROLS", "KEYBOARD", "GAMEPAD",
                                   "RESET ALL SETTINGS?", "QUIT THE GAME?", "CHEATS", "MARIO CHEATS",
                                   "WORLD CHEATS", "MOVE CHEATS", "ENEMY CHEATS", "LEVEL SELECT" };
  if (p == P_LEVEL_LIST)
    return LevelSelect_GroupName(g_m.level_group);
  if (p == P_BOSS_RUSH)
    return "BOSS RUSH";
  return t[p];
}

static int PageItems(int p, const Item **items) {
  switch (p) {
  case P_MAIN: *items = kMain; return sizeof(kMain) / sizeof(Item);
  case P_VIDEO: *items = kVideo; return sizeof(kVideo) / sizeof(Item);
  case P_SOUND: *items = kSound; return sizeof(kSound) / sizeof(Item);
  case P_GAMEPLAY: *items = kGameplay; return sizeof(kGameplay) / sizeof(Item);
  case P_CONTROLS: *items = kControls; return sizeof(kControls) / sizeof(Item);
  case P_CHEATS: *items = kCheats; return sizeof(kCheats) / sizeof(Item);
  case P_CHEATS_MARIO: *items = kCheatsMario; return sizeof(kCheatsMario) / sizeof(Item);
  case P_CHEATS_WORLD: *items = kCheatsWorld; return sizeof(kCheatsWorld) / sizeof(Item);
  case P_CHEATS_MOVES: *items = kCheatsMoves; return sizeof(kCheatsMoves) / sizeof(Item);
  case P_CHEATS_ENEMIES: *items = kCheatsEnemies; return sizeof(kCheatsEnemies) / sizeof(Item);
  case P_KEYBOARD: case P_GAMEPAD:
    for (int i = 0; i < kBind_Count; i++)
      g_bind_items[i] = (Item){ kBindLabels[i], K_BIND, (uint8)i };
    g_bind_items[kBind_Count] = (Item){ "RESET TO DEFAULTS", K_ACTION, p == P_KEYBOARD ? A_RESET_KEYS : A_RESET_PAD };
    g_bind_items[kBind_Count + 1] = (Item){ "BACK", K_ACTION, A_BACK };
    *items = g_bind_items;
    return kBind_Count + 2;
  case P_LEVELS:  // Mario Combat's level, the boss rush, the groups, then BACK
    g_level_items[0] = (Item){ "MARIO COMBAT", K_ACTION, A_MC_LEVEL };
    g_level_items[1] = (Item){ "BOSS RUSH", K_LINK, P_BOSS_RUSH };
    for (int g = 0; g < kLevelSelectGroups; g++)
      g_level_items[2 + g] = (Item){ LevelSelect_GroupName(g), K_LGROUP, (uint8)g };
    g_level_items[2 + kLevelSelectGroups] = (Item){ "BACK", K_ACTION, A_BACK };
    *items = g_level_items;
    return kLevelSelectGroups + 3;
  case P_BOSS_RUSH: *items = kBossRush; return sizeof(kBossRush) / sizeof(Item);
  case P_LEVEL_LIST: {  // the levels of g_m.level_group, in order, then BACK
    int n = 0;
    for (int i = 0; i < LevelSelect_Count() && n < kMaxLevelItems - 1; i++)
      if (LevelSelect_Entry(i)->group == g_m.level_group)
        g_level_items[n++] = (Item){ LevelSelect_Entry(i)->name, K_LEVEL, (uint8)i };
    g_level_items[n++] = (Item){ "BACK", K_ACTION, A_BACK };
    *items = g_level_items;
    return n;
  }
  case P_RESET: *items = kReset; return 2;
  case P_QUIT: *items = kQuit; return 2;
  }
  *items = NULL;
  return 0;
}

static bool InLevel(void) { return misc_game_mode == 0x14; }

static void OptionText(int id, char *buf, int n) {
  switch (id) {
  case O_DISPLAY: snprintf(buf, n, "%s", g_config.fullscreen == 0 ? "WINDOW" : g_config.fullscreen == 1 ? "FULLSCREEN" : "EXCLUSIVE"); break;
  case O_WINDOW: snprintf(buf, n, "%dx  %dx%d", g_config.window_scale, 256 * g_config.window_scale, 224 * g_config.window_scale); break;
  case O_ASPECT: snprintf(buf, n, "%s", g_config.ignore_aspect_ratio ? "STRETCH" : "ORIGINAL"); break;
  case O_FILTER: snprintf(buf, n, "%s", g_config.linear_filtering ? "SMOOTH" : "SHARP"); break;
  case O_VSYNC: snprintf(buf, n, "%s", g_config.vsync ? "ON" : "OFF"); break;
  case O_FPS: snprintf(buf, n, "%s", g_config.show_fps ? "ON" : "OFF"); break;
  case O_VOLUME: snprintf(buf, n, "%d%%", g_config.master_volume); break;
  case O_MCSOUNDS: snprintf(buf, n, "%s", g_config.mc_sounds ? "ON" : "OFF"); break;
  case O_MCVOLUME: snprintf(buf, n, "%d%%", g_config.mc_sound_volume); break;
  case O_CHARACTER: snprintf(buf, n, "%s", McSmw_CharacterIsMc() ? "MARIO COMBAT" : "SUPER MARIO W."); break;
  case O_UPJUMP: snprintf(buf, n, "%s", g_config.mc_up_jumps ? "ON" : "OFF"); break;
  case O_LIFEBAR: snprintf(buf, n, "%s", g_config.mc_life_bar ? "ON" : "OFF"); break;
  case O_DAMAGE: snprintf(buf, n, "%.1f", g_config.mc_damage_per_hit); break;
  case O_HITBOX: snprintf(buf, n, "%s", g_config.mc_debug_hitboxes ? "ON" : "OFF"); break;
  case O_SAVE100: snprintf(buf, n, "%s", g_config.save100_file3 ? "ON" : "OFF"); break;
  case O_POWERUP: {
    static const char *const k[] = { "OFF", "MUSHROOM", "FEATHER", "FIRE FLOWER" };
    snprintf(buf, n, "%s", k[g_config.cheat_powerup & 3]);
    break;
  }
  case O_KNOCKBACK: snprintf(buf, n, "%dX", Cheats_KnockbackScale()); break;
  case O_MONO: snprintf(buf, n, "%s", g_config.audio_mono ? "MONO" : "STEREO"); break;
  case O_BACKGROUND: {
    static const char *const k[] = { "PLAY ON", "MUTE", "PAUSE" };
    snprintf(buf, n, "%s", k[g_config.in_background % 3]);
    break;
  }
  case O_RUMBLE: {
    static const char *const k[] = { "OFF", "LIGHT", "FULL" };
    snprintf(buf, n, "%s", k[g_config.rumble % 3]);
    break;
  }
  case O_DEADZONE: snprintf(buf, n, "%d%%", Config_StickDeadzone()); break;
  case O_RUSH_BOSSES: snprintf(buf, n, "%s", g_config.boss_rush_all ? "EVERY BOSS" : "CASTLES"); break;
  case O_RUSH_DEATH: snprintf(buf, n, "%s", g_config.boss_rush_end_on_death ? "END RUN" : "RETRY BOSS"); break;
  default:
    if (id >= O_CHEAT0 && id < O_CHEAT0 + kCheat_Count)
      snprintf(buf, n, "%s", g_config.cheat[id - O_CHEAT0] ? "ON" : "OFF");
    else
      buf[0] = 0;
  }
}

static int Wrap(int v, int lo, int hi) { return v < lo ? hi : v > hi ? lo : v; }

static void ChangeOption(int id, int dir) {
  g_m.dirty = true;
  switch (id) {
  case O_DISPLAY: g_config.fullscreen = (uint8)Wrap(g_config.fullscreen + dir, 0, 2); Main_SetFullscreen(g_config.fullscreen); break;
  case O_WINDOW: g_config.window_scale = (uint8)Wrap(g_config.window_scale + dir, 1, Main_MaxWindowScale()); Main_SetWindowScale(g_config.window_scale); break;
  case O_ASPECT: g_config.ignore_aspect_ratio ^= 1; Main_ApplyVideo(); break;
  case O_FILTER: g_config.linear_filtering ^= 1; Main_ApplyVideo(); break;
  case O_VSYNC: g_config.vsync ^= 1; Main_ApplyVideo(); break;
  case O_FPS: g_config.show_fps ^= 1; break;
  case O_VOLUME: g_config.master_volume = (uint8)IntMax(0, IntMin(100, g_config.master_volume + dir * 10)); Main_ApplyVolume(); break;
  case O_MCSOUNDS: g_config.mc_sounds ^= 1; break;
  case O_MCVOLUME: g_config.mc_sound_volume = (uint8)IntMax(0, IntMin(100, g_config.mc_sound_volume + dir * 10)); break;
  case O_CHARACTER:
    if (InLevel()) McSmw_ToggleCharacter();
    else Menu_Notice("CHANGE MARIO IN A LEVEL");
    break;
  case O_UPJUMP: g_config.mc_up_jumps ^= 1; break;
  case O_LIFEBAR: g_config.mc_life_bar ^= 1; break;
  case O_DAMAGE: {
    static const float kSteps[] = { 0.5f, 1.0f, 2.0f, 2.5f, 3.4f, 5.0f, 10.0f };
    int i = 0, n = sizeof(kSteps) / sizeof(kSteps[0]);
    while (i < n - 1 && kSteps[i] < g_config.mc_damage_per_hit - 0.01f) i++;
    g_config.mc_damage_per_hit = kSteps[Wrap(i + dir, 0, n - 1)];
    break;
  }
  case O_HITBOX: g_config.mc_debug_hitboxes ^= 1; break;
  case O_SAVE100: {
    bool want = !g_config.save100_file3;
    if (Cheats_SetSave100(want)) {
      g_config.save100_file3 = want;
      Menu_Notice(want ? "FILE 3 IS NOW 100%" : "YOUR FILE 3 IS BACK");
    } else {
      Menu_Notice("COULD NOT CHANGE FILE 3");
    }
    break;
  }
  case O_POWERUP: g_config.cheat_powerup = (uint8)Wrap(g_config.cheat_powerup + dir, 0, 3); break;
  case O_KNOCKBACK: g_config.cheat_knockback = (uint8)Wrap(Cheats_KnockbackScale() + dir, 1, 20); break;
  case O_MONO: g_config.audio_mono ^= 1; break;
  case O_BACKGROUND: g_config.in_background = (uint8)Wrap(g_config.in_background + dir, 0, kBackground_Pause); break;
  case O_RUMBLE:
    g_config.rumble = (uint8)Wrap(g_config.rumble + dir, 0, 2);
    Main_Rumble(0.5f, 0.5f, 200);  // a taste of the new strength
    break;
  case O_DEADZONE: {
    static const uint8 kSteps[] = { 10, 15, 20, 25, 30, 40, 50 };
    int i = 0, cnt = sizeof(kSteps);
    while (i < cnt - 1 && kSteps[i] < Config_StickDeadzone()) i++;
    g_config.stick_deadzone = kSteps[Wrap(i + dir, 0, cnt - 1)];
    break;
  }
  case O_RUSH_BOSSES: g_config.boss_rush_all ^= 1; break;
  case O_RUSH_DEATH: g_config.boss_rush_end_on_death ^= 1; break;
  default:
    if (id >= O_CHEAT0 && id < O_CHEAT0 + kCheat_Count) {
      int c = id - O_CHEAT0;
      g_config.cheat[c] ^= 1;
      // MC Koopas and King Koopas both replace the Koopas: switching one on switches the other off
      int other = c == kCheat_McKoopas ? kCheat_KingKoopas : c == kCheat_KingKoopas ? kCheat_McKoopas : -1;
      if (other >= 0 && g_config.cheat[c] && g_config.cheat[other]) {
        g_config.cheat[other] = false;
        Menu_Notice(other == kCheat_McKoopas ? "MC KOOPAS TURNED OFF" : "KING KOOPAS TURNED OFF");
      }
    }
  }
}

static void GoPage(int p) {
  if (g_m.depth < 8) g_m.stack[g_m.depth++] = g_m.page;
  g_m.page = p;
  g_m.cursor[p] = 0;
  g_m.scroll = 0;
}

static void Back(void) {
  if (g_m.depth == 0) { Menu_Close(); return; }
  g_m.page = g_m.stack[--g_m.depth];
  g_m.scroll = 0;
}

static void Activate(const Item *it) {
  switch (it->kind) {
  case K_LINK: GoPage(it->id); break;
  case K_LGROUP: g_m.level_group = it->id; GoPage(P_LEVEL_LIST); break;
  case K_LEVEL: {
    const char *why = LevelSelect_Request(it->id);
    if (why) Menu_Notice(why);
    else Menu_Close();  // the level starts on the next game frame
    break;
  }
  case K_OPTION: ChangeOption(it->id, +1); break;
  case K_BIND: g_m.capture = it->id; g_m.capture_start = g_m.now; break;
  case K_ACTION:
    switch (it->id) {
    case A_RESUME: Menu_Close(); break;
    case A_BACK: Back(); break;
    case A_RESET_KEYS: Config_ResetKeyBindings(); g_m.dirty = true; Menu_Notice("KEYBOARD RESET"); break;
    case A_RESET_PAD: Config_ResetPadBindings(); g_m.dirty = true; Menu_Notice("GAMEPAD RESET"); break;
    case A_RESET_ALL_YES:
      Config_ResetToDefaults();
      Main_SetFullscreen(g_config.fullscreen);
      Main_SetWindowScale(g_config.window_scale);
      Main_ApplyVideo();
      Main_ApplyVolume();
      g_m.dirty = true;
      Back();
      Menu_Notice("SETTINGS RESET");
      break;
    case A_QUIT_YES: Main_Quit(); break;
    case A_CHEATS_OFF:
      memset(g_config.cheat, 0, sizeof(g_config.cheat));
      g_config.cheat_powerup = 0;
      g_config.cheat_knockback = 1;
      g_m.dirty = true;
      Menu_Notice("ALL CHEATS OFF");
      break;
    case A_RUSH_START: {
      const char *why = LevelSelect_StartBossRush();
      if (why) Menu_Notice(why);
      else Menu_Close();  // the first fight starts on the next game frame
      break;
    }
    case A_MC_LEVEL: {
      const char *why = McLevel_Start();
      if (why) Menu_Notice(why);
      else Menu_Close();  // its first area starts on the next game frame
      break;
    }
    }
    break;
  }
}

static void Move(int dy) {
  const Item *items;
  int n = PageItems(g_m.page, &items);
  g_m.cursor[g_m.page] = Wrap(g_m.cursor[g_m.page] + dy, 0, n - 1);
}

static void Side(int dx) {
  const Item *items;
  PageItems(g_m.page, &items);
  const Item *it = &items[g_m.cursor[g_m.page]];
  if (it->kind == K_OPTION)
    ChangeOption(it->id, dx);
}

static void Ok(void) {
  const Item *items;
  PageItems(g_m.page, &items);
  Activate(&items[g_m.cursor[g_m.page]]);
}

bool Menu_IsOpen(void) { return g_m.open; }

void Menu_Open(void) {
  if (g_m.open) return;
  g_m.open = true;
  g_m.page = P_MAIN;
  g_m.depth = 0;
  g_m.cursor[P_MAIN] = 0;
  g_m.capture = -1;
  g_m.pad_dir = 0;
}

void Menu_Close(void) {
  if (!g_m.open) return;
  g_m.open = false;
  g_m.capture = -1;
  if (g_m.dirty) {
    g_m.dirty = false;
    if (Config_SaveUserIni()) Menu_Notice("SETTINGS SAVED");
  }
}

// A notice's clock: real time, or under a test pilot (which runs faster than real time) its frames at 60 a second, so
// a notice covers the same frames in every run of a script (its screenshots and recordings).
static uint32 NoticeClock(void) {
  return Pilot_Active() ? Pilot_Frame() * 50 / 3 : SDL_GetTicks();
}

void Menu_Notice(const char *text) {
  snprintf(g_m.notice, sizeof(g_m.notice), "%s", text);
  g_m.notice_until = NoticeClock() + 1800;
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------
bool Menu_HandleKey(SDL_Keycode key, SDL_Keymod mod, bool pressed, bool repeat) {
  if (!g_m.open) return false;
  if (!pressed) return true;
  if (g_m.capture >= 0) {
    if (g_m.page == P_KEYBOARD) {
      if (key == SDLK_ESCAPE) { g_m.capture = -1; return true; }
      if (key == SDLK_LSHIFT || key == SDLK_LCTRL || key == SDLK_LALT || key == SDLK_RCTRL || key == SDLK_RALT)
        return true;  // wait for the key itself (a lone modifier is not a binding, except Right Shift)
      Config_SetKeyBinding(g_m.capture, Config_KeyFromSdl(key, (SDL_Keymod)0));
      g_m.dirty = true;
      g_m.capture = -1;
    } else if (key == SDLK_ESCAPE) {
      g_m.capture = -1;
    }
    return true;
  }
  switch (key) {
  case SDLK_UP: Move(-1); break;
  case SDLK_DOWN: Move(+1); break;
  case SDLK_LEFT: Side(-1); break;
  case SDLK_RIGHT: Side(+1); break;
  case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: if (!repeat) Ok(); break;
  case SDLK_ESCAPE: case SDLK_BACKSPACE: if (!repeat) Back(); break;
  default: {
    if (repeat) break;
    // the player's own Settings key closes the menu too; the FPS / fullscreen hotkeys work here as well
    int cmd = FindCmdForSdlKey(key, mod);
    if (Config_KeyFromSdl(key, mod) == Config_GetKeyBinding(kBind_Settings)) {
      Menu_Close();
    } else if (cmd == kKeys_ShowFps) {
      g_config.show_fps ^= 1;
      g_m.dirty = true;
    } else if (cmd == kKeys_FullscreenToggle || cmd == kKeys_Fullscreen) {
      g_config.fullscreen = g_config.fullscreen ? 0 : 1;
      Main_SetFullscreen(g_config.fullscreen);
      g_m.dirty = true;
    }
  }
  }
  return true;
}

static int PadButtonFromSdl(int b) {
  switch (b) {
  case SDL_CONTROLLER_BUTTON_A: return kGamepadBtn_A;
  case SDL_CONTROLLER_BUTTON_B: return kGamepadBtn_B;
  case SDL_CONTROLLER_BUTTON_X: return kGamepadBtn_X;
  case SDL_CONTROLLER_BUTTON_Y: return kGamepadBtn_Y;
  case SDL_CONTROLLER_BUTTON_BACK: return kGamepadBtn_Back;
  case SDL_CONTROLLER_BUTTON_GUIDE: return kGamepadBtn_Guide;
  case SDL_CONTROLLER_BUTTON_START: return kGamepadBtn_Start;
  case SDL_CONTROLLER_BUTTON_LEFTSTICK: return kGamepadBtn_L3;
  case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return kGamepadBtn_R3;
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return kGamepadBtn_L1;
  case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return kGamepadBtn_R1;
  case SDL_CONTROLLER_BUTTON_DPAD_UP: return kGamepadBtn_DpadUp;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return kGamepadBtn_DpadDown;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return kGamepadBtn_DpadLeft;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return kGamepadBtn_DpadRight;
  }
  return -1;
}

static void PadDir(int dir) {  // 1 up, 2 down, 3 left, 4 right
  if (dir == 1) Move(-1);
  else if (dir == 2) Move(+1);
  else if (dir == 3) Side(-1);
  else if (dir == 4) Side(+1);
  g_m.pad_dir = dir;
  g_m.pad_next_repeat = SDL_GetTicks() + 380;
}

static void CapturePad(int btn) {
  if (btn < 0) return;
  Config_SetPadBinding(g_m.capture, btn);
  g_m.dirty = true;
  g_m.capture = -1;
}

bool Menu_HandlePadButton(int sdl_button, bool pressed) {
  if (!g_m.open) return false;
  int btn = PadButtonFromSdl(sdl_button);
  if (!pressed) {
    if (g_m.pad_dir && (btn == kGamepadBtn_DpadUp || btn == kGamepadBtn_DpadDown || btn == kGamepadBtn_DpadLeft || btn == kGamepadBtn_DpadRight))
      g_m.pad_dir = 0;
    return true;
  }
  if (g_m.capture >= 0) {
    if (g_m.page == P_GAMEPAD) CapturePad(btn);
    return true;
  }
  switch (btn) {
  case kGamepadBtn_DpadUp: PadDir(1); break;
  case kGamepadBtn_DpadDown: PadDir(2); break;
  case kGamepadBtn_DpadLeft: PadDir(3); break;
  case kGamepadBtn_DpadRight: PadDir(4); break;
  case kGamepadBtn_A: Ok(); break;
  case kGamepadBtn_B: Back(); break;
  case kGamepadBtn_Start: case kGamepadBtn_L3: case kGamepadBtn_R3: Menu_Close(); break;
  }
  return true;
}

void Menu_HandlePadAxis(int axis, int value) {
  if (!g_m.open) return;
  if (g_m.capture >= 0 && g_m.page == P_GAMEPAD && value > 20000) {
    if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) { CapturePad(kGamepadBtn_L2); return; }
    if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) { CapturePad(kGamepadBtn_R2); return; }
  }
  if (axis == SDL_CONTROLLER_AXIS_LEFTX) g_m.pad_axis_x = value;
  else if (axis == SDL_CONTROLLER_AXIS_LEFTY) g_m.pad_axis_y = value;
  else return;
  if (g_m.capture >= 0) return;
  int dir = g_m.pad_axis_y < -20000 ? 1 : g_m.pad_axis_y > 20000 ? 2 : g_m.pad_axis_x < -20000 ? 3 : g_m.pad_axis_x > 20000 ? 4 : 0;
  if (dir && dir != g_m.pad_dir) PadDir(dir);
  else if (!dir && g_m.pad_dir >= 1) g_m.pad_dir = 0;
}

void Menu_Update(uint32 now) {
  g_m.now = now;
  if (!g_m.open) return;
  if (g_m.capture >= 0 && now - g_m.capture_start > 5000)
    g_m.capture = -1;  // nothing pressed within 5 s
  if (g_m.pad_dir && now >= g_m.pad_next_repeat && g_m.capture < 0) {
    int d = g_m.pad_dir;
    PadDir(d);
    g_m.pad_next_repeat = now + 110;
  }
}

// ---------------------------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------------------------
void Menu_FrameShown(uint32 now) {
  g_m.fps_frames++;
  if (now - g_m.fps_t0 >= 1000) {
    g_m.fps = (int)(g_m.fps_frames * 1000.0 / (now - g_m.fps_t0) + 0.5);
    g_m.fps_frames = 0;
    g_m.fps_t0 = now;
  }
}

static void BindText(int bind, char *buf, int n) {
  if (g_m.page == P_KEYBOARD) Config_KeyName(Config_GetKeyBinding(bind), buf, n);
  else snprintf(buf, n, "%s", Config_PadButtonName(Config_GetPadBinding(bind)));
}

static int CheatsOn(int page) {
  if (page == P_CHEATS)
    return CheatsOn(P_CHEATS_MARIO) + CheatsOn(P_CHEATS_MOVES) + CheatsOn(P_CHEATS_ENEMIES) + CheatsOn(P_CHEATS_WORLD);
  if (page != P_CHEATS_MARIO && page != P_CHEATS_WORLD && page != P_CHEATS_MOVES && page != P_CHEATS_ENEMIES)
    return 0;
  const Item *items;
  int n = PageItems(page, &items), on = 0;
  for (int i = 0; i < n; i++) {
    char val[40];
    OptionText(items[i].id, val, sizeof(val));
    // (the knockback scale is off at 1X, its normal value)
    on += items[i].kind == K_OPTION && strcmp(val, "OFF") != 0 && strcmp(val, "1X") != 0;
  }
  return on;
}

// What the selected option does, shown at the bottom of the panel (at most two lines of 27 characters).
static const char *Describe(const Item *it) {
  switch (it->kind) {
  case K_LINK:
    switch (it->id) {
    case P_VIDEO: return "Window, fullscreen and how the picture looks.";
    case P_SOUND: return "Volumes, Mario Combat's sounds, stereo or mono.";
    case P_GAMEPLAY: return "Which Mario, his life bar, damage, pausing.";
    case P_CONTROLS: return "Keys, gamepad buttons, rumble and the stick.";
    case P_CHEATS: return "Cheats for Mario, his moves, enemies and levels.";
    case P_LEVELS: return "Play any level from its start, unused ones too.";
    case P_RESET: return "Put every setting back to how it started.";
    case P_QUIT: return "Close the game.";
    case P_KEYBOARD: return "Choose a key for each button.";
    case P_GAMEPAD: return "Choose a gamepad button for each button.";
    case P_CHEATS_MARIO: return "Protection, knockback, power-ups and Yoshi.";
    case P_CHEATS_MOVES: return "Jumps, dashes, attacks and moves for Mario.";
    case P_CHEATS_ENEMIES: return "More, faster, smarter and returning enemies.";
    case P_CHEATS_WORLD: return "Time, lives, the map, gravity and level effects.";
    case P_BOSS_RUSH: return "Fight the bosses back to back, against the clock.";
    }
    return "";
  case K_ACTION:
    switch (it->id) {
    case A_RUSH_START: return "Start the run. Your map and save aren't changed.";
    case A_MC_LEVEL: return "Mario Combat's own level, all the way to Bowser!";
    case A_RESUME: return "Back to the game.";
    case A_BACK: return "Back to the previous page.";
    case A_RESET_KEYS: return "Put the default keys back.";
    case A_RESET_PAD: return "Put the default gamepad buttons back.";
    case A_CHEATS_OFF: return "Turn every cheat off.";
    }
    return "";
  case K_BIND:
    return g_m.page == P_KEYBOARD ? "Press Enter, then the new key." : "Press A, then the new button.";
  case K_LGROUP:
    return LevelSelect_GroupNote(it->id);
  case K_LEVEL: {
    const LevelSelectEntry *e = LevelSelect_Entry(it->id);
    static char b[64];
    if (e->note) return e->note;
    snprintf(b, sizeof(b), "Level %03X from its start. Your map isn't changed.", e->level);
    return b;
  }
  }
  if (it->id >= O_CHEAT0) {
    switch (it->id - O_CHEAT0) {
    case kCheat_InfiniteLife: return "Mario Combat Mario's life bar never goes down.";
    case kCheat_NoDamage: return "Enemies and hazards can't hurt Mario. Pits can.";
    case kCheat_InfiniteTime: return "The level timer stops.";
    case kCheat_InfiniteLives: return "Lives never go down.";
    case kCheat_Invincible: return "Star power that never ends, without its music.";
    case kCheat_MoonJump: return "Hold the jump button to keep rising.";
    case kCheat_PSpeed: return "Run meter always full: take off with the cape.";
    case kCheat_Switches: return "All four switch palaces are pressed.";
    case kCheat_ReplayLevels: return "Enter beaten castles and fortresses again.";
    case kCheat_TripleEnemies: return "Every enemy comes with two more.";
    case kCheat_NoKnockback: return "Mario Combat Mario isn't knocked back by hits.";
    case kCheat_SuperPunch: return "Mario Combat's attacks beat any enemy at once.";
    case kCheat_AlwaysYoshi: return "Every level starts riding Yoshi.";
    case kCheat_SlowMotion: return "Levels run at half speed.";
    case kCheat_McKoopas: return "Koopas become Mario Combat's Koopas.";
    case kCheat_Mirror: return "Levels are flipped left to right.";
    case kCheat_OneHit: return "Any hit costs a life. A challenge!";
    case kCheat_EnemyCoins: return "Every defeated enemy drops a coin.";
    case kCheat_KingKoopas: return "Koopas become Bowser, Mario Combat's final boss!";
    case kCheat_DoubleJump: return "Both Marios can jump once more in the air.";
    case kCheat_GroundPound: return "Down in the air: slam down, knock enemies out.";
    case kCheat_WallJump: return "SMW Mario slides down walls and kicks off them.";
    case kCheat_Slippery: return "Every level is as slippery as an ice level.";
    case kCheat_CoinMagnet: return "Coins near Mario jump out to him.";
    case kCheat_FlyingYoshi: return "Yoshi has wings: tap jump to fly!";
    case kCheat_McHits: return "Enemies take MC hits: 10 life, knocked flying.";
    case kCheat_LifeSteal: return "Mario Combat Mario's hits give him life back.";
    case kCheat_FirePunch: return "Every punch and kick also throws a fireball.";
    case kCheat_InstantHits: return "Attacks hit at once: combo as fast as you press!";
    case kCheat_BlockBuster: return "Punches break grey and used blocks too.";
    case kCheat_PitRescue: return "A pit costs a hit, not a life: back to safe ground.";
    case kCheat_InfiniteJumps: return "Jump again in the air as often as you like.";
    case kCheat_OpenMap: return "Walk past unbeaten levels and locked paths.";
    case kCheat_StompAll: return "Stomp any enemy, even spiky ones.";
    case kCheat_PunchAll: return "Punches hurt Boos, Thwomps, Wigglers and more.";
    case kCheat_FastEnemies: return "Enemies move and act twice as fast.";
    case kCheat_ChaseEnemies: return "Walking enemies turn to walk toward Mario.";
    case kCheat_JumpEnemies: return "Walking enemies jump whenever Mario jumps.";
    case kCheat_LowGravity: return "Both Marios float: jumps go twice as high.";
    case kCheat_AirDash: return "In the air L dashes left, R right. Once a jump.";
    case kCheat_Respawn: return "Beaten enemies come back when you scroll back.";
    }
    return "";
  }
  switch (it->id) {
  case O_DISPLAY: return "Window, fullscreen or exclusive fullscreen.";
  case O_WINDOW: return "Size of the game window.";
  case O_ASPECT: return "Keep the SNES picture shape or stretch it.";
  case O_FILTER: return "Sharp or smoothed pixels.";
  case O_VSYNC: return "Wait for the monitor: no tearing.";
  case O_FPS: return "Frames per second in the top corner.";
  case O_VOLUME: return "Volume of everything.";
  case O_MCSOUNDS: return "Mario Combat's punch and hit sounds.";
  case O_MCVOLUME: return "Volume of Mario Combat's sounds.";
  case O_CHARACTER: return "Mario Combat Mario or SMW Mario (in a level).";
  case O_UPJUMP: return "Up jumps too, as in Mario Combat.";
  case O_LIFEBAR: return "Mario Combat Mario loses life, not power-ups.";
  case O_DAMAGE: return "How much of the life bar one hit takes.";
  case O_HITBOX: return "Show Mario Combat Mario's attack boxes.";
  case O_SAVE100: return "File 3 becomes a finished game. Yours is kept.";
  case O_POWERUP: return "Mario always has this power-up.";
  case O_KNOCKBACK: return "How far hits knock MC Mario: 1X normal to 20X.";
  case O_MONO: return "Stereo, or the same sound from both speakers.";
  case O_BACKGROUND: return "Switched to another window: play on, mute or pause.";
  case O_RUMBLE: return "Gamepad shakes on hits, falls and quakes.";
  case O_DEADZONE: return "How far the stick moves before Mario does.";
  case O_RUSH_BOSSES:
    return g_config.boss_rush_all ? "Every boss, Big Boo and Reznor too: 10 fights."
                                  : "The seven Koopa Kids, then Bowser: 8 fights.";
  case O_RUSH_DEATH:
    return g_config.boss_rush_end_on_death ? "A death ends the run." : "After a death, fight the same boss again.";
  }
  return "";
}

// Word-wraps s into lines of at most `cols` characters; returns the number of lines (at most max).
static int WrapText(const char *s, int cols, int max, int *start, int *len) {
  int n = 0, i = 0, L = (int)strlen(s);
  while (i < L && n < max) {
    while (s[i] == ' ') i++;
    int end = i + cols < L ? i + cols : L;
    if (end < L) {  // break at the last space that fits
      int b = end;
      while (b > i && s[b] != ' ') b--;
      if (b > i) end = b;
    }
    start[n] = i, len[n] = end - i, n++;
    i = end;
  }
  return n;
}

static void DrawMenu(void) {
  const Item *items;
  int n = PageItems(g_m.page, &items);
  // The CHEATS hub has taller rows for its page icons (SMW's own item sprites).
  const bool hub = g_m.page == P_CHEATS;
  const int row_h = hub ? 17 : 11, max_rows = 12;
  int vis = n < max_rows ? n : max_rows;
  int cur = g_m.cursor[g_m.page];
  if (cur < g_m.scroll) g_m.scroll = cur;
  if (cur >= g_m.scroll + vis) g_m.scroll = cur - vis + 1;
  bool confirm = g_m.page == P_RESET || g_m.page == P_QUIT;
  const int desc_h = confirm ? 0 : 25;  // a line, then two lines of the message-box font
  int w = 232, h = 30 + vis * row_h + desc_h + 16;
  int x = (256 - w) / 2, y = (224 - h) / 2;
  DimFrame();
  DrawPanel(x, y, w, h);
  const char *title = PageTitle(g_m.page);
  DrawText(x + (w - TextW(title)) / 2, y + 7, title, kColGold);
  FillRect(x + 8, y + 18, w - 16, 1, kColBorder2);
  for (int r = 0; r < vis; r++) {
    int i = g_m.scroll + r;
    const Item *it = &items[i];
    bool sel = i == cur;
    int ry = y + 24 + r * row_h + (hub ? 4 : 0);
    uint32 col = sel ? kColYellow : kColWhite;
    if (sel) DrawIcon(x + 7, ry, kIcon_Coin, kColYellow);  // SMW's status-bar coin as the cursor
    char label[40];
    snprintf(label, sizeof(label), "%s", it->label);
    if (confirm) {
      DrawText(x + (w - TextW(label)) / 2, ry, label, col);
      continue;
    }
    int lx = x + 20;
    if (hub && it->kind == K_LINK && g_items_ok) {  // the page's icon, then its name
      int icon = it->id == P_CHEATS_MARIO ? kItemMushroom : it->id == P_CHEATS_MOVES ? kItemFeather :
                 it->id == P_CHEATS_ENEMIES ? kItemShell : kItemStar;
      DrawItemIcon(lx, ry - 4, icon);
    }
    if (hub) lx += 20;
    DrawText(lx, ry, label, col);
    char val[40] = "";
    if (it->kind == K_OPTION) {
      OptionText(it->id, val, sizeof(val));
      if (it->id == O_CHARACTER && !InLevel()) col = sel ? kColYellow : kColGray;
    } else if (it->kind == K_BIND) {
      if (g_m.capture == it->id) {
        int left = 5 - (int)((g_m.now - g_m.capture_start) / 1000);
        snprintf(val, sizeof(val), g_m.page == P_KEYBOARD ? "PRESS KEY" : "PRESS BTN %d", left);
        col = (g_m.now / 250) & 1 ? kColYellow : kColWhite;
      } else {
        BindText(it->id, val, sizeof(val));
      }
    } else if (it->kind == K_LGROUP) {
      snprintf(val, sizeof(val), ">");
    } else if (it->kind == K_LEVEL) {  // the level number, or UNUSED for the unused levels
      const LevelSelectEntry *e = LevelSelect_Entry(it->id);
      if (e->group == kLevelSelectGroups - 1) snprintf(val, sizeof(val), "UNUSED");
      else snprintf(val, sizeof(val), "%03X", e->level);
      if (!sel) col = e->group == kLevelSelectGroups - 1 ? kColGold : kColGray;
    } else if (it->kind == K_LINK && it->id != P_RESET && it->id != P_QUIT) {
      int on = CheatsOn(it->id);  // cheat pages show how many of their cheats are on
      if (on) snprintf(val, sizeof(val), "%d ON >", on);
      else snprintf(val, sizeof(val), ">");
    }
    int maxlen = (w - 20 - TextW(label) - 20) / 8;
    if ((int)strlen(val) > maxlen && maxlen > 1) val[maxlen] = 0;
    int vx = x + w - 12 - TextW(val);
    // A cheat that is on gets SMW's bonus star in front of its value.
    bool cheat_on = it->kind == K_OPTION && (it->id >= O_CHEAT0 || it->id == O_POWERUP) && strcmp(val, "OFF") != 0;
    if (cheat_on && it->id == O_POWERUP && g_items_ok)  // the locked item itself, as SMW draws it
      DrawItemIcon(vx - (sel ? 30 : 19), ry - 4, (g_config.cheat_powerup & 3) - 1);
    else if (cheat_on)
      DrawIcon(vx - (sel ? 22 : 11), ry, kIcon_Star, kColYellow);
    if (sel && it->kind == K_OPTION) {
      DrawChar(vx - 10, ry, '<', kColYellow);
      DrawText(vx, ry, val, kColYellow);
    } else {
      // OFF in grey, so the options that are on stand out at a glance (e.g. on the CHEATS page)
      DrawText(vx, ry, val, it->kind == K_OPTION && !strcmp(val, "OFF") ? kColGray : col);
    }
  }
  if (g_m.scroll > 0) DrawChar(x + w - 12, y + 20, '<', kColGray);  // more above
  if (desc_h) {
    int dy = y + 24 + vis * row_h + 2;
    FillRect(x + 8, dy, w - 16, 1, kColBorder2);
    if (g_m.notice[0] && NoticeClock() < g_m.notice_until) {  // a notice takes the description's place
      DrawText(x + (w - TextW(g_m.notice)) / 2, dy + 8, g_m.notice, kColYellow);
    } else {
      const char *d = Describe(&items[cur]);
      int st[2], ln[2], cols = (w - 16) / 8;
      int nl = WrapText(d, cols, 2, st, ln);
      for (int l = 0; l < nl; l++)
        DrawMsgText(x + (w - ln[l] * 8) / 2, dy + 4 + l * 10, d + st[l], ln[l], kColWhite);
    }
  }
  const char *hint = g_m.capture >= 0 ? (g_m.page == P_KEYBOARD ? "ESC CANCEL" : "WAIT 5 SEC TO CANCEL")
                                       : "ENTER/A OK  ESC/B BACK";
  DrawText(x + (w - TextW(hint)) / 2, y + h - 12, hint, kColGray);
}

void Menu_DrawOverlay(uint8 *pixels, int pitch, int width, int height, int render_scale) {
  g_fb.px = pixels; g_fb.pitch = pitch; g_fb.w = width; g_fb.h = height; g_fb.s = render_scale > 0 ? render_scale : 1;
  if (!g_font_ok && !g_m.open && !g_config.show_fps)
    return;
  if (g_m.open)
    DrawMenu();
  if (g_config.show_fps && g_m.fps > 0) {  // nothing until the first one-second sample
    char b[16];
    snprintf(b, sizeof(b), "%d FPS", g_m.fps);
    DrawText(256 - 4 - TextW(b), 4, b, kColWhite);
  }
  char rush[40];  // a boss rush: which fight and the run's time, in the bottom corner
  if (!g_m.open && LevelSelect_BossRushHud(rush, sizeof(rush)))
    DrawText(6, 224 - 13, rush, kColWhite);
  if (g_m.notice[0] && NoticeClock() < g_m.notice_until && !g_m.open) {  // (in the menu: inside the panel)
    int w = TextW(g_m.notice) + 16;
    int x = (256 - w) / 2, y = 224 - 34;
    DrawPanel(x, y, w, 16);
    DrawText(x + 8, y + 4, g_m.notice, kColWhite);
  }
}
