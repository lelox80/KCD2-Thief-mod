# Original Thief gem and compact indicators

v0.0.15 uses the actual Thief2014 light-gem artwork from the user's installed donor, beside KCD2's bottom health HUD. Only the visibility gem is drawn. Noise remains in background diagnostics; the user requested removal of the added noise bars after v0.0.14. No separate player noise meter was found in the inspected donor HUD inventory.

## Provenance and conversion

Pinned donor: `ThiefGame.u`, SHA-256 `c061289fdc634484badc780379992b6daa1827998b114d4b861338367d87fa35`, export36717 `PROG_GUI_Gem.Gem`. Extracted15,036-byte movie SHA-256 `28119b1a21a59d57357cf8a6eebd35aa18d02eec0747a3242bfa43a2292325d3`. Its light-gem container is sprite56: base41, dark54 and light55. Intro frame8 supplies the mature dark/light layers.

`tools/stealth_movie.py` retains original vector definitions29,30,37–43,45,46 and the two frozen states54/55. It embeds the original64×64 `Gem_I2C` texture (export36847) as standard premultiplied-ARGB Lossless2. The base remains below dark/light with the original166/256 alpha. It omits the shared smoke-particle clips52/53, donor ActionScript, imported movies and all unrelated HUD components. The result preserves the selected art with our continuous crossfade; original smoke animation and complete donor rendering fidelity are not claimed.

Private originals, intermediate XML/SVG/PNG, texture/export receipts and selected runtime-independent evidence are in `.local/research/20261021-hud-donor/`. The final movie and source/tool/asset receipt are `.local/assets/stealth/KTStealth.swf` and `receipt.json`. All these proprietary/generated files are ignored; only our adapter, XML, generator and documentation are source files. The package includes the converted movie for this local installation.

The inspected portable tool is [FFDec26.4.0](https://github.com/jindrapetrik/jpexs-decompiler/releases/tag/version26.4.0), official ZIP SHA-256 `56ce960aeafc7caf034e2052ba49f878a883f7c7ca690da6eeffca9587b5e4b9`. Its command-line JAR and dependencies are verified against that archive. It supports Scaleform extraction and compiles our AS2 adapter; no third-party installer or donor scripts are executed. The local build requires that tool, existing JDK21 and Pillow for embedding the extracted texture. `python tools/stealth_movie.py` regenerates the movie; `python tools/project.py build` reuses only a source/tool/hash-matched output.

## Receiving interface and presentation

The owned `Libs/UI/UIElements/KTStealth.xml` is discovered through retail's `Libs/UI/UIElements/*.xml` scan. It loads `Libs/UI/KTStealth.swf`, instance0, without replacing another UI element. Its input flags are off. Lua uses `UIAction.ShowElement`, then `CallFunction("KTStealth",0,"Update",v,vKnown)`. Our AS2 function returns the exact string `kt15`; Lua requires that receipt rather than assuming native call success means successful rendering. Static retail registration/return/discovery evidence is in the private renderer findings.

The gem is centered at(760,1015) in the1920×1080 design stage, just left of the retail health group. Its health-bar placement chain is `bc` at(960,1080), `mc_StatsContainer` at(0,0), `mc_StatsEle` at(0,-60), and `mc_HealthBar` at(-141.5,-25). These are authored anchors, not a runtime query of every HUD customization. Selected retail metadata is retained in `.local/research/20261021-hud-layout/`.

XML now uses dynamic center/bottom alignment with scale1/max0. [CryEngine's constraint documentation](https://www.cryengine.com/docs/static/engines/cryengine-3/categories/1638401/pages/15011348) distinguishes uniform dynamic fitting from fullscreen stretch. The v0.0.14 screenshot showed the stretch defect; actual corrected retail placement remains to be seen. Only aggregate visibility `vib` is normalized to a chosen[-2,3.5] brightness range, not a detection probability or pure light measurement. Negative values darken the gem. Raw `vib/evi/noi/fsm/con` remain available through `kt_stealth_status`; neither Flash nor text fallback draws noise.

The adapter starts hidden, snaps to the first current value and smooths later changes over about120ms. Unknown visibility hides the gem even when background noise is known. Normal lifecycle invalidation hides the owned element; stop/reload unloads it. A350ms Flash heartbeat expiry hides stale feedback when Lua callbacks cease while the UI keeps advancing. Complete engine/render freezes cannot visibly advance any watchdog. A sticky graphical-load/update failure falls back to a small visibility text readout with an explicit reason; `kt_stealth_on` retries.

## Acceptance

The raw host readings were retail/user verified in v0.0.13. v0.0.14 then verified actual graphical loading, updates and off/on controls, but the user rejected its stretched layout. v0.0.15 collector fixtures and compiled-art checks pass; a brief visual check of round shape and placement is sufficient after installation. No new light/control/movement sequence is required for this correction. [Recorded results](StealthResults.md).
