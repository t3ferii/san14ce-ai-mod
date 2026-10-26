static Obj domestic_region,domestic_officer,domestic_controlled,domestic_city_district,domestic_store;
static bool domestic_auto=true,domestic_lookup=true,domestic_write=true;
static unsigned domestic_written,domestic_orders,domestic_tick_calls;
static bool domestic_reject;
static int __fastcall domestic_level(Obj region,unsigned off) {
    unsigned v=word(region,off);return v<100?1:v<300?2:v<600?3:v<1000?4:5;
}
static int __fastcall domestic_commerce(Obj a) {return domestic_level(a,0x3C);}
static int __fastcall domestic_agriculture(Obj a) {return domestic_level(a,0x3E);}
static int __fastcall domestic_barracks(Obj a) {return domestic_level(a,0x40);}
static Obj __fastcall domestic_get_store(Obj) {return domestic_store;}
static unsigned char __fastcall domestic_get_area(Obj,unsigned key,Obj *out,int) {
    if(!domestic_lookup || key!=123)return 0;*out=domestic_region;return 1;
}
static unsigned char __fastcall domestic_store_selection(Obj,unsigned key,unsigned selection) {
    if(key!=456)return 0;domestic_written=selection;return domestic_write;
}
static uint64_t __fastcall domestic_original_node(Obj,Obj,Obj) {
    if(domestic_reject)return 2;
    unsigned cost=domestic_command_cost();
    if(domestic_orders<cost)return 2;
    domestic_orders-=cost;return 1;
}
static Obj __fastcall domestic_controlled_district(Obj) {return domestic_controlled;}
static int __fastcall domestic_auto_allowed() {return domestic_auto;}
static Obj __fastcall domestic_city_get_district(Obj) {return domestic_city_district;}
static unsigned __fastcall domestic_city_owner(Obj) {return 2;}
static unsigned __fastcall domestic_area_owner(Obj a) {return a[0x78];}
static int __fastcall domestic_original_tick(Obj,int preview,int *output) {
    ++domestic_tick_calls;*output=777;
    if(!preview) {
        unsigned policy=domestic_region[0x72];
        if(policy>=1 && policy<=3) {
            unsigned off=0x3A+policy*2;
            put16(domestic_region,off,word(domestic_region,off)+1);
        }
    }
    return 1;
}
static DWORD WINAPI domestic_other_thread(LPVOID) {return domestic_command_cost();}
static void domestic_bridge_tests() {
    Obj memory=static_cast<Obj>(VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    check(memory!=nullptr,"machine-code cost harness allocated");
    unsigned n=0;
    auto emit=[&](std::initializer_list<unsigned char> code) {for(auto b:code)memory[n++]=b;};
    auto immediate=[&](uint64_t value) {memcpy(memory+n,&value,8);n+=8;};
    alignas(16) uint64_t xmm_data[2]={0x123456789ABCDEF0ull,0xFEDCBA9876543210ull};
    // Two saved registers and 40 stack bytes leave RSP aligned before CALL.
    emit({0x53,0x57,0x48,0x83,0xEC,0x28,0x48,0x89,0xCF}); // preserve RBX,RDI; RDI=output
    emit({0x48,0xB8});immediate(reinterpret_cast<uint64_t>(xmm_data));
    for(unsigned i=0;i<6;++i)emit({0xF3,0x0F,0x6F,static_cast<unsigned char>(i*8)}); // MOVDQU XMMi,[RAX]
    const uint64_t sentinels[7]={0xAABBCCDDEEFF0011ull,0x1122334455667788ull,0x5566778899AABBCCull,
                                 0x2222333344445555ull,0x6666777788889999ull,7,0xAAAABBBBCCCCDDDDull};
    const unsigned char rex[7]={0x48,0x48,0x48,0x49,0x49,0x49,0x49};
    const unsigned char op[7]={0xB8,0xB9,0xBA,0xB8,0xB9,0xBA,0xBB};
    for(unsigned i=0;i<7;++i) {emit({rex[i],op[i]});immediate(sentinels[i]);}
    emit({0x48,0x39,0xC0,0x9C,0x8F,0x07}); // CMP RAX,RAX; save initial flags
    memory[n++]=0xE8;int32_t displacement=512-static_cast<int32_t>(n+4);memcpy(memory+n,&displacement,4);n+=4;
    emit({0x9C,0x8F,0x47,0x08}); // save returned flags before any flag-changing instruction
    const unsigned char stores[7]={0x47,0x4F,0x57,0x47,0x4F,0x57,0x5F};
    for(unsigned i=0;i<7;++i)emit({static_cast<unsigned char>(i<3?0x48:0x4C),0x89,stores[i],static_cast<unsigned char>(16+i*8)});
    emit({0x48,0x89,0x5F,0x48}); // save returned RBX
    for(unsigned i=0;i<6;++i) {
        emit({0xF3,0x0F,0x7F,static_cast<unsigned char>(0x87+i*8)});
        int32_t offset=80+i*16;memcpy(memory+n,&offset,4);n+=4;
    }
    emit({0x41,0x0F,0xAF,0xDA,0x89,0x9F,0xB0,0,0,0}); // native IMUL EBX,R10D
    emit({0x48,0x83,0xC4,0x28,0x5F,0x5B,0xC3});
    check(n<512,"machine-code cost harness leaves separate bridge storage");
    unsigned bridge_size=make_domestic_cost_bridge(memory+512);
    DWORD old=0;check(VirtualProtect(memory,0x1000,PAGE_EXECUTE_READ,&old)!=0,"cost harness and bridge executable");
    FlushInstructionCache(GetCurrentProcess(),memory,512+bridge_size);
    alignas(16) uint64_t output[24]={};
    for(unsigned auto_scope=0;auto_scope<2;++auto_scope) {
        in_auto_domestic=auto_scope;
        reinterpret_cast<void(__fastcall *)(void *)>(memory)(output);
        check(output[0]==output[1],"cost bridge preserves caller flags");
        check(memcmp(output+2,sentinels,sizeof(sentinels))==0,"cost bridge preserves all seven volatile integer registers");
        bool xmm_equal=true;for(unsigned i=0;i<6;++i)xmm_equal=xmm_equal && memcmp(output+10+i*2,xmm_data,16)==0;
        check(xmm_equal,"cost bridge preserves all six volatile SSE registers");
        check(output[9]==(auto_scope?0ull:3ull) && *reinterpret_cast<unsigned *>(output+22)==(auto_scope?0u:21u),
              "native cost multiplication sees zero only inside automatic officer scope");
    }
    in_auto_domestic=0;VirtualFree(memory,0,MEM_RELEASE);
}
static void domestic_tests() {
    BIND(0x22E2B0,domestic_commerce);BIND(0x22C760,domestic_agriculture);BIND(0x231170,domestic_barracks);
    BIND(0xBF7A40,domestic_get_store);BIND(0xB26470,domestic_get_area);BIND(0xB26990,domestic_store_selection);
    BIND(0x36320,domestic_original_node);BIND(0x32E8D0,domestic_controlled_district);
    BIND(0x3329B0,domestic_auto_allowed);BIND(0x22CC00,domestic_area_owner);
    domestic_region=object(499);domestic_officer=object(5999);domestic_store=object(1);
    slot(0x6CAF0,499,domestic_region);slot(0x158,5999,domestic_officer);
    put16(domestic_region,0x36,5999);domestic_region[0x35]=1;domestic_region[0x78]=2;
    domestic_city_district=object(51);slot(0xE0D0,51,domestic_city_district);domestic_officer[0xE0]=51;
    Obj city=object(1),vt=object(1),node=object(1);put16(city,0x4E,1);putptr(city,0,vt);
    putptr(vt,0x80,reinterpret_cast<void *>(&domestic_city_get_district));
    putptr(vt,0xB0,reinterpret_cast<void *>(&domestic_city_owner));
    *reinterpret_cast<unsigned *>(node+0x38)=123;*reinterpret_cast<unsigned *>(node+0x3C)=456;
    const unsigned values[5]={0,100,300,600,1000};
    for(unsigned a=1;a<=5;++a)for(unsigned b=1;b<=5;++b)for(unsigned c=1;c<=5;++c) {
        put16(domestic_region,0x3C,values[a-1]);put16(domestic_region,0x3E,values[b-1]);put16(domestic_region,0x40,values[c-1]);
        unsigned expected=a==5&&b==5&&c==5?0:a<=b&&a<=c?1:b<=c?2:3;
        check(balanced_development(domestic_region)==expected,"all 125 level combinations select lowest level with commerce agriculture barracks tie order");
    }
    put16(domestic_region,0x3C,1000);put16(domestic_region,0x3E,100);put16(domestic_region,0x40,100);
    domestic_region[0x72]=1;
    check(domestic_selection_node(node,node,node)==1 && domestic_written==2,"MAX commerce cannot trap automatic selection");
    domestic_region[0x72]=2;
    check(domestic_selection_node(node,node,node)==1 && domestic_written==0,"same correct policy and valid officer do not trigger reassignment");
    put16(domestic_region,0x36,0);
    check(domestic_selection_node(node,node,node)==1 && domestic_written==2,"missing officer still receives a concrete policy");
    put16(domestic_region,0x36,5999);domestic_lookup=false;
    check(domestic_selection_node(node,node,node)==2,"failed area lookup preserves native failure");domestic_lookup=true;
    domestic_write=false;check(domestic_selection_node(node,node,node)==2,"failed storage write preserves native failure");domestic_write=true;
    *reinterpret_cast<unsigned *>(image+0x1A483DC)=3;domestic_orders=8;
    check(domestic_original_node(node,node,node)==1 && domestic_orders==5,"direct human command retains native cost");
    domestic_orders=0;check(domestic_officer_node(node,node,node)==1 && domestic_orders==0 && in_auto_domestic==0,
                          "automatic assignment succeeds at zero orders without granting orders");
    domestic_reject=true;check(domestic_officer_node(node,node,node)==2 && in_auto_domestic==0,"failure restores automatic scope");domestic_reject=false;
    in_auto_domestic=1;
    HANDLE worker=CreateThread(nullptr,0,domestic_other_thread,nullptr,0,nullptr);DWORD other_cost=0;
    check(worker && WaitForSingleObject(worker,10000)==WAIT_OBJECT_0 && GetExitCodeThread(worker,&other_cost) && other_cost==3,
          "automatic free cost is thread local and does not affect a concurrent manual command");
    CloseHandle(worker);in_auto_domestic=0;
    enabled=0;domestic_orders=8;
    check(domestic_officer_node(node,node,node)==1 && domestic_orders==5,"disabled callback forwards original assignment cost");enabled=1;
    domestic_controlled=domestic_city_district;domestic_auto=false;
    put16(domestic_region,0x3C,100);put16(domestic_region,0x3E,0);put16(domestic_region,0x40,0);domestic_region[0x72]=1;
    synchronize_domestic(city);check(domestic_region[0x72]==1,"controlled district with automatic domestic disabled retains manual policy");
    domestic_auto=true;synchronize_domestic(city);check(domestic_region[0x72]==1,"ruler direct district stays manual even with native automatic domestic");
    mod_features.domestic_direct=true;domestic_auto=false;synchronize_domestic(city);check(domestic_region[0x72]==2,"1st-corps switch automates the direct district");
    mod_features.domestic_direct=false;domestic_region[0x72]=1;
    domestic_controlled=nullptr;domestic_auto=false;domestic_region[0x72]=1;
    synchronize_domestic(city);check(domestic_region[0x72]==2,"delegated or computer district rotates independently of human auto switch");
    domestic_officer[0xE0]=50;domestic_region[0x72]=1;synchronize_domestic(city);
    check(domestic_region[0x72]==1,"mismatched officer district is left unchanged");domestic_officer[0xE0]=51;
    domestic_region[0x78]=3;synchronize_domestic(city);check(domestic_region[0x72]==1,"occupied region is left unchanged");domestic_region[0x78]=2;
    original_domestic_tick=domestic_original_tick;int out=0;domestic_tick_calls=0;
    put16(domestic_region,0x3C,99);put16(domestic_region,0x3E,0);put16(domestic_region,0x40,0);domestic_region[0x72]=3;
    check(domestic_tick(city,1,&out)==1 && out==777 && domestic_region[0x72]==3 && word(domestic_region,0x40)==0,
          "preview calculation never writes a policy or development value");
    check(domestic_tick(city,0,&out)==1 && out==777 && word(domestic_region,0x3C)==100 && domestic_region[0x72]==2,
          "real development tick chooses commerce then changes to agriculture after level up");
    put16(domestic_region,0x3E,99);domestic_tick(city,0,&out);
    check(word(domestic_region,0x3E)==100 && domestic_region[0x72]==3,"2 2 1 changes next development to barracks");
    put16(domestic_region,0x40,99);domestic_tick(city,0,&out);
    check(word(domestic_region,0x40)==100 && domestic_region[0x72]==1,"2 2 2 returns to commerce");
    put16(domestic_region,0x3C,1000);put16(domestic_region,0x3E,1000);put16(domestic_region,0x40,1000);
    synchronize_domestic(city);check(domestic_region[0x72]==0 && word(domestic_region,0x36)==5999,"all MAX stops development but keeps regional officer");
    slot(0x6CAF0,499,nullptr);slot(0x158,5999,nullptr);slot(0xE0D0,51,nullptr);
    domestic_bridge_tests();
}
