"""Read Henry's living-entity dimensions from the pinned native callback."""
import native_probe

native_probe.__doc__ = __doc__
native_probe.BUILD_ROOT = native_probe.ROOT / ".local/native/clearance-probe"
native_probe.DEFAULT_BUILD = "collider-v026"
native_probe.DLL = native_probe.BUILD_ROOT / "collider-v026/ColliderProbe.dll"
native_probe.EXPECTED_PROBE = ("player-collider", 1)
native_probe.EXPECTED_HOOKS = [
    {"name": "request", "rva": "0x837e7c", "prologue": "48895c240848896c2410488974241857"},
]

if __name__ == "__main__":
    native_probe.main()
