"""Explicit-PID dry fixture registration. Start at the Kuttenberg main menu.

Stop seals the audit and disarms further index activation. The traversal guard
stays armed in this process until normal game exit.
"""
import hashlib
import json

import ledge_probe

native_probe = ledge_probe.native_probe
native_probe.__doc__ = __doc__
native_probe.DEFAULT_BUILD = "dry-guide-v026"
native_probe.EXPECTED_MODE = "dry-guide-guard"
native_probe.EXPECTED_HOOKS = [
    *native_probe.EXPECTED_HOOKS,
    {"name": "index", "rva": "0x7e6a50", "prologue": "405355565741544155415641574883ec"},
]

EXPECTED_FILES = {
    "mod.manifest": "742e4ce2d623a0c6d85fe039dd2328c7a29bb29a1e7079aa4234f572d504355a",
    "Data/kuttenberg_thief.pak": "204811ed6e57409c6864c0b7b643f1ea68c0cbaa6493adc089e83b63f5099f14",
    "Data/kuttenberg_thief_guide_append.pak": "2cdf163040f3f89f8fcea6a54b1845394a67ee5c95e1e9497b933c116d446b03",
}


def check_install(game_dir, profile="guide-append", expected_files=EXPECTED_FILES):
    from observer_patch import check_conflicts
    from project import GUIDE_MEMBER

    dest = game_dir / "Mods/kuttenberg_thief"
    receipt = json.loads((native_probe.ROOT / ".local/install-receipt.json").read_text())
    if receipt.get("profile") != profile or receipt.get("sha256") != expected_files:
        raise RuntimeError("Dry guard requires the exact inactive append installation")
    actual = {path.relative_to(dest).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in dest.rglob("*") if path.is_file()}
    if actual != expected_files:
        raise RuntimeError("Inactive append installation does not match its pins")
    check_conflicts(dest.parent, dest.name, extra_assets=(GUIDE_MEMBER,))
    order = dest.parent / "mod_order.txt"
    if order.is_file() and dest.name not in order.read_text(encoding="utf-8-sig").splitlines():
        raise RuntimeError("Mod load order excludes the inactive append fixture")


if __name__ == "__main__":
    native_probe.main(check_start=check_install)
