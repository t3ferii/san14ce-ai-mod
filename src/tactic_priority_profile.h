#pragma once
// Tactic priority engine v2 (docs/TACTIC_PRIORITY_V2_PLAN.md 7 WP-1). Pure: no game dependency, used by native_tactic_priority.inl,
// native_tactic_priority_grid.inl and tests/tactic_priority_fixture.cpp.
//  Ids are tactic ids; kUnique (-1) is the single entry that stands for "the officer's own unique tactics".
//  A profile stores only the RANKED ids per mode (the order the player clicked); effective(ranked,defaults) = ranked + the rest in default order.
//  default_list(martial,catalog): catalog = known tactic ids (empty -> kind_of ranges). It is also the list of grid tiles.
//  select(eligible,ranked) -> up to 3 ids in ranked order (0 = empty slot, never filled from anything else); select_remembered(remembered,eligible)
//  -> exactly the remembered usable ids (same slots, 0 where dropped). build_tiles() sorts the grid into the 7 fixed groups (siege and naval
//  tactics are hidden); layout_groups(...) places the tiles (each group starts a new row);
//  classify(...) decides what changed for one sortie record;
//  encode/decode = the `[tactic_priority] profile=` text (v2, v1 is migrated).
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
namespace tactic_priority_profile {
static const int kUnique=-1,kMaxTactic=200,kSlots=10,kMaxFormation=20;
static const size_t kMaxProfile=2047;
enum class Kind {None,Unique,Common,Trade};
inline Kind kind_of(int id) {
    if(id<1 || id>kMaxTactic)return Kind::None;
    if(id<=24)return Kind::Common;
    if(id==25 || id==26 || (id>=136 && id<=139))return Kind::Trade;
    return Kind::Unique;
}
struct Settings {std::vector<int> martial,intellect;};   // ranked ids only (clicked order)
inline bool known(int id,const std::vector<int> &catalog) {
    const Kind k=kind_of(id);
    if(k!=Kind::Common && k!=Kind::Trade)return false;
    return catalog.empty() || std::find(catalog.begin(),catalog.end(),id)!=catalog.end();
}
inline std::vector<int> default_list(bool martial,const std::vector<int> &catalog) {
    std::vector<int> out{kUnique};
    auto add=[&](int id) {if(known(id,catalog) && std::find(out.begin(),out.end(),id)==out.end())out.push_back(id);};
    add(137);
    if(martial)add(1);
    add(17);add(6);
    for(int id=1;id<=kMaxTactic;++id)if(kind_of(id)==Kind::Common)add(id);
    for(int id=1;id<=kMaxTactic;++id)if(kind_of(id)==Kind::Trade)add(id);
    return out;
}
inline bool listed(const std::vector<int> &list,int id) {return std::find(list.begin(),list.end(),id)!=list.end();}
// Keeps only ids that are tiles (known or kUnique); first occurrence wins.
inline void sanitize(std::vector<int> &ranked,const std::vector<int> &catalog) {
    std::vector<int> out;
    for(int id:ranked)if((id==kUnique || known(id,catalog)) && !listed(out,id))out.push_back(id);
    ranked.swap(out);
}
// Full order the selector uses: ranked ids first (only ones the defaults know), then the remaining defaults in default order.
inline std::vector<int> effective(const std::vector<int> &ranked,const std::vector<int> &defaults) {
    std::vector<int> out;
    for(int id:ranked)if(listed(defaults,id) && !listed(out,id))out.push_back(id);
    for(int id:defaults)if(!listed(out,id))out.push_back(id);
    return out;
}
// 1-based rank of id, 0 when not ranked.
inline unsigned rank_of(const std::vector<int> &ranked,int id) {
    for(size_t i=0;i<ranked.size();++i)if(ranked[i]==id)return static_cast<unsigned>(i+1);
    return 0;
}
// Click on a tile: unranked -> appended last, ranked -> removed (later ranks move up). false when id is not a tile.
inline bool toggle_rank(std::vector<int> &ranked,int id,const std::vector<int> &tiles) {
    if(!listed(tiles,id))return false;
    auto it=std::find(ranked.begin(),ranked.end(),id);
    if(it!=ranked.end())ranked.erase(it);else ranked.push_back(id);
    return true;
}
inline void clear(std::vector<int> &ranked) {ranked.clear();}
// "Defaults" button: every tile ranked 1..N in default order.
inline std::vector<int> preset(bool martial,const std::vector<int> &catalog) {return default_list(martial,catalog);}
// v1 profiles stored the full order; the ranked part is the shortest prefix that still reproduces it.
inline std::vector<int> ranked_from_full(const std::vector<int> &full,const std::vector<int> &defaults) {
    const std::vector<int> want=effective(full,defaults);
    for(size_t k=0;k<=full.size();++k) {
        const std::vector<int> prefix(full.begin(),full.begin()+static_cast<std::ptrdiff_t>(k));
        if(effective(prefix,defaults)==want)return prefix;
    }
    return full;
}
inline bool martial(int war,int intel) {return war>=intel;}
typedef std::array<unsigned,3> Three;
inline bool contains(const std::vector<unsigned> &v,unsigned id) {return std::find(v.begin(),v.end(),id)!=v.end();}
inline bool has(const Three &t,unsigned id) {return std::find(t.begin(),t.end(),id)!=t.end();}
// ranked = the player's ranked ids for the officer's type (clicked order); eligible = the officer's usable tactic ids.
// Result: the usable ranked tactics in ranked order, at most 3; kUnique stands for the officer's own unique tactics (slot order).
// Unranked tactics are never added: the remaining slots stay 0 (the original "no tactic" value). Empty ranked or nothing usable -> all 0.
inline Three select(const std::vector<unsigned> &eligible,const std::vector<int> &ranked) {
    Three out{};unsigned n=0;
    auto add=[&](unsigned id) {if(n<3 && id && contains(eligible,id) && !has(out,id))out[n++]=id;};
    for(int p:ranked) {
        if(p==kUnique) {for(unsigned id:eligible)if(kind_of(static_cast<int>(id))==Kind::Unique)add(id);}
        else if(p>0)add(static_cast<unsigned>(p));
    }
    return out;
}
inline bool any(const Three &t) {return t[0] || t[1] || t[2];}
// Remembered sortie: exactly the remembered tactics, each in its own slot, only dropping ids that are not usable now (slot becomes 0).
inline Three select_remembered(const Three &remembered,const std::vector<unsigned> &eligible) {
    Three out{};
    for(unsigned i=0;i<3;++i)if(remembered[i] && contains(eligible,remembered[i]) && !has(out,remembered[i]))out[i]=remembered[i];
    return out;
}
// ---- grid groups (user 2026-11-07): the game is Korean, groups are matched on the tactic name read from the tactic table ----
// 0 unique, 1..5 listed groups, 6 other. Siege and naval tactics are hidden: not tiles, not ranked (the original picks them in siege
// formations and on ships).
static const unsigned kGroups=7;
static const wchar_t *const kGroupMembers[5]={
    L"아수라진 안식사법 구갑대열",L"분전 돌격 급습 연노 제사",L"매성 업화 화시",L"철벽 경파 치료 진정 격려 도발 고무",L"대갈 교란 견제 저지"};
static const wchar_t *const kHiddenMembers=L"정란 충차 투석 전상돌격 구축 강습 시람";
// index of `name` in the space separated list, or -1
inline int word_index(const wchar_t *list,const std::wstring &name) {
    if(name.empty())return -1;
    const std::wstring all(list);size_t i=0;int k=0;
    while(i<=all.size()) {
        size_t j=all.find(L' ',i);if(j==std::wstring::npos)j=all.size();
        if(j-i==name.size() && all.compare(i,j-i,name)==0)return k;
        i=j+1;++k;
    }
    return -1;
}
inline bool hidden_name(const std::wstring &name) {return word_index(kHiddenMembers,name)>=0;}
// group 1..5 for a listed name, 6 for anything else. order = index inside the group list.
inline unsigned tile_group(const std::wstring &name,unsigned *order=nullptr) {
    for(unsigned g=0;g<5;++g) {
        const int k=word_index(kGroupMembers[g],name);
        if(k>=0) {if(order)*order=static_cast<unsigned>(k);return g+1;}
    }
    if(order)*order=0;
    return 6;
}
struct Tile {int id;unsigned group;};
// ids = default_list order; name_of(id) -> tactic name. Result: kUnique first (group 0), then group 1..6, inside a group the listed order
// (group 6 keeps the incoming order); hidden tactics are dropped.
template<class Name> inline std::vector<Tile> build_tiles(const std::vector<int> &ids,Name name_of) {
    struct Item {int id;unsigned group,order,seq;};
    std::vector<Item> items;unsigned seq=0;
    for(int id:ids) {
        if(id==kUnique) {items.push_back({id,0,0,seq++});continue;}
        const std::wstring name=name_of(id);
        if(hidden_name(name))continue;
        unsigned order=0;const unsigned g=tile_group(name,&order);
        items.push_back({id,g,order,seq++});
    }
    std::stable_sort(items.begin(),items.end(),[](const Item &a,const Item &b) {
        return a.group!=b.group?a.group<b.group:(a.group==6?a.seq<b.seq:a.order<b.order);
    });
    std::vector<Tile> out;
    for(const Item &i:items)out.push_back({i.id,i.group});
    return out;
}
inline std::vector<int> tile_ids(const std::vector<Tile> &tiles) {
    std::vector<int> out;
    for(const Tile &t:tiles)out.push_back(t.id);
    return out;
}
// Grid placement: tiles are laid out by group (`groups[i]` non-decreasing), five columns, every group starts a new row and wraps inside the group;
// every row (inside a group or at a group start) uses the same vertical gap (user 2026-11-07: even spacing). Cells are relative to the grid area (x includes the side margin, y starts at 0); the popup frame is w wide.
struct Cell {int x,y;};
struct Grid {unsigned cols=0,rows=0;int tile_w=0,tile_h=0,gap_x=0,gap_y=0,w=0,h=0;std::vector<Cell> cells;};
static const int kMargin=39,kTileH=48,kGapY=14,kTileW=164,kCols=5,kGapMin=12,kGapMax=24;
inline Grid layout_groups(const std::vector<unsigned> &groups,int avail_w) {
    Grid g;g.tile_h=kTileH;g.gap_y=kGapY;g.cols=kCols;g.tile_w=kTileW;
    const int inner=(std::max)(1,avail_w-2*kMargin);
    int gap=(inner-kCols*kTileW)/(kCols-1);
    gap=(std::max)(kGapMin,(std::min)(kGapMax,gap));
    g.gap_x=gap;
    const int used=kCols*kTileW+(kCols-1)*gap;
    g.w=(std::max)(avail_w,used+2*kMargin);
    const int x0=(g.w-used)/2;
    unsigned row=0,col=0;int y=0;
    for(size_t i=0;i<groups.size();++i) {
        if(i && groups[i]!=groups[i-1]) {++row;col=0;y+=kTileH+kGapY;}
        else if(col==static_cast<unsigned>(kCols)) {++row;col=0;y+=kTileH+kGapY;}
        g.cells.push_back({x0+static_cast<int>(col)*(kTileW+gap),y});
        ++col;
    }
    g.rows=groups.empty()?0u:row+1;
    g.h=groups.empty()?0:y+kTileH;
    return g;
}
// ---- hint text fitting (user 2026-11-07: the rule text must fit the hint node, never be clipped) ----
// Estimated advance in logical 1920x1080 px: hangul/CJK 24, ASCII 13 (the original text font at 24 px; deliberately a little wide).
inline int text_width(const std::wstring &text) {
    int w=0;
    for(wchar_t c:text)w+=c<0x80?13:24;
    return w;
}
// First candidate (longest first) whose estimated width is within `width` (3% safety margin); the last candidate when none fits.
inline std::wstring fit_hint(const std::vector<std::wstring> &candidates,int width) {
    for(const std::wstring &c:candidates)if(text_width(c)*103/100<=width)return c;
    return candidates.empty()?std::wstring():candidates.back();
}
// Vertical extent of the popup frame: the grid, two full-width text lines under it and the button row; never below the original window height.
static const int kGridTopY=86,kFrameMinH=783,kLineH=26,kLineGap=2,kButtonRowH=48,kButtonBottomGap=20,kBlockGap=10;
inline int frame_height(int grid_h) {
    const int need=kGridTopY+grid_h+kBlockGap+2*kLineH+kLineGap+kBlockGap+kButtonRowH+kButtonBottomGap;
    return (std::max)(kFrameMinH,need);
}
// Record tracking (plan 4). Memo is what was seen the last frame for one sortie record (keyed by record pointer).
enum class Event {None,Person,Formation,Settings};
struct Memo {unsigned person=0,formation=0,generation=0;unsigned tactics[3]={0,0,0};};
inline Event classify(const Memo *memo,unsigned person,unsigned formation,unsigned generation) {
    if(!memo || memo->person!=person)return Event::Person;
    if(memo->formation!=formation)return Event::Formation;
    if(memo->generation!=generation)return Event::Settings;
    return Event::None;
}
inline std::string encode_list(const std::vector<int> &list) {
    std::string s;
    for(size_t i=0;i<list.size();++i) {if(i)s+=',';s+=list[i]==kUnique?std::string("U"):std::to_string(list[i]);}
    return s;
}
inline std::string encode(const Settings &s) {return "v=2;martial="+encode_list(s.martial)+";intellect="+encode_list(s.intellect);}
inline bool decode_list(const std::string &text,std::vector<int> &out,bool allow_empty) {
    out.clear();
    if(text.empty())return allow_empty;
    size_t i=0;
    while(i<=text.size()) {
        size_t j=text.find(',',i);if(j==std::string::npos)j=text.size();
        const std::string item=text.substr(i,j-i);
        if(item=="U")out.push_back(kUnique);
        else {
            if(item.empty() || item.size()>3 || item.find_first_not_of("0123456789")!=std::string::npos)return false;
            const int id=std::atoi(item.c_str());if(id<1 || id>kMaxTactic)return false;
            out.push_back(id);
        }
        i=j+1;if(j==text.size())break;
    }
    return !out.empty();
}
// false (out untouched) for anything malformed; callers then use an empty profile (= default order).
// v2: v,martial,intellect (lists may be empty). v1: v,restore,martial,intellect with full lists -> converted by ranked_from_full;
// *restore_v1 reports the dropped restore flag. *version = 1 or 2 on success. The version must be the first field.
inline bool decode(const std::string &text,const std::vector<int> &catalog,Settings &out,int *version=nullptr,bool *restore_v1=nullptr) {
    if(text.empty() || text.size()>kMaxProfile)return false;
    for(char c:text)if(static_cast<unsigned char>(c)<0x20 || static_cast<unsigned char>(c)>0x7E)return false;
    Settings s;int ver=0;bool restore=false,martial=false,intellect=false,restore_value=false;size_t i=0;
    while(i<text.size()) {
        size_t j=text.find(';',i);if(j==std::string::npos)j=text.size();
        const std::string field=text.substr(i,j-i);const size_t eq=field.find('=');i=j+1;
        if(eq==std::string::npos)return false;
        const std::string key=field.substr(0,eq),value=field.substr(eq+1);
        if(key=="v") {if(ver || (value!="1" && value!="2"))return false;ver=value=="1"?1:2;}
        else if(!ver)return false;
        else if(key=="restore") {if(ver!=1 || (value!="0" && value!="1") || restore)return false;restore=true;restore_value=value=="1";}
        else if(key=="martial") {if(martial || !decode_list(value,s.martial,ver==2))return false;martial=true;}
        else if(key=="intellect") {if(intellect || !decode_list(value,s.intellect,ver==2))return false;intellect=true;}
        else return false;
    }
    if(!ver || !martial || !intellect || (ver==1 && !restore))return false;
    sanitize(s.martial,catalog);sanitize(s.intellect,catalog);
    if(ver==1) {
        s.martial=ranked_from_full(s.martial,default_list(true,catalog));
        s.intellect=ranked_from_full(s.intellect,default_list(false,catalog));
    }
    if(version)*version=ver;
    if(restore_v1)*restore_v1=restore_value;
    out=std::move(s);return true;
}
}
namespace tp=tactic_priority_profile;
