// Gamepad rumble (see feedback.h): watches SMW's own state once per game frame.
#include "feedback.h"
#include "config.h"
#include "smw_rtl.h"
#include "variables.h"

void Feedback_Frame(void) {
  static uint8 prev_state, prev_hurt;
  static int quiet;  // frames before a hurt can rumble again (an SMW power-down sets the state, then the hurt timer)
  if (!g_config.rumble || misc_game_mode != 0x14) {
    prev_state = player_current_state, prev_hurt = timer_player_hurt;
    return;
  }
  if (quiet > 0)
    quiet--;
  if (player_current_state == 9 && prev_state != 9) {  // a life lost (enemy, pit, lava, time)
    Main_Rumble(1.0f, 0.6f, 500);
    quiet = 60;
  } else if (!quiet && ((player_current_state == 1 && prev_state != 1) || (timer_player_hurt && !prev_hurt))) {
    Main_Rumble(0.6f, 0.8f, 250);  // hurt: an SMW power-down, Mario Combat life lost, Yoshi lost
    quiet = 60;
  }
  if (timer_shake_layer1 && (timer_shake_layer1 & 3) == 0)  // the screen shakes: a long low rumble while it lasts
    Main_Rumble(0.7f, 0.15f, 90);
  prev_state = player_current_state, prev_hurt = timer_player_hurt;
}
