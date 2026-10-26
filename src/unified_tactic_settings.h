#pragma once
#include "unified_personnel.h"
#include <sstream>
namespace unified_config {
inline std::wstring path() {
    wchar_t exe[32768]={};DWORD count=GetModuleFileNameW(nullptr,exe,32768);
    if(!count || count>=32768)return {};
    std::wstring result(exe,count);auto slash=result.find_last_of(L"\\/");
    if(slash==std::wstring::npos)return {};
    return result.substr(0,slash)+L"\\AI_TacticsMod\\data\\unified_personnel.ini";
}
inline bool valid_entry(int id) {return (id>=1 && id<=200) || id==unified::Unique || id==unified::Common || id==unified::Trade;}
inline bool decode(const wchar_t *text,std::vector<int> &out) {
    out.clear();std::wistringstream input(text?text:L"");std::wstring token;std::set<int> seen;
    while(std::getline(input,token,L',')) {
        if(out.size()>=203 || token.empty())return false;
        wchar_t *end=nullptr;long id=wcstol(token.c_str(),&end,10);
        if(!end || *end || id<-3 || id>200 || !valid_entry(static_cast<int>(id)))return false;
        if(seen.insert(static_cast<int>(id)).second)out.push_back(static_cast<int>(id));
    }
    return !out.empty();
}
inline std::wstring encode(const std::vector<int> &entries) {
    std::wstring out;for(int id:entries){if(!out.empty())out+=L',';out+=std::to_wstring(id);}return out;
}
inline unified::Priorities load() {
    unified::Priorities result;result.martial={unified::Unique,137,1,17,6,unified::Common,unified::Trade};
    result.intellect={unified::Unique,137,17,6,unified::Common,unified::Trade};
    auto file=path();if(file.empty() || GetPrivateProfileIntW(L"selection",L"version",0,file.c_str())!=1)return result;
    wchar_t text[4096]={};std::vector<int> parsed;
    GetPrivateProfileStringW(L"selection",L"martial",L"",text,4096,file.c_str());
    if(!decode(text,parsed))return result;result.martial=parsed;
    GetPrivateProfileStringW(L"selection",L"intellect",L"",text,4096,file.c_str());
    if(!decode(text,parsed))return result;result.intellect=parsed;
    result.enabled=GetPrivateProfileIntW(L"selection",L"enabled",0,file.c_str())==1;return result;
}
inline bool save(const unified::Priorities &value) {
    std::wstring file=path();if(file.empty())return false;
    std::wstring text=L"\xFEFF[selection]\r\nversion=1\r\nenabled="+std::to_wstring(value.enabled?1:0)+
        L"\r\nmartial="+encode(value.martial)+L"\r\nintellect="+encode(value.intellect)+L"\r\n";
    std::wstring temp=file+L".tmp";HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    DWORD written=0,bytes=static_cast<DWORD>(text.size()*sizeof(wchar_t));
    bool ok=WriteFile(h,text.data(),bytes,&written,nullptr) && written==bytes && FlushFileBuffers(h);
    CloseHandle(h);
    if(ok)ok=MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temp.c_str());return ok;
}
inline std::vector<unified::TacticInfo> catalog() {
    std::vector<unified::TacticInfo> out;
    for(unsigned i=1;i<=200;++i)out.push_back({i,
        (i==25 || i==26 || (i>=136 && i<=139))?unified::TacticKind::Trade:
        i<=24?unified::TacticKind::Common:unified::TacticKind::Unique});
    return out;
}
}
