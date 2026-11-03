import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import guide_member as guide


def fixture():
    return (b'<Root version="1"><LedgeManager><Ledges><Ledge><entityId>0</entityId>'
        b'<markersStartIdx>0</markersStartIdx><markersCount>2</markersCount>'
        b'<ledgeCornerEndAdjustAmount>0.5</ledgeCornerEndAdjustAmount><Flags0>559</Flags0><Flags1>3</Flags1>'
        b'</Ledge></Ledges><Markers><Marker><worldPosition>1,2,3</worldPosition>'
        b'<facingDirection>0,-1,0</facingDirection><endOrCorner>1</endOrCorner></Marker>'
        b'<Marker><worldPosition>2,2,3</worldPosition><facingDirection>0,-1,0</facingDirection>'
        b'<endOrCorner>0</endOrCorner></Marker></Markers></LedgeManager>'
        b'<GameProfileManager><Untouched value="x" /></GameProfileManager></Root>')


class GuideMemberTests(unittest.TestCase):
    def analyze_fixture(self, data):
        with patch.object(guide, "SOURCE_SHA256", hashlib.sha256(data).hexdigest()), patch.object(guide, "SOURCE_COUNTS", (1, 2)):
            return guide.inspect(data)

    def test_native_field_order_width_and_padding(self):
        data = fixture()
        with patch.object(guide, "SOURCE_SHA256", hashlib.sha256(data).hexdigest()), patch.object(guide, "SOURCE_COUNTS", (1, 2)):
            ledges, markers = guide.canonical_records(data)
        self.assertEqual(ledges, struct.pack('<IHHHHf', 0, 0, 2, 559, 3, .5))
        self.assertEqual(markers, struct.pack('<ffffffB', 1, 2, 3, 0, -1, 0, 1) + struct.pack('<ffffffB', 2, 2, 3, 0, -1, 0, 0))
        self.assertEqual((len(ledges), len(markers)), (16, 50))
        self.assertEqual(guide.fnv1a64(b"hello"), "a430d84680aabd0b")

    def test_unpinned_member_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "changed"):
            guide.inspect(fixture())

    def test_inactive_append_preserves_base_records_and_profile_data(self):
        source = fixture().replace(b"</Ledges>", b"\t\t</Ledges>").replace(b"</Markers>", b"\t\t</Markers>")
        with patch.object(guide, "SOURCE_SHA256", hashlib.sha256(source).hexdigest()), patch.object(guide, "SOURCE_COUNTS", (1, 2)):
            output, report = guide.append_inactive(source)
        original, appended = ET.fromstring(source), ET.fromstring(output)
        for section in ("Ledges", "Markers"):
            old = original.find(f"LedgeManager/{section}")
            new = appended.find(f"LedgeManager/{section}")
            for before, after in zip(old, new):
                self.assertEqual(guide.fields(before, before.tag, tuple(c.tag for c in before)),
                                 guide.fields(after, after.tag, tuple(c.tag for c in after)))
        tail = appended.find("LedgeManager/Ledges")[-1]
        self.assertEqual([tail.findtext(key) for key in ("entityId", "markersStartIdx", "markersCount", "Flags0", "Flags1")],
                         ["0", "2", "2", "0", "0"])
        self.assertEqual(output.split(b"<GameProfileManager>")[1], source.split(b"<GameProfileManager>")[1])
        self.assertEqual((report["ledge_count"], report["marker_count"]), (2, 4))
        self.assertFalse(report["allows_added_climbs"])

    def test_structural_digest_excludes_only_entity_reference(self):
        original = self.analyze_fixture(fixture())
        remapped = self.analyze_fixture(fixture().replace(b'<entityId>0', b'<entityId>123456789'))
        self.assertNotEqual(original['canonical_ledges_fnv1a64'], remapped['canonical_ledges_fnv1a64'])
        self.assertEqual(original['structural_ledges_fnv1a64'], remapped['structural_ledges_fnv1a64'])
        changed = self.analyze_fixture(fixture().replace(b'<Flags0>559', b'<Flags0>558'))
        swapped = self.analyze_fixture(fixture().replace(b'<Flags0>559</Flags0><Flags1>3', b'<Flags0>3</Flags0><Flags1>559'))
        self.assertNotEqual(original['structural_ledges_fnv1a64'], changed['structural_ledges_fnv1a64'])
        self.assertNotEqual(original['structural_ledges_fnv1a64'], swapped['structural_ledges_fnv1a64'])
        self.assertEqual(original['canonical_markers_fnv1a64'], remapped['canonical_markers_fnv1a64'])

    def test_marker_ranges_and_native_widths_refuse(self):
        for old, new in ((b'<markersStartIdx>0', b'<markersStartIdx>1'),
                         (b'<markersCount>2', b'<markersCount>3'),
                         (b'<markersCount>2', b'<markersCount>1'),
                         (b'<Flags0>559', b'<Flags0>65536'),
                         (b'<entityId>0', b'<entityId>4294967296'),
                         (b'<entityId>0', b'<entityId>-1')):
            with self.subTest(new=new), self.assertRaises(ValueError):
                self.analyze_fixture(fixture().replace(old, new))

    def test_nonfinite_and_overflow_geometry_refused(self):
        for value in (b'nan', b'inf', b'-inf', b'1e99'):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "finite"):
                self.analyze_fixture(fixture().replace(b'1,2,3', value+b',2,3'))
        for replacement in (b'1,2', b'1,2,3,4'):
            with self.assertRaisesRegex(ValueError, "vector"):
                self.analyze_fixture(fixture().replace(b'1,2,3', replacement))
        with self.assertRaisesRegex(ValueError, "Zero"):
            self.analyze_fixture(fixture().replace(b'0,-1,0', b'0,0,0'))
        with self.assertRaisesRegex(ValueError, "Zero"):
            self.analyze_fixture(fixture().replace(b'0,-1,0', b'0,1e-99,0'))

    def test_unsupported_record_schema_and_corner_refuse(self):
        for old,new in ((b'<Flags0>559</Flags0>', b'<Flags0>559</Flags0><Flags0>0</Flags0>'),
                        (b'<endOrCorner>1', b'<endOrCorner>2'),
                        (b'<Marker>', b'<Marker unknown="1">'),
                        (b'<Root version="1">', b'<Root version="2">')):
            with self.subTest(new=new), self.assertRaises(ValueError):
                self.analyze_fixture(fixture().replace(old,new))

    def test_duplicate_or_wrong_case_archive_member_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = Path(folder)/'input.pak'
            for names in (('WHDATA_1',), ('whdata_1','WHDATA_1')):
                with zipfile.ZipFile(archive,'w') as z:
                    for name in names: z.writestr(name, fixture())
                with self.assertRaisesRegex(ValueError,"case-exact"):
                    guide.read_member(archive)

    def test_noop_preserves_entire_member_and_refuses_overwrite_escape(self):
        data = fixture()
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            archive = root/'input.pak'
            with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as z:
                z.writestr('whdata_1', data)
            original = archive.read_bytes()
            with patch.object(guide,'ROOT',root), patch.object(guide,'SOURCE_SHA256',hashlib.sha256(data).hexdigest()), patch.object(guide,'SOURCE_COUNTS',(1,2)):
                destination = root/'.local'/'new-stage'
                receipt = guide.stage_noop(archive,destination)
                self.assertEqual((destination/'whdata_1').read_bytes(),data)
                self.assertFalse(receipt['archive_created'])
                self.assertFalse(receipt['installed'])
                self.assertFalse(receipt['registered'])
                self.assertEqual(receipt['source_zip_compression'],zipfile.ZIP_DEFLATED)
                self.assertEqual(json.loads((destination/'guide-member.json').read_text())['source_sha256'],hashlib.sha256(data).hexdigest())
                with self.assertRaises(FileExistsError):guide.stage_noop(archive,destination)
                for output in (root,root/'.local',root/'outside',root/'build'/'..'/'escaped'):
                    with self.subTest(output=output), self.assertRaises(ValueError):guide.stage_noop(archive,output)
            self.assertEqual(archive.read_bytes(),original)


if __name__ == '__main__':
    unittest.main()
