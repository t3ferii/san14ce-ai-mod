#pragma once
// [tactic_priority] profile=... raw ini value (docs/TACTIC_PRIORITY_PLAN.md section 6). Same form as ccz_appearance_io.h.
#include <windows.h>
#include <string>
namespace tactic_priority_io {
inline const wchar_t *section=L"tactic_priority";
inline const wchar_t *key=L"profile";
// Profile text is ASCII only; anything else is treated as a failed conversion.
inline bool to_ascii(const std::wstring &in,std::string &out){
    out.clear();out.reserve(in.size());
    for(wchar_t c:in){if(c<1 || c>126)return false;out.push_back(static_cast<char>(c));}
    return true;
}
inline std::wstring to_wide(const std::string &in){
    std::wstring out;out.reserve(in.size());
    for(char c:in)out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
    return out;
}
inline std::wstring read_profile(const std::wstring &path){
    wchar_t text[2048]={};GetPrivateProfileStringW(section,key,L"",text,2048,path.c_str());return text;
}
inline bool save_profile(const std::wstring &path,const std::wstring &before,const std::wstring &next){
    // Another settings editor may have changed this profile while the dialog was open.
    // Update exactly one key, never rewrite the file; refuse when the current value is not what the dialog loaded.
    if(next.empty() || next.size()>2047)return false;
    if(read_profile(path)!=before)return false;
    if(!WritePrivateProfileStringW(section,key,next.c_str(),path.c_str()))return false;
    return read_profile(path)==next;
}
}
