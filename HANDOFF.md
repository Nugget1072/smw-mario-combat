# HANDOFF: Mario Combat Mario in SMW

## State (2026-10-20, after round 20; brief `PROJECT_BRIEF.md` (all rounds))
**Done**
- `finished\` holds the portable build. It contains no ROM data:

  | file | what |
  |---|---|
  | `smw.exe` | static SDL2, gcc |
  | `smw.ini` | defaults, including `[MarioCombat]`, `ToggleMario`/`Settings` keys and pad buttons |
  | `smw_assets.bps` | about 23 KB; rebuilds the assets from the player's own ROM at start-up |
  | `README.txt` | controls, settings menu |
  | `LICENSE.txt` | |

  The player adds their own *Super Mario World (USA)* `.sfc` under any name. The settings menu writes
  `smw.user.ini` next to the exe; when present it is read instead of `smw.ini`.
- The Mario Combat hero core (`src/mc/mc_core.c`) matches the original game tick-for-tick: 6,159 of 6,159 ticks
  across 7 reference traces. The only round-2 core change is `McWorld.no_wall_pull` (false in the harness).
- Round 2 (all verified with pilots and screenshots; see `notes/verification.md`):
  - Block walls: `ClampIntoWalls` stops sideways moves at solid tiles. No MC wall pull in SMW. No more crush deaths
    in the YI1 cement U.
  - Long falls: `ClampIntoFloor` lands on floor tops; landing drops the queued fall.
  - Head bumps on blocks and solid sprites (message boxes): one hit per jump; a held jump is used up.
  - Bigger attack boxes: pad 4 px, +10 px reach, 4-frame linger. Cape-immune but stompable enemies (Banzai Bill)
    are knocked out stomp-style.
  - Switch Mario (Q / RB): `McSmw_ToggleCharacter`. Every level entered from the map starts as MC.
  - Settings menu (Esc / L3 / R3): `src/menu.c`, drawn with SMW's GFX28 font. Pages for video, sound, gameplay,
    keyboard/pad remapping with reset, reset-all and quit.
  - Special states:
    - Yoshi: seated jump pose, facing follows SMW, dismount keeps SMW's launch speed.
    - Pipes, swimming, nets, goal: verified.
    - Iggy's tilting platform: the in-air flag is kept while airborne.

- Round 3 (brief `PROJECT_BRIEF.md`, round 3, results `notes/verification.md`):
  - **Upstream fix:** vanilla secondary-entrance tables (`assets/compile_resources.py`). Side pipes and secondary
    doors used to lead to level 100. Verified frame-exact against the ROM code (`RM_BOTH`).
  - Side pipes for MC Mario: 1 px wall contact.
  - Run / P-meter on held X or Y. Cape take-off hands that jump to SMW's physics.
  - Carry with held Y: Y stays in `$15`, only `$16` presses are masked.
  - Punches hit blocks through `CheckIfBlockWasHit`; turn blocks shatter.
  - Power-up colours from SMW's live player palette.
  - MC sounds at -14 dB.
  - Crisp HP-bar label (`mc_hud_export.py`).
  - F3 FPS / F11 fullscreen (new rebindable commands `ShowFps`, `FullscreenToggle`; load-state F3 removed).
  - `src/cheats.c`: CHEATS page and the "file 3 = 100% save" option (`BuildCompletedSaveData` in smw_00.c; backup in
    `saves\file3_backup.srm`).

- Round 4 (brief `PROJECT_BRIEF.md`, round 4):
  - Fire outline black.
  - MC Mario ordered against sprites by OAM slot (`McOverlay_FillObjLine` in `ppu_evaluateSprites`).
  - Multi-hit enemies: Rex, Chucks, Dino-Rhino, with strong = hit level ≥ 4.
  - Replay-castles cheat (A/B = SMW's L+R re-entry).
  - SMW cape on MC Mario (`PlayerDraw_CapeOnly`, `UpdatePlayerCapeImage` split from SetPlayerPose).
  - Infinite lives also on the map.
  - Cheat sweep of 52 combinations.
  - Grey OFF values in the menu.
  - The YI "white grass top" is SMW's own art: identical in snes9x.

- Round 5 (brief `PROJECT_BRIEF.md`, round 5, results `notes/verification.md`):
  - SMW's own sprites where MC has no art: Yoshi, climbing, cape flight, P-balloon, wall-run (`McSmw_UseSmwSprite`).
  - Fire teal → black.
  - Any power-up refills the life bar.
  - Interaction audit: `sprite_sweep.py` (201 sprites × touch / stomp / punch, MC vs SMW) plus tile pilots.
  - Fixes from the audit:
    - Launch carry-over: an upward speed SMW set since the last frame becomes MC `grav`, height-matched. Covers stomp
      bounce, springboards, note blocks.
    - `TouchWallTile`: side tile code (Munchers / castle spikes / side-hit blocks).
    - Wiggler punch = stomp (angry).
    - A punched Rex runs away.
  - New cheats, indices 9–13 after ReplayLevels: `TripleEnemies` (hook in `LoadOneSprite`), `NoKnockback`, `SuperPunch`,
    `AlwaysYoshi` (hook in `LoadSublevel_02A751`), `SlowMotion` (main loop skips every other level frame).
  - Menu: CHEATS hub → MARIO / WORLD pages, ALL CHEATS OFF, "N ON" counts, GFX28 coin cursor and star ON marker.
  - Pilots never write `smw.user.ini`. `build_smw.ps1` does a full rebuild when a header changed.
  - `tools\scripts\regress.ps1` runs the regression pilots.

- Round 6 (brief `PROJECT_BRIEF.md`, round 6, results `notes/verification.md`):
  - Cape spin on X: `UpdateCapePose` runs SMW's spin branch (`SetPlayerPose_00CF0A`) while `timer_active_cape_spin`;
    SMW's spin sprite is shown.
  - Air attack plays out against walls: `Resolve` / `Hit` hide walls from the core while fighting in the air.
  - Hit chains: `ChainLevel` / `ChainCommit` and `McSmw_PointsOverride` (read by `GivePoints`).
  - Punches hit ! blocks 0x16A / 0x16B and flying ? blocks (`SolidSpriteBlock_Entry2`).
  - Triple enemies:
    - kinds walk / fly / terrain, terrain matching, pipe sharing (phase-timer delay), pending copies;
    - audit `tools/scripts/triple_audit.py` (pilot `scroll`, `triplelog`).
  - MC Koopas cheat:
    - `src/mc/mc_koopa.c`, data `mc_koopa_data.c` from `tools/scripts/mc_koopa_export.py` (replaced by `mc_enemy_export.py` in round 7; DefineSprite 163, baked
      frames);
    - hooks in `SprStatus08` / `SprStatus02_Dead` and `PunchSprite`.
  - Mirror (PPU flips the z-buffer between BG2 and BG3; `McOverlay_ApplyLine(..., mirror)`), One-hit, Coins from enemies.
  - Menu: new rows; POWER-UP row shows SMW's item icon from GFX00.

- Round 7 (brief `PROJECT_BRIEF.md`, round 7, results `notes/verification.md`):
  - Flashing fix: frames shown without running the game (slow motion's skipped frames, the open menu) reuse the last
    real frame (`RtlDrawPpuFrame(..., fresh)` in `main.c`) instead of re-rendering the PPU from its post-IRQ state.
  - King Koopas cheat (`kCheat_KingKoopas`, index 18): `src/mc/mc_bowser.c`, MC's Bowser (DefineSprite 532) with fire
    (555) and shockwave (558); data `mc_bowser_data.c` from `tools/scripts/mc_enemy_export.py` (the generic exporter
    that replaced `mc_koopa_export.py`). Hooks: `SprStatus08` / `SprStatus02_Dead`, end of `ProcessNormalSprites`
    (flames / shockwaves), `PunchSprite`, the fireball and cape collisions, sprite clipping A/B (his body box).
    Exclusive with MC Koopas.
  - MC enemies: sprite-frame counter + `SprStatus01_Init` reset (pause no longer resets them), off-screen flags for
    `SubOffscreen`, `McSmw_EnemyHurtsPlayer` (Yoshi), Yoshi hero box, spin kill / slide / lava.
  - Move cheats: Double jump (19), Ground pound (20), Wall jump (21; SMW's `HackHandleWalljump`, SMW Mario only).
  - Menu: four cheat pages (MARIO / MOVES / ENEMIES / WORLD), description line in SMW's message-box font (GFX2A).

- Round 8 (brief `PROJECT_BRIEF.md`, round 8, results `notes/verification.md`):
  - Bowser (King Koopas) has 50 life (`kMaxLife` in `mc_bowser.c`; MC's is 100, halved at the user's request).
  - Cheats 22–24: `Slippery` (`flag_ice_level` forced; MC Mario's ground speed changes at 1/5 rate in `TickOnce`),
    `CoinMagnet` (`CoinMagnet()` in `cheats.c`, tiles 2B–2E within 64 px), `FlyingYoshi` (wings check in
    `Spr035_Yoshi_MouthState00`).
  - Menu: the CHEATS hub rows carry SMW item icons (mushroom, feather, shell, star from GFX00/GFX01).
  - Agent play-testing: `tools/scripts/play.py` (turn-based play through pilot `follow` mode), `tools/play_guide.md`,
    `tools/play_levels.json`, `tools/scripts/replay_session.py` (replays a session's moves on the main build). Haiku
    agents played every level, then the 74 unbeaten ones again on the fixed build (reports `work/play/reports/`,
    collected in `work/play/all_issues*.txt`); each report was checked on Opus by replay.
  - Fixes from the play-testing (all in `src/mc/mc_smw.c` unless noted):
    - `Resolve`: walls are hidden while rising (a jump next to a wall carries Mario up; cling from the apex); the
      auto-scroll screen edge (blocked flag 0x80) is no wall.
    - `ClampIntoScreen`: no-horizontal-scroll levels (auto-scroll, fixed rooms) stop MC's move at screen x 8 / 232, as
      `HandlePlayerLevelColl_00E9C8` does for SMW Mario (MC sets the speed every frame, so SMW's speed reset did not
      hold him and he could pass x 240, SMW's crush line).
    - `ActsLike`: MC's tile tests see the switch-palace switch (EC–FB) as a used block and the P-switch coin/block swaps.
    - `Keys`: Up in front of a door (`InFrontOfDoor`) enters it; a jump held since take-off doesn't wall kick.
    - Running jump: take-off speed scaled by SMW's run table (`HandlePlayerPhysics_JumpSpeedForRun`, `smw_00.c`).
    - Life kept across sublevels (`g_level_fresh`); refilled only when a level is entered from the map.
    - Sunken Ghost Ship: MC Mario drawn when SMW puts OBJ on the sub screen only (`subz` in `McOverlay_ApplyLine`).
    - Layer 2: `SolidAt` / `ClampIntoWalls` / `ClampIntoFloor` / `TouchWallTile` test layer 2 too when `$5B` & 0x80
      (and skip layer 1 when & 0x40), on layer 2's own tile grid (`Map16AtLayer`, offset `$26/$28`). Valley of Bowser 2's
      pillars crushed MC Mario (he moved into them faster than SMW's 1 px push-out). `tileat` prints the layer-2 tile.

- Round 9 (chat request; results `notes/verification.md`):
  - Cheats 25–27: `McHits` (`src/mc/mc_hits.c`), `LifeSteal` (+0.5 `g_life` per landed hit in `AttackSprites`),
    `FirePunch` (`SpawnPlayerFireball` when an MC attack's boxes appear, 20-frame cooldown).
  - MC hit reactions: `PunchSprite` → `McHitsKind` (ordinary enemy kind from `Cheats_EnemyKind`, Lakitu / Magikoopa /
    Hammer Bro anchored; only enemies a punch can defeat; not the Wiggler) → `McHits_Punch` (MC Koopa `TakeHit`
    numbers, life 10). Absorbed hits return; the final hit continues as a strong punch (SMW's knock-out). While
    knocked, `SprStatus08` runs the sprite's routine with `flag_sprites_locked = 1` (it only draws) after
    `McHits_Knocked` moved it (walkers: MC gravity / friction / wall + landing bounces through
    `HandleNormalSpriteLevelCollision`; fliers and `$1686` bit-7 sprites: no gravity, no downward push; anchored: no
    move); the hurt clip (154) frames end it; the sprite's own x/y speeds are restored then. Chain points only on the
    knock-out. `tileat`/`sprites` show `mc-hits life=...`.
  - Audits: `tools/scripts/mchits_sweep.py` (+ `mchits_report.py`, `mchits_sheets.py`): every sprite in Yoshi's House,
    8 combos, cheat on/off; `tools/scripts/mchits_levels.py`: every enemy in a level it appears in (pilot `seek <id>
    [y]`: fly right until that sprite is near, drop Mario 40 px left of it).

- Round 10 (chat request; results `notes/verification.md`):
  - Cheats 28–30: `InstantHits`, `BlockBuster` (`PunchBlocks`: tiles 130 / 132 above the floor row → `GenerateTile`
    2 + `SpawnBrickPieces`), `PitRescue` (`Cheats_PitRescue` in `PlayerState00_CheckPlayerPitFall`: last tile-ground
    position per room; on a fall Mario goes back there, `McSmw_OnTeleported`, `DamagePlayer_Hurt`).
  - Instant hits (`mc_smw.c`): `mc_core.c` gained two appended API functions (scripts unchanged): `McHero_AttackStep`
    (one playhead step of the attack clip, frame scripts run, onEnterFrame doesn't) and `McHero_AttackGotoFrame`.
    `ProbeAttackFrames` finds each move's hit frames with a scratch hero (pilot `attackframes` prints them). After
    each tick `InstantHitsStartup` steps a new move to its first hit frame; before a tick `InstantHitsCombo` steps
    a hit move to the next combo window for the keys held (windows copied from `AttackEnterFrame`), or to its end when
    none is left. xSpeed is capped at maxMoveSpeed while fighting; the per-enemy hit wait is kLingerFrames + 1.
  - Checks: `tools/pilots/instant_hits.txt` + `tools/scripts/instant_hits_check.py` (every move's hit box on its first
    frame), `cheats_r10`, `menu_r10`.

- Round 11 (chat request; results `notes/verification.md`):
  - Cheats 31–33: `InfiniteJumps` (`Cheats_PlayerMoves` / `AirMoves`), `OpenMap`, `StompAll` (`smw_01.c` player–sprite
    contact: a sprite without the "can be jumped on" bit takes SMW's own stomp path).
  - Ground pound animation: SMW Mario `kPoundWindup` 16 frames hanging with `player_spin_jump_flag` (SMW's spin
    poses), then the ducking drop; MC Mario `kPoundWindupTicks` 13 hanging, drawn by `DrawPoundPose` (MC shape 437,
    the hurt clip's curled tumble, somersaulting to `kPoundDropAngle` 135°); landing `Cheats_GroundPoundLanding` adds
    two smoke clouds (type 1) at the feet.
  - Open map (`smw_04.c`): the leaving-direction check of `OwProcess03_StandingStill` is skipped; `OverworldOpenMap`
    (called in `GameMode0C_LoadOverworld` after `LoadOverworldLayer1AndEvents`) draws every untriggered event's
    layer-1 tile change and layer-2 path tiles into RAM only (no flags, no castle destruction, no silent events);
    with the cheat off again it reloads layer 2 from the flags. SMW's own arrival bit ($1EA2 way back) stays.
  - Menu: COIN MAGNET moved to WORLD CHEATS (MOVE CHEATS stays at 12 rows).
  - Tools: `sprite_sweep.py --cheat N` (results in `work/pilot/sprite_sweep_cN`); pilots `pound_anim`, `open_map`,
    `cheats_r11`, `menu_r11`; `cheats_r7_moves` waits 30 frames for the pound landing now.

- Round 12 (chat request; results `notes/verification.md`):
  - Knockback scale: `g_config.cheat_knockback` 1–20 (ini `CheatKnockback`, menu row `O_KNOCKBACK` on MARIO CHEATS,
    `Cheats_KnockbackScale`). `ScaleKnockback` in `mc_smw.c` runs after `McHero_TakeHit` (core untouched): xSpeed (and
    grav for a heavy hit) × √N, so the distance (ground friction, gravity) is N times. Measured by `knockback_r12` +
    `tools/scripts/knockback_check.py` (pilot command `hurt [heavy] [dir]`).
  - `ClampIntoScreen` now also works in scrolling levels: a move stops 1 px inside the screen, because SMW kills the
    player whose on-screen x is 0xF0–0xFF (a 20× knockback at a level's start did that in one frame). Side-exit
    levels are left alone.
  - Cheats 34–37: `FastEnemies` (SprStatus08 runs an ordinary enemy's routine a second time, after stepping its timers
    once more with SMW's `ProcessNormalSprites_GetNormalSpriteOAMIndexAndDecrementTimers`: double time, so a
    timer-triggered throw happens once), `ChaseEnemies` / `JumpEnemies` (`Cheats_EnemyFrame` before the routine, for
    the plain walkers of `IsPlainWalker`: SMW's `ChangeNormalSpriteDirection` toward Mario; yspeed −0x38 on the frame
    Mario leaves the ground), `LowGravity` (SMW Mario: `Cheats_LowGravity_Smw` after `HandlePlayerPhysics_InAir`
    halves the gravity step, fall cap 0x30; MC Mario: grav += 0.25 after each air tick, not walling / pounding).
  - Tools: `sprite_sweep.py --cheat 34,35,36` (several cheats), `sweep_compare.py` (two sweeps side by side),
    `enemy_cheats_levels.py` + `enemy_cheats_sheets.py` (every ordinary enemy in a level it appears in, cheats on /
    off); pilots `knockback_r12`, `cheats_r12`, `menu_r12`. The cheat sweep adds knockback 20× alone, with each cheat
    and with all of them.

- Round 13 (chat request; results `notes/verification.md`):
  - Cheats 38–39: `AirDash` (SNES L/R = `io_controller_press2` 0x20/0x10, swapped in Mirror mode by
    `Cheats_FilterInputs`; SMW Mario: `Cheats_PlayerMoves` starts it, `Cheats_AfterPlayerPhysics` (after
    `HandlePlayerPhysics_InAir`, also the low gravity) holds xspeed 0x68 / yspeed 0 for 12 frames; MC Mario: the press is
    latched per frame in `McSmw_PlayerPhysics` (`g_dash_request`), `AirMoves` holds xSpeed 24 / grav 0 for 10 ticks),
    `Respawn` (`WatchRespawns` in `Cheats_Frame`: a tracked enemy slot whose sprite is gone clears its
    `sprites_load_status` entry ($1938) once no other slot holds that list index; reset on every room load).
    LOW GRAVITY moved to WORLD CHEATS.
  - Settings: `audio_mono` ([Sound] `MonoAudio`), `in_background` ([General] `InBackground`, `kBackground_*`),
    `rumble` / `stick_deadzone` ([GamepadMap] `Rumble`, `StickDeadzone`). `Main_PostProcessAudio` (mono / mute, also on
    the pilot's WAVs); focus from `SDL_WINDOWEVENT_FOCUS_*` (`g_unfocused`; pilots: `focus 0/1`), pause shares the menu's
    frozen-frame path and clears held input; `Main_Rumble` + `src/feedback.c` (`Feedback_Frame`: hurt, death, layer-1
    shake; MC hits in `mc_smw.c`); deadzone radius in `HandleGamepadAxisInput` (64-bit math).
  - Pilot commands: `setting mono|background|rumble|deadzone <v>`, `focus 0/1`, `glideby dx dy`; `rumble` lines on stdout.
  - Tools: `respawn_levels.py`, `settings_check.py`; pilots `airdash_r13`, `settings_r13`, `menu_r13`.

- Round 14 (chat request; results `notes/verification.md`):
  - LEVEL SELECT (`src/levelselect.c/.h`; menu pages `P_LEVELS` / `P_LEVEL_LIST`, rows `K_LGROUP` / `K_LEVEL`): 93
    entries in 11 groups (`kEntries`: level number, group, kind, translevel, name, note). Catalogue from
    `tools/scripts/level_catalogue.py` over pilot `levelscan` (data fingerprints, headers, screen exits, map
    translevels): 92 map levels, 132 rooms, 6 code-loaded rooms, 264 test-level slots, 9 unused rooms (x2 banks).
  - Start: `LevelSelect_Request` (from the menu; refuses outside mode 14 / map standing still) -> next frame
    `LevelSelect_Frame` -> `Launch`: snapshot of the save buffer ($1EA2-$1F2E) + `ow_level_number_lo` (once per
    session), map's per-player coins/lives/power/Yoshi (from the map) or in-level cleanup (from a level),
    `ow_players_map` = the level's bank, midway bit cleared, `McSmw_OnLevelSelect` (fresh life), game mode 0F.
    `LoadLevel` (smw_05.c) takes the level for every fresh-start load (`LevelSelect_TakeLevel`: castles / ghost
    houses load twice, door scene then level) and sets `ow_level_number_lo`; non-map entries skip the door scene
    (`LevelSelect_SkipEntranceScene`). Bonus game / wings: SMW's own flags (`flag_active_bonus_game`,
    `in_yoshi_wings_bonus_area`), loaded as rooms.
  - Practice: at game mode 0C `RestoreMap` puts the snapshot back and clears the exit action, midway, boss-cutscene
    and map-entry flags; title / file select / ending (mode <= 0A or >= 1D) drop the session.
  - Pilot commands: `levelselect <level>`, `levellist`, `levelscan`, `whereami`, `progress` (save-buffer hash),
    `takeexit` (the current screen's exit).
  - Tests: `levelselect_test.py` (every entry from the map and from a level, 186 runs) + `levelselect_sheets.py`,
    `levelselect_scenarios.py` (25 situations); pilots `levelselect_r14`, `menu_r14`.
  - Pilot notes (found in round 13, still true): save state 9 is inside Yoshi's Island 1 (mode 0x14), so a `toggle` carries over to the next
    `loadstate 9` run (the character only resets to MC from the map). After a save state the music only starts in a
    level that loads its own song (e.g. `warp 101`), so sound tests record there.
- Round 15 (chat request; results `notes/verification.md`). `ISSUES_FIXED.txt` is retired: the user deleted it and
  asked for no more; its source was removed and `package.ps1` no longer copies it.
  - Bosses take MC punches (`mc_smw.c` `PunchBoss` / `PunchTouches`): each landed punch is one of SMW's own hits for
    that boss, with its own hurt / invulnerable timing: Morton / Roy / Ludwig the stomp (`McSmw_PunchMortonRoyLudwig`
    in smw_01.c, next to the stomp, for its static table; shell / wall frames bounce it off), Iggy / Larry the stomp
    (screen-space box, slides him down the tilt), Lemmy / Wendy the stomp (decoys duck), Reznor the hit from below,
    Big Boo a thrown block (only while visible), Bowser a Mechakoopa on the head (clown car + head clippings 36 / 37,
    only while `BowserCanBeHit`). Bosses give their own points and are not part of the hit chain.
  - Stomp-to-carry enemies (`StompStuns` / `Stun`): a punch has SMW's stomp result (stunned, status 9; a shelled
    Koopa's shell stays and the Koopa walks out, `$1564` keeps the same punch off it). A punch on a stunned enemy
    kicks it as SMW Mario's touch does (`KickStunned`; not keys or P-switches). With MC hit reactions the final hit
    stuns instead of knocking out.
  - MC hit reactions: Monty Moles are walkers once out of the ground (c2 >= 2); the kind is re-read at each new
    knock. A knocked walker stays knocked until its slide is spent (MC's `x += walk + xSpeed` after the hurt clip),
    so Chucks and others visibly fly and slide before acting again.
  - Ice: MC Mario uses the ice rule on SMW's ice levels too (`flag_ice_level`: rooms 10B, 12D) and stops when a
    slide is slower than one tick's friction with nothing held (the script's `xSpeed -= 1; x += xSpeed` flipped a
    speed under 1 into a backwards step every tick).
  - Jumping enemies: MC Mario counts his ground take-offs (`McSmw_GroundJumps`); each take-off opens an 8-frame
    window in which every plain walker on the ground jumps once (`g_enemy_jump_serial`).
  - Air dash: SMW Mario's dash looks ahead along his side probes (`DashRoomAhead`) and stops at the first wall, and
    every end of the dash (wall, landing, jump) caps his speed to SMW's run speed 0x2F (`EndDash`); before, a dash
    that landed kept 6.5 px/frame and could run him into a ledge or pipe corner where SMW crushed him (reproduced with
    small Mario). MC Mario keeps SMW's in-air flag during a dash (SMW's side pipes only take a standing player).
  - Boss rush (`levelselect.c` "Boss rush"; menu LEVEL SELECT > BOSS RUSH, `P_BOSS_RUSH`; settings
    `BossRushAll`, `BossRushEndOnDeath` in `[MarioCombat]`): 13 boss rooms in game order (`kRushBosses`, rooms from
    pilot `bossrooms` + the map levels' exits), 8 for "castles". Each fight is a `Launch` of the room (practice
    session), no Yoshi. Win: `$13C6` / end-level timer, or Bowser in states 4-6; the next fight starts 150 frames
    later (before the castle cutscene). Death: just before PlayerState09 takes the life, retry the room (lives and
    the fight's power-up back) or end the run. End: lives, coins, item box, power-up and Yoshi as at the start,
    START+SELECT-style exit (mode 0B), the map comes back by the practice session. Clock: game frames without SMW's
    pause, stops at the last win; notices per fight and the run line `BOSS n/N  m:ss.cc` (menu font, bottom left).
  - `package.ps1` no longer wipes `finished\`: it replaces only the delivered files and leaves the player's own ROM,
    `smw.user.ini` and `saves\` there (the user plays from that folder).
  - Pilot commands: `bossrooms`, `bossrush [all] [end]`, `rushinfo`, `attack boss ...` (nearest boss of any kind).
  - Tests: `boss_punch_test.py` (the 7 Koopa Kids, Big Boo and Bowser beaten by MC punches; its bot dies in the
    Reznor rooms' lava, the Reznor fight was won in `bossrush_test.py real_fights`), `bossrush_test.py` (7 scenarios),
    `stun_check.py`, `dash_pipe_test.py` (240 dashes), `mchits_sweep.py` again; pilots `*_r15` (8).
- Round 16 (chat request; results `notes/verification.md`):
  - Title screen (`mc_smw.c` `DrawTitleLogo`, game modes 04-07 only: the file menu takes the place from 08): under
    SMW's logo a "+" (drawn in code, SMW-logo style) and Mario Combat's own logo, its main menu's fiery lettering
    (shape 90 of the SWF; `tools/scripts/mc_export_logo.py` -> generated `src/mc/mc_logo_data.c`, `kMcTitleLogo`,
    177x45: 1.25x the art scale, its black backdrop kept as a soft patch behind the lettering for readability). Drawn
    through the MC overlay at the HUD's z, so it fades with the screen's brightness.
  - Level select / boss rush starts skip the map, and with it the map's clean-up of the level RAM
    (`ClearOverworldAndCutsceneRAM` in `GameMode0C_LoadOverworld`: $1A-$D7 and $13D3-$1BA1). `levelselect.c` now runs
    it once per start in game mode 10 (after the fade, screen black; `ClearAsTheMapDoes`) and puts back the start's
    own settings there (bonus game / wings flags, riding Yoshi, sublevel count 0). It fixes: mode 7 rotation left
    from Iggy / Larry / Reznor (Bowser tilted and not hurt by Mechakoopas, kids on walls), the secondary-entrance
    flag and scroll state of a room behind a pipe (the next level at the wrong place, camera stuck, death), the
    castle-door walk state of ghost houses / wings rooms leaking into normal levels, boss variables.
  - Boss rooms and other non-map starts never play the castle / ghost house door intro: `LevelSelect_SkipEntranceScene`
    (now non-consuming) also guards `InitializeLevelRAM`'s walk through the door (player state 0A), which after Big
    Boo's ghost house put a door scene into Morton's room ("glitched door").
  - Pilot commands: `waitmode <mode> [max]`, `ramdump <name>` (RAM to `<out>/<name>.ram`), `lsclear 0/1` (the
    clean-up off / on, tests only).
  - Tests: `transition_probe.py` (each boss room fresh vs in a boss rush: RAM diff by variable name, screenshots),
    `transition_matrix.py` (16 starting places x 11 targets vs the same target from the map; `--noclear` shows what
    the clean-up fixes), `bossrush_test.py` + `bowser_after_all`, `kids_move`; `levelselect_scenarios.py` menu rows
    shifted by BOSS RUSH (+ `rush_title_refused`); pilots `title_r16`, `bossrush_transitions_r16`,
    `levelselect_subroom_r16`.
- Round 17 (chat request; results `notes/verification.md`):
  - Castle / ghost house door walk (player state 0A): SMW walks Mario to the door without its collision code (no
    blocked flags, in-air flag 36), so MC Mario's probes saw no ground and he glided in his jump pose. `Resolve` now
    counts state 0A as ground: he walks / runs (MC holds the forced Right) along the floor and goes in at the door.
  - Air attacks against blocks: MC's core starts an air attack only from the jump pose (frame 9) and cuts one that
    touches a wall back to it. Falling beside a block the probes reported the wall on the very tick the attack began
    (it was cut at once), and clinging to it (wall pose 17) no attack could start. `g_air_attack_press` (the punch
    newly pressed in the air, set around `McHero_Tick`) makes `Resolve` hide the walls on that tick, as it already did
    while an attack plays: the cling ends and the kick starts. The core is unchanged.
  - Pilot command: `setting mc 0/1` (MC Mario off / on altogether, for SMW-Mario reference runs).
  - Tests: `air_attack_wall_test.py` (YI1's cement column: full jumps, short hops that cling, a head bump under a turn
    block; each press vs the same press in open air), pilots `door_walk_r17`, `air_attack_cling_r17`.
- Round 18 (chat request; results `notes/verification.md`):
  - P-switch coins: `TouchWallTile` handed SMW's side-block code the raw map16 number. A coin the blue P-switch made a
    block is 0x02B but acts as the used block 0x132; read raw, it was a hittable block, which SMW turned into a
    P-switch block and then a yellow block for good. It now passes `ActsLike(...)` (as SMW's own side check does);
    `PunchBlocks` likewise.
  - Solid sprites are walls: `SolidSpriteBlock` (flying ? blocks before and after the hit, Reznor's platforms, the
    line-guided / floating / castle-block platforms, message boxes) pushes the player out sideways without a blocked
    flag. `McSmw_NoteSolidSprite` (smw_01.c) records each call's box and the x SMW parks Mario at on either side
    (`McSmw_SolidSpriteBox`), and `sub_1B505` reports the push (`McSmw_SolidSpritePush`): `Resolve` sees a wall
    (`SolidSpriteWall`, SMW's own side band: body boxes overlap, not on top, not under), so MC clings and wall-jumps.
    Sprites solid on top only ($190F bit 0) are no walls. `ClampIntoFloor` also stops a fast fall inside a solid
    sprite's top band (about 5 px, which MC's 8 px-a-frame fall skipped); the brown chained platform's own routine
    records its top too (`McSmw_TopPlatformBox`).
  - Sideways knockbacks from SMW sprites (Reznor's rebound off his side, the Koopa Kids' bounce, a Chargin' Chuck's
    stomp knockback, a spiky enemy's): `McSmw_SideLaunch` after each such `player_xspeed` write; MC takes the speed on
    its next frame (MC sets its speed every frame and used to walk on into Reznor).
  - Boss punches: `AttackSprites` compared the sprite's layer ($1632) with Mario's even where SMW's contact check
    doesn't ($0D9B bit 6: the mode-7 boss rooms), so every punch on Morton, Roy and Ludwig missed. The Koopa Kids'
    touch timer $1FE2 (set whenever they touch Mario) no longer blocks the punch; a bounced punch waits like a hit.
    `bosspunches` (pilot) counts punches that hurt a boss; `boss_punch_test.py --ground` (punches from the ground only):
    Morton, Roy and Ludwig beaten with 3 punches each.
  - Bowser: two punches per Mechakoopa hit (`g_bowser_half`: the first gives the kick sound and SMW's contact star at
    the fist; Mechakoopas count as in SMW).
  - Boss rush: EVERY BOSS fights Reznor once (Vanilla Fortress): 10 fights. The music: a boss win leaves
    `misc_music_register_backup` ($0DDA) at 0xFF, and the next level load starts its music only under 0x40 (the map
    clears it, which a boss rush never visits): every start now clears it (`Launch`).
  - New cheat PUNCH ANYTHING (MC) (`kCheat_PunchAll`, ini `CheatPunchAll`, ENEMY CHEATS): Thwomp, Thwimp, Big Boo,
    Podoboo, Boos, Eeries, Urchins, Torpedo Ted, Grinders, line Fuzzy, Wiggler, spike ball, Sparky / Fuzzy, Hothead,
    Blargg, Fishin' Boo, Boo block, falling spike are knocked out by a punch (`PunchAllTarget`), or with MC HIT
    REACTIONS take MC hits. Bosses, items, platforms and stage hazards are not in it.
  - Pilot commands: `ontop <slot> [dx] [dy]` (Mario onto a platform sprite), `bosspunches`.
  - Tests: `solid_sprite_wall_test.py` (message-box wall and flying ? blocks, fresh and used: cling and wall jump),
    `platform_ride_test.py` (18 platform sprites, MC vs SMW Mario: ride, drops, after jumps), `punch_all_test.py`
    (18 enemies x cheat off / on / with MC hits), `bossrush_test.py` (10 rooms, `real_fights` checks each fight's music
    at $1DFF), `boss_punch_test.py --ground`; pilots `pswitch_coins_r18`, `solid_wall_r18`, `reznor_platform_r18`,
    `menu_r18`, `menu_rush_r18`.
- Round 19 (chat request; results `notes/verification.md`):
  - PUNCH ANYTHING also covers Pokey (`PunchPokey`: the segment at the fist's height flies off, as SMW's own hit
    does), Fishbone, the gas bubble and 22 more ordinary enemies a punch couldn't hurt (76 audited:
    `punch_all_test.py`). Any punch pops a bubble (9D).
  - Moving platforms and blocks: 35 kinds in real levels and 21 spawned, MC vs SMW Mario (`platform_levels_test.py`,
    `platform_ride_test.py`, `platform_contact_test.py`); the per-kind table and rules are in `notes/platforms.md`.
    The one real bug: jumping off a rope, vine, net or Yoshi went far too high (Yoshi about 130 px vs SMW's 68).
    The hand-back from SMW-driven states now converts the rise by height (`grav = -yspeed * 0.177` held, `0.125`
    not) and a released jump is cut once (`g_held_rise`): rope 41 / 49 / 64 px vs SMW 41 / 51 / 66, Yoshi 75 vs 68.
  - Videos in `finished\` (unedited; the pilot's `record` / `audio` dump every frame the game draws, no game code
    involved; `make_video.py` scales 4x and mixes the two WAVs): `MarioCombat_boss_rush_speedrun.mp4` (EVERY BOSS,
    7:10.07 on the rush clock, 0 hits), `MarioCombat_final_level_speedrun.mp4` (Back Door, 3:44, 0 hits),
    `MarioCombat_21_hit_combo.mp4` (King Koopas + Instant hits, 21 jabs). No cheats and no deaths in the two
    speedruns. The runs were found with `tas.py` sessions (`work\tas\`), fight policies with
    backtracking (`tas_backtrack.py`, `tas_policy.py`) and `tas_combo2.py` (one jab on the first frame after MC
    Bowser's hurt clip); `tas_hits.py` counts the hits in a kept run.
  - Menu notices ("SETTINGS SAVED") count frames under a pilot (`NoticeClock`), so recordings and screenshots show
    them for the same frames every run (real time otherwise, as before).
  - Pilot: `record <name>` / `record off` (raw frames), `ontopid`, `spritefind`; status.json gains `rush`,
    `hero_frame`, `hurt_timer`, `facing_right`, `floor`, `reznor_broken`, `boss_punches`, sprite slots / states /
    speeds and `ext_near`; the log gets an `onspr` column.

- Round 20 (chat request; design `notes/mc_level.md`, results `notes/verification.md`):
  - LEVEL SELECT > MARIO COMBAT (the page's first row): MC's story level, five areas (root frames 486-496), as one SMW
    level for both Marios (`src/mc/mc_level.c`). Tiles from MC's ground shapes (`mc_level_render.py` ->
    `mc_level_export.py` -> `src/mc/mc_level_data.c`, generated: don't hand-edit) over vanilla room 007's castle
    tileset (the collision; not shown); SMW door / pipe exits and MC's arrow, taken with Jump, Punch or Up; area 4's
    lift (MC's, 378 frames; a platform for both Marios; it comes back down if Mario falls off); MC Koopas and MC Bowser
    forced on. A death restarts the area (a life less), Bowser's fall ends it ("MARIO COMBAT CLEAR!", back to the map,
    a practice run as the boss rush). SMW Mario gets the wall jump there, and area 2 two helper blocks.
  - It looks like MC, aligned with the collision: SMW's layers 1 and 2 are off in the level and `McLevel_ArtLine`
    (called from `ppu.c` after a line is composed) draws where the line shows the backdrop: the ground from the tiles
    at runtime in MC's style (MC's weave and 8 px bevel frame, rounded outer / turned inner corners, MC's brown block),
    over a picture of MC's sky, props snapped onto the tile ground (statues, pillars, chains, castle, door) and MC's
    lava / spikes / pipe / bridge drawn on their own tiles, over MC's screen-locked castle wall. Data: `mc_level_art.py`
    -> `src/mc/mc_level_art.c` (generated). Area 5's bridge tiles are 130 (solid; drawn as MC's bridge). MC's brown
    blocks are its pixel art with seams, outlined round each group.
  - Follow-up fixes: MC's lava kills (SMW's tiles 0-5 are water: the level has air there, `InLava` kills) and is animated
    (MC's sprite 141 frames); area 3 starts 2 columns earlier (MC's first pillar), its pillars stand whole behind the
    slab, its two chain-hung slabs are one-way ledges (map16 100) drawn as MC's; area 5's right pillar is flush with the
    bridge; Bowser is never erased off screen in the level. Both runs re-searched from area 3 (0 hits).
  - MC's Bowser music in area 5 (MC's sound 560, looped), instead of SMW's castle music: `McAudio_StartMusic` /
    `StopMusic` / `MixMusic` in `mc_audio.c` (4-bit IMA ADPCM decoded as it plays, -12 dB, the game's volume), data:
    `python -I tools\scripts\mc_music_export.py work\ffdec\sounds games\SuperMarioWorld\src\mc\mc_music.c`.
  - MC Mario's damage per SMW hit is MC's own 0.5 (20 hits from full). The ini key is renamed `HitDamage`;
    `DamagePerHit` (the old 2.5) is read and ignored, so a saved `smw.user.ini` doesn't keep the old value.
  - Hooks: `smw_00.c` (after `LoadLevel`, before the palette upload), `smw_05.c` (after the room's objects; the sprite
    list), `smw_01.c` (`ProcessNormalSprites` start: the lift, after SMW clears the on-sprite flag), `levelselect.c`,
    `menu.c`, `mc_koopa.c` / `mc_bowser.c` (forced on), `cheats.c` (wall jump), `mc_smw.c` (the lift's landing box).
  - Pilot: `mclevel [area 1-5]`, `mclevelinfo`, `mctiles <first> [n]` (a map16 catalogue on screen), `map16grid`,
    `ppuw <reg> <val>` (e.g. `212C` to show one layer); status.json gains `mcl`, `blocked`, `on_solid_sprite`, sprite
    speeds. Runs: `tas_backtrack.py <session> <checkpoint> mc_runner --mcl` per area, `mclevel_chain.ps1`.
  - Beaten by both Marios with 0 hits (MC Mario 10,650 frames, SMW Mario 9,915 after the follow-ups; sessions `mcl` / `mcls`,
    `notes/verification/r20_runs.txt`). Fixed on the way: the lift didn't carry SMW Mario with ground friction (he slid
    off it), MC's ceiling spikes were written as turn blocks (yellow blocks at the top of area 3; now the castle's own
    spikes 159 / 15A, which hurt).
  - A kept TAS run replayed by a plain pilot script stays in step only if every `shot` takes the place of a `wait 1`
    (a shot runs a frame of its own: shots added on top made MC Mario's replay die in area 1).
  - Follow-up 4: the camera follows MC Mario's wall jumps (his wall cling sets SMW's `camera_bounce_off_spring_flag`,
    as SMW Mario's wall jump does; cleared on landing). MC Bowser's feet no longer sink while he pants (the overlay's
    near-unit blit keeps the scaled image's bottom edge and centre). SMW Mario hurts MC Bowser only with a spin jump or
    Yoshi; a plain jump bounces off. MC Mario has no stomp on him (as in MC; he used to get a strong hit). SMW Mario's cape hits MC Koopas and Bowser at most every 32 frames
    (`McKoopa_CapeHit` / `McBowser_CapeHit` set `$154C`; MC Mario's punches unchanged). MC's area ends: invisible
    solid columns at the hero's MC limits and the camera clamped to MC's lScroll / rScroll (`McLevel_ClampCamera`,
    hooked in `smw_00.c`; limits from `mc_level_export.py`). MC's lava glow drawn by the game (half alpha at the surface,
    13 px). The exe has an icon (`tools/scripts/make_icon.py` -> `src/platform/win32/smw.ico`, `smw_icon.rc`,
    windres in the Makefile): MC Mario's idle fighting stance (MC shape 166), whole-number nearest-neighbour scales,
    his upper body at 16-24 and 40-48 px (a first logo-style icon was rejected by the player as unreadable).
    Runs re-searched (session checkpoints now a1-a5 / won at the real area starts): MC Mario 3 hits (all against
    Bowser), BOWSER DOWN! at frame 12,745; SMW Mario 0 hits, frame 9,598 (`notes/verification/r20_runs.txt`).

**Known limitations**
- Not verified: 2-player Luigi, fullscreen/window size/F11 on a real display (pilot windows are hidden); rumble and the
  stick deadzone on a real gamepad (checked through the `rumble` lines and the code path only); IN BACKGROUND with real
  window focus changes (checked with the pilot's `focus` command).
- The title-screen demo shows the original Mario (MC is only active in game mode 0x14).
- Round 15: MC Mario "fake entering" and breaking a pipe with an air dash was not reproduced (dashes into a side-pipe
  exit, ledge walls and corners at every jump height); it got the in-air-flag fix above. Page-1 map16 numbers depend
  on the tileset (133/134 are a dirt ledge in tileset 0), so pilot `findtiles` does not find pipes by number.

**Kept in `work\` (after the round-16 cleanup, which removed ~10 GB of regenerable output, and the round-20 one, ~17 GB:
the TAS sessions' per-step screenshots, raw recording dumps and superseded recordings, clone sessions, all of `pilot\`)**
- Inputs: `swf\` and `ffdec\` (the exporters' sources), `refcap\` (reference traces), `mctest\` (trace harness),
  `saves_backup_r3\` (the dev save restored after tests), `ghidra\` (the projector RE project), `*_export\` (exporter
  previews), `play\reports`, `play\wave2`, `play\all_issues*.txt` (round-8 findings).
- TAS runs: `tas\<session>\session.json` (the moves; `mcl` / `mcls` the MC level, `rush3b` / `final2` / `combo` round 19),
  replayed by `tas.py`; `mc_level\` / `mc_art\` the MC level exporters' renders; `video\mc_art3` the latest MC level
  recording (mp4).
- Regenerated on demand: `pilot\` (every test and pilot output), `package\` and `finished_test\` (package.ps1),
  `play\game_*` / `play\sessions` (play.py). Installer archives are not kept (`tools\` holds the unpacked tools).

## How to
| task | command |
|---|---|
| Tools on PATH | `. .\tools\env.ps1` |
| Build | `powershell -File tools\build_smw.ps1`, gives `games\SuperMarioWorld\smw.exe` and `work\mctest\mc_test.exe` |
| Re-export MC data (after changing exporters) | `python -I tools\scripts\mc_export.py work\swf\MarioCombat.swf work\ffdec\images games\SuperMarioWorld\src\mc\mc_data.c work\mc_export` |
| | `python -I tools\scripts\mc_sound_export.py work\ffdec\sounds work\mc_export\export_report.json,work\koopa_export\koopa_report.json,work\bowser_export\bowser_report.json games\SuperMarioWorld\src\mc\mc_sounds.c` |
| | `python -I tools\scripts\mc_hud_export.py work\ffdec\hud454_z05\DefineSprite_454\1.png games\SuperMarioWorld\src\mc\mc_hud_data.c work\hud_preview.png` |
| Core check (each must print `RESULT PASS`) | `work\mctest\mc_test.exe work\mctest\ground486.bin work\refcap\runs\<run>\trace.csv -1540.7 2525.7 411.1 -1129.35 286.5 --oracle` |
| Pilots (hidden window, silent) | `powershell -File tools\scripts\run_pilot.ps1 <name>` runs `tools\pilots\<name>.txt` and makes `work\pilot\<name>_sheet.png`. Snapshot 8 = Yoshi's House, 9 = YI1 (in the level; `games\SuperMarioWorld\saves`). |
| Pilot commands | `hold/wait/shot/log/loadstate/quit`. Debug: `glide x y`, `setpos`, `warp <lvl>` (3 digits = with bank, e.g. `00A`, `1F6`), `yoshi`, `power n`, `poke addr val`, `kill`, `toggle`, `menu`/`menukey`/`menupad`, `waitnear dx [id]`, `hitboxes 1`, `sprites`, `exits`, `findtiles lo hi` (prints each vertical run's height), `shotre <name>` (the frame drawn again without running the game), `scroll`, `triplelog`, `tileat x y`, `audio <name>` / `audio off` (WAVs of the SNES mix and of MC's sounds), `cheat <n> <0/1>` / `cheat powerup <0-3>` / `cheat knockback <1-20>`, `hurt [heavy] [dir]` (an enemy hit on Mario), `setting <name> <v>`, `focus 0/1`, `glideby dx dy`, `levelselect <lvl>`, `whereami`, `progress`, `takeexit`, `levelscan`, `levellist`, `attack <id> <reach> [air] [delay]` (walk to a sprite and punch / air-kick it), `owgoto <tile>`, `palettes`, `follow <min>` (wait for appended lines), `status` (status.json), `ack <n>`, `levelnames`, `peek <addr> [n]`, `findtiles0 lo hi` (page-0 tiles), `ontop <slot> [dx] [dy]`, `bosspunches` (list in `src/pilot.c`) |
| Agent play-testing | `python -I tools\scripts\play.py setup` (copies the build to `work\play\game_<time>`), then `start/do/checkpoint/rewind/stop` as in `tools\play_guide.md`. `python -I tools\scripts\replay_session.py <session> <checkpoint> --then "<moves>" --every 6` replays a session on the main build (a rebuilt game may diverge from the agent's). (The round-8 sessions and game copies were removed in the round-16 cleanup; their reports and `all_issues*.txt` remain.) |
| Cheat sweep | `python -I tools\scripts\cheat_sweep.py <root>` (910 runs for 41 cheats and the knockback scale, 4 in parallel, summary in `work\pilot\cheat_sweep\summary.csv`) |
| Regression pilots | `powershell -File tools\scripts\regress.ps1` (100 pilots, one summary line each; look at the `_sheet.png`s) |
| Triple-enemies audit | `python -I tools\scripts\triple_audit.py <root> [000 1FF] [--jobs 6]` (summary in `work\pilot\triple_audit\summary.txt`) |
| MC enemy data | `python -I tools\scripts\mc_enemy_export.py work\swf\MarioCombat.swf work\ffdec\images games\SuperMarioWorld\src\mc work\koopa_export --prefix Koopa --roots 163` and `... work\bowser_export --prefix Bowser --roots "532,555,558@0.3614654541"` (then `mc_sound_export.py` with `export_report.json,koopa_report.json,bowser_report.json`) |
| Sprite interaction sweep | `python -I tools\scripts\sprite_sweep.py <root> 00 31` (likewise 32 63, 64 95, 96 C8 in parallel), then `python -I tools\scripts\sweep_report.py work\pilot\sprite_sweep\results_??_??.json`. Add `reanalyse` to re-read existing CSVs |
| Original-vs-C check | Put a temporary `smw.user.ini` with `[MarioCombat] Enable = 0`, then run `smw.exe "Rom\Super Mario World.sfc" --pilot ...` from boot (snapshots made in C-only mode have no CPU state). Any divergence prints `Memory compare failed` with the RAM diffs. Delete the ini afterwards. |
| Package + smoke test | `powershell -File tools\package.ps1` (rebuilds `finished\` and tests a copy in `work\finished_test`) |
| MC level (round 20) | Re-export after changing the exporters: `python -I tools\scripts\mc_level_render.py work\swf\MarioCombat.swf work\ffdec\images <frame> work\mc_level\area_<frame>.png` for frames 486 488 490 492 496, then `python -I tools\scripts\mc_level_export.py work\swf\MarioCombat.swf work\ffdec\images work\mc_level games\SuperMarioWorld\src\mc\mc_level_data.c work\mc_level\preview` (look at `preview\area<n>_tiles.png`), then the pictures: `python -I tools\scripts\mc_level_art.py work\swf\MarioCombat.swf work\ffdec\images games\SuperMarioWorld\src\mc\mc_level_data.c.json games\SuperMarioWorld\src\mc\mc_level_art.c work\mc_art` (look at `work\mc_art\area<n>_art.png`). Play an area: `tools\scripts\mclevel_chain.ps1 <session> <first area> <seed> <checkpoint>` (sessions `mcl` MC Mario, `mcls` SMW Mario; base scripts `tools\tas\mclevel*_base.txt`) |
| Windows icon (round 20) | `python -I tools\scripts\make_icon.py work\mc_export\shape_166.png games\SuperMarioWorld\src\platform\win32\smw.ico work\mc_art\icon_preview.png` (look at the preview), then build (windres links `smw_icon.rc`) |
| Videos (round 19) | Runs are `tas.py` sessions in `work\tas\` (`rush3b` boss rush, `final2` final level, `combo` combo; base scripts in `tools\tas\`). Record: `python -I tools\scripts\tas.py record <session> work\video\<x> <name>`, then `python -I tools\scripts\make_video.py work\video\<x> <name> 224 finished\<file>.mp4`; check with `video_sheet.py <mp4> <png>` and `tas_hits.py <session> <checkpoint>`. Fights: `tas_backtrack.py <session> <checkpoint> <policy> [--bowser] [--patience N] [--stall N]`; the combo: `tas_combo2.py combo approach --reach 18 --stopsub 6 --target 21`. A rebuilt game can change a session's outcome: record before changing game code |

Pilot runs never write `smw.user.ini` (round 5). A hand-made one (e.g. for the original-vs-C check) must be deleted
afterwards, or the dev build keeps those settings (`package.ps1` never copies it).

## Next steps
1. Optional: more play-testing waves (`play.py setup`, then agents with `tools/play_guide.md`); the unbeaten levels
   and where each run stopped are in `work/play/all_issues_wave2.txt`. P-switch doors (tiles 27/28) were only
   checked against SMW's code: no level's main room has one for a `warp` test.
2. Optional: King Koopas in water (his script ignores water, as MC's arena has none) and on layer-2 platforms.
3. Try fullscreen, window size and F3/F11 on a real display.
