# Mario Combat → SMW port: project rules

Read `PROJECT_BRIEF.md` (goal and rules), `HANDOFF.md` (state and next step) and `notes/DATA_NOTES.md` (RE knowledge base).

## Hard rules
- **Stay in the project folder.** Everything lives in this folder. Tools go in `tools\`. Anything done outside it is logged in `OUTSIDE_FOLDER_CHANGES.txt`.
- **No ROM distribution.** Never copy the SMW ROM or ROM-derived data into `finished\`. The player supplies `smw.sfc`. `smw_assets.dat` is generated or patched from it.
- **Verbatim Mario Combat.** Use MC's own art, timelines and numbers, exported from the SWF by `tools/scripts/mc_export.py`. Don't hand-edit `src/mc/mc_data.c`.
- **Ported AS2 stays line-by-line.** Keep the statement order of the original scripts in `src/mc/mc_core.c`. It is verified tick-for-tick against the real game.

## Tools
- Put every tool on PATH first: `. .\tools\env.ps1`. This gives python, git, gcc (w64devkit), node, uv, ffdec and the Ghidra env.
- SMW source: `games\SuperMarioWorld` (snesrev/smw clone).
- Build (from `games\SuperMarioWorld`):
  ```
  make -j16 "CFLAGS=..." "SDLFLAGS=..." smw
  ```
  `tools\build_smw.ps1` wraps this.
- Mario Combat reference captures (real Flash Player 32 runtime):
  ```
  python -I tools\refcap\build_instrumented.py games\MarioCombat\MarioCombat.exe tools\ffdec\ffdec.jar work\refcap
  python -I tools\refcap\refcap_server.py work\refcap\MarioCombat_refcap.exe tools\refcap\plans\<plan>.json work\refcap\runs\<plan>
  ```
  A capture briefly opens a topmost game window.
- Core verification:
  ```
  work\mctest\mc_test.exe work\mctest\ground486.bin work\refcap\runs\<plan>\trace.csv -1540.7 2525.7 411.1 -1129.35 286.5 --oracle
  ```
  Every run must print `RESULT PASS`.

## Conventions
- Files in `notes/` use H/M/L confidence labels.
- Converters are re-runnable and write sidecar JSON.
- Look at every contact sheet and screenshot before calling anything done.
