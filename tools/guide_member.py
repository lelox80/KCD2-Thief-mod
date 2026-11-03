"""Inspect the pinned retail ledge member or stage an exact local no-op copy.

No installation, process access, guide append, mount-path assumption or game call.
The staged file is not a mod package and does not prove VFS source selection.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA256 = "dfd631becc308ee4be1d460fbb0ba3c600582f8776e1e1bb8fa8de4a6ab1162c"
SOURCE_COUNTS = (4129, 27564)
MEMBER = "whdata_1"
MAX_MEMBER_BYTES = 16 * 1024 * 1024
LEDGE_FIELDS = ("entityId", "markersStartIdx", "markersCount", "ledgeCornerEndAdjustAmount", "Flags0", "Flags1")
MARKER_FIELDS = ("worldPosition", "facingDirection", "endOrCorner")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fnv1a64(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f"{value:016x}"


def integer(text, maximum, field):
    if text is None or re.fullmatch(r"[0-9]+", text) is None:
        raise ValueError("Invalid unsigned integer: " + field)
    value = int(text)
    if value > maximum:
        raise ValueError("Unsigned field exceeds native width: " + field)
    return value


def number(text, field):
    try:
        value = float(text)
        if not math.isfinite(value):
            raise ValueError()
        packed = struct.pack("<f", value)
        native = struct.unpack("<f", packed)[0]
        if not math.isfinite(native):
            raise ValueError()
        return native
    except (TypeError, ValueError, OverflowError) as error:
        raise ValueError("Invalid finite native float: " + field) from error


def vector(text, field):
    parts = text.split(",") if text is not None else []
    if len(parts) != 3:
        raise ValueError("Invalid three-component vector: " + field)
    return tuple(number(v, field) for v in parts)


def fields(record, tag, names):
    if record.tag != tag or record.attrib or tuple(c.tag for c in record) != names:
        raise ValueError("Unsupported record schema: " + tag)
    if any(len(c) or c.attrib or c.text is None for c in record):
        raise ValueError("Unsupported scalar schema: " + tag)
    return dict(zip(names, (c.text for c in record)))


def canonical_records(payload):
    """Return typed record bytes in serialized order, excluding native padding.

    The source is pinned before XML parsing. No coordinates or entity IDs are
    emitted by the CLI. Exact decimal-to-float serializer equivalence is a
    runtime question; these fingerprints are hypotheses for observation.
    """
    if len(payload) > MAX_MEMBER_BYTES or digest(payload) != SOURCE_SHA256:
        raise ValueError("Source member changed; repeat the retail assessment")
    return _parse_records(payload, SOURCE_COUNTS)


def _parse_records(payload, expected_counts):
    if b"<!DOCTYPE" in payload.upper() or b"<!ENTITY" in payload.upper():
        raise ValueError("XML declarations outside the pinned schema refused")
    root = ET.fromstring(payload)
    if root.tag != "Root" or root.attrib != {"version": "1"} or [e.tag for e in root] != ["LedgeManager", "GameProfileManager"]:
        raise ValueError("Unsupported Root/member schema")
    manager = root[0]
    if manager.attrib or [e.tag for e in manager] != ["Ledges", "Markers"]:
        raise ValueError("Unsupported LedgeManager schema")
    ledges, markers = manager
    if ledges.attrib or markers.attrib or (len(ledges), len(markers)) != expected_counts:
        raise ValueError("Pinned record counts/schema changed")
    ledge_bytes, marker_bytes = bytearray(), bytearray()
    next_marker = 0
    for record in ledges:
        row = fields(record, "Ledge", LEDGE_FIELDS)
        entity = integer(row["entityId"], 0xffffffff, "entityId")
        start = integer(row["markersStartIdx"], 0xffff, "markersStartIdx")
        count = integer(row["markersCount"], 0xffff, "markersCount")
        flag0 = integer(row["Flags0"], 0xffff, "Flags0")
        flag1 = integer(row["Flags1"], 0xffff, "Flags1")
        adjust = number(row["ledgeCornerEndAdjustAmount"], "ledgeCornerEndAdjustAmount")
        if start != next_marker or count < 2 or start + count > len(markers):
            raise ValueError("Invalid/noncontiguous marker range")
        next_marker = start + count
        ledge_bytes.extend(struct.pack("<IHHHHf", entity, start, count, flag0, flag1, adjust))
    if next_marker != len(markers):
        raise ValueError("Unreferenced marker records")
    for record in markers:
        row = fields(record, "Marker", MARKER_FIELDS)
        position = vector(row["worldPosition"], "worldPosition")
        facing = vector(row["facingDirection"], "facingDirection")
        if not any(facing):
            raise ValueError("Zero facingDirection")
        corner = integer(row["endOrCorner"], 1, "endOrCorner")
        marker_bytes.extend(struct.pack("<ffffffB", *position, *facing, corner))
    return bytes(ledge_bytes), bytes(marker_bytes)


def inspect(payload):
    ledges, markers = canonical_records(payload)
    return _describe_records(payload, ledges, markers)


def _describe_records(payload, ledges, markers):
    # Entity references use a special serializer and may be remapped on load.
    # Keep the full fingerprint for exact identity, plus an order-preserving
    # structural stream that omits only the four entity-ID bytes per ledge.
    structural = b"".join(ledges[start+4:start+16] for start in range(0, len(ledges), 16))
    return {
        "source_member": MEMBER, "source_sha256": digest(payload), "source_bytes": len(payload),
        "ledge_count": len(ledges) // 16, "marker_count": len(markers) // 25,
        "canonical_encoding": "little-endian ledges <IHHHHf; markers <ffffffB; serialized order; native padding excluded",
        "canonical_ledges_sha256": digest(ledges), "canonical_markers_sha256": digest(markers),
        "canonical_ledges_fnv1a64": fnv1a64(ledges), "canonical_markers_fnv1a64": fnv1a64(markers),
        "structural_ledges_encoding": "little-endian <HHHHf per ledge in serialized order; entityId excluded",
        "structural_ledges_sha256": digest(structural), "structural_ledges_fnv1a64": fnv1a64(structural),
        "other_root_data": "GameProfileManager preserved byte-for-byte with the complete member",
        "mount_path": "unknown; no package generated", "registered": False, "allows_added_climbs": False,
        "limitation": "Offline schema/fingerprints do not prove native serializer rounding, VFS selection, registration or body clearance.",
    }


def append_inactive(payload, profile="guide-append"):
    """Append one inactive guide at the specified measured wall."""
    base_ledges, base_markers = canonical_records(payload)
    ledge_count, marker_count = SOURCE_COUNTS
    guides = {
        "guide-append": ("guide-append",),
        "guide-tall": ("guide-tall",),
        "guide-walls": ("guide-tall", "guide-append"),
    }[profile]
    geometry = {
        "guide-append": (((2896.821826, 767.061523, 62.353310), (2897.421826, 767.061523, 62.353310)), "0,-1,0"),
        "guide-tall": (((2895.372195, 794.028068, 65.958130), (2895.302609, 794.523202, 65.958130)), "0.990268,0.139173,0"),
    }
    ledges, markers = [], []
    added_ledges, added_markers = bytearray(), bytearray()
    for guide in guides:
        start = marker_count + len(markers)
        ledges.append(
            f"\t\t\t<Ledge>\r\n\t\t\t\t<entityId>0</entityId>\r\n"
            f"\t\t\t\t<markersStartIdx>{start}</markersStartIdx>\r\n"
            "\t\t\t\t<markersCount>2</markersCount>\r\n"
            "\t\t\t\t<ledgeCornerEndAdjustAmount>0</ledgeCornerEndAdjustAmount>\r\n"
            "\t\t\t\t<Flags0>0</Flags0>\r\n\t\t\t\t<Flags1>0</Flags1>\r\n\t\t\t</Ledge>\r\n"
        )
        added_ledges.extend(struct.pack("<IHHHHf", 0, start, 2, 0, 0, 0))
        positions, facing = geometry[guide]
        normal = tuple(map(float, facing.split(",")))
        for x, y, z in positions:
            markers.append(
                "\t\t\t<Marker>\r\n"
                f"\t\t\t\t<worldPosition>{x:.6f},{y:.6f},{z:.6f}</worldPosition>\r\n"
                f"\t\t\t\t<facingDirection>{facing}</facingDirection>\r\n"
                "\t\t\t\t<endOrCorner>1</endOrCorner>\r\n\t\t\t</Marker>\r\n"
            )
            added_markers.extend(struct.pack("<ffffffB", x, y, z, *normal, 1))
    output = payload
    for boundary, records in ((b"\t\t</Ledges>", ledges), (b"\t\t</Markers>", markers)):
        if output.count(boundary) != 1:
            raise ValueError("Unexpected append boundary in pinned member")
        output = output.replace(boundary, "".join(records).encode("ascii") + boundary, 1)
    new_ledges, new_markers = _parse_records(output, (ledge_count + len(ledges), marker_count + len(markers)))
    if new_ledges != base_ledges + added_ledges or new_markers != base_markers + added_markers:
        raise ValueError("Append changed the base record prefix or the inactive guides")
    report = _describe_records(output, new_ledges, new_markers)
    report.update(profile=profile, base_member_sha256=SOURCE_SHA256,
                  appended_ledge_index=ledge_count, appended_marker_start=marker_count,
                  appended_side_flags=[0, 0], baseline_record_prefix_verified=True)
    return output, report


def read_member(archive):
    with zipfile.ZipFile(archive) as z:
        matches = [i for i in z.infolist() if i.filename.lower() == MEMBER]
        if len(matches) != 1 or matches[0].filename != MEMBER or matches[0].is_dir():
            raise ValueError("Expected exactly one case-exact source member")
        info = matches[0]
        if info.file_size > MAX_MEMBER_BYTES or info.flag_bits & 1:
            raise ValueError("Oversized/encrypted source member refused")
        payload = z.read(info)
    return payload, info.compress_type


def stage_noop(archive, output):
    payload, compression = read_member(archive)
    report = inspect(payload)
    output = Path(output).resolve()
    # Resolve the output before comparison, but retain the intended workspace
    # bases so a symlinked ignored root cannot redirect staging elsewhere.
    allowed = [ROOT.resolve() / name for name in (".local", "build")]
    if not any(output.is_relative_to(base) and output != base for base in allowed):
        raise ValueError("Stage destination must be a new folder under ignored .local/ or build/")
    output.mkdir(parents=True, exist_ok=False)
    with (output / MEMBER).open("xb") as stream:
        stream.write(payload)
    if (output / MEMBER).read_bytes() != payload:
        raise RuntimeError("Staged member byte verification failed")
    report.update({"stage": "byte-identical no-op member only", "source_zip_compression": compression,
                   "archive_created": False, "installed": False})
    with (output / "guide-member.json").open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    return report


def configured_archive():
    config = json.loads((ROOT / ".local/environment.json").read_text(encoding="utf-8-sig"))
    return Path(config["game"]) / "Data/Levels/kutnohorsko/level.pak"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("inspect", "stage-noop"), nargs="?", default="inspect")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if (args.command == "stage-noop") != (args.output is not None):
        parser.error("--output is required only for stage-noop")
    archive = args.archive if args.archive is not None else configured_archive()
    if args.command == "stage-noop":
        report = stage_noop(archive, args.output)
    else:
        payload, compression = read_member(archive)
        report = inspect(payload)
        report["source_zip_compression"] = compression
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
