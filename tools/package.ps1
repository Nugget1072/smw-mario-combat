# Assemble the portable build in finished\ (no ROM, no ROM-derived .dat: assets are rebuilt from the player's ROM
# at start-up via smw_assets.bps). Then smoke-test a copy in work\finished_test with the local ROM.
$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
. "$root\tools\env.ps1"
$smw = "$root\games\SuperMarioWorld"
$fin = "$root\finished"
$work = "$root\work\package"
New-Item -ItemType Directory -Force $work | Out-Null

# 1. ROM-free asset patch: smw.sfc -> smw_assets.dat (built without the embedded ROM)
Copy-Item "$smw\Rom\Super Mario World.sfc" "$work\smw.sfc" -Force
Push-Location $smw
Copy-Item smw_assets.dat "$work\smw_assets_dev.dat" -Force
python assets\restool.py --rom "$work\smw.sfc" --no-include-rom | Out-Null
Move-Item -Force smw_assets.dat "$work\smw_assets_norom.dat"
Copy-Item "$work\smw_assets_dev.dat" smw_assets.dat -Force
Pop-Location
python -I "$root\tools\scripts\make_bps.py" "$work\smw.sfc" "$work\smw_assets_norom.dat" "$work\smw_assets.bps"

# 2. finished\: only the delivered files are replaced. The player's own files there are left alone: the ROM they
# supply, their settings (smw.user.ini), their saves (saves\) and the asset file the game builds from their ROM.
$delivered = 'smw.exe', 'smw.ini', 'smw_assets.bps', 'README.txt', 'LICENSE.txt'
New-Item -ItemType Directory -Force $fin | Out-Null
foreach ($f in $delivered) { Remove-Item "$fin\$f" -Force -ErrorAction SilentlyContinue }
Copy-Item "$smw\smw.exe" "$fin\smw.exe"
Copy-Item "$smw\smw.ini" "$fin\smw.ini"
Copy-Item "$work\smw_assets.bps" "$fin\smw_assets.bps"
Copy-Item "$root\tools\package\README.txt" "$fin\README.txt"
Copy-Item "$smw\LICENSE.txt" "$fin\LICENSE.txt"
foreach ($f in $delivered) { if ((Get-Item "$fin\$f").Extension -in '.sfc', '.smc', '.dat') { throw "ROM data delivered: $f" } }
# (the round-19 videos, MarioCombat_*.mp4, are recorded straight into finished\ by tas.py / make_video.py and kept)
$videos = Get-ChildItem $fin -Filter 'MarioCombat_*.mp4'
if ($videos) { "videos kept: $(($videos | ForEach-Object Name) -join ', ')" }
$players = Get-ChildItem $fin -Force | Where-Object { $_.Name -notin $delivered -and $_.Name -notlike 'MarioCombat_*.mp4' }
if ($players) { "left in place (the player's own files, not delivered): $(($players | ForEach-Object Name) -join ', ')" }
Get-ChildItem $fin | Select-Object Name, Length | Format-Table -AutoSize | Out-String

# 3. Smoke test from a copy (never the delivered folder), with the user's ROM under its original name
$t = "$root\work\finished_test"
Remove-Item $t -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $t | Out-Null
foreach ($f in $delivered) { Copy-Item "$fin\$f" "$t\$f" }  # (as delivered: without the player's settings and saves)
Copy-Item "$smw\Rom\Super Mario World.sfc" "$t\Super Mario World.sfc"
New-Item -ItemType Directory -Force "$t\shots" | Out-Null
$p = Start-Process -FilePath "$t\smw.exe" -WorkingDirectory $t -ArgumentList '--pilot', "$root\tools\pilots\smoke.txt", '--out', "$t\shots" -PassThru -Wait
"smoke exit code $($p.ExitCode); shots: $((Get-ChildItem "$t\shots\*.bmp").Count)"
