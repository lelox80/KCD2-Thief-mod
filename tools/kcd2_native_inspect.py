"""Bounded, read-only PE inspection for the pinned KCD2 retail movement study.

Uses Capstone/pefile already unpacked under .local/tools/native_inspection/tooldeps.
It never opens a process or writes the input binary. Results are build-specific
static leads, not permission to invoke a native function or assume an ABI.
"""
from __future__ import annotations

import argparse
import bisect
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / ".local/tools/native_inspection/tooldeps"))
import capstone  # noqa: E402
import pefile  # noqa: E402

PINNED_SHA256 = "bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7"


def number(value: str) -> int:
    return int(value, 0)


class Image:
    def __init__(self, path: Path):
        self.data = path.read_bytes()
        self.sha256 = hashlib.sha256(self.data).hexdigest()
        if self.sha256 != PINNED_SHA256:
            raise SystemExit(f"Refusing unpinned binary: SHA-256 {self.sha256}")
        self.pe = pefile.PE(data=self.data, fast_load=True)
        self.pe.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXCEPTION"]]
        )
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.funcs = sorted(
            (e.struct.BeginAddress, e.struct.EndAddress)
            for e in self.pe.DIRECTORY_ENTRY_EXCEPTION
        )
        self.starts = [start for start, _ in self.funcs]
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        self.md.detail = True

    def bounds(self, rva: int) -> tuple[int, int] | None:
        i = bisect.bisect_right(self.starts, rva) - 1
        return self.funcs[i] if i >= 0 and rva < self.funcs[i][1] else None

    def section(self, rva: int):
        return self.pe.get_section_by_rva(rva)

    def read(self, rva: int, size: int) -> bytes:
        section = self.section(rva)
        if section is None or rva + size > section.VirtualAddress + section.SizeOfRawData:
            raise ValueError(f"RVA outside backed section: {rva:#x}, length {size:#x}")
        return self.pe.get_data(rva, size)

    def text(self, rva: int, max_size: int = 160) -> str | None:
        section = self.section(rva)
        if section is None:
            return None
        size = min(max_size, section.VirtualAddress + section.SizeOfRawData - rva)
        if size <= 0:
            return None
        value = self.pe.get_data(rva, size).split(b"\0")[0]
        return value.decode("ascii") if value and all(32 <= c < 127 for c in value) else None

    def raw_matches(self, pattern: bytes, executable: bool | None = None):
        for section in self.pe.sections:
            if executable is not None and bool(section.Characteristics & 0x20000000) != executable:
                continue
            for match in re.finditer(pattern, section.get_data(), re.S):
                yield section.VirtualAddress + match.start(), match

    def instructions(self, rva: int, limit: int = 120, context: int = 0):
        bounds = self.bounds(rva)
        start, end = bounds or (rva, rva + min(1024, limit * 15))
        if rva - start > 32768:
            raise ValueError("Request is too far into a large function; choose a bounded entry")
        code_end = min(end, rva + limit * 15, start + 32768)
        decoded = list(self.md.disasm(self.read(start, code_end - start), self.base + start))
        if bounds is None:
            padding = next((i for i, ins in enumerate(decoded) if ins.mnemonic == "int3"), len(decoded))
            decoded = decoded[:padding]
        index = next((i for i, ins in enumerate(decoded) if ins.address - self.base >= rva), len(decoded))
        return bounds, decoded[max(0, index - context):index + limit]

    def instruction_record(self, ins):
        item = {"rva": hex(ins.address - self.base), "bytes": ins.bytes.hex(),
                "mnemonic": ins.mnemonic, "operands": ins.op_str}
        refs = []
        for operand in ins.operands:
            if operand.type == capstone.CS_OP_MEM and operand.mem.base == capstone.x86.X86_REG_RIP:
                target = ins.address + ins.size + operand.mem.disp - self.base
                ref = {"rva": hex(target)}
                value = self.text(target)
                if value:
                    ref["text"] = value
                refs.append(ref)
            elif ins.mnemonic in ("call", "jmp") and operand.type == capstone.CS_OP_IMM:
                refs.append({"code_rva": hex(operand.imm - self.base)})
        if refs:
            item["references"] = refs
        return item

    def riprefs(self, targets: set[int]):
        # Raw candidates are decoded and their RIP operand verified. This is a
        # targeted operand scan, not a complete executable disassembly.
        pattern = rb"[\x40-\x4f]?[\x8b\x8d\x89\x3b\x39\x85\xff][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]...."
        found = {}
        for rva, match in self.raw_matches(pattern, executable=True):
            candidate = rva + len(match.group()) + struct.unpack("<i", match.group()[-4:])[0]
            if candidate not in targets:
                continue
            for ins in self.md.disasm(self.read(rva, 15), self.base + rva, count=1):
                for operand in ins.operands:
                    if operand.type == capstone.CS_OP_MEM and operand.mem.base == capstone.x86.X86_REG_RIP:
                        target = ins.address + ins.size + operand.mem.disp - self.base
                        if target in targets:
                            bounds = self.bounds(rva)
                            found[(rva, target)] = {"rva": hex(rva), "target": hex(target),
                                "function": [hex(n) for n in bounds] if bounds else None,
                                "instruction": self.instruction_record(ins)}
        return list(found.values())

    def rtti(self, query: str):
        types = []
        for rva, match in self.raw_matches(rb"\.\?A[VU][\x20-\x7e]{1,180}\x00", executable=False):
            name = match.group().rstrip(b"\0").decode("ascii")
            if query.lower() not in name.lower():
                continue
            td = rva - 16
            locators = []
            for typeref, _ in self.raw_matches(re.escape(struct.pack("<I", td)), executable=False):
                col = typeref - 12
                try:
                    sig, offset, cd, typ, hierarchy, selfrva = struct.unpack("<6I", self.read(col, 24))
                except ValueError:
                    continue
                if sig != 1 or selfrva != col or typ != td:
                    continue
                tables = [hex(p + 8) for p, _ in self.raw_matches(
                    re.escape(struct.pack("<Q", self.base + col)), executable=False)]
                locators.append({"rva": hex(col), "object_offset": hex(offset),
                    "hierarchy_rva": hex(hierarchy), "vtables": tables})
            types.append({"name": name, "type_descriptor_rva": hex(td), "locators": locators})
            if len(types) >= 80:
                break
        return types


def run(args, image: Image):
    if args.command == "fingerprint":
        return {"sha256": image.sha256, "image_base": hex(image.base),
            "unwind_function_count": len(image.funcs)}
    if args.command == "strings":
        result = []
        for rva, match in image.raw_matches(rb"[\x20-\x7e]{4,240}\x00", executable=False):
            value = match.group().rstrip(b"\0").decode("ascii")
            if any(q.lower() in value.lower() for q in args.queries):
                result.append({"rva": hex(rva), "text": value})
                if len(result) >= args.limit:
                    break
        return result
    if args.command == "rtti":
        return image.rtti(args.query)
    if args.command == "riprefs":
        return image.riprefs(set(args.rvas))
    if args.command == "pointerrefs":
        result = []
        for target in args.rvas:
            for rva, _ in image.raw_matches(re.escape(struct.pack("<Q", image.base + target)), executable=False):
                result.append({"rva": hex(rva), "target": hex(target)})
        return result
    if args.command == "dump":
        bounds, insns = image.instructions(args.rva, args.limit, args.context)
        return {"function": [hex(n) for n in bounds] if bounds else None,
            "instructions": [image.instruction_record(ins) for ins in insns]}
    if args.command == "vtable":
        result = []
        for offset in range(args.offset, args.offset + args.count * 8, 8):
            value, = struct.unpack("<Q", image.read(args.rva + offset, 8))
            rva = value - image.base
            section = image.section(rva)
            if section is None or not section.Characteristics & 0x20000000:
                result.append({"offset": hex(offset), "end_reason": "next value does not point into executable section"})
                break
            bounds = image.bounds(rva)
            result.append({"offset": hex(offset), "rva": hex(rva),
                "function": [hex(n) for n in bounds] if bounds else None})
        return result
    if args.command in ("callrefs", "coderefs"):
        result = []
        targets = set(args.rvas)
        pattern = rb"\xe8(....)" if args.command == "callrefs" else rb"[\xe8\xe9](....)"
        for rva, match in image.raw_matches(pattern, executable=True):
            target = rva + 5 + struct.unpack("<i", match[1])[0]
            if target not in targets:
                continue
            bounds = image.bounds(rva)
            # Confirm candidate lies on a decoded instruction boundary when the
            # unwind fragment is small enough. Cold fragments may have no record.
            boundary = None
            if bounds and rva - bounds[0] <= 32768:
                decoded = image.md.disasm(image.read(bounds[0], rva + 5 - bounds[0]), image.base + bounds[0])
                boundary = any(ins.address == image.base + rva and ins.mnemonic in ("call", "jmp") for ins in decoded)
            if boundary is False:
                continue
            result.append({"rva": hex(rva), "target": hex(target),
                "function": [hex(n) for n in bounds] if bounds else None,
                "kind": "call" if match.group()[0] == 0xe8 else "jmp",
                "instruction_boundary_verified": boundary})
        return result
    if args.command == "patterns":
        result = {}
        for label, pattern in {
            "script_update": "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 3D ? ? ? ? 48 8B F1 33 D2",
            "script_init_call": "E8 ? ? ? ? 84 C0 74 ? E8 ? ? ? ? 41 38 BE",
            "ray_world_intersection_call": "E8 ? ? ? ? 48 FF 03 48 81 C4",
            "old_player_ctor": "40 55 53 56 57 41 56 48 8B EC 48 81 EC ? ? ? ? 48 8B D9 E8 ? ? ? ? 33 F6",
            "old_movement_ctor": "48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC ? 48 8B F9 E8",
        }.items():
            regex = b"".join(b"." if t == "?" else re.escape(bytes([int(t, 16)])) for t in pattern.split())
            matches = []
            for rva, _ in image.raw_matches(regex, executable=True):
                item = {"rva": hex(rva)}
                if image.read(rva, 1) == b"\xe8":
                    item["call_target_rva"] = hex(rva + 5 + struct.unpack("<i", image.read(rva + 1, 4))[0])
                matches.append(item)
            result[label] = matches
        return result
    raise ValueError(args.command)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--output", type=Path, help="JSON evidence destination; use ignored .local/native/kcd2")
    parser.add_argument("--plain", action="store_true", help="Compact instruction text for dump; JSON is still saved")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("fingerprint")
    sub.add_parser("patterns")
    strings = sub.add_parser("strings")
    strings.add_argument("queries", nargs="+")
    strings.add_argument("--limit", type=int, default=80)
    rtti = sub.add_parser("rtti")
    rtti.add_argument("query")
    for name in ("riprefs", "callrefs", "coderefs", "pointerrefs"):
        cmd = sub.add_parser(name)
        cmd.add_argument("rvas", nargs="+", type=number)
    dump = sub.add_parser("dump")
    dump.add_argument("rva", type=number)
    dump.add_argument("--limit", type=int, default=120)
    dump.add_argument("--context", type=int, default=0)
    vtable = sub.add_parser("vtable")
    vtable.add_argument("rva", type=number)
    vtable.add_argument("--offset", type=number, default=0)
    vtable.add_argument("--count", type=int, default=32)
    args = parser.parse_args()
    if hasattr(args, "limit") and not 1 <= args.limit <= 500:
        parser.error("--limit must be 1..500")
    if hasattr(args, "count") and not 1 <= args.count <= 256:
        parser.error("--count must be 1..256")
    if hasattr(args, "context") and not 0 <= args.context <= 40:
        parser.error("--context must be 0..40")
    binary = args.binary
    if binary is None:
        environment = json.loads((ROOT / ".local/environment.json").read_text(encoding="utf-8-sig"))
        binary = Path(environment["game"]) / "Bin/Win64MasterMasterSteamPGO/WHGame.dll"
    image = Image(binary)
    report = {"warning": "Pinned static evidence only. No process access, ABI guarantee or runtime behavior proof.",
        "sha256": image.sha256, "command": args.command, "result": run(args, image)}
    encoded = json.dumps(report, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded + "\n", encoding="utf-8")
    if args.plain and args.command == "dump":
        print("FUNCTION", report["result"]["function"])
        for ins in report["result"]["instructions"]:
            print(ins["rva"], ins["mnemonic"], ins["operands"],
                  json.dumps(ins.get("references", [])))
    else:
        print(encoded)


if __name__ == "__main__":
    main()
