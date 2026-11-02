# Native movement capture: retail test procedure

The probe is built, fixture-tested and passed an initial **read-only retail capture**. It observes the final local-player movement request and always forwards it unchanged. There is no swoop key or native movement write in this build. The existing Lua mod remains v0.0.5.

## Verified result, 2026-10-17

3,673 player requests over 69.813 seconds, nine worker threads, zero dropped/unreadable requests. The user performed stand/walk/crouch/pause and reported normal movement unchanged. Explicit stop succeeded. `status` independently confirmed the original request prologue restored; our DLL remains pinned until normal exit. [Detailed findings](KCD2MovementInterface.md). This is observation evidence, not a playable swoop or collision-control pass.

Build from the repository's PowerShell prompt:

```powershell
.\tools\build_native_probe.ps1
```

This requires the inspected Visual Studio 18/MSVC toolchain and private pinned MinHook 1.3.4 archive. The build re-extracts that verified archive, compiles our DLL plus two owned test executables, runs the fixtures, includes MinHook's license and writes a DLL hash receipt. Build output is `.local/native/probe/build/`.

## First retail capture

1. Close KCD2 and run `python tools/project.py backup`. The latest backup from this investigation is `.local/backups/20261017T205853077530Z`, 237 files, archive contents verified against their SHA-256 manifest.
2. Launch normal retail through Steam and load an existing backed-up Kuttenberg save. Keep normal KCD2 movement and collision active.
3. In a separate PowerShell prompt in this repository, identify the explicit PID:

   ```powershell
   Get-Process KingdomCome | Select-Object Id,ProcessName
   python tools/native_probe.py start --pid 12345
   ```

   Replace `12345` with the actual PID. The loader refuses a different executable, module path, retail hash or owned DLL hash. It requires a verified backup receipt. It may require a shell with the same privilege level as the game. A refusal is not a successful attachment.

4. Stand still for five seconds. Walk forward, sideways and backwards on safe open ground. Stop, toggle crouch, crouch-walk briefly, then pause/resume. The capture is observational; do not invoke the old speed trial in this first capture. Note the order and approximate duration of these phases.
5. Stop while the game is still open:

   ```powershell
   python tools/native_probe.py stop --pid 12345
   python tools/native_probe.py status --pid 12345
   ```

6. Inspect the new `movement-probe-<PID>-<ticks>.jsonl` under `.local/native/probe/build/`:

   ```powershell
   python tools/analyze_native_probe.py '.local/native/probe/build/movement-probe-12345-EXACT-TICKS.jsonl'
   ```

7. Exit KCD2 normally. Our DLL and trampoline remain pinned until exit even after stop; do not rebuild/reload a new DLL during that process. There is one initialization attempt per process. No automatic unload is attempted.

Successful first capture requires a `started` header, nonzero `player_request` records correlated with ordinary input, `stopped` counters, zero unreadable requests, and ordinary movement unaffected. Count observed threads and establish cadence before choosing any action clock. A sample limit of 12,000 bounds capture; reaching it stops recording, while the forwarding detour remains until explicit stop or exit. Missing records or a failed stop must be investigated before enabling motion.

The hook temporarily changes instructions **in memory** through MinHook; original executable files are not edited. It logs flags, speed pair, delta vector, high-resolution wall-clock ticks and numeric thread ID. It keeps no actor/request pointers after a call and makes no Lua calls on actor threads. The captured clock is diagnostic wall time, not a proven simulation clock. No grounded, stance, collision or animation ownership inference is made from this log alone.

## Next control gate

After a valid capture, integrate a fresh player-generation/state snapshot on the proper host thread, prove controller ownership and delta semantics, and wire the reconstructed trajectory into one bounded opt-in request. Only then test actual displacement, wall contact, normal-control recovery, stand-up/pause/load cancellation, stairs/slopes and ledges. The offline trajectory fixtures do not pass those retail acceptance gates.

The Computer Use JavaScript helper could not initialize its kernel assets after retries/reset. The user supplied the manual launch/load and test inputs; the agent performed the explicit attachment, capture inspection, stop and read-only instruction-restoration check. Automated desktop control remains unavailable in this session.
