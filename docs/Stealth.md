# Shadows and sound - next playable feature

## Current presentation — v0.0.15

Only the original Thief visibility gem is displayed, just left of the bottom health HUD. Dynamic center/bottom alignment preserves the art's proportions. Noise readings stay in background diagnostics, including `kt_stealth_status`; the user requested removal of the visible gauge. The original graphical route and off/on commands worked in v0.0.14, while its fullscreen stretch failed visual acceptance. A brief corrected-placement check remains after installation. Native movement, guard behavior and real sound production are unchanged.

## Previous presentation — v0.0.14

The raw v0.0.13 readings below now drive a compact original Thief light gem and our four-bar noise gauge near the lower-left. The owned movie/XML and existing canonical collector add no movement or perception writes. Aggregate `vib` uses a chosen[-2,3.5] brightness range and `noi` a[0,1] bar range; this is not a pure light sensor, invisibility promise or instantaneous heard sound. Unknown channels hide, stale updates expire, and graphical failures have an explicit text fallback. [Original-art provenance and build](HudAssets.md).

`kt_stealth_on/off/status` are unchanged; raw five-stat diagnostics remain available. The new graphical route needs one combined retail visual check. The v0.0.13 raw readings and movement baselines remain accepted independently. No F8 enable is needed for the HUD.

The user accepted the swoop and selected shadows and sound on October19. Keep v0.0.12's tested movement implementation and tuning fixed. Additional terrain microtests are not the critical path.

## First feature: host visibility and noise feedback

v0.0.13 adds a small read-only overlay using KCD2's own derived player statistics. The first display uses their raw values because no verified conversion to percentages, lux, decibels or detection probabilities has been established.

| Display / diagnostic | Host getter key | Meaning and boundary |
|---|---|---|
| Visibility | `vib` | Context-modified visibility. May include equipment, illumination, posture and other modifiers; not a pure light sensor or a promise of concealment. |
| Noisiness | `noi` | RPG noise value modified by equipment/posture/activity; not an instantaneous sound waveform or hearing radius. |
| Footsteps x | `fsm` | Host footstep-sound multiplier; surface events and listener perception remain separate. |
| Equipment visibility, diagnostic only | `evi` | Equipment-related visibility, retained to distinguish equipment from changing context. |
| Conspicuousness, diagnostic only | `con` | Separate host conspicuousness value. |

The source is `player.soul:GetDerivedStat(key)`, documented by the installed bindings and now observed returning valid values for all five keys in retail. The player confirmed the readouts display and change, including lower signed visibility at night. Unknown, missing, thrown or nonfinite values remain unknown; never fabricate darkness or silence. [Runtime results](StealthResults.md).

The overlay uses the shipped `System.DrawText(x,y,text)` caller subset, refreshed from the existing script-thread bridge after packet publication. Reads are cached at about10Hz; drawing follows native script updates. This adds no movement hook, timer, persistent entity, gameplay modifier, perception stimulus or change to the movement packet. HUD failure must remain isolated from movement publication. Attachment remains explicit, and native movement starts dry; the HUD does not need movement enabled.

## Why the host signals are useful

Inspected retail `Tables.pak/Libs/Tables/rpg/buff.xml` contains `Cpp:ActorIlluminanceMeter`, with seven modifier groups affecting `vib`. Retail `system.cfg` supplies six matching illumination cut points. Crouch, torch, sprint, weather/interior and equipment modifiers contribute separately. Configuration establishes a real illumination-to-visibility mechanism, while live activation, occlusion response and update behavior still need observation. Do not add another copy of the existing buff.

The active meter group is not exposed by a verified Lua getter. Subtracting equipment visibility from overall visibility would still mix posture, torch and other modifiers, so it cannot recover a pure seven-step light value. A calibrated light gem can follow once a direct signal or a defensible mapping exists.

KCD2's RPG sound channels and XGen perception trees already handle movement, doors, distracting sounds, hearing, investigation and crime contexts. Preserve that pipeline. The shipped `Crime.ProduceAiSoundOnDudePosition` helper is a future distraction interface, not something the feedback layer should call.

## Thief reference and later behavior

Thief2014's [official game help](https://dlassets-ssl.xboxlive.com/public/content/9a187675-fd56-4698-878c-16dfb9fbbfad/GameManual/b44edb2d-29be-40aa-8108-b05b840c3366/en-AU/index.html) describes the light gem as shadow feedback associated with reduced chance of being spotted. It does not promise invisibility.

Targeted inspection of the pinned donor finds body/world light sampling at about0.115-second cadence and separate movement/surface sound-event families. Slow stealth walk/crouch suppresses some noisy-surface variants; swoop uses the crouch sound family. These support later creeping and surface-noise work. They do not justify importing donor distance numbers into KCD2 or replacing guard awareness wholesale.

After useful real feedback is running, prioritize deliberately slow creeping and surface-dependent sound, then readable suspicion/search feedback and environmental distractions. Each addition should change or expose a real host mechanism and preserve NPC/crime/save continuity.

## Validation and evidence

Automated checks cover missing/error/nonfinite readings, lifecycle/cache invalidation, renderer failure and unchanged bridge publication. Existing movement regression fixtures remain the checks for the frozen writer. No dedicated wall/stair sequence is required for this read-only addition.

The double/float32 HUD suites, existing native/wire fixtures and17 Python checks passed on October20. On October21 all237 current save files still matched that verified backup, and the combined retail check passed for visible changing feedback. Individual light-only calibration, heard-sound behavior and new perception changes remain open. The completed test ended with both hooks restored; see [StealthResults](StealthResults.md).

## Controls

Attach the reviewed explicit-PID `prototype` build using the [existing loader procedure](PrototypeControls.md). It starts dry. The HUD is enabled by default and does not require `kt_native_enable`. It uses the existing native script-frame callback, so the PAK alone cannot refresh it before attachment.

- `kt_stealth_off`: hide only the feedback.
- `kt_stealth_on`: show feedback again and retry a failed renderer.
- `kt_stealth_status`: log the current finite values or typed unknown status; expired values are cleared.

Existing F8/swoop commands are unchanged. No new key binding is installed. Stop leaves the owned DLL inert until normal exit; a fresh process is required for another native session.

One combined manual session is sufficient for the first checkpoint: keep clothing fixed, move between shade and light, toggle crouch and torch, and compare stationary/walking/sprinting readings with the inventory stats. Report display presence/readability and whether the values respond sensibly. Collect the bounded logs locally. Do not require repeated game restarts for individual getter trials.

Original research summaries and extracted proprietary inputs stay in `.local/research/20261019-stealth/`: `host-findings.md`, `ui-findings.md` and `donor-findings.md`. Written/built/installed/runtime evidence is recorded separately in STATUS and MODLOG.
