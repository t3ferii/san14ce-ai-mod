import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""User 2026-10-29 R2: mutable control-map and two authorized overlay lines."""
from pathlib import Path
import hashlib
import json
import re

P = Path(__file__).resolve().parents[1]
checks = 0

def ok(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)

def read(rel):
    return (P / rel).read_text(encoding='utf-8-sig')

overlay = (P / 'src/native_unified_overlay.inl').read_bytes()
# User/Claude 2026-10-29 local49: remove only explicitly marked diagnostic blocks.
# Keep the original preimage hash and every existing R1/R2 assertion unchanged.
perf_blocks = re.findall(rb'(?m)^// perf-begin\n#ifndef NO_RUNTIME_LOG\n[\s\S]*?\n#endif\n// perf-end\n', overlay)
ok(len(perf_blocks) == overlay.count(b'// perf-begin\n') == overlay.count(b'// perf-end\n'),
   'every perf marker encloses a complete NO_RUNTIME_LOG diagnostic block')
overlay = re.sub(rb'(?m)^// perf-begin\n#ifndef NO_RUNTIME_LOG\n[\s\S]*?\n#endif\n// perf-end\n', b'', overlay)
line = b'    terrain_render::on_device(device);\n'
ok(overlay.count(line) == 1, 'exactly one device observer call')
# User R2 follow-up explicitly authorizes exactly the upload and dirty gate lines.
upload_line = b'    terrain_render::on_present(context);\n'
gate = b' && !terrain_render::needs_present()'
ok(overlay.count(upload_line) == 1 and overlay.count(gate) == 1, 'exactly two authorized R2 overlay changes')
ok(line + upload_line in overlay, 'upload immediately follows device observation')
# 사용자 결정 2026-11-03: 통합 인사 제거. The exact shared overlay hash excludes its removed officer panel.
ok(hashlib.sha256(overlay.replace(upload_line, b'').replace(gate, b'').replace(line, b'')).hexdigest() ==
   '762b9f37bb5e734caa4374bc3a5010f3b5070fd6d245ada66fd908810d3dda7e',
   'shared overlay source hash matches after undoing the R1 observer and two R2 lines')
ok(b'} else current->Release();\n' + line in overlay, 'observer follows device acquisition')
native = read('src/native_terrain_render.inl')
adapter = read('src/native_adapter.cpp')
split = read('src/native_split_install.inl')
terrain = read('src/native_terrain_edit.inl')
ok(re.search(r'#if !defined\(SPLIT_COMBAT\)\n#include "native_terrain_render.inl"\n#endif', adapter), 'observer absent from combat compilation')
ok('CreateTexture2DSlot=5' in native, 'COM ABI slot five')
ok('InterlockedCompareExchangePointer(table+CreateTexture2DSlot' in native, 'atomic publication preserves competing owner')
# User 2026-10-29: a forwarding game-folder proxy may own the basename;
# the actual implementation and vtable must both belong to the system path.
ok('GetModuleHandleW(L"d3d11.dll")' not in native and 'data!=code' in native and
   'GetModuleFileNameW(code,path,MAX_PATH)' in native and
   'GetSystemDirectoryW(system,MAX_PATH)' in native and
   'L"\\\\d3d11.dll"' in native and '_wcsicmp(path,system)' in native,
   'actual code/table module must match system D3D11 path, independently of proxy basename')
ok('unsigned char bytes[16]' in native and 'memcmp(bytes,reinterpret_cast<void *>(original),sizeof(bytes))' in native, 'original bytes checked before publication')
ok('s.table[CreateTexture2DSlot]!=reinterpret_cast<void *>(&create_texture_2d)' in native, 'foreign slot fails guard')
ok('memcmp(s.bytes,reinterpret_cast<void *>(s.original),sizeof(s.bytes))' in native, 'saved original bytes rechecked')
ok('split_terrain_render_slot={terrain_render::CreateTexture2DSlot,&terrain_render_guards_match,&terrain_render::restore}' in split, 'convenience registration owns slot, guard and restoration')
ok('split_disable(terrain_render::permitted,28)' in split, 'renderer guard isolated from terrain editing')
rollback = split.split('static void split_rollback() {', 1)[1].split('static bool split_prepare()', 1)[0]
ok(rollback.index('split_terrain_render_slot.restore();') < rollback.index('if(!split_install.attempted)return;'), 'runtime rollback also handles pending split publication')
ok('struct Lifetime {~Lifetime(){restore();}};' in native, 'DLL lifetime restores owned slot')
ok('InterlockedCompareExchangePointer(s.table+CreateTexture2DSlot,reinterpret_cast<void *>(s.original),reinterpret_cast<void *>(&create_texture_2d))' in native, 'restore never overwrites foreign owner')
ok('if(!dev || !active())return;' in native and 'enabled && mod_features.convenience && mod_features.terrain_edit' in native, 'existing feature gates installation')
# User 2026-10-29 local36: initialized assets only; all large BC share the quota.
ok('MaxTextures=64' in native and 'record_count<MaxTextures' in native and
   'MaxBCTextures=4' in native and 'bc_count<MaxBCTextures' in native,
   '64 total records with a shared four-record BC quota')
ok('d.Width>=1024 && d.Height>=1024' in native and
   '(format>=70 && format<=84) || (format>=94 && format<=99)' in native and
   '!bc_format(d.Format) || d.Width>=4096' in native and 'initial && initial[0].pSysMem' in native,
   'initialized large assets only; BC formats require width at least 4096')
ok('desc->MipLevels*1000u+desc->ArraySize,desc->Height,desc->CPUAccessFlags*100000u+desc->MiscFlags' in native and
   'if(saved_count==MaxTextures)log_event("terrain_texture_full",saved_count,0,0)' in native,
   'height and array size packed as specified; full event only on final saved record')
# User/Claude 2026-10-29 local54: add verified mask capture to the same native call; retain ordering assertions.
# User 2026-10-29 R2b permits Copy/Map only in the diagnostic one-pixel reader.
native_scan = native
for forbidden in ['WriteProcessMemory', 'CreateRemoteThread', 'fn<', 'world()', '->GetDesc(']:
    ok(forbidden not in native_scan, 'no texture/game mutation or original game call: ' + forbidden)
wrapper = native.split('static HRESULT STDMETHODCALLTYPE create_texture_2d(', 1)[1].split('static bool guards_locked()', 1)[0]
ok(wrapper.index('HRESULT result=original(dev,(capture || height_capture || mask_capture)?&writable:desc,create_initial,out);') < wrapper.index('#ifndef NO_RUNTIME_LOG\n    if(SUCCEEDED'), 'native create/retry completes before observation')
ok('return result;' in wrapper, 'native HRESULT returned unchanged')
ok('GetCurrentThreadId()' in wrapper and 'true,GetCurrentThreadId()' in wrapper, 'creation thread retained internally for initialized assets')
ok('UINT words[4]' in wrapper and 'memcpy(words,initial[0].pSysMem,sizeof(words))' in wrapper and
   wrapper.index('HRESULT result=original(dev,(capture || height_capture || mask_capture)?&writable:desc,create_initial,out);') < wrapper.index('memcpy(words,'),
   'exactly four u32 input words copied after native call, accepting unaligned input')
ok('log_event("terrain_texture_data",words[0],words[1],words[2])' in wrapper and
   'log_event("terrain_texture_pitch",pitch,desc->Format,words[3])' in wrapper and
   'pitch=initial[0].SysMemPitch' in wrapper,
   'all 16 fingerprint bytes and first subresource pitch logged through shared logger')
ok('terrain_render::before_world_change();' in terrain.split('static void before_world_change()', 1)[1], 'existing load boundary connected before empty edit early return')
ok('#ifndef NO_RUNTIME_LOG\nstruct TextureRecord' in native and '#ifndef NO_RUNTIME_LOG\n    if(SUCCEEDED' in native, 'release omits records and logs')
entry = json.loads(read('development/features.json'))['modules']['terrain-edit']
ok('terrain-render' in entry['tests'] and 'src/native_terrain_render.inl' in entry['sources'], 'feature route registered')
ok("'terrain-render'" in read('tools/check_feature.ps1'), 'dispatcher registered')
# User 2026-10-29 R1b: install on the existing initialization worker, before UI.
activate = split.split('static void split_activate() {', 1)[1]
ok(activate.count('terrain_render::install_early();') == 1 and
   activate.index('InterlockedExchange(&enabled,1);') < activate.index('terrain_render::install_early();') < activate.index('if(mod_features.distance)'),
   'one early call immediately in initial convenience activation, after enabled gate')
loader = read('src/module_loader.cpp')
worker = loader.split('static DWORD WINAPI initialize_modules(LPVOID) {', 1)[1].split('BOOL WINAPI DllMain', 1)[0]
ok('CreateThread(nullptr,0,initialize_modules' in loader and
   worker.index('load_module(L"SAN14_Convenience.dll")') < worker.index('start(split_module_abi,api);') and
   'start(' not in loader.split('BOOL WINAPI DllMain', 1)[1],
   'loader worker calls convenience initializer after LoadLibrary returns, outside DllMain')
ok('initialize(nullptr);' in adapter.split('SAN14ConvenienceInitialize', 1)[1].split('#endif', 1)[0] and
   'terrain_render::install_early' not in adapter.split('BOOL WINAPI DllMain', 1)[1],
   'convenience initializer runs install flow, DllMain does not invoke early probe')
early = native.split('static void install_early() {', 1)[1].split('static void on_device(', 1)[0]
ok('LoadLibraryW(system)' in early and 'GetProcAddress(library,"D3D11CreateDevice")' in early and
   'D3D_DRIVER_TYPE_HARDWARE' in early and 'D3D11CreateDeviceAndSwapChain' not in native and 'CreateWindow' not in native,
   'early probe uses system hardware device without a window or swapchain')
ok('if(!early)dev->AddRef();' in native and 'early?nullptr:dev' in native and
   'if(temporary)temporary->Release();' in early,
   'temporary device is not retained by slot and is always released')
ok('if(rejected && !early)permitted=false;' in native and 'if(!installed)FreeLibrary(library);' in early,
   'probe failure releases its library and leaves late installation eligible')
ok('log_event("terrain_texture_hook",early?2:1,0,0)' in native and
   'log_event("terrain_texture_table",static_cast<unsigned>(match),0,0)' in native and 'table==early_table?1:0' in native,
   'early reason two and game-vtable identity diagnostics')
print(json.dumps({'terrain_render_source_checks': checks, 'failed': 0}))

# Every panel keeps its original independent visibility gate in draw_locked.
text = overlay.decode('utf-8-sig')
for condition in ['if(context && briefing_overlay::needs_draw() && bind(swap.OutputWindow))',
                  'if(context && read_flag(&visible) && next.width>0 && next.height>0)']:
    ok(condition in text, 'panel visibility guard preserved: ' + condition)
ok('InterlockedCompareExchange(&present_pending,0,0)' in native,
   'needs_present reads atomic flag without SRW lock')
ok('ctrl_texture && dirty_count && !render_failed?1:0' in native,
   'atomic flag requires both a retained texture and dirty cells')
ok('std::vector<unsigned> original_ctrl,shadow_ctrl' in native and
   'initial[0].SysMemPitch,CtrlPitch' in native, 'separate original/shadow maps with pitched row copies')
ok('if(install_present)unified_overlay::install();' in terrain and
   'sync_render();' in read('src/native_terrain_edit_save.inl'), 'UI/load host installs Present after capture')
ok('terrain_render::mark(hex,terrain,original);' in terrain and
   'terrain_render::mark(c.hex,c.next,c.original);' in read('src/native_terrain_edit_save.inl'), 'apply and save restore deliver goals')
for r in ['log_event("terrain_render_capture",desc->Format,saved?1:0,0)',
          'log_event("terrain_render_present",1,0,0)',
          'log_event("terrain_render_upload",cells,pixels,0)']:
    ok(r in native, 'required diagnostic: ' + r)
print(json.dumps({'terrain_render_r2_source_checks': checks, 'failed': 0}))

# R2b: local37 uploads alone did not change the displayed terrain.
reader = native.split('#ifndef NO_RUNTIME_LOG\n// Diagnostic only,', 1)[1].split('\n#endif', 1)[0]
ok('context->CopySubresourceRegion(readback_texture,0,0,0,0,ctrl_texture,0,&pixel)' in reader and
   'context->Map(readback_texture,0,D3D11_MAP_READ,0,&mapped)' in reader and
   'context->Unmap(readback_texture,0)' in reader, 'one-pixel synchronous GPU copy/read/unmap is diagnostic only')
ok('log_event("terrain_render_readback",hex,value,expected)' in reader and
   'if(written && cells==0)readback_locked(context,hex)' in native, 'only first uploaded cell is read and compared per frame')
ok('ctrl_create(ctrl_device,&d,nullptr,&readback_texture)' in reader, 'staging creation bypasses wrapper lock re-entry')
ok('D3D11_USAGE_STAGING' in reader and 'D3D11_CPU_ACCESS_READ' in reader and
   'd.Width=d.Height=d.MipLevels=d.ArraySize=d.SampleDesc.Count=1' in reader, 'staging is one pixel, read-only CPU access')
ok('if(readback_texture){readback_texture->Release();readback_texture=nullptr;}' in native, 'readback resource released with captured resource')
boundary = native.split('static void before_world_change()', 1)[1].split('static void restore()', 1)[0]
ok('clear_queue_locked()' in boundary and 'clear_targets_locked()' not in boundary, 'R2b world boundary retains targets')
ok('world_transition && goal==RestoreOriginal' in native, 'CPU world rollback cannot erase retained render targets')
ok('patched={shadow.data(),CtrlPitch,0}' in wrapper and 'write_cell(shadow,base,hex,goals[hex])' in wrapper and
   wrapper.index('write_cell(shadow,base,hex,goals[hex])') < wrapper.index('HRESULT result=original('), 'preserved goals applied to private initial buffer before native create')
ok('log_event("terrain_render_initial",initial_cells,0,0)' in wrapper, 'initial application count diagnostic')
# User/Claude 2026-10-29 local48: resetting a target must also reset its cliff profile.
ok('goals[hex]=RestoreOriginal;has_goal[hex]=false;height_delta[hex]=0.0f;carve_delta[hex]=0.0f;cliff[hex]=false;cpu_cell_locked(hex);enqueue_locked(hex)' in native, 'full UI resync restores targets and cliff flags missing from new sidecar')
print(json.dumps({'terrain_render_r2b_source_checks': checks, 'failed': 0}))

cpu = native.split('static void cpu_cell_locked(unsigned hex) {', 1)[1].split('static void heightfield_created', 1)[0]
gpu = native.split('static unsigned height_cell_locked(', 1)[1].split('#ifndef NO_RUNTIME_LOG', 1)[0]
ok('float height=height_at(x,z)' in cpu and 'shadow_height[at]=height_at(x,z)' in gpu,
   'CPU collision and GPU textures both evaluate production height_at')
ok('memset(cliff,0,sizeof(cliff))' in boundary and 'cliff[hex]=terrain==20 && original_terrain!=20' in native,
   'world boundaries clear cliff flags and native marks set or clear them')
ok('#ifndef NO_RUNTIME_LOG\nstatic void log_cliff_locked(unsigned hex) {' in native and
   'log_event("terrain_cliff",hex,static_cast<unsigned>(range),range>=150.0f?1:2)' in native,
   'cliff mark diagnostic is excluded from release')
print(json.dumps({'terrain_render_cliff_source_checks': checks, 'failed': 0}))

# User/Claude 2026-10-29 local55: requested dump, expanded fit and release proof.
ok('best_x-uv[2]' not in native and 'best_z-uv[3]' not in native and
   'log_event("water_mask_bound",bound>=0?1:0,selected?1:0,uv_bits[2])' in native, 'UV zw is diagnostic bits only')
# User/Claude 2026-10-29 local56: the mask carries per-terrain carve, so fit on inliers only.
ok('model<1' in native and 'dz=-8;dz<=8' in native and 'dx=-8;dx<=8' in native and
   'log_event("water_mask_fit2"' in native, 'nearest model searches full signed integer grid and reports errors')
ok('std::fabs(value-ground)<200' in native and 'if(e<200)errors.push_back(e);' in native and
   'inliers>=0.6f && median<=40.0f && tail<=200.0f' in native, 'carved water excluded from fit; inlier thresholds')
dump = native.split('#ifndef NO_RUNTIME_LOG\nstatic bool mask_dump_attempted;', 1)[1].split('\n#endif\nstatic void confirm_mask', 1)[0]
ok('CREATE_NEW' in dump and 'WriteFile(file,data.data(),bytes,&written,nullptr)' in dump and
   'mask_dump_attempted=true' in dump and 'L"water_mask"' in dump and 'L"ground_height"' in dump,
   'raw original dumps are diagnostic only, never overwrite and run once')
ok('static void restore_mask_writes_locked' in native and 'water_diag is off by default' in native and
   'mask_selected=accepted?selected:-1;mask_ready=accepted>0;' in native and 'if(!mask_overflow)' in native and 'confirm_release_masks_locked(context);' in native,
   'every build selects the newest passing candidate and restores writes to a replaced one')
ok('m.original[at]+offset*mask_scale:m.original[at]' in native and
   'field_at(carve_delta,' in native and 'carve(terrain)-carve(original_terrain)' in native,
   'mask = original + ground offset + carve change; zero offset writes original bits')
print(json.dumps({'terrain_render_local55_source_checks': checks, 'failed': 0}))
