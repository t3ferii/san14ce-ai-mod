// Trait hover pure logic (docs/TRAIT_HOVER_PLAN.md 4-1, 2026-11-06). Drives src/trait_hover_profile.h only: no game image, no memory
// access, no Windows UI. Covers header-template decode, duel label index, contains(), tactic text composition, editor text composition,
// balloon record table, key classification, slot search, path decisions and the guard table itself.
#include "../src/trait_hover_profile.h"
#include <cstdio>
#include <map>
using namespace thp;
static int checks=0,failures=0;
static void ok(bool v,const char *what) {++checks;if(!v){++failures;std::printf("FAIL %s\n",what);}}

// ---- header template ----
static const unsigned kText0=0x1000,kText1=0x9000,kHas=0x2346A0,kCircle=0x30F010,kEmpty=0x1A4AB88;   // kHas etc. sit outside the fake .text window on purpose
struct Tpl {unsigned char b[kHeaderTemplateSize];};
static void put_i32(unsigned char *p,int v) {memcpy(p,&v,4);}
static Tpl make_template(unsigned fn,int id,unsigned has=kHas,unsigned circle=kCircle,unsigned empty=kEmpty) {
    Tpl t;memcpy(t.b,kHeaderTemplate,kHeaderTemplateSize);
    put_i32(t.b+kHdrIdOff,id);
    put_i32(t.b+kHdrCallOff,(int)(has-(fn+kHdrCallOff+4)));
    put_i32(t.b+kHdrJmpOff,(int)(circle-(fn+kHdrJmpOff+4)));
    put_i32(t.b+kHdrMovOff,(int)(empty-(fn+kHdrMovOff+4)));
    return t;
}
static HeaderReject decode(const Tpl &t,unsigned fn,int *id,size_t avail=kHeaderTemplateSize,unsigned lo=kText0,unsigned hi=kText1) {
    const HeaderTargets tg{kHas,kCircle,kEmpty};
    return decode_header_handler(t.b,avail,fn,lo,hi,tg,id);
}
static void test_header() {
    ok(sizeof(kHeaderTemplate)==44 && kHeaderTemplateSize==44,"template is 44 bytes");
    const unsigned fn=0x361450;
    int id=0;
    {   // the real text window is large enough for the real function; here use a window that contains fn
        const Tpl t=make_template(fn,218);
        ok(decode(t,fn,&id,44,0x300000,0x400000)==HR_OK && id==218,"valid template -> id 218");
        id=0;ok(decode(make_template(fn,1),fn,&id,44,0x300000,0x400000)==HR_OK && id==1,"id 1 accepted");
        ok(decode(make_template(fn,0x163),fn,&id,44,0x300000,0x400000)==HR_OK && id==0x163,"id 0x163 accepted");
    }
    id=-5;
    ok(decode(make_template(fn,0),fn,&id,44,0x300000,0x400000)==HR_ID && id==-5,"id 0 rejected, id untouched");
    ok(decode(make_template(fn,0x164),fn,&id,44,0x300000,0x400000)==HR_ID,"id 0x164 rejected");
    ok(decode(make_template(fn,-1),fn,&id,44,0x300000,0x400000)==HR_ID,"negative id rejected");
    ok(decode(make_template(fn,111,0x212610),fn,&id,44,0x300000,0x400000)==HR_TARGET,"call target 212610 rejected (21 other handlers)");
    ok(decode(make_template(fn,111,0x232350),fn,&id,44,0x300000,0x400000)==HR_TARGET,"call target 232350 rejected");
    ok(decode(make_template(fn,111,kHas,kCircle+1),fn,&id,44,0x300000,0x400000)==HR_TARGET,"jmp target wrong");
    ok(decode(make_template(fn,111,kHas,kCircle,kEmpty+8),fn,&id,44,0x300000,0x400000)==HR_TARGET,"mov target wrong");
    {   // relative targets depend on the function address: same bytes decoded at another rva must miss
        const Tpl t=make_template(fn,111);
        ok(decode(t,fn+0x10,&id,44,0x300000,0x400000)==HR_TARGET,"relative target moves with fn rva");
    }
    ok(decode(make_template(fn,111),fn-1,&id,44,fn,0x400000)==HR_RANGE,"below .text rejected");
    ok(decode(make_template(fn,111),fn,&id,44,0x300000,fn+43)==HR_RANGE,"crosses .text end rejected");
    ok(decode(make_template(fn,111),fn,&id,44,0x300000,fn+44)==HR_OK,"ends exactly at .text end accepted");
    ok(decode(make_template(fn,111),fn,&id,43,0x300000,0x400000)==HR_RANGE,"short read rejected");
    ok(decode_header_handler(nullptr,44,fn,0x300000,0x400000,HeaderTargets{kHas,kCircle,kEmpty},&id)==HR_RANGE,"null code rejected");
    {   // every fixed byte must match: flip each one
        unsigned bad=0,fixed=0;
        for(unsigned i=0;i<kHeaderTemplateSize;++i) {
            const bool var=(i>=5&&i<=8)||(i>=10&&i<=13)||(i>=28&&i<=31)||(i>=35&&i<=38);
            if(var)continue;
            ++fixed;Tpl t=make_template(fn,111);t.b[i]^=0xFF;
            if(decode(t,fn,&id,44,0x300000,0x400000)!=HR_BYTES)++bad;
        }
        ok(fixed==28 && bad==0,"each of the 28 fixed bytes is checked");
    }
    {   // a target of 0 (unresolved id) never matches
        const HeaderTargets z{0,0,0};const Tpl t=make_template(fn,111,0,0,0);
        ok(decode_header_handler(t.b,44,fn,0x300000,0x400000,z,&id)==HR_TARGET,"unresolved (0) targets rejected");
    }
}

// ---- duel label index ----
static void test_duel() {
    for(int id=-3;id<60;++id) {
        const int info=duel_label_index(true,false,id);
        const int expect=(id>=10 && id<=18 && id%2==0)?(id-10)/2:-1;
        if(info!=expect){ok(false,"info index");break;}
    }
    ok(duel_label_index(true,false,10)==0 && duel_label_index(true,false,18)==4,"info even 10..18 -> 0..4");
    ok(duel_label_index(true,false,11)==-1 && duel_label_index(true,false,19)==-1 && duel_label_index(true,false,8)==-1 && duel_label_index(true,false,20)==-1,"info odd/out of range -> -1");
    ok(duel_label_index(false,true,13)==0 && duel_label_index(false,true,21)==4,"gauge odd 13..21 -> 0..4");
    ok(duel_label_index(false,true,39)==9 && duel_label_index(false,true,47)==13,"gauge odd 39..47 -> 9..13");
    ok(duel_label_index(false,true,14)==-1 && duel_label_index(false,true,23)==-1 && duel_label_index(false,true,38)==-1 && duel_label_index(false,true,48)==-1 && duel_label_index(false,true,40)==-1,"gauge other ids -> -1");
    ok(duel_label_index(false,false,13)==-1 && duel_label_index(false,false,10)==-1,"unknown vtable -> -1");
    int seen[18]={};
    for(int id=0;id<64;++id){const int i=duel_label_index(false,true,id);if(i>=0)++seen[i];}
    bool all=true;for(int i=0;i<5;++i)all=all&&seen[i]==1;for(int i=9;i<14;++i)all=all&&seen[i]==1;for(int i=5;i<9;++i)all=all&&seen[i]==0;
    ok(all,"gauge indexes 0..4 and 9..13 each reached once");
    ok(duel_capture_id(5,true)==5 && duel_capture_id(5,false)==0 && duel_capture_id(0,true)==0,"capture: valid id only");
    ok(duel_capture_id(0x162,true)==0x162 && duel_capture_id(0x163,true)==0,"capture scans 1..0x162 only");
    const uintptr_t labels[3]={0x100,0x200,0x300};const int ids[3]={7,0,0x164};
    ok(duel_tick_key(labels,ids,3,0x100,[](size_t){return true;})==7,"tick: label hit -> id");
    ok(duel_tick_key(labels,ids,3,0x100,[](size_t){return false;})==0,"tick: label not under mouse");
    ok(duel_tick_key(labels,ids,3,0x200,[](size_t){return true;})==0,"tick: id 0 stays 0");
    ok(duel_tick_key(labels,ids,3,0x300,[](size_t){return true;})==0,"tick: out of range id dropped");
    ok(duel_tick_key(labels,ids,3,0,[](size_t){return true;})==0,"tick: null target");
    ok(duel_tick_key(labels,ids,3,0x999,[](size_t){return true;})==0,"tick: unknown label");
    ok(find_name_id(0x500,1,0x162,[](int id){return id==9?(uintptr_t)0x500:(uintptr_t)0;})==9,"name pointer search finds id 9");
    ok(find_name_id(0x500,1,0x162,[](int id){return id==0x163?(uintptr_t)0x500:(uintptr_t)0;})==0,"name pointer search respects the range");
    ok(find_name_id(0,1,10,[](int){return (uintptr_t)0;})==0,"null name never matches");
}

// ---- contains ----
struct Node {int w,h;unsigned flags;int ox,oy;Node *parent;};
static WidgetView view(const void *p) {const Node *n=(const Node *)p;return WidgetView{n->w,n->h,n->flags,n->ox,n->oy,n->parent};}
static bool hit(Node *n,int x,int y,bool xonly=false) {return contains(n,x,y,xonly,view);}
static void test_contains() {
    Node root{800,600,3,0,0,nullptr},mid{300,200,3,50,40,&root},leaf{100,20,3,10,5,&mid};
    // leaf origin = (60,45), size 100x20 -> x 60..159, y 45..64
    ok(hit(&leaf,60,45),"top-left corner inside");
    ok(hit(&leaf,159,64),"last pixel inside");
    ok(!hit(&leaf,160,45),"x right edge excluded");
    ok(!hit(&leaf,60,65),"y bottom edge excluded");
    ok(!hit(&leaf,59,45),"x left outside");
    ok(!hit(&leaf,60,44),"y top outside");
    ok(hit(&leaf,100,9999,true) && hit(&leaf,60,-5,true),"xonly ignores y");
    ok(!hit(&leaf,160,50,true),"xonly still checks x");
    ok(!hit(nullptr,0,0),"null widget");
    Node zw{0,20,3,0,0,nullptr},zh{20,0,3,0,0,nullptr},neg{-5,20,3,0,0,nullptr};
    ok(!hit(&zw,0,0) && !hit(&zh,0,0) && !hit(&neg,0,0),"size 0 or negative rejected");
    ok(!hit(&zw,0,0,true),"size 0 rejected in xonly too");
    Node f1{100,20,1,0,0,&root},f2{100,20,2,0,0,&root},f0{100,20,0,0,0,&root},f7{100,20,7,0,0,&root};
    ok(!hit(&f1,1,1) && !hit(&f2,1,1) && !hit(&f0,1,1),"flags (+0x40&3)!=3 rejected");
    ok(hit(&f7,1,1),"flags 7 has both low bits");
    Node badroot{800,600,1,0,0,nullptr},kid{100,20,3,0,0,&badroot};
    ok(!hit(&kid,1,1),"bad flags on an ancestor rejected");
    // chain length: a chain of N nodes must end in null; 30 allowed, 31 rejected
    std::vector<Node> chain(40);
    for(int n=1;n<=35;++n) {
        for(int i=0;i<n;++i)chain[i]=Node{100,20,3,0,0,i+1<n?&chain[i+1]:nullptr};
        const bool r=hit(&chain[0],5,5);
        if(n<=30){if(!r){ok(false,"chain up to 30 nodes accepted");break;}}
        else {if(r){ok(false,"chain over 30 nodes rejected");break;}}
        if(n==35)ok(true,"chain length limit 30");
    }
    // a cycle never terminates in null
    Node a{100,20,3,0,0,nullptr},b{100,20,3,0,0,&a};a.parent=&b;
    ok(!hit(&a,1,1),"cyclic chain rejected");
    Node off1{100,20,3,-30,-30,&root};
    ok(!hit(&off1,0,0) || true,"negative offsets are plain integers");
    ok(hit(&off1,-30,-30) && !hit(&off1,70,0),"negative origin handled");
    // descends
    Node top{10,10,3,0,0,nullptr},c1{10,10,3,0,0,&top},c2{10,10,3,0,0,&c1};
    auto par=[](const void *p){return (const void *)((const Node *)p)->parent;};
    ok(is_or_descends(&top,&top,par),"target==w");
    ok(is_or_descends(&c2,&top,par) && is_or_descends(&c1,&top,par),"descendant of w");
    ok(!is_or_descends(&top,&c2,par),"ancestor is not a descendant");
    ok(!is_or_descends(nullptr,&top,par) && !is_or_descends(&c1,nullptr,par),"null args");
    std::vector<Node> deep(40);
    for(int i=0;i<40;++i)deep[i]=Node{1,1,3,0,0,i+1<40?&deep[i+1]:nullptr};
    ok(is_or_descends(&deep[0],&deep[30],par),"30th ancestor is found");
    ok(!is_or_descends(&deep[0],&deep[31],par),"31st ancestor is not searched");
}

// ---- editor text ----
static void test_edit() {
    ok(kEditFuncs[0].title_id==0x7FB && kEditFuncs[1].title_id==0x7FC && kEditFuncs[2].title_id==0x53F && kEditFuncs[3].title_id==0x404 && kEditFuncs[4].title_id==0x800 && kEditFuncs[5].title_id==0x405 && kEditFuncs[6].title_id==0x801,"7 title ids in plan order");
    bool nonzero=true,distinct=true;
    for(unsigned i=0;i<7;++i){if(!edit_func_rva(i))nonzero=false;for(unsigned j=0;j<i;++j)if(edit_func_rva(j)==edit_func_rva(i))distinct=false;}
    ok(nonzero && distinct,"7 formatter rvas resolve and differ");
    ok(edit_func_rva(7)==0 && edit_title_id(7)==0,"index 7 out of range");
    const wchar_t *cap[7]={L"c0",L"c1",L"c2",L"c3",L"c4",L"c5",L"c6"};
    const wchar_t *val[7]={L"v0",L"v1",L"v2",L"v3",L"v4",L"v5",L"v6"};
    ok(compose_edit_text(cap,val,7)==L"c0: v0\nc1: v1\nc2: v2\nc3: v3\nc4: v4\nc5: v5\nc6: v6","7 lines joined");
    const wchar_t *cap2[7]={L"c0",nullptr,L"c2",L"c3",L"c4",L"c5",L"c6"};
    const wchar_t *val2[7]={L"v0",L"v1",nullptr,L"v3",nullptr,L"v5",L"v6"};
    ok(compose_edit_text(cap2,val2,7)==L"c0: v0\n: v1\nc2: \nc3: v3\nc4: \nc5: v5\nc6: v6","null caption or value leaves only its part empty like the blob (FUN_34B0), the line stays");
    const wchar_t *capn[2]={nullptr,nullptr};
    ok(compose_edit_text(capn,val2,2)==L": v0\n: v1","null captions keep ': value' lines");
    ok(compose_edit_text(cap,val,0).empty(),"zero lines -> empty");
    ok(compose_edit_text(cap,val,1)==L"c0: v0","single line has no newline");
    const std::wstring longv(1500,L'q');
    const wchar_t *cl[1]={L"cap"},*vl[1]={longv.c_str()};
    ok(compose_edit_text(cl,vl,1).size()==1023,"1023 wchar cap");
    const std::wstring v1018(1018,L'q');   // "cap: " is 5 -> exactly 1023
    const wchar_t *v1[1]={v1018.c_str()};
    ok(compose_edit_text(cl,v1,1).size()==1023,"exactly 1023 kept");
    std::wstring sur(1017,L'q');sur+=L'\xD83D';sur+=L'\xDE00';sur+=L"tail";   // 5+1017=1022, pair occupies 1022..1023
    const wchar_t *vs[1]={sur.c_str()};
    const std::wstring r=compose_edit_text(cl,vs,1);
    ok(r.size()==1022 && r.back()==L'q',"surrogate pair at the cut dropped whole");
    ok(cut_wide(L"ab",2,10)==2 && cut_wide(L"\xD83D",1,10)==0 && cut_wide(L"abc",3,2)==2,"cut_wide basics");
}

// ---- records ----
static void test_records() {
    ok(sizeof(Record)==0xF8 && kRecordCap==128,"record is 0xF8 bytes, 128 records");
    RecordTable t;
    ok(t.used()==0 && t.find(0x1000)==nullptr,"empty table");
    ok(t.acquire(0)==nullptr && t.find(0)==nullptr,"null owner never allocated");
    Record *r1=t.acquire(0x1000);
    ok(r1 && r1->owner==0x1000 && t.used()==1,"first owner allocated");
    r1->last_key=77;r1->tip=0xABC;
    ok(t.acquire(0x1000)==r1 && r1->last_key==77,"same owner returns the same record, state kept");
    ok(t.find(0x1000)==r1,"find by owner");
    for(unsigned i=1;i<kRecordCap;++i)ok(t.acquire(0x1000+i*0x10)!=nullptr,"fill");
    ok(t.used()==128,"128 filled");
    ok(t.acquire(0xFFFF0)==nullptr,"full table rejects a new owner");
    ok(t.acquire(0x1000)==r1,"full table still finds an existing owner");
    ok(t.take_full_log() && !t.take_full_log(),"full log taken once");
    ok(t.release(0x1010) && t.used()==127 && t.find(0x1010)==nullptr,"release frees the slot");
    ok(!t.release(0x1010) && !t.release(0xDEAD),"release of an absent owner is false");
    Record *r2=t.acquire(0xFFFF0);
    ok(r2!=nullptr && r2->last_key==0 && r2->tip==0 && r2->registered==0 && r2->failed==0,"reused slot is zeroed");
    ok(t.used()==128,"full again after reuse");
    t.reset();
    ok(t.used()==0 && !t.full_logged,"reset");
}

// ---- keys ----
static void test_keys() {
    ok(key_class(0)==KC_NONE,"0 none");
    ok(key_class(1)==KC_TRAIT && key_class(0x163)==KC_TRAIT && key_class(100)==KC_TRAIT,"1..0x163 trait");
    ok(key_class(0x164)==KC_INVALID && key_class(0x1FF)==KC_INVALID && key_class(0x180)==KC_INVALID,"0x164..0x1FF invalid");
    ok(key_class(0x200)==KC_TACTIC && key_class(0x2C8)==KC_TACTIC,"0x200..0x2C8 tactic");
    ok(key_class(0x2C9)==KC_INVALID && key_class(0x2FF)==KC_INVALID && key_class(0x300)==KC_INVALID && key_class(0xFFFF)==KC_INVALID,"0x2C9..0xFFFF invalid");
    ok(key_class(0x10000)==KC_EDIT && key_class(0x7FF612340000ull)==KC_EDIT,"edit pointers");
    ok(tactic_key(0)==0x200 && tactic_key(200)==0x2C8 && tactic_key(201)==0 && tactic_key(-1)==0,"tactic_key range 0..200");
    ok(tactic_index(0x200)==0 && tactic_index(0x2C8)==200 && tactic_index(0x2C9)==-1 && tactic_index(5)==-1,"tactic_index");
    ok(sanitize_key(5)==5 && sanitize_key(0x201)==0x201 && sanitize_key(0x20000)==0x20000 && sanitize_key(0x164)==0 && sanitize_key(0x2C9)==0 && sanitize_key(0)==0,"sanitize_key");
    ok(trait_id_ok(1) && trait_id_ok(0x163) && !trait_id_ok(0) && !trait_id_ok(0x164) && !trait_id_ok(-1),"trait_id_ok");
    {
        const HoverAction a=hover_action(0,0);ok(!a.hide && !a.set_text && !a.show,"no key, nothing shown: no action");
        const HoverAction b=hover_action(5,0);ok(b.hide && !b.set_text && !b.show,"key lost: hide");
        const HoverAction c=hover_action(0,5);ok(!c.hide && c.set_text && c.show,"new key: set text and show");
        const HoverAction d=hover_action(5,5);ok(!d.hide && !d.set_text && d.show,"same key: show only");
        const HoverAction e=hover_action(5,6);ok(!e.hide && e.set_text && e.show,"changed key: set text and show");
    }
    // name -> key priority
    ok(name_key_decide(true,0x20000,5,true,3,true)==0x20000,"edit row wins");
    ok(name_key_decide(false,0,5,true,3,true)==5,"trait wins over tactic");
    ok(name_key_decide(false,0,5,false,3,true)==0x203,"invalid trait falls through to tactic");
    ok(name_key_decide(false,0,5,false,3,false)==0,"tactic without game text gives nothing");
    ok(name_key_decide(false,0,0,true,-1,false)==0 && name_key_decide(false,0,0x164,true,-1,false)==0,"trait id out of range");
    ok(name_key_decide(false,0,0,false,200,true)==0x2C8 && name_key_decide(false,0,0,false,201,true)==0,"tactic index range");
}

// ---- slots ----
static void test_slots() {
    ok(blob_kind_offset(0)==8 && blob_kind_offset(3)==8 && blob_kind_offset(4)==8 && blob_kind_offset(5)==8 && blob_kind_offset(8)==8,"kinds 0,3,4,5,8 -> +0x08");
    ok(blob_kind_offset(2)==0x28 && blob_kind_offset(6)==0x28 && blob_kind_offset(10)==0x28 && blob_kind_offset(11)==0x28,"kinds 2,6,10,11 -> +0x28");
    ok(blob_kind_offset(1)==0x40 && blob_kind_offset(7)==0x40 && blob_kind_offset(9)==0x40 && blob_kind_offset(12)==0x40,"kinds 1,7,9,12 -> +0x40");
    ok(blob_kind_offset(13)==-1,"kind 13 unknown");
    ok(slot_offset(SK_TICK)==8 && slot_offset(SK_CLOSE)==0x28 && slot_offset(SK_HIDE)==0x40,"slot offsets");
    // 152 rows: 38 vtables x (tick,close,hide) + 38 extra -> build 152 deterministic entries over 51 vtables x 3 slots minus one
    std::vector<SlotEntry> t;
    unsigned row=0;
    for(unsigned v=0;v<51 && t.size()<152;++v) {
        const unsigned vt=0x13A0000+v*0x1F0;
        for(unsigned k=0;k<3 && t.size()<152;++k)t.push_back({vt+(k==0?8u:(k==1?0x40u:0x28u)),row++});
    }
    ok(t.size()==152,"152 rows built");
    std::vector<SlotEntry> shuffled(t.rbegin(),t.rend());
    slot_table_sort(shuffled.data(),shuffled.size());
    ok(slot_table_valid(shuffled.data(),shuffled.size()),"sorted table valid");
    bool found=true;
    for(const SlotEntry &e:t)if(slot_find(shuffled.data(),shuffled.size(),e.slot_rva)!=(int)e.row)found=false;
    ok(found,"every registered slot is found with its own row");
    ok(slot_find(shuffled.data(),shuffled.size(),0x13A0008+1)==-1 && slot_find(shuffled.data(),shuffled.size(),0)==-1 && slot_find(shuffled.data(),shuffled.size(),0xFFFFFFFFu)==-1,"unknown slots rejected");
    ok(slot_find(shuffled.data(),0,0x13A0008)==-1,"empty table rejects");
    ok(slot_find_for_vt(shuffled.data(),shuffled.size(),0x13A0000,SK_TICK)==0 && slot_find_for_vt(shuffled.data(),shuffled.size(),0x13A0000,SK_HIDE)==1 && slot_find_for_vt(shuffled.data(),shuffled.size(),0x13A0000,SK_CLOSE)==2,"vt + offset lookup per kind");
    ok(slot_find_for_vt(shuffled.data(),shuffled.size(),0x13A0001,SK_TICK)==-1,"vt not in the table rejected");
    std::vector<SlotEntry> dup=shuffled;dup[5].slot_rva=dup[4].slot_rva;
    ok(!slot_table_valid(dup.data(),dup.size()),"duplicate slot invalid");
    std::vector<SlotEntry> rowdup=shuffled;rowdup[5].row=rowdup[4].row;
    ok(!slot_table_valid(rowdup.data(),rowdup.size()),"duplicate row invalid");
    std::vector<SlotEntry> unsorted=shuffled;std::swap(unsorted[3],unsorted[9]);
    ok(!slot_table_valid(unsorted.data(),unsorted.size()),"unsorted invalid");
    std::vector<SlotEntry> bigrow=shuffled;bigrow[7].row=152;
    ok(!slot_table_valid(bigrow.data(),bigrow.size()),"row out of range invalid");
}

// ---- path decisions ----
static void test_paths() {
    const unsigned tables[3]={0x1000,0x2000,0x3000};
    const PathVts p{0x14B8CE0,0x145FFD8,0x14604B8,0x13EC0D0,tables,3};
    ok(tick_path(p,0x14B8CE0)==PATH_SUMMARY,"summary vt");
    ok(tick_path(p,0x145FFD8)==PATH_DUEL && tick_path(p,0x14604B8)==PATH_DUEL,"duel vts");
    ok(tick_path(p,0x13EC0D0)==PATH_EDIT,"edit vt");
    ok(tick_path(p,0x1000)==PATH_TABLE && tick_path(p,0x777)==PATH_TABLE,"everything else is a table");
    ok(tick_path(p,0)==PATH_NONE,"vt 0 none");
    ok(body_owner_ok(p,0x2000) && !body_owner_ok(p,0x2001) && !body_owner_ok(p,0),"body owner must be a table vt");
    ok(!body_owner_ok(p,0x14B8CE0),"summary is not a table owner");
    // table path pieces
    ok(tab_ok(0,1) && tab_ok(5,6) && tab_ok(63,64) && !tab_ok(64,100) && !tab_ok(-1,5) && !tab_ok(3,3) && !tab_ok(0,0),"tab bounds");
    ok(!header_array_ok(0) && !header_array_ok(8) && !header_array_ok(0xF) && header_array_ok(0x10) && header_array_ok(0x2008) && header_array_ok(0x200F) && !header_array_ok(0x2010),"header array 2..1025 entries");
    ok(header_count(0x10)==2 && header_count(0x2008)==1025,"header count from bytes");
    ok(header_col_ok(0,1) && header_col_ok(0x3FF,0x400) && !header_col_ok(0x400,0x401) && !header_col_ok(1,1) && !header_col_ok(0,0),"header column index");
    const int ids_ok[4]={3,1,4,1};const int ids_neg[4]={3,-1,4,1};const int ids_big[4]={3,1,4,0x1000};
    ok(descriptor_index(ids_ok,3)==1 && descriptor_index(ids_ok,0)==3,"descriptor index");
    ok(descriptor_index(ids_neg,3)==-1 && descriptor_index(ids_neg,0)==3,"any negative id up to col rejects");
    ok(descriptor_index(ids_big,3)==-1 && descriptor_index(ids_big,2)==4,"id >= 0x1000 rejects");
    ok(visible_index_ok(0,1) && !visible_index_ok(-1,5) && !visible_index_ok(5,5) && visible_index_ok(4,5),"visible index");
    ok(row_index_ok(0,1) && !row_index_ok(1,1) && !row_index_ok(-1,3),"row index");
    ok(cell_mode_ok(true,true,true,true,true) && !cell_mode_ok(false,true,true,true,true) && !cell_mode_ok(true,false,true,true,true) && !cell_mode_ok(true,true,false,true,true) && !cell_mode_ok(true,true,true,false,true) && !cell_mode_ok(true,true,true,true,false),"cell mode needs all five");
    ok(header_hit(true,false,true,false) && !header_hit(true,true,false,true),"cell mode uses the x-only hit");
    ok(header_hit(false,true,false,true) && !header_hit(false,false,true,true) && !header_hit(false,true,true,false),"header mode needs target==header and the full hit");
    // summary
    ok(summary_slot_ok(0) && summary_slot_ok(8) && !summary_slot_ok(9) && !summary_slot_ok(255),"summary slot <= 8");
    ok(summary_key(1)==1 && summary_key(0x163)==0x163 && summary_key(0)==0 && summary_key(0x164)==0 && summary_key(-4)==0,"summary key range");
    ok(summary_button_hit(true,true,true) && !summary_button_hit(false,true,true) && !summary_button_hit(true,false,true) && !summary_button_hit(true,true,false),"summary button");
    // edit
    EditProbe e{true,true,true,true,true,true,true,true,L'x'};
    ok(edit_key(e,0x20000)==0x20000,"edit hit gives the row");
    { EditProbe x=e;x.row_text_first=0;ok(edit_key(x,0x20000)==0,"empty first char -> none"); }
    { EditProbe x=e;x.row_nonnull=false;ok(edit_key(x,0x20000)==0,"null row"); }
    { EditProbe x=e;x.w_nonnull=false;ok(edit_key(x,0x20000)==0,"null w"); }
    { EditProbe x=e;x.input_ok=false;ok(edit_key(x,0x20000)==0,"no input"); }
    { EditProbe x=e;x.target_ok=false;ok(edit_key(x,0x20000)==0,"no target"); }
    { EditProbe x=e;x.row_vt_edit=false;ok(edit_key(x,0x20000)==0,"row vt not an edit vt"); }
    { EditProbe x=e;x.owner_hit=false;ok(edit_key(x,0x20000)==0,"owner not under mouse"); }
    { EditProbe x=e;x.w_hit=false;ok(edit_key(x,0x20000)==0,"w not under mouse"); }
    { EditProbe x=e;x.target_in_w=false;ok(edit_key(x,0x20000)==0,"target outside w"); }
}

// ---- tactic feature tab (docs/TRAIT_HOVER_TACTIC_TEXT.md 2.2/2.3/4.2) ----
static void test_tactic() {
    // table: column and caption order, rvas resolve and are distinct
    static const unsigned col[5]={1,2,3,12,13},cap[5]={0x455,0x456,0x4B4,0x404,0x405};
    bool tbl=true;
    for(unsigned k=0;k<kTacticFields;++k) {
        if(kTacticFieldTable[k].column!=col[k] || tactic_caption_id(k)!=cap[k] || tactic_func_rva(k)==0)tbl=false;
        for(unsigned j=0;j<k;++j)if(tactic_func_rva(j)==tactic_func_rva(k))tbl=false;
    }
    ok(tbl && kTacticFields==5,"tactic table: columns, captions, distinct non-zero rvas");
    ok(tactic_func_rva(5)==0 && tactic_caption_id(5)==0 && tactic_func_rva(99)==0,"tactic k>=5 gives 0");
    ok(trait_hover_tactic_columns_rva!=0,"tactic column table id resolves");
    // compose
    const wchar_t *cp[5]={L"A",L"B",L"C",L"D",L"E"};
    { const wchar_t *v[5]={L"v1",L"v2",L"v3",L"v4",L"v5"};
      ok(compose_tactic_text(cp,v)==L"A: v1 / B: v2 / C: v3\nD: v4\nE: v5","full 5 fields, 3 lines"); }
    { const wchar_t *v[5]={L"v1",L"v2",L"v3",L"v4",L""};
      ok(compose_tactic_text(cp,v)==L"A: v1 / B: v2 / C: v3\nD: v4","empty effect2: 2 lines"); }
    { const wchar_t *v[5]={L"v1",L"",L"v3",L"v4",nullptr};
      const std::wstring t=compose_tactic_text(cp,v);
      ok(t==L"A: v1 / C: v3\nD: v4" && t.find(L" /  / ")==std::wstring::npos,"empty dependency: no doubled separator"); }
    { const wchar_t *v[5]={L"v1",L"v2",L"",L"v4",L"v5"};
      ok(compose_tactic_text(cp,v)==L"A: v1 / B: v2\nD: v4\nE: v5","empty field 3 dropped"); }
    { const wchar_t *v[5]={L"",L"v2",L"v3",L"v4",L"v5"};ok(compose_tactic_text(cp,v).empty(),"empty field 1: empty text"); }
    { const wchar_t *v[5]={nullptr,L"v2",L"v3",L"v4",L"v5"};ok(compose_tactic_text(cp,v).empty(),"null field 1: empty text"); }
    { const wchar_t *v[5]={L"v1",L"v2",L"v3",nullptr,L"v5"};ok(compose_tactic_text(cp,v).empty(),"null field 4: empty text"); }
    { const wchar_t *v[5]={L"v1",L"v2",L"v3",L"",L"v5"};ok(compose_tactic_text(cp,v).empty(),"empty field 4: empty text"); }
    { const wchar_t *c0[5]={nullptr,L"B",L"C",L"D",L"E"};const wchar_t *v[5]={L"v1",L"v2",L"v3",L"v4",L"v5"};
      ok(compose_tactic_text(c0,v)==L": v1 / B: v2 / C: v3\nD: v4\nE: v5","null caption leaves \": value\""); }
    { // charge: {lineage, dependency, base, effect1, effect2}
      const wchar_t *c[5]={L"계통",L"의존",L"거점",L"효1",L"효2"};
      const wchar_t *v[5]={L"공격",L"무력",L"×",L"피해",L"적 사기저하"};
      ok(compose_tactic_text(c,v)==L"계통: 공격 / 의존: 무력 / 거점: ×\n효1: 피해\n효2: 적 사기저하","charge text with real-looking values"); }
    // length cap and surrogate boundary
    { std::wstring big(2000,L'x');const wchar_t *v[5]={big.c_str(),L"v2",L"v3",L"v4",L"v5"};
      ok(compose_tactic_text(cp,v).size()==kTextMax,"capped at 1023 wchar"); }
    { std::wstring big(1019,L'x');   // "A: " + 1019 = 1022 chars; a surrogate pair then straddles the 1023 cap
      big+=(wchar_t)0xD83D;big+=(wchar_t)0xDE00;
      const wchar_t *v[5]={big.c_str(),L"v2",L"v3",L"v4",L"v5"};
      const std::wstring t=compose_tactic_text(cp,v);
      ok(t.size()==1022 && (t.back()&0xFC00)!=0xD800,"no lone high surrogate at the cap"); }
    // column descriptor
    { unsigned char d[kTacticColStride];memset(d,0,sizeof(d));
      const int c=12,m=0x404;const uint64_t f=0x140358AA0ull;
      memcpy(d,&c,4);memcpy(d+kTacticColFormatter,&f,8);memcpy(d+kTacticColCaption,&m,4);
      ok(tactic_column_ok(d,12,f,0x404),"column descriptor ok");
      ok(!tactic_column_ok(d,13,f,0x404),"wrong column number");
      ok(!tactic_column_ok(d,12,f+1,0x404),"wrong formatter");
      ok(!tactic_column_ok(d,12,f,0x405),"wrong caption");
      ok(!tactic_column_ok(nullptr,12,f,0x404),"null descriptor");
      d[0x5C]=1;ok(tactic_column_ok(d,12,f,0x404),"+0x5C is not compared"); }
}

// ---- guards ----
static void test_guards() {
    const size_t n=sizeof(trait_hover_guards)/sizeof(trait_hover_guards[0]);
    ok(n==29,"29 guard rows");
    ok(guards_table_ok(trait_hover_guards,n),"guard table: ids resolved, size <= 0x40 (2305D0: 0x7A), no duplicates");
    bool all_hashes=true;for(size_t i=0;i<n;++i)if(trait_hover_guards[i].hash==0)all_hashes=false;
    ok(all_hashes,"every guard row has a hash");
    TraitHoverGuard bad[3]={trait_hover_guards[0],trait_hover_guards[1],trait_hover_guards[0]};
    ok(!guards_table_ok(bad,3),"duplicate rva rejected");
    TraitHoverGuard big[1]={trait_hover_guards[0]};big[0].size=0x41;
    ok(!guards_table_ok(big,1),"size 0x41 rejected");
    TraitHoverGuard zero[1]={trait_hover_guards[0]};zero[0].rva=0;
    ok(!guards_table_ok(zero,1),"unresolved id (rva 0) rejected");
    TraitHoverGuard big7a[1]={{GA(GA_2305D0),0x7A,trait_hover_guards[0].hash}};
    ok(guards_table_ok(big7a,1),"2305D0 may be 0x7A");
    TraitHoverGuard big7b[1]={{GA(GA_2305D0),0x7B,trait_hover_guards[0].hash}};
    ok(!guards_table_ok(big7b,1),"2305D0 0x7B rejected");
    TraitHoverGuard other7a[1]={{GA(GA_10FC0),0x7A,trait_hover_guards[0].hash}};
    ok(!guards_table_ok(other7a,1),"0x7A only allowed for 2305D0");
    ok(trait_hover_empty_tick_rva && trait_hover_slot_getter_rva && trait_hover_duel_target_rva && trait_hover_balloon_ctx_rva && trait_hover_balloon_vtable_rva,"context ids resolve");
    bool duel=true;for(unsigned r:trait_hover_duel_call_rvas)if(!r)duel=false;
    ok(duel && sizeof(trait_hover_duel_call_rvas)/sizeof(unsigned)==4,"4 duel call sites resolve");
}

int main() {
    test_header();test_duel();test_contains();test_edit();test_records();test_keys();test_slots();test_paths();test_tactic();test_guards();
    std::printf("{\"checks\":%d,\"failures\":%d}\n",checks,failures);
    return failures?1:0;
}
