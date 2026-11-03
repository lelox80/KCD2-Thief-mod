"""Validate the bounded retail adjustment-sweep capture."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path

LIMIT = 4096
HEADER = {
    "event": "started",
    "probe": "clearance",
    "revision": 1,
    "motion_compiled": False,
    "record_limit": LIMIT,
}


def finite(value):
    return type(value) in (float, int) and math.isfinite(value)


def review(path):
    with path.open(encoding="utf-8") as capture:
        rows = []
        for line in capture:
            if len(rows) >= LIMIT + 2 or len(line) > 512:
                raise ValueError("Capture exceeds the native bounds")
            rows.append(json.loads(line))
    if len(rows) < 2 or rows[0] != HEADER or rows[-1].get("event") != "stopped":
        raise ValueError("Expected a complete clearance capture")
    footer = rows[-1]
    counts = Counter()
    directions = Counter()
    for sequence, row in enumerate(rows[1:-1], start=1):
        if row.get("event") != "sweep" or row.get("sequence") != sequence:
            raise ValueError(f"Invalid sweep sequence {sequence}")
        side = row["side"]
        if type(side) is not int or not 1 <= side <= 4 or row["tick"] <= 0 or row["thread"] <= 0:
            raise ValueError(f"Invalid sweep identity at {sequence}")
        directions[side] += 1
        vectors = [row[name] for name in ("center", "half_extent", "delta")]
        known = vectors[0] is not None
        if known:
            if any(type(v) is not list or len(v) != 3 or not all(map(finite, v)) for v in vectors):
                raise ValueError(f"Invalid sweep vectors at {sequence}")
        elif vectors != [None, None, None]:
            raise ValueError(f"Partial sweep vectors at {sequence}")
        if type(row["returned"]) is not bool:
            raise ValueError(f"Invalid return state at {sequence}")
        result = row["result"]
        if result is not None and (not finite(result) or not row["returned"]):
            raise ValueError(f"Invalid result at {sequence}")
        used = known and result is not None and 0 < result < abs(vectors[2][(side - 1) // 2])
        if row["used_by_adjustment"] is not used:
            raise ValueError(f"Inconsistent adjustment result at {sequence}")
        counts["unreadable"] += not known
        counts["exceptional"] += not row["returned"]
        counts["nonfinite"] += row["returned"] and result is None
        counts["used_by_adjustment"] += used
        counts["zero_return"] += result == 0
    total = len(rows) - 2
    if (
        type(footer["overflow"]) is not int
        or footer["overflow"] < 0
        or (footer["overflow"] and total != LIMIT)
        or footer["matched"] != total + footer["overflow"]
        or footer["unreadable"] != counts["unreadable"]
    ):
        raise ValueError("Capture counters do not reconcile")
    return {
        "sweeps": total,
        "sweeps_by_direction": dict(sorted(directions.items())),
        "unreadable": counts["unreadable"],
        "exceptional": counts["exceptional"],
        "nonfinite": counts["nonfinite"],
        "used_by_adjustment": counts["used_by_adjustment"],
        "zero_return": counts["zero_return"],
        "overflow": footer["overflow"],
        "player_association_established": False,
        "whole_body_clearance_established": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(review(args.capture), indent=2))
    except (ValueError, KeyError, TypeError, AttributeError, OSError) as error:
        parser.exit(1, f"Clearance capture refused: {error}\n")


if __name__ == "__main__":
    main()
