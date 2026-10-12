# Targeted native inspection — 2026-10-11

This is static disassembly of a few Lua bindings, not recovered source or an ABI guarantee. No native code was injected and no process memory was written. Target: retail WHGame.dll SHA-256 `bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7` (1.5.6). Addresses below are RVAs relative to this module, not reusable runtime addresses.

Capstone 5.0.6 was downloaded as a binary wheel into the ignored work area. Its file inventory was inspected: no installation scripts or entry points. The wheel was extracted locally without changing system Python. A read-only PE script locates selected string references, bounds functions by x64 unwind records where available, and disassembles small windows. Whole-game disassembly/decompilation was not performed.

## actor:IsFlying — promising physics contact query

`IsFlying` string RVA `0x3ecc010` has a registration reference at `0x128a733`. The binding object constructor at `0x1289fd0` assigns vtable RVA `0x3ecb620`. The registered thunk at `0x2139e14` dispatches slot `0x128`, which contains function RVA `0x2abe08c`.

The 115-byte implementation acquires the actor, obtains its physical object, builds a status request with type **2**, calls a virtual status getter, and only on success returns whether the integer at request offset **+4** is nonzero. Missing actor, physical object or unsuccessful status returns no value. This explains why nil must not be treated as false.

This matches the legacy `pe_status_living` layout (`type=2`, followed by `bFlying`) and CryEngine's documented ground-contact query. It is a strong static correspondence, not yet verified by KCD2 stand/jump/land observations. The public CryEngine version and KCD2 fork are different; only the inspected wrapper layout is supported here, not the entire native structure.

References: [CryEngine physical entity interface](https://www.cryengine.com/docs/static/engines/cryengine-5/categories/23756813/pages/24282108), [physics and movement example](https://www.cryengine.com/docs/static/engines/cryengine-5/categories/23756813/pages/26216224). Diagnostic v0.0.3 samples `IsFlying()` at 10 Hz, logs changes, and keeps grounded status unverified until the playtest passes. It also reads the separate player fly-mode getter; flight mode must not be confused with physical contact.

## AI.GetStance — unsuitable with current runtime evidence

Registration reference `0xb98868` leads to wrapper `0x300fe5c`. It resolves entity ID, follows AI-related virtual interfaces and returns a member at +0x18 of a returned object. No direct player controller stance read was established. All **44** v0.0.2 samples returned 3 despite camera-height changes (1.1851–1.6594 m), walking and jumps. Do not use this query as the current player crouch gate.

Actor binding registration confirms `E_ACTORSTANCE_NORMAL=3`, `E_ACTORSTANCE_CROUCH=1`. Generic `STANCE_STAND=0` is a different enum; do not mix them.

## Remaining leads

- A separate legacy binding registers `GetStats` at `0xef8262`; wrapper `0x2abd7a8` returns a script table when its actor lookup succeeds. Availability on the actual player binding is unverified. v0.0.3 tests presence and reads only if present, plus selected historical `stats`/`actorStats` fields. This is a candidate, not an assumed solution.
- Actor-state reflection at `0x12bb3a4` describes `crouch`, `inAir`, `cutscene`, `interactiveScene`, `sprinting`, `dialogue` and other states. Their existence is a native investigation lead, not evidence of a Lua accessor. No guessed memory layout is used.
- `SetSpeedMultiplier` was not found as an exact binding name in the inspected retail module. Its legacy documentation does not establish availability.

Next: verify the contact wrapper at runtime, establish an authoritative crouch/accessor and lifecycle gate, then select the least invasive controller request. A native adapter is justified only for the missing surface, not a replacement for the host collision engine.


## Additional results

v0.0.3 runtime returned 26 successful IsFlying state observations (14 false, 12 true) through the requested movement/jump sequence, with no contact errors. This is now live evidence in addition to the static correspondence; complete slope/stair coverage remains open.

SetVelocity registration at RVA 0x14509e0 points to wrapper 0x37df550. The wrapper sends physics action type 10 with velocity fields, consistent with legacy pe_action_set_velocity. It is not a transform teleport implementation, but ownership, timing, actor overrides and actual collision behavior of custom movement remain untested. Do not call it merely on the strength of this static finding.

The community loader's inspected C_Player constructor signature returned **zero** matches in retail 1.5.6. Targeted RTTI tracing identifies a constructor candidate at 0xbc780c (primary vtable 0x3aa4b98), rather than validating the loader's pattern. This is additional reason not to install the whole loader on assumed compatibility. Native adapter work is deferred while the supported XGen observer is tested.

Legacy layout reference: [Lumberyard CryCommon physinterface.h](https://github.com/aws/lumberyard/blob/master/dev/Code/CryEngine/CryCommon/physinterface.h), locally inspected ePE_status_living=2 and bFlying at the first status payload integer; ePE_action_set_velocity=10. This is a comparison reference, not KCD2 source.

## SimulateOnAction inspection, 2026-10-12

Same pinned retail WHGame.dll. Actor scriptbind registration near RVA 0x128a4b3 supplies thunk 0x213a0e4; the thunk dispatches vtable +0xb8. Scriptbind vtable 0x3ecb620 resolves that slot to 0x2ac3010. This wrapper resolves the actor, interns the action string and forwards mode/value to actor virtual +0x4a0. C_Player primary vtable 0x3aa4b98 resolves the slot to 0x8e5618 -> 0x8e5644.

That function calls 0x8e56d4 and only enqueues the action through cold block 0x2255e0c when the filter accepts it. The filter compares six interned action members at +0x100/+0x108/+0x130/+0x128/+0xe8/+0x118 in the action table. The table initializer at 0x17387d0 maps those to prepare_to_attack, attack_primary, draw, block, attack_stop and attack_special. Moveforward is at +0x18. Thus the inspected primary player dispatch **does not provide forward locomotion via this Lua method**. No unsafe runtime press/release experiment was attempted. Runtime vtable identity was not read from process memory; this is static build-specific evidence, not a live call trace.

Installed input definitions in IPL_GameData.pak confirm moveforward/moveback under movement_y, and toggle_crouch. Numeric activation modes were not needed or guessed after the forwarding path was rejected.

The crouch CVar registers at 0x1593cd6 with configuration-object field +0xa8, but a limited reference search did not establish active player consumption. Absence from that limited search is not proof of unused code. wh_actor_MovementSpeedMultDefault's own description identifies terrain-surface fallback behavior; it is not a per-player swoop control. None of these CVars is written. Installed per-character rms buff definitions offer a better supported next experiment.
