"""Capture living-entity ground status and position during Henry's normal callback."""
import collider_probe

native_probe = collider_probe.native_probe
native_probe.__doc__ = __doc__
native_probe.DEFAULT_BUILD = "collider-ground-v026"
native_probe.EXPECTED_PROBE = ("player-collider", 2)

if __name__ == "__main__":
    native_probe.main()
