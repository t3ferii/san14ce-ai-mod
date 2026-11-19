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
// hex: the original log line's place (entry +0x84 when +0x82 bit 0x10; 671260 moves the camera there on a click), 48400 = none.
// topic (local273): the Topic the capture decided (the calling original function, else the wording); 255 = decided later from the text.
struct Event {Kind kind=Kind::News;std::wstring text;unsigned speaker=0,day=0;std::uint64_t gold=0;bool read=false;std::wstring report,row;unsigned hex=48400;unsigned char topic=255;};
struct Card {Kind kind=Kind::News;std::vector<Event> events;unsigned unread=0;std::uint64_t gold=0;unsigned tab=0;};   // tab: report_tab (Inbox::tabs only)
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
    // local64 (user 2026-11-12: the scene after beating an enemy in the field did not play). Every variant of the
    // capturer lines msggame 11012-11027 and the captive lines 10798-10813 / 10977-10983 / 11008-11009 (origlib) is
    // covered now; the first list missed e.g. "적장을 붙잡았다", "포로를 잡았다", "포로를 어떻게 할지 ...".
    static const wchar_t *const keys[]={L"포박",L"사로잡",L"생포",L"포로를 데리고",L"포로를 붙잡",L"포로를 연행",L"포로를 잡아",L"포로로 잡",
        L"붙잡아 왔",L"때려잡",L"포로 신세",L"적도 오합지졸",L"붙잡은 적장",L"이 포로를",L"포로를 어떻게",L"적장은 내 힘 앞에",L"구속한 적",
        L"포로의 처우",L"붙잡은 포로",L"적장을 붙잡",L"포로를 잡았",L"적장을 잡아오",
        L"날 붙잡",L"나를 붙잡",L"붙잡히다니",L"붙잡혀",L"가두어둔다고",L"갇혀있을",L"패장",L"패배는 인정",L"적의 수중",L"포로의 몸",L"포로가 되",
        L"이런 추태",L"살아서 수모",L"줄을 조금만 느슨",L"밧줄이 너무",L"어서 풀",L"당장 풀",L"썩 풀지",L"이 몸을 묶어",L"결코 항복할 수는",
        L"나를 죽일 생각",L"어서 죽여라",L"이런 꼴",L"이 무슨 꼴",L"험하게 다루지",L"목숨만은",L"목숨만 붙어있으면",L"목숨이 붙어있어야",
        L"살아만 있으면",L"실책을 범했",L"계책이 다 무용",L"마음대로 해봐라",L"뜻대로 해",L"뜻대로 하십시오",L"각오는 되어 있습니다",
        L"하늘만 아시겠지",L"조용히 있다 보면",L"패자가 입에 담을"};
    for(const wchar_t *k:keys)if(s.find(k)!=std::wstring::npos)return true;
    return false;
}
// local245 (user 2026-11-19): a captive of ours escaping plays natively like the capture scene. msggame_exce 23938-23957 are the
// escapee's lines (portrait), 20985-20995 our officers' report ("측이 에서 도주했다는 보고"), 24123 the upkeep-shortage escape.
inline bool escape_scene_line(const std::wstring &s) {
    static const wchar_t *const keys[]={L"감옥",L"탈옥",L"옥졸",L"잡아두려",L"잡아둘 수",L"달아날 기회",L"달아나야",L"달아나볼까",L"열렸다!",L"열렸구나",
        L"줄행랑",L"삼십육계",L"감시에 틈",L"감시의 눈",L"감시에 빈틈",L"경계가 허술",L"탈출할 때다",L"탈출할 기회",L"탈출토록",L"탈출하도록 하",
        L"탈출할 수 있겠",L"탈출해볼까",L"탈출은 성공",L"돌아가도록 하지",L"돌아가도록 하마",L"도주했다",L"달아난 모양",L"달아났수",L"달아났다고",
        L"탈출해버렸",L"탈출을 했다고",L"포로가 탈출하는"};
    for(const wchar_t *k:keys)if(s.find(k)!=std::wstring::npos)return true;
    return false;
}
// local245 (user 2026-11-19): the food-pay shortage notices (msggame_exce 24125 desertion, 24126 morale) show as the original window.
inline bool food_shortage_line(const std::wstring &s) {
    return s.find(L"군량을 지급하지 못")!=std::wstring::npos || s.find(L"군량이 부족하여")!=std::wstring::npos;
}
// local252 (user 2026-11-19: "the same 부웅 sound keeps ringing and I can't tell what happened"). The notices whose original function
// plays sound 6 (406D30(DAT_27D5718,6,0)) besides 천시 (290E60) and 계략/이간 (2D6460 24247-24249): 봉록·유지비 shortage 2DF350
// (24122/24124), 반 연합 2A5140 (24211-24215), 공격요청 진군 2B94B0 (21426, 24250-24255), 세력 통합 2C3BD0/2D6460 (21422-21425,
// 24238-24245), 우리 장수 포박 2E3020 (21250), 독립·반란 2E8100 (24137, 24152), 관직 불만 741A20 (22909). One-line turn-report forms
// (24127-24130 "... 문제 발생") stay in the log.
inline bool alarm_sound_line(const std::wstring &s) {
    static const wchar_t *const keys[]={L"봉록을 지급하지 못",L"줄 봉록이",L"반 연합",L"연합은 결성되지",L"공격요청을 받고",L"요청에 응한 것은",
        L"우리 군을 향해 진군",L"손을 잡았다고",L"통합을 이루었",L"통합을 이루었다고",L"통합되었습니다",L"세력통합이 이루어",L"좌시하지 않겠다",
        L"지켜보고 있을 수만은",L"사로잡혔다고",L"붙잡혔습니다",L"포로로 잡혔다고",L"반기를 들고",L"반란에 호응",L"관직에 불만"};
    for(const wchar_t *k:keys)if(s.find(k)!=std::wstring::npos)return true;
    return false;
}
// The lines the original shows on screen; the briefing keeps a copy when its topic is collected.
inline bool native_scene_line(const std::wstring &s){return capture_scene_line(s) || escape_scene_line(s) || food_shortage_line(s) || alarm_sound_line(s);}
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
// local240 (user 2026-11-19 "량 부대가 공격온다 ... 이것도 로그처럼 지도로 이동"): the first city name standing as a word in a message,
// or -1. A name must not continue a Hangul word on its left ("다가오" is not 오). A one-syllable name (오) must be followed by a
// particle or a non-Hangul character (오에서, 오의, 오 방면; not 오고); longer names may run into any following text (광릉 방면으로).
inline int city_in_text(const std::wstring &text,const std::vector<std::wstring> &names) {
    auto hangul=[](wchar_t c){return c>=0xAC00 && c<=0xD7A3;};
    static const std::wstring particles=L"에의은는이가을를으로와과방성도";
    size_t best=std::wstring::npos;int found=-1;
    for(size_t n=0;n<names.size();++n) {
        const std::wstring &name=names[n];if(name.empty())continue;
        for(size_t at=text.find(name);at!=std::wstring::npos && at<=best;at=text.find(name,at+1)) {
            if(at && hangul(text[at-1]))continue;
            const size_t end=at+name.size();
            if(name.size()==1 && end<text.size() && hangul(text[end]) && particles.find(text[end])==std::wstring::npos)continue;
            if(at==best && name.size()<=names[static_cast<size_t>(found)].size())break;   // same start: the longer name wins
            best=at;found=static_cast<int>(n);break;
        }
    }
    return found;
}
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
// local99 (user 2026-11-12 "군사보고가 수집하는 항목을 게임 안에서 켜고 끌 수 있으면 ... 수집하는 종류별로 나눠야지
// 명품발견 / 탐색 인재발견 / 명승지 / 포로"). What the player sees, not which original window carried it. A topic that is
// switched off is not stored and its original window, scene or line plays as in the unmodded game.
// local220 (user 2026-11-17 "수집목록을 더 세분해서 나눠야해"): six more topics split from wider ones. The values are the bits
// of [advisor_briefing] collect, so new topics are appended and the old ones keep their numbers; topic_order is the list order.
// local229 (user 2026-11-18 "그밖의 소식이 너무 많은데 좀 세세하게 구분좀해주지"): six topics split from 그 밖의 소식, read from the
// original turn-report lines (msggame_exce one-line "<name>, <rest>" templates): officers' deaths, officer moves, marriages and sworn
// brothers, rise and fall of forces, territory changes, city affairs. Bits 22..27; a value saved before them inherits 그 밖의 소식's bit.
// local273 (user 2026-11-20 "대사로만 수집하는게 있는지 보고 방금 포로탈출처럼 함수 기준으로 좀 더 정확하게 나누고 세분화"): ten topics named
// after the original functions that send them (native_briefing_sources.inl); 군정 입수 (Intel) became 정찰·순시. Bits 28.. live in
// [advisor_briefing] collect2; a save without collect2 gives each one the setting of the topic it came from.
enum class Topic:unsigned {Treasure,Goods,Talent,Employ,Scenery,Advantage,Growth,Captive,Duel,Army,Plot,Weather,Disaster,Diplomacy,Income,Other,
    Gold,SearchFail,EmployFail,Intel,Season,Envoy,Death,Officer,Bond,Rise,Land,City,
    MissionStop,Journey,Coalition,AttackRequest,Emperor,Omen,Shortage,Loyalty,Bandit,Proposal,Count};
// local222: Season is retired and never listed. Nothing produces a 계절 notice: the original message tables name the season only
// in BGM and help texts, and the 4계절 news goes to the history log. Its bit (20) stays unused so saved values keep their meaning.
inline const wchar_t *topic_name(Topic t) {
    switch(t) {
    case Topic::Treasure:return L"명품 발견";case Topic::Goods:return L"물품 발견";case Topic::Talent:return L"인재 발견";
    case Topic::Employ:return L"등용 성공";case Topic::Scenery:return L"명승지 발견";case Topic::Advantage:return L"지리 이점";
    case Topic::Growth:return L"무장 성장";case Topic::Captive:return L"포로";case Topic::Duel:return L"일기토";
    case Topic::Army:return L"부대 소식";case Topic::Plot:return L"계략 결과";case Topic::Weather:return L"천시";
    case Topic::Disaster:return L"재해";case Topic::Diplomacy:return L"외교 보고";case Topic::Income:return L"수입과 지출";
    case Topic::Gold:return L"금 발견";case Topic::SearchFail:return L"탐색 실패";case Topic::EmployFail:return L"등용 실패";
    case Topic::Intel:return L"정찰·순시";case Topic::Season:return L"계절";case Topic::Envoy:return L"사자 방문";
    case Topic::Death:return L"무장 사망";case Topic::Officer:return L"인사 이동";case Topic::Bond:return L"혼인·의형제";
    case Topic::Rise:return L"세력 흥망";case Topic::Land:return L"영토 변화";case Topic::City:return L"도시·내정";
    case Topic::MissionStop:return L"임무 중지·귀환";case Topic::Journey:return L"사절 여정";case Topic::Coalition:return L"반 연합";
    case Topic::AttackRequest:return L"공격요청";case Topic::Emperor:return L"황제·한실";case Topic::Omen:return L"인망·징조";
    case Topic::Shortage:return L"봉록 부족";case Topic::Loyalty:return L"충성·불만";case Topic::Bandit:return L"도적 출현";
    case Topic::Proposal:return L"군사 제안 결과";
    default:return L"그 밖의 소식";
    }
}
// [수집 설정] list order: related topics next to each other.
inline constexpr Topic topic_order[]={Topic::Treasure,Topic::Gold,Topic::Goods,Topic::Talent,Topic::Scenery,Topic::Advantage,Topic::SearchFail,
    Topic::Employ,Topic::EmployFail,Topic::MissionStop,Topic::Growth,Topic::Captive,Topic::Duel,Topic::Army,Topic::Bandit,Topic::Plot,Topic::Intel,
    Topic::Proposal,Topic::Death,Topic::Officer,Topic::Loyalty,Topic::Bond,Topic::Rise,Topic::Coalition,Topic::Emperor,Topic::Land,Topic::City,
    Topic::Shortage,Topic::Omen,Topic::Weather,Topic::Disaster,Topic::Diplomacy,Topic::AttackRequest,Topic::Envoy,Topic::Journey,Topic::Income,
    Topic::Other};
inline constexpr unsigned kTopicListed=sizeof(topic_order)/sizeof(*topic_order);
static_assert(kTopicListed+1==static_cast<unsigned>(Topic::Count),"every topic but the retired Season appears once in the list");
// The topic a split topic came from: a value saved before local220 gives the new topic the setting of that one.
inline Topic topic_parent(Topic t) {
    switch(t) {
    case Topic::Gold:return Topic::Goods;case Topic::EmployFail:return Topic::Employ;case Topic::Season:return Topic::Weather;
    case Topic::Envoy:return Topic::Diplomacy;case Topic::SearchFail:case Topic::Intel:return Topic::Other;
    case Topic::Death:case Topic::Officer:case Topic::Bond:case Topic::Rise:case Topic::Land:case Topic::City:return Topic::Other;   // local229
    // local273: where these notices went by their wording before
    case Topic::MissionStop:case Topic::Journey:case Topic::Loyalty:case Topic::Bandit:case Topic::Proposal:return Topic::Other;
    case Topic::Coalition:case Topic::Emperor:return Topic::Rise;
    case Topic::AttackRequest:return Topic::Diplomacy;case Topic::Omen:return Topic::Weather;case Topic::Shortage:return Topic::City;
    default:return t;
    }
}
inline constexpr unsigned kTopicFirst=28;              // topics 0..27 are the bits of collect, 28.. the bits of collect2 (local273)
inline constexpr unsigned kTopicAll=(1u<<kTopicFirst)-1u;
inline constexpr unsigned kTopicAll2=(1u<<(static_cast<unsigned>(Topic::Count)-kTopicFirst))-1u;
inline constexpr unsigned kTopicFormat=0x40000000u;    // marks a value written with the local220 list (a pre-local220 value is <= 0xFFFF)
inline constexpr unsigned kTopicFormat2=0x20000000u;   // local229: also marks the list with bits 22..27 (written together with kTopicFormat)
inline constexpr unsigned kTopicsLocal220=22;          // the topics a kTopicFormat value without kTopicFormat2 knows
static_assert(kTopicFirst<=29 && static_cast<unsigned>(Topic::Count)-kTopicFirst<=29,"topic bits stay below the two format marks");
inline unsigned collect_from_ini(unsigned value) {
    if(value&kTopicFormat2)return value&kTopicAll;
    const unsigned known=value&kTopicFormat?kTopicsLocal220:16u;
    unsigned mask=value&((1u<<known)-1u);
    for(unsigned i=known;i<kTopicFirst;++i)   // a topic split later takes the setting of the one it came from
        if((mask>>static_cast<unsigned>(topic_parent(static_cast<Topic>(i))))&1u)mask|=1u<<i;
    return mask;
}
inline unsigned collect_to_ini(unsigned mask){return kTopicFormat|kTopicFormat2|(mask&kTopicAll);}
// collect2 (local273): kTopicFormat | bits of topics kTopicFirst..; a value without the mark (the key is missing) takes each topic's
// setting from the topic it came from in mask (the collect bits).
inline unsigned collect2_from_ini(unsigned value,unsigned mask) {
    if(value&kTopicFormat)return value&kTopicAll2;
    unsigned out=0;
    for(unsigned i=kTopicFirst;i<static_cast<unsigned>(Topic::Count);++i)
        if((mask>>static_cast<unsigned>(topic_parent(static_cast<Topic>(i))))&1u)out|=1u<<(i-kTopicFirst);
    return out;
}
inline unsigned collect2_to_ini(unsigned mask2){return kTopicFormat|(mask2&kTopicAll2);}
// Result cell (the 4th) of a table row; empty when the row is not one.
inline std::wstring row_result(const std::wstring &row) {
    if(row.size()<2 || row[1]!=L'\t')return {};
    const auto cells=split_cells(row);
    return cells.size()==5?cells[4]:std::wstring();
}
// The topic a table row decides (Event::row or a row parsed from a template); Topic::Count when the row does not decide it.
inline Topic topic_of_row(const std::wstring &row) {
    if(row.size()<2 || row[1]!=L'\t')return Topic::Count;
    const std::wstring result=row_result(row);
    switch(row[0]) {
    case L'M':return Topic::Army;
    case L'R':return has(result,L"발견")?Topic::Talent:has(result,L"실패")?Topic::EmployFail:Topic::Employ;   // "발견 후 등용 ..." = discovery_scene_row type 1
    case L'P':return Topic::Diplomacy;
    case L'E':return Topic::Envoy;
    case L'D':
        if(has(row,L"명품"))return Topic::Treasure;
        if(has(row,L"명승"))return Topic::Scenery;
        if(has(row,L"지리"))return Topic::Advantage;
        if(has(row,L"물품"))return Topic::Goods;
        if(result==L"실패" || result==L"단념")return Topic::SearchFail;   // 50089 / 50100
        break;
    default:break;
    }
    return Topic::Count;
}
// local229: a notice no older frame claims, by the words of the original turn-report lines (msggame_exce one-line templates, e.g.
// 10798 "의 , 사망", 24089 "에서 하야", 34785 "의 포로가 됨", 1 ", 멸망", 10 "의  제압", 24407 "병사감소로,  퇴각개시", 20348 "치안 저하").
// Order matters: a duel line that ends in a capture or a death is a duel, a death that brings a surrender is a death.
inline Topic news_topic(const std::wstring &s) {
    const auto any=[&](std::initializer_list<const wchar_t *> words){for(const wchar_t *w:words)if(has(s,w))return true;return false;};
    if(any({L"단기접전",L"일기토"}))return Topic::Duel;
    if(any({L"포박",L"포로",L"해방"}))return Topic::Captive;
    // 병사 is also "soldiers" (24407 병사감소로 퇴각개시, 20463 "의 , 병사  보충"): only the death forms count (10799 "의 , 병사",
    // 10803 ", 에서 병사", 45 "병사하고")
    const bool illness=(any({L"에서 병사",L"병사하고",L"병사했"}) || ends_with(s,L", 병사")) && !any({L"보충",L"감소"});
    if(any({L"사망",L"전사",L"처형",L"살해",L"주살"}) || illness)return Topic::Death;
    if(any({L"혼인",L"의형제",L"배우자"}))return Topic::Bond;
    // local273: the topics named after the sending functions, for lines that come without one (ten-day report lines, unknown callers)
    if(has(s,L"공격요청"))return Topic::AttackRequest;
    if(has(s,L"연합"))return Topic::Coalition;   // 반◇ 연합 (12802-12821, 24200-24220)
    if(any({L"황제",L"한실",L"헌제",L"후한",L"폐위"}))return Topic::Emperor;
    if(any({L"흉조",L"길조",L"기린",L"인망"}))return Topic::Omen;
    if(has(s,L"봉록"))return Topic::Shortage;
    if(any({L"충성이 저하",L"충성 저하",L"불만"}))return Topic::Loyalty;
    if(any({L"도적",L"비적",L"산적",L"수적"}))return Topic::Bandit;
    if(has(s,L"사절단"))return Topic::Journey;
    if(any({L"동맹",L"친선",L"항복권고",L"강화 체결",L"교전 개시"}))return Topic::Diplomacy;
    if(has(s,L"의 제안"))return Topic::Proposal;   // 20408 "<officer>의 제안이었던 ...", 20465 "의 제안(...) 실행 중지"
    if(any({L"정찰",L"순시"}))return Topic::Intel;
    if(any({L"멸망",L"거병",L"독립",L"항복",L"통합",L"종속",L"군주로 취임",L"즉위",L"국호",L"자칭",L"옹립",L"선양",L"찬탈",L"칙명",L"봉작",L"봉해짐",L"쇠퇴",L"대두",L"건국",L"작위"}))return Topic::Rise;
    if(any({L"제압",L"점령",L"탈취",L"함락",L"탈환",L"평정",L"지역이점",L"거점을 이동"}))return Topic::Land;
    // battle before officers: 24401 "<force> 소속 <unit> 격파 후" is a battle, not a transfer
    if(any({L"부대",L"격파",L"퇴각",L"진군",L"공격개시",L"출진",L"패배",L"패해",L"대패",L"격퇴",L"전투",L"철수",L"공략",L"건설개시",L"건설완료",L"보수개시",L"보수완료",L"상태로 변화",L"상태가",L"에서 회복"}))return Topic::Army;
    if(any({L"사관",L"하야",L"전향",L"투항",L"탈출",L"추방",L"재야",L"소속",L"해임",L"임무",L"임명",L"취임",L"발탁",L"맞이함",L"건강을 회복"}))return Topic::Officer;
    if(any({L"치안",L"순시",L"군량",L"보충",L"내구",L"증축",L"인구",L"풍작",L"지역개발"}))return Topic::City;
    return Topic::Other;
}
// A plain notice has no type of its own: the topic comes from its wording (the same frames the report tables use).
inline Topic topic_of_text(const std::wstring &raw) {
    const std::wstring s=display_text(raw);
    if(capture_scene_line(s) || escape_scene_line(s))return Topic::Captive;
    const std::wstring row=parse_row(raw);
    if(!row.empty()){const Topic t=topic_of_row(row);if(t!=Topic::Count)return t;}
    if(has(s,L"건물파괴") || has(s,L"계략") || has(s,L"유언비어") || has(s,L"이간"))return Topic::Plot;
    // local229: the other plots of the original log (12502 이호경식, 12523 구호탄랑, 12583 매복공작, 12587 매복지독, 12599 지역회유, 51564 위보,
    // 50001 화계). "매복기동" (the mod's own ambush march) is not a plot result and does not match.
    if(has(s,L"이호경식") || has(s,L"구호탄랑") || has(s,L"매복공작") || has(s,L"매복지독") || has(s,L"지역회유") || has(s,L"위보") || has(s,L"화계"))return Topic::Plot;
    if(has(s,L"군정") && (has(s,L"입수") || has(s,L"정찰")))return Topic::Intel;   // 13373 정찰 성공, 13387 "측이 우리 군을 정찰하고 군정을 입수"
    if(has(s,L"탐색은") && has(s,L"성과를 거두지 못"))return Topic::SearchFail;   // 50090
    if(has(s,L"자금을 발견"))return Topic::Gold;   // 50078 "님이 <곳> 탐색 중에 <금>의 자금을 발견했습니다."
    switch(classify(s)) {
    case Kind::Disaster:return Topic::Disaster;
    case Kind::Weather:return Topic::Weather;
    case Kind::Treasure:return Topic::Treasure;
    case Kind::Recruitment:return has(s,L"발견")?Topic::Talent:has(s,L"실패") || has(s,L"중지")?Topic::EmployFail:Topic::Employ;
    case Kind::Growth:return Topic::Growth;
    case Kind::Gold:return Topic::Gold;
    default:return news_topic(s);
    }
}
// ---- local222 report view (user 2026-11-17 "디자인도 지금방식에서 저방식으로 바꿔야해. 저렇게 무장이름에 초상화나오는 방식이면 더좋아.
// 지금처럼 색만 입히는게 아니라"): one row per message with the person's face, the name, boxed chips, a one-line summary and the day;
// the selected row's whole text and the same name's other messages of the cycle fill the panel below. The reference screen's chart
// has no counterpart in a ten-day report, so the panel shows the original wording and the record list instead (nothing invented).
inline const wchar_t *tab_caption(Kind k) {
    switch(k) {
    case Kind::Weather:return L"천시";case Kind::Disaster:return L"재해";case Kind::Growth:return L"성장";case Kind::Recruitment:return L"인재";
    case Kind::Gold:return L"수확";case Kind::Treasure:return L"명품";case Kind::Income:return L"수입";case Kind::Diplomacy:return L"외교";
    case Kind::Reward:return L"포상";default:return L"기타";   // local228 (user-approved proposal): "소식" read as every message
    }
}
struct ReportChip {std::wstring text;unsigned tone=0;};   // tone 0 plain box, 1 positive (teal), 2 negative (red)
inline bool operator==(const ReportChip &a,const ReportChip &b){return a.tone==b.tone && a.text==b.text;}
inline bool operator!=(const ReportChip &a,const ReportChip &b){return !(a==b);}
// person: the original person id whose face goes in the square (0 = the drawn initial). named: the name is a person, force, unit or
// corps the message is about, not the topic standing in for a missing name.
struct ReportRow {std::wstring name,summary,day,detail,topic;std::vector<ReportChip> chips;unsigned person=0;bool unread=false,named=false,place=false;};   // place: local243 [지도 이동]
inline bool operator==(const ReportRow &a,const ReportRow &b) {
    return a.person==b.person && a.unread==b.unread && a.named==b.named && a.name==b.name && a.summary==b.summary && a.day==b.day && a.detail==b.detail && a.topic==b.topic && a.chips==b.chips;
}
inline bool operator!=(const ReportRow &a,const ReportRow &b){return !(a==b);}
// The topic of a stored message: its row first, then the income and gold kinds, then the wording, then the kind it was grouped under.
inline Topic topic_of_event(const Event &e) {
    if(e.topic<static_cast<unsigned>(Topic::Count))return static_cast<Topic>(e.topic);   // local273: decided at capture
    const std::wstring row=event_row(e);
    if(!row.empty()){const Topic t=topic_of_row(row);if(t!=Topic::Count)return t;}
    if(e.kind==Kind::Income)return Topic::Income;
    if(e.kind==Kind::Gold)return Topic::Gold;
    const Topic t=topic_of_text(e.report.empty()?e.text:e.report);
    if(t!=Topic::Other)return t;
    switch(e.kind) {
    case Kind::Weather:return Topic::Weather;case Kind::Disaster:return Topic::Disaster;case Kind::Growth:return Topic::Growth;
    case Kind::Treasure:return Topic::Treasure;case Kind::Diplomacy:return Topic::Diplomacy;case Kind::Recruitment:return Topic::Employ;
    default:return Topic::Other;
    }
}
// local229 report tabs (user 2026-11-18 "그밖의 소식이 너무 많은데 좀 세세하게 구분좀해주지.. 탭이 너무 길어질거같으면 2줄로하면 되는데"): one tab
// per topic of the [수집 설정] list instead of one per Kind, so 기타 holds only what no topic names. 등용 실패 sits with 등용 성공, and the
// commendation done in the lord's stead (Kind::Reward, auto_commands) keeps its own tab, listed last.
inline constexpr unsigned kTabReward=100;
inline constexpr unsigned kTabLimit=16;   // topic tabs at most (with 전체 and local232's 로그, kReportTabs); past it the least-used fold into 기타
inline unsigned report_tab(const Event &e) {
    if(e.kind==Kind::Reward)return kTabReward;
    const Topic t=topic_of_event(e);
    return static_cast<unsigned>(t==Topic::EmployFail?Topic::Employ:t==Topic::Season?Topic::Weather:t);
}
inline const wchar_t *report_tab_caption(unsigned tab) {   // short: nine tabs share a 1112 px line
    if(tab==kTabReward)return L"포상";
    switch(static_cast<Topic>(tab)) {
    case Topic::Treasure:return L"명품";case Topic::Gold:return L"금";case Topic::Goods:return L"물품";case Topic::Talent:return L"인재";
    case Topic::Scenery:return L"명승";case Topic::Advantage:return L"지리";case Topic::SearchFail:return L"탐색";case Topic::Employ:return L"등용";
    case Topic::Growth:return L"성장";case Topic::Captive:return L"포로";case Topic::Duel:return L"일기토";case Topic::Army:return L"부대";
    case Topic::Plot:return L"계략";case Topic::Intel:return L"정찰";case Topic::Death:return L"사망";case Topic::Officer:return L"인사";
    case Topic::Bond:return L"인연";case Topic::Rise:return L"세력";case Topic::Land:return L"영토";case Topic::City:return L"도시";
    case Topic::Weather:return L"천시";case Topic::Disaster:return L"재해";case Topic::Diplomacy:return L"외교";case Topic::Envoy:return L"사자";
    case Topic::Income:return L"수입";
    case Topic::MissionStop:return L"중지";case Topic::Journey:return L"사절";case Topic::Coalition:return L"연합";case Topic::AttackRequest:return L"요청";
    case Topic::Emperor:return L"황실";case Topic::Omen:return L"징조";case Topic::Shortage:return L"봉록";case Topic::Loyalty:return L"충성";
    case Topic::Bandit:return L"도적";case Topic::Proposal:return L"제안";
    default:return L"기타";
    }
}
inline std::wstring report_tab_title(unsigned tab) {   // the title band: the topic's full name (등용 holds its failures too)
    if(tab==kTabReward)return caption(Kind::Reward);
    if(tab==static_cast<unsigned>(Topic::Employ))return L"등용";
    return topic_name(static_cast<Topic>(tab));
}
inline unsigned tab_rank(unsigned tab) {   // list position: the [수집 설정] order, then 포상
    if(tab==kTabReward)return kTopicListed;
    for(unsigned i=0;i<kTopicListed;++i)if(static_cast<unsigned>(topic_order[i])==tab)return i;
    return kTopicListed-1;   // Topic::Other is last in topic_order
}
// A tab-separated table inside a message (the income summary) as one line: each row's label, stock and non-zero change.
inline std::wstring table_line(const std::wstring &text) {
    std::wstring out;bool header=true;
    for(size_t start=0;start<text.size();) {
        size_t end=text.find(L'\n',start);if(end==std::wstring::npos)end=text.size();
        const std::wstring line=text.substr(start,end-start);start=end+1;
        if(line.find(L'\t')==std::wstring::npos)continue;
        if(header){header=false;continue;}
        const auto cells=split_cells(line);
        std::wstring item=cells[0];
        if(cells.size()>1 && !cells[1].empty())item+=L" "+cells[1];
        if(cells.size()>2 && cell_tone(cells[2])!=0)item+=L" "+cells[2];
        if(!out.empty())out+=L"  ·  ";
        out+=item;
    }
    return out;
}
// One row. speaker_name: the name of e.speaker's person as the game spells it (empty when unknown); speaker_id: that person's id.
// Name order: the speaker, the row's first cell, the "<name>, ..." log line, the first marked name; else the kind's tab word.
inline ReportRow report_row(const Event &e,const std::wstring &speaker_name=std::wstring(),unsigned speaker_id=0) {
    ReportRow r;const Topic topic=topic_of_event(e);
    r.topic=topic_name(topic);r.day=day_cell(e.day);r.unread=!e.read;
    std::wstring shown=display_text(e.text);
    const bool spoken=!speaker_name.empty() && starts_with(shown,speaker_name+L"\n");   // name_speaker put the speaker's name on the first line
    if(spoken)shown.erase(0,speaker_name.size()+1);
    const std::wstring row=event_row(e);
    if(e.kind==Kind::Income) {
        const std::wstring source=e.report.empty()?shown:display_text(e.report);
        r.name=source.substr(0,source.find(L'\n'));r.summary=table_line(source);r.detail=source;
        r.person=speaker_id;   // local240: the corps row keeps its name (제1군단) and shows its face: 1st corps the 군사, others the 군단장
    } else {
        std::wstring who,rest;
        if(!speaker_name.empty())r.name=speaker_name;
        else if(valid_row(row))r.name=split_cells(row)[1];
        else if(log_line(e.text,who,rest))r.name=who;
        else {std::vector<std::wstring> names;collect_names(e.text,names);if(!names.empty())r.name=names[0];}
        std::wstring line=e.kind==Kind::Gold?shown:compact(e);
        if(spoken && starts_with(line,speaker_name+L"\n"))line.erase(0,speaker_name.size()+1);
        r.summary=flat_text(line);
        const std::wstring report=display_text(e.report);
        r.detail=!report.empty() && report!=shown?report+L"\n\n"+shown:shown;
    }
    r.named=!r.name.empty();
    if(!r.named)r.name=report_tab_caption(report_tab(e));   // local229: the row's tab word (was the kind's)
    if(r.summary.empty())r.summary=flat_text(shown);
    r.chips.push_back({r.topic,static_cast<unsigned>(cell_tone(r.topic))});
    if(valid_row(row) && row[0]==L'P') {
        const std::wstring result=row_result(row);const int tone=cell_tone(result);
        if(tone)r.chips.push_back({result,static_cast<unsigned>(tone)});
    }
    if(r.named && !speaker_name.empty() && r.name==speaker_name)r.person=speaker_id;
    return r;
}
// local243 (user 2026-11-19 "실패 소식은 아래에 깔고 성공 소식을 위에"): a failure row = 등용 실패, 탐색 실패, or a result table row whose
// result word is negative (계략 실패 ...). Report tabs list successes first, then failures, each newest first.
inline bool event_failed(const Event &e) {
    const Topic t=topic_of_event(e);
    if(t==Topic::EmployFail || t==Topic::SearchFail)return true;
    const std::wstring row=event_row(e);
    return valid_row(row) && row[0]==L'P' && cell_tone(row_result(row))==2;
}
// local243 (user 2026-11-19 "일기토/등용성공/포박 은 항상 보임이고 설정으로 on off 할 수 없어"): these scenes always play natively and
// the report always keeps a copy; the 수집 설정 list does not offer them.
inline bool always_collected(Topic t){return t==Topic::Duel || t==Topic::Employ || t==Topic::Captive;}
// Info bar, "label\tvalue" each. total counts every message of the tab (rows may be capped). local228: no name count (it said little).
inline std::vector<std::wstring> report_info(const std::vector<ReportRow> &rows,size_t total,unsigned first_day,unsigned last_day,std::uint64_t gold) {
    unsigned fresh=0;
    for(const auto &r:rows)if(r.unread)++fresh;
    std::vector<std::wstring> out{L"소식\t"+std::to_wstring(total)+L"건",L"새 소식\t"+std::to_wstring(fresh)+L"건"};
    if(first_day)out.push_back(L"기간\t"+(first_day==last_day?std::to_wstring(first_day):std::to_wstring(first_day)+L"~"+std::to_wstring(last_day))+L"일차");
    if(gold)out.push_back(L"금 합계\t"+grouped_number(std::to_wstring(gold)));
    return out;
}
// local228 detail panel heading: the name large, "topic, day" small after it; a row without a name shows its topic as the name.
inline std::wstring report_detail_title(const ReportRow &r){return r.named?r.name:r.topic;}
inline std::wstring report_detail_caption(const ReportRow &r){return r.named?r.topic+L", "+r.day:r.day;}
// local228 구분 column: shown only when the tab's rows have more than one topic (전체, 기타); one topic says nothing per row.
inline bool report_kinds(const std::vector<ReportRow> &rows) {
    for(const auto &r:rows)if(r.topic!=rows.front().topic)return true;
    return false;
}
// The chosen row's other messages this cycle under the same name, newest first, at most `limit`: "day\ttopic\tsummary".
inline std::vector<std::wstring> report_records(const std::vector<ReportRow> &all,const ReportRow &chosen,size_t limit=8) {
    std::vector<std::wstring> out;if(!chosen.named)return out;bool skipped=false;
    for(size_t i=all.size();i-- && out.size()<limit;) {
        const ReportRow &r=all[i];if(r.name!=chosen.name)continue;
        if(!skipped && r.day==chosen.day && r.summary==chosen.summary && r.topic==chosen.topic){skipped=true;continue;}
        out.push_back(r.day+L"\t"+r.topic+L"\t"+r.summary);
    }
    return out;
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
    // local229 report tabs: the tabs present now, in list order. Past kTabLimit the least-used fold into 기타 (Topic::Other).
    std::vector<unsigned> tab_keys()const {
        std::vector<std::pair<unsigned,unsigned>> used;   // tab, messages
        for(const auto &e:events) {
            const unsigned t=report_tab(e);
            auto it=std::find_if(used.begin(),used.end(),[&](const std::pair<unsigned,unsigned> &u){return u.first==t;});
            if(it==used.end())used.emplace_back(t,1u);else ++it->second;
        }
        const unsigned other=static_cast<unsigned>(Topic::Other);
        if(used.size()>kTabLimit) {
            std::stable_sort(used.begin(),used.end(),[](const std::pair<unsigned,unsigned> &a,const std::pair<unsigned,unsigned> &b){return a.second>b.second;});
            std::vector<std::pair<unsigned,unsigned>> kept;
            for(const auto &u:used)if(u.first!=other && kept.size()<kTabLimit-1)kept.push_back(u);
            kept.emplace_back(other,0u);used.swap(kept);
        }
        std::vector<unsigned> keys;for(const auto &u:used)keys.push_back(u.first);
        std::sort(keys.begin(),keys.end(),[](unsigned a,unsigned b){return tab_rank(a)<tab_rank(b);});
        return keys;
    }
    static unsigned shown_tab(unsigned tab,const std::vector<unsigned> &keys) {
        return std::find(keys.begin(),keys.end(),tab)!=keys.end()?tab:static_cast<unsigned>(Topic::Other);
    }
    std::vector<Card> tabs()const {
        const std::vector<unsigned> keys=tab_keys();std::vector<Card> out(keys.size());
        for(size_t i=0;i<keys.size();++i)out[i].tab=keys[i];
        for(const auto &e:events) {
            const unsigned t=shown_tab(report_tab(e),keys);
            Card &c=out[static_cast<size_t>(std::find(keys.begin(),keys.end(),t)-keys.begin())];
            if(c.events.empty())c.kind=e.kind;else if(c.kind!=e.kind)c.kind=Kind::News;   // card_body's gold total needs one kind
            c.events.push_back(e);if(!e.read)++c.unread;c.gold+=e.gold;
        }
        return out;
    }
    unsigned unread()const {unsigned n=0;for(const auto &e:events)if(!e.read)++n;return n;}
    void mark(Kind kind){for(auto &e:events)if(e.kind==kind)e.read=true;++generation;}
    void erase(Kind kind) {
        events.erase(std::remove_if(events.begin(),events.end(),[&](const Event &e){return e.kind==kind;}),events.end());++generation;
    }
    void mark_all(){for(auto &e:events)e.read=true;++generation;}
    void mark_tab(unsigned tab){const auto keys=tab_keys();for(auto &e:events)if(shown_tab(report_tab(e),keys)==tab)e.read=true;++generation;}
    void erase_tab(unsigned tab) {
        const auto keys=tab_keys();
        events.erase(std::remove_if(events.begin(),events.end(),[&](const Event &e){return shown_tab(report_tab(e),keys)==tab;}),events.end());++generation;
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
// Window look. local222 (user 2026-11-17, the reference screen: parchment tabs with the open one orange, a gold-brown column header,
// alternating dark rows, an orange selection) replaces the 2026-11-07 navy with warm dark brown. All colours are rgb(r,g,b).
namespace look {
inline constexpr unsigned rgb(unsigned r,unsigned g,unsigned b){return (r<<16)|(g<<8)|b;}   // 0xRRGGBB packed, as the DIB pixel and COLORREF conversions expect
// local228 (user 2026-11-18 "원작의 무장일람 ... 테두리랑 배경판만 그걸로", "흐림없이 88%", "안의 컴포넌트도 배경과 어울리게"): the panel is the
// original list window's frame (face_files::kFrameSprite: gold double line, corner knots, warm olive-brown fill rgb(41,35,18) at 75%
// plus a scene blur). Without the blur the whole panel is 88% opaque; the inner parts take the fill's hue.
// local229 (user 2026-11-18 "불투명도 93%로 가자"): 93%.
inline constexpr float kPanelOpacity=0.93f;                       // the panel lets a little of the game scene through
inline constexpr unsigned kPanelTop=rgb(44,34,26),kPanelBottom=rgb(27,21,16);  // the toasts' and balloons' gradient
inline constexpr unsigned kPanelFill=rgb(41,35,18),kFrameLine=rgb(208,190,119);  // the original frame's fill and line (own frame when the file is missing)
inline constexpr int kFrameMargin=4,kFrameSlice=14;                     // the frame cell's clear margin (cut off) and its corner size after the cut
inline constexpr unsigned kInset=kPanelFill;                            // body area (and its frame fill): the panel itself, framed by a line
inline constexpr unsigned kRowTint=rgb(51,44,26),kHeaderFill=rgb(66,54,32);  // alternate table rows, table header row
inline constexpr unsigned kGoldBorder=rgb(166,134,88),kGoldDim=rgb(104,84,58),kGold=rgb(214,180,122);
inline constexpr unsigned kTitle=rgb(242,222,178),kText=rgb(238,230,214),kDim=rgb(188,176,154),kDimmer=rgb(142,130,110);
inline constexpr unsigned kButtonFill=rgb(48,41,22),kButtonHover=rgb(84,68,38),kButtonDown=rgb(30,25,12),kButtonHoverText=rgb(255,242,214);
inline constexpr unsigned kPositive=rgb(140,214,150),kNegative=rgb(238,136,114);
inline constexpr unsigned kNames[6]={rgb(110,200,220),rgb(235,130,130),rgb(140,170,240),rgb(130,210,140),rgb(200,150,230),rgb(235,180,100)};   // officer names, indexed by name_color_index
// Report view (local222; local228 user-approved first proposal: no orange, the open tab parchment and joined to the rows).
inline constexpr unsigned kTabFill=rgb(50,42,24),kTabEdge=kGoldDim,kTabText=rgb(184,162,122);           // closed tabs
inline constexpr unsigned kTabPicked=rgb(228,218,192),kTabPickedEdge=kGoldBorder,kTabPickedText=rgb(60,47,34);   // the open tab (parchment)
inline constexpr unsigned kPicked=kGold,kPickedTint=rgb(84,63,38);                                       // the selected row's border and fill
inline constexpr unsigned kInfoFill=kPanelFill,kHeaderText=rgb(246,224,170),kRule=rgb(64,55,33);
// Table metrics in body texture pixels (the text column starts at 56).
inline constexpr int kTablePad=16,kCellGap=28,kMinColumn=48,kMaxExtra=60,kLinePitch=34,kRowPad=8,kTableGap=18;
// local232 (user 2026-11-18 "좌하단의 그 전체내역 글자가 너무 커서 레이아웃이 조금어색해. (금 군량 군단 수입같은거) 레이아웃에 맞춰서 넘치지 않게"):
// a table in the report's detail panel (the bottom-left box of 전체, e.g. a corps' income) in the small report font (17 px) with these
// tighter metrics, so a corps' header and four rows fit the panel without scrolling (the body page keeps the metrics above).
inline constexpr int kDetailTablePad=8,kDetailCellGap=18,kDetailLinePitch=24,kDetailRowPad=6,kDetailTableGap=10;
}
// User 2026-10-27: double the window dimensions only, retaining font/button sizes.
inline constexpr Rect portrait{30,956,88,88},bubble{142,30,1160,1000};
// drop: how far the info bar moved down (local229 tab_shift: a second tab line in the report view; 0 elsewhere).
inline Rect control(unsigned id,Rect panel=bubble,float drop=0) {
    switch(id) {
    case 2:return {panel.x+panel.w-68,panel.y+31,36,36};
    case 3:return {panel.x+panel.w-324,panel.y+30,44,38};
    case 4:return {panel.x+panel.w-156,panel.y+30,44,38};
    case 5:return {panel.x+panel.w-276,panel.y+panel.h-64,104,38};
    case 6:return {panel.x+panel.w-164,panel.y+panel.h-64,112,38};
    case 7:return {panel.x+panel.w-160,panel.y+187+drop,112,30};   // local222: in the info bar (kInfoTop 178, 48 high)
    case 8:return {panel.x+panel.w-280,panel.y+187+drop,112,30};   // [군사 제안] page, left of [부동산 개발] (local68)
    case 13:return {panel.x+panel.w-400,panel.y+187+drop,112,30};  // [수집 설정] page, left of [군사 제안] (local99; 9 = body, 10..12 = balloon)
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
// local222 report view, panel-relative (panel 1160x1000): the tabs under the title band, the info bar holding buttons 13/8/7, the column
// header, seven 52px rows that scroll, and the detail panel above the footer buttons. Rows have a fixed pitch, so a hit test and a face
// rectangle are arithmetic. Row-local columns: face square, name, chips, summary, and the day right-aligned at the far edge.
inline constexpr int kTabTop=124,kTabHeight=46,kTabGap=6,kTabMaxWidth=200,kInfoTop=178,kInfoHeight=48,kHeadTop=234,kHeadHeight=36,
    kRowTop=270,kRowPitch=52,kRowsHeight=364,kDetailTop=642,kDetailHeight=280,kReportRows=150,kReportTabs=kTabLimit+2;   // + 전체 + 로그 (local232): eighteen, two lines of nine
inline constexpr int kColFace=14,kFaceSize=44,kColName=68,kColChips=214,kColSummary=440,kColDayWidth=96,kColDayPad=16,kDetailFace=112;
// local243 (user 2026-11-19 "지도로 이동으로 버튼 만들어줬으면 ... 클릭만해도 넘어가서 불편"): a row with a place has a [지도 이동] button
// left of the day column; only the button moves the camera (a row click selects). Row-local x, the same for the drawing and the hit
// test (rows have no viewport offset: the rows texture sits at rows_rect). Hit id kMapHit+i.
inline constexpr int kMapWidth=108,kMapHeight=32,kMapHit=20000;
inline int map_button_x(int rows_w){return rows_w-kColDayPad-kColDayWidth-8-kMapWidth;}
// local229: more than nine tabs take two lines, half in each (the first line holds the odd one), the same width in both. The second
// line pushes the info bar (with its buttons 7/8/13), the column header and the rows down by one tab line; the rows lose one row
// (364 -> 312 px, six rows) and the detail panel stays where it is. With nine tabs or fewer nothing moves.
inline constexpr unsigned kTabsPerLine=9;
inline unsigned tab_lines(unsigned count){return count>kTabsPerLine?2u:1u;}
inline float tab_shift(unsigned count){return tab_lines(count)>1?static_cast<float>(kTabHeight+kTabGap):0.0f;}
inline Rect tab_rect(Rect p,unsigned index,unsigned count) {
    if(!count || index>=count)return {};
    const unsigned lines=tab_lines(count),per=(count+lines-1)/lines,line=index/per,column=index%per;
    const float room=p.w-48,width=(std::min)(static_cast<float>(kTabMaxWidth),(room-kTabGap*static_cast<float>(per-1))/per);
    return {p.x+24+static_cast<float>(column)*(width+kTabGap),p.y+kTabTop+static_cast<float>(line)*(kTabHeight+kTabGap),width,static_cast<float>(kTabHeight)};
}
// tabs: the report's tab count (0 outside the report view).
inline Rect info_rect(Rect p,unsigned tabs=0){return {p.x+24,p.y+kInfoTop+tab_shift(tabs),p.w-48,static_cast<float>(kInfoHeight)};}
inline Rect column_rect(Rect p,unsigned tabs=0){return {p.x+24,p.y+kHeadTop+tab_shift(tabs),p.w-48,static_cast<float>(kHeadHeight)};}
// local235 (user 2026-11-18 "로그 누른거에 한해서 우리 군사보고 하단 레이아웃을 없애줄 수 있나?"): tall = the 로그 tab, which has no
// detail panel; its rows reach the detail panel's bottom edge (twelve rows instead of seven).
inline Rect rows_rect(Rect p,unsigned tabs=0,bool tall=false){return {p.x+24,p.y+kRowTop+tab_shift(tabs),p.w-48,static_cast<float>(tall?kDetailTop+kDetailHeight-kRowTop:kRowsHeight)-tab_shift(tabs)};}
inline Rect detail_rect(Rect p){return {p.x+24,p.y+kDetailTop,p.w-48,static_cast<float>(kDetailHeight)};}
// The scrolling window of the open view: the rows in the report view, the text body otherwise.
inline Rect view_rect(Rect p,bool report,unsigned tabs=0,bool tall=false){return report?rows_rect(p,tabs,tall):body_rect(p);}
inline Rect map_button_rect(Rect p,unsigned index,double scroll,unsigned tabs=0,bool tall=false) {
    const Rect r=rows_rect(p,tabs,tall);const float top=static_cast<float>(r.y+static_cast<double>(index)*kRowPitch-scroll);
    return {r.x+static_cast<float>(map_button_x(static_cast<int>(r.w))),top+(kRowPitch-kMapHeight)/2.0f,static_cast<float>(kMapWidth),static_cast<float>(kMapHeight)};
}
// Row `index` (scrolled by `scroll` pixels) and its face square; the row under panel y, or -1.
inline Rect row_rect(Rect p,unsigned index,double scroll,unsigned tabs=0,bool tall=false){const Rect r=rows_rect(p,tabs,tall);return {r.x,static_cast<float>(r.y+static_cast<double>(index)*kRowPitch-scroll),r.w,static_cast<float>(kRowPitch)};}
inline Rect face_rect(Rect p,unsigned index,double scroll,unsigned tabs=0,bool tall=false){const Rect r=row_rect(p,index,scroll,tabs,tall);return {r.x+kColFace,r.y+4,static_cast<float>(kFaceSize),static_cast<float>(kFaceSize)};}
// The selected row's face in the detail panel (only when the row has a person): top-left, the title and text start right of it.
inline Rect detail_face_rect(Rect p){const Rect d=detail_rect(p);return {d.x+18,d.y+18,static_cast<float>(kDetailFace),static_cast<float>(kDetailFace)};}
// The detail text window, which scrolls on its own: right of the face square when the row has a person, left of the records rule.
inline Rect detail_text_rect(Rect p,bool face) {
    const Rect d=detail_rect(p);const float split=d.x+static_cast<float>(static_cast<int>(d.w)*11/20),x=face?d.x+18+kDetailFace+16:d.x+18;
    return {x,d.y+52,split-8-x,d.h-60};
}
inline int row_at(Rect p,float y,double scroll,unsigned count,unsigned tabs=0,bool tall=false) {
    const Rect r=rows_rect(p,tabs,tall);if(y<r.y || y>=r.y+r.h)return -1;
    const double at=(y-r.y)+scroll;if(at<0)return -1;
    const unsigned i=static_cast<unsigned>(at/kRowPitch);return i<count?static_cast<int>(i):-1;
}
// 2026-11-08 user: the real-estate button replaces the report body with the owned-castle list.
// Three columns of 34px rows fit all 51 cities inside the 618px body window.
inline constexpr int kCityColumns=3,kCityPitch=34;
// local240 (user 2026-11-19 "카드 형식으로 ... 텍스트 느낌이 나면 클릭감이 없잖아"): each item is a card; few items get taller cards,
// 51 cities still fit the 618px body without scrolling (17 rows x 36). The hit cell and the drawn card use this same pitch.
inline int city_pitch(unsigned count){const int rows=static_cast<int>((count+kCityColumns-1)/kCityColumns);return rows?(std::max)(kCityPitch,(std::min)(56,612/rows)):kCityPitch;}
inline constexpr int kCardInsetX=8,kCardInsetY=4;
inline Rect city_cell(Rect p,unsigned index,unsigned count){
    Rect body=body_rect(p);
    if(!count || index>=count)return {};
    const float width=body.w/kCityColumns;const int pitch=city_pitch(count);
    return {body.x+static_cast<float>(index%kCityColumns)*width,body.y+static_cast<float>(index/kCityColumns)*pitch,width,static_cast<float>(pitch)};
}
inline Rect city_card(Rect p,unsigned index,unsigned count){Rect c=city_cell(p,index,count);if(c.w<=0)return c;return {c.x+kCardInsetX,c.y+kCardInsetY,c.w-2*kCardInsetX,c.h-2*kCardInsetY};}
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
