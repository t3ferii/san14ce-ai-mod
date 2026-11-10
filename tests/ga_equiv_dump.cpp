// Address-table equivalence dump (docs/MODULARIZATION_PLAN.md 1-C). The address guard fixture with the dump hooks of src/game_addresses.h
// switched on: every address-table value that is read (V), the EFFECTIVE expectation of every ga::mcmp (M) / ga::mcmp_at (A) guard
// and every guard hash (H, G) that runs, and the source files the address gates deny (D). The output holds no ids, only RVAs and bytes, so a dump taken
// before the re-basing of the table (old-build ids, build index 1) and one taken after it (1a4a447d ids, build index 0) must be equal
// (tools/ga_equiv_compare.py). Test build only; the shipped DLLs never define GA_EQUIV_DUMP.
//   ga_equiv_dump <unpacked game exe> <build index>        output file: environment variable GA_EQUIV_OUT
#define GA_EQUIV_DUMP
#include "address_guard_fixture.cpp"

namespace {
struct DumpWriter {
    ~DumpWriter() {
        char path[MAX_PATH]={};
        if(!GetEnvironmentVariableA("GA_EQUIV_OUT",path,sizeof(path)))return;
        FILE *f=nullptr;
        if(fopen_s(&f,path,"wb")||!f)return;
        const ga::State &s=ga::state();
        for(unsigned i=0;i<GA_COUNT;++i)if(s.vals[i])fprintf(f,"T %X\n",s.vals[i]);
        for(unsigned i=0;i<GA_FILE_COUNT;++i)if(s.denied[i>>6]&(1ull<<(i&63)))fprintf(f,"D %s\n",ga_file_names[i]);
        for(const auto &line:ga::dump_set())fprintf(f,"%s\n",line.c_str());
        fclose(f);
    }
} dump_writer;
}
