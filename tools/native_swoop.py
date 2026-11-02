"""Explicit-PID loader for the build-pinned state bridge / bounded prototype.

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
DLL = ROOT / '.local/native/swoop/build-playable/KuttenbergBridge.dll'
SHA = 'bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7'


def main():
    global DLL
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['start', 'stop', 'status'])
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--profile',choices=['bridge','prototype'],default='bridge',
                        help='bridge compiles out movement; prototype requires a passed bridge validation receipt')
    parser.add_argument('--build-name',default='build-playable',
                        help='Existing validated subdirectory under .local/native/swoop')
    args = parser.parse_args()
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,63}',args.build_name):
        parser.error('--build-name must be a lowercase alphanumeric/hyphen directory name')
    DLL=ROOT/'.local/native/swoop'/args.build_name/'KuttenbergBridge.dll'
    DLL=DLL.with_name('KuttenbergBridge.dll' if args.profile=='bridge' else 'KuttenbergSwoop.dll')
    if sys.platform != 'win32' or C.sizeof(C.c_void_p) != 8 or args.pid <= 0:
        parser.error('64-bit Windows Python and a positive explicit PID are required')
    config = json.loads((ROOT / '.local/environment.json').read_text(encoding='utf-8-sig'))
    game_dir = Path(config['game']).resolve()
    expected_exe = game_dir / 'Bin/Win64MasterMasterSteamPGO/KingdomCome.exe'
    expected_game = expected_exe.with_name('WHGame.dll')
    receipt=json.loads(DLL.with_name('native-build.json').read_text(encoding='utf-8-sig'))
    if receipt['protocol']!=2 or receipt['mode']!='bounded-prototype' or receipt['fixtures']!='passed' or hashlib.sha256(DLL.read_bytes()).hexdigest()!=receipt['payloads'][DLL.name]:
        raise RuntimeError('Native build receipt/hash mismatch')
    if args.action=='start':
        for name,pin in receipt['sources'].items():
            source=(ROOT/name).resolve()
            if not source.is_relative_to(ROOT.resolve()) or hashlib.sha256(source.read_bytes()).hexdigest()!=pin:
                raise RuntimeError('Source changed since native validation; rebuild before start')
        installed=json.loads((ROOT/'.local/install-receipt.json').read_text())
        if installed['version']!='0.0.21' or Path(installed['destination']).resolve()!=(game_dir/'Mods/kuttenberg_thief').resolve():
            raise RuntimeError('Install the matching v0.0.21 protocol2 native-only Lua state bridge before starting')
        for name,pin in installed['sha256'].items():
            if hashlib.sha256((Path(installed['destination'])/name).read_bytes()).hexdigest()!=pin:
                raise RuntimeError('Installed Lua package differs from its receipt')
        if args.profile=='prototype':
            validation_path=ROOT/'.local/evidence/state-bridge-validation.json'
            if not validation_path.is_file():
                raise RuntimeError('Live movement locked: complete and review the observation-only state-bridge test first')
            validation=json.loads(validation_path.read_text())
            if validation.get('allow_live') is not True or validation.get('sources')!=receipt['sources'] or validation.get('target_sha256')!=SHA or validation.get('package_sha256')!=installed['sha256']:
                raise RuntimeError('Live movement locked: validation does not match this native/Lua build')
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
            result={'owned_module_loaded':os.path.normcase(str(DLL.resolve())) in current,'profile':args.profile}
            for label,rva,pin in [('request',0x837e7c,'48895c240848896c2410488974241857'),('script_update',0xa26b94,'48895c24184889742420574883ec3048')]:
                code=C.create_string_buffer(16);read=C.c_size_t()
                checked(kernel.ReadProcessMemory(process,current[game_key][0]+rva,code,16,C.byref(read)))
                if read.value!=16: raise RuntimeError('Incomplete prologue read')
                result[label+'_prologue_restored']=code.raw==bytes.fromhex(pin)
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
        export=getattr(local,'KtSwoopStart' if args.action=='start' else 'KtSwoopStop')
        offset=C.cast(export,ptr).value-local._handle
        result=remote_call(current[key][0]+offset)
        if result: raise RuntimeError(f'Probe refused {args.action}: Windows status {result}')
        print(f'STATE BRIDGE {args.action.upper()} succeeded for explicit PID {args.pid}; profile={args.profile}. Normal game exit releases the pinned DLL.')
        if args.action=='start':
            print('Movement writes are compiled out.' if args.profile=='bridge' else 'Motion still requires explicit Lua enable, a one-shot request and every state/owner gate.')
    finally: kernel.CloseHandle(process)


if __name__ == '__main__':
    main()
