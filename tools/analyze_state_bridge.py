"""Review bounded bridge captures; this tool never unlocks live movement."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path

GATES = ('module generation player not_loading game_started not_paused frame_dt clock '
         'observer crouch contact alive resources not_carrying not_mounted not_ladder '
         'not_dialogue not_pickpocket weapon_sheathed not_combat ordinary_body '
         'normal_animation ordinary_xgen attached_camera native_only_session').split()
ADAPTER = ('no_snapshot context_changed serial_rollback unsafe stale identity_mismatch '
           'request_mode repeated_snapshot idle core contention').split()
MOTION = ('disabled invalid_settings no_trigger active cooldown completed travel_budget '
          'manual_stop binding_changed unknown_or_unsafe stale_native stale_observer '
          'invalid_clock sequence_rollback frame_gap rejected_request invalid_request zero_input').split()
MASK = (1 << len(GATES)) - 1


def reject_constant(value):
    raise ValueError('Nonfinite JSON constant: ' + value)


def read_capture(path):
    rows = []
    with Path(path).open(encoding='utf-8') as source:
        while True:
            line = source.readline(4097)
            if not line:
                break
            if len(line) > 4096 or len(rows) >= 100003:
                raise ValueError('Capture exceeds bridge bounds')
            row = json.loads(line, parse_constant=reject_constant)
            if not isinstance(row, dict):
                raise ValueError('Expected JSON objects')
            rows.append(row)
    return rows


def enum_counts(rows, key, names):
    return dict(Counter(names[r[key]] if type(r.get(key)) is int and 0 <= r[key] < len(names)
                        else 'unknown' for r in rows))


def numeric_range(rows, key):
    values = [r[key] for r in rows if type(r.get(key)) in (int, float) and math.isfinite(r[key])]
    return [min(values), max(values)] if values else None


def summarize(path, expected_motion=False):
    rows = read_capture(path)
    starts = [r for r in rows if r.get('event') == 'started']
    stops = [r for r in rows if r.get('event') == 'stopped']
    if len(starts) != 1 or starts[0].get('protocol') not in (1, 2):
        raise ValueError('Expected exactly one supported bridge header')
    header = starts[0]
    frequency = header.get('qpc_frequency')
    if type(frequency) is not int or frequency <= 0:
        raise ValueError('Invalid QPC frequency')
    states = [r for r in rows if r.get('event') == 'state']
    requests = [r for r in rows if r.get('event') == 'request']
    for row in states + requests:
        if type(row.get('qpc')) is not int:
            raise ValueError('Invalid sample timestamp')
        for key in ('known', 'safe'):
            if type(row.get(key)) is not int or not 0 <= row[key] <= MASK:
                raise ValueError('Invalid gate mask')
        if row['safe'] & ~row['known']:
            raise ValueError('Safe gate without known evidence')
    ordered = sorted(requests, key=lambda r: r['qpc'])
    spans = []
    run = []
    previous = None
    for row in ordered:
        context = tuple(row.get(k) for k in ('epoch', 'generation', 'identity', 'request_serial', 'reset_serial'))
        gap = (row['qpc'] - previous['qpc']) / frequency if previous else 0
        previous_context = tuple(previous.get(k) for k in ('epoch', 'generation', 'identity', 'request_serial', 'reset_serial')) if previous else None
        if row.get('would_apply') is not True or gap > .1 or context != previous_context:
            if run:
                spans.append(run)
                run = []
        if row.get('would_apply') is True:
            run.append(row)
        previous = row
    if run:
        spans.append(run)
    pulse_runs = [{'requests': len(run), 'wall_span_seconds': (run[-1]['qpc'] - run[0]['qpc']) / frequency,
                   'summed_simulation_dt': sum(r.get('dt', 0) for r in run),
                   'request_serial': run[0].get('request_serial')} for run in spans]
    all_samples = states + requests
    full_states = [r for r in states if r['known'] == MASK and r['safe'] == MASK]
    valid_requests = [r for r in requests if r.get('valid') is True]
    shutdown = stops[0] if len(stops) == 1 else None
    times = sorted(r['qpc'] for r in states)
    gaps = [(b - a) / frequency for a, b in zip(times, times[1:])]
    problems = []
    if shutdown is None:
        problems.append('No complete shutdown record')
    elif any(shutdown.get(k) for k in ('publish_errors', 'dropped', 'fatal')):
        problems.append('Shutdown reports errors, drops or fatal refusal')
    if header.get('motion_compiled') is not expected_motion:
        problems.append('Capture motion profile differs from the requested review profile')
    if len({r.get('thread') for r in states}) != 1:
        problems.append('Expected exactly one observed script publisher thread')
    if not full_states:
        problems.append('No all-safe state observed')
    # The adapter can return ContextChanged before its identity gate executes.
    # A default false identity_match is not itself a failed identity check.
    if any(r.get('adapter_reason') == 5 or not r.get('entity') or
           r.get('entity') != r.get('owner_entity') for r in valid_requests):
        problems.append('Valid snapshots include an entity disagreement or adapter identity refusal')
    actual = [r for r in requests if r.get('modified') is True]
    if shutdown and shutdown.get('modified') != len(actual):
        problems.append('Applied-write counter differs from retained motion audit records')
    if shutdown and shutdown.get('audit_busy', 0):
        problems.append('Audit reservation contention needs review; affected motion must refuse')
    if not expected_motion and (actual or (shutdown and shutdown.get('modified'))):
        problems.append('Movement modification reported')
    unsafe_actual = [r for r in actual if r.get('valid') is not True or r.get('safe') != MASK
                     or r.get('known') != MASK or r.get('dry_run') is not False
                     or r.get('enabled') is not True or r.get('identity_match') is not True
                     or r.get('flags') not in (0x1060, 0x201060)]
    vertical_actual = [r for r in actual if r.get('before', [None]*3)[2] != r.get('candidate', [None]*3)[2]]
    if unsafe_actual or vertical_actual:
        problems.append('Modified requests violate a safety or vertical-preservation invariant')
    groups = {}
    for row in actual:
        key = tuple(row.get(k) for k in ('generation', 'identity', 'request_serial', 'reset_serial'))
        groups.setdefault(key, []).append(row)
    action_groups = []
    bound_violations = 0
    for key, group in groups.items():
        travel = sum(math.hypot(*r['candidate'][:2]) * r['dt'] for r in group)
        span = (max(r['qpc'] for r in group) - min(r['qpc'] for r in group)) / frequency
        peak = max(r.get('gain', 1) for r in group)
        speed = max(math.hypot(*r['candidate'][:2]) for r in group)
        exceeded = any(type(header.get(name)) in (int, float) and value > header[name] + 1e-4
                       for name, value in [('request_travel_budget', travel), ('duration_limit', span),
                                           ('peak_gain', peak), ('horizontal_cap', speed)])
        bound_violations += int(exceeded)
        action_groups.append({'context': list(key), 'modified_requests': len(group),
                              'wall_span_seconds': span, 'integrated_candidate_request_travel': travel,
                              'peak_gain': peak, 'maximum_candidate_horizontal': speed,
                              'exceeds_declared_bounds': exceeded})
    if bound_violations:
        problems.append('Modified action groups exceed a declared header bound')
    return {
        'header': header, 'complete_shutdown': shutdown is not None,
        'state_records': len(states), 'request_records': len(requests),
        'script_thread_count': len({r.get('thread') for r in states}),
        'request_thread_count': len({r.get('thread') for r in requests}),
        'all_safe_state_records': len(full_states),
        'unknown_gate_state_counts': {name: sum(not r['known'] & (1 << bit) for r in states) for bit, name in enumerate(GATES)},
        'unsafe_gate_state_counts': {name: sum(not r['safe'] & (1 << bit) for r in states) for bit, name in enumerate(GATES)},
        'first_refusal_counts': dict(Counter(r.get('gate') for r in states)),
        'native_entity_agreements': sum(bool(r.get('entity')) and r.get('entity') == r.get('owner_entity') for r in valid_requests),
        'adapter_identity_matches': sum(r.get('identity_match') is True for r in requests),
        'adapter_identity_refusals': sum(r.get('adapter_reason') == 5 for r in requests),
        'bridge_events': dict(Counter(r.get('reason') for r in rows if r.get('event') == 'bridge_event')),
        'eligible_requests': sum(r.get('eligible') is True for r in requests),
        'adapter_reasons': enum_counts(requests, 'adapter_reason', ADAPTER),
        'motion_reasons': enum_counts(requests, 'motion_reason', MOTION),
        'request_flags': dict(Counter(hex(r.get('flags', 0)) for r in requests)),
        'dt_range': numeric_range(states, 'dt'), 'observer_age_range': numeric_range(states, 'observer_age'),
        'snapshot_age_range': numeric_range(requests, 'snapshot_age'),
        'zero_dt_state_records': sum(r.get('dt') == 0 for r in states),
        'maximum_logged_state_gap_seconds': max(gaps, default=0),
        'contexts': sorted({(r.get('epoch'), r.get('generation'), r.get('identity')) for r in states}),
        'reset_serials': sorted({r.get('reset_serial') for r in states}),
        'would_modify_requests': sum(r.get('would_modify') is True for r in requests),
        'would_apply_requests': sum(r.get('would_apply') is True for r in requests),
        'actual_modified_requests': len(actual),
        'unsafe_modified_requests': len(unsafe_actual),
        'vertical_modified_requests': len(vertical_actual),
        'modified_gain_range': numeric_range(actual, 'gain'),
        'modified_action_groups': action_groups,
        'groups_exceeding_declared_bounds': bound_violations,
        'maximum_contiguous_apply_span_seconds': max((r['wall_span_seconds'] for r in pulse_runs), default=0),
        'maximum_contiguous_apply_requests': max((r['requests'] for r in pulse_runs), default=0),
        'apply_runs': pulse_runs, 'shutdown_counters': shutdown, 'review_problems': problems,
        'limitation': 'Logged state is sampled/change-only. Candidate/applied request records do not measure world displacement or prove collision. Manual cancellation/pause/load review is required. This report never unlocks live movement.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--motion', action='store_true', help='Review the controlled motion profile; never grants permission')
    args = parser.parse_args()
    report = json.dumps(summarize(args.capture, expected_motion=args.motion), indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report + '\n', encoding='utf-8')
    print(report)
