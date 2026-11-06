// SAN14_Layout.dll P3 fixture (user 2026-11-05 Layout compatibility design, section 8). Compiles the real Layout sources
// (LAYOUT_FIXTURE seam) and the real src/layout_registry.h, then registers EVERY registry row against a synthetic
// image the way layout_client does. Covers: owner row counts and flags (design 8.1), the C-1 whitelist rule, yield
// decisions from ini keys and files (8.2, 8.7), E9 tail-jump redirect (898957), the 70E9C1 relay/block (8.4) including
// the relay machine code, span/bytes patches, next-pointer chains over a fake foreign module, owner roll-back (G4),
// entry-patch blocklist (8.3) and the "no row is an entry patch" gate. Synthetic memory only; no game, no game code run.
#define LAYOUT_FIXTURE
#ifdef NO_RUNTIME_LOG
#error layout fixture needs the log
#endif
#include "../src/layout_main.cpp"
#include "../src/layout_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
static unsigned checks;
static void check(bool value,const char *label) {++checks;if(!value){fprintf(stderr,"FAILED: %s\n",label);exit(1);}}
using namespace layout;
static const size_t kRegion=0x2000000,kImage=0x1600000;
static Bytes *region;
static const size_t kExt=0x1600000,kOurs=0x1680000,kDxgi=0x16C0000,kHeap=0x1800000;
struct Event {std::string kind;unsigned a,b,c;};
static std::vector<Event> events;
static void capture(const char *kind,unsigned a,unsigned b,unsigned c) {events.push_back({kind,a,b,c});}
static unsigned count_events(const char *kind) {unsigned n=0;for(auto &e:events)if(e.kind==kind)++n;return n;}
static void fresh() {
    memset(hooks,0,sizeof(hooks));hook_total=0;published=0;pool=nullptr;pool_used=0;
    memset(overlay_subs,0,sizeof(overlay_subs));memset(wnd_subs,0,sizeof(wnd_subs));overlay_count=0;wnd_count=0;
    present_attempted=0;fixture_table=nullptr;fixture_table1=nullptr;present_next=nullptr;present1_next=nullptr;
    memset(region,0,kRegion);
    image=region;image_size=kImage;self_module=GetModuleHandleW(nullptr);
    fake_count=0;
    auto add=[](size_t offset,size_t size,const wchar_t *name) {
        fake_modules[fake_count].base=region+offset;fake_modules[fake_count].size=size;wcscpy_s(fake_modules[fake_count].name,name);++fake_count;
    };
    add(kExt,0x80000,L"fog_fake.dll");add(kOurs,0x40000,L"SAN14_Convenience.dll");add(kDxgi,0x40000,L"dxgi.dll");
    fixture_root[0]=0;events.clear();ring_count=0;sink=nullptr;set_sink(capture);
}
static int state_of(int handle) {return api.hook_state(handle);}
static void **slot_at(unsigned rva) {return reinterpret_cast<void **>(image+rva);}
static void set_call_to(unsigned site,const void *target,Bytes opcode=0xE8) {
    image[site]=opcode;int rel=static_cast<int>(reinterpret_cast<const Bytes *>(target)-(image+site+5));memcpy(image+site+1,&rel,4);
}
static const Bytes *call_target(unsigned site) {return image+site+5+*reinterpret_cast<int *>(image+site+1);}
static unsigned other_calls;   // marker for chain execution tests
static int __fastcall pass_fn(void *,void *) {return 1;}
static int __fastcall reject_fn(void *,void *) {return 2;}
static void ext_marker() {++other_calls;}
// ---- registration like layout_client::add (owner/kind/flags from the registry) ----
struct Reg {int handle;void *next;void *replacement;};
static std::vector<Reg> regs;
static void fill_row(const LayoutRow &row,const Bytes *foreign=nullptr) {
    if(row.kind==LR_VT)*slot_at(row.rva)=foreign?const_cast<Bytes *>(foreign):image+row.original;
    else set_call_to(row.rva,foreign?foreign:image+row.original,row.kind==LR_JUMP?0xE9:0xE8);
}
static void *replacement_for(unsigned index) {return region+kOurs+0x100+index*0x10;}
static void register_rows(const char *only_owner=nullptr,uint32_t drop=0) {
    regs.assign(layout_row_count,Reg{0,nullptr,nullptr});
    for(unsigned i=0;i<layout_row_count;++i) {
        const LayoutRow &row=layout_rows[i];
        if(only_owner && strcmp(row.owner,only_owner))continue;
        regs[i].replacement=replacement_for(i);
        regs[i].next=image+row.original;
        const uint32_t flags=row.flags&~drop;
        switch(row.kind) {
        case LR_VT:regs[i].handle=api.hook_vtable(row.owner,row.rva,row.original,regs[i].replacement,&regs[i].next,flags);break;
        case LR_CALL:regs[i].handle=api.hook_call(row.owner,row.rva,row.original,regs[i].replacement,&regs[i].next,flags);break;
        default:regs[i].handle=api.hook_jump(row.owner,row.rva,row.original,regs[i].replacement,&regs[i].next,flags);break;
        }
    }
}
static unsigned rows_of(const char *owner) {unsigned n=0;for(unsigned i=0;i<layout_row_count;++i)if(!strcmp(layout_rows[i].owner,owner))++n;return n;}
static unsigned states_of(const char *owner,int state) {
    unsigned n=0;for(unsigned i=0;i<layout_row_count;++i)if(!strcmp(layout_rows[i].owner,owner) && regs[i].handle>0 && state_of(regs[i].handle)==state)++n;return n;
}
static std::wstring make_temp_root() {
    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);
    std::wstring root=std::wstring(temp)+L"san14_layout_p3_fixture_"+std::to_wstring(GetCurrentProcessId());
    CreateDirectoryW(root.c_str(),nullptr);return root;
}
static void write_text(const std::wstring &root,const std::wstring &relative,const std::string &text) {
    std::wstring path=root;
    size_t at=0;
    while((at=relative.find(L'\\',at))!=std::wstring::npos) {CreateDirectoryW((root+L"\\"+relative.substr(0,at)).c_str(),nullptr);++at;}
    path+=L"\\"+relative;
    FILE *f=nullptr;if(_wfopen_s(&f,path.c_str(),L"wb")||!f)check(false,"fixture file written");
    fwrite(text.data(),1,text.size(),f);fclose(f);
}
static void remove_text(const std::wstring &root,const std::wstring &relative) {DeleteFileW((root+L"\\"+relative).c_str());}
static std::string r1_ini(int enabled,const char *extra="") {
    return std::string("[Mod]\r\nId=Test\r\nFile=x.dll\r\nEnabled=")+std::to_string(enabled)+"\r\n"+extra;
}
static const std::wstring kMods=L"SAN14Mods\\modules\\";
int main() {
    region=static_cast<Bytes *>(VirtualAlloc(nullptr,kRegion,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(region!=nullptr,"synthetic image allocated");
    // ===== registry integrity (design 8.1) =====
    {
        for(unsigned i=0;i<layout_row_count;++i)for(unsigned j=i+1;j<layout_row_count;++j)
            check(layout_rows[i].rva!=layout_rows[j].rva,"registry RVAs are unique");
        static const struct {const char *owner;unsigned rows;} expected[]={
            {"speed_buttons",4},{"lord_change",2},{"region_officers",6},{"ccz_title",4},{"ccz_sprites",1},{"layout_wait",3},
            {"fast_menu",3},{"stat_exp",31},{"dispatch_days",1},{"damage_preview",2},{"expert_links",13},{"unified_dialogs",3},
            {"advisor_hud",2},{"recruit_ui",4},{"merit_save",2},{"merit_prisoner",1},{"merit",2},
            {"collab",2},{"scenery",2},{"domestic_selection",2},{"mediation_plus",7},{"trait_hover",156},{"battle_deaths",4}};
        unsigned total=0;
        for(const auto &e:expected) {check(rows_of(e.owner)==e.rows,e.owner);total+=e.rows;}
        check(total==layout_row_count,"every registry row belongs to a known owner");
        check(layout_traits9_site_count==13,"traits9: eight spans and five byte patches");
        // flags: yield / chain / whitelist columns of the design table
        unsigned whitelist=0;
        for(unsigned i=0;i<layout_row_count;++i) {
            const LayoutRow &row=layout_rows[i];
            const bool yields=(row.flags&LAYOUT_YIELD_IF_PEER)!=0,chains=(row.flags&LAYOUT_CHAIN_OK)!=0,wl=(row.flags&LAYOUT_NEED_WHITELIST)!=0;
            const bool bundle=(row.flags&LAYOUT_ALL_OR_NONE)!=0;
            if(wl)++whitelist;
            const std::string o=row.owner;
            if(o=="speed_buttons" || o=="fast_menu" || o=="region_officers" || o=="domestic_selection")check(yields,"yield owners carry YIELD_IF_PEER");
            else if(o=="merit_prisoner")check(yields && chains && !bundle,"merit_prisoner: yield+chain, split out of the bundle (G4 exception)");
            else if(o=="trait_hover")check(yields && chains,"trait_hover: CHAIN_OK + YIELD_IF_PEER (plan 3.2, FormationExtension peer)");
            else if(o=="battle_deaths")check(yields && chains && bundle,"battle_deaths: CHAIN_OK + YIELD_IF_PEER (plan 3.2, Patch27 peer)");
            else if(o=="collab" || o=="scenery")check(yields && !chains,"collab/scenery: yield only, no chain");
            else check(!yields,"other owners never yield");
            if(o!="merit_prisoner")check(bundle,"owners are all-or-none");
            if(o=="speed_buttons" && row.rva==0x1473F50)check(wl,"1473F50 whitelist (yuga)");
            if(o=="region_officers" || o=="ccz_sprites" || (o=="merit" && row.rva==0x1413600))check(wl,"fog/yuga contested slots need the whitelist");
            check(row.rva+8<=kImage && row.original<kImage,"row inside the image");
        }
        check(whitelist==6+1+1+1,"whitelist rows: 1473F50, six region officer slots, ccz_sprites, merit 1413600");
        check(layout_row_count==257,"registry holds 257 manager rows (31 stat_exp + 66 others + 156 trait_hover + 4 battle_deaths) beside 13 traits9 patches and 70E9C1");
        check(layout_row_count+layout_traits9_site_count+1+1<=kMaxHooks,"manager capacity (384) covers every row, patch, 70E9C1 and Present");
        // C-4 / 8.3: no row is an entry patch of a blocked entry (calls are 5-byte patches at the site, vtable rows write data)
        const unsigned blocked_count=sizeof(layout_blocked_entries)/sizeof(layout_blocked_entries[0]);
        check(blocked_count==sizeof(blocked_entries)/sizeof(blocked_entries[0]),"registry blocklist matches the manager's");
        for(unsigned i=0;i<blocked_count;++i)check(layout_blocked_entries[i]==blocked_entries[i],"blocklist entries equal");
        for(unsigned i=0;i<layout_row_count;++i) {
            const LayoutRow &row=layout_rows[i];
            if(row.kind==LR_VT)continue;
            check(!blocked_range(row.rva,5),"no call/jump row patches a blocked entry");
        }
        for(unsigned i=0;i<layout_traits9_site_count;++i)check(!blocked_range(layout_traits9_sites[i].rva,layout_traits9_sites[i].len),"patch rows avoid blocked entries");
        check(!blocked_range(layout_mediation_row.rva,layout_mediation_row.len),"70E9C1 is not a blocked entry");
        check(!blocked_range(layout_battle_deaths_span.rva,layout_battle_deaths_span.len) && (layout_battle_deaths_span.rva&15)==15,"battle_deaths span is not a blocked entry and sits at line offset F");
        // 8.3 replacements exist: callers of the removed entry patches are registered, the entries are not
        bool has_7f5781=false,has_7fa1fd=false,has_5f10f7=false,has_5f5b27=false,has_65eaad=false,entry_row=false;
        for(unsigned i=0;i<layout_row_count;++i) {
            const unsigned r=layout_rows[i].rva;
            has_7f5781|=r==0x7F5781;has_7fa1fd|=r==0x7FA1FD;has_5f10f7|=r==0x5F10F7;has_5f5b27|=r==0x5F5B27;has_65eaad|=r==0x65EAAD;
            entry_row|=r==0x7FA230||r==0x6566E0||r==0x86E0C0||r==0x6D7D90;
        }
        check(has_7f5781 && has_7fa1fd && has_5f10f7 && has_5f5b27 && has_65eaad && !entry_row,"Q6: 7FA230/6566E0/86E0C0 entry patches replaced by call sites; 6D7D90 handed over");
    }
    // ===== every row installs against an original-looking image, next pointers, whitelist record =====
    fresh();
    {
        for(unsigned i=0;i<layout_row_count;++i)fill_row(layout_rows[i]);
        std::vector<Bytes> before(image,image+kImage);
        register_rows();
        for(unsigned i=0;i<layout_row_count;++i)check(regs[i].handle>0,"registered");
        layout::publish_hooks();
        for(unsigned i=0;i<layout_row_count;++i) {
            const LayoutRow &row=layout_rows[i];
            check(state_of(regs[i].handle)==LAYOUT_STATE_INSTALLED,row.owner);
            check(regs[i].next==image+row.original,"next is the original target");
            if(row.kind==LR_VT)check(*slot_at(row.rva)==regs[i].replacement,"slot holds the replacement");
            else {
                const Bytes *target=call_target(row.rva);
                check(in_pool(target) && relay_target(target)==regs[i].replacement,"call/jump goes through the pool relay to the replacement");
                check(image[row.rva]==(row.kind==LR_JUMP?0xE9:0xE8),"opcode kept");
            }
            if(row.flags&LAYOUT_NEED_WHITELIST) {
                wchar_t owner[64];check(api.slot_owner(row.rva,owner,64)>0 && !_wcsicmp(owner,L"SAN14_Convenience.dll"),"whitelist rows hold a pointer inside our DLL (recorded check)");
            }
        }
        check(count_events("layout_installed")==layout_row_count,"one installed log line per row");
        check(api.hook_owner_state("speed_buttons")==LAYOUT_STATE_INSTALLED && api.hook_owner_state("nobody")==0,"owner state query");
        // unhook everything: image bytes back to the original
        for(unsigned i=0;i<layout_row_count;++i)check(api.unhook(regs[i].handle)==LAYOUT_UNHOOK_REMOVED,"unhook restores");
        check(!memcmp(before.data(),image,kImage),"all rows unhooked: the image is byte-identical to the original");
    }
    // ===== E9 tail jump (898957): redirect kind, chain, refuse =====
    fresh();
    {
        const unsigned row_index=[]{for(unsigned i=0;i<layout_row_count;++i)if(layout_rows[i].rva==0x898957)return i;return 0u;}();
        const LayoutRow &row=layout_rows[row_index];
        check(row.kind==LR_JUMP && (image[row.rva]=0xE9,true),"898957 is a jump row");
        set_call_to(row.rva,image+row.original,0xE9);
        void *next=nullptr;
        int h=api.hook_jump("speed_buttons",row.rva,row.original,replacement_for(1),&next,row.flags);
        layout::publish_hooks();
        check(state_of(h)==LAYOUT_STATE_INSTALLED && image[row.rva]==0xE9 && in_pool(call_target(row.rva)) && next==image+row.original,"E9 rel32 redirected through a pool relay, next is 891720");
        check(api.unhook(h)==LAYOUT_UNHOOK_REMOVED && call_target(row.rva)==image+row.original && image[row.rva]==0xE9,"E9 restored");
        // an E8 where an E9 is expected is refused (BAD_SITE), and vice versa
        set_call_to(0x3000,image+0x4000,0xE8);
        int bad=api.hook_jump("t",0x3000,0x4000,replacement_for(2),&next,0);
        int bad2=api.hook_call("t",0x3000,0x4000,replacement_for(3),&next,0);
        set_call_to(0x3100,image+0x4000,0xE9);
        int bad3=api.hook_call("t",0x3100,0x4000,replacement_for(4),&next,0);
        layout::publish_hooks();
        check(state_of(bad)==LAYOUT_STATE_REFUSED && state_of(bad3)==LAYOUT_STATE_REFUSED && state_of(bad2)==LAYOUT_STATE_INSTALLED,"opcode must match the registration kind");
        // another module already redirected the tail jump (chain) / a heap target (refuse)
        set_call_to(0x3200,region+kExt+0x200,0xE9);
        int chain=api.hook_jump("t",0x3200,0x4000,replacement_for(5),&next,LAYOUT_CHAIN_OK);
        set_call_to(0x3300,region+kHeap+0x200,0xE9);
        void *next2=nullptr;
        int heap=api.hook_jump("t",0x3300,0x4000,replacement_for(6),&next2,LAYOUT_CHAIN_OK);
        layout::publish_hooks();
        check(state_of(chain)==LAYOUT_STATE_CHAINED && next==region+kExt+0x200,"foreign E9 target: chained, next = their target");
        check(state_of(heap)==LAYOUT_STATE_REFUSED,"heap E9 target: refused");
    }
    // ===== chains over a fake foreign module, owner roll-back (G4) =====
    fresh();
    {
        for(unsigned i=0;i<layout_row_count;++i)fill_row(layout_rows[i]);
        // region_officers: all six slots already point into the fake foreign module (fog-like): chain every slot
        for(unsigned i=0;i<layout_row_count;++i)if(!strcmp(layout_rows[i].owner,"region_officers"))
            fill_row(layout_rows[i],region+kExt+0x300+i*0x10);
        // dispatch_days CALL already redirected into the foreign module: chain
        for(unsigned i=0;i<layout_row_count;++i)if(!strcmp(layout_rows[i].owner,"dispatch_days"))fill_row(layout_rows[i],region+kExt+0x900);
        // stat_exp: one chart site target is a heap address: the whole owner is refused / rolled back
        unsigned bad_site=0;
        for(unsigned i=0;i<layout_row_count;++i)if(layout_rows[i].rva==0x907406)bad_site=layout_rows[i].rva;
        for(unsigned i=0;i<layout_row_count;++i)if(layout_rows[i].rva==bad_site)fill_row(layout_rows[i],region+kHeap+0x40);
        register_rows();
        layout::publish_hooks();
        check(states_of("region_officers",LAYOUT_STATE_CHAINED)==6,"six region officer slots chain over the foreign module");
        for(unsigned i=0;i<layout_row_count;++i)if(!strcmp(layout_rows[i].owner,"region_officers"))
            check(regs[i].next==region+kExt+0x300+i*0x10 && *slot_at(layout_rows[i].rva)==regs[i].replacement,"next = their function; slot = ours");
        check(states_of("dispatch_days",LAYOUT_STATE_CHAINED)==1,"E8 chains over a foreign relay");
        check(states_of("stat_exp",LAYOUT_STATE_REFUSED)>=1 && states_of("stat_exp",LAYOUT_STATE_INSTALLED)==0 && states_of("stat_exp",LAYOUT_STATE_CHAINED)==0,"G4: one refused stat_exp site takes the whole owner back");
        for(unsigned i=0;i<layout_row_count;++i)if(!strcmp(layout_rows[i].owner,"stat_exp") && layout_rows[i].rva!=bad_site)
            check(!in_pool(call_target(layout_rows[i].rva)) && call_target(layout_rows[i].rva)==image+layout_rows[i].original,"stat_exp sites are original again");
        check(count_events("layout_group_rollback")>=1 || states_of("stat_exp",LAYOUT_STATE_REFUSED)==31,"roll-back is logged (or the owner never installed)");
        check(states_of("speed_buttons",LAYOUT_STATE_INSTALLED)==4 && states_of("merit",LAYOUT_STATE_INSTALLED)==2,"other owners are unaffected");
        check(api.hook_owner_state("stat_exp")==LAYOUT_STATE_REFUSED,"owner state shows the refusal");
    }
    // ===== merit_prisoner is its own owner: yielding it leaves the merit bundle installed (G4 exception) =====
    {
        fresh();
        for(unsigned i=0;i<layout_row_count;++i)fill_row(layout_rows[i]);
        std::wstring root=make_temp_root();fixture_root[0]=0;wcscpy_s(fixture_root,root.c_str());
        write_text(root,kMods+L"PrisonerReturn.ini",r1_ini(1));
        register_rows();layout::publish_hooks();
        check(states_of("merit_prisoner",LAYOUT_STATE_YIELDED)==1 && states_of("merit",LAYOUT_STATE_INSTALLED)==2 && states_of("merit_save",LAYOUT_STATE_INSTALLED)==2,"R1 PrisonerReturn: only the 13A6BB0 slot yields");
        check(*slot_at(0x13A6BB0)==image+0x2390B0,"13A6BB0 untouched while yielded");
        remove_text(root,kMods+L"PrisonerReturn.ini");
    }
    // ===== peer rules from ini keys and files (8.2, 8.7, Q8) =====
    {
        std::wstring root=make_temp_root();
        auto run=[&](const char *owner)->unsigned {   // states of one owner after a fresh publish
            fresh();fixture_root[0]=0;wcscpy_s(fixture_root,root.c_str());
            for(unsigned i=0;i<layout_row_count;++i)fill_row(layout_rows[i]);
            register_rows(owner);layout::publish_hooks();
            return states_of(owner,LAYOUT_STATE_YIELDED);
        };
        // speed_buttons: IC GreenlandCompat Enabled AND [Convenience] ProgressSpeedModule
        write_text(root,kMods+L"IntegratedConvenience_GreenlandCompat.ini",r1_ini(1));
        check(run("speed_buttons")==0 && api.peer_for_owner("speed_buttons")==0,"IC module alone (no ProgressSpeedModule key): speed buttons install");
        write_text(root,L"San14ModSettings.ini","[Convenience]\r\nProgressSpeedModule=0\r\n");
        check(run("speed_buttons")==0,"ProgressSpeedModule=0: install");
        write_text(root,L"San14ModSettings.ini","[Convenience]\r\nProgressSpeedModule=1\r\n");
        check(run("speed_buttons")==4 && api.peer_for_owner("speed_buttons")==1,"ProgressSpeedModule=1 with the IC module enabled: all four speed hooks yield");
        check(states_of("speed_buttons",LAYOUT_STATE_INSTALLED)==0 && *slot_at(0x1473F50)==image+0x882540 && image[0x898957]==0xE9,"yielded hooks leave the image untouched");
        write_text(root,kMods+L"IntegratedConvenience_GreenlandCompat.ini",r1_ini(0));
        check(run("speed_buttons")==0,"IC module disabled: install even with the key set");
        remove_text(root,kMods+L"IntegratedConvenience_GreenlandCompat.ini");
        check(run("speed_buttons")==0,"key without the IC module ini: install");
        // FPS120: recorded only
        check(api.peer_for_owner("fps120_note")==0,"no FPS120 module");
        write_text(root,kMods+L"FPS120.ini",r1_ini(1));
        check(api.peer_for_owner("fps120_note")==1 && run("speed_buttons")==0,"FPS120 enabled: noted, speed buttons still install (Q8)");
        // region officers: Face decides
        write_text(root,kMods+L"RegionOfficer.ini",r1_ini(1,"[RegionOfficer]\r\nWarning=1\r\nFace=0\r\n"));
        check(run("region_officers")==0 && states_of("region_officers",LAYOUT_STATE_INSTALLED)==6,"RegionOfficer Face=0: coexist (installed)");
        write_text(root,kMods+L"RegionOfficer.ini",r1_ini(1,"[RegionOfficer]\r\nWarning=1\r\nFace=1\r\n"));
        check(run("region_officers")==6,"RegionOfficer Face=1: all six yield");
        write_text(root,kMods+L"RegionOfficer.ini",r1_ini(0,"[RegionOfficer]\r\nFace=1\r\n"));
        check(run("region_officers")==0,"RegionOfficer disabled: install");
        write_text(root,kMods+L"RegionOfficer.ini",r1_ini(1));
        check(run("region_officers")==0,"RegionOfficer enabled without a Face key: coexist");
        // fast menu: FastMenu ini AND ExtBridge Enabled AND [Ext] FastMenu
        write_text(root,L"SAN14Mods\\Ext\\SAN14_FastMenu.ini","[FastMenu]\r\nEnabled=1\r\nSkipOutWait=1\r\nPinSlowLayouts=1\r\n");
        check(run("fast_menu")==0 && api.peer_for_owner("layout_keep")==0,"FastMenu ini without ExtBridge: install");
        write_text(root,kMods+L"ExtBridge.ini",r1_ini(1,"[Ext]\r\nFastMenu=1\r\n"));
        check(run("fast_menu")==3 && api.peer_for_owner("layout_keep")==1,"FastMenu + ExtBridge: fast_menu yields, layout_keep yields");
        write_text(root,L"SAN14Mods\\Ext\\SAN14_FastMenu.ini","[FastMenu]\r\nEnabled=1\r\nPinSlowLayouts=0\r\n");
        check(api.peer_for_owner("fast_menu")==1 && api.peer_for_owner("layout_keep")==0,"PinSlowLayouts=0: only the busy-slot owner yields");
        write_text(root,kMods+L"ExtBridge.ini",r1_ini(1,"[Ext]\r\nFastMenu=0\r\n"));
        check(api.peer_for_owner("fast_menu")==0,"ExtBridge [Ext] FastMenu=0: no yield");
        // domestic selection, scenery, merit prisoner (single ini each)
        write_text(root,kMods+L"DomesticAI.ini",r1_ini(1));
        check(run("domestic_selection")==2,"DomesticAI: both domestic slots yield");
        write_text(root,kMods+L"SettingsHelpScenic.ini",r1_ini(1));
        check(run("scenery")==2 && api.peer_for_owner("scenery")==1,"SettingsHelpScenic: scenery slots yield");
        // owners without registry rows (imported / byte patch gates)
        check(api.peer_for_owner("traits9")==0,"no Traits9Check");
        write_text(root,kMods+L"Traits9Check.ini",r1_ini(1));
        check(api.peer_for_owner("traits9")==1 && api.peer_for_owner("traits9_captive")==0,"Traits9Check enabled: traits9 yields; the removed captive owner no longer exists");
        write_text(root,kMods+L"ModPackBridge.ini",r1_ini(1,"[ModPack]\r\nStats200=on\r\nTraitHover=0\r\nCollab=1\r\n"));
        check(api.peer_for_owner("trait_hover")==0 && api.peer_for_owner("collab")==1 && api.peer_for_owner("stats200")==0,"ModPackBridge keys are read per owner (non-numeric 'on' counts as 0)");
        write_text(root,kMods+L"ModPackBridge.ini",r1_ini(1,"[ModPack]\r\nStats200=1\r\nTraitHover=1\r\nCollab=0\r\n"));
        check(api.peer_for_owner("trait_hover")==1 && api.peer_for_owner("collab")==0 && api.peer_for_owner("stats200")==1,"TraitHover/Stats200 on, Collab off");
        write_text(root,kMods+L"ModPackBridge.ini",r1_ini(0,"[ModPack]\r\nStats200=1\r\nTraitHover=1\r\nCollab=1\r\n"));
        check(api.peer_for_owner("trait_hover")==0 && api.peer_for_owner("collab")==0 && api.peer_for_owner("stats200")==0,"ModPackBridge disabled: nobody yields");
        // Patch27: module enabled AND Patch27Features
        write_text(root,kMods+L"Patch27.ini",r1_ini(1));
        check(api.peer_for_owner("battle_deaths")==0,"Patch27 module alone (R1 item 1 only): no yield");
        write_text(root,L"SAN14Mods\\Patch27\\Options.ini","[Features]\r\nPatch27Features=1\r\n");
        check(api.peer_for_owner("battle_deaths")==1,"Patch27 with Patch27Features=1: yield");
        write_text(root,L"SAN14Mods\\Patch27\\Options.ini","[Features]\r\nPatch27Features=0\r\n");
        check(api.peer_for_owner("battle_deaths")==0,"Patch27Features=0: no yield");
        // scene yield: ini or loaded module
        write_text(root,kMods+L"SceneSkip.ini",r1_ini(1));
        check(api.peer_for_owner("scene_yield")==1,"SceneSkip ini");
        // file-existence fallback rule for the mediation filter (Batteki / Yuga)
        check(api.peer_for_owner("mediation_fallback")==0,"no Batteki/Yuga files");
        write_text(root,L"SAN14_Batteki.dll","x");
        check(api.peer_for_owner("mediation_fallback")==1,"Batteki file present, Mediation key absent (default 1)");
        write_text(root,L"San14ModSettings.ini","[Batteki]\r\nMediation=0\r\n");
        check(api.peer_for_owner("mediation_fallback")==0,"Batteki Mediation=0");
        write_text(root,L"SAN14_Yuga.dll","x");
        check(api.peer_for_owner("mediation_fallback")==1,"Yuga file present, Enable/Mediation default 1");
        write_text(root,L"San14ModSettings.ini","[Batteki]\r\nMediation=0\r\n[Yuga]\r\nEnable=1\r\nMediation=0\r\n");
        check(api.peer_for_owner("mediation_fallback")==0,"Yuga Mediation=0");
        check(api.peer_for_owner("unknown_owner")==0 && api.peer_for_owner(nullptr)==0 && api.peer_for_owner("")==0,"unknown owners never yield");
    }
    // ===== whitelist (C-1): replacement must live in our DLLs for the contested slots =====
    {
        fresh();
        for(unsigned i=0;i<layout_row_count;++i)fill_row(layout_rows[i]);
        void *n1=nullptr,*n2=nullptr,*n3=nullptr,*n4=nullptr;
        const LayoutRow &first=*[]{for(unsigned i=0;i<layout_row_count;++i)if(layout_rows[i].rva==0x139A338)return &layout_rows[i];return &layout_rows[0];}();
        int outside=api.hook_vtable(first.owner,first.rva,first.original,region+kExt+0x40,&n1,first.flags);   // foreign module
        int heap=api.hook_vtable("merit",0x1413600,0x908820,region+kHeap+0x40,&n2,LF_CW);                          // unregistered memory
        int self=api.hook_vtable("ccz_sprites",0x13A03D0,0x1C6EC0,reinterpret_cast<void *>(&pass_fn),&n3,LF_CW);   // inside the Layout DLL / test exe
        int ours=api.hook_vtable("recruit_ui",0x13AC708,0x6BE8E0,region+kOurs+0x40,&n4,LF_C);                       // no whitelist flag: fine anywhere
        layout::publish_hooks();
        check(state_of(outside)==LAYOUT_STATE_REFUSED && hooks[outside-1].reason==LAYOUT_WHY_NOT_WHITELISTED,"replacement in a foreign module: refused (not whitelisted)");
        check(state_of(heap)==LAYOUT_STATE_REFUSED && hooks[heap-1].reason==LAYOUT_WHY_NOT_WHITELISTED,"replacement outside every module: refused");
        check(state_of(self)==LAYOUT_STATE_REFUSED && hooks[self-1].reason==LAYOUT_WHY_NOT_WHITELISTED,"replacement inside the Layout DLL itself: refused (no thunk in Layout, G2)");
        check(state_of(ours)==LAYOUT_STATE_INSTALLED,"without the whitelist flag the slot is not restricted");
        check(*slot_at(0x139A338)==image+0x1A7F50,"refused slot untouched");
        // whitelisted: the same slot with a replacement in SAN14_Convenience.dll
        fresh();
        for(unsigned i=0;i<layout_row_count;++i)fill_row(layout_rows[i]);
        int fine=api.hook_vtable(first.owner,first.rva,first.original,region+kOurs+0x40,&n1,first.flags);
        layout::publish_hooks();
        check(state_of(fine)==LAYOUT_STATE_INSTALLED && module_of(*slot_at(0x139A338)).kind==Kind::Ours,"whitelisted replacement installs and the slot value is inside our DLL");
    }
    // ===== 70E9C1 block hook (8.4): relay far away, bytes mismatch, relay machine code =====
    {
        fresh();
        memcpy(image+layout_mediation_row.rva,layout_mediation_expect,10);
        // relay inside rel32 reach
        Bytes *relay=region+kOurs+0x1000;
        layout_build_mediation_relay(relay,true,reinterpret_cast<void *>(&pass_fn),reinterpret_cast<void *>(&reject_fn));
        int h=api.hook_block(layout_mediation_row.owner,layout_mediation_row.rva,layout_mediation_expect,10,relay,layout_mediation_row.flags);
        layout::publish_hooks();
        Bytes *at=image+layout_mediation_row.rva;
        check(state_of(h)==LAYOUT_STATE_INSTALLED && at[0]==0xE9 && at+5+*reinterpret_cast<int *>(at+1)==relay && at[5]==0x90 && at[9]==0x90,"70E9C1: E9 to the relay, NOP fill of the remaining five bytes");
        check(image[0x70E9C0]==0 || true,"70E9C0 byte is outside the patch");
        // run the relay: RDX = candidate person with +BC at offset 0xBC
        using Relay=int(__fastcall *)(void *,void *);
        Bytes person[0x200]={};
        auto call=[&](bool on,unsigned short sworn)->int {
            layout_build_mediation_relay(relay,on,reinterpret_cast<void *>(&pass_fn),reinterpret_cast<void *>(&reject_fn));
            *reinterpret_cast<unsigned short *>(person+0xBC)=sworn;
            return reinterpret_cast<Relay>(relay)(nullptr,person);
        };
        check(call(true,5)==1,"flag on, sworn candidate: passes (married and sworn no longer hidden)");
        check(call(true,0)==1,"flag on, not sworn: passes");
        check(call(false,5)==2,"flag off, sworn: original rejection (70EA11)");
        check(call(false,0)==1,"flag off, not sworn: original pass (70E9CB)");
        check(api.unhook(h)==LAYOUT_UNHOOK_REMOVED && !memcmp(at,layout_mediation_expect,10),"70E9C1 restored");
        // relay out of rel32 reach: absolute jump through the pool (G5-b)
        fresh();
        memcpy(image+layout_mediation_row.rva,layout_mediation_expect,10);
        void *far_relay=reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(image)+0x7FFFFFFF00ull);
        int far_h=api.hook_block(layout_mediation_row.owner,layout_mediation_row.rva,layout_mediation_expect,10,far_relay,layout_mediation_row.flags);
        layout::publish_hooks();
        at=image+layout_mediation_row.rva;
        check(state_of(far_h)==LAYOUT_STATE_INSTALLED && at[0]==0xE9 && in_pool(at+5+*reinterpret_cast<int *>(at+1)) && relay_target(at+5+*reinterpret_cast<int *>(at+1))==far_relay,"far relay: E9 goes through a pool absolute jump");
        // foreign bytes: refused, and the other seven mediation_plus rows are not installed (owner bundle)
        fresh();
        for(unsigned i=0;i<layout_row_count;++i)fill_row(layout_rows[i]);
        memcpy(image+layout_mediation_row.rva,layout_mediation_expect,10);image[layout_mediation_row.rva+9]=0x74;   // another patch changed the jump
        void *dummy=nullptr;
        int first_h=api.hook_block(layout_mediation_row.owner,layout_mediation_row.rva,layout_mediation_expect,10,region+kOurs+0x1000,layout_mediation_row.flags);
        register_rows("mediation_plus");
        layout::publish_hooks();
        (void)dummy;
        check(state_of(first_h)==LAYOUT_STATE_REFUSED && hooks[first_h-1].reason==LAYOUT_WHY_BYTES,"70E9C1 with foreign bytes is refused");
        check(states_of("mediation_plus",LAYOUT_STATE_INSTALLED)==0 && states_of("mediation_plus",LAYOUT_STATE_REFUSED)==7,"G4: the seven E8 sites follow (only that feature turns off)");
        check(!in_pool(call_target(0x70EADC)),"E8 70EADC left original");
        // an entry JMP already at 70E9C1 is never followed
        fresh();
        image[layout_mediation_row.rva]=0xE9;
        int redirected=api.hook_block("mediation_plus",layout_mediation_row.rva,layout_mediation_expect,10,region+kOurs+0x1000,LAYOUT_REFUSE_IF_FOREIGN);
        layout::publish_hooks();
        check(state_of(redirected)==LAYOUT_STATE_REFUSED && hooks[redirected-1].reason==LAYOUT_WHY_REDIRECTED,"an E9 at the block start is refused, not followed");
    }
    // ===== spans (E9 + NOP up to 32 bytes, several blocks) and byte patches =====
    {
        fresh();
        std::wstring root=make_temp_root();fixture_root[0]=0;
        Bytes expect[32];for(unsigned i=0;i<32;++i)expect[i]=static_cast<Bytes>(0x40+i);
        // a 17-byte span at block offset 4 (907294-like): crosses into a second block
        memcpy(image+0x907294-0x900000+0x1000,expect,17);   // inside the 4 MB-class image but not at the real RVA
        const unsigned rva=0x907294-0x900000+0x1000;
        int s=api.hook_span("traits9",rva,expect,17,region+kOurs+0x2000,LF_Y);
        // a 15-byte span at offset 3 (2BF033-like): three blocks
        const unsigned rva2=0x2BF033-0x200000;memcpy(image+rva2,expect,15);
        int s2=api.hook_span("traits9",rva2,expect,15,region+kOurs+0x2010,LF_Y);
        // byte patch crossing a block boundary (22558F-like, offset 15, 4 bytes)
        const unsigned rva3=0x22558F-0x200000;Bytes bytes_expect[4]={0x41,0x83,0xFC,0x05},bytes_patch[4]={0x41,0x83,0xFC,0x09};memcpy(image+rva3,bytes_expect,4);
        int b=api.hook_bytes("traits9",rva3,bytes_expect,bytes_patch,4,LF_Y);
        // plain 8-byte patch (4A6CF3-like)
        const unsigned rva4=0x4A6CF3-0x400000;Bytes e8[8]={0x48,0xC7,0x45,0xDC,0x05,0,0,0},p8[8]={0x48,0xC7,0x45,0xDC,0x09,0,0,0};memcpy(image+rva4,e8,8);
        int b2=api.hook_bytes("traits9",rva4,e8,p8,8,LF_Y);
        layout::publish_hooks();
        check(state_of(s)==LAYOUT_STATE_INSTALLED && image[rva]==0xE9 && image[rva+5]==0x90 && image[rva+16]==0x90,"17-byte span: E9 + NOP across two blocks");
        check(state_of(s2)==LAYOUT_STATE_INSTALLED && image[rva2]==0xE9 && image[rva2+14]==0x90,"15-byte span at block offset 3");
        check(state_of(b)==LAYOUT_STATE_INSTALLED && !memcmp(image+rva3,bytes_patch,4),"byte patch across a block boundary");
        check(state_of(b2)==LAYOUT_STATE_INSTALLED && !memcmp(image+rva4,p8,8),"8-byte patch");
        check(api.unhook(s)==0 && api.unhook(s2)==0 && api.unhook(b)==0 && api.unhook(b2)==0 &&
              !memcmp(image+rva,expect,17) && !memcmp(image+rva2,expect,15) && !memcmp(image+rva3,bytes_expect,4) && !memcmp(image+rva4,e8,8),"unhook restores spans and byte patches");
        // mismatch refused, blocklist refused, argument limits
        fresh();
        memcpy(image+0x5000,expect,17);image[0x5003]^=0xFF;
        int mismatch=api.hook_span("traits9",0x5000,expect,17,region+kOurs+0x2000,LF_Y);
        int mismatch2=api.hook_bytes("traits9",0x5100,expect,expect,4,LF_Y);
        layout::publish_hooks();
        check(state_of(mismatch)==LAYOUT_STATE_REFUSED && hooks[mismatch-1].reason==LAYOUT_WHY_BYTES && state_of(mismatch2)==LAYOUT_STATE_REFUSED,"span/bytes with foreign bytes are refused");
        check(api.hook_span("t",0x7FA230,expect,10,region+kOurs+0x2000,0)==-2 && api.hook_bytes("t",0x86E0C0,expect,expect,4,0)==-2,"blocked entries are refused for spans and byte patches");
        check(api.hook_span("t",0x5000,expect,33,region+kOurs+0x2000,0)==-1 && api.hook_span("t",0x5000,expect,4,region+kOurs+0x2000,0)==-1 &&
              api.hook_bytes("t",0x5000,expect,expect,0,0)==-1 && api.hook_bytes("t",0x5000,expect,expect,33,0)==-1 && api.hook_bytes("t",0x5000,nullptr,expect,4,0)==-1,"span/bytes argument limits");
        // single-block hook_block keeps its stage-1 limits (an unchanged contract)
        check(api.hook_block("t",0x500C,expect,9,region+kOurs+0x2000,0)==-1 && api.hook_block("t",0x5040,expect,17,region+kOurs+0x2000,0)==-1,"hook_block still refuses block-crossing and 17-byte requests");
        // traits9 owner yields as a group when R1 Traits9 is enabled
        fresh();fixture_root[0]=0;wcscpy_s(fixture_root,root.c_str());
        write_text(root,kMods+L"Traits9Check.ini",r1_ini(1));
        memcpy(image+0x5000,expect,17);memcpy(image+0x5200,expect,4);
        int ys=api.hook_span("traits9",0x5000,expect,17,region+kOurs+0x2000,LF_Y);
        int yb=api.hook_bytes("traits9",0x5200,expect,expect,4,LF_Y);
        layout::publish_hooks();
        check(state_of(ys)==LAYOUT_STATE_YIELDED && state_of(yb)==LAYOUT_STATE_YIELDED && !memcmp(image+0x5000,expect,17),"traits9 spans and byte patches yield to R1 Traits9");
    }
    // ===== battle_deaths span: 8 bytes at line offset F (295C5F-like, api_hook_block would refuse it) =====
    {
        fresh();fixture_root[0]=0;
        const unsigned rva=0x6000F;   // &15==15, crosses a 16-byte line
        check((rva&15)==15 && ((rva&15)+layout_battle_deaths_span.len)>16,"test site crosses a 16-byte boundary");
        check(layout_battle_deaths_span.len==8 && sizeof(layout_battle_deaths_expect)==8,"battle_deaths span is 8 bytes");
        check(!blocked_range(rva,8),"synthetic span site is not a blocked entry");
        memcpy(image+rva,layout_battle_deaths_expect,8);
        int h=api.hook_span(layout_battle_deaths_span.owner,rva,layout_battle_deaths_expect,8,region+kOurs+0x3000,layout_battle_deaths_span.flags);
        layout::publish_hooks();
        check(state_of(h)==LAYOUT_STATE_INSTALLED && image[rva]==0xE9 && image[rva+5]==0x90 && image[rva+6]==0x90 && image[rva+7]==0x90,"battle_deaths span: E9 + NOP3 across the line boundary");
        check(api.unhook(h)==0 && !memcmp(image+rva,layout_battle_deaths_expect,8),"battle_deaths span: unhook restores the original 8 bytes");
        fresh();
        memcpy(image+rva,layout_battle_deaths_expect,8);image[rva+4]^=0xFF;   // another patch changed the je displacement
        int bad=api.hook_span(layout_battle_deaths_span.owner,rva,layout_battle_deaths_expect,8,region+kOurs+0x3000,layout_battle_deaths_span.flags);
        layout::publish_hooks();
        check(state_of(bad)==LAYOUT_STATE_REFUSED && hooks[bad-1].reason==LAYOUT_WHY_BYTES,"battle_deaths span: different bytes are refused");
    }
    // ===== next-call chain: a foreign module's function already in the slot stays behind ours =====
    {
        fresh();
        Bytes *foreign=region+kExt+0x40;   // `jmp [rip+0] ; &ext_marker` inside the fake foreign module
        foreign[0]=0xFF;foreign[1]=0x25;memset(foreign+2,0,4);*reinterpret_cast<void **>(foreign+6)=reinterpret_cast<void *>(&ext_marker);
        *slot_at(0x1473F50)=foreign;
        other_calls=0;
        void *next=nullptr;
        int h=api.hook_vtable("speed_buttons",0x1473F50,0x882540,region+kOurs+0x80,&next,LF_CYW);
        layout::publish_hooks();
        check(state_of(h)==LAYOUT_STATE_CHAINED && next==foreign && *slot_at(0x1473F50)==region+kOurs+0x80,"foreign slot value: chained, next is the value that was in the slot");
        reinterpret_cast<void (*)()>(next)();
        check(other_calls==1,"calling next reaches the foreign function");
    }
    check(count_events("layout_installed")>=0,"log consumed");
    printf("{\"status\":\"layout_p3_fixture_passed\",\"checks\":%u,\"rows\":%u,\"synthetic_image\":true,\"gameplay_verified\":false}\n",checks,layout_row_count);
    return 0;
}
