// Equivalence check: the ORIGINAL E09910 + 102FC20 machine code, read from the
// DRM-free analysis copy and run in a private buffer, versus name_lookup::find.
// Usage: name_lookup_fixture <game.exe.unpacked.exe> <running_code_text.bin>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "../src/name_lookup.h"
using Find=unsigned char *(*)(unsigned char *,const char *);
static std::vector<unsigned char> read_file(const char *path) {
    std::vector<unsigned char> data;FILE *f=nullptr;
    if(fopen_s(&f,path,"rb") || !f)return data;
    fseek(f,0,SEEK_END);long size=ftell(f);fseek(f,0,SEEK_SET);
    data.resize(static_cast<size_t>(size));
    if(fread(data.data(),1,data.size(),f)!=data.size())data.clear();
    fclose(f);return data;
}
struct Pe {
    const std::vector<unsigned char> &d;
    const unsigned char *at(unsigned rva,unsigned size) const {
        unsigned pe=*reinterpret_cast<const unsigned *>(&d[0x3C]);
        unsigned n=*reinterpret_cast<const unsigned short *>(&d[pe+6]),opt=*reinterpret_cast<const unsigned short *>(&d[pe+20]);
        for(unsigned i=0;i<n;++i) {
            const unsigned char *s=&d[pe+24+opt+i*40];
            unsigned vs=*reinterpret_cast<const unsigned *>(s+8),va=*reinterpret_cast<const unsigned *>(s+12),
                     raw=*reinterpret_cast<const unsigned *>(s+20);
            if(rva>=va && rva+size<=va+vs)return &d[raw+rva-va];
        }
        return nullptr;
    }
    // Printable ASCII runs inside .rdata: real game identifiers/strings.
    std::vector<std::string> strings(size_t limit) const {
        std::vector<std::string> out;const unsigned char *p=at(0x1340000,0x6DF000);if(!p)return out;
        std::string cur;
        for(unsigned i=0;i<0x6DF000 && out.size()<limit;++i) {
            unsigned char c=p[i];
            if(c>=0x20 && c<0x7F) {cur+=static_cast<char>(c);continue;}
            if(c==0 && cur.size()>=3 && cur.size()<0x50)out.push_back(cur);
            cur.clear();
        }
        return out;
    }
};
static unsigned long long rng=0x9E3779B97F4A7C15ull;
static unsigned next(unsigned n) {rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return static_cast<unsigned>(rng%n);}
struct Table {
    // Chain of objects: [8] parent, parent [0x20] next table. Last table is searched.
    std::vector<std::vector<unsigned char>> objects,records;
    unsigned char *head=nullptr;
    void build(const std::vector<std::string> &names,unsigned depth) {
        objects.assign(depth*2+1,std::vector<unsigned char>(0x40,0xCD));
        records.assign(depth+1,{});
        for(unsigned level=0;level<=depth;++level) {
            auto &table=objects[level*2];auto &recs=records[level];
            // Decoy tables earlier in the chain hold the reversed list.
            std::vector<std::string> list=names;
            if(level<depth)list.assign(names.rbegin(),names.rend());
            recs.assign(list.size()*name_lookup::record_size+8,0xEE);
            for(size_t i=0;i<list.size();++i) {
                unsigned char *r=&recs[i*name_lookup::record_size];
                memcpy(r,list[i].c_str(),list[i].size()+1);
            }
            *reinterpret_cast<unsigned char **>(&table[0])=recs.data();
            *reinterpret_cast<unsigned *>(&table[0x18])=static_cast<unsigned>(list.size());
            if(level<depth) {
                auto &parent=objects[level*2+1];
                *reinterpret_cast<unsigned char **>(&table[8])=parent.data();
                *reinterpret_cast<unsigned char **>(&parent[0x20])=objects[level*2+2].data();
            } else *reinterpret_cast<unsigned char **>(&table[8])=nullptr;
        }
        head=objects[0].data();
    }
};
int main(int argc,char **argv) {
    if(argc<3) {puts("{\"ok\":false,\"error\":\"usage\"}");return 2;}
    auto exe=read_file(argv[1]),text=read_file(argv[2]);
    if(exe.size()<0x1000 || text.empty()) {puts("{\"ok\":false,\"error\":\"input\"}");return 2;}
    Pe pe{exe};using namespace name_lookup;
    const unsigned char *entry=pe.at(entry_rva,sizeof(entry_bytes)),*compare=pe.at(compare_rva,sizeof(compare_bytes));
    bool exe_guard=entry && compare && !memcmp(entry,entry_bytes,sizeof(entry_bytes)) && !memcmp(compare,compare_bytes,sizeof(compare_bytes));
    bool running_guard=text.size()>compare_rva-0x1000+sizeof(compare_bytes) &&
        !memcmp(&text[entry_rva-0x1000],entry_bytes,sizeof(entry_bytes)) && !memcmp(&text[compare_rva-0x1000],compare_bytes,sizeof(compare_bytes));
    int rel=*reinterpret_cast<const int *>(entry_bytes+compare_call+1);
    bool call_target=entry_bytes[compare_call]==0xE8 && entry_rva+compare_call+5+rel==compare_rva;
    if(!exe_guard || !running_guard || !call_target) {
        printf("{\"ok\":false,\"exe_guard\":%d,\"running_guard\":%d,\"call_target\":%d}\n",exe_guard,running_guard,call_target);return 1;
    }
    // Run the original bytes: E09910 copy at 0, strncmp copy at 0x100.
    auto code=static_cast<unsigned char *>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    memset(code,0xCC,4096);memcpy(code,entry,sizeof(entry_bytes));memcpy(code+0x100,compare,sizeof(compare_bytes));
    int local=0x100-(compare_call+5);memcpy(code+compare_call+1,&local,4);
    DWORD old;VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old);FlushInstructionCache(GetCurrentProcess(),code,4096);
    Find original=reinterpret_cast<Find>(code);
    unsigned checks=0,mismatches=0,hits=0;
    auto compare_one=[&](Table &t,const char *key) {
        unsigned char *a=original(t.head,key),*b=find(t.head,key);
        ++checks;if(a)++hits;
        if(a!=b && mismatches++<5)printf("mismatch key=\"%s\"\n",key?key:"(null)");
    };
    // Empty tables never read the key: null key is valid in both.
    {Table t;t.build({},0);compare_one(t,nullptr);t.build({},2);compare_one(t,nullptr);}
    // Random tables: small alphabet (prefix/duplicate collisions), high bytes, long names.
    const char alphabet[]={'a','b','c','\x80','\xEA','\xFF'};
    for(unsigned round=0;round<4000;++round) {
        unsigned count=next(5)==0?0:next(64)+1,depth=next(4);
        std::vector<std::string> names(count);
        for(auto &n:names) {
            unsigned len=next(8)==0?next(0x58):next(6);
            for(unsigned i=0;i<len;++i)n+=alphabet[next(sizeof(alphabet))];
        }
        Table t;t.build(names,depth);
        for(unsigned q=0;q<16;++q) {
            std::string key;
            unsigned kind=next(4);
            if(kind==0 && count)key=names[next(count)];
            else if(kind==1 && count) {key=names[next(count)];if(!key.empty())key.pop_back();}
            else if(kind==2 && count) {key=names[next(count)]+alphabet[next(sizeof(alphabet))];}
            else {unsigned len=next(6);for(unsigned i=0;i<len;++i)key+=alphabet[next(sizeof(alphabet))];}
            compare_one(t,key.c_str());
        }
        // Key that aliases a record inside the searched table.
        if(count) {auto &recs=t.records.back();compare_one(t,reinterpret_cast<const char *>(&recs[next(count)*record_size]));}
    }
    // Real strings from the game's .rdata, with duplicates.
    auto real=pe.strings(60000);
    unsigned real_checks_before=checks;
    for(unsigned round=0;round<40 && real.size()>4000;++round) {
        std::vector<std::string> names;
        unsigned start=next(static_cast<unsigned>(real.size()-4000));
        for(unsigned i=0;i<2000;++i)names.push_back(real[start+next(4000)]);
        Table t;t.build(names,next(3));
        for(unsigned q=0;q<500;++q) {
            std::string key=next(2)?names[next(2000)]:real[next(static_cast<unsigned>(real.size()))];
            if(next(8)==0 && !key.empty())key.pop_back();
            compare_one(t,key.c_str());
        }
    }
    unsigned real_checks=checks-real_checks_before;
    // Timing on a real-string table (half present, half absent keys).
    std::vector<std::string> names(real.begin(),real.begin()+(real.size()>3000?3000:real.size()));
    Table t;t.build(names,1);
    std::vector<std::string> keys;
    for(unsigned i=0;i<2000;++i)keys.push_back(i%2?names[next(static_cast<unsigned>(names.size()))]:real[next(static_cast<unsigned>(real.size()))]+"#");
    LARGE_INTEGER f,a0,a1,b0,b1;QueryPerformanceFrequency(&f);
    uintptr_t sink=0;
    QueryPerformanceCounter(&a0);for(auto &k:keys)sink+=reinterpret_cast<uintptr_t>(original(t.head,k.c_str()));QueryPerformanceCounter(&a1);
    QueryPerformanceCounter(&b0);for(auto &k:keys)sink-=reinterpret_cast<uintptr_t>(find(t.head,k.c_str()));QueryPerformanceCounter(&b1);
    double ms_original=(a1.QuadPart-a0.QuadPart)*1000.0/f.QuadPart,ms_new=(b1.QuadPart-b0.QuadPart)*1000.0/f.QuadPart;
    bool ok=!mismatches && sink==0 && real_checks>=10000 && hits>0;
    printf("{\"ok\":%s,\"checks\":%u,\"real_string_checks\":%u,\"real_strings\":%zu,\"hits\":%u,\"mismatches\":%u,"
           "\"exe_guard\":true,\"running_guard\":true,\"bench_records\":%zu,\"bench_queries\":%zu,"
           "\"original_ms\":%.2f,\"new_ms\":%.2f,\"speedup\":%.1f}\n",
           ok?"true":"false",checks,real_checks,real.size(),hits,mismatches,names.size(),keys.size(),ms_original,ms_new,ms_new>0?ms_original/ms_new:0.0);
    return ok?0:1;
}
