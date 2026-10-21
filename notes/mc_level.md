# Mario Combat's own level inside SMW (round 20)

LEVEL SELECT > MARIO COMBAT plays Mario Combat's story mode (the SWF's root frames 486-496) as one SMW level in five
areas, for both Marios, and ends when MC's Bowser falls. Confidence labels as elsewhere in `notes/` (H = checked in
the SWF or by a run).

## Mario Combat's story mode (H, `work/ffdec/scripts`, `work/swf/timelines.json`)
| root frame | area | what is there | how it is left |
|---|---|---|---|
| 486 | 1 | castle wall over lava: a floor, stairs, three pillars, the floor to the castle | exit marker (sprite 445) at the castle: on it, on the ground, not hurt, Space -> `hud.fader` "nextLvl" |
| 488 | 2 | inside: a shaft, a big block, brown block climbs, the red door (476) on a ledge | the marker at the door |
| 490 | 3 | the hall: platforms over lava, ceiling spikes, chains, stairs, the grey pipe | the marker over the pipe |
| 492 | 4 | stairs, the lift (sprite 492 in the ground clip 495, 315 frames: up 640 stage px), two caged Koopas up top | Space on the marker starts the lift; at its top (frame 316) the cages and floor go (the ground's depth 1 is removed), the Koopas drop; when Koopa 26 falls below y -110 (knocked off or defeated: a defeated MC Koopa falls through everything) the fader moves on |
| 496 | 5 | Bowser's bridge between two pillars (sprite 532, flames 555, shockwave 558) | Bowser's death: `fader` "nextLvl" (the score screen, 497-498) |

Every area: `ground` (invisible, the hero's `hitTest` shape: some fills have alpha 0), `spawnPoint` (the hero's feet,
usually a few px inside the ground), `lScroll` / `rScroll` (the camera's limits), `deadZone`. Koopas are sprite 163
(life 10, the hero's damage per hit 0.5), the hero's life 10. The marker (445) is an invisible box (441) with a
bobbing white arrow (443) and the word SPACE (444), shown while the hero stands in it.

## The port
- **Pictures and grids** (`tools/scripts/mc_level_render.py`): each area at SNES scale (1 px = 3 stage px), its
  ground as a mask (every fill opaque, as `hitTest` sees it), and the marks (Koopas, markers, spawn, scroll limits).
- **Tiles** (`tools/scripts/mc_level_export.py` -> `src/mc/mc_level_data.c`): the mask in 16 x 16 cells (solid at
  half covered, at the grid offset that cuts the fewest edges), 27 rows (SMW's horizontal level), whole screens from
  `lScroll`. Air the hero can't reach (MC draws walls as outlines) is filled. Kinds from MC's picture: stone, brown
  block (R-G about G-B: MC's tan blocks; lava glow is R-G far bigger), lava (red, from the bottom row up), spikes
  (listed). SMW tiles (castle tileset, base room 007 Morton's Castle), the collision only (not shown): stone = the
  castle's 3x3 block set 15D-165 (each MC block outlined), brown = used block 132, lava 004 / 005, spikes 15A (the
  castle's spikes, hurt tiles in tileset 1: 00F120), door 1F / 20, pipe 137 / 138 over 135 / 136.
- **Changes to MC's layout** (round 20 follow-ups): area 3 starts 2 columns before the grid's first (MC's camera shows a
  little left of its lScroll: its first pillar); its two chain-hung slabs are one-way ledge tiles (map16 100: stood on
  from above, passed from below), MC's slab drawn on them (its ends, its middle repeated) under MC's chains (the first
  isn't in MC's hitTest shape, but MC's Mario stands on it in the game); area 5's right pillar is cut to the bridge's
  height (both ends can be fallen off, as in MC).
- **Lava**: SMW's tiles 0-5 are water (Mario swam in MC's lava): the level has air where the area's table has lava
  (004 / 005) and `InLava` kills Mario there (both Marios), as SMW's lava and MC's deadZone do. MC's lava surface
  (sprite 141: six shapes, five MC frames each) is drawn by the game from the table, animated (`McLevel_ArtLine`).
- **Bowser** is never erased off screen in the level (`SubOffscreen` skipped): walking away doesn't reload him at full
  life. SMW Mario hurts him only with a spin jump (or from Yoshi); a plain jump bounces off (sound 1, no damage).
  MC Mario has no stomp (as in MC): landing on Bowser does nothing (the bounce, applied to him at first, held him on
  Bowser's head under his flames: the runner's search stalled there).
- **Edges, as MC's** (follow-up 4): MC pushes the hero back inside `lScroll + 22` / `rScroll - 22` and its camera
  never shows past lScroll / rScroll. The exporter writes both per area (`McLevelArea` cam_x0 / cam_x1, wall_x0 /
  wall_x1; area 3's left camera limit 0, for its first pillar); `McLevel_BuildArea` puts invisible solid columns (tile
  130 in the air cells, every row) just outside the hero's limits, and `McLevel_ClampCamera` (SMW's camera scroll,
  `smw_00.c`) keeps the camera inside the camera limits, so nothing past MC's picture is ever on screen. Area 5's
  pillars stay open on their outer sides (the walls stand inside the camera only), so both Marios and Bowser can still
  fall off there.
- **Camera on walls**: SMW's camera scrolls up with an airborne player only while `camera_bounce_off_spring_flag` is
  set (SMW Mario's wall jump sets it in `lm.c`); MC Mario's wall cling sets it too (`mc_smw.c`, cleared on landing),
  so the camera follows his wall jumps everywhere.
- **Lava glow** (follow-up 4): MC's gradient (shapes 128 / 486: red, alpha 255 -> 0, centred on the surface): half
  alpha at the surface fading out over 13 SNES px, drawn by `McLevel_ArtLine` per column from the lava's surface
  (`FindLavaTops`; pillars standing in a pool take the pool's), over the ground and the picture (it was baked into the
  picture, too strong and too tall).
- **Cape**: SMW Mario's cape gives MC Koopas and Bowser 32 frames of invulnerability per hit (`$154C`,
  `kMcCapeInvincible`), so the cape can't shred them; MC Mario's punches are untouched.
- **Fitting 27 rows**: area 2 moved down 2 rows (its door was under SMW's status bar, which covers the top 32 px of
  the screen at the level's top), area 4 compressed: the lift rises 144 px (MC: 213) and the Koopas drop 80 px.
- **SMW Mario**: MC's climbs are made for MC Mario's wall jumps and higher jump (about 98 px vs SMW's 70). In this
  level SMW Mario has the wall jump (the WALL JUMP cheat's, `Cheats_WallJump`), and area 2 has two brown blocks MC
  doesn't (helpers: steps on to the floating platform and off it).
- **Enemies**: the areas' Koopas are SMW Koopas (sprite 04) at MC's places (a sprite list of the level's own,
  `GetSpriteListPtr`) that the MC Koopa code takes over (`McKoopa_Enabled` while the level is on); area 5's is MC's
  Bowser (`McBowser_Enabled`). The cheats MC KOOPAS / KING KOOPAS don't matter there.
- **Exits**: SMW's own door and pipe (every screen's exit leads to the next area's room), and as in MC Jump, Punch
  or Up on them (`Pressed`): MC's arrow bobs over the exit while Mario stands at it. The lift: Jump / Punch / Up on
  its marker starts it; it rises as MC's (378 frames = 315 MC ticks), MC's brown blocks, a platform for both
  Marios (`McLevel_SpritePhase`: SMW's on-solid-sprite, and MC's landing box `McSmw_LevelPlatformBox`); at the top it
  is blocks again, then the ledge with the cages goes and the Koopas drop. The next area when fewer than two Koopas
  stand on it (one knocked off or defeated). MC's fall from the lift is the end of the run; here the lift comes back
  down for Mario (he fell off on the way up, or stands on the floor 90 frames while it waits at the top) and Jump /
  Punch / Up on the marker sends it up again (the cages stay open). SMW Mario is carried as by SMW's platforms: the
  on-sprite flag is set at the start of `ProcessNormalSprites`, after SMW clears it (set earlier, it was lost and he
  slid off the lift's end with his running speed).
- **Music**: areas 1-4 SMW's castle music (the base room's). Area 5 MC's Bowser music (sound 560, which MC starts on
  Bowser's frame 496; 194 s, looped here): SMW's music fades during the load and none starts (the room's music register
  marked as continuing), MC's starts with the area (`McAudio_StartMusic`: 4-bit IMA ADPCM from
  `tools/scripts/mc_music_export.py` -> `src/mc/mc_music.c`, decoded as it plays, at -12 dB to match SMW's music, at
  the game's volume, not SoundVolume), fades when Bowser falls (SMW's boss-clear tune follows), stops on a death,
  leaving or the end.
- **Deaths**: the area again (a life less; no lives: back to the map). **The end**: Bowser down, SMW's boss tune,
  "MARIO COMBAT CLEAR!", back to the map (a practice run: the map and save as they were).
- **The look: MC's own art, on the grid** (`tools/scripts/mc_level_art.py` -> `src/mc/mc_level_art.c`,
  `McLevel_ArtLine`). SMW's layers 1 and 2 are off in the level (`McLevel_BuildArea`); after the PPU composes each line,
  every pixel still showing the backdrop (z-buffer 0x0500: no BG, sprite or status bar pixel) is drawn, at BG1's scroll
  position, in three layers:
  1. the ground, from the tiles at runtime, so it is always exactly where the collision is (also where tiles come and
     go: area 4's ledge, cages, lift): MC's stone weave (a 16 x 16 tile of the area's own, which repeats every 16 px)
     framed by MC's 8 px bevel on every side that meets no stone (its exact pixel-art colours, measured in MC's
     pictures; the right side's band shades the other way), rounded at outer corners, turned at inner ones (each
     quarter tile looks at its two neighbours and the diagonal); MC's brown block (MC's pixel art: a 14 x 14 bevelled
     block, 1 px seams, 15 px apart, a 1 px dark outline round each group; here the block one flat row and column
     wider for 16 px, the seam on the tile's top and left, the outline on a group's open right and bottom);
  2. a picture of the area: MC's sky and clouds (areas 1, 5; area 5's fog) as MC draws them; MC's props taken out of
     its pictures (MC's ground removed by its hitTest shape and, in big pieces, by its exact palette; its lava and
     spikes art too) and snapped onto the tile ground they stand on (or the ceiling they hang from): Bowser statues,
     pillars, chains, the castle; the red door centred on its door tiles; MC's lava (surface sprite 141 on the top lava
     tiles, its colour, a glow above), spikes (two to a tile), the pipe (rim and body trimmed to the tiles' 32 px) and
     the bridge (its plank-and-brick strip, tiles 130) drawn on their own tiles;
  3. behind: the sky's colour per row, or MC's castle wall, locked to the screen as MC's BG clip (469 / 489: the camera
     script sets `_root.BG` to its own position every frame), screen-wide, repeating every copy (BG 489 holds two,
     400 stage px apart); area 4's scrolls one copy down while the lift rises, as MC's plays.
  The first try drew MC's whole pictures over the tile grid: MC's shapes are not on a 16 px grid (its brown blocks are
  15.5 px), so edges stood up to 8 px off the collision and the filler tiles didn't match MC's own blocks.
## Testing
`tools/scripts/tas_backtrack.py <session> <checkpoint> mc_runner --mcl` plays an area: waypoints per area
(`MC_WAYPOINTS`, snapped to the ground in mc_level_data.c.json), jumps walls and gaps, wall-jumps up shafts, punches
(SMW Mario: stomps) Koopas, presses at the exits; a hit or a death goes back a few steps. SMW Mario runs (X) only to
far waypoints, and spin-jumps onto MC Bowser, steered over him in the air (stomps are not boss punches: that area
needs a long `--stall`). Sessions `mcl` (MC Mario,
base `tools/tas/mclevel_base.txt`) and `mcls` (SMW Mario, `mclevel_smw_base.txt`), chained by
`tools/scripts/mclevel_chain.ps1`.
