"""Arm one host-owned climb at the measured 1.64 m Kuttenberg wall."""
import tall_guide_probe

native_probe = tall_guide_probe.native_probe
native_probe.__doc__ = __doc__
native_probe.DEFAULT_BUILD = "dry-guide-selection-v026-tall-live"
native_probe.EXPECTED_MODE = "bounded-tall-vault"

if __name__ == "__main__":
    native_probe.main(check_start=tall_guide_probe.check_install, allow_arm=True)
