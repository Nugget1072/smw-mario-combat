# Run one pilot script hidden and build a contact sheet of its screenshots.
# Usage: powershell -File tools\scripts\run_pilot.ps1 <pilot name without .txt> [columns]
param([string]$Name, [int]$Cols = 4)
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = Join-Path $root 'games\SuperMarioWorld'
$out = Join-Path $root "work\pilot\$Name"
$script = Join-Path $root "tools\pilots\$Name.txt"
Remove-Item $out -Recurse -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $game
Start-Process -FilePath .\smw.exe -ArgumentList '--pilot', $script, '--out', $out -Wait -NoNewWindow
Pop-Location
$py = Join-Path $root 'tools\Python3.14.8\python.exe'
$shots = Get-ChildItem "$out\*.bmp" | Sort-Object Name | ForEach-Object FullName
if ($shots) {
  & $py -I -c @'
import sys, os
from PIL import Image, ImageDraw
sheet, cols, fs = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
rows = (len(fs) + cols - 1) // cols
s = Image.new('RGB', (256 * cols, 224 * rows), (30, 30, 40))
d = ImageDraw.Draw(s)
for i, f in enumerate(fs):
    x, y = (i % cols) * 256, (i // cols) * 224
    s.paste(Image.open(f).convert('RGB'), (x, y))
    d.text((x + 3, y + 212), os.path.splitext(os.path.basename(f))[0], fill=(255, 255, 0))
s.save(sheet)
'@ (Join-Path $root "work\pilot\${Name}_sheet.png") $Cols @shots
}
$csv = Get-ChildItem "$out\*.csv" | Select-Object -First 1
if ($csv) {
  & $py -I -c @'
import csv, sys
p = list(csv.DictReader(open(sys.argv[1])))
st = sorted(set(r['state'] for r in p)); md = sorted(set(r['mode'] for r in p))
print('frames', len(p), 'states', st, 'modes', md, 'min life', min(float(r['mclife']) for r in p),
      'puppet frames', sum(r['mcpuppet'] == '1' for r in p))
'@ $csv.FullName
}
