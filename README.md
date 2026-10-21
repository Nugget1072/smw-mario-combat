# ⚠️ This is AI slop ⚠️

**Almost everything in this repository was written by an AI (Claude, via Claude Code) under a human's direction:**
the code, the tools, the reverse-engineering notes, the docs, and this README. Expect AI-flavoured code, AI-flavoured
comments and very long notes. It works on the machine it was made on; beyond that, no promises.

---

# Super Mario World × Mario Combat

Super Mario World on PC, with **Mario from the Flash game "Mario Combat"** playing it:

- his own sprites, animations, punches, kicks, combos, wall slides and sounds;
- his movement, ported line by line from the original game's ActionScript and checked tick for tick against the real
  Flash runtime;
- Mario Combat's Koopas and Bowser as optional enemy swaps, with their AI, attacks and health bars;
- **Mario Combat's own story level** (all five areas, Bowser at the end) playable from the level select, drawn with
  Mario Combat's art;
- a switch (Q) between Mario Combat Mario and the original SMW Mario at any time, a settings menu, a level select, a boss
  rush and a pile of cheats.

It is built on [snesrev/smw](https://github.com/snesrev/smw), a C reimplementation of Super Mario World.
The player guide (controls, menus, cheats) is [`tools/package/README.txt`](tools/package/README.txt).

## You need

- **Your own Super Mario World (USA) ROM.** It is not included and never will be.
  - Unmodified, 512 KB; SHA-1 `6B47BB75D16514B6A476AA0C73A683A2A4C18765` (a 512-byte copier header is fine).
- **Windows**, 64-bit (the only platform this was built and tested on).
- **A MinGW-w64 GCC toolchain** with `make` and `windres`: [w64devkit](https://github.com/skeeto/w64devkit) or
  [MSYS2](https://www.msys2.org) (MINGW64).
- **SDL2's MinGW development files**: `SDL2-devel-2.x.x-mingw.zip` from
  [SDL's releases](https://github.com/libsdl-org/SDL/releases) (SDL 2, not 3).
- **Python 3** with `bsdiff4` (`pip install bsdiff4`), for extracting the game's assets from your ROM.

Everything the game itself needs (including Mario Combat's exported art, sounds and music) is already in the
repository: the only file you add is the ROM.

## Build (Windows, PowerShell)

```powershell
git clone https://github.com/Nugget1072/smw-mario-combat.git
cd smw-mario-combat
# your ROM, renamed:
copy "path\to\Super Mario World.sfc" games\SuperMarioWorld\smw.sfc
# where SDL2's MinGW files are (the folder that has include\ and lib\):
$env:SDL2_DIR = "C:\path\to\SDL2-2.32.10\x86_64-w64-mingw32"
# gcc, make, windres and python on PATH (w64devkit's bin folder, for example), then:
powershell -ExecutionPolicy Bypass -File tools\build_smw.ps1
```

This builds `games\SuperMarioWorld\smw.exe` and, the first time, extracts `smw_assets.dat` from your ROM.
Copy `SDL2.dll` from `%SDL2_DIR%\bin` next to `smw.exe` if your toolchain links SDL dynamically (the build script links
it statically), then run `smw.exe` from `games\SuperMarioWorld`.

With MSYS2 you can also run `make -j16` in `games/SuperMarioWorld` (it uses `sdl2-config`, and extracts the assets
from `smw.sfc` too). The Visual Studio project and `run_with_tcc.bat` are upstream's and don't know the new files.

Optional: `tools\package.ps1` assembles a portable folder (`finished\`) that rebuilds its data from the player's ROM at
start-up instead of shipping `smw_assets.dat`. It expects the ROM in `games\SuperMarioWorld\Rom\`.

## What's where

| Path | What |
|---|---|
| `games/SuperMarioWorld/` | the game: snesrev/smw plus this port (`src/mc/` Mario Combat's Mario, enemies and level; `src/menu.c`, `src/levelselect.c`, `src/cheats.c`, `src/pilot.c` the menu, level select, cheats and the scripted test pilot) |
| `games/SuperMarioWorld/src/mc/*_data.c`, `mc_sounds.c`, `mc_music.c`, `mc_level_*.c` | generated from Mario Combat's SWF by the exporters in `tools/scripts/` (not hand-edited) |
| `tools/scripts/` | exporters, TAS / test tools, regression runners |
| `tools/pilots/` | scripted test runs (`smw.exe --pilot <script> --out <dir>`) |
| `tools/package/README.txt` | the player guide |
| `notes/` | reverse-engineering notes and verification records (with screenshots) |
| `HANDOFF.md`, `PROJECT_BRIEF.md`, `CLAUDE.md` | the AI's running state, brief and rules for the project |

Not included (the `.gitignore` keeps them out): ROMs and anything extracted from one, saves and settings, build
output, the original Mario Combat executable and SWF, the multi-gigabyte portable toolchain, and the local working
folder of reference captures and recordings (`work/`). The exporters and reference-capture tools need those local
inputs; building and playing don't.

## License and disclaimer

**Do whatever you want with it.** The work added in this repository is released into the **public domain** under
[The Unlicense](LICENSE): copy it, change it, sell it, no credit needed.

That can only cover what this project made (the Mario Combat port, its tools, scripts, notes and docs). Some things
here belong to others and keep their own terms:

- **snesrev/smw** (`games/SuperMarioWorld`): MIT License, see
  [`games/SuperMarioWorld/LICENSE.txt`](games/SuperMarioWorld/LICENSE.txt) (also Opus' license); `third_party/`
  holds stb_image and gl_core under their own terms.
- **Mario Combat**'s art, animations, sounds and music, exported into the source files listed above, belong to the
  creators of that Flash game.
- **Super Mario World**, Mario and all related names and characters are trademarks and copyrights of **Nintendo**.

This is an unofficial fan project. It is **not affiliated with, endorsed or sponsored by Nintendo** or by the makers of
Mario Combat. No ROM or ROM-derived data is distributed. It is provided **as is, without any warranty**; use it at your
own risk.
