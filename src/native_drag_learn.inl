// [officer_ui] drag_learn (2026-10-23, user request; port of reference_mod SAN14_DragLearn's verified
// logic and patch points, every address re-checked against this EXE). The tactic-learning window lets
// several books and several officers be chosen; [결정] learns every learnable pair, one book + one order
// per learning, as the original command does for one pair.
// Original (근거: 1a4a447d 이전 기록, docs/HISTORY.md), learning command object = cmd:
//   7482E4 book list opens: selection limits {mode 1, max 1, 0} at [rbp-48] -> 30CF80(0x5BC) dialog.
//   748359 book list closed: result list handle [rsp+38]; officer cmd+488, orders source cmd+478.
//   74A32F officer list opens: limits at [rsp+58] -> 30CF80(0x32E). 74A37A closed: result [rsp+28].
//   748102..74816C book grey-out: a book is enabled (7481CB) if cmd+488 has a formation that can use it.
//   740125 CALL 1F0520(args{officer,book,extra,source}) learns one pair.
//   Helpers: tactic table world+774D0[book+52] (<=200), 22E0A0 tactic id, 22A320(person,slot) slot
//   tactic, formations world+77428..774D0, 22DD10 formation id, tactic+6C formation mask,
//   formation+7E==2 -> 42A80(person) special formation else 232350(person,id); 22DD70 book index,
//   source+120 u16[] book stock; 22E7E0(source)+14 orders, cost [1A48584].
// A mode-2 limit (max > 1) turns the original list into a multi-select list; nothing else in the
// dialogs changes. Names in the window still show the first selection.
#include "game_addresses.h"
namespace drag_learn {
static const unsigned kBookOpen=GA(GA_74A574),kBookClosed=GA(GA_74A5E9),kOfficerOpen=GA(GA_74C5E8),kOfficerClosed=GA(GA_74C62A),
    kGrey=GA(GA_74A405),kLearnCall=GA(GA_7423B5),kLearn=GA(GA_1F18E0),
    // PW34: CStrategyLearnState slot 5 (frame) of vtable GA_1447540. The frame ends every call in 75D130(state),
    // which refills the preview pane, so a wrapper that runs the original first can overwrite the settled text.
    kFrameSlot=GA(GA_1447540),kFrame=GA(GA_737050);
static const unsigned kMaxSelect=10;
static Obj books[16],officers[16],book_officer;
static unsigned book_count,officer_count;
static Obj tactic_of(Obj book) {unsigned i=book[0x52];return pointer(world(),0x774D0+(i<=200?i:0)*8);}
static unsigned slot_tactic(Obj person,unsigned i) {return fn<unsigned(__fastcall *)(Obj,unsigned)>(GA(GA_22BD60))(person,i);}
static unsigned slots_used(Obj person) {
    unsigned used=0;
    for(unsigned i=0;i<10;++i) {unsigned t=slot_tactic(person,i);if(valid(pointer(world(),0x774D0+(t<=200?t:0)*8)))++used;}
    return used;
}
static bool can_learn(Obj person,Obj book) {
    if(!valid(person) || !book)return false;
    Obj tactic=tactic_of(book);if(!valid(tactic))return false;
    unsigned id=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22FB30))(tactic);
    for(unsigned i=0;i<10;++i)if(slot_tactic(person,i)==id)return false;
    for(unsigned at=0x77428;at<0x774D0;at+=8) {
        Obj form=pointer(world(),at);if(!valid(form))continue;
        unsigned f=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F7A0))(form);
        if(f>20 || !(dword(tactic,0x6C)>>f&1))continue;
        if(form[0x7E]==2?fn<unsigned(__fastcall *)(Obj)>(GA(GA_43660))(person)==f:fn<int(__fastcall *)(Obj,unsigned)>(GA(GA_233CF0))(person,f)!=0)return true;
    }
    return false;
}
static unsigned orders_for(Obj source) {
    Obj o=source?fn<Obj(__fastcall *)(Obj)>(GA(GA_230270))(source):nullptr;
    unsigned cost=dword(image,GA(GA_1A4B5CC));
    return o?(cost?o[0x14]/cost:o[0x14]):0;
}
static unsigned collect(unsigned *handle,Obj *out,unsigned max) {
    if(!handle || !pointer(image,GA(GA_27D4138)) || *handle>=dword(image,GA(GA_27D4170)))return 0;
    Obj heads=pointer(image,GA(GA_27D4140));if(!heads)return 0;
    unsigned n=0;
    for(Link *l=*reinterpret_cast<Link **>(heads+*handle*8);l && n<max;l=l->next)out[n++]=l->value;
    return n;
}
static bool multi_officers(Obj cmd) {return officer_count>1 && pointer(cmd,0x488)==officers[0];}
static void __fastcall book_open(int *limits,Obj cmd) { COST_SCOPE("drag_learn::book_open");
    if(!mod_features.drag_learn || valid(pointer(cmd,0x490)))return;
    unsigned limit=kMaxSelect,orders=orders_for(pointer(cmd,0x478));
    bool multi=multi_officers(cmd);
    if(multi)orders/=officer_count;
    if(orders<limit)limit=orders;
    unsigned used=slots_used(pointer(cmd,0x488));
    if(multi)for(unsigned i=1;i<officer_count;++i)if(officers[i]) {unsigned u=slots_used(officers[i]);if(u<used)used=u;}
    unsigned free=used<10?10-used:0;if(free<limit)limit=free;
    if(limit>1) {limits[0]=2;limits[1]=static_cast<int>(limit);limits[2]=0;}
}
static void __fastcall book_closed(unsigned *handle,Obj cmd) { COST_SCOPE("drag_learn::book_closed");
    book_count=collect(handle,books,16);book_officer=pointer(cmd,0x488);
}
static void __fastcall officer_open(int *limits,Obj cmd) { COST_SCOPE("drag_learn::officer_open");
    if(!mod_features.drag_learn)return;
    unsigned limit=orders_for(pointer(cmd,0x478));if(limit>kMaxSelect)limit=kMaxSelect;
    if(limit>1) {limits[0]=2;limits[1]=static_cast<int>(limit);limits[2]=0;}
}
static void __fastcall officer_closed(unsigned *handle) { COST_SCOPE("drag_learn::officer_closed");
    Obj got[16];unsigned n=collect(handle,got,16);
    if(n) {memcpy(officers,got,sizeof(got));officer_count=n;}
}
static int __fastcall book_enabled(Obj cmd,Obj book) { COST_SCOPE("drag_learn::book_enabled");
    if(!mod_features.drag_learn || !multi_officers(cmd))return 0;
    for(unsigned i=0;i<officer_count;++i)if(officers[i] && can_learn(officers[i],book))return 1;
    return 0;
}
static unsigned long long __fastcall learn(Obj *args) { COST_SCOPE("drag_learn::learn");
    auto original=fn<unsigned long long(__fastcall *)(Obj *)>(kLearn);
    Obj officer=args[0],book=args[1],extra=args[2],source=args[3];
    if(!mod_features.drag_learn || !officer || !book)return original(args);
    unsigned nb=book_count>1 && book==books[0] && officer==book_officer?book_count:1;
    unsigned no=officer_count>1 && officer==officers[0]?officer_count:1;
    unsigned long long result=no>1 && !can_learn(officer,book)?1:original(args);
    if(!result || (nb==1 && no==1))return result;
    bool skip_first=valid(extra);
    unsigned learned=0,fixed_stock=0,fixed_orders=0;
    unsigned cost=dword(image,GA(GA_1A4B5CC));   // one 명령서 per learning (user 2026-11-09)
    for(unsigned o=0;o<no;++o) {
        Obj who=o?officers[o]:officer;
        if(!who || (o && who==officer) || (!o && skip_first))continue;
        for(unsigned b=0;b<nb;++b) {
            if(!o && !b)continue;   // the original call above
            Obj what=b?books[b]:book;
            if(!what || (b && (what==book || pointer(what,0)!=pointer(book,0))))continue;
            if(!can_learn(who,what))continue;
            unsigned index=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F800))(what);
            if(index<1 || index>30)continue;
            unsigned stock0=word(source,0x120+index*2);
            if(!stock0)continue;      // one book per learning: none left
            Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_230270))(source);
            if(!force)goto done;
            unsigned orders0=force[0x14];
            if(orders0<cost)goto done;   // one 명령서 per learning: none left
            if(slots_used(who)>=10)break;
            args[0]=who;args[1]=what;args[2]=nullptr;
            bool ok=original(args)!=0;
            args[0]=officer;args[1]=book;args[2]=extra;
            if(ok) ++learned;
            if(ok && mod_features.drag_learn_multi) {   // local17: stock fix belongs to drag_learn_multi; off = v15.7.1
                // The native call is supposed to take the book/order itself; if it
                // did not, take them here so a batch cannot exceed its stock.
                unsigned stock1=word(source,0x120+index*2);
                if(stock1>=stock0) {
                    *reinterpret_cast<unsigned short *>(source+0x120+index*2)=static_cast<unsigned short>(stock0-1);
                    ++fixed_stock;
                }
                unsigned orders1=force[0x14];
                if(orders1+cost>orders0) {
                    force[0x14]=static_cast<unsigned char>(orders0>=cost?orders0-cost:0);
                    ++fixed_orders;
                }
            }
        }
    }
done:
    args[0]=officer;args[1]=book;args[2]=extra;
    log_event("drag_learn_batch",learned,nb|(no<<8),fixed_stock|(fixed_orders<<8));
    return result;
}
// PW34 (user 2026-11-09, local9 screenshot): the 습득 confirm pane still printed one officer and one book. The frame
// (CStrategyLearnState slot 5, kFrameSlot -> kFrame) ends every call in 75D130(state), which refills the pane through
// 7573A0 (officer name node 0x57), 75A820 (전법서 rows 0xAC/0xAE/0xAF = "%d", "%d", "(^03-%d^00)") and 6BFF10
// (명령서 rows 0xB5/0xB7/0xB8). learn_frame calls the original first and then rewrites those nodes with the same text
// call (9332B0(layout+0x130,node,text,1)), so the pane always matches what [결정] would learn (learn()).
// Only a multi selection of this state's officer/book is touched; single selection and drag_learn off keep the original.
static void *next_learn_frame;
static unsigned book_stock(Obj source,Obj book) {
    unsigned index=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F800))(book);
    return source && index>=1 && index<=30?static_cast<unsigned>(word(source,0x120+index*2)):0u;
}
// Same pairs and the same stop rules as learn(): the command's own pair first, then officers x books in the same
// order, one book and one 명령서 cost each, stopping when the orders or a book run out (slots_used>=10 breaks the
// book loop of that officer only).
static unsigned count_pairs(Obj source,Obj officer0,Obj book0,unsigned no,unsigned nb,
    unsigned &stock_before,unsigned &orders_now,unsigned &orders_left) {
    stock_before=0;orders_now=0;orders_left=0;
    if(!source || !officer0 || !book0)return 0;
    Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_230270))(source);
    if(!force)return 0;
    unsigned cost=dword(image,GA(GA_1A4B5CC));
    orders_now=force[0x14];orders_left=orders_now;
    Obj list[16]={};unsigned stock[16]={},used[16]={};
    for(unsigned b=0;b<nb && b<16;++b) {
        Obj what=b?books[b]:book0;
        if(!what || (b && (what==book0 || pointer(what,0)!=pointer(book0,0))))continue;
        list[b]=what;
        stock[b]=book_stock(source,what);
        stock_before+=stock[b];
    }
    for(unsigned o=0;o<no && o<16;++o)used[o]=officers[o]?slots_used(officers[o]):10;
    if(!can_learn(officer0,book0) || !stock[0] || orders_left<cost)return 0;
    unsigned pairs=1;--stock[0];orders_left-=cost;++used[0];
    for(unsigned o=0;o<no && o<16;++o) {
        Obj who=o?officers[o]:officer0;
        if(!who || (o && who==officer0))continue;
        for(unsigned b=0;b<nb && b<16;++b) {
            if(!o && !b)continue;
            Obj what=list[b];
            if(!what || !can_learn(who,what) || !stock[b])continue;
            if(orders_left<cost)return pairs;   // learn() stops the whole batch here
            if(used[o]>=10)break;               // learn() breaks this officer's book loop only
            ++pairs;--stock[b];orders_left-=cost;++used[o];
        }
    }
    return pairs;
}
// Computed once per key change, re-applied every frame (the original rewrites these nodes every frame).
static wchar_t pv_name[64],pv_stock0[32],pv_stock1[32],pv_stockd[32],pv_ord0[32],pv_ord1[32],pv_ordd[32];
static bool pv_valid,pv_name_on,pv_book_on;
static Obj pv_state,pv_officer,pv_book;
static unsigned pv_no,pv_nb,pv_pairs,pv_stock_key,pv_orders_key;
static void preview(Obj state) {
    if(!mod_features.drag_learn || !mod_features.drag_learn_multi || !state)return;
    // 7422F0 builds the learn args as {+0x488 officer, +0x480 book, +0x490 extra, +0x478 source} (Claude, ghidra).
    Obj layout=pointer(state,0x4B8),officer=pointer(state,0x488),book=pointer(state,0x480),source=pointer(state,0x478);
    if(!layout || !officer || !book || !source)return;
    if(!officer_count || officer_count>16 || !book_count || book_count>16)return;
    if(officer!=officers[0])return;                       // another officer than the dragged selection
    if(book_count>1 && book!=books[0])return;             // learn() only batches from the first book
    if(officer_count<2 && book_count<2)return;            // single selection: original text
    unsigned no=officer_count,nb=book_count>1?book_count:1,stock_key=0;
    for(unsigned b=0;b<nb;++b) {Obj what=b?books[b]:book;if(what)stock_key+=book_stock(source,what);}
    Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_230270))(source);
    unsigned orders_key=force?static_cast<unsigned>(force[0x14]):0u;
    if(!pv_valid || pv_state!=state || pv_officer!=officer || pv_book!=book || pv_no!=no || pv_nb!=nb ||
       pv_stock_key!=stock_key || pv_orders_key!=orders_key) {
        unsigned before=0,orders_now=0,orders_left=0;
        pv_valid=true;pv_state=state;pv_officer=officer;pv_book=book;pv_no=no;pv_nb=nb;
        pv_stock_key=stock_key;pv_orders_key=orders_key;
        pv_pairs=count_pairs(source,officer,book,no,nb,before,orders_now,orders_left);
        pv_name_on=no>1;
        pv_book_on=pv_pairs>1 || nb>1;
        if(pv_name_on) {
            const wchar_t *name=fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(officer);
            _snwprintf_s(pv_name,64,_TRUNCATE,L"%s 외 %u명",name?name:L"",no-1);
        }
        _snwprintf_s(pv_stock0,32,_TRUNCATE,L"%d",static_cast<int>(before));
        _snwprintf_s(pv_stock1,32,_TRUNCATE,L"%d",static_cast<int>(before>=pv_pairs?before-pv_pairs:0));
        _snwprintf_s(pv_stockd,32,_TRUNCATE,L"(^03-%d^00)",static_cast<int>(pv_pairs));
        _snwprintf_s(pv_ord0,32,_TRUNCATE,L"%d",static_cast<int>(orders_now));
        _snwprintf_s(pv_ord1,32,_TRUNCATE,L"%d",static_cast<int>(orders_left));
        _snwprintf_s(pv_ordd,32,_TRUNCATE,L"(^03-%d^00)",static_cast<int>(pv_pairs*dword(image,GA(GA_1A4B5CC))));
#ifndef NO_RUNTIME_LOG
        log_event("drag_learn_preview",no|(nb<<8),pv_pairs,orders_now);
#endif
    }
    auto text=fn<void(__fastcall *)(Obj,unsigned,const wchar_t *,int)>(GA(GA_9332B0));
    if(pv_name_on) {
        text(layout+0x130,0x57,pv_name,1);           // original node (kept)
        // PW36a (R8b §3): the name actually drawn in this pane comes from the CUtilPInfoLayout sub-widget, so
        // write the "외 N명" text into its own manager as well (same rule as the recruit window).
        Obj sub=pointer(layout,0x1B0);
        if(sub) {
            auto control=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_92F390));
            unsigned node=control(sub+0x130,2)?2:7;
            text(sub+0x130,node,pv_name,1);
        }
    }
    if(pv_book_on) {
        text(layout+0x130,0xAC,pv_stock0,1);
        text(layout+0x130,0xAE,pv_stock1,1);
        text(layout+0x130,0xAF,pv_stockd,1);
    }
    text(layout+0x130,0xB5,pv_ord0,1);
    text(layout+0x130,0xB7,pv_ord1,1);
    text(layout+0x130,0xB8,pv_ordd,1);
}
static void __fastcall learn_frame(Obj state) { COST_SCOPE("drag_learn::learn_frame");
    if(!next_learn_frame)next_learn_frame=image+kFrame;
    reinterpret_cast<void(__fastcall *)(Obj)>(next_learn_frame)(state);
    preview(state);
}
// Stubs (keystone-assembled; markers A..F patched below). Each replays the displaced instructions.
static const unsigned char kStubBookOpen[]={0xBF,0x01,0x00,0x00,0x00,0x89,0x7D,0xB8,0x48,0x89,0x7D,0xBC,0x48,0x8D,0x4D,0xB8,
    0x4C,0x89,0xEA,0x48,0x83,0xEC,0x20,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xD0,0x48,0x83,0xC4,0x20,0xBF,0x01,0x00,0x00,0x00,
    0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE0};                                                    // A 25, B 46
static const unsigned char kStubBookClosed[]={0x48,0x8B,0x4C,0x24,0x38,0x4C,0x89,0xEA,0x48,0x83,0xEC,0x20,0x48,0xB8,0,0,0,0,
    0,0,0,0,0xFF,0xD0,0x48,0x83,0xC4,0x20,0x4D,0x8B,0x85,0xB8,0x04,0x00,0x00,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE0}; // A 14, B 37
static const unsigned char kStubOfficerOpen[]={0x48,0x89,0x74,0x24,0x5C,0x48,0x8D,0x4C,0x24,0x58,0x48,0x89,0xDA,0x48,0x83,0xEC,
    0x20,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xD0,0x48,0x83,0xC4,0x20,0xBE,0x01,0x00,0x00,0x00,0x48,0xB8,0,0,0,0,0,0,0,0,
    0xFF,0xE0};                                                                                // A 19, B 40
static const unsigned char kStubOfficerClosed[]={0x48,0x8B,0x4C,0x24,0x28,0x48,0x83,0xEC,0x20,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,
    0xD0,0x48,0x83,0xC4,0x20,0x41,0xBE,0x00,0x40,0x01,0x00,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE0};             // A 11, B 33
static const unsigned char kStubGrey[]={0x4C,0x89,0xE9,0x4C,0x89,0xE2,0x48,0x83,0xEC,0x20,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xD0,
    0x48,0x83,0xC4,0x20,0x85,0xC0,0x74,0x0C,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE0,0x48,0xB8,0,0,0,0,0,0,0,0,0x83,0x38,0x00,
    0x75,0x0C,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE0,0x48,0xB9,0,0,0,0,0,0,0,0,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xE0};
                                                                     // A 12, B 32, C 44, D 59, E 71, F 81
static bool guards_match() {
    static const unsigned char open[]={0xBF,0x01,0,0,0,0x89,0x7D,0xB8,0x48,0x89,0x7D,0xBC,0xB9,0xBC,0x05,0,0};
    static const unsigned char closed[]={0x4D,0x8B,0x85,0xB8,0x04,0,0,0x4D,0x8B,0x88,0xA0,0x01,0,0};
    static const unsigned char oopen[]={0x48,0x89,0x74,0x24,0x5C,0xB9,0x2E,0x03,0,0};
    static const unsigned char oclosed[]={0x41,0xBE,0x00,0x40,0x01,0x00,0x48,0x8B,0x54,0x24,0x28};
    static const unsigned char grey[]={0x83,0x3D},grey2[]={0x74,0x0D,0x48,0x8D,0x0D},grey0[]={0x75,0x94};
    return !ga::mcmp_at(image,kBookOpen,open,sizeof(open)) && !ga::mcmp_at(image,kBookClosed,closed,sizeof(closed)) &&
        !ga::mcmp_at(image,kOfficerOpen,oopen,sizeof(oopen)) && !ga::mcmp_at(image,kOfficerClosed,oclosed,sizeof(oclosed)) &&
        !ga::mcmp(image,GA_74A3FE,0,grey,2) && image[GA(GA_74A404)]==0 && !ga::mcmp_at(image,kGrey,grey2,5) && !ga::mcmp(image,GA_74A3FC,0,grey0,2) &&
        image[kLearnCall]==0xE8 && kLearnCall+5+*reinterpret_cast<int *>(image+kLearnCall+1)==kLearn &&
        image[GA(GA_74A3E6)]==0x75 && image[GA(GA_74A3E7)]==0x73;
}
static void put(Obj at,unsigned offset,const void *value) {memcpy(at+offset,&value,8);}
static bool jump_patch(CodeLink &link,unsigned rva,unsigned length,Obj target) {
    unsigned char bytes[16];bytes[0]=0xE9;
    intptr_t delta=reinterpret_cast<intptr_t>(target)-reinterpret_cast<intptr_t>(image+rva+5);
    if(delta<INT32_MIN || delta>INT32_MAX || length<5 || length>16)return false;
    int d=static_cast<int>(delta);memcpy(bytes+1,&d,4);memset(bytes+5,0x90,length-5);
    return prepare_bytes(link,rva,bytes,length);
}
static CodeLink links[6];
static bool install() {
    if(!guards_match()) {log_event("drag_learn_guard_rejected");return false;}
    Obj relay=allocate_near(image+kBookOpen);if(!relay)return false;
    Obj s1=relay,s2=relay+0x60,s3=relay+0xC0,s4=relay+0x120,s5=relay+0x180,s6=relay+0x200;
    memcpy(s1,kStubBookOpen,sizeof(kStubBookOpen));put(s1,25,reinterpret_cast<void *>(&book_open));put(s1,46,image+GA(GA_74A580));
    memcpy(s2,kStubBookClosed,sizeof(kStubBookClosed));put(s2,14,reinterpret_cast<void *>(&book_closed));put(s2,37,image+GA(GA_74A5F0));
    memcpy(s3,kStubOfficerOpen,sizeof(kStubOfficerOpen));put(s3,19,reinterpret_cast<void *>(&officer_open));put(s3,40,image+GA(GA_74C5ED));
    memcpy(s4,kStubOfficerClosed,sizeof(kStubOfficerClosed));put(s4,11,reinterpret_cast<void *>(&officer_closed));put(s4,33,image+GA(GA_74C630));
    memcpy(s5,kStubGrey,sizeof(kStubGrey));put(s5,12,reinterpret_cast<void *>(&book_enabled));put(s5,32,image+GA(GA_74A45B));
    put(s5,44,image+GA(GA_74A405)+*reinterpret_cast<int *>(image+GA(GA_74A400)));          // cmp [rip+disp],0 at 74816E
    put(s5,59,image+GA(GA_74A414));
    put(s5,71,image+GA(GA_74A40E)+*reinterpret_cast<int *>(image+GA(GA_74A40A)));          // lea rcx,[rip+disp] at 748177
    put(s5,81,image+GA(GA_74A40E));
    absolute_jump(s6,reinterpret_cast<void *>(&learn));
    bool ok=jump_patch(links[0],kBookOpen,12,s1) && jump_patch(links[1],kBookClosed,7,s2) && jump_patch(links[2],kOfficerOpen,5,s3) &&
        jump_patch(links[3],kOfficerClosed,6,s4) && jump_patch(links[4],kGrey,9,s5) && prepare_link(links[5],kLearnCall,0xE8,s6);
    DWORD old;
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,0x240)) {
        VirtualFree(relay,0,MEM_RELEASE);return false;}
    for(unsigned i=0;i<6;++i)if(!publish_link(links[i])) {while(i)restore_link(links[--i]);return false;}
    // PW34: the pane preview rides on the Layout manager (layout_registry row drag_learn LR_VT GA_1447540). A refusal
    // only costs the preview text; the batch learning above stays installed.
    if(mod_features.drag_learn_multi && layout_client::vt(kFrameSlot,reinterpret_cast<void *>(&learn_frame),reinterpret_cast<void **>(&next_learn_frame))<=0)
        log_event("drag_learn_frame_rejected",kFrameSlot,0,0);
    log_event("drag_learn_installed");return true;
}
}
