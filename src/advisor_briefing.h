#pragma once
// One completed ten-day cycle, owned by the native UI thread. No game pointers.
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <utility>
namespace advisor_briefing_core {
enum class Kind {News,Weather,Disaster,Growth,Recruitment,Gold,Treasure,Income,Diplomacy};
struct Event {Kind kind=Kind::News;std::wstring text;unsigned speaker=0,day=0;std::uint64_t gold=0;bool read=false;std::wstring report;};
struct Card {Kind kind=Kind::News;std::vector<Event> events;unsigned unread=0;std::uint64_t gold=0;};
// 사용자 2026-10-28 설계 정정: result-only summaries; no native formatter.
inline std::wstring scene_particle(const std::wstring &name,const wchar_t *closed,const wchar_t *open) {
    wchar_t c=name.empty()?0:name.back();return name+(c>=0xAC00 && c<=0xD7A3 && (c-0xAC00)%28?closed:open);
}
inline std::wstring recruitment_scene_summary(const std::wstring &envoy,const std::wstring &target,bool success) {
    if(envoy.empty() || target.empty())return {};
    return scene_particle(envoy,L"이 ",L"가 ")+target+L" 등용에 "+(success?L"성공했습니다.":L"실패했습니다.");
}
inline std::wstring discovery_scene_summary(unsigned type,const std::wstring &actor,const std::wstring &found,const std::wstring &place,bool success) {
    if(type>5 || actor.empty() || found.empty())return {};
    std::wstring where=place.empty()?L"":place+L"에서 ";
    std::wstring who=scene_particle(actor,L"이 ",L"가 "),what=scene_particle(found,L"을 ",L"를 ");
    switch(type) {
    case 0:return who+where+L"명품 "+what+L"발견했습니다.";
    case 1:return who+where+what+L"발견했으며, 등용에 "+(success?L"성공했습니다.":L"실패했습니다.");
    case 2:return who+where+L"물품 "+what+L"발견했습니다.";
    case 3:return who+found+L"의 지리 이점을 발견했습니다.";
    case 4:return who+found+L"의 지리 이점을 잃었습니다.";
    default:return who+L"명승 "+what+L"발견했습니다.";
    }
}
// 사용자 2026-10-30: native donation data, after the resource was already applied.
inline std::wstring destiny_summary(const std::wstring &force,const std::wstring &city,unsigned message,unsigned amount,unsigned total) {
    if(force.empty() || city.empty() || (message!=0x5EE4 && message!=0x5EE5))return {};
    const wchar_t *resource=message==0x5EE4?L"금":L"군량";
    return L"천시의 축복으로 거상이 "+force+L"군에 "+std::to_wstring(amount)+resource+L"을 기부했습니다. "+city+L"의 "+resource+L"이 "+std::to_wstring(total)+L"이 되었습니다.";
}
inline const wchar_t *caption(Kind k) {
    switch(k) {
    case Kind::Weather:return L"천시와 계절";case Kind::Disaster:return L"재해 소식";
    case Kind::Growth:return L"무장의 성장";case Kind::Recruitment:return L"새로운 인재";
    case Kind::Gold:return L"뜻밖의 수확";case Kind::Treasure:return L"발견한 명품";
    case Kind::Income:return L"수입과 지출";case Kind::Diplomacy:return L"외교 보고";default:return L"전해 온 소식";
    }
}
inline bool has(const std::wstring &s,const wchar_t *v){return s.find(v)!=std::wstring::npos;}
inline std::wstring display_text(const std::wstring &raw) {
    std::wstring out;out.reserve(raw.size());
    for(size_t i=0;i<raw.size();++i) {
        if(raw[i]==L'^' && i+2<raw.size() && raw[i+1]>=L'0' && raw[i+1]<=L'9' && raw[i+2]>=L'0' && raw[i+2]<=L'9'){i+=2;continue;}
        out+=raw[i];
    }
    return out;
}
inline void add_name(std::vector<std::wstring> &names,const std::wstring &name) {
    if(name.size()<2 || name.size()>24 || names.size()>=512)return;
    for(wchar_t c:name)if(!((c>=0x3400 && c<=0x9FFF) || (c>=0xAC00 && c<=0xD7A3) || (c>=0x3040 && c<=0x30FF) || (c>=L'A' && c<=L'Z') || (c>=L'a' && c<=L'z')))return;
    if(std::find(names.begin(),names.end(),name)==names.end())names.push_back(name);
}
inline void collect_names(const std::wstring &raw,std::vector<std::wstring> &names) {
    for(size_t at=0;(at=raw.find(L"^02",at))!=std::wstring::npos;) {
        at+=3;size_t end=raw.find(L'^',at);if(end==std::wstring::npos)break;
        add_name(names,raw.substr(at,end-at));at=end;
    }
}
inline unsigned name_color_index(const std::wstring &name) {
    unsigned h=2166136261u;for(wchar_t c:name)h=(h^static_cast<unsigned>(c))*16777619u;return h%6;
}
inline std::wstring grouped_number(const std::wstring &raw) {
    std::wstring text=display_text(raw),out;
    for(size_t i=0;i<text.size();) {
        if(text[i]<L'0' || text[i]>L'9'){out+=text[i++];continue;}
        size_t end=i;while(end<text.size() && text[end]>=L'0' && text[end]<=L'9')++end;
        bool already=(i && text[i-1]==L',') || (end<text.size() && text[end]==L',');
        for(size_t j=i;j<end;++j){out+=text[j];if(!already && end-j>1 && (end-j-1)%3==0)out+=L',';}i=end;
    }
    return out;
}
inline std::wstring income_summary(const std::wstring &title,const std::vector<std::pair<int,std::wstring>> &cells) {
    auto value=[&](int id){for(const auto &c:cells)if(c.first==id)return grouped_number(c.second);return std::wstring(L"—");};
    const int rows[]={10,19,28,37};const wchar_t *labels[]={L"금",L"군량",L"병사",L"군정"};
    for(int id:rows)if(value(id)==L"—")return {};
    std::wstring out=title+L"\n항목\t보유량\t변동\t수입\t지출";
    for(unsigned i=0;i<4;++i)out+=L"\n"+std::wstring(labels[i])+L"\t"+value(rows[i])+L"\t"+value(rows[i]+1)+L"\t"+value(rows[i]+2)+L"\t"+value(rows[i]+3);
    return out;
}
inline Kind classify(const std::wstring &s) {
    if(has(s,L"홍수") || has(s,L"역병") || has(s,L"메뚜기") || has(s,L"재해") || has(s,L"가뭄"))return Kind::Disaster;
    if(has(s,L"천시") || has(s,L"계절"))return Kind::Weather;
    if(has(s,L"명품") || has(s,L"보물"))return Kind::Treasure;
    if(has(s,L"등용") || has(s,L"임관") || has(s,L"님을 발견"))return Kind::Recruitment;
    if(has(s,L"통솔") || has(s,L"무력") || has(s,L"지력") || has(s,L"정치") || has(s,L"매력") || has(s,L"성장"))return Kind::Growth;
    if((has(s,L" 금") || s.rfind(L"금 ",0)==0) && (has(s,L"발견") || has(s,L"찾았") || has(s,L"찾아")))return Kind::Gold;
    return Kind::News;
}
inline bool passive_notice(const std::wstring &s) {
    // 사용자 결정 2026-10-28: 클릭형은 모두 수집. Wording, including ?/？,
    // never decides whether a native state requires a player response.
    std::wstring text=display_text(s);
    return !text.empty();
}
inline bool interactive_state(const char *name,size_t size) {
    std::string s(name,size);
    return s=="CStrategyFirstExplainTalkState" || s.find("Diplomacy")!=std::string::npos || s.find("TreatPrisoner")!=std::string::npos || s.find("YesNo")!=std::string::npos || s.find("Select")!=std::string::npos || s.find("Tutorial")!=std::string::npos || (s.find("Input")!=std::string::npos && s!="EventView::CInputWaitState");
}
inline bool passive_event_state(const char *name,size_t size) {
    // Confirmed passive names remain available to existing checks. The native
    // collector also accepts other noninteractive parents with a matching layout.
    std::string value(name,size);
    for(const char *allowed:{"CEventPlaybackState","CStrategyAdviceState","CStrategyEmployTalkState","CSearchDiscoveryState","CStrategyCreditRankupState","CDestinyRandomBonusState","CPoemEventState","CCaravanReturnState"})if(value==allowed)return true;
    return false;
}
struct DiplomaticFacts {
    bool friendly=false,ruler=false,known_sides=false,incoming=false;
    int result=-1;unsigned kind=0;std::wstring counterpart;
};
inline const wchar_t *diplomatic_request(unsigned kind) {
    switch(kind){case 4:return L"친선";case 5:return L"동맹";case 6:return L"항복";case 7:return L"교환";case 8:return L"포로 반환";case 9:return L"공격";default:return L"외교";}
}
inline std::wstring diplomatic_report(const std::wstring &raw,const DiplomaticFacts &facts) {
    std::wstring text=display_text(raw),side=facts.counterpart.empty()?L"상대 측":display_text(facts.counterpart)+L" 측";
    const std::wstring request=diplomatic_request(facts.kind);
    if(facts.friendly) {
        if(facts.known_sides && !facts.incoming && facts.result==0)return L"주군께서 제안하신 "+request+L" 교섭이 성사되지 않았습니다.";
        if(facts.known_sides && facts.result==1) {
            if(!facts.incoming)return L"주군께서 제안하신 "+request+L" 교섭이 성사되었습니다.";
            return (facts.ruler?L"주군께서 ":L"아군 사신이 ")+side+L"의 "+request+(facts.ruler?L" 요청을 받아들이셨습니다.":L" 요청을 받아들였습니다.");
        }
        bool refused=has(text,L"거절") || has(text,L"곤란") || has(text,L"응할 수 없") || has(text,L"받아들일 수 없") || has(text,L"물러나주") || (facts.known_sides && facts.incoming && facts.result==0);
        if(refused)return (facts.ruler?L"주군께서 ":L"아군 사신이 ")+side+L"의 "+request+(facts.ruler?L" 요청을 거절하셨습니다.":L" 요청을 거절했습니다.");
        return facts.ruler?L"주군께서 교섭에 관한 뜻을 전하셨습니다.":L"아군 사신이 주군의 뜻을 전했습니다.";
    }
    bool departing=has(text,L"물러") || has(text,L"돌아") || has(text,L"기약");
    // A character's anger is evidence of expressed displeasure, not proof of
    // a numerical change in the game's force relationship. Never invent one.
    if(has(text,L"무시") || has(text,L"불쾌") || has(text,L"괘씸") || has(text,L"무례") || has(text,L"노여"))return side+(departing?L"이 불만을 표하고 물러났습니다.":L"이 불만을 표했습니다.");
    if(has(text,L"유감"))return side+L"이 유감을 표했습니다.";
    if(has(text,L"감사") || has(text,L"고맙"))return side+L"이 감사의 뜻을 전했습니다.";
    if(departing)return side+L"이 교섭을 마치고 물러났습니다.";
    return side+L"이 교섭 중 의견을 전했습니다.";
}
// Only an explicit "금 <amount>" token is summed. Dates, officer stats and
// treasure names containing digits never become money. Unparsed text is kept.
inline std::uint64_t gold_amount(const std::wstring &s) {
    size_t pos=0;
    for(;;) {
        pos=s.find(L"금",pos);if(pos==std::wstring::npos)return 0;
        if((pos==0 || s[pos-1]==L' ' || s[pos-1]==L'\n') && pos+1<s.size() && (s[pos+1]==L' ' || (s[pos+1]>=L'0' && s[pos+1]<=L'9')))break;
        ++pos;
    }
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
    if(!e.report.empty())return display_text(e.report);
    std::wstring text=display_text(e.text);
    size_t newline=text.find(L'\n'),comma=text.find(L", ");
    std::wstring head=text.substr(0,newline);
    if(comma!=std::wstring::npos && comma<head.size() && comma>0 && comma<25) {
        bool success=head.size()>=3 && head.compare(head.size()-3,3,L" 성공")==0;
        bool failure=head.size()>=3 && head.compare(head.size()-3,3,L" 실패")==0;
        if(success || failure) {
            std::wstring name=head.substr(0,comma),action=head.substr(comma+2,head.size()-comma-5);
            wchar_t last=name.back();bool batchim=last>=0xAC00 && last<=0xD7A3 && (last-0xAC00)%28;
            std::wstring sentence=name+(batchim?L"이 ":L"가 ")+action;
            if(action.empty() || action.back()!=L'에')sentence+=L"에";
            sentence+=success?L" 성공했습니다.":L" 실패했습니다.";
            if(newline!=std::wstring::npos)sentence+=success?L"\n주공, 임무가 성공해 다행입니다.":L"\n좋은 소식을 전하지 못해 아쉽습니다.";
            return sentence;
        }
    }
    if(e.kind!=Kind::Gold || !e.gold || text.find(L'\n')!=std::wstring::npos)return text;
    auto at=text.find(L"가 금");if(at==std::wstring::npos)at=text.find(L"이 금");
    if(at==std::wstring::npos || at==0 || at>40)return text;
    return text.substr(0,at)+L"  ·  "+std::to_wstring(e.gold);
}
// Only explicit mission-result summaries supply a location. Ambiguous prose is
// kept as separate news; a mission name alone must never merge two cities.
inline bool mission_result(const Event &e,std::wstring &mission,std::wstring &place) {
    std::wstring text=compact(e);
    if(!has(text,L"성공했습니다") && !has(text,L"실패했습니다"))return false;
    size_t at=std::wstring::npos;
    for(const wchar_t *name:{L"건물파괴",L"지역회유",L"유언",L"매복",L"이간"}) {
        at=text.find(name);if(at!=std::wstring::npos){mission=name;break;}
    }
    if(at==std::wstring::npos)return false;
    size_t end=text.rfind(L"에서",at);if(end==std::wstring::npos)return false;
    size_t start=text.find_last_of(L" \n\t",end);start=start==std::wstring::npos?0:start+1;
    place=text.substr(start,end-start);
    return place.size()>=2 && place.size()<=12;
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
            if(e.text.empty() || e.text.size()>text_limit || e.report.size()>text_limit){fallback=true;return false;}
            // Categories affect presentation only. All original text survives.
            if(e.kind==Kind::News)e.kind=classify(display_text(e.text));
            if(e.kind==Kind::Gold)e.gold=gold_amount(display_text(e.text));
        }
        for(auto &e:batch) {
            std::wstring mission,place;
            if(final_report && mission_result(e,mission,place)) {
                auto it=std::find_if(events.begin(),events.end(),[&](const Event &earlier){
                    auto text=display_text(earlier.text);
                    return has(text,mission.c_str()) && has(text,place.c_str());
                });
                if(it!=events.end() && e.text.size()+it->text.size()+2<=text_limit) {
                    e.report=compact(e);e.day=it->day;e.read=it->read;
                    e.text+=L"\n\n"+it->text;*it=std::move(e);continue;
                }
            }
            events.push_back(std::move(e));
        }
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
// User 2026-10-30: confirm two captures before hiding the native adviser.
// Channel order may be RGB or BGR, but must stay identical between samples.
struct PortraitSample {unsigned char cells[16*16][3]{};};
inline bool portrait_sample_from_pixels(const void *pixels,unsigned pitch,int width,int height,PortraitSample &out) {
    if(!pixels || width<=8 || height<=8 || width>4096 || height>4096 || pitch<static_cast<unsigned>(width)*4)return false;
    const auto *bytes=static_cast<const unsigned char *>(pixels);
    unsigned lit=0,total=0,lowest=255,highest=0;
    for(int y=height/8;y<7*height/8;y+=2)for(int x=width/8;x<7*width/8;x+=2){
        const auto *p=bytes+y*pitch+x*4;unsigned rgb=(static_cast<unsigned>(p[0])+p[1]+p[2])/3;
        ++total;if(rgb>16)++lit;lowest=(std::min)(lowest,rgb);highest=(std::max)(highest,rgb);
    }
    if(!total || lit<=total/20 || highest-lowest<=8)return false;
    PortraitSample sample;
    const int left=width/8,top=height/8,w=7*width/8-left,h=7*height/8-top;
    for(int cy=0;cy<16;++cy)for(int cx=0;cx<16;++cx){
        int x0=left+cx*w/16,x1=(std::max)(x0+1,left+(cx+1)*w/16);
        int y0=top+cy*h/16,y1=(std::max)(y0+1,top+(cy+1)*h/16);unsigned sums[3]{};
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)for(unsigned c=0;c<3;++c)sums[c]+=bytes[y*pitch+x*4+c];
        for(unsigned c=0;c<3;++c)sample.cells[cy*16+cx][c]=static_cast<unsigned char>(sums[c]/((x1-x0)*(y1-y0)));
    }
    out=sample;return true;
}
inline bool portrait_samples_stable(const PortraitSample &a,const PortraitSample &b,unsigned &diff) {
    unsigned sums[3]{},maximum=0;
    for(unsigned i=0;i<16*16;++i)for(unsigned c=0;c<3;++c){
        unsigned delta=static_cast<unsigned>(std::abs(static_cast<int>(a.cells[i][c])-b.cells[i][c]));
        sums[c]+=delta;maximum=(std::max)(maximum,delta);
    }
    unsigned largest=(std::max)(sums[0],(std::max)(sums[1],sums[2]));
    diff=(largest+255)/256;return largest<=10*256 && maximum<=40;
}
inline bool portrait_rect_matches(Rect a,Rect b) {
    return std::abs(std::lround(a.x)-std::lround(b.x))<=1 && std::abs(std::lround(a.y)-std::lround(b.y))<=1 &&
           std::abs(std::lround(a.w)-std::lround(b.w))<=1 && std::abs(std::lround(a.h)-std::lround(b.h))<=1;
}
struct PortraitConfirmation {
    PortraitSample sample;Rect rect{};int revision=0;std::uint64_t at=0;bool present=false;
    void reset(){present=false;at=0;}
    bool observe(const PortraitSample &next,int serial,Rect source,std::uint64_t now,unsigned &diff) {
        diff=0;
        if(present && revision==serial && portrait_rect_matches(rect,source)){
            if(now<at || now-at<500)return false;
            if(portrait_samples_stable(sample,next,diff))return true;
        }
        sample=next;rect=source;revision=serial;at=now;present=true;return false;
    }
};
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
    // 2026-10-31 user: the native speech bubble edge showed in the lower part of the captured face.
    // Keep the top, centre horizontally, sample 85% of the box (same aspect) -> the bubble stays outside.
    r.x+=w*0.075f;r.w=w*0.85f;r.h=h*0.85f;
    out=r;return true;
}
// User 2026-10-27: double the window dimensions only, retaining font/button sizes.
inline constexpr Rect portrait{30,956,88,88},bubble{142,30,1160,1000};
inline Rect control(unsigned id,Rect panel=bubble) {
    switch(id) {
    case 2:return {panel.x+panel.w-68,panel.y+31,36,36};
    case 3:return {panel.x+panel.w-324,panel.y+30,44,38};
    case 4:return {panel.x+panel.w-156,panel.y+30,44,38};
    case 5:return {panel.x+panel.w-276,panel.y+panel.h-64,104,38};
    case 6:return {panel.x+panel.w-164,panel.y+panel.h-64,112,38};
    case 7:return {panel.x+panel.w-152,panel.y+160,96,30};
    default:return {};
    }
}
// 사용자 결정 2026-10-28 드래그 헤드: session-only virtual coordinates.
inline float bounded(float v,float lo,float hi){return (std::max)(lo,(std::min)(hi,v));}
struct HeadMotion {
    float x=74,y=1000,start_x=0,start_y=0,offset_x=0,offset_y=0;
    float from_x=74,from_y=1000,target_x=74,target_y=1000;
    double elapsed=0;bool pressed=false,dragging=false,snapping=false;
    void press(float px,float py){pressed=true;dragging=snapping=false;start_x=px;start_y=py;offset_x=x-px;offset_y=y-py;}
    bool move(float px,float py){
        if(!pressed)return false;
        float dx=px-start_x,dy=py-start_y;
        if(!dragging && dx*dx+dy*dy>=64)dragging=true;
        if(dragging){x=bounded(px+offset_x,74,1846);y=bounded(py+offset_y,110,1000);}
        return dragging;
    }
    bool release(){
        bool tap=pressed && !dragging;pressed=false;
        if(dragging){from_x=x;from_y=y;target_x=x<960?74.0f:1846.0f;target_y=bounded(y,110,1000);elapsed=0;snapping=true;}
        dragging=false;return tap;
    }
    void cancel(){pressed=dragging=snapping=false;}
    void advance(double dt){
        if(!snapping)return;elapsed+=dt;
        // Analytic underdamped spring: zeta=.75, settles in approximately 350ms.
        double t=elapsed,omega=22,zeta=.75,wd=omega*std::sqrt(1-zeta*zeta);
        float k=static_cast<float>(std::exp(-zeta*omega*t)*(std::cos(wd*t)+zeta*omega/wd*std::sin(wd*t)));
        x=bounded(target_x+(from_x-target_x)*k,74,1846);y=bounded(target_y+(from_y-target_y)*k,110,1000);
        if(t>=.35){x=target_x;y=target_y;snapping=false;}
    }
};
inline Rect panel_rect(float hx){return {bounded(hx<960?hx+68:hx-68-bubble.w,16,1920-16-bubble.w),bubble.y,bubble.w,bubble.h};}
inline Rect head_rect(const HeadMotion &h,float zoom=1){return {h.x-44*zoom,h.y-44*zoom,88*zoom,88*zoom};}
inline Rect body_rect(Rect p){return {p.x+56,p.y+250,p.w-112,p.h-382};}
inline double ease_back(double t){if(t<=0)return 0;if(t>=1)return 1;double u=t-1;return 1+2.70158*u*u*u+1.70158*u*u;}
inline Rect panel_transform(Rect p,float hx,float hy,double zoom){return {static_cast<float>(hx+(p.x-hx)*zoom),static_cast<float>(hy+(p.y-hy)*zoom),static_cast<float>(p.w*zoom),static_cast<float>(p.h*zoom)};}
struct ScrollMotion {
    double shown=0,target=0,velocity=0,last_y=0,start_y=0,start_shown=0;
    bool pressed=false,dragging=false;
    void reset(){*this=ScrollMotion{};}
    void wheel(int delta,int maximum){target=bounded(static_cast<float>(target+delta),0,static_cast<float>(maximum));velocity=0;}
    void press(float y){pressed=true;dragging=false;last_y=start_y=y;start_shown=shown;velocity=0;}
    void move(float y,double dt,int maximum){
        if(!pressed)return;if(std::fabs(y-start_y)>=6)dragging=true;
        if(dragging){double next=start_shown+start_y-y;if(next<0)next=-48*(1-std::exp(next/120));else if(next>maximum)next=maximum+48*(1-std::exp((maximum-next)/120));
            velocity=(next-shown)/(std::max)(.008,dt);shown=target=next;}
        last_y=y;
    }
    void release(){pressed=dragging=false;target=shown;}
    bool active()const{return dragging || std::fabs(shown-target)>.01 || std::fabs(velocity)>.1 || shown<0;}
    void advance(double dt,int maximum){
        if(pressed)return;
        if(std::fabs(velocity)>.1){target=bounded(static_cast<float>(target+velocity*dt),-48,static_cast<float>(maximum+48));velocity*=std::exp(-10*dt);}
        if(target<0 || target>maximum){target+=(bounded(static_cast<float>(target),0,static_cast<float>(maximum))-target)*(1-std::exp(-24*dt));velocity*=std::exp(-24*dt);}
        shown+=(target-shown)*(1-std::exp(-19.188*dt));
        if(std::fabs(velocity)<.1 && std::fabs(shown-target)<.02){target=bounded(static_cast<float>(target),0,static_cast<float>(maximum));shown=target;velocity=0;}
    }
};
// Coordinates use the same centered 1920x1080 viewport as the existing overlay.
inline bool client_point(int x,int y,int cw,int ch,int bw,int bh,float &vx,float &vy) {
    if(cw<=0 || ch<=0 || bw<=0 || bh<=0)return false;
    double scale=(std::min)(bw/1920.0,bh/1080.0);if(scale<=0)return false;
    vx=static_cast<float>((double(x)*bw/cw-(bw-1920*scale)/2)/scale);
    vy=static_cast<float>((double(y)*bh/ch-(bh-1080*scale)/2)/scale);return true;
}
}
