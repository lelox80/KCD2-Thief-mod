"""Explicit-PID observation of KCD2's own animation-adjustment box sweeps."""
import native_probe

native_probe.__doc__ = __doc__
native_probe.BUILD_ROOT = native_probe.ROOT / ".local/native/clearance-probe"
native_probe.DEFAULT_BUILD = "build-v026"
native_probe.DLL = native_probe.BUILD_ROOT / "build-v026/ClearanceProbe.dll"
native_probe.EXPECTED_PROBE = ("clearance", 1)
native_probe.EXPECTED_HOOKS = [
    {"name": "sweep", "rva": "0x8115bc", "prologue": "488bc448895808488968104889701848"},
]
native_probe.STATUS_RVA = 0x8115BC
native_probe.STATUS_BYTES = bytes.fromhex("488bc448895808488968104889701848")
native_probe.STATUS_KEY = "original_clearance_prologue_restored"

if __name__ == "__main__":
    native_probe.main()
