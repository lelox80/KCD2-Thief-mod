# Player-state observer experiment (v0.0.4)

Purpose: obtain KCD2's actual actor-state checks without native code. Retail Lua's AI.GetStance remained unchanged and legacy actor stats were missing. The shipped `AI/npc/basic/switch/handleAwareness_trespass.xml` and `handleStimulusSound.xml` both use `ActorStateCheck Who="$__player" State="crouch"`. `ExecuteLua` and timed `While` branches also occur in shipped player behavior.

The package builder reads `AI/player/switch/switch.xml` directly from the installed Scripts.pak and requires SHA-256 `fd115777802e2a2ca1a7b0d3b11f1c2656c16e0c44d9bcc83377e01014d51d2a`. It appends one observation branch to the existing root Parallel. All original bytes and original child order are preserved. Removing our inserted bytes exactly recreates the original asset. Installed game assets never enter tracked source: only the generator is tracked; the generated override stays in the ignored build/package.

The observer checks crouch, inAir, cutscene, interactiveScene, dialogue, horse, minigame, actorAnim and onBed. Both branches of each check explicitly send true/false to Lua; a commit accepts only a complete batch. Lua errors are protected with pcall; a suppressed-failure sequence is followed by a 100 ms wait to prevent a retry loop with no delay. State logs occur only on changes and are capped at 300 lines per gameplay generation. No controller, physics, camera, animation, perception or inventory setter is invoked.

Tradeoff: this is a behavior-tree override, so it can conflict with another mod of the same asset. The installer rejects detected loose-file or PAK conflicts, including uninspectable mod archives. It is less invasive than a native loader but still changes the runtime tree structure. Its scheduling, existing-save loading and removal must be tested. Do not use a new save created during this experiment as the only copy of progress; the pre-test save backup is the baseline. No claim yet that tree state serialization/removal is compatible with newly saved games. The existing editor metadata is preserved; this generated asset is not intended for manual editing in Skald.

Build checks: source hash, unique insertion location, XML parsing, original branch equivalence and complete removal round trip; Lua incomplete-batch rejection and load reset; installer conflict fixture. Runtime proof required: `actor_observer crouch=false`, then true when crouching, then false standing; fresh timestamps, pause/resume and same-save reload. Missing/stale observation remains unknown. This observation is not itself a movement-control test.

If the retail game does not activate this branch or the bridge fails, inspect its specific load/parse error. Do not patch arbitrary NPC or quest trees to get a reading. A minimal native state adapter remains the fallback.

## Retail result, reviewed 2026-10-12

v0.0.4 produced 47 complete change observations, including two true/false crouch cycles, inAir, dialogue/interactiveScene, horse, onBed, minigame and actorAnim. Capture: `.local/logs/20261012T190752390468Z-probe.log`. No relevant error matched the filter. This establishes a working state bridge; it does not verify every state, absence predicate or save serialization. Only one gameplay generation occurs in this capture. Timestamps continued after long interruptions; actual cancellation was not exercised because v0.0.4 had no action.
