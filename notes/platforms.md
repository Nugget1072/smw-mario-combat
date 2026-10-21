# Mario Combat Mario on SMW's platforms and moving blocks (round 19)

How MC Mario interacts with every kind of platform, moving block and ride in Super Mario World, as checked against
SMW's own Mario (the reference: `setting mc 0`). Confidence labels as elsewhere in `notes/` (H = measured).

## How it works
- **Standing and riding** (H): SMW's own sprite routines put the player on a platform (SolidSpriteBlock and the
  platforms' own routines set `misc_player_on_solid_sprite`, his height and the platform's move). MC Mario counts that
  as ground: he stands, walks, runs and attacks on it and is carried along, up, down and round as SMW Mario is.
- **Landing** (H): from any height. MC's fall has no top speed, so a fall that would skip a platform's catch band
  (about 5 px for SolidSpriteBlock, the brown chained platform's own) is stopped inside it (`ClampIntoFloor`, round 18).
- **From below** (H): solid blocks stop his head (one bump per jump, the held jump used up, as with blocks);
  platforms solid on top only let him through, and he can land on them from above.
- **Sides** (H): the solid ones are walls: he stops at them, clings and wall-jumps (`SolidSpriteWall`, round 18). Those
  solid on top only ($190F bit 0) are no walls.
- **Pushed and knocked** (H): a platform's push out of its side, and SMW's sideways knockbacks (Reznor's side, a
  Koopa Kid's or a Chargin' Chuck's bounce), move MC Mario as they move SMW Mario (`McSmw_SideLaunch`, round 18).
- **Jumping off** (H): his own Mario Combat jump: higher and longer than SMW Mario's (a full standing jump about 98 px
  against 70), so a fast platform may have moved on when he comes down; a short hop (tap jump) lands back on as SMW
  Mario's jump does.
- **Rides SMW drives** (H): ropes, vines, nets, Yoshi, Lakitu's cloud, swimming: SMW moves Mario (drawn as SMW's own
  Mario where MC has no art). Jumping off one: MC takes SMW's take-off speed converted to reach SMW's height, and
  letting go of jump early cuts the rise as it does SMW's (round 19; before, he rose almost three times as high).

## Every kind
Test columns: **ride** = stand / ride 140 frames, short walk, two hops (`platform_ride_test.py` in open sky,
`platform_levels_test.py` where the game puts it); **below** / **above** = jumping into it from below, dropping onto it
from 100 px (`platform_contact_test.py`). "as SMW" = the same as SMW Mario's.

| id | kind | ride | from below | from above | sides |
|---|---|---|---|---|---|
| 2F | springboard (item) | bounces as SMW Mario; hold jump for the high bounce | bumps a rising head under it (SMW's rule) | lands, bounces | — |
| 41 / 42 / 43 | dolphins | as SMW (level 002) | through | lands | — |
| 49 | growing / shrinking pipe end | as SMW | bumps | lands | wall |
| 55 / 56 | checkerboard / rock platform, horizontal | as SMW | through | lands | — |
| 57 / 58 | checkerboard / rock platform, vertical | as SMW | through | lands | — |
| 59 / 5A | turn block bridges | as SMW | bumps | lands | wall |
| 5B / 5D | platforms floating in water | as SMW (levels 11C, 102) | through | lands | — |
| 5C | checkerboard platform that falls | as SMW (falls with him) | through | lands | — |
| 5E | orange platform that goes on forever | as SMW | through | lands | — |
| 5F | brown platform on a chain (swinging) | as SMW, slides off as he does | through | lands | — |
| 60 | flat switch palace switch | as SMW | through | lands | — |
| 61 | skull raft on lava | as SMW (level 10A) | — | lands | — |
| 62 / 63 | line-guided platforms | as SMW (level 005) | — | lands | — |
| 64 | line-guided rope | climbs as SMW Mario (SMW's sprite); jumps off to SMW Mario's height | — | — | — |
| 6B / 6C | wall springboards | as SMW | through | lands | — |
| 6D | invisible solid block | as SMW | bumps | lands | wall |
| 83 / 84 | flying ? blocks (fresh / hit) | as SMW; a hit block stops flying | bumps (gives its item) | lands | wall: cling, wall jump |
| 87 | Lakitu's cloud (empty) | flies it as SMW Mario (SMW's sprite), jump to leave | — | gets in | — |
| 8F | scale platforms | as SMW | through | lands | — |
| 9C | flying grey turn blocks | as SMW | through | lands | — |
| A3 | grey platform on a chain (rotating) | as SMW | through | lands (as SMW when it is there) | — |
| A9 | Reznor's platforms (Reznor knocked off) | rides the wheel round (round 18) | bumps | lands | wall |
| B1 | creating / eating block | as SMW | bumps | lands | wall |
| B7 / B8 | carrot top lifts (slanted) | as SMW | through | lands | — |
| B9 | message box | as SMW | bumps (opens the message) | lands | wall: cling, wall jump |
| BA | timed lift | as SMW (falls when the time is up) | through | lands | — |
| BB | moving castle block | as SMW | bumps | lands | wall |
| BC | Bowser statue | as SMW | bumps | lands | wall |
| BF | Mega Mole | rides it as SMW (its own routine, y - 42) | — (hurts from the side, as SMW) | lands | — |
| C0 | sinking lava platform | as SMW | through | lands | — |
| C1 | flying grey platform with wings | as SMW | through | lands | — |
| C4 | falling grey platform | as SMW (falls with him) | through | lands | — |
| C8 | light switch block | as SMW | bumps (switches) | lands | wall |
| layer 2 | moving layers (Valley of Bowser pillars, castle layer platforms) | carried; no crush (round 8, pilot `layer2_pillar`) | | | walls |
| mode 7 | Iggy / Larry's tilting platform, Reznor's wheel | as SMW (rounds 2, 16, 18) | | | |

Differences that are not bugs (H): MC Mario's own jump and fall (from a standstill his fall starts a little slower):
off a fast or swinging platform he lands at another moment than SMW Mario, sometimes past it; a springboard falling
onto a rising Mario bumps his head when it meets it lower down (SMW's own rule). Kinds whose rooms kill a warped-in
Mario at once (wall springboards in 119, the invisible block in 0C9, the scale platforms in 00C, the creating block in
020, the carrot lifts in 01D, the light switch in 10E) were checked spawned in open sky instead.
