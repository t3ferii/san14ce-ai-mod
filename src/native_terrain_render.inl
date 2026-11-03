// R2b (user 2026-10-29): creation-time control-map targets and diagnostic GPU readback.
#pragma once
#define SAN14_TERRAIN_RENDER_INCLUDED 1
#include <d3d11.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "terrain_call_profile.h"
namespace terrain_render {
using CreateTexture2D=HRESULT(STDMETHODCALLTYPE *)(ID3D11Device *,const D3D11_TEXTURE2D_DESC *,const D3D11_SUBRESOURCE_DATA *,ID3D11Texture2D **);
static void profile_upload(ID3D11DeviceContext *context,ID3D11Resource *resource,UINT subresource,
    const D3D11_BOX *box,const void *data,UINT pitch,UINT depth) {
    TERRAIN_SCOPE(UploadNative);context->UpdateSubresource(resource,subresource,box,data,pitch,depth);
}
static constexpr unsigned CreateTexture2DSlot=5,MaxTables=4,MaxTextures=64,MaxBCTextures=4;
static SRWLOCK slot_lock=SRWLOCK_INIT,record_lock=SRWLOCK_INIT;
static bool permitted=true,stopping=false;
struct DeviceSlot {
    void **table;
    CreateTexture2D original;
    ID3D11Device *keep;
    unsigned char bytes[16];
    bool published;
};
static DeviceSlot slots[MaxTables]={};
static bool early_attempted;
static void **early_table;
// Keep the system implementation loaded for process lifetime, including calls
// already inside our wrapper during restoration. No temporary device is kept.
static HMODULE early_library;
#ifndef NO_RUNTIME_LOG
struct TextureRecord {
    ID3D11Texture2D *address; // Non-owning identity, never dereferenced or released.
    UINT Width,Height,MipLevels,ArraySize;
    DXGI_FORMAT Format;
    D3D11_USAGE Usage;
    UINT BindFlags,CPUAccessFlags,MiscFlags;
    bool initial_data;
    DWORD thread_id;
    UINT first_words[4];
    UINT sys_mem_pitch;
};
static TextureRecord records[MaxTextures]={};
static unsigned record_count,bc_count;
static bool saw_2049;
static void **reported_tables[MaxTables]={};
static unsigned reported_count;
static bool bc_format(DXGI_FORMAT format) {
    return (format>=70 && format<=84) || (format>=94 && format<=99);
}
static bool target(const D3D11_TEXTURE2D_DESC &d) {
    // User 2026-10-29 local36: keep assets, exclude uninitialized render targets.
    return d.Width>=1024 && d.Height>=1024 && (!bc_format(d.Format) || d.Width>=4096);
}
#endif
static bool active() {return enabled && mod_features.convenience && mod_features.terrain_edit && !(mod_features.terrain_diag_mode & 2u);}
static constexpr unsigned CtrlSize=2049,CtrlPitch=CtrlSize*4,HexCount=48400,FrameLimit=256;
static constexpr int RestoreOriginal=-1;
static constexpr double RowHeight=810.5997779422346; // sqrt(3)*468
static SRWLOCK ctrl_lock=SRWLOCK_INIT;
static ID3D11Texture2D *ctrl_texture;
static ID3D11Device *ctrl_device; // Texture owns the device lifetime.
// local39 live log: right after the BGRA asset, the render thread creates two
// 2049² R8G8B8A8_TYPELESS DEFAULT SRV|RT|UAV work textures without initial data
// and the ground shader samples those, not the asset. Mirror cells into them.
static constexpr unsigned MaxWork=2;
static ID3D11Texture2D *ctrl_work[MaxWork];
static unsigned ctrl_work_count;
// R3 (user 2026-10-29): same layout for height — R32 asset plus two R32 work textures.
static ID3D11Texture2D *height_texture,*height_work[MaxWork];
static ID3D11Device *height_device;
static unsigned height_work_count;
static std::vector<float> original_height,shadow_height;
// local55: diagnostic requires SOCEAN VS t0 plus fitting; release requires
// exactly one passing creation candidate. Originals remain byte-identical.
static constexpr unsigned MaskSize=2048,MaskPitch=MaskSize*4,MaxMasks=2;
struct WaterMask {
    ID3D11Texture2D *texture=nullptr;
    ID3D11Device *device=nullptr;
    std::vector<float> original,shadow;
};
static WaterMask water_masks[MaxMasks];
static ID3D11Texture2D *mask_work[MaxWork];
static unsigned mask_count,mask_work_count;
static int mask_selected=-1,mask_offset_x,mask_offset_z;
static float mask_scale=1;
static unsigned mask_model; // 0 nearest, 1 bilinear s=0, 2 bilinear s=0.5
static bool mask_overflow,mask_mirror_pending;
static bool mask_fit_attempted,mask_ready;
static float height_delta[HexCount]={},carve_delta[HexCount]={};
static bool cliff[HexCount]={};
static std::vector<unsigned> original_ctrl,shadow_ctrl;
static signed char goals[HexCount]={};
static bool queued[HexCount]={},has_goal[HexCount]={},resync_needed,render_failed,present_install_requested,world_transition;
static unsigned short dirty_hex[HexCount]={};
static unsigned dirty_head,dirty_count;
static volatile LONG present_pending;
#ifndef NO_RUNTIME_LOG
static volatile LONG present_reported;
static ID3D11Texture2D *readback_texture;
static CreateTexture2D ctrl_create;
static bool readback_failed;
#endif
static bool needs_present() {return InterlockedCompareExchange(&present_pending,0,0)!=0;}
static void publish_pending_locked() {
    InterlockedExchange(&present_pending,ctrl_texture && dirty_count && !render_failed?1:0);
}
static void clear_queue_locked() {
    memset(queued,0,sizeof(queued));dirty_head=dirty_count=0;
    publish_pending_locked();
}
static void clear_targets_locked() {
    memset(goals,0,sizeof(goals));memset(has_goal,0,sizeof(has_goal));memset(height_delta,0,sizeof(height_delta));
    memset(carve_delta,0,sizeof(carve_delta));memset(cliff,0,sizeof(cliff));
    world_transition=false;clear_queue_locked();
}
static void enqueue_locked(unsigned hex) {
    if(!queued[hex]) {
        dirty_hex[(dirty_head+dirty_count)%HexCount]=static_cast<unsigned short>(hex);
        queued[hex]=true;++dirty_count;
    }
}
static void discard_ctrl_locked() {
#ifndef NO_RUNTIME_LOG
    if(readback_texture){readback_texture->Release();readback_texture=nullptr;}
    ctrl_create=nullptr;readback_failed=false;
#endif
    for(unsigned i=0;i<ctrl_work_count;++i){ctrl_work[i]->Release();ctrl_work[i]=nullptr;}
    ctrl_work_count=0;
    if(ctrl_texture){ctrl_texture->Release();ctrl_texture=nullptr;}
    ctrl_device=nullptr;original_ctrl.clear();shadow_ctrl.clear();publish_pending_locked();
}
static void discard_height_locked() {
    for(unsigned i=0;i<height_work_count;++i){height_work[i]->Release();height_work[i]=nullptr;}
    height_work_count=0;
    if(height_texture){height_texture->Release();height_texture=nullptr;}
    height_device=nullptr;original_height.clear();shadow_height.clear();
}
static void discard_mask_candidate_locked(WaterMask &m) {
    if(m.texture)m.texture->Release();
    m.texture=nullptr;m.device=nullptr;
    std::vector<float>().swap(m.original);std::vector<float>().swap(m.shadow);
}
static void discard_masks_locked() {
    for(auto &m:water_masks)discard_mask_candidate_locked(m);
    for(auto &w:mask_work)if(w){w->Release();w=nullptr;}
    mask_count=mask_work_count=0;mask_selected=-1;mask_fit_attempted=mask_ready=false;mask_overflow=mask_mirror_pending=false;
}
// Median hex-centre height per terrain in the shipped heightmap (MapExCe × tex[0],
// 2026-10-29). Regional elevation varies widely, so only differences are applied.
static float terrain_height(unsigned terrain) {
    static constexpr float table[25]={-1,786,810,803,773,807,850,840,1166,1447,1528,1243,625,318,78,
        -1,-1,-1,-1,2072,705,139,-1,-1,67};
    return terrain<25?table[terrain]:-1.0f;
}
static float height_change(unsigned terrain,unsigned original_terrain) {
    float to=terrain_height(terrain),from=terrain_height(original_terrain);
    if(terrain==20 || terrain==original_terrain || to<0 || from<0)return 0.0f;
    // User 2026-10-29: water-to-water edits rose above the river surface; keep the bed.
    auto water=[](unsigned t) {return t==12 || t==13 || t==14 || t==21 || t==24;};
    if(water(terrain) && water(original_terrain))return 0.0f;
    float delta=to-from;return std::fabs(delta)<30.0f?0.0f:delta;
}
// User 2026-10-29: the ocean HeightMask (VS t0, level 450) is the ground plus a
// per-terrain carve; medians of (mask-ground) at original hex centres. Rapids'
// rough water comes from this depth, not from the ground or material maps.
static float carve(unsigned terrain) {
    switch(terrain) {
        case 12:return -1785.0f;case 13:return -1518.0f;case 21:return -3266.0f;case 20:return -932.0f;
        default:return 0.0f;
    }
}
static int material(unsigned terrain) {
    switch(terrain) {
        case 1:return 0;case 3:return 1;case 2:return 2;
        case 5:case 6:return 4;
        case 8:case 9:case 10:case 11:return 5;
        case 19:return 6;
        case 20:return 6; // Cliff: inferred, not live-game verified.
        case 4:return 7;case 7:return 8;
        case 12:case 13:case 14:case 24:return 9;
        case 21:return 9; // Rapids: inferred, not live-game verified.
        default:return -2;
    }
}
static void cpu_cell_locked(unsigned hex);
static void refresh_cliffs_locked(unsigned hex);
#ifndef NO_RUNTIME_LOG
static void log_cliff_locked(unsigned hex);
#endif
// UI/save host caller only. No game pointer crosses into the renderer.
static void mark(unsigned hex,unsigned terrain,unsigned original_terrain) {
    if(hex>=HexCount || !active())return;
#ifdef SAN14_TERRAIN_TREES_INCLUDED
    terrain_trees::mark(hex,terrain,original_terrain);
#endif
    int goal=terrain==original_terrain?RestoreOriginal:material(terrain);
    if(goal<-1)return;
    AcquireSRWLockExclusive(&ctrl_lock);
    // before_world_change precedes terrain_edit's CPU rollback. Those original
    // notifications must not erase the next-load targets requested by R2b.
    if(!render_failed && !(world_transition && goal==RestoreOriginal)) {
        goals[hex]=static_cast<signed char>(goal);has_goal[hex]=true;
        height_delta[hex]=height_change(terrain,original_terrain);
        carve_delta[hex]=terrain==original_terrain?0.0f:carve(terrain)-carve(original_terrain);
        cliff[hex]=terrain==20 && original_terrain!=20;
#ifndef NO_RUNTIME_LOG
        if(cliff[hex])log_cliff_locked(hex);
#endif
        cpu_cell_locked(hex);
        enqueue_locked(hex);
        refresh_cliffs_locked(hex);
        publish_pending_locked();
    }
    ReleaseSRWLockExclusive(&ctrl_lock);
}
// Invoked only by terrain_edit's existing UI/load-host paths. First queue raw
// asset restoration of old targets; the current sidecar list overrides these.
static bool take_ui_work(bool &install_present) {
    install_present=false;
    AcquireSRWLockExclusive(&ctrl_lock);
    bool resync=ctrl_texture && !render_failed && resync_needed;
    world_transition=false;
    if(resync) {
        resync_needed=false;clear_queue_locked();
        for(unsigned hex=0;hex<HexCount;++hex)if(has_goal[hex]) {
            goals[hex]=RestoreOriginal;has_goal[hex]=false;height_delta[hex]=0.0f;carve_delta[hex]=0.0f;cliff[hex]=false;cpu_cell_locked(hex);enqueue_locked(hex);
        }
        publish_pending_locked();
    }
    if(ctrl_texture && !render_failed && !present_install_requested) {
        present_install_requested=true;install_present=true;
    }
    ReleaseSRWLockExclusive(&ctrl_lock);
    return resync;
}
static bool control_desc(const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial) {
    return d && d->Width==CtrlSize && d->Height==CtrlSize && d->Format==DXGI_FORMAT_B8G8R8A8_TYPELESS &&
        d->MipLevels==1 && d->ArraySize==1 && d->Usage==D3D11_USAGE_IMMUTABLE &&
        d->BindFlags==D3D11_BIND_SHADER_RESOURCE && !d->CPUAccessFlags && !d->MiscFlags &&
        d->SampleDesc.Count==1 && !d->SampleDesc.Quality &&
        initial && initial[0].pSysMem && initial[0].SysMemPitch>=CtrlPitch;
}
static bool work_desc(const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial) {
    return d && d->Width==CtrlSize && d->Height==CtrlSize && d->Format==DXGI_FORMAT_R8G8B8A8_TYPELESS &&
        d->MipLevels==1 && d->ArraySize==1 && d->Usage==D3D11_USAGE_DEFAULT &&
        d->BindFlags==(D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET|D3D11_BIND_UNORDERED_ACCESS) &&
        !d->CPUAccessFlags && d->SampleDesc.Count==1 && !(initial && initial[0].pSysMem);
}
static bool height_desc(const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial) {
    return d && d->Width==CtrlSize && d->Height==CtrlSize && d->Format==DXGI_FORMAT_R32_TYPELESS &&
        d->MipLevels==1 && d->ArraySize==1 && d->Usage==D3D11_USAGE_IMMUTABLE &&
        d->BindFlags==D3D11_BIND_SHADER_RESOURCE && !d->CPUAccessFlags && !d->MiscFlags &&
        d->SampleDesc.Count==1 && initial && initial[0].pSysMem && initial[0].SysMemPitch>=CtrlPitch;
}
static bool height_work_desc(const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial) {
    return d && d->Width==CtrlSize && d->Height==CtrlSize && d->Format==DXGI_FORMAT_R32_TYPELESS &&
        d->MipLevels==1 && d->ArraySize==1 && d->Usage==D3D11_USAGE_DEFAULT &&
        d->BindFlags==(D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET|D3D11_BIND_UNORDERED_ACCESS) &&
        !d->CPUAccessFlags && d->SampleDesc.Count==1 && !(initial && initial[0].pSysMem);
}
static bool mask_desc(const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial) {
    return d && d->Width==MaskSize && d->Height==MaskSize && d->Format==DXGI_FORMAT_R32_TYPELESS &&
        d->MipLevels==1 && d->ArraySize==1 && d->Usage==D3D11_USAGE_IMMUTABLE &&
        d->BindFlags==D3D11_BIND_SHADER_RESOURCE && !d->CPUAccessFlags && !d->MiscFlags &&
        d->SampleDesc.Count==1 && !d->SampleDesc.Quality &&
        initial && initial[0].pSysMem && initial[0].SysMemPitch>=MaskPitch;
}
static bool mask_work_desc(const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial) {
    return d && d->Width==MaskSize && d->Height==MaskSize &&
        (d->Format==DXGI_FORMAT_R32_TYPELESS || d->Format==DXGI_FORMAT_R32_FLOAT) &&
        d->MipLevels==1 && d->ArraySize==1 && d->Usage==D3D11_USAGE_DEFAULT &&
        (d->BindFlags&D3D11_BIND_SHADER_RESOURCE) &&
        (d->BindFlags&(D3D11_BIND_RENDER_TARGET|D3D11_BIND_UNORDERED_ACCESS)) &&
        !d->CPUAccessFlags && !d->MiscFlags && d->SampleDesc.Count==1 && !d->SampleDesc.Quality &&
        !(initial && initial[0].pSysMem);
}
static float mask_median(std::vector<float> &values) {
    auto middle=values.begin()+values.size()/2;
    std::nth_element(values.begin(),middle,values.end());return *middle;
}
// local55 (user 2026-10-29): ground-space errors, independent of shader UV zw.
struct MaskFit {
    float score=1e30f,median=1e30f,tail=1e30f,scale=1,spread=0;
    int dx=0,dz=0;unsigned model=0;float inliers=0;
};
static float sample_ground(const std::vector<float> &data,int x,int z,unsigned model) {
    size_t at=static_cast<size_t>(z)*CtrlSize+x;
    if(model!=2)return data[at];
    return (data[at]+data[at+1]+data[at+CtrlSize]+data[at+CtrlSize+1])*0.25f;
}
// User 2026-10-29 local56: water hexes are carved 900-3300 below the ground,
// so fit on inliers (|mask-ground|<200) only. The 4856 dump gave offset (+1,+1),
// inlier median 15 and 90% 118, with about 85% of samples inside.
static bool mask_acceptable(float median,float tail,float spread,float inliers) {
    return spread>=50 && inliers>=0.6f && median<=40.0f && tail<=200.0f;
}
static void use_mask_fit(const MaskFit &fit) {
    mask_offset_x=fit.dx;mask_offset_z=fit.dz;mask_scale=fit.scale;mask_model=fit.model;
}
static bool fit_mask_locked(WaterMask &m,const float *uv,unsigned &error,MaskFit *result=nullptr) {
    TERRAIN_SCOPE(MaskFit);
    (void)uv;error=0xffffffffu;
    if(original_height.size()!=static_cast<size_t>(CtrlSize)*CtrlSize ||
       m.original.size()!=static_cast<size_t>(MaskSize)*MaskSize)return false;
    MaskFit best;
    for(unsigned model=0;model<1;++model) {
        MaskFit model_best;model_best.model=model;
        for(int dz=-8;dz<=8;++dz)for(int dx=-8;dx<=8;++dx) {
            std::vector<float> ratios,errors,grounds;
            ratios.reserve(324);errors.reserve(324);grounds.reserve(324);
            for(unsigned z=16;z<MaskSize-16;z+=113)for(unsigned x=16;x<MaskSize-16;x+=113) {
                float ground=sample_ground(original_height,static_cast<int>(x)+dx,static_cast<int>(z)+dz,model);
                float value=m.original[static_cast<size_t>(z)*MaskSize+x];
                if(!std::isfinite(ground) || !std::isfinite(value))return false;
                grounds.push_back(ground);
                if(std::fabs(ground)>50 && std::fabs(value-ground)<200)ratios.push_back(value/ground);
            }
            if(ratios.size()<150)continue;
            float scale=mask_median(ratios);
            if(!std::isfinite(scale) || scale<0.01f || scale>100)continue;
            if(std::fabs(scale-1)<0.0001f)scale=1;
            size_t i=0;
            for(unsigned z=16;z<MaskSize-16;z+=113)for(unsigned x=16;x<MaskSize-16;x+=113) {
                float e=std::fabs(m.original[static_cast<size_t>(z)*MaskSize+x]/scale-grounds[i++]);
                if(e<200)errors.push_back(e);
            }
            if(errors.size()<150)continue;
            float inliers=static_cast<float>(errors.size())/static_cast<float>(grounds.size());
            float median=mask_median(errors);
            std::sort(errors.begin(),errors.end());float tail=errors[errors.size()*9/10];
            float score=median+tail*0.25f;
            if(score<model_best.score) {
                auto bounds=std::minmax_element(grounds.begin(),grounds.end());
                model_best={score,median,tail,scale,*bounds.second-*bounds.first,dx,dz,model,inliers};
            }
        }
#ifndef NO_RUNTIME_LOG
        if(model_best.score<1e29f)log_event("water_mask_fit2",model*1000+(model_best.dx+16)*32+(model_best.dz+16),
            static_cast<unsigned>(model_best.median*10),static_cast<unsigned>(model_best.tail*10));
#endif
        if(model_best.score<best.score)best=model_best;
    }
    if(best.score>=1e29f)return false;
    error=static_cast<unsigned>(best.median);use_mask_fit(best);
    if(result)*result=best;
    return mask_acceptable(best.median,best.tail,best.spread,best.inliers);
}
static void queue_mask_targets_locked() {
    for(unsigned hex=0;hex<HexCount;++hex)if(has_goal[hex])enqueue_locked(hex);
    publish_pending_locked();
}
#ifndef NO_RUNTIME_LOG
static bool mask_dump_attempted; // Once per process, even if the fit is rejected.
static bool write_mask_raw(const wchar_t *directory,const wchar_t *name,const std::vector<float> &data) {
    wchar_t path[MAX_PATH]={};
    if(swprintf_s(path,L"%s\\%s_%lu.bin",directory,name,GetCurrentProcessId())<0)return false;
    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0,bytes=static_cast<DWORD>(data.size()*sizeof(float));
    bool ok=WriteFile(file,data.data(),bytes,&written,nullptr) && written==bytes;
    CloseHandle(file);return ok;
}
static void dump_mask_locked(const WaterMask &m) {
    if(mask_dump_attempted || original_height.empty())return;
    mask_dump_attempted=true;
    // One synchronous pair on the confirming render caller. No worker can race
    // DLL unload or retain live game pointers; these are untouched CPU originals.
    wchar_t directory[MAX_PATH]={};
#ifdef TERRAIN_MASK_DUMP_FIXTURE
    GetCurrentDirectoryW(MAX_PATH,directory);
#else
    DWORD length=GetModuleFileNameW(nullptr,directory,MAX_PATH);
    if(!length || length>=MAX_PATH)return;
    wchar_t *slash=wcsrchr(directory,L'\\');if(!slash)return;*slash=0;
    if(wcscat_s(directory,L"\\AI_TacticsMod"))return;
    if(wcscat_s(directory,L"\\data"))return;CreateDirectoryW(directory,nullptr);
    if(wcscat_s(directory,L"\\performance"))return;CreateDirectoryW(directory,nullptr);
#endif
    bool mask_ok=write_mask_raw(directory,L"water_mask",m.original);
    bool ground_ok=write_mask_raw(directory,L"ground_height",original_height);
    log_event("water_mask_dump",mask_ok?1:0,ground_ok?1:0,GetCurrentProcessId());
}
#endif
// Optional water_diag callback ([diagnostics] water_diag=1): reports whether the
// bound VS t0 is the fitted mask. Selection belongs to the fit (local56).
static void confirm_mask(ID3D11Texture2D *texture,const float *uv) {
#ifndef NO_RUNTIME_LOG
    if(!active())return;
    AcquireSRWLockShared(&ctrl_lock);
    int bound=-1;for(unsigned i=0;i<MaxMasks;++i)if(texture && water_masks[i].texture==texture)bound=static_cast<int>(i);
    bool selected=bound>=0 && bound==mask_selected;
    ReleaseSRWLockShared(&ctrl_lock);
    unsigned uv_bits[4]={};if(uv)memcpy(uv_bits,uv,sizeof(uv_bits));
    static volatile LONG reported;
    if(!InterlockedExchange(&reported,1))log_event("water_mask_bound",bound>=0?1:0,selected?1:0,uv_bits[2]);
#else
    (void)texture;(void)uv;
#endif
}
// local56: the fit alone selects the mask in every build (water_diag is off by default).
static void restore_mask_writes_locked(ID3D11DeviceContext *context) {
    if(mask_selected<0)return;
    auto &m=water_masks[mask_selected];
    for(unsigned z=0;z<MaskSize;++z)for(unsigned x=0;x<MaskSize;) {
        size_t at=static_cast<size_t>(z)*MaskSize+x;
        if(!memcmp(&m.shadow[at],&m.original[at],4)){++x;continue;}
        unsigned start=x;
        while(x<MaskSize && memcmp(&m.shadow[static_cast<size_t>(z)*MaskSize+x],&m.original[static_cast<size_t>(z)*MaskSize+x],4))++x;
        at=static_cast<size_t>(z)*MaskSize+start;
        memcpy(m.shadow.data()+at,m.original.data()+at,(x-start)*4);
        D3D11_BOX row={start,z,0,x,z+1,1};
        profile_upload(context,m.texture,0,&row,m.original.data()+at,(x-start)*4,0);
        for(unsigned i=0;i<mask_work_count;++i)profile_upload(context,mask_work[i],0,&row,m.original.data()+at,(x-start)*4,0);
    }
}
static void confirm_release_masks_locked(ID3D11DeviceContext *context) {
    if(mask_fit_attempted || original_height.empty() || !mask_count || world_transition || render_failed)return;
    mask_fit_attempted=true;unsigned accepted=0;int selected=-1;MaskFit chosen;
    if(!mask_overflow)try {
        for(unsigned i=0;i<mask_count;++i)if(water_masks[i].device==height_device) {
            unsigned error=0;MaskFit fit;
            if(fit_mask_locked(water_masks[i],nullptr,error,&fit)){++accepted;selected=static_cast<int>(i);chosen=fit;}
        }
    } catch(...) {accepted=0;}
    // A load creates the mask twice (live local53/55: the shader bound the later
    // one). Prefer the newest passing candidate; an older one keeps native bytes.
    if(mask_selected>=0 && (!accepted || mask_selected!=selected))restore_mask_writes_locked(context);
    mask_selected=accepted?selected:-1;mask_ready=accepted>0;
    if(mask_ready){use_mask_fit(chosen);queue_mask_targets_locked();}
#ifndef NO_RUNTIME_LOG
    log_event("water_mask_fit3",mask_offset_x+16,mask_offset_z+16,static_cast<unsigned>(chosen.median*10));
    log_event("water_mask_ready",mask_ready?1:0,accepted,static_cast<unsigned>(chosen.inliers*100));
    if(mask_selected>=0)dump_mask_locked(water_masks[mask_selected]);
#endif
}
// Asset B,G,R,A bytes -> work-texture R,G,B,A bytes (shader .x stays the material).
static constexpr unsigned to_rgba(unsigned bgra) {
    return (bgra&0xff00ff00u)|((bgra>>16)&0xffu)|((bgra&0xffu)<<16);
}
static_assert(to_rgba(0xff05ff00u)==0xff00ff05u && to_rgba(0xff06ff1au)==0xff1aff06u,"pure material 5 -> R=5; blend byte moves to B");
struct Center {double x,z;};
static Center center(unsigned hex) {
    unsigned row=hex/220,col=hex%220;
    return {37594.0+col*702.0,13335.3+row*RowHeight+((col%2==0)?RowHeight/2:0)};
}
static D3D11_BOX pixel_box(int px,int pz) {
    auto clip=[](int value) {return static_cast<UINT>(value<0?0:value>static_cast<int>(CtrlSize)?CtrlSize:value);};
    return {clip(px-6),clip(pz-6),0,clip(px+7),clip(pz+7),1};
}
static D3D11_BOX hex_box(unsigned hex) {
    Center c=center(hex);
    return pixel_box(static_cast<int>(std::floor(c.x/100+0.5)),static_cast<int>(std::floor(c.z/100+0.5)));
}
static bool owns_point(unsigned hex,double px,double pz) {
    Center c=center(hex);double x=px*100.0,z=pz*100.0;
    double nearest=(x-c.x)*(x-c.x)+(z-c.z)*(z-c.z);
    int row=static_cast<int>(hex/220),col=static_cast<int>(hex%220);
    for(int r=row-1;r<=row+1;++r)for(int k=col-1;k<=col+1;++k) {
        if(r<0 || r>=220 || k<0 || k>=220)continue;
        unsigned neighbour_hex=static_cast<unsigned>(r*220+k);if(neighbour_hex==hex)continue;
        Center n=center(neighbour_hex);double distance=(x-n.x)*(x-n.x)+(z-n.z)*(z-n.z);
        if(distance<nearest || (distance==nearest && neighbour_hex<hex))return false;
    }
    return true;
}
static bool owns_pixel(unsigned hex,unsigned px,unsigned pz) {return owns_point(hex,px,pz);}
static unsigned write_cell(std::vector<unsigned> &pixels,const std::vector<unsigned> &original,unsigned hex,int goal) {
    D3D11_BOX box=hex_box(hex);unsigned written=0;
    for(unsigned z=box.top;z<box.bottom;++z)for(unsigned x=box.left;x<box.right;++x)if(owns_pixel(hex,x,z)) {
        size_t at=static_cast<size_t>(z)*CtrlSize+x;
        pixels[at]=goal==RestoreOriginal?original[at]:0xff00ff00u|(static_cast<unsigned>(goal)<<16);++written;
    }
    return written;
}
// Work textures may hold engine-composited pixels, so only the cell's own
// pixels are written, one row run at a time.
static void mirror_cell_locked(ID3D11DeviceContext *context,unsigned hex) {
    if(!ctrl_work_count)return;
    D3D11_BOX box=hex_box(hex);unsigned run[16];
    for(unsigned z=box.top;z<box.bottom;++z) {
        for(unsigned x=box.left;x<box.right;) {
            if(!owns_pixel(hex,x,z)){++x;continue;}
            unsigned start=x,count=0;
            while(x<box.right && count<16 && owns_pixel(hex,x,z))run[count++]=to_rgba(shadow_ctrl[static_cast<size_t>(z)*CtrlSize+x++]);
            D3D11_BOX row={start,z,0,start+count,z+1,1};
            for(unsigned i=0;i<ctrl_work_count;++i)profile_upload(context,ctrl_work[i],0,&row,run,count*4,0);
        }
    }
}
// User 2026-10-29: per-cell bells left valleys between adjacent mountains. The
// height offset is now one smooth field interpolated from the cell deltas (normalised
// cubic kernel): a centre keeps its own delta, shared edges take the average, so
// edited neighbours form one ridge and an all-zero neighbourhood is the asset.
static float field_at(const float *values,unsigned px,unsigned pz) {
    double x=px*100.0,z=pz*100.0;
    int col=static_cast<int>(std::floor((x-37594.0)/702.0+0.5));
    int row=static_cast<int>(std::floor((z-13335.3-((col%2==0)?RowHeight/2:0))/RowHeight+0.5));
    double weight=0,sum=0;
    for(int r=row-1;r<=row+1;++r)for(int k=col-1;k<=col+1;++k) {
        if(r<0 || r>=220 || k<0 || k>=220)continue;
        unsigned hex=static_cast<unsigned>(r*220+k);Center c=center(hex);
        double d=std::sqrt((x-c.x)*(x-c.x)+(z-c.z)*(z-c.z))/600.0;
        if(d>=1.0)continue;
        double w=(1.0-d)*(1.0-d)*(1.0-d);weight+=w;sum+=w*values[hex];
    }
    return weight>0?static_cast<float>(sum/weight):0.0f;
}
static float delta_at(unsigned px,unsigned pz) {return field_at(height_delta,px,pz);}
// User 2026-10-29 local48: only the owning hex receives a cliff profile. Match
// owns_pixel's nearest-centre rule, including its lower-hex tie break.
static unsigned pixel_owner(double px,double pz) {
    double x=px*100.0,z=pz*100.0;
    int col=static_cast<int>(std::floor((x-37594.0)/702.0+0.5));
    int row=static_cast<int>(std::floor((z-13335.3-((col%2==0)?RowHeight/2:0))/RowHeight+0.5));
    unsigned owner=HexCount;double nearest=0;
    for(int r=row-1;r<=row+1;++r)for(int k=col-1;k<=col+1;++k) {
        if(r<0 || r>=220 || k<0 || k>=220)continue;
        unsigned hex=static_cast<unsigned>(r*220+k);Center c=center(hex);
        double d=(x-c.x)*(x-c.x)+(z-c.z)*(z-c.z);
        if(owner==HexCount || d<nearest || (d==nearest && hex<owner)){owner=hex;nearest=d;}
    }
    return owner;
}
static unsigned center_pixel(double value) {
    int p=static_cast<int>(std::floor(value/100.0+0.5));
    return static_cast<unsigned>(p<0?0:p>=static_cast<int>(CtrlSize)?CtrlSize-1:p);
}
struct CliffSection {float lo,hi;Center low,high;};
static CliffSection cliff_section(unsigned hex) {
    Center c=center(hex);CliffSection section={0,0,c,c};bool found=false;
    int row=static_cast<int>(hex/220),col=static_cast<int>(hex%220);
    // Even columns are shifted down by half a row; diagonal neighbours are
    // row/row+1 there, row-1/row in odd columns. No row wrapping at map edges.
    int side=(col%2==0)?1:-1;
    const int offsets[6][2]={{-1,0},{1,0},{0,-1},{side,-1},{0,1},{side,1}};
    for(const auto &offset:offsets) {
        int r=row+offset[0],k=col+offset[1];
        if(r<0 || r>=220 || k<0 || k>=220)continue;
        Center n=center(static_cast<unsigned>(r*220+k));
        unsigned x=center_pixel(n.x),z=center_pixel(n.z);
        float h=original_height[static_cast<size_t>(z)*CtrlSize+x]+delta_at(x,z);
        if(!found || h<section.lo){section.lo=h;section.low=n;}
        if(!found || h>section.hi){section.hi=h;section.high=n;}
        found=true;
    }
    return section;
}
static float height_at(unsigned px,unsigned pz) {
    size_t at=static_cast<size_t>(pz)*CtrlSize+px;
    float base=original_height[at]+delta_at(px,pz);
    unsigned hex=pixel_owner(px,pz);
    if(hex==HexCount || !cliff[hex])return base;
    Center c=center(hex);CliffSection section=cliff_section(hex);
    double x=px*100.0-c.x,z=pz*100.0-c.z;
    if(section.hi-section.lo>=150.0f) {
        double dx=section.high.x-section.low.x,dz=section.high.z-section.low.z;
        double t=(x*dx+z*dz)/(std::sqrt(dx*dx+dz*dz)*405.0);
        t=t<-1?-1:t>1?1:t;
        double s0=1.0/(1.0+std::exp(7.0)),s1=1.0-s0;
        double w=(1.0/(1.0+std::exp(-7.0*t))-s0)/(s1-s0);
        float original_center=original_height[static_cast<size_t>(center_pixel(c.z))*CtrlSize+center_pixel(c.x)];
        return static_cast<float>(section.lo+(section.hi-section.lo)*w+(original_height[at]-original_center)*0.3);
    }
    double r=std::sqrt(x*x+z*z)/468.0,w=(0.82-r)/0.12;
    w=w<0?0:w>1?1:w;
    return base+static_cast<float>(400.0*w);
}
static void refresh_cliffs_locked(unsigned hex) {
    // A neighbour's +/-11 box can miss the opposite edge of a cliff. Its six
    // samples also include nearby deltas, so refresh cliff owners up to two
    // rows/columns away rather than leaving stale CPU/GPU profile pixels.
    int row=static_cast<int>(hex/220),col=static_cast<int>(hex%220);
    for(int r=row-2;r<=row+2;++r)for(int k=col-2;k<=col+2;++k) {
        if(r<0 || r>=220 || k<0 || k>=220)continue;
        unsigned neighbour=static_cast<unsigned>(r*220+k);
        if(neighbour==hex || !cliff[neighbour])continue;
        cpu_cell_locked(neighbour);enqueue_locked(neighbour);
    }
}
#ifndef NO_RUNTIME_LOG
static void log_cliff_locked(unsigned hex) {
    if(original_height.empty())return;
    CliffSection section=cliff_section(hex);float range=section.hi-section.lo;
    log_event("terrain_cliff",hex,static_cast<unsigned>(range),range>=150.0f?1:2);
}
#endif
// User 2026-10-29: units sank into raised cells. They stand on the CPU height
// collision objects that 0xBC2380 builds from `mhk` (same 2049² grid and values as
// the GPU heightmap, u32 normalised to [+0x50,+0x54]). Pointers arrive from the
// creation call sites in native_split_install.inl; we never call the game here.
static constexpr unsigned MaxHeightfields=2;
static unsigned char *heightfields[MaxHeightfields];
static const void *heightfield_vtable;
static std::vector<unsigned char> cpu_touched;
static bool heightfield_ok(const unsigned char *o) {
    return o && heightfield_vtable && *reinterpret_cast<const void *const *>(o)==heightfield_vtable &&
        !memcmp(o+0x38,"_MHK0100",8) && *reinterpret_cast<const unsigned short *>(o+0x48)==CtrlSize-1 &&
        *reinterpret_cast<const unsigned short *>(o+0x4A)==CtrlSize-1;
}
// UI/load threads, ctrl_lock held exclusively. Offsets only where we changed heights,
// so engine-side CPU edits elsewhere stay intact.
static void cpu_cell_locked(unsigned hex) {
    TERRAIN_SCOPE(CpuCell);
    if(original_height.empty())return;
    if(cpu_touched.empty()) {try {cpu_touched.assign(static_cast<size_t>(CtrlSize)*CtrlSize,0);} catch(...) {return;}}
    Center c=center(hex);int px=static_cast<int>(std::floor(c.x/100+0.5)),pz=static_cast<int>(std::floor(c.z/100+0.5));
    auto clip=[](int value) {return static_cast<unsigned>(value<0?0:value>static_cast<int>(CtrlSize)?CtrlSize:value);};
    unsigned left=clip(px-11),top=clip(pz-11),right=clip(px+12),bottom=clip(pz+12);
    for(auto &o:heightfields) {
        if(o && !heightfield_ok(o))o=nullptr;
        if(!o)continue;
        float low=*reinterpret_cast<const float *>(o+0x50),high=*reinterpret_cast<const float *>(o+0x54);
        if(!(high>low))continue;
        auto data=reinterpret_cast<unsigned *>(o+0x58);
        for(unsigned z=top;z<bottom;++z)for(unsigned x=left;x<right;++x) {
            size_t at=static_cast<size_t>(z)*CtrlSize+x;float height=height_at(x,z);
            if(height==original_height[at] && !cpu_touched[at])continue;
            double t=(height-low)/(high-low);t=t<0?0:t>1?1:t;
            data[at]=static_cast<unsigned>(t*4294967295.0+0.5);
        }
    }
    for(unsigned z=top;z<bottom;++z)for(unsigned x=left;x<right;++x)
        cpu_touched[static_cast<size_t>(z)*CtrlSize+x]=height_at(x,z)!=original_height[static_cast<size_t>(z)*CtrlSize+x];
}
// Called on the loading thread right after A07B70 returns, before the owner publishes it.
static void heightfield_created(unsigned index,unsigned char *object,const void *vtable) {
    TERRAIN_SCOPE(HeightCapture);
    if(index>=MaxHeightfields || !active())return;
    AcquireSRWLockExclusive(&ctrl_lock);
    heightfield_vtable=vtable;heightfields[index]=heightfield_ok(object)?object:nullptr;
    bool ok=heightfields[index]!=nullptr;
    if(ok) {
        // A fresh object holds asset heights: reapply every kept offset.
        unsigned char *others[MaxHeightfields];memcpy(others,heightfields,sizeof(others));
        for(unsigned i=0;i<MaxHeightfields;++i)if(i!=index)heightfields[i]=nullptr;
        if(!cpu_touched.empty())memset(cpu_touched.data(),0,cpu_touched.size());
        for(unsigned hex=0;hex<HexCount;++hex)if(has_goal[hex] && (height_delta[hex]!=0.0f || cliff[hex]))cpu_cell_locked(hex);
        memcpy(heightfields,others,sizeof(others));
    }
    ReleaseSRWLockExclusive(&ctrl_lock);
#ifndef NO_RUNTIME_LOG
    log_event("terrain_height_cpu",index,ok?1:0,original_height.empty()?0:1);
#endif
}
// The mask is not a copy of the ground (about 15 units apart even on land), so
// write the original mask plus our ground offset and the terrain carve change.
// A zero offset writes the original bits, so restores are exact.
static unsigned mask_cell_locked(ID3D11DeviceContext *context,const D3D11_BOX &ground_box) {
    if(!mask_ready || mask_selected<0 || world_transition || render_failed)return 0;
    WaterMask &m=water_masks[mask_selected];
    if(!m.texture || m.device!=height_device)return 0;
    unsigned pixels=0;float run[24];
    int left=(std::max)(0,static_cast<int>(ground_box.left)-mask_offset_x);
    int top=(std::max)(0,static_cast<int>(ground_box.top)-mask_offset_z);
    int right=(std::min)(static_cast<int>(MaskSize),static_cast<int>(ground_box.right)-mask_offset_x);
    int bottom=(std::min)(static_cast<int>(MaskSize),static_cast<int>(ground_box.bottom)-mask_offset_z);
    for(int z=top;z<bottom;++z) {
        for(int x=left;x<right;) {
            unsigned start=static_cast<unsigned>(x),count=0;
            while(x<right && count<24) {
                int gx=x+mask_offset_x,gz=z+mask_offset_z;
                if(gx<0 || gz<0 || gx>=static_cast<int>(CtrlSize) || gz>=static_cast<int>(CtrlSize)){if(count)break;++x;start=static_cast<unsigned>(x);continue;}
                size_t at=static_cast<size_t>(z)*MaskSize+x;
                float offset=height_at(static_cast<unsigned>(gx),static_cast<unsigned>(gz))-original_height[static_cast<size_t>(gz)*CtrlSize+gx]+
                    field_at(carve_delta,static_cast<unsigned>(gx),static_cast<unsigned>(gz));
                float value=offset!=0.0f?m.original[at]+offset*mask_scale:m.original[at];
                if(!memcmp(&value,&m.shadow[at],4) && (!mask_mirror_pending || !memcmp(&value,&m.original[at],4))){if(count)break;++x;start=static_cast<unsigned>(x);continue;}
                memcpy(&m.shadow[at],&value,4);memcpy(run+count,&value,4);++count;++x;
            }
            if(!count)continue;
            D3D11_BOX row={start,static_cast<UINT>(z),0,start+count,static_cast<UINT>(z)+1,1};
            profile_upload(context,m.texture,0,&row,run,count*4,0);
            for(unsigned i=0;i<mask_work_count;++i)profile_upload(context,mask_work[i],0,&row,run,count*4,0);
            pixels+=count;
        }
    }
    return pixels;
}
static unsigned height_cell_locked(ID3D11DeviceContext *context,unsigned hex) {
    TERRAIN_SCOPE(HeightCell);
    confirm_release_masks_locked(context);
    if(!height_texture || shadow_height.empty())return 0;
    // The field reaches about one neighbour beyond the edited cell.
    Center c=center(hex);int px=static_cast<int>(std::floor(c.x/100+0.5)),pz=static_cast<int>(std::floor(c.z/100+0.5));
    auto clip=[](int value) {return static_cast<unsigned>(value<0?0:value>static_cast<int>(CtrlSize)?CtrlSize:value);};
    D3D11_BOX box={clip(px-11),clip(pz-11),0,clip(px+12),clip(pz+12),1};
    static bool touched[23*23];float run[23];unsigned width=box.right-box.left;
    for(unsigned z=box.top;z<box.bottom;++z)for(unsigned x=box.left;x<box.right;++x) {
        size_t at=static_cast<size_t>(z)*CtrlSize+x;float before=shadow_height[at];
        shadow_height[at]=height_at(x,z);
        // Work textures may carry engine edits: write only pixels we offset now or before.
        touched[(z-box.top)*width+(x-box.left)]=before!=original_height[at] || shadow_height[at]!=original_height[at];
    }
    profile_upload(context,height_texture,0,&box,shadow_height.data()+static_cast<size_t>(box.top)*CtrlSize+box.left,CtrlPitch,0);
    if(!height_work_count)return mask_cell_locked(context,box);
    for(unsigned z=box.top;z<box.bottom;++z) {
        for(unsigned x=box.left;x<box.right;) {
            if(!touched[(z-box.top)*width+(x-box.left)]){++x;continue;}
            unsigned start=x,count=0;
            while(x<box.right && touched[(z-box.top)*width+(x-box.left)])run[count++]=shadow_height[static_cast<size_t>(z)*CtrlSize+x++];
            D3D11_BOX row={start,z,0,start+count,z+1,1};
            for(unsigned i=0;i<height_work_count;++i)profile_upload(context,height_work[i],0,&row,run,count*4,0);
        }
    }
    return mask_cell_locked(context,box);
}
#ifndef NO_RUNTIME_LOG
// Diagnostic only, one synchronous pixel per uploading Present. Keeping Map's
// flags at zero avoids a pending readback extending the dirty-only Present gate.
static void readback_locked(ID3D11DeviceContext *context,unsigned hex) {
    TERRAIN_SCOPE(Readback);
    if(readback_failed || !ctrl_create)return;
    if(!readback_texture) {
        D3D11_TEXTURE2D_DESC d={};d.Width=d.Height=d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_B8G8R8A8_TYPELESS;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        // Use the saved system implementation, avoiding slot_lock re-entry
        // while ctrl_lock is held (restore acquires slot_lock then ctrl_lock).
        terrain_profile::Function<CreateTexture2D,terrain_profile::Id::TextureNative> native_create{ctrl_create};
        HRESULT hr=native_create(ctrl_device,&d,nullptr,&readback_texture);
        if(FAILED(hr) || !readback_texture) {
            readback_failed=true;log_event("terrain_render_readback_error",static_cast<unsigned>(hr),1,0);return;
        }
    }
    Center c=center(hex);unsigned x=static_cast<unsigned>(std::floor(c.x/100+0.5)),z=static_cast<unsigned>(std::floor(c.z/100+0.5));
    D3D11_BOX pixel={x,z,0,x+1,z+1,1};
    unsigned expected=shadow_ctrl[static_cast<size_t>(z)*CtrlSize+x];
    {TERRAIN_SCOPE(CopyNative);context->CopySubresourceRegion(readback_texture,0,0,0,0,ctrl_texture,0,&pixel);}
    D3D11_MAPPED_SUBRESOURCE mapped={};HRESULT hr;
    {TERRAIN_SCOPE(MapNative);hr=context->Map(readback_texture,0,D3D11_MAP_READ,0,&mapped);}
    if(SUCCEEDED(hr)) {
        unsigned value=0;memcpy(&value,mapped.pData,sizeof(value));context->Unmap(readback_texture,0);
        log_event("terrain_render_readback",hex,value,expected);
    } else {readback_failed=true;log_event("terrain_render_readback_error",static_cast<unsigned>(hr),2,0);}
}
#endif
static void on_present(ID3D11DeviceContext *context) {
    TERRAIN_SCOPE(RenderUpload);
    if(!context || !active())return;
#ifndef NO_RUNTIME_LOG
    if(InterlockedCompareExchange(&present_reported,1,0)==0)log_event("terrain_render_present",1,0,0);
#endif
    if(!needs_present())return;
    unsigned cells=0,pixels=0,mask_cells=0,mask_pixels=0;
    AcquireSRWLockExclusive(&ctrl_lock);
    if(ctrl_texture && !render_failed) {
        while(dirty_count && cells<FrameLimit) {
            unsigned hex=dirty_hex[dirty_head];dirty_head=(dirty_head+1)%HexCount;--dirty_count;queued[hex]=false;
            D3D11_BOX box=hex_box(hex);unsigned written=write_cell(shadow_ctrl,original_ctrl,hex,goals[hex]);
            if(written) {
                profile_upload(context,ctrl_texture,0,&box,shadow_ctrl.data()+static_cast<size_t>(box.top)*CtrlSize+box.left,CtrlPitch,0);
                mirror_cell_locked(context,hex);
                unsigned updated=height_cell_locked(context,hex);
                if(updated){++mask_cells;mask_pixels+=updated;}
            }
#ifndef NO_RUNTIME_LOG
            if(written && cells==0)readback_locked(context,hex);
#endif
            ++cells;pixels+=written;
        }
    }
    if(!dirty_count)mask_mirror_pending=false;
    publish_pending_locked();ReleaseSRWLockExclusive(&ctrl_lock);
#ifndef NO_RUNTIME_LOG
    if(cells)log_event("terrain_render_upload",cells,pixels,0);
    if(mask_cells)log_event("water_mask_upload",mask_cells,mask_pixels,0);
#else
    (void)pixels;(void)mask_cells;(void)mask_pixels;
#endif
}
// D3D11 is supplied by Windows, not by an EXE RVA. Admit only its executable
// implementation and its own immutable vtable; retain and recheck 16 original
// bytes before publication. A foreign proxy/slot disables only this observer.
static bool original_site(void **table,CreateTexture2D original) {
#ifdef TERRAIN_RENDER_FIXTURE
    return terrain_render_fixture_site(table,reinterpret_cast<void *>(original));
#else
    HMODULE code=nullptr,data=nullptr;
    auto address=reinterpret_cast<const wchar_t *>(original);
    if(!original || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,address,&code) ||
       !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const wchar_t *>(table),&data) ||
       data!=code)return false;
    // User 2026-10-29: a game-folder forwarding proxy can own the basename.
    // Validate the actual code/table module against the system DLL's full path.
    wchar_t path[MAX_PATH]={},system[MAX_PATH]={};
    DWORD length=GetModuleFileNameW(code,path,MAX_PATH);
    UINT system_length=GetSystemDirectoryW(system,MAX_PATH);
    constexpr wchar_t suffix[]=L"\\d3d11.dll";
    if(!length || length>=MAX_PATH || !system_length ||
       system_length+sizeof(suffix)/sizeof(*suffix)>MAX_PATH ||
       wcscat_s(system,MAX_PATH,suffix) || _wcsicmp(path,system))return false;
    MEMORY_BASIC_INFORMATION info={};
    if(!VirtualQuery(reinterpret_cast<void *>(original),&info,sizeof(info)) || info.State!=MEM_COMMIT ||
       (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    return (info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;
#endif
}
static HRESULT STDMETHODCALLTYPE create_texture_2d(ID3D11Device *dev,const D3D11_TEXTURE2D_DESC *desc,
    const D3D11_SUBRESOURCE_DATA *initial,ID3D11Texture2D **out) { COST_SCOPE("terrain_render::create_texture_2d");
    TERRAIN_SCOPE(TextureHook);
    terrain_profile::Function<CreateTexture2D,terrain_profile::Id::TextureNative> original{nullptr};
    bool observing=false;
    auto table=*reinterpret_cast<void ***>(dev);
    AcquireSRWLockShared(&slot_lock);
    for(const auto &s:slots)if(s.table==table){original=s.original;break;}
    observing=permitted && !stopping;
    ReleaseSRWLockShared(&slot_lock);
    // An installed slot always has a saved original, including during restore.
    bool capture=false;
    if(observing && active() && out && control_desc(desc,initial)) {
        AcquireSRWLockShared(&ctrl_lock);capture=!render_failed;ReleaseSRWLockShared(&ctrl_lock);
    }
    bool capture_attempt=capture;
    D3D11_TEXTURE2D_DESC writable={};D3D11_SUBRESOURCE_DATA patched={};
    const D3D11_SUBRESOURCE_DATA *create_initial=initial;
    std::vector<unsigned> base,shadow;unsigned initial_cells=0;
    if(capture) {
        try {
            base.resize(static_cast<size_t>(CtrlSize)*CtrlSize);
            auto bytes=static_cast<const unsigned char *>(initial[0].pSysMem);
            for(unsigned row=0;row<CtrlSize;++row)memcpy(base.data()+static_cast<size_t>(row)*CtrlSize,
                bytes+static_cast<size_t>(row)*initial[0].SysMemPitch,CtrlPitch);
            shadow=base;
            AcquireSRWLockShared(&ctrl_lock);
            for(unsigned hex=0;hex<HexCount;++hex)if(has_goal[hex] && goals[hex]!=RestoreOriginal) {
                if(write_cell(shadow,base,hex,goals[hex]))++initial_cells;
            }
            ReleaseSRWLockShared(&ctrl_lock);
            writable=*desc;writable.Usage=D3D11_USAGE_DEFAULT;
            if(initial_cells){patched={shadow.data(),CtrlPitch,0};create_initial=&patched;}
        } catch(...) {
            AcquireSRWLockExclusive(&ctrl_lock);render_failed=true;discard_ctrl_locked();ReleaseSRWLockExclusive(&ctrl_lock);capture=false;
        }
#ifndef NO_RUNTIME_LOG
        if(capture)log_event("terrain_render_initial",initial_cells,0,0);
#endif
    }
    // R3: the height asset is made writable the same way; its pixels stay native.
    bool height_capture=!capture && observing && active() && out && height_desc(desc,initial);
    std::vector<float> height_base;
    if(height_capture) {
        AcquireSRWLockShared(&ctrl_lock);height_capture=!render_failed;ReleaseSRWLockShared(&ctrl_lock);
    }
    if(height_capture) {
        try {
            height_base.resize(static_cast<size_t>(CtrlSize)*CtrlSize);
            auto bytes=static_cast<const unsigned char *>(initial[0].pSysMem);
            for(unsigned row=0;row<CtrlSize;++row)memcpy(height_base.data()+static_cast<size_t>(row)*CtrlSize,
                bytes+static_cast<size_t>(row)*initial[0].SysMemPitch,CtrlPitch);
            writable=*desc;writable.Usage=D3D11_USAGE_DEFAULT;
        } catch(...) {height_capture=false;}
    }
    bool mask_capture=observing && active() && out && mask_desc(desc,initial);
    WaterMask mask_base;
    if(mask_capture) {
        AcquireSRWLockExclusive(&ctrl_lock);
        // A newly created immutable asset after a confirmed one is a new load
        // generation. Never carry its old mapping or original bytes across it.
        mask_fit_attempted=mask_ready=false;
        if(mask_count>=MaxMasks)mask_overflow=true;
        queue_mask_targets_locked();
        mask_capture=!render_failed && mask_count<MaxMasks;
        ReleaseSRWLockExclusive(&ctrl_lock);
    }
    if(mask_capture) {
        try {
            mask_base.original.resize(static_cast<size_t>(MaskSize)*MaskSize);
            auto bytes=static_cast<const unsigned char *>(initial[0].pSysMem);
            for(unsigned row=0;row<MaskSize;++row)memcpy(mask_base.original.data()+static_cast<size_t>(row)*MaskSize,
                bytes+static_cast<size_t>(row)*initial[0].SysMemPitch,MaskPitch);
            mask_base.shadow=mask_base.original;
            writable=*desc;writable.Usage=D3D11_USAGE_DEFAULT;
        } catch(...) {mask_capture=false;}
    }
    HRESULT result=original(dev,(capture || height_capture || mask_capture)?&writable:desc,create_initial,out);
    if(mask_capture) {
        if(FAILED(result))result=original(dev,desc,initial,out);
        else if(*out) {
            AcquireSRWLockExclusive(&ctrl_lock);
            if(!render_failed && mask_count<MaxMasks) {
                WaterMask &m=water_masks[mask_count++];
                (*out)->AddRef();m.texture=*out;m.device=dev;
                m.original.swap(mask_base.original);m.shadow.swap(mask_base.shadow);
                // Publish the changed candidate set atomically with invalidation.
                mask_fit_attempted=mask_ready=false;queue_mask_targets_locked();
            }
#ifndef NO_RUNTIME_LOG
            log_event("water_mask_capture",mask_count,0,0);
#endif
            ReleaseSRWLockExclusive(&ctrl_lock);
        }
    }
    if(height_capture) {
        if(FAILED(result))result=original(dev,desc,initial,out);
        else if(*out) {
            AcquireSRWLockExclusive(&ctrl_lock);
#ifdef NO_RUNTIME_LOG
            if(height_texture)discard_masks_locked();
#endif
            (*out)->AddRef();discard_height_locked();height_texture=*out;height_device=dev;
            original_height.swap(height_base);shadow_height=original_height;
            mask_fit_attempted=mask_ready=false;
            ReleaseSRWLockExclusive(&ctrl_lock);
        }
#ifndef NO_RUNTIME_LOG
        log_event("terrain_render_capture",desc->Format,SUCCEEDED(result) && height_texture?1:0,1);
#endif
    }
    if(capture_attempt) {
        bool saved=false;
        if(capture && FAILED(result)) {
            AcquireSRWLockExclusive(&ctrl_lock);render_failed=true;discard_ctrl_locked();ReleaseSRWLockExclusive(&ctrl_lock);
            // Session fallback: preserve the native descriptor, arguments and HRESULT.
            result=original(dev,desc,initial,out);
        } else if(capture && *out) {
                AcquireSRWLockExclusive(&ctrl_lock);
                if(!render_failed) {
                    (*out)->AddRef();discard_ctrl_locked();ctrl_texture=*out;ctrl_device=dev;
#ifndef NO_RUNTIME_LOG
                    ctrl_create=original;
#endif
                    original_ctrl.swap(base);shadow_ctrl.swap(shadow);clear_queue_locked();world_transition=false;resync_needed=true;saved=true;
                }
                ReleaseSRWLockExclusive(&ctrl_lock);
        }
#ifndef NO_RUNTIME_LOG
        log_event("terrain_render_capture",desc->Format,saved?1:0,0);
#else
        (void)saved;
#endif
    }
    if(SUCCEEDED(result) && observing && out && *out && active() && work_desc(desc,initial)) {
        unsigned kept=0;
        AcquireSRWLockExclusive(&ctrl_lock);
        // Pair only with the asset captured for this device; any later pair is ignored.
        if(ctrl_texture && ctrl_device==dev && !render_failed && ctrl_work_count<MaxWork) {
            (*out)->AddRef();ctrl_work[ctrl_work_count++]=*out;kept=ctrl_work_count;
            // Cells uploaded before this texture existed must be mirrored again.
            for(unsigned hex=0;hex<HexCount;++hex)if(has_goal[hex])enqueue_locked(hex);
            publish_pending_locked();
        }
        ReleaseSRWLockExclusive(&ctrl_lock);
#ifndef NO_RUNTIME_LOG
        log_event("terrain_render_work",static_cast<unsigned>(desc->Format),kept,0);
#else
        (void)kept;
#endif
    }
    if(SUCCEEDED(result) && observing && out && *out && active() && height_work_desc(desc,initial)) {
        unsigned kept=0;
        AcquireSRWLockExclusive(&ctrl_lock);
        if(height_texture && height_device==dev && !render_failed && height_work_count<MaxWork) {
            (*out)->AddRef();height_work[height_work_count++]=*out;kept=height_work_count;
            for(unsigned hex=0;hex<HexCount;++hex)if(has_goal[hex])enqueue_locked(hex);
            publish_pending_locked();
        }
        ReleaseSRWLockExclusive(&ctrl_lock);
#ifndef NO_RUNTIME_LOG
        log_event("terrain_render_work",static_cast<unsigned>(desc->Format),kept,1);
#else
        (void)kept;
#endif
    }
    if(SUCCEEDED(result) && observing && out && *out && active() && mask_work_desc(desc,initial)) {
        unsigned kept=0;
        AcquireSRWLockExclusive(&ctrl_lock);
        bool same_device=false;
        for(const auto &m:water_masks)if(m.texture && m.device==dev)same_device=true;
        if(same_device && !render_failed && mask_work_count<MaxWork) {
            (*out)->AddRef();mask_work[mask_work_count++]=*out;kept=mask_work_count;
            // Preserve the actual shadow for exact rollback if a late candidate
            // makes release selection ambiguous before this mirror is uploaded.
            mask_mirror_pending=true;
            for(unsigned hex=0;hex<HexCount;++hex)if(has_goal[hex])enqueue_locked(hex);
            publish_pending_locked();
        }
        ReleaseSRWLockExclusive(&ctrl_lock);
#ifndef NO_RUNTIME_LOG
        log_event("water_mask_work",desc->Format,kept,0);
#else
        (void)kept;
#endif
    }
#ifndef NO_RUNTIME_LOG
    if(SUCCEEDED(result) && desc && out && *out && observing && active() &&
       (target(*desc) || (desc->Width==2049 && desc->Height==2049))) {
        bool saved=false;unsigned saved_count=0;UINT words[4]={},pitch=0;
        AcquireSRWLockExclusive(&record_lock);
        if(desc->Width==2049 && desc->Height==2049)saw_2049=true;
        bool bc=bc_format(desc->Format);
        if(initial && initial[0].pSysMem && record_count<MaxTextures && target(*desc) && (!bc || bc_count<MaxBCTextures)) {
            // Only read the admitted first subresource, after the original call.
            // memcpy accepts unaligned source data; no texture/resource mapping.
            memcpy(words,initial[0].pSysMem,sizeof(words));pitch=initial[0].SysMemPitch;
            records[record_count++]={*out,desc->Width,desc->Height,desc->MipLevels,desc->ArraySize,desc->Format,
                desc->Usage,desc->BindFlags,desc->CPUAccessFlags,desc->MiscFlags,true,GetCurrentThreadId(),
                {words[0],words[1],words[2],words[3]},pitch};
            if(bc)++bc_count;
            saved=true;saved_count=record_count;
        }
        ReleaseSRWLockExclusive(&record_lock);
        if(saved) {
            log_event("terrain_texture",desc->Format,desc->Width,desc->Usage*100000u+desc->BindFlags*10u+(initial?1u:0u));
            log_event("terrain_texture_flags",desc->MipLevels*1000u+desc->ArraySize,desc->Height,desc->CPUAccessFlags*100000u+desc->MiscFlags);
            log_event("terrain_texture_data",words[0],words[1],words[2]);
            log_event("terrain_texture_pitch",pitch,desc->Format,words[3]);
            if(saved_count==MaxTextures)log_event("terrain_texture_full",saved_count,0,0);
        }
    }
#endif
    return result;
}
static bool guards_locked() {
    for(const auto &s:slots)if(s.published &&
        (s.table[CreateTexture2DSlot]!=reinterpret_cast<void *>(&create_texture_2d) ||
         !original_site(s.table,s.original) || memcmp(s.bytes,reinterpret_cast<void *>(s.original),sizeof(s.bytes))))return false;
    return true;
}
static bool guards_match() {
    AcquireSRWLockShared(&slot_lock);bool ok=guards_locked();ReleaseSRWLockShared(&slot_lock);return ok;
}
static bool hooks_active() {
    AcquireSRWLockShared(&slot_lock);
    bool active=false;for(const auto &s:slots)if(s.published){active=true;break;}
    ReleaseSRWLockShared(&slot_lock);return active;
}
static bool install_device(ID3D11Device *dev,bool early) {
    bool installed=false,rejected=false,usable=false;
    AcquireSRWLockExclusive(&slot_lock);
    if(permitted && !stopping) {
        auto table=*reinterpret_cast<void ***>(dev);
        DeviceSlot *free=nullptr;bool known=false;
        for(auto &s:slots) {if(s.table==table)known=true;else if(!s.table && !free)free=&s;}
        if(!guards_locked())rejected=true;
        else if(known)usable=true;
        else if(!known && free) {
            auto original=reinterpret_cast<CreateTexture2D>(table[CreateTexture2DSlot]);
            if(!original_site(table,original))rejected=true;
            else {
                unsigned char bytes[16];memcpy(bytes,reinterpret_cast<void *>(original),sizeof(bytes));
                DWORD prior=0;
                if(VirtualProtect(table+CreateTexture2DSlot,sizeof(void *),PAGE_READWRITE,&prior)) {
                    // Validate again at publication; preserve foreign replacements.
                    if(table[CreateTexture2DSlot]==reinterpret_cast<void *>(original) &&
                       !memcmp(bytes,reinterpret_cast<void *>(original),sizeof(bytes))) {
                        if(!early)dev->AddRef();
                        *free={table,original,early?nullptr:dev,{},false};memcpy(free->bytes,bytes,sizeof(bytes));
                        void *previous=InterlockedCompareExchangePointer(table+CreateTexture2DSlot,
                            reinterpret_cast<void *>(&create_texture_2d),reinterpret_cast<void *>(original));
                        if(previous==reinterpret_cast<void *>(original)) {
                            usable=installed=free->published=true;
                        }
                        else {if(!early)dev->Release();*free={};rejected=true;}
                    } else rejected=true;
                    DWORD ignored=0;VirtualProtect(table+CreateTexture2DSlot,sizeof(void *),prior,&ignored);
                }
            }
        }
        // A failed probe must leave the real-device fallback available.
        if(rejected && !early)permitted=false;
    }
    ReleaseSRWLockExclusive(&slot_lock);
#ifndef NO_RUNTIME_LOG
    if(installed)log_event("terrain_texture_hook",early?2:1,0,0);
    if(rejected && !early)log_event("feature_guard_mismatch",28,0,0);
#else
    (void)installed;
#endif
    return usable;
}
static void install_early() {
#ifdef SAN14_WATER_DIAG_INCLUDED
    water_diag::mask_sample=&confirm_mask;
    water_diag::install_early();
#endif
    if(!active())return;
    AcquireSRWLockExclusive(&slot_lock);
    bool attempt=!early_attempted && permitted && !stopping;
    if(attempt)early_attempted=true;
    ReleaseSRWLockExclusive(&slot_lock);
    if(!attempt)return;
    wchar_t system[MAX_PATH]={};
    UINT length=GetSystemDirectoryW(system,MAX_PATH);
    constexpr wchar_t suffix[]=L"\\d3d11.dll";
    if(!length || length+sizeof(suffix)/sizeof(*suffix)>MAX_PATH || wcscat_s(system,MAX_PATH,suffix))return;
    HMODULE library=LoadLibraryW(system);if(!library)return;
    auto create=reinterpret_cast<decltype(&D3D11CreateDevice)>(GetProcAddress(library,"D3D11CreateDevice"));
    ID3D11Device *temporary=nullptr;
    HRESULT result=create?create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&temporary,nullptr,nullptr):E_FAIL;
    bool installed=SUCCEEDED(result) && temporary && install_device(temporary,true);
    if(installed) {
        AcquireSRWLockExclusive(&slot_lock);
        early_table=*reinterpret_cast<void ***>(temporary);early_library=library;
        ReleaseSRWLockExclusive(&slot_lock);
    }
    if(temporary)temporary->Release();
    if(!installed)FreeLibrary(library);
}
static void on_device(ID3D11Device *dev) {
    TERRAIN_SCOPE(RenderDevice);
#ifdef SAN14_WATER_DIAG_INCLUDED
    water_diag::mask_sample=&confirm_mask;
    water_diag::on_device(dev);
#endif
    if(!dev || !active())return;
    AcquireSRWLockExclusive(&ctrl_lock);
    if(ctrl_texture && ctrl_device!=dev) {discard_ctrl_locked();resync_needed=true;}
    if(height_texture && height_device!=dev)discard_height_locked();
    for(const auto &m:water_masks)if(m.texture && m.device!=dev){discard_masks_locked();break;}
    ReleaseSRWLockExclusive(&ctrl_lock);
#ifndef NO_RUNTIME_LOG
    int match=-1;
    AcquireSRWLockExclusive(&slot_lock);
    if(early_table && !stopping) {
        auto table=*reinterpret_cast<void ***>(dev);bool seen=false;
        for(unsigned i=0;i<reported_count;++i)if(reported_tables[i]==table)seen=true;
        if(!seen && reported_count<MaxTables) {
            reported_tables[reported_count++]=table;match=table==early_table?1:0;
        }
    }
    ReleaseSRWLockExclusive(&slot_lock);
    if(match>=0)log_event("terrain_texture_table",static_cast<unsigned>(match),0,0);
#endif
    if(!install_device(dev,false)) {
        AcquireSRWLockExclusive(&ctrl_lock);render_failed=true;discard_ctrl_locked();ReleaseSRWLockExclusive(&ctrl_lock);
    }
}
static void before_world_change() {
    // R2b diagnostic: retain targets across a same-session reload. A different
    // save/new game can briefly inherit these until its sidecar resynchronizes.
    AcquireSRWLockExclusive(&ctrl_lock);world_transition=true;memset(cliff,0,sizeof(cliff));clear_queue_locked();resync_needed=true;ReleaseSRWLockExclusive(&ctrl_lock);
#ifndef NO_RUNTIME_LOG
    if(!active())return;
    AcquireSRWLockShared(&record_lock);bool missing=!saw_2049;ReleaseSRWLockShared(&record_lock);
    if(missing)log_event("terrain_texture_none",0,0,0);
#endif
}
static void restore() {
#ifdef SAN14_WATER_DIAG_INCLUDED
    water_diag::restore();
#endif
    AcquireSRWLockExclusive(&slot_lock);stopping=true;
    AcquireSRWLockExclusive(&ctrl_lock);render_failed=true;clear_targets_locked();discard_ctrl_locked();discard_height_locked();discard_masks_locked();ReleaseSRWLockExclusive(&ctrl_lock);
    for(auto &s:slots)if(s.published) {
        DWORD prior=0;
        if(!VirtualProtect(s.table+CreateTexture2DSlot,sizeof(void *),PAGE_READWRITE,&prior))continue;
        InterlockedCompareExchangePointer(s.table+CreateTexture2DSlot,reinterpret_cast<void *>(s.original),reinterpret_cast<void *>(&create_texture_2d));
        DWORD ignored=0;VirtualProtect(s.table+CreateTexture2DSlot,sizeof(void *),prior,&ignored);
        s.published=false;
        // Keep saved call targets for any wrapper already entered before restore.
        if(s.keep){s.keep->Release();s.keep=nullptr;}
    }
    ReleaseSRWLockExclusive(&slot_lock);
}
struct Lifetime {~Lifetime(){restore();}};
static Lifetime lifetime;
}
static bool terrain_render_guards_match() {return terrain_render::guards_match();}
