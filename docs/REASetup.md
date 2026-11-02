# Local REA setup and focused workflow

Set up on 2026-10-17 for the explicitly authorized Thief/KCD2 investigation.

| Component | Pinned local installation |
|---|---|
| REA | npm `rea-agents` 6.5.0, registry SHA-512 integrity verified, `.local/tools/rea`; dependency lifecycle scripts disabled |
| Ghidra | Official 12.1.4 public release; ZIP SHA-256 `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db` |
| Java | Existing full JDK 21.0.5, `C:\Program Files\Java\jdk-21` |
| Node | Bundled Codex runtime 24.19.0; system Node remains unchanged |

`tools/rea.ps1` reads ignored `.local/rea-toolchain.json` and scopes environment changes to the command. `doctor --provider ghidra --json` passed. REA's inspected `setup --client codex` was first previewed, then applied: it preserved unrelated Codex configuration, made a `.rea.backup`, and installed its matching `reverse-engineer-anything` skill. Codex configuration is `C:\Users\leonh\.codex\config.toml`; the installed skill is `C:\Users\leonh\.agents\skills\reverse-engineer-anything\SKILL.md`. New MCP connections can use that registration. This active investigation used a persistent project-local MCP session without restarting Codex.

The REA MCP environment was then aligned with the verified local profile (temporary directory, analysis provider/budget, Java limits and disabled analyzers). Only REA's environment table changed; parsed unrelated configuration was checked unchanged, with a further `config.toml.rea-project-settings.backup` preserved.

```powershell
.\tools\rea.ps1 doctor --provider ghidra --json
python tools/rea_session.py --session my-investigation
```

The session runner accepts MCP calls as JSON lines, records full responses/tool schemas under ignored `.local/rea-evidence/<session>/`, and explicitly closes the binary on exit. Example after selecting a private analysis container:

```json
{"name":"open_binary","arguments":{"path":"C:\\absolute\\private-analysis.pe","provider_id":"ghidra"}}
{"name":"procedure_pseudo_code","arguments":{"procedure":"0x14088d8f0"}}
{"name":"close_binary","arguments":{}}
{"quit":true}
```

## Import limitation and working fallback

Default full-PE Ghidra indexing exceeded the 900-second startup budget for both games. Neither import produced a successful deep-analysis result. The timed-out providers were closed. Do not describe those attempts as completed whole-binary analysis.

Windows REA 6.5.0 rejects a headerless `raw-binary` target on this route. `tools/extract_native_region.py` therefore makes a small **derived, analysis-only PE** from a specified byte range. It checks the original SHA-256, preserves original virtual addresses and instruction bytes, and records separate original, region and container hashes. It never edits the installed target. It permits output only in `.local`. The generated container is for analysis and must never be launched.

The local profile disables `Decompiler Switch Analysis` and `Stack` for these tiny scoped imports. Successful REA/Ghidra results were obtained for Thief's Dash tick and Hermite callback, plus KCD2's request merger and speed mapping. Evidence receipts identify the **derived container**, not the complete original executable. External constants, callees, RTTI, unwind fragments and distant branch targets are absent. Correlate with direct original-PE reads. In particular, the host speed-mapping result has omitted hot/cold branches and is partial; it does not establish a complete ABI or controller semantics.

Ghidra requires dot-free project paths. Its transient runtime lives in the workspace sibling `C:\Dev\KCD2 Mod\KCD2AnalysisRuntime`; provider close removes each temporary target/project. Retained proprietary bytes, disassembly and pseudocode are in ignored `.local`. Early runtime directories at the checkout root are additionally ignored by `/rea-ghidra-*/` and removed after close. Never add them to Git.

Readiness evidence: `.local/research/20261017/rea-doctor.json`, `rea-codex-setup-result.json`. Focused analysis receipts: `.local/rea-evidence/thief-focused/` and `kcd2-focused/`.

Primary distributions: [REA npm package](https://www.npmjs.com/package/rea-agents), [official Ghidra release](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.4_build).
