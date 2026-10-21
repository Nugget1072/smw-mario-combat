// "MC hit reactions" cheat: SMW's ordinary enemies take Mario Combat Mario's punches and kicks the way the Mario
// Combat Koopas do (mc_koopa.c): 10 life, Mario Combat's damage per hit level, and Mario Combat's knockback. Their own
// behaviour is untouched; only the response to a hit changes. Bosses are not ordinary enemies.
#pragma once
#include <stdbool.h>
#include "../types.h"

enum {
  kMcHitsNo,        // not handled (not an ordinary enemy, or one a punch can't defeat): the normal punch
  kMcHitsAbsorbed,  // the hit took life and knocked the enemy about; it is still alive
  kMcHitsFinal,     // the hit emptied its life: knock it out the normal way, as a strong hit
};

void McHits_FrameBegin(void);            // 50 Hz tick clock (Mario Combat's frame rate)
void McHits_OnSpriteInit(uint8 k);       // a new sprite in slot k
// A punch / kick of hit level `level` on sprite k. kind: Cheats_EnemyKind (1 walks, 2 flies / swims, 3 in terrain).
int McHits_Punch(uint8 k, int level, int kind);
// Called by SprStatus08 before the sprite's own code: true while the enemy is knocked about. It has then been moved
// (MC knockback through SMW's sprite collision); the caller runs the sprite's code with the sprites-locked flag (so
// it only draws) and then McHits_AfterDraw.
bool McHits_Knocked(uint8 k);
void McHits_AfterDraw(uint8 k);
bool McHits_IsKnocked(uint8 k);
int McHits_DebugInfo(uint8 k, char *buf, int n);
