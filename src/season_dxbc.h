// 4계절 D (user 2026-11-13 "셰이더 최소수정까지"): patch the game's two ground pixel shaders at creation time, in
// memory only (no KOEI-derived code is shipped). The one write of the G-buffer albedo
//     movc o0.xyz, a, b, c
// becomes
//     movc rN.xyz, a, b, c
//     sample rN+1.xyz, uv, t100, s10      (D = 1 - M, the Season Map multiplier, baked)
//     mad  rN.xyz, -rN.xyz, rN+1.xyz, rN.xyz
//     sample rN+1.xyz, uv, t101, s10      (A, the Season Map added colour, baked)
//     add  o0.xyz, rN.xyz, rN+1.xyz
// i.e. o0 = o0 * M + A. Both samples are copies of the shader's own `sample t10, s10` (the ground map multiplier, same
// map uv), so input and sampler are the game's. An unbound t100/t101 reads 0 = the original picture.
// Self-contained (fixture + DLL). Numbers are decimal on purpose (address-table check).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace season_dxbc {
// ---- DXBC checksum: MD5 with the container's own padding (bytes 20..end) ---------------------------------------
struct Md5 {
    uint32_t s[4]={1732584193u,4023233417u,2562383102u,271733878u};
    uint32_t k[64];
    Md5(){for(int i=0;i<64;++i)k[i]=static_cast<uint32_t>(std::floor(std::fabs(std::sin(static_cast<double>(i+1)))*4294967296.0));}
    static uint32_t rol(uint32_t x,int c){return (x<<c)|(x>>(32-c));}
    void block(const unsigned char *p) {
        static const int r[64]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                                4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
        uint32_t w[16];memcpy(w,p,64);
        uint32_t a=s[0],b=s[1],c=s[2],d=s[3];
        for(int i=0;i<64;++i) {
            uint32_t f;int g;
            if(i<16){f=(b&c)|(~b&d);g=i;}
            else if(i<32){f=(d&b)|(~d&c);g=(5*i+1)%16;}
            else if(i<48){f=b^c^d;g=(3*i+5)%16;}
            else {f=c^(b|~d);g=(7*i)%16;}
            uint32_t t=d;d=c;c=b;b=b+rol(a+f+k[i]+w[g],r[i]);a=t;
        }
        s[0]+=a;s[1]+=b;s[2]+=c;s[3]+=d;
    }
};
static void checksum(const unsigned char *data,size_t size,uint32_t out[4]) {
    data+=20;size-=20;
    Md5 m;const uint32_t bits=static_cast<uint32_t>(size*8);
    const size_t full=size&~static_cast<size_t>(63),left=size-full;
    for(size_t i=0;i<full;i+=64)m.block(data+i);
    unsigned char b[64]={};const uint32_t tail=(bits>>2)|1u;
    if(left>=56) {
        memcpy(b,data+full,left);b[left]=128;m.block(b);
        memset(b,0,64);memcpy(b,&bits,4);memcpy(b+60,&tail,4);m.block(b);
    } else {
        memcpy(b,&bits,4);memcpy(b+4,data+full,left);b[4+left]=128;memcpy(b+60,&tail,4);m.block(b);
    }
    memcpy(out,m.s,16);
}
// ---- SM5 tokens -----------------------------------------------------------------------------------------------
enum : uint32_t {OpAdd=0,OpMad=50,OpMovc=55,OpSample=69,OpCustom=53,OpDclResource=88,OpDclTemps=104};
enum : uint32_t {TypeTemp=0,TypeInput=1,TypeOutput=2,TypeImm32=4,TypeSampler=6,TypeResource=7};
static uint32_t opcode(uint32_t t){return t&2047u;}
static uint32_t op_type(uint32_t t){return (t>>12)&255u;}
static uint32_t dest_token(uint32_t type,uint32_t mask){return 2u|(mask<<4)|(type<<12)|(1u<<20);}
static uint32_t src_token(uint32_t type){return 2u|(1u<<2)|(228u<<4)|(type<<12)|(1u<<20);}   // .xyzw
// Walks the operands of one instruction. Calls visit(position of operand token, type, position of index0 or 0).
// Returns false on anything it does not understand (relative indexing, >1D index on a patched operand...).
template<class F> static bool operands(const uint32_t *t,size_t begin,size_t end,F visit) {
    size_t i=begin+1;
    while(t[i-1]&2147483648u){if(i>=end)return false;++i;}   // extended opcode tokens (first one sits on the opcode)
    while(i<end) {
        const size_t at=i;const uint32_t tok=t[i++];
        while((t[i-1]&2147483648u) && i<end)++i;   // extended operand tokens
        const uint32_t type=op_type(tok),dims=(tok>>20)&3u;
        if(type==TypeImm32){i+=((tok&3u)==2u)?4:1;visit(at,type,static_cast<size_t>(0));continue;}
        size_t index0=0;
        for(uint32_t d=0;d<dims;++d) {
            const uint32_t rep=(tok>>(22+3*d))&7u;
            if(rep!=0)return false;   // only immediate indices
            if(d==0)index0=i;
            ++i;
        }
        visit(at,type,index0);
    }
    return i==end;
}
// Returns true and fills out with the patched container; false = leave the shader alone.
static bool patch_ground(const unsigned char *code,size_t size,std::vector<unsigned char> &out,uint32_t d_slot=100,uint32_t a_slot=101) {
    if(!code || size<32 || memcmp(code,"DXBC",4))return false;
    uint32_t total=0,count=0;memcpy(&total,code+24,4);memcpy(&count,code+28,4);
    if(total!=size || count==0 || count>64 || 32+4*static_cast<size_t>(count)>size)return false;
    std::vector<uint32_t> offsets(count);memcpy(offsets.data(),code+32,4*count);
    int shex=-1;
    for(uint32_t c=0;c<count;++c) {
        if(offsets[c]+8>size)return false;
        if(!memcmp(code+offsets[c],"SHEX",4) || !memcmp(code+offsets[c],"SHDR",4))shex=static_cast<int>(c);
    }
    if(shex<0)return false;
    uint32_t chunk=0;memcpy(&chunk,code+offsets[shex]+4,4);
    if(offsets[shex]+8+static_cast<size_t>(chunk)>size || chunk<8 || chunk%4)return false;
    std::vector<uint32_t> t(chunk/4);memcpy(t.data(),code+offsets[shex]+8,chunk);
    if(t[1]!=t.size())return false;
    // Scan.
    size_t temps_at=0,res10_at=0,res10_len=0,movc_at=0,movc_dest=0,movc_count=0;std::vector<uint32_t> sample;size_t sample_dest=0,sample_res=0;
    for(size_t i=2;i<t.size();) {
        const uint32_t op=opcode(t[i]);size_t len=op==OpCustom?(i+1<t.size()?t[i+1]:0):((t[i]>>24)&127u);
        if(!len || i+len>t.size())return false;
        if(op==OpDclTemps)temps_at=i;
        else if(op==OpDclResource && len>=3 && op_type(t[i+1])==TypeResource && t[i+2]==10){res10_at=i;res10_len=len;}
        else if(op==OpMovc) {
            size_t dest=0;uint32_t dest_type=99;bool first=true;
            if(!operands(t.data(),i,i+len,[&](size_t at,uint32_t type,size_t index0){if(first){dest=at;dest_type=type;first=false;(void)index0;}}))return false;
            if(dest_type==TypeOutput && t[dest+1]==0 && ((t[dest]>>4)&15u)==7u){movc_at=i;movc_dest=dest;++movc_count;}
        } else if(op==OpSample && sample.empty()) {
            size_t dest=0,res=0,res_index=0;unsigned n=0;
            if(!operands(t.data(),i,i+len,[&](size_t at,uint32_t type,size_t index0){if(n==0)dest=at;if(type==TypeResource){res=at;res_index=index0;}++n;}))return false;
            if(res && res_index && t[res_index]==10 && op_type(t[dest])==TypeTemp && !(t[dest]&2147483648u)) {
                sample.assign(t.begin()+i,t.begin()+i+len);sample_dest=dest-i;sample_res=res_index-i;
            }
        }
        i+=len;
    }
    if(!temps_at || !res10_at || movc_count!=1 || sample.empty())return false;
    const uint32_t n=t[temps_at+1];
    if(n+2>4096)return false;
    // Rebuild the token stream.
    std::vector<uint32_t> o;o.reserve(t.size()+64);
    for(size_t i=0;i<t.size();++i) {
        if(i==temps_at+1){o.push_back(n+2);continue;}
        if(i==movc_dest){o.push_back((t[i]&~(255u<<12))|(TypeTemp<<12));continue;}
        if(i==movc_dest+1){o.push_back(n);continue;}
        o.push_back(t[i]);
        if(i+1==res10_at+res10_len)   // two more texture declarations, copies of t10's
            for(uint32_t slot:{d_slot,a_slot})for(size_t k=0;k<res10_len;++k)o.push_back(k==2?slot:t[res10_at+k]);
        if(movc_at && i+1==movc_at+((t[movc_at]>>24)&127u)) {
            auto emit_sample=[&](uint32_t slot){std::vector<uint32_t> s=sample;s[sample_dest+1]=n+1;s[sample_res]=slot;o.insert(o.end(),s.begin(),s.end());};
            emit_sample(d_slot);
            // mad rN.xyz, -rN.xyzw, rN+1.xyzw, rN.xyzw
            o.push_back(OpMad|(10u<<24));
            o.push_back(dest_token(TypeTemp,7));o.push_back(n);
            o.push_back(src_token(TypeTemp)|2147483648u);o.push_back(1u|(1u<<6));o.push_back(n);
            o.push_back(src_token(TypeTemp));o.push_back(n+1);
            o.push_back(src_token(TypeTemp));o.push_back(n);
            emit_sample(a_slot);
            // add o0.xyz, rN.xyzw, rN+1.xyzw
            o.push_back(OpAdd|(7u<<24));
            o.push_back(dest_token(TypeOutput,7));o.push_back(0);
            o.push_back(src_token(TypeTemp));o.push_back(n);
            o.push_back(src_token(TypeTemp));o.push_back(n+1);
        }
    }
    o[1]=static_cast<uint32_t>(o.size());
    // Rebuild the container with the new SHEX size.
    const size_t grow=(o.size()-t.size())*4;
    out.assign(code,code+size);
    std::vector<unsigned char> shex_bytes(o.size()*4);memcpy(shex_bytes.data(),o.data(),shex_bytes.size());
    const size_t body=offsets[shex]+8;
    out.erase(out.begin()+body,out.begin()+body+chunk);
    out.insert(out.begin()+body,shex_bytes.begin(),shex_bytes.end());
    const uint32_t new_chunk=static_cast<uint32_t>(o.size()*4);memcpy(out.data()+offsets[shex]+4,&new_chunk,4);
    for(uint32_t c=0;c<count;++c)if(offsets[c]>offsets[shex]){uint32_t v=offsets[c]+static_cast<uint32_t>(grow);memcpy(out.data()+32+4*c,&v,4);}
    const uint32_t new_total=static_cast<uint32_t>(out.size());memcpy(out.data()+24,&new_total,4);
    uint32_t sum[4];checksum(out.data(),out.size(),sum);memcpy(out.data()+4,sum,16);
    return true;
}
}
