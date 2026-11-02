# Ledge climbing: first playable plan


## Current runtime - v0.0.21 wall candidate verified

v21 follows the captured lower-face/bevel/upper-face profile without merging offset planes. It rebases the top search, keeps final-face cap evidence separate, and requires four junction samples plus the bare final-face checks before any hint. One join/max46 rays under hard48; all previous gates remain. The completed October22 retail scan now supplies the actual top samples: `candidate_hint`,44 rays, height1.015720m, five landing hits and five headroom misses. The observation-only bridge made zero movement writes and recorded shutdown; the prior independent audit reports both hook sites restored. All789 pre-instruction clock publication refusals and the raw warning are preserved. No admission observer or added action was used. Offline suites and installed/package/source hashes pass again November2.

The completed v20 test produced ordinary Jump only and no native handle. Static selected-member data has no nearby guide at that scan pose; the v21 pose differs. Geometry diagnostics, registered native target and suitable motion are separate pieces. The next playable milestone remains one owned ordinary-range native climb with proven guide lifetime/full-body route/handoff, followed by higher motion. No repeated wall scan or Jump baseline is needed. Resolve native member/source selection before preparing the no-op mounting experiment. [Test](TestPlan.md), [native route](KCD2HigherClimbRoute.md).

2026-10-21. The user selected **approach a reachable ledge and press Jump to climb onto it**. Hanging, shimmying and drop-to-grab are later work. Target remains pinned retail KCD2 1.5.6, Steam build23914554. This document distinguishes implemented diagnostics from the proposed gameplay path.

## Decision

Use Thief's approach/edge/landing selection behavior with **KCD2's native ledge action, animation and controller**. Start with one grounded, solid, wide ledge inside the host's existing climb-onto range. Keep ordinary Jump available when no added candidate qualifies. Do not extend the accepted horizontal swoop into a vertical position writer.

Both games have an authored guidance system. A wall/top raycast is useful for finding geometry but does not register a host ledge, prove capsule clearance or authorize a climb. [Thief evidence](ThiefLedgeFindings.md); [retail host interface](KCD2LedgeInterface.md).


## Previous tested implementation - v0.0.20

Native request tracing now works on the saved ordinary-climb baseline:2 matched query/selection/request chains, all2047 ownership bits, framework acceptance and zero admission errors. The user separately confirmed visible climbing. Both observers are stopped/all six original sites restored; the71-file tested v19 snapshot is preserved. [Proof](KCD2LedgeAdmission.md), [results](LedgeRuntimeResults.md).

v20 fixes continuous slanted-face grouping/top anchoring and includes face lean in cap intersection. A bare slanted face requires near-lip and midpoint face matches to reject disconnected shelves. World-coordinate double/float32 regressions retain conservative gap/roof/ridge/narrow-footprint refusals. No flat-normal, height, landing/headroom, lifecycle or expiry gate is relaxed; the added path uses at most44 rays under the hard48 limit. Native ledge/movement code is unchanged. All offline suites pass; builds are staged separately. v20 is installed/hash verified with a fresh independently verified237-file save backup; its completed scan refuses the unsupported compound wall profile at27 rays, before top/landing/headroom. All2,525 native searches returned no handle, and the selected shipped member has no guide within1.6m of this scan.

Native type2 is now mapped to ordinary VaultOver, explaining why the small obstacle's native action can succeed while the onto footprint refuses. High types5/6/7 exist, but absent player XML nodes leave no authored positive higher-height interval; the separate native animation weight clamps to0..1. Higher climbs still need a configured compatible action/animation, registered guide, body clearance and interruption ownership. [Native route](KCD2HigherClimbRoute.md).

The next necessary manual check is geometry at a broad flat-topped ledge, preferably a slightly slanted face within ordinary onto height. It does not require another native-baseline repetition at the narrow obstacle. The user has now chosen/saved a broad raised-pavement wall that refuses vanilla climbing, and closed-game installation is complete. The observation is complete, with ordinary Jump only. Both observers stopped/all six sites restored and zero movement writes; one pre-test clock/publication refusal is retained. Continue offline bounded compound-profile and authored-guide work; no unchanged-v20 repeat is requested. [Test plan](TestPlan.md).

## Previous implementation — v0.0.18, manual check complete

Native tracing was corrected first: revision3 identifies each failed predicate, passed checks and target-relative action class, separates copied-request mismatch and true memory-read faults, and retains exact per-guard counters. Original constructor/RTTI review corroborates current offsets and gives no basis to weaken them. v17's actual failing predicate was not retained, so a fresh guarded baseline is required before claiming candidate-associated native acceptance. [Evidence](KCD2LedgeAdmission.md).

The detector now follows a connected sloped cap to an observed flat top. Retained rays showed that v17's reported1.48/2.06 heights were rejected cap hits; they were not measured flat ledges. Up to five inward top probes replace the single6cm sample, then five landing and five headroom rays validate the chosen point footprint. With no verified face, pitched/downward low-surface rays remain diagnostic only. Maximum42/hard48 rays;32-scan cap,0.15s expiry and native0.65–1.5 hint gate are unchanged. Both number models and adversarial world-coordinate fixtures pass.

The known native handle now maps to an authored edge within the scan footprint and between sampled height rows; its collider shape remains unknown. The shipped retail XML ledge loader/index rebuild and HighVault action data are concrete routes toward higher climbs, with additive export/load support and suitable taller motion still unproved. [Higher-climb route](KCD2HigherClimbRoute.md).

v18 package/native observers are built separately, with25 Python tests, native fixtures and package checks passing. The v17 tested62-file baseline and accepted native movement code/tuning remain preserved. After the initial ready-build checkpoint, normal closed-game deployment installed/hash verified v18 with a fresh verified237-file save backup. No v18 observer attachment or runtime validation yet. Receipts: `.local/evidence/20261022-v018-ledge-build.json` and `20261022-v018-install.json`. The next action requires a manual ordinary-climb/geometry baseline as described in [TestPlan](TestPlan.md); added climbing is still disabled.

Retail `Scripts/Entities/actor/Parameters/Player_Params.xml` SHA-256 `9cc1225288be58675704f0b17067ef9798052bdd613f7a1178b502ef6a63e4cc` specifies wide, inward `VaultOnto`, allowed relative height0.65–1.5, movement duration1.0s and correction0.05s. Its nearby search has maxDistance1.6, radius0.5 and ordinary facing deviation40degrees. These are authored host settings, not demonstrated final motion limits or Thief values. The first implementation should retain them until a native baseline establishes actual selection and animation behavior.

## Implementation sequence

1. **One existing native climb baseline.** Observe KCD2's selector during ordinary Jump at a known climbable wall. Correlate candidate/no-candidate, action type, player identity and visible completion. Include one refusal facing away or with no ledge. This confirms the selector ABI and owner on the actual retail thread; selection alone is not action acceptance. The standalone observer forwards the original call and reads its output only.
2. **One additional registered ledge.** Prove the retail registration and removal route, or use a supported authored level/layer ledge if dynamic insertion is unavailable. Reuse a valid host ledge descriptor and host action. An editor-only rebuild routine is not a retail registration API. Verify that the candidate survives validation and that its lifetime is bounded to the current gameplay generation.
3. **Thief-style candidate selection.** Use forward intent and approach angle; select one stable edge with adequate width, a supported landing footprint, headroom and a full-body swept route. Revalidate just before committing. Initially reject moving props, actors, doors, thin rails, sloped/irregular edges, ceilings, gaps and ambiguous collision. Rays remain hints until the native volume/host eligibility checks succeed. Do not convert donor units by guesswork.
4. **Exclusive handoff and recovery.** Consume a fresh Jump press once, cancel any swoop intent, and submit one host climb request. After host acceptance, its controller owns motion/camera/animation. The traversal-specific state must allow expected airborne/climb animation signals; the swoop's crouched/grounded gates cannot be reused during a climb. On refusal, preserve ordinary input; on completion/cancel/load/death/player replacement, release only the owned candidate/intent. Never resume a pre-load traversal.
5. **One integrated acceptance pass.** Prove a valid climb, blocked headroom, insufficient landing width/depth, too-high ledge, facing away, held/repeated Jump, interruption and fresh-press recovery. Test the integrated sequence at30/60/120FPS where controllable. Record actual clearance/collision, animation completion, camera continuity, fall behavior, no stale candidate after reload and no resumed swoop. Then broaden to more geometry. Hanging is outside this milestone.

The important unresolved dependency is retail ledge registration/selection ownership, rather than a speed curve. If the host index cannot accept safe temporary candidates, prefer a small authored set of supported climbs before considering a separate swept traversal controller. A custom controller would also require verified root-motion arbitration, body sweep, camera and interruption behavior; it is a larger fallback.

## Previous build — v0.0.17, installed; comparison complete

Work first stopped at the ready build as requested; the user then authorized installation and completed the same-spot test. v0.0.17 is installed/hash verified with a fresh verified save backup. Ordinary Jump climbed the low obstacle and refused higher walls. The low scan still missed; higher scans detected faces but failed lip consistency. Three guarded request captures failed, leaving candidate-associated framework acceptance unproven. Both observers stopped and all six original entries restored, with no bridge movement writes. This checkpoint is finished; no further attempts are needed. [Results](LedgeRuntimeResults.md), [current status](../STATUS.md).

The geometry probe keeps the explicit dry-only command, next published canonical frame, known lifecycle/owner gates,0.15s direct async expiry and32-scan cap. Revision2 uses the byte-verified `ent_all=287`,1.6m reach, three lateral lanes and nine relative heights from0.2 to3.1. It filters face normals before selection, groups a connected face band and probes that band's lip before the five landing and five headroom samples. A separate ceiling is not the lower obstacle's lip. Successful scans use38 rays; the hard limit is48. Sampling coordinates are relative to entity origin, not a measured capsule floor.

Low/tall surfaces can now return measured diagnostic heights and explicit `height_outside_native_onto`; `candidate_hint` still requires0.65–1.5 and the chosen point footprint/headroom tests. Narrow walls can return `landing_sample_missing`. These results never certify a body sweep or modify native climbing limits. Logs include scan/frame/context, entity and camera pose, normalized scan basis, each ray origin/delta/mask/status/hit and final bracket/refusal. Only bounded scalar summaries survive the callback. [Ray evidence and limitations](LedgeDetectionEvidence.md).

The separate native observer now follows an existing standing attempt through the manager query, selection and framework boolean result. Four exact caller/owner-guarded hooks forward originals unchanged and aggregate only callback-scoped matching data. It distinguishes absent search results, selection refusal, missing request and framework acceptance/refusal. The framework result is request acceptance, not animation start/completion. No ledge insertion, custom action submission, Jump simulation or movement writer is added. [Admission ABI, evidence and limits](KCD2LedgeAdmission.md).

Build commands, from the repository:

```powershell
python tools/project.py build
./tools/build_native_swoop.ps1 -OutputName build-ledge-v017
./tools/build_ledge_probe.ps1
python -m unittest discover -s tests -p 'test_*.py'
```

Artifacts are the local `build/kuttenberg_thief` package, `.local/native/swoop/build-ledge-v017` and `.local/native/ledge-probe/build-v017`. The prior accepted native movement sources and binaries are preserved. Offline double/float32 geometry, seven native bridge/core fixtures, owned four-hook admission fixtures, emitted JSONL parser conformance and22 Python tests pass. Build pins are in `.local/evidence/20261021-v017-ledge-build.json`; the subsequent runtime results/limitations are separate in `.local/evidence/20261021-v017-ledge-runtime-session.json`.

## Historical geometry probe — v0.0.16

`LedgeProbe.lua` is loaded with the mod but does no geometry work until `kt_ledge_probe`. The command stages one request for the next successfully published native frame. It requires a fresh canonical context, known lifecycle/normal-body/animation/camera/contact gates and dry mode; standing and crouched starts are both allowed. It performs13 rays at most on the ordinary success path, with a hard20-ray budget and32-scans-per-process cap. Work runs on the script-update thread after publication.

The probe samples a forward face, five top/landing points and five vertical headroom rays. It reports `candidate_hint` or a typed refusal in `[KT] ledge` log lines. Diagnostic radius0.32, depth0.38 and height1.8 are chosen sampling dimensions; they are **not recovered capsule dimensions**. Every result states `movement=false body_clearance=unknown host_ledge=unknown`. It never spawns ledges, writes movement, simulates Jump, modifies host parameters, or authorizes the movement bridge. `kt_ledge_status` expires results after0.15s even when callbacks have stopped. No raw hit handles, entity pointers or world targets are retained for later actuation.

Analytical geometry fixtures use actual ray/box intersections for reachable platforms, insufficient width/depth, low obstacles and overhead obstructions. They also exercise unknown/error/malformed rays, lifecycle changes, delayed requests, standing/crouch, every safety gate, scan limits, stale status and callback isolation under both double and retail float32 Lua. These establish probe behavior only; no playable climb or actual body-clearance acceptance is claimed.

## Completed same-spot check and next implementation boundary

The saved small-obstacle/higher-wall check used the v017 admission observer followed by the observation-only state bridge, with marked phases and ordinary Jump. Timing proximity alone does not establish identical pose/frame. Visible native climb completion was recorded separately; all six sites restored after successful stops. No code or tuning changed during the test. Next explain the low-obstacle ray misses, higher-wall lip association and the exact guarded request/owner refusal before preparing another build. Added candidate registration, swept clearance and host handoff remain separate prerequisites. Do not ask for repeated attempts with this unchanged diagnostic build.

Builds, source pins, save backups, installed package and eventual runtime evidence are recorded in [STATUS](../STATUS.md). No runtime outcome is inferred from compilation or the offline fixtures.
