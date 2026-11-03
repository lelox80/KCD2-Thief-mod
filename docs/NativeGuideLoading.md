# Native guide loading and registration

## Appended inactive record verified in retail

The byte-identical mount test established the owned archive route. The next profile appended one inactive guide and two markers at the proven logical member path. On November 3, PID 33944 returned 4,130 ledges and 27,566 markers with full, structural and marker fingerprints matching the staged XML. The user confirmed normal control. Stop and independent verification of all three original hook entries passed.

A read-only snapshot of the loaded manager found the appended record and markers intact, with both side flags zero. The original retail index gate at `0x7e6ad1` skips such records. The marker stream remained identical. All 76 base ledge differences were eligibility-bit clears on records with nonzero entity references, consistent with the post-index validation path at `0x133137c`. The observation covers persistence after load in this world. Future work covers unload and reload, index eligibility and body clearance. Local evidence `.local/evidence/20261103-v025-guide-append-runtime-session.json` and `.local/evidence/20261103-v025-loaded-manager-diff.json`.

2026-11-02. The target remains **climbable higher walls through KCD2's native traversal, animation and collision systems**. The immediate milestone is proving how the host loads and indexes guide data, then adding one ordinary-range guide with a supported lifetime. Higher action configuration and suitable motion follow separately. No added climb is enabled by this work.

## Recovered retail route

The original retail1.5.6/build23914554 DLL still matches SHA-256 `bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7`.

| Boundary | Evidence from original bytes |
| --- | --- |
| Logical member producer | C_Game primary vtable+0xf0 points to RVA0x170ff94. Incoming RDX is the folder; R9d is the member index. The function constructs `<folder>/whdata_<index>`. |
| XML load | System virtual+0x418, CSystem0xd1b0ec and CXmlUtils0x9792b4 return a node reference. The producer checks it for null before constructing the serializer. |
| Serializer lifetime | Constructor0x17101bc creates a stack-scoped XML input serializer, mode byte+8 and node reference+0x30. It lives only through synchronous module dispatch. |
| Module dispatch | Event0x1f at0x171010b carries that serializer at message+0x18. Dispatcher0x479bec invokes module virtual+8 synchronously. C_EntityModule0x9b4664 passes its manager+0x110 to wrapper0x13311dc. |
| Arrays and normal index | Body0x1331224 resets/reads arrays and calls0x7e6a50 to rebuild the normal spatial index, then0x133137c validates entity IDs. Missing entities can have eligibility flag bits cleared at that last stage. |

Only selected complete functions and their relevant references were inspected. A focused551-byte address-preserving REA input corroborates the producer's void return and four incoming arguments; external code/data are absent from that derived input and were checked against original bytes. Ghidra12.1.4 evidence `ev_a189b17ce4f38302dfa1fab4103682aef82929c4c438345bb1d751cf80d003d2`; private trace and source manifests are `.local/evidence/v022-loader-trace/`.

The requested virtual member path does **not** identify the physical archive selected by VFS. Matching output from a byte-identical override would not resolve that question. No mod member overlay or new guide has been created or installed.

## Pinned member and comparison data

`level.pak:whdata_1` is7,589,941 bytes, SHA-256 `dfd631becc308ee4be1d460fbb0ba3c600582f8776e1e1bb8fa8de4a6ab1162c`. It contains4,129 ledges,27,564 markers and a separate GameProfileManager. The complete member must be preserved in any later no-op experiment; rewriting only LedgeManager would discard unrelated state.

`python tools/guide_member.py inspect` validates the pinned source, record schema, finite float32 geometry, native field widths and marker ranges. Explicit `stage-noop --output .local/<new-folder>` stages exact complete-member bytes and a receipt. It creates no PAK and performs no installation or registration.

| Comparison | Canonical typed encoding | FNV1a64 |
| --- | --- | --- |
| Full ledges | little-endian `<IHHHHf`,16 bytes per record, serialized order | `41075d281a1defb3` |
| Structural ledges | bytes4..15 of each ledge; excludes only entityId | `8bfc91bfec28f68e` |
| Markers | little-endian `<ffffffB`,25 typed bytes per record; excludes3 padding bytes | `d7c2ce5751170770` |

Entity IDs use a special serializer operation and may be remapped during load;240 source ledges have nonzero IDs. Keep full and structural comparisons distinct. For the completed capture below, all three typed fingerprints and counts match the source hypotheses; this is scoped data agreement rather than proof about every serializer path. FNV values are diagnostic fingerprints, not cryptographic provenance. Exact SHA-256 comparisons and source receipts remain local.

## Observation-only probe

`native/GuideProbe` observes the producer, array body and normal index builder. It forwards the original calls and their arguments, with no registration, serializer invocation, traversal request or movement writer. Same-thread bounded callback context links the manager only while the body runs. Exact caller-return checks guard body/index association. The post-index sample occurs before entity-ID validation; it copies counts and typed fingerprints and retains no live pointer, entity ID or arbitrary path. Only a closed level classification and member index are logged.

Work is bounded to four nested source scopes,65,535 elements per array, a256-slot audit queue and1,024 records. Faulted reads, wrong callers/owners/modes, missing events, exceptions, overflow, drops and writer failures remain explicit. Stable pointer/count rereads do not prove atomic array contents against unrelated host-thread mutation. Complete source/body/index records establish callback-scoped data observation, not the contents of every spatial-index node, streaming lifetime, archive origin or permission to climb.

Build with `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_guide_probe.ps1`. Artifacts and source/payload pins are separate in `.local/native/guide-probe/build-v022/`; the installed Lua/PAK version remains0.0.21. `tools/guide_probe.py` reuses the existing explicit-PID, verified-backup, exact-binary/module and source-receipt checks. No automatic attachment occurs.

Validation passes39 Python tests, three actual owned-process detours with exactly-once original forwarding, C++/SEH propagation, normal host outputs, invalid reads/callers/modes/owners, nested/parallel/overflow scopes, audit bounds, realistic absent-source/save-mode paths, typed-fingerprint known answers, production JSONL conformance and exact three-entry restoration. Independent review found no remaining actionable issue. All10 new source pins, DLL hash and hook/caller bytes are checked against the prepared receipt and original retail file. Final local readiness receipt: `.local/evidence/20261102-native-guide-ready.json`. No retail process has been opened for this new observer.

## Completed retail load baseline

PID34548,2026-11-02: the user loaded the backed-up save and confirmed ordinary control. The capture retains two source calls. Member0 has no ledge body/index callback and remains `not_observed`; member1 has exactly one guarded body/index pair. Counts are4,129 ledges/27,564 markers, with all three FNV fingerprints matching the pinned member above. All four capture records and shutdown counters reconcile: calls[2,1,1], two transactions/ended/reserved/committed, zero dropped/unlinked/overflow, no truncation/writer fault. An independent audit reconstructs the typed source fingerprints without importing the producers.

Stop succeeded and independent process-memory reads verify all three original entry bytes restored. The DLL is pinned/inert until normal exit. Current sources/payload/installed files match their tested pins.49 tested files are preserved under `.local/evidence/v022-guide-tested-baseline`; session `.local/evidence/20261102-v022-guide-runtime-session.json` and independent audit `.local/evidence/v022-guide-runtime-review/` retain exact scope. Capture SHA-256 `196c1fbbc26fa76f2039af1d3600a2a0d8aa54569116d25e3c52fca7b0a384ba`.

This validates the observed normal native source/body/index sequence and copied typed data for this load. It does not establish physical archive provenance, every index node, persistent/streaming lifetime, a newly registered guide, body clearance or higher motion. No unchanged-build load/Jump/geometry repeat is needed. Continue offline from actual byte-acquisition/VFS evidence before creating a mount experiment.

## Completed manual procedure

1. Launch the normal retail game to its main menu and stop before loading a save. Tell the agent it is ready. A fresh237-file backup is independently verified in `.local/evidence/20261102-guide-preflight.json`; recheck it if saves change before testing.
2. The agent attaches only this guide observer to the explicit verified game PID. Load the backed-up Kuttenberg save normally, then report when the world has loaded and ordinary control is available. No Jump, ledge scan, movement sequence or new save is needed.
3. Stop the observer, retain the full capture and independently verify all three original entries restored. The DLL remains pinned/inert until normal game exit.
4. Analyze with `python tools/analyze_guide_probe.py <capture> --output <local-review>`. Require complete accounting and inspect the Kutnohorsko/member1 source/body/index records. Compare counts and full/structural/marker fingerprints with the pinned-member inspector. Any discrepancy needs explanation; it is not permission to change pins or insert a guide.

Attaching before the first save load avoids relying on a same-save reload that may reuse an already resident level. If the producer is absent, record missing coverage and continue from that evidence; do not infer unsupported loading or request unchanged movement tests.

After this baseline, the next work is physical-source attribution and a reversible no-op mounting experiment, then one owned ordinary-range guide with proven handle/lifetime/streaming cleanup. Whole-body route clearance and exclusive host action/interruption precede actuation. HighVault types5/6/7 additionally need a complete native action profile and suitable taller motion; changing a maximum height alone does not supply either. [Higher-climb route](KCD2HigherClimbRoute.md).
