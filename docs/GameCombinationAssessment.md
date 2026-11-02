# KCD2 + Thief: game-combination feasibility assessment

Research date: 2026-10-17. This is a researched proposal, not a new implementation or runtime pass. The installed mod remains v0.0.5.

## Assessment

Combining selected Thief (2014) systems with KCD2 is a credible direction for a bounded prototype. The best-supported route is **targeted reverse engineering of Thief's movement, an independent implementation of the selected mechanics, and a small KCD2 native adapter**. This warrants investigation now, alongside the existing inexpensive speed experiment. Official Lua limitations do not establish the limits of the project.

The previous assessment underweighted relevant precedents in the supplied resources. In particular, Faith Runner is a close example of recovering first-person movement from a UE3 game and attaching it to another commercial game's world. The difficulty is the required interface and its integration, rather than the mere fact that KCD2 and Thief use different engines.

Running both original games with a shared-memory bridge is also architecturally conceivable. It currently has a weaker cost/benefit case for this pair: native access on both ends, donor collision injection, control ownership and lifecycle handoff are still unresolved. A whole-game rebuild has a much larger scope than the requested mechanics.

## What was actually examined

- Re-read the four supplied resources: REA, universal-modder, ai-game-modding-guides and KCD2ModLoader. Refreshed public commit metadata and selected source for the available repositories. The guides repository's public API returned HTTP 404; its retained local snapshot at `0c14be4...` remains available. Do not interpret that response as proof of deletion.
- Inspected current public implementations of SkyCraft, ER Mario, GTA San AnSkateas, the 2010 Rust Rewrite Mashup, er-apex and Faith Runner, including its separate Skyrim and Minecraft adapters. Read implementation files for movement ownership, geometry transfer, FFI and native hooks, rather than relying only on demonstrations.
- Located ThiefVR and inspected its camera/input hooks and executable guards. Inspected UEViewer's Thief-specific package and static-mesh handling.
- Performed narrow, read-only checks of the installed Thief executable and one package header. No game was launched, no downloaded implementation was executed, and no native hook or movement write was installed.
- Verified that the bundled Node runtime is 24.19.0. No REA/Ghidra installation or decompilation was performed in this assessment.

Public-source snapshots, tree inventories, commit metadata and local observations are in ignored `.local/research/20261017/`. They are research inputs, not shipping dependencies. Third-party gameplay results below remain creator reports supported by inspected code; none was reproduced locally during this assessment.

## Relevant precedents

| Project | Actual mechanism | What transfers to this project |
|---|---|---|
| [Faith Runner](https://github.com/tnrjns/faith-runner/tree/2e8b92d8520ee56440d235e9c7100771e4fdbf51) and [Skyrim adapter](https://github.com/tnrjns/faith-runner-skyrim/tree/f00ad21345091cae5b394913bd24b94f8ba5a5f9) | Selected Mirror's Edge script/native behavior is rebuilt in a separate movement crate. The Skyrim plugin supplies collision and applies its output through a C interface. | Closest pattern: recover the mechanics we need, then build a host adapter. It does not require running the original donor alongside the host. |
| [SkyCraft](https://github.com/chasmlol/SkyCraft/tree/bfcaf178524b92c2cdeb88e4ce0f13ef9ded6f32) | Skyrim and Minecraft run together. Plugins exchange geometry, input, player state and events. | A real dual-runtime precedent. Its access through SKSE/CommonLib and Fabric is a major prerequisite, not an incidental detail. |
| [ER Mario](https://github.com/deltarooo/er-mario/tree/11744b855bf546ca0b66dfd09068858feaa18d0e) | A native DLL embeds libsm64; Elden Ring geometry feeds the donor simulation and the native character follows it. | Shows how a host actor can preserve interactions while another controller owns locomotion. libsm64 already provides a portable donor system; we have not found an equivalent Thief library. |
| [GTA San AnSkateas](https://github.com/ryglizzy/GTA-San-AnSkateas/tree/bd0fe268f0f4fae3c342036a5558eec80251c233) | A C++ ASI plugin calls a reconstructed Skate runtime through a Rust C API. | Another concrete example of a reusable mechanic module, independent of the original donor process. |
| [er-apex](https://github.com/umiiii/er-apex/tree/ca16d4897381e7b05f2efd9db3cd84af2e237021) | A separate movement controller runs against exported host collision; native player movement is arbitrated and its pose is synchronized. | A closer modern first-person example. Its code documents geometry-window omissions and fall-through recovery, illustrating costs that a successful clip can conceal. |
| [2010 Rust Rewrite Mashup](https://github.com/chasmlol/2010-rust-rewrite-mashup/tree/f608f85e407ff1b7689d54a9aafdd16e95711ac4) | Combines rebuilt systems in a Rust host runtime. | Demonstrates the approach when reusable rebuilt components already exist. Starting a comparable KCD2 runtime would add a much larger prerequisite. |

Faith Runner also exposes the limits of fidelity claims: some behavior is substituted, inferred or host-adapted. Its Skyrim implementation documents stale moving-object collision and city-specific collision repairs. Extracted numbers and passing fixtures are useful evidence, but do not prove exact original behavior or unrestricted traversal.

## Concrete findings for Thief and KCD2

**Thief native access has a relevant public precedent.** [ThiefVR's engine hooks](https://github.com/farmerarmor/ThiefVR/blob/bf593bf132f0e352811217cd871f3ad1aa8752fa/src/EngineCamera.cpp) intercept camera construction and a semantic input-axis read. This supports investigating similar access for observation and donor measurements. It does not establish a movement, collision-scene or perception bridge.

**The public hooks do not match this installed executable.** ThiefVR pins SHA-256 `BE1BACBB...A10EA11D`; the local Win64 Epic executable is `ee707ecd140dc88317b631de4eb0a6173754bdde8b5a15590d4c06e0471edbf9`. Five selected hook-site byte checks at its recorded RVAs all failed. Relocation and semantic validation are required; changing a hash guard would not establish compatibility. This is a build-porting task, not evidence that native access is impossible.

**There are useful donor investigation anchors.** The local binary contains `UThief3CSettings_Dash`, `Dash_bSwooping` and `Dash_bExhausted`. These are names to trace into class registration, defaults and callers, not recovered movement rules. The local `ThiefGame.u` header reports package version 721 / licensee 148. A bounded direct name-table read did not succeed; compression/layout still need resolving. Mirror's Edge's package reader targets 536/43 and must not be assumed compatible. [UEViewer has explicit Thief handling](https://github.com/gildor2/UEViewer/blob/a0bfb468d42be831b126632fd8a0ae6b3614f981/Unreal/UnrealPackage/UnPackage3.cpp), which is a better format lead than UE4/5 tools. No asset extraction pass was validated here.

**KCD2 remains the host-side uncertainty.** Existing local evidence establishes official Lua loading and useful player-state observations. Earlier targeted native inspection found locating patterns for script update/init and ray queries; it also found that the inspected loader's player-constructor signature did not match this retail build. None of that proves a safe per-frame controller takeover, body sweep interface, camera layer or perception hook. These are specific research targets, rather than reasons to abandon the route.

## What the supplied tools can contribute

| Resource | Useful role | Boundary |
|---|---|---|
| [REA, inspected at 3ddb2e4](https://github.com/morluto/rea/tree/3ddb2e4cf7c5678e2fd2c4ef2471e5f795e9355d) | Ghidra-assisted native string/caller/function analysis, evidence recording and bounded exploration of both binaries. | Windows support is an experimental read-only analysis boundary. It does not automatically identify game semantics or provide live movement control. |
| [universal-modder, b8675c1](https://github.com/rehan-remade/universal-modder/tree/b8675c10d7091d7e4e00a2e40a15c601040b69a8) | Concrete mashup patterns, bridge contracts, reusable examples and field notes. | Its inspected Unreal recipe covers UE4/5, and its CryEngine detector looks for CrySystem.dll. Use game-specific evidence for this UE3 donor and WHGame.dll host. |
| ai-game-modding-guides, retained local 0c14be4 | Route comparisons and practical case studies that point to primary implementations. | Guidance and creator reports are leads; inspect the linked code and test the chosen surface. |
| [KCD2ModLoader, c7b12a6](https://github.com/xiaoxiao921/KCD2ModLoader/tree/c7b12a61330ea4d743b8eb1bc21d9e0a1a1f8f8e) | KCD2-specific native integration examples and locating information. | Its entire native surface has not been validated against the installed build. Reuse narrowly after validation. |

The old Node prerequisite is avoidable: the system Node is 22.18.0, but the already bundled 24.19.0 meets the inspected REA requirement. Its executable is `C:\Users\leonh\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe`. REA still requires an appropriately configured analysis provider, such as its documented Ghidra/JDK combination. A working REA provider has not been demonstrated here. Missing setup is a finite tooling task, not a verdict about the game pair.

## Candidate architectures

| Route | Assessment | First decisive proof |
|---|---|---|
| Recover Thief rules; use KCD2's controller | Recommended first prototype. Keeps host collision and interactions, while improving fidelity beyond an arbitrary speed multiplier. Exact acceleration/steering authority remains unknown. | A native adapter submits one bounded motion request at the correct update phase, with normal wall contact and restoration. |
| Recover Thief rules; use a separate collision-aware controller | Credible alternative, analogous to Faith Runner and er-apex. More authority and more integration work. | Prove a reliable full-body collision-query/geometry surface, exclusive control and host-actor synchronization. |
| Run original Thief alongside KCD2 | Possible in principle, currently less attractive. Requires new native access on both sides and a donor mirror scene or collision-query adapter. | Make Thief's real pawn collide with one KCD2-owned test wall, then mirror its resulting pose while retaining KCD2 interactions. |
| Import Kuttenberg into Thief | Potential geometry/traversal demonstration. Preserving KCD2 NPCs, quests, crime and saves would become a separate major project. | One correctly scaled scene with collision and a native Thief traversal action. |
| Rebuild an entire engine/runtime | Broad freedom, weakest near-term efficiency for the present goal. | Establish the actual missing feature that cannot reasonably be delivered by a targeted adapter. |

A live bridge would not eliminate reverse engineering. It would move some of it into donor input, physics queries, scene injection, output capture, clock synchronization, menu handoff and failure recovery. Minecraft's Fabric surface makes that side of SkyCraft unusually accessible. No equivalent broad Thief interface has been verified.

A useful middle ground is to run instrumented Thief as a **reference during development**, record its response to controlled inputs, and compare a reconstructed movement module against those observations. Players would then need only KCD2 at runtime, with any required donor data converted locally from their own install.

## Collision ownership: correction to the earlier framing

A native `SetPosition` call is not, by itself, evidence of noclip or valid collision. In SkyCraft and Faith Runner, a different controller resolves collision and the host actor follows the result. That can be a legitimate architecture.

For this project, every movement step must have one explicit owner and a proved collision path. Two controllers fighting over the player, or transform writes without a collision solution, remain unacceptable. If a separate controller is selected, deliberately revise the current host-controller-only design and verify the replacement before enabling it. This report does not make that implementation change.

The important costs include dynamic doors/NPCs, streamed geometry, thin walls, moving supports, steps, trigger volumes, body/head hit locations and release back to native interactions. Ray samples alone do not establish complete body collision. Camera smoothness must not conceal a physically incorrect player position.

## Movement, animation and stealth have different boundaries

- **Movement:** a bounded set of state transitions, curves and queries is a plausible extraction/reconstruction target. Start with swoop; expand after proving the adapter.
- **Animation/camera:** donor data may be reusable through conversion or a separate animation module. UE3 assets are not directly KCD2 Mannequin resources. Skeleton mapping, camera anchoring and root-motion ownership need explicit handling. Start without imported presentation to isolate locomotion.
- **Stealth:** transferring a light-gem value or player position will not reproduce guard behavior. Thief's perception depends on world sensing, NPC state and associated behaviors. We must identify which KCD2 visibility/noise/awareness decisions can be modified, or intentionally replace a defined subset. Movement success does not validate this much larger integration.
- **World traversal:** a reconstructed controller does not create missing supports, authored contextual links or walkable roof collision. Expect a curated first route and named unsupported geometry.

## Recommended research milestone

Put a bounded native investigation on the critical path. The installed speed comparison remains a useful control experiment; it should not be the prerequisite for investigating the wider architecture.

1. **Recover one donor mechanic.** Trace the Dash settings/state anchors in the exact installed Thief build. Identify activation, movement profile, steering, exhaustion/cooldown, camera contribution and collision dependencies. Record what is directly recovered, measured, inferred and substituted. Use a configured REA/Ghidra provider for selected functions rather than a whole-game export.
2. **Prove the KCD2 adapter.** Establish game-thread update timing, input/state/lifecycle access and one collision-aware movement interface. Prefer an existing controller request if it supplies sufficient authority. Otherwise validate the native sweep/overlap or geometry-export surface before attempting a separate controller.
3. **Build a small independent movement core.** Use plain inputs/state/outputs and an explicit host-query contract. Compare identical short scenarios with the donor: free movement, obstacle contact and interruption. Native pointers stay in the adapter.
4. **Validate one retail action.** One activation, one owner, no overlap with a wall, ordinary control recovered, and cancellation on state change, pause and load. Keep an immediate disable path and the existing backup/install discipline. Check NPC targeting and interactions with the synchronized player.
5. **Make a route decision.** Continue with the host controller if it can reproduce the behavior. Evaluate a separate controller if it cannot. Consider the original two-runtime route only if retaining original donor execution provides a demonstrated advantage over reconstruction.

Stop each probe with an implementation result or an identified missing dependency. Do not treat additional getter lists, a successful handshake, recognized function totals or a rendered clip as completion of the movement proof. No reliable calendar estimate is justified before the donor mechanic and host control surface are established.

## Research outcome

The source evidence is sufficient to justify a targeted Thief-mechanics/native-KCD2 prototype. It is insufficient to promise a faithful full movement-and-stealth transplant, certify a two-game bridge, or discard the existing mod. The next useful result is a working narrow adapter and a recovered donor action, with a decision about collision ownership based on those results.
