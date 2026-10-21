# DATA_NOTES: Mario Combat reverse-engineering knowledge base

Confidence labels:
- **H**: verified by code or by measurement.
- **M**: strong guess.
- **L**: hunch.

## 1. Container: `games/MarioCombat/MarioCombat.exe`
- It is a Flash Player *projector* (H). The layout is:
  ```
  <Flash Player stub exe, 15,983,672 bytes><SWF><u32 0xFA123456><u32 swf_len>
  ```
- The SWF starts at offset 15,983,672 and is 3,934,211 bytes long (H).
- `CWS` means zlib-compressed. SWF version 10; it decompresses to 5,559,220 bytes (H).
- The stage is 700×400 px at **50 fps** with 499 root frames (H). Source: `tools/scripts/carve_swf.py` → `work/swf/swf_info.json`.
- The code is AS1/AS2 (AVM1) (H). FFDec export: `work/ffdec/scripts/`. There's no AS3.
- The art is mostly bitmaps: 132 DefineBitsLossless2 and 1 DefineBitsLossless (H).

## 2. Hero = DefineSprite 440 (instance `_root.hero`)
Its timeline has 19 frames. Labels (H):

| label | frame | child at depth 3 | frame script |
|---|---|---|---|
| idle | 1 | 171 (scale 2.98) | defines onEnterFrame (no stop) |
| walk | 2–7 | 192 `walk` | f2 `play()`, f7 `gotoAndStop(1)` |
| skid | 8 | 194 | `stop()` |
| jump | 9 | 195 | `stop()` |
| run | 10–16 | 202 `walk` (x-mirrored, -1 scale) | f10 `play()`, f16 `gotoAndStop(1)` |
| wall | 17 | 204 | `stop()` |
| attack | 18 | 432 `attack` | `fighting=true; stop()` |
| hurt | 19 | 439 `hurt_mc` | `stop()` |

Depth 1 always holds 165 `bound`.

### Init (onClipEvent(load)) (H)
- `xSpeed = 0`, `maxMoveSpeed = 10`, `maxAirMoveSpeed = 10`, `grav = 0`, `jump = 14`, `stopSliding = 10`, `shortHop = 0`, `life = 10`.
- Keys: Left = 37, Right = 39, Jump = 38 (the **Up arrow**), Down = 40, Punch = 65 (**A key**).

### Per-tick logic (onEnterFrame), in this order (H, `DefineSprite_440/frame_1/DoAction.as`)
1. `grav -= 0.5; _y -= grav` (y grows downward; `grav` > 0 means moving up).
2. Probes (`ground.hitTest`, shapeFlag true):
   - `onGround` = point (x, y+2);
   - `onLeftWall` = (x−22, y−20);
   - `onRightWall` = (x+22, y−20).
3. Push-outs:
   - while (x, y−1) is solid: `grav = 0; y--`;
   - while (x−20, y−20) is solid: `x++`, and `xSpeed = 0` if onGround or fighting;
   - mirror case for (x+20, y−20);
   - while (x, y−80) is solid: `grav = -2; y++`. This is the head; the **body is 80 px tall**.
4. Input. This only runs when `activated && life>0 && !walling && !fighting && !hurt`.
   - Left on the ground: if `xSpeed > -10`, then `xSpeed -= 2`, face left, and if not skidding play `run` when `xSpeed <= -10`, else `walk`.
   - Left in the air: if `xSpeed > -10`, then `xSpeed -= 0.5`.
   - Right is the mirror image.
5. Friction and move:
   - if `xSpeed > 0`: subtract 1 on the ground or 0.25 in the air, then `x += xSpeed`;
   - mirror case for `xSpeed < 0`.
6. Clamp the hero to the camera bounds (`lScroll`/`rScroll`) ±22. This is MC-only and not ported.
7. `stopSliding`: on frame 1 (idle) with `xSpeed != 0`, count down 10 ticks, then set `xSpeed = 0`. Any other frame resets it to 10.
8. Jump:
   - If Jump is held and `onGround && !hurt`: `grav = 14`, `jumpAnimThres = 5`, `shortHop = 5`.
   - `shortHop--` while > 0.
   - If `0 < shortHop < 5 && !onGround && !JumpHeld && !hurt`: `grav = 7`. This is the short hop.
   - `jumpAnimThres--`.
   - If `onGround && frame == 9 && jumpAnimThres == 0`: `gotoAndStop("idle")`.
   - If `!onGround && !hurt && !fighting && !walling`: `gotoAndStop("jump")`.
9. Wall, when `!onGround && !hurt`:
   - onLeftWall gives `x -= 5` and onRightWall gives `x += 5`. Each pulls the hero into the wall, and the push-out pushes back.
   - If `xSpeed < -3 && onLeftWall`: `xSpeed = 0`, `grav = 2`, `walling`, `gotoAndStop("wall")`, face right. Mirror case on the right.
   - While walling: if no wall, go to `jump` and clear walling. On a wall: `grav += 0.25`, and x moves 1 toward the wall.
   - Jump held while walling: `grav = 10`, `xSpeed = ±10` (away from the wall, in the facing direction).
   - `onGround && walling` goes to `idle`. If frame != 17, `walling = false`.
10. Skid, when on the ground and not fighting or hurt:
    - `xSpeed > 3` with Left held, or `xSpeed < -3` with Right held: `skiding = true`, `gotoAndStop("skid")`.
    - `xSpeed == 0` while skiding: `gotoAndStop(1)`.
    - Moving with the held direction while skiding: `gotoAndPlay("walk")`.
11. Fighting:
    - `fighting = (frame == 18)`.
    - On the ground and not `pressingAttack` (edge-triggered) and not hurt:
      - Punch with no L/R on frame 1: `gotoAndStop("attack")` (attack clip plays from 1 = `attack1`);
      - Punch with L/R and not fighting: attack, then `attack7`;
      - Punch with Down: attack, then `attack11` (if attack frame < 340).
    - In the air, frame 9 (jump), new Punch: `attack12`.
    - On the ground with attack frame 375–409: `gotoAndStop(1)`. Landing ends the air attack.
    - Attack frames 375–409 while holding forward: `xSpeed ±= 0.5` (up to max).
    - `pressingAttack = Key.isDown(Punch)`.
12. Air fighting: on a wall or with a probe at ±30 hitting, `gotoAndStop("jump")` and `xSpeed = 0`.
13. `hurt = (frame == 19)`. Death zone means respawn. `life <= 0` means `activated = false` and the hurt clip plays `die1`/`die2`.

### Derived movement numbers (H, computed from the rules above)
- **Ground top speed** alternates 10 and 9 px per tick (mean 9.5) once saturated. At 50 fps that's 475 px/s.
- **Jump**: initial `grav` 14, −0.5 per tick, apex about 14²/1 ≈ 200 px (exact value computed in `tools/scripts/mc_sim.py`).

## 3. Attack clip = DefineSprite 432 (instance `hero.attack`, 410 frames)
- Labels:
  - attack1 = 1, attack2 = 27, attack3 = 49, attack4 = 76, attack5 = 100, attack6 = 133, attack7 = 178, attack8 = 206;
  - attack9 = 220, attack10 = 248, attack102 = 263, attack103 = 278, attack104 = 293, attack105 = 308;
  - attack11 = 340, attack12 = 375.
- Each move ends with a frame script `_parent.gotoAndStop(1)`. attack12 ends with `gotoAndStop("jump")` (f410).

Impulses from frame scripts (H):

| frame | effect |
|---|---|
| 4 | `xSpeed ±= 5` (facing) |
| 30 | ±5 |
| 53 | ±7 |
| 78 | ±5 |
| 102 | ±5 |
| 111 | ±5 |
| 134 | ∓5 (backwards) |
| 146 | ±10 |
| 182 | ±10 |
| 208 | ±5 |
| 223 | ±10 |
| 248 | ±7 |
| 263, 278, 293 | `xSpeed = ±7` (set) |
| 315 | ±7 |
| 340 | `xSpeed −= 7` if moving left; the `xSpeed > 0` test uses a local var, so the right branch never fires (an original bug, kept) |

Hit-box frames (H, probed with the pilot `attackframes`, round 10): attack1 4 (hit1), attack2 30 (hit2), attack3 55
(hit3), attack4 80 (hit1), attack5 105 and 113 (hit1), attack6 148 (hit4), attack7 186 (hit2), attack8 209 (hit1),
attack9 230 (hit3), attack10 249, attack102 264, attack103 279, attack104 294 (hit1), attack105 324 (hit4), attack11
349 (hit5), attack12 385 (hit6). Each move's hit box is on one frame only (attack5: two). The combo windows open 0-5
frames after the hit (attack1's at 8 and 10, attack9's at 235).

Ends (`_parent.gotoAndStop(1)`) are at frames 26, 48, 75, 99, 132, 177, 205, 219, 247, 262, 277, 292, 307 and 339 (and 374 → see above).

The combo state machine is the attack clip's onEnterFrame (`DefineSprite_432/frame_1/DoAction.as`). It's edge-triggered on Punch, and the windows are on the attack clip's current frame. The direction flip on combo matches frame 27's script.

## 3b. Art, sound and scale (H)
- Hero art is SNES-style pixel art. Each art pixel is 3 stage px. The art comes in two forms:
  - Bitmaps pre-scaled 3×, placed at scale 1.
  - Vector-traced pixel art (or native bitmaps) placed at about 2.96–3.02.
- So **SCALE = 3: 1 SMW px = 3 MC stage px**. MC Mario at native size (about 17–25 × 28–32 px) is SMW Big Mario size.
- `tools/scripts/mc_export.py` rasterizes all 160 shapes of the hero tree at that scale, giving `src/mc/mc_data.c`
  with 16 sprite timelines and 436 ops.
- Sounds: 17 sounds (ADPCM 22/11 kHz) on attack-clip frames and on the hit-spark clips' frame 1. They are exported to
  `src/mc/mc_sounds.c`.
- HUD: DefineSprite 454 (bar 450, frame 452, DefineText 453 "SUPER MARIO", `mask_mc` 449 with `_xscale = life*10`).
  FFDec renders it at 0.5 into `src/mc/mc_hud_data.c`.

## 3c. Flash runtime facts (H, from the traces and Ghidra)
- Flash Player 32.0.0.465 projector; frames are on a 20 ms timer.
- Per tick:
  - clips run newest-first;
  - `onEnterFrame` is queued before the playhead advance;
  - the action queue runs after all clips have advanced;
  - goto-target DoActions and new clips' frame-1 scripts run at the end of the queue.
- A `gotoAndPlay`/`gotoAndStop` target frame's StartSound plays too. All combo label frames carry one.
- `hitTest(x, y, true)` rounds through the vcam (camera) transform, so resting heights vary by about ±1 px with zoom.
  This is MC-only and not reproduced; SMW collision is used instead.

## 5. SMW integration (`src/mc/mc_smw.c`)
**Clock**
- MC logic runs 5 ticks per 6 SMW frames (ticks start every 1.2 frames).
- Each tick's displacement (MC px) is spread over the frames it overlaps in time: speed in SMW 1/16 px per frame
  = d·16/3 per frame-share. Position over time therefore equals MC's (verified: 0.4 px max error on a full jump).

**Collision**
- SMW's own block collision keeps working (slopes, ledges, ?-blocks, coins, doors, pipes).
- MC's probe and push-out side effects come from SMW's blocked flags (`$77`). Floor gives grav = 0, ceiling gives
  grav = −2, wall gives xSpeed = 0 when on the ground or fighting.
- MC's wall probes at (x±22, y−20) stage px test SMW map16 solidity directly.
- Round 2:
  - `McWorld.no_wall_pull` turns off MC's 1–5 px pulls toward a wall. Those pulls plus SMW's 1 px/frame push-out
    sank Mario into separate blocks until `(flags & 0x1C) == 0x1C`, SMW's crush kill.
  - `ClampIntoWalls` stops each frame's sideways move at the tile edge, probing the body edge ±6 px at three
    heights.
  - `ClampIntoFloor` stops a fall at the first floor top it would cross (map16 0x100–0x16D). MC has no terminal
    speed, and SMW only catches the feet in a tile's top few px. Landing (`$77 & 4`) drops queued fall motion.
  - Head bump = `$77 & 8`, or a solid sprite setting the rising player's y speed to +16 (message boxes,
    `SolidSpriteBlock`). It cancels the queued rise. The held jump is then ignored until released, because MC
    re-jumps on landing while jump is held.
  - `player_in_air_flag` is set while airborne (11 rising, 36 falling). SMW's skipped air physics normally does
    this, and the Mode-7 tilting-platform rooms pin a "grounded" player to the platform.

**Input** (SMW pad to MC keys)
- Left / Right / Down map directly.
- Jump = B or A, or Up when `UpJumps` is set (MC's Up-arrow jump).
- Punch = Y, read from the raw pad byte.
- Y is masked out of SMW's merged Y|X bit, so X alone grabs, runs and fires (fireball/cape still on X).

**Drawing**
- The SNES palette can't hold MC's colours, so `PlayerDraw` hands the MC draw list to a true-colour overlay
  (`mc_overlay.c`).
- `ppu.c` merges it per scanline by z-buffer, using Mario's OBJ priority. Mario still goes behind pipes and
  foreground. The overlay also respects screen brightness and SMW's hurt blink.

**Damage**
- `LifeBar=1` (default): MC life 10, each SMW hit = `HitDamage` (0.5 since round 20, as in MC; was `DamagePerHit`
  2.5, a key now ignored). MC hurt with knockback away from the sprite,
  plus `timer_player_hurt` 0x7F.
- Death at 0 life plays MC `die1`/`die2` through SMW's death motion. Power-ups heal 5 and Mario keeps a Big Mario
  hitbox.

**Attacks**
- `hit1..hit6` AABBs × SMW sprite clipping use `CheckPlayerAttackToNormalSpriteColl_029404`, SMW's cape/net-punch
  reaction. The spark clip plays (`McHero_ShowHitEffect`) with its sound.
- Round 2:
  - Boxes are padded 4 px and get +10 px forward reach (`kHitPad`, `kHitReach`).
  - They are kept feet-relative for 4 frames (`kLingerFrames`) while the hero stays on frame 18 and isn't hurt.
  - Sprites with `$167A & 2` or `$166E & 0x20` (cape-immune) but `$1656 & 0x10` (stompable) get a stomp-style
    knockout: status 2, y speed −48, points, contact star.
  - Other cape-immune sprites ignore punches.

**Other states**
- Yoshi, swimming, climbing, cape flight, wall-run, the goal walk and every non-normal player state run SMW movement.
- The MC clock keeps ticking in "puppet" mode for the animation. In puppet mode SMW's facing drives MC's `xscale`.
- Yoshi: MC has no sitting frame, so the hero is held on "jump" (frame 9) and drawn 4 px higher (`kYoshiSeatDy`).
- Leaving puppet mode (dismount, jumping off a net, leaving water) copies SMW's speeds into MC: SMW 1/16 px/frame
  × 3 × 1.2 / 16 = MC px/tick.
- MC's run speed (10 MC px/tick ≈ 2.8 SMW px/frame) is below SMW's P-speed, so MC Mario never takes off with the cape.

**Round 3 (H, each verified by pilot)**
- `ClampIntoWalls` stops 1 px inside a solid tile, where SMW Mario rests. SMW's side collision then sees the wall:
  - side pipes (tile 0x13F, `RunPlayerBlockCode_CheckIfEnteringHorizontalPipe`);
  - throw blocks;
  - the blocked flags.
- Run:
  - Y (raw pad) or X held, on the ground at MC top speed (xSpeed ≥ 8.5, it alternates 9/10): `$13E4` +2 per
    frame up to 0x70, -1 otherwise.
  - Horizontal motion is scaled ×1.12 while running and ×1.30 at a full P-meter, eased 0.02 per frame and kept
    through jumps.
- Cape take-off: a B/A press on the ground with the cape and `$13E4` ≥ 0x70 puts the glue in puppet mode until
  landing, so SMW's `HandlePlayerPhysics` does the P-jump (in-air mode 12, `$149F` = 80) and the flight (needs
  `$15 & 0x40` held).
- Blocks:
  - New MC hit boxes run `CheckIfBlockWasHit(tile, 0)` (the cape's entry) on the nearest page-1 tile 0x111–0x12D.
  - For 0x11E with Mario big they run `CheckIfBlockWasHit_Entry3(0x21, 3)` (the spin-jump break).
  - 10-frame cooldown.
- Colours:
  - `pointer_player_palette` → `GetPlayerPalette()` (Mario, Luigi, Fire Mario and Fire Luigi palettes; the star
    cycles through all four).
  - Each pixel maps to its nearest normal-Mario palette slot plus the RGB delta, cached per palette.
- Secondary entrances: `kLevelInfo_05F800/FA00/FC00` = level / y+layers / x for secondary exit number
  (map != 0) << 8 | lo. `kLm5FE00` = entrance action & 7. The vanilla ROM has them at `$05:F800/FA00/FC00/FE00`.
- Save file: 0x8D bytes = `$1EA2–$1F2E`:
  - 0x00–0x5F level flags (0x80 beaten, 0x40 midway, low 4 bits = walkable directions, checked in
    `smw_04.c` OW movement);
  - 0x60–0x6E event bits;
  - 0x6F–0x84 maps / animation / positions (new-game values from `kInitializeSaveData_InitialOWPlayerPos`);
  - 0x85–0x88 switch palaces;
  - 0x8C exits (≥ 0x60 shows the star on file select);
  - checksum word = 0x5A5A − byte sum;
  - slots at 0x000 / 0x08F / 0x11E, mirror copy +429.
- Level timer: `misc_status_bar_tilemap[55]` ($0F30) is the 40-frame divider before a timer tick.

**Round 4 (H, verified by pilot)**
- Sprite order:
  - The SNES orders sprites against each other by OAM index (the first one found on a line wins). Priority bits
    only decide against BG layers.
  - SMW Mario's first OAM slot = sprite 64 + `kPlayerGFXRt_PlayerStartingOAMIndex[layer]` / 4 (68 normally,
    117 / 122 behind layers).
  - MC Mario's opaque pixels enter `objBuffer` when `ppu_evaluateSprites` reaches that slot. `ApplyLine` paints
    them only where that z survived composition.
- MC hit levels:
  - jab 1, straight 2, hook 3, combo finisher (attack6) 4, dash (attack7) 2, low sweep (attack11) 5, air kick
    (attack12) 6.
  - MC's own enemies take damage equal to the level.
- Stomp reactions reused for punches:
  - Rex `$C2` 0→1 (`$1FE2` = 12, `$1662` = 0);
  - Chuck `$1528` +1 with hurt state `$C2` = 3, KO at 3 (`Spr091_CharginChuck_ProcessPlayerInteraction`);
  - Dino-Rhino → 0x6F (CheckPlayerToNormalSpriteColl transform).
- Overworld:
  - tiles ≤ 0x80 are enterable;
  - 0x81 (destroyed castle) only with L+R held (an original SMW feature);
  - 0x82 / 0x5B pipes, 0x5F star.
- Cape: `UpdatePlayerCapeImage` = 00ceb1–00cf09 (cape image + `$14A2` flap timer from in-air flag, x/y speed).
  `PlayerDraw_CapeOnly` = PlayerDraw's cape tile path without the 4 body tiles (hide-flag bits shifted by 4).

**Round 5**
- Launch carry-over (H, verified by pilot):
  - SMW sprite and block code writes `player_yspeed` after the player physics. Examples: stomp `BoostMarioSpeed` −48,
    note block −96 (bounce sprite props & 3 == 3, `smw_02.c` ~1554), springboard −80 with B.
  - MC must read it at the start of the next `McSmw_PlayerPhysics`: speed < 0 and < last written − 4.
  - Height match: SMW gravity while rising is 3 /16 px per frame² with jump held, 6 without. MC loses 0.5 grav per
    tick and 1 SMW px = 3 MC px, so `g0 = v * sqrt(3 / 32a)` (0.177 / 0.125).
- Side tile code (H):
  - SMW calls `RunPlayerBlockCode_CheckIfPlayerTouchingHurtBlock(tile_lo, side 1 right / 2 left, true)` from
    `HandlePlayerLevelCollision` when the body sensor is inside a wall tile.
  - Hurt tiles: 0x12F Muncher, plus tileset-specific spikes 0x59–0x5C / 0x66–0x69. Otherwise the call goes to
    `CheckIfBlockWasHit` with the side mask from `kCheckIfBlockWasHit_DATA_00F0EC`.
- Map16 (H, poked in pilots):
  - 0x115 = plain note block (bounces from above; `DATA_00F0A4` 0xC);
  - 0x114 = solid from above, hit from below only;
  - 0x02B coin, 0x038 midway bar, 0x12F Muncher.
  - Horizontal level layout: lo byte `$7E:C800 + screen*0x1B0 + (y>=256 ? 0x100 : 0) + (y&0xF0) + (x>>4)&0xF`, hi byte at
    `$7F:` same offset.
- P-balloon (H):
  - `timer_inflate_from_pballoon` ($13F3) != 0 → `PlayerState00_00CD39` skips the physics (MC puppet).
  - Pose 66/67; `timer_player_has_pballoon` ($1891) counts down.
- Wiggler (H):
  - `$166E` = 0xF5: bit 0x20 → no cape kill.
  - `$1656` = 0 → not stompable-killable.
  - Stomp reaction (`Spr086_Wiggler` draw routine): `$1540` = 64, `++$151C` (angry), `Spr086_Wiggler_02F2D7` (the
    flower extended sprite). Only a star kills it.
  - `SprStatus02_Dead` runs `SprStatus08` for 0x86.
- Level sprite loader (H):
  - `LoadOneSprite` (smw_02.c) picks a slot from `r15` (max) down to `r6`+1 (sprite-memory tables).
  - A sprite that finds no slot is retried only when its column scrolls in again.
  - `spr_load_status_table_index` = 0xFF marks "not from the list" (despawn code skips it, except
    `Spr0C0_SinkingLavaPlatform`).
- Yoshi carry-over (H):
  - `yoshi_carry_over_levels_flag` ($0DC1) + `yoshi_current_yoshi_color` ($13C7, 10 = green).
  - `LoadSublevel_02A751` spawns Yoshi unless `flag_prevent_yoshi_carry_over`.
- Stun (H): `timer_stun_player` ($18BD) is only set by the Koopa Kid ground pound (`smw_01.c` ~6233). While nonzero,
  `PlayerState00` skips the physics.
- GFX28 icons (H): 0x2E = status-bar coin (colour 3 gold, 1 black lines), 0x64 = bonus star (colour 2 only), 0x76 clock.

**Round 6**
- Cape spin (H): `CheckPowerUpSpecificPlayerAttacks` only sets `timer_active_cape_spin` ($14A6) = 18. The spin pose,
  alternating facing and the cape hit box come from the pose code: `SetPlayerPose_00CF0A` →
  `InitializeCapeSwingOrNetPunch` (sets `flag_cape_to_sprite_interaction` and `player_cape_hitbox_x/y`). Then
  `HandleCapeInteraction` (sprites) and `HandleCapeLevelCollision` (blocks) run.
- MC air-attack wall cut-off (H): `mc_core.c` ~594: `if(!onGround && fighting) { if (walls) jump; if (hit(x+-30, y-20))
  jump; }`. In SMW mode `hit` is only called there.
- Stomp chain (H): `CheckPlayerToNormalSpriteColl_01AB46`: n = `$1626` + counter++; sound `kStompSoundTable_Bank01[n-1]`
  (0x13–0x19); `GivePoints(k, min(n+1, 8))`. GivePoints adds 5: score sprite 6 = 200 … 0x0C = 8000, 0x0D = 1-UP.
  Landing resets `counter_consecutive_enemies_stomped` (smw_00.c ~5810).
- ! blocks with items (H): `ExtObj8E` yellow / green → map16 0x16A / 0x16B when the switch is on.
  `CheckIfBlockWasHit` maps them to `Entry3` index 34 / 35 only if `kInitializeLevelRAM_DATA_00A625[tileset] & 3 == 0`,
  i.e. not tilesets 4, 5, 13 (Yoshi's House is 4). The search of all 512 levels found no vanilla level that places them
  in a tileset where they work (M); the poked test in YI1 gives a feather.
- Flying ? blocks (H): head bump = `SolidSpriteBlock_Entry2` (`$1558` = 16, `$C2` = 1). The item comes out at
  `$1558 == 8`.
- Level dirt (H): under a grass top (0x100) YI1's dirt is 0x03F, a page-0 fill tile with no collision.
- Sprite slots (H):
  - `kParseLevelSpriteList_SpriteSlotMax/Start[setting]` bound both level loading and run-time spawns
    (`FindFreeNormalSpriteSlot_*`).
  - Some settings give special sprites a one-slot range (Banzai Bill, Big Boo: `lo = 0xFF, hi = 0`).
- Piranha Plants / pipe Lakitu (H): hidden-phase wait = `$1540`. The init only moves the sprite (x+8, y-17).
- Status after defeat (H): a punched / stomped Goomba or Koopa becomes 9 (stunned / shell), not 2.
- Fireballs turn a Koopa into sprite 0x21 (coin) in its slot (H).
- `$15A0` / `$186C` (off-screen flags) are only updated by SMW's GetDrawInfo, i.e. by sprites drawn by SMW (H).
- PPU (H): `PpuDrawBackgrounds` draws sprites, BG1, BG2 and then BG3 into one z-buffer per line. Flipping the buffer
  before BG3 mirrors the level and keeps the status bar / message boxes (BG3) readable.
- GFX00 (3bpp, 24 bytes per tile) (H):
  - mushroom 24/25/34/35, fire flower 26/27/36/37, feather 0E/0F/1E/1F, star 48/49/58/59;
  - palette rows: mushroom 12, feather 10, flower 13 (`$166E` & 0xE).
  - Sprite rows are loaded from `kGlobalPalettes_Objects` (columns 2–7), with column 1 = white.
- MC enemy Koopa, DefineSprite 163 (H):
  - frames walk / hurt / attack; clips 149 (walk, 14 frames), 154 (hurt_mc, 108 frames: hurt1 1, hurt3 16, hurt4 33,
    hurt5 69, hurt6 83, dead 108), 153 (dead child, 70 frames), 162 (attack, 45 frames: hit1 from frame 16, hit2 from
    frame 33, sounds 155 at frame 5 and 160 at frame 17), 159 (hit box).
  - Frame scripts: 154 frames 15 / 32 / 68 / 107 → `_parent.gotoAndStop(1)`, frame 82 → `gotoAndPlay("hurt5")`;
    162 frame 45 → `_parent.gotoAndStop(1)`.
  - Load values: life 10, moveSpeed 1.5, eyeSight 500, jumpHeight 8, deathBounce true, `_xscale` -100.

- MC boss Bowser, DefineSprite 532 (`_root.bowser`, round 7) (H):
  - Root labels: idle 1, walk 2 (clip 518, never used by the script), breath 3, fire 4-21 (frame 21: `gotoAndStop(1)`),
    hurt 22 (clip 527, 16 frames, frame 16: `_parent.gotoAndStop(1)`), jump 23 (clip 528, 35 frames), pant 24 (clip
    529), teleport 25 (clip 531: frame 9 `_parent._x = 320; _y = 250`, frame 18 `_parent.gotoAndStop(1)`).
  - 527 has no labels: the script's `hurt_mc.gotoAndPlay("hurt1")` etc. only keep it playing; the hurt5 landing bounce
    (hurt_mc frames 69-82) can't happen.
  - Jump clip 528: frame 2 `grav = jumpPower * 2` (20), 11 `grav /= 2`, 24 `grav -= 20`, 35 `stop()` + an onEnterFrame
    that stays defined: on the ground (`parent y + 5`) at frame 35 it duplicates `groundHitOrig` and does `jump--`; on
    the ground at any frame it restarts (`gotoAndPlay(1)`).
  - Health bar: clip 510 (dark-red bar 507 + `bar` 509 = bright-red 508, 53 x 3 SNES px); 510 sets its `_xscale` to
    the parent's sign, so it never reads mirrored; `bar._xscale = life`; hidden once life < 0.
  - Load values: attackSpeed 100, attackSpeedDefult 100, life 100, moveSpeed 3, stopSliding 10, attack1 500, jump 5,
    jumpPower 10, jumpSpeed 6, pant 300. No contact damage.
  - Fire: the root `flame` template (555, scale 1) spawns a copy at (bowser.x, bowser.y - 70) whenever
    `bowser.attackSpeed == 1`. Copies: life 300, `_rotation = 270` reads back as -90, so Angle = -180; speed 5 along
    Angle, `twist` from `random(150)` swings Angle by +-1 per tick (period 200). `bound` (442, hidden) hits
    `hero.bound`: grav 5, xSpeed -+5, life -0.25, the flame is removed. Sound 554.
  - Shockwave: `groundHitOrig` (558, placed at scale 0.3614654541; copies keep it), 21 frames of a growing ring, alpha -5
    per tick. Shape hit test at the hero's feet: hurt, xSpeed 10, life -= alpha / 90. `_root.hero._x < _root.bowser`
    compares a number with a movie clip (always false), so MC always pushes right; the port pushes away from the ring.
    Sound 557.
  - Quirk kept: if attack1 runs out while attackSpeed is 1 (hits pause attackSpeed but not attack1), the template sees
    attackSpeed == 1 every tick of the jump / pant phases and keeps spawning flames (capped at 32 in the port).
- SMW (H): `RunPlayerBlockCode_TriggerCapeDiveGroundPound` knocks out every grounded sprite in slots 0-9 through
  `CheckPlayerAttackToNormalSpriteColl_029404(i, 53)`; the landing sound is `io_sound_ch3 = 9`.
- SMW (H): the port's `HackHandleWalljump` (lm.c) uses free RAM `-`; nothing else touches them.
- SMW (H): GFX2A is the message-box font: A-Z 0x00-0x19, `! . - , ?` 0x1A-0x1E, a-z 0x40-0x59, `# ( ) '`
  0x5A-0x5D, digits 1-7 0x64-0x6A and 0 0x6B (no 8 or 9). Letter = colour 3, background colour 1.
- SMW (H): paused (`flag_pause`), game mode 0x14 skips `ProcessNormalSprites` and `PlayerDraw`.
- Round 8 (agent play-testing):
  - SMW (H): `ModifyMap16IDForSpecialBlocks` (00F545) changes what a tile acts like for the player: page-0 tiles EC–FB
    (the switch-palace switch) are used block 0x132; with the blue P-switch 2B (coin) ↔ 132 (used block) and 29 → 24;
    with the silver one 12F → coin. MC's own tile tests go through `ActsLike` for this.
  - SMW (H): `HandlePlayerLevelColl_00E98C`: in levels without horizontal scroll (`$1411` = 0: auto-scroll, header
    scroll setting 3, boss rooms, Morton/Roy's pillars) a player past screen x 232 (or left of 8) gets blocked flag
    0x80 plus the side bit and his x speed set to the scroll speed; at screen x ≥ 240 he is crushed (00EA08).
  - SMW (H): door entry needs `io_controller_press1 & 8` (Up) on door tiles 1F / 20 (and 9C in tileset 1), feet ±8 px.
  - SMW (H): switch palaces: after the switch, `timer_title_screen_input_timer` (160 x 4 frames) runs the message, then
    the level ends by itself; no button closes it.
  - SMW (H): Sunken Ghost Ship (018) draws main screen = BG3 only (TM 04) and sub screen = BG1/BG2/OBJ (TS 13) added by
    colour math, so a hero drawn only on the main screen's OBJ layer was invisible there.
  - SMW (H): `HandlePlayerLevelCollision` (00E92B): when `$5B` & 0x80, the block code runs for layer 2 first with the
    player moved by `$26/$28` (layer 2's tile map = level-data screens 16+ horizontal, 14+ vertical when `$5B` & 0x02);
    layer 1 is skipped when `$5B` & 0x40. Valley of Bowser 2 (115): layout 0x80, layer-2 offset (0, -53) at the first
    pillar. A head inside a block is pushed down out of it (`player_ypos += ~v`, flag 8); flags & 0x1C == 0x1C crush.
  - Round 9: MC Koopa hurt clip 154 as a stun timer for SMW enemies (H, from the clip data): hurt1 frames 1-14 (ends at
    15), hurt3 16-31 (ends 32), hurt4 33-67 (ends 68), hurt5 69-81 looping until the landing bounce (grav = -grav / 2,
    goto hurt6 83), hurt6 83-106 (ends 107). TakeHit damage / knockback per level: 1: -1, xSpeed hx+s, air grav -2;
    2: -2, grav 3, hx+1.5s; 3: -3, grav 4, hx+6s; 4: -4, grav 4, hx+15s; 5: -1, grav 20, hx+rand(7)s; 6: air -3
    grav -20 / ground -1 grav 7.
  - SMW (H): running a normal sprite's status-8 routine with `flag_sprites_locked` set draws it without moving or
    thinking (SMW's own freezes do this); `$1686` bit 7 = the sprite never collides with the level (Bullet Bills,
    Super Koopas...). The Super Koopa (73) climbs on its upward y speed (c2 = 1) and starts its descending flight (c2 =
    2) when that speed runs out.
  - Round 11, SMW overworld (H, smw_04.c): leaving a tile is gated only by `ow_level_tile_settings` ($1EA2) low bits
    (8 up, 4 down, 2 left, 1 right; 0x80 beaten, 0x40 midway) in `OwProcess03_StandingStill`; the next tile must be
    1..0x86 in `blocks_map16_table_lo` (C800, the layer-1 map, which holds the unrevealed paths too). Arriving at a tile
    ORs the way back into $1EA2. Events ($1F02 bit `kBitTable_DATA_04E44B[n & 7]` of byte n >> 3) are drawn at map load
    (layer 1: `LoadOverworldLayer1AndEvents_04DA49`; layer 2 only at file select / continue:
    `LoadOverworldLayer2AndEventsTilemaps`, RAM 0x14000); the flag itself is set only in
    `OwEventProcess07_SilentEventsAndEndOfEvent`. Saves copy $1EA2..$1F2E (`ow_save_buffer`, `SaveGame`).
  - Round 11: MC shape 437 (a hurt-clip frame, Mario curled up tumbling, 31 x 27 art px) rotated 135° clockwise reads
    as a tucked seat-first drop.
  - SMW (H): scale platforms (sprite 8F) are two `SolidSpriteBlock` caps (the second at $1602/$C2, $151C/$1534); the
    stems are map16 tiles they generate as they move.
  - SMW (H): running take-off speed `kHandlePlayerPhysics_JumpHeightTable` (0xB0 standing … 0x9F at full run, negative
    = up); MC's jump is scaled by the same ratio.
  - SMW (H): the CHEATS hub icons are GFX00 mushroom (MARIO), feather (MOVES), star 48 / row 10 (WORLD) and GFX01
    shell 0C / row 13 (ENEMIES).
  - Round 12, SMW (H, smw_00.c `HandlePlayerLevelColl` ~line 5291): with side exits off, an on-screen player x of
    0xF0–0xFF kills the player (`HandlePlayerLevelColl_00EA08`, the autoscroll squeeze); under 8 px into the screen he
    is pushed back. `flag_layer1_horiz_scroll_level_setting` ($1411) is nonzero in ordinary scrolling levels.
  - Round 12, SMW (H): normal-sprite timers $1540, $154C, $1558, $1564, $15AC, $163E, $1FE2 count down once a frame in
    `ProcessNormalSprites_GetNormalSpriteOAMIndexAndDecrementTimers` (before the sprite's routine, not while sprites
    are locked). Many sprite events fire on an exact timer value (Sumo Bro's lightning at $1540 = 46, the Pitchin'
    Chuck's throw at ($1540 & 0x1F) = 6), so running a routine twice in one frame needs a timer step in between.
  - Round 12, SMW (H): the generic walkers' routine (`SprXXX_Generic_Spr0to13Main`) sets x speed from $157C (0 right,
    1 left) every frame on the floor; `ChangeNormalSpriteDirection` turns a sprite (flips $157C and x speed) only when
    its turn timer $15AC is 0, then sets it to 8. Sprite gravity (`HandleNormalSpriteGravity`) adds 3 a frame (y speed
    −0x38 ≈ a 2-tile hop).
  - Round 12, SMW (H): player gravity is added in `HandlePlayerPhysics_InAir` (3 while A/B is held and rising,
    otherwise 6; capped at 0x40 before the add).
  - Round 13, SMW (H): the level's sprite-list "loaded" flags are `sprites_load_status` ($1938, one per list entry,
    `spr_load_status_table_index` $161A per slot, 0xFF = not from the list). SMW clears an entry when its sprite is
    erased off-screen while alive (status ≥ 8, `SubOffscreen_*`), never when it is defeated; all are cleared on a level
    load (`InitializeAllSpritesOnLevelLoad`). A cleared entry loads again when its column scrolls into the load range.
  - Round 13, SMW (H): SNES L / R are `io_controller_hold2` / `press2` 0x20 / 0x10 (A 0x80, X 0x40); SMW only uses them
    for the L/R camera scroll (held 16 frames, `PlayerState00_HandleLRScrolling`). The cape swish is `io_sound_ch3 = 4`.
  - Round 13 (H): save state 9 is inside Yoshi's Island 1 (game mode 0x14), not on the map. After loading a state the
    pilot's SNES mix has no music (sound effects only), also after `warp 106`; `warp 101` (Iggy's Castle) starts its
    song. (M: probably because the state holds no running song and YI2 doesn't load a new one.)
  - Round 14, SMW levels (H, `levelscan` + level_catalogue.py): level data `kLevelData_Layer1(n)` = 5-byte header,
    3-byte objects (object number `(b1 >> 4) | ((b0 & 0x60) >> 1)`; 0 = extended, type b2), 0xFF end. Screen exit =
    extended object 0 plus one byte: screen `b0 & 0x1F` (decimal in the pilot output), destination low byte, `b1 & 1`
    (kept in `misc_subscreen_exit_properties`), `b1 >> 1` = secondary (vanilla: one flag for the whole level, the
    last exit object wins). The destination's bank is the player's map: `ow_players_map != 0` -> 0x100 (LoadLevel);
    secondary exits go to bank | `kLevelInfo_05F800[bank | exit]`. Translevels 01-24 = levels 001-024 (main map),
    25-5F = 101-13B (submaps); all of 01-5C have a map tile. The timer comes from layer-1 header byte 3 >> 6
    (`kLoadLevelHeader_TimerTable`), only on a fresh start (`counter_sublevels_entered == 0`).
  - Round 14 (H): of the 512 slots: 92 map levels, 132 rooms reached through exits, 6 loaded by code (bonus game
    000 / 100 and wings area 0C8 / 1C8 by bank via `kLoadLevel_BonusLevelSublevelsLo` = 00, C8; intro 0C5 =
    `misc_intro_level_flag` E9 - 24; title screen 0C7 = EB - 24), 264 hold a 99-byte test level (as do 13 map tiles:
    the Star Road warps 012 01E 10C 124 129 12E 12F 131 133 137 138, and 019, 112), 9 hold rooms nothing reaches,
    each also in bank 1: 095 (Reznor), 098 099 09A (Koopa Kid fights), 093 094 096 097 09B (= used rooms 1F2 0D3 1EB
    1F6 1C7, the last Bowser's). Map tiles 016 / 017 hold Donut Plains 1, 108 a small room.
  - Round 14, SMW (H): a castle / ghost house started from the map (tileset in `kLoadLevel_LevelEntranceTileset`, no
    `flag_disable_no_yoshi_intro`) loads twice from its start: first the door scene (`kEntranceData_Layer1`, no
    timer), then, after the door, the level itself again with `counter_sublevels_entered` 0 (the level from the map
    tile, its timer). A Koopa Kid's defeat goes through game modes 1A / 1B (castle cutscene, waits for a button),
    then 0B / 0C; the map's own load (0C, `OwProcess00`) applies `misc_exit_level_action`, `flag_got_midpoint` and
    `misc_currently_active_boss_end_cutscene` to the tile under Mario. The 1-UP bonus game ends when all 9 blocks
    have `$C2` set (`sprites_bonus_game_is_over_flag`), then `timer_bonus_game_end`.
  - Round 12, MC (H): `McHero_TakeHit` light hit = xSpeed ±6 on the ground (friction 1 a tick: 21 MC px, ~7 SMW px);
    heavy = xSpeed ±10, grav 10 (~65 SMW px along, 32 up). Distance scales with the square of the speed.
  - Round 15, boss rooms (H, pilot `bossrooms` = sprite data of all 512 slots, + map levels' exits followed through
    their rooms): Koopa Kid 29 (kind = the sprite's y tile, `Spr029_KoopaKids_Init`: 0 Morton 1 Roy 2 Ludwig 3 Iggy
    4 Larry 5 Lemmy 6 Wendy), Bowser A0, Reznor A9 (four per room), Big Boo boss C5. Used: Iggy 1F6 (from 101),
    Morton 0E5 (007), Lemmy 1F2 (11C), Ludwig 0D9 (00E), Roy 0CC (020), Wendy 0D3 (01A), Larry 1EB (110), Bowser
    1C7 (10D / 10E), Big Boo 0E4 (013), Reznor 0DF (Vanilla Fortress 00B), 0D5 (Forest Fortress 01F), 0E2 (Chocolate
    Fortress 01B), 1DE (Valley Fortress 111). Unused copies in 093-09B / 193-19B.
  - Round 15, boss damage (H, code): Morton / Roy / Ludwig: box `01D40B` (x - 8, w 16; y + 8 or + 19 from frame 0x69,
    h 16, level coords), needs `151c < 3`, `!1fe2` (set 8); frames 6-0F and >= 1B and `1594 & 1` (wall / ceiling)
    bounce the stomp; a stomp sets 151c = 3, 1540 = 0x80; three hurts. Iggy / Larry: box in screen space (pt from
    `01FD50`: x - 8 w 15, y + 96 h 12) against `player_on_screen_pos`; a stomp only sets `154c` = 24 (into the shell)
    and the tilt slides him (`kSpr029_KoopaKid_IggyLarry_HurtXSpeed`): the lava defeats him (`160e`). Lemmy / Wendy:
    standard clipping, only in state 151c == 2; 151c = 4, 1540 80 (decoys, `1570`: 31, no sound). Reznor: hit from
    below sets 1564 = 15; his own code knocks him off at 1564 == 12 (one hit each). Big Boo: thrown block only in
    c2 == 2 (visible); c2 = 3, 1540 64; three hits. Bowser: a stunned sprite in slot 0/1 against clipping 36 (car:
    bounces) and 37 (head: hit, `bowserbattle_var14b5` = 76) while `m7_angle` low byte 32, rotation 0, 151c >= 7
    (phases 7-9); defeat = state 4 (rise up to die), 5, 6; `$13C6` only after the Peach scene. Clipping 36 = dx -12,
    w 56, dy 32, h 56; 37 = dx -14, w 60, dy 4, h 20. Defeat elsewhere: `$13C6` != 0 and `timer_end_level` = FF,
    which counts down every 2nd frame (palette fade below A0, end at 0).
  - Round 15, stomp result (H, `CheckPlayerToNormalSpriteColl` 01A80F): `1686 & 0x40` transforms (para -> plain,
    Super Koopa -> feather); else `1656 & 0x20` squishes (status 3); else `(1662 & 0x80) == 0` stuns (status 9;
    1540 = FF for 0F / 11 / A2 / 0D, else 2: a shelled Koopa then spawns the Koopa from `kSprStatus09_Stunned_
    SpriteKoopasSpawn` with `1564` = 16); else status 2. Touching a status-9 sprite without grabbing kicks it
    (`01AA42`: status 10, `154c` 16, x speed ±0x2E; kicked Goombas / Bob-ombs / Mechakoopas land stunned again).
  - Round 15, player (H): SMW Mario's side probes (`kGetPlayerLevelCollisionMap16ID_DATA_00E832/E89C`): x + 14 right,
    x + 2 left, at y + 0x1A and y + 0x16 (small) / y + 0x0F (big); head x + 8. At 6.5 px a frame a body can end in a
    ledge / pipe corner where all four blocked bits (0x1D) are set and SMW kills him as crushed. Death:
    `PlayerState09_Death` decrements `player_anim_timer` every 4th frame; at 0 it takes the life and sets game mode
    0B (or 15 game over / time up). Side pipes (`00F3C4`, tile x3F) enter only when `temp8f` (= `player_in_air_flag`)
    is 0 and the direction is held. Page-1 map16 numbers are tileset-specific in the data (133 / 134 are a dirt ledge
    in tileset 0, 137 / 138 a hill), so `findtiles` can't find pipes by number. Ice: `flag_ice_level` from entrance
    setting 5 (rooms 10B Donut Secret 2, 12D).
  - Round 17, SMW (H, pilot logs): the walk to a castle / ghost house door is player state 0A (set by
    `InitializeLevelRAM`, `player_pipe_action` 1): SMW moves Mario along y 368 with Right forced, without its level
    collision (blocked flags 0, `player_in_air_flag` 36), to the door (x 232 in Donut Ghost House's entrance) and in.
  - Round 18, SMW solid sprites (H, smw_01.c `SolidSpriteBlock` 01B457): contact by clipping A; on top when
    `player_y + 24 < spr_y` (feet less than 8 px below the sprite's y) and not rising: `player_yspeed = 16`,
    `misc_player_on_solid_sprite = 1`, `player_ypos = spr_y - 31` (47 on Yoshi), `player_xpos += $1528` (the
    sprite's x move). Sprites with `$190F` bit 0 stop there (solid on top only). Side push when `player_y + v7 <
    spr_y` (v7 = 8 small / ducking, +8 on Yoshi): `sub_1B505` sets `player_xpos = spr_x + 14` (right of it) or
    `spr_x - 15` (left; Reznor A9: +16 / -32; 9C, BB, 60, 49: +31 / -15) and `player_xspeed = 0`, no blocked flag.
    Otherwise (under it) a rising player bumps it (`player_yspeed = 16`; flying ? blocks 83/84: `$1558` = 16, `$C2`
    = 1). Callers move the sprite onto the platform for the call (Reznor's platforms, the line-guided and the grey
    chained platform A3) and back. A hit flying ? block (`$C2` != 0) stops flying. The brown chained platform (5F) has
    its own top-only routine (`sub_1C9EC`: box pt.x-24, pt.y-12, 64 x 19; stands the player at pt.y - 40, yspeed 0).
    A living Reznor's platform is not solid: his own contact rebounds the player sideways (`player_xspeed` +-0x20)
    unless he comes from below rising (the hit); from above it hurts.
  - Round 18, SMW music between levels (H, smw_00.c GameMode11 / smw_05.c LoadLevelHeader): the level's song starts
    at its load only if `misc_music_register_backup` ($0DDA) < 0x40; the header code keeps bit 7 of the old value
    and sets bit 6 when the song is the same one. Goals, deaths, the switch palace and boss wins leave $0DDA at 0xFF;
    the map's load (`GameMode0C`) clears it. The last music command sent to the SPC is `$1DFF`
    (`io_copy_of_music_ch1`); `$1DFB` is cleared once sent. ($18C5, the mirrored APU port 2, stayed 0 in the pilot.)
  - Round 18, Koopa Kids / layers (H): `CheckPlayerToNormalSpriteColl_01A80F` compares the sprite's layer `$1632`
    with `player_current_layer_priority` only when `$0D9B` bit 6 is clear; the mode-7 boss rooms set it, and Morton /
    Roy / Ludwig sit on layer 0 while Mario is 3. Their own contact (`Spr029_..._01D2A8`) sets `$1FE2` = 8 on each
    touch (hurting a grounded player) and ignores layers. Their stomp bounces when `$1602` is 6-0F (shell) or the
    player is not above, bounces sideways (+-32, yspeed -80) on a wall (`$1594` & 1), else hurts unless `$1602` >= 1B.
  - Round 18, P-switch coins (H): with the blue P-switch running, map16 0x02B (coin) acts as the used block 0x132
    (`ModifyMap16IDForSpecialBlocks`); SMW's side check is handed the acted tile. Handed the raw 0x02B it took it for
    a hittable block and spawned a P-switch block, then left a yellow block (`$12B`) that never turned back.
  - Round 19, SMW Pokey (H, smw_02.c `Spr070_Pokey`): a stomp-free hit takes off one segment:
    `Spr070_Pokey_RemovePokeySegment(k, y_offset)` clears the segment bit at that height in `$C2`, and
    `Spr070_Pokey_02B82E` spawns the falling segment (a sprite 70 in status 2, `$1534` set). PUNCH ANYTHING does the
    same at the fist's height (`PunchPokey`), the segment flying at x speed +-0x10 the way Mario faces, y speed -32. A
    bubble (9D) pops when `$1534` is set to 6 (from >= 7), with the contact star: any punch does it.
  - Round 19, SMW -> MC jump hand-back (H, pilot logs: rope, vine, net, Yoshi): from a state SMW drives (climbing,
    riding), MC's rise is `grav = -yspeed * 0.177` with the jump held (`0.125` without), and a release before the top
    cuts it to `grav * 0.7071` once (`g_held_rise`). Heights: rope 41 / 49 / 64 px vs SMW Mario's 41 / 51 / 66; Yoshi
    dismount 75 vs 68 (before: about 130).
  - Round 19, MC Bowser hurt (H, `mc_bowser.c` `TakeHit`, pilot `sprites` log every frame): a hit while he is in
    the hurt label (root 22) only takes life and sets speeds: `RootGoto(kFrameHurt)` on the label he is on does not
    restart its clip (Flash `gotoAndStop` on the current frame), so the hurt clip runs its sub-frames 1-15 (about 17
    SMW frames) whatever the hits, then he goes to fire breath (root 4) or breath (3). A jab on that first root-4 frame
    puts him back in hurt: one jab every ~18 frames keeps him from acting (the round-19 combo: 21 jabs, King Koopas +
    Instant hits, `tas_combo2.py`). Mario's own x speed adds to a hit's knockback (`hx + s * level`): a jab while
    walking sends him out of reach. Labels: 1 idle, 3 breath, 4 fire, 22 hurt, 23 jump, 24 pant, 25 teleport.
  - Round 19, SMW bosses (H, code + boss rush runs): Bowser (A0) takes 2 hits per phase (`$187B` from 2), phases
    `$151C` 7, 8, 9; a punch counts in a fighting phase only (mode 7 angle 32, no rotation, `$14B5` 0), two punches a
    hit. Ludwig is hurt only while his frame `$1602` is under 6 or 0x10-0x1A and `$1594` bit 0 is clear (not in or
    coming out of his shell, `McSmw_PunchMortonRoyLudwig`); his fireballs (sprite 34) fly level at Mario at 2 px a
    frame. Lemmy / Wendy rooms: seven pipes (Lemmy's tops at y 303 / 319 / 319 / 303 / 287 / 303 / 319, Wendy's all
    319), the real Koopa Kid keeps his slot (the dummies are added each round), out of his pipe for about 65 frames;
    the reflecting fireballs (B6) move 1 px a frame on both axes.
  - Round 19, settings notices (H, `menu.c`): "SETTINGS SAVED" and the other notices last 1800 ms of real time; under
    a test pilot (which runs faster than real time) they count frames at 60 a second instead (`NoticeClock`), so a
    script's screenshots and recordings show them over the same frames every run.
  - Round 20, MC story mode (H, `work/ffdec/scripts`, timelines): root frames 486 / 488 / 490 / 492 / 496 are the
    five areas (wall over lava, inside, the hall, the lift, Bowser's bridge); each has an invisible `ground` (the
    hero's hitTest shape, some fills alpha 0), `spawnPoint` (the hero's feet, a few px inside the ground), `lScroll` /
    `rScroll`. The exit marker (sprite 445) shows its arrow while the hero stands in it on the ground, not hurt; Space
    there starts `hud.fader` "nextLvl". Area 4: Space starts clip 495, the lift (492) rises 640 stage px over 315
    frames; at 316 the cages and floor go and the Koopas drop; Koopa 26 below y -110 moves on. Koopas hit the hero
    for 0.5 (life 10: 20 hits), Bowser's flames 0.25, his shockwave alpha / 90. Design: `notes/mc_level.md`.
  - Round 20, castle tileset (H, `RunPlayerBlockCode_CheckIfPlayerTouchingHurtBlock` 00F120, pilot `mctiles`): in
    tileset 1 the hurt tiles are 0x159-0x15C (the yellow spikes: up, down, right, left) and 0x166-0x169; 0x11E-0x12B
    are turn blocks (yellow, with eyes), not spikes. SMW clears `misc_player_on_solid_sprite` at the start of
    `ProcessNormalSprites`; a platform set before that is lost (no ground friction: the player slides as in the air).
    Pilot `ppuw 212C <mask>` shows one layer at a time (TM is written only at level load).
  - Round 20, MC story-mode art (H, SWF + renders): the camera clip (447) sets `_root.BG`'s position and scale to its
    own every frame: the BG clips (469 in areas 2-3; 489 in area 4, two copies of the same wall 400 stage px apart,
    239 frames moving one copy down, started by the lift's Space) are locked to the screen, 705 x 405 stage px.
    Areas 1 and 5 have no BG: their sky is a gradient shape in the picture. Area 2's grey rectangle (shape 14, depth
    1) sits under its BG (depth 2): never seen. MC's stone weave repeats every 16 px at SNES scale (exactly
    across, about down); its brown blocks are about 15.5 px; the stone's border from the edge in: dark (32), light
    (160,168,152), (128,136,120) x2, (96,104,88), (64,72,56) x3, dark (32). Area 4's cages and ledge are ground only
    (no art). PPU (ppu.c): a pixel no layer covers keeps ClearBackdrop's 0x0500 in the main z-buffer; a BG row is
    line + 1 + vScroll.
  - Round 20, MC's stone frame at SNES scale (H, pixel reads of the renders, flat pixel-art colours): 8 px, from the
    outside in: top and left 32 / 160,168,152 / 128,136,120 x2 / 96,104,88 / 64,72,56 x2 / 32; bottom 32 / 160 /
    128 / 96 x2 / 64 x2 / 32; right 32 / 64 x2 / 96 / 128 x2 / 160 / 32 (shaded the other way); outer corners rounded
    (the 3 corner pixels open, the next diagonal dark). MC's art isn't on a 16 px grid (brown blocks 15.5 px, the lava
    surface sprite 141 at about 15.7 px, spikes 9 px apart, the pipe 37 px wide at its rim): drawn over SMW's tile
    collision it stands up to 8 px off, so the port draws its ground from the tiles in this style instead. The scene
    items: area 1 sky 120, clouds 123, castle 144, lava 141 / 142; area 2 door 476, everything else one bitmap (480);
    area 3 one bitmap (486); area 4 one bitmap (497); area 5 sky 498, clouds 500, bridge and pillars 504, fog 559.
  - Round 20, MC's brown blocks (H, pixel dump of area 2's row, 1468-1536 x 412-427): a 14 x 14 bevelled block
    (248,248,248 highlight, 208,192,176 / 176,160,144 / 144,128,112 faces), a 1 px seam 104,88,72 after each (15 px
    apart), a 1 px outline of the same colour round the whole group.
  - Round 20, MC's music (H, SWF tags): the areas' music is sound 464 (69 s) in the `music` clip (465 / 466, track1);
    root frame 496 (Bowser) starts sound 560 itself (StartSound, flags 0x10: no loop; 194 s, 22050 Hz mono ADPCM);
    frames 493 and 498 `stopAllSounds()`. Loudness: 560 about 4300 RMS, SMW's castle music about 1000 in the port's
    SNES mix. SMW requests the room's music in GameMode11 unless `misc_music_register_backup` has bit 6 (continuing).
  - Round 20, SMW tiles (H, smw_00.c `RunPlayerBlockCode_00F2C2`, pilot logs): page-0 tiles 0-5 set the player's water
    flags (swimming): no lava death through them; map16 100 (page 1 below 111) is a ledge: landed on from above,
    passed from below. Lava death in the MC level is the port's own check against the area's table.
  - Round 20, MC area 3 (H, SWF): the ground shape 483 is one fill (no lines); the first chain-hung slab (picture x
    313-437, top 236) is not in it, the second (900-1000, 264-272) is. MC's lava surface is sprite 141: shapes 130, 132,
    134, 136, 138, 140, five frames each (30 a loop), all instances in step. MC's camera (clip 447) keeps its own
    rectangle inside lScroll / rScroll; the screen shows a little more (area 3's first pillar, left of lScroll).
  - Round 20, MC's edges (H, hero onEnterFrame + the areas' `lScroll` / `rScroll`): MC has no wall at an area's ends;
    the hero is pushed back inside `lScroll + 22` / `rScroll - 22` every tick (`while (_x - 22 < lScroll._x) _x++`)
    and the camera never shows past lScroll / rScroll (clip 447, 701 x 401 = the screen). Port, SNES px from the
    area's origin: camera 12-1367 / 13-1353 / 0-1464 (left kept at 0 for the first pillar) / 1-917 / 14-569, hero's
    middle 19-1360 / 20-1345 / 47-1457 / 8-909 / 22-562 (invisible solid columns, tile 130). Area 5's pillars stay
    open on their outer sides: MC lets the hero and Bowser fall off there (the walls only stand inside the camera).
  - Round 20, MC's lava glow (H, shapes 128 / 486): a linear gradient, red (255,0,0) alpha 255 -> 0, centred on the
    lava's surface: on screen about half alpha at the surface, fading out over 40 stage px (13 SNES px) above it.
  - Round 20, MC Bowser's pant (H, clip 529 / shape 506): the pant squashes him (sy 0.94-1.0) with a ty that keeps his
    feet where they are (3.1 stage px into the ground every frame). The port's overlay blits scales within 0.07 of 1
    unscaled; anchored at the top-left that sank his feet up to 3 px; it now keeps the scaled image's bottom edge and
    horizontal centre (feet on the same row in every frame).
  - Round 20, SMW's cape (H, `CheckPlayerAttackToNormalSpriteColl_029404`): the cape sets $154C = 8 on a sprite it hits,
    and the cape and contact checks skip sprites whose $154C is not 0. The MC Koopa and Bowser hooks set 32 (MC's
    own hurt time is longer than SMW's 8 frames of spin): one cape hit every 32 frames.
  - Round 20, SMW's camera (H, `HandleStandardLevelCameraScroll_00F7F4`): the camera scrolls up with an airborne player
    only while `camera_bounce_off_spring_flag` (or flying / climbing / swimming) is set; SMW Mario's wall jump
    (`lm.c`) and MC Mario's wall cling (`mc_smw.c`, cleared on landing) set it.
  - Round 17, MC core (H, `mc_core.c` hero onEnterFrame): an air attack starts only from the jump pose (`!onGround &&
    !pressingAttack && heroFrame == 9 && Key(punch)` -> attack12); the wall cling (frame 17, `walling`) starts when
    `xSpeed > 3` into a wall in the air, and while `walling` the punch does nothing; a fighting hero in the air that
    touches a wall (`onLeftWall` / `onRightWall`, or `hitTest(x +- 30, y - 20)`) goes back to "jump" with xSpeed 0.
    The probes run before the attack check in the same tick.
  - Round 16, SMW level RAM between levels (H, code + `transition_probe.py` / `transition_matrix.py`): a level loaded
    from the map gets RAM the map cleaned: `GameMode0C_LoadOverworld` -> `ClearOverworldAndCutsceneRAM` zeroes $1A-$D7
    (layer positions, mode 7 rotation $36 / angle $38, player and sprite tables) and $13D3-$1BA1 (timers, boss and
    Bowser battle variables, scroll sprites $143E-, layer 3 speeds, `counter_sublevels_entered` $141A,
    `flag_show_player_start` $141D, `flag_active_bonus_game` $1425, `player_riding_yoshi_flag` $187A,
    `flag_use_secondary_entrance` $1B93, `in_yoshi_wings_bonus_area` $1B95). `InitializeLevelRAM` itself only clears
    $71-$93 and 56 bytes from $13D9. Not cleared (kept by vanilla too): `misc_mode7_tilemap_index` $1BA2 (Reznor
    rooms set 2C). `misc_nmito_use_flag` comes from each level header (`kLoadLevelHeader_SpecialLevTable`).
    `LoadLevel` takes the header's player / layer positions only when `flag_use_secondary_entrance` is 0.
    `InitializeLevelRAM` starts the castle / ghost house door walk (player state 0A, `player_pipe_action` 1) for a
    castle / ghost house tileset when `flag_disable_no_yoshi_intro`, `counter_sublevels_entered` and
    `flag_show_player_start` are all 0: a boss room started fresh after another room gets it.
  - Round 16, SMW title (H): game modes 01 Nintendo presents, 02-04 load / prepare, 05-06 fade in, 07 the demo (logo
    up), 08 file select (its menu at y 115-165), 09 erase, 0A player select. Logo lines y ~35-102, demo ground ~170,
    "(c) 1990-1991 Nintendo" ~190.
  - Round 16, MC (H): the main menu (root label `menuMenu`, frame 472) slides shape 90 (placed at depth 2 from frame
    306) into place: the "Mario Combat" fire lettering, a bitmap fill on black, 600x300 stage px (bounds
    -6000..6000 x -3000..3000 twips).
  - Round 15, MC (H): ground friction is `xSpeed -= 1; x += xSpeed` (and the mirror for < 0): a speed between -1 and
    0 comes out positive and moves him the other way; the original's speeds are whole or half numbers, so only the
    ice cheat's 1/5 change produced such speeds. The MC Koopa keeps its `xSpeed` slide after the hurt clip ends
    (`x += walk * moveSpeed + xSpeed`).
**Character switch and settings** (round 2)
- `g_active` = MC Mario in use. A switch request is applied in player state 0 with no sprite lock or message.
- Coming from game modes ≤ 0x0E (title/map) resets it to MC.
- `src/menu.c`:
  - Uses SMW GFX28 (layer-3 font: digits 0x00–0x09, A–Z 0x0A–0x23, `.,×-!` 0x24–0x28), drawn into the PPU
    output, plus outlined extra glyphs.
  - Saves `smw.user.ini` as a full copy of `smw.ini` with the managed keys rewritten.

**Run mode**
- With MC enabled, smw runs C-only (`RM_MINE`), because the RAM comparison against the original ROM code can't pass
  with a changed player.
- Snapshot fix for that mode is in `common_rtl.c`.

## 4. Other
- Sprite 447 is the camera (zooms 200–300% while airborne). It's MC presentation only; SMW keeps its own camera.
- Sprite 532 is a boss; 163 and 445 are enemies. Enemies react to `hero.attack.hit1..hit6` hit shapes. Knockback and damage per hit level are in 532's script (hit1 = 1 dmg … hit4 = 4 dmg).
