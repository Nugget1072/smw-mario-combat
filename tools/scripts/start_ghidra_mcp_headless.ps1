# Starts the GhidraMCP 6.0.0 headless HTTP server (com.xebyte.headless.GhidraMCPHeadlessServer)
# on 127.0.0.1:<Port> using the project-local Ghidra 12.1.4. Mirrors the upstream docker/entrypoint.sh
# classpath (GhidraMCP jar + Ghidra Framework/Features/Processors lib jars).
# Usage:  powershell -File tools\scripts\start_ghidra_mcp_headless.ps1 [-Port 8089] [-Foreground]
# Writes the PID to work\ghidra\ghidra_mcp_server.pid and logs to work\ghidra\server_stdout.log / server_stderr.log
param([int]$Port = 8089, [switch]$Foreground)
$R = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$G = "$R\tools\ghidra\ghidra_12.1.4_PUBLIC"
$jar = "$G\Ghidra\Extensions\GhidraMCP\lib\GhidraMCP-6.0.0.jar"
$cp = @($jar)
foreach ($d in 'Framework', 'Features', 'Processors') {
  $cp += Get-ChildItem "$G\Ghidra\$d\*\lib\*.jar" | ForEach-Object FullName
}
$cpFile = "$R\work\ghidra\classpath.txt"
New-Item -ItemType Directory -Force "$R\work\ghidra", "$R\work\ghidra\user_settings", "$R\work\ghidra\cache", "$R\work\ghidra\tmp" | Out-Null
# Java @argfile keeps the very long classpath off the command line.
$argLines = @(
  '-Xmx4g', '-XX:+UseG1GC', '-XX:-UsePerfData',   # no hsperfdata file in the OS temp dir
  "-Dghidra.home=$G", '-Dapplication.name=GhidraMCP',
  # keep Ghidra user settings / cache / temp inside the project folder
  ("-Dapplication.settingsdir=$R\work\ghidra\user_settings" -replace '\\', '/'),
  ("-Dapplication.cachedir=$R\work\ghidra\cache" -replace '\\', '/'),
  ("-Djava.io.tmpdir=$R\work\ghidra\tmp" -replace '\\', '/'),
  '-Dfile.encoding=UTF8', '-Djava.awt.headless=true',
  '-classpath', ('"' + (($cp -join ';') -replace '\\', '/') + '"'),
  'com.xebyte.headless.GhidraMCPHeadlessServer',
  '--port', "$Port", '--bind', '127.0.0.1'
)
$argLines | Set-Content -Encoding ascii $cpFile
$env:GHIDRA_MCP_FILE_ROOT = "$R\work"
# Prefer a real JDK java.exe over the Oracle "javapath" shim so the PID we record is the JVM itself.
$java = if ($env:JAVA_HOME -and (Test-Path "$env:JAVA_HOME\bin\java.exe")) { "$env:JAVA_HOME\bin\java.exe" }
        elseif (Test-Path 'C:\Program Files\Java\jdk-26.0.2.1\bin\java.exe') { 'C:\Program Files\Java\jdk-26.0.2.1\bin\java.exe' }
        else { (Get-Command java).Source }
if ($Foreground) { & $java "@$cpFile"; exit $LASTEXITCODE }
$p = Start-Process -FilePath $java -ArgumentList "@$cpFile" -PassThru -WindowStyle Hidden `
  -RedirectStandardOutput "$R\work\ghidra\server_stdout.log" -RedirectStandardError "$R\work\ghidra\server_stderr.log"
$p.Id | Set-Content "$R\work\ghidra\ghidra_mcp_server.pid"
"GhidraMCP headless server started, PID $($p.Id), port $Port"
