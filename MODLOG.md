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
- v0.0.2 ran in normal retail: 44 samples, stamina/health readable, MotionJump and MotionLand captured, AI stance remained 3 through camera-height changes. Physical stats contain mass/gravity/flags only. Archived filtered capture at `.local/logs/20261011T171321429451Z-probe.log`.
- Traced the native IsFlying Lua wrapper using bounded static disassembly: physical status type 2, nonzero integer at +4 returned only after successful query. Matches a living-entity contact lead; runtime verification pending. Details and limits in NativeInvestigation.
- Built v0.0.3: contact getter sampled at 10 Hz with change-only logs, summaries at 1 Hz, separate fly-mode/sitting/lying reads and historical stats candidates. Tests cover false, true, missing/error contact without treating unknown as grounded. No movement writes; no native plugin installed.
- Installed v0.0.3 after owned archive/removal and verified 237-save backup. Runtime: 79 snapshots, 26 successful contact-state observations, two generations after same-save reload. Timers show a 6.8348 s async versus 1 s simulation gap during requested pause. GetStats and all historical stats candidates absent.
- Found actual player crouch checks in shipped guard XGen behavior. Chose to test an appended observational branch in the existing player tree before native injection. Original source bytes/node order are preserved; hash-pinned generator and conflict checks added. No installed original is overwritten.
- v0.0.4 passed Lua regression/syntax, XML generation/preservation and five Python tests. Installed with another verified 237-save backup, after archiving/removing v0.0.3. Awaiting runtime stand/crouch/pause/load test. New-save tree serialization remains unverified; no movement or stealth changes.

## 2026-10-12 — v0.0.4 result and v0.0.5 movement comparison

- Archived v0.0.4 retail test: 120 state snapshots, 17 successful contact changes, 47 XGen change records, two crouch on/off cycles and multiple incompatible activity transitions. No relevant error matched. Reload and custom movement were not proved by this capture.
- Narrow native inspection rejects SimulateOnAction for forward movement in the inspected C_Player dispatch. Verified action names in installed input assets. Global speed CVars remain unwritten.
- Added an opt-in, player-only rms speed trial using one new finite/nonpersistent buff definition based on installed perk/quest mechanisms. Refusal gates, uniquely scoped removal, recovery samples, error lockout, identity checks and pause-gap cancellation; no forced input or transform writes.
- Lua diagnostic and trial tests passed; five deployment/observer Python tests passed. Initial trial fixture stopped before the designed recovery duration and was corrected; no failed build was installed. Final installed/runtime status is recorded in STATUS.

- Deployment: v0.0.5 installed and hashes verified while KCD2 was closed. Archived the previous owned package and backed up/verified 237 saves. No v0.0.5 running-game claim yet; user test requested.
