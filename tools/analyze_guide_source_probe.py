"""Validate source archive observations; never enable mounting/registration/motion."""
import argparse
import json
from pathlib import Path


def analyze(path):
    rows = [json.loads(s) for s in Path(path).read_text(encoding='utf-8').splitlines()]
    issues = []
    denied = dict(allows_live_movement=False, allows_registration=False, allows_mounting=False)
    if not rows or not isinstance(rows[0], dict) or rows[0].get('event') != 'started':
        return dict(review_problems=['Missing start header'], **denied)
    header = rows[0]
    for k, value in dict(probe='guide-source', protocol=1, motion_compiled=False, mounts_archives=False,
                         registers_guides=False, record_limit=1024, logical_path_capacity=260,
                         archive_path_capacity=512).items():
        if type(header.get(k)) is not type(value) or header[k] != value:
            issues.append('Unknown or unsafe observation contract')
    stopped = rows[-1] if isinstance(rows[-1], dict) and rows[-1].get('event') == 'stopped' else None
    samples = rows[1:-1] if stopped else rows[1:]
    seen = set()
    attributed = []
    lookup_total = 0
    results = ('not_observed', 'caller', 'path_unknown', 'member_mismatch', 'unreadable', 'changed',
               'provider_unknown', 'empty', 'truncated', 'bad_path', 'archive_observed')
    archives = ('unknown', 'base_level_pak', 'owned_mod_pak', 'other_pak', 'other_path')
    for row in samples:
        prior = len(issues)
        if not isinstance(row, dict) or row.get('event') != 'guide_source':
            issues.append('Unknown capture event')
            continue
        tx = row.get('transaction')
        if type(tx) is not int or tx <= 0:
            issues.append('Invalid transaction')
        elif tx in seen:
            issues.append('Duplicate transaction')
        else:
            seen.add(tx)
        for key in ('complete', 'exceptional', 'suppressed', 'expected_known', 'archive_attributed'):
            if type(row.get(key)) is not bool:
                issues.append('Invalid boolean: ' + key)
        for key in ('begin_qpc', 'end_qpc', 'thread', 'depth', 'lookups', 'matches'):
            if type(row.get(key)) is not int or row[key] < 0:
                issues.append('Invalid scalar: ' + key)
        if type(row.get('member_index')) is not int or not -2147483648 <= row['member_index'] <= 2147483647:
            issues.append('Invalid member index')
        if type(row.get('depth')) is int and not 1 <= row['depth'] <= 4:
            issues.append('Unbounded callback depth')
        if type(row.get('end_qpc')) is int and type(row.get('begin_qpc')) is int and row['end_qpc'] < row['begin_qpc']:
            issues.append('Clock order invalid')
        if type(row.get('lookups')) is int and row['lookups'] >= 0:
            lookup_total += row['lookups']
            if type(row.get('matches')) is int and row['matches'] > row['lookups']:
                issues.append('More file matches than observed lookup calls')
        if row.get('namespace') not in ('unknown', 'levels', 'data_levels', 'other'):
            issues.append('Invalid closed logical namespace classification')
        if row.get('level') not in ('unknown', 'other', 'kutnohorsko'):
            issues.append('Invalid closed level classification')
        if row.get('result') not in results or row.get('archive') not in archives:
            issues.append('Invalid closed result/archive classification')
        qualified = (row.get('complete') is True and row.get('exceptional') is False and row.get('suppressed') is False
                     and row.get('expected_known') is True and row.get('lookups') == 1 and row.get('matches') == 1
                     and row.get('result') == 'archive_observed' and row.get('archive') in archives[1:4])
        if row.get('archive_attributed') is not qualified:
            issues.append('Attribution differs from native context/provider gates')
        if qualified and len(issues) == prior:
            attributed.append(row)
    if len(samples) > 1024:
        issues.append('Record limit exceeded')
    if stopped is None:
        issues.append('Missing complete shutdown')
    else:
        for key in ('transactions', 'ended', 'reserved', 'committed'):
            if type(stopped.get(key)) is not int or stopped[key] != len(samples):
                issues.append('Shutdown accounting differs: ' + key)
        if stopped.get('dropped') != 0 or stopped.get('overflow') != 0 or stopped.get('truncated') is not False or stopped.get('write_failed') is not False:
            issues.append('Shutdown reports dropped/truncated/failed observation')
        calls = stopped.get('calls')
        if not isinstance(calls, list) or len(calls) != 2 or any(type(v) is not int or v < 0 for v in calls):
            issues.append('Invalid hook counters')
        elif calls[0] < len(samples) or calls[1] < lookup_total:
            issues.append('Hook counters smaller than retained observations')
    return dict(header=header, complete_shutdown=stopped is not None, transactions=len(samples),
                archive_attributions=len(attributed),
                kutnohorsko_member1_attributions=sum(r['level'] == 'kutnohorsko' and r['member_index'] == 1 for r in attributed),
                attributions=attributed, shutdown_counters=stopped, review_problems=sorted(set(issues)),
                limitation='Callback-scoped exact logical-file/source match and known CryPak archive-name return. No lookup is invoked by the probe. Missing, empty, skipped, unsupported or ambiguous results are unknown and never prove loose-file loading. Archive names do not prove overlay precedence, load success, registry contents/lifetime, collision clearance or traversal.',
                **denied)


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
