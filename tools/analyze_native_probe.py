"""Summarize a bounded native movement capture without inferring gameplay gates."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path


def summarize(path):
    rows=[]
    with Path(path).open(encoding='utf-8') as source:
        for line in source:
            if len(rows)>=12003 or len(line)>4096:
                raise ValueError('Capture exceeds the probe bounds')
            row=json.loads(line)
            if not isinstance(row,dict): raise ValueError('Expected JSON objects')
            rows.append(row)
    starts=[r for r in rows if r.get('event')=='started']
    stops=[r for r in rows if r.get('event')=='stopped']
    samples=[r for r in rows if r.get('event')=='player_request']
    if len(starts)!=1 or starts[0].get('mode')!='observe-only':
        raise ValueError('Expected one observation-only capture header')
    frequency=starts[0]['qpc_frequency']
    if not isinstance(frequency,int) or frequency<=0: raise ValueError('Invalid clock frequency')
    flags=Counter();threads=set();speeds=[];deltas=[];times=[]
    for row in samples:
        flags[hex(row['flags'])]+=1; threads.add(row['thread']); times.append(row['qpc'])
        for key,target,width in [('speed',speeds,2),('delta',deltas,3)]:
            value=row[key]
            if value is not None:
                if len(value)!=width or not all(isinstance(v,(float,int)) and math.isfinite(v) for v in value):
                    raise ValueError('Invalid movement scalars')
                target.append(value)
    return {'complete_shutdown':len(stops)==1,'samples':len(samples),'observed_thread_count':len(threads),
            'capture_seconds':(max(times)-min(times))/frequency if times else 0,
            'flag_counts':dict(flags),'speed_pair_samples':len(speeds),'delta_samples':len(deltas),
            'nonzero_delta_samples':sum(any(abs(v)>1e-6 for v in d) for d in deltas),
            'desired_speed_range':[min(s[0] for s in speeds),max(s[0] for s in speeds)] if speeds else None,
            'shutdown_counters':stops[0] if len(stops)==1 else None,
            'limitation':'Observed request scalars only; no stance/contact/collision or playable-swoop proof.'}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture',type=Path)
    print(json.dumps(summarize(parser.parse_args().capture),indent=2))
