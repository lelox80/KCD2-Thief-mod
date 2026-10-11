"""Local-only package, backup, deployment and diagnostics. Python 3.12 stdlib."""
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MOD_ID = "kuttenberg_thief"


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def timestamp():
    return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")


def config():
    return json.loads((ROOT / ".local/environment.json").read_text(encoding="utf-8-sig"))


def require_closed():
    result = subprocess.run(["tasklist", "/FI", "IMAGENAME eq KingdomCome.exe", "/FO", "CSV", "/NH"],
                            capture_output=True, text=True, check=True)
    if '"KingdomCome.exe"' in result.stdout:
        raise RuntimeError("Close KCD2 before backup, installation or removal.")


def verify_build(c):
    for key, expected in c["retail_sha256"].items():
        if digest(Path(c["game"]) / key) != expected:
            raise RuntimeError("Installed retail binary changed; repeat capability assessment: " + key)


def build():
    c = config()
    lua = Path(c["lua"])
    subprocess.run([str(lua), "tests/probe_test.lua"], cwd=ROOT, check=True)
    for p in sorted((ROOT / "src/Data").rglob("*.lua")):
        # loadfile checks syntax without executing engine-dependent code.
        check = "local f,e=loadfile(" + json.dumps(p.as_posix()) + "); assert(f,e)"
        subprocess.run([str(lua), "-e", check], cwd=ROOT, check=True)
    manifest = ROOT / "src/mod.manifest"
    assert ET.parse(manifest).findtext("info/modid") == MOD_ID
    out = ROOT / "build" / MOD_ID
    (out / "Data").mkdir(parents=True, exist_ok=True)
    shutil.copy2(manifest, out / "mod.manifest")
    pak = out / "Data/kuttenberg_thief.pak"
    with zipfile.ZipFile(pak, "w", compression=zipfile.ZIP_STORED) as z:
        for p in sorted((ROOT / "src/Data").rglob("*")):
            if p.is_file():
                info = zipfile.ZipInfo(p.relative_to(ROOT / "src/Data").as_posix(), (2026, 10, 11, 0, 0, 0))
                z.writestr(info, p.read_bytes())
    with zipfile.ZipFile(pak) as z:
        assert z.testzip() is None
        assert "Scripts/Mods/kuttenberg_thief.lua" in z.namelist()
        assert all(i.compress_type == zipfile.ZIP_STORED for i in z.infolist())
    result = {p.relative_to(out).as_posix(): digest(p) for p in out.rglob("*") if p.is_file()}
    (ROOT / "build/package-hashes.json").write_text(json.dumps(result, indent=2))
    print("BUILD PASS: Lua 5.1 tests/syntax, manifest, ZIP_STORED PAK and CRC.")
    return out


def backup():
    require_closed()
    c = config()
    source = Path(c["saves"])
    if not source.is_dir():
        raise RuntimeError("Configured save directory is absent.")
    files = sorted(p for p in source.rglob("*") if p.is_file())
    if not files:
        raise RuntimeError("No saves found; do not claim a backup.")
    dest = ROOT / ".local/backups" / timestamp()
    dest.mkdir(parents=True)
    entries = {}
    with zipfile.ZipFile(dest / "kcd2-saves.zip", "w", compression=zipfile.ZIP_STORED) as z:
        for p in files:
            name = p.relative_to(source).as_posix()
            entries[name] = digest(p)
            z.write(p, name)
    with zipfile.ZipFile(dest / "kcd2-saves.zip") as z:
        for name, expected in entries.items():
            with z.open(name) as f:
                h = hashlib.sha256()
                for block in iter(lambda: f.read(1024 * 1024), b""): h.update(block)
                if h.hexdigest() != expected: raise RuntimeError("Save backup verification failed")
    manifest = {"source": str(source), "file_count": len(entries), "sha256": entries}
    (dest / "manifest.json").write_text(json.dumps(manifest, indent=2))
    (ROOT / ".local/last-backup.json").write_text(json.dumps({"folder": str(dest)}))
    print("BACKUP VERIFIED:", len(entries), "files;", dest)


def install():
    require_closed()
    c = config()
    verify_build(c)
    source = build()
    mods = Path(c["game"]) / "Mods"
    order = mods / "mod_order.txt"
    if order.exists() and MOD_ID not in order.read_text(encoding="utf-8-sig").splitlines():
        raise RuntimeError("Existing mod_order.txt excludes this mod. Review it before changing load order.")
    dest = mods / MOD_ID
    expected = json.loads((ROOT / "build/package-hashes.json").read_text())
    if dest.exists():
        actual = {p.relative_to(dest).as_posix(): digest(p) for p in dest.rglob("*") if p.is_file()}
        if actual != expected: raise RuntimeError("Existing mod differs. Uninstall the owned version first.")
        print("INSTALL VERIFIED: identical package already present.")
        return
    backup()
    dest.mkdir(parents=True)
    for name, sha in expected.items():
        target = dest / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, target)
        if digest(target) != sha: raise RuntimeError("Installed hash mismatch: " + name)
    receipt = {"destination": str(dest), "sha256": expected,
               "version": ET.parse(source / "mod.manifest").findtext("info/version")}
    (ROOT / ".local/install-receipt.json").write_text(json.dumps(receipt, indent=2))
    print("INSTALL VERIFIED:", dest)


def uninstall():
    require_closed()
    c = config()
    rp = ROOT / ".local/install-receipt.json"
    receipt = json.loads(rp.read_text())
    dest = Path(receipt["destination"]).resolve()
    allowed = (Path(c["game"]) / "Mods" / MOD_ID).resolve()
    if dest != allowed: raise RuntimeError("Unexpected removal destination")
    expected = receipt["sha256"]
    actual = {p.relative_to(dest).as_posix(): digest(p) for p in dest.rglob("*") if p.is_file()}
    if actual != expected: raise RuntimeError("Mod folder has changes; preserve and inspect them before removal.")
    archive = ROOT / ".local/backups" / (timestamp() + "-uninstalled")
    shutil.copytree(dest, archive)
    for name in expected:
        p = (dest / name).resolve()
        if not p.is_relative_to(allowed): raise RuntimeError("Unsafe receipt path")
        p.unlink()
    for p in sorted(dest.rglob("*"), key=lambda p: len(p.parts), reverse=True):
        if p.is_dir(): p.rmdir()
    dest.rmdir()
    rp.rename(archive / "install-receipt.json")
    print("UNINSTALLED: owned files archived at", archive)


def collect():
    c = config()
    log = Path(c["game"]) / "kcd.log"
    folder = ROOT / ".local/logs"
    folder.mkdir(parents=True, exist_ok=True)
    lines = log.read_text(encoding="utf-8", errors="replace").splitlines()
    selected = [s for s in lines if "[KT]" in s or ("[Mod]" in s and MOD_ID in s.lower())
                or ("[Error]" in s and ("KuttenbergThief" in s or MOD_ID in s))]
    dest = folder / (timestamp() + "-probe.log")
    dest.write_text("\n".join(selected) + "\n", encoding="utf-8")
    print("COLLECTED:", len(selected), "relevant lines;", dest)
    print("boot=" + str(any("[KT] boot" in s for s in selected))
          + " player_samples=" + str(sum("[KT] sample" in s and "pos=(" in s for s in selected)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["build", "backup", "install", "uninstall", "collect"])
    try:
        globals()[parser.parse_args().command]()
    except Exception as error:
        print("FAILED:", error, file=sys.stderr)
        sys.exit(1)
