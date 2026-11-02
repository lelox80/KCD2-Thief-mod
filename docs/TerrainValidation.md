# Slopes, stairs and low-step validation — v0.0.12

v0.0.11 passed controlled clear-ground, F8 and ordinary wall tests. v0.0.12 adds passive terrain observations and analysis; its native movement code, protocol and tuning are unchanged. The October19 paired slope/stair/low-step trials passed on the tested routes, with the diagnostic and clock-refusal limitations retained in [TerrainResults](TerrainResults.md). This remains the repeatable procedure for new terrain.

The action scales an existing horizontal request while KCD2 resolves the world. It cancels when either physical `IsFlying` or XGen `inAir` stops confirming support. This is not edge prediction, braking, a ledge grab or gap crossing. Previously accepted momentum and ordinary held movement remain with KCD2. Zero request Z does not establish grounded state, slope clearance or world vertical velocity.

## Capture and controls

Use a fresh verified save backup and fresh process. Attach the reviewed explicit-PID `prototype` build as described in [PrototypeControls](PrototypeControls.md); it starts dry. The passive module is inactive until a phase command. These commands only label and record observations:

| Command | Recorded phase |
|---|---|
| `kt_terrain_slope` | Gentle slope up, down and across |
| `kt_terrain_stairs` | Short ordinary stair flight up and down |
| `kt_terrain_ledge` | One low step or curb with a clear landing |
| `kt_terrain_stop` | Finish the current diagnostic phase |

Each phase has at most 90 seconds, 900 position/velocity samples at about 10 Hz and 2,048 events including stop. There are at most 32 sessions per process. `kt_watch` restarts diagnostics, so run it before starting a phase. Stop an active phase before starting another. Automatic truncation or a lifecycle stop is explicit and requires reviewing or repeating that trial.

The trace runs after native publication on the script thread. It reacquires current ownership, labels missing/error/nonfinite vectors unknown, and never contributes safety evidence or writes movement. Logs include epoch/generation/identity, canonical frame, observer and request serials, reset, mode and exact ASCII masks. Reset/gap events preserve diagnostic discontinuities; module, player, identity, load and clock rollback end the trace. Cached stop kinematics are distinguished from a current canonical boundary; an invalid boundary makes native association incomplete.

## First in-game checkpoint: ordinary slope traversal

With the attached build still in dry mode, locate a gentle nearby slope before starting the capture. Put the weapon away. Enter these console commands one at a time:

```text
kt_watch
kt_native_bridge
kt_terrain_slope
```

Close the console and crouch-walk normally up, down and across the slope for about 20–30 seconds, without F8. Enter `kt_terrain_stop`. Report whether ordinary control remained normal. Review actual position/velocity getter status, packet joins, native identity and contact transitions before enabling the terrain pulse. An in-progress native capture is only a preliminary review; complete shutdown is required for final acceptance.

## Controlled live sequence after ordinary capture review

Enable only after the first checkpoint is reviewed. The existing user-selected F8 binding remains `Bind f8 kt_swoop`; `kt_native_enable` opts into the unchanged bounded writer.

| Terrain | Paired trial | Observe |
|---|---|---|
| Gentle slope | Normal crouch-walk, then one moving F8 tap uphill, downhill and across | Snagging, unexpected vertical launch, camera jerk, control and conservative refusals |
| Short stairs | Normal up/down, then one moving F8 tap each direction | Step continuity, wall/rail blocking, support-loss cancellation and control afterward |
| Low step/curb | Normal descent onto clear ground, then one moving F8 tap approaching the same step | No added scaling after support becomes unsafe; normal gravity and landing; no resumed old burst while forward remains held |
| Landing recovery | After the low-step trial, keep moving without another F8 press, then make a fresh grounded tap after cooldown | Old request stays consumed; only the fresh press may activate |

Use separate phase windows and stop each with `kt_terrain_stop`. The ledge phase is confined to the low step just traversed ordinarily. A high drop, jump across a gap or hanging/climbing action would require a different implementation and evidence. If the current guard cancels on a stair or uneven contact, record that result before considering any change to the guard.

Finish with `kt_native_disable`, stop the native hooks and independently verify both original prologues. Review the complete captures:

```powershell
python tools/project.py collect
python tools/analyze_terrain.py <collected-probe.log> <state-bridge.jsonl> --output .local/evidence/terrain-review.json
```

The analyzer rejects mismatched packet joins and withholds native association counts when coverage is incomplete. It checks native safety/vertical/travel bounds and writes after consumed/refused serials, including across resets. Raw publication errors remain visible and require an exact review; earlier clock-refusal exceptions do not waive future errors.

Phase names are operator labels. Ten-Hz position chords are not exact pulse distance, collision penetration, ground clearance or physical edge-to-contact-signal latency. Stop boundaries retain up to one frame of command ordering ambiguity. Written/built diagnostics and owned-input fixtures do not establish terrain acceptance; native evidence and the player's observations must agree.
