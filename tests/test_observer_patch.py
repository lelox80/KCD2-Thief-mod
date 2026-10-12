import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch as mock_patch
import xml.etree.ElementTree as ET
import zipfile
sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import observer_patch as observer


class ObserverTests(unittest.TestCase):
    def test_preserves_existing_branches_and_rejects_drift(self):
        source = (b'<BehaviorTrees><BehaviorTree name="root"><Root><Behavior>'
                  b'<Parallel successMode="All" failureMode="Any"><Wait duration="original" />\r\n'
                  b'\t\t\t\t</Parallel>\r\n\t\t\t</Behavior></Root></BehaviorTree></BehaviorTrees>')
        with self.assertRaisesRegex(RuntimeError, 'changed'):
            observer.patch(source)
        with mock_patch.object(observer, 'SOURCE_SHA256', hashlib.sha256(source).hexdigest()):
            result = observer.patch(source)
        root = ET.fromstring(result)
        self.assertEqual(root.find('.//Parallel/Wait').get('duration'), 'original')
        self.assertEqual(len(root.findall('.//ActorStateCheck')), len(observer.STATES))
        self.assertEqual(root.findall('.//Wait')[-1].get('duration'), "'100ms'")

    def test_conflicting_mod_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            mods=Path(tmp)
            folder=mods/'other'/'Data'
            folder.mkdir(parents=True)
            with zipfile.ZipFile(folder/'other.pak','w') as z:
                z.writestr(observer.ASSET.upper(),'<other/>')
            with self.assertRaisesRegex(RuntimeError,'Another mod'):
                observer.check_conflicts(mods,'kuttenberg_thief')
