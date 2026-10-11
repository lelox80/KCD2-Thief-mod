# KCD2 Thief movement project

- Target: retail KCD2 1.5.6, Steam build 23914554. Installed tools are 1.5.5, build 22816715; never assume parity.
- Preserve KCD2 as host for world, collision, NPCs, crime, interactions and saves. Thief 2014 is the behavioral reference for movement and stealth.
- Work in this repository. Inspect Git status before editing. Preserve unrelated changes. No publication, uploads or pushes without explicit user instruction.
- Use official mod interfaces first. Capability existence, successful call and correct in-game behavior are distinct evidence.
- Verification order: code loads and reads state -> collision-aware movement control -> swoop. Teleport/noclip cannot satisfy movement acceptance.
- Never write movement when stance, grounded state, lifecycle safety or controller ownership is unknown. Keep unsupported actions disabled.
- Do not import UE4/5 tool assumptions into Thief's UE3 fork. No two-game bridge or whole-game decompilation without demonstrated need.
- Inspect third-party installation/build scripts before running. Do not log credentials, account IDs, private configs or full process command lines.
- Installed assets, binaries, extracted scripts, saves, logs and third-party checkouts belong in ignored `.local/`, never tracked source.
- Back up and verify saves before playtests. Do not replace original executables. Own only `Mods/kuttenberg_thief`; preserve other mods/load order.
- Use `python tools/project.py build|backup|install|uninstall|collect`. Install/removal require a closed game. Installer hashes the inspected retail binaries.
- Record written / built / installed / running-game-verified / user-confirmed separately in STATUS and MODLOG.
- Keep README, docs/Architecture.md, docs/MovementSpec.md, docs/TestPlan.md, STATUS.md and MODLOG.md current.

