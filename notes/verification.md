# Verification log

## Mario Combat core (`src/mc/mc_core.c`) vs the original game
The reference traces were recorded from the real `MarioCombat.exe` runtime (Adobe Flash Player 32 projector stub). The
SWF was patched with a scripted-input driver (`tools/refcap/`). The comparison covers these fields per tick:
- x, y;
- xSpeed, grav;
- hero frame, facing;
- attack-clip frame, hurt-clip frame;
- onGround, walling.

Collision in the harness uses "oracle" mode. Probe results and push-outs come from the reference, because Flash's
shape `hitTest` rounds points through the vcam camera transform (resting height varies 266.5–268.5 above a 268.05
surface). Inside SMW, collision is SMW's own anyway.

| run | what it exercises | result |
|---|---|---|
| basic_moves | walk, run, slide, jump, short hop, punch 1-2, dash (attack7), down attack (attack11), air attack (attack12), skid, enemy hits | 540/540 PASS |
| combos | punch timing sweep 3–30 ticks, directional chains; attack1-9, 105, 11 | 2688/2688 PASS |
| combos2 | attack4 → attack10 → 102 → 103 → 104 → 105 chain | 400/400 PASS |
| air | jump hold 1–40 ticks, short hops, air punch, air control | 940/940 PASS |
| moves | taps of 2–30 ticks both directions, skids, stopSliding | 660/660 PASS |
| wall | stair hops, pillar wall slide, wall jump (tick 203: xSpeed −10, grav 10), dead-zone respawn | 340/340 PASS |

| jumpposes | jumps from idle / walk / run / after a skid (with screenshots) | 591/591 PASS |

**Total: 6,159 / 6,159 ticks identical** (re-run after the final core changes). All 16 attack-clip moves have been triggered and verified (H).

## Port inside SMW vs the original (pilot logs vs reference traces)
The comparisons convert MC stage px to SMW px (÷3) and ticks to milliseconds.
- Script: `tools/scripts/compare_jump.py`, `tools/scripts/compare_run.py`.
- Pilots: `tools/pilots/*.txt` (run with `smw.exe --pilot <script> --out <dir>`).

| measurement | original | port | after a 1-frame alignment* |
|---|---|---|---|
| full jump apex | 62.83 px | 62.50 px (-0.5%) | max error 0.40 px over the arc |
| full jump airtime | 1120 ms | 1150 ms | — |
| short hop (1-tick tap) apex | 23.83 px | 23.56 px | max error 0.66 px |
| run-up from standstill (first 240 ms) | — | — | max error 0.06 px |

\* The port moves one SMW frame after its physics step (SMW applies speeds at the start of the next frame, the same
as for SMW's own Mario). The best-fit shift is +18 to +30 ms.

Plots: `notes/verification/jump_arc_full.png`, `jump_arc_short_hop.png` (red = original, blue = port).

Visual checks:
- **Every pose matches.** `notes/verification/side_by_side_moves.png` pairs real Mario Combat screenshots with port
  screenshots of the same moves: idle, walk, run, slide, skid, jump, punch 1, punch 2, dash, down attack, air attack.
  The art and the size per art pixel are identical (1 art px = 3 MC stage px = 1 SNES px).
- `mc_original_jump_poses.png`: the original shows the raised-fist jump art in every jump, as the port does.
  - One earlier reference screenshot looked like the idle pose mid-air. It was a capture-timing outlier: the trace
    says frame 9 (jump) at that tick.
- `port_punch_koopa.png`: punches knock enemies out through SMW's cape-hit reaction, with MC's hit spark and points.
- `port_life_bar_and_death.png`: life-bar HUD, hurt knockback and blink, then death at 0 life with MC's `die1`
  animation through SMW's death motion.
- `packaged_smoke_test.png`: the delivered `finished\` build, run from a copy with only the user's ROM added and a
  fresh save. It boots, plays the intro, map and YI1, then moves, punches and jumps.

## Round 2 (fixes, settings menu, special states)
Every row is a pilot in `tools/pilots/`, run with `tools/scripts/run_pilot.ps1 <name>`. That gives
`work/pilot/<name>/` (screenshots + per-frame CSV) and `work/pilot/<name>_sheet.png`. The screenshots below were
copied from those sheets. The core traces were re-run after the last `src/mc` change: all 7 PASS (6,159/6,159
ticks).

Levels other than YI1/Yoshi's House are reached with the pilot `warp <level>` command. It takes the current screen's
exit the way a pipe does (SMW's `IncrementSublevelsEnteredAndPrepareToLoadSublevel`).

| check | pilot | result | screenshot |
|---|---|---|---|
| Cement-block U + gap by the diagonal pipe (video 0:36–end) | `blockwall_yi1` | 949 frames, never crushed, life 10. Wall-sticks and wall-jumps off both columns; no clipping in the gap | `r2_block_walls_u_and_gap.png` |
| Run/jump into a block column | `blockwall2_yi1` | stops flush (body edge 1903, wall 1904); slides; wall-jumps | `r2_block_wall_slide.png` |
| Long falls | (probe) | used to fall through ground/ledges after a long drop; now lands (`ClampIntoFloor`) | — |
| Message block (Yoshi's House) | `msgblock_yh` | 1 bump and 1 message per jump (opens at frames 31 and 197 for 2 jumps). Before: re-hit every few frames | `r2_message_block_single_hit.png` |
| Punch Banzai Bill | `rex_<dx>` runs | knocked out (stomp reaction) when its nose is ≤ 44 px from Mario's x; misses at 52+. Before: never hit (cape-immune skip) | `r2_punch_banzai_bill.png` |
| Punch Rex | `rexb_<dx>` runs | knocked out at dx ≤ 40–44 (fist ≈ 12 px from the body); clear whiffs at 48+ still miss | `r2_punch_rex.png` |
| Switch Mario | `toggle_yi1` | MC → SMW (life bar hidden, SMW sprite/physics) → MC (bar back) → SMW. Then die → map → re-enter: starts as MC | `r2_switch_mario.png` |
| Settings menu | `menu_pages` | every page, option change, key rebind, gamepad page, reset confirm, "SETTINGS SAVED" toast | `r2_settings_menu_pages.png` |
| Settings persistence | (persist1/2) | Show FPS set in run 1 → `smw.user.ini` written → still on in run 2 (dev ini deleted after) | `r2_settings_saved_and_reloaded.png` |
| Yoshi (YI2) | `yoshi_yi2` | sits in the MC jump pose raised 4 px onto the saddle; faces where Yoshi faces; A dismounts (fixed: Mario used to drop straight back on) | `r2_state_yoshi.png`, `r2_state_yoshi_closeup.png` |
| Vertical pipe (YI3 → cave) | `pipe_yi3` | sinks behind the pipe (overlay z), sublevel loads, drops out of the ceiling pipe, plays on | `r2_state_pipe.png` |
| Swimming (level 00A) | `swim_00a` | SMW swim physics (strokes, sinking); MC jump pose while floating, walk/idle on the floor | `r2_state_swim.png` |
| Climbing net (Iggy's Castle) | `climb_iggy` | grabs, climbs, SMW fence punch knocks a net Koopa off, jumps off, lands | `r2_state_climb_net.png` |
| Cape | `cape_yi1` | feather heals; X = SMW cape spin; **no flight** (MC never reaches P-speed: documented limitation) | `r2_state_cape.png` |
| Goal tape (YI1) | `goal_yi1` | tape, walk-off, COURSE CLEAR tally, back to the map | `r2_state_goal.png` |
| Castle boss (Iggy, level 1F6) | `boss_iggy` | jumps/lands on the tilting platform, takes hits; no crash. Fixed: jumping used to snap Mario off-screen and kill him (in-air flag) | `r2_state_castle_boss.png` |

Not covered: horizontal pipes (no enterable one in the Yoshi's Island levels used), vines (nets were tested; vines use
the same climbing state), and the fullscreen/window-size options on a real display (the pilot window is hidden).
Horizontal pipes are covered in round 3.

## Round 3 (SMW mechanics, colours, sound, hotkeys, cheats, 100% save; brief `PROJECT_BRIEF.md`, round 3)
The core traces were re-run after all round-3 changes: 7/7 PASS. `tools\package.ps1`: smoke test exit 0.

**YI1 bonus-room pipe → glitch area (H).** The sideways pipe in sublevel 1CB led to level 100, a cave with YI1's
objects in it.
- Found with the in-process original-vs-C comparison (snesrev's `RM_BOTH`: pass the ROM with MC disabled). At the
  first diverging frame the ROM code had loaded level 105 with the camera at x 0x78C; the C port had loaded a
  1-screen level. `$1B93` (secondary entrance) was handled differently.
- Cause: `assets/compile_resources.py` only exported the secondary-entrance tables (`kLevelInfo_05F800/FA00/FC00`,
  `kLm5FE00`) for Lunar Magic ROMs. For the vanilla ROM they were empty, so every secondary exit read 0 and went to
  level 100.
  - Upstream snesrev/smw bug, not an MC change. It affects every sideways pipe / secondary-entrance door in the game.
- Fix: export the vanilla tables at `$05:F800/FA00/FC00/FE00`.
- After the fix, the same run (boot → Yoshi's House → map → YI1 → bonus room → pipe → YI1) has **zero** mismatches
  against the ROM code, so it behaves exactly as the original game, as snes9x runs it. The packaged build (assets
  rebuilt from the ROM through `smw_assets.bps`) also exits to YI1 (20 screens).

| check | pilot | result | screenshot |
|---|---|---|---|
| Bonus-room side pipe, SMW Mario | `hpipe_1cb_smw` | enters, shoots out of the pipe in YI1 past the cement U (state 7, 20 screens) | `r3_side_pipe_smw_mario_fixed.png`, `r3_side_pipe_destination.png` |
| Side pipe, MC Mario | `hpipe_1cb_mc` | walks in (drawn behind the pipe) and comes out in YI1. `ClampIntoWalls` now lets him rest 1 px inside a wall as SMW Mario does, so SMW's side checks see the pipe; the wall regressions still pass | `r3_side_pipe_mc_mario.png` |
| Run / P-speed | `run_yi1` | walk ≈ 2.7 px/frame; X or Y held ≈ 3.1; P-meter full after ~1 s → ≈ 3.6 | `r3_run_pspeed.png` |
| Cape take-off + flight | `cape_flight_yi2` | P-speed jump → SMW's in-air mode 12, flight phases 3–6 with X held, dive, landing, MC control again | `r3_cape_takeoff_flight.png` |
| Carry with the punch button | `carry_yi1` | Y held → walks into the shell → carried (status 11), turns with it, release → kicked | `r3_carry_shell.png` |
| Punch a ?-block (level 003) | `qblk_*` | one block per hit → used block, the item comes out (SMW's `CheckIfBlockWasHit`, as the cape) | `r3_punch_question_block.png` |
| Kick a turn block (YI1) | `pblk_*` | shatters (SMW's spin-jump break, map16 0x11E → 0x025) | `r3_punch_turn_block.png` |
| Power-up colours | `powerup_colors` | normal / fire (white+red) / cape / star cycle, MC shading kept | `r3_powerup_colours.png` |
| MC sound level | `audio_levels` | SMW kick peak 10,500 / RMS 3,400; MC hit was clipped at 32,767 / RMS ~20,000 → now ~12,000 / ~4,600 (-14 dB) | — |
| HP-bar label | (export) | opaque white MC text with a dark-red 1 px outline; options compared on light, blue and dark backgrounds | `r3_hp_bar_label.png`, `r3_hp_bar_label_options.png` |
| Menu: CHEATS, 100% option, F3/F11 rows | `menu_r3` | pages render in the SMW font; settings saved | `r3_menu_cheats_gameplay_keys.png`, `r3_cheats_page.png` |
| Cheats in game | `cheats_yi1` | lives 99, timer frozen, life 10 after walking into Rex/Banzai, moon jump to the top, fire lock | `r3_cheats_in_game.png` |
| 100% save in file 3 | `save100` (+ off run) | file select shows "MARIO C ... ★96"; the map has every path open and the post-Special-World palette. Switching off puts the original `smw.srm` back byte-for-byte and removes the backup | `r3_save100_file3.png` |

Not covered: F3/F11 on a real window (pilots are hidden; the command code is shared with the tested menu options).
The cape graphic is not drawn on MC Mario (MC has no cape art); round 4 adds it.

## Round 4 (brief `PROJECT_BRIEF.md`, round 4)
Core traces: 7/7 PASS.
- Original-vs-C comparison from boot through the bonus-room pipe (MC off): 0 mismatches after the round-4 changes to
  shared code (SetPlayerPose split, sprite scan in `ppu.c`, overworld entry check).
- All 21 regression pilots of rounds 2–4 behave as before. `tools\package.ps1`: smoke exit 0.

| check | how | result | screenshot |
|---|---|---|---|
| Fire outline colour | `powerup_colors` + pixel count | MC outline (#070000) used to become #004a4a through slot 2 (grey in the fire palettes). Near-black now maps to SMW's black in every recoloured palette; 0 teal pixels in all states | `r4_powerup_colours_black_outline.png` |
| "White grass top" in Yoshi's House | snes9x 1.63 (user's copy, same ROM) lossless F12 screenshot vs the port vs the ROM code in smw.exe | **identical**: row y 192 is #eff7ff in all three. It is SMW's own ground highlight, not a port bug. Whole-frame diff vs snes9x: only animated sprites (bird, smoke, flame timing) | `r4_yh_grass_vs_snes9x.png` |
| MC Mario behind the Yoshi's House fire | `fire.txt` (MC vs toggled SMW Mario at the fireplace) | Cause: the overlay sorted the hero by priority bits, but SNES sprite-vs-sprite order is by OAM index. MC Mario is now put into the PPU sprite line at Mario's own OAM slot (64 + `kPlayerGFXRt_PlayerStartingOAMIndex[layer]/4`): in front of the fire like SMW Mario; still behind pipes; Yoshi and the carried shell ordered as SMW does | `r4_fireplace_mc_vs_smw.png`, `r4_sprite_order_yoshi_shell.png` |
| Multi-hit enemies | `multihit`, `strong_hit` (`attack` pilot command) | Rex: jab → small Rex (`$C2` = 1, 200 pts), jab → knocked out. Clappin' Chuck: 3 jabs (hits during its hurt animation don't count) → knocked out (800 pts). Air spin kick (level 6) knocks out a full Rex at once. Strong = MC hit level ≥ 4 (finisher 4, low sweep 5, air kick 6; jab 1, straight 2, hook 3, dash 2) | `r4_multi_hit_rex_chuck_strong.png` |
| Replay castles cheat | `replay_castle` (100% file, `owgoto 81`) | B on Iggy's destroyed castle: nothing with the cheat off (as SMW), castle entrance and level with it on (SMW's own L+R re-entry path) | `r4_replay_castle_cheat.png` |
| SMW cape on MC Mario | `cape_look` | stand, walk, P-speed run, take-off, flight (yellow flight cape), jump, fall, punch, landing; flips with facing, behind MC Mario | `r4_cape_states.png`, `r4_cape_run_punch_fly.png` |
| Cheat combinations | `tools/scripts/cheat_sweep.py`: none, 9 singles, 36 pairs, all, 3 power-up locks, all + feather/fire (52 runs) | every run plays the same YI1 route (run, P-jumps, punches, a moon-jump hold, goal tape, tally) and reaches the map; 0 crashes, 0 soft-locks (Infinite time does not block the tally) | `r4_cheat_sweep_summary.csv` |
| Infinite lives + death | `cheat_lives_death` | map showed ×98 after a death (the counter is drawn at map load). Now ×99 on the map too | `r4_cheat_lives_death.png` |
| Menu | `menu_r3` | REPLAY CASTLES row; OFF values in grey so active options stand out | `r4_menu_cheats_off_grey.png` |

## Round 5 (brief `PROJECT_BRIEF.md`, round 5)
Core traces: 7/7 PASS on the final build.
- Original-vs-C comparison from boot through the bonus-room pipe (MC off): 0 mismatches (`r5_original_vs_c_0_mismatches.png`).
- `tools\scripts\regress.ps1`: all 32 regression pilots (rounds 2–5) behave as before.
- `tools\package.ps1`: smoke exit 0 (`r5_packaged_smoke_test.png`).

### Interaction audit
- `tools/scripts/sprite_sweep.py <root> lo hi`: every sprite 00–C8 vs MC Mario and SMW Mario in Yoshi's House: touch, stomp,
  punch. Run in 4 parallel chunks. `sweep_report.py` prints the table.
- First run: 57/201 differences. Main cause: the stomp-bounce bug, plus test artefacts (invulnerability carried over between
  tests, a too-strict bounce detector).
- Final: 18/201, all explained (H). Report: `r5_sprite_sweep_report.txt`; first run: `r5_sprite_sweep_report_first_run.txt`.
  - 29 / 36 crash and 33 hangs for both Marios (outside their rooms).
  - Moving / falling sprites gone before the slower-falling MC Mario lands.
  - SMW Mario still in his power-down from test 1.

| check | pilot | result | screenshot |
|---|---|---|---|
| Stomp bounce | sweep t2 | SMW's −48 bounce used to be overwritten by MC's motion next frame (MC fell back on the enemy and got hurt by para-Koopas, Chucks...). Upward speed set by SMW since the last frame now becomes MC `grav` | `r5_sprite_sweep_report.txt` |
| Springboard | `objects_r5` | launch carried into MC; peak 269 vs SMW 290 with B held (MC ~20 px higher, left as is) | `r5_spring_balloon_cloud.png` |
| Note block (map16 115, poked) | `noteblock` | bounce −96 → MC peak y 254 vs SMW 255 after the height-matched conversion `g0 = v·sqrt(3/32a)` (a = 3 held, 6 not) | — |
| P-balloon | `objects_r5` | SMW moves Mario (MC puppet); SMW's inflated pose now drawn (`McSmw_UseSmwSprite`) | `r5_spring_balloon_cloud.png` |
| Lakitu's cloud | `objects_r5` | rides, steers, jumps off | `r5_spring_balloon_cloud.png` |
| Munchers from the side | `tiles_mc` (+ SMW reference) | MC used to stop 1 px short of SMW's side check, so no hurt. `TouchWallTile` runs `RunPlayerBlockCode_CheckIfPlayerTouchingHurtBlock`. Now life 10 → 7.5 at x 210, as SMW Mario's power-down | `r5_tiles_coin_midway_muncher_mc_vs_smw.png` |
| Coin, midway gate | `tiles_mc` | collected / passed like SMW Mario | same |
| Thwomp | `thwomp_mc` | drops on MC: life down | `r5_thwomp_hits_mc.png` |
| Wiggler | `wiggler_punch` | punch = SMW's stomp reaction (angry, flower off, 100 pts). Cape-immune (`$166E` & 0x20), not killable by stomps | `r5_wiggler_punch_angry.png` |
| Rex after a non-lethal punch | sweep t3 | used to walk on into MC → hurt. Now turns away + `$154C` = 16 | — |
| Power-up heal | `powerup_heal` | 7.5 → 10 with mushroom (already big), flower, feather | `r5_powerup_heal.png`, `r5_life_bar_7_5_and_full.png` |
| Fire outline | `powerup_colors` + pixel count | 0 teal pixels in normal / fire / cape / star | `r5_fire_black_outline.png` |
| Yoshi / climbing / cape flight poses | `yoshi_yi2`, `climb_iggy`, `cape_flight_poses` | SMW's sprites in those states, MC art otherwise | `r5_yoshi_smw_riding_pose.png`, `r5_climb_smw_pose.png`, `r5_cape_flight_smw_poses.png` |
| Triple enemies | `triple_enemies` | Rex at 516 gets copies at 492 / 540. The sliding Koopa on the slope gets none (no floor beside it). Two slots kept free | `r5_triple_enemies.png` |
| No knockback / super punch / always Yoshi / slow motion | `cheats_r5` | life 7.5, hurt flag 0, idle frame. Urchin knocked out by a jab. Re-entered YI1 on Yoshi. Game keeps running | `r5_noknockback_superpunch_yoshi.png` |
| Cheat combinations | `cheat_sweep.py` (14 cheats: singles, 91 pairs, all, power-up locks; 112 runs, 4 parallel; route re-enters YI1) | 112/112 reach the map, 0 crashes | `r5_cheat_sweep_summary.csv`, `r5_cheat_sweep_samples.png` |
| Menu | `menu_r5`, `cheats_yi1` | CHEATS hub (MARIO / WORLD / ALL CHEATS OFF, "N ON" counts), SMW status-bar coin cursor and bonus-star ON marker (GFX28 tiles 0x2E / 0x64) | `r5_menu_cheat_pages.png`, `r5_menu_mario_cheats_coin_star.png`, `r5_cheats_in_game.png` |
| Wall-run triangle pose | — | code only. The poked tile 1B4 is not a wall-run triangle (SMW Mario doesn't wall-run on it either) | — |

Test-infrastructure fixes (H):
- Pilot runs never write `smw.user.ini`. A menu pilot used to leave cheats on for later runs, which contaminated a sweep chunk
  and the first cheat sweep.
- `build_smw.ps1` rebuilds everything when any header is newer than smw.exe. The Makefile has no header dependencies, and a
  stale `pilot.o` saw the old `kCheat_Count` / `g_config` layout.

## Round 20 (chat request: Mario Combat's own level in the level select, for both Marios; MC's damage)
Core traces: 7/7 PASS. `regress.ps1`: 100 pilots; identical summaries to round 19 except the min life of the 10 pilots
that take hits (each hit now 0.5, was 2.5: one hit 7.5 -> 9.5, Iggy's two 5.0 -> 9.0) (`r20_regress_summary.txt`).
Three menu pilots (`bossrush_r15`, `levelselect_r14`, `menu_rush_r18`) step past the new first row; `levelselect_r14`
had entered Yoshi's Island since round 15's BOSS RUSH row and reaches the Donut Secret House again. Cheat sweep: 911/911
OK (`r20_cheat_sweep_summary.csv`). `levelselect_scenarios.py`: 27/27 PASS (one new: MARIO COMBAT refused on the
title; the scenarios' menu paths step past the new row; given a relative root every scenario fails with exit code 2:
pass the absolute path). The two runs: `r20_runs.txt`.

| check | pilot / tool | result | evidence |
|---|---|---|---|
| The areas | `mc_level_export.py` previews (MC's picture with the SMW tiles over it) | five areas, 27 rows; ground, lava, spikes, brown blocks, door / pipe / lift where MC's are (H) | `r20_area1_tiles.png` .. `r20_area5_tiles.png` |
| MC Mario beats it | `tas_backtrack.py mcl ... mc_runner --mcl` per area (`mclevel_chain.ps1`), then the kept run replayed by a plain pilot script with shots | all five areas, Bowser down, "BOWSER DOWN!", "MARIO COMBAT CLEAR!", the map with the run's lives given back; 0 hits in 9,464 frames (`tas_hits.py mcl a1 --to won_a5`) (H) | `r20_mc_mario_run.png` |
| SMW Mario beats it | the same with session `mcls` (wall jump on, spin jumps on Bowser) | all five areas (wall jumps up area 2's shafts, the lift, Bowser stomped), the same ending; 0 hits, no power-down or life lost in 9,043 frames (`tas_hits.py mcls a1 --to won_a5`, which now also counts SMW Mario's power-downs). Runner fixes on the way: an SMW power-down and a death hidden by 1-UPs now count (one earlier "win" against Bowser was a death's restart: the replay showed it), the wall jump is pressed only while falling (SMW's grabs a wall only then, never the one it last left), X only to far or higher targets, a running hop across a gap to a lower one (H) | `r20_smw_mario_run.png` |
| Menu | pilot (LEVEL SELECT from the settings menu, Return) | MARIO COMBAT first, its description, "MARIO COMBAT 1/5" and area 1; on the title: "START OR LOAD A GAME FIRST" (H) | `r20_menu_start.png`, `r20_menu_title_refused.png` |
| Lift, SMW Mario | pilot (run onto the marker, Up while running) | before: no ground friction on the lift (SMW clears the on-sprite flag at `ProcessNormalSprites`' start, after the old hook set it): he slid off its end at running speed while it rose, and the area went on without him. After: stops on it, rides 304 -> 160, cages open, Koopas drop (H) | `r20_lift_smw_mario.png` |
| Yellow blocks at the top of area 3 | pilot `ppuw 212C` (one layer at a time), `map16grid`, `mctiles` catalogue | they were layer 1: MC's ceiling spikes written as map16 128 / 129, turn blocks in the castle tileset (no hurt). Now 159 / 15A, the castle's yellow spikes, hurt tiles in tileset 1 (H) | `r20_yellow_blocks_layers.png`, `r20_area3_spikes.png` |
| MC's look (follow-up request) | both kept runs replayed with shots after the change; pilot up the lift; `mc_level_art.py` previews | every area drawn with MC's own pictures (stone, Bowser statues, pillars, chains, red door, grey pipe, lava, spikes, dusk sky, the castle wall behind); helper blocks, ledge, cages and lift in MC's brown block / bevelled stone; both runs still end in "MARIO COMBAT CLEAR!" and the map. `regress.ps1` summaries identical to the round's first run; core 7/7 PASS (H) | (superseded by the row below) |
| Art on the grid (follow-up: misaligned art / hitboxes) | both kept runs replayed with shots; close-ups of area 2's climb and door ledge (the reported spots); a new full recording | before: MC's pictures (blocks at 15.5 px, shapes off the 16 px grid) over the tile collision: edges up to 8 px off where Mario stands, 16 px filler blocks beside MC's own. After: the ground drawn from the tiles in MC's style (weave, 8 px bevel frame, rounded / inner corners, brown blocks), props snapped onto it, lava / spikes / pipe / bridge on their tiles: every edge on the collision. Both runs still end in "MARIO COMBAT CLEAR!"; `regress.ps1` identical (H) | `r20_grid_area2.png`, `r20_grid_areas.png`, `r20_mc_mario_run.png`, `r20_smw_mario_run.png` |
| Tan blocks, Bowser music (follow-up) | replay close-ups; pilot audio dumps of areas 4 and 5; a new recording | blocks: MC's pixel art (14 px block, 1 px seams, the outline round each group, single blocks included) instead of a blurred 16 px resample. Music: area 5's audio matches MC's sound 560 (normalised correlation 1.00 at 1.95 s in; no SMW music under it), about 1200 RMS vs the castle music's 830 in area 3; regress identical, core 7/7 (H) | `r20_brown_blocks.png`, `r20_runs.txt` |
| Lava, area 3, bridge, Bowser (follow-up 3) | pilots: lava from above (both Marios), frame-by-frame logs; chain platforms landed on (both) and jumped at from below; Bowser with Mario 190 px off the far side; area 3 start and area 5 shots; both runs re-searched from area 3 and replayed | lava: state 9 on touching it (SMW's death; before: swimming, tiles 0-5 are water); the surface's 6 frames differ (MC's 141). Area 3: 3 pillars, caps over the slab as MC's; both chain slabs stood on (MC Mario cannot reach the second from right below: 63 px jump to 80, as in MC it is reached from the stairs). Area 5: right pillar flush with the bridge; Mario falls off its far side. Bowser: same slot and life throughout, not erased. Runs: MC Mario 0 hits / 10,650 frames, SMW Mario 0 hits / 9,915 frames; regress identical, scenarios 27/27, core 7/7 (H) | `r20_fix3_area3.png`, `r20_fix3_bridge_lava.png`, `r20_mc_mario_run.png`, `r20_smw_mario_run.png` |
| Follow-up 4: camera, Bowser, edges, cape, glow, icon | pilots: MC Mario wall-jumping off area 1's end wall (log of layer-1 y); Bowser idle / fire frames every 3 frames (lowest pixel row); SMW Mario dropped on Bowser and spin-jumping onto him; cape spam on Bowser and an MC Koopa with per-frame `sprites`; both ends of every area (hold Left / Right, `peek 94` / `peek 1A`); lava shots; the exe's icon extracted; both runs re-searched and replayed | camera rises with the wall jump (layer-1 y 150 -> 68) and settles on landing. Bowser's feet on the same row (one into the ground's top, as MC's 3.1 stage px) in all 75 frames. Plain landings bounce, life stays 50; a spin jump takes 4. Cape hits 32 frames apart (Bowser 244 / 276; Koopa 298 / 332 / 364, life 10 -> 4 instead of at once). Hero and camera stop at MC's limits in all five areas (area 2: by the door, camera right edge 1353); area 5's outer sides still open. Glow: half alpha at the surface, 13 px. Icon: MC Mario's idle sprite, crisp at 16-256 (a first logo-style icon was rejected). The first MC Mario Bowser search stalled: the new bounce applied to MC Mario too and held him on Bowser under his flames; fixed (no stomp for MC Mario, as in MC). Runs: MC Mario 3 hits (Bowser fight) / frame 12,745, SMW Mario 0 hits / frame 9,598; regress identical (100/100), King Koopas pilots identical after the fix, scenarios all PASS, core 7/7 (H) | `r20_fix4_walljump_camera.png`, `r20_fix4_bowser_feet.png`, `r20_fix4_walls.png`, `r20_fix4_lava_glow.png`, `r20_fix4_icon.png`, `r20_fix4_runs_end.png`, `r20_runs.txt` |
| Damage | `regress.ps1` min life; `config.c` | 0.5 per hit (20 from full); `HitDamage` key, old `DamagePerHit` ignored (H) | `r20_regress_summary.txt` |

## Round 19 (chat request: punch anything on Pokeys and the like, every moving platform, three videos)
Core traces: 7/7 PASS. `regress.ps1`: 100 pilots, identical summaries before and after this round's last code changes
(`r19_regress_summary.txt`). Cheat sweep: 911/911 OK (`r19_cheat_sweep_summary.csv`; `cheat_sweep.py` now makes its root absolute: given `.` it had passed paths the game, running in its own folder, could not open). Round-19 test outputs: `r19_*.txt`.

| check | pilot / tool | result | evidence |
|---|---|---|---|
| Punch anything | `punch_all_test.py` (76 ordinary enemies audited with `--on`; the test's 22 from the added ones x off / on / with MC hit reactions) | off: all immune as before; on: each knocked out (Pokey: a segment per punch at the fist's height, 3 punches for 3 segments, as SMW's hits); with MC hit reactions they take MC hits (still up after the test's 6 attacks, as round 18's; 44 and the Pokey's segments go at once). 22/22 (H) | `r19_punch_all.txt` |
| Platforms where the game puts them | `platform_levels_test.py` (35 kinds, MC vs SMW Mario: stand 140 frames, walk, two hops) | standing / riding: frames on it and drops identical to SMW Mario's for every kind found (e.g. timed lift 85/140 both, sinking lava platform 53/140 both). 2 "FAIL"s come after the hops: off the swinging brown platform (5F) and the winged grey platform (C1) MC Mario's higher jump comes down after the platform has moved on and he falls past it (5F: into the pit) where SMW Mario lands back on; MC's own jump, not a platform bug (H) | `r19_platform_levels.txt`, `r19_platform_5F_C1.png` |
| Platforms in open sky | `platform_ride_test.py` (18 sprites) | 18/18 (H) | `r19_platform_ride.txt` |
| From below / above | `platform_contact_test.py` (31 kinds: a jump into it from below, a drop from 100 px) | 25/31 as SMW Mario. The 6 others differ in when MC's jump meets a moving platform (higher, longer jump: "above" where SMW Mario's head is "on" / bumped, a missed landing on the rotating A3 or hit flying block 84 that has moved on); every kind still bumps or lets him through as SMW's rule says (H) | `r19_platform_contact.txt`, `notes/platforms.md` |
| Jump off ropes / Yoshi | adhoc pilots (rope, vine, Yoshi; MC vs SMW) | before: Yoshi dismount about 130 px vs SMW's 68. After: rope 41 / 49 / 64 px vs 41 / 51 / 66, Yoshi 75 vs 68 (H) | - |
| Video runs | `tas_hits.py` on the kept runs | final level 0 hits (11,894 frames); boss rush 0 hits (25,796 frames, BOSS RUSH CLEAR! 7:10.07); combo 21 hits, Bowser out of his hurt clip at most 2 frames between jabs (H) | `r19_video_runs.txt` |
| Videos | `video_sheet.py` on the mp4s | 1024x896, 60.0988 fps, AAC; frames in = frames out (1173 / 13423 / 26261); the menu, every fight, Bowser's defeat and the ending are there; no overlays (H) | `r19_video_combo.png`, `r19_video_final.png`, `r19_video_final_end.png`, `r19_video_rush.png`, `r19_video_rush_end.png` |
| Notices under the pilot | combo recording | "SETTINGS SAVED" stays 108 frames after the menu closes (before: real time, so a fast replay showed it for 20 s of game) (H) | `r19_video_combo.png` |

## Round 18 (chat request: wall jumps off flying ? blocks, P-switch coin blocks, punching the Koopa Kids, boss rush music and Reznors, Bowser x2, moving platforms, punch anything cheat)
Core traces: 7/7 PASS. `regress.ps1`: 100 pilots; the 95 of round 17 give identical summaries, plus the 5 `*_r18` (`r18_regress_summary.txt`). Cheat sweep: 911/911 OK with the new
cheat (`r18_cheat_sweep_summary.csv`). `air_attack_wall_test.py` 120/120, `levelselect_scenarios.py` 26/26,
`transition_matrix.py` 176/176 (`r18_transition_matrix.txt`).

| check | pilot / tool | result | screenshot |
|---|---|---|---|
| P-switch coins | `pswitch_coins_r18` (YI2's coin arc, blue P-switch on via $14AD, MC Mario jumps into the blocks) | before: the touched block became map16 12B (yellow block) and stayed after the switch ran out (old code rebuilt and run). After: 02B throughout, brown used blocks while it runs, coins again after (H) | `r18_pswitch_coins.png` |
| Solid sprite walls | `solid_sprite_wall_test.py` (message-box wall, flying ? blocks 83 / 84 fresh and used; hops and jumps at 10 timings), `solid_wall_r18` | before: pushed out with no wall seen, no cling (frame 9 throughout). After: box wall and used blocks 10/10 cling, 10/10 wall jumps; fresh flying blocks cling and kick wherever SMW pushes Mario out of their side (2-3 of 10 timings: the others pass under the moving block, where SMW pushes nothing); full jumps land on top (H) | `r18_solid_sprite_wall.txt`, `r18_solid_wall.png` |
| Moving platforms | `platform_ride_test.py` (18 platform sprites in open sky, MC vs SMW Mario: standing, 3-frame walk, two jumps), `reznor_platform_r18` (a knocked-off Reznor's platform round the wheel) | 18/18: frames on the platform and drops within SMW Mario's (most 140/140 both), on it after the jumps whenever SMW Mario is; the brown and grey chained platforms ride and slide off as SMW Mario does. Reznor's platform: on the ground 407 of 408 frames round the wheel (all but the drop onto it) (H) | `r18_platform_ride.txt`, `r18_reznor_platform.png`, `r18_chained_platforms.png` |
| Reznor rebound | pilot log (hops under the wheel) | MC x speed 9.75 -> -6.45 on touching a live Reznor's side (SMW's -0x20 = -7.2 MC, less one tick's friction); before, MC walked on into him (M) | |
| Koopa Kid punches | `boss_punch_test.py --ground` (punches from the ground only) and the mixed run | before: 0 punches hurt Morton / Roy / Ludwig (layer $1632 0 vs Mario's 3; every punch skipped). After: 3 punches beat each of them; Iggy, Larry, Lemmy, Big Boo, Wendy beaten; the bot still dies in the Reznor rooms' lava after 2 hits (the Reznor fight is won in `bossrush_test.py`) (H) | `r18_boss_punch_ground.txt`, `r18_boss_punch.txt` |
| Bowser x2 | `boss_punch_test.py bowser --rounds 150`, `bossrush_test.py bowser_after_all` | punches 1..6 -> hits on 2, 4, 6 ($14B5 set after every second punch); his phase moves on in the boss rush (H) | `r18_bossrush.txt` |
| Boss rush | `bossrush_test.py` (10 scenarios) | EVERY BOSS = 10 rooms in order, one Reznor (0DF); `real_fights` samples $1DFF each round: music on in every fight (52/52, 43/43, 27/27, 138/138). With the $0DDA reset taken out: 0/43, 1/27, 0/138 after the first fight (H) | `r18_bossrush.txt`, `r18_menu_rush.png` |
| Punch anything | `punch_all_test.py` (18 enemies x off / on / with MC hit reactions) | off: all immune; on: each knocked out by the first attack; with MC hits: MC life down (4-9 of 10), knocked flying (Torpedo Ted, no MC kind: knocked out). Podoboo, line-guided Grinder / Fuzzy and the spike ball can't be spawned on dry ground (same code path) (H) | `r18_punch_all.txt`, `r18_punch_all.png`, `r18_menu_cheat.png` |

## Round 17 (chat request: MC Mario floating into ghost houses, air attacks stopped against bricks)
Core traces: 7/7 PASS. `regress.ps1`: 95 pilots; the 93 of round 16 give identical summaries, plus `door_walk_r17`,
`air_attack_cling_r17` (`r17_regress_summary.txt`). Cheat sweep: 869/869 OK (`r17_cheat_sweep_summary.csv`).

| check | pilot / tool | result | screenshot |
|---|---|---|---|
| Door walk | pilot log (Donut Ghost House from the level select), `door_walk_r17` (Donut Ghost House, Iggy's Castle, Front Door) | before: state 0A with MC hero frame 9 (jump pose), on-ground 0 the whole walk: he glided over the floor. After: on-ground 1, walk then run frames (2, 10, 11), a skid at the door, into the door; same in the castles. SMW Mario's walk unchanged (reference run with `setting mc 0`) (H) | `r17_ghost_house_walk.png`, `r17_door_walk.png` |
| Air attacks at blocks | `air_attack_wall_test.py` (YI1 cement column: full jumps, short hops that cling; a straight jump under a turn block; Y pressed 2-40 frames after take-off; each vs the same press in open air) | 120 runs, 0 failed, 10 of them pressed while clinging (wall pose). With the fix switched off: 22 of 80 failed, no attack at all when falling against the column or clinging to it. Attacks under a block run until he lands (H) | `r17_air_attack_wall.txt`, `r17_air_attack_cling.png` |

## Round 16 (chat request: Mario Combat logo on the title screen, boss rush transitions, level select from rooms, transition bug hunt)
Core traces: 7/7 PASS. `regress.ps1`: 93 pilots; the 90 of round 15 give identical summaries, plus `title_r16`,
`bossrush_transitions_r16`, `levelselect_subroom_r16` (`r16_regress_summary.txt`). Cheat sweep: 869/869 OK
(`r16_cheat_sweep_summary.csv`). Round 14's `levelselect_test.py`: 186/186; `levelselect_scenarios.py`: 26 PASS
(two menu scenarios re-counted for the BOSS RUSH row, `rush_title_refused` added).

| check | pilot / tool | result | screenshot |
|---|---|---|---|
| Title screen | pilot `title_r16` | "+" and Mario Combat's logo (shape 90, 177x45) under SMW's logo, readable over the demo; fades in with the screen (drawn through the overlay with the PPU brightness); gone in the file menu (H) | `r16_title.png`, `r16_title_zoom.png` |
| What a room inherits | `transition_probe.py` (13 boss rooms: warped in fresh vs reached in a boss rush; RAM diff by variable) | before: player state 0A (door walk) and Boos in Morton's room after Big Boo, Mario on the wrong side; mode 7 rotation 0x1DA left from Iggy / Reznor in 12 rooms; scroll sprites, layer 3, boss variables. After the fix every fight looks like the fresh room; left: lava scroll direction, a 1-frame Reznor wheel offset, Wendy keeping Reznor's mode 7 tilemap index (as vanilla does after a fortress) (H) | `r16_transition_probe.png`, `r16_transition_probe_detail.png` |
| Transition matrix | `transition_matrix.py` (16 places x 11 targets incl. the boss rush; each vs the same target from the map: level, state, Mario's place, camera after a 150-frame walk, mode 7 rotation, scroll flags) | 176 pairs, 0 failed. With the clean-up switched off (`--noclear`, first 91 pairs): 32 failed, e.g. YI3's pipe room -> YI2: camera stuck at x 0, Mario falling to his death (the reported lock); door-walk state after ghost house / wings rooms; mode 7 rotation after Iggy / Reznor (H) | `r16_transition_matrix.png`, `r16_transition_matrix.txt`, `r16_transition_matrix_noclear.txt` |
| Boss rush | `bossrush_test.py` (9 scenarios), pilot `bossrush_transitions_r16` | 9 PASS, new: `bowser_after_all` (12 fights first, then Bowser upright in his fighting phases and hurt by kicks), `kids_move` (Morton and Roy move about after the earlier bosses; Ludwig waits for Mario in a fresh room too) (H) | `r16_bossrush_transitions.png`, `r16_bossrush_test.txt` |
| Level select from a room | pilot `levelselect_subroom_r16` | from the room behind YI3's pipe, YI2 starts at its start and the camera follows (H) | `r16_levelselect_subroom.png` |

## Round 15 (chat request: MC punches on bosses, punched carryables fall over, ice, jumping enemies, MC hit flings, air dash into pipes, boss rush; `ISSUES_FIXED.txt` retired)
Core traces: 7/7 PASS. `regress.ps1`: 90 pilots; the 82 of round 14 give identical summaries, plus 8 `*_r15`
(`r15_regress_summary.txt`). Cheat sweep: 869/869 OK (`r15_cheat_sweep_summary.csv`).

| check | pilot / tool | result | screenshot |
|---|---|---|---|
| Bosses take punches | `boss_punch_test.py` (warp into each boss room from YI1, No damage + Infinite lives, MC Mario walks up and punches / jump-kicks) | Iggy, Morton, Big Boo, Lemmy, Ludwig, Roy, Wendy, Larry beaten (end-of-level, no death); Bowser beaten with jump kicks (13 hurt checks, Peach scene, `$13C6` 1). Reznor: punches knock them off (`$151C` per slot 0 -> 1), but the bot dies in the lava once the bridge crumbles (2 down); the full Reznor fight was won in the boss rush run below (H) | `r15_boss_punch.png`, `r15_boss_punch.txt` |
| Boss rush | `bossrush_test.py` (7 scenarios), pilots `bossrush_r15` (the real menu), `bossrush_death_r15` | 7 PASS: 8 / 13 rooms in order, map / save bytes / lives / power back, retry and end-run on death, abandon by level select, started from a level, real fights by punches through 6 rooms (H) | `r15_bossrush_menu.png`, `r15_bossrush_death.png`, `r15_bossrush_and_dash.txt` |
| Punched carryables | `stun_check.py`, pilot `stun_r15` | 0D Bob-omb, 0F Goomba, 11 Buzzy, A2 Mechakoopa: status 9 (stunned); 04-07 shelled Koopas: shell stunned + the Koopa walks out (no longer hit by the same punch); 08 / 0A para-Koopas: shell; a second punch kicks (status 10; Goombas / Bob-ombs / Mechakoopas land stunned again, as in SMW); 00-03, 13, 30, 31, 6F, BF knocked out as before; 46 Diggin' Chuck, AB Rex as before (H) | `r15_stun.png`, `r15_stun_check.txt` |
| MC hit reactions | `mchits_sweep.py` (201 ids, on / off), pilots `mchits_chuck_r15`, `mchits_mole_r15` | Chucks and Monty Moles (out of the ground) fly back and slide; new flags only from the longer slide: 92-98 Chucks and 31 Bony Beetle slide past the right edge of one-screen Yoshi's House and are despawned (as anything knocked off screen); 33 Podoboo timeout and 29 / 36 / 1C / BD as since round 9 (M: by design) | `r15_mchits_chuck.png`, `r15_mchits_mole.png`, `r15_mchits_sweep_report.txt` |
| Ice | pilot `ice_r15` | Slippery cheat: standing still stays still (before: a constant backward drift, MC friction flipping a speed under 1); slides on after a run and stops; also in room 10B (Donut Secret 2, SMW's own ice) (H) | `r15_ice.png` |
| Jumping enemies | pilot `jumpenemies_r15` | the Goomba jumps on every ground jump of MC Mario (before: never) and of SMW Mario (H) | `r15_jumpenemies.png` |
| Air dash into walls | `dash_pipe_test.py` (240 runs), pilot `dash_pipe_r15` | 0 deaths, no pipe state; before the fix small SMW Mario died once (landing kept the dash speed into a corner). MC Mario's reported pipe entry not reproduced (side-pipe exit in 024: no entry during a dash, enters normally after) (M) | `r15_dash_corner.png`, `r15_bossrush_and_dash.txt` |

## Round 14 (chat request: a level select page, every level incl. unused ones, extensive bug tests)
Core traces: 7/7 PASS. `regress.ps1`: 82 pilots; the 80 of round 13 give identical summaries, plus `levelselect_r14`,
`menu_r14` (`r14_regress_summary.txt`). Cheat sweep: 869/869 OK, deaths only in One-hit runs
(`r14_cheat_sweep_summary.csv`).

| check | pilot / tool | result | screenshot |
|---|---|---|---|
| Catalogue | pilot `levelscan` + `level_catalogue.py` | 512 slots: 92 map, 132 rooms (exits from the map levels), 6 code-loaded, 264 test level, 18 unreached = 9 contents in two banks; the menu's 93 entries: 76 map levels in 9 worlds (every map translevel except the 16 tiles never entered), intro, 4 bonus rooms, 12 unused (test level, DP1 copy, 108, 9 rooms); no duplicates, translevel / level numbers consistent (H) | `r14_level_catalogue.txt` |
| Every entry | `levelselect_test.py` | 186 runs (93 entries from the map and from inside YI1): the named level loads (the bonus game / wings areas as SMW's own rooms), after 150 frames of play still running (castles and ghost houses: door scene, then the level with its timer and map number), after a death back on the map with the same save-buffer hash, tile and submap, session closed: 0 failures (H) | `r14_levelselect_yi.png`, `r14_levelselect_unused.png`, `r14_levelselect_all.txt` |
| Situations | `levelselect_scenarios.py` | 25 PASS: dying, goal walk, message box, riding Yoshi, star + P-switch, in a room, in a pipe, SMW pause, title (refused), map walking (refused), fade (refused), same level, chain of 3 + goal, midway, game over (to the title, session dropped), SMW Mario, Triple enemies + Always Yoshi, Start+Select, bonus game to its end, wings area to its end, Iggy beaten (castle cutscene, castle not destroyed, `$1EC7` 00), rooms in bank 0 (001 screen 17 -> 0D8) and bank 1 (YI3 pipe -> 1xx), the menu itself (H) | `r14_levelselect_scenarios.png`, `r14_levelselect_scenarios.txt` |
| Bugs found | same | castles / ghost houses: the second (post-door) load took the map tile's level (000 / 105): fixed by taking the level for every fresh start; non-map entries started a door scene leading nowhere: skipped; `#` missing from the menu font: castle names without it (H) | |
| Menu | `menu_r14` | LEVEL SELECT on the main page, hub of 11 groups, each list with level numbers (grey) or UNUSED (gold), descriptions; the unused list scrolls (12 + BACK) (H) | `r14_menu.png`, `r14_levelselect_pilot.png` |

## Round 13 (chat request: more gameplay cheats, settings unlike the existing ones)
Core traces: 7/7 PASS. `regress.ps1`: 80 pilots; the 77 of round 12 give identical summaries, plus `airdash_r13`,
`settings_r13`, `menu_r13` (`r13_regress_summary.txt`). Cheat sweep: 869/869 OK (40 cheats, every pair, knockback 20x
runs; deaths only in One-hit runs) (`r13_cheat_sweep_summary.csv`).

| check | pilot / tool | result | screenshot |
|---|---|---|---|
| Air dash | `airdash_r13` (YI2) | 12 frames after the press: SMW Mario +71..74 / -71..75 px (R / L), MC Mario +72..75 / -71..75; without the cheat 0; Mirror mode: R goes left in the level, L right; a second R in the same jump adds nothing (126 px in all, as one dash) (H) | `r13_airdash.png` |
| Respawning enemies | `respawn_levels.py` (every ordinary enemy in a level it appears in; Super punch, glide 400 px on and back) | back only with the cheat: 22 (00 01 03 13 27 37 38 4D 6E 72 73 91 92 94 97 98 99 9A AB B4 BD BF); back either way: 11 (other copies in the level); not defeated by the test: 16; 38 where the run never got back to the spot (Mario fell, or a pipe / start of level; same with the cheat off); rc 0 everywhere; the cheat adds at most the one enemy (on - off = 1 in the counts) (H) | `r13_respawn.png`, `r13_respawn_levels.txt` |
| Output (mono) | `settings_r13` + `settings_check.py` (Iggy's Castle music) | stereo: left != right in 62,292 of 64,080 samples; mono: 0 (H) | `r13_settings_audio.txt` |
| In background | `settings_r13` (pilot `focus`) | PAUSE: frame counter $13 0x58 -> 0x59 over 60 unfocused frames (the frame the command ran), Mario stands still; back in focus 30 frames -> 0x76; MUTE: 60 frames run (-> 0xB3), its WAVs all zero; PLAY ON: runs (-> 0xD1) (H) | `r13_settings.png`, `r13_settings_log.txt` |
| Rumble | `settings_r13` | FULL: hurt 39300/52400 250 ms, MC hit 9825/32750 70 ms, pound quake 45850/9825 90 ms every 4 frames while it shakes, lost life 65500/39300 500 ms; LIGHT: 40% (hurt 15720/20960); OFF: no line (H); real gamepad not tried | `r13_settings_log.txt` |
| Stick deadzone | code / menu | radius = setting x 32767 / 100 in 64-bit math (the old fixed radius 10000 = 30.5%); menu steps 10-50% (H for the code, real stick not tried) | |
| Menu | `menu_r13` | new rows with values and descriptions; IN BACKGROUND value shortened to PLAY ON after it overflowed; MOVE / WORLD CHEATS 12 rows, no scrolling (H) | `r13_menu.png` |

## Round 12 (chat request: knockback scale 1x-20x, enemy-changing cheats, more cheats)
Core traces: 7/7 PASS. `regress.ps1`: 77 pilots; the 74 of round 11 give identical summaries except `cheats_r7_moves`
(413 frames: the stored round-11 line, 379, was saved 33 s before that pilot's two landing waits went from 13 to 30
frames; 402 frames of waits + 11 shots = 413), plus `knockback_r12`, `cheats_r12`, `menu_r12`
(`r12_regress_summary.txt`). Cheat sweep: 788/788 OK (38 cheats, every pair, plus knockback 20x alone / with each cheat
/ with all; deaths only in One-hit runs) (`r12_cheat_sweep_summary.csv`).

| check | pilot / tool | result | screenshot |
|---|---|---|---|
| Knockback scale | `knockback_r12` + `knockback_check.py` (YI2 flat ground, pilot `hurt`) | light hit 5 / 10 / 27 / 57 / 110 px at 1 / 2 / 5 / 10 / 20x (2.0, 5.4, 11.4, 22.0x); heavy 65 / 128 / 329 / 658 / 1127 px along, 32 / 65 / 163 / 288 / 288 up (10x, 20x stop at the level's top, y 64) (H) | `r12_knockback.png`, `r12_knockback.txt` |
| Screen edge | cheat sweep `knock20+fastenemies` (first run) | a fast enemy hit Mario at YI1's start (x 39): 20x knockback took him from x 4 to -1 in one frame, SMW's off-screen kill; with `ClampIntoScreen` he stops at x 2 and SMW pushes him back to 8, run ends on the map with no death (H) | |
| Low gravity | `cheats_r12` | standing jump with B held: SMW Mario 64 -> 127 px, MC Mario 63 -> 124 px (H) | `r12_cheats.png` |
| Fast enemies | `cheats_r12` | a Goomba walks 40 px in 40 frames (20 without) (H) | `r12_cheats.png` |
| Chasing enemies | `cheats_r12` (No damage, Mario walks through a Goomba) | without: the Goomba walks on left (x 84); with: it turns and follows Mario right (x 163 -> 175) (H) | `r12_cheats.png` |
| Jumping enemies | `cheats_r12` | Goomba, Koopa, Spiny, Buzzy Beetle all at y 338 (30 px up) 14 frames after Mario's jump, both Marios (H) | `r12_cheats.png` |
| Enemy cheats, every sprite | `sprite_sweep.py --cheat 34,35,36` vs the plain sweep (`sweep_compare.py`) | 201 sprites x 2 Marios: no exit-code change (33 Podoboo times out in both), no new death; 126 other changes, all timing (enemies reach Mario / leave the screen sooner) or SMW's own transformations reached sooner (46 -> 91 Chuck, BD -> 02 Koopa, 08 -> 04 Koopa) (H) | `r12_enemy_sweep_samples.png`, `r12_enemy_sweep_compare.txt` |
| Enemy cheats in levels | `enemy_cheats_levels.py` + `enemy_cheats_sheets.py` | 87 ordinary enemies, each in a level it appears in, cheats on / off: rc 0 everywhere; the 23 runs that end on the map do so with the cheats off too (Mario falls into a pit after the seek); 15 sheets looked at: Sumo Bro one lightning bolt at a time, Chucks normal, Mega Mole carries Mario as it chases (H) | `r12_enemy_levels_sample.png`, `r12_enemy_levels_summary.txt` |
| Menu | `menu_r12` | KNOCKBACK (MC) 1X -> 2X -> 20X -> wraps to 1X, counts as on only above 1X; LOW GRAVITY, FAST / CHASING / JUMPING ENEMIES rows with descriptions; ALL CHEATS OFF puts 1X back; no page scrolls (H) | `r12_menu.png` |

## Round 11 (chat request: infinite jumps, ground pound animation, open map, more cheats)
Core traces: 7/7 PASS. `regress.ps1`: 74 pilots; the 70 of round 10 give identical summaries (`cheats_r7_moves` now
waits 30 frames for the pound landing), plus `pound_anim`, `open_map`, `cheats_r11`, `menu_r11`
(`r11_regress_summary.txt`). Cheat sweep: 602/602 OK (34 cheats, every pair; deaths only in One-hit runs) (`r11_cheat_sweep_summary.csv`).

| check | pilot / tool | result | screenshot |
|---|---|---|---|
| Infinite jumps | `cheats_r11` + jump test | eight presses: both Marios from y 352 to the top of the level (y 64), land safely (H) | |
| Ground pound animation | `pound_anim` | MC: shape 437 somersaults while hanging 16 frames (grav 0), then the tucked seat-down drop at grav -20, lands at frame 46; SMW: spin-jump poses while hanging, ducking drop; both: shake, smoke left / right, Koopa knocked out (H) | `r11_pound_anim.png` |
| Open map | `open_map` + peeks | own map / all paths drawn / walked YI1 -> YH -> YI2-4 -> Iggy -> ladder -> Donut Plains -> DP1 entered; $1F02 zero before and after; own map again with the cheat off; only SMW's own arrival bits in $1EA2 (levels 25-2A) (H) | `r11_open_map.png` |
| Stomp anything | `sprite_sweep.py --cheat 33` vs the round-8 sweep | stomp test: 62 sprite / Mario pairs changed to a stomp (bounce; flipped / stunned / knocked out); no crash, no new death; still a hurt after the bounce: MC on 50, SMW on B0; touch / punch tests changed only where the enemy ends up under Mario (B3, C2, C3, 1D, 39, AE) (H) | |
| Menu | `menu_r11` | new rows, COIN MAGNET on WORLD, no page scrolls (H) | `r11_menu.png` |

## Round 10 (chat request: instant hits, more gameplay cheats)
Core traces: 7/7 PASS (with the two API functions appended to `mc_core.c`). `regress.ps1`: 70 pilots; the 67 of round 9
give identical summaries, plus `instant_hits`, `cheats_r10`, `menu_r10` (`r10_regress_summary.txt`). Cheat sweep:
503/503 OK (31 cheats, every pair; deaths only in One-hit runs) (`r10_cheat_sweep_summary.csv`).

| check | pilot | result | screenshot |
|---|---|---|---|
| Hit-box frames | `attackframes` | each move's hit frame(s) (DATA_NOTES §3); wind-ups 1-16 frames (H) | |
| First-frame hits | `instant_hits` + `instant_hits_check.py` | 29 move starts covering all 16 moves (punch combo, walking punches, low sweeps, air kicks, timed presses for attack4/5/6/9/10/102-105): 0 without a hit box on the first frame (H) | `r10_instant_hits.png` |
| Combo speed | tap Y every 5 frames, Yoshi's House | hit-box frames 16 (cheat) vs 5 (off); attack1 -> 2 -> 3 -> 1 ... a move every 5 frames (H) | |
| Lunge stacking | same | before the cap xSpeed reached 19 and Mario slid out of Yoshi's House; with the cap max 10 (H) | |
| With MC hit reactions | 10 taps on a Goomba | life 1.0 (instant) vs 6.0 (off): each move lands (hit wait kLingerFrames + 1) (H) | |
| Block buster | `cheats_r10` (YI1 grey tower x 1904) | tiles at y 320, 352, 368 -> 025 after punches, air kick and low sweep; floor 100 at y 384 stays (H) | `r10_cheats.png` |
| Pit rescue | `cheats_r10` (Butter Bridge 1) + SMW run | MC: back at the log end (314, 304), life 10 -> 7.5, lives 5 kept; SMW Mario big -> small (state 1), lives kept (H) | `r10_cheats.png` |
| Menu | `menu_r10` | new rows with stars and descriptions, MOVE CHEATS 12 rows without scrolling (H) | `r10_menu.png` |

## Round 9 (chat request: MC hit reactions for SMW's enemies, more gameplay cheats)
Core traces: 7/7 PASS. `regress.ps1`: 67 pilots; the 65 of round 8 give identical summaries, plus `cheats_r9` and
`menu_r9` (`r9_regress_summary.txt`). Cheat sweep: 413/413 OK (28 cheats, every pair; the only deaths are One-hit
runs) (`r9_cheat_sweep_summary.csv`).

| check | tool | result | screenshot |
|---|---|---|---|
| Every sprite, Yoshi's House | `mchits_sweep.py` 00-C8 (8 combos each, cheat on / off), `mchits_report.py` | 54 enemies taken over; no crash, floor sinking, stuck knock or lost sprite caused by the cheat. Flags left: 29 / 33 / 36 crash or time out with the cheat off too (out of their rooms); 1C Bullet Bill knocked back keeps flying off the left edge (its own flight); BD becomes a blue Koopa on landing and the stationary test Mario dies to it when its jab no longer knocks it out (H) | `r9_mchits_yh_00..08.png`, `r9_mchits_sweep_report.txt` |
| Every enemy in a level | `mchits_levels.py` (pilot `seek`, 6 combos, cheat on / off, 140 sprite ids) | all found; 31 taken over (others out of reach or immune); bosses (29, A0, A9, C5) untouched; enemies knocked over ledges fall in pits and are removed; the only Mario deaths are the test `attack` walking after them into the pit (00, 01, 10, 94); nothing in walls (H) | `r9_mchits_levels_00..07.png` |
| Super Koopa | `mchits_sweep.py 71 73` + trace | before: hit while taking off (c2 = 1) it got up with y speed 0, started its flight on the ground and flew 19 px into the floor; fix: its own speeds are restored when it gets up: climbs to 340, flies at 358 (H) | |
| Fliers | sheets | no downward knockback: a pass-through flier (`$1686` bit 7) can't be pushed into the ground (H) | |
| Life steal / fire punch | `cheats_r9` | life 7.5 -> 9.5 over the hits on a Goomba; each punch throws a fireball, which turns a Koopa into a coin (H) | `r9_cheats.png` |
| Menu | `menu_r9` | LIFE STEAL (MC), FIRE PUNCH (MC), MC HIT REACTIONS rows with stars and descriptions, hub counts (H) | `r9_menu.png` |

## Round 8 (brief `PROJECT_BRIEF.md`, round 8)
Core traces: 7/7 PASS. `regress.ps1`: 65 pilots. The 55 earlier ones give the same summaries as round 7 except
`blockwall_yi1` (967 frames vs 949: the higher running jump), plus 10 round-8 pilots (`r8_regress_summary.txt`; the
layer-2 change left all 64 others identical). Cheat sweep: 332/332 OK (25 cheats, every pair; the only deaths are One-hit runs) (`r8_cheat_sweep_summary.csv`). Sprite
sweep: the same 18 explained differences as rounds 5-7 (`r8_sprite_sweep_report.txt`). Original-vs-C (MC off, boot
to YI1 and a run through it, including a death): 0 mismatches (`r8_original_vs_c_0_mismatches.png`). `package.ps1`: smoke
exit 0 (`r8_packaged_smoke_test.png`). The cheat sweep and regression were re-run on the final build (with the layer-2
change); the sprite sweep ran on the build before it (Yoshi's House has no layer-2 collision, so its tests take the
same path).

| check | pilot | result | screenshot |
|---|---|---|---|
| Bowser 50 life | `king_koopas*` + life log | starts at 50; fire-rate thresholds at 70 / 50 / 20 % of 50 (default rate 20 at life 24); bar half at life 24; all King Koopas pilots as in round 7 (H) | `r8_king_koopas_hits.png` |
| New cheats | `cheats_r8` | slippery: MC and SMW slide; magnet: coins within 64 px fly to Mario one per frame, Yoshi coin counted; flying Yoshi: taps of B keep Yoshi up (H) | `r8_cheats.png` |
| Menu | `menu_r8` | hub icons (mushroom, feather, shell, star) from GFX00/GFX01; descriptions; notices (H) | `r8_menu.png` |
| Running jump | `jump_distance`, flat YI1 | standing 63 px (SMW 64), after 60 frames walking 83 px, full run 109 px (SMW 109) (H) | |
| Wall + held jump | `mc_wall_held_jump`, `blockwall_yi1` | a held jump rises along the wall and clings without a kick; a fresh press kicks (H) | `r8_wall_held_jump.png` |
| Doors | `door_up` | Up in front of the Donut Ghost House door enters it (state 0x0D) (H); P-switch doors 27/28 from SMW's code only (M) | `r8_door_up.png` |
| Ghost ship | `ghostship_draw` | MC Mario drawn with OBJ on the sub screen only (H) | `r8_ghostship.png` |
| Auto-scroll edge | `autoscroll_edge` | MC Mario held at screen x 232 (no cling, no crush), waits, lands on both scale platforms (H) | `r8_autoscroll_edge.png` |
| Layer-2 walls | `layer2_pillar` (VoB2) | the jump that crushed him with life 10 (blocked 0x1D) now stops at the pillar (no 0x1C, state 0); after 50 frames' wait a jump lands on the sinking pillar (x 338); 3 of 5 agent timings crashed before, 0 of 6 after (H) | `r8_layer2_pillar.png` |

Agent play-testing (`tools/scripts/play.py`, guide `tools/play_guide.md`): wave 1 (agents p01-p20, every level) and
wave 2 (q01-q19, the 74 levels wave 1 didn't beat, on the fixed build). Reports in `work/play/reports/`, collected
in `work/play/all_issues.txt` and `all_issues_wave2.txt` (about 440 issue reports). Each candidate was replayed on
Opus with `replay_session.py` (compared with SMW Mario by `toggle` at the same point where it mattered).

| report | verdict | fix |
|---|---|---|
| Can't jump onto 3-tile ledges / YI1 pipe; walls stop the rise | real (H) | `Resolve`: walls hidden while rising |
| Running jumps too low / short | real (H) | take-off scaled by SMW's run table |
| "Pushed sideways" at walls | real: held jump wall-kicked (H) | `Keys`: kick only on a fresh press |
| Up jumps at doors | real (H) | `InFrontOfDoor`; P-switch doors added from wave 2 (M) |
| Life refilled at pipes / doors | real (H) | `g_level_fresh` |
| Crush death beside the Yellow Switch Palace switch | real (H) | `ActsLike` (EC-FB = 132, P-switch swaps) |
| MC Mario invisible in the Sunken Ghost Ship | real (H) | `subz` in `McOverlay_ApplyLine` |
| Butter Bridge 1: sinking past the mushroom caps | real: wall cling on the auto-scroll edge, crush past it (H) | `Resolve` (flag 0x80), `ClampIntoScreen` |
| VoB2: death with full life at a yellow pillar | real: crushed inside the layer-2 pillar (H) | layer 2 in MC's tile tests |
| `--every` changes the result (tool) | real (H) | `play.py`: a shot takes the last frame of its stretch |
| Gnarly rope column, Front Door door 2, Vanilla Secret 2 posts (midway gate) | same for SMW Mario (H) | - |
| Falls through bridges / ledges / mushroom tops (004, 107, 005, 119, 135) | reached from the side below the top; a jump lands (004: x 1151 on the bridge) (H) | - |
| Hanging at walls / pipes (006, 010) | MC's wall cling (pose 17, grav 2) (H) | - |
| YI4 "hangs" over the ledge | swimming (SMW puppet), sinks without B (H) | - |
| Lemmy +50 coins / thrown up | score from a turn block, the attack's own rise (H) | - |
| Blue "water" kills (Lemmy), Wendy / CI4 shafts, Back Door pit | lava / pits, same for SMW (H) | - |
| Vanilla Dome 1 floating mushroom | on the upper of two ledge rows (0x100) (H) | - |
| CI4 "feather" in the item box | carrot-top lift (B7/B8) drawn over the HUD (H) | - |
| Dotted blocks wall (YI3), x frozen (CI5) | death freezes x; pressed against a wall (H) | - |

## Round 7 (brief `PROJECT_BRIEF.md`, round 7)
Core traces: 7/7 PASS. `regress.ps1`: the 43 earlier pilots give the same summaries as round 6 (logs kept in
`work\pilot\regress_r6`), plus 12 round-7 pilots (`r7_regress_summary.txt`). Cheat sweep: 260/260 OK (22 cheats, every
pair; the only deaths are One-hit runs) (`r7_cheat_sweep_summary.csv`). Level 105 of the sweep route has no Koopas, so
King Koopas is checked by its own pilots. Sprite sweep: the same 18 explained differences as rounds 5-6
(`r7_sprite_sweep_report.txt`). Original-vs-C (MC off, boot to YI1 and a run through it, including a death): 0 mismatches
(`r7_original_vs_c_0_mismatches.png`). `package.ps1`: smoke exit 0 (`r7_packaged_smoke_test.png`).

| check | pilot | result | screenshot |
|---|---|---|---|
| Flashing (slow motion / mirror / menu) | `rerender_test` (`shotre`: the frame drawn again without running the game) | before: 1371 px differ (6357 mirrored), status bar lost; after (`RtlDrawPpuFrame` shows the kept last real frame): 0 px (H) | `r7_rerender_before.png`, `r7_rerender_after.png` |
| King Koopas script | `king_koopas` | breath / fire phases, a flame each time attackSpeed hits 1 (wavy path), after attack1 = 500 ticks five jumps toward Mario with a shockwave on each landing, then pant 300 ticks | `r7_king_koopas.png`, `r7_king_koopas_zoom.png`, `r7_bowser_export_sheet.png` |
| Hits on Bowser | `king_koopas_hits`, `king_koopas_super`, `king_koopas_shell` | MC jab -1 (hurt clip, knockback); super punch: life 0, 1-UP, falls through the floor; fireball 99; cape 98; stomp -3 x2; kicked shell -4 once (shell slides on); star -4 per 24 frames; Yoshi's tongue: nothing (inedible); poked below the level: teleport to mid-screen, lands | `r7_king_koopas_hits.png`, `r7_king_koopas_super_punch.png`, `r7_king_koopas_shell.png` |
| Bowser's attacks | `king_koopas_damage` | flame on MC Mario: life 10 -> 7.5, knocked back; on big SMW Mario: power-down | `r7_king_koopas_damage.png` |
| In a level | `king_koopas_yi2` | YI2's eight Koopas become eight Bowsers on the ledge, breathing fire; nothing stuck | `r7_king_koopas_yi2.png` |
| Mirror + slow motion | `king_koopas_mirror` | Bowser, flames and bar mirrored with the level, script at half speed | `r7_king_koopas_mirror_slowmo.png` |
| Interactions audit | `mc_enemies_yoshi`, `mc_enemies_pause_yoshi`, `mc_enemies_despawn`, `mc_koopas*` | see the list below | `r7_mc_enemies_*.png` |
| Double jump / ground pound | `cheats_r7_moves` | SMW Mario: second jump to y 231 (first 291); MC Mario likewise; pound lands with SMW's cape-dive pound: both Goombas flipped (status 9), 100 points each, for both Marios | `r7_double_jump_ground_pound.png` |
| Wall jump | `cheats_r7_walljump` (level 004, the wall at x 1280) | slide (y speed held at 42, blocked right), kick: x speed -32, y speed -66 | `r7_wall_jump.png` |
| Menu | `menu_r7` | description in GFX2A message font, four cheat pages, King/MC Koopas notice in the panel | `r7_menu_pages.png`, `r7_menu_description_zoom.png` |

Interaction audit, issues found and fixed (round 7):
- Pause reset the MC enemies: SMW runs no sprites while paused, and the "new sprite" check counted every frame. Now
  sprite-processing frames, plus a reset in `SprStatus01_Init` (pilot: life 98 kept across a 120-frame pause).
- MC Koopas never had SMW's off-screen flags set (only `GetDrawInfo` sets them), so `SubOffscreen` could not despawn
  them (found by reading the code; the fix is checked by `mc_enemies_despawn`).
- Hits on Mario riding Yoshi powered him down: now `McSmw_EnemyHurtsPlayer` (SMW's enemy-fire rule: lose Yoshi).
- Riding Yoshi, the hero box ended at Mario's feet, so an MC Koopa walking up was "stomped": the box now reaches
  Yoshi's feet and a stomp needs Mario in the air.
- Spin jump / Yoshi on an MC Koopa: SMW's spin kill. Slide: knocked out (Bowser: strong hit). MC Koopa in lava: MC
  death instead of SMW's Koopa sinking.
- Bowser's 2-second flame sound from eight Bowsers at once: at most one start per 20 frames per sound.
## Round 6 (brief `PROJECT_BRIEF.md`, round 6)
Core traces: 7/7 PASS. `regress.ps1`: 43 pilots (rounds 2–6), all as before. Cheat sweep: 178/178 OK (18 cheats; mirrored
runs press Left). Sprite sweep: the same 18 explained differences as round 5 (`r6_sprite_sweep_report.txt`). Original-vs-C
(MC off): 0 mismatches (`r6_original_vs_c_0_mismatches.png`). `package.ps1`: smoke exit 0 (`r6_packaged_smoke_test.png`).

| check | pilot | result | screenshot |
|---|---|---|---|
| Cape spin on X | `cape_spin` | Goomba and Koopa beside Mario flipped (status 9) with points; SMW's spin poses; air spin too. Cause: `timer_active_cape_spin` was set but only `SetPlayerPose_00CF0A` places the cape hit box (`InitializeCapeSwingOrNetPunch`), and MC mode ran only `UpdatePlayerCapeImage` | `r6_cape_spin.png` |
| Air attack against walls | `air_kick_bricks` (column of turn blocks poked into Yoshi's House) | kick keeps frame 18 for 30+ frames while pressed against the column; brick at y 304 shattered. Glue: walls hidden from the core while fighting in the air (`Resolve`, `Hit`) | `r6_air_kick_bricks.png` |
| Hit chain | `hit_chain` | Koopa → shell → shell: 200, 400, 800 (score 1400), rising stomp sounds. `McSmw_PointsOverride` makes the punch's `GivePoints` use the chain step | `r6_hit_chain.png` |
| ! block with item | `excl_blocks` (16A poked into YI1, standing jab, no jump) | 16A → 132 (used), feather out. Yoshi's House (tileset 4) has no ! blocks: the 0x16A path is tileset-gated | `r6_exclamation_block.png` |
| Flying ? block | `flying_qblock` | punched → used block, coin | `r6_flying_question_block.png` |
| Triple enemies audit | `tools/scripts/triple_audit.py` (all 512 level numbers, `scroll` pilot command at 3 px/frame) | run 1: 1334/2360 (56.5%); run 3 (terrain, pipe sharing, 2-D terrain grid, wider list): 1538/2632 (58.4%). Remaining: SMW's per-level slot limit ("no slot"), Banzai Bill / Big Boo one reserved slot, curved lines | `r6_triple_audit_summary.txt`, `r6_triple_terrain.png` |
| Mario Combat Koopas | `mc_koopas`, `mc_koopas_yi2`, `mc_koopas_ledge`, `mc_koopas_kills` | walk, punch Mario (life 10 → 7.5), take hits (10 → 4 after 6 jabs), KO tumble + fall through the floor (MC's way), 200 points; SMW Mario stomps chain 200–1000. `mc_koopas_kills` (SMW Mario): fireball → coin (sprite 21), cape → MC tumble, star → knocked out, kicked shell → MC tumble, Yoshi's tongue → swallowed (status 7) | `r6_mc_koopas.png`, `r6_mc_koopas_zoom.png`, `r6_mc_koopas_yi2.png`, `r6_mc_koopas_vs_smw.png`, `r6_mc_koopa_export_sheet.png` |
| Mirror / One-hit / Coins | `cheats_r6` | level mirrored with BG3 unflipped (status bar, life bar readable, no seam); Right moves to screen right. One touch kills. Punched Goomba (stunned, status 9) pops a coin (+1) | `r6_new_cheats.png`, `r6_mirror.png` |
| Menu | `menu_r6` | new rows on MARIO / WORLD pages; POWER-UP shows SMW's fire flower (GFX00 tiles 26/27/36/37, object palette row 13) | `r6_menu_pages.png`, `r6_menu_item_icon.png` |

Found while testing (fixed): MC Koopa hits ran the chain to 1-UPs (only the KO counts now); coins missed stunned kills;
mirror v1 flipped the finished frame (status bar mirrored, seam); the MC Koopa ledge probe saw cliffs everywhere (SMW dirt
is a page-0 fill tile).


Timing model confirmed by the traces (H):
- clips run newest-first;
- onEnterFrame is queued before the playhead advance;
- actions run after all clips advance;
- goto-target frame scripts run at the end of the queue;
- new clips run frame 1 immediately;
- goto keeps a child if the same placement exists at that depth.
