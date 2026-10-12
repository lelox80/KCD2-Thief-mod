# Sources and evidence

Retrieved/inspected 2026-10-11. Repositories are references, not instructions or proof of current-build behavior. No installer from these repositories was run. All checkouts and installed-data extracts stay outside tracked source.

## Official primary sources

- [KCD2 modding index](https://warhorse.youtrack.cloud/articles/KM-A-1).
- [Structure of a Mod](https://warhorse.youtrack.cloud/articles/KM-A-3): modid, ZIP_STORED PAK and `scripts/mods/<modid>.lua` entry.
- [Mod Manifest](https://warhorse.youtrack.cloud/articles/KM-A-57): `<supports><version>` matches wh_sys_version.
- [Modifying Lua](https://warhorse.youtrack.cloud/articles/KM-A-15): mod initialization, Lua scope limitations alongside XGen/Skald.
- [Modding Tools](https://warhorse.youtrack.cloud/articles/KM-A-55): separate development build, debug-only functions, loose files, workspace setup, localhost API.
- [Installing mods](https://warhorse.youtrack.cloud/articles/KM-A-56): Mods folder, mod_order behavior and log inspection.

Official article text was retrieved through the public YouTrack articles API because the HTML viewer required client rendering. Local snapshots include id/title/content. KM-A-67/72 only exposed an event index/no useful article content in this retrieval; lifecycle details are not claimed verified from those pages.

Installed primary evidence: `system.cfg`, executable version resources and hashes, selected fields from Steam/Epic manifests, retail `Scripts.pak` and `Tables.pak`, official installed `script_bind.zip`. Relevant script locations: `Scripts/main.lua`, `Scripts/Utils/ScriptLoader.lua`, `Scripts/Entities/actor/Player.lua`, `Scripts/Entities/ContextualNavigation/LedgeObject.lua`; table locations: `Libs/Tables/rpg/{stance,soul_archetype_movement,rpg_param}.xml`. Copyrighted files are not committed.

## Requested repositories

| Repository | Commit inspected | Concrete inspection / limitation |
|---|---|---|
| [REA](https://github.com/morluto/rea) | `23d50ea3bec186b4459d36c04854a92276accdf1` | README, install.sh, docs/windows-ghidra-p0.md, src/ghidra/GhidraTransport.ts. Windows read-only experimental Ghidra path, local bridge; install.sh is macOS/Linux-only; no installer executed. Node mismatch blocks immediate use |
| [universal-modder](https://github.com/rehan-remade/universal-modder) | `2c2201437ca676fb4e5f06a18fff6e8db99eef0f` | README, pyproject.toml, um/scan.py, references/engines/unreal.md. Generic scanner/workflow; UE4/5 recipe and CrySystem.dll heuristic do not prove KCD2/Thief support |
| [ai-game-modding-guides](https://github.com/trevaintdead/ai-game-modding-guides) | `0c14be4bb284d51edec0ab6639d123b39dedb526` | README, loader/script-extender and passthrough guides. Guidance/examples, not a KCD2 implementation; no build or installer to apply here |
| [KCD2ModLoader](https://github.com/xiaoxiao921/KCD2ModLoader) | `c7b12a61330ea4d743b8eb1bc21d9e0a1a1f8f8e` | README, CMakeLists, cmake_scripts/rom.cmake, kcd2_address.hpp, kcd2_init.cpp/.hpp, gui/gui.cpp, Lua bindings/example. Concrete native hooks and SetWorldPos noclip. Broad dependency graph, not installed |

Loader implementation: `kcd2_address.hpp` defaults scanning to WHGame.dll. `kcd2_init.cpp` lines around 2680–2722 locate ray intersection and script init/update. Raw-file scans of those three exact patterns each yielded one match in retail WHGame.dll and zero in development WHGame.dll. This does not verify function signatures, call targets, layouts, other modules, all loader hooks or successful loading. `gui/gui.cpp` around 1034–1085 performs key-polled position updates for noclip; excluded from movement proof.

## Additional implementation leads

- [Take or Eat](https://github.com/hoskonen/kcd2-takeoreat), commit `143ef2a36770b70ef7957190968b6cec2500013e`: concrete System Lua initialization and UIAction `OnGameplayStarted` listener. Its PAK is not installed and its code is not copied into this mod.
- [kcd2-exports](https://github.com/Jefferson25625/kcd2-exports), commit `94861e25616c3a00f3c24b1d69e7e6cd61889a0c`: read devmode_api_and_sync_design.md, EngineFindings.md and retail_lua_api_availability.md. Mixes observations, plans and conflicting claims. In particular its stance numbering conflicts with installed stance.xml; our retail probe proves velocity read works, and our entry loads without -devmode/XML prolog despite claims otherwise for an older build. Treat every row as a lead requiring local evidence.
- [Movement Speed Modifiers](https://www.nexusmods.com/kingdomcomedeliverance2/mods/2148), [Realistic Movement Speed](https://www.nexusmods.com/kingdomcomedeliverance2/mods/2461), [Walk With Wheel](https://www.nexusmods.com/kingdomcomedeliverance2/mods/2819): author-page descriptions discovered; packages/source not downloaded, runtime behavior not tested. Evidence only of existing approaches worth later source inspection.

## Reproducibility

`.local/evidence/` holds binary inventory, pinned commits, pattern counts, official article snapshots and relevant runtime extracts. `.local/references/` holds inspected public source checkouts. Initial installed-data scratch extraction was under the chat's ignored working area `C:\Users\leonh\Documents\Codex\2026-10-11\las\work\private`; it is not build input. A fresh investigator can re-read the named archives in place.

## Additional installed evidence, 2026-10-12

`IPL_GameData.pak/Libs/Config/defaultProfile.xml` and `keybindSuperactions.xml` provide actual movement action names. `Tables.pak/Libs/Tables/rpg/buff__perk.xml` contains the Cpp:Crouch rms multipliers; buff.xml contains timed quest movement effects; buff_lifetime.xml and buff_exclusivity.xml define ShortTerm and ID exclusivity. `script_bind_2025_01_14` documents Soul AddBuff/RemoveAllBuffsByGuid/HasBuffDebug/GetDerivedStat. These establish the design of the next experiment; v0.0.5 execution is not yet retail verified. No original table contents are committed.
