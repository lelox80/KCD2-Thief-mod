# Capability assessment — 2026-10-11

## Decision

Start with an **official retail PAK/Lua mod**. The first concrete prototype is a read-only API/player-state probe, already built. Prove runtime loading and state reads before adding any motion. Keep KCD2's character controller as owner of collision/gravity/ground response. Prefer a supported movement request/input path if exposed, rather than a parallel position writer.

**Official scripting is sufficient for the diagnostic design. It is not yet proven sufficient for swoop.** A native adapter becomes necessary if retail does not expose a usable movement request, reliable state/lifecycle gates or the required physical queries. Do not equate an exported getter/setter name with working player control. No game bridge or engine reconstruction offers a demonstrated advantage here.

Evidence labels: **D** official documentation; **I** inspected installed data; **S** inspected third-party implementation; **R** verified in our running game. Initial R results are summarized below. See [Sources](Sources.md) for pins and exact files.

**First runtime result:** official entry loads in normal retail 1.5.6 without -devmode. 120 samples confirm position/orientation, velocity/speed, camera, animation state, physicalization profile, mounted/ladder/dialogue/weapon/death/KO/combat/loading getters. `GetStance`, the tested pause getters and `EnableActionFilter` are absent. MotionIdle/MotionMovement/AnimationControlled observed; no labelled crouch test yet. SetVelocity, SimulateOnAction, GetPhysicalStats and ray functions are present but their execution/semantics are not verified. This supersedes the preflight unknowns in the table only for those explicit reads.

**Second runtime result:** 240 samples total. `kt_watch` and `kt_rays` execute through the normal console. `Physics.RayWorldIntersection` returned one wall-facing hit at 2.2592 m with position and normal. Camera height changes during the requested crouch test, but animation names remain MotionIdle/MotionMovement. `AI.GetStance(player.id)` is an independently documented candidate, added to v0.0.2 for testing. Neither camera height nor the table enum alone is a proven crouch gate.

**Additional lower-impact movement lead:** strings `pl_movement.speedScale`, `pl_movement.crouch_SpeedScale`, and `wh_actor_MovementSpeedMult*` exist in installed WHGame.dll. A temporary locomotion multiplier could leave collision fully with KCD2, but registration, write access, actual effect, player-only scope and restoration are not established. v0.0.2 only reads these CVars. Native-binary string presence is not evidence that a variable is active or has the assumed meaning.

| Capability | Evidence and candidate interface | Retail 1.5.6 | Development 1.5.5 / remaining question |
|---|---|---|---|
| Mod initialization | D: `Scripts/Mods/<modid>.lua`, uncompressed PAK, `mod.manifest` | R: package loads and executes in normal retail | Loose-file loading documented only for development |
| Input/key bindings | I: `Player:OnAction(action,activation,value)`; D: `actor:SimulateOnAction`, `ActionMapManager`, `Game.KeybindsRebindInput`; XML keybind patches in source example | Actual delivered actions, mode constants, remapping, focus and ownership unverified | Debug build does not prove retail input support. M0 only registers console diagnostics |
| Position/orientation/velocity read | D: entity `GetWorldPos`, `GetWorldAngles`, `GetVelocity`, `GetSpeed` | R: meaningful values during movement and standstill | Community reports differing retail/dev behavior; not accepted as our verification |
| Crouched state | I: `stance.xml` maps **1=crouch, 3=normal**; local NPC script agrees | Authoritative player runtime accessor still unknown. `GetStance` is a presence/read probe, not assumed API | Community document's 2=crouch claim conflicts with this table; could be a different enum or error. Never map by guessing |
| Movement control | D: entity `SetVelocity`, `AddImpulse`; actor simulated action and movement restrictions | No motion API called yet. SetVelocity may be overwritten by actor locomotion or bypass desired behavior | Next experiment must establish correct frame/physics timing and arbitration |
| Collision/ground/slopes/stairs/falling | I: living physical parameters in NPC scripts and player slopeSlowdown; native controller exists | No authoritative grounded/normal/contact accessor established. No collision claim | Native living-entity status/request path is fallback; validate body volume and stairs, not one ray |
| Raycasts | D: `Physics.RayWorldIntersection(pos,dir,maxHits,mask,skip...)`; S: native loader inspector performs rays | Manual `kt_rays` provided; return schema/result not yet verified | Three relevant loader signatures match retail once each, zero in development raw DLL; ABI remains unverified |
| Shape sweep/overlap | D: `SamplePhysEnvironment` samples sphere environment; no capsule sweep found in inspected Lua surface | Unsupported until actual full-body query/controller path is identified | Native physics sweep may be required. Sphere sampling is not a swept capsule |
| Camera read/write | D: System view camera getters/FOV setter; actor view-angle, shake and view limits; Action camera functions | Read probes only. Smooth local offset, collision and restoration unverified | Lean needs both body/camera clearance and perception-origin investigation |
| Animation/root motion | I: Player uses Mannequin controller/ADB; D: `SetMovementControlledByAnimation`, `GetCurrentAnimationState`, resource-hosted animation actions | No forced animation; read-state candidate only | Documented animation control does not establish safe player root motion or asset compatibility |
| Update and timing | D: Script timers; entity `OnUpdate(dt)` and System times; S: loader hooks `CryScriptSystem::Update` | M0 timer sampling only; not suitable evidence for movement timing | Native update/game-thread ordering and pause behavior need validation |
| Pause/load/save/gameplay transitions | D: Game loading query; S: UIAction OnGameplayStarted listener; runtime query candidates | Reacquire player on every sample; bounded capture; lifecycle coverage unverified | Do not retain native pointers across loads or silently resume an action |
| Combat/dialogue/horse/interaction | D: human IsMounted/IsOnLadder/IsInDialog/IsPickpocketing/IsWeaponDrawn, actor death/KO/corpse; soul combat query candidate | Probe records supported reads; missing values remain unknown | Unknown incompatible state means movement disabled, not presumed safe |
| Noise/stamina/fall damage/guard vision | I: footstep/foley properties, RPG movement/fall/visibility parameters and AI XML | Unchanged by M0; custom movement integration unverified | Need paired observations and targeted perception investigation before stealth changes |

## Why these sources do not prove runtime support

- The installed Lua binding docs identify themselves as `script_bind_2025_01_14`. They contain generic CryEngine and Warhorse entries; some obsolete, debug-only or missing at runtime.
- The official tools explicitly include debug commands/functions not in the published build. Installed tools are a different revision and lack `Data/Scripts.pak` until workspace setup copies/links retail data. They are not currently a runnable-equivalence baseline.
- KCD2ModLoader has concrete hooks, Lua VM integration, entity inspector/raycast and noclip code. Its d3d12 proxy, native layouts/vtables, signatures and update hook require verification against this build. A matched byte sequence is only a locating hint. Noclip is explicitly outside swoop acceptance.
- `universal-modder` implements scanning and workflow helpers; its Unreal engine recipe is explicitly UE4/5. Its CryEngine scanner looks for CrySystem.dll, whereas this install's main engine/game module is WHGame.dll. Generic classification cannot replace inspection.
- `ai-game-modding-guides` is methodology, not a KCD2 movement implementation. Its UE4SS discussion cannot establish UE3 compatibility.
- REA provides analysis infrastructure, not recovered original source. Installed Node 22.18.0 is below current REA's supported 22.19 minimum. No REA installation or whole-game analysis was performed.

## Next decision gates and fallback

1. Read fresh M0 retail log. Confirm official entry loads and position/velocity/state calls return meaningful values. Check crouch empirically across stand/crouch transitions.
2. Investigate supported native locomotion request/input scaling or game speed modifier. Do a short, bounded movement experiment only after required gates are known. A setter must preserve collision and hand control back cleanly; no transform teleport fallback.
3. If supported Lua cannot do this, build a minimal build-pinned native adapter exposing state, queries and a single movement request to Lua. Validate only relevant native functions with targeted disassembly/REA/Ghidra as needed. Require exact image hashes, unique signature plus semantic validation, game-thread execution and fail-closed behavior.
4. Use the development build for richer inspection only after resolving data setup and version mismatch. Never silently change the project's retail target or use development-only success as retail evidence.
5. Implement swoop after gate 2/3 passes; then mantle/vault, lean, selected ledge traversal, presentation and Thief-style stealth.

## Existing movement mods

Movement Speed Modifiers for NPC Henry and Terrain and Realistic Movement Speed describe table-level speed changes; Walk With Wheel describes gradual locomotion speed control. Their descriptions are leads, not tested compatibility. Installed `soul_archetype_movement.xml` independently confirms speed ranges and animation tags exist. No inspected mod establishes collision-aware Thief swooping, general mantling or whole stealth replacement. Avoid assuming an NPC table change can control only Henry during a timed action.

## Kuttenberg traversal

The retail region is `Data/Levels/kutnohorsko`; tools contain editable level/layer sources and roof-related export profiles. Shipped `LedgeObject.lua` includes legacy Crytek-style vault fields (even `bUsableByMarines`), so its mere presence is not proof of an active player traversal feature. Actual roof/ledge collision, support and landing space have not been tested. First mantle will target a user-confirmed obstacle with a recorded position and repeatable save.
