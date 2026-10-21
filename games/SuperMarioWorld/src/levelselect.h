// Level select (settings menu LEVEL SELECT): every level of the game, by world in the order they come up in normal
// play, then the bonus rooms the game loads by itself, then the unused levels. Choosing one starts it from its
// beginning, the way SMW starts a level from the map. A level played from here is practice: when it ends (goal, death,
// exit) the map comes back exactly as it was before the first level-select level (no level counts as beaten, no path
// opens, no midway point is kept, the save is not touched); coins, lives, score and power-ups carry on as normal.
#pragma once
#include "types.h"

enum { kLevelSelectGroups = 11 };

typedef struct LevelSelectEntry {
  uint16 level;      // SMW level number (000-1FF)
  uint8 group;       // 0 .. kLevelSelectGroups - 1
  uint8 kind;        // kLsKind_*
  uint8 translevel;  // the map's level number (for the map levels), else 0
  const char *name;  // menu row
  const char *note;  // menu description (with the level number)
} LevelSelectEntry;

enum {
  kLsKind_Map,        // a level on the overworld map
  kLsKind_Plain,      // a level started by number (the intro, the title screen's level, the unused levels)
  kLsKind_BonusGame,  // the 1-UP bonus game (SMW's flag_active_bonus_game)
  kLsKind_Wings,      // Yoshi's wings bonus area (SMW's in_yoshi_wings_bonus_area)
};

const char *LevelSelect_GroupName(int group);
const char *LevelSelect_GroupNote(int group);
int LevelSelect_Count(void);
const LevelSelectEntry *LevelSelect_Entry(int i);
// From the menu: start entry i. Returns NULL when it will start (on the next game frame), else a short notice why not.
const char *LevelSelect_Request(int i);
// Once per game frame, before the game frame runs: starts a requested level, puts the map back after a practice run.
void LevelSelect_Frame(void);
// From LoadLevel (smw_05.c) when a level starts from the map: true and the level number when the level select starts
// one (consumed).
bool LevelSelect_TakeLevel(uint16 *level);
bool LevelSelect_SessionActive(void);  // a practice run is going on (the map will be put back)
int LevelSelect_FindLevel(uint16 level);  // entry index of a level number, or -1
// From LoadLevel: the level (or room) number just loaded; LevelSelect_LastLoaded returns it (-1 before any).
void LevelSelect_NoteLoaded(uint16 level);
int LevelSelect_LastLoaded(void);
// From LoadLevel and InitializeLevelRAM, where SMW would show a castle / ghost house door scene first (the entrance
// room, the walk through the door): true while a level select start that is not a map level (a boss room, an unused
// room, the intro...) loads, since there is no door before such a room.
bool LevelSelect_SkipEntranceScene(void);

// Boss rush (LEVEL SELECT > BOSS RUSH): the bosses back to back in the game's order, each in its own room, timed, as
// one practice run (the map and save come back as they were; so do lives, coins and the item box). g_config chooses
// the bosses (boss_rush_all: every boss, else the Koopa Kids and Bowser) and what a death does (boss_rush_end_on_death:
// the run ends, else the boss is fought again).
void LevelSelect_DebugClear(bool on);  // pilot `lsclear 0/1`: the starts' level-RAM clean-up off / on (tests only)
const char *LevelSelect_StartBossRush(void);  // from the menu: NULL when it starts (next game frame), else why not
bool LevelSelect_BossRushActive(void);
int LevelSelect_BossRushCount(bool all);      // fights in a run
bool LevelSelect_BossRushHud(char *buf, int n);
// For the Mario Combat level (mc/mc_level.c, a run of its own): where a start is possible now (NULL) or why not; a room
// started by number as a practice run (the boss rush's way); the boss rush ended.
const char *LevelSelect_WhyNotStart(void);
void LevelSelect_LaunchRoom(uint16 room, uint8 translevel, const char *name);
void LevelSelect_AbandonRush(void);  // the line shown during a run ("BOSS 2/8  1:23.45"); false if none
