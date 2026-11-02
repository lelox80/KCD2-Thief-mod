"""Read-only pinned-retail bridge ABI evidence, never process access or mutation.

Reference-project signatures locate candidates only. Function names and layouts
need corroborating original instructions and, before use, a controlled runtime
capture. Extracted proprietary bytes are written only below ignored .local.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import struct

from kcd2_native_inspect import Image, ROOT

PATTERNS = {
    "script_update": "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 3D ? ? ? ? 48 8B F1 33 D2",
    "lua_getfield_call": "E8 ? ? ? ? 44 8D 7D",
    "lua_pcall_call": "E8 ? ? ? ? 48 8B 4E ? 8B D7 8B D8",
    "lua_index2adr": "85 D2 7F ? B8",
    "entity_vtable": "48 8D 05 ? ? ? ? 48 89 01 4C 89 A1 A0 00 00 00",
    "lua_checkstack_call": "E8 ? ? ? ? 85 C0 75 ? 48 8D 15 ? ? ? ? 48 8B CF E8 ? ? ? ? 80 7B",
}


def locate(image: Image):
    results = {}
    for label, pattern in PATTERNS.items():
        regex = b"".join(b"." if v == "?" else re.escape(bytes([int(v, 16)])) for v in pattern.split())
        matches = []
        for rva, match in image.raw_matches(regex, executable=True):
            record = {"rva": hex(rva), "bytes": match.group().hex()}
            if label.endswith("_call"):
                record["target_rva"] = hex(rva + 5 + struct.unpack("<i", image.read(rva + 1, 4))[0])
            elif label == "entity_vtable":
                record["target_rva"] = hex(rva + 7 + struct.unpack("<i", image.read(rva + 3, 4))[0])
            matches.append(record)
        results[label] = matches
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("locate", "dump", "callrefs", "rtti", "vtable", "exports", "api-candidates", "leaf-candidates", "lua-region", "closure-candidates", "manifest"))
    parser.add_argument("values", nargs="*")
    parser.add_argument("--limit", type=int, default=160)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[a-z0-9-]{1,80}", args.label):
        parser.error("label must use lowercase letters, digits and hyphens")
    if not 1 <= args.limit <= 500:
        parser.error("limit must be 1..500")
    env = json.loads((ROOT / ".local/environment.json").read_text(encoding="utf-8-sig"))
    image = Image(Path(env["game"]) / "Bin/Win64MasterMasterSteamPGO/WHGame.dll")
    if args.command == "manifest":
        addresses = {
            "script_update": (0xa26b94, "uintptr_t __fastcall(void* script_system); opaque RAX preserved"),
            "lua_getfield": (0xad496c, "void __fastcall(lua_State*, int idx, const char* key)"),
            "lua_setfield": (0xa26d60, "void __fastcall(lua_State*, int idx, const char* key)"),
            "lua_type": (0x715e00, "int __fastcall(lua_State*, int idx)"),
            "lua_tolstring": (0x718a18, "const char* __fastcall(lua_State*, int idx, size_t* len)"),
            "lua_touserdata": (0x71805c, "void* __fastcall(lua_State*, int idx)"),
            "lua_pushcclosure": (0xb9663c, "void __fastcall(lua_State*, int (__fastcall*)(lua_State*), int n)"),
            "lua_pcall": (0x713914, "int __fastcall(lua_State*, int nargs, int nresults, int errfunc)"),
            "lua_checkstack": (0xb96168, "int __fastcall(lua_State*, int extra)"),
            "lua_iscfunction": (0x3994030, "int __fastcall(lua_State*, int idx)"),
            "actor_base_init": (0x7070b8, "static evidence only; not called by bridge"),
            "actor_init": (0xae86e0, "static evidence only; not called by bridge"),
            "actor_get_entity": (0x602360, "static evidence only; not called by bridge"),
            "controller_ctor": (0xf2f688, "static evidence only; not called by bridge"),
            "entity_get_id": (0x1a76130, "static evidence only; not called by bridge"),
            "system_get_frame_time": (0x39a7d54, "static evidence only; Lua wrapper returns float"),
            "timer_get_frame_time": (0x5d5f7c, "static evidence only; float __fastcall(void* timer, int timer_index)"),
            "timer_is_paused": (0x2468d3c, "static evidence only; bool __fastcall(void* timer, int timer_index)"),
        }
        result = {
            "functions": {name: {"rva": hex(rva), "first16": image.read(rva,16).hex(), "prototype": proto}
                          for name, (rva, proto) in addresses.items()},
            "lua_layout": {"script_system_state": "0x10", "top": "0x10", "base": "0x18", "stack_last": "0x38", "tvalue_stride": 16,
                           "global_index": -10002, "type_none": -1, "type_nil": 0, "type_lightuserdata": 2, "type_string": 4, "type_table": 5, "type_function": 6,
                           "stack_restore": "Record scalar (top-base)/16 only. After calls use CURRENT base; require original height within current top; shrink top only. Never retain old stack pointers across calls.",
                           "stack_growth": "Use retail lua_checkstack(L,8), require nonzero, then recapture all pointers. No own allocator or foreign Lua object code."},
            "identity": {"controller_vtable_rva": "0x3e74ac0", "player_vtable_rva": "0x3aa4b98", "entity_vtable_rva": "0x4072c78",
                         "player_return_rva": "0x82faa8", "controller_actor": "0x8", "actor_controller": "0x180", "actor_entity": "0x38", "actor_entity_id": "0x30", "entity_id": "0xc",
                         "required": "Known vtables, matching player return, actor.controller==hook controller, actor.entityID==entity.id==lightuserdata token (nonzero uint32), fresh lifecycle snapshot. Runtime matching capture required before movement."},
            "timer": {"vtable_rva": "0x3a6ccd0", "frame_time_slot": "0x48", "is_paused_slot": "0xa8", "lua_timer_index": 0,
                      "pause_byte": "0x54", "enabled_byte": "0x60", "game_frame_delta_float": "0x48", "other_timer_delta_float": "0x4c",
                      "proof": "System wrapper sets EDX=0 and calls slot+0x48. Timer returns zero when disabled or index0 paused byte nonzero; otherwise game frame float. Independent pause getter returns byte+0x54 for index0.",
                      "limitation": "Positive game delta is static gate evidence. Runtime pause/resume timing and snapshot expiry still require controlled test."},
        }
    elif args.command == "locate":
        result = locate(image)
    elif args.command == "dump":
        rva = int(args.values[0], 0)
        bounds, instructions = image.instructions(rva, args.limit)
        result = {"function": [hex(v) for v in bounds] if bounds else None,
                  "instructions": [image.instruction_record(i) for i in instructions]}
    elif args.command == "callrefs":
        targets = {int(v, 0) for v in args.values}
        result = []
        for rva, match in image.raw_matches(rb"\xe8....", executable=True):
            target = rva + 5 + struct.unpack("<i", match.group()[1:])[0]
            if target in targets:
                result.append({"rva": hex(rva), "target": hex(target),
                               "function": [hex(v) for v in image.bounds(rva)] if image.bounds(rva) else None})
    elif args.command == "rtti":
        result = image.rtti(args.values[0])
    elif args.command == "vtable":
        rva = int(args.values[0], 0)
        result = []
        for slot in range(args.limit):
            target = struct.unpack("<Q", image.read(rva + 8 * slot, 8))[0] - image.base
            result.append({"slot": slot, "offset": hex(slot * 8), "target_rva": hex(target),
                           "prologue": image.read(target, 16).hex() if image.section(target) else None})
    elif args.command == "closure-candidates":
        starts = set()
        for rva, _ in image.raw_matches(rb"\xc7[\x40-\x47]\x08\x06\x00\x00\x00", executable=True):
            bounds = image.bounds(rva)
            if bounds and bounds[1]-bounds[0] <= 2048: starts.add(bounds)
        result = {}
        for begin, end in sorted(starts):
            instructions = list(image.md.disasm(image.read(begin,end-begin),image.base+begin))
            if any(i.mnemonic == "mov" and re.search(r"qword ptr \[r[a-z0-9]+ \+ 0x20\], r[a-z0-9]+$", i.op_str) for i in instructions):
                result[hex(begin)] = [image.instruction_record(i) for i in instructions]
    elif args.command == "leaf-candidates":
        patterns = {"top_getter": rb"\x48\xc1\xf8\x04\xc3", "entity_id_actor": rb"\x48\x8b[\x41\x49]\x38\x8b[\x40\x41]\x0c",
                    "top_getter_near_ret": rb"\x48\xc1\xf8\x04.{0,10}\xc3"}
        result = {}
        for label, pattern in patterns.items():
            found = []
            for rva, _ in image.raw_matches(pattern, executable=True):
                bounds = image.bounds(rva)
                begin, end = bounds or (rva-24, rva+12)
                if end-begin > 256: continue
                found.append({"rva": hex(rva), "function": [hex(begin), hex(end)], "instructions": [image.instruction_record(i) for i in image.md.disasm(image.read(begin,end-begin),image.base+begin)]})
                if len(found) >= args.limit: break
            result[label] = found
    elif args.command == "lua-region":
        result = {}
        for begin, end in image.funcs:
            if not 0x711000 <= begin < 0x71c000 or end-begin > 768: continue
            instructions = list(image.md.disasm(image.read(begin,end-begin),image.base+begin))
            if any("[rax + 0x20]" in i.op_str or "[rcx + 0x10]" in i.op_str and i.mnemonic == "mov" for i in instructions):
                result[hex(begin)] = [image.instruction_record(i) for i in instructions]
    elif args.command == "api-candidates":
        candidates = json.loads((ROOT / ".local/native/bridge-abi/index2adr-callers.json").read_text())["result"]
        starts = sorted({int(v["function"][0], 0) for v in candidates if v["function"]
                         and int(v["function"][1], 0) - int(v["function"][0], 0) <= 256})
        result = {}
        for start in starts:
            end = image.bounds(start)[1]
            result[hex(start)] = [image.instruction_record(i) for i in image.md.disasm(image.read(start, end-start), image.base+start)]
        patterns = {"gettop_shape": rb"\x48\x8b\x41\x10\x48\x2b\x41\x18",
                    "settop_shape": rb"\x85\xd2.{0,12}\x48\x8b\x41\x18"}
        for label, pattern in patterns.items():
            result[label] = [{"rva": hex(rva), "function": [hex(v) for v in image.bounds(rva)] if image.bounds(rva) else None}
                             for rva, _ in image.raw_matches(pattern, executable=True)]
    else:
        image.pe.parse_data_directories(directories=[0])
        result = [{"name": e.name.decode("ascii", "replace") if e.name else None, "rva": hex(e.address)}
                  for e in getattr(getattr(image.pe, "DIRECTORY_ENTRY_EXPORT", None), "symbols", [])]
    output = ROOT / ".local/native/bridge-abi" / (args.label + ".json")
    output.parent.mkdir(parents=True, exist_ok=True)
    report = {"source_sha256": image.sha256, "image_base": hex(image.base),
              "warning": "Static original-byte evidence; no runtime ABI guarantee.", "result": result}
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.command == "dump":
        print("Function", result["function"])
        for item in result["instructions"]:
            print(item["rva"], item["mnemonic"], item["operands"], json.dumps(item.get("references", [])))
    else:
        print(json.dumps(result, indent=2))
    print("Saved", output)


if __name__ == "__main__":
    main()
