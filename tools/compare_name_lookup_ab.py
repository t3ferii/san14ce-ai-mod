"""Compare two WPR sampled-CPU runs (name_lookup enabled=0 vs 1); no live access.

Input: `xperf -i run.etl -o run.csv -a dumper` CSV per run,
the game PID and load base (thread start SAN14CE_KO.exe!X = base+0x1031448).
Metric: samples whose stack contains the caller 8A8B80 (covers both the
original E09910 and the replacement in SAN14_Convenience.dll; E09910 has other
callers too, so original_E09910 must drop to 0 in B), grouped into
turn-boundary bursts (seconds with such samples, gaps <= --gap seconds).
"""
import argparse, csv, json
from collections import Counter
from pathlib import Path

RANGES={'caller_8A8B80':(0x8A8B80,0x8A8CDD),'original_E09910':(0xE09910,0xE099DE),'strncmp_102FC20':(0x102FC20,0x102FC9D)}

def summarize(path,pid,base,gap,min_samples):
    per_second=Counter();hits=Counter();replacement=0;total=0;current=None;seen=set()
    def account():
        nonlocal replacement
        if not current:return
        for name in seen:hits[name]+=current[1]
        if 'caller_8A8B80' in seen:
            per_second[current[0]//1000000]+=current[1]
            if current[2]:replacement+=current[1]
    with path.open(encoding='utf-8-sig',errors='replace',newline='') as file:
        for row in csv.reader(file):
            if not row:continue
            kind=row[0].strip()
            if kind=='SampledProfile':
                account();current=None;seen=set()
                if len(row)<10 or not row[1].strip().isdigit() or row[2].rsplit('(',1)[-1].strip(' )')!=str(pid):continue
                count=int(row[8]) if row[8].strip().isdigit() else 1
                total+=count;at=int(row[1]);address=int(row[4],16)
                # [time, count, leaf in the replacement DLL, thread]
                current=[at,count,'SAN14_Convenience' in row[7],int(row[3])]
                frames=[address]
            elif kind=='Stack' and current and len(row)>=6 and row[1].strip().isdigit():
                if int(row[1])!=current[0] or int(row[2])!=current[3]:continue
                frames=[int(row[4],16)]
            else:continue
            for address in frames:
                rva=address-base
                for name,(start,end) in RANGES.items():
                    if start<=rva<end:seen.add(name)
    account()
    bursts=[];start=None;last=None;samples=0
    for second in sorted(per_second):
        if start is not None and second-last>gap:
            bursts.append({'start_s':start,'seconds':last-start+1,'caller_samples':samples});start=None
        if start is None:start=second;samples=0
        last=second;samples+=per_second[second]
    if start is not None:bursts.append({'start_s':start,'seconds':last-start+1,'caller_samples':samples})
    # Stray single samples are not turn boundaries.
    bursts=[b for b in bursts if b['caller_samples']>=min_samples]
    return {'csv':str(path),'pid':pid,'process_samples':total,'stack_samples':dict(hits),
            'caller_leaf_in_convenience_dll':replacement,'bursts':bursts,
            'mean_burst_seconds':sum(b['seconds'] for b in bursts)/len(bursts) if bursts else None,
            'mean_caller_samples_per_burst':sum(b['caller_samples'] for b in bursts)/len(bursts) if bursts else None}

def main():
    p=argparse.ArgumentParser()
    for side in 'ab':
        p.add_argument(f'--{side}-csv',type=Path,required=True);p.add_argument(f'--{side}-pid',type=int,required=True)
        p.add_argument(f'--{side}-base',type=lambda s:int(s,0),required=True)
    p.add_argument('--gap',type=int,default=2);p.add_argument('--min-burst-samples',type=int,default=100);p.add_argument('--out',type=Path,required=True)
    args=p.parse_args()
    a=summarize(args.a_csv,args.a_pid,args.a_base,args.gap,args.min_burst_samples)
    b=summarize(args.b_csv,args.b_pid,args.b_base,args.gap,args.min_burst_samples)
    ratio=(b['mean_caller_samples_per_burst']/a['mean_caller_samples_per_burst']
           if a['mean_caller_samples_per_burst'] and b['mean_caller_samples_per_burst'] else None)
    result={'a_original':a,'b_replacement':b,'caller_samples_per_burst_ratio_b_over_a':ratio,
            'note':'CPU samples only; wall-clock turn time needs the same save, same turns and a stopwatch/click log.'}
    args.out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('caller_samples_per_burst_ratio_b_over_a',)}|
                     {s:{k:v for k,v in result[s].items() if k!='bursts'} for s in ('a_original','b_replacement')},indent=2))

if __name__=='__main__':main()
