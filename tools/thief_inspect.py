"""Bounded, read-only PE inspection of an owned Thief executable.

This intentionally does not decompile the game or edit the installation. Install
pefile and capstone in .local/tools/native_inspection/tooldeps. Redirect native
outputs to ignored .local/; tracked findings should contain observations only.
"""
from __future__ import annotations

import argparse
import bisect
import ctypes
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / ".local/tools/native_inspection/tooldeps"))
import capstone  # type: ignore
import pefile  # type: ignore

EXE_SHA256 = 'ee707ecd140dc88317b631de4eb0a6173754bdde8b5a15590d4c06e0471edbf9'
PACKAGE_SHA256 = 'c061289fdc634484badc780379992b6daa1827998b114d4b861338367d87fa35'


def inspect_dash_package(path: Path, decoder_path: Path) -> dict:
    """Read tables and four selected objects; decompress only their chunks.

    Format references: UEViewer a0bfb468, UnPackage3.cpp and its UE3 compressed
    archive reader. Only this inspected UE3 fork/version is accepted. The caller
    supplies its own local lzo1x_decompress_safe library; none ships with the mod.
    """
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != PACKAGE_SHA256:
        raise ValueError('ThiefGame.u differs from the inspected donor package')
    if struct.unpack_from("<IHH", raw) != (0x9E2A83C1, 721, 148):
        raise ValueError("Expected inspected Thief UE3 package version 721 / licensee 148")
    # Exact-version summary fields, not a general UE3/UE4/UE5 package reader.
    name_count, name_offset, export_count, export_offset, import_count, import_offset = struct.unpack_from("<6I", raw, 25)
    compression, chunk_count = struct.unpack_from("<2I", raw, 109)
    if compression != 2 or not 0 < chunk_count < 4096:
        raise ValueError("Expected bounded UE3 LZO chunks")
    chunks = [struct.unpack_from("<4I", raw, 117 + i * 16) for i in range(chunk_count)]
    library = ctypes.CDLL(str(decoder_path.resolve()))
    decode = library.lzo1x_decompress_safe
    decode.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                       ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p]
    decode.restype = ctypes.c_int
    cache = {}

    def chunk(index: int) -> bytes:
        if index in cache:
            return cache[index]
        _, size, offset, compressed = chunks[index]
        tag, block_size, _, uncompressed = struct.unpack_from("<4I", raw, offset)
        if tag != 0x9E2A83C1 or uncompressed != size or not 0 < size <= 16 * 1024 * 1024 or not 0 < block_size <= 1024 * 1024:
            raise ValueError("Invalid or unbounded compressed chunk")
        count = (size + block_size - 1) // block_size
        blocks = [struct.unpack_from("<2I", raw, offset + 16 + i * 8) for i in range(count)]
        cursor = offset + 16 + count * 8
        result = bytearray()
        for compressed_size, output_size in blocks:
            if not 0 < output_size <= block_size or cursor + compressed_size > len(raw):
                raise ValueError("Invalid compressed block")
            source = ctypes.create_string_buffer(raw[cursor:cursor + compressed_size])
            target = ctypes.create_string_buffer(output_size)
            length = ctypes.c_size_t(output_size)
            status = decode(source, compressed_size, target, ctypes.byref(length), None)
            if status != 0 or length.value != output_size:
                raise ValueError(f"LZO decode failed: status {status}, output {length.value}")
            result.extend(target.raw)
            cursor += compressed_size
        if len(result) != size or cursor != offset + compressed:
            raise ValueError("Compressed chunk boundaries do not match summary")
        cache[index] = bytes(result)
        return cache[index]

    def read(offset: int, size: int) -> bytes:
        result = bytearray()
        for index, (logical, length, _, _) in enumerate(chunks):
            if logical <= offset < logical + length:
                take = min(size, logical + length - offset)
                result.extend(chunk(index)[offset - logical:offset - logical + take])
                offset += take
                size -= take
                if size == 0:
                    return bytes(result)
        raise ValueError("Requested package range is not covered by chunks")

    names, cursor = [], 0
    table = read(name_offset, import_offset - name_offset)
    for _ in range(name_count):
        count = struct.unpack_from("<i", table, cursor)[0]
        cursor += 4
        if not 0 < abs(count) < 2048:
            raise ValueError("Invalid FString in name table")
        length = count if count > 0 else -count * 2
        value = table[cursor:cursor + length].decode("latin1" if count > 0 else "utf-16le").rstrip("\0")
        names.append(value)
        cursor += length + 8
    if cursor != len(table):
        raise ValueError("Name table does not end at import table")
    imports, exports = [], []
    table = read(import_offset, export_offset - import_offset)
    if len(table) != import_count * 28:
        raise ValueError("Import table boundary mismatch")
    for index in range(import_count):
        _, _, _, _, outer, name, number = struct.unpack_from("<7i", table, index * 28)
        imports.append({"outer": outer, "name": names[name], "number": number})
    depends_offset = struct.unpack_from("<I", raw, 49)[0]
    table, cursor = read(export_offset, depends_offset - export_offset), 0
    for index in range(export_count):
        cls, super_index, outer, name, number, archetype, flags, size, offset, export_flags = struct.unpack_from("<6iQ3i", table, cursor)
        cursor += 44
        count = struct.unpack_from("<i", table, cursor)[0]
        if not 0 <= count < 100:
            raise ValueError("Unbounded export net-object array")
        cursor += 4 + count * 4 + 16 + 4
        if export_flags & 8:  # Explicit Thief fork field in UEViewer.
            cursor += 4
        exports.append({"index": index + 1, "class": cls, "super": super_index,
                        "outer": outer, "name": names[name], "number": number,
                        "archetype": archetype, "size": size, "offset": offset})
    if cursor != len(table):
        raise ValueError("Export table does not end at dependency table")

    def object_name(index: int, depth: int = 0) -> str:
        if not index:
            return "ThiefGame"
        if depth > 32:
            raise ValueError("Cyclic object outer chain")
        obj = exports[index - 1] if index > 0 else imports[-index - 1]
        suffix = obj["name"] + (f"_{obj['number'] - 1}" if obj["number"] else "")
        return object_name(obj["outer"], depth + 1) + "." + suffix

    def properties(export: dict) -> dict:
        blob, cursor, result = read(export["offset"], export["size"]), 4, {}
        while cursor < len(blob):
            name_index, number = struct.unpack_from("<2i", blob, cursor)
            cursor += 8
            name = names[name_index]
            if name == "None":
                break
            type_index, _, size, array_index = struct.unpack_from("<4i", blob, cursor)
            cursor += 16
            kind = names[type_index]
            if kind in ("StructProperty", "ByteProperty"):
                cursor += 8
            if kind == "BoolProperty":
                value = bool(blob[cursor])
                cursor += 1
            else:
                value_bytes = blob[cursor:cursor + size]
                cursor += size
                if kind == "FloatProperty":
                    value = struct.unpack("<f", value_bytes)[0]
                elif kind in ("ObjectProperty", "ComponentProperty"):
                    value = object_name(struct.unpack("<i", value_bytes)[0])
                else:
                    value = {"serialized_size": size}
            result[name] = {"type": kind, "value": value, "array_index": array_index}
        if cursor != len(blob):
            raise ValueError(f"Property stream boundary mismatch for {export['name']}")
        return result

    selected = {}
    for export in exports:
        if export["name"] in ("Default__Thief3CSettings_Dash", "Settings_Dash",
                              "Default__ThiefHeroSettings", "Settings_ThiefHero"):
            parsed = properties(export)
            if "Hero" in export["name"]:
                parsed = {key: value for key, value in parsed.items() if key in ("mDashSettings", "mStaminaSettings")}
            selected[object_name(export["index"])] = {
                "export": export["index"], "offset": hex(export["offset"]),
                "bytes": export["size"], "properties": parsed,
            }
    return {"package_sha256": hashlib.sha256(raw).hexdigest(), "version": [721, 148],
            "table_counts": {"names": name_count, "imports": import_count, "exports": export_count},
            "objects": selected, "decompressed_chunk_indices": sorted(cache),
            "warning": "Serialized settings and bindings; runtime timing, units and collision behavior remain to be verified."}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    commands = parser.add_subparsers(dest="command", required=True)
    anchors = commands.add_parser("anchors", help="Find exact ASCII/UTF-16 names and RIP LEA references")
    anchors.add_argument("names", nargs="+")
    dump = commands.add_parser("function", help="Disassemble one bounded function/window")
    dump.add_argument("rva", type=lambda value: int(value, 0))
    dump.add_argument("--instructions", type=int, default=160)
    refs = commands.add_parser("refs", help="Find direct code calls, LEAs and absolute pointers to an RVA")
    refs.add_argument("rva", nargs="+", type=lambda value: int(value, 0))
    package = commands.add_parser("dash-settings", help="Read only Dash defaults, authored settings and hero binding from ThiefGame.u")
    package.add_argument("--lzo-dll", type=Path, required=True, help="Locally inspected LZO safe decoder library")
    args = parser.parse_args()
    if args.command == "dash-settings":
        print(json.dumps(inspect_dash_package(args.binary, args.lzo_dll), indent=2))
        return
    data = args.binary.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        parser.error('Executable differs from the inspected Thief donor')
    pe = pefile.PE(data=data, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXCEPTION"]])
    base = pe.OPTIONAL_HEADER.ImageBase
    runtime_functions = {entry.struct.BeginAddress:
                         (entry.struct.EndAddress, entry.struct.UnwindData)
                         for entry in getattr(pe, "DIRECTORY_ENTRY_EXCEPTION", [])}

    def unwind_root(begin: int, end: int, unwind: int) -> int:
        seen = set()
        while unwind not in seen:
            seen.add(unwind)
            header = pe.get_data(unwind, 4)
            if not (header[0] >> 3) & 4:
                return begin
            chain_offset = unwind + ((4 + 2 * header[2] + 3) & ~3)
            begin, end, unwind = struct.unpack("<3I", pe.get_data(chain_offset, 12))
        raise ValueError("Cyclic unwind chain")

    roots = {begin: unwind_root(begin, end, unwind)
             for begin, (end, unwind) in runtime_functions.items()}
    root_ends = {}
    for begin, (end, _) in runtime_functions.items():
        root = roots[begin]
        root_ends[root] = max(root_ends.get(root, end), end)
    functions = sorted((begin, end) for begin, (end, _) in runtime_functions.items())
    starts = [begin for begin, _ in functions]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True

    def containing(rva: int) -> tuple[int, int]:
        index = bisect.bisect_right(starts, rva) - 1
        if index >= 0 and rva < functions[index][1]:
            root = roots[functions[index][0]]
            return root, root_ends[root]
        return rva, rva + 256

    def ref_record(kind: str, rva: int, target: int) -> dict:
        return {"kind": kind, "rva": hex(rva), "target": hex(target),
                "function": [hex(value) for value in containing(rva)]}

    def code_refs(targets: set[int], include_calls: bool) -> list[dict]:
        result = []
        for section in pe.sections:
            if not section.Characteristics & 0x20000000:
                continue
            raw = section.get_data()
            for match in re.finditer(rb"[\x48\x4c][\x8d\x8b\x89\x3b\x39][\x05\x0d\x15\x1d\x25\x2d\x35\x3d](....)", raw, re.S):
                rva = section.VirtualAddress + match.start()
                target = rva + 7 + struct.unpack("<i", match[1])[0]
                if target in targets:
                    result.append(ref_record("rip_reference_candidate", rva, target))
            if include_calls:
                for match in re.finditer(rb"[\xe8\xe9](....)", raw, re.S):
                    rva = section.VirtualAddress + match.start()
                    target = rva + 5 + struct.unpack("<i", match[1])[0]
                    if target in targets:
                        result.append(ref_record("call_or_jump_candidate", rva, target))
        return result

    def strings_near(rva: int) -> list[str]:
        try:
            chunk = pe.get_data(rva, 160)
        except Exception:
            return []
        result = []
        for encoding in ("ascii", "utf-16le"):
            try:
                value = chunk.decode(encoding).split("\0", 1)[0]
                if len(value) > 2 and all(32 <= ord(char) < 127 for char in value):
                    result.append(value)
            except UnicodeDecodeError:
                pass
        return result

    if args.command == "anchors":
        found = []
        for name in args.names:
            for encoding in ("ascii", "utf-16le"):
                needle = (name + "\0").encode(encoding)
                for match in re.finditer(re.escape(needle), data):
                    found.append({"name": name, "encoding": encoding,
                                  "file_offset": hex(match.start()),
                                  "rva": hex(pe.get_rva_from_offset(match.start()))})
        result = {"sha256": hashlib.sha256(data).hexdigest(), "image_base": hex(base),
                  "anchors": found,
                  "references": code_refs({int(item["rva"], 16) for item in found}, False)}
        print(json.dumps(result, indent=2))
    elif args.command == "refs":
        result = code_refs(set(args.rva), True)
        for target in args.rva:
            for match in re.finditer(re.escape(struct.pack("<Q", base + target)), data):
                result.append(ref_record("absolute_pointer", pe.get_rva_from_offset(match.start()), target))
        print(json.dumps(result, indent=2))
    else:
        if not 1 <= args.instructions <= 1500:
            parser.error("--instructions must be between 1 and 1500")
        begin, end = containing(args.rva)
        print(f"IMAGE_BASE {base:#x} FUNCTION {begin:#x} {end:#x} SIZE {end-begin}")
        code = pe.get_data(begin, min(end - begin, 25000))
        insns = list(md.disasm(code, base + begin))
        selected = next((index for index, ins in enumerate(insns) if ins.address >= base + args.rva), 0)
        for ins in insns[max(0, selected - 12):selected + args.instructions]:
            annotations = []
            for operand in ins.operands:
                if operand.type == capstone.CS_OP_MEM and operand.mem.base == capstone.x86.X86_REG_RIP:
                    target = ins.address + ins.size + operand.mem.disp - base
                    annotations.extend(repr(text) for text in strings_near(target))
            print(f"{ins.address-base:#x} {ins.mnemonic:8} {ins.op_str} {' '.join(annotations)}")


if __name__ == "__main__":
    main()
