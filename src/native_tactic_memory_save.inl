#include "tactic_memory_store.h"
#include "sidecar_cache.h"
#include "save_host.h"
#include <memory>
/* Per-save sortie memory sidecar (docs/TACTIC_PRIORITY.md section 5). Same structure as native_awards_save.inl:
 * live table under tactic_memory_lock, a snapshot taken on the game thread at save phase 1, written atomically by the save host. */
static SRWLOCK tactic_memory_lock=SRWLOCK_INIT;
static tactic_memory::Table tactic_memory_live,tactic_memory_saved;
static Obj tactic_memory_world;
static SidecarCache tactic_memory_cache;
// Caller holds tactic_memory_lock. A new world (new game or load that restored nothing) never inherits another save's records.
static void tactic_memory_check_world() {
    Obj current=world();
    if(current && current!=tactic_memory_world) {
        bool had=tactic_memory_world!=nullptr;
        tactic_memory_live.fill(tactic_memory::Record());tactic_memory_world=current;
        if(had)log_event("tactic_memory_world_reset");
    }
}
[[maybe_unused]] static void tactic_memory_record(unsigned person,unsigned formation,const unsigned *tactics) {
    if(!tactics || person>=tactic_memory::MaxPeople || formation>tactic_memory::MaxFormation)return;
    uint8_t clean[3];
    for(unsigned k=0;k<3;++k)clean[k]=tactics[k]>=1 && tactics[k]<=tactic_memory::MaxTactic?static_cast<uint8_t>(tactics[k]):0xFF;
    AcquireSRWLockExclusive(&tactic_memory_lock);
    tactic_memory_check_world();
    tactic_memory_live[person]=tactic_memory::merge_record(tactic_memory_live[person],static_cast<uint8_t>(formation),clean);   // keeps the remember flag
    ReleaseSRWLockExclusive(&tactic_memory_lock);
}
// A remembered sortie exists (formation recorded); a flag-only record is not a memory.
[[maybe_unused]] static bool tactic_memory_lookup(unsigned person,tactic_memory::Record &out) {
    if(person>=tactic_memory::MaxPeople)return false;
    bool found=false;
    AcquireSRWLockExclusive(&tactic_memory_lock);
    tactic_memory_check_world();
    if(tactic_memory_live[person].formation!=0xFF) {out=tactic_memory_live[person];found=true;}
    ReleaseSRWLockExclusive(&tactic_memory_lock);return found;
}
// Per-officer 「출진 기억」 switch (bit0 of the record flags).
[[maybe_unused]] static bool tactic_memory_flag(unsigned person) {
    if(person>=tactic_memory::MaxPeople)return false;
    AcquireSRWLockExclusive(&tactic_memory_lock);
    tactic_memory_check_world();
    const bool on=(tactic_memory_live[person].flags&tactic_memory::kFlagRemember)!=0;
    ReleaseSRWLockExclusive(&tactic_memory_lock);return on;
}
[[maybe_unused]] static void tactic_memory_set_flag(unsigned person,bool on) {
    if(person>=tactic_memory::MaxPeople)return;
    AcquireSRWLockExclusive(&tactic_memory_lock);
    tactic_memory_check_world();
    uint8_t &flags=tactic_memory_live[person].flags;
    flags=on?static_cast<uint8_t>(flags|tactic_memory::kFlagRemember):static_cast<uint8_t>(flags&~tactic_memory::kFlagRemember);
    ReleaseSRWLockExclusive(&tactic_memory_lock);
}
[[maybe_unused]] static void tactic_memory_snapshot() {
    AcquireSRWLockExclusive(&tactic_memory_lock);
    tactic_memory_check_world();tactic_memory_saved=tactic_memory_live;
    ReleaseSRWLockExclusive(&tactic_memory_lock);
}
static bool tactic_memory_path(const merit::SaveCoordinator::Completed &job,wchar_t *path,bool create) {
    return save_host::path(self_module,save_host::Job{job.name,job.binding},L"tactics",path,MAX_PATH,create);
}
[[maybe_unused]] static bool tactic_memory_write_sidecar(const merit::SaveCoordinator::Completed &job,save_host::Written *report=nullptr) {
    save_host::Written local;
    return save_host::store(self_module,save_host::Job{job.name,job.binding},L"tactics",tactic_memory::encode(tactic_memory_saved,job.binding),
        tactic_memory_cache,report?*report:local);
}
// Always replaces the live table after a successful native load: a missing/rejected sidecar means empty memory.
[[maybe_unused]] static bool tactic_memory_restore_sidecar(const merit::SaveCoordinator::Completed *job) {
    auto table=std::make_unique<tactic_memory::Table>();unsigned status=0,count=0,version=2;bool exists=false;
    if(job) {
        wchar_t path[MAX_PATH];
        if(tactic_memory_path(*job,path,false)) {
            HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file!=INVALID_HANDLE_VALUE) {
                exists=true;LARGE_INTEGER length={};DWORD got=0;
                if(!GetFileSizeEx(file,&length) || (length.QuadPart!=static_cast<LONGLONG>(tactic_memory::EncodedSize) && length.QuadPart!=static_cast<LONGLONG>(tactic_memory::EncodedSizeV1)))status=1;
                else {
                    std::vector<uint8_t> bytes(static_cast<size_t>(length.QuadPart));
                    if(!ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr) || got!=bytes.size())status=5;
                    else if(memcmp(bytes.data(),"TPRM",4) || bytes[4]!=(bytes.size()==tactic_memory::EncodedSize?2:1) || bytes[5] || bytes[6] || bytes[7])status=2;   // size and magic version disagree
                    else if(memcmp(bytes.data()+8,job->binding.data(),32))status=3;
                    else if(!tactic_memory::decode(bytes.data(),bytes.size(),job->binding,*table,&version))status=4;
                }
                CloseHandle(file);
            }
        }
    }
    if(status)table->fill(tactic_memory::Record());
    else for(const auto &r:*table)if(!tactic_memory::empty(r))++count;
    AcquireSRWLockExclusive(&tactic_memory_lock);
    tactic_memory_live=*table;tactic_memory_world=world();
    ReleaseSRWLockExclusive(&tactic_memory_lock);
    if(status)log_event("tactic_memory_load_rejected",0,status);
    else if(exists && count) {
        log_event("tactic_memory_load_restored",version,count);
        if(version==1)log_event("tactic_memory_migrated",1,count);   // read as v1, the next save writes v2 (flags 0)
    }
    else log_event(job?"tactic_memory_load_none":"tactic_memory_unavailable");
    return !status && exists;
}
// Sidecar plugin of the sortie memory (.tactics): only while 전법 우선순위 is on at save time, always restored.
static const save_host::SidecarPlugin tactic_memory_sidecar_plugin={"tactics",L"tactics",2,"tactic_memory_save_written","tactic_memory_save_write_failed",
    []() {return mod_features.tactic_priority;},
    []() {if(mod_features.tactic_priority)tactic_memory_snapshot();},
    [](const save_host::Job &job,save_host::Written &out) {return tactic_memory_write_sidecar(*static_cast<const merit::SaveCoordinator::Completed *>(job.source),&out);},
    []() {tactic_memory_cache.clear();},
    [](const save_host::Job *job) {return tactic_memory_restore_sidecar(job?static_cast<const merit::SaveCoordinator::Completed *>(job->source):nullptr);}};
