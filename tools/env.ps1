# Dot-source this (. .\tools\env.ps1) to put the project's portable tools on PATH for the current shell.
# The portable tools (tools\w64devkit, tools\sdl2, tools\Python3.14.8, ...) are not in the repository: whichever are
# present locally are used, anything else comes from the system PATH. SDL2_DIR (SDL2's mingw development folder,
# e.g. ...\SDL2-2.32.10\x86_64-w64-mingw32) is kept when already set.
$T = Split-Path -Parent $MyInvocation.MyCommand.Path
$local = @(
  "$T\Python3.14.8", "$T\Python3.14.8\Scripts",
  "$T\git\cmd",
  "$T\gh\bin",
  "$T\w64devkit\bin",
  "$T\node\node-v24.22.0-win-x64",
  "$T\uv",
  "$T\uv-tools\bin",
  "$T\node\global"
) | Where-Object { Test-Path $_ }
if ($local) { $env:PATH = ($local -join ';') + ';' + $env:PATH }
if (Test-Path "$T\uv-tools") {
  $env:UV_TOOL_DIR = "$T\uv-tools\tools"
  $env:UV_TOOL_BIN_DIR = "$T\uv-tools\bin"
  $env:UV_CACHE_DIR = "$T\uv-tools\cache"
  $env:UV_PYTHON_INSTALL_DIR = "$T\uv-tools\python"
}
if (Test-Path "$T\node") {
  $env:npm_config_cache = "$T\node\npm-cache"
  $env:npm_config_prefix = "$T\node\global"
}
if (Test-Path "$T\gh") { $env:GH_CONFIG_DIR = "$T\gh\config" }  # (GitHub CLI settings in the project; the token in Windows' credential store)
if (Test-Path "$T\ghidra\ghidra_12.1.4_PUBLIC") { $env:GHIDRA_INSTALL_DIR = "$T\ghidra\ghidra_12.1.4_PUBLIC" }
if (-not $env:SDL2_DIR) {
  $sdl = Get-ChildItem "$T\sdl2\SDL2-*\x86_64-w64-mingw32" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($sdl) { $env:SDL2_DIR = $sdl.FullName }
}
