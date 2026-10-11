# Inspected environment — 2026-10-11

User workspace: `C:\Dev\KCD2 Mod\KCD2-Thief-mod`, the pre-existing empty repository inside the requested project folder. Windows **10 Pro 22H2, build 19045.6466**, x64. No existing source files were present; branch `main` was unborn. No push/upload performed.

| Component | Exact observed version | Store/build | Executable |
|---|---|---|---|
| KCD2 retail | File/Product 1.5.6.0; `wh_sys_version=1.5.6` | Steam app 1771300, build 23914554 | `C:\Program Files (x86)\Steam\steamapps\common\KingdomComeDeliverance2\Bin\Win64MasterMasterSteamPGO\KingdomCome.exe` |
| KCD2 development | File/Product 1.5.5.0; `wh_sys_version=1.5.5` | Steam tools app 2429020, build 22816715 | `C:\Program Files (x86)\Steam\steamapps\common\KCD2Mod\Bin\Win64ReleaseSteamLTO_DLL\KingdomCome.exe` |
| KCD2 editor | File/Product 1.5.5.0 | Same tools installation | Same development bin folder, `Editor.exe` |
| Thief 2014 | File/Product 1.7.0.5 | Epic manifest `TH4_EPIC_1.7.0.5` | `S:\Spiele\ThiefvWwV2\Binaries\Win64\Shipping-ThiefGame.exe` |

Thief also ships `Binaries\Win32\Shipping-ThiefGame.exe`, same file version. The Epic manifest selects Win64. Inspected retail/development/Thief Win64 PEs have machine 0x8664. KCD2 main implementation is in WHGame.dll, not its small launcher executable. Development WHGame.dll is much smaller and uses a different module layout; scanning that DLL alone is not scanning its entire engine.

Retail binary pins:

```
KingdomCome.exe SHA-256 d85b5355842d16290dc0d97e1d02856c6f2811a2021c8957aa24d7782a771b77
WHGame.dll     SHA-256 bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7
```

The installer checks these two hashes. Further inventories are in ignored `.local/evidence/binary-inventory.json`. Do not promote a changed binary to supported merely by updating a hash.

## Development tools

- Python **3.12.2** at `C:\Users\leonh\AppData\Local\Programs\Python\Python312\python.exe`.
- Git **2.56.0.windows.1**, CMake **3.31.5**, Node **22.18.0** found on PATH. GitHub CLI found; no authentication settings inspected.
- Visual Studio Community at `C:\Program Files\Microsoft Visual Studio\18\Community`, MSVC toolset **14.51.36231**, discovered via vswhere. Build environment is `VC\Auxiliary\Build\vcvars64.bat`.
- Java directories `C:\Program Files\Java\jdk-17.0.2` and `jdk-21` found. Their runtime versions were not executed/verified. No Ghidra found on PATH or in inspected likely tool locations; not a whole-disk absence claim.
- Lua **5.1.5** built locally from `https://www.lua.org/ftp/lua-5.1.5.tar.gz` using MSVC. Local validation interpreter `.local/tools/lua.exe`; source/license under `.local/references/lua-5.1.5` when copied during setup. Build is for script validation, not an engine replacement.
- Official `Bin\Win64SharedPrivate\LuaCompiler.exe`, resource compiler, WorkspaceSetup, editor, remote console, Skald and Smid present in KCD2Mod. They were not used to modify the installation.
- Current REA source requires newer Node than installed 22.18.0, plus Ghidra/JDK for that route. No Node/tool upgrades performed.

## Data and workspace state

KCD2 PAKs open as ZIP containers. Retail `Scripts.pak` has 26,442 entries, including Lua, AI/quest XML; `Tables.pak` contains XML and `.tbl` database data. Region files include `Data/Levels/kutnohorsko/{level,terrain,recast,...}.pak`. Model/animation resources use KCD2/CryEngine formats and Mannequin ADB/controller definitions; no animation conversion performed.

The installed tools provide `Tools/modding/docs/script_bind/script_bind.zip` (5,017 entries, inner folder `script_bind_2025_01_14`), editable Kuttenberg layers and skeleton assets. `KCD2Mod/Data/Scripts.pak` is absent: official workspace setup has not yet linked/copied required retail PAK data. Do not launch this development install as if it were configured; setup is a separate task with the 1.5.5/1.5.6 mismatch acknowledged.

Thief `ThiefGame/CookedPCNG` contains `.u`, `.upk`, `.tfc`, `.bnk`, `.pck`, `.bin`, `.dat`; Config contains UE3-style INIs and Engine.PlatformManager/PlatformSavePC classes. No UE4/5 `.uasset` pipeline is assumed. `Dash` and crouch UI names are present, but no exact original swoop duration/distance has been established.

## Saves and privacy

**KCD2 verified:** `C:\Users\leonh\Saved Games\kingdomcome2\saves`. Backup before first playtest contained **237 files, 784,657,364 bytes**, including 236 files directly in `playline0`. Full archive and per-file SHA-256 manifest: `.local/backups/20261011T164850575290Z`. Configuration/profiles also exist under `kingdomcome2`, but this backup covers the complete saves subtree only.

**Thief unresolved:** no actual `.sav` found in its installation or inspected likely profile folders. Redirected Documents is `C:\Users\leonh\OneDrive\Dokumente`; checked its My Games folder, ordinary Documents, Saved Games, Local/Roaming app data and Thief-specific Epic save-cache candidates. The executable contains `\\My Games\\`, `\\Thief\\`, and `%sSaveData\\Thief%i\\%s.sav` strings; INI has a relative `ThiefGame/SaveData/Profile/Profile.sav`. These are clues, not verified active save locations. Before Thief recording/playtesting, launch it once, create a disposable save, then identify the actual newly written directory and back it up. No Thief save has been backed up or overwritten.

Keep save contents, logs, storefront account identifiers and configs in ignored `.local`. Only relevant `[KT]` and mod/error lines are collected. The old pre-test kcd.log was from February 2026 and was not used as current-build runtime evidence.
