#pragma once
#include "types.h"
#include <SDL_keycode.h>

enum {
  kKeys_Null,
  kKeys_Controls,
  kKeys_Controls_Last = kKeys_Controls + 11,

  kKeys_ControlsP2,
  kKeys_ControlsP2_Last = kKeys_ControlsP2 + 11,

  kKeys_Load,
  kKeys_Load_Last = kKeys_Load + 19,
  kKeys_Save,
  kKeys_Save_Last = kKeys_Save + 19,
  kKeys_Replay,
  kKeys_Replay_Last = kKeys_Replay + 19,
  kKeys_LoadRef,
  kKeys_LoadRef_Last = kKeys_LoadRef + 19,
  kKeys_ReplayRef,
  kKeys_ReplayRef_Last = kKeys_ReplayRef + 19,
  kKeys_CheatLife,
  kKeys_CheatJump,
  kKeys_ToggleWhichFrame,
  kKeys_ClearKeyLog,
  kKeys_StopReplay,
  kKeys_Fullscreen,
  kKeys_Reset,
  kKeys_Pause,
  kKeys_PauseDimmed,
  kKeys_Turbo,
  kKeys_ReplayTurbo,
  kKeys_WindowBigger,
  kKeys_WindowSmaller,
  kKeys_DisplayPerf,
  kKeys_ToggleRenderer,
  kKeys_VolumeUp,
  kKeys_VolumeDown,
  kKeys_ToggleMario,   // switch between Mario Combat Mario and SMW Mario
  kKeys_Settings,      // open the settings menu
  kKeys_ShowFps,       // toggle the FPS counter (F3)
  kKeys_FullscreenToggle,  // toggle fullscreen (F11; Alt+Enter is kKeys_Fullscreen)
  kKeys_Total,
};

// Bindings the settings menu can change (the 12 SNES buttons, then the commands above).
enum {
  kBind_Up, kBind_Down, kBind_Left, kBind_Right, kBind_Select, kBind_Start,
  kBind_A, kBind_B, kBind_X, kBind_Y, kBind_L, kBind_R,
  kBind_ToggleMario, kBind_Settings, kBind_ShowFps, kBind_Fullscreen,
  kBind_Count,
};

// Cheats (settings menu CHEATS page, saved in [MarioCombat] as Cheat... keys)
enum {
  kCheat_InfiniteLife,   // Mario Combat life never goes down
  kCheat_NoDamage,       // enemies and hazards don't hurt either Mario (pits, lava, crushing still do)
  kCheat_InfiniteTime,
  kCheat_InfiniteLives,
  kCheat_Invincible,     // star power without the star music
  kCheat_MoonJump,       // hold jump to keep rising
  kCheat_PSpeed,         // P-meter always full (cape take-off any time)
  kCheat_Switches,       // all four switch palaces pressed
  kCheat_ReplayLevels,   // destroyed castles / fortresses can be entered again (A/B, like SMW's own L+R)
  kCheat_TripleEnemies,  // every ordinary enemy a level loads comes with two more (no bosses / scripted sprites)
  kCheat_NoKnockback,    // Mario Combat Mario takes hits without being knocked back or stunned
  kCheat_SuperPunch,     // every Mario Combat attack is a strong hit that defeats any ordinary enemy at once
  kCheat_AlwaysYoshi,    // every level and room starts riding Yoshi (where SMW allows Yoshi)
  kCheat_SlowMotion,     // levels run at half speed
  kCheat_McKoopas,       // SMW's Koopa Troopas become Mario Combat's Koopas (art, AI, attacks, hit reactions)
  kCheat_Mirror,         // the whole game is mirrored left <-> right, controls included
  kCheat_OneHit,         // any hit is fatal (a challenge mode)
  kCheat_EnemyCoins,     // every defeated enemy drops a coin
  kCheat_KingKoopas,     // SMW's Koopa Troopas become Mario Combat's final boss, Bowser (never with kCheat_McKoopas)
  kCheat_DoubleJump,     // both Marios can jump once more in the air
  kCheat_GroundPound,    // Down in the air: Mario drops straight down and lands with SMW's cape-dive ground pound
  kCheat_WallJump,       // SMW Mario slides down walls and kicks off them (MC Mario already does)
  kCheat_Slippery,       // every level plays with SMW's ice physics (both Marios slide)
  kCheat_CoinMagnet,     // coins (and Yoshi coins) near Mario jump to him
  kCheat_FlyingYoshi,    // Yoshi always has wings, as in SMW's wings bonus area
  kCheat_McHits,         // SMW's ordinary enemies take MC Mario's hits as the MC Koopas do (10 life, knocked flying)
  kCheat_LifeSteal,      // MC Mario's hits that land give him life back
  kCheat_FirePunch,      // each MC punch / kick also throws SMW's fireball
  kCheat_InstantHits,    // MC attacks hit on their first frame; a new press after a hit chains the combo at once
  kCheat_BlockBuster,    // MC punches also break grey (cement) and used blocks
  kCheat_PitRescue,      // a pit puts Mario back on the last ground he stood on, as a hit instead of a lost life
  kCheat_InfiniteJumps,  // jump again in the air as often as you like (both Marios)
  kCheat_OpenMap,        // on the map Mario can walk past unbeaten levels and along paths not yet revealed
  kCheat_StompAll,       // any enemy can be stomped from above, even spiky ones (both Marios)
  kCheat_FastEnemies,    // ordinary enemies run their code twice a frame: they move and act twice as fast
  kCheat_ChaseEnemies,   // walking enemies always turn to walk toward Mario
  kCheat_JumpEnemies,    // walking enemies on the ground jump whenever Mario jumps off the ground
  kCheat_LowGravity,     // both Marios fall with half of their gravity (jumps go twice as high)
  kCheat_AirDash,        // in the air L dashes left, R dashes right (once per jump, both Marios)
  kCheat_Respawn,        // defeated ordinary enemies come back when their spot scrolls into view again
  kCheat_PunchAll,       // MC attacks hurt the enemies nothing but a star can (Boos, Thwomps, Wigglers, Urchins...)
  kCheat_Count,
};

enum {
  kBackground_Play,   // keep playing with sound (as before)
  kBackground_Mute,   // keep playing, silent
  kBackground_Pause,  // the game and its sound stop until the window has the focus again
};

enum {
  kOutputMethod_SDL,
  kOutputMethod_SDLSoftware,
  kOutputMethod_OpenGL,
};

typedef struct Config {
  int window_width;
  int window_height;
  bool enhanced_mode7;
  bool new_renderer;
  bool ignore_aspect_ratio;
  uint8 fullscreen;
  uint8 window_scale;
  bool enable_audio;
  bool linear_filtering;
  uint8 output_method;
  uint16 audio_freq;
  uint8 audio_channels;
  uint16 audio_samples;
  bool autosave;
  uint8 extended_aspect_ratio;
  bool extend_y;
  bool no_sprite_limits;
  bool display_perf_title;
  uint8 enable_msu;
  bool resume_msu;
  bool disable_frame_delay;
  bool save_playthrough;
  uint8 msuvolume;
  uint32 features0;

  const char *link_graphics;
  char *memory_buffer;
  const char *shader;
  const char *msu_path;

  bool enable_gamepad[2];

  // Which players have keyboard controls
  uint8 has_keyboard_controls;

  // [MarioCombat] Mario Combat player port
  bool mc_enable;           // play as Mario Combat's Mario (0 = original SMW Mario)
  bool mc_up_jumps;         // Up also jumps, like the original's Up-arrow jump
  bool mc_sounds;           // Mario Combat's own move sounds
  uint8 mc_sound_volume;    // 0-100
  bool mc_debug_hitboxes;   // draw attack hit boxes
  bool mc_life_bar;         // Mario Combat life (10) and HUD instead of SMW's small/big damage
  float mc_damage_per_hit;  // life lost per SMW hit (MC's own enemies deal 0.5)

  // Settings-menu options
  bool show_fps;
  bool vsync;
  uint8 master_volume;      // 0-100
  bool save100_file3;       // save file 3 is a 100% (96 exit) completed game; the player's own file 3 is backed up
  bool cheat[kCheat_Count];
  uint8 cheat_powerup;      // 0 = off, 1 mushroom, 2 feather, 3 fire flower: kept every frame in a level
  uint8 cheat_knockback;    // 1-20: Mario Combat Mario is knocked this many times as far by hits (1 = normal)
  bool audio_mono;          // both speakers get the same mix (left + right)
  uint8 in_background;      // when the window loses focus: kBackground_*
  uint8 rumble;             // gamepad rumble on hits, falls and quakes: 0 off, 1 light, 2 full
  uint8 stick_deadzone;     // left stick: percent of its throw that does nothing (0 = the default, 30)
  bool boss_rush_all;       // boss rush: every boss of the game (else the 7 Koopa Kids and Bowser)
  bool boss_rush_end_on_death;  // boss rush: a death ends the run (else the boss is fought again)
} Config;

enum {
  kMsuEnabled_Msu = 1,
  kMsuEnabled_MsuDeluxe = 2,
  kMsuEnabled_Opuz = 4,
};
enum {
  kGamepadBtn_Invalid = -1,
  kGamepadBtn_A,
  kGamepadBtn_B,
  kGamepadBtn_X,
  kGamepadBtn_Y,
  kGamepadBtn_Back,
  kGamepadBtn_Guide,
  kGamepadBtn_Start,
  kGamepadBtn_L3,
  kGamepadBtn_R3,
  kGamepadBtn_L1,
  kGamepadBtn_R1,
  kGamepadBtn_DpadUp,
  kGamepadBtn_DpadDown,
  kGamepadBtn_DpadLeft,
  kGamepadBtn_DpadRight,
  kGamepadBtn_L2,
  kGamepadBtn_R2,
  kGamepadBtn_Count,
};

extern Config g_config;

void ParseConfigFile(const char *filename);
int FindCmdForSdlKey(SDL_Keycode code, SDL_Keymod mod);
int FindCmdForGamepadButton(int button, uint32 modifiers);

// Runtime bindings (settings menu). Keys use the internal key format (keycode | modifier flags).
uint16 Config_KeyFromSdl(SDL_Keycode code, SDL_Keymod mod);
uint16 Config_GetKeyBinding(int bind);
int Config_GetPadBinding(int bind);                // gamepad button of player 1, or -1
void Config_SetKeyBinding(int bind, uint16 key);   // swaps with another menu binding that used the key
void Config_SetPadBinding(int bind, int button);
void Config_ResetKeyBindings(void);
void Config_ResetPadBindings(void);
void Config_KeyName(uint16 key, char *buf, int size);
const char *Config_PadButtonName(int button);
void Config_ResetToDefaults(void);                 // every option the menu shows, plus all bindings
bool Config_SaveUserIni(void);                     // writes smw.user.ini next to the exe
int Config_StickDeadzone(void);                    // the left stick's dead zone in percent (5-60, default 30)
