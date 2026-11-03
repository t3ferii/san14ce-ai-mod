#pragma once
#include "ccz_appearance.h"
namespace ccz_appearance {
inline std::wstring read_profile(const wchar_t *path){wchar_t text[2048]={};GetPrivateProfileStringW(section,key,L"",text,2048,path);return text;}
inline bool save_profile(const wchar_t *path,const std::wstring &before,const Settings &next){
    // Another settings editor may have changed this profile while the dialog was open.
    // Other INI values may change independently: update exactly one key, never rewrite the file.
    if(read_profile(path)!=before)return false;
    std::wstring text=encode(next);Settings checked;if(!decode(text.c_str(),checked))return false;
    if(!WritePrivateProfileStringW(section,key,text.c_str(),path))return false;
    return read_profile(path)==text;
}
}
