// [officer_ui] drag_learn (2026-10-23, user request; port of reference_mod SAN14_DragLearn's verified
// logic and patch points, every address re-checked against this EXE). The tactic-learning window lets
// several books and several officers be chosen; [결정] learns every learnable pair, one book + one order
// per learning, as the original command does for one pair.
// Original (unpacked 1fbe6be7...), learning command object = cmd:
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
namespace drag_learn {
static const unsigned kBookOpen=0x7482E4,kBookClosed=0x748359,kOfficerOpen=0x74A338,kOfficerClosed=0x74A37A,
    kGrey=0x748175,kLearnCall=0x740125,kLearn=0x1F0520;
static const unsigned kMaxSelect=10;
static Obj books[16],officers[16],book_officer;
static unsigned book_count,officer_count;
static Obj tactic_of(Obj book) {unsigned i=book[0x52];return pointer(world(),0x774D0+(i<=200?i:0)*8);}
static unsigned slot_tactic(Obj person,unsigned i) {return fn<unsigned(__fastcall *)(Obj,unsigned)>(0x22A320)(person,i);}
static unsigned slots_used(Obj person) {
    unsigned used=0;
    for(unsigned i=0;i<10;++i) {unsigned t=slot_tactic(person,i);if(valid(pointer(world(),0x774D0+(t<=200?t:0)*8)))++used;}
    return used;
}
static bool can_learn(Obj person,Obj book) {
    if(!valid(person) || !book)return false;
    Obj tactic=tactic_of(book);if(!valid(tactic))return false;
    unsigned id=fn<unsigned(__fastcall *)(Obj)>(0x22E0A0)(tactic);
    for(unsigned i=0;i<10;++i)if(slot_tactic(person,i)==id)return false;
    for(unsigned at=0x77428;at<0x774D0;at+=8) {
        Obj form=pointer(world(),at);if(!valid(form))continue;
        unsigned f=fn<unsigned(__fastcall *)(Obj)>(0x22DD10)(form);
        if(f>20 || !(dword(tactic,0x6C)>>f&1))continue;
        if(form[0x7E]==2?fn<unsigned(__fastcall *)(Obj)>(0x42A80)(person)==f:fn<int(__fastcall *)(Obj,unsigned)>(0x232350)(person,f)!=0)return true;
    }
    return false;
}
static unsigned orders_for(Obj source) {
    Obj o=source?fn<Obj(__fastcall *)(Obj)>(0x22E7E0)(source):nullptr;
    unsigned cost=dword(image,0x1A48584);
    return o?(cost?o[0x14]/cost:o[0x14]):0;
}
static unsigned collect(unsigned *handle,Obj *out,unsigned max) {
    if(!handle || !pointer(image,0x27D10E8) || *handle>=dword(image,0x27D1120))return 0;
    Obj heads=pointer(image,0x27D10F0);if(!heads)return 0;
    unsigned n=0;
    for(Link *l=*reinterpret_cast<Link **>(heads+*handle*8);l && n<max;l=l->next)out[n++]=l->value;
    return n;
}
static bool multi_officers(Obj cmd) {return officer_count>1 && pointer(cmd,0x488)==officers[0];}
static void __fastcall book_open(int *limits,Obj cmd) {
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
static void __fastcall book_closed(unsigned *handle,Obj cmd) {
    book_count=collect(handle,books,16);book_officer=pointer(cmd,0x488);
}
static void __fastcall officer_open(int *limits,Obj cmd) {
    if(!mod_features.drag_learn)return;
    unsigned limit=orders_for(pointer(cmd,0x478));if(limit>kMaxSelect)limit=kMaxSelect;
    if(limit>1) {limits[0]=2;limits[1]=static_cast<int>(limit);limits[2]=0;}
}
static void __fastcall officer_closed(unsigned *handle) {
    Obj got[16];unsigned n=collect(handle,got,16);
    if(n) {memcpy(officers,got,sizeof(got));officer_count=n;}
}
static int __fastcall book_enabled(Obj cmd,Obj book) {
    if(!mod_features.drag_learn || !multi_officers(cmd))return 0;
    for(unsigned i=0;i<officer_count;++i)if(officers[i] && can_learn(officers[i],book))return 1;
    return 0;
}
static unsigned long long __fastcall learn(Obj *args) {
    auto original=fn<unsigned long long(__fastcall *)(Obj *)>(kLearn);
    Obj officer=args[0],book=args[1],extra=args[2],source=args[3];
    if(!mod_features.drag_learn || !officer || !book)return original(args);
    unsigned nb=book_count>1 && book==books[0] && officer==book_officer?book_count:1;
    unsigned no=officer_count>1 && officer==officers[0]?officer_count:1;
    unsigned long long result=no>1 && !can_learn(officer,book)?1:original(args);
    if(!result || (nb==1 && no==1))return result;
    bool skip_first=valid(extra);
    unsigned learned=0;
    for(unsigned o=0;o<no;++o) {
        Obj who=o?officers[o]:officer;
        if(!who || (o && who==officer) || (!o && skip_first))continue;
        for(unsigned b=0;b<nb;++b) {
            if(!o && !b)continue;   // the original call above
            Obj what=b?books[b]:book;
            if(!what || (b && (what==book || pointer(what,0)!=pointer(book,0))))continue;
            if(!can_learn(who,what))continue;
            unsigned index=fn<unsigned(__fastcall *)(Obj)>(0x22DD70)(what);
            if(index<1 || index>30 || !word(source,0x120+index*2))continue;
            Obj force=fn<Obj(__fastcall *)(Obj)>(0x22E7E0)(source);
            if(!force || force[0x14]<dword(image,0x1A48584))goto done;
            if(slots_used(who)>=10)break;
            args[0]=who;args[1]=what;args[2]=nullptr;
            if(original(args))++learned;
            args[0]=officer;args[1]=book;args[2]=extra;
        }
    }
done:
    args[0]=officer;args[1]=book;args[2]=extra;
    log_event("drag_learn_batch",learned,nb,no);
    return result;
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
    return !memcmp(image+kBookOpen,open,sizeof(open)) && !memcmp(image+kBookClosed,closed,sizeof(closed)) &&
        !memcmp(image+kOfficerOpen,oopen,sizeof(oopen)) && !memcmp(image+kOfficerClosed,oclosed,sizeof(oclosed)) &&
        !memcmp(image+0x74816E,grey,2) && image[0x748174]==0 && !memcmp(image+kGrey,grey2,5) && !memcmp(image+0x74816C,grey0,2) &&
        image[kLearnCall]==0xE8 && kLearnCall+5+*reinterpret_cast<int *>(image+kLearnCall+1)==kLearn &&
        image[0x748156]==0x75 && image[0x748157]==0x73;
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
    memcpy(s1,kStubBookOpen,sizeof(kStubBookOpen));put(s1,25,reinterpret_cast<void *>(&book_open));put(s1,46,image+0x7482F0);
    memcpy(s2,kStubBookClosed,sizeof(kStubBookClosed));put(s2,14,reinterpret_cast<void *>(&book_closed));put(s2,37,image+0x748360);
    memcpy(s3,kStubOfficerOpen,sizeof(kStubOfficerOpen));put(s3,19,reinterpret_cast<void *>(&officer_open));put(s3,40,image+0x74A33D);
    memcpy(s4,kStubOfficerClosed,sizeof(kStubOfficerClosed));put(s4,11,reinterpret_cast<void *>(&officer_closed));put(s4,33,image+0x74A380);
    memcpy(s5,kStubGrey,sizeof(kStubGrey));put(s5,12,reinterpret_cast<void *>(&book_enabled));put(s5,32,image+0x7481CB);
    put(s5,44,image+0x748175+*reinterpret_cast<int *>(image+0x748170));          // cmp [rip+disp],0 at 74816E
    put(s5,59,image+0x748184);
    put(s5,71,image+0x74817E+*reinterpret_cast<int *>(image+0x74817A));          // lea rcx,[rip+disp] at 748177
    put(s5,81,image+0x74817E);
    absolute_jump(s6,reinterpret_cast<void *>(&learn));
    bool ok=jump_patch(links[0],kBookOpen,12,s1) && jump_patch(links[1],kBookClosed,7,s2) && jump_patch(links[2],kOfficerOpen,5,s3) &&
        jump_patch(links[3],kOfficerClosed,6,s4) && jump_patch(links[4],kGrey,9,s5) && prepare_link(links[5],kLearnCall,0xE8,s6);
    DWORD old;
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,0x240)) {
        VirtualFree(relay,0,MEM_RELEASE);return false;}
    for(unsigned i=0;i<6;++i)if(!publish_link(links[i])) {while(i)restore_link(links[--i]);return false;}
    log_event("drag_learn_installed");return true;
}
}
