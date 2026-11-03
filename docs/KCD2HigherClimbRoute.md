# Higher climbs: the remaining native route

Updated 2026-11-03. The native loader selected the owned Kuttenberg member and retained an appended inactive guide and its markers after the save loaded. Both experiments were archived and the baseline restored. [Loading contract](NativeGuideLoading.md) and [archive attribution](GuideSourceAttribution.md).

The target is retail KCD2 1.5.6/build 23914554, `WHGame.dll` SHA-256 `bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7`. Tools 1.5.5 export compatibility remains a separate question.

## Registered geometry has a concrete shipped representation

Retail `Data/Levels/kutnohorsko/level.pak:whdata_1` contains plain XML with4,129 ledges and27,564 markers. Ledges reference marker ranges, side flags and entity IDs; markers provide position, facing and corner data. The selected member SHA-256 is `dfd631becc308ee4be1d460fbb0ba3c600582f8776e1e1bb8fa8de4a6ab1162c`.

The original loader at RVA `0x1331224` reads ledges and markers, then calls `0x7e6a50` to build the normal spatial index. The original resolver at `0xf44f18` decodes the same object/segment/side fields. The tested mod used an overlay of the complete member and preserved the serialized base record prefix. General runtime registration and additive layer mounting remain unverified.

The v17 candidate handle47251457 decodes to object721, segment0, side1. Its authored guide lies about0.49m ahead and0.766m above the scan1 entity origin. Both observed handles identify opposite sides of this decoded segment under the inspected encoding, rather than proving two world obstacles. The guide crosses all three detector lanes between the0.7 and1.0 height rows. This gives a useful test baseline, but guide data does not establish the physical collider shape. Exact runtime array identity and same-frame joining remain unverified.

## Current wall: native data and load ownership

At scan pose(2896.809814,766.515259,61.381580), all2,525 observed native queries returned no handle. Independent analysis of all23,435 adjacent segments in the pinned shipped member finds none within1.6m horizontally; nearest object1246/segment0 is17.256561m away. Across every observed query position, a conservative distance lower bound is14.846653m. These static-member results support a local guide-coverage gap without proving live-registry completeness or the absence of other registration sources.

Original bytes identify C_EntityModule primary vtable `0x3eaa180`, virtual +8 to dispatcher `0x9b4664`. Event `0x1f` takes a serializer from message +0x18 and manager from owner +0x110. At `0x9b4849` it invokes wrapper `0x13311dc` and the normal load/index path. C_Game virtual +0xf0 leads to `0x170ff94`, which requests `<folder>/whdata_<index>`, obtains XML, creates a callback-local serializer and dispatches that event synchronously. [Source evidence and observer](NativeGuideLoading.md).

Retail selected the complete byte-identical `whdata_1` overlay from the owned PAK. A separate load retained the inactive appended record and both markers in the live manager. This establishes a path for shipping guide data. Index eligibility, base-handle stability under queries, unload/reload and save behavior for an enabled guide remain open. [Retail results](../STATUS.md).

## Eligibility and body clearance

The index builder loads the first side's flags at `0x7e6acc` and skips insertion at `0x7e6ad4` when bit 1 is clear. The inactive fixture kept both side flags at zero. Setting the first side to `2` would pass this gate. In action selection, the slot-kind branch at `0xf44cba` can reach the height gate at `0xf44ce8` without the action-type flag tests at `0xf44cd1` through `0xf44ce6`. This leaves traversal eligibility unresolved for an index-only flag setting. Use an independent traversal guard before testing indexed data.

The separate dry guard build intercepts the native query wrapper at `0x832188`. Its two known direct callers are `0x8320f5` and `0x89efa2`. The guard retains the spatial result for the audit and returns the native no-guide sentinel to the caller for object 4129, on either side and across segment values. In the owned fixture the normal base handle reaches the action framework, while both fixture sides stop before action selection.

The dry build starts with the original inactive append archive. After all hooks are armed, the pinned loader-to-index callback can set the one index bit in memory when the manager, record count, marker count, typed fingerprints and exact inactive tail match. The disk profile stays inactive. Stop closes the audit and disarms further activation while the query guard stays armed until game exit. This fixed-slot guard is scoped to the dry Kuttenberg session. Retail index visibility and lifecycle remain to be measured.

The mandatory resolver path `0xf44e00` checks resolved validity and the actor's side of the edge. These are geometric selection checks. The separate `CProceduralContext_AdjustPos` routine at `0x36e5b34` supplies a host collision-query lead. It issues four directional box sweeps via the physical world's virtual slot +0x1c8 (retail `0x8115bc`, worker `0x811680`), tagged `PrimitiveWorldIntersection(Action)`. It uses a box selector of 0, entity mask `0x10117` and sweep components at packet offsets +0x8c through +0x94. At `0x36e60e4` through `0x36e6132` it accepts distances strictly greater than zero and shorter than their sweep length for its adjustment logic.

Those comparisons leave initial overlap, endpoint contact and other return cases open for a clearance diagnostic. Recover the live collider dimensions and pivot, self exclusion, query filters and safe calling context before issuing an independent query. The shipped normal stance's `heightCollider=1.05`, `heightPivot=0.0` and `useCapsule=1` are authored inputs. They do not establish the live collider envelope. Establish open, blocked and initial-overlap controls before using a sweep to evaluate the rise, crest and landing along a climb. The pinned disassembly is archived under `.local/evidence/v026-eligibility/`.

The clearance observer captured zero calls at those four sites during a user-confirmed vanilla low climb. Continue with the separate capsule path. `C_CapsuleWorldInteraction` writes type 6 in its query packet at `0x27d48e8`, and the physics worker dispatches type 6 via `0x812aad` to the primitive constructor at `0x80f508`.

The collider probe obtains the physical entity through `0x8a49fc`, also used by the shipped `GetPhysicalStats` binding. It requires the `CLivingEntity` vtable and pinned type 1 dimensions getter `0x3d5384`. The retail getter returns pivot, collider size and center, and capsule mode at packet offsets +4, +0xc, +0x18 and +0x34. The probe reads from the verified controller callback for Henry. Retail dimensions, the final capsule envelope, filtering, initial overlap, trajectory and higher animation still need evidence.

## Captured actions and HighVault slots are now mapped

The completed v19 baseline has two fully associated accepted requests, both type2. The selector at`0xf44648` iterates11 slots, advances the parameter pointer by0x44 and returns the winning index. Consumer`0xf43600` checks that index and reads Params+0x2c+type*0x44. Combined with the named XML loader at`0xbfe050`, this establishes the mapping:

| Type | Action |
|---:|---|
|0|PullUpMidAir|
|1|PullUpFalling|
|2|VaultOver|
|3|VaultOverIntoFall|
|4|VaultOnto|
|5|HighVaultOver|
|6|HighVaultOverIntoFall|
|7|HighVaultOnto|
|8|PullUpQuick|
|9|VaultFrom|
|10|VaultWindow|

Thus the successful small obstacle is an ordinary **VaultOver**, not evidence of VaultOnto or HighVault. Its thin/vault authored guide is consistent with that result. The detector's flat supported climb-onto footprint must remain a separate requirement. Request tail`[1,0,255,255]` is not a HighVault classification; the inspected standing route assigns only the first two of those four bytes, and the final two remain uninterpreted.

## HighVault defaults and animation policy remain distinct gates

The global parameter bank getter`0x11b3794` initializes defaults via`0x11b3848`; that constructor initializes11 action slots with stride0x44 using`0x16aeaac`. New per-ID records also receive fresh common defaults instead of inheriting a loaded fallback profile. The slot defaults explicitly set movement duration, animation-height bounds and allowed-height bounds to0, ledgeType0 and isVault/isHighVault false. The inspected constructor does not establish correctionDuration+4. The XML reader at`0xbfe338` returns immediately for missing nodes, and shipped Player_Params has no HighVault nodes. Therefore this load route provides **no authored positive higher-height interval/high classification**. Actual runtime values, other overrides and universal disablement are not established by this static result; absent fields must not be treated as a complete valid action.

Authoring-side type handling at`0x2a86f40` has a real HighVault flag, and one shipped Kuttenberg ledge carries it. That classification alone does not prove that Henry can use it or that it is more than1.5m above support. Guide flags, numeric action index and request-tail bytes are separate data layers.

Allowed-height selection reads slot+0x38/+0x3c. Animation setup instead uses slot+8/+0xc; leaf`0xf432e8` normalizes measured height and clamps the resulting weight to0..1. Raising only allowedHeightMax does not supply taller animation. Raising heightMax remaps those same normalized samples rather than creating a taller motion. Host root displacement/warping, hand placement, collision and interruption still need proof.

The first taller trial used the existing type 4 action at the measured 1.639595 m wall. It extended only this selector call's allowed height to 1.70 m in a temporary parameter copy. The action setup resolves the ledge target through `0xf43aa0`, writes it at action offsets +0x64 through +0x6c at `0xf43392`, and passes the transform to the native grab action. The latter copies it at `0xf45322` through `0xf45346` and identifies the `TargetPos` parameter. This absolute target is separate from the clamped animation weight. The planned capsule corridor cleared in retail. One native request was accepted and the user confirmed the climb, landing and return of control. Hand placement and interruption remain open.

The selected retail male animation database has43 `LedgeGrab` fragments, with no explicit `high` fragment tags in this bounded query. Its ordinary upward-vault combined blendspace references examples named90/120/150 in the upper subspace. Those names and blend coordinates are asset evidence, not measured climb limits or permission to extend the motion. Greater height, warping, hand placement, collision and interruption support remain unproved.

The host action coordinates FullBody/HoldItem scopes, bone alignment, movement control and camera procedures. Preserve that single owner while recovering the actual selection and animation mapping. A separate vertical transform writer would leave those problems unresolved.

## Implementation order

1. **Complete:** revision4 passed two fully associated native request acceptances, with no admission errors and all six hooks restored. No repeated native-baseline test is needed.
2. **Complete for this wall:** v21 retail scan produced a1.015720m `candidate_hint` in44 rays with all mandatory profile/final-face confirmations, five landing support hits and five headroom misses. Observation-only shutdown is recorded with zero movement writes. Retained pre-instruction clock refusals remain explicit. No repeated scan/Jump baseline is needed; a sampled hint remains separate from a body sweep.
3. **One added ordinary-range wall climbed.** The guard verified index visibility and native VaultOnto selection. The one-shot trial passed the live capsule corridor check and forwarded one request. The user confirmed the climb and return of control at the 1.02 m wall. Unload/reload and interruption remain separate checks.
4. Specify one complete higher native action: type7 for a wide flat landing, or type5/6 for a crest with independently validated far-side landing/drop. Prove duration/correction fields, allowed and animation height intervals, guide-side requirements, flags and native tag/motion. Missing-node defaults cannot serve as that profile; a parameter maximum alone is insufficient.
5. Establish suitable taller motion/warping, full-body clearance and interruption/fall ownership, then prepare the bounded manual higher-climb experiment.

The authored-load route now supports the bounded ordinary-range climb above. Dynamic insertion still requires a verified allocator, index, thread and lifetime contract.

The tested higher wall's cap has no sampled flat standing surface. That blocks the current climb-onto candidate at this location; it does not settle future wall-crossing capability. A native HighVaultOver/HighVaultOverIntoFall route could target clearance over the crest and a separately validated far-side landing/drop, rather than treating the steep cap as standing support. Such a route still needs proven action/tag selection, compatible high motion, full-body trajectory clearance and interruption/fall ownership. It is a concrete long-term investigation after native request association, not an enabled fallback in the current detector.

Private reproducibility ledgers and original-byte/member evidence: `.local/native/ledge-v18/higher-climb-route.md`, `.local/native/ledge-v19/action-map/action-map.md`/`action-map.json` and `.local/native/ledge-v19/high-defaults/independent-review.json`, with linked raw reports. Only selected members/regions were inspected in this offline follow-up; no whole-game analysis, game access or modification was performed. [Current plan](LedgeClimbing.md), [runtime results](LedgeRuntimeResults.md).
