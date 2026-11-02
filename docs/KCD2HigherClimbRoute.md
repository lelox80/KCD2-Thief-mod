# Higher climbs: the remaining native route

2026-10-22. Higher climbs remain the long-term target. This offline investigation narrows the host data and animation questions; it does not enable a new climb. Target is retail KCD2 1.5.6/build23914554, `WHGame.dll` SHA-256 `bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7`. Tools1.5.5 export compatibility remains a separate question.

## Registered geometry has a concrete shipped representation

Retail `Data/Levels/kutnohorsko/level.pak:whdata_1` contains plain XML with4,129 ledges and27,564 markers. Ledges reference marker ranges, side flags and entity IDs; markers provide position, facing and corner data. The selected member SHA-256 is `dfd631becc308ee4be1d460fbb0ba3c600582f8776e1e1bb8fa8de4a6ab1162c`.

The original loader at RVA`0x1331224` reads ledges and markers, then calls `0x7e6a50` to build the normal spatial index. The original resolver at `0xf44f18` decodes the same object/segment/side fields. This supports investigating a small authored guide whose index the host builds during load. Additive layer mounting, compatible export and avoiding replacement of base-level records still need proof. No level override was created. Existing editor-only insertion and existing-ID transform updates remain insufficient for general runtime registration.

The v17 candidate handle47251457 decodes to object721, segment0, side1. Its authored guide lies about0.49m ahead and0.766m above the scan1 entity origin. Both observed handles identify opposite sides of this decoded segment under the inspected encoding, rather than proving two world obstacles. The guide crosses all three detector lanes between the0.7 and1.0 height rows. This gives a useful test baseline, but guide data does not establish the physical collider shape. Exact runtime array identity and same-frame joining remain unverified.

## Current wall: native data and load ownership

At scan pose(2896.809814,766.515259,61.381580), all2,525 observed native queries returned no handle. Independent analysis of all23,435 adjacent segments in the pinned shipped member finds none within1.6m horizontally; nearest object1246/segment0 is17.256561m away. Across every observed query position, a conservative distance lower bound is14.846653m. These static-member results support a local guide-coverage gap without proving live-registry completeness or the absence of other registration sources.

Original-byte one-hop follow-up names the loader owner: C_EntityModule primary vtable0x3eaa180, virtual+8 -> event dispatcher0x9b4664. Numeric event0x1f takes an existing serializer from message+0x18 and manager from owner+0x110, then callsite0x9b4849 invokes wrapper0x13311dc and the normal load/index path. That callback does not open a member/archive; source/member/VFS selection remains upstream and unknown. Entity init/update/rebuild paths inspected so far still establish no normal new-entity insertion route.

A source-pinned full `whdata_1` member preserving the Root except appended owned guide/markers is a candidate packaging experiment. These bytes neither prove that overlay works nor make a full exported level mandatory. First prove byte-identical no-op member mounting/source selection and load/save behavior; only then consider a bounded append that preserves base handles and lifecycle. No overlay, guide or parameter change was generated. Private reports `.local/evidence/v020-wall-registry/findings.md`, `registration-lead-review.md`, `serialize-owner-route.md` and their evidence manifest retain the exact search/format limits.

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

The selected retail male animation database has43 `LedgeGrab` fragments, with no explicit `high` fragment tags in this bounded query. Its ordinary upward-vault combined blendspace references examples named90/120/150 in the upper subspace. Those names and blend coordinates are asset evidence, not measured climb limits or permission to extend the motion. Greater height, warping, hand placement, collision and interruption support remain unproved.

The host action coordinates FullBody/HoldItem scopes, bone alignment, movement control and camera procedures. Preserve that single owner while recovering the actual selection and animation mapping. A separate vertical transform writer would leave those problems unresolved.

## Implementation order

1. **Complete:** revision4 passed two fully associated native request acceptances, with no admission errors and all six hooks restored. No repeated native-baseline test is needed.
2. **Complete for this wall:** v21 retail scan produced a1.015720m `candidate_hint` in44 rays with all mandatory profile/final-face confirmations, five landing support hits and five headroom misses. Observation-only shutdown is recorded with zero movement writes. Retained pre-instruction clock refusals remain explicit. No repeated scan/Jump baseline is needed; a sampled hint remains separate from a body sweep.
3. Prove one owned ordinary-range guide through supported retail load/registration, first resolving serializer/member selection with a no-op mounting experiment. Verify base-handle stability, streaming/load/save cleanup and rollback. Existing VaultOnto type4 is the first climb-onto profile to validate; no height extension is required for that milestone. Full-body trajectory clearance and exclusive host action/interruption remain required before an added climb.
4. Specify one complete higher native action: type7 for a wide flat landing, or type5/6 for a crest with independently validated far-side landing/drop. Prove duration/correction fields, allowed and animation height intervals, guide-side requirements, flags and native tag/motion. Missing-node defaults cannot serve as that profile; a parameter maximum alone is insufficient.
5. Establish suitable taller motion/warping, full-body clearance and interruption/fall ownership, then prepare the bounded manual higher-climb experiment.

Dynamic insertion remains an alternative if its allocator/index/thread/lifetime contract can be proven. The authored-load route is currently the more concrete lead. Neither route is validated for added climbs in the current build.

The tested higher wall's cap has no sampled flat standing surface. That blocks the current climb-onto candidate at this location; it does not settle future wall-crossing capability. A native HighVaultOver/HighVaultOverIntoFall route could target clearance over the crest and a separately validated far-side landing/drop, rather than treating the steep cap as standing support. Such a route still needs proven action/tag selection, compatible high motion, full-body trajectory clearance and interruption/fall ownership. It is a concrete long-term investigation after native request association, not an enabled fallback in the current detector.

Private reproducibility ledgers and original-byte/member evidence: `.local/native/ledge-v18/higher-climb-route.md`, `.local/native/ledge-v19/action-map/action-map.md`/`action-map.json` and `.local/native/ledge-v19/high-defaults/independent-review.json`, with linked raw reports. Only selected members/regions were inspected in this offline follow-up; no whole-game analysis, game access or modification was performed. [Current plan](LedgeClimbing.md), [runtime results](LedgeRuntimeResults.md).
