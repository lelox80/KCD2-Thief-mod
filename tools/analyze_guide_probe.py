"""Validate retained guide-load observations; never authorize registration/motion."""
import argparse
import json
from pathlib import Path
import re


def analyze(path):
    rows = [json.loads(s) for s in Path(path).read_text(encoding='utf-8').splitlines()]
    issues = []
    if not rows or not isinstance(rows[0], dict) or rows[0].get('event') != 'started':
        return {'review_problems': ['Missing start header'], 'allows_live_movement': False, 'allows_registration': False}
    header = rows[0]
    if any(header.get(k) != v for k, v in {
            'probe': 'guide-load', 'protocol': 1, 'motion_compiled': False, 'registers_guides': False,
            'record_limit': 1024, 'max_elements': 65535, 'fingerprint': 'fnv1a64-typed-le',
            'archive_source_verified': False}.items()):
        issues.append('Unknown or unsafe observation contract')
    stopped = rows[-1] if isinstance(rows[-1], dict) and rows[-1].get('event') == 'stopped' else None
    samples = rows[1:-1] if stopped else rows[1:]
    seen = set()
    observed = []
    body_total = index_total = 0
    for row in samples:
        prior_issues = len(issues)
        if not isinstance(row, dict):
            issues.append('Non-object capture record')
            continue
        if row.get('event') != 'guide_load':
            issues.append('Unknown capture event')
            continue
        tx = row.get('transaction')
        if type(tx) is not int or tx <= 0:
            issues.append('Duplicate or invalid transaction')
        elif tx in seen:
            issues.append('Duplicate or invalid transaction')
        else:
            seen.add(tx)
        qualified = (row.get('complete') is True and row.get('exceptional') is False
                     and row.get('suppressed') is False and row.get('bodies') == 1
                     and row.get('indexes') == 1 and row.get('result') == 'observed')
        if row.get('registry_observed') is not qualified:
            issues.append('Observation qualification differs from native gates')
        for key in ('complete', 'exceptional', 'suppressed', 'registry_observed'):
            if type(row.get(key)) is not bool:
                issues.append('Invalid boolean: ' + key)
        if type(row.get('depth')) is not int or not 1 <= row['depth'] <= 4:
            issues.append('Unbounded callback depth')
        if row.get('level') not in ('unknown', 'other', 'kutnohorsko'):
            issues.append('Invalid level classification')
        for k in ('begin_qpc', 'end_qpc', 'thread', 'bodies', 'indexes', 'ledge_count', 'marker_count', 'member_index'):
            if type(row.get(k)) is not int or row[k] < 0:
                issues.append('Invalid scalar: ' + k)
        if type(row.get('end_qpc')) is int and type(row.get('begin_qpc')) is int and row['end_qpc'] < row['begin_qpc']:
            issues.append('Clock order invalid')
        for k in ('ledge_count', 'marker_count'):
            if type(row.get(k)) is int and row[k] > 65535:
                issues.append('Array work bound exceeded')
        for k in ('ledge_fnv1a64', 'structural_ledge_fnv1a64', 'marker_fnv1a64'):
            if not isinstance(row.get(k), str) or not re.fullmatch(r'[0-9a-f]{16}', row[k]):
                issues.append('Invalid typed digest: ' + k)
        if type(row.get('bodies')) is int and row['bodies'] >= 0:
            body_total += row['bodies']
        if type(row.get('indexes')) is int and row['indexes'] >= 0:
            index_total += row['indexes']
        if qualified and len(issues) == prior_issues:
            observed.append(row)
    if len(samples) > 1024:
        issues.append('Record limit exceeded')
    if stopped is None:
        issues.append('Missing complete shutdown')
    else:
        for k in ('transactions', 'ended', 'reserved', 'committed'):
            if stopped.get(k) != len(samples):
                issues.append('Shutdown accounting differs: ' + k)
        if stopped.get('dropped') != 0 or stopped.get('overflow') != 0 or stopped.get('truncated') is not False or stopped.get('write_failed') is not False:
            issues.append('Shutdown reports dropped/truncated/failed observation')
        calls = stopped.get('calls')
        if (not isinstance(calls, list) or len(calls) != 3 or
                any(type(v) is not int or v < 0 for v in calls)):
            issues.append('Invalid hook counters')
        elif calls[0] < len(samples) or calls[1] < body_total or calls[2] < index_total:
            issues.append('Hook counters smaller than retained observations')
    return {'header': header, 'complete_shutdown': stopped is not None, 'transactions': len(samples),
            'registry_observations': len(observed), 'kutnohorsko_member1_observations': sum(r['level'] == 'kutnohorsko' and r['member_index'] == 1 for r in observed),
            'observations': observed, 'shutdown_counters': stopped, 'review_problems': sorted(set(issues)),
            'allows_live_movement': False, 'allows_registration': False, 'archive_source_verified': False,
            'limitation': 'Copied callback-scoped typed arrays after native index construction. Pointer/count stability does not prove atomic content against unrelated concurrent host mutations. FNV fingerprints are diagnostic comparisons, not cryptographic proof. Input archive/mount choice, persistent registry lifetime, body clearance and traversal remain unverified.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = analyze(args.capture)
    text = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(text + '\n', encoding='utf-8')
    print(text)


if __name__ == '__main__':
    main()
