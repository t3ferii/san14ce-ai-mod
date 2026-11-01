// User/Claude 2026-10-29: standalone fixture instrumentation declaration.
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
// User/Claude 2026-10-29: R1 observes native CreateTexture2D, never texture contents.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <vector>
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;} mod_features;
static unsigned checks;
static void require(bool ok,const char *label) {++checks;if(!ok){fprintf(stderr,"FAIL %s (%u)\n",label,checks);ExitProcess(1);}}
struct FakeDevice {void **table;LONG references=1;};
static void *accepted_tables[4];
static bool site_valid=true;
static bool terrain_render_fixture_site(void **table,void *original);
#define TERRAIN_RENDER_FIXTURE
#ifndef NO_RUNTIME_LOG
struct Event {const char *name;unsigned a,b,c;};
static SRWLOCK events_lock=SRWLOCK_INIT;
static std::vector<Event> events;
static void log_event(const char *name,unsigned a,unsigned b,unsigned c) {
    AcquireSRWLockExclusive(&events_lock);events.push_back({name,a,b,c});ReleaseSRWLockExclusive(&events_lock);
}
#endif
#include "../src/native_terrain_render.inl"
static const D3D11_TEXTURE2D_DESC *wanted_desc;
static const D3D11_SUBRESOURCE_DATA *wanted_initial;
static ID3D11Texture2D **wanted_out;
static ID3D11Device *wanted_device;
static HRESULT native_result=S_OK;
static bool empty_output=false,check_arguments=true;
static volatile LONG calls,argument_failures;
static ID3D11Texture2D *identity=reinterpret_cast<ID3D11Texture2D *>(static_cast<uintptr_t>(0x12345678));
static HRESULT STDMETHODCALLTYPE original_texture(ID3D11Device *dev,const D3D11_TEXTURE2D_DESC *desc,
    const D3D11_SUBRESOURCE_DATA *initial,ID3D11Texture2D **out) {
    InterlockedIncrement(&calls);
    if(check_arguments && (dev!=wanted_device || desc!=wanted_desc || initial!=wanted_initial || out!=wanted_out))InterlockedIncrement(&argument_failures);
    // Observation must not have happened before this native call completes.
#ifndef NO_RUNTIME_LOG
    if(check_arguments && terrain_render::record_count)InterlockedIncrement(&argument_failures);
#endif
    if(out)*out=empty_output?nullptr:identity;
    return native_result;
}
static HRESULT STDMETHODCALLTYPE second_texture(ID3D11Device *,const D3D11_TEXTURE2D_DESC *,const D3D11_SUBRESOURCE_DATA *,ID3D11Texture2D **out) {
    InterlockedIncrement(&calls);if(out)*out=identity;return S_FALSE;
}
static HRESULT STDMETHODCALLTYPE foreign_texture(ID3D11Device *,const D3D11_TEXTURE2D_DESC *,const D3D11_SUBRESOURCE_DATA *,ID3D11Texture2D **) {return E_NOTIMPL;}
static ULONG STDMETHODCALLTYPE add_ref(ID3D11Device *dev) {return static_cast<ULONG>(InterlockedIncrement(&reinterpret_cast<FakeDevice *>(dev)->references));}
static ULONG STDMETHODCALLTYPE release(ID3D11Device *dev) {return static_cast<ULONG>(InterlockedDecrement(&reinterpret_cast<FakeDevice *>(dev)->references));}
static bool terrain_render_fixture_site(void **table,void *original) {
    bool known=false;for(void *candidate:accepted_tables)if(candidate==table)known=true;
    return site_valid && known && (original==reinterpret_cast<void *>(&original_texture) || original==reinterpret_cast<void *>(&second_texture));
}
static void reset_records() {
#ifndef NO_RUNTIME_LOG
    terrain_render::record_count=terrain_render::bc_count=0;terrain_render::saw_2049=false;events.clear();
#endif
}
static unsigned event_count(const char *name) {
#ifndef NO_RUNTIME_LOG
    unsigned count=0;for(const auto &e:events)if(!strcmp(e.name,name))++count;return count;
#else
    (void)name;return 0;
#endif
}
int main() {
    using namespace terrain_render;
    void *tables[4][6]={};FakeDevice devices[5]={};
    for(unsigned i=0;i<4;++i) {
        accepted_tables[i]=tables[i];tables[i][1]=reinterpret_cast<void *>(&add_ref);tables[i][2]=reinterpret_cast<void *>(&release);
        tables[i][5]=reinterpret_cast<void *>(i==1?&second_texture:&original_texture);devices[i].table=tables[i];
    }
    devices[4].table=tables[0];auto dev=reinterpret_cast<ID3D11Device *>(&devices[0]);
    auto invoke=[&](const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial,ID3D11Texture2D **out) {
        wanted_device=dev;wanted_desc=d;wanted_initial=initial;wanted_out=out;
        return reinterpret_cast<CreateTexture2D>(devices[0].table[5])(dev,d,initial,out);
    };
    require(guards_match(),"pending device is a valid deferred guard");
    for(unsigned off=0;off<3;++off) {
        enabled=off!=0;mod_features.convenience=off!=1;mod_features.terrain_edit=off!=2;
        on_device(dev);require(tables[0][5]==reinterpret_cast<void *>(&original_texture) && devices[0].references==1,"OFF installs no slot and retains no device");
    }
    enabled=mod_features.convenience=mod_features.terrain_edit=true;
    on_device(nullptr);require(!slots[0].table,"null device ignored");
    on_device(dev);require(tables[0][5]==reinterpret_cast<void *>(&create_texture_2d) && devices[0].references==2,"slot 5 installed once and device retained");
    require(guards_match(),"owned slot and original bytes match");
#ifndef NO_RUNTIME_LOG
    require(event_count("terrain_texture_hook")==1,"hook diagnostic once");
#endif
    on_device(dev);on_device(reinterpret_cast<ID3D11Device *>(&devices[4]));
    require(devices[0].references==2 && devices[4].references==1 && slots[0].original==&original_texture,"same vtable on replacement device does not rehook or chain itself");
    reset_records();before_world_change();
#ifndef NO_RUNTIME_LOG
    require(event_count("terrain_texture_none")==1,"world boundary reports missed 2049 even before any edits");
#else
    require(event_count("terrain_texture_none")==0,"release has no diagnostics");
#endif
    D3D11_TEXTURE2D_DESC d={};d.Width=d.Height=2049;d.MipLevels=3;d.ArraySize=2;d.Format=DXGI_FORMAT_R32_FLOAT;
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0x8;d.CPUAccessFlags=0x20000;d.MiscFlags=0x4;d.SampleDesc.Count=1;
    // User 2026-10-29 local36: unaligned first-subresource fingerprint and pitch.
    const UINT expected_words[]={0x11223344u,0x89abcdefu,0x3f800000u,0xffffffffu};
    unsigned char initial_bytes[20]={};memcpy(initial_bytes+1,expected_words,sizeof(expected_words));
    D3D11_SUBRESOURCE_DATA initial={initial_bytes+1,8196,0};ID3D11Texture2D *out=nullptr;
    reset_records();LONG before=calls;require(invoke(&d,&initial,&out)==S_OK && out==identity && calls==before+1 && !argument_failures,"native first, exact four arguments, output and HRESULT preserved");
#ifndef NO_RUNTIME_LOG
    require(record_count==1,"one target record");const auto &r=records[0];
    require(r.address==identity && r.Width==2049 && r.Height==2049 && r.MipLevels==3 && r.ArraySize==2 && r.Format==d.Format &&
        r.Usage==d.Usage && r.BindFlags==d.BindFlags && r.CPUAccessFlags==d.CPUAccessFlags && r.MiscFlags==d.MiscFlags && r.initial_data && r.thread_id==GetCurrentThreadId(),"all description fields, raw identity and creator thread recorded");
    require(events.size()==4 && !strcmp(events[0].name,"terrain_texture") && events[0].a==DXGI_FORMAT_R32_FLOAT && events[0].b==2049 && events[0].c==300081 &&
        !strcmp(events[1].name,"terrain_texture_flags") && events[1].a==3002 && events[1].b==2049 && events[1].c==d.CPUAccessFlags*100000u+d.MiscFlags,"local35 packing exposes mip count, array size, height and access/misc flags");
    require(!strcmp(events[2].name,"terrain_texture_data") && events[2].a==expected_words[0] && events[2].b==expected_words[1] && events[2].c==expected_words[2] &&
        !strcmp(events[3].name,"terrain_texture_pitch") && events[3].a==8196 && events[3].b==DXGI_FORMAT_R32_FLOAT && events[3].c==expected_words[3],"all 16 input bytes and first subresource pitch logged bit-exactly");
    require(!memcmp(r.first_words,expected_words,sizeof(expected_words)) && r.sys_mem_pitch==8196,"record retains input fingerprint values without retaining source pointer");
    before_world_change();require(!event_count("terrain_texture_none"),"seen 2049 suppresses none diagnostic");
#endif
    check_arguments=false;
    // User 2026-10-29 local36: require input data; any BC at width >=4096 shares quota.
    for(auto width:{1023u,1024u,2048u,2049u,4096u,8192u})for(auto height:{1023u,1024u,1536u,2049u,4096u})for(unsigned format=0;format<=100;++format)for(bool has_initial:{false,true}) {
        reset_records();d.Width=width;d.Height=height;d.Format=static_cast<DXGI_FORMAT>(format);native_result=S_FALSE;
        require(invoke(&d,has_initial?&initial:nullptr,&out)==S_FALSE,"all success HRESULTs preserved");
#ifndef NO_RUNTIME_LOG
        bool bc=(format>=70 && format<=84) || (format>=94 && format<=99);
        bool expected=has_initial && width>=1024 && height>=1024 && (!bc || width>=4096);
        require(record_count==(expected?1u:0u) && event_count("terrain_texture")==record_count && event_count("terrain_texture_data")==record_count && event_count("terrain_texture_pitch")==record_count,"initial-data gate, BC width threshold and independent dimensions");
        if(expected)require(records[0].initial_data && !memcmp(records[0].first_words,expected_words,sizeof(expected_words)) && records[0].sys_mem_pitch==8196,"each admitted asset has first-subresource fingerprint and pitch");
#endif
    }
    reset_records();d.Width=1023;d.Height=2049;d.Format=DXGI_FORMAT_R32_FLOAT;invoke(&d,&initial,&out);
    d.Width=2049;d.Height=1023;invoke(&d,&initial,&out);
    d.Width=d.Height=2049;native_result=E_INVALIDARG;require(invoke(&d,&initial,&out)==E_INVALIDARG,"failed HRESULT unchanged");
    native_result=S_OK;empty_output=true;invoke(&d,&initial,&out);empty_output=false;
    invoke(nullptr,&initial,&out);invoke(&d,&initial,nullptr);
    D3D11_SUBRESOURCE_DATA null_memory={};invoke(&d,&null_memory,&out);
#ifndef NO_RUNTIME_LOG
    require(!record_count && events.empty(),"undersized, failed, null output and validation-only calls unrecorded");
#endif
    D3D11_SUBRESOURCE_DATA unreadable={reinterpret_cast<void *>(static_cast<uintptr_t>(1)),16,0};
    for(unsigned off=0;off<3;++off) {
        enabled=off!=0;mod_features.convenience=off!=1;mod_features.terrain_edit=off!=2;
        require(invoke(&d,&unreadable,&out)==S_OK,"OFF wrapper remains pass-through without reading input bytes");
    }
    enabled=mod_features.convenience=mod_features.terrain_edit=true;
#ifndef NO_RUNTIME_LOG
    require(!record_count && events.empty(),"OFF wrapper emits no records");
#endif
    reset_records();std::vector<std::thread> threads;
    for(unsigned i=0;i<8;++i)threads.emplace_back([&] {for(unsigned n=0;n<16;++n){ID3D11Texture2D *local=nullptr;create_texture_2d(dev,&d,&initial,&local);}});
    for(auto &thread:threads)thread.join();
#ifndef NO_RUNTIME_LOG
    require(record_count==64 && event_count("terrain_texture")==64 && event_count("terrain_texture_flags")==64 && event_count("terrain_texture_data")==64 && event_count("terrain_texture_pitch")==64 && event_count("terrain_texture_full")==1,"64-record SRW cap, all four log events and one full event under concurrent callers");
    for(const auto &event:events)if(!strcmp(event.name,"terrain_texture_full"))
        require(event.a==64 && !event.b && !event.c,"full event carries reached record count regardless of concurrent log ordering");
    // Reproduce local35: title render targets must not exhaust the asset records.
    reset_records();d.Width=1920;d.Height=1080;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Usage=D3D11_USAGE_DEFAULT;
    for(unsigned i=0;i<80;++i)invoke(&d,nullptr,&out);
    require(!record_count && events.empty(),"80 title render targets without initial data consume no capacity");
    d.Width=d.Height=2048;d.Usage=D3D11_USAGE_IMMUTABLE;
    for(unsigned i=0;i<31;++i) {const unsigned formats[]={70,76,79,97};d.Format=static_cast<DXGI_FORMAT>(formats[i%4]);invoke(&d,&initial,&out);}
    require(!record_count && events.empty(),"2048 BC assets stay excluded even with initial data");
    d.Width=d.Height=4096;d.Format=DXGI_FORMAT_BC7_UNORM;invoke(&d,&initial,&out);
    d.Width=d.Height=2048;d.Format=DXGI_FORMAT_R32_TYPELESS;invoke(&d,&initial,&out);
    d.Width=d.Height=2049;d.Format=DXGI_FORMAT_R32_FLOAT;invoke(&d,&initial,&out);
    require(record_count==3 && bc_count==1 && records[1].Format==DXGI_FORMAT_R32_TYPELESS && records[2].Width==2049 && saw_2049,"immutable R32 heightmap candidate survives render-target and small-BC bursts");
    reset_records();d.Width=8192;d.Height=1024;
    for(unsigned i=0;i<20;++i) {const unsigned formats[]={70,76,79,94,97,99};d.Format=static_cast<DXGI_FORMAT>(formats[i%6]);invoke(&d,&initial,&out);}
    require(record_count==4 && bc_count==4 && event_count("terrain_texture")==4 && !event_count("terrain_texture_full"),"all large BC variants and nonsquare textures share a four-record quota");
    invoke(&d,&unreadable,&out);require(record_count==4,"BC quota rejection does not read initial bytes");
    d.Width=1024;d.Height=1536;d.Format=DXGI_FORMAT_R32_TYPELESS;d.MipLevels=0;d.ArraySize=7;d.CPUAccessFlags=0;d.MiscFlags=0x200;
    D3D11_SUBRESOURCE_DATA subresources[2]={initial,unreadable};invoke(&d,subresources,&out);
    const auto &flags=events[events.size()-3];
    require(records[4].Height==1536 && records[4].ArraySize==7 && !strcmp(flags.name,"terrain_texture_flags") && flags.a==7 && flags.b==1536 && flags.c==0x200 && !memcmp(records[4].first_words,expected_words,sizeof(expected_words)),"flags retain dimensions and array size; only subresource zero is read");
    for(unsigned i=0;i<59;++i)invoke(&d,&initial,&out);
    require(record_count==64 && bc_count==4 && event_count("terrain_texture_full")==1,"BC quota leaves sixty asset slots within total 64");
    for(unsigned i=0;i<10;++i)invoke(&d,&unreadable,&out);
    d.Width=d.Height=2049;d.Format=DXGI_FORMAT_BC3_UNORM;invoke(&d,&unreadable,&out);before_world_change();
    require(record_count==64 && saw_2049 && !event_count("terrain_texture_none") && event_count("terrain_texture_full")==1,"2049 detection survives both BC exclusion and full capacity; full event never repeats");
    reset_records();d.Width=4096;d.Height=1024;d.Format=DXGI_FORMAT_BC3_UNORM;
    threads.clear();
    for(unsigned i=0;i<8;++i)threads.emplace_back([&] {for(unsigned n=0;n<16;++n){ID3D11Texture2D *local=nullptr;create_texture_2d(dev,&d,&initial,&local);}});
    for(auto &thread:threads)thread.join();
    require(record_count==4 && bc_count==4 && event_count("terrain_texture")==4 && event_count("terrain_texture_data")==4 && event_count("terrain_texture_pitch")==4 && !event_count("terrain_texture_full"),"BC four-record quota and fingerprints remain atomic across concurrent creators");
#else
    require(invoke(&d,&unreadable,&out)==S_OK,"release wrapper never reads initial bytes");
#endif
    require(!memcmp(initial_bytes+1,expected_words,sizeof(expected_words)),"input data is never modified");
    on_device(reinterpret_cast<ID3D11Device *>(&devices[1]));
    require(tables[1][5]==reinterpret_cast<void *>(&create_texture_2d),"distinct table independently hooked");
    require(create_texture_2d(reinterpret_cast<ID3D11Device *>(&devices[1]),&d,nullptr,&out)==S_FALSE,"second vtable uses its own original");
    slots[0].bytes[0]^=1;require(!terrain_render_guards_match(),"changed original bytes reject only observer guard");slots[0].bytes[0]^=1;
    require(guards_match(),"restored fingerprint matches");
    tables[0][5]=reinterpret_cast<void *>(&foreign_texture);require(!guards_match(),"foreign replacement rejected");
    on_device(dev);require(!permitted && mod_features.terrain_edit && mod_features.convenience,"late mismatch isolates renderer while retaining editing and convenience");
    reset_records();create_texture_2d(dev,&d,nullptr,&out);
#ifndef NO_RUNTIME_LOG
    require(!record_count && events.empty(),"isolated observer wrapper passes without recording");
#endif
    {Lifetime unload;}
    require(tables[0][5]==reinterpret_cast<void *>(&foreign_texture) && tables[1][5]==reinterpret_cast<void *>(&second_texture),"unload preserves foreign owner and restores own slots");
    require(devices[0].references==1 && devices[1].references==1,"unload releases retained devices without touching textures");
    require(create_texture_2d(reinterpret_cast<ID3D11Device *>(&devices[1]),&d,nullptr,&out)==S_FALSE,"already entered wrapper retains original after restoration");
    restore();require(devices[1].references==1,"repeated restore is harmless");
    // Reset only fixture state to exercise first-publication rejection.
    memset(slots,0,sizeof(slots));stopping=false;permitted=true;site_valid=false;
    on_device(reinterpret_cast<ID3D11Device *>(&devices[2]));
    require(!permitted && devices[2].references==1 && tables[2][5]==reinterpret_cast<void *>(&original_texture),"untrusted original never installed");
    stopping=true;
    printf("{\"terrain_render_checks\":%u,\"failed\":0,\"native_calls\":%ld,\"runtime_log\":%s}\n",checks,calls,
#ifdef NO_RUNTIME_LOG
        "false"
#else
        "true"
#endif
    );return 0;
}
