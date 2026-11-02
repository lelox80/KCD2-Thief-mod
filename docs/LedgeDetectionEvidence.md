# Ledge detection evidence and historical requirements

Current result, rechecked2026-11-02: the October22 v21 retail scan completed the previously missing top search and returned `candidate_hint`,44 rays, height1.015720m. It includes five landing support hits, five upward headroom misses, four compound-profile and two final-face confirmations. Retained source/log hashes and both number-model replays match. This establishes sampled diagnostic geometry; whole-body clearance, guide registration and native admission remain unproved. No unchanged-build repeat is needed. [Runtime results](LedgeRuntimeResults.md).


Historical v21 build correction,2026-10-22: the v20 wall scan hits lower face -> bevel -> recessed upper face, then refuses before any top query. Bounded compound-profile support now proves inferred joins within observed brackets/reach and rebases to the upper band, with four junction confirmations and separate final-face cap evidence. The exact27 captured rays reproduce v20's refusal; the pre-test replay reached only missing/unknown top ray28. No real top/footprint/headroom result is fabricated. Max46/hard48 rays and one-join limit; existing gates remain. Double/float32 connected/disconnected adversarial fixtures and independent review pass. Private implementation/replay `.local/evidence/v021-profile-review`; [runtime evidence](LedgeRuntimeResults.md).

v20 offline correction,2026-10-22: the accepted wall-normal range includes slightly slanted faces, but the old connected-band residual omitted normal.z times hit-height difference. A continuous receding plane could be misclassified as an unrelated transition, and its lowest-hit top anchor could miss the actual top. The detector now uses the full3D plane, anchors from the highest connected observed hit and projects a receding face only within its observed first-clear-row bracket and reach. Face/cap intersection accounts for lean and refuses degenerate intersections. A bare slanted face receives same-plane/normal confirmation rays above the last measured face hit and just below its observed flat top to reject a separated shelf; this remains sampled continuity evidence, not a body sweep.

World-coordinate double/retail-float32 regressions cover lean in both directions, legitimate caps and disconnected surfaces, retaining the existing narrow-obstacle and steep-ridge refusals. The added path uses at most44 rays, under hard48/32 scans; flat/footprint/headroom/height/lifecycle/expiry limits are unchanged. Native tracing is independently verified by the completed v19 baseline, whose type2 requests are ordinary VaultOver. The later v20 retail scan refused a compound profile before the top search; v21's completed diagnostic is recorded above. Private replay/fix evidence: `.local/evidence/v020-geometry-review`. [Current plan](LedgeClimbing.md), [native mapping](KCD2HigherClimbRoute.md).

v18 offline correction,2026-10-22: exact v17 replay under double/float32 confirms that higher scans hit a cap with normal.z about0.57. The old detector excluded that row as a vertical face, then sampled only6cm inward and rejected the same slope as a top. Schema3 retains transition hits, follows associated rising geometry and searches inward for an observed flat top before landing/headroom sampling. Low-obstacle authored guidance is now located within the scan footprint between the0.7/1.0 rows, but the actual collider miss cause remains unknown; bounded pitched/downward fallback is diagnostic only. No live result is claimed from analytic fixtures. [Current plan](LedgeClimbing.md), [authored guide evidence](KCD2HigherClimbRoute.md). Private reproducibility: `.local/evidence/v018-geometry-review/review.json` and `.local/native/ledge-v18/test-spot-authored-ledge.json`.

Subsequent v0.0.17 same-spot observation is complete in [runtime results](LedgeRuntimeResults.md): the low scan still returned27 valid misses; two higher scans detected faces but failed lip association. Their rejected diagnostic heights do not validate a supported top or clearance. The evidence and coverage analysis below remains the v16 basis for the v17 changes, not a claim that broader sampling resolved the actual test geometry.

Investigation date: 2026-10-21. This is an offline review of the retained v0.0.16 capture, shipped retail script parameters and a focused retail binding. No game process was inspected, attached to, installed into or modified during this review. Runtime results remain those in [LedgeRuntimeResults.md](LedgeRuntimeResults.md).

## What the failed scans establish

The collected log `.local/logs/20261021T161509131053Z-probe.log` contains exactly three completed v1 scans: `no_wall` at lines 172 and 209, then `no_landing_sample` at line 227. SHA-256 is `1110858d2aaa01fcfb1b445a340e5a12407eed9b56728d621a5a9bdc9af6680d`. No completed scan reported `ray_unknown`, `ray_shape` or `ray_segment`.

In v0.0.16, `no_wall` means all three forward rays returned valid empty hit tables; it is not a getter, context or hit-decoding failure. `no_landing_sample` means the face stage found an acceptable face and at least one downward point sample returned an empty hit table. The log does not identify the failing sample, individual rays, exact scan stamp/frame, face coordinates, camera direction or input pose. Adjacent one-second watch samples and the wider selector phase windows cannot supply the missing exact join. Therefore no single physical root cause is established for any failed scan, and candidate samples elsewhere in the small-obstacle phase do not prove that the native selector accepted the exact scan pose.

## Retail ray contract: original bytes corroborate the existing call

Target: retail KCD2 1.5.6/build 23914554, `WHGame.dll` SHA-256 `bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7`; image base `0x180000000`. The following locations are RVAs and are not portable offsets for another build.

| Contract observation | Pinned original-byte location |
|---|---|
| `Physics` registration associates `RayWorldIntersection` with implementation `0x39aa4b0` | `0x144ab05`, `0x144ab43`, registration callback at `0x144ab72` |
| Arguments 1/2 are vectors, 3/4 are integers | helper `0x39a4e6c..0x39a4ee7`; accesses argument indices 1, 2, 3, 4 |
| Argument 4 is the entity-type mask, not native ray flags | `0x39aa6ab` loads it to R9D; wrapper call at `0x39aa6fc` |
| Native `ent_all` value is `0x11f` (287) | registration constant at `0x14518fc` and name at `0x1451903` |
| The binding's initial entity mask is also `0x11f` | `0x39aa52c` |
| Optional arguments 5/6 resolve entity IDs, then obtain their physical entities for the skip list | `0x39aa56c..0x39aa5f3`, `0x39aa64c..0x39aa679` |
| Requested max hits is capped at 10 | `0x39aa61d..0x39aa635` |
| Native ray flags are fixed separately to `0x1000f` | `0x39aa6f4` |
| Origin and direction/delta are passed through without normalization or an added origin | `0x39aa67d..0x39aa6fc`, wrapper `0x4846e4..0x4847ad` |
| Each hit exposes `pos`, `normal` and `dist` | `0x39aa737..0x39aa76b` |
| Hit array is populated from index 1 | counter starts at zero; increment at `0x39aa816`, indexed insertion at `0x39aa823` |
| With fewer than seven arguments, return is the newly populated hit table, including an empty table for zero hits | table creation `0x39aa534`; zero-hit branch `0x39aa70c`; table-return branch `0x39aa85f..0x39aa867` |
| Supplying argument 7 selects a caller-provided output-table path and numeric hit-count return | `0x39aa5f6..0x39aa613`, `0x39aa84b..0x39aa85d` |

The current five-argument Lua call `Physics.RayWorldIntersection(origin, delta, 1, ent_all, player.id)` follows this contract. Retain strict `hits[1].pos/normal/dist` validation, including segment consistency; do not replace it with a guessed numeric-count or zero-based return parser. The engine collision implementation's treatment of rays starting inside solids, exact surface starts and hit ordering is not recovered by this binding review. An empty result or zero-distance contact must not be interpreted as body clearance. Filtering the player does not filter every other dynamic object.

The retained ordinary camera ray at log line 65 returned a table with distance `0.3871`, a 3D hit position and unit-like wall normal using the same five-argument form. This is execution evidence for that form at that earlier frame, not a result for any later ledge scan. The existing test fixture's arbitrary mask `797` was not retail evidence; use the pinned `287` in fixtures and record the actual runtime value.

Focused REA/Ghidra 12.1.4 evidence `ev_f3e00b729781bbf684282aefdb2dd9b5f2281471ac61634a3fd9973172d0df78` covers the complete inclusive binding body `0x1839aa4b0..0x1839aa8a2` (1,011 bytes). The address-preserving derived analysis PE has source-region SHA-256 `6bedd526658404de3704f3959f2ba1b0ee02cbb90bfc9433d90b76e35fed4a4a` and container SHA-256 `9adec5d23ffd15b0cc0522cb85a5e5cc56452d642801a04d98bd673e8a89175f`. External code/data, RTTI and unwind data are absent from that derived input and were correlated with the original-byte reports. In particular, a decompiler's incomplete return-type inference for this reduced container is not the Lua API contract.

## Confirmed coverage differences, plausible miss causes

The shipped retail member `Scripts/Entities/actor/Parameters/Player_Params.xml` was reread from the installed `Data/Scripts.pak`; member SHA-256 remains `9cc1225288be58675704f0b17067ef9798052bdd613f7a1178b502ef6a63e4cc`. These are shipped parameters, not proof of the current actor's effective runtime values or proof of any exact failed obstacle's dimensions.

| v0.0.16 sampling choice | Consequence and corresponding native evidence |
|---|---|
| Three rays only, at player world Z +0.50/+0.95/+1.30, on one flattened view axis | Low obstacles below 0.50, gaps between sampled bands, lateral edges and oblique faces can be missed. Native nearby settings specify a 0.5 search radius and 40/45 degree angular limits rather than a single line. |
| Forward segment length 1.10 | Geometry between 1.10 and the shipped 1.60 base nearby-search distance is not sampled. The shipped speed coefficient is 1.8, but its formula and effective runtime reach are not asserted here. |
| Requires a nearly frontal, nearly vertical face; accepts the globally nearest face before checking its normal | A nearer sloped/low/decorative hit can mask another sampled acceptable wall. A single nearest result per ray cannot prove there is no useful face behind it. |
| Face distance below 0.15 is rejected | Very close contact is a deliberate v16 blind spot. A zero-distance/inside-origin result needs explicit diagnostics rather than promotion to a clear path. |
| Downward rays always start at player Z +1.65 and end at +0.50 | A top above 1.65 may put the ray origin inside geometry or above the reachable search band; a lower top may fall outside the sampled segment. A miss does not measure wall height. |
| Top heights accepted only in 0.65..1.50 | This matches shipped `VaultOnto` allowed heights, but shipped thin `VaultOver` allows 0.45..1.50. A vanilla small obstacle may belong to a different action class; numeric runtime action type 2 remains unmapped. |
| Landing centre is 0.38 behind the face, with forward sample offsets +/-0.32 and lateral offsets +/-0.32 | The footprint spans approximately 0.06..0.70 behind a frontal face. A narrow rail/wall may fail that climb-onto footprint while supporting a native vault-over. This is not a ray-parser error or proof of a defective native candidate. |

The pose source is also an explicit assumption. `GetWorldPos()` supplies the player's entity world origin; v16 treats its Z as the height reference without a same-frame ground/contact or capsule-floor measurement. Nearby retained watch samples show a camera roughly 1.5 m above and about 0.12 m horizontally away from that origin, which is consistent with different origin and camera spaces but does not prove exact foot contact. `GetViewCameraDir()` supplies the view ray axis used by the already working camera-ray command; v16 flattens its XY part and discards pitch. It does not derive a body-forward vector from the entity world angles. The native selector's inspected route builds its query in actor-local space. Camera direction and actor orientation must be logged independently before deciding whether their mismatch matters at this spot. No getter's name alone establishes correspondence to the native selector basis.

## Minimal next detector and same-spot evidence

Keep the geometry component read-only and bounded. Extend discovery to low sample heights and a small lateral set, using 1.60 m as a documented diagnostic reach. Associate each proposed top with the face that produced it. Find/bracket the lip before sampling landing depth, and distinguish a thin/low obstacle hint, a climb-onto footprint result and an out-of-native-height observation. A higher diagnostic-only vertical band can explain a wall refusal; it does not change the host parameters or authorize that height. Validate every ray and retain explicit unknowns. Point headroom/footprint rays still do not establish a swept capsule trajectory, stable support or registered native ledge.

Each requested scan should log a bounded scalar record containing:

- Scan ID, bridge epoch/generation/identity/reset/frame sequence and exact async stamp; request-to-scan age and completion reason.
- Actual `ent_all`, entity ID, entity position, entity angles, camera position, raw camera direction and normalized horizontal scan axis; every sampled relative height/lateral/depth offset and configured diagnostic limits.
- Per-ray stage/index, origin, full delta, invocation status and valid empty/hit/error distinction; copied hit distance/position/normal and derived vertical height/approach dot. Retain no physics/entity handles or world-point objects for later movement.
- Selected face/sample index, lip bracket, top/footprint/headroom result and the exact failing sample rather than only a generic final reason; finite ray count and explicit budget exhaustion.
- Native observer frame/time join, candidate action/encoded handle and the later native eligibility/admission observations, without mapping numeric action types or calling candidate presence action completion.

The same-spot test should then distinguish geometry outside the old sampling volume, a true geometry/landing refusal, native candidate absence and later native refusal. Exact same-frame joining remains a separate requirement if a Lua scan and native callback are only timestamped near one another. Actuation stays disabled until native registration/admission, body clearance and controller/lifecycle ownership are established independently.

Private reproducibility records are under `.local/native/ledge-detection-v17/`: `detection-source-evidence.json`, `ray-strings.json`, `ray-xrefs.json`, `ray-binding.json`, `ray-getargs.json`, `ray-binding-refs.json`, `ray-physics-registration.json`, `ent-all.json`, the derived PE/manifest and `rea-evidence/raybinding/*-raybinding-analysis.json`. `collect_detection.py` rereads only the existing retained log and shipped parameter member; `rea_detection.py` owns a separate project-local REA session. No existing shared analysis session was changed.
