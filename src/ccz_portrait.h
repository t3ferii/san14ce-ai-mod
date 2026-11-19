#pragma once
// 2026-11-18 의용병 무리 깃발 초상화 (사용자: 조조전 그림이 꺼져 있으면 원작 3D 모형 + 부대 깃발 초상화에 조조전 병종 그림,
// docs/NEUTRAL_BANDITS.md 13절). 순수 자료·계산: 게임 호출·주소 없음.
// 원작 얼굴 그림 로더 8AD530(관리자, zp1 바이트, 그림 번호, p4)은 "zp1" 묶음을 풀어 GT1G0600 텍스처 한 장(BC3 = 형식 91, 밉 1)을
// 만든다. 얼굴 6000~6214는 크기 셋(얼굴 종류 9 2048x1024, 10 512x512, 11 1024x1024)이고, 머리 56바이트는 0005/002.bin 얼굴
// 6214의 세 항목과 같다(origlib.bin0005로 확인, San14Sim/docs/orig/data_formats_archives.md 1.4).
// 그림 한 점을 4의 배수 배율로 키워 4의 배수 위치에 놓으므로 BC3 블록 하나 = 그림 한 점 = 단색 블록(정확).
// 조각(30AB30 풀기 규칙, 2026-11-19 기계어 확인): 마지막이 아닌 조각은 크기 칸이 조각 크기(262144) 이상이면 압축 없는 원본으로
// 보고 그 크기만큼 그대로 복사한다(30AC72 mov r8d,esi -> memcpy). 저장 zlib은 262144를 넘으므로 local231은 zlib 머리까지 그림으로
// 복사돼 953AB0이 거절했다. 그래서 마지막이 아닌 조각 = 원본 262144바이트(크기 칸 262144), 마지막 조각 = u32 길이 + 저장 zlib.
#include <stddef.h>
#include <string.h>
namespace ccz_portrait {
constexpr unsigned kFirstFace=6187,kLastFace=6214;   // 0005/002.bin의 끝 28명 = 무리 종류 1~28 (ccz_band_kinds.h)
constexpr unsigned kHeader=56,kChunk=262144,kZpHead=2048,kAlign=128,kStored=65535,kAdler=65521;
struct Size {unsigned w,h;};
inline Size size_of(unsigned face_kind) {return face_kind==9?Size{2048,1024}:face_kind==11?Size{1024,1024}:Size{512,512};}
inline unsigned face_of(unsigned kind) {return kind>=1 && kind<=kLastFace-kFirstFace+1?kFirstFace+kind-1:0;}
inline unsigned kind_of_face(unsigned face) {return face>=kFirstFace && face<=kLastFace?face-kFirstFace+1:0;}
inline void put32(unsigned char *p,unsigned v) {
    p[0]=static_cast<unsigned char>(v);p[1]=static_cast<unsigned char>(v>>8);p[2]=static_cast<unsigned char>(v>>16);p[3]=static_cast<unsigned char>(v>>24);
}
inline unsigned log2u(unsigned v) {unsigned r=0;while((1u<<r)<v)++r;return r;}
inline unsigned texture_size(Size s) {return kHeader+s.w*s.h;}
inline unsigned chunk_count(unsigned raw) {return (raw+kChunk-1)/kChunk;}
inline unsigned piece_of(unsigned raw,unsigned i) {return raw-i*kChunk<kChunk?raw-i*kChunk:kChunk;}
inline unsigned stored_size(unsigned piece) {return 4+2+(piece+kStored-1)/kStored*5+piece+4;}   // u32 length, zlib head, blocks, adler
// bytes of chunk i in the entry: raw kChunk before the last chunk, stored zlib for the last
inline unsigned chunk_bytes(unsigned raw,unsigned i) {return i+1<chunk_count(raw)?kChunk:stored_size(piece_of(raw,i));}
inline size_t align(size_t v) {return (v+kAlign-1)/kAlign*kAlign;}
inline size_t zp1_size(Size s) {
    const unsigned raw=texture_size(s),n=chunk_count(raw);size_t at=kZpHead;
    for(unsigned i=0;i<n;++i)at=align(at+chunk_bytes(raw,i));
    return at;
}
// One picture: pw x ph palette indices (row stride), palette RGBA (R | G<<8 | B<<16 | A<<24); index 0 = clear.
struct Picture {const unsigned char *index;unsigned stride,pw,ph;const unsigned *palette;};
struct Place {unsigned scale,x0,y0,bx,by,bw,bh;};
// Bounding box of the drawn points, scaled by the largest multiple of 4 that fits 7/8 of the texture, centred on a 4-pixel grid.
inline bool place(const Picture &p,Size s,Place &r) {
    unsigned lx=p.pw,ly=p.ph,hx=0,hy=0;
    for(unsigned y=0;y<p.ph;++y)for(unsigned x=0;x<p.pw;++x)
        if(p.index[y*p.stride+x]){lx=x<lx?x:lx;ly=y<ly?y:ly;hx=x>hx?x:hx;hy=y>hy?y:hy;}
    if(lx>hx || ly>hy)return false;
    r.bx=lx;r.by=ly;r.bw=hx-lx+1;r.bh=hy-ly+1;
    const unsigned fx=s.w*7/8/r.bw,fy=s.h*7/8/r.bh;
    r.scale=(fx<fy?fx:fy)/4*4;if(r.scale<4)return false;
    r.x0=(s.w-r.bw*r.scale)/2/4*4;r.y0=(s.h-r.bh*r.scale)/2/4*4;
    return true;
}
// zp1 writer: raw bytes in order -> 256 KB chunks: raw before the last chunk, the last one stored zlib blocks (u32 length + 78 01 +
// blocks + Adler-32 big-endian).
struct Sink {
    unsigned char *out;size_t at;unsigned raw,chunk,n,piece,in_chunk,block_left,a,b;size_t chunk_at;bool ok;
    bool plain() const {return chunk+1<n;}
    void begin() {
        piece=piece_of(raw,chunk);chunk_at=at;in_chunk=0;block_left=0;a=1;b=0;
        if(!plain()){put32(out+at,stored_size(piece)-4);at+=4;out[at++]=120;out[at++]=1;}
    }
    void end() {
        ok=ok && at-chunk_at==chunk_bytes(raw,chunk);
        put32(out+16+chunk*4,static_cast<unsigned>(at-chunk_at));
        at=align(at);
        if(++chunk<n)begin();
    }
    void put(unsigned char c) {
        if(plain()) {out[at++]=c;if(++in_chunk==piece)end();return;}
        if(!block_left) {
            const unsigned len=piece-in_chunk<kStored?piece-in_chunk:kStored;
            out[at++]=in_chunk+len==piece?1:0;
            out[at++]=static_cast<unsigned char>(len);out[at++]=static_cast<unsigned char>(len>>8);
            out[at++]=static_cast<unsigned char>(~len);out[at++]=static_cast<unsigned char>((~len)>>8);
            block_left=len;
        }
        out[at++]=c;a=(a+c)%kAdler;b=(b+a)%kAdler;++in_chunk;--block_left;
        if(in_chunk==piece) {
            const unsigned adler=(b<<16)|a;
            out[at++]=static_cast<unsigned char>(adler>>24);out[at++]=static_cast<unsigned char>(adler>>16);
            out[at++]=static_cast<unsigned char>(adler>>8);out[at++]=static_cast<unsigned char>(adler);
            end();
        }
    }
    void put_n(const unsigned char *p,unsigned count) {for(unsigned i=0;i<count;++i)put(p[i]);}
};
// GT1G0600 head for one BC3 texture, one mip (the 56 bytes of the original faces; byte 0x26 = log2 w | log2 h << 4).
inline void header(unsigned char (&h)[kHeader],Size s) {
    static const unsigned char tail[20]={16,91,0,0,0,16,33,16,12,0,0,0,0,0,0,0,0,0,0,1};
    memset(h,0,sizeof(h));memcpy(h,"GT1G0600",8);
    put32(h+8,texture_size(s));put32(h+12,32);put32(h+16,1);put32(h+20,10);put32(h+32,4);
    memcpy(h+36,tail,sizeof(tail));h[38]=static_cast<unsigned char>(log2u(s.w)|(log2u(s.h)<<4));
}
// Whole zp1 entry into out (cap >= zp1_size(s)). False: empty picture, too small a texture or a short buffer.
inline bool build(const Picture &p,Size s,unsigned char *out,size_t cap) {
    Place pl{};const size_t need=zp1_size(s);
    if(!out || cap<need || s.w%4 || s.h%4 || !place(p,s,pl))return false;
    memset(out,0,need);
    const unsigned raw=texture_size(s);
    memcpy(out,"zp1",4);put32(out+4,raw);put32(out+8,kChunk);
    Sink k{out,kZpHead,raw,0,chunk_count(raw),0,0,0,1,0,0,true};put32(out+12,k.n);
    k.begin();
    unsigned char h[kHeader];header(h,s);k.put_n(h,kHeader);
    unsigned char clear[16]={};
    for(unsigned y=0;y<s.h;y+=4)for(unsigned x=0;x<s.w;x+=4) {
        if(x<pl.x0 || y<pl.y0 || x>=pl.x0+pl.bw*pl.scale || y>=pl.y0+pl.bh*pl.scale){k.put_n(clear,16);continue;}
        const unsigned sx=pl.bx+(x-pl.x0)/pl.scale,sy=pl.by+(y-pl.y0)/pl.scale;
        const unsigned char i=p.index[sy*p.stride+sx];
        if(!i){k.put_n(clear,16);continue;}
        const unsigned c=p.palette[i];
        const unsigned r=c&255,g=(c>>8)&255,bl=(c>>16)&255;
        const unsigned v=((r>>3)<<11)|((g>>2)<<5)|(bl>>3);
        const unsigned char block[16]={255,255,0,0,0,0,0,0,static_cast<unsigned char>(v),static_cast<unsigned char>(v>>8),
            static_cast<unsigned char>(v),static_cast<unsigned char>(v>>8),0,0,0,0};
        k.put_n(block,16);
    }
    return k.ok && k.chunk==k.n && k.at==need;
}
}
