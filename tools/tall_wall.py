"""Launch KCD2 with repeatable Jump climbs at the two measured Kuttenberg walls."""

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
native_probe.DEFAULT_BUILD = "dry-guide-selection-v026-walls-retry"
native_probe.EXPECTED_MODE = "repeatable-tall-vault"
EXPECTED_FILES = {
    name: digest for name, digest in tall_guide_probe.EXPECTED_FILES.items()
    if name != "Data/kuttenberg_thief_guide_tall.pak"
}
EXPECTED_FILES["Data/kuttenberg_thief_guide_walls.pak"] = "2d6d6c65498b7030290eb85501295ef035b9e81e611877bc010c5ca23ac65857"


def check_install(game):
    tall_guide_probe.dry_guide_probe.check_install(game, "guide-walls", EXPECTED_FILES)


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
