# M1 experiment: ordinary crouched movement with a speed buff

## Evidence and choice

Installed `Tables.pak/Libs/Tables/rpg/buff__perk.xml` defines the two Cpp:Crouch perks `perk_plizivej_prizrak` and `perk_plizivej_prizrak_ii` with `rms*1.15` and `rms*1.2`. `buff.xml` contains quest effects using rms, including `quest_utokNebakov_half_movement`, as well as BasicTimed effects with finite durations and is_persistent=false. Installed binding documentation provides soul:AddBuff, RemoveAllBuffsByGuid, HasBuffDebug and GetDerivedStat. These sources justify a bounded experiment; their actual retail Lua availability and semantics are not yet verified.

Official [mod structure](https://warhorse.youtrack.cloud/articles/KM-A-3) documents `tablename__modid.xml` table patches. Our single new definition uses ID `eacd8708-9dde-43a6-bddb-76d56b9338d1`, `rms*1.25`, BasicTimed, duration=2, ShortTerm, ID exclusivity and is_persistent=false. It neither replaces a vanilla buff nor uses class/target exclusivity. Build checks reject an ID collision with installed tables. Table loading and whether the engine honors persistence/lifetime fields require a playtest.

The 25% boost is an intentionally modest diagnostic value, **not a measured Thief value**. Lua requests removal after 1 second of uninterrupted test time; the data's finite duration is a secondary engine safeguard, not yet a measured seconds guarantee. No AddBuff call occurs automatically.

## Ownership and cancellation

KCD2 alone owns keyboard/gamepad input, direction, acceleration, character collision/gravity, camera and animation. Lua owns one RPG modifier on the current player, never position/velocity or a simulated held key. Releasing normal movement input still belongs to KCD2. No competing controller is introduced. Actual effect on contact, locomotion speed and footsteps remains to be verified.

The command requires fresh complete XGen crouch/activity states; physical IsFlying=false; alive/conscious/not carrying; no mounted/ladder/dialogue/pickpocket/weapon/combat state; normal physicalization and idle/movement animation; camera close to the body; numeric positive rms and known absence of our buff. Missing/nil/error values refuse activation. IsOnLadder=0 and GetFlyMode=0 are the explicit values observed in this retail build. Pending phases recheck these conditions every 50 ms; XGen updates about every 100 ms. There is no new frame update hook.

State sequence: arm 3 s -> baseline 2 s -> boost <=1 s -> recovery 2 s. Repeated activation is refused; cooldown is 2 s. A callback gap >0.35 s, stale XGen >0.3 s, incompatible gate, changed player identity/generation, excessive boost displacement (>3 m), manual stop or script reload cancels. A time gap never causes catch-up movement. The timer compares the monotonic async clock rather than subtracting the coarse large simulation float.

Removal is restricted to our GUID. A failed removal locks further activation; `kt_speed_reset` retries and only clears the lock when the getter confirms absence. No old player is dereferenced after entity replacement; an old entity's finite nonpersistent buff is left to the engine. This fallback is **not yet runtime validated**. Save/load or very short menu interruptions could retain the effect until a callback/removal/expiry; callback order before the next physics step is unknown. Thus this experiment does not satisfy the final swoop's interruption contract. Full game exit is the fallback if reset cannot confirm removal. Original saves and other buffs are not intentionally modified.

## Exact local test

1. Use the existing backed-up Kuttenberg save, a level open area, weapon sheathed, no interaction/combat. Crouch and wait a second.
2. Enter `kt_speed_test` in the console, close it immediately and hold the ordinary forward key while remaining crouched for about eight seconds. Expected candidate behavior: normal walking, a brief modest speed increase, then normal walking. It may refuse or produce no movement difference; both are diagnostic outcomes.
3. Run `kt_speed_reset`. Repeat from about one body length facing a wall. Expected: the same normal wall contact, no penetration, and normal movement afterward. Log positions alone cannot prove collision; report what actually happens.
4. Repeat and stand up during the effect; repeat and pause for two seconds during the effect; reload the same old save. Expected: cancellation, no automatically resumed test, no lingering increase. Short pauses, input loss, stairs/slopes/ledge tests remain separate acceptance work.
5. Exit and run `python tools/project.py collect`, then `python tools/analyze_probe.py <filtered-log>`. Send a brief description of speed, collision and recovery.

Logs: `speed_trial begin`, phase samples (5 Hz, position, velocity, rms, buff presence, accepted ground/crouch gates), applied before/after rms, cleanup verification and end/cancellation reason. Refusals name the exact missing gate/API. No instance handles or private identifiers are logged.

Before calling this collision-aware speed control proven, require normal/boost/recovery speed comparison, confirmed removal, wall contact and user observation. Before calling it swoop, add smooth configurable distance/duration/cooldown, input activation and verified lifecycle timing, plus the entire TestPlan. Camera response is not part of this experiment.
