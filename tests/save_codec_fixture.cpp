// Save codec check on real autosave files (read-only copies) and random data:
// 1) our decoder reproduces each original block exactly to its recorded size,
// 2) our rank table equals the stored table (9332B0 semantics),
// 3) fast encoder output decodes back to the same bytes; size/time vs original.
// Usage: save_codec_fixture <file.s14>...
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <random>
#include "../src/save_codec.h"
static std::vector<uint8_t> load(const char *p){std::vector<uint8_t> d;FILE *f=nullptr;if(fopen_s(&f,p,"rb")||!f)return d;
    fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);d.resize(n);if(fread(d.data(),1,n,f)!=(size_t)n)d.clear();fclose(f);return d;}
static double ms(LARGE_INTEGER a,LARGE_INTEGER b){LARGE_INTEGER f;QueryPerformanceFrequency(&f);return (b.QuadPart-a.QuadPart)*1000.0/f.QuadPart;}
int main(int argc,char **argv) {
    unsigned checks=0,failures=0,blocks=0;double encode_ms=0;size_t ours=0,theirs=0,raw_total=0;
    std::vector<uint32_t> work((1u<<save_codec::kHashBits)+save_codec::kWindow);
    auto roundtrip=[&](const uint8_t *raw,size_t n,size_t *coded)->bool{
        uint8_t rank[256],table[256];save_codec::ranks(raw,n,rank,table);
        std::vector<uint8_t> out(2*n+1024),back(n);   // incompressible input may expand
        LARGE_INTEGER a,b;QueryPerformanceCounter(&a);
        size_t m=save_codec::encode(raw,n,rank,out.data(),out.size(),work.data());
        QueryPerformanceCounter(&b);encode_ms+=ms(a,b);
        if(coded)*coded=m;
        return m && save_codec::decode(out.data(),m,table,back.data(),n) && !memcmp(back.data(),raw,n);
    };
    for(int i=1;i<argc;++i) {
        auto d=load(argv[i]);
        for(size_t at=0;at+0x10C<=d.size();++at) {
            if(memcmp(d.data()+at,"LWC\x1a",4))continue;
            uint32_t raw,coded;memcpy(&raw,d.data()+at+4,4);memcpy(&coded,d.data()+at+8,4);
            if(at+0x10C+coded>d.size())continue;
            ++blocks;const uint8_t *table=d.data()+at+12;
            std::vector<uint8_t> plain(raw);
            ++checks;if(!save_codec::decode(d.data()+at+0x10C,coded,table,plain.data(),raw)){++failures;printf("decode failed %s\n",argv[i]);continue;}
            uint8_t rank[256],mine[256];save_codec::ranks(plain.data(),raw,rank,mine);
            ++checks;if(memcmp(mine,table,256)){++failures;printf("rank table differs %s\n",argv[i]);}
            size_t m=0;++checks;if(!roundtrip(plain.data(),raw,&m)){++failures;printf("roundtrip failed %s\n",argv[i]);}
            ours+=m;theirs+=coded;raw_total+=raw;
        }
    }
    std::mt19937 rng(1234);
    for(int t=0;t<300;++t) {
        size_t n=1+rng()%50000;std::vector<uint8_t> v(n);int mode=t%4;
        for(size_t k=0;k<n;++k)v[k]=static_cast<uint8_t>(mode==0?rng():mode==1?rng()%3:mode==2?(k/((rng()%7)+1))&0xFF:(k%97<40?0:rng()%16));
        if(mode==3)for(size_t k=600;k<n;k+=1500)memcpy(v.data()+k-500,v.data()+k-600,100);
        ++checks;if(!roundtrip(v.data(),n,nullptr)){++failures;printf("random roundtrip failed t=%d n=%zu\n",t,n);}
    }
    printf("{\"ok\":%s,\"checks\":%u,\"failures\":%u,\"real_blocks\":%u,\"raw_bytes\":%zu,\"original_coded\":%zu,\"ours_coded\":%zu,\"encode_ms_total\":%.1f}\n",
           failures||!blocks?"false":"true",checks,failures,blocks,raw_total,theirs,ours,encode_ms);
    return failures||!blocks?1:0;
}

