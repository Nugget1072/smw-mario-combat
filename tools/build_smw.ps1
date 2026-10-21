# Build games\SuperMarioWorld\smw.exe (gcc from w64devkit, static SDL2) and the mc_test harness.
param([switch]$Clean)
$ErrorActionPreference = 'Continue'  # make prints a harmless 'sdl2-config: not found' on stderr
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
. "$root\tools\env.ps1"
Push-Location "$root\games\SuperMarioWorld"
try {
  if (-not $env:SDL2_DIR) { throw "SDL2_DIR is not set: point it at SDL2's mingw development folder (see README.md)" }
  $sdl = $env:SDL2_DIR -replace '\\', '/'
  $cf = "-O2 -fno-strict-aliasing -I$sdl/include/SDL2 -Dmain=SDL_main -DSYSTEM_VOLUME_MIXER_AVAILABLE=0 -I."
  $lf = "-L$sdl/lib -Wl,-Bstatic -lmingw32 -lSDL2main -lSDL2 -Wl,-Bdynamic -mwindows -lm -lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32 -ldinput8"
  # The Makefile has no header dependencies: when any header is newer than smw.exe, rebuild every object, or files
  # compiled against an old struct layout (e.g. g_config) would be linked with new ones.
  $exe = Get-Item smw.exe -ErrorAction SilentlyContinue
  $newest_h = Get-ChildItem src -Recurse -Filter *.h | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if (-not $exe -or ($newest_h -and $newest_h.LastWriteTime -gt $exe.LastWriteTime)) { $Clean = $true }
  if ($Clean) { make clean_obj 2>$null | Out-Null }
  $out = make -j16 "CFLAGS=$cf" "SDLFLAGS=$lf" smw 2>&1
  $errs = $out | Select-String -Pattern ': error|undefined reference|Error \d'
  if ($errs -or $LASTEXITCODE -ne 0) { $out | Select-String -Pattern 'error|warning: implicit|undefined' | Select-Object -First 40; throw "smw build failed" }
  New-Item -ItemType Directory -Force "$root\work\mctest" | Out-Null
  gcc -O2 -o "$root\work\mctest\mc_test.exe" src/mc/mc_test.c src/mc/mc_core.c src/mc/mc_data.c -I src 2>$null
  # The game's assets come from the player's own ROM (never in the repository): smw.sfc here, or Rom\*.sfc
  if (-not (Test-Path smw_assets.dat)) {
    $rom = @(Get-Item smw.sfc -ErrorAction SilentlyContinue) + @(Get-ChildItem Rom\*.sfc -ErrorAction SilentlyContinue) |
      Select-Object -First 1
    if ($rom) { python assets\restool.py --rom "$($rom.FullName)"; "assets extracted from $($rom.Name)" }
    else { "no smw_assets.dat yet: put your Super Mario World ROM here as smw.sfc and build again" }
  }
  "BUILD OK: $((Get-Item smw.exe).Length) bytes"
} finally { Pop-Location }
