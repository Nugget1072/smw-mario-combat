// Mario Combat's own level (its story mode: five areas, Bowser at the end) played inside SMW (round 20). The areas are
// built from MC's own ground shapes (tools/scripts/mc_level_export.py -> mc_level_data.c) as SMW castle tiles over a
// vanilla castle room, which gives the music; the tiles are SMW's collision, but the tile layers are hidden and MC's own
// pictures of the areas are drawn instead (tools/scripts/mc_level_art.py -> mc_level_art.c). The level select starts it
// (a practice run, as the boss rush): one area after another through SMW's own room changes, a death starts the area
// again, and Bowser's fall ends it.
#ifndef SMW_MC_MC_LEVEL_H_
#define SMW_MC_MC_LEVEL_H_
#include "../types.h"

const char *McLevel_Start(void);    // the menu's MARIO COMBAT: NULL, or why it can't start now
const char *McLevel_StartAt(int area);  // (pilot mclevel <area>: from that area, 0-4)
void McLevel_Abandon(void);         // another level or the boss rush was chosen
bool McLevel_Active(void);
int McLevel_AreaNumber(void);       // 1-5 while active, else 0
bool McLevel_McKoopas(void);        // the areas' Koopas are MC's Koopas (whatever the cheats)
bool McLevel_Bowser(void);          // ... or MC's Bowser (the last area)

// Hooks (SMW's level load and frame)
void McLevel_Frame(void);                         // every game frame (LevelSelect_Frame)
void McLevel_AfterLoadLevel(void);                // GameMode11, after LoadLevel: the area to build, Mario's start
void McLevel_BuildArea(void);                     // LoadSublevel, after the base room's objects: the area's tiles
const uint8 *McLevel_SpriteList(const uint8 *base);  // GetSpriteListPtr: the area's sprites while active
void McLevel_ClampCamera(void);                   // SMW's camera scroll: within MC's scroll limits
void McLevel_SpritePhase(void);                   // ProcessNormalSprites' start: the lift carries Mario
void McLevel_Draw(void);                          // overlay: the "press" arrow
// The PPU, a composed line (main screen): MC's pictures where it shows the backdrop (SMW's layers 1 and 2 are off in the
// level); hscroll / vscroll: BG1's scroll registers (where layer 1 would be).
void McLevel_ArtLine(int line, uint32 *dst, const uint16 *zbuf, uint16 hscroll, uint16 vscroll, int brightness,
                     bool mirror);

// Map16 tile at tile (tx, ty) of the loaded horizontal level (no checks against the level's size beyond 32 screens).
void McLevel_SetMap16(int tx, int ty, uint16 tile);
int McLevel_GetMap16(int tx, int ty);
// Pilot `mctiles <first> [count]`: the tiles first.. laid out in a grid on the screen around the camera, every other
// row and column, 7 to a row, and the screen redrawn (a catalogue of the level's tileset).
void McLevel_DebugTiles(int first, int count);
// Pilot `map16grid`: the visible screen's map16 numbers, a row per line.
void McLevel_DebugGrid(void);
void McLevel_DebugInfo(void);  // pilot `mclevelinfo`
int McLevel_DebugStatus(char *buf, int n);  // pilot status.json: the run's area, exit, prompt, lift (a JSON object)

#endif  // SMW_MC_MC_LEVEL_H_
