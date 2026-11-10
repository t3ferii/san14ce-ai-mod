// Private native-object simulations; not proof of live ABI or UI behavior.
#define TACTICS_FIXTURE
#define MERIT_CANDIDATE
#include <map>
#include <stdlib.h>
#include "../src/native_adapter.cpp"
static std::map<unsigned,void *> functions;
static void *fixture_function(unsigned rva) {
    auto it=functions.find(rva);if(it==functions.end()) {fprintf(stderr,"Missing native %X\n",rva);abort();}
    return it->second;
}
static unsigned checks,native_calls;
static Obj root,force1,force2;
static Link *heads[256];static size_t list_counts[256];static unsigned next_list=1;
static std::map<unsigned,unsigned> next_grades;
static unsigned batch_result=1,join_result=1;
static void expect_at(bool ok,const char *file,unsigned line) {++checks;if(!ok) {fprintf(stderr,"Merit fixture failed at %u (%s:%u)\n",checks,file,line);exit(1);}}
#define expect(...) expect_at((__VA_ARGS__),__FILE__,__LINE__)
static void put16(Obj p,unsigned at,unsigned v) {*reinterpret_cast<uint16_t *>(p+at)=static_cast<uint16_t>(v);}
static void put32(Obj p,unsigned at,unsigned v) {*reinterpret_cast<unsigned *>(p+at)=v;}
static void putptr(Obj p,unsigned at,const void *v) {*reinterpret_cast<const void **>(p+at)=v;}
static Obj object(unsigned id) {Obj p=static_cast<Obj>(calloc(1,0x500));put16(p,0x10,id);return p;}
static void slot(unsigned offset,unsigned id,Obj p) {putptr(root,offset+id*8,p);}
static int __fastcall mock_valid(Obj p) {return p && word(p,0x10);}
static Obj __fastcall mock_force(Obj p) {return p[0xE0]==1?force1:p[0xE0]==2?force2:nullptr;}
static void __fastcall mock_init(GameList *out) {out->handle=new unsigned(next_list++);}
static Obj __fastcall mock_add(Obj,unsigned handle,unsigned char) {
    auto *link=new Link{nullptr,nullptr};Link **tail=&heads[handle];while(*tail)tail=&(*tail)->next;
    *tail=link;++list_counts[handle];return reinterpret_cast<Obj>(link);
}
static void __fastcall mock_free(GameList *out) {
    Link *it=heads[*out->handle];while(it) {Link *next=it->next;delete it;it=next;}
    heads[*out->handle]=nullptr;list_counts[*out->handle]=0;delete out->handle;out->handle=nullptr;
}
static int __fastcall mock_has(Obj p,Obj other) {
    for(unsigned i=0;i<8;++i)if(word(p,0xC0+i*2)==word(other,0x10))return 1;return 0;
}
static unsigned __fastcall mock_list(Obj p,GameList *out,int) {
    unsigned count=0;
    for(unsigned i=0;i<8;++i) {
        unsigned id=word(p,0xC0+i*2);Obj other=table(0x158,id,6000);
        if(id && valid(other)) {list_add(*out,other);++count;}
    }
    return count;
}
static unsigned __fastcall mock_batch(Obj) {
    ++native_calls;for(const auto &row:next_grades) {
        Obj p=table(0x158,row.first,6000);p[0x1C2]=static_cast<unsigned char>(row.second&15);
        p[0x1C3]=static_cast<unsigned char>(row.second>>4);
    }
    return batch_result;
}
static uint64_t __fastcall mock_join(Obj p,Obj d,Obj city,Obj event,int flag,uint64_t context,int mode) {
    ++native_calls;expect(city==root && event==image && flag==17 && context==0x123456789ABCDEFull && mode==19);
    if(join_result) {p[0xE0]=d[0x10];put16(p,0x1C2,0);}return join_result;
}
static uint64_t __fastcall mock_query(Obj p,unsigned effect) {
    unsigned total=0;
    for(unsigned i=1;i<=p[0x1C2];++i) {
        Obj row=pointer(root,0x85DF0+i*8);if(row[0x10]==effect)total+=word(row,0x12);
    }
    for(unsigned i=0;i<p[0x1C3];++i) {   // native 2A9BB0 also adds the civil rows 4..
        Obj row=pointer(root,0x85DF0+(4+i<7?4+i:0)*8);if(row[0x10]==effect)total+=word(row,0x12);
    }
    return total;
}
static const wchar_t *__fastcall mock_merit_text(int stage) {
    return stage==3?L"원래 3단계 설명":stage==4?L"문공 설명":L"무공 설명";
}
static void __fastcall mock_reset(Obj p) {put16(p,0x1C2,0);}
static std::map<unsigned,merit::Context> contexts;
static merit::Context read_context(unsigned id,void *) {return contexts[id];}
static void test_store() {
    SidecarCache cache;std::vector<uint8_t> cached,first_bytes={1,2,3},changed={1,2,4};
    expect(!cache.get(L"save-A",cached));cache.remember(L"save-A",first_bytes);
    expect(cache.matches(L"save-A",first_bytes) && !cache.matches(L"save-A",changed));
    expect(cache.get(L"save-A",cached) && cached==first_bytes);
    expect(!cache.matches(L"save-B",first_bytes));cache.remember(L"save-A",changed);
    expect(cache.matches(L"save-A",changed));
    for(unsigned i=0;i<65;++i)cache.remember((L"slot-"+std::to_wstring(i)).c_str(),first_bytes);
    expect(!cache.get(L"save-A",cached) && cache.get(L"slot-64",cached));
    merit::Store store;contexts={{1,{1,1,0,true}},{2,{1,1,3,true}},{3,{2,3,0,true}}};
    expect(!store.award(0,0,read_context,nullptr));expect(!store.award(1,0,read_context,nullptr));
    expect(!store.award(2,3,read_context,nullptr));expect(store.award(2,2,read_context,nullptr));
    expect(store.contains(2,1,read_context,nullptr) && store.contains(1,2,read_context,nullptr));
    expect(store.list(2,read_context,nullptr)==std::vector<uint16_t>{1});
    contexts[2].force=2;contexts[2].ruler=3;contexts[2].grade=0;store.prune(read_context,nullptr);
    expect(store.size()==0);contexts[2].grade=1;expect(!store.award(2,0,read_context,nullptr));
    contexts[2].grade=3;expect(store.award(2,2,read_context,nullptr));
    expect(store.contains(2,3,read_context,nullptr) && !store.contains(2,1,read_context,nullptr));
    contexts[3].alive=false;store.prune_person(3,read_context,nullptr);expect(store.size()==0);
    contexts[3].alive=true;expect(!store.contains(2,3,read_context,nullptr));
    expect(store.award(2,2,read_context,nullptr));contexts[3].ruler=1;store.prune(read_context,nullptr);
    expect(store.size()==0);contexts[3].ruler=3;
    for(unsigned id=4;id<=503;++id) {
        contexts[id]={2,3,3,true};expect(store.award(id,2,read_context,nullptr));
    }
    expect(store.list(3,read_context,nullptr).size()==500);expect(store.list(500,read_context,nullptr).size()==1);
    contexts[504]={2,3,3,true};expect(!store.award(504,2,read_context,nullptr));
    merit::Binding hash={};hash[0]=19;auto bytes=store.encode(hash);
    expect(bytes.size()==52+500*6 && bytes.size()<36053);
    merit::Store restored;expect(restored.decode(bytes.data(),bytes.size(),hash));
    expect(restored.size()==500);auto wrong=hash;wrong[1]=1;
    expect(!restored.decode(bytes.data(),bytes.size(),wrong) && restored.size()==500);
    for(size_t cut:{size_t(0),size_t(51),bytes.size()-1})expect(!restored.decode(bytes.data(),cut,hash));
    for(size_t at:{size_t(0),size_t(8),size_t(12),size_t(44),size_t(48),bytes.size()-1}) {
        auto bad=bytes;bad[at]^=1;expect(!restored.decode(bad.data(),bad.size(),hash));
    }
    auto trailing=bytes;trailing.push_back(0);expect(!restored.decode(trailing.data(),trailing.size(),hash));
    restored.erase_person(3);expect(restored.size()==0);
    merit::SaveCoordinator jobs;merit::SaveCoordinator::Completed completed;
    uint64_t epoch=jobs.begin_save("fixture_merit.s14",store);store.clear();
    expect(!jobs.observe_save(epoch,"another.s14",hash,true));
    expect(jobs.observe_save(epoch,"fixture_merit.s14",hash,true));
    expect(jobs.finish_save(epoch,true,completed) && completed.store.size()==500);
    expect(!jobs.finish_save(epoch,true,completed));
    epoch=jobs.begin_load("fixture_merit.s14");expect(!jobs.observe_load(epoch,"fixture_merit.s14",hash,false));
    expect(!jobs.finish_load(epoch,true,completed));
    epoch=jobs.begin_save("fixture_merit.s14",store);jobs.begin_load("other.s14");
    expect(!jobs.observe_save(epoch,"fixture_merit.s14",hash,true));
    expect(jobs.begin_save("../bad.s14",store)==0);
}
struct CommandTree {
    unsigned char command[24]={},head[48]={},nodes[3][48]={};
    CommandTree(unsigned first,unsigned second,unsigned third) {
        head[0x19]=1;putptr(command,0,head);*reinterpret_cast<size_t *>(command+8)=3;
        putptr(head,0,nodes[0]);putptr(head,8,nodes[1]);putptr(head,16,nodes[2]);
        for(unsigned i=0;i<3;++i) {putptr(nodes[i],0,head);putptr(nodes[i],8,nodes[1]);putptr(nodes[i],16,head);}
        putptr(nodes[1],0,nodes[0]);putptr(nodes[1],8,head);putptr(nodes[1],16,nodes[2]);
        put32(nodes[0],0x1C,first);put32(nodes[1],0x1C,second);put32(nodes[2],0x1C,third);
    }
};
static void test_bindings() {
    original_merit_has=&mock_has;original_merit_list=&mock_list;original_merit_batch=&mock_batch;
    original_merit_join=&mock_join;original_merit_query=&mock_query;original_merit_reset=&mock_reset;
    original_merit_text=&mock_merit_text;
    Obj a=object(100),b=object(101),c=object(102),r1=object(10),r2=object(20);
    for(Obj p:{a,b,c,r1,r2}) {slot(0x158,word(p,0x10),p);p[0xE0]=p==r2?2:1;}
    std::vector<Obj> native;
    for(unsigned i=0;i<8;++i) {
        Obj p=object(200+i);slot(0x158,200+i,p);native.push_back(p);put16(a,0xC0+i*2,200+i);
    }
    memset(a+0xD0,0xA7,16);unsigned char unchanged[32];memcpy(unchanged,a+0xC0,32);
    CommandTree tree(100,101,102);auto snapshot=merit_batch_snapshot(tree.command);
    expect(snapshot.size()==3 && snapshot[0].id==100 && snapshot[1].id==101 && snapshot[2].id==102);
    for(unsigned stage=1;stage<=3;++stage) {
        next_grades={{100,stage},{101,stage},{102,0x30}};expect(merit_batch(tree.command)==1);
        expect(merit_has(a,r1)==(stage==3));expect(merit_has(r1,a)==(stage==3));
        expect(!merit_has(c,r1));expect(!memcmp(unchanged,a+0xC0,32));
    }
    expect(merit_bonds.size()==2);GameList list;list_init(list);
    expect(merit_list(a,&list,0)==9);list_free(list);list_init(list);
    expect(merit_list(r1,&list,0)==2);list_free(list);list_init(list);
    expect(merit_list(a,&list,1)==8);list_free(list);
    // Native ordinary friendship persists after its separate merit expires.
    expect(word(b,0xC0)==10);put16(b,0xC2,10);join_result=0;unsigned calls=native_calls;
    expect(merit_join(a,table(0xE0D0,2,51),root,image,17,0x123456789ABCDEFull,19)==0);
    expect(native_calls==calls+1 && a[0x1C2]==3 && merit_has(a,r1));
    join_result=1;expect(merit_join(a,table(0xE0D0,2,51),root,image,17,0x123456789ABCDEFull,19)==1);
    expect(a[0x1C2]==0 && a[0x1C3]==0 && !merit_has(a,r1));
    expect(!memcmp(unchanged,a+0xC0,32));next_grades={{100,1}};merit_batch(tree.command);
    expect(!merit_has(a,r2));next_grades={{100,2}};merit_batch(tree.command);
    expect(!merit_has(a,r2));next_grades={{100,3}};merit_batch(tree.command);
    expect(merit_has(a,r2) && merit_has(r2,a) && !merit_has(a,r1));
    put16(force1,0x10,102);expect(merit_has(b,r1)); // ordinary native relation remains
    expect(!merit_bonds.contains(101,10,merit_context,nullptr));
    merit_bonds.prune(merit_context,nullptr);put16(force1,0x10,10);
    expect(!merit_bonds.contains(101,10,merit_context,nullptr));
    r2[0xE6]=9;expect(!merit_has(a,r2));r2[0xE6]=0;expect(!merit_has(a,r2));
    a[0x1C2]=2;batch_result=0;next_grades={{100,3}};
    expect(merit_batch(tree.command)==0 && merit_has(a,r2));batch_result=1;
    merit_reset(a);expect(!merit_has(a,r2) && !memcmp(unchanged,a+0xC0,32));
    // Verified effect rows change only the first three military values.
    Obj records=static_cast<Obj>(calloc(7,24));const unsigned types[]={0,1,1,1,2,2,3},values[]={0,500,500,1000,1,1,1};
    for(unsigned i=0;i<7;++i) {Obj row=records+i*24;putptr(row,0,image+0x1234);row[0x10]=static_cast<unsigned char>(types[i]);
        put16(row,0x12,values[i]);putptr(root,0x85DF0+i*8,row);}
    for(unsigned stage=0;stage<=3;++stage) {a[0x1C2]=static_cast<unsigned char>(stage);
        expect(merit_query(a,1)==(stage==0?0:stage==1?3000:6000));}
    expect(word(records+3*24,0x12)==0 && word(records+4*24,0x12)==1);
    // 2026-10-23 one merit route per officer: with both counts the civil (tie) or larger route counts.
    a[0x1C2]=1;a[0x1C3]=1;expect(merit_query(a,1)==0 && merit_query(a,2)==1);
    a[0x1C2]=2;expect(merit_query(a,1)==6000 && merit_query(a,2)==0);a[0x1C2]=0;a[0x1C3]=0;
    {   // Award command (node+20: low military, high civil) keeps the route already held.
        CommandTree route(100,101,102);auto rows=merit_batch_snapshot(route.command);
        c[0x1C2]=0;c[0x1C3]=0;a[0x1C3]=1;route.nodes[0][0x20]=0x11;b[0x1C2]=1;route.nodes[1][0x20]=0x21;route.nodes[2][0x20]=0x22;
        merit_route_lock(rows);
        expect(route.nodes[0][0x20]==0x10 && route.nodes[1][0x20]==0x01 && route.nodes[2][0x20]==0x20);
        a[0x1C3]=0;b[0x1C2]=0;
    }
    {   // 2026-11-06 (user): both counts at once is abnormal; the winner (larger, tie civil) grows, the loser keeps its value, nothing is lowered.
        CommandTree route(100,101,102);auto rows=merit_batch_snapshot(route.command);
        a[0x1C2]=2;a[0x1C3]=1;route.nodes[0][0x20]=0x23;          // military 2 > civil 1: military wins, civil stays 1
        b[0x1C2]=1;b[0x1C3]=1;route.nodes[1][0x20]=0x32;          // tie: civil wins, military stays 1
        c[0x1C2]=0;c[0x1C3]=0;route.nodes[2][0x20]=0x12;          // none held: chosen 2 military vs 1 civil -> military
        merit_route_lock(rows);
        expect(route.nodes[0][0x20]==0x13 && route.nodes[1][0x20]==0x31 && route.nodes[2][0x20]==0x02);
        a[0x1C2]=2;a[0x1C3]=1;route.nodes[0][0x20]=0x10;          // winner chosen below what he holds is not lowered
        merit_route_lock(rows);expect(route.nodes[0][0x20]==0x12);
        a[0x1C2]=0;a[0x1C3]=0;b[0x1C2]=0;b[0x1C3]=0;
        CommandTree zero(100,101,102);auto zrows=merit_batch_snapshot(zero.command);
        zero.nodes[0][0x20]=0x11;zero.nodes[1][0x20]=0x31;zero.nodes[2][0x20]=0x13;   // 0/0, both chosen: tie civil, larger civil, larger military
        merit_route_lock(zrows);expect(zero.nodes[0][0x20]==0x10 && zero.nodes[1][0x20]==0x30 && zero.nodes[2][0x20]==0x03);
    }
    expect(!wcscmp(merit_effect_text(3),L"군주와 상호 친애"));
    expect(!wcscmp(merit_effect_text(4),L"문공 설명"));
    {   // 2026-11-05 (user): stage 1/2 troops are chosen (merit_troops_level 1..4 = 1500/2000/2500/3000) in both builds; stage 3 friendship
        // (merit_friendship) is added on top of the native effect: old shape (troops) 0 troops vs original 1000, new patch shape
        // (non-troop effect, build 25953386) keeps the native row and only adds the friendship text.
        const unsigned choices[]={1500,2000,2500,3000};
        auto rows=[&](unsigned v1,unsigned v2,unsigned t3,unsigned v3) {
            put16(records+24,0x12,v1);put16(records+48,0x12,v2);records[3*24+0x10]=static_cast<unsigned char>(t3);put16(records+72,0x12,v3);
            for(unsigned i=4;i<7;++i)put16(records+i*24,0x12,1);put16(records,0x12,0);
            putptr(root,0x85DF0+3*8,records+72);
        };
        for(unsigned level=1;level<=4;++level)for(unsigned friend_on=0;friend_on<2;++friend_on) {
            mod_features.merit_troops_level=level;mod_features.merit_friendship=friend_on!=0;
            const unsigned want=choices[level-1];
            const unsigned old_start[][3]={{500,500,1000},{3000,3000,0},{1500,2500,1000},{1000,1000,1000}};
            for(const auto &s:old_start) {   // shape A: old build original, old mod output, other earlier choices
                rows(s[0],s[1],1,s[2]);
                expect(merit_effect_state()==1 && merit_effects());
                expect(word(records+24,0x12)==want && word(records+48,0x12)==want && word(records+72,0x12)==(friend_on?0u:1000u) && records[3*24+0x10]==1);
                expect(!wcscmp(merit_effect_text(3),friend_on?L"군주와 상호 친애":L"원래 3단계 설명"));
            }
            for(unsigned type3=2;type3<=3;++type3)for(unsigned start:{1000u,500u,3000u}) {   // shape B: patch 25953386
                rows(start,start,type3,1);
                expect(merit_effect_state()==2 && merit_effects());
                expect(word(records+24,0x12)==want && word(records+48,0x12)==want && word(records+72,0x12)==1 && records[3*24+0x10]==type3);
                expect(!wcscmp(merit_effect_text(3),friend_on?L"원래 3단계 설명 / 군주와 상호 친애":L"원래 3단계 설명"));
                expect(!wcscmp(merit_effect_text(4),L"문공 설명"));
            }
        }
        mod_features.merit_troops_level=4;mod_features.merit_friendship=true;
        // Rejections never touch a row: unknown value, unknown type, wrong value on rows 0/4..6, broken contiguity.
        auto untouched=[&](unsigned step,unsigned index) {
            expect(merit_effect_state()==0 && static_cast<unsigned>(merit_effect_reject_step)==step && static_cast<unsigned>(merit_effect_reject_index)==index);
        };
        rows(1700,1700,1,1000);untouched(3,1700);expect(word(records+24,0x12)==1700 && word(records+72,0x12)==1000);
        rows(500,500,1,500);untouched(3,500);expect(word(records+24,0x12)==500 && word(records+72,0x12)==500);
        rows(1000,1000,3,2);untouched(3,1000);rows(1000,1000,2,3);untouched(3,1000);   // local17: shape B stage 3 accepts 0 (written by local16) and 1
        rows(1000,1000,5,1);untouched(2,3);expect(word(records+24,0x12)==1000);
        rows(1000,1000,0,1);untouched(2,3);
        rows(500,500,1,1000);records[4*24+0x10]=5;untouched(2,4);records[4*24+0x10]=2;
        rows(500,500,1,1000);put16(records+5*24,0x12,7);untouched(4,0);expect(word(records+24,0x12)==500);
        rows(500,500,1,1000);put16(records,0x12,3);untouched(4,0);
        rows(1000,1000,3,1);putptr(root,0x85DF0+3*8,records+80);untouched(2,3);expect(word(records+24,0x12)==1000);
        putptr(root,0x85DF0+3*8,records+72);expect(merit_effect_state()==2);
        putptr(records+72,0,image+0x1238);untouched(2,3);putptr(records+72,0,image+0x1234);
        putptr(records+72,8,image+0x1234);untouched(2,3);putptr(records+72,8,nullptr);
        // Friendship switch: awards stop and the stored bond stops acting (records stay saved), turning it back on restores both.
        rows(500,500,1,1000);merit_effect_state();
        a[0x1C2]=2;batch_result=0;next_grades={{100,3}};
        expect(merit_batch(tree.command)==0 && merit_has(a,r2));
        mod_features.merit_friendship=false;expect(!merit_has(a,r2));
        mod_features.merit_friendship=true;expect(merit_has(a,r2));
        merit_reset(a);batch_result=1;
        a[0x1C2]=2;batch_result=0;next_grades={{100,3}};mod_features.merit_friendship=false;
        expect(merit_batch(tree.command)==0);mod_features.merit_friendship=true;expect(!merit_has(a,r2));
        merit_reset(a);batch_result=1;a[0x1C2]=0;
        rows(500,500,1,1000);expect(merit_effect_state()==1);   // final: original table -> 3000/3000/0 as before
        expect(word(records+24,0x12)==3000 && word(records+72,0x12)==0);
    }
    enabled=0;expect(!merit_has(a,r2));enabled=1;
    free(records);for(Obj p:native) {slot(0x158,word(p,0x10),nullptr);free(p);}
}
#include "merit_save_fixture.inl"
#include "merit_combat_fixture.inl"
static std::wstring profile_label,original_biography=L"기존 관우 열전";
static unsigned profile_friend_count;
static int __fastcall mock_hidden(Obj) {return 0;}
static const wchar_t *__fastcall mock_biography(unsigned) {return original_biography.c_str();}
static const wchar_t *__fastcall mock_name(Obj p) {return word(p,0x10)==20?L"손권":word(p,0x10)==100?L"관우":L"장비";}
static unsigned __fastcall mock_key(Obj,unsigned key) {expect(key==0x30);return 0;}
static void __fastcall mock_label(Obj,unsigned key,const wchar_t *text) {expect(key==0);profile_label=text;}
static void __fastcall mock_profile(Obj,Obj p) {
    expect(in_merit_profile==1);GameList list;list_init(list);profile_friend_count=merit_list(p,&list,0);list_free(list);
}
static void __fastcall mock_profile_tick(Obj) {}
static unsigned __fastcall mock_profile_visible(Obj) {return 1;}
#include "merit_scroll_fixture.inl"
static void test_profile() {
    Obj front=object(1);strcpy_s(reinterpret_cast<char *>(front+0x70),128,"CInfoDlgState");Obj states[]={front};
    putptr(image,0x1B65100+0x20,states);putptr(image,0x1B65100+0x10,reinterpret_cast<void *>(1));
    functions[0x87ABC0]=reinterpret_cast<void *>(&mock_profile_tick);
    functions[0x871600]=reinterpret_cast<void *>(&mock_profile_visible);
    // Own this test's native friends; the preceding test frees its objects.
    std::vector<Obj> friends;
    for(unsigned i=0;i<8;++i) {Obj p=object(200+i);slot(0x158,200+i,p);friends.push_back(p);}
    original_merit_has=&mock_has;original_merit_list=&mock_list;
    functions[0x90ACC0]=reinterpret_cast<void *>(&mock_profile);
    functions[0x233D70]=reinterpret_cast<void *>(&mock_hidden);
    functions[0x233DA0]=reinterpret_cast<void *>(&mock_hidden);
    functions[0x30E0B0]=reinterpret_cast<void *>(&mock_biography);
    functions[0x230950]=reinterpret_cast<void *>(&mock_name);
    functions[0x9332B0]=reinterpret_cast<void *>(&mock_label);
    Obj profile=object(1);void *vt[64]={};vt[0x198/8]=reinterpret_cast<void *>(&mock_key);putptr(profile,0,vt);
    Obj desc=object(1);Obj descriptors[]={desc};putptr(profile,0x138,descriptors);put32(profile,0x140,1);
    Obj p=table(0x158,100,6000);p[0x1C2]=3;merit_bonds.clear();expect(merit_bonds.award(100,2,merit_context,nullptr));
    merit_profile(profile,p);
    expect(profile_label==L"[열전 1/1 · 휠/PgUp·PgDn]\n논공 친애: 손권\n\n기존 관우 열전" && profile_friend_count==8 && in_merit_profile==0);
    GameList list;list_init(list);expect(merit_list(p,&list,0)==9);list_free(list);
    merit_bonds.clear();merit_profile(profile,p);expect(profile_label==L"[열전 1/1 · 휠/PgUp·PgDn]\n논공 친애: 없음\n\n기존 관우 열전");
    enabled=0;profile_label=L"unchanged";merit_profile(profile,p);expect(profile_label==L"unchanged" && in_merit_profile==0);enabled=1;
    expect(merit_bonds.award(100,2,merit_context,nullptr));original_biography=std::wstring(801,L'가');profile_label=L"preserved";
    merit_profile(profile,p);expect(profile_label!=L"preserved" && merit_scrolls.back().text.find(original_biography)!=std::wstring::npos);
    original_biography=std::wstring(4096,L'가')+L"열전 끝";merit_profile(profile,p);
    merit_profile_tick(profile);MSG msg={};msg.message=WM_KEYDOWN;msg.wParam=VK_END;
    merit_scroll_message(0,PM_REMOVE,reinterpret_cast<LPARAM>(&msg));expect(msg.message==WM_NULL);
    merit_profile_tick(profile);expect(profile_label.find(L"열전 끝")!=std::wstring::npos && profile_label.size()<=900);
    msg.message=WM_KEYDOWN;msg.wParam=VK_HOME;merit_scroll_message(0,PM_REMOVE,reinterpret_cast<LPARAM>(&msg));
    merit_profile_tick(profile);expect(profile_label.find(L"[열전 1/")==0 && profile_label.find(L"논공 친애")!=std::wstring::npos);
    msg.message=WM_MOUSEWHEEL;msg.wParam=static_cast<WPARAM>(static_cast<unsigned short>(-WHEEL_DELTA))<<16;
    merit_scroll_message(0,PM_REMOVE,reinterpret_cast<LPARAM>(&msg));merit_profile_tick(profile);
    expect(merit_scrolls.back().top==3);
    msg.message=WM_KEYDOWN;msg.wParam=VK_PRIOR;merit_scroll_message(0,PM_REMOVE,reinterpret_cast<LPARAM>(&msg));
    merit_profile_tick(profile);expect(merit_scrolls.back().top==0);
    enabled=0;merit_profile_tick(profile);msg.message=WM_KEYDOWN;msg.wParam=VK_END;
    merit_scroll_message(0,PM_REMOVE,reinterpret_cast<LPARAM>(&msg));expect(msg.message==WM_KEYDOWN);enabled=1;
    original_biography=std::wstring(800,L'가');merit_profile(profile,p);
    expect(profile_label.size()<=900 && merit_scrolls.back().text.find(original_biography)!=std::wstring::npos);
    original_biography=L"군주 열전";merit_profile(profile,table(0x158,20,6000));
    expect(profile_label==L"[열전 1/1 · 휠/PgUp·PgDn]\n논공 친애: 관우\n\n군주 열전");
    test_scroll_input(profile,front);
    original_biography=L"기존 관우 열전";merit_scrolls.clear();free(profile);free(desc);
    putptr(image,0x1B65100+0x20,nullptr);putptr(image,0x1B65100+0x10,nullptr);free(front);
    for(Obj friend_object:friends) {slot(0x158,word(friend_object,0x10),nullptr);free(friend_object);}
}
// Merit dialog toggle (local7): a scripted "native" handler writes one nibble, the wrapper must undo raises of a forbidden route.
static unsigned dialog_target,dialog_value,dialog_refreshes,dialog_native_calls;
static void __fastcall mock_dialog(Obj data,Obj) {
    ++dialog_native_calls;std::vector<MeritDialogRow> rows;if(!merit_map_rows(data+0x198,rows))return;
    for(auto &row:rows)if(row.id==dialog_target)row.node[0x20]=static_cast<unsigned char>(dialog_value);
}
static void __fastcall mock_refresh(Obj) {++dialog_refreshes;}
static void test_dialog() {
    original_merit_dialog=&mock_dialog;functions[0x783F50]=reinterpret_cast<void *>(&mock_refresh);
    Obj p1=object(110),p2=object(111),p3=object(112);for(Obj p:{p1,p2,p3})slot(0x158,word(p,0x10),p);
    CommandTree tree(110,111,112);Obj data=static_cast<Obj>(calloc(1,0x400));putptr(data,0x198,tree.head);
    auto node=[&](unsigned id)->unsigned char & {return tree.nodes[id-110][0x20];};
    auto click=[&](unsigned id,unsigned value)->unsigned {   // returns refresh count caused by this click
        dialog_target=id;dialog_value=value;unsigned before=dialog_refreshes;
        merit_dialog_toggle(data,nullptr);return dialog_refreshes-before;
    };
    auto setup=[&](unsigned id,unsigned military,unsigned civil,unsigned chosen) {
        Obj p=table(0x158,id,6000);p[0x1C2]=static_cast<unsigned char>(military);p[0x1C3]=static_cast<unsigned char>(civil);node(id)=static_cast<unsigned char>(chosen);
    };
    // Military only: civil raise undone, military raise and lowering stay.
    setup(110,2,0,0x02);expect(click(110,0x12)==1 && node(110)==0x02);
    expect(click(110,0x03)==0 && node(110)==0x03);expect(click(110,0x01)==0 && node(110)==0x01);
    // Civil only (mirror).
    setup(111,0,2,0x20);expect(click(111,0x21)==1 && node(111)==0x20);
    expect(click(111,0x30)==0 && node(111)==0x30);
    // Nothing held, both picked: the first pick stands, the opposite raise is undone, deselecting frees the other route.
    setup(112,0,0,0x00);expect(click(112,0x01)==0 && node(112)==0x01);
    expect(click(112,0x11)==1 && node(112)==0x01);
    expect(click(112,0x00)==0 && node(112)==0x00);expect(click(112,0x10)==0 && node(112)==0x10);
    expect(click(112,0x11)==1 && node(112)==0x10);
    // Held one route but the dialog entry was lowered to 0 first: the other route is still refused (held route wins).
    setup(110,2,0,0x00);expect(click(110,0x10)==1 && node(110)==0x00);
    // Both held (abnormal): only the winner (larger, tie civil) may grow, the loser never.
    setup(110,2,1,0x12);expect(click(110,0x22)==1 && node(110)==0x12);expect(click(110,0x13)==0 && node(110)==0x13);
    setup(111,1,1,0x11);expect(click(111,0x12)==1 && node(111)==0x11);expect(click(111,0x21)==0 && node(111)==0x21);
    // Untouched rows: a click on one officer never rewrites another.
    setup(112,0,0,0x00);node(110)=0x13;expect(click(112,0x01)==0 && node(110)==0x13 && node(112)==0x01);
    // Disabled mod: native handler only, no refresh. A broken map is left alone too.
    setup(110,2,0,0x02);enabled=0;unsigned calls=dialog_native_calls;expect(click(110,0x12)==0 && node(110)==0x12 && dialog_native_calls==calls+1);enabled=1;
    putptr(data,0x198,nullptr);calls=dialog_native_calls;expect(click(110,0x22)==0 && dialog_native_calls==calls+1);putptr(data,0x198,tree.head);
#ifndef NO_RUNTIME_LOG
    {Obj current=world();merit_dual_world=nullptr;p1[0x1C2]=1;p1[0x1C3]=1;merit_dual_note(p1);expect(merit_dual_seen[110]==1 && merit_dual_world==current);
     p1[0x1C2]=0;p1[0x1C3]=0;}
#endif
    free(data);for(Obj p:{p1,p2,p3}) {slot(0x158,word(p,0x10),nullptr);free(p);}
}
// local8 lock display and auto fill (user 2026-11-06). The mocks follow the extracted 6DBAA0 / 6C6810 logic with three tiers.
static std::map<Obj,std::wstring> lock_labels;static unsigned lock_label_calls,fill_cost,fill_refreshes;
static void __fastcall mock_lock_label(Obj label,const wchar_t *text) {++lock_label_calls;lock_labels[label]=text;}
static const wchar_t *__fastcall mock_tier(int slot) {static const wchar_t *t[]={L"",L"T1",L"T2",L"T3",L"T4",L"T5",L"T6"};return t[slot];}
static void __fastcall mock_slot_states(Obj data,Obj person,int *s) {
    for(unsigned i=0;i<6;++i)s[i]=3;
    unsigned char chosen=0;merit_dialog_entry(data,word(person,0x10),chosen);
    unsigned cm=chosen&15,cc=chosen>>4,am=person[0x1C2],ac=person[0x1C3],tiers=person[0x14C]?person[0x14C]:3;
    for(unsigned i=0;i<tiers;++i) {s[i]=i<cm?(i<am)+1:0;s[3+i]=i<cc?(i<ac)+1:0;}
    if(cm+cc<tiers) {s[cm]=1;s[3+cc]=1;}
}
static void __fastcall mock_fill_refresh(Obj data) {   // 783F50 cost: officers whose map entry differs from the held tiers
    ++fill_refreshes;fill_cost=0;std::vector<MeritDialogRow> rows;merit_map_rows(data+0x198,rows);
    for(const auto &row:rows) {Obj p=table(0x158,row.id,6000);if(row.packed!=(p[0x1C2]|p[0x1C3]<<4))++fill_cost;}
}
static void __fastcall mock_autofill(Obj data,int mode) {
    std::vector<MeritDialogRow> rows;merit_map_rows(data+0x198,rows);
    for(auto &row:rows) {
        Obj p=table(0x158,row.id,6000);unsigned m=p[0x1C2],c=p[0x1C3];
        bool civil=mode==3?(p[0x30]+p[0x31]<p[0x32]+p[0x33]):mode!=4;   // stand-in for the stat comparison
        if(m+c<3) {if(civil)++c;else ++m;}
        row.node[0x20]=static_cast<unsigned char>(m|c<<4);
    }
    mock_fill_refresh(data);
}
static void test_lock_display() {
    original_merit_slot_states=&mock_slot_states;original_merit_autofill=&mock_autofill;
    functions[0x877D50]=reinterpret_cast<void *>(&mock_lock_label);functions[0x358BF0]=reinterpret_cast<void *>(&mock_tier);
    functions[0x783F50]=reinterpret_cast<void *>(&mock_fill_refresh);
    Obj p1=object(110),p2=object(111),p3=object(112);for(Obj p:{p1,p2,p3})slot(0x158,word(p,0x10),p);
    CommandTree tree(110,111,112);Obj data=static_cast<Obj>(calloc(1,0x400));putptr(data,0x198,tree.head);
    auto node=[&](unsigned id)->unsigned char & {return tree.nodes[id-110][0x20];};
    Obj row=static_cast<Obj>(calloc(1,0x300));Obj labels[6];
    for(unsigned i=0;i<6;++i) {labels[i]=static_cast<Obj>(calloc(1,16));putptr(row,0x1E8+i*8,labels[i]);}
    putptr(row,8,data);
    auto states=[&](Obj p,unsigned m,unsigned c,unsigned chosen,int *s) {
        p[0x1C2]=static_cast<unsigned char>(m);p[0x1C3]=static_cast<unsigned char>(c);node(word(p,0x10))=static_cast<unsigned char>(chosen);
        putptr(row,0x178,p);merit_slot_states(data,p,s,row);
    };
    auto same=[](const int *s,std::initializer_list<int> want) {unsigned i=0;for(int w:want)if(s[i++]!=w)return false;return true;};
    int s[6];
    // Military held (2/0), nothing new picked: civil slots locked with our text, military stays native.
    unsigned calls=lock_label_calls;   // local9: lock text stays native, the hook never writes labels
    states(p1,2,0,0x02,s);expect(same(s,{2,2,1,3,3,3}));
    // The live 842 case (held 2/0, stale civil pick 0x12 from the old auto fill): the picked civil tier stays clickable so it can
    // be undone, the rest of civil locks, held military kept.
    states(p1,2,0,0x12,s);expect(same(s,{2,2,0,1,3,3}));
    // Civil held (mirror).
    states(p2,0,1,0x10,s);expect(same(s,{3,3,3,2,1,0}));
    // Nothing held, nothing picked: native states, our texts restored on the reused row.
    states(p3,0,0,0x00,s);expect(same(s,{1,0,0,1,0,0}));
    // Nothing held, military picked: civil locked until the pick is undone.
    states(p3,0,0,0x01,s);expect(same(s,{1,1,0,3,3,3}));
    states(p3,0,0,0x00,s);expect(same(s,{1,0,0,1,0,0}));
    // Both held (abnormal): the loser route keeps its held slots, the rest locks.
    states(p1,2,1,0x12,s);expect(same(s,{2,2,0,2,3,3}));
    // Native tier lock (rank allows two tiers) on an allowed route keeps the native text.
    p2[0x14C]=2;states(p2,0,1,0x10,s);expect(same(s,{3,3,3,2,1,3}) && lock_label_calls==calls);p2[0x14C]=0;
    // Wrong call shape (row does not belong to this call) or mod off: native states, no label writes.
    p1[0x1C2]=2;p1[0x1C3]=0;node(110)=0x02;putptr(row,0x178,p2);merit_slot_states(data,p1,s,row);
    expect(same(s,{2,2,1,1,0,0}) && lock_label_calls==calls);
    enabled=0;putptr(row,0x178,p1);merit_slot_states(data,p1,s,row);expect(same(s,{2,2,1,1,0,0}) && lock_label_calls==calls);enabled=1;
    // Auto fill: each officer fills only the route he holds; 0/0 follows the chosen priority (5 civil, 4 military).
    auto fill=[&](int mode,unsigned m1,unsigned c1,unsigned m2,unsigned c2) {
        p1[0x1C2]=static_cast<unsigned char>(m1);p1[0x1C3]=static_cast<unsigned char>(c1);
        p2[0x1C2]=static_cast<unsigned char>(m2);p2[0x1C3]=static_cast<unsigned char>(c2);p3[0x1C2]=0;p3[0x1C3]=0;
        node(110)=node(111)=node(112)=0;fill_refreshes=0;merit_autofill(data,mode);
    };
    fill(5,1,0,0,1);   // civil first: military holder 110 gets military, civil holder 111 civil, 0/0 112 civil
    expect(node(110)==0x02 && node(111)==0x20 && node(112)==0x10 && fill_refreshes==2 && fill_cost==3);
    fill(4,1,0,0,1);   // military first: civil holder 111 keeps civil, 0/0 112 military
    expect(node(110)==0x02 && node(111)==0x20 && node(112)==0x01 && fill_refreshes==2 && fill_cost==3);
    fill(4,2,0,0,2);   // held route already matches the priority for 110: no fix needed for him
    expect(node(110)==0x03 && node(111)==0x30 && node(112)==0x01);
    fill(5,0,0,0,0);expect(node(110)==0x10 && node(111)==0x10 && node(112)==0x10 && fill_refreshes==1 && fill_cost==3);
    fill(4,0,0,0,0);expect(node(110)==0x01 && node(111)==0x01 && node(112)==0x01 && fill_refreshes==1);
    fill(4,1,1,2,0);   // both held 1/1 (tie: civil wins) moves to civil; 2/0 military
    expect(node(110)==0x21 && node(111)==0x03 && fill_cost==3);
    fill(5,2,1,0,0);expect(node(110)==0x12 && fill_cost==2);   // full (sum 3): untouched, no raise
    p1[0x30]=1;p1[0x31]=1;p1[0x32]=9;p1[0x33]=9;fill(3,1,0,0,0);expect(node(110)==0x02);   // stats favour civil, held military wins
    enabled=0;fill(5,1,0,0,0);expect(node(110)==0x11 && fill_refreshes==1);enabled=1;
    free(row);for(Obj l:labels)free(l);
    free(data);for(Obj p:{p1,p2,p3}) {slot(0x158,word(p,0x10),nullptr);free(p);}
    functions[0x783F50]=reinterpret_cast<void *>(&mock_refresh);
}
static void test_publication() {
    Obj saved=image;
    Obj memory=static_cast<Obj>(VirtualAlloc(nullptr,0x2801000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    expect(memory!=nullptr);image=memory;Obj relay=memory+0x2800000;
    for(const auto &entry:merit_entries)memcpy(image+entry.rva,entry.bytes,entry.size);
    const unsigned char save[]={0xFF,0x10,0x48,0x83,0x7B,0x18,0x10},load[]={0xFF,0x50,0x08,0x85,0xC0};
    memcpy(image+0x3FA342,save,sizeof(save));memcpy(image+0x3FCAC4,load,sizeof(load));
    putptr(image,0x13E9A30,image+0x50DD30);putptr(image,0x13E9188,image+0x50BD20);putptr(image,0x13A9BF0,image+0x23AA60);
    putptr(image,0x1416690,image+0x90ACC0);putptr(image,0x1416518,image+0x87ABC0);
    memcpy(image+GA(GA_7726B0),merit_dialog_bytes,sizeof(merit_dialog_bytes));
    memcpy(image+GA(GA_6DBAA0),merit_slot_bytes,5);memcpy(image+GA(GA_6C6810),merit_autofill_bytes,6);
    MeritCode code;expect(merit_guards_match() && prepare_merit(code,relay) && code.dialog_ready && code.lock_ready && code.autofill_ready);
    expect(!memcmp(relay+1120,merit_dialog_bytes,5) && pointer(relay+1120,11)==image+GA(GA_7726B0)+5);
    // Lock stub: mov r9,rbp then jump to the wrapper; gate = prologue + jump back. Auto fill: six bytes, sixth patched byte NOP.
    expect(relay[1152]==0x49 && relay[1153]==0x89 && relay[1154]==0xE9 && pointer(relay+1155,6)==reinterpret_cast<Obj>(&merit_slot_states));
    expect(!memcmp(relay+1184,merit_slot_bytes,5) && pointer(relay+1184,11)==image+GA(GA_6DBAA0)+5);
    expect(!memcmp(relay+1248,merit_autofill_bytes,6) && pointer(relay+1248,12)==image+GA(GA_6C6810)+6);
    expect(pointer(relay+1216,6)==reinterpret_cast<Obj>(&merit_autofill));
    for(unsigned i=0;i<6;++i) {
        expect(!memcmp(relay+544+i*64,merit_entries[i].bytes,merit_entries[i].size));
        expect(pointer(relay+544+i*64,merit_entries[i].size+6)==image+merit_entries[i].rva+merit_entries[i].size);
    }
    int32_t relative=*reinterpret_cast<int32_t *>(relay+928+3);
    expect(relay+928+7+relative==image+0x2780F60);
    expect(pointer(relay+928,13)==image+0x3589B7);
    expect(!memcmp(relay+992,merit_entries[7].bytes,5));
    expect(pointer(relay+992,11)==image+0x21CCC5);
    expect(publish_merit(code) && code.links[10].published && image[GA(GA_7726B0)]==0xE9);
    expect(code.links[11].published && image[GA(GA_6DBAA0)]==0xE9 && code.links[12].published && image[GA(GA_6C6810)]==0xE9 &&
           image[GA(GA_6C6810)+5]==0x90);expect(pointer(image,0x13A9BF0)==reinterpret_cast<Obj>(&merit_person_read));
    restore_merit(code);expect(merit_guards_match() && !memcmp(image+GA(GA_7726B0),merit_dialog_bytes,5));
    expect(!memcmp(image+GA(GA_6DBAA0),merit_slot_bytes,5) && !memcmp(image+GA(GA_6C6810),merit_autofill_bytes,6));
    // Lock display and auto fill fail on their own: changed bytes switch only that part off.
    image[GA(GA_6DBAA0)+1]^=1;image[GA(GA_6C6810)+3]^=1;
    {MeritCode other;expect(prepare_merit(other,relay) && other.dialog_ready && !other.lock_ready && !other.autofill_ready && publish_merit(other) &&
                            other.links[10].published && image[GA(GA_6DBAA0)]==0x40 && image[GA(GA_6C6810)]==0x40);restore_merit(other);}
    image[GA(GA_6DBAA0)+1]^=1;image[GA(GA_6C6810)+3]^=1;
    // The dialog hook fails on its own: changed bytes there keep every other merit hook installed.
    image[GA(GA_7726B0)+1]^=1;{MeritCode other;expect(prepare_merit(other,relay) && !other.dialog_ready && publish_merit(other) && image[GA(GA_7726B0)]==0x40);restore_merit(other);}
    image[GA(GA_7726B0)+1]^=1;
    expect(prepare_merit(code,relay));image[merit_entries[2].rva+10]^=1;
    expect(!publish_merit(code));restore_merit(code);
    expect(!memcmp(image+merit_entries[0].rva,merit_entries[0].bytes,5));
    image=saved;VirtualFree(memory,0,MEM_RELEASE);
}
int main() {
    test_store();image=static_cast<Obj>(calloc(1,0x2800000));root=static_cast<Obj>(calloc(1,0x90000));
    putptr(image,0x2780F60,root);putptr(image,0x27D4138,root);putptr(image,0x27D4140,heads);put32(image,0x27D4170,256);
    putptr(image,0x27D1108,list_counts);
    enabled=1;merit_world=root;
    functions={{0x331780,reinterpret_cast<void *>(&mock_valid)},{0x22E450,reinterpret_cast<void *>(&mock_force)},
        {0x01AC60,reinterpret_cast<void *>(&mock_init)},{0x18D50,reinterpret_cast<void *>(&mock_add)},
        {0x1BFFF0,reinterpret_cast<void *>(&mock_free)}};
    force1=object(10);force2=object(20);slot(0xDF30,1,force1);slot(0xDF30,2,force2);
    for(unsigned id=1;id<=2;++id) {Obj d=object(id);d[0x10]=static_cast<unsigned char>(id);slot(0xE0D0,id,d);}
    test_bindings();test_dialog();test_lock_display();test_persistence();test_profile();test_captured_combat();test_publication();
    printf("{\"status\":\"merit_native_fixture_passed\",\"checks\":%u,\"installed\":false,\"gameplay_verified\":false,\"ui_verified\":false}\n",checks);
    return 0;
}
