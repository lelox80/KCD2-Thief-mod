# Movement and stealth specification

## Normal Jump wall climbs

The two local Kuttenberg wall guides share the normal Jump gate. The live pose and aim choose a lane across the authored ledge. Standing posture, ground contact, capsule corridor and landing support are checked on each attempt. The 1.64 m guide has a scoped height allowance and a bounded lookup retry for nearby approaches. A recovered approach still lands within the authored strip. The 1.02 m guide uses stock VaultOnto parameters.

## Higher-wall target — normal guide load baseline passed

The goal is climbing higher walls through the host's traversal, animation and controller. The normal save-load capture, owned archive attribution and inactive guide storage all passed in retail. The experiment packages were archived and the original installed hashes were verified. The next registration test needs an independent traversal guard because the selector can bypass some action-type flag checks. [Native route](KCD2HigherClimbRoute.md).

The next playable increment remains one owned ordinary-range guide with proven source, registration/handle lifetime, whole-body route and exclusive host handoff. Higher action types5/6/7 need complete configuration and suitable taller motion; changing allowedHeightMax alone is insufficient. No added climb is enabled by the observer.


## Current runtime - v0.0.21 wall candidate verified, no added climbing

The latest implemented change supports one measured compound wall profile and mandatory junction/final-face samples. It preserves normal/height/footprint/headroom and all canonical lifecycle gates; max46/hard48 rays. Both number models and native/Python suites pass, rechecked November2. The completed October22 v21 scan returned a1.015720m `candidate_hint` in44 rays, with all mandatory profile/landing/headroom samples. Observation-only bridge shutdown is recorded with zero movement writes. All789 clock publication refusals preceded instructions; their raw warning remains explicit. No admission observer or added climb was used.

The user-observed ordinary Jump is expected: neither v20 nor v21 submits an added climb. Sampled top and landing geometry are now known; whole-body clearance and native guide registration remain unknown. The selected shipped member has no nearby guide at the v20 pose; its distance must not be transferred to the different v21 pose or treated as live-registry completeness. No repeated scan or Jump baseline is needed. The next playable milestone is one owned ordinary native climb target with proven load/lifetime, full-body route and host handoff; taller native motion follows separately. [Route](KCD2HigherClimbRoute.md).

## Current correction - v0.0.20 wall check complete; compound-profile work next

The native ownership correction passed retail:2 fully matched requests and user-confirmed ordinary climbing, with no admission errors and all six hooks restored. Captured type2 is ordinary VaultOver; the low obstacle's unsupported climb-onto footprint remains a valid refusal. Higher climbs are still disabled.

v20 fixes a continuous slanted-face sampling error using the full3D face plane, a bounded upper anchor, exact face/cap intersection and a near-lip confirmation for bare slanted faces. It preserves flat top/footprint/headroom,0.65–1.5 onto height, identity/lifecycle/expiry and finite work. The new path adds at most two rays, conservative maximum44 under the hard48 limit. Offline tests are separate from the next retail geometry check at a broad flat top. v20 is now installed/hash verified with fresh independently verified saves; its completed wall scan refuses an unsupported lower-face/bevel/upper-face profile before all top/landing/headroom checks. All native searches return no handle. No new climb was enabled; all six hooks are now restored.

Higher climbs require a registered target, configured native high action, suitable taller animation/warping, whole-body clearance and native interruption/camera ownership. High-slot defaults supply no authored positive higher interval; animation weighting separately clamps to0..1. Changing a maximum height alone does not meet these requirements. [Route](KCD2HigherClimbRoute.md).

## Historical v0.0.18 specification at build time

The long-term target remains higher climb-onto ledges through native traversal. v18 first improves request failure tracing, then fixes the observed sloped-cap discovery error. A positive geometry hint still requires a verified flat surface, five landing points, five headroom rays and the existing0.65–1.5 native onto band. Pitched/downward low-surface discovery without a verified face never becomes a hint. Higher diagnostic tops are measured/refused; no parameter, player limit or animation is extended by this build.

Native tracing retains exact owner/caller guards and reports the failed predicate; actual candidate-associated admission still needs a fresh ordinary-climb baseline. Registered-target lifetime, swept body clearance and exclusive host action handoff remain prerequisites for any new climb. The retail authored index loader and HighVault classes are concrete future leads, with export/mounting/default/animation limits explicitly unresolved. [Route](KCD2HigherClimbRoute.md), [last test](LedgeRuntimeResults.md).

## Current ledge scope — v0.0.17 comparison complete

The v0.0.17 same-spot check is complete: small obstacle climbed, higher walls refused. Broader geometry still missed the low obstacle and rejected higher-wall lips. Native search/selection were observed, but three guarded request copies failed and no candidate-associated framework acceptance is qualified. Both observers stopped/all six entries restored, with zero bridge movement writes. These are diagnostic results; added climbs remain disabled. [Results](LedgeRuntimeResults.md), [admission boundary](KCD2LedgeAdmission.md).

The user chose approach+fresh Jump to climb onto a reachable static ledge. Hanging, shimmying and drop-to-grab are deferred. Proposed gameplay selects a valid oriented target and hands it to KCD2's native ledge action, retaining host collision/animation/camera ownership. The first range follows the authored wide `VaultOnto` allowed0.65–1.5 band; it is not a new measured climb limit. Registered target lifetime, full-body clearance, admission and interruption remain prerequisites.

Current code implements read-only geometry hints and native admission observation only. Diagnostics reach1.6m forward and inspect faces through3.1m above entity origin to explain low/tall surfaces; these are sampling bounds, not playable climb heights or measured foot/capsule geometry. A hint still requires the native0.65–1.5 onto band, landing footprint and point headroom. `kt_ledge_probe` never starts a climb or certifies a swept route. Framework request acceptance does not establish animation completion. The swoop's grounded/crouched gates and horizontal writer are preserved; a future climb needs its own phase-aware state, registered target lifetime, full-body clearance and explicit host handoff. [Detailed plan](LedgeClimbing.md), [donor evidence](ThiefLedgeFindings.md), [host interface](KCD2LedgeInterface.md).

v0.0.15 changes only feedback presentation: original Thief gem art driven by KCD2 aggregate visibility, beside the bottom health HUD with uniform scaling. Noise stays in background diagnostics. It preserves the accepted movement writer/tuning and does not add perception or sound writes. Display mappings and runtime acceptance are separate from movement acceptance; see [HUD assets](HudAssets.md).

Behavioral reference: installed Epic Thief 2014 1.7.0.5, UE3-derived engine. Static authored settings and trajectory math are now recovered; no original runtime travel/timing measurements or world-scale conversion have been made. No extracted animations are assumed compatible. See [donor evidence](ThiefSwoopFindings.md).

## Swoop target (full acceptance pending)

- Trigger only from supported crouched locomotion; choose direction from current intended movement, with a configurable forward fallback.
- Short directional burst with smooth acceleration and deceleration. Preserve normal collision, gravity and ground response. Shorten/stop at obstacles; no penetration or transform teleport.
- Predictable stairs, slopes, narrow passages and uneven ground. Initial prototype cancels on lost support/ledge rather than trying to propel across gaps.
- Configurable distance, duration, cooldown, input binding/direction policy and camera response. No default values labelled as original Thief values.
- Integrate a normalized smooth speed profile by simulation time; compare measured distance/time at 30/60/120 FPS. Never use fixed distance per frame.
- The recovered donor uses a cubic Hermite trajectory with endpoint velocities and continuous retargeting while steering. The independent reconstruction now implements that primitive. Authored duration is 1.15 seconds, distance 525 original units, with different exhausted/default values; these are not established host meters or a complete runtime profile. Host speed clamps and collision can change achieved travel.
- Stop on cancellation or incompatible state and release only state owned by this action. Cooldown policy after cancellation is configurable and recorded.

Planned config keys: `swoop.enabled=false`, `distanceMeters`, `durationSeconds`, `cooldownSeconds`, `maxFrameGapSeconds`, `cameraResponseEnabled`, `cameraDipMeters`, `cameraRollDegrees`, `cameraBlendSeconds`, `cancelOnLostGround=true`, `logging.level`, `logging.sampleHz`. Movement values are deliberately unset until the collision-control gate; M0's real configuration only controls diagnostic logging.

## Reference measurement procedure

Capture Thief at a fixed FPS with visible input/activation timestamp on clear flat ground. Repeat five forward and five lateral swoops from stationary crouch and moving crouch. Count start, peak and recovery frames; record cooldown/retrigger behavior, steering authority, camera translation/roll, interruption, walls, slopes and stairs. Use a verified world scale or report distance in reference-relative units. State recording FPS, game settings, uncertainty and equipment/stance. Do not derive exact travel from perspective alone.

Record table: trial, build, FPS, start/end frame, travel/scale method, repeat interval, obstruction response, camera response, notes. No measurements recorded yet.

## Progressive traversal

1. Mantle/vault one known Kuttenberg obstacle: obstacle height/reach, upward ledge normal, full body clearance, landing footprint, swept path, support continuity and cancel recovery. Record world location/save; then generalize.
2. Lean: smooth configurable camera translation/rotation with near-wall clamp; verify guard detection origin and exposure. Lean cannot grant untested invisibility.
3. Selected ledges: grab, hang, climb up, move along support and drop, with explicit supported geometry. Require clearance and load/interruption recovery. Roof visuals alone are not collision evidence.
4. Presentation: compatible first-person hands, camera, footsteps, landings and animation transitions. Prefer compatible existing KCD2 animations before authoring; Thief extraction/retargeting remains unverified.

## Thief-style stealth target

Measure the reference's light exposure, movement/surface noise, crouch effects, attention/suspicion/recovery, search response and feedback. Then compare KCD2 at controlled baseline positions. Define which host perception inputs can be modified while retaining NPC/crime/save consistency. Noise and stamina costs during swoop, fall damage, guard reactions to rooftop movement and lean visibility are explicit acceptance items. Stealth remains KCD2 vanilla during M0 and all movement feasibility probes.

v0.0.13's passive visibility/noisiness/footstep-multiplier HUD exposes host-derived values without changing perception or sound production. The combined retail feedback check passed on October21: visible changing values and lower signed visibility at night. Raw visibility includes context and is not a pure illumination sensor; noisiness and the footstep multiplier are not instantaneous heard sound. Calibration and behavioral changes remain next. The accepted swoop is frozen; no further terrain microtests are prerequisites. [Stealth results](StealthResults.md).

## Current implementation boundary

v0.0.12 adds only passive terrain diagnostics and evaluation tools. Native movement code, 0.9-second envelope, gain/travel/cooldown caps and strict support/Z/mode guards remain unchanged from v0.0.11. Paired ordinary/live retail trials passed on the tested gentle slope, short stairs and low step; the player confirmed smooth boosts and normal landing/fresh-press recovery. Serial31 cancelled when support became unsafe and did not resume on later safe requests. New terrain still requires ordinary traversal before boosted comparisons. Support-loss cancellation stops new scaling; it does not provide an edge brake, gap traversal or a ledge grab. [Results and evidence limits](TerrainResults.md); [procedure](TerrainValidation.md).

Current v0.0.11 retains v0.0.10's nominal 0.90-second envelope, 2x peak horizontal gain, 4.0 total request-travel budget, 1.0-second cooldown and 6.25 absolute request cap. At base 3.101, offline travel is 3.975415410 request units over 0.833333333 s. A final averaged command applies for one host frame, so physical duration has at most one frame of quantization; this is not an exact 0.900000-second world-motion cutoff.

The user confirmed immediate noticeable moving-crouch F8 bursts and normal initial control. Seven groups / 364 actual writes satisfied gate, identity, horizontal/vertical and travel bounds. A logger fault then stopped further activation, leaving held-key/no-queue/tuned-wall acceptance incomplete. v0.0.11 fixes logging and reports safe logger faults as disabled/restart-required; the corrected build passed the controlled F8, held/no-queue/release and wall retest, including a successful final burst after a session longer than six minutes. The 1,022 audited writes have no safety/vertical/bound violations and logger shutdown is clean; one already-unsafe clock rollback refusal is retained as a capture-bound reviewed exception. The earlier smaller v0.0.9 profile passed dry reset/pause/reload and live wall/control tests.

This remains a host request pulse, with KCD2 retaining collision, gravity and animation. It does not implement the donor's full Hermite retargeting, calibrated scale, camera response or stealth integration. [Implementation](StateBridgePrototype.md) and [repeat test](PrototypeControls.md).

## v0.0.5 comparison before swoop

The first movement experiment leaves ordinary direction/input with KCD2 and briefly applies a player-only rms modifier. Arm 3 s, baseline 2 s, boost <=1 s at a diagnostic 1.25 multiplier, recovery 2 s, cooldown 2 s. Timing/guards are constants in SpeedTrial.lua; the owned buff table uses a build-pinned diagnostic multiplier of 1.25. These are not Thief measurements; full swoop tuning is still to be implemented. No camera change, automatic displacement or smooth envelope yet. The full crouched swoop criteria above remain unmet until comparison/collision/cancellation pass and the actual burst controller is implemented. [Procedure](SpeedTrial.md).
