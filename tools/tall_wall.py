"""Launch KCD2 with guarded Jump climbs on walls detected during play."""

import csv
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import tall_guide_probe

native_probe = tall_guide_probe.native_probe
native_probe.__doc__ = __doc__
native_probe.DEFAULT_BUILD = "dry-guide-selection-v033-crest-profile-crossing"
native_probe.EXPECTED_MODE = "crest-profile-crossing"
native_probe.EXPECTED_HOOKS = [
    *native_probe.EXPECTED_HOOKS,
    {"name": "body", "rva": "0x1331224", "prologue": "48895c240848897c242055488bec4883"},
    {"name": "movement_request", "rva": "0x837e7c", "prologue": "48895c240848896c2410488974241857"},
    {"name": "grab_create", "rva": "0xf45298", "prologue": "4c8bdc49895b084989731049897b184d"},
    {"name": "grab_exit", "rva": "0x702b7c", "prologue": "48895c2410574881eca0000000488b05"},
    {"name": "living_action", "rva": "0x845b90", "prologue": "488bc448895810488970185557415441"},
]
EXPECTED_FILES = {
    name: digest for name, digest in tall_guide_probe.EXPECTED_FILES.items()
    if name != "Data/kuttenberg_thief_guide_tall.pak"
}
EXPECTED_FILES["Data/kuttenberg_thief_guide_runtime.pak"] = "24ef9c3c3ffb2dda0d2743f6561c17d47b486734637a162b08fe40f0f5a3285c"


def check_install(game):
    tall_guide_probe.dry_guide_probe.check_install(game, "guide-runtime", EXPECTED_FILES)


def game_pids():
    result = subprocess.run(
        ["tasklist.exe", "/FI", "IMAGENAME eq KingdomCome.exe", "/FO", "CSV", "/NH"],
        check=True, capture_output=True, text=True,
    )
    return [int(row[1]) for row in csv.reader(result.stdout.splitlines())
            if len(row) >= 2 and row[0].lower() == "kingdomcome.exe"]


def launch():
    if sys.platform != "win32":
        raise SystemExit("This launcher requires Windows and the pinned Steam installation.")
    if game_pids():
        raise SystemExit("Close KCD2 before using launch, or use start --pid from the main menu.")
    root = native_probe.ROOT
    game = Path(json.loads((root / ".local/environment.json").read_text(encoding="utf-8-sig"))["game"])
    check_install(game)
    build = native_probe.BUILD_ROOT / native_probe.DEFAULT_BUILD / native_probe.DLL.name
    if not build.is_file() or not build.with_name("probe-build.json").is_file():
        raise SystemExit("Build the repeatable tall wall DLL before launching.")
    print("Opening KCD2 through Steam. Wait at the main menu for the ready message.", flush=True)
    os.startfile("steam://rungameid/1771300")
    deadline = time.monotonic() + 120
    while time.monotonic() < deadline:
        pids = game_pids()
        if len(pids) > 1:
            raise SystemExit("Multiple KingdomCome.exe processes found.")
        if pids:
            result = subprocess.run(
                [sys.executable, str(Path(__file__).resolve()), "start", "--pid", str(pids[0])],
                capture_output=True, text=True,
            )
            if result.returncode == 0:
                print(result.stdout.strip())
                print("Kuttenberg wall climbs ready. Load the save and use normal Jump.", flush=True)
                return
            if not result.stderr.rstrip().endswith(
                "RuntimeError: Configured WHGame module is not loaded in this process"
            ):
                raise SystemExit(result.stderr.strip() or result.stdout.strip())
        time.sleep(1)
    raise SystemExit("Timed out waiting for the pinned KCD2 game module.")


if __name__ == "__main__":
    if sys.argv[1:] == ["launch"]:
        launch()
    else:
        native_probe.main(check_start=check_install)
