# Mod log

## 2026-10-11 — M0 capability investigation and probe

- Inspected the pre-existing empty Git repository; no existing source changes to overwrite.
- Identified retail 1.5.6, tools 1.5.5, Thief Epic 1.7.0.5 and Windows 10 Pro 22H2.
- Read official packaging/Lua/development documentation, installed API reference and targeted retail scripts; inspected the four requested source repositories.
- Chose the official Lua mod entry as initial route. No native loader or third-party installer executed.
- Added a read-only capability/player-state probe plus repeatable build, verified save backup, install, removal and log collection.
- Built Lua 5.1.5 from official source locally. Diagnostic regression checks passed: absent player, missing getters, getter error, entity replacement, bounded logging, cancellation, stale callback, ray-call arguments and no movement writes.
- PAK uses ZIP_STORED; manifest and archive checks passed. Deployment and in-game results are recorded in STATUS when available.
- Installed only Mods/kuttenberg_thief after a verified 237-file save backup (784,657,364 bytes). Original installation files were not replaced.
- First retail run used no -devmode launch argument. `[KT] boot`, gameplay-start listener and 120 samples verified; positions, velocities, camera and activity flags returned data; bounded logging completed. No probe-specific Lua errors found in this capture.
- GetStance and tested pause getters are absent. MotionIdle/MotionMovement/AnimationControlled observed. Additional labelled crouch and ray tests requested. User confirmation, collision-aware movement and swoop remain pending.
- Second capture reached 240 samples; kt_watch and kt_rays executed. Ray hit at 2.2592 m with hit position/normal. Crouch-like camera-height change did not produce a distinct animation state; do not use that animation as a stance gate.
- Prepared v0.0.2 with official AI.GetStance and physical-stats probes, documented soul health/stamina reads and read-only CVar candidates located in the installed DLL. No movement variables are written. Awaiting closed-game deployment.
- Three temporary-fixture deployment tests passed: verified backup/install/idempotence/owned removal, preservation of edited installed files, rejection of changed game binary and excluding mod order. Live uninstall not yet exercised.
- After KCD2 closed, archived and removed owned v0.0.1 successfully, backed up 237 saves again, installed and hash-verified v0.0.2. This verifies live removal/reinstallation of the owned package; v0.0.2 running-game results are pending.
