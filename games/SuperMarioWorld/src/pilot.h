#pragma once
#include <stdbool.h>
#include "types.h"

bool Pilot_Init(int *argc, char ***argv);
bool Pilot_Active(void);
uint32 Pilot_Frame(void);  // frames run so far (for log lines)
bool Pilot_WantQuit(void);
bool Pilot_WantShot(void);
uint32 Pilot_BeginFrame(void);
void Pilot_EndFrame(void);
void Pilot_SaveShot(const uint8 *argb, int pitch, int w, int h);
bool Pilot_WantRerender(void);  // `shotre`: draw the frame once more and save it as <name>_re
void Pilot_SaveRerenderShot(const uint8 *argb, int pitch, int w, int h);
void Pilot_Shutdown(void);
