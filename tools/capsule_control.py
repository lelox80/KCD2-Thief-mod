"""Run bounded capsule controls and the pinned wall route from Henry's standing callback."""
import collider_probe

native_probe = collider_probe.native_probe
native_probe.__doc__ = __doc__
native_probe.DEFAULT_BUILD = "capsule-route-v026"
native_probe.DLL = native_probe.BUILD_ROOT / "capsule-route-v026/CapsuleControl.dll"
native_probe.EXPECTED_PROBE = ("capsule-control", 2)

if __name__ == "__main__":
    native_probe.main()
