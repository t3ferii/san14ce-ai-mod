#include "sidecar_cache.h"
static bool merit_sidecar_path(const merit::SaveCoordinator::Completed &,wchar_t *,size_t,bool);
static awards::Inventory award_save_snapshot;
static SidecarCache award_sidecar_cache;
static void award_snapshot() {award_synchronize();award_save_snapshot=award_storage;}
static bool award_save_path(const merit::SaveCoordinator::Completed &job,wchar_t *path,bool create) {
    if(!merit_sidecar_path(job,path,MAX_PATH,create))return false;
    wchar_t *extension=wcsrchr(path,L'.');if(!extension)return false;
    wcscpy_s(extension,static_cast<size_t>(MAX_PATH-(extension-path)),L".warehouse");return true;
}
static bool award_write_sidecar(const merit::SaveCoordinator::Completed &job) {
    wchar_t path[MAX_PATH],temporary[MAX_PATH];
    if(!award_save_path(job,path,true) || swprintf_s(temporary,L"%s.tmp",path)<0)return false;
    auto bytes=awards::encode(award_save_snapshot,job.binding);
    if(award_sidecar_cache.matches(path,bytes) && GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return true;
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=0 && written==bytes.size() && FlushFileBuffers(file)!=0;
    CloseHandle(file);if(ok)ok=MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temporary);else award_sidecar_cache.remember(path,bytes);return ok;
}
static void award_restore_sidecar(const merit::SaveCoordinator::Completed *job) {
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
}
