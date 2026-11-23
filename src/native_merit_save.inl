#include "game_addresses.h"
/* Unpublished candidate bindings. Host callbacks run on the game thread;
 * only hashing native archive bytes runs on the native save/load worker.
 * No worker calls a gameplay getter, enumerates Person, or modifies relations. */
#include "merit_bond.h"
#include "save_host.h"   // sidecar writer, paths, plugin list and manifest (docs/MODULARIZATION_PLAN.md F-09)
static SidecarCache merit_sidecar_cache;
static save_host::Plugins<12> merit_sidecars;   // joined through the generated registry (9 rows since wall_age_save, docs/NEUTRAL_WALL_AGE.md A-2)
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
    return save_host::sha256(bytes,static_cast<size_t>(count),out);
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
// <name>.<sha256>.meritbond next to the other sidecars (save_host::path); merit's own file only.
static bool merit_sidecar_path(const merit::SaveCoordinator::Completed &job,
                                    wchar_t *out,size_t capacity,bool make_directory) {
    return save_host::path(self_module,save_host::Job{job.name,job.binding},L"meritbond",out,capacity,make_directory);
}
static bool merit_write_sidecar(const merit::SaveCoordinator::Completed &job,save_host::Written *report=nullptr) {
    COST_SCOPE("merit_write_sidecar");
    save_host::Written local;
    return save_host::store(self_module,save_host::Job{job.name,job.binding},L"meritbond",job.store.encode(job.binding),
        merit_sidecar_cache,report?*report:local);
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
static void merit_host_log(const char *kind,unsigned a,unsigned b,unsigned c) {log_event(kind,a,b,c);}
static merit::Store merit_load_replacement;   // merit plugin result of the load in progress (host moves it into merit_bonds)
static bool merit_load_restored;
[[maybe_unused]] static void __fastcall merit_save_host(Obj host) {
    COST_SCOPE("merit_save_host");
    unsigned phase=dword(host,0x470);
    if(enabled && phase==1) {
        save_host::snapshot_all(merit_sidecars);   // terrain, tactic memory, warehouse: registered plugins in position order
        merit::Store snapshot;std::string name;
        try {
            name=merit_job_name(GA(GA_27D5AE8));
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
    {COST_SCOPE("native_save_host");next_or<SaveHost>(next_merit_save_host,GA(GA_50DD30))(host);}
    if(enabled && host==merit_save_host_object && phase==2 && dword(host,0x470)==3) {
        try {
            merit::SaveCoordinator::Completed job;bool ready;
            AcquireSRWLockExclusive(&merit_job_lock);
            try {ready=merit_jobs.finish_save(merit_save_epoch,dword(image,GA(GA_27D5A34))==1,job);}
            catch(...) {ReleaseSRWLockExclusive(&merit_job_lock);throw;}
            ReleaseSRWLockExclusive(&merit_job_lock);merit_save_epoch=0;
            // every sidecar in plugin order, then the manifest last (save_host.h)
            if(ready)save_host::save_all(merit_sidecars,self_module,save_host::Job{job.name,job.binding,&job},&merit_host_log);
        } catch(...) {log_event("merit_save_write_failed");}
    }
}
// local166 start guard (user 2026-11-15 "둘다 고쳐", crash after a load: AV at 17FA38 in 50AE80 case 3, worker thread):
// the game-start state (vtable 13EA938 slot 5 = 50AE80) creates its loader holder 1BB6D48 in case 2 (4EA810 -> 17ED60)
// only while 27D5C30 == 0, but 27D5C30 is set once (4C7290) and never cleared, and the teardown 572840 -> 17ED90 frees the
// holder. A start after that teardown reaches case 3 with the holder null. When case 3 is about to run with no holder,
// create it the way case 2 does (17ED60(*(14DAE0()+0x210)): two plain allocations), on this same state thread, then let
// the original run. Any other phase, or a holder present, is the original call untouched.
static void *next_start_state;
static void __fastcall start_state_guard(Obj state,unsigned long long a,unsigned long long b,unsigned long long c) {
    if(state && dword(state,0x470)==3 && !*reinterpret_cast<void **>(image+GA(GA_1BB6D48))) {
        Obj owner=fn<Obj(__fastcall *)()>(GA(GA_14DAE0))();
        const unsigned long long arg=owner?*reinterpret_cast<unsigned long long *>(owner+0x210):0;   // review CR-10: no call on a null argument
        if(arg)fn<void(__fastcall *)(unsigned long long)>(GA(GA_17ED60))(arg);
        log_event("start_holder_recreated",*reinterpret_cast<void **>(image+GA(GA_1BB6D48))?1u:0u,dword(image,GA(GA_27D5C30)),owner?1u:0u);
    }
    next_or<void(__fastcall *)(Obj,unsigned long long,unsigned long long,unsigned long long)>(next_start_state,GA(GA_50AE80))(state,a,b,c);
}
[[maybe_unused]] static void __fastcall merit_load_host(Obj host) {
    COST_SCOPE("merit_load_host");
    unsigned phase=dword(host,0x470);
#ifndef NO_RUNTIME_LOG
    // local161 diagnostic (user 2026-11-15 crash after alt-tab): the game-start state 50AE80 case 3 read the global 1BB6D48
    // (created by 17EDD0 through 4EA810 only while 27D5C30 == 0, destroyed by 572840 while it is set) as null right after a
    // load. Each load-host phase change logs the phase, whether that global exists and the 27D5C30 flag. Reads only.
    {static unsigned last_phase=~0u;
     if(phase!=last_phase) {last_phase=phase;
        log_event("load_host_phase",phase,*reinterpret_cast<void **>(image+GA(GA_1BB6D48))?1u:0u,dword(image,GA(GA_27D5C30)));}}
#endif
    if(enabled && phase==1)try {
        // Loading is an explicit boundary: respect missing/replaced sidecars
        // and never resurrect a relation from a previous save's RAM cache.
        save_host::begin_load_all(merit_sidecars);   // terrain, tactic memory, merit, warehouse: caches and per-world state
        std::string name=merit_job_name(GA(GA_27D5AB8));
        AcquireSRWLockExclusive(&merit_job_lock);
        try {merit_load_epoch=merit_jobs.begin_load(name);merit_load_host_object=host;}
        catch(...) {ReleaseSRWLockExclusive(&merit_job_lock);throw;}
        ReleaseSRWLockExclusive(&merit_job_lock);
    } catch(...) {merit_load_epoch=0;merit_load_host_object=host;log_event("merit_load_begin_failed");}
    {COST_SCOPE("native_load_host");next_or<SaveHost>(next_merit_load_host,GA(GA_50BD20))(host);}
    if(enabled && host==merit_load_host_object && phase==2 && dword(host,0x470)==3 &&
       dword(image,GA(GA_27D5AA4))!=0) {
        // Successful native load always replaces the extras, even if the
        // sidecar is absent/corrupt or the world uses the same address.
        merit::Store replacement;bool restored=false;
        try {
            merit::SaveCoordinator::Completed job;bool ready;
            AcquireSRWLockExclusive(&merit_job_lock);
            try {ready=merit_jobs.finish_load(merit_load_epoch,true,job);}
            catch(...) {ReleaseSRWLockExclusive(&merit_job_lock);throw;}
            ReleaseSRWLockExclusive(&merit_job_lock);
            merit_load_replacement.clear();merit_load_restored=false;
            save_host::Job host_job{job.name,job.binding,&job};
            save_host::restore_all(merit_sidecars,self_module,ready?&host_job:nullptr,&merit_host_log);   // terrain, tactic memory, merit read, warehouse
            replacement=std::move(merit_load_replacement);restored=merit_load_restored;
        } catch(...) {replacement.clear();
            save_host::restore_all(merit_sidecars,self_module,nullptr,&merit_host_log);
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
// Sidecar plugin of the merit bond store itself (the host owner; its snapshot is taken inline under merit_lock in merit_save_host).
static const merit::SaveCoordinator::Completed &merit_completed(const save_host::Job &job) {return *static_cast<const merit::SaveCoordinator::Completed *>(job.source);}
static const save_host::SidecarPlugin merit_sidecar_plugin={"merit",L"meritbond",1,"merit_save_written","merit_save_write_failed",nullptr,nullptr,
    [](const save_host::Job &job,save_host::Written &out) {return merit_write_sidecar(merit_completed(job),&out);},
    []() {merit_sidecar_cache.clear();},
    [](const save_host::Job *job) {
        merit_load_replacement.clear();merit_load_restored=false;
        if(!job)return false;
        merit_load_restored=merit_read_sidecar(merit_completed(*job),merit_load_replacement);return merit_load_restored;
    }};
