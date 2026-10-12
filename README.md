# Kuttenberg Thief

Thief (2014)-inspired movement and stealth inside Kingdom Come: Deliverance II.

**Current source: v0.0.5, state diagnostics plus an opt-in speed comparison.** Automatic startup only observes state. `kt_speed_test` can temporarily raise Henry's normal crouched movement speed; this is an experiment, not the swoop. Stealth, camera and animation control remain with KCD2. See STATUS for installation/runtime evidence.

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

Build requires Python 3.12 and a Lua 5.1 interpreter, configured in ignored `.local/environment.json`. The local interpreter was built from official Lua 5.1.5 source using installed MSVC. No pip packages, native loader or engine recompile are needed. [Environment](docs/Environment.md) records paths and hashes.

Build runs diagnostic regression checks and Lua syntax checks, validates the one additive, nonpersistent buff definition, then creates `build/kuttenberg_thief/Data/kuttenberg_thief.pak` as an uncompressed ZIP. It packages our Lua and a generated observation branch in the exact installed player behavior tree. Original assets remain outside tracked source; the source asset hash is required and other mods overriding the same asset are rejected. See [observer design and limitations](docs/XGenObserver.md). The official entry is `Scripts/Mods/kuttenberg_thief.lua`.

Install checks the inspected retail binary hashes, requires the game closed, creates a complete SHA-256-verified save backup under `.local/backups/`, then adds `Mods/kuttenberg_thief`. Existing files with different hashes cause an explicit stop. An existing `mod_order.txt` that excludes this mod also causes a stop, preserving its order. Run an elevated shell if Windows denies writing to Program Files.

Uninstall verifies the install receipt and every owned file, archives the mod under `.local/backups/`, then removes only these unchanged files. Modified/unknown files cause a stop. For updates, close the game, uninstall the owned version, rebuild and install.

## First in-game check

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

### Speed comparison v0.0.5

After installation, load an existing backed-up save in Kuttenberg. On level open ground, crouch with weapon put away. Enter `kt_speed_test`, close the console, then hold your normal forward key for about eight seconds. The experiment waits 3 seconds, records 2 seconds of baseline, applies its speed effect for at most 1 second, removes it and records 2 seconds of recovery. Remain crouched throughout. A refusal is logged if any gate or required API is unknown.

Repeat facing a wall, then test cancellation by standing up and by pausing. `kt_speed_reset` explicitly removes only our test effect; `kt_stop` also cancels the trial. No keys are rebound. Do not create a new progress save during the trial. See [exact procedure and limitations](docs/SpeedTrial.md); report whether speed changes and whether normal movement returns. Exit and collect the log as above. No automatic experiment runs at startup.
