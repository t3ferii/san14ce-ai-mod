"""Read only this explicitly enabled session's new log bytes; preserve original log."""
import collections,hashlib,json,re
from datetime import datetime,timezone
from pathlib import Path
from package_input import PROJECT,GAME,digest
from package_mod import atomic_write

LINE=re.compile(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d) (?:day=-?\d+ thread=\d+ )?(\w+) army=(\d+) value=(\d+) hex=(\d+)')
def main():
    session_path=PROJECT/'build/runtime_diagnostics.json';session=json.loads(session_path.read_text(encoding='utf-8'))
    assert session['enabled'] and session['dll_sha256']==digest(GAME/'dinput8.dll')
    log=Path(session['log']).resolve();assert log==GAME/'SAN14_AITactics.log'
    start=session['log_start_offset'];end=log.stat().st_size;assert end>=start,'Original log truncated; session offset invalid'
    if end==start:
        print(json.dumps({'status':'waiting_for_new_runtime_records','new_bytes':0}));return
    folder=PROJECT/'research/retreat_diagnostics'/datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f')
    folder.mkdir(parents=True,exist_ok=False);destination=folder/'session.log';sha=hashlib.sha256()
    with log.open('rb') as source,destination.open('xb') as output:
        source.seek(start);remaining=end-start
        while remaining:
            part=source.read(min(remaining,1048576));assert part,'Log shortened while collecting'
            output.write(part);sha.update(part);remaining-=len(part)
    counts=collections.Counter();armies=collections.Counter();relevant=collections.deque(maxlen=200);startup=False;timing=False;faults=0
    with destination.open('r',encoding='utf-8',errors='replace') as source:
        for text in source:
            match=LINE.match(text.strip())
            if not match:
                if 'native_exception_first_chance' in text:faults+=1
                continue
            time,kind,army,value,hex_id=match.groups();counts[kind]+=1
            if kind=='merit_recruitment_ui_candidate_hooks_installed':startup=True
            if kind.startswith('progress_') and kind.endswith('_microseconds'):timing=True
            if any(key in kind for key in ('retreat','return','sortie','campaign_','morale','facility_','build_','cut_')):
                item={'time':time,'kind':kind,'army':int(army),'value':int(value),'hex':int(hex_id)}
                relevant.append(item);armies[int(army)]+=1
    result={'status':'new_runtime_log_segment_collected','source_start':start,'source_end':end,'new_bytes':end-start,
        'segment':str(destination),'segment_sha256':sha.hexdigest(),'event_counts':dict(counts),'first_field_counts':dict(armies),
        'field_semantics':'Event-specific: cohesive_sortie_departed army=member count, value=soldiers, hex=target city ID. Not every army field denotes an individual unit.',
        'startup_records_verified':startup,'progress_timing_records_present':timing,'exception_records':faults,
        'last_relevant_events':list(relevant),'limits':'Existing mod events do not record every native target/order transition; this alone cannot prove native retreat cause.'}
    data=(json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode();atomic_write(folder/'summary.json',data)
    atomic_write(PROJECT/'build/runtime_diagnostics_collection.json',data)
    session['startup_verified']=startup;session['latest_collection']=str(folder/'summary.json')
    atomic_write(session_path,(json.dumps(session,ensure_ascii=False,indent=2)+'\n').encode())
    print(json.dumps({key:result[key] for key in ['status','new_bytes','segment','startup_records_verified','progress_timing_records_present','event_counts','exception_records']},ensure_ascii=False))
if __name__=='__main__':main()
