# v0.0.12 retail terrain results - 2026-10-19

The unchanged F8 crouch-burst prototype passed the player's gentle-slope, short-stair and previously traversed low-step checks. The player reported expected slope bursts, smooth boosts both up and down stairs, normal descent/landing, no automatic resumed burst after landing, and successful fresh-press recovery. This is scoped traversal evidence on the tested routes, not general clearance, edge prevention or high-drop acceptance.

## Paired observations

| Session | Operator-labelled route | Samples / exact packet joins | Applied writes | Result |
|---|---|---|---|---|
| 1 | Ordinary dry slope up/down/across | 587 / 27 | 0 | Normal control; command stop |
| 2 | Live slope up/down/across | 793 / 134 | 677, manually reviewed boundary | Expected bursts; passive trace reached its duration limit after motion |
| 3 | Ordinary dry short stairs up/down | 233 / 8 | 0 | Normal control; command stop |
| 4 | Live short stairs up/down | 372 / 49 | 191 | Smooth boosts in both directions; command stop |
| 5 | Ordinary dry low-step descent/landing | 171 / 11 | 0 | Normal descent and landing; command stop |
| 6 | Live low-step approach and landing recovery | 637 / 45 | 143 | Normal descent/landing, no resumed old burst, fresh press works; command stop |

All 2,793 position/velocity samples were finite, and all 274 exact native packet joins agreed. The returned velocity Z remained zero despite changing position height; it is not usable as a reconstruction of world vertical motion. Position sampling is about 10 Hz. Chord sums do not measure exact pulse travel or collision penetration. The phase names describe the player's chosen geometry; slope angles, ground normals, support distances and clearances were not measured.

Session2's diagnostic stopped at 90.003296 seconds with a valid canonical boundary. Its 677 writes ended 41.603577 seconds before that boundary; no later native writes belong to the slope window. The analyzer correctly retains its automatic-stop warning and withholds automatic association counts. The manually reviewed count above does not turn it into a clean command-stop trace.

## Complete native audit and cancellation

The stopped capture contains 49,310 publications and 32,657 player requests. All 1,011 applied-write branches are retained and agree with the shutdown counter. There are zero unsafe/vertical writes, declared-bound violations, dropped records, audit faults, audit contention or fatal state. A write branch count includes near-unity terminal steps and is not a count of perceptible boosts. Request serial groups also need not correspond one-to-one with physical key presses or distinct actions.

On low-step serial31, four modified requests were followed by a contact-unsafe request 16.703 ms after the last write. Four later logged safe nonzero requests under the same serial did not resume scaling; fresh serial32 did activate. This measures cancellation after the reported support change, not physical edge-to-signal latency. Later serials33-35 refused their request mode before the contact signal changed, so their cancellation cannot be attributed solely to contact loss. Cancelling scaling does not brake normal host input or momentum already accepted by KCD2.

Two publication errors remain visible in the raw analyzer: already pause/frame/clock/observer-unsafe clock rollbacks at sequence23179 (609.917419 to609.917175, reset7) and sequence29856 (736.555542 to736.554871, reset9, dry). Both were outside terrain phases and had no nearby candidate or applied write. Their exact attribution is retained; there is no general allowance for future publication errors. The underlying timer ordering is unresolved.

The player disabled native movement. Stop succeeded for explicit PID33064, and independent process reads confirmed both original hook prologues restored. The owned DLL remains pinned and inert until normal game exit. No movement code, tuning, input profile or original executable changed during these tests.

## Evidence and remaining work

Private evidence stays in ignored `.local/`:

- Native capture: `.local/native/swoop/build-playable/state-bridge-33064-1488724750.jsonl`.
- Lua capture: `.local/logs/20261019T185702178445Z-probe.log`.
- Analyzer summaries: `.local/evidence/20261019-v012-native-final-summary.json` and `.local/evidence/20261019-v012-terrain-final-summary.json`.
- Player reports: `.local/evidence/20261019-terrain-player-notes.md`.
- Independent final review: `.local/native/bridge-abi/20261019-v012-terrain-final-independent-review.json`; scoped acceptance with the exact warnings above. It also checked all 22 contiguous write runs, including those crossing request serials, against timing/travel bounds.
- Source/package-bound final review: `.local/evidence/20261019-v012-terrain-reviewed.json`; tested owned source/payload/receipt snapshot: `.local/evidence/v012-terrain-baseline`.

Native capture SHA-256: `5403fcee66fdb3ef8b5b8edaceb99998828687053553843a60706c064e6f955f`. Independent review matched all 18 build source pins, both payload hashes, retained-write/shutdown accounting and the six phase windows. No writes occurred outside those windows or after native disable.

Next coverage is narrow passages/door frames and broader uneven terrain, then integration of stamina, noise/detection and feedback. Donor timing/steering/camera fidelity remains a separate task. The present action neither predicts an edge nor supplies a ledge grab, fall brake or gap traversal. [Repeatable procedure](TerrainValidation.md).
