"""Check whether the dry fixture was indexed and withheld before selection."""
import argparse
from collections import Counter
import json
from pathlib import Path

from analyze_ledge_admission import analyze, integer

FIXTURE = 4129
MISS = 0xFFFFFFFF


def owned(handle):
    return integer(handle, 0, MISS) and handle >> 16 == FIXTURE


def review(rows):
    if len(rows) < 2 or any(not isinstance(row, dict) for row in rows):
        raise ValueError("Missing dry guard records")
    header = rows[0]
    contract = (header.get("mode"), header.get("probe"), header.get("probe_revision"), header.get("hook_count"))
    if contract not in {("dry-guide-guard", "ledge-admission", 4, 5), ("dry-tall-preview", "ledge-admission", 4, 6)}:
        raise ValueError("Expected a dry registration or tall selection preview")
    stop = rows[-1]
    if stop.get("event") != "stopped" or stop.get("guard_remains_armed") is not True:
        raise ValueError("Expected a complete audit with the guard still armed")
    if not integer(stop.get("dry_activations")) or not integer(stop.get("guarded_queries")):
        raise ValueError("Missing dry registration counters")

    # All ordinary transaction and shutdown checks still apply.
    ordinary_header = dict(header, mode="observe-only", hook_count=4)
    report = analyze([ordinary_header, *rows[1:]])
    problems = report["review_problems"]
    samples = [row for row in rows if row.get("event") == "ledge_admission"]
    found = [row for row in samples if owned(row.get("query_handle"))]
    if any(owned(row["request"][1]) for row in samples
           if isinstance(row.get("request"), list) and len(row["request"]) >= 2):
        problems.append("A framework request used the fixture")
    if header.get("selection_preview") is True:
        if not integer(stop.get("withheld_selections")):
            raise ValueError("Missing selection preview counter")
        selected = [row for row in found if isinstance(row.get("selection"), list)
                    and len(row["selection"]) == 2 and row["selection"][1] == row["query_handle"]]
        if any(row.get("framework_requests") != 0 for row in found):
            problems.append("A fixture query reached framework admission")
        if stop["withheld_selections"] < len(selected):
            problems.append("Selection guard counters do not reconcile")
        choices = Counter(row["selection"][0] for row in selected)
        clones = stop.get("tall_parameter_clones")
        if contract[0]=="dry-tall-preview" and (not integer(clones) or clones<choices[4]):
            problems.append("Tall parameter copies do not account for the type 4 selections")
        return {
            "dry_activations": stop["dry_activations"],
            "observed_fixture_queries": len(found),
            "native_choices": dict(choices),
            "withheld_selections": stop["withheld_selections"],
            "index_results": stop.get("dry_index_results"),
            "tall_parameter_clones": clones,
            "vault_onto_selected": not problems and stop["dry_activations"] > 0 and choices[4] > 0,
            "review_problems": problems,
            "allows_live_movement": False,
        }
    withheld = [
        row for row in found
        if row.get("stage") == "selection_refused"
        and row.get("selection") == [-1, MISS]
        and row.get("framework_requests") == 0
    ]
    if len(withheld) != len(found):
        problems.append("An observed fixture query lacks a matching refusal")
    if stop["guarded_queries"] < len(found):
        problems.append("Fixture query counters do not reconcile")
    return {
        "dry_activations": stop["dry_activations"],
        "guarded_queries": stop["guarded_queries"],
        "observed_fixture_queries": len(found),
        "withheld_fixture_queries": len(withheld),
        "transactions": report["records"],
        "indexed_fixture_observed": not problems and stop["dry_activations"] > 0 and bool(withheld),
        "review_problems": problems,
        "allows_live_movement": False,
    }


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
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(1, f"Dry capture refused: {error}\n")


if __name__ == "__main__":
    main()
