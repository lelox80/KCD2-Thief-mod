# Status â€” 2026-10-11

Milestone: **M0 â€” retail capability probe**. Source/build v0.0.2; installed and runtime-tested v0.0.1 until the next closed-game deployment. Movement and stealth replacement remain disabled.

| Evidence level | Result |
|---|---|
| Code written | v0.0.2 read-only Lua probe adds AI.GetStance, physical stats, stamina/health and movement-variable queries |
| Build passed | v0.0.2 Lua 5.1.5 regression/syntax checks; stored ZIP PAK/CRC; 3 deployment lifecycle tests passed in disposable fixture |
| Installed | Own Mods/kuttenberg_thief folder verified by SHA-256; 237 save files backed up and verified |
| Verified in running game | v0.0.1 official entry loaded in normal retail 1.5.6; 240 player-state samples, successful kt_watch and kt_rays; position, velocity, camera and several activity getters returned meaningful data |
| User playtest confirmed | Pending |

Selected route: official PAK/Lua mod first. A native adapter is a fallback if runtime evidence shows that retail Lua cannot supply a safe character movement request, state gates, or physics queries. The loader and development executable are not assumed compatible with retail 1.5.6.

Next action: close KCD2, archive/remove owned v0.0.1 and install v0.0.2; repeat a short stand/crouch/jump/wall test to inspect AI stance and physical stats. Presence of `SetVelocity` is insufficient: next gate is reliable state/grounding plus a controlled collision-aware motion experiment with normal actor control and interruption handling. Do not implement swoop before this gate passes.

Open: runtime availability and semantics of stance/grounded state, movement ownership, full-body sweep, pause/load events, footsteps/noise, stamina/fall damage and detection origin; Thief save folder/behavior measurements; installed tools' data workspace setup and version mismatch.

Observed missing in retail: `player:GetStance()`, `CryAction.IsGamePaused`, `Game.IsGamePaused`, `ActionMapManager.EnableActionFilter`. `GetPhysicalStats`, `SetVelocity`, `SimulateOnAction` and Physics ray calls exist, but their motion/contact semantics remain untested. First animation samples: MotionIdle, MotionMovement, AnimationControlled. No evidence yet for a crouch-specific state or pause/load recovery. No movement writes have been performed.

Second capture: camera height relative to entity position changed from about 1.60 to 1.17 m during the requested crouch test, while animation still reports only MotionIdle/MotionMovement. Camera height is not accepted as authoritative crouch. Ray returned one hit at 2.2592 m, a position and normal. Raycast read works; full-body sweep and grounded state remain unproven.
