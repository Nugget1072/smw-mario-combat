# ghidra-mcp + rea: how to run them again

Everything is project-local. Run commands from the project folder (the repository's root) in Windows PowerShell 5.1.
Start each session with `. .\tools\env.ps1`, which puts python, node/rea and the uv tools on PATH and sets
`GHIDRA_INSTALL_DIR`.

## What is installed where

| Piece | Location |
|---|---|
| Ghidra 12.1.4 | `tools\ghidra\ghidra_12.1.4_PUBLIC`. It runs on the system JDK 26 (`C:\Program Files\Java\jdk-26.0.2.1`); Ghidra and rea both accept it, so no JDK 21 was downloaded. |
| GhidraMCP 6.0.0 extension | Unzipped into `tools\ghidra\ghidra_12.1.4_PUBLIC\Ghidra\Extensions\GhidraMCP\` (the install-dir Extensions folder, not the user-profile one). The jar is `...\GhidraMCP\lib\GhidraMCP-6.0.0.jar`. Its `extension.properties` says `version=12.1.2`. The GUI may warn about that; the headless server below does not care. |
| GhidraMCP bridge 6.0.0 | `uv tool install tools\downloads\ghidra_mcp_bridge-6.0.0-py3-none-any.whl`. The venv is at `tools\uv-tools\tools\ghidra-mcp-bridge` and the exe at `tools\uv-tools\bin\bridge-mcp-ghidra.exe`. It uses the project Python 3.14.8. |
| rea 6.7.0 (`rea-agents`) | `npm install --global rea-agents` with prefix `tools\node\global`. The CLI is `tools\node\global\rea.cmd`/`rea.ps1`. `rea setup` was **not** run, so there is no MCP or skill registration in any Claude config. |
| Helper scripts | `tools\scripts\start_ghidra_mcp_headless.ps1`, `stop_ghidra_mcp_headless.ps1`, `gmcp.py` (HTTP client), `bridge_smoke_test.py`, `as2_to_js_mirror.py`, `rea_summarize_js.py` |
| Ghidra project (analysed) | `work\ghidra\project\MarioCombat.gpr` holds the program `/MarioCombat.exe`, already auto-analysed (56,509 functions). Re-analysis took about 380 s. |
| Ghidra user settings, cache and temp | `work\ghidra\user_settings`, `work\ghidra\cache` and `work\ghidra\tmp` (set by `-Dapplication.settingsdir/cachedir` and `-Djava.io.tmpdir`). |

## 1. Start the GhidraMCP headless server

**Port:** use **8090**, not the default 8089 (on the original machine another project's GhidraMCP server was
listening on 127.0.0.1:8089).

```powershell
. .\tools\env.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\scripts\start_ghidra_mcp_headless.ps1 -Port 8090
curl.exe -s http://127.0.0.1:8090/check_connection     # -> Connection OK - GhidraMCP Headless Server v6.0.0-headless
```

The script runs `java @work\ghidra\classpath.txt`, using the upstream `docker/entrypoint.sh` recipe:
- the classpath is the GhidraMCP jar plus every `Ghidra\{Framework,Features,Processors}\*\lib\*.jar`;
- the main class is `com.xebyte.headless.GhidraMCPHeadlessServer --port 8090 --bind 127.0.0.1`.

It also sets `GHIDRA_MCP_FILE_ROOT=work`. Logs go to `work\ghidra\server_stdout.log` and `server_stderr.log`, and the
PID goes to `work\ghidra\ghidra_mcp_server.pid`. Add `-Foreground` to run it in the console instead.

**Open the saved, already-analysed program.** POST bodies must be JSON; form-encoded bodies are ignored.
`gmcp.py post` sends JSON.

```powershell
$g = "tools\scripts\gmcp.py"; $env:GMCP_PORT = "8090"
python -I $g post open_project path=<project>/work/ghidra/project/MarioCombat.gpr
python -I $g post load_program_from_project path=/MarioCombat.exe
python -I $g get  get_metadata
```

**Import again from scratch** (only if the project is lost):

```powershell
python -I $g post create_project parentDir=<project>/work/ghidra/project name=MarioCombat
python -I $g post load_program file=<project>/work/ghidra/MarioCombat.exe
python -I $g post run_analysis        # ~6-7 min
python -I $g get  save_program program=MarioCombat.exe
```

`/import_file` returns "Import requires GUI mode" on the headless server. Use `/load_program` instead.

**Useful queries:**
- `python -I $g schema <word>` lists the endpoints and their parameters.
- Examples:

  ```
  get decompile_function address=0x0046b629
  get disassemble_function address=0x0046b629
  get search_byte_patterns "pattern=56 34 12 FA"
  get search_strings "search_term=WIN 32,"
  get get_function_xrefs name=timeSetEvent
  get get_xrefs_to address=0x004a86d0
  get search_instructions mnemonic=CALL "operand_pattern=+ 0xe4]"
  get read_memory address=0x01213c00 length=256
  ```

- Set `$env:GMCP_OUT=<file>` to also save the raw response to a file.

**Stop the server** (always stop it when you are done):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\scripts\stop_ghidra_mcp_headless.ps1
```

## 2. The MCP bridge (for an MCP client)

```powershell
$env:GHIDRA_MCP_URL = "http://127.0.0.1:8090"       # the bridge defaults to 8089 (the other project's server)
tools\uv-tools\bin\bridge-mcp-ghidra.exe --transport stdio
```

The bridge is a stdio MCP server. To check the round trip without registering it anywhere:

```powershell
$env:GHIDRA_MCP_URL = "http://127.0.0.1:8090"
tools\uv-tools\tools\ghidra-mcp-bridge\Scripts\python.exe -I tools\scripts\bridge_smoke_test.py
```

Expected result: the bridge exposes 244 MCP tools, and `get_metadata` returns MarioCombat.exe. A saved run is in
`work\ghidra\bridge_smoke_test.txt`.

To register the bridge with Claude Code, add a `.mcp.json` in the project with:
- command `tools\uv-tools\bin\bridge-mcp-ghidra.exe`;
- args `["--transport","stdio"]`;
- env `{"GHIDRA_MCP_URL":"http://127.0.0.1:8090"}`.

Writing it into the user-level `~/.claude.json` would be an outside-folder change and has to be logged in
`OUTSIDE_FOLDER_CHANGES.txt`.

## 3. rea

```powershell
. .\tools\env.ps1
$env:JAVA_HOME = 'C:\Program Files\Java\jdk-26.0.2.1'
$env:REA_ANALYSIS_PROVIDER = 'ghidra'
$env:REA_GHIDRA_STARTUP_TIMEOUT_MS = '1800000'
$env:TEMP = "$PWD\work\rea\tmp"; $env:TMP = $env:TEMP   # keep rea's private runtime/ephemeral Ghidra projects in the project
rea doctor --format json                                  # ghidra checks should all be "healthy" (hopper/ida/jadx missing is fine)
```

**Native analysis of the projector, using rea's own ephemeral headless Ghidra:**
- Each command re-imports and re-analyses the exe, which takes about 400 s.
- Use `--format json` and redirect stdout. PowerShell `>` writes UTF-8 with a BOM, so read the files with `utf-8-sig`.

```powershell
rea inspect   work\ghidra\MarioCombat.exe --provider ghidra --snapshot work\rea\MarioCombat.snapshot --format json > work\rea\inspect_exe.json
rea function  work\ghidra\MarioCombat.exe 0x0046b629   --provider ghidra --format json > work\rea\function_0046b629.json
rea xrefs     work\ghidra\MarioCombat.exe timeSetEvent --provider ghidra --format json > work\rea\xrefs_timeSetEvent.json
rea search    work\ghidra\MarioCombat.exe "WIN 32,"    --provider ghidra --format json > work\rea\search_WIN32.json
```

**Static analysis of the decompiled ActionScript.** rea's JavaScript analyzer only picks up `.js` files. On the raw
FFDec tree it reports "Processing 0 selected application files". So build a JS mirror first; the FFDec output is never
modified:

```powershell
python -I tools\scripts\as2_to_js_mirror.py work\ffdec\scripts\scripts work\rea\as_js_normalized --normalize
rea analyze-javascript-application "<project>/work/rea/as_js_normalized" --artifact-format directory --format json > work\rea\as_scripts_js_normalized.json
python -I tools\scripts\rea_summarize_js.py work\rea\as_scripts_js_normalized.json DefineSprite_440/frame_1/DoAction.js DefineSprite_432/frame_1/DoAction.js > work\rea\as_scripts_summary.txt
```

`--normalize` rewrites AS2-only syntax that JavaScript parsers reject:
- `or`, `and`, `not`, `<>`, `add`;
- `var x:Type` annotations;
- `onClipEvent(load){` and `on(press){` handlers.

Without it, rea fails to parse the hero script (440) and the combo script (432), on `or`. The two AS2 `class`
packages (`NewgroundsAPI`, `Armor_Bot_30_En_AS1`) still fail to parse, and that is expected.

## 4. Clean-up checklist

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\scripts\stop_ghidra_mcp_headless.ps1
Get-CimInstance Win32_Process | ? { $_.Name -match 'java|node|bridge-mcp' -and $_.CommandLine -like '*MarioCombatSMW*' } | select ProcessId,Name
```

A java process listening on 8089 belongs to some other project. Leave it alone.
