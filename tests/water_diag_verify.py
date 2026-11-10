import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""User 2026-10-29: bounded read-only water diagnostics, release exclusion."""
from pathlib import Path
import json
import re

P = Path(__file__).resolve().parents[1]
checks = 0
def ok(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)
def read(name):
    return (P / name).read_text(encoding='utf-8-sig')

water = read('src/native_water_diag.inl')
terrain = read('src/native_terrain_render.inl')
adapter = read('src/native_adapter.cpp')
split = read('src/native_split_install.inl')
ok(water.split('#pragma once\n', 1)[1].startswith('#ifndef NO_RUNTIME_LOG\n') and water.endswith('#endif\n'), 'entire diagnostic including headers and globals excluded from release')
import registry_text as R   # W5: includes come from the generated per-DLL lists
ok(R.hosts('native_terrain_trees.inl') == ['convenience'] and R.hosts('native_water_diag.inl') == ['convenience'] and R.unity().count('#include "native_water_diag.inl"') == 1, 'sole include in convenience only')
# User/Claude 2026-10-29 local54: optional identity callback precedes the original guarded call.
for method in ('install_early', 'on_device', 'restore'):
    ok(re.search(r'#ifdef SAN14_WATER_DIAG_INCLUDED\n(?:    water_diag::mask_sample=&confirm_mask;\n)?    water_diag::' + method + r'\([^\n]*\);\n#endif', terrain), method + ' guarded integration')
ok('data!=code' in water and 'GetModuleFileNameW(code,path,MAX_PATH)' in water and '_wcsicmp(path,system)' in water and 'GetModuleHandleW(' not in water, 'System32 function and device table module validation independent of proxy basename')
# User/Claude 2026-10-29 rework: real context tables are private heap allocations.
ok('if(context_table)' in water and 'table_in_image || !VirtualQuery(table' in water and 'region.Type!=MEM_PRIVATE' in water and 'region.State!=MEM_COMMIT' in water and 'PAGE_READONLY|PAGE_READWRITE' in water, 'context table exception requires nonimage committed readable private region')
ok('install_slots(ctx,context_slots,context_hooks,1,true)' in water and 'device->GetImmediateContext(&ctx)' in water, 'private table exception only used for GetImmediateContext result')
ok('if(early)return;' in water, 'early device probe never saves freed context heap tables')
ok('memcmp(s.bytes,s.original,sizeof(s.bytes))' in water and 'memcmp(free->bytes,target,sizeof(free->bytes))' in water, 'original fingerprints checked before publication and later guards')
ok('InterlockedCompareExchangePointer(s.table+s.slot,s.original,s.replacement)' in water, 'restore preserves foreign replacements')
ok('if(!ok)for(unsigned i=used;i>0;--i)restore_site(*added[i-1]);' in water, 'failed publication rolls back owned transaction')
ok('water_diag_guards_match()' in split and 'split_disable(water_diag::permitted,30);water_diag::restore();' in split, 'split guard isolates diagnostic mismatch 30')
ok('device_slots[]={12,15}' in water and 'context_slots[]={11}' in water, 'D3D11 vtable slot ABI')
ok('on_device(device,true)' in water and 'if(early)return;' in water and 'if(device)device->Release();' in water, 'early probe does not retain temporary context or device')
for word in ('fWaterLevel', 'sOceanHeightMask', 'sRiverHeightMask', 'vHeightMaskUV'):
    ok('"' + word + '"' in water, 'shader string ' + word)
for name, expected in [('ocean_vs', 'e98cd1cb824753846e8bdf90ed3c63c125d9ab7cf627170849dbcd99993ee9fd'), ('ocean_ps', 'cd3e0d0225f72704be18a6e1358500a883dce801fa793ec9b54431399b6eb889')]:
    array = re.search(name + r'\[32\]=\{([^}]+)\}', water)[1]
    ok(bytes(int(n.strip(), 16) for n in array.split(',')).hex() == expected, 'WATER.md SHA ' + name)
ok('first<8' in water and 'now-last_sample<5000' in water and 'now-c.budget.last_report<5000' in water, 'bounded samples and five-second binding report')
ok('ctx->CopyResource(c.staging,source);source->Release();' in water and 'D3D11_MAP_READ,0,&mapped' in water and 'ctx->Unmap(c.staging,0)' in water, 'private staging read-only synchronized Map, source released')
for text in ('view->Release()', 'resource->Release()', 'texture->Release()', 'device->Release()', 'vs->Release()', 'source->Release()'):
    ok(text in water, 'getter reference released: ' + text)
for forbidden in ('UpdateSubresource', 'WriteProcessMemory', 'CreateRemoteThread', '->VSSetConstantBuffers(', '->VSSetShaderResources(', 'D3D11_MAP_WRITE', 'fn<', 'world()', 'std::thread'):
    ok(forbidden not in water, 'no game resource/binding mutation or worker readback: ' + forbidden)
ok('if(c.readback_failed)return' in water and 'water_cb_unknown' in water, 'failed readback isolated; unknown and short layouts not decoded')
ok('c.object->VSGetShader(&vs,nullptr,nullptr)' in water, 'ClearState and command-list binding changes cannot leave false-positive water state')
entry = json.loads(read('development/features.json'))['modules']['terrain-edit']
ok('water-diag' in entry['tests'] and 'src/native_water_diag.inl' in entry['sources'], 'feature fixture registered')
# User/Claude 2026-10-29: remove all draw hooks; sample outgoing VS region.
for removed in ('DrawIndexed', 'DrawInstanced', 'observe_draw', 'water_draws', 'water_draw'):
    ok(removed not in water, 'removed unstable draw observation: ' + removed)
set_vs = water.split('static void STDMETHODCALLTYPE set_vs(', 1)[1].split('static void restore();', 1)[0]
ok(set_vs.index('observe_bind(*state,v)') < set_vs.index('call(c,v,i,n)'), 'outgoing region sampled before native binding replacement')
ok('water_binds' in water and '++c.budget.binds' in water, 'counter means incoming water bindings')
ok('s.published && !s.context_table && !site_matches(s)' in water, 'strict shared guard covers device slots only')
ok('water_hook_replaced' in water and 'c.disabled=true;report=true;' in water and 'site->published=false' in water, 'replacement permanently abandons context and reports once')
ok('if(s.context_table && s.table[s.slot]!=s.replacement){s.published=false;return;}' in water, 'rollback never attempts replacement write')

ok('if(mask.object && mask_sample)mask_sample(mask.object,uv);' in water and 'float uv[4];memcpy(uv,words+0xd0/4,sizeof(uv));' in water, 'local54 observer forwards actual t0 and cb0 float4 only after successful readback')

print(json.dumps({'water_diag_source_checks': checks, 'failed': 0}))
