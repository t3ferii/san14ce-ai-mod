#pragma once
// Save host (docs/MODULARIZATION_PLAN.md F-09, W4-5). The native archive (.s14) is written by the game; every mod sidecar next to it
// (<name>.<sha256>.meritbond / .warehouse / .terrain / .tactics) is written through this host: one atomic writer, one path rule, one
// plugin list in a fixed order, and a manifest <name>.<sha256>.sidecars published after the last sidecar. The manifest never changes
// what is loaded (every plugin still validates its own file against the archive hash); it only tells a missing, rejected or
// superseded sidecar apart from one that was never written. The original .s14 is already on disk when the manifest is written, so
// this detects and classifies, it does not make the pair atomic. Old saves without a manifest load exactly as before.
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <string.h>
#include <array>
#include <string>
#include <vector>
#include "sidecar_cache.h"
#include "core_module.h"
#pragma comment(lib,"bcrypt.lib")
namespace save_host {
using Binding=std::array<uint8_t,32>;
struct Job {std::string name;Binding binding;const void *source=nullptr;};   // source: the caller's own completed-job object (plugins that need more than name and binding)
enum State : uint8_t {kWritten=1,kOff=2,kEmpty=3,kFailed=4};
struct Written {uint8_t state=kFailed;uint32_t len=0;Binding hash={};};
struct Entry {char ext[12];uint16_t version;uint8_t state;uint32_t len;Binding hash;};
// Plugin hooks run on the game thread inside the native save/load host phases. write/restore keep the sidecar's own format.
struct SidecarPlugin {
    const char *name;                  // log id and manifest label
    const wchar_t *ext;                // file extension without dot
    unsigned version;                  // plugin format version recorded in the manifest
    const char *log_ok,*log_fail;      // write result events (null = silent); the sidecar's historical names
    bool (*active)();                  // false: not written (manifest state OFF); null = always
    void (*snapshot)();                // save phase 1 (null = none)
    bool (*write)(const Job &,Written &);   // save phase 3
    void (*begin_load)();              // load phase 1 boundary: caches and per-world state
    bool (*restore)(const Job *);      // load phase 3 (null = no matching save); true = a sidecar was restored
};
template<unsigned N=8> struct Plugins {
    const SidecarPlugin *rows[N]={};unsigned order[N]={};unsigned count=0;
    bool add(const SidecarPlugin *plugin,unsigned position) {
        if(count>=N || !plugin)return false;
        unsigned at=count;
        while(at>0 && order[at-1]>position) {rows[at]=rows[at-1];order[at]=order[at-1];--at;}
        rows[at]=plugin;order[at]=position;++count;return true;
    }
    void clear() {count=0;}
    unsigned order_hash() const {unsigned h=kModuleHashSeed;for(unsigned i=0;i<count;++i)h=module_name_hash(rows[i]->name,h);return h;}
};
// ---------------------------------------------------------------- fault injection (fixture builds only)
#ifdef SAVE_HOST_FIXTURE
static unsigned fail_step;   // 1 create temp, 2 write, 3 flush, 4 replace; 0 = none
static bool fails(unsigned step) {return fail_step==step;}
#else
static bool fails(unsigned) {return false;}
#endif
// ---------------------------------------------------------------- hashing and paths
static bool sha256(const void *bytes,size_t count,Binding &out) {
    if(!bytes || count>0xFFFFFFFFu)return false;
    struct Algorithm {
        BCRYPT_ALG_HANDLE handle=nullptr;
        Algorithm() {BCryptOpenAlgorithmProvider(&handle,BCRYPT_SHA256_ALGORITHM,nullptr,0);}
        ~Algorithm() {if(handle)BCryptCloseAlgorithmProvider(handle,0);}
    };
    static Algorithm provider;
    BCRYPT_HASH_HANDLE hash=nullptr;bool result=false;
    if(provider.handle && BCryptCreateHash(provider.handle,&hash,nullptr,0,nullptr,0,0)>=0) {
        result=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<void *>(bytes)),static_cast<ULONG>(count),0)>=0 &&
               BCryptFinishHash(hash,out.data(),static_cast<ULONG>(out.size()),0)>=0;
        BCryptDestroyHash(hash);
    }
    return result;
}
static bool valid_name(const std::string &name) {
    if(name.empty() || name.size()>64)return false;
    for(unsigned char c:name)if(c<0x20 || strchr("\\/:*?\"<>|",c))return false;
    return name!="." && name!="..";
}
// <folder of module>\AI_TacticsMod\data\merit, created on demand (the historical sidecar folder).
static bool directory(HMODULE module,wchar_t *out,size_t capacity,bool create) {
    wchar_t folder[MAX_PATH];
    DWORD n=GetModuleFileNameW(module,folder,MAX_PATH);
    if(!n || n>=MAX_PATH)return false;
    wchar_t *slash=wcsrchr(folder,L'\\');if(!slash)return false;*slash=0;
    wchar_t step[MAX_PATH];
    static const wchar_t *const parts[]={L"%s\\AI_TacticsMod",L"%s\\AI_TacticsMod\\data",L"%s\\AI_TacticsMod\\data\\merit"};
    for(const wchar_t *part:parts) {
        if(swprintf_s(step,part,folder)<0)return false;
        if(create && !CreateDirectoryW(step,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    }
    return wcscpy_s(out,capacity,step)==0;
}
static void hex(const Binding &binding,wchar_t out[65]) {for(unsigned i=0;i<32;++i)swprintf_s(out+i*2,3,L"%02x",binding[i]);}
// path(binding, ext): <directory>\<name>.<sha256 hex>.<ext>
static bool path_in(const wchar_t *dir,const Job &job,const wchar_t *ext,wchar_t *out,size_t capacity) {
    if(!valid_name(job.name))return false;
    wchar_t digest[65];hex(job.binding,digest);
    std::wstring name(job.name.begin(),job.name.end());
    return swprintf_s(out,capacity,L"%s\\%s.%s.%s",dir,name.c_str(),digest,ext)>=0;
}
#ifdef SAVE_HOST_FIXTURE
static unsigned path_calls;   // fixture builds: every sidecar path computation (proves a switched-off feature does no path IO)
#endif
static bool path(HMODULE module,const Job &job,const wchar_t *ext,wchar_t *out,size_t capacity,bool create) {
#ifdef SAVE_HOST_FIXTURE
    ++path_calls;
#endif
    wchar_t dir[MAX_PATH];
    return directory(module,dir,MAX_PATH,create) && path_in(dir,job,ext,out,capacity);
}
// ---------------------------------------------------------------- atomic write (the one copy of the former four)
static bool write_atomic(const wchar_t *target,const void *data,size_t size) {
    wchar_t temporary[MAX_PATH];
    if(swprintf_s(temporary,L"%s.tmp",target)<0)return false;
    HANDLE file=fails(1)?INVALID_HANDLE_VALUE:CreateFileW(temporary,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;
    bool ok=!fails(2) && WriteFile(file,data,static_cast<DWORD>(size),&written,nullptr)!=0 && written==size;
    ok=ok && !fails(3) && FlushFileBuffers(file)!=0;
    CloseHandle(file);
    if(ok)ok=!fails(4) && MoveFileExW(temporary,target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temporary);
    return ok;
}
static bool exists(const wchar_t *file) {return GetFileAttributesW(file)!=INVALID_FILE_ATTRIBUTES;}
// Sidecar store for a plugin: unchanged bytes already confirmed on disk are not rewritten (the cache rule of the former four copies).
static bool store(HMODULE module,const Job &job,const wchar_t *ext,const std::vector<uint8_t> &bytes,SidecarCache &cache,Written &out) {
    out=Written();
    wchar_t file[MAX_PATH];
    if(!path(module,job,ext,file,MAX_PATH,true))return false;
    out.len=static_cast<uint32_t>(bytes.size());
    if(!bytes.empty() && !sha256(bytes.data(),bytes.size(),out.hash))return false;
    out.state=bytes.empty()?kEmpty:kWritten;
    if(cache.matches(file,bytes) && exists(file))return true;
    if(!write_atomic(file,bytes.data(),bytes.size())) {out.state=kFailed;return false;}
    cache.remember(file,bytes);return true;
}
// ---------------------------------------------------------------- manifest
// "S14M" | u16 version 1 | archive sha256 | u16 count | count x {ext[12], u16 plugin_version, u8 state, u32 len, sha256} | crc32
static uint32_t crc32(const uint8_t *p,size_t size) {
    uint32_t v=~0u;
    for(size_t i=0;i<size;++i) {v^=p[i];for(unsigned bit=0;bit<8;++bit)v=(v>>1)^(0xEDB88320u&(0u-(v&1u)));}
    return ~v;
}
static void put(std::vector<uint8_t> &out,uint32_t v,unsigned bytes) {for(unsigned i=0;i<bytes;++i)out.push_back(static_cast<uint8_t>(v>>(i*8)));}
static uint32_t get(const uint8_t *p,unsigned bytes) {uint32_t v=0;for(unsigned i=0;i<bytes;++i)v|=static_cast<uint32_t>(p[i])<<(i*8);return v;}
static constexpr size_t kEntrySize=12+2+1+4+32;
static std::vector<uint8_t> encode_manifest(const Binding &archive,const std::vector<Entry> &entries) {
    std::vector<uint8_t> out={'S','1','4','M'};
    put(out,1,2);out.insert(out.end(),archive.begin(),archive.end());put(out,static_cast<uint32_t>(entries.size()),2);
    for(const Entry &e:entries) {
        out.insert(out.end(),e.ext,e.ext+12);put(out,e.version,2);out.push_back(e.state);put(out,e.len,4);
        out.insert(out.end(),e.hash.begin(),e.hash.end());
    }
    put(out,crc32(out.data(),out.size()),4);return out;
}
static bool decode_manifest(const std::vector<uint8_t> &bytes,const Binding &archive,std::vector<Entry> &out) {
    out.clear();
    if(bytes.size()<4+2+32+2+4 || memcmp(bytes.data(),"S14M",4) || get(bytes.data()+4,2)!=1)return false;
    if(get(bytes.data()+bytes.size()-4,4)!=crc32(bytes.data(),bytes.size()-4))return false;
    if(memcmp(bytes.data()+6,archive.data(),32))return false;
    unsigned count=get(bytes.data()+38,2);
    if(bytes.size()!=40+count*kEntrySize+4)return false;
    for(unsigned i=0;i<count;++i) {
        const uint8_t *p=bytes.data()+40+i*kEntrySize;Entry e={};
        memcpy(e.ext,p,12);e.version=static_cast<uint16_t>(get(p+12,2));e.state=p[14];e.len=get(p+15,4);memcpy(e.hash.data(),p+19,32);
        e.ext[11]=0;out.push_back(e);
    }
    return true;
}
static bool write_manifest(HMODULE module,const Job &job,const std::vector<Entry> &entries) {
    wchar_t file[MAX_PATH];
    if(!path(module,job,L"sidecars",file,MAX_PATH,true))return false;
    auto bytes=encode_manifest(job.binding,entries);
    return write_atomic(file,bytes.data(),bytes.size());
}
// 0 = no manifest (old save, or unreadable: behave as before), 1 = read
static bool read_manifest(HMODULE module,const Job &job,std::vector<Entry> &out) {
    out.clear();
    wchar_t file[MAX_PATH];
    if(!path(module,job,L"sidecars",file,MAX_PATH,false))return false;
    HANDLE handle=CreateFileW(file,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return false;
    std::vector<uint8_t> bytes;LARGE_INTEGER length={};
    if(GetFileSizeEx(handle,&length) && length.QuadPart>0 && length.QuadPart<=64*1024) {
        bytes.resize(static_cast<size_t>(length.QuadPart));DWORD got=0;
        if(!ReadFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr) || got!=bytes.size())bytes.clear();
    }
    CloseHandle(handle);
    return !bytes.empty() && decode_manifest(bytes,job.binding,out);
}
// ---------------------------------------------------------------- load classification
enum Load : uint8_t {kNoManifest=0,kRestored=1,kAbsentOff=2,kAbsentFailed=3,kMismatch=4,kManifestStale=5,kSaveFailed=6,kAbsentNew=7};
// Pure: what the manifest entry (null = none), the file on disk and the plugin's own validation say. The plugin has already
// replaced its state (restore or reset) from the file alone, so every outcome except kRestored is a reset it already did.
//   file_ok      the plugin accepted the file against the archive hash
//   file_exists  a file with the sidecar's name exists
//   file_hash    sha256 of the file bytes (only meaningful when file_exists)
inline Load classify(const Entry *entry,bool file_exists,bool file_ok,const Binding &file_hash) {
    if(!entry)return file_ok?kRestored:(file_exists?kMismatch:kAbsentNew);
    if(entry->state==kFailed)return file_ok?kManifestStale:kSaveFailed;
    if(entry->state==kOff)return file_ok?kManifestStale:(file_exists?kMismatch:kAbsentOff);
    if(!file_exists)return kAbsentFailed;
    if(!file_ok)return kMismatch;
    return (entry->state==kEmpty || file_hash==entry->hash)?kRestored:kManifestStale;
}
// ---------------------------------------------------------------- host phases (called from the native save/load host hooks)
using Log=void (*)(const char *,unsigned,unsigned,unsigned);
template<unsigned N> static void snapshot_all(const Plugins<N> &plugins) {
    for(unsigned i=0;i<plugins.count;++i)if(plugins.rows[i]->snapshot)plugins.rows[i]->snapshot();
}
template<unsigned N> static void begin_load_all(const Plugins<N> &plugins) {
    for(unsigned i=0;i<plugins.count;++i)if(plugins.rows[i]->begin_load)plugins.rows[i]->begin_load();
}
static void ext_label(const SidecarPlugin &p,char (&label)[12]) {
    memset(label,0,sizeof(label));
    for(unsigned i=0;i<11 && p.ext[i];++i)label[i]=static_cast<char>(p.ext[i]);
}
static void fill_entry(Entry &e,const SidecarPlugin &p,const Written &w) {
    memset(&e,0,sizeof(e));
    ext_label(p,e.ext);
    e.version=static_cast<uint16_t>(p.version);e.state=w.state;e.len=w.len;e.hash=w.hash;
}
// Save phase 3: every plugin in order (a failing or throwing plugin never stops the next one), the manifest last.
template<unsigned N> static void save_all(const Plugins<N> &plugins,HMODULE module,const Job &job,Log log) {
    std::vector<Entry> entries;
    for(unsigned i=0;i<plugins.count;++i) {
        const SidecarPlugin &p=*plugins.rows[i];
        Written w;
        if(p.active && !p.active())w.state=kOff;
        else {
            bool ok=false;
            try {ok=p.write(job,w);} catch(...) {ok=false;}
            if(!ok)w.state=kFailed;
            const char *event=ok?p.log_ok:p.log_fail;
            if(event && log)log(event,0,0,0);
        }
        Entry e;fill_entry(e,p,w);entries.push_back(e);
        if(log)log("sidecar_write",i,w.state,w.len);
    }
    bool published=write_manifest(module,job,entries);
    if(log)log("sidecar_manifest",published?1u:0u,static_cast<unsigned>(entries.size()),0);
}
// Files of the same save name written for another archive version: expected after a slot is overwritten, logged only, never touched.
inline unsigned count_old_versions(HMODULE module,const Job &job) {
    wchar_t dir[MAX_PATH],pattern[MAX_PATH];
    if(!directory(module,dir,MAX_PATH,false) || !valid_name(job.name))return 0;
    std::wstring name(job.name.begin(),job.name.end()),own;wchar_t digest[65];hex(job.binding,digest);own=digest;
    if(swprintf_s(pattern,L"%s\\%s.*",dir,name.c_str())<0)return 0;
    WIN32_FIND_DATAW found;HANDLE h=FindFirstFileW(pattern,&found);if(h==INVALID_HANDLE_VALUE)return 0;
    unsigned count=0;
    do {
        std::wstring file=found.cFileName;
        if(file.size()<name.size()+1+64+2 || file[name.size()]!=L'.' || file[name.size()+65]!=L'.')continue;
        std::wstring middle=file.substr(name.size()+1,64);bool hexdigits=true;
        for(wchar_t c:middle)if(!((c>=L'0' && c<=L'9') || (c>=L'a' && c<=L'f')))hexdigits=false;
        std::wstring ext=file.substr(name.size()+66);
        if(hexdigits && middle!=own && ext.find(L".tmp")==std::wstring::npos)++count;
    } while(FindNextFileW(h,&found));
    FindClose(h);return count;
}
#ifndef NO_RUNTIME_LOG
static bool file_digest(const wchar_t *file,Binding &out) {
    HANDLE h=CreateFileW(file,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length={};bool ok=false;
    if(GetFileSizeEx(h,&length) && length.QuadPart>0 && length.QuadPart<=64*1024*1024) {
        std::vector<uint8_t> bytes(static_cast<size_t>(length.QuadPart));DWORD got=0;
        ok=ReadFile(h,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr)!=0 && got==bytes.size() && sha256(bytes.data(),bytes.size(),out);
    }
    CloseHandle(h);return ok;
}
#endif
// Load phase 3: every plugin restores from its own file exactly as before; the manifest is only read to classify the outcome
// (the classification and its logs exist in diagnostic builds only).
template<unsigned N> static void restore_all(const Plugins<N> &plugins,HMODULE module,const Job *job,Log log) {
    std::vector<Entry> manifest;
    const bool have=job && read_manifest(module,*job,manifest);
    for(unsigned i=0;i<plugins.count;++i) {
        const SidecarPlugin &p=*plugins.rows[i];
        bool ok=false;
        try {ok=p.restore(job);} catch(...) {ok=false;try {p.restore(nullptr);} catch(...) {}}
#ifndef NO_RUNTIME_LOG
        if(job && log) {
            wchar_t file[MAX_PATH];
            const bool exists_now=path(module,*job,p.ext,file,MAX_PATH,false) && exists(file);
            Binding file_hash={};
            if(have && exists_now)file_digest(file,file_hash);   // only to tell a superseded file from the recorded one
            const Entry *entry=nullptr;char label[12];ext_label(p,label);
            if(have)for(const Entry &e:manifest)if(!strcmp(e.ext,label))entry=&e;
            const unsigned outcome=have?classify(entry,exists_now,ok,file_hash):(ok?kRestored:(exists_now?kMismatch:kAbsentNew));
            log("sidecar_load",i,outcome,have?1u:0u);
        }
#else
        (void)ok;
#endif
    }
#ifndef NO_RUNTIME_LOG
    if(job && log) {
        log("sidecar_manifest_load",have?1u:0u,static_cast<unsigned>(manifest.size()),0);
        if(unsigned old_versions=count_old_versions(module,*job))log("sidecar_old_versions",old_versions,0,0);
    }
#else
    (void)have;(void)log;
#endif
}
}
