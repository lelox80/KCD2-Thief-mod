"""Preview the taller Kuttenberg guide with an isolated selector parameter copy."""
import dry_guide_probe

native_probe = dry_guide_probe.native_probe
native_probe.__doc__ = __doc__
native_probe.DEFAULT_BUILD = "dry-guide-selection-v026-tall"
native_probe.EXPECTED_MODE = "dry-tall-preview"
native_probe.EXPECTED_HOOKS = [
    *native_probe.EXPECTED_HOOKS,
    {"name": "params", "rva": "0x832310", "prologue": "8954241053574883ec38488bd98bfa48"},
]
EXPECTED_FILES = {
    name: digest for name, digest in dry_guide_probe.EXPECTED_FILES.items()
    if name != "Data/kuttenberg_thief_guide_append.pak"
}
EXPECTED_FILES["Data/kuttenberg_thief_guide_tall.pak"] = "4e0f57b11e1fbf5b1feefa4ca4954420cc28117c41e6868e048e20eb10e7191b"


def check_install(game_dir):
    dry_guide_probe.check_install(game_dir, "guide-tall", EXPECTED_FILES)


if __name__ == "__main__":
    native_probe.main(check_start=check_install)
