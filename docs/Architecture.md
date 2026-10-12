# Architecture

## Implemented M0

Official `Scripts/Mods/kuttenberg_thief.lua` loads our Lua probe. It registers diagnostic console commands and a gameplay-start listener, queries getters under `pcall`, and observes contact at 10 Hz for up to 120 game-time seconds, with 1 Hz player summaries and change-only contact logs. It reacquires the current player every time and holds no actor/physics pointer across loads. A stale timer from a script reload checks module identity before doing anything. No physics, input, camera, animation, NPC or save data is changed.

The probe reports **method presence**, **getter success** and **raw values** separately. Missing/nil/error state is unknown. `grounded=unknown` is deliberate: neither a low vertical speed nor a downward ray proves character ground contact. Engine callbacks/state-transition reliability still need a running-game test.

Source, tests and tools are tracked; `.local` contains configuration, interpreter, backup, diagnostics and receipts. Builds are ignored. Package hashes verify install/removal ownership. Original game files are never replaced by M0.

## Proposed movement ownership (not implemented)

| State | Input owner | Physics owner | Camera owner | Animation owner |
|---|---|---|---|---|
| Idle/cooldown | KCD2 plus one activation observer | KCD2 living-character controller | KCD2 | KCD2 |
| Swoop active | Adapter provides one directional request, arbitrate ordinary movement | KCD2 controller resolves collision/gravity/grounding | KCD2 plus one verified additive response | KCD2 locomotion unless compatible authored action is proven |
| Mantle/vault | Adapter owns traversal input; cancel routed explicitly | One verified swept path/controller mode | One owned blend | One resource-hosted action, no competing root motion |
| Lean | KCD2 ordinary movement | KCD2 body; checked camera offset | One additive lean owner with collision clamp | KCD2, then optional compatible pose |
| Hanging | Adapter handles climb/drop/cancel | Verified constrained support/controller | Owned blend | Compatible hang/climb action |
| Suspended/failed | KCD2 | KCD2 | KCD2 after removing our layer | KCD2 after releasing our resource |

Never run a position writer alongside a velocity writer/root-motion owner. Prefer steering the existing controller over changing its physicalization. Do not set noclip, disable world collision, cancel gravity or teleport to force an outcome.

Action state machine: `Unavailable -> Idle -> Active -> Recovering -> Cooldown -> Idle`. Any unknown gate, loss of player, load, pause, incompatible activity or controller loss cancels to `Suspended/Unavailable`. No automatic continuation after a load. Re-evaluate all gates on a new gameplay generation.

Gate contract: loaded/alive/conscious, supported build, current player handle, crouched, grounded, not mounted/climbing/swimming/in combat/dialogue/cutscene/interaction/pickpocket/corpse carry, normal camera/controller available. Individual reliable interfaces remain to be proven. Missing gate disables action. Do not infer safe state from `nil`.

Frame-independent profile uses elapsed game simulation time and normalized progress. Evaluate the integral between consecutive timestamps so target travel is independent of FPS. First runtime samples show a large `GetCurrTime` value around 547,000 seconds with coarse fractional increments; do not base short swoop timing on subtracting that float without testing its precision. Prefer accumulated verified per-frame delta or a suitable native simulation clock. Physics still resolves allowed movement each step. Abort on excessive time gaps, lost support, unexpected displacement, blocked path, controller takeover or user cancel; never catch up missed travel with a large jump.

## Native fallback contract

Only if needed: build-pinned adapter for read-state, controller request, physical query and lifecycle notification. Native pointers stay in native code, with generation validation; Lua owns high-level tuning/state. Validate signatures, disassembly, layouts, virtual-call targets and update order on the inspected DLL hash. Refuse startup on mismatch. Do not copy the loader's noclip implementation into the movement adapter.

## Stealth integration

KCD2 owns NPC identity, schedules, world perception, crime and persistence. Thief-style lighting/noise/suspicion/search behavior must be mapped to measured KCD2 mechanisms. First instrument baseline guard responses at fixed positions, equipment, weather and time. Determine whether detection traces target eyes/head/body and whether lean changes these points. Do not make a visual light gem, silent footstep effect or camera peek stand in for an actual perception change.

The replacement's eventual scope includes visibility, noise, suspicion buildup/decay, search behavior and player feedback. Exact formulas/thresholds are open until measured. Keep KCD2 crime outcomes and saves coherent; record any unavoidable behavioral differences.

## Contact investigation

v0.0.3 reads actor:IsFlying with explicit success/nil/error handling. Its native wrapper matches a living-physics status query; [static evidence](NativeInvestigation.md) is stronger than documentation alone but not a playtest. Keep `grounded=unverified` in contact logs until standing/jumping/landing behavior is confirmed. The separate player:GetFlyMode getter is not a ground query. AI.GetStance is rejected as a sole crouch gate based on the v0.0.2 observations.


## XGen observer (implemented, retail state transitions observed)

v0.0.4's build-time generated override appends one state observer to the player's existing Parallel; Lua accepts complete nine-state batches and clears them on gameplay start. All original branches and bytes are preserved. The observer is the supported bridge tested in retail for crouch/activity gates absent from Lua. [Design, conflict and save limits](XGenObserver.md). Its state must be treated as unknown when absent, incomplete or stale; v0.0.5 uses these observations as refusal gates for the opt-in comparison. v0.0.3 verified physical contact transitions and a new gameplay generation after reload, but not action cancellation.

## v0.0.5 selected movement experiment

The XGen observer is now runtime verified for crouch and several incompatible activities. v0.0.5 adds an opt-in RPG modifier trial, described in [SpeedTrial](SpeedTrial.md). KCD2 retains all input/physics/camera/animation control; Lua owns one uniquely identified buff and its cleanup. It is a bounded comparison of normal locomotion, not directional injection or a completed swoop. Pause/load callback ordering and finite buff expiry still need runtime validation; this limits the experiment's acceptance. SimulateOnAction is not the chosen movement request: inspected player dispatch excludes moveforward.
