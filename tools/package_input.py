"""Automatic native input loader. Never modifies the game EXE or _files.
Existing dinput8 providers are retained and called through a named chain.
"""
from pathlib import Path
import argparse,json,sys
from package_mod import sha,atomic_write,no_game,ORIGINAL_SHA
PROJECT=Path(__file__).resolve().parents[1];GAME=PROJECT.parent
PACKAGE=PROJECT/'build/input_package';STATE=PROJECT/'backup/input_state.json'
TARGET=GAME/'dinput8.dll';CHAIN=GAME/'SAN14_AI_PreviousInput.dll'
def digest(path):return sha(path.read_bytes()) if path.exists() else None
def main():
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['build','install','update','restore','verify']);action=parser.parse_args().action
    if action=='build':
        dll=(PROJECT/'build/SAN14_AIInput.dll').read_bytes();PACKAGE.mkdir(exist_ok=True)
        (PACKAGE/'dinput8.dll').write_bytes(dll)
        report={'loader':'original_exe_input_proxy','dll_sha256':sha(dll),'supported_original_exe_sha256':ORIGINAL_SHA,
                'exe_modified':False,'adjacency':'actual_combat_pair_ai_vs_ai_fixed_4_percent_native_ui','gameplay_verified':False,
                'behavior':'campaign_v7_domestic','drifting_turns':6,'continuous_building':True,'observer_support':True,
                'idle_recovery':{'depleted_return_below':3000,'healthy_automatic_return':False,'join_without_wait':True},
                'regional_tasks_before_siege':True,'cooperate_on_reserved_region':True,
                'exact_region_waypoint_and_next_step_required':True,
                'neutral_regions':True,'build_skip_audit':True,'assembly_enabled':False,
                'computer_wall_building':'blocked','blocked_facility_ids':[9,10],
                'native_build_permission_entry_rva':'0x279190',
                'existing_ai_wall_orders':'cancel_on_ai_callback',
                'human_wall_building':'native','wall_demolition':'unchanged',
                'retreat_logic':'unchanged_from_v5',
                'automatic_domestic':{'officer_assignment_cost':0,'manual_assignment_cost':'native',
                                     'selection':'lowest_level_then_commerce_agriculture_barracks',
                                     'skip_level_max':5,'cycle_existing_officers':True,
                                     'manual_development_when_auto_off':'native',
                                     'forecast_mutation':False,'scope':'current_auto_on_or_delegated_or_computer_district'},
                'domestic_links':{'officer_vtable':'0x1346E70','selection_vtable':'0x134C8F8',
                                  'tick_entry':'0x295160','automatic_budget_guard':'0x36423',
                                  'thread_scoped_cost_load':'0x1ED475'}}
        (PACKAGE/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return
    manifest=json.loads((PACKAGE/'manifest.json').read_text())
    if action=='verify':
        assert digest(GAME/'SAN14CE_KO.exe')==ORIGINAL_SHA,'Game EXE is not the supported original'
        state=json.loads(STATE.read_text())
        assert state['installed'] and digest(TARGET)==state['installed_sha256'],'Installed input loader differs from recorded installation'
        assert digest(CHAIN)==state['previous_sha256'],'Chained provider differs'
        print('Original EXE unchanged; installed input loader and preserved provider verified.')
        if state['installed_sha256']!=manifest['dll_sha256']:print('A newer candidate is ready; close the game and use update.');return
        return
    no_game()
    if action=='update':
        if digest(GAME/'SAN14CE_KO.exe')!=ORIGINAL_SHA:raise RuntimeError('Supported original EXE required.')
        state=json.loads(STATE.read_text())
        if not state['installed'] or digest(TARGET)!=state['installed_sha256']:raise RuntimeError('Recorded AI loader must be installed; refusing replacement.')
        if digest(CHAIN)!=state['previous_sha256']:raise RuntimeError('Preserved input provider changed.')
        new=(PACKAGE/'dinput8.dll').read_bytes();assert sha(new)==manifest['dll_sha256']
        if sha(new)==state['installed_sha256']:print('Already up to date.');return
        old=TARGET.read_bytes();newstate={**state,'installed_sha256':sha(new)}
        try:
            no_game();atomic_write(TARGET,new)
            atomic_write(STATE,(json.dumps(newstate,indent=2)+'\n').encode())
        except BaseException:
            if digest(TARGET)==sha(new):atomic_write(TARGET,old)
            raise
        print('Updated AI input loader; original EXE and preserved provider unchanged.');return
    if action=='install':
        if digest(GAME/'SAN14CE_KO.exe')!=ORIGINAL_SHA:raise RuntimeError('Restore the supported original EXE first.')
        new=(PACKAGE/'dinput8.dll').read_bytes();assert sha(new)==manifest['dll_sha256']
        current=digest(TARGET)
        if current==manifest['dll_sha256']:
            state=json.loads(STATE.read_text());assert digest(CHAIN)==state['previous_sha256'];print('Already installed.');return
        if STATE.exists() and json.loads(STATE.read_text())['installed']:
            raise RuntimeError('AI input loader already installed; use update. Never chain an old AI loader into itself.')
        if CHAIN.exists():raise RuntimeError('Existing chain filename; refusing overwrite.')
        old=TARGET.read_bytes() if TARGET.exists() else None
        STATE.parent.mkdir(exist_ok=True)
        original_backup=PROJECT/'backup/dinput8.previous.dll'
        if old is not None:
            if original_backup.exists() and digest(original_backup)!=current:raise RuntimeError('Different original input backup exists.')
            if not original_backup.exists():original_backup.write_bytes(old)
        state={'installed_sha256':sha(new),'previous_sha256':current,'exe_unchanged':True,'installed':True}
        try:
            if old is not None:atomic_write(CHAIN,old)
            no_game();atomic_write(TARGET,new)
            atomic_write(STATE,(json.dumps(state,indent=2)+'\n').encode())
        except BaseException:
            if digest(TARGET)==sha(new):
                if old is None:TARGET.unlink()
                else:atomic_write(TARGET,old)
            if old is not None and digest(CHAIN)==current:CHAIN.unlink()
            raise
        assert digest(GAME/'SAN14CE_KO.exe')==ORIGINAL_SHA
        print('Installed automatic input loader; game EXE and _files unchanged.');return
    state=json.loads(STATE.read_text());oldhash=state['previous_sha256'];current=digest(TARGET)
    if not state['installed']:
        if current!=oldhash:raise RuntimeError('Input provider changed since removal.')
        print('Already removed.');return
    if current!=state['installed_sha256']:raise RuntimeError('Input loader changed; refusing overwrite/removal.')
    if digest(CHAIN)!=oldhash:raise RuntimeError('Original chained provider changed.')
    if oldhash is None:TARGET.unlink()
    else:
        old=CHAIN.read_bytes();assert sha(old)==oldhash;atomic_write(TARGET,old);CHAIN.unlink()
    state['installed']=False;atomic_write(STATE,(json.dumps(state,indent=2)+'\n').encode())
    print('Removed AI input loader; original input configuration restored. Game EXE unchanged.')
if __name__=='__main__':main()
