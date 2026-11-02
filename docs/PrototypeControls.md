# v0.0.15 prototype controls and compact stealth feedback

v0.0.15 shows only the original visibility gem beside the bottom health HUD, with uniform scaling. Noise remains in `kt_stealth_status` background diagnostics; no visible noise meter or noise text is drawn. Commands and dry startup are unchanged. The v0.0.14 renderer/control check passed functionally but its stretched layout was rejected; only a brief proportional-placement check remains after installing the correction.

v0.0.14 replaces the default numeric readout with original Thief gem art and our noise bars. Attach the reviewed build as below; it starts dry and feedback does not require movement enabled. `kt_stealth_off`, `kt_stealth_on` and `kt_stealth_status` retain their meanings. A raw text fallback explicitly reports graphical failure. The new movie awaits one combined visual check; [provenance and limits](HudAssets.md).

v0.0.13 preserves the tested movement writer and adds a passive visibility/noisiness/footstep HUD. It starts enabled after native attachment, independently of movement enable. Use `kt_stealth_off`, `kt_stealth_on` or `kt_stealth_status`; see [Stealth](Stealth.md). The October21 retail check confirmed visible changing readings, including lower signed visibility at night. Both test hooks are restored; a fresh process is required for another native session. [Results and limitations](StealthResults.md). No additional wall/stair test is required for this read-only addition.

v0.0.10 produced immediate, noticeable bursts with normal initial control, then the logger latched movement off. Its final held-key/no-queue/wall sequence is inconclusive. v0.0.11 corrects the logger and passed the retail retest: immediate/spaced bursts, held-key/no-queue/release checks, wall blocking and final responsiveness after more than six minutes of capture. All 1,022 writes are audited and within the safety/request bounds; logger drops/contention/fatal counts are zero. The capture includes pauses and one reviewed already-unsafe clock rollback refusal. Full Thief fidelity, broader traversal and stealth acceptance remain open.

v0.0.12 adds passive terrain diagnostics with unchanged native movement. Paired ordinary/live tests passed on the tested gentle slope, short stairs and low step; the player confirmed smooth boosts and normal landing/fresh-press recovery. All 1,011 applied writes match the complete audit, and both hooks in the test process PID33064 are restored. The owned DLL remains pinned/inert until normal exit. [Results and retained warnings](TerrainResults.md). New terrain still follows [TerrainValidation](TerrainValidation.md); attachment remains explicit and starts dry.

The user chose **F8** as unused. Startup does not bind keys or enable native movement automatically. Load the existing backed-up safe save in normal retail. In PowerShell, identify its PID and attach the reviewed `prototype` profile, replacing `<PID>` with that number:

```powershell
Set-Location 'C:\Dev\KCD2 Mod\KCD2-Thief-mod'
Get-Process KingdomCome | Select-Object Id,StartTime
python tools/native_swoop.py start --profile prototype --pid <PID>
```

Enter these commands one at a time in the game console:

```text
kt_watch
Bind f8 kt_swoop
kt_native_enable
```

Close the console. On clear ground with weapon put away, crouch and start walking forward, then **tap F8**. It should activate immediately on the next native frame and finish in about a second. The nominal envelope is 0.9 seconds, with final host-step quantization. Release forward to cancel. `kt_native_disable` cancels and disables; `kt_native_bridge` returns to observation. `kt_native_swoop` remains a separate five-second delayed diagnostic; it is not the F8 command.

Tuning is up to 0.90 seconds, 2x peak horizontal gain, 4.0 total requested travel, one-second cooldown and a 6.25 absolute horizontal request cap. At the observed 3.101 base speed, fixtures end around 0.833 seconds due the travel budget. These are host prototype units/tuning; no original Thief scale or exact achieved world travel is implied. Camera, animation, gravity and collision remain with KCD2.

Test in this order:

1. One moving-crouch tap: recognizable boost, smooth end and normal control.
2. Three taps separated by at least two seconds: independent bursts, no stacking.
3. Hold F8 for three seconds while moving: one burst, not automatic repeated activation.
4. Tap while stationary, then begin walking without pressing again: no queued burst. Tap while standing: refusal.
5. Release forward during a burst: cancellation and ordinary control. Repeat facing an ordinary solid wall: body stays blocked, no shaking/penetration, then step away normally.
6. Continue ordinary play on safe ground for at least six minutes, then repeat one moving F8 tap and confirm it still responds. The operator checks that the native publisher remains fresh. This directly repeats the duration that exposed the logger failure.
7. Enter `kt_native_disable` before returning results. The operator stops/status-checks the `prototype` profile and reviews the capture with `tools/analyze_state_bridge.py --motion`.

The retail `Bind` registration at RVA `0xd8de24` calls handler `0x2475bfc`, requiring a key plus command. `Bind key` is not a query. The map insertion does not replace an existing console binding. Input dispatch `0xb93018` only executes this path for event state 1; the held-key behavior passed the v0.0.11 retail test above. No verified Unbind command was found, and binding persistence/removal has not been tested; do not invent an undo command. Our installer changes no profile or input XML. The user explicitly selected this unused key.

Native attachment is per process. Stop leaves the owned closure/DLL pinned and inert until normal exit; use a fresh process before changing payloads or starting another native session. The `.local/evidence/state-bridge-validation.json` receipt is bound to the current source/package and reviewed retail evidence. Every movement write requires a reserved audit record; review checks retained records against the applied-write counter. Idle records are sampled, and the corrected session is bounded at 100,000 records; queue exhaustion, capture limit or disk failure disable movement. A safe logger fault reports `native_fault movement_disabled=true restart_required=true`; enabling again cannot repair a failed native session. The corrected logger, controls and scoped slope/stair/low-step checks passed. Narrow passages/broader uneven terrain remain next; current support-loss refusal remains enabled.
