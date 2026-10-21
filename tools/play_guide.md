# Playing SMW as Mario Combat Mario: guide for test players (agents)

You play Super Mario World levels as **Mario Combat Mario** (a Flash-game Mario with punches, kicks and a life bar,
inserted into SMW) and report every problem you notice. The game is turn-based for you: it is frozen while you think.

Run everything from the project folder (the repository's root) with
`tools\Python3.14.8\python.exe -I tools\scripts\play.py ...` (PowerShell).

## Commands
| what | command |
|---|---|
| start a level from its beginning | `play.py start <session> <level>` (level = 3-digit number from your list, e.g. `105`) |
| move | `play.py do <session> "hold RIGHT B; wait 20" "hold RIGHT; wait 30"` |
| move and see the in-between frames | add `--every 10` (a strip image, one frame every 10) |
| remember a good moment | `play.py checkpoint <session> <name>` |
| go back to it (after a death or a mistake) | `play.py rewind <session> <name>` (`start` = the level's beginning) |
| end the session | `play.py stop <session>` |

Use one session name per level, e.g. `<your agent name>_105`. Each call prints `STATUS {...}` and `NOW: <png>`
(the screen after the move). **Look at the NOW image (Read tool) after every move** and at strips when something
happens fast.

- Moves are `hold <buttons>` and `wait <frames>` (60 frames = 1 second; at most 900 frames per call). Held buttons
  stay held until the next `hold`; `hold` alone releases everything. Several moves in one call are fine.
- Buttons: `B` jump (hold longer = higher), `A` jump too, `Y` punch / kick (Mario Combat attacks; combos when tapped
  repeatedly; `DOWN`+`Y` low attack; `Y` in the air = air kick), `X` run (hold while moving), grab / carry items, and
  with a fire flower throw fireballs / with a feather spin the cape. `UP` also jumps (setting "Up to jump").
  `LEFT` / `RIGHT` walk. `START` pauses.
- Tapping a button: `"hold Y; wait 3" "hold; wait 8"`. Running jump: `"hold RIGHT X; wait 30" "hold RIGHT X B; wait 25"`.
- STATUS fields: `in_level`, `last_level_result` (`goal`, `secret goal`, `died`, `left`), `x`, `y` (level pixels,
  y grows downward; a 16-pixel block is one tile), `screen_x/y` (where Mario is on screen), `on_ground`,
  `mc_life` (life bar, 0-10), `power_up` (0 small, 1 big, 2 cape, 3 fire), `riding_yoshi`, `message_box`,
  `sprites_near` (enemy/object ids with their distance `dx`/`dy` from Mario).
- When a level ends (goal or death) the game goes back to the overworld map (`in_level` false). Use `start` again to
  retry the level, or `rewind` to a checkpoint inside it. Lives are infinite.

## How to play well
- Make small moves (10-60 frames) near enemies and gaps; longer ones (60-200) on easy ground.
- Checkpoint often (before every jump over a pit, before a boss). After a death, rewind instead of restarting.
- Punch enemies (`Y`) or jump on them. Mario Combat Mario has a life bar instead of shrinking: hits cost life, any
  power-up refills it, and when it reaches 0 he dies.
- Pits, lava, crushing and time-out still kill.
- To finish a level, reach the goal tape (or the boss / switch / door that ends it). Castles: go through the door at the
  end and beat the boss. Ghost houses: find the exit door. Switch palaces: hit the big switch.
- Jumps: hold `B` longer for a higher jump. Running (`X` held) for a second before jumping makes the jump higher and
  longer, as in SMW: run-ups clear gaps and tall steps that standing jumps don't. Release the direction to drop
  straight down; tap `B` again in the air next to a wall for a wall kick.
- When a wall or gap seems impossible, look for the level's own way on, as in SMW: a pipe (stand on it and hold
  `DOWN`, or walk into a sideways one), a door (stand in front of it and press `UP`), a vine out of a block (punch or
  bump `?` blocks), a P-switch, a moving platform you have to wait for, or a way up (y gets smaller going up; some
  levels are vertical).
- You may compare with SMW's own Mario: `play.py start <session> <level> --smw` plays the level as SMW Mario (same
  buttons, but `Y` runs instead of punching). If SMW Mario can't do it either, it is the level, not a bug.
- If you are stuck on one section after about 20 tries, write it down as "not beaten" (with where and why) and go on to
  your next level. Don't spend more than ~200 moves on one level.

## What to report (and what not)
Report anything where the game or Mario Combat Mario behaves wrongly. Examples:
- Mario falls through floors, gets stuck in walls or blocks, is pushed or teleported oddly, can't go somewhere SMW
  Mario could (a pipe, a door, a vine, a net, a platform);
- an enemy, block or item reacts wrongly to a punch, stomp or touch (or not at all), damage when there should be none
  (or the opposite);
- Mario dies with no visible reason, the level ends unexpectedly, softlocks (nothing responds), the game crashes
  (the tool prints ERROR);
- graphics glitches: Mario drawn in the wrong place / wrong layer / missing art (not the blink below), garbage tiles,
  flicker;
- the life bar / HUD / camera behaving wrongly.

**Not issues:** after a switch palace's switch is pressed, its message stays for about 11 seconds (no button closes
it) and then the level ends by itself, as in SMW; Mario blinks (is invisible on some frames) for ~2 s after being hit; when riding Yoshi, climbing,
flying with the cape or in a P-balloon Mario is drawn as SMW's own Mario (Mario Combat has no art for that); Mario has a
life bar instead of shrinking; you dying because the level is hard.
Also checked and **not issues** (found by earlier testers):
- a punch breaking a yellow turn block gives score (the number under the coin counter), not coins;
- auto-scrolling levels: the screen edge pushes Mario and he can't run ahead of it;
- pits and lava kill at once, and the camera does not follow below the bottom of the level (a narrow gap at the
  bottom of the screen is usually a pit). Lava can look blue or purple in castles;
- some ropes, columns and thick walls are solid for SMW Mario too;
- `UP` is a jump (except in front of a door, where it enters the door);
- Mario Combat Mario's picture is bigger than his hit box: overlapping an enemy's picture is not a touch. His punches
  reach at chest height (`DOWN`+`Y` for low ones);
- STATUS `x` stays the same while Mario is blocked, even though the screen may still scroll.

For each issue write: a short title, the level, what happened, what you expected (what SMW Mario would do), how to repeat
it (session name, checkpoint or "from start", and the exact moves), the screenshot path(s), and how often it happened.

## Your report
Write `work/play/reports/<your agent name>.md` **after each level** (so nothing is lost):

```
# <agent name>
## <level> <name>: BEATEN (goal) | BEATEN (secret goal) | NOT BEATEN (<where / why>)
moves used: N, deaths: N
### Issues
1. <title>
   - what: ...
   - expected: ...
   - repeat: session <name>, rewind <checkpoint|start>, then `play.py do ... "<moves>"`
   - shots: <png paths>
   - seen: N times
(or "none")
```
Finish with a short summary list of all issues. Stop all your sessions at the end.
