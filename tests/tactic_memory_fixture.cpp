// tactic_memory_store.h fixture (docs/TACTIC_PRIORITY_V2_PLAN.md 5, 7 WP-3). Pure: no game image, no game memory.
#include "../src/tactic_memory_store.h"
#include <cstdio>
static int checks=0,failures=0;
static void ok(bool v,const char *what) {++checks;if(!v){++failures;std::printf("FAIL %s\n",what);}}
using namespace tactic_memory;
static Binding make_binding(uint8_t seed) {Binding b;for(unsigned i=0;i<32;++i)b[i]=static_cast<uint8_t>(seed+i*7);return b;}
static bool same(const Table &a,const Table &b) {
    for(unsigned i=0;i<MaxPeople;++i) {
        if(a[i].formation!=b[i].formation || a[i].flags!=b[i].flags)return false;
        for(unsigned k=0;k<3;++k)if(a[i].tactics[k]!=b[i].tactics[k])return false;
    }
    return true;
}
// Builds a v1 file image (24040 bytes) from a table, flags dropped.
static std::vector<uint8_t> encode_v1(const Table &table,const Binding &binding) {
    std::vector<uint8_t> bytes(EncodedSizeV1);
    memcpy(bytes.data(),"TPRM\1\0\0\0",8);memcpy(bytes.data()+8,binding.data(),32);
    for(unsigned i=0;i<MaxPeople;++i) {
        uint8_t *at=bytes.data()+40+i*4;at[0]=table[i].formation;
        for(unsigned k=0;k<3;++k)at[1+k]=table[i].tactics[k];
    }
    return bytes;
}
int main() {
    Binding binding=make_binding(3),other=make_binding(4);
    Table table;
    ok(empty(table[0]) && empty(table[MaxPeople-1]),"default records are empty");
    table[1].formation=0;table[1].tactics[0]=1;table[1].tactics[1]=137;table[1].tactics[2]=200;
    table[2999].formation=20;table[2999].tactics[0]=17;table[2999].flags=kFlagRemember;
    table[5999].formation=7;table[5999].tactics[0]=25;table[5999].tactics[1]=26;table[5999].tactics[2]=6;
    table[77].flags=kFlagRemember;   // flag only, no sortie yet
    ok(!empty(table[77]),"flag-only record is not empty");
    auto bytes=encode(table,binding);
    ok(bytes.size()==30040 && bytes.size()==EncodedSize && EncodedSizeV1==24040,"encoded size 30040, v1 24040");
    ok(!memcmp(bytes.data(),"TPRM\2\0\0\0",8),"magic v2");
    ok(!memcmp(bytes.data()+8,binding.data(),32),"binding embedded");
    ok(bytes[40+5]==0 && bytes[40+6]==1 && bytes[40+7]==137 && bytes[40+8]==200 && bytes[40+9]==0,"record layout person 1");
    ok(bytes[40+5*2999+4]==1 && bytes[40+5*77+4]==1 && bytes[40+5*77]==0xFF,"flags byte");
    Table back;back[10].formation=9;unsigned version=0;
    ok(decode(bytes.data(),bytes.size(),binding,back,&version) && version==2,"roundtrip decodes as v2");
    ok(same(table,back),"roundtrip equal incl. flags");
    ok(decode(bytes.data(),bytes.size(),binding,back) ,"version out-param is optional");
    Table blank,blankBack;blankBack[5].formation=3;auto blankBytes=encode(blank,binding);
    ok(decode(blankBytes.data(),blankBytes.size(),binding,blankBack) && same(blank,blankBack),"empty table roundtrip");
    // v1 files are read with flags 0 and re-encoded as v2
    {
        Table v1table=table;for(auto &r:v1table)r.flags=0;v1table[77]=Record();
        auto v1=encode_v1(v1table,binding);
        ok(v1.size()==24040,"v1 image size");
        Table got;got[3].flags=1;version=0;
        ok(decode(v1.data(),v1.size(),binding,got,&version) && version==1 && same(got,v1table),"v1 decodes with flags 0");
        auto again=encode(got,binding);
        ok(again.size()==30040 && !memcmp(again.data(),"TPRM\2\0\0\0",8),"v1 data is written back as v2");
        Table keep;keep[7].formation=11;Table out=keep;
        {auto b=v1;b[4]=2;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"v1 size with v2 magic rejected");}
        {auto b=bytes;b[4]=1;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"v2 size with v1 magic rejected");}
        {auto b=v1;b[40+4*2999]=21;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"v1 formation 21 rejected");}
        ok(!decode(v1.data(),v1.size(),other,out) && same(out,keep),"v1 other binding rejected");
    }
    // every rejection leaves out unchanged
    Table out;out[7].formation=11;out[7].tactics[1]=99;Table keep=out;
    ok(!decode(nullptr,bytes.size(),binding,out) && same(out,keep),"null rejected");
    ok(!decode(bytes.data(),bytes.size()-1,binding,out) && same(out,keep),"short size rejected");
    {auto longer=bytes;longer.push_back(0xFF);ok(!decode(longer.data(),longer.size(),binding,out) && same(out,keep),"long size rejected");}
    ok(!decode(bytes.data(),0,binding,out) && same(out,keep),"zero size rejected");
    {auto b=bytes;b[0]='X';ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"bad magic rejected");}
    {auto b=bytes;b[4]=3;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"bad version rejected");}
    {auto b=bytes;b[5]=1;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"bad magic padding rejected");}
    ok(!decode(bytes.data(),bytes.size(),other,out) && same(out,keep),"other binding rejected");
    {auto b=bytes;b[8+31]^=1;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"one-bit binding change rejected");}
    {auto b=bytes;b[40+5*2999]=21;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"formation 21 rejected");}
    {auto b=bytes;b[40+5*2999]=20;ok(decode(b.data(),b.size(),binding,out),"formation 20 accepted");out=keep;}
    {auto b=bytes;b[40+5*2999+1]=201;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"tactic 201 rejected");}
    {auto b=bytes;b[40+5*2999+2]=0;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"tactic 0 rejected");}
    {auto b=bytes;b[40+5*5999+3]=254;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"tactic 254 rejected");}
    {auto b=bytes;b[40+5*5999+3]=200;ok(decode(b.data(),b.size(),binding,out),"tactic 200 accepted");out=keep;}
    {auto b=bytes;b[40+5*100]=0xFE;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"formation 0xFE rejected");}
    {auto b=bytes;b[40+5*100+4]=2;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"flags bit1 rejected");}
    {auto b=bytes;b[40+5*100+4]=0x81;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"flags bit7 rejected");}
    {auto b=bytes;b[bytes.size()-1]=2;ok(!decode(b.data(),b.size(),binding,out) && same(out,keep),"last flags byte invalid leaves out unchanged");}
    // merge_record keeps the flag, replaces formation and tactics
    {
        Record old;old.flags=kFlagRemember;old.formation=3;old.tactics[0]=1;
        const uint8_t t[3]={17,0xFF,6};
        Record merged=merge_record(old,9,t);
        ok(merged.flags==kFlagRemember && merged.formation==9 && merged.tactics[0]==17 && merged.tactics[1]==0xFF && merged.tactics[2]==6,"merge_record keeps flag, replaces sortie");
        Record fresh;merged=merge_record(fresh,0,t);
        ok(merged.flags==0 && merged.formation==0,"merge_record without flag stays flagless");
    }
    ok(decode(bytes.data(),bytes.size(),binding,back) && same(table,back),"decode still works after rejections");
    std::printf("{\"status\":\"%s\",\"checks\":%d,\"failures\":%d}\n",failures?"tactic_memory_fixture_failed":"tactic_memory_fixture_passed",checks,failures);
    return failures?1:0;
}
