SUPER MARIO WORLD  x  MARIO COMBAT
===================================

This is Super Mario World with the Mario from the Flash game "Mario Combat":
- his own sprites, animations, punches, kicks, combos, wall slides and sounds;
- his movement, ported line by line from the original game's ActionScript and
  checked against the original;
- inside the snesrev/smw C reimplementation of Super Mario World.
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
- CHEATS (on/off, or a choice of value; saved with your settings). A gold star
  marks the ones that are on; the CHEATS page shows how many are on and has ALL CHEATS OFF.
  The line under the options explains the highlighted one.
  - MARIO CHEATS:
    - Infinite MC life, No damage (enemies can't hurt either Mario),
      Invincible (star power without the music).
    - No knockback (MC): hits still cost life, but Mario Combat Mario is not
      knocked back or stunned (also not by a Koopa Kid's ground pound).
    - Knockback (MC): how far hits knock Mario Combat Mario, from 1X
      (normal) to 20X (twenty times as far; left / right picks the step).
      A light hit (SMW's enemies) pushes him along the ground, a heavy one
      (a Mario Combat enemy's strong attack) also throws him up, so at the
      top steps he can be knocked a long way, even into a pit. No knockback
      wins when both are on.
    - One-hit mode: any hit is fatal (a challenge; No damage and Infinite MC
      life still protect).
    - Power-up lock (mushroom / feather / fire flower; the row shows the item).
    - Always Yoshi: every level and room starts on Yoshi (except where SMW
      itself leaves Yoshi outside, like castles and ghost houses).
    - Flying Yoshi: Yoshi always has wings (as in SMW's wings bonus area):
      tap jump while riding to fly.
    - Life steal (MC): every punch or kick that lands on an enemy gives
      Mario Combat Mario half a point of life back.
  - MOVE CHEATS:
    - Super punch (MC): every punch and kick is a strong hit. It knocks out
      any ordinary enemy at once, even ones SMW makes immune to the cape and
      stomps (Thwomps, Boos, Urchins, Hotheads...). Bosses are not affected.
    - Double jump: both Marios can jump once more in the air (A: a spin jump).
    - Infinite jumps: jump again in the air as often as you like.
    - Ground pound: press Down in the air. Mario stops, spins once in the
      air (Mario Combat Mario somersaults in his own curled-up tumble; SMW
      Mario in his spin-jump spin), then drops straight down (curled up /
      ducking); the landing shakes the screen, puffs smoke to both sides and
      knocks out enemies standing on the ground (SMW's cape-dive pound).
    - Wall jump (SMW Mario): hold toward a wall in the air to slide down it,
      press jump to kick off (Mario Combat Mario already wall jumps).
    - Moon jump (hold jump to keep rising), Always P-speed.
    - Air dash: in the air, L dashes left and R dashes right (keyboard C / V,
      gamepad triggers by default): Mario shoots about five blocks straight
      across without falling, with the cape's swish and a puff of smoke.
      Once per jump (landing gives it back); a jump, a ground pound, an
      attack or a wall ends it early: Mario stops at the wall (a pipe's side
      too) and goes on at normal running speed. Both Marios.
    - Fire punch (MC): every punch and kick also throws SMW's fireball,
      whatever the power-up (two at a time, as SMW allows).
    - Instant hits (MC): every attack's hit box comes out on its first
      frame, and a new press after a hit goes straight into the next move
      of the combo (or, after a finisher, a new attack), so combos are as
      fast as you press. Pressing a little later still picks Mario Combat's
      later combo branches. While attacking, Mario's lunges don't add up
      beyond his running speed.
    - Block buster (MC): punches and kicks also shatter grey (cement) and
      used blocks, one block per hit (never the floor Mario stands on).
  - ENEMY CHEATS:
    - Triple enemies: every ordinary enemy a level loads comes with two more.
      Walkers get them beside them on the same floor, fliers and swimmers also
      above / below, and enemies that live in terrain where the same terrain
      is (moles in dirt, plants in another pipe or taking turns in their own,
      net Koopas on the net, Podoboos in lava, line-guided enemies on the
      line). Bosses, Koopa Kids, Magikoopa, Lakitu and Hammer Bros are left
      alone. SMW only allows a few sprites at once per level: when the screen
      is full, a copy waits for a free slot (up to 10 seconds), and one slot
      always stays free so items, keys and P-switches still appear.
    - MC Koopas: SMW's Koopa Troopas become Mario Combat's Koopas (its art,
      animations, AI and punches; 10 life, Mario Combat's damage and
      knockback per hit). Stomps hit them; a spin jump, shells, fire, the
      cape, a slide, a star and Yoshi work on them as on any Koopa. The cape
      hits them at most once every half second.
    - King Koopas: SMW's Koopa Troopas become Bowser, Mario Combat's final
      boss (50 life, half of Mario Combat's 100; fire breath, jump attacks
      with shockwaves, his health bar). Mario Combat Mario's attacks do Mario Combat's damage; SMW Mario
      hurts him with fireballs (light), the cape (medium, at most once every
      half second), spin jumps, Yoshi, shells, a cape dive, a slide or the
      star (strong). A plain jump only bounces off his spiked shell: land on
      him with a spin jump.
      Falling into a pit or lava teleports him back to the middle of the
      screen. Defeating him gives a 1-UP. Can't be on with MC Koopas.
    - MC hit reactions: SMW's ordinary enemies take Mario Combat Mario's
      punches and kicks the way the MC Koopas do. Each has 10 life and loses
      Mario Combat's damage per hit; jabs push it back, combo finishers and
      kicks send it flying (it bounces off walls, tumbles and bounces when it
      lands, slides until its push is spent, and can be knocked into pits or
      off the screen). Then it carries on as before. Chargin' Chucks and
      Monty Moles (once out of the ground) fly back too. Fliers are pushed
      through the air; enemies that are in a pipe, the dirt, a net, lava or
      on a line are stunned in place. The hit that empties its life knocks it
      out the normal way (or knocks it over, for the ones Mario can pick up).
      Bosses, the Wiggler and enemies a punch can't defeat (Thwomps, Boos...)
      are not affected (with Punch anything on, they are).
    - Coins from enemies: every defeated enemy pops out a coin.
    - Stomp anything: any enemy can be stomped from above, even spiky ones
      (Spinies, Spike Tops, Piranha Plants...); it reacts as SMW's stomped
      enemies do (flipped, stunned or knocked out).
    - Punch anything (MC): Mario Combat Mario's punches and kicks hurt the
      enemies that nothing but a star can (Boos, Eeries, Big Boos, Fishin'
      Boos, Wigglers, Thwomps, Thwimps, Urchins, Hotheads, Sparkies, Blarggs,
      Grinders, Torpedo Teds, Fishbones, gas bubbles...): a hit knocks one out
      like any other enemy, and with MC hit reactions on they take Mario
      Combat's hits and fly. A Pokey loses the segment the punch lands on, as
      when a shell hits it. Bosses, items and platforms are not affected.
    - Fast enemies: ordinary enemies move and act twice as fast: they walk,
      fly, jump, throw and come out of pipes at double speed (on double
      time, so a throw still happens once).
    - Chasing enemies: walking enemies (Koopas, Goombas, Spinies, Buzzy
      Beetles, Bob-ombs, Dry Bones, Rex, Dino-Rhinos, Wigglers, Mega Moles,
      Mechakoopas...) keep turning to walk toward Mario instead of walking
      on.
    - Jumping enemies: those walking enemies jump (about two blocks high)
      every time Mario jumps off the ground (either Mario).
    - Respawning enemies: a defeated enemy comes back when Mario goes on
      and its spot scrolls back into view, as in the older Mario games
      (SMW itself only brings back the ones that walked off the screen).
      Items, keys and P-switches don't come back.
    Bosses, Koopa Kids and Mario Combat's Koopas and Bowser are never changed
    by these three.
  - WORLD CHEATS:
    - Infinite time, Infinite lives, All switch blocks.
    - Replay castles (A/B enters a destroyed castle or fortress again; the
      original game only allows L+R on a destroyed castle).
    - Slow motion: levels run at half speed (the music keeps its speed).
    - Mirror mode: levels are mirrored left <-> right and Left / Right swap.
      The status bar, message boxes and life bar stay readable.
    - Slippery levels: every level is as slippery as an ice level, for both
      Marios (slow starts, long slides; standing still stays still). Mario
      Combat Mario slides on the game's own ice levels too.
    - Pit rescue: falling into a pit puts Mario back on the last ground he
      stood on in that room, and costs a hit instead of a life (Mario Combat
      Mario loses life, SMW Mario powers down; small SMW Mario still dies).
    - Coin magnet: coins within 4 blocks of Mario fly out to him, one per
      frame (Yoshi coins count as collected Yoshi coins). Off while a blue
      P-switch runs, since coins are blocks then.
    - Open map: on the map Mario can walk past levels he hasn't beaten and
      along paths that aren't open yet (they are drawn), and play any level.
      Your save's progress is not changed: with the cheat off, the next map
      shows your own map again. (SMW still marks the way back from a level
      Mario arrives at, as it always does.)
    - Low gravity: both Marios fall with half of their gravity, so every
      jump goes about twice as high and far (also on Yoshi). Swimming,
      climbing and cape flight are unchanged.
  - Pits, lava and being crushed still cost a life.
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
This is a personal fan project. Please don't sell it, and don't share it with a
ROM included.
