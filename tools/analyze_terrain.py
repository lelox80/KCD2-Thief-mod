"""Review passive terrain traces alongside native audits; never grants motion permission.

Phase names are operator labels, not inferred geometry. Sampled position chords
are observations, not exact pulse distance, ground clearance or edge look-ahead.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re

from analyze_state_bridge import read_capture, summarize as summarize_bridge, MASK

MARKER = '[KT] terrain '
COUNTERS = ('session', 'seq', 'samples', 'generation', 'epoch', 'identity', 'request', 'reset', 'frame', 'observer')
PHASES = ('slope', 'stairs', 'ledge')
EVENTS = ('start', 'sample', 'stop', 'context', 'gap', 'refused')


def number(value):
    try:
        n = float(value)
    except (TypeError, ValueError):
        raise ValueError('Invalid terrain number') from None
    if not math.isfinite(n) or abs(n) >= 1e12:
        raise ValueError('Nonfinite/out-of-range terrain number')
    return n


def vector(value):
    if value == 'unknown':
        return None
    if not value.startswith('(') or not value.endswith(')'):
        raise ValueError('Invalid terrain vector')
    parts = value[1:-1].split(',')
    if len(parts) != 3:
        raise ValueError('Invalid terrain vector arity')
    return [number(p) for p in parts]


def mask(value):
    if not re.fullmatch(r'b[01]{25}', value):
        raise ValueError('Invalid exact terrain gate mask')
    return sum(1 << i for i, c in enumerate(value[1:]) if c == '1')


def parse_line(line):
    if MARKER not in line:
        return None
    raw = {}
    for field in line.split(MARKER, 1)[1].strip().split():
        key, sep, value = field.partition('=')
        if not sep or not value or key in raw:
            raise ValueError('Malformed or duplicate terrain field')
        raw[key] = value
    if raw.get('v') != '1' or raw.get('event') not in EVENTS:
        raise ValueError('Unsupported terrain schema/event/phase')
    if raw['event'] == 'refused':
        if raw.get('diagnostic') != 'true' or not re.fullmatch(r'[a-z_]+', raw.get('reason', '')):
            raise ValueError('Malformed terrain refusal')
        return dict(raw)
    if raw.get('phase') not in PHASES:
        raise ValueError('Unsupported terrain phase')
    result = dict(raw)
    for key in COUNTERS:
        if not re.fullmatch(r'[0-9]+', raw.get(key, '')) or int(raw[key]) > 8388607:
            raise ValueError('Invalid bounded terrain counter: ' + key)
        result[key] = int(raw[key])
    if result['session'] < 1 or result['session'] > 32:
        raise ValueError('Invalid terrain session')
    if raw.get('entity') == 'unknown':
        result['entity'] = None
    elif not re.fullmatch(r'[0-9]+', raw.get('entity', '')) or int(raw['entity']) > 0xffffffff:
        raise ValueError('Invalid diagnostic entity token')
    else:
        result['entity'] = int(raw['entity'])  # Zero can mean an opaque Lua handle.
    for key in ('async', 'dt', 'elapsed'):
        result[key] = number(raw.get(key))
    if 'boundary_frame' in raw or 'boundary_async' in raw:
        if not re.fullmatch(r'[0-9]+', raw.get('boundary_frame', '')) or int(raw['boundary_frame']) > 8388607:
            raise ValueError('Invalid terrain stop boundary')
        result['boundary_frame'] = int(raw['boundary_frame'])
        result['boundary_async'] = number(raw.get('boundary_async'))
        if raw.get('boundary_valid') not in ('true', 'false'):
            raise ValueError('Missing canonical-boundary validity')
        result['boundary_valid'] = raw['boundary_valid'] == 'true'
    for key in ('native_enabled', 'dry_run', 'pos_valid', 'velocity_valid', 'truncated', 'cached', 'diagnostic'):
        if raw.get(key) not in ('true', 'false'):
            raise ValueError('Invalid terrain boolean: ' + key)
        result[key] = raw[key] == 'true'
    if not result['diagnostic']:
        raise ValueError('Terrain observation must remain diagnostic')
    for key in ('pos', 'velocity'):
        result[key] = vector(raw.get(key, ''))
        if result[key + '_valid'] != (result[key] is not None):
            raise ValueError('Unknown terrain vector cannot become zero/valid')
        if result[key + '_valid'] and raw.get(key + '_status') != 'ok':
            raise ValueError('Valid vector without successful getter')
    result['known_mask'] = mask(raw.get('known', ''))
    result['safe_mask'] = mask(raw.get('safe', ''))
    if result['safe_mask'] & ~result['known_mask']:
        raise ValueError('Safe terrain gate without known evidence')
    return result


def read_trace(path):
    rows = []
    with Path(path).open(encoding='utf-8', errors='replace') as source:
        for count in range(100001):
            line = source.readline(4097)
            if not line:
                return rows
            if count == 100000 or len(line) > 4096:
                raise ValueError('Terrain log exceeds read bounds')
            row = parse_line(line)
            if row:
                rows.append(row)
                if len(rows) > 65536:
                    raise ValueError('Terrain events exceed bounded sessions')
    raise ValueError('Terrain read bound')


def context(row):
    return tuple(row.get(k) for k in ('epoch', 'generation', 'identity'))


def packet_agrees(sample, native):
    # Native audit prints %.9g; Lua prints six decimals. Compare within the
    # combined display precision rather than silently accepting different runs.
    for key, other in [('known_mask', 'known'), ('safe_mask', 'safe'), ('reset', 'reset_serial'),
                       ('request', 'request_serial'), ('observer', 'observer_sequence'),
                       ('native_enabled', 'enabled'), ('dry_run', 'dry_run')]:
        if sample[key] != native.get(other):
            return False
    if sample['entity'] and sample['entity'] != native.get('entity'):
        return False
    for key, other in [('async', 'async_seconds'), ('dt', 'dt')]:
        value = native.get(other)
        if type(value) not in (int, float) or not math.isfinite(value):
            return False
        precision = .5 * 10 ** (math.floor(math.log10(abs(value))) - 8) if value else 0
        if abs(sample[key] - value) > precision + .500001e-6:
            return False
    return True


def cancellation_review(rows):
    """A consumed/refused serial must never write when its context returns safe."""
    refused, violations = {}, []
    for row in sorted((r for r in rows if r.get('event') == 'request'), key=lambda r: r['qpc']):
        if not row.get('request_serial'):
            continue
        key = context(row) + (row['request_serial'],)
        unsafe = (not row.get('valid') or row.get('known') != MASK or row.get('safe') != MASK
                  or not row.get('enabled') or row.get('adapter_reason') in (1, 2, 4, 5, 6, 7, 10)
                  or (row.get('adapter_reason') == 9 and row.get('motion_reason') in (2, 4, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17)))
        if row.get('modified') and key in refused:
            violations.append({'context': list(key), 'first_refusal_qpc': refused[key], 'write_qpc': row['qpc']})
        if unsafe:
            refused.setdefault(key, row['qpc'])
    return {'refused_serial_contexts': len(refused), 'writes_after_refused_serial': violations}


def summarize(trace_path, native_path):
    traces, native = read_trace(trace_path), read_capture(native_path)
    bridge = summarize_bridge(native_path, expected_motion=True)
    samples = [r for r in traces if r['event'] == 'sample']
    native_by_frame = {}
    for row in native:
        if row.get('event') in ('request', 'state'):
            native_by_frame.setdefault(context(row) + (row['sequence'],), []).append(row)
    phases = []
    phase_events = [r for r in traces if r['event'] != 'refused']
    for key in sorted({(r['epoch'], r['session'], r['phase']) for r in phase_events}):
        events = [r for r in phase_events if (r['epoch'], r['session'], r['phase']) == key]
        records = [r for r in events if r['event'] == 'sample']
        problems = []
        if sum(r['event'] == 'start' for r in events) != 1:
            problems.append('Expected one phase start')
        if sum(r['event'] == 'stop' for r in events) != 1:
            problems.append('Phase has no unique stop')
        stops = [r for r in events if r['event'] == 'stop']
        if stops and (stops[0].get('reason') != 'command' or stops[0]['truncated']):
            problems.append('Phase ended automatically or on a diagnostic fault; trial coverage needs review')
        if stops and stops[0].get('boundary_valid') is not True:
            problems.append('Cached stop boundary; final native association coverage is incomplete')
        if not records:
            problems.append('No terrain samples')
        if len(records) > 900 or len(events) > 2048:
            problems.append('Phase exceeded diagnostic bounds')
        if [r['seq'] for r in events] != list(range(1, len(events) + 1)):
            problems.append('Missing, duplicate or rollback terrain event sequence')
        if ([r['samples'] for r in records] != list(range(1, len(records) + 1))
                or events[-1]['samples'] != len(records)):
            problems.append('Terrain sample counter differs from retained samples')
        if any(b['async'] < a['async'] for a, b in zip(records, records[1:])):
            problems.append('Terrain sample clock rollback')
        points = [r['pos'] for r in records if r['pos'] is not None]
        chord, vertical, intervals = 0., 0., 0
        for a, b in zip(records, records[1:]):
            if (a['pos'] is not None and b['pos'] is not None and context(a) == context(b)
                    and a['reset'] == b['reset'] and 0 < b['async'] - a['async'] <= .25
                    and a['dt'] > 0 and b['dt'] > 0):
                chord += math.hypot(b['pos'][0] - a['pos'][0], b['pos'][1] - a['pos'][1])
                vertical += b['pos'][2] - a['pos'][2]
                intervals += 1
        joined = [r for r in records if context(r) + (r['frame'],) in native_by_frame]
        joined_disagreements = [r for r in joined if any(
            not packet_agrees(r, n) for n in native_by_frame[context(r) + (r['frame'],)])]
        if joined_disagreements:
            problems.append('Exact-frame terrain/native packet disagreement')
        if not joined:
            problems.append('No exact-frame packet candidates; run correlation is unverified')
        start_frame = events[0]['frame']
        end_frame = events[-1].get('boundary_frame', events[-1]['frame'])
        if end_frame < start_frame or any(r['frame'] < start_frame or r['frame'] > end_frame for r in records):
            problems.append('Terrain samples exceed their canonical phase window')
        applied = [r for r in native if r.get('modified') and context(r) == context(events[0])
                   and start_frame <= r['sequence'] <= end_frame]
        association_reliable = bool(joined) and not problems
        phases.append({'epoch_session_phase': list(key), 'samples': len(records),
                       'stop_reasons': [r.get('reason') for r in events if r['event'] == 'stop'],
                       'explicit_gaps': sum(r['event'] == 'gap' for r in events),
                       'context_changes': sum(r['event'] == 'context' for r in events),
                       'unknown_position_samples': sum(r['pos'] is None for r in records),
                       'unknown_velocity_samples': sum(r['velocity'] is None for r in records),
                       'contact_unsafe_or_unknown_samples': sum(not (r['known_mask'] & r['safe_mask'] & (1 << 10)) for r in records),
                       'height_range': [min(p[2] for p in points), max(p[2] for p in points)] if points else None,
                       'sampled_horizontal_chord_sum': chord, 'sampled_vertical_delta_sum': vertical,
                       'continuous_positive_dt_intervals': intervals,
                       'exact_frame_key_candidates': len(joined),
                       'exact_frame_joined_samples': len(joined) - len(joined_disagreements),
                       'exact_frame_disagreements': len(joined_disagreements),
                       'native_association_reliable': association_reliable,
                       'associated_actual_write_branches': len(applied) if association_reliable else None,
                       'associated_request_serials': sorted({r['request_serial'] for r in applied}) if association_reliable else None,
                       'review_problems': problems})
    cancellations = cancellation_review(native)
    problems = [str(p['epoch_session_phase']) + ': ' + issue for p in phases for issue in p['review_problems']]
    if not phases:
        problems.append('No terrain phase records')
    if cancellations['writes_after_refused_serial']:
        problems.append('Native writes after a refused serial; cancellation/rearm review failed')
    return {'phases': phases, 'sample_records': len(samples),
            'review_problems': problems,
            'events': dict(Counter(r['event'] for r in traces)),
            'native_review_problems': bridge['review_problems'],
            'native_shutdown': bridge['shutdown_counters'],
            'native_unsafe_or_vertical_writes': bridge['unsafe_modified_requests'] + bridge['vertical_modified_requests'],
            'native_bound_violations': bridge['groups_exceeding_declared_bounds'],
            'cancellation_review': cancellations,
            'terrain_acceptance': 'manual observation and complete capture review required',
            'limitations': ['Phase names are operator labels, not measured geometry.',
                            'Sampled chords are not exact trajectory/pulse distance or a clearance measurement.',
                            'A zero entity token may be an opaque Lua handle; native identity remains independently verified.',
                            'Canonical phase boundaries retain up to one frame of console-command ordering ambiguity.',
                            'Support predicates do not predict an edge or measure physical contact-loss latency.',
                            'Refusing new scaling does not brake ordinary movement or momentum already accepted by KCD2.']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('native', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = json.dumps(summarize(args.trace, args.native), indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(result + '\n', encoding='utf-8')
    print(result)
