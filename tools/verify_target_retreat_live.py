"""Interpret the explicitly enabled v11.4 log segment; never touches the game."""
import json
from pathlib import Path
from datetime import datetime,timezone
from package_input import PROJECT,GAME,digest
from collect_runtime_diagnostics import LINE

def load(path):return json.loads(path.read_text(encoding='utf-8'))
def main():
    proof=load(PROJECT/'build/recruitment_ui_candidate.json')
    request=load(PROJECT/'build/runtime_diagnostics.json')
    collection=load(PROJECT/'build/runtime_diagnostics_collection.json')
    assert request['enabled'] and proof['release_version']=='v11.4'
    assert request['dll_sha256']==proof['candidate_sha256']==digest(GAME/'dinput8.dll')
    assert collection['source_start']==request['log_start_offset']
    segment=Path(collection['segment']).resolve()
    assert segment.is_relative_to((PROJECT/'research/retreat_diagnostics').resolve())
    assert digest(segment)==collection['segment_sha256']
    events=[];installed=False;rejected=False
    with segment.open(encoding='utf-8',errors='replace') as source:
        for line in source:
            match=LINE.match(line.strip())
            if not match:continue
            time,kind,army,value,where=match.groups()
            if not kind.startswith('target_retreat_'):continue
            event={'time':time,'kind':kind,'army':int(army),'value':int(value),'hex':int(where)};events.append(event)
            if kind=='target_retreat_hooks_installed' and event['value']==3 and event['hex']==6:installed=True
            if kind.endswith(('_rejected','_disabled')):rejected=True
    kept=[row for row in events if row['kind']=='target_retreat_nearby_attack_kept']
    other=[row for row in events if row['kind']=='target_retreat_other_native_decision']
    # Army slots change after loading a different save. Identify the six
    # commanders in the latest compatible snapshot, never assume old IDs.
    focus=set();focus_mapping={};orders_path=None
    marker=PROJECT/'build/retreat_orders_snapshot.json'
    if marker.exists():
        orders_path=Path(load(marker)['snapshot']).resolve()
        assert orders_path.is_relative_to((PROJECT/'research/retreat_diagnostics').resolve())
        orders=load(orders_path)
        if orders['dll_sha256']==proof['candidate_sha256']:
            names={35:'원소',186:'안량',812:'문추',607:'장각',659:'장보',664:'장량'}
            for row in orders['armies']:
                if row['commander_id'] in names:
                    focus.add(row['id']);focus_mapping[row['id']]=names[row['commander_id']]
    result={'status':'target_retreat_runtime_log_checked','checked_at':datetime.now(timezone.utc).isoformat(),
        'dll_sha256':proof['candidate_sha256'],'segment':str(segment),'segment_sha256':collection['segment_sha256'],
        'startup_verified':installed and not rejected and collection['startup_records_verified'],
        'target_hook_rejected_or_disabled':rejected,'exception_records':collection['exception_records'],
        'nearby_attack_kept_events':len(kept),'other_native_decision_events':len(other),
        'focus_army_ids_at_snapshot':focus_mapping,'orders_snapshot':str(orders_path) if orders_path else None,
        'focused_kept_events':[row for row in kept if row['army'] in focus],
        'last_target_events':events[-100:],'gameplay_verified':False,
        'limits':'Hook startup and scoped decisions are log observations; repeated Nanpi/Pingyuan gameplay and all order changes need separate confirmation. Focus IDs come from the latest snapshot, not a full history of reused army slots.'}
    (PROJECT/'build/target_retreat_live_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({key:result[key] for key in ['status','startup_verified','target_hook_rejected_or_disabled','exception_records','nearby_attack_kept_events','other_native_decision_events','focused_kept_events']},ensure_ascii=False))
if __name__=='__main__':main()
