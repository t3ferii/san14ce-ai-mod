#pragma once
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include <mutex>
// Cache only bytes confirmed on disk after a successful atomic write/read.
// A distinct native save binding always has its own path and is persisted.
class SidecarCache {
    std::map<std::wstring,std::vector<uint8_t>> entries;
    size_t used=0;
    mutable std::mutex lock;
public:
    void clear() {std::lock_guard<std::mutex> guard(lock);entries.clear();used=0;}
    bool get(const wchar_t *path,std::vector<uint8_t> &out) const {
        std::lock_guard<std::mutex> guard(lock);
        auto it=entries.find(path);if(it==entries.end())return false;out=it->second;return true;
    }
    bool matches(const wchar_t *path,const std::vector<uint8_t> &bytes) const {
        std::lock_guard<std::mutex> guard(lock);
        auto it=entries.find(path);return it!=entries.end() && it->second==bytes;
    }
    void remember(const wchar_t *path,const std::vector<uint8_t> &bytes) {
        std::lock_guard<std::mutex> guard(lock);
        auto old=entries.find(path);if(old!=entries.end()) {used-=old->second.size();entries.erase(old);}
        if(entries.size()>=64 || used+bytes.size()>4*1024*1024) {entries.clear();used=0;}
        entries.emplace(path,bytes);used+=bytes.size();
    }
};
