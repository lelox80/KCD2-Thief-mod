import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('guide_capture', Path(__file__).resolve().parents[1]/'tools/analyze_guide_probe.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class GuideCaptureTests(unittest.TestCase):
    def fixture(self):
        return [dict(event='started', probe='guide-load', protocol=1, motion_compiled=False,
                     registers_guides=False, record_limit=1024, max_elements=65535,
                     fingerprint='fnv1a64-typed-le', archive_source_verified=False),
                dict(event='guide_load', transaction=1, thread=1, depth=1, begin_qpc=10, end_qpc=20,
                     level='kutnohorsko', member_index=1, bodies=1, indexes=1, complete=True,
                     exceptional=False, suppressed=False, result='observed', registry_observed=True,
                     ledge_count=4129, marker_count=27564, ledge_fnv1a64='41075d281a1defb3',
                     structural_ledge_fnv1a64='8bfc91bfec28f68e', marker_fnv1a64='d7c2ce5751170770'),
                dict(event='stopped', calls=[1, 1, 1], transactions=1, ended=1, reserved=1, committed=1,
                     dropped=0, unlinked=0, overflow=0, truncated=False, write_failed=False)]

    def analyze(self, rows):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'capture.jsonl'
            path.write_text('\n'.join(json.dumps(row) for row in rows)+'\n', encoding='utf-8')
            return MODULE.analyze(path)

    def test_complete_context_never_authorizes_registration_or_motion(self):
        result=self.analyze(self.fixture())
        self.assertEqual(result['registry_observations'],1)
        self.assertEqual(result['kutnohorsko_member1_observations'],1)
        self.assertEqual(result['review_problems'],[])
        for key in ('allows_live_movement','allows_registration','archive_source_verified'):
            self.assertIs(result[key],False)

    def test_missing_and_bad_shutdown(self):
        for field,value in (('dropped',1),('overflow',1),('write_failed',True),('committed',0),('truncated',True)):
            rows=self.fixture();rows[-1][field]=value
            self.assertTrue(self.analyze(rows)['review_problems'],field)
        self.assertTrue(self.analyze(self.fixture()[:-1])['review_problems'])

    def test_qualification_cannot_borrow_unknown_context(self):
        for field,value in (('complete',False),('exceptional',True),('suppressed',True),('bodies',0),('indexes',2),('result','owner')):
            rows=self.fixture();rows[1][field]=value
            self.assertTrue(self.analyze(rows)['review_problems'],field)
            rows[1]['registry_observed']=False
            result=self.analyze(rows)
            self.assertEqual(result['registry_observations'],0,field)

    def test_bounded_fields_and_duplicates(self):
        for field,value in (('marker_count',65536),('depth',5),('ledge_fnv1a64','0xdead'),('level','private/path'),('begin_qpc',30)):
            rows=self.fixture();rows[1][field]=value
            self.assertTrue(self.analyze(rows)['review_problems'],field)
        rows=self.fixture();rows.insert(2,copy.deepcopy(rows[1]));self.assertTrue(self.analyze(rows)['review_problems'])

    def test_malformed_scalars_are_inconclusive_without_crashing(self):
        for field,value in (('end_qpc','x'),('bodies','x'),('transaction',[]),('marker_count',-1),('complete',1),('ledge_fnv1a64','bad')):
            rows=self.fixture();rows[1][field]=value
            result=self.analyze(rows)
            self.assertTrue(result['review_problems'],field)
            self.assertEqual(result['registry_observations'],0,field)
        rows=self.fixture();rows[1]=[]
        self.assertTrue(self.analyze(rows)['review_problems'])


if __name__=='__main__':unittest.main()
