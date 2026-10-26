#pragma once
#include <windows.h>
// Caller owns the lock and keeps path unchanged while the cached handle exists.
// Writes remain synchronous; no queue, dropped events or game calls on workers.
namespace diagnostics {
struct AppendFile {
    HANDLE cached=INVALID_HANDLE_VALUE;
    void close() {if(cached!=INVALID_HANDLE_VALUE){CloseHandle(cached);cached=INVALID_HANDLE_VALUE;}}
    bool write(const wchar_t *path,const char *bytes,DWORD size,bool reuse) {
        HANDLE handle=cached;
        if(!reuse || handle==INVALID_HANDLE_VALUE) {
            handle=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
                nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(reuse)cached=handle;
        }
        if(handle==INVALID_HANDLE_VALUE)return false;
        DWORD written=0;bool result=WriteFile(handle,bytes,size,&written,nullptr)!=0 && written==size;
        if(!reuse)CloseHandle(handle);
        else if(!result)close(); // A later event retries opening after an IO failure.
        return result;
    }
};
}
