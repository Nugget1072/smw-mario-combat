# MarioCombat.exe projector: native RE with ghidra-mcp and rea

Confidence labels: **H** means verified directly in the bytes or the decompile. **M** means a strong inference from
decompiled code. **L** means a hunch.

## Tools, target and run data

**Tools**
- Ghidra 12.1.4, running headless on the system JDK 26.
- GhidraMCP 6.0.0 headless HTTP server on 127.0.0.1:**8090**. Port 8089 is taken by another project's GhidraMCP
  server.
- The GhidraMCP bridge 6.0.0, which was also checked over MCP stdio.
- rea 6.7.0, using its Ghidra provider.
- How to run them again: `tools/README_ghidra_mcp_rea.md`.

**Target**
- File: `work/ghidra/MarioCombat.exe`, a byte copy of `games/MarioCombat/MarioCombat.exe`.
- Size: 19,917,891 bytes.
- SHA-256 (from rea): `a96fbbb97524e1021201507cf6d9b2ab2ede3ed8686d810afd56c0c13c9e7502`.

**Ghidra load and analysis**
- Loader: PE, x86:LE:32:default, image base 0x00400000, 9 memory blocks.
- After auto-analysis: 56,509 functions and 202,008 symbols. The analysis took 378.7 s
  (`work/ghidra/run_analysis.json`).
- The program references PDB `FlashPlayer.pdb` (GUID 2bde7bc3-3b06-4369-b0d4-807baae3894d, age 2). The PDB is not
  available, so all names are `FUN_xxxxxxxx`.

**Raw outputs**
- ghidra-mcp: `work/ghidra/*.txt|*.c|*.json`.
- rea: `work/rea/`.

## (a) How the projector finds its embedded SWF — **H**

The SWF is appended to the stub. The stub reads the last 8 bytes of its own exe as a trailer:
- `u32 magic = 0xFA123456`;
- `u32 swfLen`.

If the magic matches, the stub seeks to `fileSize - swfLen - 8` and loads the movie from there.

**Function:** `FUN_0046b629`, body 0x0046b629–0x0046bdd6. It is called from `FUN_0046ca6a` at 0x0046cf6a, the
projector's startup and message-loop routine; `FUN_0046ca6a` also calls `GetMessageW`. After loading, `FUN_0046b629`
calls `ShowWindow` and `UpdateWindow`.

**How it was found:** `search_byte_patterns "56 34 12 FA"` gave exactly one hit, at **0x0046ba33**. That is the
immediate operand of:

```
0046ba12: CALL dword ptr [0x00e07330]          ; ReadFile(hFile, &trailer, 8, &nRead, 0)
0046ba18: TEST EAX,EAX / JZ 0x0046bc51
0046ba20: CMP dword ptr [EBP-0x8f8],0x8        ; nRead == 8
0046ba27: JNZ 0x0046bc51
0046ba2d: CMP dword ptr [EBP-0x914],0xfa123456 ; trailer.magic
0046ba37: JNZ 0x0046bc51
0046ba3d: MOV byte ptr [EBX+0xc],0x1           ; "has embedded movie" flag
...
0046ba55: MOV EAX,[EBP-0x8f0] ; fileSize
0046ba5b: SUB EAX,[EBP-0x910] ; - trailer.swfLen
0046ba65: SUB EAX,0x8         ; - 8   -> SetFilePointer(h, fileSize - swfLen - 8, 0, FILE_BEGIN)
```

Decompiled by ghidra-mcp `/decompile_function?address=0x0046b629`; saved as `work/ghidra/decomp_FUN_0046b629.c`.
The excerpt:

```c
GetModuleFileNameA(0,auStack_214,0x104);                       // own exe path (W path variant via fn ptrs)
iVar4 = CreateFileW(auStack_214,0x80000000,1,0,3,0x80,0);      // GENERIC_READ, OPEN_EXISTING
...
iStack_8f4 = GetFileSize(iVar4,0);
SetFilePointer(iVar4,iStack_8f4 + -8,0,0);                     // last 8 bytes
iVar5 = ReadFile(iVar4,&iStack_918,8,&iStack_8fc,0);
if (((iVar5 != 0) && (iStack_8fc == 8)) && (iStack_918 == -0x5edcbaa)) {   // 0xFA123456
  *(undefined1 *)(iVar2 + 0xc) = 1;
  ...
  SetFilePointer(iVar4,(iStack_8f4 - uStack_914) + -8,0,0);    // start of SWF = size - len - 8
  FUN_004bd090(&piStack_8e8, ...);                             // create stream/movie object
  ...
      SetFilePointer(iVar4,3,0,1);                             // skip "FWS"/"CWS"
      ReadFile(iVar4,abStack_8e2,1,&iStack_8ec,0);             // SWF version byte
      SetFilePointer(iVar4,0xfffffffc,0,1);                    // rewind 4
      if (abStack_8e2[0] < 6) { ... legacy (<SWF6) path ... } else { FUN_0047e483(iVar5,1,0); }
```

Additional detail (**M**): the stub reads the SWF version byte at offset 3 and takes a separate path for SWF < 6. This
movie is SWF 10, so it takes the `>= 6` path.

**Cross-check against the file (H).** Python over the exe bytes gives:
- the trailer is `0xFA123456`, length **3,934,211**;
- the stub is 19,917,891 − 8 − 3,934,211 = **15,983,672** bytes, which matches the brief;
- the payload starts `CWS`, version **10**, with an uncompressed length of 5,559,220 bytes;
- the payload is identical in size to `work/swf/MarioCombat.swf` (3,934,211 bytes).

## (b) Flash Player version in the stub — **H**

The stub is **Adobe Flash Player 32.0.0.465** (projector, `WIN 32,0,0,465`).

**Strings** (ghidra-mcp `/search_strings`; saved as `work/ghidra/search_strings_version.txt` and
`search_strings_shockwave.txt`):

| Address | String |
|---|---|
| 0x00e175bc | `"WIN 32,0,0,465"`, the `$version` / `getVersion()` string |
| 0x00e1823c | `"32.0.0.465"` |
| 0x00e1a0d0 | `"Adobe Flash Player 32"` |
| 0x00e199a0 | `"Shockwave Flash"` |
| 0x00e09028 | `"ShockwaveFlash.ShockwaveFlash"` |
| 0x00e0a1ec | `"<AdobeIP#A0000904_Flash Player 10.1>"`, an old IP marker that is not the build version |

Code references to these strings:
- `"WIN 32,0,0,465"` is referenced from `FUN_004ab0e0` (0x004ab16e), `FUN_004c29d0` and `FUN_004c07f0`;
- `"32.0.0.465"` is referenced from `FUN_004b3f80` and `FUN_00584214` (`work/ghidra/xrefs_to_*.txt`).

**VS_VERSIONINFO resource.** Found as the UTF-16 "FileVersion" byte pattern at 0x01213cea in `.rsrc`; dumped with
`/inspect_memory_content` to `work/ghidra/versioninfo_rsrc_dump.txt`:

| Key | Value |
|---|---|
| CompanyName | `Adobe` |
| FileDescription | `Adobe Flash Player 32.0 r0` |
| FileVersion | `32,0,0,465` |
| InternalName | `Adobe Flash Player 32.0` |
| LegalCopyright | `Adobe® Flash® Player. Copyright © 1996-2020 Adobe. …` |

**Emulator relevance.** Flash 32 is the final build, so it is the reference behaviour for AS2 timing and rendering.

## (c) Frame timing for the 50 fps SWF

**SWF header (H):** frame rate 0x3200 in 8.8 fixed point = **50.0 fps**, with **499** frames (decoded from the CWS
header of `work/swf/MarioCombat.swf`).

**Timer imports (H).** These are all imported (`work/ghidra/list_imports.txt`, 651 imports):
- `timeSetEvent`, `timeKillEvent`;
- `timeBeginPeriod`, `timeEndPeriod`, `timeGetDevCaps`, `timeGetTime`;
- `SetTimer`, `KillTimer`;
- `QueryPerformanceCounter`, `QueryPerformanceFrequency`;
- `Sleep`, `SleepEx`;
- `CreateWaitableTimerW/A`, `SetWaitableTimer`, `CreateTimerQueueTimer`;
- `GetTickCount`.

`MsgWaitForMultipleObjects` is not used. Xrefs are in `work/ghidra/timer_api_xrefs.txt`.

**Rate setter (H).** `FUN_0060924d(double fps)` (`decomp_FUN_0060924d.c`):
- clamps fps to [0.01, 1000];
- stores `player+0x210 = fps` and `player+0x218 = 1000.0 / fps`, the frame period in ms (**20.0 ms at 50 fps**);
- emits telemetry `".as.framerate"`;
- then either restarts the timer through `FUN_004a8860`, or, in CoreLoop mode, sets the next-frame deadline:
  `player+0x2b0 = player+0x2b8 = now + period`.

**Timer interval (H).** `FUN_004b4460`:

```c
iVar1 = (int)ROUND(*(double *)(param_1 + 0x218) / 10.0);
if (iVar1 < 1) iVar1 = 1;
return iVar1;
```

At 50 fps this gives **2 ms**. The player's tick timer runs at about one tenth of the frame period. The ticks poll the
clock, and the frame advances when the deadline passes. **M**: the polling interpretation follows from the 1/10
interval plus the deadline fields; the exact comparison site was not traced.

**Start timer (H).** `FUN_004a86d0(int intervalMs)` (`decomp_FUN_004a86d0.c`) emits telemetry `".starttimer"`. It
then takes one of two paths:
- **CoreLoop path:** if vtable+0xb0 returns true and the field at +0x4a8 is set, it spawns a thread named
  `"CoreLoop"` (`FUN_006314b1(FUN_004a86c0, …, "CoreLoop")`).
- **Window-timer path:** otherwise it calls vtable+0xe4, which is `FUN_004cebd8(intervalMs, 400)`.

**Window-timer path (H).** `FUN_004cebd8` (`decomp_FUN_004cebd8.c`):

```c
iVar2 = SetTimer(hwnd, 3, 400, 0);                                          // 400 ms watchdog timer id 3
iVar3 = timeSetEvent(interval, interval / 2, FUN_004ce28b, this, 1 /*TIME_PERIODIC*/);
if (iVar3 == 0) iVar2 = SetTimer(hwnd, 1, interval, 0);                     // fallback: WM_TIMER id 1
```

The callback `FUN_004ce28b` posts `WM_USER+1` (0x401) to the player window. The `+0xd48` flag ensures only one such
message is pending at a time. The message pump (`FUN_004cdbdd`) uses `PeekMessageW` with 0x401 and 0x113 (WM_TIMER),
plus `GetQueueStatus`.

Teardown is `FUN_004cec50`, which calls `KillTimer(3)` and then `timeKillEvent`, or `KillTimer(1)` on the fallback
path.

**Multimedia-timer resolution (H).** Two functions set it:
- `FUN_004cba3b` (vtable+0xa4) calls `timeGetDevCaps`. It then calls `timeBeginPeriod(min(caps.wPeriodMin, …))`
  only if that value is below 16 ms; the default argument is 16.0.
- `FUN_004c83d0` (vtable+0xa8) adjusts the period at run time:
  - minimum 10 ms when unfocused or throttled (`+0x508==0 && +0xacf!=0`), otherwise minimum 1 ms;
  - capped at 16;
  - divided by 10 if the frame period exceeds 100 ms;
  - when the period rises, it waits more than 2000 ms (`0x7d1`) before calling `timeEndPeriod`.
- `FUN_004cd09f` calls `timeEndPeriod` on shutdown.

**Clock (H).** `FUN_0049b5a0` returns milliseconds since start:
- it uses `QueryPerformanceCounter` and `QueryPerformanceFrequency` (×1000 / freq);
- it falls back to `timeGetTime() & 0x7fffffff`;
- it is used as `now` throughout, for example in `FUN_004c83d0`.

**CoreLoop thread (M).** `FUN_004a8450` loops on:

```
wait = 1 + (nextFrameDeadline - now)
```

with these rules:
- for the first 30 iterations (`+0x50c = 0x1e`), `wait` is forced to 16.667 ms;
- `wait` is capped at 1000 ms;
- each pass calls vtable+0xa4 and vtable+0xa8 (the timeBeginPeriod adjustments above);
- each pass waits on a waitable timer (`FUN_006315c2` → `CreateWaitableTimerW`).

**Projector mode not determined (L).** Static analysis did not settle which path a projector run uses: the
vtable+0xb0 predicate for CoreLoop was not resolved. Either way, frames are deadline-scheduled at `1000/fps` =
**20 ms**, on a ≤2 ms multimedia-timer tick or a waitable-timer sleep with timeBeginPeriod ≤ 16 ms.

**Implications for the port (M).**
- The real exe advances AS2 frames at 50 Hz with an accurate clock (QPC). It does not lock to vsync.
- For the SMW port at 60 fps, convert 50 fps to 60 fps with a time accumulator: 1 MC frame = 1.2 SMW frames. Do not
  run the logic at 60 Hz.
- No frame skipping or catch-up logic was characterised. If the host is slow, Flash 32 drops rendering but runs one
  `onEnterFrame` per frame tick. **L**: this is not verified here.

## rea results

### (i) Native analysis of the projector exe (Ghidra provider on Windows) — **succeeded**

- `rea doctor` on win32/x64 reports every Ghidra check as `healthy`: version 12.1.4, `analyzeHeadless.bat`, the
  native decompiler, Java 26 "accepted a 64-bit full JDK 21 or newer", and the native Job Object/DACL controls. Hopper,
  IDA and JADX are reported as missing; that is expected. Output: `work/rea/doctor.json`.
- `rea inspect work/ghidra/MarioCombat.exe --provider ghidra` ran with exit 0 in **409.7 s** (cold ephemeral import
  plus analysis). Output: `work/rea/inspect_exe.json`. rea reported:
  - format `pe`, architecture `x86`, image_base 0x400000;
  - **56,509 procedures**, the same count as ghidra-mcp, and **17,174 strings**;
  - 9 segments: `.text` 0x401000 (10,499,584 B), `.rodata`, `.rdata` 0xe07000, `.data` 0x104a000, `.gfids`,
    `.rsrc` 0x120d000 (1,838,080 B), `.reloc` and `tdb`;
  - the `FlashPlayer.pdb` RSDS reference.
- Each follow-up run below took 365–412 s, because each one re-imports and re-analyses the exe. Together they
  confirm the ghidra-mcp findings independently:
  - **`rea function … 0x0046b629`** (`work/rea/function_0046b629.json`; pseudocode extracted to
    `work/rea/function_0046b629_pseudocode.c`) returns:
    - body 0x46b629–0x46bdd6 (1,966 bytes);
    - caller `FUN_0046ca6a`;
    - callees including `KERNEL32.DLL::GetModuleFileNameA/CreateFileW/GetFileSize/SetFilePointer/ReadFile` and
      `USER32.DLL::ShowWindow/UpdateWindow`;
    - the same trailer logic, at pseudocode lines 178–187: `SetFilePointer(hFile, size-8)`,
      `ReadFile(hFile,&trailer,8,…)`, `trailer == -0x5edcbaa (0xFA123456)`, then
      `SetFilePointer(hFile, size - len - 8)`;
    - after that, chunked `ReadFile(hFile, buf, nNumberOfBytesToRead, …)` (line 234) streams the SWF in.
  - **`rea search … "WIN 32,"`** (`work/rea/search_WIN32.json`) returns `0xe175bc "WIN 32,0,0,465"`.
  - **`rea xrefs … timeSetEvent`** fails with exit 1. The JSON error says: `Unknown analyzed symbol name "timeSetEvent".
    Use an exact analyzed name or a hexadecimal address` (`work/rea/xrefs_timeSetEvent.json`).
  - **`rea xrefs … 0x01118274`** (the timeSetEvent IAT slot) returns `0x4cec25`, `0x81b30b` and `0xbecee5`
    (`work/rea/xrefs_timeSetEvent_IAT_01118274.json`). `0x4cec25` is the call inside `FUN_004cebd8`, which matches
    ghidra-mcp. ghidra-mcp's import-xref list did not show `0x81b30b`.
- Caveats rea itself reports:
  - each CLI call re-imports into an ephemeral project, so each takes about 400 s;
  - only Ghidra-defined strings are inventoried;
  - the PDB is not loaded.

### (ii) Static analysis of the decompiled ActionScript (`work/ffdec/scripts`)

**Raw `.as` tree: not usable.** `rea analyze-javascript-application <dir> --artifact-format directory` exits 0 but
reports "Processing 0 selected application files" (`work/rea/as_scripts_direct.json`). rea only inventories `.js` files.

**Plain `.as` → `.js` copy (`work/rea/as_as_js`):**
- 215 files parsed;
- 4 parse failures (`work/rea/as_scripts_js_mirror.json`), including both target scripts:
  - **`DefineSprite_440/frame_1/DoAction` (hero):** fails at line 337 `... && (Key.isDown(Right) or Key.isDown(Left))`
    on the AS1/Flash-4 `or` operator;
  - **`DefineSprite_432/frame_1/DoAction` (combos):** fails at line 116 on `or`, the same way.

**Normalized mirror (`tools/scripts/as2_to_js_mirror.py --normalize`):**
- rewrites `or`/`and`/`not`/`<>`/`add`, type annotations, and the `onClipEvent()`/`on()` headers; nothing else changes;
- output in `work/rea/as_scripts_js_normalized.json`, summary in `work/rea/as_scripts_summary.txt`;
- statistics:
  - 215 parsed, with only the 2 AS2 `class` packages failing (`NewgroundsAPI`, `ab3.rankz.Armor_Bot_30_En_AS1`);
  - 12,061 AST nodes visited;
  - semantic graph of 5,714 nodes, 1,515 relations, 131 function fingerprints and 1,696 unknowns;
  - **0 findings** (rea's Electron/IPC/network detectors have nothing to match in game code).

**What rea reports for the hero script (`DefineSprite_440/frame_1/DoAction`).** One module and one function (the
`onEnterFrame` closure, fingerprint `rea.javascript-semantic-function` `7aa7a079e8c2b66b`), with:
- 59 dynamic call sites;
- 90 `object-flow/ambiguous-target` unknowns, because the receivers `this`, `_parent`, `_root` and `Key` cannot be
  resolved statically.

Its recovered property-access map is the useful part:

| Access | Lines |
|---|---|
| writes `grav` | 3 |
| writes `xSpeed` | 165 |
| writes `_x` | 147, 152, 418 |
| writes `_y` | 419 |
| writes `onEnterFrame` | 1 |
| reads `ground.hitTest` | 5, 13, 21, 29, 34, 46, 58, 402 (ground collision probes) |
| reads `isDown` | 16 sites (67 … 386) |
| reads `_currentframe` | 155, 179, 197, 273, 316, 330, 353, 362, 408 |
| reads `gotoAndStop` | 14 sites |
| reads `gotoAndPlay` | 79, 83, 105, 109, 302, 310 |
| reads `attack._currentframe` / `attack.gotoAndPlay` | 179, 346, 364, 369 / 340, 348, 359 |
| reads `lScroll._x` / `rScroll._x` | 145 / 150 (screen-edge scroll clamps) |
| reads `deadZone._y`, `spawnPoint._x/_y` | 416 / 418–419 (fall-out and respawn) |
| reads `hurt_mc.gotoAndStop` | 428, 432 |
| reads `_xscale` | 371, 378 (facing) |

**What rea reports for the combo script (`DefineSprite_432/frame_1/DoAction`).** One function (`onEnterFrame`,
fingerprint `353b78346817ebc8`), with 59 dynamic calls and 203 ambiguous-target unknowns. Recovered accesses:
- reads `_parent.pressingAttack` (line 3);
- reads `Key.isDown` (35 sites), plus `_parent.Punch`, `_parent.Left` and `_parent.Right`;
- reads `this._currentframe` (20 sites) and `this.gotoAndPlay` (20 sites, lines 9 … 211);
- reads and writes `_parent._xscale` (32 writes): it flips facing to the held direction when a combo link fires.

**Interpretation (H, from reading the same source rea indexed).** This is a frame-window combo table. It runs only
while `_parent.pressingAttack == false`, i.e. on a fresh press. Each rule is "if Punch is held and the attack clip's
current frame is inside a window, `gotoAndPlay` the next label":

| Input | Current-frame window | Next label |
|---|---|---|
| Punch alone (no direction) | 10–22 | `attack4` |
| | 80–87 | `attack3` |
| | 86–93 and 36–43 | `attack5` |
| | 107–131 | `attack6` |
| | 186–190 | `attack3` |
| | 188–202 | `attack5` |
| | 241–246 and 210–218 | `attack4` |
| Punch + Left/Right | 186–190 | `attack8` |
| | 188–202 | `attack9` |
| | 78–86 | `attack10` |
| | 210–218 | `attack105` |
| Punch (any direction) | 235–243 | `attack6` |
| | 250–258 | `attack102` |
| | 265–273 | `attack103` |
| | 280–287 | `attack104` |
| | 295–303 | `attack105` |
| | 8–11 | `attack2` |
| | 31–37 | `attack3` |

- All windows are exclusive bounds, as `>` and `<` in the source; the table lists them as inclusive ranges.
- The `attack102`–`attack105` rows form a mash chain.
- These frame windows are in the attack clip's own timeline, at 50 fps. They belong in `notes/DATA_NOTES.md` and
  `mario_combat_moves.json` together with the label→frame map from FFDec.

**Verdict on rea for AS2.** rea gives a reliable, line-referenced property and call inventory and per-function
fingerprints. It does not resolve AS2 timeline semantics: `_parent`, `_root` and frame labels stay "ambiguous-target".
The physics constants still have to be read from the AS source itself.
