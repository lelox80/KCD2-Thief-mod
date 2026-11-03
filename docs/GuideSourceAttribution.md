# Guide member archive attribution

## Retail result and staged mount experiment

The mount test passed on November 3 in PID 15484. The qualified member 1 lookup returned `owned_mod_pak` for `Levels/kutnohorsko/whdata_1`. The test PAK was the sole owned package containing the member, and the installed member matched the shipped bytes after the test. The user confirmed normal control after loading. Stop and independent restoration of both hook entries passed. The baseline was restored to its original hashes after normal exit. Runtime session `.local/evidence/20261103-v024-guide-noop-runtime-session.json` and rollback receipt `.local/evidence/20261103-v024-guide-noop-rollback.json`.

On November 3, PID 30544 produced one qualified member 1 attribution. The logical path was `Levels/kutnohorsko/whdata_1` and the archive class was `base_level_pak`. The user confirmed normal control after loading. Stop succeeded and both original hook entries were independently verified restored. The 40,698 lookup calls reconcile as 3,702 within member 0, one within member 1 and 36,995 outside a source scope. The member 0 scope was ambiguous. The capture and review are pinned in `.local/evidence/20261103-v023-source-runtime-session.json`.

`python tools/project.py build --guide-noop` stages a separate PAK at `build/guide-noop/kuttenberg_thief/Data/kuttenberg_thief_guide_noop.pak`. The sole member is the complete 7,589,941-byte shipped member at the observed logical path. Source SHA-256 `dfd631becc308ee4be1d460fbb0ba3c600582f8776e1e1bb8fa8de4a6ab1162c`. Staged PAK SHA-256 `aca008e95e7b97c33c2579bf700d987d909bfd0bdb2c276faddee24064b15d22`. The staged manifest and main PAK match the current installed hashes. The deployment tests cover profile switching, conflict refusal and rollback.

1. Close the game. Verify the owned installation, archive it with `python tools/project.py uninstall`, and install with `python tools/project.py install --guide-noop`. Installation requires a verified fresh backup and writes a receipt for all three files.
2. At the main menu, attach the source observer and load the backed-up save normally. Require normal control and a qualified member 1 attribution to `owned_mod_pak`. A `base_level_pak` result means the base source won for that read. Treat a missing qualified observation as inconclusive.
3. Stop the observer and verify both original entries. After a normal exit, roll back with `python tools/project.py uninstall` and `python tools/project.py install`. Compare the restored install to the baseline hashes. An owned-archive attribution plus normal control validates this byte-identical mount. Registry lifetime and behavior with an appended record remain separate tests.

2026-11-02. The normal guide source/body/index baseline passed in retail:4,129 ledges,27,564 markers and all three typed fingerprints match shipped whdata_1. The stopped three-hook observer and49 tested files are preserved. The next step toward higher climbable walls is identifying the actual archive selected for that member before a reversible no-op mount and an owned guide. No mount or guide is changed by this investigation.

## Existing host lookup

Pinned retail WHGame.dll SHA-256 remains `bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7`. The selected normal XML file parser calls the existing file archive-name lookup after a full read and before closing the file.

| Boundary | Original-byte evidence |
| --- | --- |
| XML parser | RVA0x463d38; existing provenance lookup call0x464058, return0x46405d; file close follows at0x464087. |
| File wrapper | Lookup0xf41730 takes RCX file pointer and returns the original const-char pointer in RAX. File logical filename starts at+0 with260-byte capacity; handle+0x108 and provider+0x110. |
| Provider | C_CryPak primary vtable0x3a97328, slot+0x248 resolves to0xf41774. It indexes the open-file record by handle and obtains the backing cache's appended name. |
| Name semantics | The same cache+0x9a8+[cache+0x70] string is compared against requested pak names and passed to the original `Closing pak` log in0x4d4a90. Factory0x4d5dd4 copies the complete caller name into that storage. |

This is an existing host provenance call, not an additional lookup invoked by the observer. Empty names remain unknown: null/invalid handles, unsupported providers and a cache-factory flag can all yield no name. A resource-provider route can bypass this file lookup entirely. None of those cases proves loose-file access.

Selected original instruction reports, exact file offsets/callsite pins and24 report hashes are in `.local/evidence/v023-source-trace/`. This evidence establishes the lookup contract and pak-name meaning, not which runtime branch or archive is selected.

## Separate source-attribution probe

The new probe observes the normal C_Game source producer and existing archive-name lookup. Same-thread, callback-scoped context copies the expected `<folder>/whdata_<index>` name. Before interpreting the returned name, require the exact XML-parser return site, exact logical filename agreement, nonzero file handle, expected provider vtable and original provider implementation. Recheck the file identity after the original lookup. Forward the original arguments, returned pointer and exceptions unchanged.

Only closed namespace/archive classifications are logged. Unknown, mismatched, unreadable, truncated, empty, ambiguous or skipped observations remain unknown. Arbitrary paths, handles, live pointers and file contents are not retained. A copied pak filename is source-attribution evidence for that read; it is not a cryptographic archive-content measurement or permission to register guides.

The previously tested GuideProbe is preserved. The new DLL and build receipt are isolated in `.local/native/guide-source-probe/build-v023/`. It performs no mounting, registration, parameter change, traversal request or movement write. Its first retail use remains a manual dependency until recorded otherwise in STATUS.

## Manual boundary

The probe passed 44 Python tests, two native detour fixtures, and validation of the emitted capture. The receipt records the original retail byte checks and pinned DLL and source hashes. The closed-game backup verified 237 saves. [Status](../STATUS.md).

1. Start a fresh retail process and wait at the main menu before loading a save.
2. Attach the source-attribution observer to the explicit verified PID. Load the backed-up Kuttenberg save normally and report when normal control is available.
3. Stop and independently verify both original hook entries restored. Retain the complete capture and compare its source/member/file association, namespace classification, archive classification and shutdown accounting. Treat missing provenance as inconclusive.

This is a new source-attribution measurement. The earlier data/index baseline needs no unchanged repeat. A fresh process avoids relying on a resident XML/resource cache that may omit the file read. No override is installed for this baseline.

After actual namespace and archive selection are known, prepare a byte-identical no-op package with an attributable owned source. Prove selection, load behavior and rollback before appending a guide. Base-handle ordering, streaming/save lifetime, whole-body clearance and exclusive native action ownership remain separate requirements. Higher actions additionally need suitable taller motion and complete native configuration; increased height limits alone remain insufficient.
