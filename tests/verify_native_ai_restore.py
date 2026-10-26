"""Audit production removal, paired with the compiled behavioral fixtures."""
from pathlib import Path
import hashlib, json, re

P=Path(__file__).resolve().parents[1]
read=lambda n:(P/n).read_text(encoding='utf-8-sig')
checks=0
def check(value,label):
    global checks
    assert value,label
    checks+=1

campaign=read('src/native_campaign.inl')
operations=read('src/native_campaign_operations.inl')
adapter=read('src/native_adapter.cpp')
sortie=read('src/native_sortie.inl')
split=read('src/native_split_install.inl')
check('campaign_stationary_days=40;' in campaign,'forty-day deadline')
check(not re.search(r'motion_day\s*=',campaign+operations+sortie),'activity cannot restart stationary timer')
check(len(re.findall(r'motion_day\s*=',adapter))==2,'only identity and movement reset timer')
for name in ('campaign_local_interceptor','campaign_engaged'):
    check(name not in campaign,name+' removed')
check('campaign_operation_enemy' not in operations,'custom operation interception removed')
check('install_target_retreat(' not in adapter+split,'original daily and candidate filters unpatched')
check('#ifdef TARGET_RETREAT_FIXTURE\n#include "native_target_retreat.inl"' in adapter,'old policy is fixture only')
check(not re.search(r'\[0x1B\]\s*(?:==|!=)\s*7',campaign+operations+sortie),'no formation-seven classifier')
for n,minimum in (('native_fixture',1527),('sortie_fixture',1916)):
    result=json.loads(read('build/'+n+'.json'))
    check('passed' in result['status'] and result['checks']>=minimum and not result['failures'],n)
    check((P/'build'/f'{n}.json').stat().st_mtime>=max((P/'src'/name).stat().st_mtime for name in
          ('native_campaign.inl','native_campaign_operations.inl','native_adapter.cpp','native_sortie.inl','sortie_policy.h','native_split_install.inl')),n+' freshness')
files={}
for name in ('dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll'):
    data=(P/'build/development/loader_split'/name).read_bytes()
    files[name]=hashlib.sha256(data).hexdigest()
    for marker in ('target_retreat.ini','campaign_local_interceptor','campaign_operation_enemy'):
        check(marker.encode() not in data and marker.encode('utf-16le') not in data,name+' '+marker)
log=json.loads(read('build/session_release_binary_fixture.json'))
check(files==log['files'] and log['runtime_log_writers_compiled_out'] and log['function_timing_writer_compiled_out'],'exact production DLLs without log writers')
report=dict(status='native_ai_restore_fixture_passed',checks=checks,files=files,
            original_interception_filters_restored=True,formation_7_restrictions_removed=True,
            stationary_days=40,activity_exemptions=False,gameplay_verified=False)
(P/'build/native_ai_restore_fixture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
