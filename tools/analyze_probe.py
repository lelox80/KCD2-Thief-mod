"""Summarize a filtered KT log; measurements are not movement acceptance results."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re


def analyze(path):
    lines = path.read_text(encoding='utf8', errors='replace').splitlines()
    samples = []
    for line in lines:
        if '[KT] sample reason=' in line and 'pos=(' in line:
            samples.append(dict(part.strip().split('=',1) for part in line.split('|') if '=' in part))
    def counts(key):
        return dict(Counter(s.get(key,'not_recorded') for s in samples))
    heights=[]
    for s in samples:
        try:
            heights.append(float(s['camera'].strip('()').split(',')[2])
                           -float(s['pos'].strip('()').split(',')[2]))
        except (KeyError, ValueError, IndexError):
            pass
    contacts=[line for line in lines if '[KT] contact_change ' in line]
    observers=[line for line in lines if '[KT] actor_observer ' in line]
    trials=[line for line in lines if '[KT] speed_trial ' in line]
    gaps=[]
    for before, after in zip(samples,samples[1:]):
        if before.get('generation')!=after.get('generation'):continue
        try:
            sim=float(after['time'])-float(before['time'])
            wall=float(after['async'])-float(before['async'])
        except (KeyError,ValueError):continue
        if wall-sim>1: gaps.append({'simulation_seconds':round(sim,4),'async_seconds':round(wall,4)})
    return {
        'source': path.name,
        'versions': sorted(set(re.findall(r'\[KT\] boot version=([^ ]+)', '\n'.join(lines)))),
        'player_samples':len(samples), 'generations':counts('generation'),
        'ai_stance_raw':counts('ai_stance_raw'), 'xgen_crouch_raw':counts('xgen_crouch_raw'),
        'animations':counts('anim'),
        'camera_height_range_m': [round(min(heights),4),round(max(heights),4)] if heights else None,
        'contact_events':len(contacts),
        'contact_raw_values':dict(Counter(re.findall(r'flying_raw=(\w+)', '\n'.join(contacts)))),
        'contact_unsuccessful_events':sum('status=ok' not in line for line in contacts),
        'actor_observer_events':len(observers),
        'actor_observer_crouch_values':dict(Counter(re.findall(r'crouch=(\w+)', '\n'.join(observers)))),
        'speed_trial_samples':sum(' sample phase=' in line for line in trials),
        'speed_trial_events':[line.split('[KT] ',1)[1] for line in trials if ' sample phase=' not in line],
        'watch_complete':sum('[KT] watch_complete' in line for line in lines),
        'async_simulation_gaps_over_1s':gaps,
        'interpretation':'Raw observations only. Timing gaps do not identify pause causes; camera height is not a crouch gate. Speed-trial logs do not alone prove wall collision, subjective feel or full swoop acceptance.'
    }


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    result=json.dumps(analyze(args.log),indent=2)
    if args.out:
        args.out.parent.mkdir(parents=True,exist_ok=True)
        args.out.write_text(result+'\n',encoding='utf8')
    print(result)
