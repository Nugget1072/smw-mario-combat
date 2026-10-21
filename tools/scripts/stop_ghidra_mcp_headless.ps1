# Stops the GhidraMCP headless server started by start_ghidra_mcp_headless.ps1 (PID file + any child JVM).
$R = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$pidFile = "$R\work\ghidra\ghidra_mcp_server.pid"
$ids = @()
if (Test-Path $pidFile) { $ids += [int](Get-Content $pidFile) }
# Also catch any JVM launched with this project's classpath arg-file (e.g. via the javapath shim).
$ids += Get-CimInstance Win32_Process -Filter "Name='java.exe'" |
  Where-Object { $_.CommandLine -like "*MarioCombatSMW\work\ghidra\classpath.txt*" } | ForEach-Object ProcessId
foreach ($id in ($ids | Sort-Object -Unique)) {
  try { Stop-Process -Id $id -Force -ErrorAction Stop; "stopped PID $id" } catch { "PID $id not running" }
}
Remove-Item $pidFile -ErrorAction SilentlyContinue
