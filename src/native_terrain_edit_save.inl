#include "merit_bond.h"
#include "sidecar_cache.h"
static bool merit_sidecar_path(const merit::SaveCoordinator::Completed &,wchar_t *,size_t,bool);
namespace terrain_edit {
static Changes save_snapshot;
static SidecarCache sidecar_cache;
static void snapshot() {
    if(!active())return;
    check_world();save_snapshot=changes;
}
static bool save_path(const merit::SaveCoordinator::Completed &job,wchar_t *path,bool create) {
    if(!merit_sidecar_path(job,path,MAX_PATH,create))return false;
    wchar_t *extension=wcsrchr(path,L'.');if(!extension)return false;
    return !wcscpy_s(extension,static_cast<size_t>(MAX_PATH-(extension-path)),L".terrain");
}
static bool write_sidecar(const merit::SaveCoordinator::Completed &job) {
    if(!active())return true;
    wchar_t path[MAX_PATH],temporary[MAX_PATH];
    if(!save_path(job,path,true) || swprintf_s(temporary,L"%s.tmp",path)<0)return false;
    auto bytes=save_snapshot.encode(job.binding);
    if(sidecar_cache.matches(path,bytes) && GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return true;
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=0 &&
        written==bytes.size() && FlushFileBuffers(file)!=0;
    CloseHandle(file);
    if(ok)ok=MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temporary);else sidecar_cache.remember(path,bytes);
    return ok;
}
static void begin_load() {before_world_change();sidecar_cache.clear();save_snapshot.clear();}
static void restore_sidecar(const merit::SaveCoordinator::Completed *job) {
    reset_ui_context();
    if(!active())return;
    changes.clear();changed_world=world();unsigned applied=0,failed=0;
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
}
}
