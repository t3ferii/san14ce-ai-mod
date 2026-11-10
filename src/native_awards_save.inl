#include "sidecar_cache.h"
#include "save_host.h"
static awards::Inventory award_save_snapshot;
static SidecarCache award_sidecar_cache;
static void award_snapshot() {award_synchronize();award_save_snapshot=award_storage;}
static bool award_save_path(const merit::SaveCoordinator::Completed &job,wchar_t *path,bool create) { COST_SCOPE("awards_save:award_save_path");
    return save_host::path(self_module,save_host::Job{job.name,job.binding},L"warehouse",path,MAX_PATH,create);
}
static bool award_write_sidecar(const merit::SaveCoordinator::Completed &job,save_host::Written *report=nullptr) { COST_SCOPE("awards_save:award_write_sidecar");
    save_host::Written local;
    return save_host::store(self_module,save_host::Job{job.name,job.binding},L"warehouse",awards::encode(award_save_snapshot,job.binding),
        award_sidecar_cache,report?*report:local);
}
static bool award_restore_sidecar(const merit::SaveCoordinator::Completed *job) { COST_SCOPE("awards_save:award_restore_sidecar");
    award_reset();bool restored=false;
    if(job) {
        wchar_t path[MAX_PATH];
        if(award_save_path(*job,path,false)) {
            HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file!=INVALID_HANDLE_VALUE) {
                std::array<uint8_t,40+awards::MaxItems*2> bytes={};DWORD got=0;LARGE_INTEGER length={};
                restored=GetFileSizeEx(file,&length) && length.QuadPart==static_cast<LONGLONG>(bytes.size()) && ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr) &&
                    got==bytes.size() && awards::decode(bytes.data(),bytes.size(),job->binding,award_storage);
                CloseHandle(file);
            }
        }
    }
    if(restored) {for(Obj item:award_items()) {
        unsigned id=award_item_id(item);award_observed[id]=word(item,0x4A);award_observed_found[id]=(item[0x4F]&1)!=0;
    }award_initialized=true;}
    award_synchronize();log_event(restored?"warehouse_load_restored":"warehouse_load_initialized");
    return restored;
}
// Sidecar plugin of the warehouse (.warehouse): written after the merit file, restored last.
static const save_host::SidecarPlugin award_sidecar_plugin={"warehouse",L"warehouse",1,"warehouse_save_written","warehouse_save_write_failed",nullptr,
    []() {award_snapshot();},
    [](const save_host::Job &job,save_host::Written &out) {return award_write_sidecar(*static_cast<const merit::SaveCoordinator::Completed *>(job.source),&out);},
    []() {award_sidecar_cache.clear();},
    [](const save_host::Job *job) {return award_restore_sidecar(job?static_cast<const merit::SaveCoordinator::Completed *>(job->source):nullptr);}};
