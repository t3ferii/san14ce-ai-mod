"""Read-only feature state from the running CE KO process. No game calls/input.

Requires the exact installed diagnostic DLL and its matching linker map.
Writes a timestamped evidence JSON under data/diagnostics, never game memory.
"""
from pathlib import Path
import ctypes as C
from ctypes import wintypes as WT
import argparse,datetime,hashlib,json,re,struct
import army_option_probe as probe

P=Path(__file__).resolve().parents[1]

def inspect(symbol_directory=None):
    base,rd=probe.attach()
    pid=probe.game_pid();snapshot=probe.k.CreateToolhelp32Snapshot(0x18,pid)
    entry=probe.ME();entry.dwSize=C.sizeof(entry);modules={}
    try:
        more=probe.k.Module32FirstW(WT.HANDLE(snapshot),C.byref(entry))
        while more:
            modules[entry.szModule.lower()]=(entry.modBaseAddr,entry.modBaseSize,entry.szExePath)
            more=probe.k.Module32NextW(WT.HANDLE(snapshot),C.byref(entry))
    finally:probe.k.CloseHandle(WT.HANDLE(snapshot))
    dll,size,path=modules['san14_convenience.dll']
    build=Path(symbol_directory) if symbol_directory else P/'build/development/loader_split_diagnostic'
    digest=lambda q:hashlib.sha256(q.read_bytes()).hexdigest()
    assert digest(Path(path))==digest(build/'SAN14_Convenience.dll'),'Installed DLL differs from diagnostic build; do not use this map'
    q=lambda a:struct.unpack('<Q',rd(a,8))[0]
    u=lambda a:struct.unpack('<I',rd(a,4))[0]
    header=rd(dll,4096);disk=Path(path).read_bytes();pe=struct.unpack_from('<I',header,60)[0];dp=struct.unpack_from('<I',disk,60)[0]
    assert header[pe+8:pe+12]==disk[dp+8:dp+12],'Loaded PE timestamp differs from disk'
    symbols={}
    for line in (build/'SAN14_Convenience.map').read_text(encoding='utf-8',errors='replace').splitlines():
        match=re.search(r'\s(\S+)\s+([0-9A-Fa-f]{16})\s',line)
        if match:symbols[match[1]]=int(match[2],16)-0x180000000
    def address(prefix,optional=False):
        found=[v for k,v in symbols.items() if k.startswith('?'+prefix)]
        if not found and optional:return None
        assert len(found)==1,'Ambiguous/missing symbol: '+prefix
        return dll+found[0]
    scalar_names=['enabled@@','diagnostic_logging@@','visible@briefing_overlay','ready@briefing_overlay',
                  'portrait_captured@briefing_overlay','portrait_requested@briefing_overlay',
                  'buffer_width@unified_overlay','buffer_height@unified_overlay',
                  'pending@briefing_overlay','pending_generation@briefing_overlay','armed@briefing_overlay','hover@briefing_overlay']
    values={name:u(address(name)) for name in scalar_names}
    for name in ('report_masked','report_tail','scene_clean','wheel_block','block_buttons'):
        location=address(name+'@briefing_overlay',optional=True)
        values[name]=u(location) if location else None
    src=(P/'src/native_adapter.cpp').read_text(encoding='utf-8-sig').split('struct ModFeatures {',1)[1].split('};',1)[0]
    src=re.sub(r'//[^\n]*|/\*.*?\*/','',src,flags=re.S);fields=[]
    for match in re.finditer(r'\bbool\s+([^;]+);',src):fields+=re.findall(r'\b([A-Za-z_]\w*)\s*=',match[1])
    raw=rd(address('mod_features@@'),len(fields))
    features={name:bool(raw[fields.index(name)]) for name in ('convenience','advisor_briefing','trace_original')}
    raw=rd(address('inbox@advisor_briefing'),48);begin,end=struct.unpack_from('<QQ',raw)
    schema=address('stored_event_bytes@advisor_briefing',optional=True)
    stride=u(schema) if schema else 64
    assert stride in (64,96),'Unrecognized event ABI; do not read the vector'
    inbox=dict(events=(end-begin)//stride if end>=begin and (end-begin)%stride==0 else None,event_bytes=stride,
               world=hex(struct.unpack_from('<Q',raw,24)[0]),force=struct.unpack_from('<I',raw,32)[0],
               generation=struct.unpack_from('<I',raw,36)[0],collecting=bool(raw[40]),fallback=bool(raw[41]),has_cycle=bool(raw[42]))
    sample=[];diplomatic_sample=[];recruitment_sample=[];event_kinds={}
    if inbox['events'] is not None and 0<=inbox['events']<=2048:
        for i in range(inbox['events']):
            event=begin+i*stride;kind=u(event);event_kinds[str(kind)]=event_kinds.get(str(kind),0)+1
            if i>=8 and not (kind==8 and len(diplomatic_sample)<8) and not (kind==4 and len(recruitment_sample)<8):continue
            length=q(event+24);capacity=q(event+32)
            if not 0<=length<=4095:continue
            string=q(event+8) if capacity>7 else event+8
            item=dict(kind=kind,day=u(event+44),text=rd(string,min(length,300)*2).decode('utf-16-le','replace'))
            if stride==96:
                length,capacity=q(event+80),q(event+88)
                if 0<length<=4095:
                    string=q(event+64) if capacity>7 else event+64
                    item['report']=rd(string,min(length,300)*2).decode('utf-16-le','replace')
            if i<8:sample.append(item)
            if kind==8 and len(diplomatic_sample)<8:diplomatic_sample.append(item)
            if kind==4 and len(recruitment_sample)<8:recruitment_sample.append(item)
    raw=rd(address('model@briefing_overlay'),120)
    model=dict(unread=struct.unpack_from('<I',raw,96)[0],pages=struct.unpack_from('<I',raw,104)[0],
               page=struct.unpack_from('<I',raw,100)[0],generation=struct.unpack_from('<I',raw,108)[0],
               collecting=bool(raw[112]),open=bool(raw[113]))
    source=list(struct.unpack('<4f',rd(address('source@briefing_overlay'),16)))
    profile=(P/'src/advisor_briefing_profile.h').read_text().split('static const Slot slots[]={')[1]
    slots=[]
    for aa,tt in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',profile):
        a,t=int(aa,16),int(tt,16);actual=q(base+a)
        slots.append(dict(slot=aa,owner='convenience' if dll<=actual<dll+size else 'native' if actual==base+t else 'other'))
    report=q(address('report_state@advisor_briefing'))
    native_report=dict(active=bool(report),phase=u(report+0x470) if report else None,
                       deferred=bool(rd(address('report_deferred@advisor_briefing'),1)[0]))
    world=q(base+0x277DF18);context=q(world+0x871E8) if world else 0
    # 110E0 returns this singleton; 56FE50 uses +10 count/+20 state array.
    manager=base+0x1B620B0;count=q(manager+0x10);array=q(manager+0x20);states=[]
    if count<=256 and array:
        for i in range(max(0,count-12),count):
            state=q(array+i*8)
            if not state:continue
            name=(rd(state+0x70,128) or b'').split(b'\0',1)[0].decode('ascii','replace')
            vt=q(state)
            states.append(dict(name=name,address=hex(state),vtable=hex(vt-base)))
    updating=q(manager+0x48)
    updating_name=(rd(updating+0x70,128) or b'').split(b'\0',1)[0].decode('ascii','replace') if updating else None
    native_ui=dict(states=states,state_count=count,updating_state=hex(updating),updating_state_name=updating_name)
    input_metrics={}
    for name in ('input_delay_ms','input_max_delay_ms','input_dispatches','input_posts','input_accepted','input_cancel_generation','input_render_ms','input_max_render_ms','raster_ms','raster_max_ms','quad_calls','draw_us','draw_max_us','present_us','present_max_us','di_state_calls','di_data_calls','di_state_modified','di_data_modified','context_resets'):
        location=address(name+'@briefing_overlay',optional=True)
        input_metrics[name]=q(location) if location else None
    stalls={}
    for name in ('stall_recent_ms','stall_max_ms'):
        location=address(name+'@advisor_briefing',optional=True)
        stalls[name]=list(struct.unpack('<6Q',rd(location,48))) if location else None
    def diagnostic_array(name,count):
        location=address(name+'@advisor_briefing',optional=True)
        return list(struct.unpack('<'+'Q'*count,rd(location,count*8))) if location else None
    activity=diagnostic_array('hook_activity',27)
    schema=address('message_entry_paths@advisor_briefing',optional=True)
    entry_paths=u(schema) if schema else 4
    schema=address('message_reason_columns@advisor_briefing',optional=True)
    reason_columns=u(schema) if schema else 13
    assert entry_paths in (4,7) and reason_columns in (13,15),'Unknown message diagnostic ABI'
    entry_counts=diagnostic_array('message_entry_counts',entry_paths*2)
    entry_reasons=('inactive','not_collecting','fallback','overlay_unavailable','tutorial','interaction',
                   'native_mode','native_context','caller_null','invalid_or_empty_text','store_failed','owner_not_allowed','recruitment_phase_or_person','params_null','summary_fields_missing')[:reason_columns]
    entry_failures=diagnostic_array('message_pass_reasons',entry_paths*len(entry_reasons))
    message_entries={name:dict(captured=entry_counts[i*2] if entry_counts else None,
                              relayed=entry_counts[i*2+1] if entry_counts else None,
                              pass_reasons=dict(zip(entry_reasons,entry_failures[i*len(entry_reasons):(i+1)*len(entry_reasons)])) if entry_failures else None)
                     for i,name in enumerate(('1F0AD0','1F09E0','1F0DD0','842380','6C43B0','6C4670','41D550')[:entry_paths])}
    hooks={name:dict(zip(('calls','tick_ms','thread_id'),activity[i*3:i*3+3])) if activity else None
           for i,name in enumerate(('user_frame','log_frame','advice_frame','narration','simple_frame',
                                    'diplomatic_wait','event_speech','removed_recruitment_callsite','report_frame'))}
    failures=diagnostic_array('wait_failures',30);false_counts=diagnostic_array('wait_false',3)
    reasons=('top_mismatch','waiting_zero','parent_missing','owner_mismatch','active_false',
             'collecting_or_cycle_false','fallback_true','available_false','tutorial_true','interaction_true')
    wait=dict(reason_semantics='first failed evaluated gate; native short-circuit order preserved',paths={})
    for i,name in enumerate(('advance',)):
        wait['paths'][name]=dict(false_count=false_counts[i] if false_counts else None,
                                failures=dict(zip(reasons,failures[i*10:i*10+10])) if failures else None)
    for name in ('wait_last_state','wait_last_rva','wait_last_top_equal','wait_zero_writes','wait_native_returns','wait_top_changed'):
        value=diagnostic_array(name,1)
        wait[name]=value[0] if value else None
    wait['top_equal_semantics']='top_state()==state, read-only equivalent of 56FE10(state)'
    threads={}
    for name in ('present_thread_id','present_tick_ms','wndproc_thread_id','wndproc_tick_ms','lock_thread_id','lock_tick_ms',
                 'di_install_attempted','di_install_success','di_patched_table','di_interface_kind','di_provider_stage','di_interface_mask'):
        location=address(name+'@briefing_overlay',optional=True)
        threads[name]=q(location) if location else None
    threads['di_interface']={0:None,1:'A',2:'W'}.get(threads['di_interface_kind'],'unknown')
    stages=('untried','proxy_missing','export_missing','DirectInput8Create_failed','input_null',
            'CreateDevice_failed','device_null','vtable_module_rejected','VirtualProtect_failed','installed','table_registry_full')
    location=address('di_install_details@briefing_overlay',optional=True)
    details=list(struct.unpack('<16Q',rd(location,128))) if location else None
    threads['di_interfaces']={}
    for i,kind in enumerate(('A','W')):
        fields=('stage','create_hresult','device_hresult','vtable_module_mask','win32_error','input','table','attempts')
        item=dict(zip(fields,details[i*8:(i+1)*8])) if details else dict.fromkeys(fields)
        item['stage_name']=stages[item['stage']] if item['stage'] is not None and item['stage']<len(stages) else None
        paths=address('di_owner_paths@briefing_overlay',optional=True)
        item['owner_paths']={name:rd(paths+(i*2+j)*260*2,260*2).decode('utf-16-le','replace').split('\0',1)[0] if paths else None
                             for j,name in enumerate(('GetDeviceState','GetDeviceData'))}
        item['chain_target_paths']=dict(item['owner_paths'])
        for field in ('create_hresult','device_hresult'):
            item[field]=f"0x{item[field]:08X}" if item[field] is not None else None
        threads['di_interfaces'][kind]=item
    threads['di_module_mask_semantics']='1=GetDeviceState allowed, 2=GetDeviceData allowed; 3=both; FROM_ADDRESS chain target must be system dinput8.dll, game SAN14_AI_PreviousInput.dll, or existing gameoverlayrenderer64.dll in the registered Steam installation'
    def overlay_array(name,count):
        location=address(name+'@briefing_overlay',optional=True)
        return list(struct.unpack('<'+'Q'*count,rd(location,count*8))) if location else [None]*count
    threads['di_owner_checks']=dict(zip(('allowed','rejected'),overlay_array('di_owner_checks',2)))
    threads['di_restore_counts']=dict(zip(('restored','skipped_new_owner'),overlay_array('di_restore_counts',2)))
    input_metrics['outside_click']=dict(zip(('close_down_consumed','matching_up_consumed'),overlay_array('outside_click_counts',2)))
    context_counts=diagnostic_array('player_context_counts',4)
    player_context=dict(zip(('log_frames','player_top','covered_top','panels_closed'),context_counts or [None]*4))
    time_counts=diagnostic_array('time_wait_counts',2)
    time_wait=dict(zip(('expired_immediately','native_relay'),time_counts or [None]*2))
    time_wait['result_semantics']='expired=0; never writes skip=1; interaction/diplomacy always native relay'
    threads['present_scope']='existing Present path entry into briefing_overlay::draw'
    threads['wndproc_scope']='existing WndProc path entry into briefing_overlay::input'
    timing=diagnostic_array('frame_metrics',16)
    frame_performance={name:dict(zip(('calls','total_us','max_us','layout_874650_calls'),timing[i*4:i*4+4])) if timing else None
                       for i,name in enumerate(('hud','log_frame','advice_frame','user_frame'))}
    moves=diagnostic_array('layout_move_calls',1)
    picture_schema=address('picture_count_slots@advisor_briefing',optional=True)
    picture_slots=u(picture_schema) if picture_schema else 0
    assert picture_slots in (0,79),'Unknown picture diagnostic ABI'
    picture_totals=diagnostic_array('picture_counts',2) if picture_slots else None
    picture_ids=diagnostic_array('picture_id_counts',picture_slots*2) if picture_slots else None
    picture_closes=diagnostic_array('picture_close_counts',3) if picture_slots else None
    picture_last=address('picture_last_id@advisor_briefing',optional=True)
    event_pictures=dict(
        suppressed=picture_totals[0] if picture_totals else None,
        displayed=picture_totals[1] if picture_totals else None,
        last_id=struct.unpack('<i',rd(picture_last,4))[0] if picture_last else None,
        by_id={str(i) if i<78 else 'other':dict(suppressed=picture_ids[i*2],displayed=picture_ids[i*2+1])
               for i in range(picture_slots) if picture_ids and any(picture_ids[i*2:i*2+2])},
        closes=dict(zip(('suppressed','displayed','unrecorded'),picture_closes)) if picture_closes else None,
        semantics='DLL lifetime totals; displayed means original display path called; IDs 0..77 exact, other aggregated')
    return dict(frame_performance=frame_performance,layout_874650_calls=moves[0] if moves else None,
                event_pictures=event_pictures,
                wait_dialog_304640=time_wait,player_context=player_context,message_entries=message_entries,hook_activity=hooks,wait_diagnostics=wait,thread_diagnostics=threads,
                stall_metrics=stalls,pid=pid,read_only=True,loaded_matches_installed=True,features=features,values=values,
                inbox=inbox,model=model,portrait_source=source,slots=slots,native_report=native_report,
                remaining_orders=u(context+0x454) if context else None,event_sample=sample,event_kinds=event_kinds,diplomatic_sample=diplomatic_sample,
                input_queue_metrics=input_metrics,native_ui=native_ui,recruitment_sample=recruitment_sample)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--symbols',help='Exact matching diagnostic DLL/MAP directory for an older running build')
    result=inspect(parser.parse_args().symbols)
    folder=P/'data/diagnostics';folder.mkdir(exist_ok=True)
    stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    output=folder/('advisor_live_'+stamp+'.json')
    with output.open('x',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False,indent=2)
    print(json.dumps(result,ensure_ascii=False))
