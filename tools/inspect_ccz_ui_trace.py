"""Read the latest call-wrapper session from the existing common game log only."""
from pathlib import Path
import argparse,json,re
P=Path(__file__).resolve().parents[1]

def inspect(path):
    records=[];calls={};data={};ended=False
    pattern=re.compile(r'thread=(\d+) (ccz_ui_\w+) army=(\d+) value=(-?\d+) hex=(-?\d+)')
    for line in path.read_text(encoding='utf-8',errors='replace').splitlines():
        match=pattern.search(line)
        if not match:continue
        thread,event,a,b,c=match.groups();a,b,c=map(int,(a,b,c))
        if event=='ccz_ui_trace_start':records=[];calls={};data={};ended=False
        records.append((event,a,b,c))
        if event=='ccz_ui_enter':calls[a]=dict(id=a,stage=b,rva=hex(c),thread=int(thread),returned=False,args={})
        elif event=='ccz_ui_trace_end':ended=True
        elif event=='ccz_ui_data':data.setdefault(str(a),{})[str(b)]=hex(c&0xffffffff)
        elif a in calls:
            call=calls[a]
            if event=='ccz_ui_exit':call['returned']=True
            elif event in ('ccz_ui_arg','ccz_ui_arg_hi'):
                arg=call['args'].setdefault(str(b),[0,0]);arg[event.endswith('_hi')]=c&0xffffffff
            elif event in ('ccz_ui_target','ccz_ui_result','ccz_ui_fault_ip','ccz_ui_fault_address','ccz_ui_fault_rcx','ccz_ui_fault_rdx','ccz_ui_fault_r8','ccz_ui_fault_r9'):
                call[event.removeprefix('ccz_ui_')]=hex((b&0xffffffff)|((c&0xffffffff)<<32))
            elif event in ('ccz_ui_us','ccz_ui_cycles'):call[event.removeprefix('ccz_ui_')]=c
            elif event in ('ccz_ui_cpp_throw','ccz_ui_fault'):call[event.removeprefix('ccz_ui_')]=hex(c)
    for call in calls.values():
        call['args']={i:hex(lo|(hi<<32)) for i,(lo,hi) in call['args'].items()}
    pending=[call for call in calls.values() if not call['returned']]
    return dict(status='trace_found' if calls else 'no_wrapper_trace',closed=ended,
        entered=len(calls),returned=sum(c['returned'] for c in calls.values()),
        last_unreturned=pending[-1] if pending else None,unreturned=pending,
        last_calls=list(calls.values())[-12:],snapshots=data)

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('log',nargs='?',type=Path,default=P.parent/'SAN14_AITactics.log');args=ap.parse_args()
    print(json.dumps(inspect(args.log),ensure_ascii=False,indent=2))
