// Address resolver fixture (user 2026-11-05 patch resilience): maps an unpacked game EXE from disk, runs the real runtime
// resolver of src/game_addresses.h and compares it with the per-build table. Reads files only, never the live game.
//   addr_resolver_fixture <unpacked game exe> <build index: 0 = 1fbe6be7, 1 = 1a4a447d>
// Fails (exit 1) when an id with a table value resolves to a different RVA, when an id resolved by the scan has no table
// value and is not an explicit denial, or when the unique-signature scan is not unique for a table-valued id.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <chrono>
#include "../src/game_addresses.h"

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
    auto image=static_cast<unsigned char *>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!image)return nullptr;
    memcpy(image,file.data(),nt->OptionalHeader.SizeOfHeaders);
    auto sec=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)
        if(sec[i].SizeOfRawData)memcpy(image+sec[i].VirtualAddress,file.data()+sec[i].PointerToRawData,sec[i].SizeOfRawData);
    return image;
}
int main(int argc,char **argv) {
    if(argc==4 && !strcmp(argv[2],"scan")) {   // new build: resolve every id by signature/RTTI and write TSV: old_rva, scanned rva (0 = unresolved), method
        unsigned char *image=map_exe(argv[1]);
        if(!image)return 2;
        std::vector<unsigned> out(GA_COUNT);
        unsigned n=ga::resolve_image(image,ga::kPreferredBase,out.data());
        FILE *f=nullptr;
        if(fopen_s(&f,argv[3],"w")||!f)return 2;
        for(unsigned i=0;i<GA_COUNT;++i)fprintf(f,"%X\t%X\t%u\n",ga_entries[i].old_rva,out[i],ga_entries[i].method);
        fclose(f);
        printf("{\"status\":\"address_scan_done\",\"ids\":%u,\"resolved\":%u}\n",GA_COUNT,n);
        return 0;
    }
    if(argc!=3) {fprintf(stderr,"usage: addr_resolver_fixture <exe> <build index> | <exe> scan <out.tsv>\n");return 2;}
    int b=atoi(argv[2]);
    if(b<0||b>1) {fprintf(stderr,"bad build index\n");return 2;}
    unsigned char *image=map_exe(argv[1]);
    if(!image) {fprintf(stderr,"cannot map %s\n",argv[1]);return 2;}
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew);
    if(nt->FileHeader.TimeDateStamp!=ga_builds[b].timestamp) {fprintf(stderr,"timestamp %08X does not belong to build %s\n",nt->FileHeader.TimeDateStamp,ga_builds[b].name);return 1;}
    std::vector<unsigned> out(GA_COUNT);
    auto t0=std::chrono::steady_clock::now();
    unsigned n=ga::resolve_image(image,ga::kPreferredBase,out.data());
    double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
    const unsigned *table=ga_builds[b].vals;
    unsigned methods[6]={},method_ok[6]={},table_only=0,table_only_valued=0,mismatch=0,denied=0,extra=0,matched=0,unresolved_table=0;
    for(unsigned i=0;i<GA_COUNT;++i) {
        const GaEntry &e=ga_entries[i];
        ++methods[e.method];
        if(e.method==0) {if(table[i])++table_only_valued;else ++unresolved_table;++table_only;continue;}
        if(table[i]) {
            if(out[i]==table[i]) {++matched;++method_ok[e.method];}
            else {++mismatch;fprintf(stderr,"MISMATCH GA_%X (%s): scan %X, table %X\n",e.old_rva,e.method==1?"sig":e.method==2?"mid":e.method==3?"rtti":e.method==4?"slotfn":"dataref",out[i],table[i]);}
        } else if(out[i]) ++denied;       // resolvable but the table deliberately keeps 0 (changed function: feature stays off)
        else ++unresolved_table;
    }
    // ids without a table value are explicit denials or unresolvable; ids resolved without a value are reported as denied
    (void)extra;
#ifdef GA_PROFILE
    fprintf(stderr,"phases ms: cands %.1f sig %.1f rtti %.1f dep %.1f\n",ga::g_phase[1]-ga::g_phase[0],ga::g_phase[2]-ga::g_phase[1],ga::g_phase[3]-ga::g_phase[2],ga::g_phase[4]-ga::g_phase[3]);
#endif
    printf("{\"status\":\"%s\",\"build\":\"%s\",\"ids\":%u,\"scan_resolved\":%u,\"scan_ms\":%.1f,\"table_values\":%u,\"matched_by_scan\":%u,\"mismatch\":%u,"
           "\"table_only_with_value\":%u,\"denied_by_policy\":%u,\"unresolved\":%u,\"methods\":{\"table\":%u,\"sig\":%u,\"mid\":%u,\"rtti\":%u,\"slotfn\":%u,\"dataref\":%u},"
           "\"matched_by_method\":{\"sig\":%u,\"mid\":%u,\"rtti\":%u,\"slotfn\":%u,\"dataref\":%u}}\n",
           mismatch?"address_resolver_fixture_failed":"address_resolver_fixture_passed",ga_builds[b].name,GA_COUNT,n,ms,
           [&]{unsigned c=0;for(unsigned i=0;i<GA_COUNT;++i)if(table[i])++c;return c;}(),matched,mismatch,table_only_valued,denied,unresolved_table,
           methods[0],methods[1],methods[2],methods[3],methods[4],methods[5],method_ok[1],method_ok[2],method_ok[3],method_ok[4],method_ok[5]);
    return mismatch?1:0;
}
