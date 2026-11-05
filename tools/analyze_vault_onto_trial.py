"""Review the single host-owned VaultOnto admission trial."""
import argparse
import json
from pathlib import Path

from analyze_dry_guide_probe import owned
from analyze_ledge_admission import analyze, integer

HANDLE = 4129 << 16
RESULTS = ("not_run", "physical", "ground", "stance", "position", "clearance", "released",
           "wall_section", "landing_support", "overlap")


def review(rows):
    if len(rows) < 2 or any(not isinstance(row, dict) for row in rows):
        raise ValueError("Incomplete trial capture")
    header, stop = rows[0], rows[-1]
    contract = (header.get("mode"), header.get("probe"), header.get("probe_revision"), header.get("hook_count"))
    if contract not in {("bounded-vault-onto", "ledge-admission", 4, 5), ("bounded-tall-vault", "ledge-admission", 4, 6)}:
        raise ValueError("Expected the bounded VaultOnto build")
    if stop.get("event") != "stopped" or stop.get("guard_remains_armed") is not True:
        raise ValueError("Expected a sealed capture with the guard resident")
    forwards = stop.get("trial_framework_forwards")
    result = stop.get("trial_result")
    if not integer(forwards, 0, 1) or not integer(result, 0, len(RESULTS) - 1):
        raise ValueError("Invalid one-shot accounting")

    report = analyze([dict(header, mode="observe-only", hook_count=4), *rows[1:]])
    requests = [row for row in rows[1:-1] if isinstance(row.get("request"), list)
                and len(row["request"]) >= 2 and owned(row["request"][1])]
    exact = [row for row in requests if row["request"][:2] == [4, HANDLE]
             and row.get("request_matches_selection") is True]
    accepted = sum(row.get("framework_accepted") is True for row in exact)
    problems = report["review_problems"]
    if len(requests) != len(exact) or len(requests) > 1 or accepted > forwards or forwards > len(exact):
        problems.append("Fixture requests do not reconcile with the one-shot guard")
    if forwards and (RESULTS[result] != "released" or stop.get("trial_armed") is not True):
        problems.append("Forwarded request lacks a matching clearance release")
    clones = stop.get("tall_parameter_clones")
    if contract[0] == "bounded-tall-vault" and not integer(clones, forwards):
        problems.append("Tall request lacks a matching selector parameter copy")
    return {"trial_armed": stop.get("trial_armed"), "gate_result": RESULTS[result],
            "ground_samples": stop.get("trial_ground_samples"), "used_takeoff": stop.get("trial_used_takeoff"),
            "tall_parameter_clones": clones,
            "framework_forwards": forwards, "framework_acceptances": accepted,
            "review_problems": problems, "animation_completed": None}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    try:
        with args.capture.open(encoding="utf-8") as capture:
            rows = []
            for line in capture:
                if len(rows) >= 60002 or len(line) > 2048:
                    raise ValueError("Capture exceeds the native bounds")
                rows.append(json.loads(line))
        print(json.dumps(review(rows), indent=2))
    except (OSError, ValueError, TypeError) as error:
        parser.exit(1, f"Trial capture refused: {error}\n")


if __name__ == "__main__":
    main()
