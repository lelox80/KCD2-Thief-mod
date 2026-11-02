import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('analyze_terrain', ROOT / 'tools/analyze_terrain.py')
terrain = importlib.util.module_from_spec(spec)
spec.loader.exec_module(terrain)
ALL = 'b' + '1' * 25


def trace_line(event='sample', seq=2, frame=102, stamp=10.1, pos='(1,0,1)', **changes):
    samples = 0 if event == 'start' else seq - 1 if event == 'sample' else max(0, seq - 2)
    values = dict(v='1', event=event, phase='slope', session=1, seq=seq, samples=samples,
                  diagnostic='true', generation=1, epoch=1, identity=1, request=4, reset=0,
                  frame=frame, observer=frame, entity=0, dt=.016667, known=ALL, safe=ALL, gate='ok',
                  pos=pos, pos_status='ok', pos_valid='true', velocity='unknown', velocity_status='missing',
                  velocity_valid='false', native_enabled='true', dry_run='false', elapsed=.1,
                  truncated='false', cached='false')
    values['async'] = stamp
    if event == 'stop':
        values.update(boundary_frame=frame, boundary_async=stamp, boundary_valid='true')
    values.update(changes)
    return '[KT] terrain ' + ' '.join(f'{k}={v}' for k, v in values.items()) + '\n'


def native_row(seq=102, serial=4, **changes):
    row = dict(event='request', qpc=seq, epoch=1, generation=1, identity=1, sequence=seq,
               thread=1, entity=42, owner_entity=42, valid=True, known=terrain.MASK, safe=terrain.MASK,
               request_serial=serial, reset_serial=0, enabled=True, dry_run=False, identity_match=True,
               adapter_reason=9, motion_reason=3, modified=True, would_modify=True, would_apply=True,
               async_seconds=10.1, observer_sequence=seq,
               gain=1.2, dt=.016667, flags=0x1060, before=[0, 3.1, 0], candidate=[0, 3.72, 0])
    row.update(changes)
    return row


class TerrainCaptureTests(unittest.TestCase):
    def test_unknown_vectors_and_refusals_are_not_invented_observations(self):
        row = terrain.parse_line(trace_line())
        self.assertIsNone(row['velocity'])
        self.assertEqual(row['entity'], 0)
        row = terrain.parse_line('[KT] terrain v=1 event=refused reason=already_active diagnostic=true\n')
        self.assertEqual(row['reason'], 'already_active')
        for bad in [trace_line(pos='(NaN,0,0)'), trace_line(pos='(0,0,0)', pos_valid='false'),
                    trace_line(known='33554431'), trace_line(seq=8388608), trace_line() + ' seq=3',
                    trace_line(velocity='(0,0,0)', velocity_valid='true', velocity_status='missing')]:
            with self.assertRaises(ValueError):
                terrain.parse_line(bad)

    def test_contact_or_zero_input_refusal_never_rearms_same_serial_even_after_reset(self):
        for refusal in [dict(safe=terrain.MASK & ~(1 << 10)), dict(motion_reason=17)]:
            rows = [native_row(seq=1), native_row(seq=2, modified=False, **refusal),
                    native_row(seq=3, modified=False, motion_reason=2), native_row(seq=4, serial=5)]
            self.assertFalse(terrain.cancellation_review(rows)['writes_after_refused_serial'])
            rows.append(native_row(seq=5, reset_serial=1))
            self.assertEqual(len(terrain.cancellation_review(rows)['writes_after_refused_serial']), 1)

    def test_phase_window_excludes_earlier_same_serial_and_does_not_bridge_gaps(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            trace = [trace_line('start', seq=1, frame=100, stamp=10, pos='(0,0,0)'),
                     trace_line(seq=2, frame=101, stamp=10, pos='(0,0,0)'),
                     trace_line(seq=3, frame=102, stamp=10.1, pos='(1,0,1)'),
                     trace_line(seq=4, frame=130, stamp=12, pos='(100,0,2)'),
                     trace_line('stop', seq=5, frame=140, stamp=12, reason='command')]
            header = dict(event='started', protocol=2, qpc_frequency=1000, motion_compiled=True)
            rows = [header, native_row(seq=90), native_row(seq=102),
                    native_row(seq=102, event='state', modified=False),
                    dict(event='stopped', modified=2, dropped=0, publish_errors=0, fatal=False)]
            (path / 'trace.log').write_text(''.join(trace))
            (path / 'native.jsonl').write_text(''.join(json.dumps(r) + '\n' for r in rows))
            result = terrain.summarize(path / 'trace.log', path / 'native.jsonl')
            phase = result['phases'][0]
            self.assertEqual(phase['associated_actual_write_branches'], 1)
            self.assertEqual(phase['exact_frame_joined_samples'], 1)
            self.assertEqual(phase['sampled_horizontal_chord_sum'], 1)
            self.assertEqual(phase['sampled_vertical_delta_sum'], 1)
            self.assertEqual(phase['unknown_velocity_samples'], 3)
            self.assertFalse(phase['review_problems'])
            (path / 'trace.log').write_text(''.join(trace[:-1]))
            result = terrain.summarize(path / 'trace.log', path / 'native.jsonl')
            self.assertIn('Phase has no unique stop', result['phases'][0]['review_problems'])
            trace[-1] = trace_line('stop', seq=5, frame=140, stamp=12, reason='duration_limit', truncated='true')
            (path / 'trace.log').write_text(''.join(trace))
            result = terrain.summarize(path / 'trace.log', path / 'native.jsonl')
            self.assertFalse(result['phases'][0]['native_association_reliable'])
            self.assertIsNone(result['phases'][0]['associated_actual_write_branches'])

    def test_exact_frame_gate_disagreement_is_reported(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            (path / 'trace').write_text(trace_line('start', seq=1) + trace_line(seq=2) + trace_line('stop', seq=3))
            rows = [dict(event='started', protocol=2, qpc_frequency=1000, motion_compiled=True),
                    native_row(modified=False, safe=0), dict(event='stopped', modified=0)]
            (path / 'native').write_text(''.join(json.dumps(r) + '\n' for r in rows))
            phase = terrain.summarize(path / 'trace', path / 'native')['phases'][0]
            self.assertEqual(phase['exact_frame_disagreements'], 1)
            self.assertFalse(phase['native_association_reliable'])
            self.assertIsNone(phase['associated_actual_write_branches'])
            self.assertIsNone(phase['associated_request_serials'])

    def test_matching_frame_cannot_hide_wrong_serial_time_or_mode(self):
        sample = terrain.parse_line(trace_line())
        self.assertTrue(terrain.packet_agrees(sample, native_row()))
        for changes in [dict(request_serial=999), dict(observer_sequence=888), dict(async_seconds=999),
                        dict(dt=.1), dict(dry_run=True), dict(enabled=False), dict(reset_serial=9)]:
            self.assertFalse(terrain.packet_agrees(sample, native_row(**changes)))
        sample['entity'] = 42
        self.assertFalse(terrain.packet_agrees(sample, native_row(entity=43)))
        sample['async'] = 10000.123456
        self.assertTrue(terrain.packet_agrees(sample, native_row(async_seconds=10000.1235)))

    def test_real_lua_output_conforms_in_both_number_models(self):
        runtimes = [ROOT / '.local/tools/lua.exe', ROOT / '.local/tools/lua-5.1.5-float32/lua-float32.exe']
        if not all(p.is_file() for p in runtimes):
            self.skipTest('Local double and retail-float32 Lua runtimes unavailable')
        with tempfile.TemporaryDirectory() as folder:
            for index, runtime in enumerate(runtimes):
                output = Path(folder) / f'{index}.log'
                subprocess.run([str(runtime), 'tests/terrain_probe_test.lua', str(output)], cwd=ROOT,
                               check=True, capture_output=True, text=True)
                rows = terrain.read_trace(output)
                self.assertEqual([r['event'] for r in rows], ['start', 'sample', 'sample', 'stop'])
                self.assertIsNone(rows[2]['velocity'])


if __name__ == '__main__':
    unittest.main()
