SUPER MARIO WORLD  x  MARIO COMBAT
===================================

This is Super Mario World with the Mario from the Flash game "Mario Combat":
- his own sprites, animations, punches, kicks, combos, wall slides and sounds;
- his movement, ported line by line from the original game's ActionScript and
  checked against the original;
- inside the snesrev/smw C reimplementation of Super Mario World;
- plus Mario Combat's own story level (Esc > LEVEL SELECT > MARIO COMBAT) and,
  as cheats, its Koopas and Bowser.
The title screen says so: under the SUPER MARIO WORLD logo a "+" and Mario
Combat's own logo (its fiery main-menu lettering).


SETUP
-----
1. Put your own Super Mario World (USA) ROM in this folder.
   - Any name works: smw.sfc, "Super Mario World.sfc", ...
   - It must be the unmodified USA ROM, 512 KB.
   - SHA-1: 6B47BB75D16514B6A476AA0C73A683A2A4C18765
   - A 512-byte copier header is fine.
   The ROM is NOT included. Use a dump of a cartridge you own.
2. Run smw.exe.
   - Nothing needs to be installed. The folder can be copied anywhere.
   - On start the game builds its data from your ROM (smw_assets.bps).
   - Saves go into the "saves" folder next to the exe.


CONTROLS (keyboard, default smw.ini)
------------------------------------
Left / Right       walk; keep holding to reach Mario Combat's run speed.
                   Mario skids when you turn around at speed.
Up, Z or X         jump.
                   - Hold for a full jump; a quick tap is a short hop.
                   - Up jumps exactly like the Up-arrow jump in Mario Combat.
A                  PUNCH (the same A key as Mario Combat). Press again with the
                   right rhythm for combos.
                   - Keep A held to RUN (speed builds up like SMW's run, up to
                     P-speed) and to PICK UP shells, blocks, keys... you touch.
                   - Let go to kick / throw what you carry.
Left/Right + A     dash punch.
Down + A           low sweep.
A in the air       air attack: a spin kick, steerable with Left/Right. It works
                   against walls and blocks too: while sliding down a wall,
                   falling beside a brick or just after bumping one.
S                  SMW's X button: hold to run and to grab/carry (no punch).
                   - Fireball with a Fire Flower, cape spin with a Feather.
Up                 enter doors, climb vines.
Down               enter pipes. Walk into a sideways pipe to enter it.
C / V              L / R (scroll the screen).
Enter              start / pause.
Right Shift        select (drop the reserve item).
Q                  SWITCH MARIO: Mario Combat Mario <-> Super Mario World Mario.
                   Every level starts as Mario Combat Mario.
Esc                SETTINGS menu (works any time, also pauses the game).
F3                 FPS counter on / off (any time).
F11                fullscreen on / off (any time; Alt+Enter too).

Wall moves: jump fast into a wall and Mario sticks to it and slides down.
Press jump while sliding to wall-jump away.

Combos
- Punches chain into a combo: jab -> straight -> hook -> uppercut / kick finish.
  - Tapping A quickly or slowly picks different branches.
  - Some of them drive Mario forward.
- Holding a direction during a combo switches to the dashing branches,
  including the 5-hit rush (A, A, Right+A, then keep tapping A).
- Every move, its timing window and its hit box are the original game's.

Gamepad (XInput)
- A or B: jump
- X: punch (hold to run and to carry)
- Y: grab / run / fire
- D-pad / left stick: move
- LT / RT: L / R
- Start: pause
- Back: select
- RB (right bumper): SWITCH MARIO
- L3 or R3 (click a stick): SETTINGS menu

Window and other keys
- Alt+Enter or F11: fullscreen
- F3: FPS counter
- Ctrl+Up / Ctrl+Down: window size
- P: pause
- Shift+F1..F10: save state; F1..F10: load state (except F3, the FPS counter)
- Ctrl+R: reset
- Tab: fast-forward (hold)


SETTINGS MENU
-------------
Esc on the keyboard, or L3 / R3 on a gamepad, opens it at any time. The game
pauses while it is open.
- Move with the arrow keys / D-pad / stick.
- Enter or A selects; Left / Right change a value.
- Esc or B goes back.
Pages:
- VIDEO
  - Display: window, fullscreen or exclusive fullscreen.
  - Window size: 1x to the biggest size your screen fits.
  - Aspect ratio, sharp or smooth pixels, VSync, FPS counter.
- SOUND
  - Master volume.
  - Mario Combat sounds and their volume.
  - Output: stereo, or mono (both speakers play the same sound; handy with
    one speaker or one ear).
- GAMEPLAY
  - Which Mario you play.
  - Up to jump.
  - Life bar on/off and damage per hit.
  - Show attack hit boxes.
  - FILE 3 = 100% SAVE: save file 3 becomes a completed game (96 exits, every
    path open, Special World beaten, all switch palaces). Your own file 3 is
    kept safe in saves\file3_backup.srm and put back when you switch it off.
  - In background: what happens when you switch to another window. PLAY ON
    (as before), MUTE (the game keeps going without sound) or PAUSE (the
    game and its sound stop until you come back).
- CONTROLS
  - Remap every keyboard key and gamepad button (including Switch Mario,
    Settings, FPS counter and Fullscreen): pick an action, then press the new
    key or button (Esc cancels).
  - "Reset to defaults" restores the original layout.
  - Rumble (OFF / LIGHT / FULL): the gamepad shakes when Mario is hurt or
    loses a life, while the screen shakes (Thwomps, ground pounds, Bowser)
    and, softly, when a Mario Combat punch or kick lands. Changing it gives
    a short sample.
  - Stick deadzone (10% to 50%, normally 30%): how far the left stick has to
    be pushed before Mario moves. Lower it for a worn or light stick, raise
    it if Mario drifts on his own.
- CHEATS: switch them on and off here (saved with your settings). A gold star
  marks the ones that are on; ALL CHEATS OFF turns them all off. The line
  under the list explains the highlighted cheat. (MC) = Mario Combat Mario only.
  MARIO
  - Infinite MC life / No damage / Invincible (star power, no star music)
  - One-hit mode: any hit is fatal
  - No knockback (MC), or Knockback (MC) 1X to 20X: how far hits throw him
  - Power-up lock: always mushroom, feather or fire flower
  - Always Yoshi / Flying Yoshi
  - Life steal (MC): every punch that lands gives back half a point of life
  MOVES
  - Super punch (MC): every punch knocks out any normal enemy at once
  - Double jump / Infinite jumps / Moon jump (hold jump to keep rising)
  - Ground pound: Down in the air
  - Wall jump for SMW Mario (MC Mario always has it)
  - Air dash: L / R in the air (keyboard C / V)
  - Always P-speed
  - Fire punch (MC): punches also throw fireballs
  - Instant hits (MC): attacks hit on their first frame, combos as fast as
    you press
  - Block buster (MC): punches break grey and used blocks
  ENEMIES
  - Triple enemies: every normal enemy comes with two more
  - MC Koopas: Koopa Troopas become Mario Combat's Koopas
  - King Koopas: Koopa Troopas become Mario Combat's Bowser (SMW Mario: spin
    jump on him, a plain jump bounces off)
  - MC hit reactions (MC): enemies take damage and fly from punches like the
    MC Koopas
  - Punch anything (MC): punches also hurt Boos, Thwomps, Wigglers...
  - Stomp anything: even spiky enemies can be stomped
  - Coins from enemies / Fast enemies / Chasing enemies / Jumping enemies
    (they jump when Mario does) / Respawning enemies
  Bosses are never changed by the enemy cheats.
  WORLD
  - Infinite time / Infinite lives / All switch blocks
  - Replay castles: enter beaten castles and fortresses again
  - Slow motion / Mirror mode / Slippery levels / Low gravity
  - Pit rescue: a fall costs a hit instead of a life
  - Coin magnet: nearby coins fly to Mario
  - Open map: walk anywhere on the map and play any level (your save is not
    changed)
  Pits, lava and being crushed still cost a life.
- LEVEL SELECT: play any level of the game from its beginning.
  - MARIO COMBAT (at the top of the page): Mario Combat's own level, all of
    its story mode, drawn with Mario Combat's own art (its stone, Bowser
    statues, pillars, chains, castle wall and sky): the castle wall over the
    lava, the climb inside to the red door, the hall to the pipe, the lift up
    to the two caged Koopas, and Bowser on his bridge (to Mario Combat's own
    Bowser music).
    - Its geometry is Mario Combat's own (one Mario Combat pixel-art pixel =
      one SNES pixel), with Mario Combat's Koopas where the original has them
      and Mario Combat's Bowser at the end (whatever the MC KOOPAS / KING
      KOOPAS cheats are set to).
    - Where Mario Combat says "press Space" (a door, the pipe, the lift's
      switch), press Jump, Punch or Up: a white arrow shows over the spot when
      Mario stands there. SMW's door (Up) and pipe (Down) work too.
    - Play it as either Mario. It is built for Mario Combat Mario's jumps and
      wall jumps: SMW Mario gets the wall jump in it (slide down a wall
      holding towards it, press Jump), and a few extra brown blocks in the
      climb inside give him steps up.
    - As in Mario Combat, Mario can't walk past an area's ends and the camera
      stops there. SMW Mario stomps Bowser only with a spin jump.
    - A fall into lava starts the area again (a life less). Beat Bowser for
      "MARIO COMBAT CLEAR!"; then you are back on the map as it was (it is
      practice, like the boss rush).
  - BOSS RUSH: the bosses back to back, each in its
    own room, against the clock.
    - START BOSS RUSH starts the first fight (in a level, or on the map with
      Mario standing still).
    - BOSSES: CASTLES = the seven Koopa Kids, then Bowser (8 fights);
      EVERY BOSS = also Big Boo and Reznor (once: his first fortress), in the
      order the game has them (10 fights).
    - ON DEATH: RETRY BOSS = the same fight starts again (as you entered it);
      END RUN = the run is over.
    - Each fight starts with "BOSS 2/8: MORTON"; the bottom corner shows the
      fight and the run's time (SMW's pause stops the clock). A beaten boss
      shows "MORTON DOWN!" with the time, and the next fight starts a moment
      later. After the last one: "BOSS RUSH CLEAR!" with your time, then the
      map.
    - It is practice: Yoshi waits outside, and at the end you are back on the
      map as it was, with your lives, coins, item and power-up as they were
      when the run started. Your save file is not changed. Choosing a level
      from the level select ends the run.
    - Every fight starts exactly as when you reach that boss in the game,
      whatever the fight before it was (no tilt left from Iggy's or Larry's
      platform, no door scene after Big Boo's ghost house), with its music.
  - Its first page lists the worlds in the order you reach them (Yoshi's
    Island, Donut Plains, Vanilla Dome, Twin Bridges, Forest of Illusion,
    Chocolate Island, Valley of Bowser, Star World, Special Zone), then BONUS
    ROOMS and UNUSED LEVELS. Each world lists its levels in the order they
    come up in normal play (secret levels right after the level that leads
    to them), with the level number on the right.
  - Every level is there: all 76 levels of the maps, the intro, the 1-UP
    bonus game, Yoshi's wings area (both versions), the title screen's
    level, and the levels that are in the game but never reached, marked
    UNUSED: the test level that fills the empty level slots, the copies of
    Donut Plains 1, level 108, an unused Reznor fight, three unused Koopa Kid
    fights, and copies of four Koopa Kid rooms and of Bowser's room. (A
    level's rooms, behind its pipes and doors, are reached from the level.)
  - Choose a level in a level or on the map (with Mario standing still).
    The menu closes and the level starts like from the map: "MARIO START",
    the castle and ghost house door scenes, the level's own timer, never
    from the midway point. In a level, whatever was going on (dying, the
    goal walk, a message, a pipe, star power) simply stops; Mario keeps his
    power-up, his Yoshi and, for Mario Combat Mario, gets a full life bar.
    The new level starts clean, as when you enter it from the map, even from
    a room behind a pipe or door, a boss room or a bonus room.
  - A level played from here is practice: when it ends (goal, death,
    Start+Select, a beaten boss) you are back on the map exactly as it was
    before the first level you chose: nothing is marked beaten, no path
    opens, no castle falls, no midway point is kept, and your save file is
    not changed. Coins, lives, score and power-ups count as usual. You can
    choose another level while playing one; the map still comes back as it
    was before the first.
  - On the title screen and in the file menu the menu says START OR LOAD A
    GAME FIRST; during a fade or with Mario walking on the map it asks you
    to wait a moment.
- RESET ALL SETTINGS (also turns the cheats off) and QUIT GAME.
Changes are saved to smw.user.ini when you close the menu, and they are loaded
on the next start. smw.ini keeps the defaults. To go back to the defaults,
delete smw.user.ini.


HOW IT PLAYS
------------
- Movement, jump arcs, speeds, combo timing and animations are Mario Combat's.
  - They run at Mario Combat's own 50 frames per second inside SMW's 60.
  - This keeps every move exactly as long and as far as in the original.
  - One Mario Combat pixel-art pixel = one SNES pixel.
- Life: Mario uses Mario Combat's life bar (top left, "SUPER MARIO").
  - Each hit costs 0.5 of his 10 life, as a hit from one of Mario Combat's own
    enemies: he takes 20 hits (GAMEPLAY > DAMAGE PER HIT, HitDamage in
    smw.ini).
  - Any power-up (mushroom, fire flower, feather, star) fills the life bar
    back to full, even when Mario already has it.
  - Life refills when you enter a level from the map. Going through a pipe
    or door inside a level keeps it.
  - Falling into a pit or running out of time still loses a life.
  - Set LifeBar = 0 in smw.ini for SMW's normal big/small rules.
- Punches and kicks knock enemies out the way SMW's cape does.
  - The hit boxes are a little bigger than the original's, so a punch that
    looks like it lands does land (SMW's enemies walk straight into you).
  - Enemies that ignore the cape but can be stomped (Banzai Bill...) are
    knocked out as if stomped. A punch pops a bubble with an enemy inside.
  - Enemies SMW Mario can pick up after jumping on them (Goombas, Bob-ombs,
    Buzzy Beetles, Mechakoopas, Koopas) fall over instead, as after a stomp:
    a Koopa leaves its shell. Pick them up with the run button (S) and throw
    them, or punch one that lies there to kick it away.
  - Bosses take punches: each punch or kick that lands counts as one of
    SMW's own hits for that boss (a stomp for the Koopa Kids, a hit from
    below for Reznor, a thrown block for Big Boo, a Mechakoopa on the head
    for Bowser), with the boss's own moments when it can't be hurt: Morton,
    Roy and Ludwig bounce punches off in their shells and on walls, Iggy and
    Larry slide into their shells (push them into the lava), Big Boo only
    while he can be seen, Bowser in his clown car (jump up and kick). Bowser
    takes two punches for each Mechakoopa hit: the first one only shakes him
    (a kick sound and a star where the fist lands). Mechakoopas thrown at
    him count as usual. Other invincible enemies (Thwomps, Boos...) still have
    to be beaten SMW's way (or with the Punch anything cheat).
  - Enemies that take more than one stomp take more than one normal hit:
    a Rex shrinks on the first hit, a Chargin' Chuck takes three, a
    Dino-Rhino turns into a Dino-Torch. Strong attacks knock them out at once:
    the combo finisher (uppercut / kick), the low sweep (Down + A) and the air
    spin kick.
- Jumping: a standing jump is Mario Combat's own. A running jump takes off
  faster the faster he runs, in the same ratio as SMW Mario's, so the gaps
  and ledges SMW's levels expect a running jump for are reachable.
- Walls: a jump next to a wall carries Mario up along it. From the top of the
  jump he clings and slides down; press jump again to wall kick (holding jump
  from the take-off doesn't kick). The sides of solid sprites are walls too:
  flying ? blocks (before and after they are hit), message boxes, Reznor's
  platforms and the other block-like platforms. The edge of an
  auto-scrolling screen holds him like SMW Mario but is not a wall to cling to.
- Platforms: Mario rides moving platforms as SMW Mario does (Reznor's wheel,
  chained, line-guided, floating, falling and flying platforms), lands on
  them from any height, and bounces off a sprite that knocks SMW Mario
  sideways (Reznor's side, a Koopa Kid's shell). Solid blocks (message
  boxes, flying ? blocks, turn block bridges...) stop his head from below;
  platforms you can only stand on let him jump up through them. Jumping off
  a rope, vine, net or Yoshi goes as high as SMW Mario's jump (let go of the
  button early for a lower one).
- Doors: Up enters a door when Mario stands in front of it (elsewhere Up
  jumps, with "Up to jump" on).
- Blocks: Mario stops at walls of blocks and slides down them; he can't get
  stuck inside them. Hitting a block from below hits it once per jump (keep
  jump held and he won't jump again until you let go).
- Punches and kicks hit blocks too: one block per hit. ?-blocks, coin and
  power-up blocks, the yellow / green ! blocks with an item and the flying
  ?-blocks give their coin / item as from a head bump, and turn blocks shatter
  as under a big Mario's spin jump. An attack in the air starts and plays out
  against a wall too (also while clinging to it), so it can break the bricks
  there.
- Castle and ghost house doors: when a castle or ghost house starts, Mario
  walks (or runs) along the ground to its door and goes in, as SMW Mario does.
- Points: hits score like SMW's stomps. Hitting the same enemy again within 2
  seconds (juggling a shell) or several enemies without landing goes up
  200, 400, 800, 1000, 2000, 4000, 8000 and then 1-UPs.
- Cape: with a Feather, the run / grab / fire button (S, SMW's X) does SMW's cape
  spin; Mario turns with SMW's spinning sprite and the cape hits enemies and
  blocks as SMW Mario's does.
- Running: hold the punch (A) or run (S) button while moving. Mario speeds up
  past his normal run, and after about a second at full speed the P-meter is
  full (top speed). With a Feather, Mario wears SMW's own cape (it flutters as
  he runs and jumps), and a jump at P-speed takes off and flies exactly like
  SMW Mario (keep holding the run button to stay up).
- Mario's colours follow his power-up like SMW Mario's: white and red with a
  Fire Flower (with SMW's black outline), the rainbow flash with a Star.
- Mario is drawn in front of or behind other sprites exactly as SMW Mario is
  (e.g. in front of the fire in Yoshi's House, behind pipes).
- Riding Yoshi, swimming, climbing and pipes use SMW's movement. Mario Combat
  has no art for sitting on Yoshi, climbing, cape flight (dive / pull-up), the
  P-balloon or running up walls, so Mario shows SMW's own poses there (with
  his power-up colours) and switches back to Mario Combat's art right after.
- Stomping an enemy, springboards, note blocks and other things that throw
  Mario upward bounce him as high as they bounce SMW Mario.
- Munchers and castle spikes hurt when Mario walks into them from the side.
- Mario Combat's sounds are mixed at the loudness of SMW's own effects
  (SOUND > MC SOUND VOLUME changes them).
- The overworld map still shows SMW's small map Mario.
- Press Q (RB) to switch to the original Mario at any time, or set Enable = 0
  in the [MarioCombat] section of smw.ini to turn Mario Combat Mario off.


CREDITS
-------
- "Mario Combat" (Flash game, rankz.armorbot.com): Mario's sprites, animations,
  move set, sounds and movement code.
- Super Mario World: Nintendo.
- snesrev/smw (MIT License, see LICENSE.txt): the C reimplementation, with PPU
  and DSP from LakeSnes.
- SDL2 (zlib license), statically linked.
- Made almost entirely by an AI (Claude, via Claude Code) under a human's
  direction: it's AI slop, and works as it is.

LICENSE AND DISCLAIMER
----------------------
The work added by this project is public domain (The Unlicense): do whatever
you want with it. That can't cover what belongs to others: snesrev/smw is MIT
(LICENSE.txt), Mario Combat's art, sounds and music belong to its creators,
and Super Mario World and Mario belong to Nintendo.
This is an unofficial fan project, not affiliated with or endorsed by Nintendo
or the makers of Mario Combat. No ROM is included: never share it with one.
It comes as is, without any warranty.
