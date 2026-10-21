// Gamepad rumble (the RUMBLE setting): what in the game makes the pad shake.
#pragma once
#include "types.h"

// Provided by main.c: rumble every open gamepad (RUMBLE off / light = 40% / full). low = the heavy motor, high = the
// light one, 0-1, for ms. In pilot runs it prints a "rumble" line instead.
void Main_Rumble(float low, float high, uint32 ms);
// Once per game frame that ran: Mario hurt, Mario losing a life, the screen shaking (Thwomps, ground pounds, Bowser).
// (Mario Combat Mario's hits that land rumble from mc_smw.c.)
void Feedback_Frame(void);
