# Test plan

## M0 — code loads and logs player state

Prerequisite: closed game, verified save backup, installed package hashes, retail 1.5.6. Launch normal retail through Steam; load a Kuttenberg save. Expect `[KT] boot version=0.0.4`, capability rows and `player_available`, followed by at most 120 one-second samples. Walk, stand, crouch, crouch-walk and turn; compare positions/velocity/raw stance. Pause/resume and reload the same save; record observed timer/event behavior. Optional `kt_probe`, `kt_mark`, `kt_rays`, `kt_watch`, `kt_stop`.

Collect with `python tools/project.py collect`. Confirm no Lua errors, meaningful player position, missing fields explicitly unknown, and game controls unaffected. Note if console commands are unavailable. A successful package install is not a runtime pass. GUI labels or method presence do not prove function semantics.

Automated: run Lua 5.1 stub tests for absent player, nil/error APIs, entity replacement, bounded capture, stop/reload, stale callback and documented ray argument shape. These check our error handling only, not engine API correctness. Build verifies manifest, archive entries, stored ZIP compression and CRC.

## M1a — collision-aware control proof (not implemented)

After M0, identify reliable gates and one movement-request path. Only then prepare a bounded diagnostic motion action with a clear cancellation command. Establish that the host controller resolves solid walls and ground contacts and releases ordinary movement after interruption. Log requested/actual velocity and displacement, collisions and controller state. A raycast, teleport or noclip demo cannot pass this gate.

## M1b — crouched swoop acceptance

| Scenario | Required result |
|---|---|
| Open flat ground | Smooth start/finish; measured travel/duration within declared tolerance; normal movement resumes |
| Wall directly ahead | Shortens/stops before full body overlaps; no penetration or shaking |
| Stairs up/down | Predictable step response, no tunneling or launch; normal ground handling retained |
| Slope up/down/cross | No unintended slide/air boost; collision and support logged |
| Narrow passage | Body width respected; side collisions clamp without getting stuck |
| Ledge | Initial policy cancels on lost support; no unexplained hovering or teleport |
| Repeated activation/held key | Cooldown honored; one action owner; no stacked velocity |
| 30/60/120 FPS | Comparable distance/time; tolerance recorded before test, not adjusted to disguise failures |
| Pause/resume | Action cancels or safely suspends per policy; no time catch-up jump |
| Load/save transition | Stale action cleared; new entity/generation reacquired; no persisted transient motion |
| Combat/dialogue/cutscene/horse/interaction | Refuse or cancel safely; ordinary control returns |
| Noise/stamina/fall/guards | Compare baseline footsteps and awareness; stamina/fall damage still meaningful; differences documented |

Each trial records action id/state, requested and actual start/end positions, elapsed simulation time, distance, velocity, collision/contact results, grounded state, frame rate, input direction, cancellation reason and build. Transition logs default on; detailed sample rate configurable and bounded.

## Later milestones

Mantle/vault: known obstacle first; too tall/far, overhead blockage, narrow landing, interrupted animation, moving blocker and failed clearance.

Lean: near-wall camera clamp, both sides, rapid alternation, pause/load, guards at known lines of sight. Compare visibility from camera versus body/head.

Hang/climb: no support, discontinuous support, roof collision, climb exit room, safe/unsafe drop and every lifecycle interruption.

Stealth: fixed equipment/time/weather/guard/save baselines; light/dark, quiet/loud surfaces, walking/crouching/swoop, suspicion/search/recovery. Confirm crime and save persistence.

## Save recovery

Backups are `.local/backups/<UTC timestamp>/kcd2-saves.zip` with SHA-256 manifest. Close game and Steam before manual restoration. Preserve the current save directory separately, extract a verified backup into the configured save directory and resolve Steam Cloud conflicts deliberately. Restoration is not automatic and has not been tested on the live profile.

## M0.3 contact test (read only)

After loading the existing Kuttenberg save, stand on clear ground for five seconds, jump three times with a pause between landings, crouch and walk for five seconds, stand up, and use an ordinary stair if nearby. Do not deliberately jump from a dangerous ledge. Pause for five seconds and resume; reload the same save after the initial capture. All normal game controls stay in charge.

Expected diagnostic result, not yet observed: IsFlying=false while supported, true during a jump, false after landing; no unknown/error values during ordinary locomotion. Logs include raw values, position, simulation/async times and animation at transitions. The probe does not cause motion. Check whether gameplay-start capture restarts after loading and whether pause produces an async-time gap. These observations do not alone prove safe action interruption. `kt_watch` starts another bounded capture if necessary; `kt_stop` stops it.


## M0.4 XGen state bridge

Load the backed-up existing save; toggle crouch three times, holding each state for 2–3 seconds. Check observer and summary fields agree, complete batches arrive at about 100 ms intervals, and timestamps stay fresh while active. Pause/resume, then reload the same save; old observations must clear. Review errors involving player/switch/switch.xml, ActorStateCheck and ExecuteLua. Confirm ordinary movement/interactions behave normally. Avoid creating a new permanent progress save until serialization/removal compatibility is assessed. The tests validate observation only, not motion or stealth changes.

## M1a — v0.0.5 bounded speed comparison

Follow [SpeedTrial](SpeedTrial.md) for the exact console sequence, expected behavior, unknown gates and cancellation limits. Capture open-ground before/boost/after samples, direct wall obstruction, standing-up cancellation, pause and same-save load. Confirm the modifier's table/API loading and removal separately from actual speed and collision. A successful automated fixture is not a retail test. Keep the full swoop matrix (stairs, slopes, narrow passage, ledge, repeated activation, frame rates, pause/load, stamina/noise/damage) outstanding until this primitive is proved.
