// Faster save-block compression (기능설정.ini [features] fast_save, default on).
// The save writer 933BB0 builds the rank tables (9332B0) and calls the encoder
// 9321F0(rank, progress, wparam, allocator) once (call at 933C5B). That Patricia-
// trie encoder takes ~1.7 s per autosave on the logic thread. This replacement
// writes the same stream (save_codec.h) with bounded hash chains, then decodes it
// and compares with the input; on any difference or failure the original
// encoder runs instead. Saves stay readable by the unmodified game decoder.
#include "save_codec.h"
namespace save_codec_hook {
static const unsigned kEncoder=0x9321F0,kDecoder=0x9330A0,kCall=0x933C5B;
static const unsigned long long kEncoderHash=0xb7d3b29eb8ff66acull,kDecoderHash=0xe8860c2d623774f0ull;
static const unsigned kSrc=0x27DB9E8,kSrcSize=0x27DB9F0,kOut=0x27DBA00,kCursor=0x27DB9F8,kCoded=0x27DB9F4,kCap=0x27DBA08,kLimit=0x27DBA0C;
using Encoder=unsigned long long(__fastcall *)(uint8_t *,void *,unsigned,Obj);
static volatile LONG used,fallbacks;
static unsigned long long fingerprint(unsigned rva,unsigned size) {
    unsigned long long h=0xcbf29ce484222325ull;
    for(unsigned i=0;i<size;++i)h=(h^image[rva+i])*0x100000001b3ull;
    return h;
}
template<class T> static T &global(unsigned rva){return *reinterpret_cast<T *>(image+rva);}
static void release(Obj allocator,void *block){reinterpret_cast<void(__fastcall *)(Obj,void *)>(pointer(pointer(allocator,0),0x58))(allocator,block);}
// AI-CONTRACT [save compatibility]: Keep decode-and-compare before committing replacement output and retain original-encoder fallback. Match the native allocator and ownership for returned buffers; a faster stream must remain readable by the unmodified decoder.
static unsigned long long __fastcall encode(uint8_t *rank,void *notify,unsigned wparam,Obj allocator) {
    const uint8_t *src=global<uint8_t *>(kSrc);const unsigned n=global<unsigned>(kSrcSize);const unsigned cap=global<unsigned>(kCap);
    uint8_t *out=nullptr;uint32_t *work=nullptr;uint8_t *check=nullptr;size_t coded=0;bool ok=false;
    if(enabled && src && n && cap) {
        struct {unsigned kind,pad;unsigned long long zero;} desc{0x30,0,0};
        out=reinterpret_cast<uint8_t *>(reinterpret_cast<void *(__fastcall *)(Obj,unsigned long long,void *)>(pointer(pointer(allocator,0),0x28))(allocator,cap,&desc));
        work=static_cast<uint32_t *>(VirtualAlloc(nullptr,((1u<<save_codec::kHashBits)+save_codec::kWindow)*4,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        check=static_cast<uint8_t *>(VirtualAlloc(nullptr,n,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(out && work && check) {
            coded=save_codec::encode(src,n,rank,out,cap,work);
            uint8_t table[256];for(int b=0;b<256;++b)table[rank[b]]=static_cast<uint8_t>(b);
            ok=coded && save_codec::decode(out,coded,table,check,n) && !memcmp(check,src,n);
        }
        if(work)VirtualFree(work,0,MEM_RELEASE);
        if(check)VirtualFree(check,0,MEM_RELEASE);
    }
    if(!ok) {
        if(out)release(allocator,out);
        InterlockedIncrement(&fallbacks);
        return fn<Encoder>(kEncoder)(rank,notify,wparam,allocator);   // original sets its own globals
    }
    InterlockedIncrement(&used);
    // Same results as the original: buffer, cursor and coded size; 0 = smaller than input.
    global<uint8_t *>(kOut)=out;global<uint8_t *>(kCursor)=out+coded;global<int>(kCoded)=static_cast<int>(coded);
    if(static_cast<int>(coded)<global<int>(kLimit)) {
        // The original notifies its progress target once more on success (IAT 13408B0).
        if(notify)reinterpret_cast<void(__stdcall *)(void *,unsigned,unsigned long long,long long)>(pointer(image,0x13408B0))(notify,wparam,0,0);
        return 0;
    }
    return 1;
}
static CodeLink link;
static bool install() {
    if(image[kCall]!=0xE8 || kCall+5+*reinterpret_cast<int *>(image+kCall+1)!=kEncoder ||
       fingerprint(kEncoder,0x3B3)!=kEncoderHash || fingerprint(kDecoder,0x184)!=kDecoderHash) {log_event("fast_save_guard_rejected");return false;}
    Obj relay=allocate_near(image+kCall);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&encode));
    DWORD old;
    if(!prepare_link(link,kCall,0xE8,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,16)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!publish_link(link)) return false;
    log_event("fast_save_installed");return true;
}
}
