# Project brief: Mario Combat's Mario inside Super Mario World (snesrev/smw)

One brief for the whole project. It merges the round-1 brief and the round 2-8 briefs; rounds 9-16 were requests in
chat and are summarised here too. What was built and verified in each round is in `HANDOFF.md` and
`notes/verification.md`; the reverse-engineering knowledge is in `notes/DATA_NOTES.md`.

## Goal
Replace the playable Mario in the snesrev/smw C reimplementation of *Super Mario World* with the Mario of the Flash game
*Mario Combat* (MC). His movement, moves, animation, timing and look match Mario Combat as closely as the SMW world
allows, and he must look and behave as if he belonged in SMW everywhere: every level, enemy, object, tile, Yoshi, vines,
cape, bosses. On top of that: a character toggle (MC Mario <-> SMW Mario), an in-game settings menu, cheats, a level
select and a boss rush. The result is a portable Windows game in `finished\` that runs on any Windows 10/11 x64 PC once
the player adds their own *Super Mario World* (USA) ROM.

## Standing rules
1. **Everything lives in the project folder (the repository's root).** Tools in `tools\` (put them on PATH with
   `. .\tools\env.ps1`), games and builds in `games\`, deliverables in `finished\`, scratch and test output in
   `work\`. Anything that touches the machine outside this folder is logged in `OUTSIDE_FOLDER_CHANGES.txt`.
2. **Reverse engineering with the right tool per layer** (round 1): `MarioCombat.exe` is a Flash Player projector
   (native runtime + appended SWF). Ghidra / ghidra-mcp and rea on the native exe (container format, SWF trailer,
   player version, frame rate); the SWF carved out and its ActionScript decompiled with JPEXS FFDec, which holds the
   real rules.
3. **Verbatim Mario Combat** (`ROM_REMIX_ONBOARDING.md` §1):
   - MC's own sprite frames, timelines, physics constants and move timings, never redrawn, invented or guessed.
   - Art, timelines and numbers come from the SWF only through the exporters in `tools\scripts\` (never hand-edited
     data: `src/mc/mc_data.c` and the other generated files).
   - The ported AS2 (`src/mc/mc_core.c`) keeps the original statement order; only API functions may be appended.
     Behaviour changes go in the glue (`mc_smw.c` and friends). The 7 reference traces must print `RESULT PASS`.
   - A number changed because the user asked for it (e.g. Bowser's health) is marked as such in the code.
   - Every constant goes into `notes/DATA_NOTES.md` with its source and a confidence label (H verified, M strong
     guess, L hunch).
4. **No ROM distribution** (onboarding §2). The ROM is never copied into `finished\`, downloaded or published, and
   nothing derived from it is delivered: the player supplies it and `smw_assets.bps` rebuilds the assets from it.
   `finished\` is also the player's play folder: packaging replaces only the delivered files and leaves the
   player's ROM, `smw.user.ini` and `saves\` alone.
5. **Readability and functionality come first.** Anything on screen looks as if it shipped with SMW: its fonts
   (menu GFX28, message box GFX2A), message-box panels, colours, icons and other game assets where possible (not
   required). When unsure, do what looks most like the game and works best.
6. **Cheats change gameplay only**: no screen effects, no slow-down (the user's rule since round 12; the older Slow
   motion and Mirror mode cheats stay). A new cheat must be substantially different from the existing ones.
7. **Prove it with evidence.**
   - Pilots (scripted input playback, `tools\pilots\`, `tools\scripts\run_pilot.ps1`) reproduce every bug before the
     fix and show it fixed after; sweeps and tools cover what pilots can't.
   - Look at every screenshot and contact sheet before calling anything done.
   - Movement compared numerically against the original (reference captures of the real Flash runtime, `mc_test
     --oracle`); the original-vs-C comparison shows 0 mismatches with MC off.
8. **Test hygiene.** Pilots never write `smw.user.ini` (delete any hand-made one); restore
   `games\SuperMarioWorld\saves\smw.srm` from `work\saves_backup_r3` after tests and remove `file3_backup.srm`.
9. **Docs stay current**: the player README (source `tools\package\README.txt`; `finished\` gets a copy),
   `HANDOFF.md`, `notes/verification.md`, `notes/DATA_NOTES.md`, `OUTSIDE_FOLDER_CHANGES.txt`. There is no issues
   list any more (`ISSUES_FIXED.txt` was retired in round 15 at the user's request).
10. **Prompts.** When the user asks, restate their request as a more detailed prompt first, then work from it.

## Definition of done (every round)
- Each item verified with pilots / tools and screenshots under `notes/verification/` (`rN_*`).
- `tools\scripts\regress.ps1` gives unchanged summaries for the older pilots; the cheat sweep is all OK; the 7 core
  traces pass.
- `tools\package.ps1` rebuilds `finished\` and its smoke test from a clean copy passes.
- Docs updated; saves restored; nothing left outside the folder unlogged.

## What each round asked for
**Round 1: the port.** Tooling under `tools\` (MinGit, w64devkit gcc + SDL2, Ghidra 12.1.4 + GhidraMCP, Node + rea,
FFDec, uv). snesrev/smw cloned into `games\SuperMarioWorld\` and built unmodified first. Projector RE, SWF carved and
decompiled; Mario's movie clip, input, physics (walk / run acceleration, friction, speeds, jump impulse, variable jump,
gravity, air control) and every move with frame data and hit boxes; SWF frame rate and stage scale. Every animation
frame rendered at native size with its origin. The port: MC rules in SMW units (60 Hz vs the SWF's rate, pixels to
subpixels, a scale where MC Mario's body matches SMW's 16x32 hitbox), SMW's collision, enemies, power-ups, damage,
death, pipes, goal and overworld kept; MC attacks hurt SMW enemies; MC Mario drawn over the frame at the player's place
with SMW's own Mario hidden; behind a switch in `smw.ini`. Done: measured movement within one frame / one pixel of the
AS-derived values, levels completable, README lists every control.

**Round 2: fixes, toggle, menu.** From the user's video: MC Mario clipping into block walls and dying wedged; blocks
hit from below re-triggering; punches missing Rex and Banzai Bill (bigger, padded hit areas, MC timing kept). Character
toggle (gamepad R, a keyboard key; every level starts as MC Mario). An in-game settings menu in SMW's style (video,
sound, gameplay, controls with key / button capture; saved to `smw.user.ini`; keyboard-only or gamepad-only use).
Special states verified: Yoshi, pipes, swimming, vines / nets, cape flight, goal walk, a castle boss.

**Round 3: SMW mechanics.** The YI1 bonus-room pipe leading to a glitch area (checked against snes9x); sideways pipes for
MC Mario; running (Y / X held, P-meter, MC's run animation; a tap still punches); carrying items; punches breaking and
hitting blocks through SMW's own block code; power-up colours on MC's art; quieter MC sounds; a crisp life-bar label;
F3 FPS and F11 fullscreen hotkeys; a 100% save in file 3 (SMW's own format, the player's file backed up); a CHEATS
page of at least 8 cheats (no walking through walls).

**Round 4: graphics and enemies.** Fire-colour outline fixed; white grass tops and other differences from the original
renderer audited away; MC Mario's sprite order against other sprites matching SMW's OAM order; multi-hit enemies (a
normal punch acts like a stomp, strong attacks knock out); a replay-castles cheat; SMW's cape drawn on MC Mario; a
pairwise cheat-combination sweep.

**Round 5: belonging everywhere.** No hue-shifted pixels in any palette; SMW's own riding and climbing poses for Yoshi
and vines / nets; cape-flight poses; power-ups heal the life bar; an automated interaction sweep of every sprite and
tile class for both Marios; cheats Triple enemies, No knockback, Super punch, Start on Yoshi, Slow motion; cheat pages.

**Round 6: more mechanics.** Cape spin on X; the air kick not cut short by walls; SMW score chains for hit chains; !
blocks and the other hittable blocks; Triple enemies complete (audit of every level, copies in matching terrain);
the Mario Combat Koopas cheat (MC's own enemy, DefineSprite 163, exported and ported line by line); Mirror mode,
One-hit mode, Coins from enemies; menu icons from the game.

**Round 7: King Koopas.** Screen flashing with Slow motion / Mirror fixed (present the last real frame); the King
Koopas cheat (MC's final boss, DefineSprite 532, with fireballs and shockwaves, exclusive with MC Koopas; decisions:
not immune to SMW Mario's fireballs and cape, teleports to the screen's middle when falling off); every interaction of
both Marios with the MC enemies tested; Double jump, Ground pound, Wall jump; option descriptions in the menu.

**Round 8: every level played.** Bowser's health 100 -> 50 (user's override, fire-rate steps scaled); an interactive
pilot and `play.py` so cheap subagents (Haiku) played and beat every level as MC Mario and reported problems, each
report re-checked on Opus by replay, real ones fixed; Slippery levels, Coin magnet, Flying Yoshi.

**Round 9:** MC hit reactions for SMW's enemies (MC damage, life 10, MC knockback), Life steal, Fire punch.
**Round 10:** Instant hits, Block buster, Pit rescue.
**Round 11:** Infinite jumps, a ground-pound animation, Open map, Stomp anything.
**Round 12:** a knockback scale for MC Mario (1x-20x), enemy-changing cheats (Fast, Chasing, Jumping enemies), more
gameplay-only cheats.
**Round 13:** more gameplay-only cheats (Low gravity, Air dash, Respawning enemies) and new settings unlike the others
(no screen filters): mono / stereo, in background, rumble, stick deadzone.
**Round 14:** a LEVEL SELECT page: every level from its beginning (also unused ones, marked), ordered by world in play
order, then bonus and unused levels; practice runs that leave the map and save as they were; tested extensively,
including switching levels from inside a level.
**Round 15:** MC punches hurt every boss as one of SMW's own hits; enemies SMW Mario can carry after a stomp fall over
instead of dying; MC Mario standing still on ice; jumping enemies on every jump; Chucks and moles flung by MC hit
reactions; the air dash into pipe sides (SMW Mario's death, MC Mario's pipe entry); a BOSS RUSH in the level select
(Koopa Kids + Bowser or every boss; retry or end on death; timed; practice). No more `ISSUES_FIXED.txt`.
**Round 16:** the Mario Combat logo with a "+" under SMW's title logo; boss rush fights and level select starts from
sub-areas broken by state left from the previous room (tilted Bowser, Koopa Kids stuck on walls, a door scene after
Big Boo, a locked camera); a bug hunt over moving between levels, rooms and bosses. Then a cleanup of bloat and this
merge of the briefs.
**Round 17:** MC Mario walks into ghost houses and castles instead of floating in; an air attack against a block is
no longer stopped.
**Round 18:** wall jumps off flying ? blocks (hit or not) and other solid sprites; coins a P-switch made blocks stay
coins after MC Mario touches them; punches hurt Morton, Roy and Ludwig (every boss re-checked); boss rush music in
every fight; Bowser takes two MC punches per Mechakoopa hit (Mechakoopas unchanged); one Reznor fight in the boss
rush; Reznor's rotating platform and every other moving platform; a "punch anything (MC)" cheat for the enemies only
a star defeats.
**Round 19:** punch anything also hurts Pokeys and the other ordinary enemies a punch couldn't; MC Mario on every kind
of moving platform and block, documented (`notes/platforms.md`), with the jump off ropes, vines, nets and Yoshi fixed;
three unedited mp4 videos in `finished\`, recorded without touching the game code: a boss rush (every boss) speedrun
and a final level speedrun (Back Door) as MC Mario with no cheats, deaths or hits, and a 20+ hit combo on one enemy
(cheats allowed).
**Round 20:** LEVEL SELECT > MARIO COMBAT: Mario Combat's whole story level (its five areas, from the wall over the
lava to Bowser's bridge) as one SMW level, for MC Mario and SMW Mario. It looks like MC: MC's own pictures of the areas
(ground, Bowser statues, pillars, door, pipe, castle wall, sky) drawn instead of SMW's tiles, which stay as the
collision; SMW's Koopas (turned into MC's), music and room changes; MC's enemies, exit arrow and lift. Where MC asks for Space, Jump, Punch
or Up. MC Mario takes MC's own damage (0.5 of 10 per hit: 20 hits). Play-tested and beaten (Bowser defeated) with
both Marios. The videos are no longer kept in `finished\` (the player removed them).
