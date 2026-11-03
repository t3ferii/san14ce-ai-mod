// Appearance only: no combat/stat/save-game changes. User 2026-11-02.
#pragma once
#include <array>
#include <string>
#include <cwchar>
#include <limits>
namespace ccz_appearance {
constexpr unsigned native=255,classes=190,formation_count=21,rows=23;
constexpr unsigned char defaults[rows][3]={
    {3,4,5},{9,40,41},{19,34,35},{13,14,29},{3,4,5},{75,76,77},{48,49,50},{31,31,31},{141,141,141},
    {255,255,255},{42,43,44},{255,255,255},{255,255,255},{255,255,255},{255,255,255},
    {255,255,255},{255,255,255},{255,255,255},{255,255,255},{81,82,83},{255,255,255},
    {0,1,2},{75,76,77}};
struct General {unsigned short id,cell;};
constexpr General generals[]={{16,752},{29,900},{32,922},{34,901},{35,809},{37,899},{92,776},{95,773},{102,740},{105,889},{114,734},{124,725},{144,920},{147,495},{161,843},{162,856},{176,881},{179,522},{183,837},{185,834},{186,894},{189,828},{203,859},{208,865},{215,516},{219,923},{231,839},{247,878},{254,525},{271,510},{281,893},{344,819},{371,838},{373,873},{379,531},{383,862},{393,767},{402,779},{405,845},{412,874},{432,868},{440,534},{442,749},{449,871},{464,892},{511,746},{515,816},{518,731},{521,480},{526,817},{535,891},{551,886},{553,866},{557,486},{586,882},{596,501},{607,540},{617,884},{618,737},{631,875},{640,528},{656,498},{658,844},{663,728},{671,912},{689,770},{703,885},{709,519},{736,913},{765,841},{766,848},{769,504},{787,846},{812,895},{822,888},{827,829},{828,877},{830,818},{842,782},{845,861},{902,803},{904,850},{913,743},{952,483},{958,864},{959,836},{962,764},{968,835},{974,872},{987,492},{988,869},{997,867}};
constexpr size_t general_count=sizeof(generals)/sizeof(generals[0]);
struct Settings {
    unsigned low=70,high=90;
    std::array<std::array<unsigned short,3>,rows> mapping{};
    std::array<unsigned char,general_count> forms{};
    Settings(){for(unsigned r=0;r<rows;++r)for(unsigned t=0;t<3;++t)mapping[r][t]=defaults[r][t];forms.fill(255);}
};
inline unsigned tier(const Settings &s,unsigned lead){return lead>=s.high?2:lead>=s.low?1:0;}
// Old generic class IDs stay compatible. Explicit source/cell IDs expose every extracted shape.
inline bool valid_code(unsigned code){return code<classes || code==native || (code>=0x1000 && code<0x1000+1500) || (code>=0x2000 && code<0x2000+1500);}
inline unsigned cell(unsigned code){return code>=0x2000?4096+code-0x2000:code>=0x1000?code-0x1000:code*3;}
inline unsigned code(unsigned id){return id>=4096?0x2000+id-4096:0x1000+id;}
inline size_t general_index(unsigned id){for(size_t i=0;i<general_count;++i)if(generals[i].id==id)return i;return general_count;}
inline unsigned personal(const Settings &s,unsigned id,unsigned formation){
    size_t i=general_index(id);return formation<8 && i<general_count && (s.forms[i]&(1u<<formation))?generals[i].cell:0;
}
struct Choice {unsigned cls=native,cell=native*3;bool unique=false;};
inline Choice choose(const Settings &s,unsigned formation,unsigned lead,unsigned leader,bool ruler,bool strategist){
    Choice c;if(formation>=formation_count)return c;
    unsigned t=tier(s,lead);c.cls=s.mapping[formation][t];c.cell=cell(c.cls);
    // Preserve local14 defaults: native ships/siege paths do not become personal sprites.
    unsigned own=personal(s,leader,formation);
    if(own){c.cell=own;c.unique=true;if(c.cls==native)c.cls=own/3;}
    if(formation>=1 && formation<=7 && (ruler || strategist)){
        c.cls=s.mapping[ruler?21:22][t];if(!c.unique)c.cell=cell(c.cls);else if(c.cls==native)c.cls=c.cell/3;
    }
    return c;
}
inline std::wstring encode(const Settings &s){
    std::wstring out=L"2,"+std::to_wstring(s.low)+L","+std::to_wstring(s.high);
    for(const auto &r:s.mapping)for(unsigned v:r)out+=L","+std::to_wstring(v);
    for(unsigned v:s.forms)out+=L","+std::to_wstring(v);return out;
}
inline bool decode(const wchar_t *text,Settings &out){
    // Strict whole-profile validation. Never keep half of a malformed or newer profile.
    if(!text || !*text)return false;
    constexpr size_t count=3+rows*3+general_count;unsigned values[count]={};size_t at=0;
    const wchar_t *p=text;
    while(*p && at<count){
        if(*p<L'0' || *p>L'9')return false;unsigned v=0;
        do{unsigned d=static_cast<unsigned>(*p++-L'0');if(v>((std::numeric_limits<unsigned>::max)()-d)/10)return false;v=v*10+d;}while(*p>=L'0' && *p<=L'9');
        values[at++]=v;if(!*p)break;if(*p++!=L',' || !*p)return false;
    }
    if(*p || at!=count || (values[0]!=1 && values[0]!=2) || values[1]>=values[2] || values[2]>255)return false;
    Settings next;next.low=values[1];next.high=values[2];at=3;
    for(auto &r:next.mapping)for(auto &v:r){unsigned n=values[at++];if(!valid_code(n) || (values[0]==1 && n>=0x1000))return false;v=static_cast<unsigned short>(n);}
    for(auto &v:next.forms){unsigned n=values[at++];if(n>255)return false;v=static_cast<unsigned char>(n);}
    out=next;return true;
}
constexpr const wchar_t *section=L"ccz_appearance",*key=L"profile";
}
