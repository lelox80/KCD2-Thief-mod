# Stealth feedback results - 2026-10-21

## v0.0.14: graphical rendering works; proportions failed

The user confirmed the actual artwork appeared and reappeared on command, then reported it was squashed. Their crop shows an approximately2:1 horizontal ellipse. Lua status reports `renderer=graphic`, `visible=true`, `reason=ok`, with no graphical fallback. Functional loading, updates and off/on controls are observed; layout acceptance is withheld. v0.0.15 switches from fullscreen stretch to uniform dynamic center/bottom alignment, removes the visible noise meter at the user's request, and places the gem just left of the health HUD.

All five getters were finite/ok in120 retained samples. Visibility ranged-1.399..3.205 (80 distinct rounded values), noisiness0.255..0.866, footsteps×0.060..0.400, equipment visibility0.217..0.218 and conspicuousness0.049..0.549. These remain aggregate modifiers, not pure light or emitted/heard sound measurements.

Complete PID38588 capture:13,474 publications,9,453 player calls, all dry, zero request serials/candidates/actual writes, zero audit loss/fault/busy/contention/fatal state. Stop succeeded and independent reads confirmed both original hook prologues restored. The DLL remains loaded and inert until normal exit.

The raw analyzer retains nine clock-unsafe publication refusals: seq5495,5497,5501,5503,5505,5507,5511,5515 and10896. Each was invalid, already clock unsafe, and request serial0; no movement was armed. Exact timer ordering/cause is unresolved. No general clock-error allowance or new movement/time-change acceptance is granted.

Private complete capture: `.local/native/swoop/build-playable/state-bridge-38588-1629240906.jsonl`; collected log: `.local/logs/20261021T094746061399Z-probe.log`; summary: `.local/evidence/20261021-v014-hud-live-summary.json`; source-bound final review: `.local/evidence/20261021-v014-hud-reviewed.json`. The layout correction's running-game visual result remains pending.

## v0.0.13: raw feedback baseline

The player confirmed that all three HUD values are displayed and change. They also changed the time to night and observed lower visibility, including negative values. This verifies the first read-only feedback feature on the tested retail build and display. No further manual check is needed for this checkpoint.

Negative visibility is valid: `vib` is a signed aggregate host stat, with additive illumination/posture modifiers. It is not a percentage, lux measurement or detection probability. Lower numbers must not be presented as guaranteed invisibility.

## Observations

All five getters returned finite values with `ok` status in all 120 retained change-only samples. Logging reached its designed 120-record budget; display updates continued independently. The explicit final status also reported `visible=true`, `reason=ok`.

| Host value | Range in the 120 samples | Distinct rounded values |
|---|---|---|
| Visibility `vib` | -1.438 to 3.500 | 57 |
| Equipment visibility `evi` | 0.216 to 0.218 | 3 |
| Noisiness `noi` | 0.255 to 0.866 | 3 |
| Footstep multiplier `fsm` | 0.060 to 0.400 | 3 |
| Conspicuousness `con` | 0.049 to 0.549 | 2 |

The explicit final status recorded visibility -1.449, outside the bounded sample window. Equipment visibility changed little while overall visibility changed substantially, supporting the distinction between the two readings. The player's night observation confirms useful time/context response. Individual shadow, torch and posture contributions were not separately timestamped, so no isolated formula or light-only calibration is claimed.

Noisiness and footstep multiplier remain host modifiers, not instantaneous emitted/heard sound. Surface-dependent event production, listener distance/occlusion and guard response were not measured by this test. The simple text readout is a working foundation; a calibrated graphical light gem is not implemented yet.

## Native session

The complete dry capture has 14,654 publications and 9,264 player calls, with zero armed requests, candidates, applied movement writes, dropped records, audit faults/contention or fatal state. Native motion stayed dry throughout. Stop succeeded for explicit PID7592, and independent process reads confirmed both original hook prologues restored. The DLL remains pinned and inert until normal exit; the HUD no longer refreshes after the test hook stops.

The raw analyzer retains 15 publication errors, clustered at seq7578..7624. Each rejected state was already clock-unsafe, with request serial0 and no candidate/write. These coincide with a tiny-delta timing transition in a session where the player changed time; the exact cause and sample ordering are not established. They remain explicit, with no general clock-error allowance or new movement/time-change acceptance. This does not invalidate the separately observed finite readings and visible display.

Private inputs: `.local/logs/20261021T090410805119Z-probe.log` and `.local/native/swoop/build-playable/state-bridge-7592-1626568171.jsonl`. Root summary: `.local/evidence/20261021-v013-hud-live-summary.json`. The source/payload/package remain the reviewed v0.0.13 build. [Feature and controls](Stealth.md).

Independent runtime review: `.local/research/20261019-stealth/20261021-v013-runtime-review.json`. Final source/package-bound record: `.local/evidence/20261021-v013-hud-reviewed.json`; preserved owned baseline: `.local/evidence/v013-stealth-tested-baseline`. Native capture SHA-256: `2af91d233bd59956ee7df98b91d49bbe9ca8117658a196d5431961242d7ae861`.
