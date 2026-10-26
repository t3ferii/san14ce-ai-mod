#pragma once
// Save-block codec of the original game (encoder 9321F0/932D40, decoder 9330A0).
// Evidence: REA_Analysis/save_20261018/README.md. Block = header 0x1A43574C, raw
// size, coded size, 256-byte rank->byte table, then an MSB-first bit stream of
// codes. A code has an n-bit prefix (n-1 ones and a zero) and an n-bit suffix:
// value = 2^n - 2 + suffix. value < 0x100: literal table[value]. Otherwise a
// copy of (next code + 3) bytes from distance value - 0x100 (byte by byte, so
// overlaps repeat). The original finds matches with a Patricia trie (window
// 0x2000, match 3..0x200, lazy evaluation); this encoder keeps those limits and
// the stream format but uses bounded hash chains. Pure: no game calls.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace save_codec {
static const unsigned kWindow=0x2000,kMinMatch=3,kMaxMatch=0x200,kHashBits=15,kChain=48;
struct Writer {
    uint8_t *out;size_t cap,pos;uint32_t acc;int bits;bool overflow;
    void put(uint32_t value,int count) {             // count <= 31
        while(count>0) {
            int take=8-bits<count?8-bits:count;count-=take;
            acc=(acc<<take)|((value>>count)&((1u<<take)-1));bits+=take;
            if(bits==8) {
                if(pos>=cap){overflow=true;return;}
                out[pos++]=static_cast<uint8_t>(acc);acc=0;bits=0;
            }
        }
    }
    void code(uint32_t value) {
        int n=1;
        while(n<31 && value>=(2u<<n)-2)++n;           // largest n with value >= 2^n - 2
        const uint32_t base=(1u<<n)-2;put(base,n);put(value-base,n);
    }
    void finish() {if(bits && !overflow)put(0,8-bits);}
};
struct Reader {
    const uint8_t *in;size_t size,pos;uint32_t cur;int left;bool bad;
    uint32_t bit() {
        if(!left){if(pos>=size){bad=true;return 0;}cur=in[pos++];left=8;}
        --left;return (cur>>left)&1;
    }
    uint32_t code() {
        int n=0;uint32_t prefix=0,b;
        do{b=bit();prefix=prefix*2+b;++n;}while(b && !bad && n<32);
        uint32_t suffix=0;for(int i=0;i<n && !bad;++i)suffix=suffix*2+bit();
        return prefix+suffix;
    }
};
// Decode the bit stream into out[raw]. table = rank->byte. False on malformed input.
static bool decode(const uint8_t *in,size_t size,const uint8_t table[256],uint8_t *out,size_t raw) {
    Reader r{in,size,0,0,0,false};size_t at=0;
    while(at<raw) {
        uint32_t v=r.code();if(r.bad)return false;
        if(v<0x100){out[at++]=table[v];continue;}
        size_t distance=v-0x100,length=static_cast<size_t>(r.code())+3;
        if(r.bad || distance==0 || distance>at || length>raw-at)return false;
        for(size_t i=0;i<length;++i,++at)out[at]=out[at-distance];
    }
    return true;
}
// Encode src with rank[byte] -> literal value. Returns stream bytes, 0 if it would exceed cap.
// work must hold (1<<kHashBits) + kWindow uint32 entries.
static size_t encode(const uint8_t *src,size_t n,const uint8_t rank[256],uint8_t *out,size_t cap,uint32_t *work) {
    uint32_t *head=work,*prev=work+(1u<<kHashBits);
    for(size_t i=0;i<(1u<<kHashBits);++i)head[i]=UINT32_MAX;
    Writer w{out,cap,0,0,0,false};
    auto hash=[&](size_t i)->uint32_t{return ((src[i]<<16|src[i+1]<<8|src[i+2])*2654435761u)>>(32-kHashBits);};
    auto insert=[&](size_t i){if(i+2<n){uint32_t h=hash(i);prev[i&(kWindow-1)]=head[h];head[h]=static_cast<uint32_t>(i);}};
    auto longest=[&](size_t i,size_t &best_pos)->size_t{
        if(i+kMinMatch>n)return 0;
        size_t limit=n-i<kMaxMatch?n-i:kMaxMatch,best=0;uint32_t cand=head[hash(i)];
        for(unsigned chain=0;cand!=UINT32_MAX && chain<kChain;++chain) {
            if(cand>=i || i-cand>kWindow)break;
            const uint8_t *a=src+i,*b=src+cand;
            if(b[best]==a[best]) {
                size_t len=0;while(len<limit && a[len]==b[len])++len;
                if(len>best){best=len;best_pos=cand;if(len==limit)break;}
            }
            uint32_t next=prev[cand&(kWindow-1)];
            if(next==UINT32_MAX || next>=cand)break;
            cand=next;
        }
        return best>=kMinMatch?best:0;
    };
    size_t i=0;
    while(i<n) {
        size_t pos=0,len=longest(i,pos);
        if(len) {
            size_t pos2=0,len2=0;
            if(len<kMaxMatch && i+1<n){insert(i);len2=longest(i+1,pos2);}
            if(len2>len) {                              // lazy: emit a literal, take the later match
                w.code(rank[src[i]]);++i;len=len2;pos=pos2;
            } else if(len<kMaxMatch && i+1<n) {
                // i already inserted
                w.code(static_cast<uint32_t>(i-pos)+0x100);w.code(static_cast<uint32_t>(len-kMinMatch));
                for(size_t k=1;k<len;++k)insert(i+k);
                i+=len;if(w.overflow)return 0;continue;
            }
            w.code(static_cast<uint32_t>(i-pos)+0x100);w.code(static_cast<uint32_t>(len-kMinMatch));
            for(size_t k=0;k<len;++k)insert(i+k);
            i+=len;
        } else {
            w.code(rank[src[i]]);insert(i);++i;
        }
        if(w.overflow)return 0;
    }
    w.finish();
    return w.overflow?0:w.pos;
}
// Same rank as 9332B0: counts, stable-by-index selection sort descending; table = rank->byte.
static void ranks(const uint8_t *src,size_t n,uint8_t rank[256],uint8_t table[256]) {
    uint32_t count[256]={};
    for(size_t i=0;i<n;++i)++count[src[i]];
    for(int i=0;i<256;++i)table[i]=static_cast<uint8_t>(i);
    for(int i=1;i<256;++i)for(int j=i;j<256;++j)
        if(static_cast<int32_t>(count[i-1])<static_cast<int32_t>(count[j])) {
            uint32_t c=count[i-1];count[i-1]=count[j];count[j]=c;
            uint8_t t=table[i-1];table[i-1]=table[j];table[j]=t;
        }
    for(int i=0;i<256;++i)rank[table[i]]=static_cast<uint8_t>(i);
}
}
