# Kuttenberg Thief

Thief (2014)-inspired movement and stealth inside Kingdom Come: Deliverance II.

**Current source: M0 read-only capability probe, v0.0.2.** See STATUS for installed/tested version. It does not implement swoop, change stealth, move the player, alter camera control or replace animations.

Target installation: Steam retail **1.5.6 / build 23914554**. Modding tools **1.5.5 / build 22816715** are a separate, currently incomplete development workspace. See [assessment](docs/CapabilityAssessment.md), [environment](docs/Environment.md) and [status](STATUS.md).

## Local commands

Run PowerShell in `C:\Dev\KCD2 Mod\KCD2-Thief-mod`:

```powershell
python tools/project.py build
python tools/project.py install
python tools/project.py collect
python tools/project.py uninstall
python -m unittest discover -s tests -p test_deployment.py
```

Build requires Python 3.12 and a Lua 5.1 interpreter, configured in ignored `.local/environment.json`. The local interpreter was built from official Lua 5.1.5 source using installed MSVC. No pip packages, native loader or engine recompile are needed. [Environment](docs/Environment.md) records paths and hashes.

Build runs diagnostic regression checks and Lua syntax checks, then creates `build/kuttenberg_thief/Data/kuttenberg_thief.pak` as an uncompressed ZIP. It packages only our source. The official entry is `Scripts/Mods/kuttenberg_thief.lua`.

Install checks the inspected retail binary hashes, requires the game closed, creates a complete SHA-256-verified save backup under `.local/backups/`, then adds `Mods/kuttenberg_thief`. Existing files with different hashes cause an explicit stop. An existing `mod_order.txt` that excludes this mod also causes a stop, preserving its order. Run an elevated shell if Windows denies writing to Program Files.

Uninstall verifies the install receipt and every owned file, archives the mod under `.local/backups/`, then removes only these unchanged files. Modified/unknown files cause a stop. For updates, close the game, uninstall the owned version, rebuild and install.

## First in-game check

1. After installation, launch normal KCD2 from Steam. No launch-option change is required for the automatic diagnostic.
2. Load a Kuttenberg save. The mod automatically records up to 120 samples, one per second. Walk, stop, crouch, crouch-walk, turn, pause/resume, then load the same save again.
3. Exit the game and run `python tools/project.py collect`. The relevant excerpts go into `.local/logs/`.
4. Tell the agent the test is complete so it can inspect `kcd.log` locally.

Optional console commands: `kt_probe` (API and snapshot), `kt_watch` (restart 120-sample capture), `kt_mark` (snapshot marker), `kt_stop` (stop capture), `kt_rays` (3 m camera ray), `kt_detail` (v0.0.2: physical stats, AI stance and movement-variable reads). `kt_watch` and `kt_rays` worked in the first retail playtest. No keys are rebound. Ray output is diagnostic and is not a whole-body collision check. Missing getters are logged as `unknown:missing`, never interpreted as a safe state.

Logging configuration is at the top of `src/Data/Scripts/KuttenbergThief/Probe.lua`: `enabled`, `intervalMs`, `maxSamples`. Rebuild/reinstall to change it. The probe does not save custom state.

## Project map

- [Architecture](docs/Architecture.md): ownership, interfaces, fallback route.
- [Movement specification](docs/MovementSpec.md): behavior, measurement and configurable targets.
- [Test plan](docs/TestPlan.md): gate checks and full swoop acceptance scenarios.
- [Sources](docs/Sources.md): pinned reference revisions and evidence limitations.
- [Change log](MODLOG.md): meaningful changes and verification.
