import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('analyze_state_bridge', Path(__file__).resolve().parents[1] / 'tools/analyze_state_bridge.py')
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


class BridgeCaptureTests(unittest.TestCase):
    def review(self, rows, expected_motion=False):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'capture.jsonl'
            path.write_text(''.join(json.dumps(r) + '\n' for r in rows), encoding='utf-8')
            return capture.summarize(path, expected_motion=expected_motion)

    def test_live_review_checks_actual_permission_and_vertical_invariants(self):
        header = {'event': 'started', 'protocol': 2, 'qpc_frequency': 1000, 'motion_compiled': True,
                  'peak_gain': 2, 'horizontal_cap': 6.25, 'request_travel_budget': 4, 'duration_limit': .9}
        row = dict(event='request', qpc=10, known=capture.MASK, safe=capture.MASK,
                   valid=True, identity_match=True, modified=True, dry_run=False,
                   enabled=True, flags=0x1060, before=[0, 3.1, 0], candidate=[0, 4.1, 0], dt=.02, gain=1.32)
        report = self.review([header, row], expected_motion=True)
        self.assertEqual(report['actual_modified_requests'], 1)
        self.assertEqual(report['unsafe_modified_requests'], 0)
        self.assertEqual(report['groups_exceeding_declared_bounds'], 0)
        row['candidate'][1] = 6.3
        report = self.review([header, row], expected_motion=True)
        self.assertEqual(report['groups_exceeding_declared_bounds'], 1)
        row['safe'] = 0
        row['candidate'][2] = 1
        report = self.review([header, row], expected_motion=True)
        self.assertEqual(report['unsafe_modified_requests'], 1)
        self.assertEqual(report['vertical_modified_requests'], 1)

    def test_separate_pulses_and_incomplete_capture_are_not_promoted(self):
        header = {'event': 'started', 'protocol': 1, 'qpc_frequency': 1000, 'motion_compiled': False}
        base = dict(known=capture.MASK, safe=capture.MASK, thread=1, epoch=1, generation=1,
                    identity=1, request_serial=1, reset_serial=0, valid=True, identity_match=True,
                    adapter_reason=9, motion_reason=3, modified=False, would_modify=True, dt=.02)
        rows = [header, dict(base, event='state', qpc=0)]
        for time, apply in [(10, True), (30, True), (50, False), (70, True), (90, True), (300, True)]:
            rows.append(dict(base, event='request', qpc=time, would_apply=apply))
        report = self.review(rows)
        self.assertEqual(report['would_apply_requests'], 5)
        self.assertEqual(report['maximum_contiguous_apply_requests'], 2)
        self.assertAlmostEqual(report['maximum_contiguous_apply_span_seconds'], .02)
        self.assertFalse(report['complete_shutdown'])
        self.assertIn('No complete shutdown record', report['review_problems'])
        self.assertNotIn('allow_live', report)

    def test_gate_subset_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Safe gate'):
            self.review([{'event': 'started', 'protocol': 1, 'qpc_frequency': 1000},
                         {'event': 'state', 'qpc': 1, 'known': 0, 'safe': 1}])

    def test_applied_counter_requires_a_retained_record_for_every_write(self):
        rows = [{'event': 'started', 'protocol': 2, 'qpc_frequency': 1000},
                {'event': 'stopped', 'modified': 1, 'fatal': False, 'dropped': 0, 'publish_errors': 0}]
        report = self.review(rows, expected_motion=True)
        self.assertIn('Applied-write counter differs from retained motion audit records', report['review_problems'])

    def test_corrected_session_limit_remains_bounded(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'bounded.jsonl'
            path.write_text('{}\n' * 21000, encoding='utf-8')
            self.assertEqual(len(capture.read_capture(path)), 21000)
            path.write_text('{}\n' * 100004, encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'bounds'):
                capture.read_capture(path)

    def test_oversized_line_and_nonfinite_json_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'bad.jsonl'
            for content in [' ' * 4097, '{"value": NaN}\n']:
                path.write_text(content, encoding='utf-8')
                with self.assertRaises(ValueError):
                    capture.read_capture(path)
