#pragma once
// U35, user 2026-11-10: retain after the first slow synchronous load.
// Caller serializes reservations; no game calls or resource ownership here.
#include <algorithm>
#include <string>
#include <vector>
namespace layout_keep_core {
constexpr size_t kMax=512;
inline bool wanted(bool keep_on,int async,const char *name,double ms) {
    return keep_on && !async && name && *name && ms>=5.0;
}
inline bool reserve(std::vector<std::string> &names,const std::string &name) {
    if(name.empty() || names.size()>=kMax || std::find(names.begin(),names.end(),name)!=names.end())return false;
    names.push_back(name);return true;
}
inline void cancel(std::vector<std::string> &names,const std::string &name) {
    auto it=std::find(names.begin(),names.end(),name);
    if(it!=names.end())names.erase(it);
}
}
