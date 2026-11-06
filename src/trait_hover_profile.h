#pragma once
// Trait hover (docs/TRAIT_HOVER_PLAN.md 3.4/3.5, 2026-11-06): address-table ids and function-hash guards.
// Hashes are the original-build (1fbe6be7) FNV-1a values; GH() maps them to the current build via the generated pair table.
#include "game_addresses.h"

struct TraitHoverGuard {unsigned rva,size;unsigned long long hash;};
static const TraitHoverGuard trait_hover_guards[]={
    {GA(GA_10FC0),0x40,GH(0xB5056BEAE1B8E535ull)},
    {GA(GA_11290),0x40,GH(0x591548C89BC4D397ull)},
    {GA(GA_86B3B0),0x14,GH(0x96F4EA5773F4FECDull)},
    {GA(GA_AE0370),0x10,GH(0xBA97BD8A3F377964ull)},
    {GA(GA_32F500),0x24,GH(0xB302042A0360860Cull)},
    {GA(GA_401C0),0x2C,GH(0x2678C1755B9BA606ull)},
    {GA(GA_2305D0),0x7A,GH(0xA84CB8298B0B46A6ull)},
    {GA(GA_3594B0),0x40,GH(0x1F087CDCF91473E6ull)},
    {GA(GA_8A2140),0x40,GH(0x1242FD2F9EE4486Full)},
    {GA(GA_8A5D60),0x40,GH(0x9C48369062BFAF08ull)},
    {GA(GA_89C320),0x40,GH(0x40B480808CDFA5E0ull)},
    {GA(GA_8A18C0),0x15,GH(0x40EE501236F2DBD8ull)},
    {GA(GA_3F6BC0),0x40,GH(0x7A7E60DC15FE1D4Eull)},
    {GA(GA_864C50),0x10,GH(0x7675919B3FCE29EDull)},
    {GA(GA_92AB70),0x10,GH(0x98C50CE4B192947Aull)},
    {GA(GA_30CF80),0x32,GH(0x81CE476597762421ull)},
    {GA(GA_92CEE0),0x2A,GH(0x3F5EB08BB653F311ull)},
    {GA(GA_363550),0x1B,GH(0xAB4EEE6A633754DFull)},
    {GA(GA_365710),0x35,GH(0x54A695AF793F0AC5ull)},
    {GA(GA_3657B0),0x40,GH(0x5D560EBBD6D8727Aull)},
    {GA(GA_358BB0),0x20,GH(0x1B86B336E008C55Aull)},
    {GA(GA_358D30),0x40,GH(0xDF3F1E8811BEA338ull)},
    {GA(GA_358C20),0x20,GH(0xCF9A463A28031891ull)},
    {GA(GA_358E20),0x40,GH(0xED797723570FC22Cull)},
    // tactic feature-tab column formatters (docs/TRAIT_HOVER_TACTIC_TEXT.md 3.2)
    {GA(GA_365FD0),0x09,GH(0x836D5B8137481FFAull)},
    {GA(GA_356F10),0x40,GH(0x152C588A0AD17874ull)},
    {GA(GA_365FE0),0x40,GH(0x236B7AE8CBB7C17Aull)},
    {GA(GA_3586C0),0x40,GH(0x5DACB979EEA32D88ull)},
    {GA(GA_358C90),0x40,GH(0xB714E8FC5887DC66ull)},
};

// Exact-byte guard sites and context ids (plan 3.5 items 3/4).
static const unsigned trait_hover_empty_tick_rva=GA(GA_92AD0);          // C2 00 00
static const unsigned trait_hover_slot_getter_rva=GA(GA_23064A);        // 83 FB 05|09
static const unsigned trait_hover_duel_call_rvas[]={GA(GA_806D51),GA(GA_8070B1),GA(GA_807D92),GA(GA_80807C)};
static const unsigned trait_hover_duel_target_rva=GA(GA_930D20);
static const unsigned trait_hover_balloon_ctx_rva=GA(GA_1A176A);        // B9 80 01 00 00 (optional guard)
static const unsigned trait_hover_balloon_vtable_rva=GA(GA_139C540);
static const unsigned trait_hover_tactic_columns_rva=GA(GA_1A3A6A0);   // CTacticsData list column descriptor table (0x60 bytes per column)

// ---------------------------------------------------------------------------------------------------------------------------
// S2 pure logic (plan 1.3-1.9, 3.3). No game memory is read here: every function takes its inputs as arguments so the fixture
// can drive it (tests/trait_hover_fixture.cpp). Offsets named in comments are the original object layout; the runtime module
// reads them and passes the values in.
// ---------------------------------------------------------------------------------------------------------------------------
namespace thp {
static const int kTraitMax=0x163;           // trait ids 1..0x163, table world+0x7DD18+id*8
static const int kDuelTraitMax=0x162;       // duel label capture only scans 1..0x162 (blob FUN_2AA0)
static const int kTacticCount=201;          // tactic index 0..200, table world+0x774D0+i*8
static const uint64_t kTacticFlag=0x200;
static const uint64_t kEditRowMin=0x10000;  // key >= this is a pointer to an edit row
static const unsigned kHeaderTemplateSize=44;
static const unsigned kRecordCap=128;
static const unsigned kTextMax=1023;        // wchar_t, excluding the terminator
static const unsigned kChainMax=30;         // widget chain length limit of contains()

// ---- key space (1.3) ----
enum KeyClass {KC_NONE=0,KC_TRAIT=1,KC_TACTIC=2,KC_EDIT=3,KC_INVALID=4};
inline uint64_t tactic_key(int i) {return (i>=0 && i<kTacticCount)?(kTacticFlag|(uint64_t)i):0;}
inline KeyClass key_class(uint64_t key) {
    if(key==0)return KC_NONE;
    if(key>=kEditRowMin)return KC_EDIT;
    if(key>=1 && key<=(uint64_t)kTraitMax)return KC_TRAIT;
    if(key>=kTacticFlag && key<=(kTacticFlag|(uint64_t)(kTacticCount-1)))return KC_TACTIC;
    return KC_INVALID;
}
inline int tactic_index(uint64_t key) {return key_class(key)==KC_TACTIC?(int)(key&0xFF):-1;}
// A key that came out of a resolver must be one of the three real classes, else it is dropped to 0.
inline uint64_t sanitize_key(uint64_t key) {const KeyClass c=key_class(key);return (c==KC_TRAIT||c==KC_TACTIC||c==KC_EDIT)?key:0;}
inline bool trait_id_ok(int id) {return id>=1 && id<=kTraitMax;}

// ---- hover state machine (1.1 / 1.9): text is reset only when the key changes, show runs every frame while key!=0 ----
struct HoverAction {bool hide,set_text,show;};
inline HoverAction hover_action(uint64_t last_key,uint64_t key) {
    HoverAction a{false,false,false};
    if(key==0){a.hide=(last_key!=0);return a;}
    a.set_text=(key!=last_key);a.show=true;return a;
}

// ---- slot table (1.2, 3.3) ----
struct SlotEntry {unsigned slot_rva,row;};
enum SlotKind {SK_TICK=0,SK_HIDE=1,SK_CLOSE=2};
static const unsigned kSlotOffsetTick=0x08,kSlotOffsetClose=0x28,kSlotOffsetHide=0x40;
inline unsigned slot_offset(SlotKind k) {return k==SK_TICK?kSlotOffsetTick:(k==SK_CLOSE?kSlotOffsetClose:kSlotOffsetHide);}
// blob kinds 0..12 -> slot offset (0x8080 table); unknown kind -> -1
inline int blob_kind_offset(unsigned kind) {
    switch(kind) {
        case 0:case 3:case 4:case 5:case 8: return (int)kSlotOffsetTick;
        case 2:case 6:case 10:case 11: return (int)kSlotOffsetClose;
        case 1:case 7:case 9:case 12: return (int)kSlotOffsetHide;
        default: return -1;
    }
}
inline void slot_table_sort(SlotEntry *t,size_t n) {std::sort(t,t+n,[](const SlotEntry &a,const SlotEntry &b){return a.slot_rva<b.slot_rva;});}
// binary search in the sorted table; returns the registered row or -1
inline int slot_find(const SlotEntry *t,size_t n,unsigned slot_rva) {
    size_t lo=0,hi=n;
    while(lo<hi) {
        const size_t mid=lo+(hi-lo)/2;
        if(t[mid].slot_rva==slot_rva)return (int)t[mid].row;
        if(t[mid].slot_rva<slot_rva)lo=mid+1;else hi=mid;
    }
    return -1;
}
inline int slot_find_for_vt(const SlotEntry *t,size_t n,unsigned vt_rva,SlotKind k) {return slot_find(t,n,vt_rva+slot_offset(k));}
inline bool slot_table_valid(const SlotEntry *t,size_t n) {   // sorted strictly ascending, rows distinct and < n
    std::vector<unsigned char> seen(n,0);
    for(size_t i=0;i<n;++i) {
        if(t[i].slot_rva==0 || t[i].row>=n || seen[t[i].row])return false;
        seen[t[i].row]=1;
        if(i && t[i-1].slot_rva>=t[i].slot_rva)return false;
    }
    return true;
}

// ---- path classification by vtable (1.1, 3.3) ----
enum Path {PATH_NONE=0,PATH_TABLE=1,PATH_BODY=2,PATH_SUMMARY=3,PATH_DUEL=4,PATH_EDIT=5};
struct PathVts {unsigned summary,duel_info,duel_gauge,edit;const unsigned *tables;size_t table_count;};
inline bool in_set(const unsigned *s,size_t n,unsigned v) {for(size_t i=0;i<n;++i)if(s[i]==v)return true;return false;}
// tick path for the object whose vtable rva is vt: summary / duel / edit / otherwise a table
inline Path tick_path(const PathVts &p,unsigned vt) {
    if(vt==0)return PATH_NONE;
    if(vt==p.summary)return PATH_SUMMARY;
    if(vt==p.duel_info || vt==p.duel_gauge)return PATH_DUEL;
    if(vt==p.edit)return PATH_EDIT;
    return PATH_TABLE;
}
// body tick: run the table path only when the body's owner has one of the table vtables
inline bool body_owner_ok(const PathVts &p,unsigned owner_vt) {return owner_vt!=0 && in_set(p.tables,p.table_count,owner_vt);}

// ---- contains (1.4, blob FUN_1540) ----
struct WidgetView {int width,height;unsigned flags;int ox,oy;const void *parent;};
// fetch(const void *w) -> WidgetView. The chain is visited node by node (at most kChainMax nodes, every node (flags&3)==3,
// offsets accumulate, must end in null). Half-open interval. xonly checks x only. width/height of w itself must be > 0.
template<class Fetch> inline bool contains(const void *w,int px,int py,bool xonly,Fetch fetch) {
    if(!w)return false;
    const WidgetView self=fetch(w);
    if(self.width<=0 || self.height<=0)return false;
    int ax=0,ay=0;const void *cur=w;unsigned n=0;
    while(cur) {
        if(++n>kChainMax)return false;
        const WidgetView v=fetch(cur);
        if((~v.flags&3u)!=0)return false;
        ax+=v.ox;ay+=v.oy;cur=v.parent;
    }
    if(!(ax<=px && px-ax<self.width))return false;
    if(xonly)return true;
    return ay<=py && py-ay<self.height;
}
// ancestor test: target==w, or w is within 30 parents of target (blob FUN_23C0 / FUN_2790)
template<class Parent> inline bool is_or_descends(const void *target,const void *w,Parent parent) {
    if(!w || !target)return false;
    if(target==w)return true;
    const void *a=target;
    for(unsigned k=0;k<30;++k) {a=parent(a);if(!a || a==w)break;}
    return a==w;
}

// ---- table path pieces (1.4, blob FUN_1000) ----
inline bool tab_ok(int tab,int tab_count) {return tab>=0 && tab<64 && tab<tab_count;}
inline bool header_array_ok(unsigned long long bytes_used) {return bytes_used>=0x10 && bytes_used-0x10<=0x1FFF;}   // 2..1025 entries
inline unsigned header_count(unsigned long long bytes_used) {return (unsigned)(bytes_used>>3);}
inline bool header_col_ok(unsigned col,unsigned j) {return col<=0x3FF && j>=1 && col==j-1;}   // j = header index (>=1)
// ids[0..col] must all be >=0 and ids[col] < 0x1000; returns the descriptor index or -1
inline int descriptor_index(const int *ids,unsigned col) {
    for(unsigned i=0;i<=col;++i)if(ids[i]<0)return -1;
    return (unsigned)ids[col]<0x1000u?ids[col]:-1;
}
inline bool visible_index_ok(int vis,int res_count) {return vis>=0 && vis<res_count;}
inline bool row_index_ok(int idx,int row_count) {return idx>=0 && idx<row_count;}
// cell-mode structural checks: body vt known, body.owner==owner, target.body==body, both contain the point
inline bool cell_mode_ok(bool body_vt_known,bool body_owner_is_self,bool target_body_is_body,bool body_hit,bool target_hit) {
    return body_vt_known && body_owner_is_self && target_body_is_body && body_hit && target_hit;
}
// header scan hit: header mode needs target==header and containment in x and y; cell mode needs x-only containment
inline bool header_hit(bool cell_mode,bool target_is_header,bool xonly_hit,bool full_hit) {return cell_mode?xonly_hit:(target_is_header && full_hit);}

// name -> key (blob FUN_15F0): edit row first, then trait (pointer-identical name AND valid), then tactic (needs game text)
inline uint64_t name_key_decide(bool edit_row,uint64_t row,int trait_id,bool trait_valid,int tactic_i,bool tactic_has_text) {
    if(edit_row)return row;
    if(trait_id_ok(trait_id) && trait_valid)return (uint64_t)trait_id;
    if(tactic_i>=0 && tactic_i<kTacticCount && tactic_has_text)return tactic_key(tactic_i);
    return 0;
}
// first id in [first,last] whose name pointer equals name (nameof(id) -> uintptr_t of id's name, 0 when none); 0 if absent
template<class NameOf> inline int find_name_id(uintptr_t name,int first,int last,NameOf nameof) {
    if(!name)return 0;
    for(int id=first;id<=last;++id) {const uintptr_t p=nameof(id);if(p && p==name)return id;}
    return 0;
}

// ---- summary path (1.4, blob FUN_23C0) ----
inline bool summary_slot_ok(unsigned slot) {return slot<=8;}
inline uint64_t summary_key(int trait_id) {return trait_id_ok(trait_id)?(uint64_t)trait_id:0;}
inline bool summary_button_hit(bool button_nonnull,bool button_hit,bool target_in_button) {return button_nonnull && button_hit && target_in_button;}

// ---- edit path (1.4, blob FUN_2790) ----
struct EditProbe {bool row_nonnull,w_nonnull,input_ok,target_ok,row_vt_edit,owner_hit,w_hit,target_in_w;unsigned short row_text_first;};
inline uint64_t edit_key(const EditProbe &p,uint64_t row) {
    if(!(p.row_nonnull && p.w_nonnull && p.input_ok && p.target_ok && p.row_vt_edit))return 0;
    if(!(p.owner_hit && p.w_hit && p.target_in_w))return 0;
    return p.row_text_first!=0?row:0;
}

// ---- duel capture (1.4, blob FUN_2AA0 / FUN_1EE0) ----
// Info label ids: even 10..18 -> 0..4. Gauge: odd 13..21 -> 0..4, odd 39..47 -> 9..13. Anything else -1.
inline int duel_label_index(bool vt_info,bool vt_gauge,int id) {
    if(vt_info) {
        if(id>=10 && id<=18 && (id&1)==0)return (id-10)/2;
        return -1;
    }
    if(vt_gauge) {
        if(id>=13 && id<=21 && (id&1)==1)return (id-13)/2;
        if(id>=39 && id<=47 && (id&1)==1)return (id-39)/2+9;
        return -1;
    }
    return -1;
}
static const int kDuelLabels=18;
// captured trait id: pointer-identical name found in 1..0x162 and valid, else 0
inline int duel_capture_id(int found_id,bool valid) {return (found_id>=1 && found_id<=kDuelTraitMax && valid)?found_id:0;}
// tick: first label equal to target that contains the point -> its captured id (1..0x163 else 0)
template<class Hit> inline int duel_tick_key(const uintptr_t *labels,const int *ids,size_t n,uintptr_t target,Hit hit) {
    for(size_t i=0;i<n;++i)if(target && labels[i]==target && hit(i))return trait_id_ok(ids[i])?ids[i]:0;
    return 0;
}

// ---- header handler template decode (1.5) ----
// 48 83 EC 28 | BA imm32 | E8 rel32->232D00 | 85 C0 74 0E | B9 38 03 00 00 | 48 83 C4 28 | E9 rel32->30CF80 | 48 8B 05 rel32->1A47B98 | 48 83 C4 28 C3
enum HeaderReject {HR_OK=0,HR_RANGE=1,HR_BYTES=2,HR_TARGET=3,HR_ID=4};
struct HeaderTargets {unsigned has_trait_rva,circle_text_rva,empty_string_rva;};   // GA(232D00), GA(30CF80), GA(1A47B98)
static const unsigned char kHeaderTemplate[kHeaderTemplateSize]={
    0x48,0x83,0xEC,0x28, 0xBA,0,0,0,0, 0xE8,0,0,0,0, 0x85,0xC0,0x74,0x0E, 0xB9,0x38,0x03,0x00,0x00, 0x48,0x83,0xC4,0x28,
    0xE9,0,0,0,0, 0x48,0x8B,0x05,0,0,0,0, 0x48,0x83,0xC4,0x28,0xC3};
// byte offsets of the variable parts: imm32 id (5), E8 rel (10), E9 rel (28), mov rel (35)
static const unsigned kHdrIdOff=5,kHdrCallOff=10,kHdrJmpOff=28,kHdrMovOff=35;
inline int read_i32(const unsigned char *p) {int v;memcpy(&v,p,4);return v;}
inline bool header_fixed_bytes_ok(const unsigned char *c) {
    static const unsigned vary[]={5,6,7,8,10,11,12,13,28,29,30,31,35,36,37,38};   // 16 variable bytes
    for(unsigned i=0;i<kHeaderTemplateSize;++i) {
        bool v=false;for(unsigned j=0;j<sizeof(vary)/sizeof(vary[0]);++j)if(vary[j]==i){v=true;break;}
        if(!v && c[i]!=kHeaderTemplate[i])return false;
    }
    return true;
}
// code: the bytes read at fn_rva (avail bytes readable). The function must lie in [text_lo,text_hi) and all 44 bytes be readable.
// On success *id receives the trait id; returns the reject reason otherwise (id untouched).
inline HeaderReject decode_header_handler(const unsigned char *code,size_t avail,unsigned fn_rva,unsigned text_lo,unsigned text_hi,const HeaderTargets &t,int *id) {
    if(!code || fn_rva<text_lo || (unsigned long long)fn_rva+kHeaderTemplateSize>text_hi || avail<kHeaderTemplateSize)return HR_RANGE;
    if(!header_fixed_bytes_ok(code))return HR_BYTES;
    const unsigned call=(unsigned)((long long)fn_rva+kHdrCallOff+4+read_i32(code+kHdrCallOff));
    const unsigned jmp=(unsigned)((long long)fn_rva+kHdrJmpOff+4+read_i32(code+kHdrJmpOff));
    const unsigned mov=(unsigned)((long long)fn_rva+kHdrMovOff+4+read_i32(code+kHdrMovOff));
    if(!t.has_trait_rva || !t.circle_text_rva || !t.empty_string_rva)return HR_TARGET;
    if(call!=t.has_trait_rva || jmp!=t.circle_text_rva || mov!=t.empty_string_rva)return HR_TARGET;
    const int v=read_i32(code+kHdrIdOff);
    if(!trait_id_ok(v))return HR_ID;
    if(id)*id=v;
    return HR_OK;
}

// ---- editor 7-function table (1.6) ----
struct EditFunc {unsigned ga_id;unsigned title_id;};
static const EditFunc kEditFuncs[7]={
    {GA_363550,0x7FB},{GA_365710,0x7FC},{GA_3657B0,0x53F},{GA_358BB0,0x404},{GA_358D30,0x800},{GA_358C20,0x405},{GA_358E20,0x801}};
inline unsigned edit_func_rva(unsigned i) {return i<7?ga::rva(kEditFuncs[i].ga_id):0;}
inline unsigned edit_title_id(unsigned i) {return i<7?kEditFuncs[i].title_id:0;}
// cut to at most limit wchar without leaving a lone high surrogate at the end
inline size_t cut_wide(const wchar_t *s,size_t len,size_t limit) {
    if(len>limit)len=limit;
    if(len>0 && (s[len-1]&0xFC00)==0xD800)--len;
    return len;
}
// Every line is "cap: value" (a null caption/value leaves its part empty, the line stays, like the blob), joined by newline, max 1023 wchar.
inline std::wstring compose_edit_text(const wchar_t *const *caption,const wchar_t *const *value,size_t n) {
    // Same as the blob (FUN_1800034B0, research/ghidra_refs/SAN14_TraitHover_8cfad1a6ada78eca): EVERY line is written, a newline before each
    // line but the first, then caption, ": ", value; a null caption or value only leaves its part empty. Capped at 1023.
    std::wstring out;
    for(size_t i=0;i<n;++i) {
        if(i)out+=L'\n';
        if(caption[i])out+=caption[i];
        out+=L": ";
        if(value[i])out+=value[i];
        if(out.size()>kTextMax)break;
    }
    out.resize(cut_wide(out.c_str(),out.size(),kTextMax));
    return out;
}

// ---- tactic feature tab (docs/TRAIT_HOVER_TACTIC_TEXT.md 2.2/2.3) ----
struct TacticField {unsigned ga_id,caption_id,column;};
static const unsigned kTacticFields=5,kTacticColStride=0x60,kTacticColFormatter=0x18,kTacticColCaption=0x48;
static const TacticField kTacticFieldTable[kTacticFields]={   // CTacticsData list feature tab (old 1A3A6A0 / new 1A3D680)
    {GA_365FD0,0x455,1},{GA_356F10,0x456,2},{GA_365FE0,0x4B4,3},{GA_3586C0,0x404,12},{GA_358C90,0x405,13}};
inline unsigned tactic_func_rva(unsigned k) {return k<kTacticFields?ga::rva(kTacticFieldTable[k].ga_id):0;}
inline unsigned tactic_caption_id(unsigned k) {return k<kTacticFields?kTacticFieldTable[k].caption_id:0;}
// column descriptor check: +0x00 column number, +0x18 formatter VA, +0x48 caption message id (+0x5C is mutable at run time, not compared)
inline bool tactic_column_ok(const unsigned char *desc,unsigned column,uint64_t fn_va,unsigned caption) {
    if(!desc)return false;
    int c;uint64_t f;int m;
    memcpy(&c,desc,4);memcpy(&f,desc+kTacticColFormatter,8);memcpy(&m,desc+kTacticColCaption,4);
    return c>=0 && (unsigned)c==column && f==fn_va && m>=0 && (unsigned)m==caption;
}
// "cap: v1 / cap: v2 / cap: v3", newline, "cap: v4", newline, "cap: v5"; empty parts are dropped; v1 or v4 empty -> empty text.
inline std::wstring compose_tactic_text(const wchar_t *const *caption,const wchar_t *const *value) {
    auto has=[&](unsigned k){return value[k] && value[k][0];};
    if(!has(0) || !has(3))return std::wstring();
    auto piece=[&](unsigned k){std::wstring p;if(caption[k])p+=caption[k];p+=L": ";p+=value[k];return p;};
    std::wstring out;
    for(unsigned k=0;k<3;++k)if(has(k)) {if(!out.empty())out+=L" / ";out+=piece(k);}
    for(unsigned k=3;k<5;++k)if(has(k)) {out+=L'\n';out+=piece(k);}
    out.resize(cut_wide(out.c_str(),out.size(),kTextMax));
    return out;
}

// ---- balloon records (1.9): {owner,tip,labels[18],ids[18],registered,failed,last_key}, 0xF8 bytes, 128 of them ----
struct Record {uintptr_t owner;uintptr_t tip;uintptr_t labels[18];int ids[18];unsigned char registered,failed,pad[2];uint64_t last_key;};
static_assert(sizeof(Record)==0xF8,"record layout");
struct RecordTable {
    Record rec[kRecordCap];bool full_logged;
    RecordTable() {reset();}
    void reset() {memset(rec,0,sizeof(rec));full_logged=false;}
    Record *find(uintptr_t owner) {if(!owner)return nullptr;for(unsigned i=0;i<kRecordCap;++i)if(rec[i].owner==owner)return &rec[i];return nullptr;}
    // existing record, else the first free one; nullptr when the table is full (caller logs trait_hover_records_full once)
    Record *acquire(uintptr_t owner) {
        if(!owner)return nullptr;
        if(Record *r=find(owner))return r;
        for(unsigned i=0;i<kRecordCap;++i)if(!rec[i].owner) {memset(&rec[i],0,sizeof(Record));rec[i].owner=owner;return &rec[i];}
        return nullptr;
    }
    bool release(uintptr_t owner) {Record *r=find(owner);if(!r)return false;memset(r,0,sizeof(Record));return true;}
    unsigned used() const {unsigned n=0;for(unsigned i=0;i<kRecordCap;++i)if(rec[i].owner)++n;return n;}
    bool take_full_log() {if(full_logged)return false;full_logged=true;return true;}
};

// ---- guard table self check (4-1): size <= 0x40 (2305D0 alone 0x7A), no zero ids, no duplicate rva ----
inline bool guards_table_ok(const TraitHoverGuard *g,size_t n) {
    const unsigned special=GA(GA_2305D0);
    for(size_t i=0;i<n;++i) {
        if(g[i].rva==0 || g[i].size==0)return false;
        if(g[i].size>((g[i].rva==special)?0x7Au:0x40u))return false;
        for(size_t j=0;j<i;++j)if(g[j].rva==g[i].rva)return false;
    }
    return true;
}
}   // namespace thp
