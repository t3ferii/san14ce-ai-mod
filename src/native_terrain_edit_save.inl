#include "sidecar_cache.h"
#include "save_host.h"
namespace terrain_edit {
static Changes save_snapshot;
static SidecarCache sidecar_cache;
static void snapshot() {
    if(!active())return;
    check_world();save_snapshot=changes;
}
static bool save_path(const save_host::Job &job,wchar_t *path,bool create) {
    return save_host::path(self_module,save_host::Job{job.name,job.binding},L"terrain",path,MAX_PATH,create);
}
static bool write_sidecar(const save_host::Job &job,save_host::Written *report=nullptr) {
    if(!active()) {if(report)report->state=save_host::kOff;return true;}   // feature off: nothing written (manifest records OFF)
    save_host::Written local;
    return save_host::store(self_module,save_host::Job{job.name,job.binding},L"terrain",save_snapshot.encode(job.binding),
        sidecar_cache,report?*report:local);
}
static void begin_load() {before_world_change();sidecar_cache.clear();save_snapshot.clear();}
static bool restore_sidecar(const save_host::Job *job) {
    reset_ui_context();
    if(!active())return false;
    changes.clear();changed_world=world();unsigned applied=0,failed=0;bool accepted=false;
    if(job) {
        wchar_t path[MAX_PATH];
        if(save_path(*job,path,false)) {
            HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file!=INVALID_HANDLE_VALUE) {
                LARGE_INTEGER length={};DWORD got=0;
                // Bounded stack buffer; no allocations or exceptions while a handle is open.
                std::array<uint8_t,MaxEncodedSize> bytes={};Changes saved;
                bool ok=GetFileSizeEx(file,&length) && length.QuadPart>=static_cast<LONGLONG>(HeaderSize) &&
                    length.QuadPart<=static_cast<LONGLONG>(MaxEncodedSize);
                if(ok)ok=ReadFile(file,bytes.data(),static_cast<DWORD>(length.QuadPart),&got,nullptr)!=0 &&
                    got==length.QuadPart && saved.decode(bytes.data(),got,job->binding);
                CloseHandle(file);
                accepted=ok;
                if(ok)applied=changes.restore(saved,tile_value,failed);
                if(applied)clear_cost_cache();
            }
        }
    }
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
    for(unsigned i=0;i<changes.size();++i) {
        const Change &c=changes[i];terrain_render::mark(c.hex,c.next,c.original);
    }
#endif
    // native_merit_save::merit_load_host, phase 2 -> 3 after 509580 returns:
    // completed load on the existing host/UI caller, separate from file_read IO.
    sync_render();
#ifndef NO_RUNTIME_LOG
    log_event("terrain_edit_load",applied,failed,0);
#else
    (void)applied;
#endif
    return accepted;
}
// Sidecar plugin of the terrain edit (.terrain): first in the order.
static const save_host::SidecarPlugin sidecar_plugin={"terrain",L"terrain",1,nullptr,"terrain_edit_save_failed",nullptr,&snapshot,
    [](const save_host::Job &job,save_host::Written &out) {return write_sidecar(job,&out);},
    &begin_load,
    [](const save_host::Job *job) {return restore_sidecar(job);}};
}
