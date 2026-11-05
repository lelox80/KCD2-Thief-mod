# Test plan

## Thin freestanding wall

Load Kuttenberg with the crest build. Return to the freestanding wall that previously refused at the wall-section survey. Face a straight section from about 0.7–1.0 m away on level ground and tap normal Jump once. Report whether Henry reaches the crest, how the animation ends, and when control returns. The capture logs `crest_target` with the selected guide and native result. If possible, try a broad-topped wall in the same session as a control.

For the height retry, use the same freestanding wall that stopped in PID 22964. The capture records separate `wall_broad_height_m`, `wall_crest_height_m`, `tall_broad_height_m` and `tall_crest_height_m` values when the probes measure a surface.

The 2.20 m build covers the measured 2.147 m crest. Retest from the same approach with one normal Jump. The expected sequence is a crest measurement, airborne rise, clearance to the crest and a native VaultOnto request if the stock range is reached.

The crest trace build adds `vault_target` and `vault_release` records for the same transaction. Retest the same freestanding wall, then let Henry finish the animation with no additional input. Compare `checked_crest` with `vault_target` and `physical_before_release` and `physical_after_release`. An Exit can also occur on interruption, so record what the animation did visually.

For the placement calibration, repeat the same approach and Jump. Check that `target_adjusted` is true and compare the physical release pose to `checked_crest`. Report whether Henry stays on top after the animation and when control returns.

For the support trace, hold still for about one second after the animation. Read `support_before_release` and `support_after_release` in the matched release record, then follow `vault_support` events for the same transaction. A ground height reported while airborne is historical. Use the flying flag and the physical position to distinguish support near the crest from the later landing below it.

For the crest lift, repeat that same approach and one Jump. Check `blend_lift_m` in `vault_release`, compare the physical Z with `checked_crest`, and look for `grounded_near_crest` in the support samples. Report whether Henry stays on the crest and regains control.

For the physical release build, use the same single Jump and hold still for one second after the animation. Check `vault_physical_lift` for the gate or native request result and the live `velocity_mps` and `gravity_mps2` values. Use the matching `vault_support` events to distinguish an accepted lift request from measured support at the crest.

For the gravity-wait build, check whether `waiting_for_gravity` is followed by a request, another refusal, or `release_window_expired` on the same transaction. The same one-second hold captures the first supporting contact.

For the crossing build, approach the same wall and press normal Jump once. Let the vault finish and report whether Henry goes over to the far side and regains control. Compare `crown_rise_m`, `crown_depth_m` and the `vault_crest_push` request to signed `crest_forward_m` in the subsequent pose samples. The far-side landing is player-controlled.

## One Jump into stock VaultOnto

For the 2.10 m follow-up, try a broad-topped wall slightly taller than the confirmed 1.97 m wall. Compare it with a freestanding wall in the same session. Use one normal Jump per attempt. The capture reports `air_peak`, `wall_survey`, `tall_survey` and `clearance_failure` to distinguish jump reach, wall shape and landing support.

For the repeatable build, use the wall that passed at 1.97 m. Climb once, return to the lower approach by walking around, and repeat the same normal Jump in the same session. The second attempt should log `armed_reuse` followed by a handoff using the same guide handle.

Load Kuttenberg with the `jump-into-stock-vault` build. Choose a static wall a little over Henry’s head, about 1.75–1.90 m from level ground, with a broad, clear top. Stand still about 0.7–1.0 m away, face it squarely and tap normal Jump once. Hold position through the jump. Report whether Henry simply landed or transitioned into the vault, reached the top and regained control. The capture records `air_trial`, rise, remaining ledge height, and any native selection or framework response.

## Mid-session guide registration

Launch with `python tools/tall_wall.py launch` using the pinned `guide-runtime` package. Load Kuttenberg once. Try normal Jump on three different static walls with broad flat tops about waist to head high, then return to the first. Approach each from level ground, stand about 0.7–1.0 m away and face the wall squarely. Report which climbs played an animation, landed on top and returned control.

The capture field `runtime_lookup` distinguishes geometry refusal, clearance refusal, a refused or full pool, registration and reuse. The native handle distinguishes each guide. `climb_gate` and the request record distinguish subsequent native selection, release and framework acceptance.

## Longer tall wall and session guide

Launch with `python tools/tall_wall.py launch`, then load the tall-wall save. Try Jump from the known center, then from about one metre left and right, and from closer and one short step back. Walk back down between climbs. Keep the landing clear. Record which approaches climbed and whether the animation stayed aligned. The live capture distinguishes a missing guide, approach refusal, face or top survey refusal, clearance refusal and accepted native request. A normal save reload in the same process can also exercise the new index generation.

## Two-wall normal Jump playtest

For the lookup retry build, test the tall wall from the usual spot, a small sidestep, one short step back and a closer approach in the same session. Keep the landing clear and face the wall for each Jump. Check `tall_lookup_retry` and `climb_gate` in the capture. A recovered handle can still be rejected by native selection, live clearance or the action framework.

Start with `python tools/tall_wall.py launch` and the verified `guide-walls` profile. Wait for the ready message and load Kuttenberg. Try the 1.64 m wall from the successful approach, then try roughly one short step back and a small offset to either side. Face the same part of the wall and press Jump once per attempt. Try the 1.02 m raised-pavement wall in the same session. Record positions that succeed, ordinary jumps, and any animation or control issues. The admission capture records the native guide result and the last reached gate on each fresh Jump.

## Taller 1.64 m wall corridor

The focused geometry scan found a 1.639595 m face and five level landing samples. The standing capsule check passed all four poses and three sweeps after its ground controls. The original hook was restored.

The first climb trial passed with one framework acceptance and user-confirmed animation, landing and normal control. A repeat can focus on hand contact and visible clipping. Restart before another one-shot trial.

At the main menu, start `python tools/tall_vault_trial.py start --pid <pid>` with the verified inactive tall package installed. Load the save and verify the tall guide. While still disarmed, approach the scanned wall and confirm type 4 selection with the selector-local 1.70 m limit. Arm only once the player is in the measured approach and the area is clear. Press Jump once within 30 seconds and report hand placement, animation, landing and return of control. Review the sealed result with `tools/analyze_vault_onto_trial.py`. The capsule path and stock action target justify this bounded experiment. Animation quality above the authored range and cancellation remain to be measured.

## One native VaultOnto climb — passed

PID 32612 forwarded and accepted exactly one request for the added guide. The user confirmed animation, ascent and restored control. The gate used current ground contact. No repeat at this wall is needed for the climb milestone.

Restart at the main menu with the verified inactive guide package installed. Use `python tools/vault_onto_trial.py start --build-name dry-guide-selection-v026-live-contact --pid <pid>` before loading Kuttenberg. Verify the corrected guide and approach the same raised-pavement wall from the lower street. From the last measured spot, move about 0.77 m left while facing the wall. Stand and face it straight on with the landing clear. Arm using the same build name once the player is ready. The arm lasts 30 seconds. Press Jump once and report animation, landing and return of control. The guard admits at most one exact native request after a live standing check, verified ground contact or immediate take-off from it, and a fresh corridor query.

Stop with `python tools/vault_onto_trial.py stop --pid <pid>` and review the capture using `tools/analyze_vault_onto_trial.py`. Keep the guard resident until normal exit. Framework acceptance and visible completion are separate results. This trial is scoped to the previously measured 1.02 m wall.

## Guarded native action selection

The inactive guide archive is already installed. At the menu use `python tools/dry_guide_probe.py start --build-name dry-guide-selection-v026-authored --pid <pid>`. Load Kuttenberg, approach the raised-pavement wall and face it from the street. The fixture uses the shipped single-sided wide-vault flags `0x207`, including the eligibility bit missing from the first preview. The scoped selector can evaluate the guide while its caller receives a refusal before action creation. Review the raw choice and withheld-selection count with `tools/analyze_dry_guide_probe.py`. A type 4 choice establishes native VaultOnto eligibility at the tested pose. The guard stays armed until exit.

## Capsule wall corridor — completed

The standing pose, lowered overlap and downward sweep controls passed in retail. The focused corridor capture returned zero for all four overlaps and three sweeps. The original entry was restored. The query covers the planned capsule corridor. Actual climb animation and interruption still need separate tests.

## Player collider dimensions

Use the restored baseline. Load the Kuttenberg save, stand on level ground and attach with `python tools/collider_probe.py start --pid <pid>`. Stay standing for several seconds, then crouch. Stop the probe and verify original entry restoration. The capture should identify one canonical player and report finite dimensions for both poses within 128 sampled callbacks. The test reads dimensions and preserves the original controller request. Capsule overlap and sweep controls follow separately.

## Guarded index registration — completed

The test passed with one activation, 981 fixture queries, 981 refusals before selection and zero fixture framework requests. The guard remained armed until normal exit. The test install was archived and the baseline restored with matching hashes. [Status](../STATUS.md).

## Vanilla clearance observation — completed

Use the restored baseline and a verified save backup. Build with `tools/build_clearance_probe.ps1`, then use `python tools/clearance_probe.py start --pid <pid>` after ordinary load. At the known low obstacle, perform one normal climb. Stop with `python tools/clearance_probe.py stop --pid <pid>`, verify restoration with the same tool's `status`, and review the completed JSONL with `tools/analyze_clearance_probe.py`. Capture bounds and counters must reconcile. An empty capture means these four call sites were not observed during the session. A nonempty capture still requires exact player association and a separate whole-body clearance check.

The user confirmed a normal low-obstacle climb in PID 33840. The complete capture contained zero sweeps and the original entry bytes were restored. Continue with the native capsule query before requesting another clearance test.

## Inactive guide storage — passed in retail

The tested `guide-append` profile appended one record and two markers to the proven member path. The new record used zero side flags, which the native index loop skips at `0x7e6ad1`. Its coordinates are a storage fixture near the measured v21 top.

The complete member 1 observation matched all expected counts and fingerprints. The user confirmed normal control. Stop and independent restoration of all three hook sites passed. A subsequent read-only snapshot confirmed the disabled tail remained present in the loaded manager. The append profile was archived after exit and the baseline hashes were verified.

Before the next retail test, establish a traversal guard independent of the index flag and define a read-only clearance diagnostic. The clearance fixture needs a known open path, a blocked path and initial overlap, with the live player shape and query masks accounted for. For the later enabled guide, verify index visibility and lifetime across unload and reload before trying a climb. [Native route and remaining gates](KCD2HigherClimbRoute.md#eligibility-and-body-clearance).

## Byte-identical guide mount — passed in retail

The base and owned archive were each observed on their respective loads. The byte-identical test yielded a qualified owned archive attribution and user-confirmed normal control. Both hook sites were verified restored. The experiment was archived after game exit and the restored baseline hashes matched. [Procedure and results](GuideSourceAttribution.md).

## Native guide loading — completed

The ordinary save-load baseline passed in PID34548. Member1 has exactly one guarded source/body/index chain with4,129 ledges/27,564 markers and matching full/structural/marker fingerprints. Member0 has no ledge callback and remains unattributed. All records/counters reconcile with zero drops/unlinked/overflow/truncation/writer fault. The user confirms normal control. Observer stop and independent verification of all three original entries passed; module remains pinned/inert until normal exit. [Evidence](NativeGuideLoading.md).

The subsequent archive-attribution and inactive-storage tests are recorded above. An enabled guide still requires spatial eligibility, lifetime and clearance evidence.

## Completed manual check - v0.0.21 top and landing scan

The October22 check is complete. Installed/hash-verified v21 returned `candidate_hint`,44 rays, height1.015720m, with five landing hits, five headroom misses and all required compound-profile/final-face confirmations. Only the motion-compiled-out bridge was used; no admission observer, repeated Jump baseline or added action. Shutdown is recorded with zero movement writes. All789 clock publication refusals preceded instructions by at least72.193s; the raw analyzer warning remains. The prior independent audit reports both hook sites restored. November2 source/package/install hashes and offline suites pass; no new playtest was performed. [Results](LedgeRuntimeResults.md).

**No unchanged-build scan or native-baseline repeat is needed.** The next implementation dependency is resolving the loader's exact member/source selection, then preparing a byte-identical no-op mounting experiment. No new manual run is ready yet. Guide registration/lifetime, continuous whole-body clearance and exclusive host action/interruption must be established before an added climb. A point-sampled candidate does not satisfy those requirements.

Offline checks include exact old/new replay under both number models, connected/separated/gapped profiles and final faces, stale-cap isolation, second-join refusal, all mandatory-sample failures and the exact46-ray path. Prior suites and all lifecycle/48-ray/32-scan bounds pass;26 Python tests, seven bridge/core fixtures and admission detours/conformance pass again November2. Clock warnings remain in the evidence; no clock gate was relaxed.

## Completed manual dependency - v0.0.20 wall geometry

Offline checks pass26 Python tests, both Lua number models, seven native bridge/core fixtures and four actual owned admission detours. Geometry regressions include positive/negative lean, compound cap transitions, realistic world coordinates, disconnected shelves/roofs, reach limits and all prior narrow/ridge/lifecycle cases. v20 is installed/hash verified after the user saved/closed KCD2, with fresh independently verified237-file saves. The observation is complete: user reports ordinary Jump only, one27-ray compound-profile refusal and2,525 native query misses. Both observers stopped/all six sites independently restored. Preserve the pre-test clock/publication warning and standing-only all-safe warning; zero motion writes. Session `.local/evidence/20261022-v020-ledge-runtime-session.json`. No unchanged-v20 repeat is needed. The user selected a broad raised-pavement wall and reports that vanilla climbing refuses there.

The chosen wall appears to have broad pavement behind it; the image is not collider proof. Reload the saved spot, attach the observation-only admission observer and bridge, then run one `kt_ledge_probe` while facing the wall straight on from the lower street, away from stairs, posts and NPCs. Close the console, then try ordinary Jump. Collect exact rays/refusal or hint and visible behavior separately; a hint still does not authorize a custom climb or prove a body sweep. Stop observers and independently verify their entry sites restored. The current narrow vault-over obstacle and steep cap are not positive onto test surfaces; repeating them is optional regression only.

## Completed manual dependency — v0.0.19 native identity

One ordinary small-obstacle check is complete. Revision4 retained1,237 transactions with2 fully matched accepted requests and no admission errors. The user confirmed ordinary climbing; both observers stopped/all six original sites restored. The bridge made zero movement writes and retains its standing-only all-safe warning. Tested source/payload/install pins and the71-file snapshot are verified. No repeated native-baseline attempt is required. [Results](LedgeRuntimeResults.md).

The runtime verifies the player/component/back-pointer/action/framework association for this route. It does not verify new-target lifetime, animation completion or higher-climb clearance. Those remain separate implementation/test gates.

## Completed manual dependency — v0.0.18

Offline checks pass:25 Python tests, double and retail float32 Lua, seven native bridge/core fixtures, four actual owned admission detours and native-emitted JSONL conformance. New fixtures cover each request predicate/guard counter/protected-page fault, no stale action identity, realistic world-coordinate chamfers, separated cap/roof refusals, long slopes, low thin surfaces, bounded camera direction and unchanged lifecycle/TTL isolation. Both scan paths use at most42 rays. Captured v17 ray replay reproduces the previous results rather than inventing new runtime rays. Independent review found no actionable issue.

A manual baseline is now necessary. The original combined failure did not retain the guard branch, and additional static review did not justify changing ownership/layout. v18 is installed/hash verified after normal closed-game deployment, with a fresh independently verified237-file save backup. Await the user's loaded test spot, then attach the revision3 ledge observer followed by the observation-only bridge; capture one ordinary small-obstacle climb and `kt_ledge_probe` there and at higher walls. Check exact first-failed guard (or a fully associated acceptance), cap/flat-top discovery and low-surface diagnostics. Record visible completion separately; phase proximity is not exact pose joining. Stop both observers and independently verify all six original entries restored. Added climbs stay disabled.

This completed check validated precise failure localization and useful geometry refusal evidence, not native request association. Its exact owner failure led to the v19 correction above. New-target registration, body sweeps, higher animation and interruption remain separate work.

## Current ledge check — v0.0.17 complete

The user resumed installation and completed the saved small-obstacle/higher-wall test. v0.0.17 is installed/hash verified with a fresh237-file verified save backup. Ordinary Jump climbed the low obstacle and refused the higher wall. Geometry still missed the low obstacle; two higher scans failed lip consistency. Native search/selection were observed, but three guarded request-copy failures left admission unassociated. Both observers stopped and all six entries were independently verified restored. Phase proximity remains distinct from an exact pose join. No more manual attempts are required for this checkpoint. [Results](LedgeRuntimeResults.md), [admission contract](KCD2LedgeAdmission.md).

Automated geometry checks use analytical box intersections under double and retail float32 Lua. They cover low obstacles,1.4m reach, lateral faces, tall-but-refused surfaces, inspection bounds, thin landing, overhead obstruction, a separated ceiling, sloped decoration, exact mask287, detailed ray logs and existing lifecycle/identity/gate/expiry/budget isolation. The native fixture detours all four owned functions and preserves opaque register arguments, four float stack arguments, output pointers and raw return bytes. It exercises pre-call action copying, mismatched manager/action/owner/caller, selected-only/unknown/refused stages, nested suppression, SEH/C++ exceptions, bounded parallel accounting, queue/capture/writer failure and restoration. Production-formatted native samples pass the same Python analyzer used for later retail captures. The seven existing native fixtures and22 Python checks also pass. None of these fixtures establishes retail admission or completed climbing.

The completed check distinguishes low-obstacle misses, higher-wall lip refusals, absent native query handles, selection refusal and missing framework requests. It does not qualify candidate-associated framework acceptance: preserve the raw three guarded capture failures and the admission analyzer warning. Preserve the bridge's standing-only all-safe warning too; there were zero motion writes. Correct the detector/guard contract in a later build before requesting another test. Dynamic registered-target lifetime, full-body clearance and interruption ownership remain unproven; no added climb or height change is authorized by these results.

## Completed runtime checkpoint — v0.0.16

The standalone vanilla selector baseline passed with user-confirmed small-obstacle climbing/higher-wall refusal. v0.0.16's marked read-only comparison is now complete, with clean counters and independently verified restoration at all three hook sites. Native candidates occurred in the small-obstacle phase and none in the higher-wall phase; geometric scans returned two `no_wall` and one `no_landing_sample`, with no positive hint validated. Improve detection before added-climb testing; no more manual repetitions are required for this checkpoint. Keep the raw standing-only `No all-safe state observed` analyzer warning and do not grant live movement or infer a tall wall's height/metadata. [Results](LedgeRuntimeResults.md).

## Historical ledge build — v0.0.16

Automated checks cover analytical ray/box ledge geometry, insufficient depth/width, overhead obstruction, unknown/malformed getters/rays, all canonical lifecycle gates, standing/crouch, direct async expiry, no stale queue, bounded work and zero gameplay writers under double/float32 Lua. Native owned-process tests verify exact selector forwarding/output, caller/owner rejection, guarded unreadable output, queue/capture bounds, exceptions, logger failure and restored test detour. Compilation and these fixtures grant no retail-climb acceptance.

The original v0.0.16 build was initially stopped before installation; its later baseline and geometry comparison are complete above. Avoid repeating accepted swoop microtests. Added candidate registration, body clearance and actual climb handoff still need separate proof. [Implementation plan](LedgeClimbing.md).

## Current layout correction — v0.0.15

The user already verified graphical display and off/on controls in v0.0.14; their screenshot rejected its stretched proportions. The correction removes noise from all display paths, switches to uniform fit, and positions the original gem left of health. Collector fixtures still verify unknown/noise separation, two-argument receipt, fallback and lifecycle isolation. Compiled-art validation checks no noise clip and the intended stage/placement. After installation, a brief glance to confirm round shape and correct placement is sufficient. No repeated minute-long light/control sequence or movement test is requested.

## Current graphical check — v0.0.14

Build verifies real collector fixtures under both Lua number models, UI call receipt/fallback/retry and lifecycle/unknown cases. The converted standard SWF has only selected original art plus our adapter/noise bars, resolved bitmap/placement dependencies and a350ms heartbeat. Offline art previews and successful compilation are not retail rendering proof. [Asset provenance](HudAssets.md).

One combined dry session is sufficient: check the actual gem/bars at lower-left, walk shade/light, toggle crouch/torch, pause/resume, and use `kt_stealth_off`/`kt_stealth_on`. Collect `kt_stealth_status` for renderer mode/receipt and raw stats. Stop the native callback while game UI advances to inspect automatic expiry. No F8 enable, movement retest or new perception acceptance follows from this UI change.

## Accepted input baseline — v0.0.13 passive stealth HUD

Automated real collector-to-HUD fixtures pass under double/float32 Lua: bounded getter cadence, owner/context changes, pause/load/unsupported contexts, typed unknowns and truezero, stale status without callbacks, renderer failures isolated from publication, bounded logging and no gameplay writes. Existing native/wire and17 Python checks pass. Dedicated movement microtests are unnecessary for this passive addition.

The October21 combined check passed for visible changing feedback; the user also observed lower negative visibility after switching to night. All five getters returned valid values in120 bounded samples, with zero native movement writes. Both hooks were restored;15 already clock-unsafe refusals remain explicitly reviewed, without a blanket exception. No additional manual check is required for this checkpoint. Individual light-only calibration and actual heard-noise changes remain separate. No percentage, lux, dB, hearing-radius or invisibility interpretation is accepted from raw numbers. [Results and boundaries](StealthResults.md).

## Preserved baseline — v0.0.12 terrain trials

The October19 paired ordinary/live trials passed on the tested gentle slope, short stairs and low step. The user confirmed smooth boosts, normal landing, no old resumed burst and fresh-press recovery. All 1,011 retained writes match shutdown, with zero unsafe/vertical writes, bound violations or audit loss; both hooks are restored. Two exact pause-clock refusals and one diagnostic timeout remain documented in [TerrainResults](TerrainResults.md). Physical edge-to-signal latency and broad terrain acceptance remain unverified.

For new routes, follow [TerrainValidation](TerrainValidation.md): ordinary dry traversal and packet/contact review before live comparison. Next coverage is narrow passages/door frames and broader uneven terrain. Existing support and request-mode refusals remain enabled.

Automated checks pass for passive telemetry lifecycle/error isolation, real double/float32 log conformance, missing/unknown vectors, no-catch-up bounds, packet-mismatch refusal, incomplete boundary/sequence detection, and native cancellation/no-rearm/original-Z behavior at 30/60/120 Hz. These checks establish input/diagnostic behavior, not actual terrain physics. Complete native shutdown and player observations are required after the trials.

## Previous checkpoint — v0.0.11 logger / F8 retest

**Scoped retest passed.** User confirmed all input/wall checks and a final successful burst after more than six minutes. Shutdown records 1,022 writes, zero unsafe/vertical modifications or bound violations, zero logger drops/busy/fatal state, and both hooks restored. One already-unsafe pause/clock rollback refusal is the sole publication error and is retained with a capture-bound review. The session includes pauses. The procedure below is the repeatable regression, not an outstanding request. Next acceptance: slopes, stairs and narrow passages, followed by controlled ledge/lost-support behavior.

Follow [PrototypeControls](PrototypeControls.md). v0.0.10 produced noticeable initial bursts, but the logger latched off before completion. Its final key/wall sequence is inconclusive. The v0.0.11 build must sustain an initial burst, held-key/no-queue/input-release/wall checks and a final burst after at least six minutes of normal play. Verify the state publisher is still fresh throughout; do not accept a test performed after the bridge stopped.

Owned automated coverage includes 108,000 records across nine producers and one consumer; held reservation and ordered commit; queue-full refusal and wraparound; eight-hour idle sampling; pre-actuation queue/budget refusal; disk failure; normal and exceptional host calls in both compiled motion profiles; sticky Lua fault disable/reload; no Lua reentry after an API exception. Packet/adapter/core, real double/float32 Lua and emitted-wire fixtures also pass, with 11 Python tests including retained-write counter agreement and bounded larger captures. No game is opened by these tests.

After the retail test, disable then stop the prototype, independently read both original hook prologues, and review complete shutdown. Require no unexplained publication errors, drops, fatal fault or reservation contention, exact agreement between applied-write count and retained records, all modified-request safety/vertical/travel bounds, and user-confirmed continuous response/control. Keep request-space and world-space findings separate. Nominal envelope timing can include a final host step.

Historical milestones: v0.0.9 passed dry reset/pause/reload and small live wall/control tests but felt too subtle; v0.0.10 stronger F8 activation worked initially. Prior precision/clock corrections and their failures remain documented in MODLOG. Full M1 acceptance below, including slopes/stairs/ledges and stealth consequences, remains open.

## Native foundation, 2026-10-17

Run `tools/build_native_probe.ps1` for the observation DLL plus owned-process probe and trajectory fixtures. Passed: no-game/duplicate-start refusal, scalar read/invalid-read handling, queue capacity and concurrent producer accounting, actual detour forwarding and restoration; Hermite endpoint/derivative, continuous steering splice, 30/60/120 Hz integrated deltas and invalid/time-gap refusal. These are independent fixtures, not retail motion/collision evidence.

The [explicit-PID native capture](NativeProbe.md) passed its first retail test: 3,673 requests, nine observed worker threads, zero unreadable/dropped samples, clean stop and independently verified original bytes restored. User completed stand/walk/crouch/pause and reported unchanged movement. A verified 237-file backup exists. The user opened the game manually after Computer Use failed. Establish fresh state delivery, coordinate/time semantics and ownership, then apply M1a/M1b below to the bounded action. This observation test does not pass the motion/collision gate.

## M0 — code loads and logs player state

Prerequisite: closed game, verified save backup, installed package hashes, retail 1.5.6. Launch normal retail through Steam; load a Kuttenberg save. Expect `[KT] boot version=0.0.4`, capability rows and `player_available`, followed by at most 120 one-second samples. Walk, stand, crouch, crouch-walk and turn; compare positions/velocity/raw stance. Pause/resume and reload the same save; record observed timer/event behavior. Optional `kt_probe`, `kt_mark`, `kt_rays`, `kt_watch`, `kt_stop`.

Collect with `python tools/project.py collect`. Confirm no Lua errors, meaningful player position, missing fields explicitly unknown, and game controls unaffected. Note if console commands are unavailable. A successful package install is not a runtime pass. GUI labels or method presence do not prove function semantics.

Automated: run Lua 5.1 stub tests for absent player, nil/error APIs, entity replacement, bounded capture, stop/reload, stale callback and documented ray argument shape. These check our error handling only, not engine API correctness. Build verifies manifest, archive entries, stored ZIP compression and CRC.

## M1a — collision-aware control proof (candidate built; retail pending)

After M0, identify reliable gates and one movement-request path. Only then prepare a bounded diagnostic motion action with a clear cancellation command. Establish that the host controller resolves solid walls and ground contacts and releases ordinary movement after interruption. Log requested/actual velocity and displacement, collisions and controller state. A raycast, teleport or noclip demo cannot pass this gate.

## M1b — crouched swoop acceptance

| Scenario | Required result |
|---|---|
| Open flat ground | Smooth start/finish; measured travel/duration within declared tolerance; normal movement resumes |
| Wall directly ahead | Shortens/stops before full body overlaps; no penetration or shaking |
| Stairs up/down | Predictable step response, no tunneling or launch; normal ground handling retained |
| Slope up/down/cross | No unintended slide/air boost; collision and support logged |
| Narrow passage | Body width respected; side collisions clamp without getting stuck |
| Ledge | Initial policy cancels on lost support; no unexplained hovering or teleport |
| Repeated activation/held key | Cooldown honored; one action owner; no stacked velocity |
| 30/60/120 FPS | Comparable distance/time; tolerance recorded before test, not adjusted to disguise failures |
| Pause/resume | Action cancels or safely suspends per policy; no time catch-up jump |
| Load/save transition | Stale action cleared; new entity/generation reacquired; no persisted transient motion |
| Combat/dialogue/cutscene/horse/interaction | Refuse or cancel safely; ordinary control returns |
| Noise/stamina/fall/guards | Compare baseline footsteps and awareness; stamina/fall damage still meaningful; differences documented |

Each trial records action id/state, requested and actual start/end positions, elapsed simulation time, distance, velocity, collision/contact results, grounded state, frame rate, input direction, cancellation reason and build. Transition logs default on; detailed sample rate configurable and bounded.

## Later milestones

Mantle/vault: known obstacle first; too tall/far, overhead blockage, narrow landing, interrupted animation, moving blocker and failed clearance.

Lean: near-wall camera clamp, both sides, rapid alternation, pause/load, guards at known lines of sight. Compare visibility from camera versus body/head.

Hang/climb: no support, discontinuous support, roof collision, climb exit room, safe/unsafe drop and every lifecycle interruption.

Stealth: fixed equipment/time/weather/guard/save baselines; light/dark, quiet/loud surfaces, walking/crouching/swoop, suspicion/search/recovery. Confirm crime and save persistence.

## Save recovery

Backups are `.local/backups/<UTC timestamp>/kcd2-saves.zip` with SHA-256 manifest. Close game and Steam before manual restoration. Preserve the current save directory separately, extract a verified backup into the configured save directory and resolve Steam Cloud conflicts deliberately. Restoration is not automatic and has not been tested on the live profile.

## M0.3 contact test (read only)

After loading the existing Kuttenberg save, stand on clear ground for five seconds, jump three times with a pause between landings, crouch and walk for five seconds, stand up, and use an ordinary stair if nearby. Do not deliberately jump from a dangerous ledge. Pause for five seconds and resume; reload the same save after the initial capture. All normal game controls stay in charge.

Expected diagnostic result, not yet observed: IsFlying=false while supported, true during a jump, false after landing; no unknown/error values during ordinary locomotion. Logs include raw values, position, simulation/async times and animation at transitions. The probe does not cause motion. Check whether gameplay-start capture restarts after loading and whether pause produces an async-time gap. These observations do not alone prove safe action interruption. `kt_watch` starts another bounded capture if necessary; `kt_stop` stops it.


## M0.4 XGen state bridge

Load the backed-up existing save; toggle crouch three times, holding each state for 2–3 seconds. Check observer and summary fields agree, complete batches arrive at about 100 ms intervals, and timestamps stay fresh while active. Pause/resume, then reload the same save; old observations must clear. Review errors involving player/switch/switch.xml, ActorStateCheck and ExecuteLua. Confirm ordinary movement/interactions behave normally. Avoid creating a new permanent progress save until serialization/removal compatibility is assessed. The tests validate observation only, not motion or stealth changes.

## M1a — v0.0.5 bounded speed comparison

Follow [SpeedTrial](SpeedTrial.md) for the exact console sequence, expected behavior, unknown gates and cancellation limits. Capture open-ground before/boost/after samples, direct wall obstruction, standing-up cancellation, pause and same-save load. Confirm the modifier's table/API loading and removal separately from actual speed and collision. A successful automated fixture is not a retail test. Keep the full swoop matrix (stairs, slopes, narrow passage, ledge, repeated activation, frame rates, pause/load, stamina/noise/damage) outstanding until this primitive is proved.
