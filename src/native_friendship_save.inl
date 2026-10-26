/* Unpublished candidate bindings. Host callbacks run on the game thread;
 * only hashing native archive bytes runs on the native save/load worker.
 * No worker calls a gameplay getter, enumerates Person, or modifies relations. */
#include "friendship_save.h"
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")
using SaveHost=void(__fastcall *)(Obj);
using RemoteWrite=unsigned char(__fastcall *)(Obj,const char *,const void *,int);
using RemoteRead=int(__fastcall *)(Obj,const char *,void *,int);
static friendship::SaveCoordinator friendship_jobs;
static SRWLOCK friendship_job_lock=SRWLOCK_INIT;
static Obj friendship_save_host_object,friendship_load_host_object;
static uint64_t friendship_save_epoch,friendship_load_epoch;
static bool friendship_digest(const void *bytes,int count,friendship::Binding &out) {
    if(!bytes || count<=0 || count>64*1024*1024)return false;
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    bool result=false;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0) {
        if(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0) {
            result=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<void *>(bytes)),
                                 static_cast<ULONG>(count),0)>=0 &&
                   BCryptFinishHash(hash,out.data(),static_cast<ULONG>(out.size()),0)>=0;
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg,0);
    }
    return result;
}
static std::string friendship_archive_name(const char *name) {
    if(!name)return {};
    size_t length=strnlen_s(name,65);
    if(length==65)return {};
    std::string result(name,length);
    return friendship::SaveCoordinator::valid_name(result)?result:std::string{};
}
static std::string friendship_job_name(unsigned offset) {
    Obj value=image+offset;size_t length=*reinterpret_cast<size_t *>(value+0x10);
    size_t capacity=*reinterpret_cast<size_t *>(value+0x18);
    if(length==0 || length>64 || capacity<length)return {};
    const char *data=reinterpret_cast<const char *>(capacity<16?value:pointer(value,0));
    if(!data)return {};
    std::string name(data,length);
    return friendship::SaveCoordinator::valid_name(name)?name:std::string{};
}
[[maybe_unused]] static unsigned char __fastcall friendship_file_write(
    Obj remote,const char *name,const void *data,int count) {
    auto original=*reinterpret_cast<RemoteWrite *>(pointer(remote,0));
    std::string filename;uint64_t epoch=0;friendship::Binding hash={};bool digest=false;
    if(enabled)try {
        filename=friendship_archive_name(name);
        AcquireSRWLockShared(&friendship_job_lock);epoch=friendship_jobs.save_token(filename);
        ReleaseSRWLockShared(&friendship_job_lock);
        if(epoch)digest=friendship_digest(data,count,hash);
    } catch(...) {epoch=0;}
    unsigned char success=original(remote,name,data,count);
    if(epoch && digest) {
        AcquireSRWLockExclusive(&friendship_job_lock);
        friendship_jobs.observe_save(epoch,filename,hash,success!=0);
        ReleaseSRWLockExclusive(&friendship_job_lock);
    }
    return success;
}
[[maybe_unused]] static int __fastcall friendship_file_read(Obj remote,const char *name,void *data,int count) {
    auto original=*reinterpret_cast<RemoteRead *>(pointer(remote,0)+8);
    std::string filename;uint64_t epoch=0;
    if(enabled)try {
        filename=friendship_archive_name(name);
        AcquireSRWLockShared(&friendship_job_lock);epoch=friendship_jobs.load_token(filename);
        ReleaseSRWLockShared(&friendship_job_lock);
    } catch(...) {epoch=0;}
    int result=original(remote,name,data,count);
    friendship::Binding hash={};
    if(epoch && result==count && friendship_digest(data,result,hash)) {
        AcquireSRWLockExclusive(&friendship_job_lock);
        friendship_jobs.observe_load(epoch,filename,hash,true);
        ReleaseSRWLockExclusive(&friendship_job_lock);
    }
    return result;
}
static bool friendship_sidecar_path(const friendship::SaveCoordinator::Completed &job,
                                    wchar_t *out,size_t capacity,bool make_directory) {
    if(!friendship::SaveCoordinator::valid_name(job.name))return false;
    wchar_t folder[MAX_PATH];
    DWORD n=GetModuleFileNameW(self_module,folder,MAX_PATH);
    if(!n || n>=MAX_PATH)return false;
    wchar_t *slash=wcsrchr(folder,L'\\');if(!slash)return false;*slash=0;
    wchar_t directory[MAX_PATH];
    if(swprintf_s(directory,L"%s\\AI_TacticsMod",folder)<0)return false;
    if(make_directory && !CreateDirectoryW(directory,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    if(swprintf_s(directory,L"%s\\AI_TacticsMod\\data",folder)<0)return false;
    if(make_directory && !CreateDirectoryW(directory,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    if(swprintf_s(directory,L"%s\\AI_TacticsMod\\data\\friendship",folder)<0)return false;
    if(make_directory && !CreateDirectoryW(directory,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    wchar_t digest[65];
    for(unsigned i=0;i<32;++i)swprintf_s(digest+i*2,3,L"%02x",job.binding[i]);
    std::wstring filename(job.name.begin(),job.name.end());
    return swprintf_s(out,capacity,L"%s\\%s.%s.friends500",directory,filename.c_str(),digest)>=0;
}
static bool friendship_write_sidecar(const friendship::SaveCoordinator::Completed &job) {
    wchar_t path[MAX_PATH],temporary[MAX_PATH];
    if(!friendship_sidecar_path(job,path,MAX_PATH,true) ||
       swprintf_s(temporary,L"%s.tmp",path)<0)return false;
    auto bytes=job.store.encode(job.binding);
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;
    bool result=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=0 &&
                written==bytes.size() && FlushFileBuffers(file)!=0;
    CloseHandle(file);
    if(result)result=MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!result)DeleteFileW(temporary);return result;
}
static friendship::Native friendship_load_native(unsigned id,void *) {
    return friendship_native(table(0x158,id,6000));
}
static bool friendship_read_sidecar(const friendship::SaveCoordinator::Completed &job,
                                   friendship::Store &out) {
    wchar_t path[MAX_PATH];if(!friendship_sidecar_path(job,path,MAX_PATH,false))return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length={};DWORD read=0;bool result=false;
    try {
        if(GetFileSizeEx(file,&length) && length.QuadPart>=52 && length.QuadPart<=6024052) {
            std::vector<uint8_t> bytes(static_cast<size_t>(length.QuadPart));
            if(ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr) && read==bytes.size())
                result=out.decode(bytes.data(),bytes.size(),job.binding,&friendship_load_native,nullptr);
        }
    } catch(...) {CloseHandle(file);throw;}
    CloseHandle(file);return result;
}
[[maybe_unused]] static void __fastcall friendship_save_host(Obj host) {
    unsigned phase=dword(host,0x470);
    if(enabled && phase==1) {
        friendship::Store snapshot;std::string name;
        try {
            name=friendship_job_name(0x27D2A98);
            AcquireSRWLockExclusive(&friendship_lock);
            try {friendship_check_world();snapshot=expanded_friends;}
            catch(...) {ReleaseSRWLockExclusive(&friendship_lock);throw;}
            ReleaseSRWLockExclusive(&friendship_lock);
            AcquireSRWLockExclusive(&friendship_job_lock);
            try {friendship_save_epoch=friendship_jobs.begin_save(name,snapshot);friendship_save_host_object=host;}
            catch(...) {ReleaseSRWLockExclusive(&friendship_job_lock);throw;}
            ReleaseSRWLockExclusive(&friendship_job_lock);
        } catch(...) {friendship_save_epoch=0;log_event("friendship_save_begin_failed");}
    }
    fn<SaveHost>(0x50B670)(host);
    if(enabled && host==friendship_save_host_object && phase==2 && dword(host,0x470)==3) {
        try {
            friendship::SaveCoordinator::Completed job;bool ready;
            AcquireSRWLockExclusive(&friendship_job_lock);
            try {ready=friendship_jobs.finish_save(friendship_save_epoch,dword(image,0x27D2A20)==1,job);}
            catch(...) {ReleaseSRWLockExclusive(&friendship_job_lock);throw;}
            ReleaseSRWLockExclusive(&friendship_job_lock);friendship_save_epoch=0;
            if(ready) {bool saved=friendship_write_sidecar(job);log_event(saved?"friendship_save_written":"friendship_save_write_failed");}
        } catch(...) {log_event("friendship_save_write_failed");}
    }
}
[[maybe_unused]] static void __fastcall friendship_load_host(Obj host) {
    unsigned phase=dword(host,0x470);
    if(enabled && phase==1)try {
        std::string name=friendship_job_name(0x27D2A68);
        AcquireSRWLockExclusive(&friendship_job_lock);
        try {friendship_load_epoch=friendship_jobs.begin_load(name);friendship_load_host_object=host;}
        catch(...) {ReleaseSRWLockExclusive(&friendship_job_lock);throw;}
        ReleaseSRWLockExclusive(&friendship_job_lock);
    } catch(...) {friendship_load_epoch=0;friendship_load_host_object=host;log_event("friendship_load_begin_failed");}
    fn<SaveHost>(0x509580)(host);
    if(enabled && host==friendship_load_host_object && phase==2 && dword(host,0x470)==3 &&
       dword(image,0x27D2A50)!=0) {
        // Successful native load always replaces the extras, even if the
        // sidecar is absent/corrupt or the world uses the same address.
        friendship::Store replacement;bool restored=false;
        try {
            friendship::SaveCoordinator::Completed job;bool ready;
            AcquireSRWLockExclusive(&friendship_job_lock);
            try {ready=friendship_jobs.finish_load(friendship_load_epoch,true,job);}
            catch(...) {ReleaseSRWLockExclusive(&friendship_job_lock);throw;}
            ReleaseSRWLockExclusive(&friendship_job_lock);
            if(ready)restored=friendship_read_sidecar(job,replacement);
        } catch(...) {replacement.clear();}
        friendship_load_epoch=0;
        AcquireSRWLockExclusive(&friendship_lock);
        expanded_friends=std::move(replacement);friendship_world=world();
        ReleaseSRWLockExclusive(&friendship_lock);
        log_event(restored?"friendship_load_restored":"friendship_load_no_matching_sidecar");
    }
}
