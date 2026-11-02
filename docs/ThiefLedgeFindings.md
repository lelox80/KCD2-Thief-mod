# Thief ledge climbing: bounded donor findings

Static inspection, 2026-10-21. Scope for the first host prototype is **approach a reachable ledge, press Jump, climb onto it**. Hanging, shimmying and drop-to-grab are later work. No donor executable was launched or changed, and no donor movement code/assets are included in the mod.

## Identity and evidence boundary

Inspected donor: Thief 2014 Win64 1.7.0.5 at `S:\Spiele\ThiefvWwV2\Binaries\Win64\Shipping-ThiefGame.exe`, SHA-256 `ee707ecd140dc88317b631de4eb0a6173754bdde8b5a15590d4c06e0471edbf9`; image base `0x140000000`. Its `ThiefGame\CookedPCNG\ThiefGame.u` is UE3 721/licensee 148, SHA-256 `c061289fdc634484badc780379992b6daa1827998b114d4b861338367d87fa35`.

The bounded package reader in `tools/thief_inspect.py` was extended only in ignored local scripts for this investigation. It validates the pinned hash, name/import/export tables, selected property boundaries and LZO blocks. Nested tagged properties were decoded with their exact array indices. Only chunks 0, 1, 36 and 37 were decompressed for selected defaults, settings and enums. Private receipts/scripts are `.local/native/thief/ledge/`, `.local/native/thief/ledge_settings.py` and `ledge_enums.py`.

Native REA/Ghidra 12.1.4 inspection used three small, address-preserving analysis containers (2,585, 849 and 8,279 source bytes). These are **derived inputs**, not the complete original executable. Distant callees, globals, constants, RTTI and animation data remain outside them; direct original-PE disassembly and serialized names corroborate the associations. See [REA setup limitations](REASetup.md). No whole-game decompilation was attempted.

## The relevant donor feature is CrossOver

`Default__ThiefHeroSettings.mCrossOverSettings` binds `Prog_GP_CrossOver.Settings_CrossOver`; `mVNSettings` binds `PROG_GP_VN.Settings_VN`. Authored `Settings_ThiefHero` has no replacement for either reference in its property stream. This establishes serialized bindings; runtime selection is not observed.

The package's ordered `ThiefTypes.ECrossOver` enum is:

| Value | Serialized name |
|---:|---|
| 0 | `CROSSOVER_None` |
| 1 | `CROSSOVER_StepUp` |
| 2 | `CROSSOVER_Vault` |
| 3 | `CROSSOVER_Slide` |
| 4 | `CROSSOVER_Claw` |
| 5 | `CROSSOVER_ClawVault` |
| 6 | `CROSSOVER_CornerStepUp` |

`ECrossOverHeight` distinguishes Low/High; `ECrossOverState` orders None, In, Action, Out. These are real serialized enum declarations, not roles assigned from isolated strings. The first host feature should reproduce StepUp's intent. The separate VN system contains ledge-open/closed, hop, air/ground/roof entry, transfers and claw selection data; it is substantially broader than this first prototype.

## Selection and geometry settings

Values below are **original donor units**, with no established conversion to KCD2 metres. They are serialized configuration, not measured climb limits or timing.

| CrossOver property | Class default | Authored override |
|---|---:|---:|
| `mHighCrossOverTreshold` | 140 | inherited |
| `mMaxCrossOverHeight` | 400 | inherited |
| `mMaxHighStepUpHeight` | 300 | **305** |
| `mMaxExtrudedLedgeDepth` | 50 | inherited |
| `mWalkStepUpDepth` | 25 | **32** |
| `mRushStepUpDepth` | 50 | **65** |
| `mMinStandOnDepth` | 50 | inherited |
| `mLandDistanceMin` / `Max` | 50 / 100 | **80 / 130** |
| `mMinWindowHeight` | 100 | **60** |
| `mMinWindowHeightFromGround` | 80 | inherited |
| `mMinSpeedForAgileCrossOver` | 300 | inherited |
| `mMinSpeedDuringCrossOver` | 450 | inherited |
| `mMinPlayRateModifier` / `Max` | 0.8 / 1.2 | inherited |
| `mBodyRotationTime` | 0.1 | inherited |
| `mHandDist` / `mMaxVerticalAngle` | 40 / 20 | inherited |

The same class has separate maximum vault lengths for low/high/claw/sprint (40/25/40/100) and slide length (400). These must not be conflated with a ledge's minimum standable top depth.

`mSelectionSettings` accepts guidance **Node and Edge**, rejects Surface, and enables `mbSelectFacingGuidanceOnly`. Its default maximum detection/facing angles are 15/60, with weights angle/height/distance/facing 8/2/1/1. The authored object enables `mbBoxShapeZone`, sets length 100, changes height weight to -2, and adds claw-anchor eligibility with claw bonus 10. The native score equation/sign convention was not fully traced, so the negative height weight is not interpreted as a complete ranking formula.

This is evidence for selecting an oriented, typed guidance target before attempting the action. It is not evidence that every mesh edge can be climbed. The package exposes cooked/dynamic guidance structures and guidance material flags; this pass did not inspect level geometry to establish actual coverage.

Adjacent collision/guidance settings support explicit clearance treatment: `ThiefGuidanceSettings.mCollisionBuffer=5`. Hinting-build defaults contain ceiling/front/ground collision buffers 10/5/10 and validation interval 5; its authored object changes the front buffer to 10. **Hinting settings do not establish the execution controller's capsule sweep shape or its exact clearance policy.**

Package receipts: `settings.json`, `enums-fields.json` and `package-inventory.json` under the ignored ledge evidence directory. Selected exports include CrossOver default #3249 at logical offset 4,217,425; authored #37007 at 37,616,049; VN default #3511 at 4,239,859; authored VN #37021 at 37,620,228; hero default #8390 at 4,627,886; authored hero #38458 at 38,203,690.

## Native preparation, detection and cleanup

All addresses here are **RVAs** for the pinned executable. Analytical role names below are inferences backed by their data/event relationships.

| RVA | Observed relationship |
|---|---|
| `0x1582970`–`0x1582a70` | Initializes named animation variables including `CrossOver_bActive`, `eState`, `eCurrent`, `bRush`, `bCrouch`, `iHeight`, play-rate and mirror |
| `0x194fd70` | State vtable containing preparation `0x8814e0`, cleanup `0x87e650`, event handling `0x897a80` and transition selection `0x885920` |
| `0xe4aa80` | Lazily refreshes a result via `0xe8bc50`, caches it under flag `0x400`, and returns component + `0x13c` |
| `0xe8bc50` | Queries guidance candidates, iterates 0x80-byte candidate records, performs further spatial tests and chooses a CrossOver subtype |
| `0x8814e0` | Requires a nonzero target in the refreshed result, copies the result, prepares movement/animation state and marks `CrossOver_bActive=1` |
| `0x87e650` | Counterpart restores state, clears `CrossOver_bActive=0`, clears animation/controller-related state and notifies attached components |
| `0x897a80` | Handles a typed event and copies it into owned state; **not established as a per-frame movement tick** |
| `0x885920` | Chooses transitions based on active child/state and completion-related flag; full cancellation semantics remain unresolved |

The detector normalizes a horizontal input/orientation vector, calls a guidance query using the selection settings, and examines candidates. It compares candidate height relative to a player-derived position, then calls additional test helpers (`0xe75df0`, `0xe85d20`, `0xe86140`, `0xe7e680`, `0xe86540`). It writes a subtype, target positions, height and crouch-related flags into the selected result. Failed classification leaves subtype None. Matching its result byte values to the serialized enum identifies StepUp/Vault/Slide/Claw/ClawVault/CornerStepUp branches.

These are meaningful geometry-dependent selection stages. The helper flags, trace mask `0x2086`, physics return convention, precise body shape and all collision tests are **not yet decoded**, so this pass does not claim full path-clearance proof or a complete climb algorithm. The preparation and cleanup routines also call the same actor virtual functions with paired setting offsets; this supports a lifecycle with reversible controller/collision-related changes, but exact virtual method semantics remain unproven.

REA Evidence IDs and local exports:

- Detector: `ev_add8e4d1ffbcc758b810ae0ae351729828520534b6933cf762aaeef4f5070e6a`, `rea-crossover-detection.json`; region SHA-256 `c0486d778068c7fbc6762af3c2772f22153080a8c56f6727762aae3ff53f6856`, container `9cebada4f3f149de823e37d9f0e9a00756c0f9cb157bbfe9d27e2ca1a771252b`.
- Preparation: `ev_ba342dfa685b0533de23e205e3e05cc00c61957cc5e22f3f657240581563aff2`, `rea-crossover-enter.json`; region `c2b2fad7c58390cf8784ca4ce0bc7c13de0e9a4bb33554f9ff9b2aea0fe957ad`, container `6ea7c9b7bd8ac94048dd68545d46ccf41a4bae59630f180103eaa1f13758b972`.
- Cleanup: `ev_d6496879669a9cde06d2e698575859072c3dff6d05060d94f68b18c989684c50`, `rea-crossover-exit.json`; region `d0427d91e58494045dfe6914b5964af42413dfa69483d8a41408df7d457cf369`, container `85e86e49227aba8d6ed80fd02e81555cf746707bce12d5d32956e3ea9c43e675`.

An earlier preparation import began inside a chained unwind fragment (`0x8815f0`) and produced bad-instruction pseudocode: `ev_97af30ab309d30335a4378c0e917754a1e64d3e8424be24af232c4dec5036af7`, `rea-first-pass-incomplete.json`. It is retained as rejected/incomplete evidence and was superseded by the root range above. Do not use it as an independent corroboration.

## Input, animation and remaining gaps

Installed `Config/DefaultInput.ini` describes Body and Navigation as `A_BUTTON`, with separate Rappel and Drop controls. `DefaultUI.ini` has distinct contextual actions for CrossOver, climb up/down/out and jump across/down. The input file's `-Bindings` entries removing generic Jump/SpaceBar mappings are removal directives, **not a verified live PC binding**. The exact PC action-map dispatch, press/hold threshold and action-priority ordering remain unresolved.

`CrossOverAnimPlay`/`CrossOverAnimEnd` event names, In/Action/Out enum states, low/high variants, play-rate bounds and mirror state support an animation-coordinated action. No fixed climb duration, root-motion curve, hand IK behavior, camera curve or frame-by-frame clearance was recovered here. No donor playtest or timing measurement was performed.

## Recommended host approach

Use KCD2's own ledge/vault action if its retail invocation and admission contract can be verified. Preserve host animation, collision, gravity and cleanup ownership. The donor evidence argues for a small stateful **select target → classify reachable climb → execute → restore** pipeline, rather than directly adding vertical position to a jump.

For the first host slice, choose one stable static ledge in front of the grounded player; require height, facing, top support depth, destination body clearance and travel clearance; on a fresh Jump press, submit a single admitted host action. Reject an invalid candidate without consuming ordinary Jump. Keep target identity plus player/map/session identity until completion; invalidate stale or moving targets. Use the host's interruption/completion route and never enable an action whose ownership/recovery is unknown. These are proposed host requirements, not claims that every one matches the donor's exact rules.

Separate later phases—hanging, shimmy, drop-to-grab, claw and vault-through—from this climb-onto loop. Tune in verified KCD2 units and host-supported animation ranges; do not copy donor 305/400 values as metres or claim Thief-equivalent feel. Acceptance requires real terrain playtests for false positives, low ceilings, thin/narrow tops, wall contact, interruption, save/load and repeated use. Static settings and unit tests cannot establish that outcome.
