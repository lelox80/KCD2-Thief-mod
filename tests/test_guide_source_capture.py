import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('source_capture', Path(__file__).resolve().parents[1]/'tools/analyze_guide_source_probe.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class GuideSourceCaptureTests(unittest.TestCase):
    def fixture(self):
        return [dict(event='started', probe='guide-source', protocol=1, motion_compiled=False,
                     mounts_archives=False, registers_guides=False, record_limit=1024,
                     logical_path_capacity=260, archive_path_capacity=512),
                dict(event='guide_source', transaction=1, thread=1, depth=1, begin_qpc=10, end_qpc=20,
                     level='kutnohorsko', **{'namespace': 'data_levels'}, member_index=1, lookups=1, matches=1, complete=True,
                     exceptional=False, suppressed=False, expected_known=True, result='archive_observed',
                     archive='base_level_pak', archive_attributed=True),
                dict(event='stopped', calls=[1, 1], transactions=1, ended=1, reserved=1, committed=1,
                     dropped=0, unlinked=0, overflow=0, truncated=False, write_failed=False)]

    def analyze(self, rows):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'capture.jsonl'
            path.write_text('\n'.join(json.dumps(row) for row in rows)+'\n', encoding='utf-8')
            return MODULE.analyze(path)

    def test_attribution_never_authorizes_new_mount_registration_or_motion(self):
        result=self.analyze(self.fixture())
        self.assertEqual(result['archive_attributions'],1)
        self.assertEqual(result['kutnohorsko_member1_attributions'],1)
        self.assertEqual(result['review_problems'],[])
        for key in ('allows_live_movement','allows_registration','allows_mounting'):
            self.assertIs(result[key],False)

    def test_unknown_empty_skipped_and_other_member_stay_distinct(self):
        for result in ('not_observed','empty','provider_unknown','member_mismatch','caller','unreadable','truncated','bad_path'):
            rows=self.fixture();rows[1].update(result=result, archive='unknown', archive_attributed=False)
            self.assertEqual(self.analyze(rows)['archive_attributions'],0,result)
        rows=self.fixture();rows[1]['member_index']=0
        self.assertEqual(self.analyze(rows)['kutnohorsko_member1_attributions'],0)

    def test_qualification_cannot_borrow_unknown_or_ambiguous_context(self):
        for field,value in (('complete',False),('exceptional',True),('suppressed',True),('expected_known',False),('lookups',2),('matches',0)):
            rows=self.fixture();rows[1][field]=value
            self.assertTrue(self.analyze(rows)['review_problems'],field)
            rows[1]['archive_attributed']=False
            self.assertEqual(self.analyze(rows)['archive_attributions'],0,field)

    def test_missing_and_bad_shutdown(self):
        for field,value in (('dropped',1),('overflow',1),('write_failed',True),('committed',0),('truncated',True)):
            rows=self.fixture();rows[-1][field]=value
            self.assertTrue(self.analyze(rows)['review_problems'],field)
        self.assertTrue(self.analyze(self.fixture()[:-1])['review_problems'])

    def test_malformed_fields_never_crash_or_count_positive(self):
        for field,value in (('end_qpc','x'),('lookups','x'),('transaction',[]),('complete',1),('archive','C:/private/path'),('depth',5),('matches',2)):
            rows=self.fixture();rows[1][field]=value
            result=self.analyze(rows)
            self.assertTrue(result['review_problems'],field)
            self.assertEqual(result['archive_attributions'],0,field)
        rows=self.fixture();rows[1]=[]
        self.assertTrue(self.analyze(rows)['review_problems'])
        rows=self.fixture();rows.insert(2,copy.deepcopy(rows[1]))
        self.assertTrue(self.analyze(rows)['review_problems'])


if __name__=='__main__':unittest.main()
