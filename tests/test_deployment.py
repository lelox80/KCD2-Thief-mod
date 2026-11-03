"""Exercise deployment on disposable fake game/save folders, never the real game."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import sys
import zipfile
sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))

spec = importlib.util.spec_from_file_location("project", Path(__file__).parents[1] / "tools/project.py")
project = importlib.util.module_from_spec(spec)
spec.loader.exec_module(project)


class DeploymentTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.base = Path(self.tmp.name)
        self.old = project.ROOT, project.build, project.require_closed
        project.ROOT = self.base / "repo"
        (project.ROOT / ".local").mkdir(parents=True)
        self.game = self.base / "game"
        self.game.mkdir()
        (self.game / "engine.dll").write_bytes(b"test engine")
        self.saves = self.base / "saves"
        (self.saves / "slot").mkdir(parents=True)
        (self.saves / "slot/save.whs").write_bytes(b"valuable save")
        self.mod = project.ROOT / "build/kuttenberg_thief"
        (self.mod / "Data").mkdir(parents=True)
        (self.mod / "mod.manifest").write_text("<kcd_mod><info><version>test</version></info></kcd_mod>")
        (self.mod / "Data/test.pak").write_bytes(b"test package")
        hashes = {p.relative_to(self.mod).as_posix(): project.digest(p) for p in self.mod.rglob("*") if p.is_file()}
        (project.ROOT / "build/package-hashes.json").write_text(json.dumps(hashes))
        config = {"game": str(self.game), "saves": str(self.saves),
                  "retail_sha256": {"engine.dll": project.digest(self.game / "engine.dll")}}
        (project.ROOT / ".local/environment.json").write_text(json.dumps(config))
        project.build = lambda: self.mod
        project.require_closed = lambda: None

    def tearDown(self):
        project.ROOT, project.build, project.require_closed = self.old
        self.tmp.cleanup()

    def test_install_backup_idempotence_and_owned_removal(self):
        other = self.game / "Mods/other"
        other.mkdir(parents=True)
        (other / "keep.txt").write_text("unrelated")
        project.install()
        receipt = project.ROOT / ".local/install-receipt.json"
        self.assertTrue(receipt.exists())
        backup = json.loads((project.ROOT / ".local/last-backup.json").read_text())
        self.assertTrue((Path(backup["folder"]) / "kcd2-saves.zip").exists())
        project.install()
        project.uninstall()
        self.assertEqual((other / "keep.txt").read_text(), "unrelated")
        self.assertEqual((self.saves / "slot/save.whs").read_bytes(), b"valuable save")
        self.assertFalse((self.game / "Mods/kuttenberg_thief").exists())

    def test_changed_installed_file_prevents_removal(self):
        project.install()
        dest = self.game / "Mods/kuttenberg_thief/Data/test.pak"
        dest.write_bytes(b"user modification")
        with self.assertRaisesRegex(RuntimeError, "has changes"):
            project.uninstall()
        self.assertEqual(dest.read_bytes(), b"user modification")

    def test_guide_experiment_refuses_conflicts_and_rolls_back(self):
        profile = project.ROOT / "build/guide-noop/kuttenberg_thief"
        (profile / "Data").mkdir(parents=True)
        for name in ("mod.manifest", "Data/test.pak"):
            (profile / name).write_bytes((self.mod / name).read_bytes())
        (profile / project.GUIDE_NOOP_PAK).write_bytes(b"verified guide experiment fixture")
        profile_hashes = {p.relative_to(profile).as_posix(): project.digest(p)
                          for p in profile.rglob("*") if p.is_file()}
        (profile.parent / "package-hashes.json").write_text(json.dumps(profile_hashes))
        project.build = lambda **kwargs: profile if kwargs.get("profile") == "guide-noop" else self.mod

        project.install()
        with self.assertRaisesRegex(RuntimeError, "Existing mod differs"):
            project.install(profile="guide-noop")
        project.uninstall()

        other = self.game / "Mods/other"
        other.mkdir()
        with zipfile.ZipFile(other / "other.pak", "w") as package:
            package.writestr(project.GUIDE_MEMBER.lower(), b"conflicting guide")
        with self.assertRaisesRegex(RuntimeError, "overrides a required asset"):
            project.install(profile="guide-noop")
        (other / "other.pak").unlink()

        project.install(profile="guide-noop")
        receipt = json.loads((project.ROOT / ".local/install-receipt.json").read_text())
        self.assertEqual(receipt["profile"], "guide-noop")
        self.assertEqual(receipt["sha256"], profile_hashes)
        project.uninstall()
        project.install()
        dest = self.game / "Mods/kuttenberg_thief"
        self.assertFalse((dest / project.GUIDE_NOOP_PAK).exists())
        self.assertEqual((dest / "Data/test.pak").read_bytes(), b"test package")

    def test_build_and_load_order_gates(self):
        (self.game / "engine.dll").write_bytes(b"updated engine")
        with self.assertRaisesRegex(RuntimeError, "binary changed"):
            project.install()
        (self.game / "engine.dll").write_bytes(b"test engine")
        (self.game / "Mods").mkdir()
        order = self.game / "Mods/mod_order.txt"
        order.write_text("other\n")
        with self.assertRaisesRegex(RuntimeError, "excludes"):
            project.install()
        self.assertEqual(order.read_text(), "other\n")
        self.assertFalse((self.game / "Mods/kuttenberg_thief").exists())


if __name__ == "__main__":
    unittest.main()
