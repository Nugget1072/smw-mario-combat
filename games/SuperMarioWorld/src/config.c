#include "config.h"
#include "types.h"
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "util.h"
#include "pilot.h"

enum {
  kKeyMod_ScanCode = 0x200,
  kKeyMod_Alt = 0x400,
  kKeyMod_Shift = 0x800,
  kKeyMod_Ctrl = 0x1000,
};

Config g_config;

#define REMAP_SDL_KEYCODE(key) ((key) & SDLK_SCANCODE_MASK ? kKeyMod_ScanCode : 0) | (key) & (kKeyMod_ScanCode - 1)
#define _(x) REMAP_SDL_KEYCODE(x)
#define S(x) REMAP_SDL_KEYCODE(x) | kKeyMod_Shift
#define A(x) REMAP_SDL_KEYCODE(x) | kKeyMod_Alt
#define C(x) REMAP_SDL_KEYCODE(x) | kKeyMod_Ctrl
#define N 0
static const uint16 kDefaultKbdControls[kKeys_Total] = {
  0,
  // Controls
  _(SDLK_UP), _(SDLK_DOWN), _(SDLK_LEFT), _(SDLK_RIGHT), _(SDLK_RSHIFT), _(SDLK_RETURN), _(SDLK_x), _(SDLK_z), _(SDLK_s), _(SDLK_a), _(SDLK_c), _(SDLK_v),
  // ControlsP2
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  // LoadState
  _(SDLK_F1), _(SDLK_F2), N, _(SDLK_F4), _(SDLK_F5), _(SDLK_F6), _(SDLK_F7), _(SDLK_F8), _(SDLK_F9), _(SDLK_F10), N, N, N, N, N, N, N, N, N, N,
  // SaveState
  S(SDLK_F1), S(SDLK_F2), S(SDLK_F3), S(SDLK_F4), S(SDLK_F5), S(SDLK_F6), S(SDLK_F7), S(SDLK_F8), S(SDLK_F9), S(SDLK_F10), N, N, N, N, N, N, N, N, N, N,
  // Replay State
  C(SDLK_F1), C(SDLK_F2), C(SDLK_F3), C(SDLK_F4), C(SDLK_F5), C(SDLK_F6), C(SDLK_F7), C(SDLK_F8), C(SDLK_F9), C(SDLK_F10), N, N, N, N, N, N, N, N, N, N,
  // Load Ref State
  _(SDLK_1), _(SDLK_2), _(SDLK_3), _(SDLK_4), _(SDLK_5), _(SDLK_6), _(SDLK_7), _(SDLK_8), _(SDLK_9), _(SDLK_0), _(SDLK_MINUS), _(SDLK_EQUALS), _(SDLK_BACKSPACE), N, N, N, N, N, N, N,
  // Replay Ref State
  C(SDLK_1), C(SDLK_2), C(SDLK_3), C(SDLK_4), C(SDLK_5), C(SDLK_6), C(SDLK_7), C(SDLK_8), C(SDLK_9), C(SDLK_0), C(SDLK_MINUS), C(SDLK_EQUALS), C(SDLK_BACKSPACE), N, N, N, N, N, N, N,
  // CheatLife, CheatJump, ToggleWhichFrame,
  _(SDLK_w), _(SDLK_q), S(SDLK_r),
  // ClearKeyLog, StopReplay, Fullscreen, Reset, Pause, PauseDimmed, Turbo, ReplayTurbo, WindowBigger, WindowSmaller, DisplayPerf, ToggleRenderer
  _(SDLK_k), _(SDLK_l), A(SDLK_RETURN), C(SDLK_r), S(SDLK_p), _(SDLK_p), _(SDLK_TAB), _(SDLK_t), N, N, _(SDLK_f), _(SDLK_r),
  // VolumeUp VolumeDown
  0, 0,
  // ToggleMario, Settings, ShowFps, FullscreenToggle (F3 is no longer load-state slot 3)
  _(SDLK_q), _(SDLK_ESCAPE), _(SDLK_F3), _(SDLK_F11),
};
#undef _
#undef A
#undef C
#undef S
#undef N

typedef struct KeyNameId {
  const char *name;
  uint16 id, size;
} KeyNameId;

#define M(n) {#n, kKeys_##n, kKeys_##n##_Last - kKeys_##n + 1}
#define S(n) {#n, kKeys_##n, 1}
static const KeyNameId kKeyNameId[] = {
  {"Null", kKeys_Null, 65535},
  M(Controls), M(ControlsP2),
  M(Load), M(Save), M(Replay), M(LoadRef), M(ReplayRef),
  S(CheatLife), S(CheatJump), S(ToggleWhichFrame),
  S(ClearKeyLog), S(StopReplay), S(Fullscreen), S(Reset),
  S(Pause), S(PauseDimmed), S(Turbo), S(ReplayTurbo), S(WindowBigger), S(WindowSmaller), S(VolumeUp), S(VolumeDown), S(DisplayPerf), S(ToggleRenderer),
  S(ToggleMario), S(Settings), S(ShowFps), S(FullscreenToggle),
};
#undef S
#undef M
typedef struct KeyMapHashEnt {
  uint16 key, cmd, next;
} KeyMapHashEnt;

static uint16 keymap_hash_first[255];
static KeyMapHashEnt *keymap_hash;
static int keymap_hash_size;
static bool has_keynameid[countof(kKeyNameId)];

static bool KeyMapHash_Add(uint16 key, uint16 cmd) {
  if (!key)
    return false;

  if (cmd == kKeys_Controls)
    g_config.has_keyboard_controls |= 1;
  else if (cmd == kKeys_ControlsP2)
    g_config.has_keyboard_controls |= 2;

  if ((keymap_hash_size & 0xff) == 0) {
    if (keymap_hash_size > 10000)
      Die("Too many keys");
    keymap_hash = (KeyMapHashEnt*)realloc(keymap_hash, sizeof(KeyMapHashEnt) * (keymap_hash_size + 256));
  }
  int i = keymap_hash_size++;
  KeyMapHashEnt *ent = &keymap_hash[i];
  ent->key = key;
  ent->cmd = cmd;
  ent->next = 0;
  int j = (uint32)key % 255;

  uint16 *cur = &keymap_hash_first[j];
  while (*cur) {
    KeyMapHashEnt *ent = &keymap_hash[*cur - 1];
    if (ent->key == key)
      return false;
    cur = &ent->next;
  }
  *cur = i + 1;
  return true;
}

static int KeyMapHash_Find(uint16 key) {
  int i = keymap_hash_first[key % 255];
  while (i) {
    KeyMapHashEnt *ent = &keymap_hash[i - 1];
    if (ent->key == key)
      return ent->cmd;
    i = ent->next;
  }
  return 0;
}

int FindCmdForSdlKey(SDL_Keycode code, SDL_Keymod mod) {
  if (code & ~(SDLK_SCANCODE_MASK | 0x1ff))
    return 0;
  int key = 0;
  if (code != SDLK_LALT && code != SDLK_RALT)
    key |= mod & KMOD_ALT ? kKeyMod_Alt : 0;
  if (code != SDLK_LCTRL && code != SDLK_RCTRL)
    key |= mod & KMOD_CTRL ? kKeyMod_Ctrl : 0;
  if (code != SDLK_LSHIFT && code != SDLK_RSHIFT)
    key |= mod & KMOD_SHIFT ? kKeyMod_Shift : 0;
  key |= REMAP_SDL_KEYCODE(code);
  return KeyMapHash_Find(key);
}

static void ParseKeyArray(char *value, int cmd, int size) {
  char *s;
  int i = 0;
  for (; i < size && (s = NextDelim(&value, ',')) != NULL; i++, cmd += (cmd != 0)) {
    if (*s == 0)
      continue;
    int key_with_mod = 0;
    for (;;) {
      if (StringStartsWithNoCase(s, "Shift+")) {
        key_with_mod |= kKeyMod_Shift, s += 6;
      } else if (StringStartsWithNoCase(s, "Ctrl+")) {
        key_with_mod |= kKeyMod_Ctrl, s += 5;
      } else if (StringStartsWithNoCase(s, "Alt+")) {
        key_with_mod |= kKeyMod_Alt, s += 4;
      } else {
        break;
      }
    }
    SDL_Keycode key = SDL_GetKeyFromName(s);
    if (key == SDLK_UNKNOWN) {
      fprintf(stderr, "Unknown key: '%s'\n", s);
      continue;
    }
    if (!KeyMapHash_Add(key_with_mod | REMAP_SDL_KEYCODE(key), cmd))
      fprintf(stderr, "Duplicate key: '%s'\n", s);
  }
}

typedef struct GamepadMapEnt {
  uint32 modifiers;
  uint16 cmd, next;
  uint16 button;
} GamepadMapEnt;

static uint16 joymap_first[kGamepadBtn_Count * 2];  // 2 gamepads
static GamepadMapEnt *joymap_ents;
static int joymap_size;
static uint8 has_assigned_joypad_controls;

static int CountBits32(uint32 n) {
  int count = 0;
  for (; n != 0; count++)
    n &= (n - 1);
  return count;
}

static void GamepadMap_Add(int button, uint32 modifiers, uint16 cmd) {
  if ((joymap_size & 0xff) == 0) {
    if (joymap_size > 1000)
      Die("Too many joypad keys"); 
    joymap_ents = (GamepadMapEnt*)realloc(joymap_ents, sizeof(GamepadMapEnt) * (joymap_size + 64));
    if (!joymap_ents) Die("realloc failure");
  }
  uint16 *p = &joymap_first[button];
  // Insert it as early as possible but before after any entry with more modifiers.
  int cb = CountBits32(modifiers);
  while (*p && cb < CountBits32(joymap_ents[*p - 1].modifiers))
    p = &joymap_ents[*p - 1].next;
  int i = joymap_size++;
  GamepadMapEnt *ent = &joymap_ents[i];
  ent->modifiers = modifiers;
  ent->cmd = cmd;
  ent->button = button;
  ent->next = *p;
  *p = i + 1;
}

int FindCmdForGamepadButton(int button, uint32 modifiers) {
  GamepadMapEnt *ent;
  for(int e = joymap_first[button]; e != 0; e = ent->next) {
    ent = &joymap_ents[e - 1];
    if ((modifiers & ent->modifiers) == ent->modifiers)
      return ent->cmd;
  }
  return 0;
}

static int ParseGamepadButtonName(const char **value) {
  const char *s = *value;
  // Longest substring first
  static const char *const kGamepadKeyNames[] = {
    "Back", "Guide", "Start", "L3", "R3",
    "L1", "R1", "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "L2", "R2",
    "Lb", "Rb", "A", "B", "X", "Y"
  };
  static const uint8 kGamepadKeyIds[] = {
    kGamepadBtn_Back, kGamepadBtn_Guide, kGamepadBtn_Start, kGamepadBtn_L3, kGamepadBtn_R3,
    kGamepadBtn_L1, kGamepadBtn_R1, kGamepadBtn_DpadUp, kGamepadBtn_DpadDown, kGamepadBtn_DpadLeft, kGamepadBtn_DpadRight, kGamepadBtn_L2, kGamepadBtn_R2,
    kGamepadBtn_L1, kGamepadBtn_R1, kGamepadBtn_A, kGamepadBtn_B, kGamepadBtn_X, kGamepadBtn_Y,
  };
  for (size_t i = 0; i != countof(kGamepadKeyNames); i++) {
    const char *r = StringStartsWithNoCase(s, kGamepadKeyNames[i]);
    if (r) {
      *value = r;
      return kGamepadKeyIds[i];
    }
  }
  return kGamepadBtn_Invalid;
}

// SNES L/R sit on the triggers so the right bumper is free for switching Mario (kBind_ToggleMario).
static const uint8 kDefaultGamepadCmds[] = {
  kGamepadBtn_DpadUp, kGamepadBtn_DpadDown, kGamepadBtn_DpadLeft, kGamepadBtn_DpadRight, kGamepadBtn_Back, kGamepadBtn_Start,
  kGamepadBtn_B, kGamepadBtn_A, kGamepadBtn_Y, kGamepadBtn_X, kGamepadBtn_L2, kGamepadBtn_R2,
};
static const int8 kDefaultGamepadExtra[4] = { kGamepadBtn_R1, kGamepadBtn_L3, kGamepadBtn_Invalid, kGamepadBtn_Invalid };  // ToggleMario, Settings, ShowFps, FullscreenToggle
static uint8 has_assigned_joypad_extra;

static void ParseGamepadArray(int gamepad, char *value, int cmd, int size) {
  char *s;
  int i = 0;
  for (; i < size && (s = NextDelim(&value, ',')) != NULL; i++, cmd += (cmd != 0)) {
    if (*s == 0)
      continue;
    int gamepad_cur = gamepad;
    uint32 modifiers = 0;
    const char *ss = s;
    for (;;) {
      int button = ParseGamepadButtonName(&ss);
      if (button == kGamepadBtn_Invalid) BAD: {
        fprintf(stderr, "Unknown gamepad button: '%s'\n", s);
        break;
      }
      while (*ss == ' ' || *ss == '\t') ss++;
      if (*ss == '+') {
        ss++;
        modifiers |= 1 << button;
      } else if (*ss == 0) {
        GamepadMap_Add(button + gamepad_cur * kGamepadBtn_Count, modifiers, cmd);
        break;
      } else
        goto BAD;
    }
  }
}

static void RegisterDefaultKeys(void) {
  for (int i = 1; i < countof(kKeyNameId); i++) {
    if (!has_keynameid[i]) {
      int size = kKeyNameId[i].size, k = kKeyNameId[i].id;
      for (int j = 0; j < size; j++, k++)
        KeyMapHash_Add(kDefaultKbdControls[k], k);
    }
  }
  if (!(has_assigned_joypad_controls & 1)) {
    for (int i = 0; i < countof(kDefaultGamepadCmds); i++)
      GamepadMap_Add(kDefaultGamepadCmds[i], 0, kKeys_Controls + i);
  }
  if (!(has_assigned_joypad_controls & 2)) {
    for (int i = 0; i < countof(kDefaultGamepadCmds); i++)
      GamepadMap_Add(kDefaultGamepadCmds[i] + kGamepadBtn_Count, 0, kKeys_ControlsP2 + i);
  }
  for (int i = 0; i < 4; i++)
    if (!(has_assigned_joypad_extra & (1 << i)) && kDefaultGamepadExtra[i] != kGamepadBtn_Invalid)
      GamepadMap_Add(kDefaultGamepadExtra[i], 0, kKeys_ToggleMario + i);
}

// ---------------------------------------------------------------------------------------------
// Runtime bindings for the settings menu
// ---------------------------------------------------------------------------------------------
static int GetIniSection(const char *s);

static int BindCmd(int bind) {
  return bind < 12 ? kKeys_Controls + bind : kKeys_ToggleMario + (bind - kBind_ToggleMario);
}

uint16 Config_KeyFromSdl(SDL_Keycode code, SDL_Keymod mod) {
  int key = 0;
  if (code != SDLK_LALT && code != SDLK_RALT)
    key |= mod & KMOD_ALT ? kKeyMod_Alt : 0;
  if (code != SDLK_LCTRL && code != SDLK_RCTRL)
    key |= mod & KMOD_CTRL ? kKeyMod_Ctrl : 0;
  if (code != SDLK_LSHIFT && code != SDLK_RSHIFT)
    key |= mod & KMOD_SHIFT ? kKeyMod_Shift : 0;
  return key | REMAP_SDL_KEYCODE(code);
}

enum { kDeadKey = 0xffff };

static void KillKeyEntries(uint16 key, int cmd) {  // by key (key != 0) or by command (cmd != 0)
  for (int i = 0; i < keymap_hash_size; i++) {
    KeyMapHashEnt *e = &keymap_hash[i];
    if (e->key != kDeadKey && ((key && e->key == key) || (cmd && e->cmd == cmd)))
      e->key = kDeadKey;
  }
}

static void AddKeyEntry(uint16 key, int cmd) {
  // A dead entry may still sit in the hash chain for this key; KeyMapHash_Add checks only live duplicates.
  KeyMapHash_Add(key, cmd);
}

uint16 Config_GetKeyBinding(int bind) {
  int cmd = BindCmd(bind);
  for (int i = 0; i < keymap_hash_size; i++)
    if (keymap_hash[i].key != kDeadKey && keymap_hash[i].cmd == cmd)
      return keymap_hash[i].key;
  return 0;
}

void Config_SetKeyBinding(int bind, uint16 key) {
  int cmd = BindCmd(bind);
  uint16 old = Config_GetKeyBinding(bind);
  int other_cmd = KeyMapHash_Find(key);
  KillKeyEntries(key, 0);
  KillKeyEntries(0, cmd);
  AddKeyEntry(key, cmd);
  for (int b = 0; b < kBind_Count; b++)  // swap: the action that had this key takes our old key
    if (b != bind && BindCmd(b) == other_cmd && old && Config_GetKeyBinding(b) == 0)
      AddKeyEntry(old, other_cmd);
}

static GamepadMapEnt *FindPadEnt(int cmd) {
  for (int i = 0; i < joymap_size; i++) {
    GamepadMapEnt *e = &joymap_ents[i];
    if (e->cmd == cmd && e->modifiers == 0 && e->button < kGamepadBtn_Count)
      return e;
  }
  return NULL;
}

int Config_GetPadBinding(int bind) {
  GamepadMapEnt *e = FindPadEnt(BindCmd(bind));
  return e ? e->button : -1;
}

void Config_SetPadBinding(int bind, int button) {
  int cmd = BindCmd(bind);
  int old = Config_GetPadBinding(bind);
  GamepadMapEnt *e;
  int other_cmd = 0;
  for (int i = 0; i < joymap_size; i++) {  // free the button (player 1, no modifiers)
    e = &joymap_ents[i];
    if (e->button == button && e->modifiers == 0 && e->cmd) {
      other_cmd = e->cmd;
      e->cmd = 0, e->modifiers = 0xffffffff;
    }
  }
  while ((e = FindPadEnt(cmd)) != NULL)
    e->cmd = 0, e->modifiers = 0xffffffff;
  GamepadMap_Add(button, 0, cmd);
  for (int b = 0; b < kBind_Count; b++)
    if (b != bind && BindCmd(b) == other_cmd && old >= 0 && Config_GetPadBinding(b) < 0)
      GamepadMap_Add(old, 0, other_cmd);
}

void Config_ResetKeyBindings(void) {
  for (int b = 0; b < kBind_Count; b++)
    KillKeyEntries(0, BindCmd(b));
  for (int b = 0; b < kBind_Count; b++) {
    uint16 k = kDefaultKbdControls[BindCmd(b)];
    KillKeyEntries(k, 0);
    AddKeyEntry(k, BindCmd(b));
  }
}

void Config_ResetPadBindings(void) {
  for (int b = 0; b < kBind_Count; b++) {
    GamepadMapEnt *e;
    while ((e = FindPadEnt(BindCmd(b))) != NULL)
      e->cmd = 0, e->modifiers = 0xffffffff;
  }
  for (int b = 0; b < kBind_Count; b++) {
    int btn = b < 12 ? kDefaultGamepadCmds[b] : kDefaultGamepadExtra[b - 12];
    if (btn == kGamepadBtn_Invalid)
      continue;
    for (int i = 0; i < joymap_size; i++)
      if (joymap_ents[i].button == btn && joymap_ents[i].modifiers == 0)
        joymap_ents[i].cmd = 0, joymap_ents[i].modifiers = 0xffffffff;
    GamepadMap_Add(btn, 0, BindCmd(b));
  }
}

void Config_KeyName(uint16 key, char *buf, int size) {
  buf[0] = 0;
  if (!key) {
    snprintf(buf, size, "---");
    return;
  }
  SDL_Keycode code = (key & kKeyMod_ScanCode) ? (SDLK_SCANCODE_MASK | (key & 0x1ff)) : (key & 0x1ff);
  const char *n = SDL_GetKeyName(code);
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%s%s%s%s", key & kKeyMod_Ctrl ? "CTRL+" : "", key & kKeyMod_Alt ? "ALT+" : "",
           key & kKeyMod_Shift ? "SHIFT+" : "", (n && *n) ? n : "?");
  if (!strcmp(n, "Return")) snprintf(tmp, sizeof(tmp), "ENTER");
  if (!strcmp(n, "Escape")) snprintf(tmp, sizeof(tmp), "ESC");
  for (char *p = tmp; *p; p++)
    if (*p >= 'a' && *p <= 'z') *p -= 32;
  snprintf(buf, size, "%s", tmp);
}

const char *Config_PadButtonName(int button) {
  static const char *const kNames[kGamepadBtn_Count] = {
    "A", "B", "X", "Y", "BACK", "GUIDE", "START", "L3", "R3", "LB", "RB", "D-UP", "D-DOWN", "D-LEFT", "D-RIGHT", "LT", "RT",
  };
  return button >= 0 && button < kGamepadBtn_Count ? kNames[button] : "---";
}

static const char *IniKeyName(uint16 key, char *buf, int size) {
  // ini syntax: [Shift+|Ctrl+|Alt+]<SDL key name>
  SDL_Keycode code = (key & kKeyMod_ScanCode) ? (SDLK_SCANCODE_MASK | (key & 0x1ff)) : (key & 0x1ff);
  snprintf(buf, size, "%s%s%s%s", key & kKeyMod_Shift ? "Shift+" : "", key & kKeyMod_Ctrl ? "Ctrl+" : "",
           key & kKeyMod_Alt ? "Alt+" : "", SDL_GetKeyName(code));
  return buf;
}

static const char *IniPadName(int button) {
  static const char *const kNames[kGamepadBtn_Count] = {
    "A", "B", "X", "Y", "Back", "Guide", "Start", "L3", "R3", "Lb", "Rb", "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "L2", "R2",
  };
  return button >= 0 && button < kGamepadBtn_Count ? kNames[button] : "";
}

void Config_ResetToDefaults(void) {
  g_config.fullscreen = 0;
  g_config.window_scale = 3;
  g_config.ignore_aspect_ratio = false;
  g_config.linear_filtering = false;
  g_config.show_fps = false;
  g_config.vsync = true;
  g_config.master_volume = 100;
  g_config.mc_enable = true;
  g_config.mc_up_jumps = true;
  g_config.mc_sounds = true;
  g_config.mc_sound_volume = 100;
  g_config.mc_life_bar = true;
  g_config.mc_damage_per_hit = 0.5f;  // MC's own enemies' punch: 20 hits from a full bar
  g_config.mc_debug_hitboxes = false;
  memset(g_config.cheat, 0, sizeof(g_config.cheat));
  g_config.cheat_powerup = 0;
  g_config.cheat_knockback = 1;
  g_config.audio_mono = false;
  g_config.in_background = kBackground_Play;
  g_config.rumble = 2;
  g_config.stick_deadzone = 30;
  g_config.boss_rush_all = false;
  g_config.boss_rush_end_on_death = false;
  // save100_file3 is left alone: switching it changes the save file (see Cheats_SetSave100)
  Config_ResetKeyBindings();
  Config_ResetPadBindings();
}

// smw.user.ini is read instead of smw.ini when present, so it is written as a full copy of smw.ini with the
// values the menu manages replaced (and missing keys added at the end of their section).
typedef struct { int section; const char *key; char value[256]; bool written; } IniValue;

static const char *const kCheatIniNames[kCheat_Count] = {
  "CheatInfiniteLife", "CheatNoDamage", "CheatInfiniteTime", "CheatInfiniteLives", "CheatInvincible",
  "CheatMoonJump", "CheatPSpeed", "CheatSwitches", "CheatReplayLevels", "CheatTripleEnemies",
  "CheatNoKnockback", "CheatSuperPunch", "CheatAlwaysYoshi", "CheatSlowMotion", "CheatMcKoopas", "CheatMirror",
  "CheatOneHit", "CheatEnemyCoins", "CheatKingKoopas", "CheatDoubleJump", "CheatGroundPound", "CheatWallJump",
  "CheatSlippery", "CheatCoinMagnet", "CheatFlyingYoshi", "CheatMcHits", "CheatLifeSteal", "CheatFirePunch",
  "CheatInstantHits", "CheatBlockBuster", "CheatPitRescue", "CheatInfiniteJumps", "CheatOpenMap", "CheatStompAll",
  "CheatFastEnemies", "CheatChaseEnemies", "CheatJumpEnemies", "CheatLowGravity", "CheatAirDash", "CheatRespawn",
  "CheatPunchAll",
};

int Config_StickDeadzone(void) {
  return g_config.stick_deadzone ? IntMin(60, IntMax(5, g_config.stick_deadzone)) : 30;
}

bool Config_SaveUserIni(void) {
  // Test pilots change settings through the menu; they must not leave those changes behind for the next run.
  if (Pilot_Active())
    return true;
  IniValue v[96];
  int n = 0;
#define PUT(sec, k, ...) do { v[n].section = sec; v[n].key = k; v[n].written = false; \
    snprintf(v[n].value, sizeof(v[n].value), __VA_ARGS__); n++; } while (0)
  char a[64], b[64];
  PUT(1, "Fullscreen", "%d", g_config.fullscreen);
  PUT(1, "WindowScale", "%d", g_config.window_scale);
  PUT(1, "IgnoreAspectRatio", "%d", g_config.ignore_aspect_ratio);
  PUT(1, "LinearFiltering", "%d", g_config.linear_filtering);
  PUT(1, "ShowFps", "%d", g_config.show_fps);
  PUT(1, "VSync", "%d", g_config.vsync);
  PUT(2, "MasterVolume", "%d", g_config.master_volume);
  PUT(2, "MonoAudio", "%d", g_config.audio_mono);
  PUT(3, "InBackground", "%d", g_config.in_background);
  PUT(6, "Enable", "%d", g_config.mc_enable);
  PUT(6, "UpJumps", "%d", g_config.mc_up_jumps);
  PUT(6, "Sounds", "%d", g_config.mc_sounds);
  PUT(6, "SoundVolume", "%d", g_config.mc_sound_volume);
  PUT(6, "LifeBar", "%d", g_config.mc_life_bar);
  PUT(6, "HitDamage", "%.1f", g_config.mc_damage_per_hit);
  PUT(6, "DebugHitboxes", "%d", g_config.mc_debug_hitboxes);
  PUT(6, "Save100File3", "%d", g_config.save100_file3);
  for (int i = 0; i < kCheat_Count; i++)
    PUT(6, kCheatIniNames[i], "%d", g_config.cheat[i]);
  PUT(6, "CheatPowerUp", "%d", g_config.cheat_powerup);
  PUT(6, "CheatKnockback", "%d", IntMax(1, g_config.cheat_knockback));
  PUT(6, "BossRushAll", "%d", g_config.boss_rush_all);
  PUT(6, "BossRushEndOnDeath", "%d", g_config.boss_rush_end_on_death);
  {
    char ctl[512] = "";
    for (int i = 0; i < 12; i++) {
      uint16 k = Config_GetKeyBinding(i);
      strcat(ctl, i ? ", " : "");
      if (k) strcat(ctl, IniKeyName(k, a, sizeof(a)));
    }
    PUT(0, "Controls", "%s", ctl);
    uint16 kt = Config_GetKeyBinding(kBind_ToggleMario), ks = Config_GetKeyBinding(kBind_Settings);
    PUT(0, "ToggleMario", "%s", kt ? IniKeyName(kt, a, sizeof(a)) : "");
    PUT(0, "Settings", "%s", ks ? IniKeyName(ks, b, sizeof(b)) : "");
    uint16 kf = Config_GetKeyBinding(kBind_ShowFps), kw = Config_GetKeyBinding(kBind_Fullscreen);
    char c[64], d[64];
    PUT(0, "ShowFps", "%s", kf ? IniKeyName(kf, c, sizeof(c)) : "");
    PUT(0, "FullscreenToggle", "%s", kw ? IniKeyName(kw, d, sizeof(d)) : "");
    char pad[512] = "";
    for (int i = 0; i < 12; i++) {
      strcat(pad, i ? ", " : "");
      strcat(pad, IniPadName(Config_GetPadBinding(i)));
    }
    PUT(5, "Controls", "%s", pad);
    PUT(5, "ToggleMario", "%s", IniPadName(Config_GetPadBinding(kBind_ToggleMario)));
    PUT(5, "Settings", "%s", IniPadName(Config_GetPadBinding(kBind_Settings)));
    PUT(5, "ShowFps", "%s", IniPadName(Config_GetPadBinding(kBind_ShowFps)));
    PUT(5, "FullscreenToggle", "%s", IniPadName(Config_GetPadBinding(kBind_Fullscreen)));
    PUT(5, "Rumble", "%d", g_config.rumble);
    PUT(5, "StickDeadzone", "%d", Config_StickDeadzone());
  }
#undef PUT
  char *tmpl = (char *)ReadWholeFile("smw.ini", NULL);
  FILE *f = fopen("smw.user.ini", "w");
  if (!f) {
    free(tmpl);
    return false;
  }
  fprintf(f, "# Written by the in-game settings menu. Delete this file to go back to smw.ini.\n");
  int section = -2;
  char line[1024];
  const char *p = tmpl;
  bool sections_seen[8] = { 0 };
  while (p && *p) {
    const char *e = strchr(p, '\n');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    memcpy(line, p, len);
    line[len] = 0;
    if (len && line[len - 1] == '\r') line[--len] = 0;
    p = e ? e + 1 : p + strlen(p);
    char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '[') {
      // flush keys of the section being left that the template did not have
      for (int i = 0; i < n; i++)
        if (v[i].section == section && !v[i].written)
          fprintf(f, "%s = %s\n", v[i].key, v[i].value), v[i].written = true;
      section = GetIniSection(s);
      if (section >= 0 && section < 8) sections_seen[section] = true;
      fprintf(f, "%s\n", line);
      continue;
    }
    if (*s != '#' && *s) {
      char key[128];
      int kl = 0;
      while (s[kl] && s[kl] != '=' && s[kl] != ' ' && s[kl] != '\t' && kl < 127) key[kl] = s[kl], kl++;
      key[kl] = 0;
      bool replaced = false;
      for (int i = 0; i < n; i++) {
        if (v[i].section == section && StringEqualsNoCase(key, v[i].key)) {
          if (!v[i].written)
            fprintf(f, "%s = %s\n", v[i].key, v[i].value), v[i].written = true;
          replaced = true;
          break;
        }
      }
      if (replaced)
        continue;
    }
    fprintf(f, "%s\n", line);
  }
  for (int i = 0; i < n; i++)
    if (v[i].section == section && !v[i].written)
      fprintf(f, "%s = %s\n", v[i].key, v[i].value), v[i].written = true;
  static const char *const kSecNames[8] = { "[KeyMap]", "[Graphics]", "[Sound]", "[General]", "[Features]", "[GamepadMap]", "[MarioCombat]", "" };
  for (int sct = 0; sct < 7; sct++) {
    bool any = false;
    for (int i = 0; i < n; i++)
      any |= v[i].section == sct && !v[i].written;
    if (!any) continue;
    fprintf(f, "\n%s\n", kSecNames[sct]);
    for (int i = 0; i < n; i++)
      if (v[i].section == sct && !v[i].written)
        fprintf(f, "%s = %s\n", v[i].key, v[i].value), v[i].written = true;
  }
  fclose(f);
  free(tmpl);
  return true;
}

static int GetIniSection(const char *s) {
  if (StringEqualsNoCase(s, "[KeyMap]"))
    return 0;
  if (StringEqualsNoCase(s, "[Graphics]"))
    return 1;
  if (StringEqualsNoCase(s, "[Sound]"))
    return 2;
  if (StringEqualsNoCase(s, "[General]"))
    return 3;
  if (StringEqualsNoCase(s, "[Features]"))
    return 4;
  if (StringEqualsNoCase(s, "[GamepadMap]"))
    return 5;
  if (StringEqualsNoCase(s, "[MarioCombat]"))
    return 6;
  return -1;
}

bool ParseBool(const char *value, bool *result) {
  bool rv = false;
  switch (*value++ | 32) {
  case '0': if (*value == 0) break; return false;
  case 'f': if (StringEqualsNoCase(value, "alse")) break; return false;
  case 'n': if (StringEqualsNoCase(value, "o")) break; return false;
  case 'o':
    rv = (*value | 32) == 'n';
    if (StringEqualsNoCase(value, rv ? "n" : "ff")) break;
    return false;
  case '1': rv = true; if (*value == 0) break; return false;
  case 'y': rv = true; if (StringEqualsNoCase(value, "es")) break; return false;
  case 't': rv = true; if (StringEqualsNoCase(value, "rue")) break; return false;
  default: return false;
  }
  if (result) {
    *result = rv;
    return true;
  }
  return rv;
}

static bool ParseBoolBit(const char *value, uint32 *data, uint32 mask) {
  bool tmp;
  if (!ParseBool(value, &tmp))
    return false;
  *data = *data & ~mask | (tmp ? mask : 0);
  return true;
}

static bool HandleIniConfig(int section, const char *key, char *value) {
  if (section == 6) {
    if (StringEqualsNoCase(key, "Enable"))
      return ParseBool(value, &g_config.mc_enable);
    if (StringEqualsNoCase(key, "UpJumps"))
      return ParseBool(value, &g_config.mc_up_jumps);
    if (StringEqualsNoCase(key, "Sounds"))
      return ParseBool(value, &g_config.mc_sounds);
    if (StringEqualsNoCase(key, "SoundVolume")) {
      g_config.mc_sound_volume = (uint8)IntMin(100, IntMax(0, atoi(value)));
      return true;
    }
    if (StringEqualsNoCase(key, "DebugHitboxes"))
      return ParseBool(value, &g_config.mc_debug_hitboxes);
    if (StringEqualsNoCase(key, "LifeBar"))
      return ParseBool(value, &g_config.mc_life_bar);
    if (StringEqualsNoCase(key, "HitDamage")) {
      g_config.mc_damage_per_hit = (float)atof(value);
      return true;
    }
    // (round 20: DamagePerHit, whose old default 2.5 a saved smw.user.ini still holds, gave way to HitDamage = 0.5,
    // Mario Combat's own hit: the old key is read and left alone)
    if (StringEqualsNoCase(key, "DamagePerHit"))
      return true;
    if (StringEqualsNoCase(key, "Save100File3"))
      return ParseBool(value, &g_config.save100_file3);
    for (int i = 0; i < kCheat_Count; i++)
      if (StringEqualsNoCase(key, kCheatIniNames[i]))
        return ParseBool(value, &g_config.cheat[i]);
    if (StringEqualsNoCase(key, "CheatPowerUp")) {
      g_config.cheat_powerup = (uint8)IntMin(3, IntMax(0, atoi(value)));
      return true;
    }
    if (StringEqualsNoCase(key, "CheatKnockback")) {
      g_config.cheat_knockback = (uint8)IntMin(20, IntMax(1, atoi(value)));
      return true;
    }
    if (StringEqualsNoCase(key, "BossRushAll"))
      return ParseBool(value, &g_config.boss_rush_all);
    if (StringEqualsNoCase(key, "BossRushEndOnDeath"))
      return ParseBool(value, &g_config.boss_rush_end_on_death);
    return false;
  }
  if (section == 0) {
    for (int i = 0; i < countof(kKeyNameId); i++) {
      if (StringEqualsNoCase(key, kKeyNameId[i].name)) {
        has_keynameid[i] = true;
        ParseKeyArray(value, kKeyNameId[i].id, kKeyNameId[i].size);
        return true;
      }
    }
  } else if (section == 5) {
    if (StringEqualsNoCase(key, "Rumble")) {
      g_config.rumble = (uint8)IntMin(2, IntMax(0, atoi(value)));
      return true;
    } else if (StringEqualsNoCase(key, "StickDeadzone")) {
      g_config.stick_deadzone = (uint8)IntMin(60, IntMax(5, atoi(value)));
      return true;
    } else if (StringEqualsNoCase(key, "EnableGamepad1")) {
      return ParseBool(value, &g_config.enable_gamepad[0]);
    } else if (StringEqualsNoCase(key, "EnableGamepad2")) {
      return ParseBool(value, &g_config.enable_gamepad[1]);
    } else {
      for (int i = 0; i < countof(kKeyNameId); i++) {
        if (StringEqualsNoCase(key, kKeyNameId[i].name)) {
          int id = kKeyNameId[i].id;
          has_assigned_joypad_controls |= (id == kKeys_Controls) ? 1 : (id == kKeys_ControlsP2) ? 2 : 0;
          if (id >= kKeys_ToggleMario && id <= kKeys_FullscreenToggle)
            has_assigned_joypad_extra |= 1 << (id - kKeys_ToggleMario);
          ParseGamepadArray(id == kKeys_ControlsP2 ? 1 : 0, value, kKeyNameId[i].id, kKeyNameId[i].size);
          return true;
        }
      }
    }
  } else if (section == 1) {
    if (StringEqualsNoCase(key, "WindowSize")) {
      char *s;
      if (StringEqualsNoCase(value, "Auto")){
        g_config.window_width  = 0;
        g_config.window_height = 0;
        return true;
      }
      while ((s = NextDelim(&value, 'x')) != NULL) {
        if(g_config.window_width == 0) {
          g_config.window_width = atoi(s);
        } else {
          g_config.window_height = atoi(s);
          return true;
        }
      }
    } else if (StringEqualsNoCase(key, "EnhancedMode7")) {
      return ParseBool(value, &g_config.enhanced_mode7);
    } else if (StringEqualsNoCase(key, "ShowFps")) {
      return ParseBool(value, &g_config.show_fps);
    } else if (StringEqualsNoCase(key, "VSync")) {
      return ParseBool(value, &g_config.vsync);
    } else if (StringEqualsNoCase(key, "NewRenderer")) {
      return ParseBool(value, &g_config.new_renderer);
    } else if (StringEqualsNoCase(key, "IgnoreAspectRatio")) {
      return ParseBool(value, &g_config.ignore_aspect_ratio);
    } else if (StringEqualsNoCase(key, "Fullscreen")) {
      g_config.fullscreen = (uint8)strtol(value, (char**)NULL, 10);
      return true;
    } else if (StringEqualsNoCase(key, "WindowScale")) {
      g_config.window_scale = (uint8)strtol(value, (char**)NULL, 10);
      return true;
    } else if (StringEqualsNoCase(key, "OutputMethod")) {
      g_config.output_method = StringEqualsNoCase(value, "SDL-Software") ? kOutputMethod_SDLSoftware :
                               StringEqualsNoCase(value, "OpenGL") ? kOutputMethod_OpenGL : kOutputMethod_SDL;
      return true;
    } else if (StringEqualsNoCase(key, "LinearFiltering")) {
      return ParseBool(value, &g_config.linear_filtering);
    } else if (StringEqualsNoCase(key, "NoSpriteLimits")) {
      return ParseBool(value, &g_config.no_sprite_limits);
    } else if (StringEqualsNoCase(key, "LinkGraphics")) {
      g_config.link_graphics = value;
      return true;
    } else if (StringEqualsNoCase(key, "Shader")) {
      g_config.shader = *value ? value : NULL;
      return true;
    }
  } else if (section == 2) {
    if (StringEqualsNoCase(key, "MasterVolume")) {
      g_config.master_volume = (uint8)IntMin(100, IntMax(0, atoi(value)));
      return true;
    } else if (StringEqualsNoCase(key, "MonoAudio")) {
      return ParseBool(value, &g_config.audio_mono);
    } else if (StringEqualsNoCase(key, "EnableAudio")) {
      return ParseBool(value, &g_config.enable_audio);
    } else if (StringEqualsNoCase(key, "AudioFreq")) {
      g_config.audio_freq = (uint16)strtol(value, (char**)NULL, 10);
      return true;
    } else if (StringEqualsNoCase(key, "AudioChannels")) {
      g_config.audio_channels = (uint8)strtol(value, (char**)NULL, 10);
      return true;
    } else if (StringEqualsNoCase(key, "AudioSamples")) {
      g_config.audio_samples = (uint16)strtol(value, (char**)NULL, 10);
      return true;
    } else if (StringEqualsNoCase(key, "EnableMSU")) {
        if (StringEqualsNoCase(value, "opuz"))
        g_config.enable_msu = kMsuEnabled_Opuz;
      else if (StringEqualsNoCase(value, "deluxe"))
        g_config.enable_msu = kMsuEnabled_MsuDeluxe;
      else if (StringEqualsNoCase(value, "deluxe-opuz"))
        g_config.enable_msu = kMsuEnabled_MsuDeluxe | kMsuEnabled_Opuz;
      else 
        return ParseBool(value, (bool*)&g_config.enable_msu);
      return true;
    } else if (StringEqualsNoCase(key, "MSUPath")) {
      g_config.msu_path = value;
      return true;
    } else if (StringEqualsNoCase(key, "MSUVolume")) {
      g_config.msuvolume = atoi(value);
      return true;
    } else if (StringEqualsNoCase(key, "ResumeMSU")) {
      return ParseBool(value, &g_config.resume_msu);
    }
  } else if (section == 3) {
    if (StringEqualsNoCase(key, "InBackground")) {
      g_config.in_background = (uint8)IntMin(kBackground_Pause, IntMax(0, atoi(value)));
      return true;
    } else if (StringEqualsNoCase(key, "Autosave")) {
      g_config.autosave = (bool)strtol(value, (char **)NULL, 10);
      return true;
    } else if (StringEqualsNoCase(key, "SavePlaythrough")) {
      return ParseBool(value, &g_config.save_playthrough);
    } else if (StringEqualsNoCase(key, "DisplayPerfInTitle")) {
      return ParseBool(value, &g_config.display_perf_title);
    } else if (StringEqualsNoCase(key, "DisableFrameDelay")) {
      return ParseBool(value, &g_config.disable_frame_delay);
    }
  } else if (section == 4) {
  }
  return false;
}

static bool ParseOneConfigFile(const char *filename, int depth) {
  char *filedata = (char*)ReadWholeFile(filename, NULL), *p;
  if (!filedata)
    return false;
  
  int section = -2;
  g_config.memory_buffer = filedata;

  for (int lineno = 1; (p = NextLineStripComments(&filedata)) != NULL; lineno++) {
    if (*p == 0)
      continue; // empty line
    if (*p == '[') {
      section = GetIniSection(p);
      if (section < 0)
        fprintf(stderr, "%s:%d: Invalid .ini section %s\n", filename, lineno, p);
    } else if (*p == '!' && SkipPrefix(p + 1, "include ")) {
      char *tt = p + 8;
      char *new_filename = ReplaceFilenameWithNewPath(filename, NextPossiblyQuotedString(&tt));
      if (depth > 10 || !ParseOneConfigFile(new_filename, depth + 1))
        fprintf(stderr, "Warning: Unable to read %s\n", new_filename);
      free(new_filename);
    } else if (section == -2) {
      fprintf(stderr, "%s:%d: Expecting [section]\n", filename, lineno);
    } else {
      char *v = SplitKeyValue(p);
      if (v == NULL) {
        fprintf(stderr, "%s:%d: Expecting 'key=value'\n", filename, lineno);
        continue;
      }
      if (section >= 0 && !HandleIniConfig(section, p, v))
        fprintf(stderr, "%s:%d: Can't parse '%s'\n", filename, lineno, p);
    }
  }
  return true;
}

void ParseConfigFile(const char *filename) {
  g_config.msuvolume = 100;  // default msu volume, 100%
  g_config.save_playthrough = true;
  g_config.mc_enable = true;
  g_config.mc_up_jumps = true;
  g_config.mc_sounds = true;
  g_config.mc_sound_volume = 100;
  g_config.mc_life_bar = true;
  g_config.mc_damage_per_hit = 0.5f;  // MC's own enemies' punch: 20 hits from a full bar
  g_config.vsync = true;
  g_config.master_volume = 100;

  if (filename != NULL || !ParseOneConfigFile("smw.user.ini", 0)) {
    if (filename == NULL)
      filename = "smw.ini";
    if (!ParseOneConfigFile(filename, 0))
      fprintf(stderr, "Warning: Unable to read config file %s\n", filename);
  }
  RegisterDefaultKeys();
}
