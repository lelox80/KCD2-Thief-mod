# Retail ledge selection and controller route

2026-11-02: the logical guide member producer and synchronous serializer/module/index route are now identified, with a completed normal save-load baseline: counts and all three fingerprints match, shutdown/restoration pass. Added guide registration and higher actuation remain unproved. [Guide loading evidence and test](NativeGuideLoading.md).

2026-10-22 follow-up: the revision4 native request baseline passed in retail; the captured numeric type2 is now source-mapped to ordinary VaultOver. HighVaultOver/IntoFall/Onto map to5/6/7, but their missing XML/default configuration and separately clamped animation weights do not enable higher motion. [Current native route](KCD2HigherClimbRoute.md), [runtime results](LedgeRuntimeResults.md). The original investigation below retains its historical scope.

Read-only static investigation, 2026-10-21. Target: retail KCD2 1.5.6, Steam build 23914554, `WHGame.dll` SHA-256 `bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7`, image base `0x180000000`. All addresses below are RVAs. They do not apply to the installed 1.5.5 tools or a different retail DLL. No game was started, attached to, or modified for this investigation.

The best first implementation route is to preserve KCD2's existing ledge action, animation and controller, and establish an observation baseline before attempting to add candidates. The native selector expects registered ledge handles. A valid geometric collision sample alone cannot be supplied as a native ledge. Runtime insertion and removal in the retail spatial index remain unproven; spawning a Lua `LedgeObject` is not established as an insertion method.

## Established selection and action flow

| Stage | RVA | Original-byte evidence |
|---|---|---|
| Standing traversal attempt | `0x831b20` | Reads standing mode; calls selector wrapper; checks action/handle; applies further input/approach checks; creates an actor action only on success |
| Standing eligibility and height wrapper | `0x831c78` | Calls `0x831e20`, then `0x831eac`; further checks can still reject the returned candidate |
| Candidate selector | `0x831eac` | Writes an eight-byte result pair; computes actor-local query direction and position, searches registered manager geometry, checks eligibility, selects action type |
| Manager query dispatcher | `0x832188` | Switches between editable ledge records and the normal spatial-index query |
| Normal spatial query | `0x8321ec` | Initializes handle to `0xffffffff`; queries manager spatial structure at `+0x60040` using position/direction/distance/angular bounds |
| Resolve handle | `0xf44f18` | Uses encoded side, segment and object indices to read object records and `0x1c`-stride geometry records |
| Candidate eligibility checks | `0xf442d0`, `0xf44e00` | Called before the candidate pair is written; meanings need runtime corroboration |
| Action-type selector | `0xf44648` | Uses ledge geometry, actor state and parameter data; numeric result is copied into result pair |
| Additional standing/input test | `0x1533840` | Called after selection by `0x831b20`; can still reject an apparently valid pair |
| Actor action creation | `0x175e008`, `0x175e0ac` | Allocates `C_ActorActionLedge`, copies the 12-byte action request and enters the existing actor action framework |
| Native ledge animation action construction | `0xf43600` -> `0xf45298` | Resolves the registered handle, constructs `CActionLedgeGrab`, and passes it to the host action controller |
| Native ledge action enter / cleanup | `0x702a80`, `0x702b7c` | Host animation/actor calls, control flags and cleanup; these are not standalone movement APIs |

RTTI identifies `CActionLedgeGrab` vtable `0x3e73ef8`, `C_ActorActionLedge` primary vtable `0x3c0d880`, `C_LedgeObject` vtable `0x3b53ae0`, and `C_LedgeStateRequestManager` vtable `0x3ec8448`. The request manager's virtual `0xf46788` also calls `0x831c78` and resolves an action request. It is a different layer from the geometry manager. The geometry manager global used by selection is the pointer at RVA `0x5484b38`; entity lifecycle handlers also reach it through the framework chain `+0xe0`, `+0x110`.

The shipped standing-mode CVar explicitly describes disabled, push-plus-Jump, Jump-only, and push-only behavior. That supports using the host Jump flow as the behavioral baseline. The static trace does not prove which control events reach this flow in the current runtime, the default mode, or that a new ledge candidate will pass its later gates.

## Minimal observation ABI

`native/LedgeProbe/Selection.h` records the statically established, Windows x64 calling convention:

```cpp
struct Selection {
    std::int32_t actionType;  // offset 0
    std::uint32_t handle;     // offset 4, encoded manager handle
};
using SelectionFn = void (__fastcall *)(void* actor, Selection* output, bool allowAlternate);
```

Entry `0x831eac` reads RCX as actor, RDX as output, and R8B as flag. It does not consume incoming stack arguments. It initially writes `0xffffffffffffffff` to output. On a qualifying result it writes the encoded handle at output `+4`, then the numeric action selector result at output `+0`. Failure paths have no defined return register value, and the caller ignores RAX. Therefore `void`, two pointers and a one-byte flag are corroborated by both original instructions and focused Ghidra pseudocode. The flag permits a candidate when the first eligibility test is false; its detailed gameplay meaning is not named by this study.

The only direct caller found and instruction-boundary verified is the call at `0x831cc8`, in wrapper `0x831c78`; its return RVA is `0x831ccd`. The exact call bytes are `e8 df 01 00 00`. Original selector first 16 bytes are `48 8b c4 48 89 58 10 48 89 70 18 48 89 78 20 55`; they cover complete, position-independent instructions. These are static identities, not permission to assume that an in-memory target is unmodified.

The result is **candidate selection, not action acceptance or completion**. Wrapper height/state tests and the standing caller's handle restriction, `0x1533840` result, action framework and animation checks still follow. A non-sentinel handle with action `-1` must also remain distinguishable from a selected action. The observer logs numeric action/handle fields without assigning unverified enum labels.

For ownership, read the current actor's controller at `+0x180`, then use the already traced `ktwin::owner` chain: controller vtable `0x3e74ac0`, controller `+8` back to actor, player vtable `0x3aa4b98`, actor `+0x38` to CEntity vtable `0x4072c78`, entity ID `+0x0c` equal to actor ID `+0x30`, nonzero. Require the actor returned by that verification to equal the callback's actual actor. Those pointers must not survive the callback or appear in the log; retain only entity ID and copied result scalars.

## Why a spawned shape is not yet a runtime candidate

`C_LedgeObject` rebuild at `0x2a8858c` fetches a shape by entity ID through `0x2a89d34`, requires at least two points, reads the entity transform, and builds world-space `0x1c`-stride point/normal records. It reads side/type properties through `0x2a86f40`. This matches the shipped Lua shape editor declarations and native extension.

However, rebuild registers geometry through `0x2a8a9ac` **only when geometry manager `+0x18+0x60010` is nonzero**. That branch writes a fixed array of up to `0x2000` records with object ID, vector geometry, side flags and float parameters. RTTI names the relevant fixed-array element `SLedgeObjectEditor`. The same flag switches normal selection to a separate branch. This is evidence for an editable/editor representation versus the usual prebuilt representation, rather than a general runtime insertion function.

Entity transform handling at `0x2a8bb1c` rebuilds transformed geometry and calls `0x2a8b9f0`. In the normal representation that updater looks up an **existing** entity ID, requires the new point count to equal the stored count, and copies geometry into the existing range. Missing IDs and count changes are ignored. The enabled handler `0xec5190` -> `0xec51d8` similarly changes flags on an existing ID; it does not create a record. The native selector resolves handles into object and segment arrays and uses the normal spatial index. Directly editing one array or toggling the representation flag would bypass its ownership/index invariants.

The shipped scripts prove authored shape configuration and extension enable/disable. They do not establish a supported runtime point setter or retail spatial-index insertion/removal API. Existing authored/baked ledges are the appropriate first native baseline. Arbitrary world geometry remains a separate capability gate.

## Built observation probe

`tools/build_ledge_probe.ps1` builds the separate `LedgeProbe.dll` and an owned-process fixture executable under ignored `.local/native/ledge-probe/build/`. It re-extracts the pinned MinHook 1.3.4 archive into a separate dependency folder, compiles the owned observer, runs fixtures, and emits `probe-build.json` with DLL hash, target hash, hook identity, record cap and hashes of all observer/shared/loader sources. `tools/ledge_probe.py` uses the existing explicit-PID loader with this DLL and selector prologue.

The observer verifies the complete original retail file identity, PE identity, existing shared pins, selector prologue and exact caller bytes. It pins itself for process lifetime, installs only the selector hook, forwards all three arguments exactly once, and reads the eight-byte output after the original returns. It filters the verified caller and verifies the full actor/controller/entity chain. It never invokes the selector or other engine getters, writes the result, changes control state, queues actions, calls Lua, or retains actor pointers. Host SEH and C++ exceptions propagate, with an in-flight counter decremented through `__finally`.

Only a separate writer thread performs file I/O. The callback uses a preallocated 2048-slot multiple-producer/single-consumer queue, copies ID/result scalars, and stops capture at 12,000 attempted records or queue overflow. Invalid result reads are explicit null records; invalid ownership increments a refusal counter. Log filenames are `ledge-probe-{pid}-{tick}.jsonl` beside the DLL. Stop disables the hook, waits for observed callbacks, drains the queue and closes the log; the original trampoline and pinned module remain until normal process exit. This avoids unloading referenced code.

Validation completed locally: no-game refusal and duplicate start refusal; full owner chain and mismatched IDs; exact caller filtering; invalid result reads; no-candidate sentinel; bounded/parallel queue and strict capture cap; actual MinHook detour forwarding and output preservation on owned functions; original execution after hook disable; host SEH and C++ propagation with in-flight cleanup; pointer-free JSONL/null records; writer failure disabling capture. This is **built and fixture-tested, not installed, attached, running-game verified, or movement-tested**.

## Retail observation checkpoint

The first standalone observer baseline passed in PID30580 on October21:1,216 readable exact caller/owner-matched outputs,54 candidate samples (numeric action type2, two encoded handles), zero owner/read/queue/logger failures and independently verified original-entry restoration. The user confirmed native small-obstacle climbing and higher-wall refusal. This updates the built-only checkpoint above; action admission, per-wall refusal cause and arbitrary registration remain unproven. [Complete results and boundaries](LedgeRuntimeResults.md).

## Next gates before active climbing

1. Back up and verify saves; run the separate observer against the explicit pinned retail PID. Capture an ordinary Jump near a known native ledge, a rejected attempt, and walking/Jump away from ledges. Establish whether the expected caller fires, numeric pairs are stable, owner IDs match, and original behavior is unchanged.
2. Compare native pairs with the dry geometric candidate probe at the same locations. Test low/high ledges, bad approach angle, blocked headroom, thin rails, stairs/slopes, moving props and non-native walls. A ray-only candidate is not full-body clearance evidence.
3. Establish exact action-type meanings and a reliable action-entry/completion/cancel observation before exposing an active request. Candidate selection alone cannot prove controller ownership through the traversal.
4. Find and validate supported retail index insertion/removal or another host-owned candidate route. Prove its allocator, lifetime, index update, ID/handle stability, thread affinity and cleanup on load/streaming/removal. Neither editor insertion nor an existing-ID updater satisfies this requirement.
5. Require swept body clearance, stable support, landing clearance and lifecycle cancellation before enabling any traversal on a geometric candidate. Keep unsupported actuation disabled; do not substitute camera interpolation or direct position changes for a host collision-aware climb.

## Private evidence and reproducibility

Original-byte reports are in ignored `.local/native/ledge-host/`: `selector-query.json`, `selector.json`, `selector-gates.json`, `entry-callers.json`, `player-jump-ledge.json`, `standing-ledge-call.json`, `input-ledge-call.json`, `manager-find.json`, `manager-find-segments.json`, `manager-record.json`, `ledge-rebuild.json`, `shape-fetch.json`, `register-geometry.json`, `update-geometry.json`, `ledge-activate.json`, `ledge-enable.json`, `manager-toggle-id.json`, `actor-ledge-constructor.json`, `action-request.json`, and the vtable/RTTI corroboration under adjacent `.local/native/ledge/`.

The focused REA input `selection.analysis.exe` contains only original RVA `0x831b20` through `0x83232f` (2064 bytes) in an address-preserving analysis-only PE. Source-region SHA-256 is `2d3b3e584e97695b1245b2bdd6ccd5e56f9f42065fb9a0be7a73978537e885fb`; container SHA-256 is `bd60369535ea921592f3ff7638dc94d1864ec03229daa3f0dfface0a35649736`. External code/data, RTTI and unwind metadata are absent from that derived container and were correlated against original PE reports. Ghidra 12.1.4 returned selector body `0x180831eac..0x180832186` inclusive, pseudocode with three arguments and `void` return, and caller `0x180831cc8`. Retained REA evidence is under `.local/native/ledge-host/rea-evidence/hostledge/`, analysis evidence ID `ev_eee06943f0997fa8761921374554c85e719281589f04f5579f5583487bb41b56`.

Reproduce an original-byte read with `python tools/kcd2_native_inspect.py --output .local/native/ledge-host/selector-query.json dump 0x831eac --limit 500`; the inspector refuses any source DLL whose full SHA-256 differs. Build/fixture validation uses `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_ledge_probe.ps1`. Static correspondence and owned-process tests do not establish live ABI safety, hook ordering, update thread behavior, input semantics or successful traversal.
For the bounded tall-wall retry, the pinned wrapper at RVA `0x832188` calls the normal spatial query at `0x8321ec` and returns the caller's output pointer. The selector checks the sentinel at `0x8320fa`. After a hit it calls `0xf442d0` and `0xf44e00` with the original actor and handle, then passes the original actor transform to `0xf44648`. The retry changes only the inputs to one extra spatial query after a miss. The original actor and eligibility path remain in use.
