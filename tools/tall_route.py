"""Measure the standing capsule corridor at the scanned 1.64 m Kuttenberg wall."""
import capsule_control

native_probe = capsule_control.native_probe
native_probe.__doc__ = __doc__
native_probe.DEFAULT_BUILD = "capsule-tall-v026"
native_probe.EXPECTED_PROBE = ("capsule-control", 3)

if __name__ == "__main__":
    native_probe.main()
