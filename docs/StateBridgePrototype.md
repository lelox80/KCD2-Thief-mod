# State bridge and bounded request prototype — v0.0.11

v0.0.9 passed dry reset/pause/reload and small live wall/control tests. v0.0.10 produced user-confirmed noticeable F8 bursts, then its logger latched movement off. v0.0.11 fixes that logger and passed the repeat key/wall checks plus a final burst after more than six minutes of capture. All 1,022 writes have retained audits and satisfy safety/vertical/request bounds; logger drops/contention/fatal state are zero. One already-unsafe timer rollback refusal is retained as a specifically reviewed publication error. The capture includes pauses. See [current controls and tuning](PrototypeControls.md) and [current status](../STATUS.md). The older build/test chronology below is retained as historical evidence, not the current acceptance result.

The corrected preallocated audit queue reserves each motion record and the session budget before host mutation, commits on normal/exceptional return, and samples stable idle traffic at 1 Hz. Limits are 100,000 records and 2,048 queued slots. Actual exhaustion/disk failure disables movement and reports through Lua only if the script thread/VM remains safe. Lua/API or ownership faults never re-enter the VM. Current native artifacts are in `.local/native/swoop/build-playable`.

The Lua package defaults to observation. Native DLLs attach separately to an explicit PID. `KuttenbergBridge.dll` compiles movement writes out. `KuttenbergSwoop.dll` requires a reviewed receipt matching native sources, retail target and installed package. A receipt is now granted only for the first controlled flat-ground/wall diagnostic; it does not enable motion automatically.

## Implementation

The pinned script-system update hook calls the collector on its observed script thread, using the game's own Lua API. It publishes a bounded ASCII snapshot through one owned C closure. Worker movement callbacks never call Lua, entity methods or file I/O. They copy immutable state under a short lock and require matching current actor/entity/controller identities. [Original-byte ABI and pause evidence](StateBridgeNativeEvidence.md).

The 25 known/safe gates include current module/generation/player, loading/started/paused state, finite positive game frame time, clock continuity, fresh complete XGen state, crouch, physical support, life/resources, incompatible activities, ordinary body/animation, attached camera and clean native-only session provenance. Unknown values refuse the action. The missing retail pause getter uses the statically verified pausable game timer; an existing getter that errors or returns an untyped value does not fall back.

Retail lacks HasBuffDebug despite its listing in older development docs. v0.0.7 excludes the historical SpeedTrial script and buff definition from the runtime PAK. The entry creates a sticky clean-session marker only when no previous module/legacy trial has existed. Reload preserves or poisons permission; a reload over the prior experiment requires process exit. The bridge captures the exact marker object and refuses missing, replaced, wrongly typed or poisoned state. No missing getter is interpreted as buff absence. Historical source/tests are retained.

Snapshots expire after 150 ms; independently stamped XGen observations expire after 250 ms. A repeated snapshot refuses integration rather than integrating its game delta twice. Native cancellation generations and a final snapshot check prevent an old candidate from acquiring permission after invalidation. Runtime update ordering and duplicate frequency remain unverified.

Retail Lua numbers are float32. Protocol 2 retains 19 fields but encodes each gate mask as `b` followed by exactly 25 ASCII bits, gate 1 first. The collector builds strings directly per gate; it never sums a numeric mask. Native code reconstructs the uint32 mask. Integer fields use canonical decimal formatting, with counters capped at 8,388,607 and overflow latched unsafe. The source-pinned separate float32 Lua fixture interpreter reproduces the old rounding failure and emits real packets for the native parser tests; all-safe, contact unsafe/unknown and a low-bit-only refusal remain distinct.

The first motion candidate shapes only the current request's horizontal components. Diagnostic limits are 0.55 s, 1.35 peak gain, a 1.5 host-request-unit total travel budget, at least 0.75 s cooldown and 4.5 maximum horizontal request magnitude. It requires an explicit one-shot serial after a five-second arming delay. Zero input, lost support, unsafe state, stale snapshots, identity/reset/generation changes and long frame gaps cancel without queued reactivation. These values are conservative test tuning, not recovered Thief units or final feel.

Only native-driven Tick advances frame-clock history. Begin samples current identity without advancing that cursor. Mode/reset commands immediately publish an ineligible cancellation packet, with the clock gate unknown/unsafe and the last canonical timestamp. A command-time sample ahead of a subsequent native frame cannot manufacture a rollback; real canonical-frame rollback/gaps still cancel. Bounded logs record the command lead and invalidation reason; native audit includes async time and observer sequence.

The original controller call receives a temporary bounded x/y substitution; the request is restored immediately afterward. z, flags, position, gravity and other request fields are untouched. KCD2 remains the collision and animation owner. The normal-animation gate is a compatibility class, not a recovered native animation-owner pointer; actual ownership and collision behavior still require evidence before accepting live movement.

## Build and first retail test

From `C:\Dev\KCD2 Mod\KCD2-Thief-mod`:

```powershell
./tools/build_native_swoop.ps1
python tools/project.py build
python -m unittest discover -s tests -p 'test_*.py'
```

The native build runs packet, adapter, bounded motion and owned-process detour/stack/identity fixtures, then consumes packets from both double and actual float32 Lua runtimes. It writes a hash receipt for both DLLs and their native/Lua sources under ignored `.local/native/swoop/build-v2`. The Lua package build also requires the separate `.local/tools/lua-5.1.5-float32/lua-float32.exe` and runs bridge/session tests in that model. Its inspected source/build recipe and receipts are alongside it; the existing interpreter is unchanged. Fixtures do not establish engine behavior.

v0.0.9 was installed while the game was closed. The unchanged v0.0.8 folder was archived at `.local/backups/20261018T090223678414Z-uninstalled`. Latest verified save backup: `.local/backups/20261018T090233090353Z`, 237 files.

The first v0.0.6 capture recorded 4,004 publications, 3,075 player requests, one publisher and nine worker threads. All native entity IDs agreed; no publish errors/drops/fatal state or movement changes occurred. Pause yielded 159 logged zero-dt states. The user reported normal movement. Both original hook prologues were verified restored after stop; the user then exited normally. Bit 24 remained unknown and no motion permission was granted. Private summary: `.local/evidence/20261017-state-bridge-first-summary.json` (its bit-24 label describes the old gate).

1. Launch normal retail through Steam and load the existing backed-up test save. Stand on safe ground with weapon put away. Tell the operator the game is loaded.
2. The operator identifies the retail PID and starts **only the bridge profile**:

   ```powershell
   python tools/native_swoop.py start --pid <retail PID>
   ```

3. After the operator confirms successful state publication, open the game console, enter `kt_native_bridge`, then close it. Stand five seconds, crouch five seconds, crouch-walk forward three seconds and stop. Movement should remain ordinary.
4. While crouched, enter `kt_native_swoop`, close the console promptly and hold forward for ten seconds. Repeat once. The five-second arming delay permits closing the console. Keep the game focused until the attempt finishes. The bridge build cannot alter movement; logs record the candidate and final permission checks.
5. On a further dry request, stand up during arming; pause for five seconds and resume; issue `kt_native_reset` and `kt_native_disable`. A canceled serial must never restart a pulse. Reload the same existing save if requested, to check generation invalidation. Keep the initial capture short (roughly one to two minutes); logging is bounded and reaching the cap refuses further action.
6. The operator stops the payload and independently reads both original hook prologues:

   ```powershell
   python tools/native_swoop.py stop --pid <retail PID>
   python tools/native_swoop.py status --pid <retail PID>
   python tools/analyze_state_bridge.py <capture.jsonl> --output .local/evidence/state-bridge-review.json
   python tools/project.py collect
   ```

The stop leaves the owned Lua C closure, DLL and trampolines pinned and inert until normal game exit. Only one initialization attempt per process is supported. Quit normally before another native payload/build test. No automatic DLL unload or off-thread Lua cleanup is attempted.

## Acceptance before a motion test

v0.0.9 passed this checkpoint: 24 consecutive would-apply requests over 0.3791663 s, peak gain 1.34961404, maximum candidate XY 4.18523946 and integrated requested travel 1.49382558. Zero actual writes/drops/fatal state. Reset prevented later candidates, pause refused state and reload reacquired generation 2/identity 2. One already-unsafe observer-counter reset was rejected before the new generation; its raw error counter is preserved. Both prologues were verified restored and the user confirmed normal control. The exact reviewed exception is bound to capture SHA-256 `0f9cf9bc4dfb1471f529c8d2d5761d94cea4e859f05e4c329b4aa07443fb3936`. Actual-motion cancellation remains untested.

Review exact target/package/build identities, one publisher thread, native actor/controller/entity agreement, all-safe states only during supported crouched locomotion, fresh observer timing, no drops/errors/fatal state and zero modified requests. Inspect contiguous `would_apply` spans and repeated-snapshot refusals: isolated nonzero samples do not establish a useful pulse. Correlate manual reset, pause, standing and reload against cancellation/serial baselines and confirm ordinary movement is unchanged. The analyzer only summarizes; it cannot grant live permission.

After those checks, a separately reviewed hash-matching receipt may permit the prototype profile. That build additionally requires `kt_native_enable`, an explicit request and every gate. First live tests must establish measured displacement, ordinary movement recovery and solid wall contact before stairs/slopes/ledge acceptance or a playable-swoop claim. Donor Hermite reconstruction and final input/camera/noise/stamina tuning remain later work.

For this first live diagnostic, use a fresh normal process and the backed-up safe save, then `python tools/native_swoop.py start --profile prototype --pid <PID>`. Motion is compiled but defaults to dry mode. On clear ground, crouch and enter `kt_watch`, `kt_native_enable`, then `kt_native_swoop`; close the console and hold forward ten seconds. The boost should be small and bounded; releasing input cancels. Repeat against an ordinary solid wall and confirm the body remains blocked and normal control returns. `kt_native_disable` cancels. Stop/status require `--profile prototype`; analyze with `--motion`. No binding or save-progress change is required.
