"""Read-only, narrow binary fingerprint checks. No injection/disassembly/decompilation."""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
config = json.loads((ROOT / ".local/environment.json").read_text(encoding="utf-8-sig"))
# Locating patterns from KCD2ModLoader c7b12a6, src/kcd2_init.cpp.
# Matching a pattern does NOT verify its target's ABI, semantics or loader compatibility.
patterns = {
    "RayWorldIntersection": "E8 ? ? ? ? 48 FF 03 48 81 C4",
    "CryScriptSystem_Init": "E8 ? ? ? ? 84 C0 74 ? E8 ? ? ? ? 41 38 BE",
    "CryScriptSystem_Update": "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 3D ? ? ? ? 48 8B F1 33 D2",
}
report = {"warning": "Raw-file locating evidence only; not ABI or runtime verification.", "modules": {}}
for label, dll in [
    ("retail", Path(config["game"]) / "Bin/Win64MasterMasterSteamPGO/WHGame.dll"),
    ("development", Path(config["tools"]) / "Bin/Win64ReleaseSteamLTO_DLL/WHGame.dll"),
]:
    data = dll.read_bytes()
    counts = {}
    for name, pattern in patterns.items():
        regex = b"".join(b"." if token == "?" else re.escape(bytes([int(token, 16)]))
                         for token in pattern.split())
        counts[name] = sum(1 for _ in re.finditer(regex, data, re.S))
    report["modules"][label] = {"sha256": hashlib.sha256(data).hexdigest(), "matches": counts}
dest = ROOT / ".local/evidence/native-pattern-counts.json"
dest.parent.mkdir(parents=True, exist_ok=True)
dest.write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
