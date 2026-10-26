"""Verify the final user-defined lock scope and exact no-log DLLs."""
from pathlib import Path
import hashlib,json
P=Path(__file__).resolve().parents[1]
read=lambda n:(P/n).read_text(encoding='utf-8-sig')
checks=0
def check(value,label):
    global checks
    assert value,label
    checks+=1
source=read('src/native_campaign_assault.inl');adapter=read('src/native_adapter.cpp')
for token in ('motion_day<30','g.force_march=true','o.force_march=true','else group_id=0','campaign_commit_castle(army,p,locked,1)'):
    check(token in source,token)
for token in ('completed_builds','count_completed_build','builds>2','city_attack?5','building && near_city'):
    check(token not in source+adapter,token+' removed')
check('p.forced_siege_reason==1 || (p.forced_siege_reason==3 && !campaign_group_march_depth)' in adapter,'two lock reasons and authorized group writer')
for n in ('native_sortie.inl','native_campaign_operations.inl'):
    check('++campaign_group_march_depth' in read('src/'+n) and '--campaign_group_march_depth' in read('src/'+n),n+' scope cleanup')
newest=max(q.stat().st_mtime for q in (P/'src').rglob('*') if q.suffix in ('.cpp','.c','.h','.inl','.asm'))
for n,minimum in (('native_fixture',1545),('sortie_fixture',1944)):
    r=json.loads(read('build/'+n+'.json'))
    check('passed' in r['status'] and r['checks']>=minimum and not r['failures'],n)
    check((P/'build'/f'{n}.json').stat().st_mtime>=newest,n+' freshness')
binary=P/'build/development/loader_split'
files={n:hashlib.sha256((binary/n).read_bytes()).hexdigest() for n in ('dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll')}
for n in files:check((binary/n).stat().st_mtime>=newest,n+' freshness')
log=json.loads(read('build/session_release_binary_fixture.json'))
check(files==log['files'] and log['runtime_log_writers_compiled_out'] and log['function_timing_writer_compiled_out'],'production no-log proofs')
report=dict(status='siege_policy_fixture_passed',checks=checks,files=files,stationary_group_days=30,
    lock_reasons=['actual_castle_damage','thirty_day_group_march'],construction_count_limit_removed=True,
    near_castle_attack_precedence_removed=True,gameplay_verified=False)
(P/'build/siege_policy_fixture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
