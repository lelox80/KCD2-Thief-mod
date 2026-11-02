"""Explicit-PID observation of native ledge search and framework acceptance.

Reuses the pinned loader/backup/module checks of the movement observer. The
native DLL follows existing attempts through the query, selector and framework;
framework acceptance is not proof that KCD2 completed a traversal.
"""
import native_probe

native_probe.__doc__ = __doc__
native_probe.BUILD_ROOT = native_probe.ROOT / '.local/native/ledge-probe'
native_probe.DEFAULT_BUILD = 'build-v021'
native_probe.DLL = native_probe.BUILD_ROOT / 'build-v021/LedgeProbe.dll'
native_probe.EXPECTED_PROBE = ('ledge-admission', 4)
native_probe.EXPECTED_HOOKS = [
    {'name':'attempt','rva':'0x831b20','prologue':'488bc448895808488970184889782055'},
    {'name':'selection','rva':'0x831eac','prologue':'488bc448895810488970184889782055'},
    {'name':'query','rva':'0x832188','prologue':'40534883ec40488bc1488bda4883c118'},
    {'name':'framework','rva':'0x85612c','prologue':'48895c24104889742418574883ec2048'},
]
native_probe.STATUS_RVA = 0x831eac
native_probe.STATUS_BYTES = bytes.fromhex('488bc448895810488970184889782055')
native_probe.STATUS_KEY = 'original_ledge_selection_prologue_restored'

if __name__ == '__main__':
    native_probe.main()
