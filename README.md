# Kuttenberg Thief

Thief (2014)-inspired movement and stealth inside Kingdom Come: Deliverance II.

**Kuttenberg wall climbs.** The 1.64 m wall passed repeat climbs with normal Jump in retail. The next build pairs it with the previously tested 1.02 m raised-pavement wall and sweeps the landing lane from Henry’s actual approach. With the pinned local build and `guide-walls` profile installed, start KCD2 with `python tools/tall_wall.py launch`. Wait for the ready message before loading the Kuttenberg save. [Status](STATUS.md)

**Retail loaded an owned guide fixture.** KCD2 selected the mod's Kuttenberg archive, then a separate load retained one inactive guide and its two markers after the save loaded. Both tests were rolled back and the original installation hashes were verified. Higher-wall climbing still needs an eligible guide with proven lifetime, body clearance along the trajectory and suitable native motion. [Status](STATUS.md) and [native guide route](docs/KCD2HigherClimbRoute.md).



**v0.0.21: ledge geometry verified in retail; added climbs remain disabled.** The completed October22 wall scan found a1.015720m `candidate_hint` in44 rays, including the required wall-profile, landing and headroom samples. The observation-only bridge made zero movement writes and recorded shutdown. Its789 clock publication refusals all preceded the test instructions; the raw warning remains documented. A sampled candidate does not establish whole-body clearance, native guide registration or an added climb. [Runtime results](docs/LedgeRuntimeResults.md).

Progress checked November2: the rebuilt package matches the installed v0.0.21 manifest/PAK hashes. All26 Python tests, double/retail-float32 Lua checks, seven native bridge/core fixtures and admission-hook/conformance checks pass. No game process is running and no new playtest was performed. Next work is native guide loading/registration, followed by whole-body clearance and host action ownership; no repeated wall scan or ordinary Jump baseline is needed. [Status](STATUS.md), [plan](docs/LedgeClimbing.md), [test](docs/TestPlan.md), [native guide route](docs/KCD2HigherClimbRoute.md).

**HUD introduced in v0.0.15:** original Thief light-gem art beside the bottom health HUD, with aspect-preserving scaling and no visible noise meter. Noise remains in background diagnostics. Corrected in-game proportions/placement still await a brief visual check; guard/sound behavior is unchanged. The proprietary art and generated package stay local; this repository contains the adapter and conversion tooling. [Art provenance and build](docs/HudAssets.md), [feedback results](docs/StealthResults.md), [controls](docs/Stealth.md).

The preserved v0.0.12 movement baseline passed clear-ground, ordinary-wall, gentle-slope, short-stair and low-step checks. Its complete audit retains all1,011 writes with no unsafe/vertical writes, drops or bound violations, with the exact clock-refusal/timeout limitations documented. Further movement microtests are off the critical path. [Terrain results](docs/TerrainResults.md); [F8 controls](docs/PrototypeControls.md).

The native investigation adds [REA setup](docs/REASetup.md), [recovered Thief swoop behavior](docs/ThiefSwoopFindings.md), a [retail movement-interface trace](docs/KCD2MovementInterface.md), and retail observation captures. The first full state bridge published 4,004 snapshots and matched 3,075 player requests without changing movement; its required buff getter was unavailable, so v0.0.7 isolates the native experiment. The independent donor Hermite primitive remains separate from the conservative host request pulse.

Target installation: Steam retail **1.5.6 / build 23914554**. Modding tools **1.5.5 / build 22816715** are a separate, currently incomplete development workspace. See [assessment](docs/CapabilityAssessment.md), [environment](docs/Environment.md) and [status](STATUS.md).

## Local commands

Run PowerShell in `C:\Dev\KCD2 Mod\KCD2-Thief-mod`:

```powershell
python tools/project.py build
python tools/project.py install
python tools/project.py collect
python tools/project.py uninstall
python -m unittest discover -s tests -p 'test_*.py'
```

The Lua package build requires Python 3.12 and a Lua 5.1 interpreter, configured in ignored `.local/environment.json`. The local interpreter was built from official Lua5.1.5 source using installed MSVC. Basic diagnostics load without a native loader; the current per-frame stealth HUD and F8 movement use the explicitly attached native bridge. It is not automatically deployed by `project.py`. [Environment](docs/Environment.md) records paths and hashes.

Build runs diagnostic, bridge, session-isolation and historical speed-trial regression checks plus Lua syntax checks, then creates `build/kuttenberg_thief/Data/kuttenberg_thief.pak` as an uncompressed ZIP. The historical speed script/buff are validated as source but explicitly excluded from the package. It packages our Lua and a generated observation branch in the exact installed player behavior tree. Original assets remain outside tracked source; the source asset hash is required and other mods overriding the same asset are rejected. See [observer design and limitations](docs/XGenObserver.md). The official entry is `Scripts/Mods/kuttenberg_thief.lua`.

Install checks the inspected retail binary hashes, requires the game closed, creates a complete SHA-256-verified save backup under `.local/backups/`, then adds `Mods/kuttenberg_thief`. Existing files with different hashes cause an explicit stop. An existing `mod_order.txt` that excludes this mod also causes a stop, preserving its order. Run an elevated shell if Windows denies writing to Program Files.

Uninstall verifies the install receipt and every owned file, archives the mod under `.local/backups/`, then removes only these unchanged files. Modified/unknown files cause a stop. For updates, close the game, uninstall the owned version, rebuild and install.

## Historical diagnostic check

The current combined stealth check is in [Stealth](docs/Stealth.md). The older read-only diagnostic procedure below is retained for regression use, not another outstanding manual test.

1. After installation, launch normal KCD2 from Steam. No launch-option change is required for the automatic diagnostic.
2. Load a Kuttenberg save. The mod automatically observes contact at 10 Hz and records up to 120 state summaries, roughly one per second. Walk, stop, crouch, crouch-walk, turn, pause/resume, then load the same save again.
3. Exit the game and run `python tools/project.py collect`. The relevant excerpts go into `.local/logs/`.
4. Tell the agent the test is complete so it can inspect `kcd.log` locally.

Optional console commands: `kt_probe` (API and snapshot), `kt_watch` (restart 120-sample capture), `kt_mark` (snapshot marker), `kt_stop` (stop timed capture; XGen state-change logs remain bounded independently), `kt_rays` (3 m camera ray), `kt_detail` (v0.0.2: physical stats, AI stance and movement-variable reads). `kt_watch` and `kt_rays` worked in the first retail playtest. No keys are rebound. Ray output is diagnostic and is not a whole-body collision check. Missing getters are logged as `unknown:missing`, never interpreted as a safe state.

Logging configuration is at the top of `src/Data/Scripts/KuttenbergThief/Probe.lua`: `enabled`, `intervalMs`, `maxSamples`. Rebuild/reinstall to change it. The probe does not explicitly save custom state. v0.0.4 changes the runtime behavior-tree structure: use a pre-existing backed-up save for testing; new-save serialization and removal compatibility are not yet verified.

## Project map

- [Architecture](docs/Architecture.md): ownership, interfaces, fallback route.
- [Movement specification](docs/MovementSpec.md): behavior, measurement and configurable targets.
- [Test plan](docs/TestPlan.md): gate checks and full swoop acceptance scenarios.
- [Sources](docs/Sources.md): pinned reference revisions and evidence limitations.
- [Change log](MODLOG.md): meaningful changes and verification.

### Diagnostic v0.0.3

Read-only contact sampling at 10 Hz, with state summaries at 1 Hz and contact-change logs. `kt_watch` observes for up to 120 simulation seconds; `kt_stop` stops it. This release tests IsFlying and remaining state candidates, not a swoop. See [contact test](docs/TestPlan.md) and [native inspection evidence](docs/NativeInvestigation.md).


### Diagnostic v0.0.4

Adds a generated XGen observer for actual crouch and activity states. Read [XGenObserver](docs/XGenObserver.md) before testing. Load the existing save, toggle crouch three times, pause/resume and reload that save. Expected log: `actor_observer crouch=false`, then `true`, then `false`; two crouch on/off cycles and other activity transitions are now visible in the v0.0.4 retail log. No swoop key exists yet. Summarize a collected log with `python tools/analyze_probe.py <log-path>`.

### Historical speed comparison v0.0.5

This experiment is excluded from the current native-only package because retail lacks the documented buff confirmation getter. Source/tests and the [historical procedure](docs/SpeedTrial.md) are retained. Use the [current bridge procedure](docs/StateBridgePrototype.md) for testing.

No keys are rebound automatically. The user-selected F8 binding and current native controls are documented in [PrototypeControls](docs/PrototypeControls.md).
