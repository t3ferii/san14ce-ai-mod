#pragma once
// One completed ten-day cycle, owned by the native UI thread. No game pointers.
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
namespace advisor_briefing_core {
enum class Kind {News,Weather,Disaster,Growth,Recruitment,Gold,Treasure,Income};
struct Event {Kind kind=Kind::News;std::wstring text;unsigned speaker=0,day=0;std::uint64_t gold=0;bool read=false;};
struct Card {Kind kind=Kind::News;std::vector<Event> events;unsigned unread=0;std::uint64_t gold=0;};
inline const wchar_t *caption(Kind k) {
    switch(k) {
    case Kind::Weather:return L"천시와 계절";case Kind::Disaster:return L"재해 소식";
    case Kind::Growth:return L"무장의 성장";case Kind::Recruitment:return L"새로운 인재";
    case Kind::Gold:return L"뜻밖의 수확";case Kind::Treasure:return L"발견한 명품";
    case Kind::Income:return L"이번 순의 수입과 지출";default:return L"전해 온 소식";
    }
}
inline bool has(const std::wstring &s,const wchar_t *v){return s.find(v)!=std::wstring::npos;}
inline Kind classify(const std::wstring &s) {
    if(has(s,L"홍수") || has(s,L"역병") || has(s,L"메뚜기") || has(s,L"재해") || has(s,L"가뭄"))return Kind::Disaster;
    if(has(s,L"천시") || has(s,L"계절"))return Kind::Weather;
    if(has(s,L"명품") || has(s,L"보물"))return Kind::Treasure;
    if(has(s,L"등용") || has(s,L"임관"))return Kind::Recruitment;
    if(has(s,L"통솔") || has(s,L"무력") || has(s,L"지력") || has(s,L"정치") || has(s,L"매력") || has(s,L"성장"))return Kind::Growth;
    if(has(s,L"금") && (has(s,L"발견") || has(s,L"찾았") || has(s,L"찾아")))return Kind::Gold;
    return Kind::News;
}
inline bool passive_notice(const std::wstring &s) {
    // User 2026-10-26: a diplomatic refusal must retain its entire dialogue.
    // Shared message classes also carry interaction continuations. Unknown
    // text is native; exact end-of-turn report records remain collectable.
    if(classify(s)==Kind::News)return false;
    for(const wchar_t *term:{L"외교",L"교섭",L"요구",L"거절",L"승낙",L"동맹",L"포로",L"몸값"})if(has(s,term))return false;
    return true;
}
inline bool interactive_state(const char *name,size_t size) {
    std::string s(name,size);
    return s.find("Diplomacy")!=std::string::npos || s.find("TreatPrisoner")!=std::string::npos;
}
// Only an explicit "금 <amount>" token is summed. Dates, officer stats and
// treasure names containing digits never become money. Unparsed text is kept.
inline std::uint64_t gold_amount(const std::wstring &s) {
    auto pos=s.find(L"금");if(pos==std::wstring::npos)return 0;
    ++pos;while(pos<s.size() && (s[pos]==L' ' || s[pos]==L'\t'))++pos;
    std::uint64_t n=0;bool digit=false,comma=false;
    for(;pos<s.size();++pos) {
        wchar_t c=s[pos];
        if(c==L',' && digit && !comma){comma=true;continue;}
        if(c<L'0' || c>L'9')break;
        if(n>(1000000000000ull-static_cast<unsigned>(c-L'0'))/10)return 0;
        n=n*10+static_cast<unsigned>(c-L'0');digit=true;comma=false;
    }
    return digit && !comma?n:0;
}
inline std::wstring compact(const Event &e) {
    if(e.kind!=Kind::Gold || !e.gold || e.text.find(L'\n')!=std::wstring::npos)return e.text;
    auto at=e.text.find(L"가 금");if(at==std::wstring::npos)at=e.text.find(L"이 금");
    if(at==std::wstring::npos || at==0 || at>40)return e.text;
    return e.text.substr(0,at)+L"  ·  "+std::to_wstring(e.gold);
}
struct Inbox {
    static constexpr size_t limit=2048,text_limit=4095;
    std::vector<Event> events;std::uintptr_t world=0;unsigned force=0,generation=0;
    bool collecting=false,fallback=false,has_cycle=false;
    void reset(std::uintptr_t w,unsigned f) {
        events.clear();world=w;force=f;collecting=false;fallback=false;has_cycle=false;++generation;
    }
    void context(std::uintptr_t w,unsigned f){if(world!=w || force!=f)reset(w,f);}
    // Called once when the player leaves CUserStrategyState after Progress.
    void begin(std::uintptr_t w,unsigned f) {
        reset(w,f);collecting=w && f;has_cycle=collecting;
    }
    void finish(){collecting=false;}
    bool append(std::vector<Event> batch,bool final_report=false) {
        if((!collecting && !(final_report && has_cycle)) || fallback)return false;
        if(batch.size()>limit-events.size()){fallback=true;return false;}
        for(auto &e:batch) {
            if(e.text.empty() || e.text.size()>text_limit){fallback=true;return false;}
            // Categories affect presentation only. All original text survives.
            if(e.kind==Kind::News)e.kind=classify(e.text);
            if(e.kind==Kind::Gold)e.gold=gold_amount(e.text);
        }
        for(auto &e:batch)events.push_back(std::move(e));
        ++generation;return true;
    }
    std::vector<Card> cards()const {
        std::vector<Card> out;
        for(const auto &e:events) {
            auto it=std::find_if(out.begin(),out.end(),[&](const Card &c){return c.kind==e.kind;});
            if(it==out.end()){out.push_back(Card{});it=out.end()-1;it->kind=e.kind;}
            it->events.push_back(e);if(!e.read)++it->unread;it->gold+=e.gold;
        }
        return out;
    }
    unsigned unread()const {unsigned n=0;for(const auto &e:events)if(!e.read)++n;return n;}
    void mark(Kind kind){for(auto &e:events)if(e.kind==kind)e.read=true;++generation;}
    void erase(Kind kind) {
        events.erase(std::remove_if(events.begin(),events.end(),[&](const Event &e){return e.kind==kind;}),events.end());++generation;
    }
    void clear(){events.clear();++generation;}
};
inline double ease(double t){t=(std::max)(0.0,(std::min)(1.0,t));return 1.0-(1.0-t)*(1.0-t)*(1.0-t);}
struct Rect {float x,y,w,h;bool contains(float px,float py)const{return px>=x && py>=y && px<x+w && py<y+h;}};
// 2026-10-26 live CE KO: the face resource is 484x512, but the native world
// matrix scales it by 0.3125. 92CF90 reports resource dimensions; 92CB10 also
// subtracts an unscaled anchor, so neither result is a screen capture box.
inline bool portrait_bounds(int width,int height,unsigned anchor,const float *matrix,Rect &out) {
    if(!matrix || width<=0 || height<=0 || width>4096 || height>4096)return false;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(matrix[i]))return false;
    // The adviser is axis aligned. An unknown rotated/flipped node uses the
    // visible fallback avatar instead of sampling an unrelated map region.
    if(matrix[0]<=0 || matrix[5]<=0 || std::fabs(matrix[1])>0.001f || std::fabs(matrix[4])>0.001f)return false;
    float w=width*matrix[0],h=height*matrix[5];
    Rect r{960+matrix[12]-((anchor>>4)&15)*w*0.5f,540-matrix[13]-(anchor&15)*h*0.5f,w,h};
    if(w<=8 || h<=8 || r.x<0 || r.y<0 || r.x+w>1920 || r.y+h>1080)return false;
    out=r;return true;
}
inline constexpr Rect portrait{30,956,88,88},bubble{136,458,510,574};
// Coordinates use the same centered 1920x1080 viewport as the existing overlay.
inline bool client_point(int x,int y,int cw,int ch,int bw,int bh,float &vx,float &vy) {
    if(cw<=0 || ch<=0 || bw<=0 || bh<=0)return false;
    double scale=(std::min)(bw/1920.0,bh/1080.0);if(scale<=0)return false;
    vx=static_cast<float>((double(x)*bw/cw-(bw-1920*scale)/2)/scale);
    vy=static_cast<float>((double(y)*bh/ch-(bh-1080*scale)/2)/scale);return true;
}
}
