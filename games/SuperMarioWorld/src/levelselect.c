// Level select (see levelselect.h).
//
// The catalogue comes from tools/scripts/level_catalogue.py over the game's own level data (pilot `levelscan`): the 92
// levels on the overworld map, the 132 rooms they lead to (screen exits and secondary exits), the rooms the game loads
// by itself (the 1-UP bonus game 000 / 100, Yoshi's wings area 0C8 / 1C8, the intro 0C5, the title screen's level
// 0C7), and what nothing reaches. Of the map's levels, the 16 Star Road warp tiles and other tiles that are never
// entered hold either the filler test level (99 bytes, in 277 slots) or a copy of Donut Plains 1 (016, 017) or a small
// room (108): they are listed with the unused levels. Rooms are not listed on their own: each is part of its level.
#include <stdio.h>
#include <string.h>
#include "levelselect.h"
#include "mc/mc_level.h"
#include "config.h"
#include "funcs.h"
#include "smw_rtl.h"
#include "variables.h"
#include "menu.h"
#include "mc/mc_smw.h"

enum { G_YI, G_DP, G_VD, G_TB, G_FOI, G_CI, G_VOB, G_STAR, G_SPECIAL, G_BONUS, G_UNUSED };

static const char *const kGroupNames[kLevelSelectGroups] = {
  "YOSHI'S ISLAND", "DONUT PLAINS", "VANILLA DOME", "TWIN BRIDGES", "FOREST OF ILLUSION", "CHOCOLATE ISLAND",
  "VALLEY OF BOWSER", "STAR WORLD", "SPECIAL ZONE", "BONUS ROOMS", "UNUSED LEVELS",
};
static const char *const kGroupNotes[kLevelSelectGroups] = {
  "The intro, Yoshi's House and the first island.", "The first big world of the main map.",
  "Under the ground, and its secret levels.", "The bridges, Soda Lake and Cookie Mountain.",
  "The forest's levels and Roy's castle.", "The island, its secrets and the ghost ship.",
  "The last world and Bowser's castle doors.", "The five Star World levels.", "The eight hardest levels.",
  "The bonus game and Yoshi's wings area.", "Levels in the game that are never reached.",
};

#define M(lv, g, tl, name) { lv, g, kLsKind_Map, tl, name, NULL }
static const LevelSelectEntry kEntries[] = {
  { 0x0C5, G_YI, kLsKind_Plain, 0, "INTRO", "Level 0C5: the welcome to Dinosaur Land." },
  M(0x104, G_YI, 0x28, "YOSHI'S HOUSE"),
  M(0x105, G_YI, 0x29, "YOSHI'S ISLAND 1"),
  M(0x014, G_YI, 0x14, "YELLOW SWITCH PALACE"),
  M(0x106, G_YI, 0x2A, "YOSHI'S ISLAND 2"),
  M(0x103, G_YI, 0x27, "YOSHI'S ISLAND 3"),
  M(0x102, G_YI, 0x26, "YOSHI'S ISLAND 4"),
  M(0x101, G_YI, 0x25, "IGGY'S CASTLE"),
  M(0x015, G_DP, 0x15, "DONUT PLAINS 1"),
  M(0x00A, G_DP, 0x0A, "DONUT SECRET 1"),
  M(0x013, G_DP, 0x13, "DONUT SECRET HOUSE"),
  M(0x10B, G_DP, 0x2F, "DONUT SECRET 2"),
  M(0x009, G_DP, 0x09, "DONUT PLAINS 2"),
  M(0x008, G_DP, 0x08, "GREEN SWITCH PALACE"),
  M(0x004, G_DP, 0x04, "DONUT GHOST HOUSE"),
  M(0x003, G_DP, 0x03, "TOP SECRET AREA"),
  M(0x005, G_DP, 0x05, "DONUT PLAINS 3"),
  M(0x006, G_DP, 0x06, "DONUT PLAINS 4"),
  M(0x007, G_DP, 0x07, "MORTON'S CASTLE"),
  M(0x11A, G_VD, 0x3E, "VANILLA DOME 1"),
  M(0x109, G_VD, 0x2D, "VANILLA SECRET 1"),
  M(0x001, G_VD, 0x01, "VANILLA SECRET 2"),
  M(0x002, G_VD, 0x02, "VANILLA SECRET 3"),
  M(0x00B, G_VD, 0x0B, "VANILLA FORTRESS"),
  M(0x118, G_VD, 0x3C, "VANILLA DOME 2"),
  M(0x11B, G_VD, 0x3F, "RED SWITCH PALACE"),
  M(0x107, G_VD, 0x2B, "VANILLA GHOST HOUSE"),
  M(0x10A, G_VD, 0x2E, "VANILLA DOME 3"),
  M(0x119, G_VD, 0x3D, "VANILLA DOME 4"),
  M(0x11C, G_VD, 0x40, "LEMMY'S CASTLE"),
  M(0x00F, G_TB, 0x0F, "CHEESE BRIDGE AREA"),
  M(0x011, G_TB, 0x11, "SODA LAKE"),
  M(0x010, G_TB, 0x10, "COOKIE MOUNTAIN"),
  M(0x00C, G_TB, 0x0C, "BUTTER BRIDGE 1"),
  M(0x00D, G_TB, 0x0D, "BUTTER BRIDGE 2"),
  M(0x00E, G_TB, 0x0E, "LUDWIG'S CASTLE"),
  M(0x11E, G_FOI, 0x42, "FOREST OF ILLUSION 1"),
  M(0x120, G_FOI, 0x44, "FOREST OF ILLUSION 2"),
  M(0x121, G_FOI, 0x45, "BLUE SWITCH PALACE"),
  M(0x123, G_FOI, 0x47, "FOREST OF ILLUSION 3"),
  M(0x11D, G_FOI, 0x41, "FOREST GHOST HOUSE"),
  M(0x11F, G_FOI, 0x43, "FOREST OF ILLUSION 4"),
  M(0x122, G_FOI, 0x46, "FOREST SECRET AREA"),
  M(0x01F, G_FOI, 0x1F, "FOREST FORTRESS"),
  M(0x020, G_FOI, 0x20, "ROY'S CASTLE"),
  M(0x022, G_CI, 0x22, "CHOCOLATE ISLAND 1"),
  M(0x021, G_CI, 0x21, "CHOCO-GHOST HOUSE"),
  M(0x024, G_CI, 0x24, "CHOCOLATE ISLAND 2"),
  M(0x117, G_CI, 0x3B, "CHOCOLATE SECRET"),
  M(0x023, G_CI, 0x23, "CHOCOLATE ISLAND 3"),
  M(0x01B, G_CI, 0x1B, "CHOCOLATE FORTRESS"),
  M(0x01D, G_CI, 0x1D, "CHOCOLATE ISLAND 4"),
  M(0x01C, G_CI, 0x1C, "CHOCOLATE ISLAND 5"),
  M(0x01A, G_CI, 0x1A, "WENDY'S CASTLE"),
  M(0x018, G_CI, 0x18, "SUNKEN GHOST SHIP"),
  M(0x116, G_VOB, 0x3A, "VALLEY OF BOWSER 1"),
  M(0x115, G_VOB, 0x39, "VALLEY OF BOWSER 2"),
  M(0x114, G_VOB, 0x38, "VALLEY GHOST HOUSE"),
  M(0x113, G_VOB, 0x37, "VALLEY OF BOWSER 3"),
  M(0x10F, G_VOB, 0x33, "VALLEY OF BOWSER 4"),
  M(0x111, G_VOB, 0x35, "VALLEY FORTRESS"),
  M(0x110, G_VOB, 0x34, "LARRY'S CASTLE"),
  M(0x10D, G_VOB, 0x31, "FRONT DOOR"),
  M(0x10E, G_VOB, 0x32, "BACK DOOR"),
  M(0x134, G_STAR, 0x58, "STAR WORLD 1"),
  M(0x130, G_STAR, 0x54, "STAR WORLD 2"),
  M(0x132, G_STAR, 0x56, "STAR WORLD 3"),
  M(0x135, G_STAR, 0x59, "STAR WORLD 4"),
  M(0x136, G_STAR, 0x5A, "STAR WORLD 5"),
  M(0x12A, G_SPECIAL, 0x4E, "GNARLY"),
  M(0x12B, G_SPECIAL, 0x4F, "TUBULAR"),
  M(0x12C, G_SPECIAL, 0x50, "WAY COOL"),
  M(0x12D, G_SPECIAL, 0x51, "AWESOME"),
  M(0x128, G_SPECIAL, 0x4C, "GROOVY"),
  M(0x127, G_SPECIAL, 0x4B, "MONDO"),
  M(0x126, G_SPECIAL, 0x4A, "OUTRAGEOUS"),
  M(0x125, G_SPECIAL, 0x49, "FUNKY"),
  { 0x000, G_BONUS, kLsKind_BonusGame, 0, "1-UP BONUS GAME", "Level 000: the bonus game after 100 bonus stars." },
  { 0x0C8, G_BONUS, kLsKind_Wings, 0, "YOSHI WINGS AREA", "Level 0C8: Yoshi's wings sky (main map levels)." },
  { 0x1C8, G_BONUS, kLsKind_Wings, 0, "YOSHI WINGS AREA 2", "Level 1C8: Yoshi's wings sky (the other maps)." },
  { 0x0C7, G_BONUS, kLsKind_Plain, 0, "TITLE SCREEN", "Level 0C7: the level behind the title screen." },
  { 0x019, G_UNUSED, kLsKind_Plain, 0, "TEST LEVEL", "Unused 019: the test level in 277 empty slots." },
  { 0x017, G_UNUSED, kLsKind_Plain, 0, "DONUT P. 1 COPY", "Unused 017 (and 016): copies of Donut Plains 1." },
  { 0x108, G_UNUSED, kLsKind_Plain, 0, "LEVEL 108", "Unused 108: a small room on a map tile never entered." },
  { 0x095, G_UNUSED, kLsKind_Plain, 0, "REZNOR ROOM 095", "Unused 095 (and 195): a Reznor fight." },
  { 0x098, G_UNUSED, kLsKind_Plain, 0, "BOSS ROOM 098", "Unused 098 (and 198): a Koopa Kid fight." },
  { 0x099, G_UNUSED, kLsKind_Plain, 0, "BOSS ROOM 099", "Unused 099 (and 199): a Koopa Kid fight." },
  { 0x09A, G_UNUSED, kLsKind_Plain, 0, "BOSS ROOM 09A", "Unused 09A (and 19A): a Koopa Kid fight." },
  { 0x093, G_UNUSED, kLsKind_Plain, 0, "BOSS ROOM 093", "Unused 093 (and 194): a copy of boss room 1F2." },
  { 0x094, G_UNUSED, kLsKind_Plain, 0, "BOSS ROOM 094", "Unused 094 (and 193): a copy of boss room 0D3." },
  { 0x096, G_UNUSED, kLsKind_Plain, 0, "BOSS ROOM 096", "Unused 096 (and 196): a copy of boss room 1EB." },
  { 0x097, G_UNUSED, kLsKind_Plain, 0, "BOSS ROOM 097", "Unused 097 (and 197): a copy of boss room 1F6." },
  { 0x09B, G_UNUSED, kLsKind_Plain, 0, "BOWSER ROOM 09B", "Unused 09B (and 19B): a copy of Bowser's room 1C7." },
};
#undef M

const char *LevelSelect_GroupName(int g) { return g >= 0 && g < kLevelSelectGroups ? kGroupNames[g] : ""; }
const char *LevelSelect_GroupNote(int g) { return g >= 0 && g < kLevelSelectGroups ? kGroupNotes[g] : ""; }
int LevelSelect_Count(void) { return (int)(sizeof(kEntries) / sizeof(kEntries[0])); }
const LevelSelectEntry *LevelSelect_Entry(int i) { return i >= 0 && i < LevelSelect_Count() ? &kEntries[i] : NULL; }

int LevelSelect_FindLevel(uint16 level) {
  for (int i = 0; i < LevelSelect_Count(); i++)
    if (kEntries[i].level == level)
      return i;
  return -1;
}

// ---------------------------------------------------------------------------------------------
// Practice runs: the map as it was, put back when the game returns to it
// ---------------------------------------------------------------------------------------------
enum { kSaveStart = 0x1EA2, kSaveLen = 0x8D };  // SMW's save buffer: level tiles, events, map positions, switches
static bool g_session;
static uint8 g_saved[kSaveLen], g_saved_level_lo;
static int g_pending = -1;     // entry to start on the next game frame
// The level started: every level load from its start (no room) takes it until the game is back on the map. SMW loads a
// castle or ghost house twice from its start: the door scene, then the level when Mario has walked in.
static bool g_take;
static uint16 g_take_level;
static uint8 g_take_translevel, g_take_kind;
// A start skips the map, and with it the map's clean-up of the level RAM (GameMode0C_LoadOverworld's
// ClearOverworldAndCutsceneRAM: $1A-$D7 and $13D3-$1BA1). Without it the level inherits the last room's state: mode 7
// rotation (a tilted Bowser who can't be hit), boss variables (Koopa Kids stuck on walls), the secondary-entrance flag
// (a level from a room behind a pipe started at the wrong place with the camera locked), scroll sprites, layer 3,
// timers... The same clean-up runs once the fade-out is done (game mode 10, the screen black), then the start's own
// settings in those ranges are put back.
static bool g_clear_pending, g_clear_off;  // (g_clear_off: pilot `lsclear 0`, to show what the clean-up fixes)
static uint8 g_launch_bonus, g_launch_wings;
static void RushAbandon(void);

bool LevelSelect_SessionActive(void) { return g_session; }

static void Snapshot(void) {
  memcpy(g_saved, g_ram + kSaveStart, kSaveLen);
  g_saved_level_lo = ow_level_number_lo;
  g_session = true;
}

// Back on the map (game mode 0C, before it loads): progress, map position and submap as before the first practice
// level, and nothing left over that the map would act on (a beaten level's events and path, a midway point, a Koopa
// Kid's castle cutscene).
static void RestoreMap(void) {
  memcpy(g_ram + kSaveStart, g_saved, kSaveLen);
  ow_level_number_lo = g_saved_level_lo;
  misc_exit_level_action = 0;
  flag_got_midpoint = 0;
  misc_currently_active_boss_end_cutscene = 0;
  ow_enter_level_flag = 0;
  g_session = false;
  g_take = false;
}

// Where a level can start now (the menu pauses the game, so this still holds on the next frame): in a level, or on
// the map with Mario standing still and nothing shown.
static const char *WhyNot(void) {
  uint8 m = misc_game_mode;
  if (m == 0x14)
    return NULL;
  if (m == 0x0E) {
    if (pointer_current_overworld_process != 3 || pointer_display_overworld_prompt || ow_warping_on_pipe_or_star_flag)
      return "WAIT UNTIL MARIO STANDS";
    return NULL;
  }
  if (m <= 0x0A)
    return "START OR LOAD A GAME FIRST";
  return "WAIT A MOMENT";
}

const char *LevelSelect_Request(int i) {
  if (!LevelSelect_Entry(i))
    return "NO SUCH LEVEL";
  const char *why = WhyNot();
  if (why)
    return why;
  g_pending = i;
  RushAbandon();  // a level chosen during a boss rush ends the run (the practice run goes on)
  McLevel_Abandon();  // (or during the Mario Combat level)
  return NULL;
}

static void Launch(const LevelSelectEntry *e) {
  if (!g_session)
    Snapshot();
  uint8 pl = player_current_character;
  if (misc_game_mode == 0x0E) {
    // from the map: what the map keeps per player becomes the level's, as SMW does when Mario enters a level
    player_current_coin_count = players_coins[pl];
    player_current_life_count = players_lives[pl];
    player_current_power_up = players_power_up[pl];
    yoshi_carry_over_levels_flag = players_has_yoshi[pl];
    player_current_item_box = players_item_box[pl];
  } else {
    // from a level: Mario keeps his Yoshi (as through a pipe); whatever the level was doing stops
    yoshi_carry_over_levels_flag = player_riding_yoshi_flag ? yoshi_current_yoshi_color : 0;
    timer_end_level = 0;
    timer_end_level_via_keyhole = 0;
    misc_display_message = 0;
    flag_pause = 0;
    misc_exit_level_action = 0;
    misc_currently_active_boss_end_cutscene = 0;
  }
  yoshi_current_yoshi_color = yoshi_carry_over_levels_flag;
  player_riding_yoshi_flag = yoshi_carry_over_levels_flag;
  flag_active_bonus_game = e->kind == kLsKind_BonusGame;
  in_yoshi_wings_bonus_area = e->kind == kLsKind_Wings;
  counter_sublevels_entered = 0;  // a level's start, not one of its rooms
  misc_intro_level_flag = 0;
  // The level number's bank (000-0FF main map, 100-1FF the other maps) is the bank of every room it leads to: SMW
  // takes it from the player's map (LoadLevel). It comes back with the map.
  ow_players_map[pl] = (e->level >> 8) & 1;
  ow_level_number_lo = e->translevel;
  ow_level_tile_settings[e->translevel] &= ~0x40;  // no midway start: from the beginning
  ow_enter_level_flag = 0;
  // (the bonus game and the wings area are loaded the game's own way, as a room of the current level number)
  g_take = e->kind == kLsKind_Map || e->kind == kLsKind_Plain;
  g_take_level = e->level;
  g_take_translevel = e->translevel;
  g_take_kind = e->kind;
  io_music_ch1 = 0x80;  // fade the music out, as when entering a level
  // The level's music starts on its load only while $0DDA is under 0x40 (GameMode11): the map clears it, but a boss
  // win, a goal or a death leaves it at 0xFF, and the next boss of a boss rush (no map in between) fought in silence.
  misc_music_register_backup = 0;
  McSmw_OnLevelSelect();
  g_clear_pending = true;
  g_launch_bonus = flag_active_bonus_game;
  g_launch_wings = in_yoshi_wings_bonus_area;
  misc_game_mode = 0x0F;  // the mosaic fade, then SMW's level load
}

// Game mode 10 after a start: the map's clean-up of the level RAM, then the start's settings that live there.
static void ClearAsTheMapDoes(void) {
  g_clear_pending = false;
  ClearOverworldAndCutsceneRAM();
  flag_active_bonus_game = g_launch_bonus;
  in_yoshi_wings_bonus_area = g_launch_wings;
  player_riding_yoshi_flag = yoshi_carry_over_levels_flag;  // (as the start set it: Yoshi comes along)
  counter_sublevels_entered = 0;
}

// ---------------------------------------------------------------------------------------------
// Boss rush: the bosses back to back in the order the game has them, each in its own room, inside one practice run
// ---------------------------------------------------------------------------------------------
// The boss rooms are read from the game's own data (pilot `bossrooms`: the rooms whose sprites hold a boss, and the
// map level whose exits lead to each; notes/DATA_NOTES.md). A fight is won when the boss's own end-of-level starts
// ($13C6 / the end-level timer), or for Bowser when he rises up to die; the next fight starts a moment later, before
// the castle cutscene. Lives, coins and the item box are the run's start values again at its end, as is the map.
// Reznor is fought once, in the first fortress (the other three are the same fight).
typedef struct RushBoss { uint16 room; uint8 translevel; bool castle; const char *name; } RushBoss;
static const RushBoss kRushBosses[] = {
  { 0x1F6, 0x25, true, "IGGY" },      // Iggy's Castle (101)
  { 0x0E4, 0x13, false, "BIG BOO" },  // Donut Secret House (013)
  { 0x0E5, 0x07, true, "MORTON" },    // Morton's Castle (007)
  { 0x0DF, 0x0B, false, "REZNOR" },   // Vanilla Fortress (00B)
  { 0x1F2, 0x40, true, "LEMMY" },     // Lemmy's Castle (11C)
  { 0x0D9, 0x0E, true, "LUDWIG" },    // Ludwig's Castle (00E)
  { 0x0CC, 0x20, true, "ROY" },       // Roy's Castle (020)
  { 0x0D3, 0x1A, true, "WENDY" },     // Wendy's Castle (01A)
  { 0x1EB, 0x34, true, "LARRY" },     // Larry's Castle (110)
  { 0x1C7, 0x31, true, "BOWSER" },    // Front / Back Door (10D / 10E)
};
enum { kRushAll = sizeof(kRushBosses) / sizeof(kRushBosses[0]), kRushWinFrames = 150 };

static struct {
  bool active, pending;
  uint8 order[kRushAll];
  int count, at;      // the fights of this run; the one being fought
  uint32 frames;      // run time (game frames, without SMW's pause)
  int won_wait;       // frames left after a win before the next fight
  // the player when the run started (given back at its end) and when this fight started (a retry's)
  uint8 lives, coins, item, power, yoshi, power_at_fight;
} g_rush;

int LevelSelect_BossRushCount(bool all) {
  int n = 0;
  for (int i = 0; i < kRushAll; i++)
    n += all || kRushBosses[i].castle;
  return n;
}

bool LevelSelect_BossRushActive(void) { return g_rush.active; }
static void RushAbandon(void) { g_rush.active = g_rush.pending = false; }
void LevelSelect_AbandonRush(void) { RushAbandon(); }
const char *LevelSelect_WhyNotStart(void) { return WhyNot(); }
void LevelSelect_LaunchRoom(uint16 room, uint8 translevel, const char *name) {
  LevelSelectEntry e = { room, G_UNUSED, kLsKind_Plain, translevel, name, NULL };
  Launch(&e);
}

static void RushTime(uint32 frames, char *buf, int n) {  // m:ss.cc (SMW runs 60.0988 frames a second)
  uint32 cs = (uint32)(frames * 100.0 / 60.0988);
  snprintf(buf, n, "%u:%02u.%02u", cs / 6000, cs / 100 % 60, cs % 100);
}

bool LevelSelect_BossRushHud(char *buf, int n) {
  if (!g_rush.active || g_rush.pending)
    return false;
  char t[16];
  RushTime(g_rush.frames, t, sizeof(t));
  snprintf(buf, n, "BOSS %d/%d  %s", IntMin(g_rush.at + 1, g_rush.count), g_rush.count, t);
  return true;
}

const char *LevelSelect_StartBossRush(void) {
  const char *why = WhyNot();
  if (why)
    return why;
  memset(&g_rush, 0, sizeof(g_rush));
  for (int i = 0; i < kRushAll; i++)
    if (g_config.boss_rush_all || kRushBosses[i].castle)
      g_rush.order[g_rush.count++] = (uint8)i;
  g_rush.active = g_rush.pending = true;
  g_pending = -1;
  McLevel_Abandon();
  return NULL;
}

static void LaunchBoss(void) {
  const RushBoss *b = &kRushBosses[g_rush.order[g_rush.at]];
  LevelSelectEntry e = { b->room, G_UNUSED, kLsKind_Plain, b->translevel, b->name, NULL };
  Launch(&e);
  // no Yoshi: he waits outside castles and fortresses
  yoshi_carry_over_levels_flag = yoshi_current_yoshi_color = player_riding_yoshi_flag = 0;
  g_rush.power_at_fight = player_current_power_up;
  g_rush.won_wait = 0;
  char t[40];
  snprintf(t, sizeof(t), "BOSS %d/%d: %s", g_rush.at + 1, g_rush.count, b->name);
  Menu_Notice(t);
}

// The run is over (cleared, or ended by a death): back to the map, which the practice run puts back as it was.
static void EndRun(void) {
  player_current_life_count = g_rush.lives;
  player_current_coin_count = g_rush.coins;
  player_current_item_box = g_rush.item;
  player_current_power_up = g_rush.power;
  player_current_state = 0;
  timer_end_level = 0;
  misc_currently_active_boss_end_cutscene = 0;
  flag_pause = 0;
  misc_exit_level_action = 0x80;  // as START + SELECT leaves a level: nothing counts as beaten
  yoshi_carry_over_levels_flag = g_rush.yoshi;  // (Yoshi waited outside the castles)
  io_music_ch1 = 0x80;
  misc_game_mode = 0x0B;  // fade out to the map
  g_rush.active = false;
}

static bool BossDefeated(void) {
  if (misc_currently_active_boss_end_cutscene || timer_end_level)
    return true;
  for (int k = 0; k < 12; k++)  // Bowser: rising up to die, the clown car breaking down, Peach (states 4-6)
    if (spr_current_status[k] == 8 && spr_spriteid[k] == 0xA0 && spr_table151c[k] >= 4 && spr_table151c[k] <= 6)
      return true;
  return false;
}

static void RushFrame(void) {
  uint8 m = misc_game_mode;
  if (g_rush.pending) {
    g_rush.pending = false;
    if (WhyNot()) {
      g_rush.active = false;
      return;
    }
    uint8 pl = player_current_character;
    g_rush.yoshi = m == 0x0E ? players_has_yoshi[pl] : player_riding_yoshi_flag ? yoshi_current_yoshi_color : 0;
    LaunchBoss();  // (from the map it takes the map's lives, coins, power-up and item box: those are the run's)
    g_rush.lives = player_current_life_count;
    g_rush.coins = player_current_coin_count;
    g_rush.item = player_current_item_box;
    g_rush.power = player_current_power_up;
    return;
  }
  if (m <= 0x0E || m >= 0x15) {  // left the boss rooms some other way (the map, the title...): the run is gone
    g_rush.active = false;
    return;
  }
  if (!flag_pause && !(g_rush.won_wait && g_rush.at + 1 == g_rush.count))
    g_rush.frames++;  // (the clock stops when the last boss falls)
  if (m != 0x14)
    return;  // a fight is loading
  char t[16], msg[48];
  if (g_rush.won_wait) {
    if (player_current_state == 9 && player_anim_timer <= 2) {  // fell after the win: the boss is beaten all the same
      player_current_state = 0;
      player_current_power_up = g_rush.power_at_fight;
      g_rush.won_wait = 1;
    }
    if (--g_rush.won_wait == 0) {
      if (++g_rush.at < g_rush.count)
        LaunchBoss();
      else
        EndRun();
    }
    return;
  }
  if (BossDefeated()) {
    g_rush.won_wait = kRushWinFrames;
    RushTime(g_rush.frames, t, sizeof(t));
    if (g_rush.at + 1 == g_rush.count)
      snprintf(msg, sizeof(msg), "BOSS RUSH CLEAR! %s", t);
    else
      snprintf(msg, sizeof(msg), "%s DOWN!  %s", kRushBosses[g_rush.order[g_rush.at]].name, t);
    Menu_Notice(msg);
    return;
  }
  // A death: just before SMW takes the life (PlayerState09_Death's last frames), the boss again or the run's end.
  if (player_current_state == 9 && player_anim_timer <= 2) {
    if (g_config.boss_rush_end_on_death) {
      RushTime(g_rush.frames, t, sizeof(t));
      snprintf(msg, sizeof(msg), "RUN OVER: %d/%d BOSSES  %s", g_rush.at, g_rush.count, t);
      Menu_Notice(msg);
      EndRun();
    } else {
      player_current_state = 0;
      player_current_power_up = g_rush.power_at_fight;
      player_current_life_count = g_rush.lives;
      LaunchBoss();
    }
  }
}

void LevelSelect_Frame(void) {
  if (g_rush.active)
    RushFrame();
  McLevel_Frame();
  if (g_clear_pending && misc_game_mode == 0x10 && !g_clear_off)
    ClearAsTheMapDoes();
  else if (g_clear_pending && (misc_game_mode < 0x0F || misc_game_mode > 0x10))
    g_clear_pending = false;  // (the start did not get to its load)
  if (g_take && misc_game_mode <= 0x0E && misc_game_mode != 0x0B)
    g_take = false;  // back on the map (or the title): map tiles choose the levels again
  if (g_session) {
    if (misc_game_mode == 0x0C)
      RestoreMap();
    else if (misc_game_mode <= 0x0A || misc_game_mode >= 0x1D)
      g_session = g_take = false;  // title, file select or the ending: the map it held is gone
  }
  if (g_pending >= 0) {
    const LevelSelectEntry *e = LevelSelect_Entry(g_pending);
    g_pending = -1;
    if (e && !WhyNot())
      Launch(e);
  }
}

void LevelSelect_DebugClear(bool on) { g_clear_off = !on; }

static int g_last_loaded = -1;
void LevelSelect_NoteLoaded(uint16 level) { g_last_loaded = level; }
int LevelSelect_LastLoaded(void) { return g_last_loaded; }

// A room started by number (a boss room, an unused room, the intro...) is not a level the map enters: no castle / ghost
// house door scene, neither LoadLevel's entrance room nor InitializeLevelRAM's walk through the door (player state 0A,
// which a boss room has no door for).
bool LevelSelect_SkipEntranceScene(void) {
  return g_take && g_take_kind != kLsKind_Map && !counter_sublevels_entered;
}

bool LevelSelect_TakeLevel(uint16 *level) {
  if (!g_take)
    return false;
  *level = g_take_level;
  // (LoadLevel has just set it from the map tile under Mario: the events, the midway point and the castle and ghost
  // house door scene go by it)
  ow_level_number_lo = g_take_translevel;
  return true;
}
