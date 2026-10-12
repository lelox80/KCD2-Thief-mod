# Status — 2026-10-12

**M0 state observation works in retail. M1 speed comparison v0.0.5 is built and installed; runtime pending. No completed swoop or stealth replacement.**

| Evidence level | Result |
|---|---|
| Written | Official Lua diagnostics, generated XGen observer, one additive owned speed buff, opt-in bounded speed comparison |
| Built | Lua 5.1 regression/syntax checks and five Python checks passed; final real-project package/CRC and owned buff validation passed |
| Installed | v0.0.5 hash-verified; prior v0.0.4 archived; 237 save files backed up and verified |
| Running game verified | v0.0.4: 120 snapshots, 17 successful contact changes, 47 actor-state observations including two crouch on/off cycles, air, dialogue, horse, bed, minigame and animation activity |
| User confirmed | User performed diagnostic play; no subjective speed or collision test yet |

Current route: supported PAK + Lua + XGen state observations, with a **player-only RPG speed effect** as the next locomotion experiment. Shipped crouch perks use `rms*1.15/1.2`, and quest effects also modify rms. The experiment applies only our unique, nonpersistent, finite-duration definition to Henry. KCD2 remains the sole input, collision, camera and animation controller. No native loader, executable patch, CVar modification or forced motion.

`SimulateOnAction` is rejected as the forward movement route for this build: the inspected C_Player virtual dispatch filters to six combat actions, excluding moveforward. Global speed CVars remain unproven and are not written. See [native evidence](docs/NativeInvestigation.md) and [experiment](docs/SpeedTrial.md).

Latest capture: `.local/logs/20261012T190752390468Z-probe.log`, summary `.local/evidence/v004-summary.json`. The 47 XGen lines continue for about 38 minutes; the timed 120-snapshot window ends earlier. Two long async/simulation gaps occurred, but do not independently identify their cause. Camera/body height ranged -0.3057 to 26.518 m across all snapshots: reject camera height as a stance source. No relevant error matched the collector. This capture contains one generation; it does not prove a v0.0.4 reload. Earlier v0.0.3 did observe a second gameplay generation.

Next: user runs `kt_speed_test` crouched on open ground and against a wall. Confirm the new buff loads, the rms value and actual speed change, removal, ordinary movement recovery and wall contact. If an API/state is unknown, the command refuses and logs the precise reason. Do not escalate to a full swoop before this result.

Open: runtime buff API/table loading/expiry/removal; cancellation timing on pause/load and all incompatible states; slopes/stairs/ledge behavior; smooth distance/duration control and activation binding; noise, stamina, damage and detection; Thief save location and measurements; development tools setup/version mismatch. Test only with a backed-up existing save. New XGen save serialization/removal remains unverified.

Latest verified backup: `.local/backups/20261012T192114257141Z` (237 files). Previous installed package archived at `.local/backups/20261012T192113736109Z-uninstalled`. User asked for the open-ground comparison; feedback pending.
