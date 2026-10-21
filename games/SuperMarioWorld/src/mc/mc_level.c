// Mario Combat's own level inside SMW (round 20): see mc_level.h.
#include "mc_level.h"
#include "mc_level_data.h"
#include "mc_level_art.h"
#include "mc_overlay.h"
#include "mc_smw.h"
#include "mc_koopa.h"
#include "mc_bowser.h"
#include "mc_audio.h"
#include "../funcs.h"
#include "../smw_rtl.h"
#include "../variables.h"
#include "../pilot.h"
#include "../levelselect.h"
#include "../menu.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { kRows = kMcLevelRows };  // a horizontal level screen: 0x1B0 bytes, 27 rows of 16 tiles
enum {
  kLiftFrames = 378,       // MC: the lift's 315 ticks at 50 a second
  kCageDelay = 40,         // frames at the top before the cages open (MC: 150 ticks of its timer... shortened, see below)
  kAreaClearWait = 70,     // a Koopa down: the next area after this many frames
  kWinWait = 300,          // Bowser down: the end after this many frames
};

static struct {
  bool active, pending;
  int area;                 // 0..kMcLevelAreaCount-1
  bool restart_load;        // the next level load starts `area` again (a start or a retry), not the next one
  int lift;                 // area 4: 0 waiting, 1 rising, 2 at the top (cages closed), 3 cages open, 4 cleared
  int lift_t, lift_y;       // frames of the ride; the lift's top (level px)
  int below_t;              // frames Mario has been down on the floor while the lift waits at the top
  bool cages;               // the cages are open (the lift came back down for Mario after that)
  int timer;
  int won;                  // frames since Bowser fell (0: not yet)
  uint32 anim;              // frames the level has run (not paused): MC's lava animation
  uint8 lives, coins, item, power;
  uint8 sprite_list[1 + 3 * 24 + 1];
  bool prompt;              // Mario stands at the exit: the arrow shows
  int prompt_x, prompt_y;
} g;

static const McLevelArea *Area(void) { return &kMcLevelAreas[g.area]; }

// The area's ground tiles (stone, brown blocks, spikes, pipe): page 1, but not the door, lava or air.
static bool SolidTile(uint16 t) { return t >= 0x100 && t < 0x200; }

bool McLevel_Active(void) { return g.active && !g.pending; }
int McLevel_AreaNumber(void) { return McLevel_Active() ? g.area + 1 : 0; }
bool McLevel_McKoopas(void) { return McLevel_Active() && g.area < kMcLevelAreaCount - 1; }
bool McLevel_Bowser(void) { return McLevel_Active() && g.area == kMcLevelAreaCount - 1; }

static int Index(int tx, int ty) {
  if (tx < 0 || ty < 0 || ty >= kRows || (tx >> 4) >= 32)
    return -1;
  return GetLevelLayoutPtr_Horizontal(tx >> 4) + (ty << 4) + (tx & 15);
}

void McLevel_SetMap16(int tx, int ty, uint16 tile) {
  int i = Index(tx, ty);
  if (i < 0)
    return;
  g_ram[i] = (uint8)tile;
  g_ram[0x10000 + i] = (uint8)(tile >> 8);
}

int McLevel_GetMap16(int tx, int ty) {
  int i = Index(tx, ty);
  return i < 0 ? -1 : g_ram[i] | g_ram[0x10000 + i] << 8;
}

// ---------------------------------------------------------------------------------------------
// The run: started from the level select, an area after another, ended by Bowser's fall
// ---------------------------------------------------------------------------------------------
const char *McLevel_StartAt(int area) {
  const char *why = LevelSelect_WhyNotStart();
  if (why)
    return why;
  memset(&g, 0, sizeof(g));
  LevelSelect_AbandonRush();
  g.active = g.pending = true;
  g.area = area >= 0 && area < kMcLevelAreaCount ? area : 0;
  return NULL;
}

const char *McLevel_Start(void) { return McLevel_StartAt(0); }

// MC's Bowser music (sound 560, which MC starts on Bowser's frame) in the last area, instead of SMW's castle music.
static bool g_music;
static void StopMusic(int fade_frames) {
  if (g_music)
    McAudio_StopMusic(fade_frames);
  g_music = false;
}

void McLevel_Abandon(void) {
  g.active = g.pending = false;
  StopMusic(0);
}

static void LaunchArea(void) {
  g.restart_load = true;
  LevelSelect_LaunchRoom(Area()->base_level, (uint8)(Area()->base_level & 0xFF), "MARIO COMBAT");
  // no Yoshi: MC has none (he waits outside castles anyway)
  yoshi_carry_over_levels_flag = yoshi_current_yoshi_color = player_riding_yoshi_flag = 0;
  char t[40];
  snprintf(t, sizeof(t), "MARIO COMBAT %d/%d", g.area + 1, kMcLevelAreaCount);
  Menu_Notice(t);
}

// Back to the map (which the practice run puts back as it was), with the run's start lives, coins and items.
static void EndRun(void) {
  player_current_life_count = g.lives;
  player_current_coin_count = g.coins;
  player_current_item_box = g.item;
  player_current_power_up = g.power;
  player_current_state = 0;
  timer_end_level = 0;
  flag_pause = 0;
  misc_exit_level_action = 0x80;  // as START + SELECT leaves a level
  io_music_ch1 = 0x80;
  misc_game_mode = 0x0B;  // fade out to the map
  g.active = false;
  StopMusic(30);
}

// The next area, through SMW's own room change (as a door or a pipe: every screen's exit leads to the next base room).
static void NextArea(void) {
  if (misc_game_mode != 0x14 || g.area + 1 >= kMcLevelAreaCount)
    return;
  io_sound_ch3 = 0x0F;  // SMW's door / pipe sound
  uint8 scr = HIBYTE(player_xpos);
  misc_subscreen_exit_entrance_number_lo[scr & 31] = (uint8)kMcLevelAreas[g.area + 1].base_level;
  IncrementSublevelsEnteredAndPrepareToLoadSublevel();
}

// GameMode11, right after LoadLevel chose the room: which area this load builds, and where Mario starts.
// MC's lava glow (its gradient fill over the lava in the ground shape: red, the gradient's middle on the surface, so
// half-clear there and clear 40 stage px up, over the ground too): per tile column, the y of the lava surface under it (-1: none). A column of ground standing in
// the lava (a pillar) takes its pool's surface.
enum { kGlowHeight = 13, kGlowAlpha = 128 };  // (40 stage px; alpha at the surface)
static int16 g_lava_top[32 * 16];

static void FindLavaTops(const McLevelArea *a) {
  int cmin = -1, cmax = -1, surface = -1;
  for (int c = 0; c < a->width && c < 32 * 16; c++) {
    g_lava_top[c] = -1;
    for (int r = 0; r < kRows; r++)
      if (a->tiles[r * a->width + c] == 0x004) {
        g_lava_top[c] = (int16)(r * 16);
        cmin = cmin < 0 ? c : cmin, cmax = c, surface = r * 16;
        break;
      }
  }
  for (int c = cmin; c >= 0 && c <= cmax; c++)  // (pillars in a pool: ground at the surface's row)
    if (g_lava_top[c] < 0 && SolidTile(a->tiles[(surface >> 4) * a->width + c]) &&
        ((c > 0 && g_lava_top[c - 1] >= 0) || g_lava_top[c + 1] >= 0 || (c > 1 && g_lava_top[c - 2] >= 0)))
      g_lava_top[c] = (int16)surface;
}

// SMW's camera scroll, before the layers' positions are set: no further than MC's camera goes (its scroll limits).
void McLevel_ClampCamera(void) {
  if (!McLevel_Active())
    return;
  const McLevelArea *a = Area();
  int x = (int16)mirror_current_layer1_xpos;
  if (x > a->cam_x1 - 256)
    x = a->cam_x1 - 256;
  if (x < a->cam_x0)
    x = a->cam_x0;
  mirror_current_layer1_xpos = (uint16)x;
}

void McLevel_AfterLoadLevel(void) {
  if (!McLevel_Active())
    return;
  if (g.restart_load)
    g.restart_load = false;
  else if (g.area + 1 < kMcLevelAreaCount) {
    g.area++;  // a room change inside the run (a door, the pipe, the lift): the next area
    char t[40];
    snprintf(t, sizeof(t), g.area + 1 == kMcLevelAreaCount ? "MARIO COMBAT %d/%d: BOWSER" : "MARIO COMBAT %d/%d",
             g.area + 1, kMcLevelAreaCount);
    Menu_Notice(t);
  }
  g.lift = 0, g.lift_t = 0, g.timer = 0, g.won = 0, g.below_t = 0, g.cages = false;
  g.lift_y = Area()->lift_y;
  const McLevelArea *a = Area();
  for (int i = 0; i < a->n_objs; i++)
    if (a->objs[i].kind == kMcObj_Spawn) {
      // MC's spawn point is the hero's registration point, his feet (onGround tests y + 2), usually a little inside the
      // ground (MC pushes the hero up out of it): Mario stands on the ground at that spot. SMW's player position is the
      // top left of a 16 x 32 box.
      int tx = a->objs[i].x >> 4, ty = a->objs[i].y >> 4;
      while (ty > 0 && SolidTile(a->tiles[ty * a->width + tx]))
        ty--;
      while (ty + 1 < kRows && !SolidTile(a->tiles[(ty + 1) * a->width + tx]))
        ty++;
      player_xpos = (uint16)(a->objs[i].x - 8);
      player_ypos = (uint16)((ty + 1) * 16 - 32);
    }
  flag_layer1_vert_scroll_level_setting = 1;
}

// The level's own tile for an area tile: MC's lava (004 / 005 in the tables) is air there: SMW's tiles 0-5 are water (he
// would swim in it); MC's lava kills instead (InLava).
static uint16 SmwTile(uint16 t) { return t == 0x004 || t == 0x005 ? 0x025 : t; }

// Mario in MC's lava (the area's own table: what SMW's tiles no longer say): the feet a little into it, or the middle.
static bool InLava(void) {
  const McLevelArea *a = Area();
  int tx = ((int16)player_xpos + 8) >> 4;
  for (int dy = 16; dy <= 28; dy += 12) {
    int ty = ((int16)player_ypos + dy) >> 4;
    if (tx >= 0 && tx < a->width && ty >= 0 && ty < kRows) {
      uint16 t = a->tiles[ty * a->width + tx];
      if (t == 0x004 || t == 0x005)
        return true;
    }
  }
  return false;
}

// LoadSublevel, after the base room's objects: the area's tiles over them, its size, the exits to the next area.
void McLevel_BuildArea(void) {
  if (!McLevel_Active())
    return;
  const McLevelArea *a = Area();
  int screens = a->width / 16;
  for (int s = 0; s < 32; s++)
    for (int ty = 0; ty < kRows; ty++)
      for (int tx = 0; tx < 16; tx++)
        McLevel_SetMap16(s * 16 + tx, ty, s < screens ? SmwTile(a->tiles[ty * a->width + s * 16 + tx]) : 0x25);
  // MC's invisible walls (its hero stays 22 stage px inside the scroll limits): a column of solid tiles the art doesn't
  // draw (130, as the bridge's) just past where Mario's middle may go, in the column's empty cells, the full height
  int walls[2] = { (a->wall_x0 - 6) / 16 - 1, (a->wall_x1 + 6 + 8) / 16 };
  for (int i = 0; i < 2; i++)
    if (walls[i] >= 0 && walls[i] < a->width)
      for (int ty = 0; ty < kRows; ty++)
        if (McLevel_GetMap16(walls[i], ty) == 0x25)
          McLevel_SetMap16(walls[i], ty, 0x130);
  FindLavaTops(a);
  misc_screens_in_lvl = (uint8)screens;
  camera_last_screen_horiz = (uint8)screens;
  camera_last_screen_vert = 1;
  misc_level_layout_flags = 0;
  flag_layer1_horiz_scroll_level_setting = 1;
  flag_layer1_vert_scroll_level_setting = 1;
  // SMW's tile layers stay for the collision but aren't shown: MC's own pictures are drawn instead (McLevel_ArtLine)
  mirror_main_screen_layers &= ~3;
  mirror_sub_screen_layers &= ~3;
  uint8 next = g.area + 1 < kMcLevelAreaCount ? (uint8)kMcLevelAreas[g.area + 1].base_level : 0;
  for (int s = 0; s < 32; s++) {
    misc_subscreen_exit_entrance_number_lo[s] = next;
    misc_subscreen_exit_properties[s] = 0;
  }
  flag_use_secondary_entrance = 0;
  if (g.area == kMcLevelAreaCount - 1) {  // Bowser's bridge: SMW's music fades and none starts (MC's plays: McLevel_Frame)
    io_music_ch1 = 0x80;
    misc_music_register_backup |= 0x40;
  }
}

// The area's sprites as an SMW sprite list (header, 3 bytes a sprite, 0xFF): MC's Koopas (SMW Koopas, which the
// forced MC Koopas / King Koopas turn into MC's), Bowser (a Koopa that becomes him).
const uint8 *McLevel_SpriteList(const uint8 *base) {
  if (!McLevel_Active())
    return base;
  const McLevelArea *a = Area();
  uint8 *p = g.sprite_list;
  *p++ = base[0];
  int n = 0;
  for (int i = 0; i < a->n_objs && n < 24; i++) {
    const McLevelObj *o = &a->objs[i];
    if (o->kind != kMcObj_Koopa && o->kind != kMcObj_Bowser)
      continue;
    int tx = o->x >> 4, ty = (o->y - 1) >> 4;  // the tile over its feet
    if (ty < 0) ty = 0;
    if (ty > 26) ty = 26;
    int scr = tx >> 4;
    p[0] = (uint8)((ty & 15) << 4 | (scr >> 4 & 1) << 1 | (ty >> 4 & 1));
    p[1] = (uint8)((tx & 15) << 4 | (scr & 15));
    p[2] = 0x04;  // green Koopa
    p += 3, n++;
  }
  *p = 0xFF;
  return g.sprite_list;
}

// ---------------------------------------------------------------------------------------------
// Every frame
// ---------------------------------------------------------------------------------------------
static bool Pressed(void) {  // the MC level's "space": jump (B / A), punch (Y) or up
  return (io_controller_press1 & 0xC8) || (io_controller_press2 & 0x80);
}

static bool OnGround(void) { return !player_in_air_flag || misc_player_on_solid_sprite; }

// Mario's feet (level px) and middle
static int FeetY(void) { return (int16)player_ypos + 32; }
static int MidX(void) { return (int16)player_xpos + 8; }

static void UpdatePrompt(void) {
  const McLevelArea *a = Area();
  g.prompt = false;
  if (a->exit == kMcExit_None || player_current_state || timer_end_level || (a->exit == kMcExit_Lift && g.lift))
    return;
  int ex = a->exit_x, ey = a->exit_y, x = MidX(), fy = FeetY();
  bool at;
  switch (a->exit) {
  case kMcExit_Door: at = x >= ex - 2 && x < ex + 18 && fy > ey + 16 && fy <= ey + 34; break;   // at the door
  case kMcExit_Pipe: at = x >= ex && x < ex + 32 && fy >= ey - 2 && fy <= ey + 2; break;        // on the pipe
  default: at = x >= ex - 16 && x < ex + 32 && fy >= ey + 28 && fy <= ey + 34; break;           // the lift's marker
  }
  if (!at || !OnGround() || (McSmw_Enabled() && McSmw_DebugHeroFrame() == 19))  // (MC: not while hurt)
    return;
  g.prompt = true;
  g.prompt_x = a->exit == kMcExit_Pipe ? ex + 16 : ex + 8;
  g.prompt_y = a->exit == kMcExit_Door ? ey - 4 : ey - 4 - (a->exit == kMcExit_Lift ? 0 : 16);
}

static void RedrawLevel(void) { InitializeLevelLayer1And2Tilemaps(); }

static void LiftTiles(int y, uint16 tile) {
  const McLevelArea *a = Area();
  for (int x = a->lift_x0; x < a->lift_x1; x += 16)
    McLevel_SetMap16(x >> 4, y >> 4, tile);
  RedrawLevel();
}

// The lift (area 4). States: 0 waiting in the floor, 1 rising (MC: 315 ticks), 2 at the top with the cages closed,
// 3 the cages open (the fight), 4 won (the next area a moment later), 5 going back down: Mario fell off it on the way
// up, or is down on the floor while it waits at the top (MC drops the floor there: a fall is the end; here the lift
// comes back for him).
static void LiftFrame(void) {
  const McLevelArea *a = Area();
  bool below = OnGround() && !player_current_state && FeetY() > g.lift_y + 24;
  switch (g.lift) {
  case 1:
    if (below) {
      g.lift = 5;
      break;
    }
    if (++g.lift_t >= kLiftFrames) {
      g.lift_y = a->lift_top;
      LiftTiles(g.lift_y, 0x132);  // a row of blocks again, at the top
      g.lift = g.cages ? 3 : 2, g.timer = 0, g.below_t = 0;
    } else {
      g.lift_y = a->lift_y - (a->lift_y - a->lift_top) * g.lift_t / kLiftFrames;
    }
    break;
  case 2:
    if (++g.timer >= kCageDelay) {
      // MC removes the ground with the cages: the two Koopas drop onto the lift
      g.lift = 3, g.timer = 0, g.cages = true;
      for (int ty = 0; ty < (a->lift_top >> 4) - 2; ty++)  // (the ledge reaches past the lift: all of it)
        for (int tx = (a->lift_x0 >> 4) - 6; tx < (a->lift_x1 >> 4) + 6; tx++)
          McLevel_SetMap16(tx, ty, 0x25);
      RedrawLevel();
    }
    break;
  case 3: {
    // MC: the area ends when the first caged Koopa has fallen past the lift (knocked off, or defeated: a defeated
    // MC Koopa falls through everything)
    int up = 0;
    for (int k = 0; k < 12; k++)
      if (spr_current_status[k] >= 8 && spr_spriteid[k] == 0x04 && !McKoopa_IsDead(k) &&
          (int16)GetSprYPos(k) < a->lift_top + 8 && (int16)GetSprXPos(k) >= a->lift_x0 - 48 &&
          (int16)GetSprXPos(k) < a->lift_x1 + 48)
        up++;
    if (++g.timer > 90 && up < 2) {
      g.lift = 4, g.timer = 0;
    } else if (below && ++g.below_t > 90) {
      LiftTiles(a->lift_top, 0x25);
      g.lift = 5, g.lift_t = kLiftFrames;
    } else if (!below) {
      g.below_t = 0;
    }
    break;
  }
  case 4:
    if (++g.timer >= kAreaClearWait)
      NextArea();
    break;
  case 5:
    if (--g.lift_t <= 0) {
      g.lift_y = a->lift_y;
      LiftTiles(g.lift_y, 0x132);
      g.lift = 0, g.lift_t = 0;
    } else {
      g.lift_y = a->lift_y - (a->lift_y - a->lift_top) * g.lift_t / kLiftFrames;
    }
    break;
  }
}
// ProcessNormalSprites' start (the player has moved; its on-sprite flag just cleared): the lift carries Mario as SMW's
// platforms do (the flag set: on the ground next frame, with its friction).
void McLevel_SpritePhase(void) {
  if (!McLevel_Active() || misc_game_mode != 0x14 || Area()->exit != kMcExit_Lift || (g.lift != 1 && g.lift != 5) ||
      flag_sprites_locked)
    return;
  const McLevelArea *a = Area();
  int top = g.lift_y;
  McSmw_LevelPlatformBox(a->lift_x0, top - 4, a->lift_x1 - a->lift_x0, 12, top);
  int x = (int16)player_xpos;
  if (x + 14 < a->lift_x0 || x + 2 > a->lift_x1 || (int8)player_yspeed < 0 || player_current_state)
    return;
  int fy = FeetY();
  if (fy < top - 4 || fy > top + 10)
    return;
  player_yspeed = 0;
  misc_player_on_solid_sprite = 3;
  player_ypos = (uint16)(top - 32);
}

void McLevel_Frame(void) {
  if (!g.active) {
    StopMusic(0);
    return;
  }
  uint8 m = misc_game_mode;
  if (g.pending) {
    g.pending = false;
    if (LevelSelect_WhyNotStart()) {
      g.active = false;
      return;
    }
    uint8 pl = player_current_character;
    LaunchArea();  // (from the map it takes the map's lives, coins, power-up and item box: those are the run's)
    g.lives = player_current_life_count, g.coins = player_current_coin_count;
    g.item = player_current_item_box, g.power = player_current_power_up;
    (void)pl;
    return;
  }
  if (m <= 0x0E || m >= 0x15) {  // left some other way (the map, the title...): the run is over
    g.active = false;
    StopMusic(0);
    return;
  }
  if (m != 0x14 || flag_pause)
    return;
  const McLevelArea *a = Area();
  if (!player_current_state && InLava())  // MC's lava: a death, as SMW's lava and MC's deadZone are
    DamagePlayer_Kill();
  // A death: just before SMW takes the life, the same area again (or, with no lives left, back to the map).
  if (player_current_state == 9 && player_anim_timer <= 2) {
    player_current_state = 0;
    if (!player_current_life_count) {
      Menu_Notice("MARIO COMBAT: NO LIVES LEFT");
      EndRun();
      return;
    }
    player_current_life_count--;
    player_current_power_up = g.power;
    StopMusic(20);
    LaunchArea();
    return;
  }
  if (g.won) {
    if (++g.won == 60)
      io_music_ch1 = 0x0B;  // SMW's "boss defeated" tune
    if (g.won >= kWinWait) {
      Menu_Notice("MARIO COMBAT CLEAR!");
      EndRun();
    }
    return;
  }
  if (g.area == kMcLevelAreaCount - 1) {
    for (int k = 0; k < 12; k++)
      if (spr_current_status[k] >= 8 && McBowser_IsDead(k)) {
        g.won = 1;
        StopMusic(50);  // the battle music fades
        Menu_Notice("BOWSER DOWN!");
      }
  }
  g.anim++;
  if (g.area == kMcLevelAreaCount - 1 && !g.won && !g_music) {
    McAudio_StartMusic(&kMcBowserMusic);
    g_music = true;
  }
  if (a->exit == kMcExit_Lift)
    LiftFrame();
  UpdatePrompt();
  if (g.prompt && Pressed()) {
    g.prompt = false;
    if (a->exit == kMcExit_Lift) {
      g.lift = 1, g.lift_t = 0;
      for (int x = a->lift_x0; x < a->lift_x1; x += 16)  // the lift leaves the floor
        McLevel_SetMap16(x >> 4, a->lift_y >> 4, 0x25);
      RedrawLevel();
      io_sound_ch3 = 0x0F;
    } else {
      NextArea();
    }
  }
}

// ---------------------------------------------------------------------------------------------
// What the level looks like: Mario Combat's own pictures (mc_level_art.c) instead of SMW's tile layers
// ---------------------------------------------------------------------------------------------
// a over b (RGB), a's weight w of 255
static uint32 Mix(uint32 a, uint32 b, uint32 w) {
  uint32 v = 255 - w;
  return ((a >> 16 & 255) * w + (b >> 16 & 255) * v) / 255 << 16 | ((a >> 8 & 255) * w + (b >> 8 & 255) * v) / 255 << 8 |
         ((a & 255) * w + (b & 255) * v) / 255;
}

// A BG scroll register (10 bits) as a full level position, near the game's own camera position.
static int Unwrap(uint16 scroll, int hint) {
  int d = (scroll - hint) & 0x3ff;
  return hint + (d >= 512 ? d - 1024 : d);
}

// The ground, from the tiles (so always exactly where the collision is), in MC's style. MC's stone: its weave framed by
// its 8 px bevel (MC's pictures, from the edge in: a dark line, light to dark, a dark line; the right side's band
// shades the other way), on every side that meets no stone, rounded at outer corners, turned at inner ones (each
// quarter of a tile looks at its two neighbours and the one between them). MC's brown block; the lift while it moves.
static const uint32 kFrameTop[8] = { 0x202020, 0xa0a898, 0x808878, 0x808878, 0x606858, 0x404838, 0x404838, 0x202020 };
static const uint32 kFrameBottom[8] = { 0x202020, 0xa0a898, 0x808878, 0x606858, 0x606858, 0x404838, 0x404838, 0x202020 };
static const uint32 kFrameRight[8] = { 0x202020, 0x404838, 0x404838, 0x606858, 0x808878, 0x808878, 0xa0a898, 0x202020 };

static bool IsStone(int tx, int ty) {  // (off the level: stone, no frame at its edges)
  int t = McLevel_GetMap16(tx, ty);
  return t < 0 || (t >= 0x15D && t <= 0x165);
}

static uint32 StonePixel(const McLevelPicture *p, int lx, int ly) {
  int tx = lx >> 4, ty = ly >> 4, px = lx & 15, py = ly & 15, qx = px >> 3, qy = py >> 3;
  bool h = !IsStone(tx + (qx ? 1 : -1), ty), v = !IsStone(tx, ty + (qy ? 1 : -1));
  int dh = qx ? 15 - px : px, dv = qy ? 15 - py : py;  // (0-7: from this quarter's outer sides)
  const uint32 *fh = qx ? kFrameRight : kFrameTop, *fv = qy ? kFrameBottom : kFrameTop;  // (left = top's)
  uint32 c;
  if (h && v) {  // an outer corner, rounded
    if (dh + dv < 2)
      return 0;
    c = dh + dv == 2 ? 0x202020 : dh < dv ? fh[dh] : fv[dv];
  } else if (h) {
    c = fh[dh];
  } else if (v) {
    c = fv[dv];
  } else if (!IsStone(tx + (qx ? 1 : -1), ty + (qy ? 1 : -1))) {  // an inner corner: the two bands meet
    int d = dh > dv ? dh : dv;
    c = dh > dv ? fh[d] : fv[d];
  } else {
    return p->stone[py * 16 + px];
  }
  return 0xff000000 | c;
}

// MC's brown block: the tile has its seam / outline on the top row and left column; MC's outline also closes a group of
// blocks on the right and at the bottom (where no brown block follows).
static uint32 BrownPixel(int px, int py, bool open_right, bool open_below) {
  if ((px == 15 && open_right) || (py == 15 && open_below))
    return kMcLevelBrown[0];
  return kMcLevelBrown[py * 16 + px];
}

static bool IsBrown(int tx, int ty) { return McLevel_GetMap16(tx, ty) == 0x132; }

// The ground's pixel at a level position (0: none: the picture shows there).
static uint32 Ground(const McLevelPicture *p, int lx, int ly) {
  const McLevelArea *a = Area();
  if (lx >= 0 && lx < p->w && ly >= 0 && ly < p->h) {
    int t = McLevel_GetMap16(lx >> 4, ly >> 4);
    if (t >= 0x15D && t <= 0x165)
      return StonePixel(p, lx, ly);
    if (t == 0x132)
      return BrownPixel(lx & 15, ly & 15, !IsBrown((lx >> 4) + 1, ly >> 4), !IsBrown(lx >> 4, (ly >> 4) + 1));
    if (a->tiles[(ly >> 4) * a->width + (lx >> 4)] == 0x004) {  // MC's lava surface (the level's tile there is air)
      int f = (int)(g.anim * 50 / 60 / 5 % 6);  // (6 shapes, 5 frames each at MC's 50 a second)
      return kMcLevelLava[f * 256 + (ly & 15) * 16 + (lx & 15)];
    }
  }
  if (a->exit == kMcExit_Lift && (g.lift == 1 || g.lift == 5) && lx >= a->lift_x0 && lx < a->lift_x1 &&
      ly >= g.lift_y && ly < g.lift_y + 16)
    return BrownPixel((lx - a->lift_x0) & 15, ly - g.lift_y, lx >= a->lift_x1 - 16, true);
  return 0;
}

// Behind the picture: the sky (areas 1, 5) or MC's castle wall, locked to the screen (MC's BG clip; area 4's scrolls
// one copy down while the lift rises, as MC's plays).
static uint32 Behind(const McLevelPicture *p, int ly, int sx, int line) {
  if (p->sky)
    return 0xff000000 | p->sky[ly < 0 ? 0 : ly >= p->h ? p->h - 1 : ly];
  float off = 0;
  if (Area()->exit == kMcExit_Lift)  // (BG 489: 239 frames for one copy, at the lift's 315 ticks a ride)
    off = kMcLevelWallPeriod * fminf(1.0f, g.lift_t * (315.0f / kLiftFrames) / 239.0f);
  int row = (int)fmodf(line - off + 4 * kMcLevelWallPeriod, kMcLevelWallPeriod);
  return kMcLevelWall.px[row * kMcLevelWall.w + sx];
}
void McLevel_ArtLine(int line, uint32 *dst, const uint16 *zbuf, uint16 hscroll, uint16 vscroll, int brightness,
                     bool mirror) {
  if (!McLevel_Active() || line < 0 || line >= 240)
    return;
  const McLevelPicture *p = &kMcLevelPictures[g.area];
  // the PPU's BG1 position for this line (what the hidden layer 1 would show): row vscroll + line + 1
  int cx = Unwrap(hscroll, (int16)mirror_current_layer1_xpos);
  int ly = Unwrap(vscroll, (int16)mirror_current_layer1_ypos) + line + 1;
  static uint32 row[256];
  memset(row, 0, sizeof(row));
  if (ly >= 0 && ly < p->h) {  // the picture's row from x = cx: its runs from the row's start
    const uint8 *r = p->rle + p->rows[ly], *end = p->rle + p->rows[ly + 1];
    int x = 0;
    for (; r < end && x < cx + 256; r += 2) {
      int x1 = x + r[0];
      for (int i = x > cx ? x : cx; i < x1 && i < cx + 256; i++)
        row[i - cx] = p->pal[r[1]];
      x = x1;
    }
  }
  for (int x = 0; x < 256; x++) {
    if (zbuf[x] != 0x0500)  // only the backdrop: sprites, MC Mario and the status bar stay
      continue;
    int sx = mirror ? 255 - x : x;  // (mirror-mode cheat: the PPU flipped the level's lines)
    uint32 c = Ground(p, cx + sx, ly);  // the ground in front, else the picture over the sky or the wall
    if (!(c >> 24)) {
      c = row[sx];
      if (c >> 24 < 255)
        c = Mix(c, Behind(p, ly, sx, line), c >> 24);
    }
    int lx = cx + sx, lt = lx >= 0 && lx < p->w ? g_lava_top[lx >> 4] : -1;
    if (lt >= 0 && ly < lt && ly >= lt - kGlowHeight)  // MC's lava glow, over everything but the sprites
      c = Mix(0xff0000, c, kGlowAlpha * (kGlowHeight - (lt - ly)) / kGlowHeight);
    dst[x] = Mix(c, 0, brightness * 255 / 15);  // (the screen's brightness: the fades)
  }
}

// The "press" arrow over an exit Mario stands at.
void McLevel_Draw(void) {
  if (!McLevel_Active() || misc_game_mode != 0x14)
    return;
  if (g.prompt) {
    int bob = (int)((counter_global_frames >> 2) % 8);
    bob = bob < 4 ? bob : 8 - bob;
    float m[6] = { 1, 0, 0, 1, (float)(g.prompt_x - kMcLevelArrow.w / 2 - (int16)mirror_current_layer1_xpos),
                   (float)(g.prompt_y - kMcLevelArrow.h - 4 + bob - (int16)mirror_current_layer1_ypos) };
    McOverlay_DrawImage(&kMcLevelArrow, m, NULL, 0xffff);
  }
}

// ---------------------------------------------------------------------------------------------
// Pilot helpers
// ---------------------------------------------------------------------------------------------
void McLevel_DebugGrid(void) {
  int cx = (int16)mirror_current_layer1_xpos >> 4, cy = (int16)mirror_current_layer1_ypos >> 4;
  printf("map16grid: tiles from %d,%d, camera %d,%d (frame %u)\n", cx, cy, (int16)mirror_current_layer1_xpos,
         (int16)mirror_current_layer1_ypos, Pilot_Frame());
  for (int ty = cy; ty < cy + 15; ty++) {
    printf("map16grid %2d:", ty);
    for (int tx = cx; tx < cx + 17; tx++)
      printf(" %03X", McLevel_GetMap16(tx, ty) & 0xFFF);
    printf("\n");
  }
  fflush(stdout);
}

void McLevel_DebugTiles(int first, int count) {
  int cx = (int16)mirror_current_layer1_xpos >> 4, cy = (int16)mirror_current_layer1_ypos >> 4;
  for (int ty = cy; ty < cy + 14; ty++)
    for (int tx = cx; tx < cx + 16; tx++)
      McLevel_SetMap16(tx, ty, 0x25);
  for (int i = 0; i < count; i++)
    McLevel_SetMap16(cx + 1 + (i % 7) * 2, cy + 1 + (i / 7) * 2, (uint16)(first + i));
  InitializeLevelLayer1And2Tilemaps();
  printf("mctiles: %03X.. at tile %d,%d, camera %d,%d (frame %u)\n", first, cx + 1, cy + 1,
         (int16)mirror_current_layer1_xpos, (int16)mirror_current_layer1_ypos, Pilot_Frame());
  fflush(stdout);
}

int McLevel_DebugStatus(char *buf, int n) {
  const McLevelArea *a = Area();
  return snprintf(buf, n, "{\"area\":%d,\"exit\":%d,\"exit_x\":%d,\"exit_y\":%d,\"prompt\":%s,\"lift\":%d,"
                  "\"lift_y\":%d,\"won\":%d}", McLevel_AreaNumber(), a->exit, a->exit_x, a->exit_y,
                  g.prompt ? "true" : "false", g.lift, g.lift_y, g.won);
}

void McLevel_DebugInfo(void) {
  printf("mclevel: active %d area %d lift %d lift_y %d prompt %d won %d (frame %u)\n", g.active, g.area + 1, g.lift,
         g.lift_y, g.prompt, g.won, Pilot_Frame());
  fflush(stdout);
}
