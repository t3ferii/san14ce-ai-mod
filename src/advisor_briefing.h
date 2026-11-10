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
enum class Kind {News,Weather,Disaster,Growth,Recruitment,Gold,Treasure,Income,Diplomacy,Reward};   // local17: new kinds at the END only
struct Event {Kind kind=Kind::News;std::wstring text;unsigned speaker=0,day=0;std::uint64_t gold=0;bool read=false;std::wstring report,row;};
struct Card {Kind kind=Kind::News;std::vector<Event> events;unsigned unread=0;std::uint64_t gold=0;};
// 사용자 2026-10-28 설계 정정: result-only summaries; no native formatter.
inline std::wstring scene_particle(const std::wstring &name,const wchar_t *closed,const wchar_t *open) {
    wchar_t c=name.empty()?0:name.back();return name+(c>=0xAC00 && c<=0xD7A3 && (c-0xAC00)%28?closed:open);
}
inline std::wstring recruitment_scene_summary(const std::wstring &envoy,const std::wstring &target,bool success) {
    if(envoy.empty() || target.empty())return {};
    return scene_particle(envoy,L"이 ",L"가 ")+target+L" 등용에 "+(success?L"성공했습니다.":L"실패했습니다.");
}
// 2026-11-08 user (FX2), local17 briefing16: a successful recruitment keeps the original success scene visible; failure stays summary-only.
inline bool employ_pass_through(bool success){return success;}
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
// ---- Table rows (user 2026-11-07). Scene and diplomacy events also carry Event::row; the overlay draws tab-separated runs as aligned tables. ----
// Event::row = shape code and four cells, tab separated. "D" discovery (actor, place, found, result), "R" recruitment (envoy, target, city, result),
// "P" diplomacy (counterpart, request, direction, result), "M" military (unit, reason, action), "E" envoy notice (force, content); unused cells stay empty.
// An empty row (or one that fails valid_row) keeps the plain text line. Rows the game does not hand over as data are parsed from the original message
// templates by parse_row (msggame_exce ids in its comments); a message that is not exactly such a template stays a text line.
inline std::wstring table_cell(const std::wstring &raw) {
    std::wstring out;out.reserve(raw.size());
    for(wchar_t c:raw)out+=(c==L'\t' || c==L'\n' || c==L'\r')?L' ':c;
    return out;
}
inline std::vector<std::wstring> split_cells(const std::wstring &line) {
    std::vector<std::wstring> cells;size_t from=0;
    for(;;){size_t stop=line.find(L'\t',from);cells.push_back(line.substr(from,stop==std::wstring::npos?stop:stop-from));if(stop==std::wstring::npos)break;from=stop+1;}
    return cells;
}
inline std::wstring make_row(wchar_t shape,const std::wstring &a,const std::wstring &b,const std::wstring &c,const std::wstring &d) {
    return std::wstring(1,shape)+L"\t"+table_cell(a)+L"\t"+table_cell(b)+L"\t"+table_cell(c)+L"\t"+table_cell(d);
}
// Table shapes in display order (a card shows at most one table per shape).
inline constexpr const wchar_t *kShapes=L"RDPEM";
inline unsigned shape_columns(wchar_t shape){return shape==L'M'?3:shape==L'E'?2:(shape==L'D' || shape==L'R' || shape==L'P')?4:0;}
// A cell that may be empty in a row (dropped from the table when empty everywhere): D place and found, R city.
inline bool optional_cell(wchar_t shape,unsigned column){return shape==L'D'?(column==1 || column==2):shape==L'R'?column==2:false;}
inline bool valid_row(const std::wstring &row) {
    if(row.size()<2 || row[1]!=L'\t' || shape_columns(row[0])==0)return false;
    const auto cells=split_cells(row);
    if(cells.size()!=5)return false;
    for(unsigned i=0;i<4;++i) {
        if(i>=shape_columns(row[0])){if(!cells[i+1].empty())return false;}
        else if(cells[i+1].empty() && !optional_cell(row[0],i))return false;
    }
    return true;
}
inline const wchar_t *table_header(wchar_t shape,unsigned column) {
    static const wchar_t *const discovery[4]={L"무장",L"장소",L"발견",L"결과"};
    static const wchar_t *const recruit[4]={L"실행 무장",L"대상",L"도시",L"결과"};
    static const wchar_t *const diplomacy[4]={L"상대",L"안건",L"구분",L"결과"};
    static const wchar_t *const military[4]={L"부대",L"사유",L"조치",L""};
    static const wchar_t *const envoy[4]={L"세력",L"내용",L"",L""};
    if(column>3)return L"";
    return shape==L'D'?discovery[column]:shape==L'R'?recruit[column]:shape==L'P'?diplomacy[column]:shape==L'M'?military[column]:envoy[column];
}
// One row of a table and the day (1..10, 0 unknown) of the message it came from.
struct TableRow {unsigned day=0;std::wstring row;};
inline std::wstring day_cell(unsigned day){return day?std::to_wstring(day)+L"일차":std::wstring(L"—");}
// Header line plus one line per valid row of the first valid row's shape; empty when nothing valid. Optional columns are dropped when no row fills them.
// days: a first column "N일차" is added and the rows are put in day order (stable: same-day rows keep their arrival order).
inline std::wstring table_text(std::vector<TableRow> rows,bool days) {
    std::vector<std::pair<unsigned,std::vector<std::wstring>>> cells;wchar_t shape=0;
    if(days)std::stable_sort(rows.begin(),rows.end(),[](const TableRow &a,const TableRow &b){return a.day<b.day;});
    for(const auto &r:rows) {
        if(!valid_row(r.row) || (shape && r.row[0]!=shape))continue;
        shape=r.row[0];
        auto next=split_cells(r.row);
        if(shape==L'P' && !cells.empty() && cells.back().first==r.day && cells.back().second==next)continue;   // one negotiation speaks several lines, each carrying the same facts
        cells.push_back({r.day,std::move(next)});
    }
    if(cells.empty())return {};
    const unsigned columns=shape_columns(shape);bool keep[4]={true,true,true,true};
    for(unsigned col=0;col<columns;++col)if(optional_cell(shape,col)) {
        keep[col]=false;for(const auto &c:cells)if(!c.second[col+1].empty())keep[col]=true;
    }
    std::wstring out=days?std::wstring(L"일차"):std::wstring();
    for(unsigned col=0;col<columns;++col){if(!keep[col])continue;if(!out.empty())out+=L"\t";out+=table_header(shape,col);}
    for(const auto &c:cells) {
        out+=L"\n";bool first=true;
        if(days){out+=day_cell(c.first);first=false;}
        for(unsigned col=0;col<columns;++col) {
            if(!keep[col])continue;
            if(!first)out+=L"\t";
            first=false;out+=c.second[col+1].empty()?std::wstring(L"—"):c.second[col+1];
        }
    }
    return out;
}
inline std::wstring table_block(const std::vector<std::wstring> &rows) {
    std::vector<TableRow> list;for(const auto &r:rows)list.push_back({0,r});
    return table_text(std::move(list),false);
}
inline std::wstring recruitment_scene_row(const std::wstring &envoy,const std::wstring &target,bool success) {
    if(envoy.empty() || target.empty())return {};
    return make_row(L'R',envoy,target,L"",success?L"성공":L"실패");
}
// Same fields as discovery_scene_summary. Person discoveries (type 1) are recruitment rows, the rest discovery rows.
inline std::wstring discovery_scene_row(unsigned type,const std::wstring &actor,const std::wstring &found,const std::wstring &place,bool success) {
    if(type>5 || actor.empty() || found.empty())return {};
    switch(type) {
    case 0:return make_row(L'D',actor,place,found,L"명품 발견");
    case 1:return make_row(L'R',actor,found,place,success?L"발견 후 등용 성공":L"발견 후 등용 실패");
    case 2:return make_row(L'D',actor,place,found,L"물품 발견");
    case 3:return make_row(L'D',actor,place,found,L"지리 이점 발견");
    case 4:return make_row(L'D',actor,place,found,L"지리 이점 상실");
    default:return make_row(L'D',actor,place,found,L"명승 발견");
    }
}
// Numeric cells (optional enclosing parentheses, optional sign, digits, commas, optional trailing %) align right.
inline bool numeric_cell(const std::wstring &s) {
    size_t i=0,end=s.size(),digits=0;
    if(end>=2 && s[0]==L'(' && s[end-1]==L')'){i=1;--end;}
    if(i<end && (s[i]==L'+' || s[i]==L'-'))++i;
    for(;i<end;++i) {
        if(s[i]>=L'0' && s[i]<=L'9')++digits;
        else if(s[i]==L',' && digits)continue;
        else if(s[i]==L'%' && i+1==end && digits)continue;
        else return false;
    }
    return digits>0;
}
// rows[0] is the header. A column is numeric when every body cell is a number or an em dash and at least one is a number.
inline bool column_numeric(const std::vector<std::vector<std::wstring>> &rows,size_t column) {
    bool any=false;
    for(size_t r=1;r<rows.size();++r) {
        const std::wstring empty,&cell=column<rows[r].size()?rows[r][column]:empty;
        if(numeric_cell(cell))any=true;else if(cell!=L"—" && !cell.empty())return false;
    }
    return any;
}
// 0 plain, 1 positive (green), 2 negative (red): non-zero signed numbers and result words.
inline int cell_tone(const std::wstring &s) {
    if(numeric_cell(s)) {
        const size_t at=s[0]==L'('?1:0;bool nonzero=false;
        for(wchar_t c:s)if(c>=L'1' && c<=L'9')nonzero=true;
        return !nonzero?0:s[at]==L'+'?1:s[at]==L'-'?2:0;
    }
    auto in=[&](const wchar_t *word){return s.find(word)!=std::wstring::npos;};
    if(in(L"실패") || in(L"불발") || in(L"거절") || in(L"상실") || in(L"거부") || in(L"미달성") || in(L"단념"))return 2;
    if(in(L"성공") || in(L"성사") || in(L"수락") || in(L"승낙") || in(L"달성"))return 1;
    return 0;
}
// Column widths in pixels for natural (content) widths: all natural when they fit (spare room spread equally, at most max_extra per column), otherwise
// water-filled so short columns keep their width and the long ones share the rest (cells then wrap). Total never exceeds available - gaps.
inline std::vector<int> table_column_widths(const std::vector<int> &natural,int available,int gap,int min_width,int max_extra) {
    const int n=static_cast<int>(natural.size());
    std::vector<int> width(natural.size(),0);
    if(n==0)return width;
    const int room=(std::max)(n*min_width,available-gap*(n-1));
    int sum=0;for(int w:natural)sum+=(std::max)(min_width,w);
    if(sum<=room) {
        const int extra=(std::min)(max_extra,(room-sum)/n);
        for(int i=0;i<n;++i)width[i]=(std::max)(min_width,natural[i])+extra;
        return width;
    }
    std::vector<bool> fixed(natural.size(),false);int left=room,open=n;
    for(bool again=true;again && open>0;) {
        again=false;const int share=left/open;
        for(int i=0;i<n;++i)if(!fixed[i] && (std::max)(min_width,natural[i])<=share){width[i]=(std::max)(min_width,natural[i]);fixed[i]=true;left-=width[i];--open;again=true;}
    }
    if(open>0){const int share=left/open;for(int i=0;i<n;++i)if(!fixed[i])width[i]=share;}
    return width;
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
    case Kind::Income:return L"수입과 지출";case Kind::Diplomacy:return L"외교 보고";
    case Kind::Reward:return L"포상 위임";default:return L"전해 온 소식";
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
// 2026-11-09 user, local17 briefing16: the original capture scene (msggame 11014-11041 capturer lines, 10781/10783
// "X, Y의 Z측을 포박") plays on screen again; the briefing keeps a copy only.
inline bool capture_scene_line(const std::wstring &s) {
    static const wchar_t *const keys[]={L"포박",L"사로잡",L"생포",L"포로를 데리고",L"포로를 붙잡",L"포로를 연행",L"포로를 잡아",L"포로로 잡"};
    for(const wchar_t *k:keys)if(s.find(k)!=std::wstring::npos)return true;
    return false;
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
// Diplomacy table row ("P"): only a friendly negotiation whose sides and result are known from the game state (never guessed from the speech).
// Any other diplomacy message (anger, thanks, unknown sides) keeps its text line.
inline std::wstring diplomatic_row(const DiplomaticFacts &facts) {
    if(!facts.friendly || !facts.known_sides || (facts.result!=0 && facts.result!=1))return {};
    const std::wstring side=facts.counterpart.empty()?std::wstring(L"상대"):display_text(facts.counterpart);
    const wchar_t *result=facts.incoming?(facts.result==1?L"수락":L"거절"):(facts.result==1?L"성사":L"불발");
    return make_row(L'P',side,diplomatic_request(facts.kind),facts.incoming?L"상대 요청":L"아군 제안",result);
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
// ---- Rows from message text. Templates: msggame_exce (0001_KO) ids, "<..>" = a name filled in by the game. All patterns match the whole message. ----
inline std::wstring flat_text(const std::wstring &raw) {
    std::wstring text=display_text(raw),out;bool space=true;
    for(wchar_t c:text) {
        if(c==L'\n' || c==L'\r' || c==L'\t')c=L' ';
        if(c==L' '){if(!space)out+=c;space=true;}else{out+=c;space=false;}
    }
    while(!out.empty() && out.back()==L' ')out.pop_back();
    return out;
}
inline bool ends_with(const std::wstring &s,const std::wstring &tail){return s.size()>=tail.size() && s.compare(s.size()-tail.size(),tail.size(),tail)==0;}
inline bool starts_with(const std::wstring &s,const std::wstring &head){return s.compare(0,head.size(),head)==0;}
// One word (a name): 1..16 characters, no space, comma or sentence mark.
inline bool plain_name(const std::wstring &s){return !s.empty() && s.size()<=16 && s.find_first_of(L" ,.!?")==std::wstring::npos;}
// A log line "<name>, <rest>": the single-line template of the turn report. false when the message has more than one line or no ", ".
inline bool log_line(const std::wstring &raw,std::wstring &name,std::wstring &rest) {
    const std::wstring shown=display_text(raw);
    if(shown.find(L'\n')!=std::wstring::npos)return false;
    const std::wstring text=flat_text(raw);const size_t at=text.find(L", ");
    if(at==std::wstring::npos)return false;
    name=text.substr(0,at);rest=text.substr(at+2);
    return plain_name(name) && !rest.empty();
}
// "<rest>" = "<name><suffix>": the name in front of a fixed ending.
inline bool before_suffix(const std::wstring &rest,const wchar_t *suffix,std::wstring &name) {
    const std::wstring tail=suffix;
    if(!ends_with(rest,tail) || rest.size()==tail.size())return false;
    name=rest.substr(0,rest.size()-tail.size());
    return plain_name(name);
}
// 50065/50066/50161/50162 "<official>, <person>의 등용에 성공|실패"; 50089 "<official>, <place>의 탐색에 실패"; 50100 "...의 탐색을 단념";
// 111/50077/51579 "<official>, <thing> 발견" (not the gold line 50079); 12850 "<unit>, <reason>에 의해 퇴각"; 13508 "...에 의해 출진불가";
// 13081/13088 "<force>, <force>의 교환요청 승낙|거부", 12729/12740 공격요청, 12752 포로반환 승낙, 12764 반환요청 거부 (the first force answers the
// second force's request); 12708/12709 "<force>, <force>측과 동맹 (연장 )실패" (the first force proposed).
// Envoy notice 50222 (last variant) "보고드립니다.\n<force>의 사자가\n<ruler>께 알현을 요청하였습니다." plus its other variants.
inline std::wstring parse_row(const std::wstring &raw) {
    std::wstring who,rest,name;
    if(log_line(raw,who,rest)) {
        const struct {const wchar_t *tail;bool success;} hire[]={{L"의 등용에 성공",true},{L"의 등용에 실패",false}};
        for(const auto &h:hire)if(before_suffix(rest,h.tail,name))return recruitment_scene_row(who,name,h.success);
        if(before_suffix(rest,L"의 탐색에 실패",name))return make_row(L'D',who,name,L"",L"실패");
        if(before_suffix(rest,L"의 탐색을 단념",name))return make_row(L'D',who,name,L"",L"단념");
        const struct {const wchar_t *tail,*request;} answer[]={{L"의 교환요청 승낙",L"교환"},{L"의 교환요청 거부",L"교환"},{L"의 공격요청 승낙",L"공격"},
            {L"의 공격요청 거부",L"공격"},{L"의 포로반환 승낙",L"포로 반환"},{L"의 반환요청 거부",L"포로 반환"}};
        for(const auto &a:answer)if(before_suffix(rest,a.tail,name))
            return make_row(L'P',name,a.request,who+L" 응답",ends_with(rest,L"승낙")?L"승낙":L"거부");
        const struct {const wchar_t *tail,*request;} alliance[]={{L"측과 동맹 연장 실패",L"동맹 연장"},{L"측과 동맹 실패",L"동맹"}};
        for(const auto &a:alliance)if(before_suffix(rest,a.tail,name))return make_row(L'P',name,a.request,who+L" 제안",L"실패");
        for(const wchar_t *tail:{L"에 의해 퇴각",L"에 의해 출진불가"})
            if(before_suffix(rest,tail,name))return make_row(L'M',who,name,ends_with(rest,L"퇴각")?L"퇴각":L"출진불가",L"");
        if(ends_with(rest,L" 발견") && rest.size()>3 && !starts_with(rest,L"금 ")) {
            name=rest.substr(0,rest.size()-3);
            if(name.size()<=24 && name.find(L',')==std::wstring::npos)return make_row(L'D',who,L"",name,L"발견");
        }
    }
    const std::wstring text=flat_text(raw);
    const size_t at=text.find(L"의 사자가 ");
    if(at==std::wstring::npos)return {};
    const size_t from=text.rfind(L' ',at);
    const std::wstring force=text.substr(from==std::wstring::npos?0:from+1,at-(from==std::wstring::npos?0:from+1));
    const std::wstring lead=from==std::wstring::npos?std::wstring():text.substr(0,from);
    // Before the force name only the greeting and at most one speaker name (one short word) may stand.
    bool speaker=false;
    for(size_t i=0,stop;i<lead.size();i=stop+1) {
        stop=lead.find(L' ',i);if(stop==std::wstring::npos)stop=lead.size();
        std::wstring word=lead.substr(i,stop-i);
        if(!word.empty() && word.back()==L',')word.pop_back();
        if(word==L"보고드립니다.")continue;
        if(speaker || !plain_name(word) || word.size()>6)return {};
        speaker=true;
    }
    const std::wstring tail=text.substr(at+std::wstring(L"의 사자가 ").size());
    if(!plain_name(force) || force.size()<2)return {};
    if(tail==L"뵙기를 청하고 있습니다.")return make_row(L'E',force,L"알현 요청",L"",L"");
    if(tail==L"찾아왔습니다.")return make_row(L'E',force,L"방문",L"",L"");
    if(tail==L"방문했습니다." || tail==L"방문했습니다. 만나러 가보시지요.")return make_row(L'E',force,L"방문",L"",L"");
    const size_t ask=tail.find(L"께 알현을 요청하였습니다.");
    if(ask!=std::wstring::npos && ask+std::wstring(L"께 알현을 요청하였습니다.").size()==tail.size() && plain_name(tail.substr(0,ask)))
        return make_row(L'E',force,L"알현 요청",L"",L"");
    return {};
}
// The row a message shows as: its own row when the capture gave one, else (for a plain message) the one parsed from its text.
inline std::wstring event_row(const Event &e) {
    if(!e.row.empty())return valid_row(e.row)?e.row:std::wstring();
    return e.report.empty()?parse_row(e.text):std::wstring();
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
// Body text of one card (user 2026-11-07; the 2026-11-08 request removed the original-text view). Every message with a row
// (Event::row, else parsed from its text) goes into ONE table per shape, wherever it stood among the other messages; tables
// come first (kShapes order), each with a first column "N일차" in day order. Messages without a row follow as "• day · text"
// lines in arrival order, so none is dropped. The monthly income tables stay per corps and day: their caption line carries
// the day ("• N일차 · title"), so they get no day column.
inline std::wstring card_body(const Card &card) {
    std::wstring body;
    const bool complete_gold=std::all_of(card.events.begin(),card.events.end(),[](const Event &e){return e.gold!=0;});
    if(card.kind==Kind::Gold && card.gold && complete_gold)body=L"발견한 금 합계 "+std::to_wstring(card.gold)+L"\n\n";
    std::vector<bool> tabled(card.events.size(),false);
    for(const wchar_t *shape=kShapes;*shape;++shape) {
        std::vector<TableRow> rows;
        for(size_t i=0;i<card.events.size();++i) {
            const std::wstring row=event_row(card.events[i]);
            if(!row.empty() && row[0]==*shape){rows.push_back({card.events[i].day,row});tabled[i]=true;}
        }
        const std::wstring table=table_text(std::move(rows),true);
        if(!table.empty())body+=table+L"\n\n";
    }
    for(size_t i=0;i<card.events.size();++i) {
        if(tabled[i])continue;
        const Event &e=card.events[i];
        body+=L"• "+(e.day?std::to_wstring(e.day)+L"일차 · ":std::wstring())+compact(e)+(card.kind==Kind::Gold?L"\n":L"\n\n");
    }
    return body;
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
// Window look (user 2026-11-07, "like the original dialog": dark navy panel, thin gold frame, gold title, off-white text). All colours are rgb(r,g,b).
// The gold, title, text and dim colours are the ones the mod's other dark dialog already uses (native_ccz_preview.inl: border 96805B, rule 65553B,
// title ECD5A5, text EEE4CF, secondary B8AF9C, accent C9AD75); the navy fills are new (the original UI's own colours live in its textures, not in code).
namespace look {
inline constexpr unsigned rgb(unsigned r,unsigned g,unsigned b){return (r<<16)|(g<<8)|b;}   // 0xRRGGBB packed, as the DIB pixel and COLORREF conversions expect
inline constexpr float kPanelOpacity=0.93f;                       // the panel lets a little of the game scene through
inline constexpr unsigned kNavyTop=rgb(22,33,58),kNavyBottom=rgb(14,22,40);  // panel gradient
inline constexpr unsigned kBandTop=rgb(38,58,99),kBandBottom=rgb(27,43,77);  // title band gradient
inline constexpr unsigned kInset=rgb(11,18,36);                         // body area (and its frame fill)
inline constexpr unsigned kRowTint=rgb(18,29,54),kHeaderFill=rgb(31,50,88);  // alternate table rows, table header row
inline constexpr unsigned kGoldBorder=rgb(150,128,91),kGoldDim=rgb(101,85,59),kGold=rgb(201,173,117);
inline constexpr unsigned kTitle=rgb(236,213,165),kText=rgb(238,228,207),kDim=rgb(184,175,156),kDimmer=rgb(145,134,114);
inline constexpr unsigned kButtonFill=rgb(27,42,74),kButtonHover=rgb(44,66,114),kButtonDown=rgb(15,24,48),kButtonHoverText=rgb(255,242,212);
inline constexpr unsigned kPositive=rgb(130,214,154),kNegative=rgb(240,138,122);
inline constexpr unsigned kNames[6]={rgb(110,200,220),rgb(235,130,130),rgb(140,170,240),rgb(130,210,140),rgb(200,150,230),rgb(235,180,100)};   // officer names, indexed by name_color_index
// Table metrics in body texture pixels (the text column starts at 56).
inline constexpr int kTablePad=16,kCellGap=28,kMinColumn=48,kMaxExtra=60,kLinePitch=34,kRowPad=8,kTableGap=18;
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
// 2026-11-08 user: the real-estate button replaces the report body with the owned-castle list.
// Three columns of 34px rows fit all 51 cities inside the 618px body window.
inline constexpr int kCityColumns=3,kCityPitch=34;
inline Rect city_cell(Rect p,unsigned index,unsigned count){
    Rect body=body_rect(p);
    if(!count || index>=count)return {};
    const float width=body.w/kCityColumns;
    return {body.x+static_cast<float>(index%kCityColumns)*width,body.y+static_cast<float>(index/kCityColumns)*kCityPitch,width,static_cast<float>(kCityPitch)};
}
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
