"""Stage the integrated small-army release against the installed v11 baseline."""
import json
from datetime import datetime, timezone, timedelta
from package_mod import atomic_write, ORIGINAL_SHA
from package_input import PROJECT, GAME, PACKAGE, STATE, TARGET, digest

BASELINE = '6447808ecc2eb8edad82d552b895a59df19ac2d83198ff9a448a82db0e23151d'
HISTORY = PROJECT/'build/recruitment_ui_before_small_army_20261012.json'
ROUTES = [
    ('native_fixture','native_fixture','native_adapter_fixture_passed',1504),
    ('sortie_fixture','sortie_fixture','sortie_fixture_passed',1840),
    ('merit_fixture','merit_native_fixture','merit_native_fixture_passed',6381),
    ('fixture','recruitment_ui_fixture','recruitment_ui_fixture_passed',1284),
    ('personnel_fixture','personnel_native_fixture','personnel_native_fixture_passed',1030),
    ('awards_fixture','awards_native_fixture','awards_native_fixture_passed',438),
]

def load(path): return json.loads(path.read_text(encoding='utf-8'))

def main():
    proof=PROJECT/'build/recruitment_ui_candidate.json'
    old=HISTORY.read_bytes() if HISTORY.exists() else proof.read_bytes()
    evidence=json.loads(old)
    state=load(STATE);manifest=load(PACKAGE/'manifest.json')
    if not state['installed'] or not BASELINE==digest(TARGET)==state['installed_sha256']==digest(PACKAGE/'dinput8.dll')==manifest['dll_sha256']==evidence['candidate_sha256']:
        raise RuntimeError('Installed v11 baseline changed')
    if digest(GAME/'SAN14CE_KO.exe')!=ORIGINAL_SHA:raise RuntimeError('Supported original EXE required')
    changed={'src/native_adapter.cpp','src/native_campaign.inl','src/native_campaign_reinforcements.inl',
             'src/native_campaign_support.inl','src/native_sortie.inl','src/sortie_policy.h'}
    for group in ['obstacle_source_sha256','recruitment_source_sha256']:
        for name,expected in evidence[group].items():
            if name.startswith('src/') and name not in changed and digest(PROJECT/name)!=expected:
                raise RuntimeError('Unrelated runtime source changed: '+name)
    runtime={p.relative_to(PROJECT).as_posix() for p in (PROJECT/'src').iterdir() if p.suffix in {'.cpp','.c','.h','.inl'}}
    tests={'tests/native_fixture.cpp','tests/campaign_operation_fixture.inl','tests/campaign_reinforcement_fixture.inl',
           'tests/campaign_support_fixture.inl','tests/sortie_fixture.inl'}
    latest=max((PROJECT/name).stat().st_mtime for name in runtime|tests)
    dll=PROJECT/'build/development/SAN14_AIInput_recruitment_ui_candidate.dll'
    if latest>dll.stat().st_mtime:raise RuntimeError('Integrated build predates source')
    results={}
    for key,name,status,count in ROUTES:
        result=load(PROJECT/'build'/f'{name}.json')
        if result['status']!=status or result['checks']<count:raise RuntimeError('Passing checks required: '+key)
        if latest>(PROJECT/'build'/f'{name}.exe').stat().st_mtime:raise RuntimeError('Fixture predates runtime source: '+key)
        results[key]=result
    if not HISTORY.exists():atomic_write(HISTORY,old)
    evidence.update(results)
    evidence['campaign_revision']['fixture_checks']={key:r['checks'] for key,r in results.items()}
    evidence['cohesive_sortie'].update(minimum_mobilizable=None,fixed_40000_floor_removed=True,
        reserve='max(3000,garrison/5,local_enemy*1.5)',fixture_checks=results['sortie_fixture']['checks'],
        save_recovery='eligible native attack groups reobserved; completed field-task credit reobserved')
    evidence['campaign_support_revision'].update(siege_strength_radius=8,reinforcement_min_garrison=3000,
        reinforcement_reserve='max(3000,garrison/5,local_enemy*1.5)',
        target_owned_adjacent_bases_policy={'0_to_2':'prepare_then_assault','3_or_more':'regional_capture'},
        updated_by='v11.1_small_army_campaign')
    evidence['small_army_campaign']={
        'release_version':'v11.1','fixed_40000_floor_removed':True,'max_units':7,
        'reserve_minimum':3000,'reserve_garrison_fraction':[1,5],'reserve_local_enemy_multiplier':[3,2],
        'field_join_completed_or_idle_only':True,'native_groups_merged':False,
        'siege_core_roles':['front','siege','middle_at_least_5000'],
        'defender_radius':8,'siege_core_enemy_ratio':[3,2],
        'target_owned_adjacent_bases_assault_maximum':2,'random_15_percent_assault_removed':True,
        'support_city_rally_required':False,'no_progress_release_days':20,
        'release_restores_original_orders':True,'rally_failure_forces_retreat':False,
        'existing_stationary_retreat_days':50,'new_hooks':False,
        'fixture_checks':{key:r['checks'] for key,r in results.items()},
        'startup_verified':False,'gameplay_verified':False,
    }
    evidence.update(component='v11_1_integrated_small_army_field_operations',release_version='v11.1',
        prepared_at=datetime.now(timezone(timedelta(hours=9))).isoformat(),
        candidate_sha256=digest(dll),installed_input_sha256=BASELINE)
    extras=runtime|tests|{'tools/prepare_small_army.py','tools/install_recruitment_ui_candidate.py',
        'tools/generate_handoff.py','development/features.json','research/campaign/소규모군세_현장합류.md'}
    for group in ['obstacle_source_sha256','recruitment_source_sha256']:
        evidence[group]={name:digest(PROJECT/name) for name in sorted(set(evidence[group])|extras)}
    atomic_write(PROJECT/'build/SAN14_AIInput_recruitment_ui_candidate.dll',dll.read_bytes())
    atomic_write(proof,(json.dumps(evidence,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    print(json.dumps({'candidate_sha256':evidence['candidate_sha256'],'baseline':BASELINE,'installed':False,
        'checks':sum(r['checks'] for r in results.values())},indent=2))

if __name__=='__main__':main()
