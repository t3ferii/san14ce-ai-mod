static unsigned __fastcall mock_friend_list(Obj person,GameList *out,int templates) {
    for(unsigned i=0;i<8;++i) {
        unsigned id=word(person,0xC0+i*2);
        Obj other=templates && id>=5101 && id<5201?table(0x84F30,id-5101,99):table(0x158,id,6000);
        if(id && valid(other))list_add(*out,other);
    }
    unsigned count=0;for(Link *link=list_first(*out);link;link=link->next)++count;return count;
}
static std::vector<uint8_t> archive_fixture={1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31};
static const char *archive_name="fixture_friends500.s14";
static unsigned char archive_written=1;
static int archive_read_result=16;
static bool archive_load_success=true;
static Obj archive_remote;
static unsigned char __fastcall mock_archive_write(Obj remote,const char *name,const void *data,int count) {
    expect(remote==archive_remote && !strcmp(name,archive_name));
    expect(count==static_cast<int>(archive_fixture.size()) && !memcmp(data,archive_fixture.data(),count));
    return archive_written;
}
static int __fastcall mock_archive_read(Obj remote,const char *name,void *data,int count) {
    expect(remote==archive_remote && !strcmp(name,archive_name));
    expect(count==static_cast<int>(archive_fixture.size()));
    memcpy(data,archive_fixture.data(),count);return archive_read_result;
}
static void __fastcall mock_save_host(Obj host) {
    if(dword(host,0x470)==1) {
        friendship_file_write(archive_remote,archive_name,archive_fixture.data(),static_cast<int>(archive_fixture.size()));
        put32(host,0x470,2);
    } else if(dword(host,0x470)==2) {put32(host,0x470,3);put32(image,0x27D2A20,archive_written);}
}
static void __fastcall mock_load_host(Obj host) {
    if(dword(host,0x470)==1) {
        std::vector<uint8_t> buffer(archive_fixture.size());
        friendship_file_read(archive_remote,archive_name,buffer.data(),static_cast<int>(buffer.size()));
        put32(host,0x470,2);
    } else if(dword(host,0x470)==2) {put32(host,0x470,3);put32(image,0x27D2A50,archive_load_success?1:0);}
}
static void fixture_job_name(unsigned offset) {
    Obj at=image+offset;putptr(at,0,const_cast<char *>(archive_name));
    *reinterpret_cast<size_t *>(at+0x10)=strlen(archive_name);
    *reinterpret_cast<size_t *>(at+0x18)=strlen(archive_name);
}
static void test_friendship_persistence() {
    original_friend_add=&mock_friend_add;original_friend_has=&mock_friend_has;
    original_friend_remove=&mock_friend_remove;original_friend_list=&mock_friend_list;
    Obj person=object(4000);slot(0x158,4000,person);
    std::vector<Obj> friends;
    for(unsigned id=4001;id<=4011;++id) {Obj other=object(id);slot(0x158,id,other);friends.push_back(other);}
    expanded_friends.clear();friendship_world=root;
    for(unsigned i=0;i<9;++i)friendship_add(person,friends[i]);
    GameList list;list_init(list);
    expect(friendship_list(person,&list,0)==9);
    expect(contains(list,friends[8]));list_free(list);
    // Configuration-template mode maps the placeholder to its separate
    // native table and must not accidentally append the runtime Person.
    Obj templ=object(5101),runtime=object(5101);slot(0x158,5101,runtime);slot(0x84F30,0,templ);
    expect(expanded_friends.add(4000,5101,friendship_native(person))==friendship::AddResult::Added);
    list_init(list);expect(friendship_list(person,&list,1)==10 && contains(list,templ) && !contains(list,runtime));list_free(list);
    expanded_friends.remove(4000,5101);slot(0x158,5101,nullptr);slot(0x84F30,0,nullptr);free(templ);free(runtime);
    self_module=GetModuleHandleW(nullptr);
    Obj save=static_cast<Obj>(calloc(1,0x500)),load=static_cast<Obj>(calloc(1,0x500));
    void *remote_vtable[]={reinterpret_cast<void *>(&mock_archive_write),reinterpret_cast<void *>(&mock_archive_read)};
    archive_remote=object(1);putptr(archive_remote,0,remote_vtable);
    native_functions[0x50B670]=reinterpret_cast<void *>(&mock_save_host);
    native_functions[0x509580]=reinterpret_cast<void *>(&mock_load_host);
    fixture_job_name(0x27D2A98);fixture_job_name(0x27D2A68);
    friendship::Binding hash={};expect(friendship_digest(archive_fixture.data(),16,hash));
    expect(hash!=friendship::Binding{});
    expect(!friendship_digest(nullptr,16,hash) && !friendship_digest(archive_fixture.data(),0,hash));
    friendship::SaveCoordinator::Completed job;job.name=archive_name;job.binding=hash;
    wchar_t path[MAX_PATH];expect(friendship_sidecar_path(job,path,MAX_PATH,true));
    DeleteFileW(path);
    put32(save,0x470,1);friendship_save_host(save);
    expect(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES);
    // This later relation must not leak into the pending save snapshot.
    friendship_add(person,friends[9]);friendship_save_host(save);
    expect(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES);
    friendship_add(person,friends[10]);expect(friendship_has(person,friends[10]));
    put32(load,0x470,1);friendship_load_host(load);friendship_load_host(load);
    expect(friendship_has(person,friends[8]) && !friendship_has(person,friends[9]) && !friendship_has(person,friends[10]));
    // Native load failure leaves the previous world's extras alone.
    friendship_add(person,friends[10]);archive_load_success=false;
    put32(load,0x470,1);friendship_load_host(load);friendship_load_host(load);
    expect(friendship_has(person,friends[10]));archive_load_success=true;
    // A short native FileRead is never accepted as a complete save hash.
    archive_read_result=8;put32(load,0x470,1);friendship_load_host(load);friendship_load_host(load);
    expect(!friendship_has(person,friends[8]) && friendship_has(person,friends[0]));archive_read_result=16;
    // No companion means no old extras, even when world()/Person addresses
    // are identical. Corrupt companion likewise never retains old extras.
    friendship_add(person,friends[8]);DeleteFileW(path);
    put32(load,0x470,1);friendship_load_host(load);friendship_load_host(load);
    expect(!friendship_has(person,friends[8]));
    HANDLE corrupt=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    expect(corrupt!=INVALID_HANDLE_VALUE);DWORD written=0;char invalid[52]={};
    expect(WriteFile(corrupt,invalid,sizeof(invalid),&written,nullptr)!=0);CloseHandle(corrupt);
    friendship_add(person,friends[8]);put32(load,0x470,1);friendship_load_host(load);friendship_load_host(load);
    expect(!friendship_has(person,friends[8]));DeleteFileW(path);
    // Failed native saves do not replace a companion or fabricate success.
    archive_written=0;put32(save,0x470,1);friendship_save_host(save);friendship_save_host(save);
    expect(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES);archive_written=1;
    enabled=0;friendship_add(person,friends[8]);expect(!mock_friend_has(person,friends[8]));enabled=1;
    friendship_save_host_object=nullptr;friendship_load_host_object=nullptr;expanded_friends.clear();
    for(Obj other:friends){slot(0x158,word(other,0x10),nullptr);free(other);}
    slot(0x158,4000,nullptr);free(person);free(save);free(load);free(archive_remote);archive_remote=nullptr;
}
static void test_friendship_archive_bridges() {
    Obj page=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    expect(page!=nullptr);
    void *vt[]={reinterpret_cast<void *>(&mock_archive_write),reinterpret_cast<void *>(&mock_archive_read)};
    archive_remote=object(1);putptr(archive_remote,0,vt);Obj archive=object(1);
    // A private machine-code caller supplies the game's live RBX archive
    // register, Windows shadow space and all four remote-storage arguments.
    // Record both the return value and the flags produced by the bridge.
    for(bool save:{true,false}) {
        unsigned size=make_friendship_archive_bridge(page+128,save);expect(size<32);
        const unsigned char caller[]={0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x5C,0x24,0x50,
            0xE8,0,0,0,0,0x0F,0x92,0xC2,0x0F,0xB6,0xD2,0xC1,0xE2,0x08,0x09,0xD0,
            0x48,0x83,0xC4,0x20,0x5B,0xC3};
        memcpy(page,caller,sizeof(caller));int32_t delta=128-15;memcpy(page+11,&delta,4);
        if(!save)page[16]=0x94; // SETZ instead of SETC.
        FlushInstructionCache(GetCurrentProcess(),page,160);
        auto call=reinterpret_cast<unsigned(__fastcall *)(Obj,const char *,void *,int,Obj)>(page);
        if(save) {
            for(unsigned capacity:{8u,32u})for(unsigned success:{0u,1u}) {
                *reinterpret_cast<uint64_t *>(archive+0x18)=capacity;archive_written=static_cast<unsigned char>(success);
                expect(call(archive_remote,archive_name,archive_fixture.data(),16,archive)==
                    static_cast<unsigned>(success)+(capacity<16?256u:0u));
            }
            archive_written=1;
        } else {
            std::vector<uint8_t> buffer(16);
            for(int count:{0,16}) {
                archive_read_result=count;
                expect(call(archive_remote,archive_name,buffer.data(),16,archive)==
                    static_cast<unsigned>(count)+(count==0?256u:0u));
            }
            archive_read_result=16;
        }
    }
    free(archive);free(archive_remote);archive_remote=nullptr;VirtualFree(page,0,MEM_RELEASE);
}
static void test_friendship_publication() {
    Obj previous=image;
    Obj simulated=static_cast<Obj>(VirtualAlloc(nullptr,0x2800000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    expect(simulated!=nullptr);image=simulated;
    for(const auto &entry:friendship_entries)memcpy(image+entry.rva,entry.bytes,entry.size);
    const unsigned char save[]={0xFF,0x10,0x48,0x83,0x7B,0x18,0x10},load[]={0xFF,0x50,0x08,0x85,0xC0};
    memcpy(image+0x3F7E32,save,sizeof(save));memcpy(image+0x3FA5C4,load,sizeof(load));
    putptr(image,0x13E69A0,image+0x50B670);putptr(image,0x13E60F8,image+0x509580);
    putptr(image,0x13A6BB0,image+0x2390B0);
    expect(friendship_guards_match());image[0x2283F5]^=1;expect(!friendship_guards_match());image[0x2283F5]^=1;
    Obj relay=allocate_near(image+0x223780);expect(relay!=nullptr);FriendshipCode code;
    expect(prepare_friendship(code,relay));
    expect(code.links[2].modified[5]==0x90 && code.links[7].modified[7]==0x90 && code.links[7].modified[8]==0x90);
    expect(publish_friendship(code));
    expect(pointer(image,0x13E69A0)==reinterpret_cast<Obj>(&friendship_save_host));
    expect(pointer(image,0x13E60F8)==reinterpret_cast<Obj>(&friendship_load_host));
    for(const auto &link:code.links)expect(!memcmp(image+link.block,link.modified,16));
    restore_friendship(code);expect(friendship_guards_match());
    for(const auto &link:code.links)expect(!memcmp(image+link.block,link.original,16));
    // Reject a competing writer atomically, then remove every earlier
    // successful publication; no partial set of native callbacks remains.
    expect(prepare_friendship(code,relay));image[0x2283F7]^=1;
    expect(!publish_friendship(code));restore_friendship(code);image[0x2283F7]^=1;
    expect(friendship_guards_match());
    VirtualFree(relay,0,MEM_RELEASE);image=previous;VirtualFree(simulated,0,MEM_RELEASE);
    original_friend_add=&mock_friend_add;original_friend_has=&mock_friend_has;
    original_friend_remove=&mock_friend_remove;original_friend_list=&mock_friend_list;
}
static void __fastcall mock_friend_one(Obj) {}
static void __fastcall mock_friend_reset(Obj officer) {put16(officer,0x10,0);memset(officer+0xC0,0,16);}
static unsigned char __fastcall mock_person_read(Obj,Obj archive) {return dword(archive,0x50)==0?1:0;}
static void test_friendship_lifecycle() {
    original_friend_cleanup=&mock_friend_one;original_friend_normalize=&mock_friend_one;
    original_friend_reset=&mock_friend_reset;native_functions[0x2390B0]=reinterpret_cast<void *>(&mock_person_read);
    Obj owner=object(4200),other=object(5000),custom=object(3100),archive=object(1),temporary=object(4200);
    for(Obj person:{owner,other,custom})slot(0x158,word(person,0x10),person);
    expanded_friends.clear();friendship_world=root;
    expect(expanded_friends.add_mutual(4200,5000,{},{}) && expanded_friends.add(4200,3100,{})==friendship::AddResult::Added);
    other[0xE6]=7;custom[0xE6]=7;friendship_normalize(owner);
    expect(expanded_friends.contains(4200,5000,{}) && !expanded_friends.contains(4200,3100,{}));
    friendship_cleanup(owner);expect(!expanded_friends.contains(4200,5000,{}));
    expect(expanded_friends.add(3100,5000,{})==friendship::AddResult::Added);other[0xE6]=9;
    friendship_normalize(custom);expect(expanded_friends.contains(3100,5000,{}));
    friendship_cleanup(custom);expect(!expanded_friends.contains(3100,5000,{}));
    other[0xE6]=9;expect(expanded_friends.add(4200,5000,{})==friendship::AddResult::Added);
    friendship_normalize(owner);expect(!expanded_friends.contains(4200,5000,{}));other[0xE6]=0;
    expect(expanded_friends.add_mutual(4200,5000,{},{}));
    put32(archive,0x20,0);expect(friendship_person_read(owner,archive)==1 && expanded_friends.contains(4200,5000,{}));
    put32(archive,0x20,1);put32(archive,0x50,1);
    expect(friendship_person_read(owner,archive)==0 && expanded_friends.contains(4200,5000,{}));
    put32(archive,0x50,0);expect(friendship_person_read(temporary,archive)==1 && expanded_friends.contains(4200,5000,{}));
    friendship_reset(temporary);expect(expanded_friends.contains(4200,5000,{}));
    expect(friendship_person_read(owner,archive)==1 && !expanded_friends.contains(4200,5000,{}) &&
           !expanded_friends.contains(5000,4200,{}));
    expect(expanded_friends.add_mutual(4200,5000,{},{}));friendship_reset(owner);
    expect(word(owner,0x10)==0 && !expanded_friends.contains(4200,5000,{}) && !expanded_friends.contains(5000,4200,{}));
    for(unsigned id:{4200u,5000u,3100u})slot(0x158,id,nullptr);
    free(owner);free(other);free(custom);free(archive);free(temporary);
}
