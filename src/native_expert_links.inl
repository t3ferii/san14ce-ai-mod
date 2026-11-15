#include "game_addresses.h"
/* One owner: SAN14_Convenience, alongside its existing combat-pair hook. */
struct ExpertCall {unsigned site,size;unsigned char bytes[8];void *replacement;unsigned char opcode;};
static const ExpertCall expert_calls[]={
    {GA(GA_2B2E28),5,{0xE8,0xF3,0x9A,0x13,0x00},reinterpret_cast<void *>(&expert_native_adjacency_option),0xE8},
    {GA(GA_658DF0),6,{0x40,0x57,0x41,0x56,0x41,0x57},reinterpret_cast<void *>(&expert_populate),0xE9},
    {GA(GA_638B8B),5,{0xE8,0x40,0x8A,0xF8,0xFF},reinterpret_cast<void *>(&expert_number_current),0xE8},
    {GA(GA_2AAF30),7,{0x48,0x8B,0xC4,0x44,0x89,0x48,0x20},reinterpret_cast<void *>(&expert_unit_power),0xE9},
    {GA(GA_174AF4),5,{0xE8,0x37,0x7D,0xFF,0xFF},reinterpret_cast<void *>(&expert_field_stats),0xE8},
    {GA(GA_5F3D3A),5,{0xE8,0x81,0xCC,0x33,0x00},reinterpret_cast<void *>(&expert_number_factory),0xE8}
};
static constexpr unsigned expert_call_count=sizeof(expert_calls)/sizeof(expert_calls[0]);
struct ExpertPointer {unsigned slot,original;void *replacement;void **next;};
static const ExpertPointer expert_pointers[]={
    {GA(GA_141EF18),GA(GA_5A9950),reinterpret_cast<void *>(&expert_destroy),&next_expert_destroy},
    {GA(GA_141ED58),GA(GA_5BBA10),reinterpret_cast<void *>(&expert_list_bind),&next_expert_list_bind},
    {GA(GA_141ED88),GA(GA_5A9A60),reinterpret_cast<void *>(&expert_row_destroy),&next_expert_row_destroy},
    {GA(GA_141EDB0),GA(GA_649CD0),reinterpret_cast<void *>(&expert_row_leave),&next_expert_row_leave},
    {GA(GA_142C748),GA(GA_660E20),reinterpret_cast<void *>(&expert_slider_event),&next_expert_slider},
    {GA(GA_142C7B8),GA(GA_661470),reinterpret_cast<void *>(&expert_choice_event),&next_expert_choice},
    {GA(GA_142C780),GA(GA_661ED0),reinterpret_cast<void *>(&expert_number_event),&next_expert_number_event},
    {GA(GA_142C828),GA(GA_660F00),reinterpret_cast<void *>(&expert_number_confirm),&next_expert_number_confirm}
};
#ifdef LAYOUT_ROUTED
// User 2026-11-05 Layout compatibility design 8.3: in production the expert UI hooks go through the SAN14_Layout.dll
// manager. The 6566E0 entry patch is replaced by its three callers; 6362DB / 5F14DA are E8 registrations; all eight
// vtable slots are chained/yielded by the manager. Only the three non-UI links (2B0B69, 2A8D40, 173B44) stay here.
static bool expert_routed_site(unsigned site) {return site==GA(GA_658DF0) || site==GA(GA_638B8B) || site==GA(GA_5F3D3A);}
#else
static bool expert_routed_site(unsigned) {return false;}
#endif
struct ExpertCode {CodeLink links[expert_call_count];Obj relay;};
// local101 (user 2026-11-13 "인연버프를 현행 2칸거리(3헥스)에서 5헥스까지 조절 ... 안행 사거리 현행 2헥스에서 3헥스"):
// two battle rules, both edited in 설정3 and kept per save (local105: sidecar .ranges below, no ini).
//  link_range  2..4 (2 = original): the distance inside which the link count 2B1B10 gathers friendly units. The original
//              reads one constant and passes it to 2A9680(army, range, list); only that CALL (2B1C3E) is redirected.
//  wedge_range 2..3 (2 = original): the attack radius of the ranged formation 안행 (formation 3). The original computes
//              `(2C9080(force, formation) != 0) + 1` in place at each user; seven of those places get a stub that calls
//              range_attack_radius and skips the in-place arithmetic. 2C9330 (has the army reached its attack position)
//              keeps the original 2-hex rings: a moving 안행 unit still walks up to 2 hexes, it only engages from 3.
// range_rules (values, load, store): native_expert_settings.inl
static void __fastcall range_link_units(Obj army,int range,Obj list,Obj fourth) {
    const LONG wanted=range_rules::link_range;
    fn<void(__fastcall *)(Obj,int,Obj,Obj)>(GA(GA_2A9680))(army,wanted>2 && wanted<=4?static_cast<int>(wanted):range,list,fourth);
}
static int __fastcall range_attack_radius(Obj force,Obj formation) {
    if(!fn<int(__fastcall *)(Obj,Obj)>(GA(GA_2C9080))(force,formation))return 1;
    if(range_rules::wedge_range==3 && formation && fn<int(__fastcall *)(Obj)>(GA(GA_22F7A0))(formation)==3)return 3;
    return 2;
}
// guard = the CALL and the in-place arithmetic that the stub replaces; ops = how the stub hands the radius (eax) over.
struct RangeSite {unsigned site;unsigned char guard[22];unsigned char guard_size;unsigned char ops[8];unsigned char op_size,skip;};
static const RangeSite range_sites[]={
    {GA(GA_16DCE8),{0xE8,0x93,0xB3,0x15,0x00,0x85,0xC0,0x0F,0x95,0xC3,0xFF,0xC3},12,{0x8B,0xD8},2,7},                              // ebx
    {GA(GA_16DD0F),{0xE8,0x6C,0xB3,0x15,0x00,0x85,0xC0,0x0F,0x95,0xC3,0xFF,0xC3},12,{0x8B,0xD8},2,7},                              // ebx
    {GA(GA_170331),{0xE8,0x4A,0x8D,0x15,0x00,0x33,0xDB,0x85,0xC0,0x0F,0x95,0xC3,0xFF,0xC3},14,{0x8B,0xD8},2,9},                    // ebx
    {GA(GA_1F8B7),{0xE8,0xC4,0x97,0x2A,0x00,0xBF,0x01,0x00,0x00,0x00,0xB9,0x02,0x00,0x00,0x00,0x85,0xC0,0x0F,0x45,0xF9},20,
        {0x8B,0xF8,0xB9,0x02,0x00,0x00,0x00},7,15},                                                                              // edi, ecx=2
    {GA(GA_2CB971),{0xE8,0x0A,0xD7,0xFF,0xFF,0x85,0xC0,0x74,0x02,0xFF,0xC7},11,{0x03,0xF8,0xFF,0xCF},4,6},                         // edi+=radius-1
    {GA(GA_2CCACC),{0xE8,0xAF,0xC5,0xFF,0xFF,0x85,0xC0,0xB9,0x01,0x00,0x00,0x00,0xBD,0x02,0x00,0x00,0x00,0x0F,0x45,0xCD,0x8B,0xE9},22,
        {0x8B,0xE8,0x8B,0xC8},4,17},                                                                                             // ebp, ecx
    {GA(GA_88CFF4),{0xE8,0x87,0xC0,0xA3,0xFF,0x33,0xC9,0x85,0xC0,0x0F,0x95,0xC1,0xFF,0xC1},14,{0x8B,0xC8},2,9}                     // ecx
};
static constexpr unsigned range_site_count=sizeof(range_sites)/sizeof(range_sites[0]);
static const unsigned char range_link_guard[]={0x8B,0x15,0xF1,0xB5,0x79,0x01,0x49,0x8B,0xCE,0xE8,0x3D,0x7A,0xFF,0xFF};   // mov edx,[range]; mov rcx,r14; call 2A9680
struct RangeCode {CallLink link,sites[range_site_count];bool ready=false;};
static RangeCode range_code;
// sub rsp,28h; mov rax,range_attack_radius; call rax; add rsp,28h; <ops>; add qword [rsp],skip; ret
static unsigned range_emit_stub(Obj at,const RangeSite &s) {
    unsigned n=0;
    const unsigned char head[]={0x48,0x83,0xEC,0x28,0x48,0xB8};
    memcpy(at+n,head,sizeof head);n+=sizeof head;
    void *target=reinterpret_cast<void *>(&range_attack_radius);memcpy(at+n,&target,8);n+=8;
    const unsigned char call[]={0xFF,0xD0,0x48,0x83,0xC4,0x28};
    memcpy(at+n,call,sizeof call);n+=sizeof call;
    memcpy(at+n,s.ops,s.op_size);n+=s.op_size;
    const unsigned char tail[]={0x48,0x83,0x04,0x24,s.skip,0xC3};
    memcpy(at+n,tail,sizeof tail);n+=sizeof tail;
    return n;
}
// relay: the expert relay page, offsets 1024.. (range_link_units jump) and 1088+i*64 (stubs). Any guard mismatch leaves
// both rules original (range_rules_skipped) and never fails the expert hooks.
static void prepare_range(Obj relay) {
    range_code=RangeCode{};
    if(ga::mcmp_at(image,GA(GA_2B1C3E)-9,range_link_guard,sizeof range_link_guard)) {log_event("range_rules_skipped",0,0,0);return;}
    for(unsigned i=0;i<range_site_count;++i)
        if(ga::mcmp_at(image,range_sites[i].site,range_sites[i].guard,range_sites[i].guard_size)) {log_event("range_rules_skipped",i+1,0,0);return;}
    absolute_jump(relay+1024,reinterpret_cast<void *>(&range_link_units));
    bool ok=call_prepare(range_code.link,GA(GA_2B1C3E),relay+1024);
    for(unsigned i=0;i<range_site_count && ok;++i) {
        range_emit_stub(relay+1088+i*64,range_sites[i]);
        ok=call_prepare(range_code.sites[i],range_sites[i].site,relay+1088+i*64);
    }
    range_code.ready=ok;
    if(!ok)log_event("range_rules_skipped",100,0,0);
}
static void publish_range() {
    if(!range_code.ready)return;
    bool ok=call_publish(range_code.link);
    for(unsigned i=0;i<range_site_count && ok;++i)ok=call_publish(range_code.sites[i]);
    log_event(ok?"range_rules_installed":"range_rules_partial",static_cast<unsigned>(range_rules::link_range),static_cast<unsigned>(range_rules::wedge_range),0);
}
// local105 (user 2026-11-13 "세이브 파일에 저장해야지"): sidecar <save>.<sha256>.ranges = "RNGR" + version 1 + 3 pad + 32-byte
// binding + link_range (2..4) + wedge_range (2..3) + 6 pad. A load without it (older saves) puts both back to original 2/2.
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
namespace range_rules {
static const size_t kSideSize=48;
static LONG saved_link=2,saved_wedge=2;
static SidecarCache side_cache;
static bool side_write(const merit::SaveCoordinator::Completed &job,save_host::Written &out) {
    std::vector<uint8_t> bytes(kSideSize,0);
    memcpy(bytes.data(),"RNGR",4);bytes[4]=1;memcpy(bytes.data()+8,job.binding.data(),32);
    bytes[40]=static_cast<uint8_t>(saved_link);bytes[41]=static_cast<uint8_t>(saved_wedge);
    return save_host::store(self_module,save_host::Job{job.name,job.binding},L"ranges",bytes,side_cache,out);
}
static bool side_restore(const merit::SaveCoordinator::Completed *job) {
    LONG link=2,wedge=2;bool restored=false;
    wchar_t path[MAX_PATH];
    if(job && save_host::path(self_module,save_host::Job{job->name,job->binding},L"ranges",path,MAX_PATH,false)) {
        HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {
            uint8_t bytes[kSideSize]={};DWORD got=0;LARGE_INTEGER length={};
            if(GetFileSizeEx(file,&length) && length.QuadPart==static_cast<LONGLONG>(kSideSize) &&
               ReadFile(file,bytes,static_cast<DWORD>(kSideSize),&got,nullptr) && got==kSideSize &&
               !memcmp(bytes,"RNGR",4) && bytes[4]==1 && !memcmp(bytes+8,job->binding.data(),32) &&
               bytes[40]>=2 && bytes[40]<=4 && bytes[41]>=2 && bytes[41]<=3) {link=bytes[40];wedge=bytes[41];restored=true;}
            CloseHandle(file);
        }
    }
    InterlockedExchange(&link_range,link);InterlockedExchange(&wedge_range,wedge);clear_pending();
    log_event("range_rules_load",static_cast<unsigned>(link),static_cast<unsigned>(wedge),restored?1u:0u);
    return restored;
}
static const save_host::SidecarPlugin sidecar_plugin={"ranges",L"ranges",1,"range_rules_save_written","range_rules_save_write_failed",
    []() {return mod_features.battle_rules;},
    []() {saved_link=link_range;saved_wedge=wedge_range;},
    [](const save_host::Job &job,save_host::Written &out) {return side_write(*static_cast<const merit::SaveCoordinator::Completed *>(job.source),out);},
    []() {side_cache.clear();},
    [](const save_host::Job *job) {return side_restore(job?static_cast<const merit::SaveCoordinator::Completed *>(job->source):nullptr);}};
}
#endif
static void restore_range() {
    for(unsigned i=range_site_count;i>0;--i)call_restore(range_code.sites[i-1]);
    call_restore(range_code.link);
    range_code.ready=false;
}
static bool expert_guards_match() {
    for(const auto &e:expert_calls) {
#ifdef LAYOUT_ROUTED
        // The two routed E8 sites are judged by the manager (chain/refuse); the 6566E0 entry must stay original.
        if(e.site==GA(GA_638B8B) || e.site==GA(GA_5F3D3A))continue;
#endif
        if(ga::mcmp_at(image,e.site,e.bytes,e.size))return false;
    }
#ifndef LAYOUT_ROUTED
    for(const auto &e:expert_pointers)if(pointer(image,e.slot)!=image+e.original)return false;
#endif
    return true;
}
static bool prepare_expert(ExpertCode &code) {
    if(!expert_guards_match())return false;
    code.relay=allocate_near(image+GA(GA_5BBA9E));if(!code.relay)return false;
    bool ready=true;
    for(unsigned i=0;i<expert_call_count && ready;++i) {
        absolute_jump(code.relay+i*32,expert_calls[i].replacement);
        if(expert_routed_site(expert_calls[i].site))continue;
        ready=prepare_link(code.links[i],expert_calls[i].site,expert_calls[i].opcode,code.relay+i*32);
    }
    memcpy(code.relay+256,expert_calls[1].bytes,6);
    absolute_jump(code.relay+262,image+GA(GA_658DF6));
    original_expert_populate=reinterpret_cast<ExpertPopulate>(code.relay+256);
    memcpy(code.relay+512,expert_calls[3].bytes,7);
    absolute_jump(code.relay+519,image+GA(GA_2AAF37));
    original_expert_unit_power=reinterpret_cast<ExpertUnitPower>(code.relay+512);
    if(ready)for(unsigned i=0;i<expert_call_count;++i)
        if(!expert_routed_site(expert_calls[i].site))
            for(unsigned n=5;n<expert_calls[i].size;++n)code.links[i].modified[n]=0x90;
    if(ready)prepare_range(code.relay);
    DWORD old=0;
    if(!ready || !VirtualProtect(code.relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),code.relay,4096)) {
        VirtualFree(code.relay,0,MEM_RELEASE);code={};return false;
    }
    return true;
}
#ifdef LAYOUT_ROUTED
static bool register_expert_routed() {
    // Calls first (populate sites, number_current, number_factory), then the vtable slots (same order as the former links).
    if(layout_client::call(GA(GA_5F3957),reinterpret_cast<void *>(&expert_populate_site<0>),&next_expert_populate[0])<=0 ||
       layout_client::call(GA(GA_5F8357),reinterpret_cast<void *>(&expert_populate_site<1>),&next_expert_populate[1])<=0 ||
       layout_client::call(GA(GA_6610BD),reinterpret_cast<void *>(&expert_populate_site<2>),&next_expert_populate[2])<=0 ||
       layout_client::call(GA(GA_638B8B),reinterpret_cast<void *>(&expert_number_current),&next_expert_number_current)<=0 ||
       layout_client::call(GA(GA_5F3D3A),reinterpret_cast<void *>(&expert_number_factory),&next_expert_number_factory)<=0)return false;
    for(const auto &e:expert_pointers)
        if(layout_client::vt(e.slot,e.replacement,e.next)<=0)return false;
    // PW30: optional 확인-button hooks (expert_ingame). A rejection keeps the expert hooks registered above.
    expert_ingame_install();
    normal_install();   // local102: two rows on the normal-mode 설정3; a rejection keeps everything above
    return true;
}
#endif
static bool publish_expert(ExpertCode &code) {
    for(unsigned i=0;i<expert_call_count;++i)
        if(!expert_routed_site(expert_calls[i].site) && !publish_link(code.links[i]))return false;
    publish_range();
#ifdef LAYOUT_ROUTED
    return register_expert_routed();
#else
    for(const auto &e:expert_pointers)if(!patch_pointer(e.slot,e.original,e.replacement))return false;
    return true;
#endif
}
static void restore_expert(ExpertCode &code) {
    restore_range();
#ifndef LAYOUT_ROUTED
    for(const auto &e:expert_pointers)restore_pointer(e.slot,e.original,e.replacement);
#endif
    for(unsigned i=expert_call_count;i>0;--i)if(!expert_routed_site(expert_calls[i-1].site))restore_link(code.links[i-1]);
    // A native UI callback may already have entered the relay. Retain it,
    // matching the existing split-module rollback lifetime rule.
}
