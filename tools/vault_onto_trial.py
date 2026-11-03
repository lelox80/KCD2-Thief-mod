"""Arm one host-owned VaultOnto attempt at the measured Kuttenberg wall."""
import dry_guide_probe

native_probe = dry_guide_probe.native_probe
native_probe.__doc__ = __doc__
native_probe.DEFAULT_BUILD = "dry-guide-selection-v026-live"
native_probe.EXPECTED_MODE = "bounded-vault-onto"

if __name__ == "__main__":
    native_probe.main(check_start=dry_guide_probe.check_install, allow_arm=True)
