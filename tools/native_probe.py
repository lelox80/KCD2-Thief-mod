"""Explicit-PID loader for our observation-only, build-pinned movement probe.

No automatic process discovery/attachment or executable replacement. Start only
after a verified save backup. Stop disables the detour; the DLL remains pinned
until normal game exit. Requires 64-bit Windows Python and matching privileges.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
import re
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DLL = ROOT / '.local/native/probe/build/MovementProbe.dll'
SHA = 'bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7'
STATUS_RVA = 0x837e7c
STATUS_BYTES = bytes.fromhex('48895c240848896c2410488974241857')
STATUS_KEY = 'original_request_prologue_restored'
BUILD_ROOT = None
DEFAULT_BUILD = 'build'
EXPECTED_PROBE = None
EXPECTED_HOOKS = None


def main():
    global DLL
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['start', 'stop', 'status'])
    parser.add_argument('--pid', type=int, required=True)
    if BUILD_ROOT is not None:
        parser.add_argument('--build-name', default=DEFAULT_BUILD,
                            help='Validated build directory; never installs or enables movement')
    args = parser.parse_args()
    if BUILD_ROOT is not None:
        if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,63}',args.build_name):
            parser.error('--build-name must be a lowercase alphanumeric/hyphen directory name')
        DLL=BUILD_ROOT/args.build_name/DLL.name
    if sys.platform != 'win32' or C.sizeof(C.c_void_p) != 8 or args.pid <= 0:
        parser.error('64-bit Windows Python and a positive explicit PID are required')
    config = json.loads((ROOT / '.local/environment.json').read_text(encoding='utf-8-sig'))
    game_dir = Path(config['game']).resolve()
    expected_exe = game_dir / 'Bin/Win64MasterMasterSteamPGO/KingdomCome.exe'
    expected_game = expected_exe.with_name('WHGame.dll')
    receipt=json.loads(DLL.with_name('probe-build.json').read_text(encoding='utf-8-sig'))
    if receipt['mode'] != 'observe-only' or receipt['fixtures'] != 'passed' or hashlib.sha256(DLL.read_bytes()).hexdigest() != receipt['dll_sha256']:
        raise RuntimeError('Native build receipt/hash mismatch')
    if EXPECTED_PROBE is not None:
        if (receipt.get('probe'),receipt.get('probe_revision')) != EXPECTED_PROBE:
            raise RuntimeError('Observation probe contract differs from this loader')
    if EXPECTED_HOOKS is not None and receipt.get('hooks') != EXPECTED_HOOKS:
        raise RuntimeError('Native hook identities differ from the inspected admission observer')
    if args.action=='start':
        for name,pin in receipt.get('sources',{}).items():
            source=(ROOT/name).resolve()
            if not source.is_relative_to(ROOT.resolve()) or hashlib.sha256(source.read_bytes()).hexdigest()!=pin:
                raise RuntimeError('Observation source changed since validation; rebuild before start')
    for name, pin in config['retail_sha256'].items():
        if hashlib.sha256((game_dir/name).read_bytes()).hexdigest() != pin:
            raise RuntimeError('Retail binary identity changed; refusing attachment')
    if config['retail_sha256']['Bin/Win64MasterMasterSteamPGO/WHGame.dll'] != SHA:
        raise RuntimeError('Configured target differs from the compiled native pin')
    if args.action == 'start':
        backup = json.loads((ROOT/'.local/last-backup.json').read_text())
        manifest = Path(backup['folder'])/'manifest.json'
        if not manifest.is_file() or not DLL.is_file():
            raise RuntimeError('Verified backup receipt and built probe required')
    kernel = C.WinDLL('kernel32', use_last_error=True)
    ptr = C.c_void_p
    signatures = {
        'OpenProcess': (W.HANDLE,[W.DWORD,W.BOOL,W.DWORD]),
        'CloseHandle': (W.BOOL,[W.HANDLE]),
        'QueryFullProcessImageNameW': (W.BOOL,[W.HANDLE,W.DWORD,W.LPWSTR,C.POINTER(W.DWORD)]),
        'CreateToolhelp32Snapshot': (W.HANDLE,[W.DWORD,W.DWORD]),
        'VirtualAllocEx': (ptr,[W.HANDLE,ptr,C.c_size_t,W.DWORD,W.DWORD]),
        'VirtualFreeEx': (W.BOOL,[W.HANDLE,ptr,C.c_size_t,W.DWORD]),
        'WriteProcessMemory': (W.BOOL,[W.HANDLE,ptr,ptr,C.c_size_t,C.POINTER(C.c_size_t)]),
        'ReadProcessMemory': (W.BOOL,[W.HANDLE,ptr,ptr,C.c_size_t,C.POINTER(C.c_size_t)]),
        'CreateRemoteThread': (W.HANDLE,[W.HANDLE,ptr,C.c_size_t,ptr,ptr,W.DWORD,ptr]),
        'WaitForSingleObject': (W.DWORD,[W.HANDLE,W.DWORD]),
        'GetExitCodeThread': (W.BOOL,[W.HANDLE,C.POINTER(W.DWORD)]),
        'GetModuleHandleExW': (W.BOOL,[W.DWORD,ptr,C.POINTER(W.HMODULE)]),
        'GetModuleFileNameW': (W.DWORD,[W.HMODULE,W.LPWSTR,W.DWORD]),
    }
    for name,(restype,argtypes) in signatures.items():
        fn=getattr(kernel,name);fn.restype=restype;fn.argtypes=argtypes
    class Module(C.Structure):
        _fields_=[('size',W.DWORD),('id',W.DWORD),('pid',W.DWORD),('global_count',W.DWORD),
                  ('count',W.DWORD),('base',ptr),('length',W.DWORD),('handle',W.HMODULE),
                  ('name',W.WCHAR*256),('path',W.WCHAR*260)]
    for name in ['Module32FirstW','Module32NextW']:
        fn=getattr(kernel,name);fn.restype=W.BOOL;fn.argtypes=[W.HANDLE,C.POINTER(Module)]
    def checked(value):
        if not value: raise C.WinError(C.get_last_error())
        return value
    def modules():
        snap=kernel.CreateToolhelp32Snapshot(0x18,args.pid)
        if snap == ptr(-1).value: raise C.WinError(C.get_last_error())
        values={}; item=Module();item.size=C.sizeof(item)
        try:
            checked(kernel.Module32FirstW(snap,C.byref(item)))
            while True:
                values[os.path.normcase(str(Path(item.path).resolve()))]=(int(item.base),item.length)
                if not kernel.Module32NextW(snap,C.byref(item)): break
        finally: kernel.CloseHandle(snap)
        return values
    process=checked(kernel.OpenProcess(0x410 if args.action=='status' else 0x43a,False,args.pid))
    try:
        path=C.create_unicode_buffer(32768); length=W.DWORD(len(path))
        checked(kernel.QueryFullProcessImageNameW(process,0,path,C.byref(length)))
        if Path(path.value).resolve() != expected_exe.resolve():
            raise RuntimeError('PID does not name the configured retail executable')
        current=modules()
        game_key=os.path.normcase(str(expected_game.resolve()))
        if game_key not in current:
            raise RuntimeError('Configured WHGame module is not loaded in this process')
        if args.action=='status':
            hooks=EXPECTED_HOOKS or [{'name':'request','rva':hex(STATUS_RVA),'prologue':STATUS_BYTES.hex()}]
            restored={}
            for hook in hooks:
                code=C.create_string_buffer(16);read=C.c_size_t()
                checked(kernel.ReadProcessMemory(process,current[game_key][0]+int(hook['rva'],16),code,16,C.byref(read)))
                if read.value!=16: raise RuntimeError('Incomplete hook prologue read')
                restored[hook['name']]=code.raw==bytes.fromhex(hook['prologue'])
            result={STATUS_KEY:restored.get('selection',restored.get('request')),
                    'owned_module_loaded':os.path.normcase(str(DLL.resolve())) in current}
            if EXPECTED_HOOKS is not None:
                result['hook_prologues_restored']=restored
                result['all_hook_prologues_restored']=all(restored.values())
            print(json.dumps(result))
            return
        def remote_call(address,parameter=None):
            thread=checked(kernel.CreateRemoteThread(process,None,0,address,parameter,0,None))
            try:
                if kernel.WaitForSingleObject(thread,30000) != 0:
                    raise TimeoutError('Remote call did not finish; do not unload or retry blindly')
                result=W.DWORD();checked(kernel.GetExitCodeThread(thread,C.byref(result)))
                return result.value
            finally: kernel.CloseHandle(thread)
        key=os.path.normcase(str(DLL.resolve()))
        if key not in current:
            if args.action != 'start': raise RuntimeError('Our probe is not loaded')
            # Resolve the real owner of forwarded LoadLibraryW, then relocate its RVA.
            address=C.cast(kernel.LoadLibraryW,ptr).value
            owner=W.HMODULE(); checked(kernel.GetModuleHandleExW(6,address,C.byref(owner)))
            local_path=C.create_unicode_buffer(32768);checked(kernel.GetModuleFileNameW(owner,local_path,len(local_path)))
            owner_key=os.path.normcase(str(Path(local_path.value).resolve()))
            if owner_key not in current: raise RuntimeError('Remote LoadLibrary owner module is absent')
            remote_load=current[owner_key][0]+address-owner.value
            data=C.create_unicode_buffer(str(DLL.resolve()))
            allocation=checked(kernel.VirtualAllocEx(process,None,C.sizeof(data),0x3000,4))
            finished=False
            try:
                written=C.c_size_t()
                checked(kernel.WriteProcessMemory(process,allocation,C.cast(data,ptr),C.sizeof(data),C.byref(written)))
                if written.value != C.sizeof(data): raise RuntimeError('Incomplete path write')
                remote_call(remote_load,allocation);finished=True
            finally:
                if finished: kernel.VirtualFreeEx(process,allocation,0,0x8000)
            current=modules()
            if key not in current: raise RuntimeError('Probe LoadLibrary did not load the expected DLL')
        # DllMain is inert. Mapping the same owned DLL locally supplies an export RVA.
        local=C.WinDLL(str(DLL.resolve()))
        export=getattr(local,'KtProbeStart' if args.action=='start' else 'KtProbeStop')
        offset=C.cast(export,ptr).value-local._handle
        result=remote_call(current[key][0]+offset)
        if result: raise RuntimeError(f'Probe refused {args.action}: Windows status {result}')
        print(f'PROBE {args.action.upper()} succeeded for explicit PID {args.pid}; observation only. Normal game exit releases the pinned DLL.')
    finally: kernel.CloseHandle(process)


if __name__ == '__main__':
    main()
