// Address / guard fixture (user 2026-11-05 patch resilience). Maps an UNPACKED game EXE from disk as the process image, selects the
// address table of that build, switches every feature on and runs the real module guards (split_feature_guards / split_guards_match
// of src/native_split_install.inl, address gates included). Prints which features stay on and which the guards or gates switch off.
// local11: the observer block at the end hooks the entries of ga_observers (San14Sim version.dll) and runs every guard again.
// Reads files only, never the live game. Built twice: /DSPLIT_BASE and /DSPLIT_COMBAT (plus /DSPLIT_MODULE_FIXTURE), like the DLL fixtures.
//   address_guard_fixture <unpacked game exe> <build index: 0 = 1fbe6be7, 1 = 1a4a447d>
#include "../src/split_module_api.h"
#include "../src/native_adapter.cpp"
#include <stdio.h>
// local10 G2 3.2: native_building_destruction.inl is not compiled here (adapter block 1854-1875), so its pure guard/form functions live in
// the profile header and are included on their own (pragma once keeps this harmless when the .inl is compiled too).
namespace building_destruction {
#include "../src/building_destruction_profile.h"
}
// local10 G4 5.1: same for the damage preview guard (native_damage_preview.inl is not compiled here either).
namespace damage_forecast {
#include "../src/damage_preview_profile.h"
}
// 2026-11-05 local3 (docs/INSTALL_LOCAL2_TRIAGE.md section 12): the convenience variant also runs the guards of the imported-code
// units (modpack, traits9: /DSPLIT_FIXTURE_MODPACK, generated payload headers from tools/build_addr_fixtures.ps1) and of the
// split_activate() installers (/DSPLIT_FIXTURE_ACTIVATE) against the real bytes of each build. Those used to be skipped by
// SPLIT_MODULE_FIXTURE, which is how the local2 rollback (imported code that only accepts the reference EXE) went unseen.
#ifdef SPLIT_FIXTURE_ACTIVATE
#include "../src/native_speed_buttons.inl"
#include "../src/native_stat_exp.inl"
#include "../src/native_dispatch_days.inl"
#include "../src/native_mediation_plus.inl"
#include "../src/native_lord_change.inl"
#include "../src/native_region_officers.inl"
#include "../src/native_ccz_editor.inl"
#include "../src/native_ccz_title.inl"
// local5: installers whose guards used to be skipped by SPLIT_MODULE_FIXTURE (name_lookup was rejected in the real game on 1a4a447d)
#include "../src/native_name_lookup.inl"
#include "../src/native_occupy_fast.inl"
#include "../src/native_caravan_stay.inl"
#include "../src/native_drag_learn.inl"
#endif

static unsigned char *map_exe(const char *path) {
    FILE *f=nullptr;
    if(fopen_s(&f,path,"rb")||!f)return nullptr;
    fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);
    std::vector<unsigned char> file(static_cast<size_t>(n));
    if(fread(file.data(),1,file.size(),f)!=file.size()) {fclose(f);return nullptr;}
    fclose(f);
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(file.data());
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64 *>(file.data()+dos->e_lfanew);
    unsigned size=nt->OptionalHeader.SizeOfImage;
    auto mapped=static_cast<unsigned char *>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!mapped)return nullptr;
    memcpy(mapped,file.data(),nt->OptionalHeader.SizeOfHeaders);
    auto sec=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)
        if(sec[i].SizeOfRawData)memcpy(mapped+sec[i].VirtualAddress,file.data()+sec[i].PointerToRawData,sec[i].SizeOfRawData);
    // rebase the absolute pointers (the guards compare slots with mapped+rva)
    long long delta=static_cast<long long>(reinterpret_cast<uintptr_t>(mapped))-static_cast<long long>(nt->OptionalHeader.ImageBase);
    auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned pos=0;
    while(dir.VirtualAddress && pos+8<=dir.Size) {
        auto block=reinterpret_cast<IMAGE_BASE_RELOCATION *>(mapped+dir.VirtualAddress+pos);
        if(block->SizeOfBlock<8)break;
        unsigned count=(block->SizeOfBlock-8)/2;
        auto entries=reinterpret_cast<unsigned short *>(block+1);
        for(unsigned i=0;i<count;++i) {
            unsigned type=entries[i]>>12,off=entries[i]&0xFFF;
            unsigned char *p=mapped+block->VirtualAddress+off;
            if(type==IMAGE_REL_BASED_DIR64)*reinterpret_cast<long long *>(p)+=delta;
            else if(type==IMAGE_REL_BASED_HIGHLOW)*reinterpret_cast<int *>(p)+=static_cast<int>(delta);
        }
        pos+=block->SizeOfBlock;
    }
    return mapped;
}
#define FLAGS(X) X(merit) X(awards) X(recruit) X(injury) X(recruit_ai) X(awards_ai) X(injury_herbal) X(domestic) X(population) \
    X(region_officers) X(domestic_free) X(region_officer_traits) X(domestic_direct) X(battle_rules) X(loyalty) X(terrain_edit) \
    X(building_destruction_all) X(diplomacy_overinvest) X(diplomacy_counterturn) X(damage_preview) X(traits9) X(tooltip_collab) \
    X(stats200) X(battle_deaths) X(scenery_skip) X(stats200_growth) X(speed_buttons) X(occupy_fast) X(fast_save) \
    X(caravan_stay) X(fast_menu) X(advisor_briefing) X(appear_batch) X(area_index) X(path_fast) X(face_keep) X(face_prefetch) \
    X(stat_exp) X(dispatch_days) X(mediation_plus) X(drag_learn) X(lord_change) X(distance) X(name_lookup) X(layout_keep) \
    X(ai_wall_block) X(ai_catapult_block) X(district_intercept_block) X(combat_cut) X(hit_response) X(obstacle) X(recall_veto) \
    X(sortie_formation) X(departures) X(group_advance) X(stationary_retreat) X(morale) X(support) X(mission_v2) X(builder_ruler_fill) \
    X(ai_max_aggression) X(attack_recheck_fix) X(ccz_sprites) X(ccz_block) X(ccz_critical)
int main(int argc,char **argv) {
    if(argc!=3) {fprintf(stderr,"usage: address_guard_fixture <exe> <build index>\n");return 2;}
    int b=atoi(argv[2]);
    if(b<0||b>1)return 2;
    image=map_exe(argv[1]);
    if(!image) {fprintf(stderr,"cannot map %s\n",argv[1]);return 2;}
    ga::select_build(b);
    auto reset_features=[] {
        memset(&mod_features,1,sizeof(mod_features));
        mod_features.trace_original=false;mod_features.ai_trace=false;mod_features.convenience=true;mod_features.ccz_far_keep=false;
        mod_features.timer_1ms=false;mod_features.terrain_diag_mode=0;mod_features.population_rate=4;mod_features.merit_troops_level=4;
    };
    reset_features();
    bool before[128]={};
    unsigned n=0;
#define SAVE(name) before[n++]=mod_features.name;
    FLAGS(SAVE)
#undef SAVE
    bool guards_ok=true;
    const char *guard_only[128]={};unsigned guard_only_n=0;
    std::string single_json;
#ifndef SPLIT_COMBAT
    // each guard on its own (before the gates), so a failing feature can be traced to the function whose bytes differ
    struct G {const char *name;bool ok;size_t from,to;};   // from/to: ga::uncovered() entries first seen while this guard ran (braced-init order)
    auto track=[](const char *name,bool ok) {
        static size_t last=0;size_t now=ga::uncovered().size();G g{name,ok,last,now};last=now;return g;
    };
    auto make_single=[&]()->std::vector<G> {return std::vector<G>{track("terrain_trees",terrain_trees_guards_match()),track("expert",expert_guards_match()),track("merit_save",merit_save_guards_match()),
        track("merit_gameplay",merit_gameplay_guards_match()),track("recruit_ui",recruit_ui_guards_match()),track("recruit_alert",recruit_alert_guards_match()),
        track("recruit_ai",recruit_ai_guards_match()),track("injury",injury_guards_match(true,false)),track("injury_herbal",injury_guards_match(false,true)),
        track("awards",award_guards_match()),track("loyalty",loyalty_guards_match()),track("area_index",area_index_guards_match()),track("path_fast",path_fast_guards_match()),track("path_fast_node_base",path_fast::node_base_ok()),
        track("building_destruction",building_destruction::guards_match()),track("damage_preview",damage_forecast::guards_match()),
        track("building_destruction_form",b==1?building_destruction::capture_form()==2:building_destruction::capture_form()==1),
        track("diplomacy_intel",diplomacy_intel_guards_match()),
        // local10 G4 5.2: 1a4a447d 324BC0 has the 8th (reduce) argument and the 70 percent global, 1fbe6be7 322B30 does not
        track("diplomacy_rate_form",b==1?diplomacy_intel::rate_reduce_rva()!=0 && *reinterpret_cast<int *>(image+diplomacy_intel::rate_reduce_rva())==70
            :diplomacy_intel::rate_reduce_rva()==0),
        // the four coefficient blocks (k9,k8,k5,k4 in code order) decode to exactly the ids coefficient_ids() uses; same layout in both builds
        track("diplomacy_coeff_ids",[]{
            const unsigned r=GA(GA_322B30),end=r+0x21F;
            auto rip=[](unsigned at,unsigned len){return at+len+static_cast<unsigned>(*reinterpret_cast<const int *>(image+at+len-4));};
            static const unsigned char s0[]={0x8B,0x0D},s1[]={0x44,0x8B,0x05},s2[]={0x44,0x8B,0x0D},s3[]={0x8B,0x15},s4[]={0x44,0x8B,0x15};
            unsigned starts[4]={},n=0;
            for(unsigned p=r;p+33<=end && n<4;) {
                if(!memcmp(image+p,s0,2) && !memcmp(image+p+6,s1,3) && !memcmp(image+p+13,s2,3) && !memcmp(image+p+20,s3,2) && !memcmp(image+p+26,s4,3)) {
                    starts[n++]=p;p+=33;
                } else ++p;
            }
            if(n!=4)return false;
            // dispatch: sub ebx,4 / je k4 ; sub ebx,1 / je k5 ; sub ebx,3 / je k8 ; cmp ebx,1 / jne exit ; fall through = k9
            const unsigned char d0[]={0x83,0xEB,0x04,0x74},d1[]={0x83,0xEB,0x01,0x74},d2[]={0x83,0xEB,0x03,0x74},d3[]={0x83,0xFB,0x01,0x0F,0x85};
            unsigned d=0;
            for(unsigned p=r;p+24<=end && !d;++p)
                if(!memcmp(image+p,d0,4) && !memcmp(image+p+5,d1,4) && !memcmp(image+p+10,d2,4) && !memcmp(image+p+15,d3,5))d=p;
            if(!d)return false;
            auto rel8=[&](unsigned at){return at+2+static_cast<unsigned>(static_cast<int>(static_cast<signed char>(image[at+1])));};
            if(rel8(d+3)!=starts[3] || rel8(d+8)!=starts[2] || rel8(d+13)!=starts[1] || d+24!=starts[0])return false;
            const int ks[4]={9,8,5,4};
            for(unsigned i=0;i<4;++i) {
                diplomacy_intel::CoefficientIds f;
                if(!diplomacy_intel::coefficient_ids(ks[i],f))return false;
                const unsigned p=starts[i];
                if(rip(p,6)!=f.divisor || rip(p+6,7)!=f.multiplier || rip(p+13,7)!=f.offset || rip(p+20,6)!=f.floor || rip(p+26,7)!=f.rate_divisor)return false;
            }
            return true;
        }()),track("advisor_briefing",advisor_briefing_guards_match()),track("terrain_edit",terrain_edit_guards_match())
#ifdef SPLIT_FIXTURE_MODPACK
        ,track("trait_hover",trait_hover_guards_ok()),track("battle_deaths",battle_deaths_guards_ok()),
        // BATTLE_DEATHS_PLAN 4-2: the four CALL rows (E8 -> original callee), the 295C5F span bytes and its je target, from the profile tables
        track("battle_deaths_rows",[]{
            for(const auto &st:battle_deaths_sites) {
                int rel=0;memcpy(&rel,image+st.site_rva+1,4);
                if(image[st.site_rva]!=0xE8 || static_cast<long long>(st.site_rva)+5+rel!=static_cast<long long>(st.original_rva))return false;
            }
            if(memcmp(image+battle_deaths_span_rva,battle_deaths_span_expect,battle_deaths_span_len)!=0)return false;
            int rel=0;memcpy(&rel,image+battle_deaths_span_rva+4,4);
            return static_cast<long long>(battle_deaths_span_rva)+8+rel==static_cast<long long>(battle_deaths_reject_rva);
        }()),track("modpack_stats200",modpack_guards_match(true,false,false,false)),
        track("modpack_growth",modpack_guards_match(false,true,false,false)),track("modpack_collab",modpack_guards_match(false,false,true,false)),
        track("modpack_scenery",modpack_guards_match(false,false,false,true)),track("traits9_slots",traits9_guards_match()),
        // local10 G3 4.1: the runtime `before` copy (guards passed first) equals the image of the running build for every core row and
        // site, and on the reference build the manifest literals too; the three rip-displacement sites differ from the literals on 1a4a447d
        track("traits9_before_runtime",[b]{
            if(!traits9_guards_match() || !traits9_copy_before())return false;
            for(size_t n=0;n<sizeof(traits9_core)/sizeof(traits9_core[0]);++n) {
                const auto &r=traits9_core_rt[n];
                if(!r.rva || memcmp(r.before,image+r.rva,r.size) || (b==0 && memcmp(r.before,traits9_core[n].before,r.size)))return false;
            }
            unsigned literal_differs=0;
            for(size_t n=0;n<3;++n)for(size_t i=0;i<trait9_manifests[n].site_count;++i) {
                const auto &p=traits9_states[n].patches[i];const auto &s=trait9_manifests[n].sites[i];
                if(p.rva!=s.rva || p.size!=s.size || memcmp(p.before,image+p.rva,p.size))return false;
                if(memcmp(p.before,s.before,s.size))++literal_differs;
            }
            return b==0?literal_differs==0:literal_differs==3;
        }()),
        // local10: 1fbe6be7 keeps the long JE+NOP6 (byte row), 1a4a447d the short JE+NOP3 whose JE target MOV ESI,100 is the relay site
        track("modpack_clamp_form",b==1?modpack_clamp_short()==GA(GA_230098)+0x47:modpack_clamp_short()==0),
        // local10 G1-a: the pristine image must never look like our own clamp write (the revert compares plain memcmp against `after`)
        track("modpack_clamp_restore_safe",[]{for(const auto &r:modpack_bytes)if(modpack_clamp_row(r))return memcmp(image+GA(GA_230098),r.after,8)!=0;return false;}())
#endif
#ifdef SPLIT_FIXTURE_ACTIVATE
        ,track("activate_speed_buttons",speed_buttons::guards_match()),track("activate_lord_change",lord_change::guards_match()),
        track("activate_region_officers",region_officers::guards_match()),track("activate_mediation_plus",mediation_plus::guards_match()),
        track("activate_stat_exp",stat_exp::guards_match()),track("activate_dispatch_days",dispatch_days::guards_match()),
        track("activate_ccz_title",ccz_title::guards_match()),track("activate_name_lookup",name_lookup_guards_match()),
        track("activate_occupy_fast",occupy_fast::guards_match()),track("activate_caravan_stay",caravan_stay::guards_match()),
        track("activate_drag_learn",drag_learn::guards_match())
#endif
    };};
    const std::vector<G> single=make_single();
    unsigned slots_bad=0;
    for(const auto &p:split_pointers)if(p.slot && p.original && pointer(image,p.slot)!=image+p.original)++slots_bad;
    printf("{\"guard_single\":{");
    for(size_t k=0;k<single.size();++k)printf("%s\"%s\":%s",k?",":"",single[k].name,single[k].ok?"true":"false");
    printf(",\"split_pointer_slots_bad\":%u}}\n",slots_bad);
    for(size_t k=0;k<single.size();++k) {   // the same results as a flat list for the final JSON line
        char entry[96];snprintf(entry,sizeof(entry),"%s\"%s\":%s",single_json.empty()?"":",",single[k].name,single[k].ok?"true":"false");
        single_json+=entry;
    }
#endif
    // pass 1: original-byte guards only (gates return at once when no id is unresolved)
    {
        unsigned saved=ga::state().unresolved;ga::state().unresolved=0;
        auto copy=mod_features;
#ifdef SPLIT_COMBAT
        split_guards_match();
#else
        split_install.save_ready=true;split_feature_guards();
#endif
        unsigned j=0;
#define GONLY(name) if(before[j++] && !mod_features.name)guard_only[guard_only_n++]=#name;
        FLAGS(GONLY)
#undef GONLY
        ga::state().unresolved=saved;mod_features=copy;
    }
    // pass 2: guards plus address gates
#ifdef SPLIT_MODPACK
    trait_hover_off=false;
#endif
#ifdef SPLIT_COMBAT
    guards_ok=split_guards_match();
    const char *mode="combat";
#else
    split_install.save_ready=true;
    split_feature_guards();
    const char *mode="convenience";
#endif
    // Exactness of the known-build guards (B stage 2026-11-05): on the second known build ga::mcmp / ga::mcmp_at compare the exact bytes of
    // that build, so a changed byte inside a relocatable field (rel32 of the CALL at 2B07C0+0x12) is seen. Reference build: -1 (not run).
    int exact_pristine=-1,exact_tampered=-1,exact_at_pristine=-1,exact_at_tampered=-1;
    if(b==1 && GA(GA_2B07C0)) {
        const unsigned char dummy[8]={};
        unsigned rv=GA(GA_2B07C0);
        unsigned char *p=image+rv+19,keep=*p;
        exact_pristine=ga::mcmp(image,GA_2B07C0,18,dummy,5);
        exact_at_pristine=ga::mcmp_at(image,rv+18,dummy,5);
        *p=static_cast<unsigned char>(keep^1);
        exact_tampered=ga::mcmp(image,GA_2B07C0,18,dummy,5)!=0;
        exact_at_tampered=ga::mcmp_at(image,rv+18,dummy,5)!=0;
        *p=keep;
    }
    // the entry-hook relay copies the 5 displaced bytes from the image: the fifth byte is push r15 (0x57) in 1fbe6be7, push r14 (0x56) in 1a4a447d
    const int adjacency_push=GA(GA_2B07C0)?image[GA(GA_2B07C0)+4]:-1;
    printf("{\"adjacency_push\":%d,\"exact_pristine\":%d,\"exact_tampered\":%d,\"exact_at_pristine\":%d,\"exact_at_tampered\":%d,\"status\":\"address_guard_fixture_done\",\"build\":\"%s\",\"module\":\"%s\",\"guards_ok\":%s,\"unresolved_ids\":%u,\"disabled\":[",
        adjacency_push,exact_pristine,exact_tampered,exact_at_pristine,exact_at_tampered,
        ga_builds[b].name,mode,guards_ok?"true":"false",ga::state().unresolved);
    unsigned i=0,out=0;
#define SHOW(name) if(before[i++] && !mod_features.name)printf("%s\"%s\"",out++?",":"",#name);
    FLAGS(SHOW)
#undef SHOW
    // hover is an internal flag of tooltip_collab (no setting): reported as its own entry so the expected list names it
    bool hover_off=false;
#ifdef SPLIT_MODPACK
    hover_off=trait_hover_off && mod_features.tooltip_collab;   // hover is only an entry of its own while tooltip_collab (collab slots) stays on
    if(hover_off)printf("%s\"hover\"",out++?",":"");
#endif
    printf("],\"hover_off\":%s,\"disabled_by_guard_only\":[",hover_off?"true":"false");
    for(unsigned k=0;k<guard_only_n;++k)printf("%s\"%s\"",k?",":"",guard_only[k]);
    std::string uncovered_json="{";
    {
        auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(image);
        auto nt=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+dos->e_lfanew);
        unsigned text_lo=0,text_hi=0;
        auto sec=IMAGE_FIRST_SECTION(nt);
        for(unsigned k=0;k<nt->FileHeader.NumberOfSections;++k)if(!memcmp(sec[k].Name,".text",5)) {text_lo=sec[k].VirtualAddress;text_hi=text_lo+sec[k].Misc.VirtualSize;}
        auto emit=[&](const char *name,size_t from,size_t to) {   // only .text ranges: data-id guards compare reference bytes exactly (no relocatable fields)
            std::string list;
            for(size_t k=from;k<to && k<ga::uncovered().size();++k) {
                const auto &u=ga::uncovered()[k];
                if(u.first<text_lo || u.first>=text_hi)continue;
                char entry[48];snprintf(entry,sizeof(entry),"%s[%u,%u]",list.empty()?"":",",u.first,u.second);list+=entry;
            }
            if(list.empty())return;
            uncovered_json+=std::string(uncovered_json.size()>1?",":"")+"\""+name+"\":["+list+"]";
        };
#ifndef SPLIT_COMBAT
        for(const auto &g:single)emit(g.name,g.from,g.to);
        emit("feature_guards",single.back().to,ga::uncovered().size());
#else
        emit("combat_guards",0,ga::uncovered().size());
#endif
    }
    uncovered_json+="}";
    // local11 (docs/INSTALL_LOCAL2_TRIAGE.md section 18): shared read-only observer tolerance. San14Sim's version.dll replaces the entry of 24
    // original functions (ga_observers: 23 traced functions + the daily-progress entry) by E9 rel32 + NOP padding that leaves the image.
    // The same bytes are written into the mapped image here: with ALL entries hooked every guard, the whole-text readiness and the imported-code
    // judgement must give exactly the pristine result; an E9 into the game, other padding, a changed byte behind the prefix and a longer prefix
    // must still be rejected. Fields of "observer" below are all true in a passing run (tools/check_addresses.py (c3)).
    std::string observer_json="null";
    {
        auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(image);
        const unsigned image_size=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+dos->e_lfanew)->OptionalHeader.SizeOfImage;
        constexpr unsigned N=GA_OBSERVER_COUNT;
        unsigned rv[N],len[N];
        unsigned char keep[N][16];
        bool table_ok=true;   // the generated original prefix bytes equal the bytes of this build's unpacked EXE
        for(unsigned ei=0;ei<N;++ei) {
            rv[ei]=ga_observers[ei].rva[b];len[ei]=ga_observers[ei].len;memcpy(keep[ei],image+rv[ei],16);
            if(memcmp(image+rv[ei],ga_observers[ei].bytes,len[ei]))table_ok=false;
        }
        auto index_of=[&](unsigned rva) {for(unsigned ei=0;ei<N;++ei)if(rv[ei]==rva)return ei;return N;};
        // E9 rel32 to target_rva, bytes [5,pad_len) = pad (the observer writes exactly len bytes: pad_len==len)
        auto hook=[&](unsigned ei,unsigned long long target_rva,unsigned pad_len,unsigned char pad) {
            unsigned char *e=image+rv[ei];
            memset(e+5,pad,pad_len>5?pad_len-5:0);e[0]=0xE9;
            int rel=static_cast<int>(static_cast<long long>(target_rva)-static_cast<long long>(rv[ei]+5));memcpy(e+1,&rel,4);
        };
        auto restore=[&] {for(unsigned ei=0;ei<N;++ei)memcpy(image+rv[ei],keep[ei],16);};
        auto outside=[&](unsigned ei) {return image_size+0x1000ull+ei*0x40ull;};
        auto text_ok=[&] {ga::ObserverSet set;return crc32_text_observed(set)==text_crc_now();};
        auto ready_exact=[&] {text_exact=false;CodeReadiness r={};const bool first=code_unpacked_ready(r);return first && text_exact;};
        auto raw_crc_ok=[&] {return crc32(image+0x1000,text_size_now())==text_crc_now();};
        bool all_singles_unchanged=true,all_features_unchanged=true,each_ok=true,deaths_pristine=true,deaths_all=true;
        bool bd_inside_fails=true,adv_inside_fails=true,tail_fails=true,pad_fails=true,longer_fails=true,int3_ok=true,adv_each_ok=true;
#ifndef SPLIT_COMBAT
        const unsigned ibd=index_of(GA(GA_1F42B0)),iadv0=index_of(GA(GA_448E40)),iadv1=index_of(GA(GA_44A0F0));
        auto guard_state=[&](std::string &singles,std::string &features) {
            singles.clear();features.clear();
            for(const auto &g:make_single()) {singles+=g.name;singles+=g.ok?"+":"-";singles+=",";}
            reset_features();
#ifdef SPLIT_MODPACK
            trait_hover_off=false;
#endif
            split_install.save_ready=true;split_feature_guards();
            unsigned j=0;
#define OBS_FEATURE(name) if(before[j++] && !mod_features.name){features+=#name;features+=",";}
            FLAGS(OBS_FEATURE)
#undef OBS_FEATURE
#ifdef SPLIT_MODPACK
            if(trait_hover_off)features+="hover,";
#endif
        };
        auto deaths_ok=[&](bool &ok) {
#ifdef SPLIT_MODPACK
            ok=battle_deaths_guards_ok();
#else
            ok=true;
#endif
        };
        std::string base_singles,base_features,s2,f2;
        guard_state(base_singles,base_features);
#endif
        const bool pristine_raw=raw_crc_ok(),pristine_ready=ready_exact();
#ifndef SPLIT_COMBAT
        deaths_ok(deaths_pristine);
#endif
        // (1) every entry hooked at once: the plain CRC is wrong (the local10 failure), the observed CRC is exact
        for(unsigned ei=0;ei<N;++ei)hook(ei,outside(ei),len[ei],0x90);
        const bool all_raw_differs=!raw_crc_ok(),all_text_ok=text_ok(),all_ready=ready_exact();
#ifndef SPLIT_COMBAT
        deaths_ok(deaths_all);
        guard_state(s2,f2);
        all_singles_unchanged=s2==base_singles;all_features_unchanged=f2==base_features;
        const bool all_bd=building_destruction::guards_match(),all_adv=advisor_briefing_guards_match();
#else
        const bool all_bd=true,all_adv=true;
#endif
        restore();
#ifndef SPLIT_COMBAT
        // (2) each entry alone: whole-text CRC, building_destruction and advisor_briefing guards, every guard of the module
        for(unsigned ei=0;ei<N;++ei) {
            hook(ei,outside(ei),len[ei],0x90);
            each_ok=each_ok && text_ok() && building_destruction::guards_match() && advisor_briefing_guards_match();
            guard_state(s2,f2);
            each_ok=each_ok && s2==base_singles && f2==base_features;
            restore();
        }
        // (3) rejections
        if(ibd<N) {
            hook(ibd,0x2000,len[ibd],0x90);   // JMP into the game image: another patch
            bd_inside_fails=!building_destruction::guards_match() && !text_ok();restore();
            hook(ibd,outside(ibd),len[ibd],0x90);image[rv[ibd]+len[ibd]]^=1;   // changed byte behind the prefix
            tail_fails=!building_destruction::guards_match() && !text_ok();restore();
            hook(ibd,outside(ibd),len[ibd]+1,0x90);   // observer longer than the table prefix: the byte behind it is no longer original
            longer_fails=!building_destruction::guards_match() && !text_ok();restore();
        } else bd_inside_fails=tail_fails=longer_fails=false;
        if(iadv0<N && iadv1<N) {
            hook(iadv0,0x2000,len[iadv0],0x90);adv_inside_fails=!advisor_briefing_guards_match();restore();
            hook(iadv0,outside(iadv0),len[iadv0],0x90);image[rv[iadv0]+len[iadv0]+3]^=1;
            tail_fails=tail_fails && !advisor_briefing_guards_match();restore();
            hook(iadv1,outside(iadv1),6,0x90);   // E9 + 1 NOP only: five original bytes of the 11-byte prefix remain
            pad_fails=!advisor_briefing_guards_match() && !text_ok();restore();
            hook(iadv1,outside(iadv1),len[iadv1],0xAA);   // padding that is neither NOP nor INT3
            pad_fails=pad_fails && !advisor_briefing_guards_match();restore();
            hook(iadv1,outside(iadv1),len[iadv1],0xCC);   // INT3 padding is accepted
            int3_ok=advisor_briefing_guards_match() && text_ok();restore();
        } else adv_inside_fails=pad_fails=int3_ok=false;
        adv_each_ok=advisor_briefing_guards_match();
#else
        if(index_of(GA(GA_44A0F0))<N) {
            const unsigned ei=index_of(GA(GA_44A0F0));
            hook(ei,0x2000,len[ei],0x90);bd_inside_fails=!text_ok();restore();
            hook(ei,outside(ei),len[ei],0x90);image[rv[ei]+len[ei]]^=1;tail_fails=!text_ok();restore();
            hook(ei,outside(ei),6,0x90);pad_fails=!text_ok();restore();
            hook(ei,outside(ei),len[ei]+1,0x90);longer_fails=!text_ok();restore();
            hook(ei,outside(ei),len[ei],0xCC);int3_ok=text_ok();restore();
        } else bd_inside_fails=tail_fails=pad_fails=longer_fails=int3_ok=false;
#endif
        // (4) a patch of an observed entry is a real conflict: the observer stays, the feature that wants the entry stays off
        bool conflict_blocks=false,pristine_link=false;
        {
            const unsigned ei=index_of(GA(GA_44A0F0))<N?index_of(GA(GA_44A0F0)):0;
            CodeLink link;
            pristine_link=prepare_link(link,rv[ei],0xE9,image+0x40);
            hook(ei,outside(ei),len[ei],0x90);
            conflict_blocks=!prepare_link(link,rv[ei],0xE9,image+0x40) && !prepare_bytes(link,rv[ei]+2,keep[ei],3) && ga::observer_overlaps(image,rv[ei],5);
            restore();
        }
        const bool pristine_after=raw_crc_ok() && ga::observer_overlaps(image,rv[0],5)==false;
        char text[1200];
        snprintf(text,sizeof(text),"{\"count\":%u,\"table_ok\":%s,\"pristine_raw\":%s,\"pristine_ready\":%s,\"deaths_pristine\":%s,\"all_raw_differs\":%s,\"all_text_ok\":%s,\"all_ready\":%s,"
            "\"deaths_all\":%s,\"all_bd\":%s,\"all_adv\":%s,\"all_singles_unchanged\":%s,\"all_features_unchanged\":%s,\"each_ok\":%s,\"inside_fails\":%s,\"adv_inside_fails\":%s,"
            "\"tail_fails\":%s,\"pad_fails\":%s,\"longer_fails\":%s,\"int3_ok\":%s,\"conflict_blocks\":%s,\"pristine_link\":%s,\"adv_pristine\":%s,\"restored\":%s}",
            N,table_ok?"true":"false",pristine_raw?"true":"false",pristine_ready?"true":"false",deaths_pristine?"true":"false",all_raw_differs?"true":"false",
            all_text_ok?"true":"false",all_ready?"true":"false",deaths_all?"true":"false",all_bd?"true":"false",all_adv?"true":"false",all_singles_unchanged?"true":"false",
            all_features_unchanged?"true":"false",each_ok?"true":"false",bd_inside_fails?"true":"false",adv_inside_fails?"true":"false",tail_fails?"true":"false",
            pad_fails?"true":"false",longer_fails?"true":"false",int3_ok?"true":"false",conflict_blocks?"true":"false",pristine_link?"true":"false",adv_each_ok?"true":"false",
            pristine_after?"true":"false");
        observer_json=text;
    }
    printf("],\"observer\":%s,\"uncovered\":%s,\"guard_single\":{%s}}\n",observer_json.c_str(),uncovered_json.c_str(),single_json.c_str());
    return 0;
}
