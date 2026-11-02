# KCD2 native movement interface

2026-10-17, bounded static study of retail 1.5.6/build 23914554. `WHGame.dll` SHA-256 `bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7`, image base `0x180000000`, SizeOfImage `0x5b2d000`. Installed development tools 1.5.5 are not ABI evidence for this retail build.

## Identified request path

| RVA / offset | Static evidence |
|---|---|
| `0x3e74ac0` | RTTI-supported `C_ActorMovementController` primary vtable |
| vtable `+8`, `0x837e7c` | Request merger; copies/accumulates flagged fields into controller `+0x10`, returns true in inspected path |
| vtable `+0x10`, `0x8a2728` | Movement-state getter candidate; output ABI unverified |
| `0xf2f688` | Controller constructor; actor initialization allocates `0xa48` bytes and stores the controller at actor `+0x180` |
| `0x799e9c` | Player input constructor stores owning actor at input `+0x10` |
| `0x8e5324` | Player input action dispatch; action reference RDX, activation mode R8D, value XMM3 |
| `0x1f22fb0` | Per-frame caller obtains player input from actor `+0xb88` |
| `0x82f77c` | Builds the local-player movement request, maps input, fetches controller, calls request virtual |
| `0x1a72750` | Actor controller getter: returns actor `+0x180` |
| `0x82faa5` / return `0x82faa8` | Actual `call r8` into request virtual; useful player-only caller filter |
| `0x8307d8` | Input speed/locomotion mapping before final submission; partial deep-analysis coverage |
| `0x6cf58c` | Add/copy movement vector at request `+0x7c`, sets flag `0x20` |
| `0x838568` | Sets speed pair at request `+0x38/+0x3c`, sets flag `4` |

The input path calls the controller getter via actor virtual `+0x228`, then controller virtual `+8`. The identified return RVA comes from decoding the original bytes, rather than a guessed public engine layout. The full request structure and lifetime have **not** been declared ABI stable. The diagnostic reads only six identified scalars and flags during the original call.

The speed mapper can clear/rewrite the input delta and apply host locomotion speed information before submission. A unit direction in the earlier input request is therefore not necessarily the vector received by the final controller. A bounded modification at the final request path is a promising future actuator, but static tracing alone does not establish its physical units, integration cadence, collision behavior, or ownership during animation-driven movement.

## Thread and lifecycle constraint

Retail registers `wh_actor_ParallelPrePhysicsUpdate` with a description enabling parallel actor prephysics. Do **not** assume a movement hook runs on the main thread. Do not call Lua, allocate per sample, perform file I/O, or retain actor/request pointers in it.

The observation probe uses a fixed 2,048-slot bounded queue with nonblocking producer locking. A separate worker writes at most 12,000 samples. It records numeric thread IDs for cadence analysis and does not log actor/request addresses. It forwards the identical request pointer and result regardless of capture success.

Future control needs a separately verified, generation-stamped state snapshot, produced on an appropriate host thread and expired using a measured clock. Fresh crouch/contact, lifecycle, incompatible activity, player identity and controller ownership are mandatory. A request arriving from the normal player path is **not by itself** proof that a swoop can take over safely.

## Implemented observation probe

`native/MovementProbe/Probe.cpp` and `tools/native_probe.py` provide an explicit-PID, observation-only experiment. Startup verifies the original DLL SHA-256, image size, request prologue, original caller instruction and controller vtable target. The loader separately checks the process executable path, both retail file hashes, remote module path, our DLL build receipt and backup receipt. Unknown builds are refused.

The temporary in-memory MinHook detour is installed only by an explicit export after `LoadLibrary` has returned. `DllMain` only stores its own handle and disables thread notifications. Stop disables the detour and drains the writer. Our DLL and trampoline remain pinned until normal process exit to avoid unloading in-flight code. It supports one initialization attempt per process. It does not replace an original executable, install a proxy DLL, alter a CVar, write movement, or change the Lua package.

Owned-process tests passed for absent-game refusal, duplicate initialization refusal, bounded overflow, concurrent producer accounting, scalar reads, invalid-read handling, actual wrapper forwarding and detour restoration. After Windows Computer Use initialization failed, the user manually opened retail and loaded an existing backed-up save. Explicit-PID attachment and the first read-only retail capture then succeeded.

## First retail observation result

- 3,673 local-player requests over 69.8128507 seconds; nine observed worker threads.
- 172,763 total controller calls were filtered; 3,673 matched the precise player caller and expected controller vtable. Zero unreadable requests and zero queue drops.
- Median request interval 0.01665455 seconds. Four gaps over 100 ms, largest 6.9438205 seconds, occurred during a session including pause and focus changes. Exact gap causes were not independently timestamped.
- All samples carried the movement-vector flag; none carried the separate speed-pair flag. Common flags were `0x1060` and `0x201060`; one `0x81060` sample also occurred. Do not assign untraced meanings to the other bits.
- Idle vector `(0,0,0)` occurred 1,831 times. Common forward vectors included `(0,3.710,0)` and `(0,3.101,0)`; the concurrent Lua capture showed approximately 3.68 and 3.09 world-speed magnitudes during ordinary and crouched movement. This is consistent with a local velocity-like request. Coordinate frame, units and exact timing still need a controlled trace before motion writing.
- User completed the requested stand/walk/crouch/pause sequence and reported movement unchanged.
- Explicit stop succeeded, writer emitted final counters, and an independent read of the first 16 request-function bytes confirmed the original prologue restored. The owned module remains pinned until normal process exit, as designed.

Private result: `.local/evidence/20261017-native-retail-summary.json`; native capture `.local/native/probe/build/movement-probe-10936-1324808656.jsonl`; concurrent Lua excerpt `.local/logs/20261017T211019642184Z-probe.log`. This passes observation/forwarding only. It does not prove safe motion arbitration, collision-aware actuation or an action's pause/load cancellation.

See [native test procedure](NativeProbe.md). Complete state/ownership integration before enabling motion; retain KCD2 collision and gravity.

## Private evidence

`tools/kcd2_native_inspect.py` checks the pinned original and emits bounded Capstone/PE evidence. `.local/native/kcd2/` contains RTTI, constructor, player input, submission, request, delta and parallel-update observations. REA request evidence `ev_fa258a81f3e53fbb3767b424d5f33e7c14dcb1c5a2566a18a6eed11c56471891`; partial speed mapping `ev_c853fbbb8f55b61ca961ca2e02dbf17ebb7e617df07f356c294ccaf35e135616`. Their subjects are derived bounded containers, not full original binaries. Missing external branch islands cause incomplete speed pseudocode; correlate with the original disassembly. No complete physical-controller ABI is claimed.
