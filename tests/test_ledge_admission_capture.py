import copy
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ledge_admission_review', ROOT/'tools/analyze_ledge_admission.py')
review = importlib.util.module_from_spec(spec)
spec.loader.exec_module(review)


def capture():
    return [
        {'event': 'started', 'mode': 'observe-only', 'probe': 'ledge-admission', 'probe_revision': 4,
         'motion_compiled': False, 'qpc_frequency': 10_000_000, 'hook_count': 4},
        {'event': 'ledge_admission', 'transaction': 1, 'entity': 73, 'thread': 99, 'depth': 1,
         'scope': 'standing_attempt', 'begin_qpc': 100, 'query_qpc': 110, 'selection_qpc': 120,
         'framework_qpc': 130, 'end_qpc': 140, 'completed': True, 'exceptional': False,
         'queries': 1, 'selections': 1, 'framework_requests': 1, 'allow_alternate': False, 'query_position': [1, 2, 3],
         'query_direction': [0, 1, 0], 'query_bounds': [1.6, .7, .8, .5], 'query_handle': 47251456,
         'query_return_matches_output': True, 'selection': [2, 47251456],
         'request': [2, 47251456, 1, 0, 0, 0], 'request_matches_selection': True,
         'attempt_result_byte': 1, 'framework_result_byte': 1, 'framework_accepted': True,
         'stage': 'framework_accepted', 'request_guard': 'matched', 'request_guard_bits': 2047,
         'action_vtable_kind': 'ledge_primary', 'action_vtable_rva': 0x3c0d880,
         'owner_component_vtable_kind': 'action_actor', 'owner_component_vtable_rva': 0x3b098e0},
        {'event': 'stopped', 'transactions_started': 1, 'transactions_ended': 1,
         'reserved': 1, 'committed': 1, 'dropped': 0, 'owner_refused': 0, 'unreadable': 0,
         'context_overflow': 0, 'unlinked_framework': 0, 'write_failed': False, 'truncated': False,
         'request_guard_counts': {k: int(k == 'matched') for k in review.GUARD_BITS}},
    ]


class AdmissionCaptureTests(unittest.TestCase):
    def test_clean_acceptance_stays_observation_only(self):
        result = review.analyze(capture())
        self.assertEqual([], result['review_problems'])
        self.assertEqual(1, result['framework_acceptance_records'])
        self.assertIs(False, result['allows_live_movement'])

    def test_unrelated_or_incomplete_evidence_cannot_claim_admission(self):
        for key, value in [('scope', 'selection_only'), ('request', [2, 999, 1, 0, 0, 0]),
                           ('query_handle', 0xffffffff), ('exceptional', True), ('framework_result_byte', 7),
                           ('framework_qpc', 115), ('framework_requests', 2), ('completed', False),
                           ('attempt_result_byte', 0), ('query_position', None), ('allow_alternate', None)]:
            with self.subTest(key=key):
                rows = capture()
                rows[1][key] = value
                self.assertTrue(review.analyze(rows)['review_problems'])

    def test_loss_and_partial_shutdown_are_explicit(self):
        for key, value in [('dropped', 1), ('write_failed', True), ('truncated', True),
                           ('transactions_started', 2), ('context_overflow', 1), ('committed', 0)]:
            with self.subTest(key=key):
                rows = capture()
                rows[-1][key] = value
                result = review.analyze(rows)
                self.assertTrue(result['review_problems'])
                self.assertIs(False, result['allows_live_movement'])
        self.assertTrue(review.analyze(capture()[:-1])['review_problems'])

    def test_unknown_and_selection_only_are_not_admitted(self):
        rows = capture()
        row = rows[1]
        row.update(scope='selection_only', framework_requests=0, framework_qpc=0,
                   framework_accepted=None, framework_result_byte=None, request=None,
                   request_matches_selection=False, attempt_result_byte=None, stage='selected_only',
                   request_guard='not_observed', request_guard_bits=0, action_vtable_kind='unobserved', action_vtable_rva=None,
                   owner_component_vtable_kind='unobserved', owner_component_vtable_rva=None)
        rows[-1]['request_guard_counts']['matched'] = 0
        result = review.analyze(rows)
        self.assertEqual([], result['review_problems'])
        self.assertEqual(0, result['framework_acceptance_records'])

    def test_each_precise_guard_refusal_remains_explicit(self):
        for guard, bits in review.GUARD_BITS.items():
            if guard in {'not_observed', 'matched'}:
                continue
            with self.subTest(guard=guard):
                rows = capture()
                row = rows[1]
                row.update(request=None, request_matches_selection=False, framework_accepted=None,
                           request_guard=guard, request_guard_bits=bits, stage='framework_request_guard_refused')
                if bits < 1:
                    row.update(action_vtable_kind='unobserved', action_vtable_rva=None)
                elif guard == 'unreadable_action_vtable':
                    row.update(action_vtable_kind='unobserved', action_vtable_rva=None)
                elif guard == 'action_vtable_mismatch':
                    row.update(action_vtable_kind='ledge_secondary_10', action_vtable_rva=0x3c0d840)
                if bits < 15 or guard == 'unreadable_component_vtable':
                    row.update(owner_component_vtable_kind='unobserved', owner_component_vtable_rva=None)
                elif guard == 'component_vtable_mismatch':
                    row.update(owner_component_vtable_kind='other_target_rva', owner_component_vtable_rva=0x1000)
                rows[-1]['request_guard_counts'] = {k: int(k == guard) for k in review.GUARD_BITS}
                result = review.analyze(rows)
                self.assertEqual([f'Request guard refusal: {guard} (1)'], result['review_problems'])
                self.assertEqual({guard: 1}, result['request_guards'])
                self.assertEqual(0, result['framework_acceptance_records'])
                self.assertIs(False, result['allows_live_movement'])

    def test_guard_diagnostics_and_target_relative_identity_are_validated(self):
        for key, value in [('request_guard', 'new_guard'), ('request_guard', 'not_observed'),
                           ('request_guard_bits', 1023), ('action_vtable_kind', 'ledge_secondary_10'),
                           ('action_vtable_rva', 0x7ff123450000), ('action_vtable_kind', 'outside_target'),
                           ('request_guard', 'null_holder'), ('owner_component_vtable_kind', 'outside_target'),
                           ('owner_component_vtable_rva', 0x7ff123456000), ('owner_component_vtable_rva', 0x3c0d880)]:
            rows = capture()
            rows[1][key] = value
            self.assertTrue(review.analyze(rows)['review_problems'])
        rows = capture()
        rows[-1]['request_guard_counts']['matched'] = 0
        self.assertIn('Guard shutdown accounting mismatch', review.analyze(rows)['review_problems'])

    def test_revision2_capture_remains_historical_observation(self):
        rows = capture()
        rows[0]['probe_revision'] = 2
        for key in ('request_guard', 'request_guard_bits', 'action_vtable_kind', 'action_vtable_rva',
                    'owner_component_vtable_kind', 'owner_component_vtable_rva'):
            del rows[1][key]
        del rows[-1]['request_guard_counts']
        self.assertEqual([], review.analyze(rows)['review_problems'])

    def test_revision3_owner_mismatch_keeps_original_diagnostics(self):
        rows = capture()
        rows[0]['probe_revision'] = 3
        row = rows[1]
        row.update(request_guard='action_owner_mismatch', request_guard_bits=3, request=None,
                   request_matches_selection=False, framework_accepted=None, stage='framework_request_guard_refused')
        del row['owner_component_vtable_kind']
        del row['owner_component_vtable_rva']
        rows[-1]['request_guard_counts'] = {k: int(k == 'action_owner_mismatch') for k in review.GUARD_BITS_V3}
        result = review.analyze(rows)
        self.assertEqual(['Request guard refusal: action_owner_mismatch (1)'], result['review_problems'])
        self.assertEqual({'action_owner_mismatch': 1}, result['request_guards'])

    def test_duplicate_or_out_of_scope_transactions_are_rejected(self):
        rows = capture()
        rows.insert(2, copy.deepcopy(rows[1]))
        for key in ('transactions_started', 'transactions_ended', 'reserved', 'committed'):
            rows[-1][key] = 2
        self.assertTrue(review.analyze(rows)['review_problems'])
        for key, value in [('query_qpc', 99), ('end_qpc', 90), ('query_position', [1, float('nan'), 3]),
                           ('depth', 5), ('stage', 'new_unverified_stage')]:
            rows = capture()
            rows[1][key] = value
            self.assertTrue(review.analyze(rows)['review_problems'])


if __name__ == '__main__':
    unittest.main()
