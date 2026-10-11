# Movement and stealth specification

Behavioral reference: installed Epic Thief 2014 1.7.0.5, UE3-derived engine. No original timings/distances have been measured yet; no extracted animations are assumed compatible.

## Swoop target (not implemented)

- Trigger only from supported crouched locomotion; choose direction from current intended movement, with a configurable forward fallback.
- Short directional burst with smooth acceleration and deceleration. Preserve normal collision, gravity and ground response. Shorten/stop at obstacles; no penetration or transform teleport.
- Predictable stairs, slopes, narrow passages and uneven ground. Initial prototype cancels on lost support/ledge rather than trying to propel across gaps.
- Configurable distance, duration, cooldown, input binding/direction policy and camera response. No default values labelled as original Thief values.
- Integrate a normalized smooth speed profile by simulation time; compare measured distance/time at 30/60/120 FPS. Never use fixed distance per frame.
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
