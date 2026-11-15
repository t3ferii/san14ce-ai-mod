"""Summarize xperf sampled-profile CSV for one game PID; no live access."""
import argparse, csv, json
from collections import Counter, defaultdict
from pathlib import Path

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('csv',type=Path);parser.add_argument('--pid',type=int,required=True)
    parser.add_argument('--base',type=lambda s:int(s,0),required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--lost-events',type=int,default=None)
    args=parser.parse_args()
    modules=Counter();leaves=Counter();threads=Counter();frames=Counter();bins=defaultdict(Counter)
    bin_modules=defaultdict(Counter)
    total=0;stacks=0;current=None;stack_seen=set()
    def label(address,module):
        if args.base<=address<args.base+0x29ff000:return 'SAN14CE_KO.exe+'+hex(address-args.base)
        return module
    with args.csv.open(encoding='utf-8-sig',errors='replace',newline='') as file:
        for row in csv.reader(file):
            if not row:continue
            kind=row[0].strip()
            if kind=='SampledProfile':
                current=None;stack_seen=set()
                if len(row)<10 or not row[1].strip().isdigit() or '('+str(args.pid)+')' not in row[2]:continue
                at=int(row[1]);tid=int(row[3]);count=int(row[8]);address=int(row[4],16)
                leaf=row[7].strip();module=leaf.split('!')[0]
                total+=count;modules[module]+=count;leaves[label(address,leaf)]+=count
                threads[tid]+=count;bins[at//5000000][tid]+=count;bin_modules[at//5000000][module]+=count
                current=(at,tid,count)
            elif kind=='Stack' and current and len(row)>=6:
                if not row[1].strip().isdigit():continue
                at,tid,count=current
                if int(row[1])!=at or int(row[2])!=tid:continue
                name=label(int(row[4],16),row[5].strip())
                if not stack_seen:stacks+=1
                if name not in stack_seen:frames[name]+=count;stack_seen.add(name)
    def top(counter,n):return [{'name':str(name),'retained_count':count} for name,count in counter.most_common(n)]
    result={'status':'retained_sampled_cpu_summarized','pid':args.pid,'game_base':hex(args.base),
        'retained_cpu_sample_count':total,'sample_records_with_matched_stack':stacks,
        'events_lost_in_source_trace':args.lost_events,'quantitative_percentages_reliable':args.lost_events==0,
        'exact_button_boundaries_known':False,'top_leaf_modules':top(modules,12),
        'module_basename_may_merge_distinct_loaded_images':True,
        'top_game_leaf_rvas':top(Counter({k:v for k,v in leaves.items() if k.startswith('SAN14CE_KO')}),25),
        'top_threads':top(threads,12),'top_game_stack_rvas':top(Counter({k:v for k,v in frames.items() if k.startswith('SAN14CE_KO')}),30),
        'five_second_threads':[{'start_seconds':key*5,'threads':top(value,3),'leaf_modules':top(bin_modules[key],5)} for key,value in sorted(bins.items())]}
    args.out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k not in {'five_second_threads','top_game_stack_rvas'}},indent=2))

if __name__=='__main__':main()
