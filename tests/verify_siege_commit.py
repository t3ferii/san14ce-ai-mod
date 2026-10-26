"""Bind siege ownership fixtures to the exact no-log production DLLs."""
from pathlib import Path
import hashlib,json
P=Path(__file__).resolve().parents[1]
read=lambda n:(P/n).read_text(encoding='utf-8-sig')
checks=0
def check(value,label):
    global checks
    assert value,label
    checks+=1
source=read('src/native_campaign_assault.inl')
for text in ('builds>2','motion_day<20','campaign_assault_hostile','source_city','city_attack?5','building && near_city','p.morale_saved=false','p.operation_leader=0'):
    check(text in source,text)
check('bool created=sortie_departure(node);' in read('src/native_sortie.inl'),'weak target must attempt cohort')
check('if(campaign::siege_departure(campaign::available' in read('src/native_campaign_departures.inl'),'proactive weak target must attempt cohort')
for n in ('native_adapter.cpp','native_campaign_routes.inl','native_campaign_support.inl','native_facilities.inl','native_sortie.inl'):
    check('campaign_castle_locked(army)' in read('src/'+n),n+' command writer guard')
newest=max(q.stat().st_mtime for q in (P/'src').rglob('*') if q.suffix in ('.cpp','.c','.h','.inl','.asm'))
for n,minimum in (('native_fixture',1543),('sortie_fixture',1935)):
    r=json.loads(read('build/'+n+'.json'))
    check('passed' in r['status'] and r['checks']>=minimum and not r['failures'],n)
    check((P/'build'/f'{n}.json').stat().st_mtime>=newest,n+' freshness')
binary=P/'build/development/loader_split'
files={n:hashlib.sha256((binary/n).read_bytes()).hexdigest() for n in ('dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll')}
for n in files:check((binary/n).stat().st_mtime>=newest,n+' freshness')
log=json.loads(read('build/session_release_binary_fixture.json'))
check(log['files']==files and log['runtime_log_writers_compiled_out'] and log['function_timing_writer_compiled_out'],'production no-log proofs')
report=dict(status='siege_commit_fixture_passed',checks=checks,files=files,castle_only_order_ownership=True,
            cohort_build_limit=2,cohort_stall_days=20,weak_target_bypass_removed=True,gameplay_verified=False)
(P/'build/siege_commit_fixture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
