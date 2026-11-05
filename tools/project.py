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
GUIDE_MEMBER = "Levels/kutnohorsko/whdata_1"
GUIDE_NOOP_PAK = "Data/kuttenberg_thief_guide_noop.pak"
GUIDE_PAKS = {
    "guide-noop": GUIDE_NOOP_PAK,
    "guide-append": "Data/kuttenberg_thief_guide_append.pak",
    "guide-tall": "Data/kuttenberg_thief_guide_tall.pak",
    "guide-walls": "Data/kuttenberg_thief_guide_walls.pak",
    "guide-runtime": "Data/kuttenberg_thief_guide_runtime.pak",
}


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


def package_root(profile):
    if profile is None:
        return ROOT / "build"
    if profile not in GUIDE_PAKS:
        raise ValueError("Unknown build profile")
    return ROOT / "build" / profile


def write_guide_package(archive, destination, profile):
    from guide_member import append_inactive, inspect, read_member

    payload, _ = read_member(archive)
    if profile != "guide-noop":
        payload, report = append_inactive(payload, profile)
    else:
        report = inspect(payload)
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_STORED) as package:
        package.writestr(zipfile.ZipInfo(GUIDE_MEMBER, (2026, 11, 3, 0, 0, 0)), payload)
    with zipfile.ZipFile(destination) as package:
        if (package.namelist() != [GUIDE_MEMBER] or package.testzip() is not None
                or package.read(GUIDE_MEMBER) != payload):
            raise RuntimeError("Guide experiment archive failed payload verification")
    report.update(logical_member=GUIDE_MEMBER, package=destination.name, archive_created=True)
    report.pop("mount_path", None)
    return report


def build(profile=None):
    from observer_patch import ASSET, patch
    from stealth_movie import build as build_movie
    c = config()
    lua = Path(c["lua"])
    subprocess.run([str(lua), "tests/probe_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(lua), "tests/speed_trial_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(lua), "tests/native_bridge_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(lua), "tests/native_session_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(lua), "tests/terrain_probe_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(lua), "tests/stealth_hud_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(lua), "tests/ledge_probe_test.lua"], cwd=ROOT, check=True)
    float_lua = ROOT / '.local/tools/lua-5.1.5-float32/lua-float32.exe'
    if not float_lua.is_file():
        raise RuntimeError('Retail float32 Lua fixture runtime is required for this bridge package')
    subprocess.run([str(float_lua), '-e', 'assert(16777216+1==16777216, "Expected retail float32 number model")'], cwd=ROOT, check=True)
    subprocess.run([str(float_lua), "tests/native_bridge_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(float_lua), "tests/native_session_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(float_lua), "tests/terrain_probe_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(float_lua), "tests/stealth_hud_test.lua"], cwd=ROOT, check=True)
    subprocess.run([str(float_lua), "tests/ledge_probe_test.lua"], cwd=ROOT, check=True)
    for p in sorted((ROOT / "src/Data").rglob("*.lua")):
        # loadfile checks syntax without executing engine-dependent code.
        check = "local f,e=loadfile(" + json.dumps(p.as_posix()) + "); assert(f,e)"
        subprocess.run([str(lua), "-e", check], cwd=ROOT, check=True)
    manifest = ROOT / "src/mod.manifest"
    assert ET.parse(manifest).findtext("info/modid") == MOD_ID
    # Single additive owned definition; never replace a vanilla buff or use an
    # exclusivity mode that removes unrelated character effects.
    buff_path = ROOT / "src/Data/Libs/Tables/rpg/buff__kuttenberg_thief.xml"
    buffs = ET.parse(buff_path).findall("./buffs/buff")
    assert len(buffs) == 1
    buff = buffs[0]
    assert buff.get('buff_id') == 'eacd8708-9dde-43a6-bddb-76d56b9338d1'
    assert buff.get('is_persistent') == 'false' and buff.get('buff_exclusivity_id') == '1'
    assert buff.get('implementation') == 'Cpp:BasicTimed' and 0 < float(buff.get('duration')) <= 2
    assert buff.get('buff_params') == 'rms*1.25'
    with zipfile.ZipFile(Path(c['game']) / 'Data/Tables.pak') as retail_tables:
        for name in retail_tables.namelist():
            if name.startswith('Libs/Tables/rpg/buff') and name.endswith('.xml'):
                assert buff.get('buff_id').encode() not in retail_tables.read(name), 'Owned buff ID collides with retail'
    output_root = package_root(profile)
    out = output_root / MOD_ID
    (out / "Data").mkdir(parents=True, exist_ok=True)
    shutil.copy2(manifest, out / "mod.manifest")
    pak = out / "Data/kuttenberg_thief.pak"
    with zipfile.ZipFile(Path(c['game']) / 'Data/Scripts.pak') as retail:
        observer_xml = patch(retail.read(ASSET))
    stealth_movie = build_movie()
    # Preserve historical source/tests while excluding the legacy AddBuff route.
    excluded = {"Scripts/KuttenbergThief/SpeedTrial.lua", "Libs/Tables/rpg/buff__kuttenberg_thief.xml"}
    with zipfile.ZipFile(pak, "w", compression=zipfile.ZIP_STORED) as z:
        for p in sorted((ROOT / "src/Data").rglob("*")):
            if p.is_file() and p.relative_to(ROOT / "src/Data").as_posix() not in excluded:
                info = zipfile.ZipInfo(p.relative_to(ROOT / "src/Data").as_posix(), (2026, 10, 11, 0, 0, 0))
                z.writestr(info, p.read_bytes())
        z.writestr(zipfile.ZipInfo(ASSET, (2026, 10, 11, 0, 0, 0)), observer_xml)
        z.writestr(zipfile.ZipInfo('Libs/UI/KTStealth.swf', (2026, 10, 11, 0, 0, 0)), stealth_movie.read_bytes())
    with zipfile.ZipFile(pak) as z:
        assert z.testzip() is None
        assert "Scripts/Mods/kuttenberg_thief.lua" in z.namelist()
        assert "Scripts/KuttenbergThief/NativeBridge.lua" in z.namelist()
        assert "Scripts/KuttenbergThief/TerrainProbe.lua" in z.namelist()
        assert "Scripts/KuttenbergThief/StealthHud.lua" in z.namelist()
        assert "Scripts/KuttenbergThief/LedgeProbe.lua" in z.namelist()
        assert 'Libs/UI/UIElements/KTStealth.xml' in z.namelist()
        assert z.read('Libs/UI/KTStealth.swf') == stealth_movie.read_bytes()
        assert not excluded.intersection(z.namelist()), 'Legacy movement trial must not be packaged'
        assert all(i.compress_type == zipfile.ZIP_STORED for i in z.infolist())
    files = {'mod.manifest', 'Data/kuttenberg_thief.pak'}
    if profile:
        guide_pak = GUIDE_PAKS[profile]
        report = write_guide_package(Path(c['game']) / 'Data/Levels/kutnohorsko/level.pak',
                                     out / guide_pak, profile)
        (output_root / "guide-member.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        files.add(guide_pak)
    result = {p.relative_to(out).as_posix(): digest(p) for p in out.rglob("*") if p.is_file()}
    if profile and set(result) != files:
        raise RuntimeError("Unexpected files in build output")
    (output_root / "package-hashes.json").write_text(json.dumps(result, indent=2))
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


def install(profile=None):
    from observer_patch import check_conflicts
    require_closed()
    c = config()
    verify_build(c)
    source = build(profile=profile) if profile else build()
    mods = Path(c["game"]) / "Mods"
    check_conflicts(mods, MOD_ID, extra_assets=(GUIDE_MEMBER,) if profile else ())
    order = mods / "mod_order.txt"
    if order.exists() and MOD_ID not in order.read_text(encoding="utf-8-sig").splitlines():
        raise RuntimeError("Existing mod_order.txt excludes this mod. Review it before changing load order.")
    dest = mods / MOD_ID
    expected = json.loads((package_root(profile) / "package-hashes.json").read_text())
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
    if profile:
        receipt["profile"] = profile
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
                or ("error" in s.lower() and ("KuttenbergThief" in s or MOD_ID in s
                    or "ai/player/switch/switch.xml" in s.lower().replace('\\', '/')
                    or "ActorStateCheck" in s or "kt_speed_trial" in s
                    or "eacd8708-9dde-43a6-bddb-76d56b9338d1" in s))]
    dest = folder / (timestamp() + "-probe.log")
    dest.write_text("\n".join(selected) + "\n", encoding="utf-8")
    print("COLLECTED:", len(selected), "relevant lines;", dest)
    print("boot=" + str(any("[KT] boot" in s for s in selected))
          + " player_samples=" + str(sum("[KT] sample" in s and "pos=(" in s for s in selected)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["build", "backup", "install", "uninstall", "collect"])
    profiles = parser.add_mutually_exclusive_group()
    profiles.add_argument("--guide-noop", dest="profile", action="store_const", const="guide-noop",
                          help="Build or install the byte-identical guide mount experiment")
    profiles.add_argument("--guide-append", dest="profile", action="store_const", const="guide-append",
                          help="Build or install the inactive guide storage experiment")
    profiles.add_argument("--guide-tall", dest="profile", action="store_const", const="guide-tall",
                          help="Include the inactive tall Kuttenberg guide")
    profiles.add_argument("--guide-walls", dest="profile", action="store_const", const="guide-walls",
                          help="Include both inactive Kuttenberg wall guides")
    profiles.add_argument("--guide-runtime", dest="profile", action="store_const", const="guide-runtime",
                          help="Reserve inactive slots for walls detected during play")
    args = parser.parse_args()
    if args.profile and args.command not in {"build", "install"}:
        parser.error("Guide profiles require build or install")
    try:
        if args.profile:
            globals()[args.command](profile=args.profile)
        else:
            globals()[args.command]()
    except Exception as error:
        print("FAILED:", error, file=sys.stderr)
        sys.exit(1)
