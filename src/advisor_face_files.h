#pragma once
// local224 (user 2026-11-17 "무장목록같은거 보면 아무리 목록이 길어도 초상화 그때그때 자연스럽게 불러오는데"): portrait heads read straight
// from the original archive 0002/ScreenLayout.rdb (+ .rdb.bin<bank>). No game memory, no game function: the worker thread in
// native_advisor_briefing.inl reads the files and calls these pure functions. Formats: San14Sim/docs/orig/data_formats_archives.md
// 1.1 (index _DRK0000 / IDRK0000 entries, locator "<offset>@<size>[#<bank>[&<sub>]]"), 1.2 (an entry body is 16 KB zlib chunks, each
// u32 length + zlib, ending with a 0-length chunk), 5.1 (sprite table: the G1T stored right before a table with face_NNNN_ctr holds
// face NNNN, on the cell's page), 5.4 (G1T: 512x512 BC3, one level, its pixels are the page's last 512*512 bytes).
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>
namespace face_files {
constexpr std::uint32_t kTypeSprite=0xf20de437u,kTypeG1T=0xafbec60cu;
// The head square {64,0,448,384} of the 512 face: one BC3 block per large pixel, 2x2 blocks per small pixel. RGBA, the large level
// then the small one (the overlay's mip 0 and 1: the 112 px detail square samples 96, the 44 px row square 48).
constexpr unsigned kLarge=96,kSmall=48;
constexpr size_t kPixelBytes=(kLarge*kLarge+kSmall*kSmall)*4;
constexpr size_t kEntryLimit=64u<<20;   // no face entry comes near this; a bigger size is a broken index
struct Entry {std::uint32_t ktid=0,type=0;std::uint64_t dsize=0,offset=0,size=0;std::uint32_t bank=0;int sub=-1;bool body=false;};
inline std::uint32_t u32(const std::uint8_t *p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
inline std::uint64_t u64(const std::uint8_t *p){std::uint64_t v;std::memcpy(&v,p,8);return v;}
inline bool hex_number(const char *&s,const char *end,std::uint64_t &v) {
    v=0;const char *start=s;
    while(s<end) {
        const char c=*s;unsigned d=0;
        if(c>='0' && c<='9')d=static_cast<unsigned>(c-'0');else if(c>='a' && c<='f')d=static_cast<unsigned>(c-'a'+10);else if(c>='A' && c<='F')d=static_cast<unsigned>(c-'A'+10);else break;
        if(v>>60)return false;
        v=v<<4|d;++s;
    }
    return s>start;
}
inline bool parse_locator(const char *s,size_t n,Entry &e) {
    const char *end=s+n;std::uint64_t v=0;
    if(!hex_number(s,end,e.offset) || s>=end || *s++!='@' || !hex_number(s,end,e.size))return false;
    e.bank=0;e.sub=-1;
    if(s<end && *s=='#') {
        ++s;if(!hex_number(s,end,v) || v>0xFFFF)return false;
        e.bank=static_cast<std::uint32_t>(v);
        if(s<end && *s=='&'){++s;if(!hex_number(s,end,v) || v>0xFFFF)return false;e.sub=static_cast<int>(v);}
    }
    return s==end;
}
// The whole index file; false when anything does not add up (the entry count, the end, a locator).
inline bool parse_index(const std::uint8_t *d,size_t n,std::vector<Entry> &out) {
    out.clear();
    if(n<0x20 || std::memcmp(d,"_DRK0000",8))return false;
    const std::uint32_t count=u32(d+0x10);size_t p=u32(d+8);
    while(p<n) {
        if(n-p<0x30 || std::memcmp(d+p,"IDRK0000",8))return false;
        const std::uint32_t size=u32(d+p+8);const std::uint64_t located=u64(d+p+0x10);
        if(size<0x30 || size>n-p || located>size-0x30)return false;
        Entry e;e.dsize=u64(d+p+0x18);e.ktid=u32(d+p+0x24);e.type=u32(d+p+0x28);
        if(located) {
            const char *loc=reinterpret_cast<const char *>(d+p+size-static_cast<size_t>(located));size_t len=static_cast<size_t>(located);
            while(len && !loc[len-1])--len;
            if(len){if(!parse_locator(loc,len,e))return false;e.body=true;}
        }
        out.push_back(e);
        p+=(static_cast<size_t>(size)+3u)&~static_cast<size_t>(3u);
    }
    return p==n && out.size()==count;
}
inline std::wstring bank_file(const std::wstring &name,std::uint32_t bank,int sub) {
    std::wstring s=name+L".rdb.bin";
    if(bank)s+=std::to_wstring(bank);
    if(sub>=0){s+=L"_";s+=std::to_wstring(sub);}
    return s;
}
// RFC 1951 inflate (stored, fixed and dynamic blocks), decoding a code bit by bit like zlib's reference decoder puff.
class Inflater {
public:
    Inflater(const std::uint8_t *in,size_t n,std::vector<std::uint8_t> &out,size_t limit):in_(in),n_(n),out_(out),limit_(limit){}
    bool run() {
        int last=0;
        do {
            last=bits(1);const int type=bits(2);
            if(err_)return false;
            const bool ok=type==0?stored():type==1?fixed():type==2?dynamic():false;
            if(!ok)return false;
        } while(!last);
        return true;
    }
    size_t used() const {return pos_;}   // bytes taken from the input; the unused bits of the last one are padding
private:
    struct Huffman {std::uint16_t count[16];std::uint16_t symbol[288];};
    const std::uint8_t *in_;size_t n_,pos_=0;std::uint32_t buf_=0;int cnt_=0;bool err_=false;
    std::vector<std::uint8_t> &out_;size_t limit_;
    int bits(int need) {
        std::uint32_t v=buf_;
        while(cnt_<need){if(pos_>=n_){err_=true;return 0;}v|=static_cast<std::uint32_t>(in_[pos_++])<<cnt_;cnt_+=8;}
        buf_=need<32?v>>need:0;cnt_-=need;
        return static_cast<int>(v&((1u<<need)-1u));
    }
    bool stored() {
        buf_=0;cnt_=0;
        if(n_-pos_<4)return false;
        const unsigned len=in_[pos_]|in_[pos_+1]<<8,nlen=in_[pos_+2]|in_[pos_+3]<<8;pos_+=4;
        if(len!=(~nlen&0xFFFFu) || n_-pos_<len || out_.size()+len>limit_)return false;
        out_.insert(out_.end(),in_+pos_,in_+pos_+len);pos_+=len;
        return true;
    }
    int decode(const Huffman &h) {
        int code=0,first=0,index=0;
        for(int len=1;len<16;++len) {
            code|=bits(1);if(err_)return -1;
            const int count=h.count[len];
            if(code-count<first)return h.symbol[index+(code-first)];
            index+=count;first+=count;first<<=1;code<<=1;
        }
        return -1;
    }
    // 0 complete, >0 incomplete, <0 over-subscribed
    static int build(Huffman &h,const std::uint16_t *length,int n) {
        std::memset(h.count,0,sizeof(h.count));
        for(int s=0;s<n;++s){if(length[s]>15)return -1;h.count[length[s]]++;}
        if(h.count[0]==n)return 0;
        int left=1;
        for(int len=1;len<16;++len){left<<=1;left-=h.count[len];if(left<0)return left;}
        std::uint16_t offs[16]{};
        for(int len=1;len<15;++len)offs[len+1]=static_cast<std::uint16_t>(offs[len]+h.count[len]);
        for(int s=0;s<n;++s)if(length[s])h.symbol[offs[length[s]]++]=static_cast<std::uint16_t>(s);
        return left;
    }
    bool codes(const Huffman &lencode,const Huffman &distcode) {
        static const std::uint16_t lbase[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
        static const std::uint8_t lext[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
        static const std::uint16_t dbase[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
        static const std::uint8_t dext[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
        for(;;) {
            int symbol=decode(lencode);
            if(symbol<0)return false;
            if(symbol<256){if(out_.size()>=limit_)return false;out_.push_back(static_cast<std::uint8_t>(symbol));continue;}
            if(symbol==256)return true;
            symbol-=257;if(symbol>=29)return false;
            const size_t len=lbase[symbol]+static_cast<size_t>(bits(lext[symbol]));
            const int ds=decode(distcode);if(ds<0 || ds>=30)return false;
            const size_t dist=dbase[ds]+static_cast<size_t>(bits(dext[ds]));
            if(err_ || dist>out_.size() || out_.size()+len>limit_)return false;
            const size_t from=out_.size()-dist;
            for(size_t i=0;i<len;++i){const std::uint8_t b=out_[from+i];out_.push_back(b);}
        }
    }
    bool fixed() {
        Huffman lencode,distcode;std::uint16_t lengths[288];int s=0;
        for(;s<144;++s)lengths[s]=8;
        for(;s<256;++s)lengths[s]=9;
        for(;s<280;++s)lengths[s]=7;
        for(;s<288;++s)lengths[s]=8;
        build(lencode,lengths,288);
        for(s=0;s<30;++s)lengths[s]=5;
        build(distcode,lengths,30);
        return codes(lencode,distcode);
    }
    bool dynamic() {
        static const std::uint8_t order[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
        const int nlen=bits(5)+257,ndist=bits(5)+1,ncode=bits(4)+4;
        if(err_ || nlen>286 || ndist>30)return false;
        std::uint16_t lengths[320]{};int index=0;
        for(;index<ncode;++index)lengths[order[index]]=static_cast<std::uint16_t>(bits(3));
        if(err_)return false;
        Huffman lencode,distcode;
        if(build(lencode,lengths,19)!=0)return false;
        index=0;
        while(index<nlen+ndist) {
            int symbol=decode(lencode);
            if(symbol<0)return false;
            if(symbol<16){lengths[index++]=static_cast<std::uint16_t>(symbol);continue;}
            std::uint16_t len=0;
            if(symbol==16){if(!index)return false;len=lengths[index-1];symbol=3+bits(2);}
            else if(symbol==17)symbol=3+bits(3);
            else symbol=11+bits(7);
            if(err_ || index+symbol>nlen+ndist)return false;
            while(symbol--)lengths[index++]=len;
        }
        if(!lengths[256])return false;
        int err=build(lencode,lengths,nlen);
        if(err<0 || (err>0 && nlen-lencode.count[0]!=1))return false;
        err=build(distcode,lengths+nlen,ndist);
        if(err<0 || (err>0 && ndist-distcode.count[0]!=1))return false;
        return codes(lencode,distcode);
    }
};
// One zlib stream appended to out (header, deflate data, Adler-32 checked).
inline bool inflate_zlib(const std::uint8_t *in,size_t n,std::vector<std::uint8_t> &out,size_t limit) {
    if(n<6)return false;
    const unsigned cmf=in[0],flg=in[1];
    if((cmf&0x0F)!=8 || (cmf>>4)>7 || ((cmf<<8)|flg)%31 || (flg&0x20))return false;
    const size_t start=out.size();
    Inflater inflater(in+2,n-2,out,limit);
    if(!inflater.run())return false;
    const size_t at=2+inflater.used();
    if(n-at<4)return false;
    std::uint32_t a=1,b=0;
    for(size_t i=start;i<out.size();++i){a=(a+out[i])%65521u;b=(b+a)%65521u;}
    const std::uint32_t stored=static_cast<std::uint32_t>(in[at])<<24|static_cast<std::uint32_t>(in[at+1])<<16|static_cast<std::uint32_t>(in[at+2])<<8|in[at+3];
    return ((b<<16)|a)==stored;
}
// An IDRK entry as stored in a bank file -> its body (the chunks after the head, ending with a 0-length chunk).
inline bool decompress_entry(const std::uint8_t *raw,size_t n,std::vector<std::uint8_t> &out) {
    out.clear();
    if(n<0x38 || std::memcmp(raw,"IDRK0000",8))return false;
    const std::uint64_t packed=u64(raw+0x10),plain=u64(raw+0x18);
    if(packed>n || plain>kEntryLimit)return false;
    out.reserve(static_cast<size_t>(plain));
    size_t q=n-static_cast<size_t>(packed);
    while(n-q>=4) {
        const std::uint32_t len=u32(raw+q);q+=4;
        if(!len)break;
        if(len>n-q || !inflate_zlib(raw+q,len,out,static_cast<size_t>(plain)))return false;
        q+=len;
    }
    return out.size()==plain;
}
// A 0005 portrait entry: "zp1\0", +4 plain size, +12 chunk count, +16 stored chunk sizes (each counting its own 4-byte length);
// the chunks start at 0x800, 0x80 aligned, each u32 (stored size - 4) + one zlib stream.
inline bool unpack_zp1(const std::uint8_t *d,size_t n,std::vector<std::uint8_t> &out) {
    out.clear();
    if(n<0x800 || std::memcmp(d,"zp1\0",4))return false;
    const std::uint32_t plain=u32(d+4),count=u32(d+12);
    if(plain>kEntryLimit || !count || 16+4ull*count>0x800)return false;
    out.reserve(plain);
    size_t p=0x800;
    for(std::uint32_t i=0;i<count;++i) {
        const std::uint32_t stored=u32(d+16+4*i);
        if(stored<4 || p>n || stored>n-p || u32(d+p)!=stored-4 || !inflate_zlib(d+p+4,stored-4,out,plain))return false;
        p=(p+stored+0x7F)&~static_cast<size_t>(0x7F);
    }
    return out.size()==plain;
}
// 0005/000..003.bin portraits (San14Sim data_formats_archives 1.4, traced 2026-11-17): each file holds one span of faces three
// times over (2048x1024, then the 512x512 busts, then 1024x1024). The bust of face f is entry span length + (f - span start).
// local233: 6215..6244 (003.bin, installed only under DLC\0005 with the DLC) too. The original checks DLC bits before it shows those
// (932DD0) and falls back to face 2000; an officer holds such a face only in a game where the DLC is there.
constexpr unsigned kPortraitFiles=4;
constexpr unsigned kPortraitSpans[kPortraitFiles][2]={{6000,100},{6100,15},{6115,100},{6215,30}};
struct Portrait {unsigned face=0;std::uint32_t offset=0,size=0;};
// The file's table (u32 count, count x (u32 offset, u32 size)) -> the busts of its span.
inline bool portrait_entries(const std::uint8_t *d,size_t n,unsigned file,std::vector<Portrait> &out) {
    out.clear();
    if(file>=kPortraitFiles || n<4)return false;
    const unsigned start=kPortraitSpans[file][0],span=kPortraitSpans[file][1];
    const std::uint32_t count=u32(d);
    if(count!=3*span || n<4+8ull*count)return false;
    for(unsigned i=0;i<span;++i){const std::uint8_t *e=d+4+8*(span+i);Portrait b;b.face=start+i;b.offset=u32(e);b.size=u32(e+4);out.push_back(b);}
    return true;
}
// local233 (user 2026-11-18 "손권 초상화는 왜 안나오지"): the age faces. 19 officers' face (+0xA6) is switched by the game year between
// their own face and a 2000-range one (272DE0 with the rule 2C5FA0, current EXE table 1A25E20 of {person, year, flag, face, age face};
// flag 0: the age face from that year on, flag 1: before it). The age face's cell is named after the officer's own face with a 'b'
// (8AE2D0 "face_%04db_ctr" of 2AB9F0's base face): 손권 (553) wears 2184 = face_0064b_ctr before 219. San14Sim data_formats_archives 5.1.
constexpr unsigned kAgeFaces[19][2]={{7,2197},{36,2196},{45,2195},{2,2194},{1,2193},{9,2192},{84,2191},{60,2190},{44,2189},{49,2188},
    {14,2187},{25,2186},{37,2185},{64,2184},{63,2183},{91,2182},{30,2181},{117,2180},{17,2179}};
inline unsigned age_face(unsigned face){for(const auto &f:kAgeFaces)if(f[0]==face)return f[1];return 0;}
// face_NNNN_ctr cells of a sprite table: (face number, page); face_NNNNb_ctr is NNNN's age face.
inline void sprite_faces(const std::uint8_t *d,size_t n,std::vector<std::pair<unsigned,unsigned>> &faces) {
    if(n<4)return;
    const std::uint32_t count=u32(d);
    if(count>4096 || 4+104ull*count>n)return;
    for(std::uint32_t i=0;i<count;++i) {
        const char *name=reinterpret_cast<const char *>(d+4+104*i);size_t len=0;
        while(len<64 && name[len])++len;
        if(len<10 || std::memcmp(name,"face_",5) || std::memcmp(name+len-4,"_ctr",4))continue;
        unsigned v=0;size_t k=5;
        for(;k<len-4 && name[k]>='0' && name[k]<='9' && v<100000;++k)v=v*10+static_cast<unsigned>(name[k]-'0');
        if(k+1==len-4 && name[k]=='b' && k>5){v=age_face(v);++k;if(!v)continue;}
        if(k!=len-4)continue;
        faces.emplace_back(v,u32(d+4+104*i+64));
    }
}
// The IDRK head of an entry stored in a bank file: its total size (+8), ktid (+0x24) and type (+0x28).
struct StoredHead {std::uint64_t total=0;std::uint32_t ktid=0,type=0;};
inline bool stored_head(const std::uint8_t *d,size_t n,StoredHead &h) {
    if(n<0x30 || std::memcmp(d,"IDRK0000",8))return false;
    h.total=u64(d+8);h.ktid=u32(d+0x24);h.type=u32(d+0x28);return h.total>=0x38;
}
// BC3 block -> 16 texels RGBA (BC3 colour is always the four-colour mode).
inline void bc3_block(const std::uint8_t *k,std::uint8_t px[16][4]) {
    const unsigned a0=k[0],a1=k[1];unsigned al[8]={a0,a1,0,0,0,0,0,0};
    if(a0>a1){for(unsigned i=1;i<7;++i)al[i+1]=((7-i)*a0+i*a1)/7;}
    else{for(unsigned i=1;i<5;++i)al[i+1]=((5-i)*a0+i*a1)/5;al[6]=0;al[7]=255;}
    std::uint64_t ab=0;for(int i=0;i<6;++i)ab|=static_cast<std::uint64_t>(k[2+i])<<(8*i);
    const unsigned c0=k[8]|k[9]<<8,c1=k[10]|k[11]<<8;const std::uint32_t bits=u32(k+12);
    unsigned r[4],g[4],b[4];
    r[0]=(c0>>11)*255/31;g[0]=(c0>>5&63)*255/63;b[0]=(c0&31)*255/31;
    r[1]=(c1>>11)*255/31;g[1]=(c1>>5&63)*255/63;b[1]=(c1&31)*255/31;
    r[2]=(2*r[0]+r[1])/3;g[2]=(2*g[0]+g[1])/3;b[2]=(2*b[0]+b[1])/3;
    r[3]=(r[0]+2*r[1])/3;g[3]=(g[0]+2*g[1])/3;b[3]=(b[0]+2*b[1])/3;
    for(unsigned i=0;i<16;++i) {
        const unsigned c=bits>>(2*i)&3u;
        px[i][0]=static_cast<std::uint8_t>(r[c]);px[i][1]=static_cast<std::uint8_t>(g[c]);px[i][2]=static_cast<std::uint8_t>(b[c]);
        px[i][3]=static_cast<std::uint8_t>(al[ab>>(3*i)&7u]);
    }
}
// Sums of n texels -> one pixel: alpha averaged, colour weighted by alpha (a fully clear pixel keeps the plain average).
struct Sums {std::uint32_t a=0,ar=0,ag=0,ab=0,r=0,g=0,b=0;};
inline void put_pixel(std::uint8_t *o,const Sums &s,unsigned n) {
    o[3]=static_cast<std::uint8_t>((s.a+n/2)/n);
    if(s.a){o[0]=static_cast<std::uint8_t>((s.ar+s.a/2)/s.a);o[1]=static_cast<std::uint8_t>((s.ag+s.a/2)/s.a);o[2]=static_cast<std::uint8_t>((s.ab+s.a/2)/s.a);}
    else{o[0]=static_cast<std::uint8_t>((s.r+n/2)/n);o[1]=static_cast<std::uint8_t>((s.g+n/2)/n);o[2]=static_cast<std::uint8_t>((s.b+n/2)/n);}
}
// A G1T file's page -> the head square, kLarge and kSmall levels (kPixelBytes). Only 512x512 BC3 pages are taken.
inline bool head_from_g1t(const std::uint8_t *g,size_t n,unsigned page,std::vector<std::uint8_t> &pixels) {
    if(n<0x14 || std::memcmp(g,"GT1G",4))return false;
    const std::uint32_t table=u32(g+0xC),count=u32(g+0x10);
    if(page>=count || table>n || 4ull*count>n-table)return false;
    const size_t head=static_cast<size_t>(table)+u32(g+table+4*page);
    const size_t end=page+1<count?static_cast<size_t>(table)+u32(g+table+4*(page+1)):n;
    constexpr size_t level0=512*512;
    if(head>n || end>n || end<head || end-head<level0+4)return false;
    if(g[head+1]!=0x5b || g[head+2]!=0x99)return false;
    const std::uint8_t *data=g+end-level0;
    pixels.assign(kPixelBytes,0);
    std::vector<Sums> blocks(kLarge*kLarge);std::uint8_t px[16][4];
    for(unsigned by=0;by<kLarge;++by)for(unsigned bx=0;bx<kLarge;++bx) {
        bc3_block(data+(static_cast<size_t>(by)*128+16+bx)*16,px);
        Sums &s=blocks[by*kLarge+bx];
        for(const auto &p:px){s.a+=p[3];s.ar+=p[0]*p[3];s.ag+=p[1]*p[3];s.ab+=p[2]*p[3];s.r+=p[0];s.g+=p[1];s.b+=p[2];}
        put_pixel(&pixels[(by*kLarge+bx)*4],s,16);
    }
    std::uint8_t *level1=pixels.data()+kLarge*kLarge*4;
    for(unsigned y=0;y<kSmall;++y)for(unsigned x=0;x<kSmall;++x) {
        Sums s;
        for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx) {
            const Sums &t=blocks[(y*2+dy)*kLarge+x*2+dx];
            s.a+=t.a;s.ar+=t.ar;s.ag+=t.ag;s.ab+=t.ab;s.r+=t.r;s.g+=t.g;s.b+=t.b;
        }
        put_pixel(level1+(y*kSmall+x)*4,s,64);
    }
    return true;
}
// local228 (user 2026-11-18 "원작의 무장일람을 켰을때 나오는 그 배경을 우리 군사보고 크기에 맞춰서 테두리랑 배경판만 그걸로", then "흐림없이
// 88%"): the original list window (layout 00_03_00_list_re, LCSK A3B4A82E) draws Blur and the 64x64 nine-slice cell
// 000_cmn_base_dialog_00 behind its rows. The cell is in the common UI sprite table fea5f28c; the G1T stored right before that table
// has it on page 11, 256x256 B8G8R8A8 (format 0x01, one level). San14Sim/docs/orig/ui_native.md 4.3 (무장 일람 창의 틀).
constexpr std::uint32_t kFrameTable=0xfea5f28cu;
constexpr char kFrameSprite[]="000_cmn_base_dialog_00";
constexpr size_t kChunkPlain=0x4000;   // every IDRK chunk but the last inflates to 16 KB (data_formats_archives 1.2)
struct Cell {unsigned page=0;float u0=0,v0=0,u1=0,v1=0;};
// The named cell of a sprite table (u32 count, count x {name[64], u32 page, float uv[8], u32 flag}): its page and uv box.
inline bool sprite_cell(const std::uint8_t *d,size_t n,const char *name,Cell &cell) {
    const size_t want=std::strlen(name);
    if(n<4 || !want || want>=64)return false;
    const std::uint32_t count=u32(d);
    if(count>65536 || 4+104ull*count>n)return false;
    for(std::uint32_t i=0;i<count;++i) {
        const std::uint8_t *e=d+4+104*static_cast<size_t>(i);
        if(std::memcmp(e,name,want) || e[want])continue;
        float uv[8];std::memcpy(uv,e+68,sizeof(uv));
        cell.page=u32(e+64);cell.u0=cell.u1=uv[0];cell.v0=cell.v1=uv[1];
        for(int k=1;k<4;++k){cell.u0=(std::min)(cell.u0,uv[2*k]);cell.u1=(std::max)(cell.u1,uv[2*k]);cell.v0=(std::min)(cell.v0,uv[2*k+1]);cell.v1=(std::max)(cell.v1,uv[2*k+1]);}
        return cell.u0>=0.0f && cell.v0>=0.0f && cell.u1<=1.0f && cell.v1<=1.0f && cell.u0<cell.u1 && cell.v0<cell.v1;   // false for NaN too
    }
    return false;
}
// Bytes [from, from+len) of an IDRK entry's body, inflating only the chunks that hold them (a 25 MB atlas G1T needs about 20 of 1,542).
inline bool entry_range(const std::uint8_t *raw,size_t n,std::uint64_t from,size_t len,std::vector<std::uint8_t> &out) {
    out.clear();
    if(n<0x38 || std::memcmp(raw,"IDRK0000",8))return false;
    const std::uint64_t packed=u64(raw+0x10),plain=u64(raw+0x18);
    if(packed>n || plain>(1ull<<32) || !len || from>plain || len>plain-from)return false;
    size_t q=n-static_cast<size_t>(packed);std::uint64_t at=0;std::vector<std::uint8_t> chunk;
    while(at<from+len) {
        if(n-q<4)return false;
        const std::uint32_t size=u32(raw+q);q+=4;
        if(!size || size>n-q)return false;
        if(at+kChunkPlain>from) {
            chunk.clear();
            if(!inflate_zlib(raw+q,size,chunk,kChunkPlain))return false;
            if(chunk.size()!=kChunkPlain && at+chunk.size()!=plain)return false;   // a short chunk only at the end
            const std::uint64_t lo=(std::max)(from,at),hi=(std::min)(from+len,at+chunk.size());
            if(hi>lo)out.insert(out.end(),chunk.begin()+static_cast<std::ptrdiff_t>(lo-at),chunk.begin()+static_cast<std::ptrdiff_t>(hi-at));
            at+=chunk.size();
        } else at+=kChunkPlain;
        q+=size;
    }
    return out.size()==len;
}
// A cell of a G1T page with one level (B8G8R8A8 0x01 or BC3 0x5b), read from the stored IDRK entry -> RGBA, w x h.
inline bool cell_from_g1t_entry(const std::uint8_t *raw,size_t n,const Cell &cell,std::vector<std::uint8_t> &rgba,unsigned &w,unsigned &h) {
    rgba.clear();w=h=0;
    std::vector<std::uint8_t> head,table,page,data;
    if(!entry_range(raw,n,0,0x14,head) || std::memcmp(head.data(),"GT1G",4))return false;
    const std::uint32_t at=u32(head.data()+0xC),count=u32(head.data()+0x10);
    if(cell.page>=count || count>4096 || !entry_range(raw,n,at,4ull*count,table))return false;
    const std::uint64_t plain=u64(raw+0x18),start=static_cast<std::uint64_t>(at)+u32(table.data()+4*cell.page);
    const std::uint64_t end=cell.page+1<count?static_cast<std::uint64_t>(at)+u32(table.data()+4*(cell.page+1)):plain;
    if(end<=start || end>plain || end-start<4 || !entry_range(raw,n,start,4,page))return false;
    const unsigned format=page[1],pw=1u<<(page[2]&15),ph=1u<<(page[2]>>4);
    if((page[0]>>4)!=1 || pw<4 || ph<4)return false;   // one level: its pixels are the page's last bytes
    const unsigned x0=static_cast<unsigned>(cell.u0*pw+.5f),x1=static_cast<unsigned>(cell.u1*pw+.5f),y0=static_cast<unsigned>(cell.v0*ph+.5f),y1=static_cast<unsigned>(cell.v1*ph+.5f);
    if(x1<=x0 || y1<=y0 || x1>pw || y1>ph || x1-x0>256 || y1-y0>256)return false;
    const unsigned cw=x1-x0,ch=y1-y0;
    if(format==0x01) {
        const std::uint64_t bytes=4ull*pw*ph;
        if(end-start<bytes+4 || !entry_range(raw,n,end-bytes+4ull*pw*y0,static_cast<size_t>(4ull*pw*ch),data))return false;
        rgba.resize(static_cast<size_t>(cw)*ch*4);
        for(unsigned y=0;y<ch;++y)for(unsigned x=0;x<cw;++x) {
            const std::uint8_t *p=data.data()+(static_cast<size_t>(y)*pw+x0+x)*4;std::uint8_t *o=rgba.data()+(static_cast<size_t>(y)*cw+x)*4;
            o[0]=p[2];o[1]=p[1];o[2]=p[0];o[3]=p[3];   // stored B, G, R, A
        }
    } else if(format==0x5b) {
        const std::uint64_t bytes=1ull*pw*ph;const unsigned row=pw/4*16,by0=y0/4,by1=(y1+3)/4;
        if(end-start<bytes+4 || !entry_range(raw,n,end-bytes+static_cast<std::uint64_t>(row)*by0,static_cast<size_t>(row)*(by1-by0),data))return false;
        rgba.resize(static_cast<size_t>(cw)*ch*4);std::uint8_t px[16][4];
        for(unsigned by=by0;by<by1;++by)for(unsigned bx=x0/4;bx<(x1+3)/4;++bx) {
            bc3_block(data.data()+static_cast<size_t>(by-by0)*row+bx*16,px);
            for(unsigned i=0;i<16;++i) {
                const unsigned x=bx*4+i%4,y=by*4+i/4;
                if(x>=x0 && x<x1 && y>=y0 && y<y1)std::memcpy(rgba.data()+(static_cast<size_t>(y-y0)*cw+(x-x0))*4,px[i],4);
            }
        }
    } else return false;
    w=cw;h=ch;return true;
}
// The frame cell from the parsed index. read(entry, raw) reads an entry as stored in its bank file. False when the table, the G1T
// stored right before it (same bank, at most 15 bytes of padding between) or the cell is missing; the report then draws its own frame.
template<class Read> inline bool frame_from_archive(const std::vector<Entry> &entries,Read read,std::vector<std::uint8_t> &rgba,unsigned &w,unsigned &h) {
    rgba.clear();w=h=0;
    const Entry *table=nullptr,*g1t=nullptr;
    for(const auto &e:entries)if(e.body && e.ktid==kFrameTable && e.type==kTypeSprite){table=&e;break;}
    if(!table)return false;
    for(const auto &e:entries)
        if(e.body && e.type==kTypeG1T && e.bank==table->bank && e.sub==table->sub && table->offset>=e.offset+e.size && table->offset-(e.offset+e.size)<16){g1t=&e;break;}
    if(!g1t)return false;
    std::vector<std::uint8_t> raw,plain;Cell cell;
    if(!read(*table,raw) || !decompress_entry(raw.data(),raw.size(),plain) || !sprite_cell(plain.data(),plain.size(),kFrameSprite,cell))return false;
    if(!read(*g1t,raw) || !cell_from_g1t_entry(raw.data(),raw.size(),cell,rgba,w,h))return false;
    return w>=16 && h>=16;
}
}   // namespace face_files
