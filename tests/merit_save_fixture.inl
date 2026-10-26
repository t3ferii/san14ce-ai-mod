static const char *save_name="fixture_merit_native.s14";
static const unsigned char archive_data[]={1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31};
static unsigned char archive_success=1;
static int archive_count=sizeof(archive_data);
static bool native_load_success=true;
static Obj remote;
static unsigned char __fastcall mock_write(Obj r,const char *name,const void *data,int count) {
    expect(r==remote && !strcmp(name,save_name) && count==sizeof(archive_data));
    expect(!memcmp(data,archive_data,sizeof(archive_data)));return archive_success;
}
static int __fastcall mock_read(Obj r,const char *name,void *data,int count) {
    expect(r==remote && !strcmp(name,save_name) && count==sizeof(archive_data));
    memcpy(data,archive_data,sizeof(archive_data));return archive_count;
}
static void __fastcall mock_save_host(Obj host) {
    if(dword(host,0x470)==1) {
        merit_file_write(remote,save_name,archive_data,sizeof(archive_data));put32(host,0x470,2);
    } else if(dword(host,0x470)==2) {put32(host,0x470,3);put32(image,0x27D2A20,archive_success);}
}
static void __fastcall mock_load_host(Obj host) {
    if(dword(host,0x470)==1) {
        unsigned char buffer[sizeof(archive_data)];merit_file_read(remote,save_name,buffer,sizeof(buffer));
        put32(host,0x470,2);
    } else if(dword(host,0x470)==2) {put32(host,0x470,3);put32(image,0x27D2A50,native_load_success?1:0);}
}
static void job_name(unsigned offset) {
    Obj at=image+offset;putptr(at,0,save_name);
    *reinterpret_cast<size_t *>(at+0x10)=strlen(save_name);
    *reinterpret_cast<size_t *>(at+0x18)=strlen(save_name);
}
static void test_persistence() {
    self_module=GetModuleHandleW(nullptr);
    Obj save=object(1),load=object(1);remote=object(1);
    void *vt[]={reinterpret_cast<void *>(&mock_write),reinterpret_cast<void *>(&mock_read)};
    putptr(remote,0,vt);functions[0x50B670]=reinterpret_cast<void *>(&mock_save_host);
    functions[0x509580]=reinterpret_cast<void *>(&mock_load_host);
    job_name(0x27D2A98);job_name(0x27D2A68);
    merit::Binding hash={};expect(merit_digest(archive_data,sizeof(archive_data),hash));
    merit::SaveCoordinator::Completed job;job.name=save_name;job.binding=hash;
    wchar_t path[MAX_PATH];expect(merit_sidecar_path(job,path,MAX_PATH,true));DeleteFileW(path);
    Obj person=table(0x158,100,6000),ruler=table(0x158,20,6000);person[0x1C2]=3;
    merit_bonds.clear();expect(merit_bonds.award(100,2,merit_context,nullptr));
    put32(save,0x470,1);merit_save_host(save);merit_bonds.clear();
    expect(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES);merit_save_host(save);
    expect(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES);
    merit::Store disk;expect(merit_read_sidecar(job,disk) && disk.contains(100,20,merit_context,nullptr));
    put32(load,0x470,1);merit_load_host(load);merit_load_host(load);
    expect(merit_has(person,ruler) && merit_has(ruler,person));
    // A successful old/native-only load must not inherit current bonds.
    DeleteFileW(path);put32(load,0x470,1);merit_load_host(load);merit_load_host(load);
    expect(merit_bonds.size()==0);
    expect(merit_bonds.award(100,2,merit_context,nullptr));job.store=merit_bonds;
    expect(merit_write_sidecar(job));archive_count=15;
    put32(load,0x470,1);merit_load_host(load);merit_load_host(load);expect(merit_bonds.size()==0);
    archive_count=16;expect(merit_bonds.award(100,2,merit_context,nullptr));native_load_success=false;
    put32(load,0x470,1);merit_load_host(load);merit_load_host(load);expect(merit_has(person,ruler));
    native_load_success=true;
    // Context changed in the native save: stored ruler/force cannot leak.
    person[0xE0]=1;put32(load,0x470,1);merit_load_host(load);merit_load_host(load);
    expect(merit_bonds.size()==0);person[0xE0]=2;
    // Corruption is rejected and clears supplemental data on successful load.
    expect(merit_bonds.award(100,2,merit_context,nullptr));
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    expect(file!=INVALID_HANDLE_VALUE);DWORD wrote=0;expect(WriteFile(file,archive_data,sizeof(archive_data),&wrote,nullptr)!=0);
    CloseHandle(file);put32(load,0x470,1);merit_load_host(load);merit_load_host(load);expect(merit_bonds.size()==0);
    DeleteFileW(path);archive_success=0;expect(merit_bonds.award(100,2,merit_context,nullptr));
    put32(save,0x470,1);merit_save_host(save);merit_save_host(save);
    expect(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES);archive_success=1;
    // Only filenames accepted by the coordinator can be used as paths.
    job.name="../outside.s14";expect(!merit_sidecar_path(job,path,MAX_PATH,true));
    free(save);free(load);free(remote);
}
