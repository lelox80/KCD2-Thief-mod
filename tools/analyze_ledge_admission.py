"""Validate read-only ledge admission traces; never unlocks movement."""
from __future__ import annotations
import argparse
from collections import Counter
import json
import math
from pathlib import Path


def integer(value, low=0, high=2**64-1):
    return isinstance(value, int) and not isinstance(value, bool) and low <= value <= high


def pair(value):
    return isinstance(value, list) and len(value) == 2 and integer(value[0], -(2**31), 2**31-1) and integer(value[1], 0, 2**32-1)


def vector(value, size):
    return isinstance(value, list) and len(value) == size and all(
        isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) and abs(v) <= 1e7 for v in value)


GUARD_BITS_V3 = {
    'not_observed': 0, 'matched': 255, 'null_holder': 0, 'unreadable_holder': 0, 'empty_holder': 0,
    'unreadable_action_vtable': 1, 'action_vtable_mismatch': 1, 'unreadable_action_owner': 3,
    'null_action_owner': 3, 'action_owner_mismatch': 3, 'action_owner_invalid': 7, 'action_entity_mismatch': 7,
    'null_framework': 15, 'unreadable_framework_core': 15, 'null_framework_core': 15,
    'unreadable_core_resource': 31, 'null_core_resource': 31, 'unreadable_framework_actor': 63,
    'framework_actor_mismatch': 63, 'unreadable_request': 127,
}
GUARD_BITS = {
    'not_observed': 0, 'matched': 2047, 'null_holder': 0, 'unreadable_holder': 0, 'empty_holder': 0,
    'unreadable_action_vtable': 1, 'action_vtable_mismatch': 1,
    'canonical_owner_invalid': 3, 'canonical_entity_mismatch': 3,
    'unreadable_action_component': 7, 'null_action_component': 7, 'unreadable_component_vtable': 15,
    'component_vtable_mismatch': 15, 'unreadable_component_player': 31, 'component_player_mismatch': 31,
    'unreadable_action_owner': 63, 'null_action_owner': 63, 'action_owner_mismatch': 63,
    'null_framework': 127, 'unreadable_framework_core': 127, 'null_framework_core': 127,
    'unreadable_core_resource': 255, 'null_core_resource': 255, 'unreadable_framework_actor': 511,
    'framework_actor_mismatch': 511, 'unreadable_request': 1023,
}
VTABLES = {'ledge_primary': 0x3c0d880, 'ledge_secondary_10': 0x3c0d840, 'ledge_secondary_50': 0x3c0d808}
COMPONENT_VTABLE = 0x3b098e0


def analyze(rows):
    problems = []
    if not rows or rows[0].get('event') != 'started':
        return {'review_problems': ['Missing started header'], 'allows_live_movement': False}
    header = rows[0]
    if (header.get('mode'), header.get('probe'), header.get('probe_revision')) not in {('observe-only', 'ledge-admission', 2), ('observe-only', 'ledge-admission', 3), ('observe-only', 'ledge-admission', 4)} or header.get('motion_compiled') is not False:
        problems.append('Unexpected probe contract')
    if not integer(header.get('qpc_frequency'), 1) or header.get('hook_count') != 4:
        problems.append('Invalid clock or hook contract')
    revision4 = header.get('probe_revision') == 4
    revision3 = header.get('probe_revision') in {3, 4}
    guard_bits = GUARD_BITS if revision4 else GUARD_BITS_V3
    complete = rows[-1].get('event') == 'stopped'
    if not complete:
        problems.append('Missing complete shutdown')
    stop = rows[-1] if complete else {}
    samples = [r for r in rows if r.get('event') == 'ledge_admission']
    if any(r.get('event') not in {'started', 'ledge_admission', 'stopped'} for r in rows):
        problems.append('Unknown event type')
    if sum(r.get('event') == 'started' for r in rows) != 1 or sum(r.get('event') == 'stopped' for r in rows) != int(complete):
        problems.append('Duplicate lifecycle records')
    transactions = set()
    stages = {'host_exception', 'context_unobserved', 'ambiguous_multiple_calls', 'before_query', 'query_unknown',
              'no_query_handle', 'selection_unknown', 'selection_refused', 'query_selection_mismatch',
              'selected_only', 'no_framework_request', 'framework_request_unassociated', 'framework_request_guard_refused', 'framework_request_mismatch', 'framework_result_unknown',
              'attempt_result_unknown', 'attempt_framework_inconsistent', 'framework_accepted', 'framework_refused'}
    for index, row in enumerate(samples):
        prefix = f'Transaction row {index}: '
        txn = row.get('transaction')
        if not integer(txn, 1) or txn in transactions:
            problems.append(prefix + 'invalid or duplicate transaction')
        transactions.add(txn)
        if not integer(row.get('entity'), 1, 2**32-1) or not integer(row.get('thread'), 1, 2**32-1) or not integer(row.get('depth'), 1, 4):
            problems.append(prefix + 'invalid owner/thread/depth')
        begin, end = row.get('begin_qpc'), row.get('end_qpc')
        if not integer(begin, 1) or not integer(end, 1) or end < begin:
            problems.append(prefix + 'invalid lifetime')
            continue
        for key in ('query_qpc', 'selection_qpc', 'framework_qpc'):
            tick = row.get(key)
            if not integer(tick) or tick and not begin <= tick <= end:
                problems.append(prefix + 'out-of-scope event clock')
        for key in ('queries', 'selections', 'framework_requests'):
            if not integer(row.get(key), 0, 2**32-1):
                problems.append(prefix + 'invalid stage count')
        if row.get('selections') and type(row.get('allow_alternate')) is not bool:
            problems.append(prefix + 'untyped selection input')
        if row.get('scope') not in {'standing_attempt', 'selection_only'}:
            problems.append(prefix + 'unknown scope')
        if row.get('stage') not in stages:
            problems.append(prefix + 'unknown stage')
        for key in ('completed', 'exceptional', 'query_return_matches_output', 'request_matches_selection'):
            if type(row.get(key)) is not bool:
                problems.append(prefix + 'untyped status')
        for key, size in (('query_position', 3), ('query_direction', 3), ('query_bounds', 4)):
            if row.get(key) is not None and not vector(row[key], size):
                problems.append(prefix + 'invalid query vector')
        if row.get('query_handle') is not None and not integer(row['query_handle'], 0, 2**32-1):
            problems.append(prefix + 'invalid encoded query handle')
        selection, request = row.get('selection'), row.get('request')
        if selection is not None and not pair(selection):
            problems.append(prefix + 'invalid selection')
        request_valid = isinstance(request, list) and len(request) == 6 and pair(request[:2]) and all(integer(v, 0, 255) for v in request[2:])
        if request is not None and not request_valid:
            problems.append(prefix + 'invalid copied request')
        if revision3:
            guard, bits = row.get('request_guard'), row.get('request_guard_bits')
            kind, rva = row.get('action_vtable_kind'), row.get('action_vtable_rva')
            if guard not in guard_bits or type(bits) is not int or bits != guard_bits.get(guard):
                problems.append(prefix + 'invalid request guard diagnostics')
            if kind not in {'unobserved', 'outside_target', 'other_target_rva', *VTABLES}:
                problems.append(prefix + 'unknown action vtable classification')
            if kind in VTABLES and rva != VTABLES[kind]:
                problems.append(prefix + 'incorrect action vtable identity')
            if kind == 'other_target_rva' and (not integer(rva, 0, 0x5b2d000-1) or rva in VTABLES.values()):
                problems.append(prefix + 'invalid target-relative action identity')
            if kind in {'unobserved', 'outside_target'} and rva is not None:
                problems.append(prefix + 'pointer escaped target-relative identity')
            observed_vtable = guard not in {'not_observed', 'null_holder', 'unreadable_holder', 'empty_holder', 'unreadable_action_vtable'}
            if (not observed_vtable and kind != 'unobserved') or (observed_vtable and kind == 'unobserved'):
                problems.append(prefix + 'action identity precedes its guarded read')
            if guard == 'action_vtable_mismatch' and kind == 'ledge_primary':
                problems.append(prefix + 'vtable mismatch reports expected primary')
            if integer(bits, 3, 2047) and kind != 'ledge_primary':
                problems.append(prefix + 'later request guard lacks primary action identity')
            if revision4:
                component_kind, component_rva = row.get('owner_component_vtable_kind'), row.get('owner_component_vtable_rva')
                if component_kind not in {'unobserved', 'outside_target', 'other_target_rva', 'action_actor'}:
                    problems.append(prefix + 'unknown owner component classification')
                if component_kind == 'action_actor' and component_rva != COMPONENT_VTABLE:
                    problems.append(prefix + 'incorrect C_ActionActor identity')
                if component_kind == 'other_target_rva' and (not integer(component_rva, 0, 0x5b2d000-1) or component_rva == COMPONENT_VTABLE):
                    problems.append(prefix + 'invalid target-relative component identity')
                if component_kind in {'unobserved', 'outside_target'} and component_rva is not None:
                    problems.append(prefix + 'component pointer escaped target-relative identity')
                component_observed = integer(bits, 15, 2047) and guard != 'unreadable_component_vtable'
                if component_observed != (component_kind != 'unobserved'):
                    problems.append(prefix + 'component identity precedes its guarded read')
                if guard == 'component_vtable_mismatch' and component_kind == 'action_actor':
                    problems.append(prefix + 'component mismatch reports expected class')
                if integer(bits, 31, 2047) and component_kind != 'action_actor':
                    problems.append(prefix + 'later guard lacks verified C_ActionActor class')
            if guard == 'matched' and not (request_valid and kind == 'ledge_primary'):
                problems.append(prefix + 'matched guard lacks verified request')
            if guard != 'matched' and request is not None:
                problems.append(prefix + 'request copied without complete guards')
            if (row.get('framework_requests') == 0) != (guard == 'not_observed'):
                problems.append(prefix + 'guard observation count mismatch')
            if row.get('stage') == 'framework_request_guard_refused' and guard in {'not_observed', 'matched'}:
                problems.append(prefix + 'guard refusal lacks precise failed predicate')
            if row.get('stage') == 'framework_request_mismatch' and not (guard == 'matched' and request_valid and pair(selection) and request[:2] != selection):
                problems.append(prefix + 'request mismatch lacks copied differing pair')
        if row.get('request_matches_selection') and not (request_valid and pair(selection) and request[:2] == selection):
            problems.append(prefix + 'incorrect request association')
        for key in ('attempt_result_byte', 'framework_result_byte'):
            if row.get(key) is not None and not integer(row[key], 0, 255):
                problems.append(prefix + 'invalid native return byte')
        accepted = row.get('framework_accepted')
        if accepted is not None and (type(accepted) is not bool or not request_valid or row.get('framework_result_byte') not in (0, 1) or accepted != bool(row['framework_result_byte'])):
            problems.append(prefix + 'unproven framework boolean')
        if row.get('stage') in {'framework_accepted', 'framework_refused'}:
            expected = row['stage'] == 'framework_accepted'
            ticks = [row.get(k) for k in ('query_qpc', 'selection_qpc', 'framework_qpc')]
            if not (row.get('scope') == 'standing_attempt' and row.get('completed') is True and row.get('exceptional') is False
                    and [row.get(k) for k in ('queries', 'selections', 'framework_requests')] == [1, 1, 1]
                    and pair(selection) and selection[0] != -1 and selection[1] != 2**32-1
                    and row.get('query_handle') == selection[1] and row.get('query_return_matches_output') is True
                    and all(vector(row.get(k), n) for k, n in
                            (('query_position', 3), ('query_direction', 3), ('query_bounds', 4)))
                    and request_valid and request[:2] == selection and row.get('request_matches_selection') is True
                    and (not revision3 or row.get('request_guard') == 'matched')
                    and row.get('attempt_result_byte') == 1
                    and accepted is expected and row.get('framework_result_byte') == int(expected)
                    and all(integer(t, 1) for t in ticks) and ticks == sorted(ticks)):
                problems.append(prefix + 'framework stage lacks complete comparable evidence')
        if row.get('scope') == 'selection_only' and row.get('stage') == 'framework_accepted':
            problems.append(prefix + 'selection is not framework admission')
    if complete:
        for key in ('dropped', 'owner_refused', 'unreadable', 'context_overflow', 'unlinked_framework'):
            if not integer(stop.get(key)) or stop[key] != 0:
                problems.append('Nonzero or invalid ' + key)
        for key in ('write_failed', 'truncated'):
            if stop.get(key) is not False:
                problems.append('Capture ' + key)
        for key in ('transactions_started', 'transactions_ended', 'reserved', 'committed'):
            if not integer(stop.get(key)) or stop[key] != len(samples):
                problems.append('Shutdown accounting mismatch: ' + key)
    guard_counts = dict(Counter(r.get('request_guard') for r in samples if r.get('framework_requests'))) if revision3 else {}
    if revision3 and complete:
        counts = stop.get('request_guard_counts')
        if not isinstance(counts, dict) or set(counts) != set(guard_bits) or not all(integer(v) for v in counts.values()):
            problems.append('Missing or invalid guard shutdown counters')
        elif (sum(counts.values()) != sum(r.get('framework_requests', 0) for r in samples)
              or counts['not_observed'] != 0
              or (all(r.get('framework_requests', 0) <= 1 for r in samples)
                  and any(counts[k] != guard_counts.get(k, 0) for k in guard_bits))):
            problems.append('Guard shutdown accounting mismatch')
        else:
            for guard, count in counts.items():
                if count and guard not in {'matched', 'not_observed'}:
                    problems.append(f'Request guard refusal: {guard} ({count})')
    if not samples:
        problems.append('No transaction observations')
    return {
        'header': header, 'complete_shutdown': complete, 'records': len(samples),
        'scopes': dict(Counter(r.get('scope') for r in samples)),
        'stages': dict(Counter(r.get('stage') for r in samples)),
        'request_guards': guard_counts,
        'framework_acceptance_records': sum(r.get('stage') == 'framework_accepted' for r in samples),
        'review_problems': problems, 'shutdown': stop, 'allows_live_movement': False,
        'limitation': 'Framework acceptance is queue/request acceptance only, not animation start/completion, collision clearance or new ledge registration.'
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        rows = [json.loads(line) for line in args.capture.read_text(encoding='utf-8').splitlines() if line]
        if not all(isinstance(row, dict) for row in rows):
            raise ValueError('Record is not an object')
        result = analyze(rows)
    except (ValueError, TypeError) as error:
        result = {'review_problems': ['Malformed capture: ' + str(error)], 'allows_live_movement': False}
    text = json.dumps(result, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text + '\n', encoding='utf-8')
    print(text)


if __name__ == '__main__':
    main()
