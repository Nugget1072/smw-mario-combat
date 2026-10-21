// Scripted test pilot: plays a joypad script, saves screenshots and a per-frame state log, then quits.
// Usage: smw.exe --pilot <script.txt> --out <dir>
// Script commands (one per line, '#' comments):
//   hold <buttons...>   buttons held from now on (B Y SELECT START UP DOWN LEFT RIGHT A X L R; none = release)
//   wait <frames>       run that many frames
//   shot <name>         save <out>/<name>.bmp of the current frame
//   log <file>          start writing <out>/<file> with one CSV row per frame; "log off" stops
//   savestate <n> / loadstate <n>   smw snapshot slot n (saves/save<n+1>.sav)
//   toggle              switch Mario Combat Mario <-> SMW Mario
//   menu                open the settings menu; menukey <SDL key name> / menupad <SDL button> drive it
//   quit
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "smw_rtl.h"
#include "common_rtl.h"
#include "variables.h"
#include "pilot.h"
#include "menu.h"
#include "config.h"
#include "funcs.h"
#include "mc/mc_smw.h"
#include "mc/mc_level.h"
#include "mc/mc_audio.h"
#include "cheats.h"
#include "levelselect.h"
#include "mc/mc_koopa.h"
#include "mc/mc_hits.h"
#include "mc/mc_bowser.h"
#include "assets/smw_assets.h"

static FILE *g_script, *g_log;
static char g_out[512];
static uint32 g_buttons;
static int g_wait;
static bool g_active, g_quit, g_shot_pending, g_rerender;
static char g_shot_name[128];
static FILE *g_rec;              // `record`: the raw video being written
static int g_rec_frames, g_rec_h;
static bool g_rerender_saving;
static uint32 g_frame;
static bool g_glide;
static int g_glide_x, g_glide_y;
static int g_scroll;  // scroll: 1 = flying through a horizontal level, 2/3 = a vertical level (up, then down)
static int g_seek_id = -1, g_seek_y = 160;  // seek <id> [y]: fly right until that sprite is near
static int g_near_dx, g_near_id;
static int g_watch_slot = -1;  // sprite slot logged in the CSV's w* columns (set by spawn)
enum { kAttackBoss = 0x1000 };  // `attack boss`: any boss
static int g_wait_mode = -1;     // `waitmode <hex>`: run until the game mode is this
static int g_attack_id = -1, g_attack_dx, g_attack_phase, g_attack_delay;
static bool g_attack_air;

static FILE *g_wav[2];

// Interactive play (tools/scripts/play.py): `follow <minutes>` makes the pilot wait at the end of its script for more
// lines (the game is frozen meanwhile), `status` writes status.json, `ack <id>` writes ack.txt once everything before
// it has run. The end of each level visit is tracked for the status ("goal", "secret goal", "died", "left").
static bool g_follow;
static uint32 g_follow_deadline, g_follow_ms;  // SDL ticks; the game quits when no command came for that long
static const char *g_level_result = "none";
static uint8 g_last_exit_action, g_prev_mode;
static bool g_died_in_level;

static void WriteFileAtomic(const char *name, const char *text) {
  char path[700], tmp[720];
  snprintf(path, sizeof(path), "%s/%s", g_out, name);
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE *f = fopen(tmp, "w");
  if (!f) return;
  fputs(text, f);
  fclose(f);
  remove(path);
  rename(tmp, path);
}

static void TrackLevelResult(void) {
  uint8 m = misc_game_mode;
  if (m == 0x10 && g_prev_mode != 0x10)  // a level entered from the map: a new visit
    g_died_in_level = false, g_last_exit_action = 0;
  if (m == 0x14 && player_current_state == 9)
    g_died_in_level = true;
  if (misc_exit_level_action && (m == 0x14 || m == 0x0B || m == 0x0C))  // set in the frame the level ends
    g_last_exit_action = misc_exit_level_action;
  if ((m == 0x0B || m == 0x0C) && g_prev_mode != m && (g_prev_mode >= 0x0F)) {  // back to the map
    g_level_result = g_died_in_level ? "died" : g_last_exit_action == 1 ? "goal" : g_last_exit_action == 2 ? "secret goal"
                     : g_last_exit_action == 0x80 ? "left" : "back to map";
    g_died_in_level = false;
    g_last_exit_action = 0;
  }
  g_prev_mode = m;
}

static void WriteStatus(void) {
  char s[6144], spr[2400] = "", mcl[200] = "";
  int n = 0;
  for (int i = 0; i < 12; i++) {
    if (spr_current_status[i] < 8) continue;
    int dx = (int16)GetSprXPos(i) - (int16)player_xpos, dy = (int16)GetSprYPos(i) - (int16)player_ypos;
    if (abs(dx) > 256 || abs(dy) > 224) continue;
    n += snprintf(spr + n, sizeof(spr) - n, "%s{\"id\":\"%02X\",\"dx\":%d,\"dy\":%d,\"slot\":%d,\"st\":%d,\"c2\":%d,"
                  "\"t151c\":%d,\"xs\":%d,\"ys\":%d}", n ? "," : "", spr_spriteid[i], dx, dy, i, spr_current_status[i],
                  spr_table00c2[i], spr_table151c[i], (int8)spr_xspeed[i], (int8)spr_yspeed[i]);
    if (n >= (int)sizeof(spr) - 80) break;
  }
  // the floor row under Mario's feet across the screen: 16 tiles, '1' where a tile is solid on top (map16 100-16D)
  char floor[17] = "";
  for (int t = 0; t < 16; t++) {
    int m = McSmw_LevelMap16At((int16)mirror_current_layer1_xpos + t * 16 + 8, (int16)player_ypos + 33);
    floor[t] = m >= 0x100 && m < 0x16E ? '1' : '0';
  }
  char rush[64] = "", ext[600] = "";
  LevelSelect_BossRushHud(rush, sizeof(rush));
  McLevel_DebugStatus(mcl, sizeof(mcl));
  int en = 0;  // extended sprites (fireballs, hammers, bones...) near Mario
  for (int i = 0; i < 10; i++) {
    if (!ext_spr_spriteid[i]) continue;
    int dx = (int16)(ext_spr_xpos_hi[i] << 8 | ext_spr_xpos_lo[i]) - (int16)player_xpos;
    int dy = (int16)(ext_spr_ypos_hi[i] << 8 | ext_spr_ypos_lo[i]) - (int16)player_ypos;
    if (abs(dx) > 256 || abs(dy) > 224) continue;
    en += snprintf(ext + en, sizeof(ext) - en, "%s{\"id\":%d,\"dx\":%d,\"dy\":%d,\"xs\":%d,\"ys\":%d}", en ? "," : "",
                   ext_spr_spriteid[i], dx, dy, (int8)ext_spr_xspeed[i], (int8)ext_spr_yspeed[i]);
    if (en >= (int)sizeof(ext) - 60) break;
  }
  snprintf(s, sizeof(s),
           "{\"frame\":%u,\"game_mode\":\"%02X\",\"in_level\":%s,\"last_level_result\":\"%s\",\"x\":%d,\"y\":%d,"
           "\"screen_x\":%d,\"screen_y\":%d,\"on_ground\":%s,\"player_state\":%d,\"power_up\":%d,\"mc_mario\":%s,"
           "\"mc_life\":%.2f,\"lives\":%d,\"coins\":%d,\"timer\":%d%d%d,\"riding_yoshi\":%s,\"message_box\":%s,"
           "\"paused\":%s,\"rush\":\"%s\",\"hero_frame\":%d,\"hurt_timer\":%d,\"facing_right\":%s,\"floor\":\"%s\",\"reznor_broken\":%d,\"boss_punches\":%d,\"blocked\":%d,\"on_solid_sprite\":%d,\"mcl\":%s,\"sprites_near\":[%s],\"ext_near\":[%s]}\n",
           g_frame, misc_game_mode, misc_game_mode == 0x14 ? "true" : "false", g_level_result, (int16)player_xpos,
           (int16)player_ypos, (int16)player_xpos - (int16)mirror_current_layer1_xpos,
           (int16)player_ypos - (int16)mirror_current_layer1_ypos, player_in_air_flag ? "false" : "true",
           player_current_state, player_current_power_up, McSmw_Enabled() ? "true" : "false", McSmw_GetLife(),
           (int8)player_current_life_count + 1, player_current_coin_count, counter_timer_hundreds, counter_timer_tens,
           counter_timer_ones, player_riding_yoshi_flag ? "true" : "false", misc_display_message ? "true" : "false",
           flag_pause ? "true" : "false", rush, McSmw_DebugHeroFrame(), timer_player_hurt,
           player_facing_direction ? "true" : "false", floor, counter_number_of_broken_reznor_bridge_tiles, McSmw_DebugBossPunches(), player_blocked_flags,
           misc_player_on_solid_sprite, mcl, spr, ext);
  WriteFileAtomic("status.json", s);
}

// Level catalogue (pilot `levelscan`, for tools/scripts/level_catalogue.py): every level number 000-1FF with a
// fingerprint of its data (size and FNV-1a hash of the layer-1 objects, layer 2 and sprite list), its header bytes, its
// secondary-exit table entry, and the screen exits in its layer-1 objects (extended object 0: screen, exit number,
// high bit, secondary flag).
static uint32 Fnv(const uint8 *p, int n) {
  uint32 h = 2166136261u;
  for (int i = 0; i < n; i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}

static void PrintLevelScan(void) {
  for (int lv = 0; lv < 0x200; lv++) {
    MemBlk l1 = kLevelData_Layer1(lv), l2 = kLevelData_Layer2(lv);
    const uint8 *spr = kLvlSprBlob((uint32)kLmSpritePtrBankByte[lv] << 16 | kLoadLevel_SpriteDataPtrs[lv]);
    int sn = 1;
    while (spr && sn < 3 * 128 && spr[sn] != 0xFF)
      sn += 3;
    printf("level %03X l1 %d %08X l2 %d %08X spr %d %08X hdr %02X %02X %02X %02X tileset %d sec %02X %02X %02X exits",
           lv, (int)l1.size, Fnv(l1.ptr, (int)l1.size), (int)l2.size, Fnv(l2.ptr, (int)l2.size), (sn - 1) / 3,
           spr ? Fnv(spr, sn) : 0, kLevelInfo_05F000[lv], kLevelInfo_05F200[lv], kLevelInfo_05F400[lv],
           kLevelInfo_05F600[lv], l1.size > 4 ? l1.ptr[4] & 0xF : -1, kLevelInfo_05F800[lv], kLevelInfo_05FA00[lv],
           kLevelInfo_05FC00[lv]);
    const uint8 *p = l1.ptr + 5, *end = l1.ptr + l1.size;
    while (p + 2 < end && *p != 0xFF) {
      uint8 b0 = p[0], b1 = p[1], b2 = p[2];
      int obj = (b1 >> 4) | ((b0 & 0x60) >> 1);
      if (obj == 0 && b2 == 0 && p + 3 < end) {
        printf(" %d:%02X:%d:%d", b0 & 0x1F, p[3], b1 & 1, b1 >> 1 & 1);
        p += 4;
      } else {
        p += 3;
      }
    }
    printf("\n");
  }
  // the translevels on the overworld maps (the map's level-number table, 0x800 tiles: main map and submaps)
  printf("maplevels");
  for (int t = 1; t < 0x60; t++) {
    int n = 0;
    for (int i = 0; i < 0x800; i++)
      n += ow_level_number_of_each_tiletbl[i] == t;
    if (n)
      printf(" %02X:%d", t, n);
  }
  printf("\n");
  fflush(stdout);
}

// The rooms whose sprite data holds a boss: Koopa Kid 29 (its kind is the sprite's y tile, Spr029_KoopaKids_Init:
// 0 Morton 1 Roy 2 Ludwig 3 Iggy 4 Larry 5 Lemmy 6 Wendy), Bowser A0, Reznor A9, Big Boo boss C5.
static void PrintBossRooms(void) {
  for (int lv = 0; lv < 0x200; lv++) {
    const uint8 *spr = kLvlSprBlob((uint32)kLmSpritePtrBankByte[lv] << 16 | kLoadLevel_SpriteDataPtrs[lv]);
    for (int sn = 1; spr && sn < 3 * 128 && spr[sn] != 0xFF; sn += 3) {
      uint8 id = spr[sn + 2];
      if (id == 0x29 || id == 0xA0 || id == 0xA9 || id == 0xC5)
        printf("bossroom %03X id %02X kind %d\n", lv, id, spr[sn] >> 4);
    }
  }
  fflush(stdout);
}

// Every room whose sprite data holds sprite id, with its position (horizontal-level reading of SMW's 3-byte entries:
// yyyyEESY XXXXssss id: screen = ssss + S * 16, x = screen * 256 + XXXX * 16, y = (Y * 16 + yyyy) * 16; in a vertical
// level the screen counts down the level instead).
static void PrintSpriteFind(uint8 want) {
  for (int lv = 0; lv < 0x200; lv++) {
    const uint8 *spr = kLvlSprBlob((uint32)kLmSpritePtrBankByte[lv] << 16 | kLoadLevel_SpriteDataPtrs[lv]);
    for (int sn = 1; spr && sn < 3 * 128 && spr[sn] != 0xFF; sn += 3) {
      if (spr[sn + 2] != want)
        continue;
      uint8 b0 = spr[sn], b1 = spr[sn + 1];
      int screen = (b1 & 0xF) | (b0 & 2) << 3;
      printf("spritefind %02X level %03X screen %d x %d y %d extra %d\n", want, lv, screen,
             screen * 256 + (b1 >> 4) * 16, ((b0 & 1) * 16 + (b0 >> 4)) * 16, b0 >> 2 & 3);
    }
  }
  fflush(stdout);
}

static void PrintLevelNames(void) {
  // Layer-3 overworld font: tiles 00-19 A-Z, 1A-1D ? !..., digits from 22; unknown tiles print as '~'
  static const char kMap[0x80] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  for (int t = 1; t < 0x60; t++) {
    uint8 tiles[64];
    int n = SmwLevelNameTiles((uint8)t, tiles, sizeof(tiles));
    char name[80];
    int k = 0;
    for (int i = 0; i < n && k < 70; i++) {
      uint8 c = tiles[i];
      if (c >= 0x32 && c <= 0x37) { name[k++] = "ILLUSI"[c - 0x32]; continue; }  // FOREST OF ILLUSION's own tiles
      if (c >= 0x38 && c <= 0x3C) { const char *y = c == 0x3C ? "OW" : "YELL" + (c - 0x38); name[k++] = *y; if (c == 0x3C) name[k++] = 'W'; continue; }
      name[k++] = c < 26 ? kMap[c] : c == 0x1F ? ' ' : c == 0x1C ? '-' : c == 0x5A ? '#' : c == 0x5D ? '\'' :
                  c >= 0x64 && c <= 0x6C ? '1' + c - 0x64 : '~';
    }
    while (k > 0 && name[k - 1] == ' ') k--;
    name[k] = 0;
    printf("translevel %02X level %03X raw", t, t < 0x25 ? t : t + 0xDC);
    for (int i = 0; i < n; i++) printf(" %02X", tiles[i]);
    printf(" name \"%s\"\n", name);
  }
  fflush(stdout);
}

static FILE *StartWav(const char *path) {
  FILE *f = fopen(path, "wb");
  if (f) {
    uint8 hdr[44] = { 0 };
    fwrite(hdr, 1, 44, f);  // filled in by FinishWav
  }
  return f;
}

static void FinishWav(FILE *f) {
  long size = ftell(f) - 44;
  uint8 h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0 };
  *(uint32 *)(h + 4) = (uint32)(size + 36);
  *(uint32 *)(h + 24) = 32000;
  *(uint32 *)(h + 28) = 32000 * 4;
  *(uint16 *)(h + 32) = 4;
  *(uint16 *)(h + 34) = 16;
  memcpy(h + 36, "data", 4);
  *(uint32 *)(h + 40) = (uint32)size;
  fseek(f, 0, SEEK_SET);
  fwrite(h, 1, 44, f);
  fclose(f);
}

static void WriteAudioFrame(void) {
  int16 smw[534 * 2], mc[534 * 2];
  memset(smw, 0, sizeof(smw));
  memset(mc, 0, sizeof(mc));
  RtlRenderAudio(smw, 534, 2);
  McAudio_Mix(mc, 534, 2, 32000, g_config.mc_sound_volume);
  McAudio_MixMusic(smw, 534, 2, 32000, 100);  // (MC's music stands in for SMW's: with it)
  Main_PostProcessAudio(smw, 534);  // the MONO / IN BACKGROUND = MUTE settings, as the game's own output
  Main_PostProcessAudio(mc, 534);
  fwrite(smw, sizeof(smw), 1, g_wav[0]);
  fwrite(mc, sizeof(mc), 1, g_wav[1]);
}

bool Pilot_Init(int *argc, char ***argv) {
  for (int i = 0; i + 1 < *argc; i++) {
    if (!strcmp((*argv)[i], "--pilot")) {
      g_script = fopen((*argv)[i + 1], "r");
      if (!g_script) { fprintf(stderr, "pilot: cannot open %s\n", (*argv)[i + 1]); exit(2); }
      g_active = true;
    } else if (!strcmp((*argv)[i], "--out")) {
      snprintf(g_out, sizeof(g_out), "%s", (*argv)[i + 1]);
    } else {
      continue;
    }
    memmove(&(*argv)[i], &(*argv)[i + 2], (*argc - i - 2 + 1) * sizeof(char *));  // + the NULL terminator
    *argc -= 2;
    i--;
  }
  if (g_active && !g_out[0])
    strcpy(g_out, ".");
  return g_active;
}

bool Pilot_Active(void) { return g_active; }
uint32 Pilot_Frame(void) { return g_frame; }
bool Pilot_WantQuit(void) { return g_quit; }
bool Pilot_WantShot(void) { return g_shot_pending || g_rec; }

static uint32 ParseButtons(char *s) {
  static const char *const kNames[12] = { "B", "Y", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT", "A", "X", "L", "R" };
  uint32 b = 0;
  for (char *t = strtok(s, " \t\r\n"); t; t = strtok(NULL, " \t\r\n"))
    for (int i = 0; i < 12; i++)
      if (!strcmp(t, kNames[i]))
        b |= 1 << i;
  return b;
}

// Advance the script until it asks to run frames. Returns the joypad bits for this frame.
uint32 Pilot_BeginFrame(void) {
  char line[256];
  while (g_wait == 0 && !g_quit && !g_shot_pending) {
    long at = ftell(g_script);
    if (!fgets(line, sizeof(line), g_script) || (g_follow && !strchr(line, '\n'))) {
      if (!g_follow) { g_quit = true; break; }
      // follow mode: wait (game frozen) for play.py to append the next commands; a half-written line is read again
      clearerr(g_script);
      fseek(g_script, at, SEEK_SET);
      if (SDL_GetTicks() > g_follow_deadline) { g_quit = true; break; }
      SDL_PumpEvents();
      SDL_Delay(15);
      continue;
    }
    if (g_follow)
      g_follow_deadline = SDL_GetTicks() + g_follow_ms;
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#' || *p == '\n' || *p == '\r' || !*p) continue;
    if (!strncmp(p, "hold", 4)) {
      g_buttons = ParseButtons(p + 4);
    } else if (!strncmp(p, "wait", 4) && strncmp(p, "waitnear", 8) && strncmp(p, "waitmode", 8)) {
      g_wait = atoi(p + 4);
    } else if (!strncmp(p, "shotre", 6)) {
      // shotre <name>: the shot, then the same frame drawn again without running the game (as slow motion's skipped
      // frames and the open menu show it), saved as <name>_re
      sscanf(p + 6, "%120s", g_shot_name);
      g_shot_pending = true;
      g_rerender = true;
    } else if (!strncmp(p, "shot", 4)) {
      sscanf(p + 4, "%127s", g_shot_name);
      g_shot_pending = true;
    } else if (!strncmp(p, "log", 3)) {
      char name[128] = "";
      sscanf(p + 3, "%127s", name);
      if (g_log) fclose(g_log), g_log = NULL;
      if (strcmp(name, "off")) {
        char path[700];
        snprintf(path, sizeof(path), "%s/%s", g_out, name);
        g_log = fopen(path, "w");
        if (g_log)
          fprintf(g_log, "frame,mode,px,py,psubx,psuby,pxs,pys,inair,blocked,state,power,l1x,l1y,"
                         "mcx,mcy,mcxs,mcgrav,mcframe,mcscale,mcatt,mchurt,mcground,mcwall,mcpuppet,mckeys,mcshorthop,"
                         "mcphase,mcvx,mcvy,mclife,nhit,hx0,hy0,hx1,hy1,spr,sprx,spry,sprstatus,hitlvl,msg,msgdir,msgwait,"
                         "locked,pmeter,carry,flying,capetimer,spin,wid,wstatus,wx,wy,wc2,onspr\n");
      }
    } else if (!strncmp(p, "findtiles0", 10)) {  // findtiles0 lo hi: page-0 map16 tiles (coins 2B ...)
      unsigned lo = 0, hi = 0;
      sscanf(p + 10, "%x %x", &lo, &hi);
      McSmw_DebugFindTiles(lo | 0x8000, (hi ? hi : lo) | 0x8000);
    } else if (!strncmp(p, "findtiles", 9)) {
      unsigned lo = 0, hi = 0;
      sscanf(p + 9, "%x %x", &lo, &hi);
      McSmw_DebugFindTiles(lo, hi ? hi : lo);
    } else if (!strncmp(p, "attackframes", 12)) {  // the MC attack clip's moves and their hit-box frames
      McSmw_DebugAttackFrames();
    } else if (!strncmp(p, "attack", 6)) {
      // attack <sprite id hex> <reach px> [air]: walk toward the nearest such sprite until it is within reach, then
      // tap punch facing it (with "air": jump first and spin-kick). Gives up after 400 frames. The id "boss" takes the
      // nearest boss of any kind (Koopa Kid 29, Bowser A0, Reznor A9, Big Boo boss C5).
      char air[8] = "";
      g_attack_id = -1;
      g_attack_dx = 24;
      g_attack_delay = 6;
      if (!strncmp(p + 6, " boss", 5)) {
        g_attack_id = kAttackBoss;
        sscanf(p + 11, "%d %7s %d", &g_attack_dx, air, &g_attack_delay);
      } else {
        sscanf(p + 6, "%x %d %7s %d", &g_attack_id, &g_attack_dx, air, &g_attack_delay);
      }
      g_attack_air = !strcmp(air, "air");
      g_attack_phase = 0;
      g_wait = 400;
    } else if (!strncmp(p, "seek", 4)) {
      // seek <sprite id hex> [y]: fly right through the level at height y (default 160) until that sprite is loaded
      // ahead, then drop Mario 40 px to its left (for `attack`). Prints "seek: ..." either way.
      g_seek_y = 160;
      sscanf(p + 4, "%x %d", &g_seek_id, &g_seek_y);
      g_wait = 100000;
    } else if (!strncmp(p, "waitnear", 8)) {
      // waitnear <dx> [sprite id hex]: run until a live sprite is within dx px horizontally of Mario (and roughly
      // at his height)
      g_near_id = -1;
      sscanf(p + 8, "%d %x", &g_near_dx, &g_near_id);
      g_wait = 600;
    } else if (!strncmp(p, "warp", 4)) {
      // warp <level lo hex> [secondary]: take the current screen's exit like a pipe/door would, to level
      // (map's high bit):lo, using the secondary entrance when asked
      // A 3-digit level (e.g. 003, 105) also picks the level bank: SMW takes the high bit from "player is not on
      // the main map".
      unsigned lo = 0, sec = 0;
      char lv[8] = "";
      sscanf(p + 4, "%7s %u", lv, &sec);
      lo = (unsigned)strtoul(lv, NULL, 16);
      if (strlen(lv) >= 3)
        ow_players_map[(uint8)player_current_characterx4 >> 2] = (lo >> 8) & 1;
      uint8 scr = (misc_level_layout_flags & 1) ? HIBYTE(player_ypos) : HIBYTE(player_xpos);
      misc_subscreen_exit_entrance_number_lo[scr] = (uint8)lo;
      flag_use_secondary_entrance = sec != 0;
      IncrementSublevelsEnteredAndPrepareToLoadSublevel();
    } else if (!strncmp(p, "map16grid", 9)) {
      McLevel_DebugGrid();
    } else if (!strncmp(p, "mctiles", 7)) {
      // mctiles <first hex> [count]: a catalogue of the level's map16 tiles on the screen (McLevel_DebugTiles)
      unsigned first = 0, count = 49;
      sscanf(p + 7, "%x %u", &first, &count);
      McLevel_DebugTiles((int)first, (int)count);
    } else if (!strncmp(p, "yoshi", 5)) {
      // put Mario on a green Yoshi, as when Yoshi is carried into a level
      if (!yoshi_current_yoshi_color)
        yoshi_current_yoshi_color = 10;
      SpawnMountedYoshiOnLevelLoad();
    } else if (!strncmp(p, "peek", 4)) {  // peek <ram addr hex> [count]: print RAM bytes
      unsigned a = 0, n = 1;
      sscanf(p + 4, "%x %u", &a, &n);
      printf("peek %05X:", a);
      for (unsigned i = 0; i < n && a + i < 0x20000; i++) printf(" %02X", g_ram[a + i]);
      printf("\n");
      fflush(stdout);
    } else if (!strncmp(p, "ppuw", 4)) {  // ppuw <register hex> <byte hex>: a PPU write (e.g. 212C, the main screen's layers)
      unsigned a = 0, v = 0;
      sscanf(p + 4, "%x %x", &a, &v);
      if (a >= 0x2100 && a < 0x2140) RtlPpuWrite((uint16)a, (uint8)v);
    } else if (!strncmp(p, "poke", 4)) {  // poke <ram addr hex> <byte hex>
      unsigned a = 0, v = 0;
      sscanf(p + 4, "%x %x", &a, &v);
      if (a < 0x20000) g_ram[a] = (uint8)v;
    } else if (!strncmp(p, "exits", 5)) {  // the current level's screen exits
      for (int s = 0; s < 32; s++)
        if (misc_subscreen_exit_entrance_number_lo[s])
          printf("screen %d exit %02X props %02X\n", s, misc_subscreen_exit_entrance_number_lo[s],
                 misc_subscreen_exit_properties[s]);
      printf("screens %d secondary %d underwater %d\n", misc_screens_in_lvl, flag_use_secondary_entrance,
             flag_underwater_level);
      fflush(stdout);
    } else if (!strncmp(p, "power", 5)) {
      player_current_power_up = (uint8)atoi(p + 5);
    } else if (!strncmp(p, "record", 6)) {
      // record <name>: from the next frame on, every frame to <out>/<name>.bgr (256 x lines, BGR); "record off" stops
      // and prints the frame count and size
      char name[128] = "";
      sscanf(p + 6, "%127s", name);
      if (g_rec) {
        fclose(g_rec), g_rec = NULL;
        printf("record: %d frames, 256x%d\n", g_rec_frames, g_rec_h);
        fflush(stdout);
      }
      if (strcmp(name, "off")) {
        char path[700];
        snprintf(path, sizeof(path), "%s/%s.bgr", g_out, name);
        g_rec = fopen(path, "wb");
        g_rec_frames = 0;
      }
    } else if (!strncmp(p, "audio", 5)) {
      // audio <name>: write <out>/<name>_smw.wav (the SNES mix) and <name>_mc.wav (Mario Combat's sounds at the
      // configured volume), 32 kHz stereo, one video frame of samples per frame; "audio off" stops
      char name[128] = "";
      sscanf(p + 5, "%127s", name);
      for (int i = 0; i < 2; i++)
        if (g_wav[i]) FinishWav(g_wav[i]), g_wav[i] = NULL;
      if (strcmp(name, "off")) {
        char path[700];
        snprintf(path, sizeof(path), "%s/%s_smw.wav", g_out, name);
        g_wav[0] = StartWav(path);
        snprintf(path, sizeof(path), "%s/%s_mc.wav", g_out, name);
        g_wav[1] = StartWav(path);
      }
    } else if (!strncmp(p, "palettes", 8)) {  // SMW's 4 player palettes (Mario, Luigi, Fire Mario, Fire Luigi)
      for (int i = 0; i < 40; i++) {
        uint16 c = kPlayerPalettes[i];
        printf("pal %d slot %d = %02x%02x%02x\n", i / 10, i % 10, (c & 31) << 3, (c >> 5 & 31) << 3, (c >> 10 & 31) << 3);
      }
      fflush(stdout);
    } else if (!strncmp(p, "spawn", 5)) {
      // spawn <sprite id hex> <dx> <dy>: a normal sprite at Mario's position + (dx, dy), run through its init
      unsigned id = 0;
      int dx = 0, dy = 0;
      sscanf(p + 5, "%x %d %d", &id, &dx, &dy);
      for (int k = 0; k < 12; k++) {
        if (spr_current_status[k])
          continue;
        spr_spriteid[k] = (uint8)id;
        spr_current_status[k] = 1;
        SetSprXPos(k, (uint16)((int16)player_xpos + dx));
        SetSprYPos(k, (uint16)((int16)player_ypos + 16 + dy));
        InitializeNormalSpriteRAMTables(k);
        spr_load_status_table_index[k] = 0xFF;  // not from the level's sprite list
        g_watch_slot = k;
        printf("spawn: %02X in slot %d\n", id, k);
        fflush(stdout);
        break;
      }
    } else if (!strncmp(p, "clearsprites", 12)) {
      for (int k = 0; k < 12; k++)
        spr_current_status[k] = 0;
    } else if (!strncmp(p, "owgoto", 6)) {
      // owgoto <tile hex>: on the overworld, put the current player on the first tile of that kind that has a level
      unsigned tile = 0;
      sscanf(p + 6, "%x", &tile);
      int ch = (uint8)player_current_characterx4 >> 2;
      for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
          uint16 idx = CalculateOverworldPlayerPosition(ch, x, y);
          if (idx < 0x800 && blocks_map16_table_lo[idx] == tile && ow_level_number_of_each_tiletbl[idx]) {
            ow_players_pos[ch].x = x * 16; ow_players_pos[ch].y = y * 16;
            ow_players_grid_aligned_pos[ch].x = x; ow_players_grid_aligned_pos[ch].y = y;
            ow_tile_player_is_standing_on = tile;
            printf("owgoto: tile %02X at %d,%d level %02X\n", tile, x, y, ow_level_number_of_each_tiletbl[idx]);
            fflush(stdout);
            y = 32;
            break;
          }
        }
    } else if (!strncmp(p, "kill", 4)) {
      DamagePlayer_Kill();
    } else if (!strncmp(p, "hurt", 4)) {
      // hurt [heavy 0|1] [dir -1|1]: an enemy's hit on Mario, as an MC enemy's (light or heavy, knocked toward dir)
      int heavy = 0, dir = 0;
      sscanf(p + 4, "%d %d", &heavy, &dir);
      McSmw_SetNextHitHeavy(heavy != 0);
      McSmw_SetNextHitDir(dir);
      spr_current_slotid = 0xFF;
      DamagePlayer_Hurt();
      McSmw_SetNextHitDir(0);
      McSmw_SetNextHitHeavy(false);
    } else if (!strncmp(p, "cheat", 5)) {  // cheat <kCheat_ index> <0|1>, cheat powerup <0-3>, cheat knockback <1-20>
      int a = 0, b = 0;
      if (sscanf(p + 5, " powerup %d", &b) == 1)
        g_config.cheat_powerup = (uint8)b;
      else if (sscanf(p + 5, " knockback %d", &b) == 1)
        g_config.cheat_knockback = (uint8)b;
      else if (sscanf(p + 5, "%d %d", &a, &b) == 2 && a >= 0 && a < kCheat_Count)
        g_config.cheat[a] = b != 0;
    } else if (!strncmp(p, "hitboxes", 8)) {
      g_config.mc_debug_hitboxes = atoi(p + 8) != 0;
    } else if (!strncmp(p, "sprites", 7)) {
      for (int i = 0; i < 12; i++)
        if (spr_current_status[i]) {
          char extra[240] = "";
          if (!McBowser_DebugInfo((uint8)i, extra, sizeof(extra)) && !McKoopa_DebugInfo((uint8)i, extra, sizeof(extra)))
            McHits_DebugInfo((uint8)i, extra, sizeof(extra));
          printf("sprite slot %d id %02X status %d x=%d y=%d c2=%d hits=%d%s (frame %u)\n", i, spr_spriteid[i],
                 spr_current_status[i], (int16)GetSprXPos(i), (int16)GetSprYPos(i), spr_table00c2[i], spr_table1528[i],
                 extra, g_frame);
        }
      fflush(stdout);
    } else if (!strncmp(p, "tileat", 6)) {
      int x = 0, y = 0;
      sscanf(p + 6, "%d %d", &x, &y);
      McSmw_DebugTileAt(x, y);
    } else if (!strncmp(p, "bosspunches", 11)) {  // punches that have hurt a boss so far
      printf("bosspunches %d (frame %u)\n", McSmw_DebugBossPunches(), g_frame);
      fflush(stdout);
    } else if (!strncmp(p, "ontopid", 7)) {
      // ontopid <id hex> [dy]: as ontop, on the nearest live sprite of that kind (it becomes the log's watched sprite)
      unsigned id = 0;
      int dy = 0, best = -1, bd = 1 << 30, x, sy;
      sscanf(p + 7, "%x %d", &id, &dy);
      for (int k = 0; k < 12; k++) {
        if (spr_current_status[k] < 8 || spr_spriteid[k] != id)
          continue;
        int d = abs((int16)GetSprXPos(k) - (int16)player_xpos) + abs((int16)GetSprYPos(k) - (int16)player_ypos);
        if (d < bd) bd = d, best = k;
      }
      if (best >= 0) {
        if (!McSmw_DebugPlatformTop((uint8)best, &x, &sy))
          x = (int16)GetSprXPos(best) + 8, sy = (int16)GetSprYPos(best);
        McSmw_DebugSetPos(x - 8, sy - 39 - dy);
        g_watch_slot = best;
      }
      printf("ontopid: %02X slot %d\n", id, best);
      fflush(stdout);
    } else if (!strncmp(p, "ontop", 5)) {
      // ontop <slot> [dx] [dy]: Mario 8 (+dy) px above where he stands on a sprite: on the platform its solid-sprite
      // code last stood the player on (chained platforms swing far from the sprite's own x, y), else on the sprite
      int k = 0, dx = 0, dy = 0, x, sy;
      sscanf(p + 5, "%d %d %d", &k, &dx, &dy);
      if (k >= 0 && k < 12) {
        if (!McSmw_DebugPlatformTop((uint8)k, &x, &sy))
          x = (int16)GetSprXPos(k) + 8, sy = (int16)GetSprYPos(k);
        McSmw_DebugSetPos(x - 8 + dx, sy - 39 - dy);
      }
    } else if (!strncmp(p, "setpos", 6)) {
      int x = 0, y = 0;
      sscanf(p + 6, "%d %d", &x, &y);
      McSmw_DebugSetPos(x, y);
    } else if (!strncmp(p, "glideby", 7)) {  // glideby <dx> <dy>: glide, relative to where Mario is now
      int dx = 0, dy = 0;
      sscanf(p + 7, "%d %d", &dx, &dy);
      g_glide_x = (int16)player_xpos + dx;
      g_glide_y = (int16)player_ypos + dy;
      g_glide = true;
      g_wait = 100000;
    } else if (!strncmp(p, "glide", 5)) {
      // fly Mario to level x (at height y) so the camera and level streaming follow, then drop him
      g_glide_y = 240;
      sscanf(p + 5, "%d %d", &g_glide_x, &g_glide_y);
      g_glide = true;
      g_wait = 100000;  // until arrived
    } else if (!strncmp(p, "scroll", 6)) {
      // fly Mario through the whole level at walking pace (3 px per frame; turn on No damage) at a fixed height / column so every sprite in its list is
      // loaded: a horizontal level left to right, a vertical one to the top and back down; waits until done
      g_scroll = (misc_level_layout_flags & 1) ? 2 : 1;
      g_wait = 100000;
    } else if (!strncmp(p, "statusbar", 9)) {  // the status bar's height in output lines (last drawn frame)
      printf("status bar lines %d\n", Smw_StatusBarLines());
      fflush(stdout);
    } else if (!strncmp(p, "triplelog", 9)) {
      Cheats_SetTripleLog(atoi(p + 9) != 0);
    } else if (!strncmp(p, "toggle", 6)) {
      McSmw_ToggleCharacter();
    } else if (!strncmp(p, "menukey", 7)) {
      char name[64] = "";
      sscanf(p + 7, " %63[^\r\n]", name);
      Menu_HandleKey(SDL_GetKeyFromName(name), KMOD_NONE, true, false);
    } else if (!strncmp(p, "menupad", 7)) {
      Menu_HandlePadButton(atoi(p + 7), true);
      Menu_HandlePadButton(atoi(p + 7), false);
    } else if (!strncmp(p, "menu", 4)) {
      Menu_Open();
    } else if (!strncmp(p, "savestate", 9)) {
      RtlSaveLoad(kSaveLoad_Save, atoi(p + 9));
    } else if (!strncmp(p, "loadstate", 9)) {
      RtlSaveLoad(kSaveLoad_Load, atoi(p + 9));
    } else if (!strncmp(p, "focus", 5)) {  // focus 0/1: as if the game window lost / got the focus
      Main_SetFocusForTest(atoi(p + 5) != 0);
    } else if (!strncmp(p, "setting", 7)) {
      // setting mono|background|rumble|deadzone <value>: the settings menu's MONO, IN BACKGROUND, RUMBLE, STICK DEADZONE;
      // setting mc 0/1: Mario Combat Mario off / on altogether (smw.ini [MarioCombat] Enable), for SMW-Mario references
      char name[32] = "";
      int v = 0;
      sscanf(p + 7, "%31s %d", name, &v);
      if (!strcmp(name, "mono")) g_config.audio_mono = v != 0;
      else if (!strcmp(name, "background")) g_config.in_background = (uint8)v;
      else if (!strcmp(name, "rumble")) g_config.rumble = (uint8)v;
      else if (!strcmp(name, "deadzone")) g_config.stick_deadzone = (uint8)v;
      else if (!strcmp(name, "mc")) g_config.mc_enable = v != 0;
    } else if (!strncmp(p, "follow", 6)) {  // follow <minutes>: wait for more lines at the end of the script
      int minutes = atoi(p + 6);
      g_follow = true;
      g_follow_ms = (uint32)(minutes > 0 ? minutes : 30) * 60000u;
      g_follow_deadline = SDL_GetTicks() + g_follow_ms;
    } else if (!strncmp(p, "status", 6)) {
      WriteStatus();
    } else if (!strncmp(p, "ack", 3)) {  // ack <id>: everything before this line has run
      char t[64];
      snprintf(t, sizeof(t), "%d %u\n", atoi(p + 3), g_frame);
      WriteFileAtomic("ack.txt", t);
    } else if (!strncmp(p, "levelnames", 10)) {
      PrintLevelNames();
    } else if (!strncmp(p, "levelscan", 9)) {
      PrintLevelScan();
    } else if (!strncmp(p, "spritefind", 10)) {  // spritefind <id hex>: the rooms holding that sprite, with positions
      unsigned id = 0;
      sscanf(p + 10, "%x", &id);
      PrintSpriteFind((uint8)id);
    } else if (!strncmp(p, "bossrooms", 9)) {
      PrintBossRooms();
    } else if (!strncmp(p, "bossrush", 8)) {
      // bossrush [all] [end]: LEVEL SELECT > BOSS RUSH > START with those settings (every boss; a death ends the run)
      g_config.boss_rush_all = strstr(p + 8, "all") != NULL;
      g_config.boss_rush_end_on_death = strstr(p + 8, "end") != NULL;
      const char *why = LevelSelect_StartBossRush();
      printf("bossrush: %s, %d fights (frame %u)\n", why ? why : "starts",
             LevelSelect_BossRushCount(g_config.boss_rush_all), g_frame);
      fflush(stdout);
    } else if (!strncmp(p, "mclevelinfo", 11)) {
      McLevel_DebugInfo();
    } else if (!strncmp(p, "mclevel", 7)) {
      // mclevel [area]: LEVEL SELECT > MARIO COMBAT (Mario Combat's own level); with an area (1-5), started there
      int area = 0;
      sscanf(p + 7, "%d", &area);
      const char *why = McLevel_StartAt(area > 0 ? area - 1 : 0);
      printf("mclevel: %s (frame %u)\n", why ? why : "starts", g_frame);
      fflush(stdout);
    } else if (!strncmp(p, "waitmode", 8)) {
      // waitmode <game mode hex> [max frames]: run until the game mode is that one (at most 900 frames by default)
      unsigned m = 0x14, mx = 900;
      sscanf(p + 8, "%x %u", &m, &mx);
      g_wait_mode = (int)m;
      g_wait = (int)mx;
    } else if (!strncmp(p, "ramdump", 7)) {
      // ramdump <name>: the work RAM ($0000-$1FFFF) to <out>/<name>.ram
      char name[64] = "ram", path[640];
      sscanf(p + 7, "%63s", name);
      snprintf(path, sizeof(path), "%s/%s.ram", g_out, name);
      FILE *f = fopen(path, "wb");
      if (f) {
        fwrite(g_ram, 1, 0x20000, f);
        fclose(f);
      }
      printf("ramdump: %s mode %02X (frame %u)\n", name, misc_game_mode, g_frame);
      fflush(stdout);
    } else if (!strncmp(p, "lsclear", 7)) {  // lsclear 0/1: the level select's clean-up of the level RAM off / on
      LevelSelect_DebugClear(atoi(p + 7) != 0);
    } else if (!strncmp(p, "rushinfo", 8)) {  // the boss rush line shown on screen, or "none"
      char b[48];
      printf("rushinfo: %s (frame %u)\n", LevelSelect_BossRushHud(b, sizeof(b)) ? b : "none", g_frame);
      fflush(stdout);
    } else if (!strncmp(p, "levelselect", 11)) {
      // levelselect <level hex>: the settings menu's LEVEL SELECT choosing that level (prints whether it starts)
      unsigned lv = 0;
      sscanf(p + 11, "%x", &lv);
      int i = LevelSelect_FindLevel((uint16)lv);
      const char *why = i < 0 ? "not in the list" : LevelSelect_Request(i);
      printf("levelselect: %03X %s (frame %u)\n", lv, why ? why : "starts", g_frame);
      fflush(stdout);
    } else if (!strncmp(p, "levellist", 9)) {  // the level select's list, in menu order
      for (int i = 0; i < LevelSelect_Count(); i++) {
        const LevelSelectEntry *e = LevelSelect_Entry(i);
        printf("levellist %d group %d %s level %03X kind %d translevel %02X %s\n", i, e->group,
               LevelSelect_GroupName(e->group), e->level, e->kind, e->translevel, e->name);
      }
      fflush(stdout);
    } else if (!strncmp(p, "whereami", 8)) {  // game mode, the level number last loaded, map submap and position
      printf("whereami: mode %02X level %03X sublevels %d session %d map %d pos %d,%d ow_level %02X state %d "
             "lives %d power %d yoshi %d time %d%d%d x %d (frame %u)\n", misc_game_mode, LevelSelect_LastLoaded() & 0xFFF,
             counter_sublevels_entered, LevelSelect_SessionActive(), ow_players_map[player_current_character],
             ow_players_pos[player_current_character].x, ow_players_pos[player_current_character].y,
             ow_level_number_lo, player_current_state, player_current_life_count, player_current_power_up,
             player_riding_yoshi_flag, counter_timer_hundreds, counter_timer_tens, counter_timer_ones, (int16)player_xpos, g_frame);
      fflush(stdout);
    } else if (!strncmp(p, "progress", 8)) {  // a hash of SMW's save buffer ($1EA2-$1F2E: levels, events, map)
      printf("progress: %08X ow_level %02X (frame %u)\n", Fnv(g_ram + 0x1EA2, 0x8D), ow_level_number_lo, g_frame);
      fflush(stdout);
    } else if (!strncmp(p, "takeexit", 8)) {
      // takeexit: take the current screen's own exit (as a door or pipe of the level would)
      IncrementSublevelsEnteredAndPrepareToLoadSublevel();
    } else if (!strncmp(p, "quit", 4)) {
      g_quit = true;
    }
  }
  if (g_attack_id >= 0) {
    // phase 0: approach; 1..: punch sequence
    int best = -1, bd = 1 << 30;
    for (int i = 0; i < 12; i++) {
      uint8 id = spr_spriteid[i];
      bool boss = id == 0x29 || id == 0xA0 || id == 0xA9 || id == 0xC5;
      if (spr_current_status[i] < 8 || (g_attack_id == kAttackBoss ? !boss : id != g_attack_id)) continue;
      int d = abs((int16)GetSprXPos(i) - (int16)player_xpos);
      if (d < bd) bd = d, best = i;
    }
    int dir = best >= 0 && (int16)GetSprXPos(best) < (int16)player_xpos ? 0x40 : 0x80;  // LEFT / RIGHT bits
    if (g_attack_phase == 0) {
      bool facing_ok = (dir == 0x80) == (player_facing_direction != 0);
      if (best < 0 || bd > g_attack_dx || !facing_ok) {
        g_buttons = best >= 0 ? dir : 0;  // walk toward it (or turn round)
      } else {
        g_attack_phase = 1;
        printf("attack: sprite %02X at dx %d (frame %u)\n", spr_spriteid[best], (int16)GetSprXPos(best) - (int16)player_xpos,
               g_frame);
        fflush(stdout);
      }
    }
    if (g_attack_phase == 1 && !g_attack_air && McSmw_Enabled() && McSmw_DebugHeroFrame() != 1) {
      g_buttons = 0;  // MC starts a standing punch only from idle: let Mario come to a stop first
    } else if (g_attack_phase >= 1) {
      int ph = g_attack_phase++;
      if (g_attack_air)  // jump (B) g_attack_delay frames, then B+Y for 2
        g_buttons = ph <= g_attack_delay ? 0x01 : ph <= g_attack_delay + 2 ? 0x03 : ph <= 10 ? 0x01 : 0;
      else
        g_buttons = ph <= 2 ? 0x02 : 0;  // Y
      if ((g_attack_air && ph > 10) || (!g_attack_air && ph > 2)) {
        g_attack_id = -1;
        g_buttons = 0;
        g_wait = 1;
      }
    }
    if (g_wait <= 1 && g_attack_id >= 0) {  // gave up
      g_attack_id = -1;
      g_buttons = 0;
    }
  }
  if (g_near_dx > 0) {
    for (int i = 0; i < 12; i++) {
      if (spr_current_status[i] < 8 || (g_near_id >= 0 && spr_spriteid[i] != g_near_id))
        continue;
      int dx = abs((int16)GetSprXPos(i) - (int16)player_xpos), dy = abs((int16)GetSprYPos(i) - (int16)player_ypos - 16);
      if (dx <= g_near_dx && dy < 80) {
        printf("near: sprite %02X at dx %d (frame %u)\n", spr_spriteid[i], (int16)GetSprXPos(i) - (int16)player_xpos, g_frame);
        fflush(stdout);
        g_near_dx = 0;
        g_wait = 1;
        break;
      }
    }
    if (g_wait <= 1)
      g_near_dx = 0;
  }
  if (g_wait_mode >= 0 && misc_game_mode == g_wait_mode) {
    printf("waitmode: %02X (frame %u)\n", g_wait_mode, g_frame);
    fflush(stdout);
    g_wait_mode = -1;
    g_wait = 1;
  }
  if (g_wait > 0)
    g_wait--;
  if (g_glide) {
    int16 x = (int16)player_xpos, dx = g_glide_x - x;
    if (dx > 6) dx = 6;
    if (dx < -6) dx = -6;
    player_xpos = (uint16)(x + dx);
    player_ypos = (uint16)g_glide_y;
    player_yspeed = 0;
    player_xspeed = 0;
    timer_star_power = 2;  // invulnerable on the way (no star music: that starts with the item)
    if (dx == 0) {
      g_glide = false;
      g_wait = 0;
    }
  }
  if (g_seek_id >= 0) {
    // seek: fly right at height g_seek_y (3 px per frame, invulnerable) until a live sprite of that id is loaded
    // within 160 px ahead; then put Mario 40 px to its left and 40 px above it and let him drop there.
    timer_star_power = 2;
    player_yspeed = player_xspeed = 0;
    int found = -1;
    for (int i = 0; i < 12; i++)
      if (spr_current_status[i] >= 8 && spr_spriteid[i] == g_seek_id &&
          (int16)GetSprXPos(i) - (int16)player_xpos > -40 && (int16)GetSprXPos(i) - (int16)player_xpos < 160)
        found = i;
    int16 x = (int16)player_xpos;
    if (found >= 0 || misc_game_mode != 0x14 || x + 3 >= misc_screens_in_lvl * 256 - 24) {
      if (found >= 0) {
        player_xpos = (uint16)((int16)GetSprXPos(found) - 40);
        player_ypos = (uint16)((int16)GetSprYPos(found) - 40);
        printf("seek: sprite %02X found at %d,%d (frame %u)\n", g_seek_id, (int16)GetSprXPos(found),
               (int16)GetSprYPos(found), g_frame);
      } else {
        printf("seek: sprite %02X not found (frame %u)\n", g_seek_id, g_frame);
      }
      fflush(stdout);
      g_seek_id = -1, g_wait = 0;
    } else {
      player_xpos = (uint16)(x + 3);
      player_ypos = (uint16)g_seek_y;
    }
  }
  if (g_scroll) {
    int end_x = misc_screens_in_lvl * 256 - 24, end_y = misc_screens_in_lvl * 256 - 48;
    player_yspeed = player_xspeed = 0;  // (no star power here: it would knock out the enemies being counted)
    if (misc_game_mode != 0x14) {
      g_scroll = 0, g_wait = 0;  // left the level (an exit, death...)
    } else if (g_scroll == 1) {
      player_ypos = 160;
      int16 x = (int16)player_xpos;
      player_xpos = (uint16)IntMin(end_x, x + 3);
      if (x + 3 >= end_x) g_scroll = 0, g_wait = 0;
    } else {
      player_xpos = 128;
      int16 y = (int16)player_ypos;
      if (g_scroll == 2) {
        player_ypos = (uint16)IntMax(16, y - 3);
        if (y - 3 <= 16) g_scroll = 3;
      } else {
        player_ypos = (uint16)IntMin(end_y, y + 3);
        if (y + 3 >= end_y) g_scroll = 0, g_wait = 0;
      }
    }
  }
  return g_buttons;
}

void Pilot_EndFrame(void) {
  g_frame++;
  TrackLevelResult();
  if (g_wav[0] && g_wav[1])
    WriteAudioFrame();
  if (g_log) {
    char mc[256];
    McSmw_DebugState(mc, sizeof(mc));
    int w = g_watch_slot;
    fprintf(g_log, "%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", g_frame, misc_game_mode, (int16)player_xpos,
            (int16)player_ypos, player_sub_xpos, player_sub_ypos, (int8)player_xspeed, (int8)player_yspeed,
            player_in_air_flag, player_blocked_flags, player_current_state, player_current_power_up,
            (int16)mirror_current_layer1_xpos, (int16)mirror_current_layer1_ypos, mc, misc_display_message,
            flag_message_window_size_change_direction, timer_wait_before_message_window_size_change, flag_sprites_locked,
            player_pmeter, player_carrying_something_flag2, player_cape_flying_phase,
            timer_wait_before_cape_flight_begins, player_spin_jump_flag, w >= 0 ? spr_spriteid[w] : -1,
            w >= 0 ? spr_current_status[w] : -1, w >= 0 ? (int16)GetSprXPos(w) : 0, w >= 0 ? (int16)GetSprYPos(w) : 0,
            w >= 0 ? spr_table00c2[w] : 0, misc_player_on_solid_sprite);
  }
}

bool Pilot_WantRerender(void) {
  bool r = g_rerender;
  g_rerender = false;
  return r;
}

void Pilot_SaveRerenderShot(const uint8 *argb, int pitch, int w, int h) {
  char keep[128];
  memcpy(keep, g_shot_name, sizeof(keep));
  snprintf(g_shot_name, sizeof(g_shot_name), "%s_re", keep);
  g_shot_pending = true;  // (the shot of the frame itself has just cleared it)
  g_rerender_saving = true;  // not a frame of a recording
  Pilot_SaveShot(argb, pitch, w, h);
  g_rerender_saving = false;
  memcpy(g_shot_name, keep, sizeof(keep));
}

// `record <name>`: every frame the game shows, appended to <out>/<name>.bgr as raw 24-bit pixels (blue, green, red;
// top row first) for a video (tools/scripts/make_video.py). Rendering every frame is what the game does outside the
// pilot anyway; nothing else changes.
static void RecordFrame(const uint8 *argb, int pitch, int w, int h) {
  static uint8 row[256 * 3];
  if (w > 256)
    return;
  for (int y = 0; y < h; y++) {
    const uint8 *s = argb + y * pitch;
    for (int x = 0; x < w; x++)
      row[x * 3] = s[x * 4], row[x * 3 + 1] = s[x * 4 + 1], row[x * 3 + 2] = s[x * 4 + 2];
    fwrite(row, 1, w * 3, g_rec);
  }
  g_rec_frames++;
  g_rec_h = h;
}

void Pilot_SaveShot(const uint8 *argb, int pitch, int w, int h) {
  if (g_rec && !g_rerender_saving)
    RecordFrame(argb, pitch, w, h);
  if (!g_shot_pending)
    return;  // (recording only)
  char path[700];
  snprintf(path, sizeof(path), "%s/%s.bmp", g_out, g_shot_name);
  g_shot_pending = false;
  FILE *f = fopen(path, "wb");
  if (!f) return;
  int row = w * 3, pad = (4 - row % 4) % 4, size = 54 + (row + pad) * h;
  uint8 hdr[54] = { 'B', 'M' };
  *(uint32 *)(hdr + 2) = size; *(uint32 *)(hdr + 10) = 54; *(uint32 *)(hdr + 14) = 40;
  *(int32 *)(hdr + 18) = w; *(int32 *)(hdr + 22) = h; *(uint16 *)(hdr + 26) = 1; *(uint16 *)(hdr + 28) = 24;
  fwrite(hdr, 1, 54, f);
  for (int y = h - 1; y >= 0; y--) {
    const uint8 *s = argb + y * pitch;
    for (int x = 0; x < w; x++)
      fputc(s[x * 4 + 0], f), fputc(s[x * 4 + 1], f), fputc(s[x * 4 + 2], f);
    for (int i = 0; i < pad; i++) fputc(0, f);
  }
  fclose(f);
}

void Pilot_Shutdown(void) {
  if (g_rec) {
    fclose(g_rec), g_rec = NULL;
    printf("record: %d frames, 256x%d\n", g_rec_frames, g_rec_h);
    fflush(stdout);
  }
  if (g_log) fclose(g_log);
  if (g_script) fclose(g_script);
  for (int i = 0; i < 2; i++)
    if (g_wav[i]) FinishWav(g_wav[i]), g_wav[i] = NULL;
}
