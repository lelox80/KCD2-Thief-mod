# State bridge: pinned native evidence

Read-only static investigation, 2026-10-17. Target is retail KCD2 1.5.6, Steam build 23914554, `WHGame.dll` SHA-256 `bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7`, image base `0x180000000`. These offsets do not apply to the installed 1.5.5 development tools or another retail build.

`tools/inspect_bridge_abi.py` verifies the whole original file hash before bounded PE/Capstone reads. It never opens a process or changes a game file. `manifest --label retail156-abi` reproduces `.local/native/bridge-abi/retail156-abi.json`, containing the exact first 16 bytes and inferred prototypes for each selected entry. Proprietary instruction dumps remain in ignored `.local/native/bridge-abi/`.

## Thin Lua bridge

Retail numeric representation is **float32**. Original RVA `0x41bfa0` resolves a value through `0x7170ec`, checks numeric tag 3 and returns `movss xmm0, dword ptr [rax]`. RVA `0xb9601c` checks tag 3 and converts with `cvttss2si rax, dword ptr [rax]`. This independently explains why the protocol-1 25-bit mask failed: 16,777,215 is exact but 33,554,431 rounds to 33,554,432. Raising the parser ceiling would lose gate identities. Protocol 2 transports gate-by-gate ASCII bits and limits counters to 8,388,607; uint32 masks are reconstructed only in native code. Private corroboration: `lua-number-read-float32.json`, `lua-integer-from-float32.json` under the directory below. The owned separate float32 Lua fixture runtime emits packets that the native parser tests consume directly.

The inspected KCD2ModLoader source provided locator signatures. Every selected entry below was then decoded in the pinned original retail binary and checked against its arguments and operations. No Lua APIs are exported by this DLL. No separately compiled Lua runtime, allocator, nil object, table implementation or closure implementation should operate on the game's Lua state.

| Operation | Retail RVA | Original-byte corroboration |
|---|---|---|
| Script system update | `0xa26b94` | Updates `_time`, `_frametime`, `_aitick`; reads state from script system `+0x10`; tail-calls a timer update |
| `lua_getfield` | `0xad496c` | Resolves index, interns R8 string key, table lookup, pushes one 16-byte value |
| `lua_setfield` | `0xa26d60` | Resolves index, interns R8 string key, table assignment, pops one 16-byte value |
| `lua_type` | `0x715e00` | Resolves index; returns `-1` for the game's nil-object sentinel, otherwise value type at `+8` |
| `lua_tolstring` | `0x718a18` | R8 is optional `size_t*`; type 4 string length at object `+0x10`, payload at `+0x18`; conversion has a cold branch |
| `lua_touserdata` | `0x71805c` | Type 2 returns raw pointer value; type 7 returns allocated userdata payload; all other types return null |
| `lua_pushcclosure` | `0xb9663c` | RDX callback, R8D upvalue count; engine closure allocation/GC; callback stored at closure `+0x20`; type 6 push |
| `lua_pcall` | `0x713914` | RCX state, EDX argument count, R8D result count, R9D error-handler index; computes function/error stack offsets and invokes protected engine call |
| `lua_checkstack` | `0xb96168` | RCX state, EDX extra count; checks total slots against `0x800`, grows through engine when necessary, enlarges current call-frame limit; returns integer success |
| `lua_iscfunction` | `0x3994030` | Index resolution, type 6 and nonzero closure `isC` byte |

All use the Windows x64 ABI. `lua_pcall` and `lua_checkstack` return `int`; field operations and closure push return `void`. The string operation returns `const char*`, userdata returns `void*`, type and is-C-function return `int`. An owned native callback is `int(lua_State*)`. Number conversion is intentionally absent: original retail instructions use single-precision Lua numbers in inspected paths, so an assumed standard double-number Lua build would be wrong.

Preserve the update hook's opaque integer return value and call its original exactly once. Only call Lua on that observed script-update thread, after the original. A movement worker must consume copied snapshots and never touch Lua or retain request/actor pointers.

### Stack bookkeeping

`lua_gettop` and `lua_settop` were not selected as callable entries; their behavior is heavily inlined. The selected engine functions independently establish this small layout:

| Field | State offset / value |
|---|---|
| First free stack slot (`top`) | `+0x10` |
| Current function's stack base | `+0x18` |
| Last usable stack boundary | `+0x38` |
| Value stride | 16 bytes |
| Global pseudo-index | `-10002` |
| Type tags needed by bridge | nil 0, lightuserdata 2, string 4, table 5, function 6; absent index -1 |

`lua_checkstack` explicitly subtracts base from top and shifts right by four. `lua_getfield`, `lua_setfield`, `lua_pcall`, `index2adr` and closure push independently use the same stride and top field. `lua_checkstack` reads the stack boundary at `+0x38` and adjusts the engine call-frame top.

Call the retail `lua_checkstack(L, 8)` and require success before temporary pushes. Capture only the scalar current height `(top-base)/16`. After any engine calls, reread the **current** base/top, validate alignment and bounds, and restore by shrinking top to current base plus the original height. Never keep a stack pointer across engine calls: allocation can move the stack. Never extend or allocate a stack using this layout. Strict type checks before reading packet strings or lightuserdata avoid implicit conversion/error paths.

An owned publisher global can remain pinned but inert after stop; its callback must reject publication when stopped. This avoids modifying Lua globals from a shutdown/worker thread. Pinning the DLL until process exit also prevents the Lua global or an in-flight detour from referencing unloaded code. Refuse an existing unexpected global rather than replacing another mod's function.

## Matching a state packet to the actual request owner

The previous observation capture proved the player return site and request forwarding. The new static trace provides a chain that can be read without invoking engine getters on worker threads:

1. Require request caller return RVA `0x82faa8` and controller vtable RVA `0x3e74ac0`.
2. Read actor from controller `+8`. The constructor at `0xf2f688` stores its actor argument there.
3. Require actor primary vtable RVA `0x3aa4b98` (RTTI `C_Player`) and actor `+0x180` equal to the current controller.
4. Read entity pointer from actor `+0x38`; require CEntity vtable RVA `0x4072c78`.
5. Read uint32 ID at entity `+0x0c` and require nonzero. Entity virtual slot `+8` points to `0x1a76130`, whose leaf body reads exactly that field.
6. Require actor's uint32 ID at `+0x30` to equal the entity ID and the publisher's verified lightuserdata ID.

Actor base initialization at `0x7070b8` copies game object `+0x18` into actor `+0x38`, and game object ID `+8` into actor `+0x30`. Actor initialization at `0xae86e0` invokes GetClass, GetFlags and SetFlags through actor `+0x38`, matching the independently found CEntity vtable. The player entity accessor at `0x602360` simply returns actor `+0x38`. Actor initialization also constructs the movement controller and stores it at `+0x180`.

Pass the original Lua `player.id` as a separate publisher argument. Accept it only if the game's `lua_type` reports **lightuserdata (2)** and the game's `lua_touserdata` returns a nonzero value fitting uint32. Do not parse `tostring(userdata)`, accept allocated userdata, or treat an arbitrary address as an ID. The static trace proves the native half; matching the actual Lua token and actual owner needs a read-only retail capture. Until that capture establishes repeated equality, movement remains disabled. A mismatch, unreadable pointer or vtable variation must refuse the request.

## Game time and pause

The exact System.GetFrameTime registration selects wrapper RVA `0x39a7d54`. Its success branch loads the timer from binding object `+0x80`, sets timer argument EDX to **0**, and calls timer virtual `+0x48`. System.GetCurrTime wrapper `0x39a7270` uses the same timer and index 0 through slot `+0x18`; the async-time registration is separate.

RTTI identifies CTimer vtable RVA `0x3a6ccd0`. Its frame-time slot `+0x48` points to `0x5d5f7c`. Original instructions return zero if enabled byte `+0x60` is zero. For index 0, they also return zero if pause byte `+0x54` is nonzero. Otherwise the function returns float `+0x48`. Index 1 instead returns float `+0x4c`. An independent timer-pause getter at slot `+0xa8`, RVA `0x2468d3c`, returns pause byte `+0x54` for index 0; the enabled getter at `0x1a72e80` returns byte `+0x60`.

Therefore a finite positive **System.GetFrameTime()** is a build-specific gate consistent with an enabled, unpaused game timer; it is not an async frame timer in this retail wrapper. This does not establish update/request ordering or every menu's lifecycle behavior. The read-only bridge test must capture pause and resume, verify no eligible paused snapshots, expire snapshots by a monotonic clock, and cancel on long request/publication gaps. No active motion can carry across pause, load or player replacement on the strength of an old positive delta.

## Evidence and remaining validation

Selected original-byte evidence is under `.local/native/bridge-abi/`: `retail156-abi.json`, `script-update.json`, `lua-getfield.json`, `lua-setfield.json`, `lua-type.json`, `lua-touserdata.json`, `lua-pushcclosure.json`, `lua-checkstack.json`, `lua-pcall.json`, `entity-vtable.json`, `player-primary-vtable.json`, `controller-constructor.json`, `actor-base-init.json`, `actor-init.json`, `frametime-wrapper.json`, `timer-vtable.json`, `timer-game-frametime.json` and `timer-is-paused.json`.

The expected bytes must be checked before any function call or detour; the file SHA alone does not prove in-memory bytes are unmodified. Static correspondence does not prove runtime thread safety, successful callback registration, entity-token matching, gate freshness or collision-safe movement. First capture these without changing requests. Only then enable the bounded actuator, retaining the host request path, collision and gravity.
