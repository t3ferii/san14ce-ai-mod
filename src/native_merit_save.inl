/* Unpublished candidate bindings. Host callbacks run on the game thread;
 * only hashing native archive bytes runs on the native save/load worker.
 * No worker calls a gameplay getter, enumerates Person, or modifies relations. */
#include "merit_bond.h"
#include <bcrypt.h>
#include "sidecar_cache.h"
static SidecarCache merit_sidecar_cache;
#pragma comment(lib,"bcrypt.lib")
using SaveHost=void(__fastcall *)(Obj);
using RemoteWrite=unsigned char(__fastcall *)(Obj,const char *,const void *,int);
using RemoteRead=int(__fastcall *)(Obj,const char *,void *,int);
// AI-CONTRACT [save epochs]: Native save/load worker callbacks may hash archive bytes only. Relation changes and native person access stay on host callbacks; retain epoch identity, binding and stale-job rejection when editing sidecar flow.
static merit::SaveCoordinator merit_jobs;
static SRWLOCK merit_job_lock=SRWLOCK_INIT;
static Obj merit_save_host_object,merit_load_host_object;
static uint64_t merit_save_epoch,merit_load_epoch;
static bool merit_digest(const void *bytes,int count,merit::Binding &out) {
    if(!bytes || count<=0 || count>64*1024*1024)return false;
    struct Algorithm {
        BCRYPT_ALG_HANDLE handle=nullptr;
        Algorithm() {BCryptOpenAlgorithmProvider(&handle,BCRYPT_SHA256_ALGORITHM,nullptr,0);}
        ~Algorithm() {if(handle)BCryptCloseAlgorithmProvider(handle,0);}
    };
    static Algorithm provider;
    BCRYPT_ALG_HANDLE alg=provider.handle;BCRYPT_HASH_HANDLE hash=nullptr;
    bool result=false;
    if(alg) {
        if(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0) {
            result=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<void *>(bytes)),
                                 static_cast<ULONG>(count),0)>=0 &&
                   BCryptFinishHash(hash,out.data(),static_cast<ULONG>(out.size()),0)>=0;
            BCryptDestroyHash(hash);
        }
    }
    return result;
}
static std::string merit_archive_name(const char *name) {
    if(!name)return {};
    size_t length=strnlen_s(name,65);
    if(length==65)return {};
    std::string result(name,length);
    return merit::SaveCoordinator::valid_name(result)?result:std::string{};
}
static std::string merit_job_name(unsigned offset) {
    Obj value=image+offset;size_t length=*reinterpret_cast<size_t *>(value+0x10);
    size_t capacity=*reinterpret_cast<size_t *>(value+0x18);
    if(length==0 || length>64 || capacity<length)return {};
    const char *data=reinterpret_cast<const char *>(capacity<16?value:pointer(value,0));
    if(!data)return {};
    std::string name(data,length);
    return merit::SaveCoordinator::valid_name(name)?name:std::string{};
}
[[maybe_unused]] static unsigned char __fastcall merit_file_write(
    Obj remote,const char *name,const void *data,int count) {
    COST_SCOPE("merit_file_write");
    auto original=*reinterpret_cast<RemoteWrite *>(pointer(remote,0));
    std::string filename;uint64_t epoch=0;merit::Binding hash={};bool digest=false;
    if(enabled)try {
        filename=merit_archive_name(name);
        AcquireSRWLockShared(&merit_job_lock);epoch=merit_jobs.save_token(filename);
        ReleaseSRWLockShared(&merit_job_lock);
        if(epoch)digest=merit_digest(data,count,hash);
    } catch(...) {epoch=0;}
    unsigned char success=original(remote,name,data,count);
    if(epoch && digest) {
        AcquireSRWLockExclusive(&merit_job_lock);
        merit_jobs.observe_save(epoch,filename,hash,success!=0);
        ReleaseSRWLockExclusive(&merit_job_lock);
    }
    return success;
}
[[maybe_unused]] static int __fastcall merit_file_read(Obj remote,const char *name,void *data,int count) {
    COST_SCOPE("merit_file_read");
    auto original=*reinterpret_cast<RemoteRead *>(pointer(remote,0)+8);
    std::string filename;uint64_t epoch=0;
    if(enabled)try {
        filename=merit_archive_name(name);
        AcquireSRWLockShared(&merit_job_lock);epoch=merit_jobs.load_token(filename);
        ReleaseSRWLockShared(&merit_job_lock);
    } catch(...) {epoch=0;}
    int result=original(remote,name,data,count);
    merit::Binding hash={};
    if(epoch && result==count && merit_digest(data,result,hash)) {
        AcquireSRWLockExclusive(&merit_job_lock);
        merit_jobs.observe_load(epoch,filename,hash,true);
        ReleaseSRWLockExclusive(&merit_job_lock);
    }
    return result;
}
static bool merit_sidecar_path(const merit::SaveCoordinator::Completed &job,
                                    wchar_t *out,size_t capacity,bool make_directory) {
    if(!merit::SaveCoordinator::valid_name(job.name))return false;
    wchar_t folder[MAX_PATH];
    DWORD n=GetModuleFileNameW(self_module,folder,MAX_PATH);
    if(!n || n>=MAX_PATH)return false;
    wchar_t *slash=wcsrchr(folder,L'\\');if(!slash)return false;*slash=0;
    wchar_t directory[MAX_PATH];
    if(swprintf_s(directory,L"%s\\AI_TacticsMod",folder)<0)return false;
    if(make_directory && !CreateDirectoryW(directory,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    if(swprintf_s(directory,L"%s\\AI_TacticsMod\\data",folder)<0)return false;
    if(make_directory && !CreateDirectoryW(directory,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    if(swprintf_s(directory,L"%s\\AI_TacticsMod\\data\\merit",folder)<0)return false;
    if(make_directory && !CreateDirectoryW(directory,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    wchar_t digest[65];
    for(unsigned i=0;i<32;++i)swprintf_s(digest+i*2,3,L"%02x",job.binding[i]);
    std::wstring filename(job.name.begin(),job.name.end());
    return swprintf_s(out,capacity,L"%s\\%s.%s.meritbond",directory,filename.c_str(),digest)>=0;
}
static bool merit_write_sidecar(const merit::SaveCoordinator::Completed &job) {
    COST_SCOPE("merit_write_sidecar");
    wchar_t path[MAX_PATH],temporary[MAX_PATH];
    if(!merit_sidecar_path(job,path,MAX_PATH,true) ||
       swprintf_s(temporary,L"%s.tmp",path)<0)return false;
    auto bytes=job.store.encode(job.binding);
    if(merit_sidecar_cache.matches(path,bytes) && GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return true;
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;
    bool result=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=0 &&
                written==bytes.size() && FlushFileBuffers(file)!=0;
    CloseHandle(file);
    if(result)result=MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!result)DeleteFileW(temporary);
    else merit_sidecar_cache.remember(path,bytes);
    return result;
}
static bool merit_read_sidecar(const merit::SaveCoordinator::Completed &job,
                                   merit::Store &out) {
    COST_SCOPE("merit_read_sidecar");
    wchar_t path[MAX_PATH];if(!merit_sidecar_path(job,path,MAX_PATH,false))return false;
    std::vector<uint8_t> cached;
    if(merit_sidecar_cache.get(path,cached))return out.decode(cached.data(),cached.size(),job.binding);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length={};DWORD read=0;bool result=false;
    try {
        if(GetFileSizeEx(file,&length) && length.QuadPart>=52 && length.QuadPart<=static_cast<LONGLONG>(merit::Store::MaxEncodedSize)) {
            std::vector<uint8_t> bytes(static_cast<size_t>(length.QuadPart));
            if(ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr) && read==bytes.size())
                result=out.decode(bytes.data(),bytes.size(),job.binding);
            if(result)merit_sidecar_cache.remember(path,bytes);
        }
    } catch(...) {CloseHandle(file);throw;}
    CloseHandle(file);return result;
}
[[maybe_unused]] static void __fastcall merit_save_host(Obj host) {
    COST_SCOPE("merit_save_host");
    unsigned phase=dword(host,0x470);
    if(enabled && phase==1) {
#ifdef AWARD_WAREHOUSE
        award_snapshot();
#endif
        merit::Store snapshot;std::string name;
        try {
            name=merit_job_name(0x27D2A98);
            AcquireSRWLockExclusive(&merit_lock);
            try {merit_check_world();merit_bonds.prune(merit_context,nullptr,merit_remove_native_slot);snapshot=merit_bonds;}
            catch(...) {ReleaseSRWLockExclusive(&merit_lock);throw;}
            ReleaseSRWLockExclusive(&merit_lock);
            AcquireSRWLockExclusive(&merit_job_lock);
            try {merit_save_epoch=merit_jobs.begin_save(name,std::move(snapshot));merit_save_host_object=host;}
            catch(...) {ReleaseSRWLockExclusive(&merit_job_lock);throw;}
            ReleaseSRWLockExclusive(&merit_job_lock);
        } catch(...) {merit_save_epoch=0;log_event("merit_save_begin_failed");}
    }
    {COST_SCOPE("native_save_host");fn<SaveHost>(0x50B670)(host);}
    if(enabled && host==merit_save_host_object && phase==2 && dword(host,0x470)==3) {
        try {
            merit::SaveCoordinator::Completed job;bool ready;
            AcquireSRWLockExclusive(&merit_job_lock);
            try {ready=merit_jobs.finish_save(merit_save_epoch,dword(image,0x27D2A20)==1,job);}
            catch(...) {ReleaseSRWLockExclusive(&merit_job_lock);throw;}
            ReleaseSRWLockExclusive(&merit_job_lock);merit_save_epoch=0;
            if(ready) {bool saved=merit_write_sidecar(job);log_event(saved?"merit_save_written":"merit_save_write_failed");}
#ifdef AWARD_WAREHOUSE
            if(ready) {bool saved=award_write_sidecar(job);log_event(saved?"warehouse_save_written":"warehouse_save_write_failed");}
#endif
        } catch(...) {log_event("merit_save_write_failed");}
    }
}
[[maybe_unused]] static void __fastcall merit_load_host(Obj host) {
    COST_SCOPE("merit_load_host");
    unsigned phase=dword(host,0x470);
    if(enabled && phase==1)try {
        // Loading is an explicit boundary: respect missing/replaced sidecars
        // and never resurrect a relation from a previous save's RAM cache.
        merit_sidecar_cache.clear();
#ifdef AWARD_WAREHOUSE
        award_sidecar_cache.clear();
#endif
        std::string name=merit_job_name(0x27D2A68);
        AcquireSRWLockExclusive(&merit_job_lock);
        try {merit_load_epoch=merit_jobs.begin_load(name);merit_load_host_object=host;}
        catch(...) {ReleaseSRWLockExclusive(&merit_job_lock);throw;}
        ReleaseSRWLockExclusive(&merit_job_lock);
    } catch(...) {merit_load_epoch=0;merit_load_host_object=host;log_event("merit_load_begin_failed");}
    {COST_SCOPE("native_load_host");fn<SaveHost>(0x509580)(host);}
    if(enabled && host==merit_load_host_object && phase==2 && dword(host,0x470)==3 &&
       dword(image,0x27D2A50)!=0) {
        // Successful native load always replaces the extras, even if the
        // sidecar is absent/corrupt or the world uses the same address.
        merit::Store replacement;bool restored=false;
        try {
            merit::SaveCoordinator::Completed job;bool ready;
            AcquireSRWLockExclusive(&merit_job_lock);
            try {ready=merit_jobs.finish_load(merit_load_epoch,true,job);}
            catch(...) {ReleaseSRWLockExclusive(&merit_job_lock);throw;}
            ReleaseSRWLockExclusive(&merit_job_lock);
            if(ready)restored=merit_read_sidecar(job,replacement);
#ifdef AWARD_WAREHOUSE
            award_restore_sidecar(ready?&job:nullptr);
#endif
        } catch(...) {replacement.clear();
#ifdef AWARD_WAREHOUSE
            award_restore_sidecar(nullptr);
#endif
        }
        merit_load_epoch=0;
        AcquireSRWLockExclusive(&merit_lock);
        replacement.prune(merit_context,nullptr,merit_remove_native_slot);merit_bonds=std::move(replacement);merit_world=world();
        // Off: keep saved records unchanged for a later re-enable, add no friends.
        if(mod_features.merit)merit_bonds.each([](const merit::Bond &bond){merit_use_native_slot(bond.officer,bond.ruler);});
        ReleaseSRWLockExclusive(&merit_lock);
        log_event(restored?"merit_load_restored":"merit_load_no_matching_sidecar");
    }
}
