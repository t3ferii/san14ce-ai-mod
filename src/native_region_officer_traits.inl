// Same effect types as native Related Traits column 38 (34E410/341430).
namespace traits {
struct Definition {bool read=false,related=false;std::wstring name;};
struct Officer {
    std::array<unsigned short,9> slots{};
    std::wstring text;
    unsigned revision=0;
};
static auto &definitions=*new std::array<Definition,356>;
static auto &officers=*new std::unordered_map<Obj,Officer>;
static unsigned revision;
static void clear(){officers.clear();for(auto &d:definitions)d=Definition{};}
static bool related(unsigned kind){return kind==33 || kind==34 || (kind>=36 && kind<=39);}
static const Definition &definition(Obj root,unsigned id) {
    Definition &d=definitions[id];
    if(!d.read) {
        d.read=true;
        Obj data=pointer(root,0x7DD18+id*8);
        if(data && (related(data[0xB6]) || related(data[0xBA]))) {
            const wchar_t *name=reinterpret_cast<const wchar_t *>(data+0x10);
            size_t length=wcsnlen(name,80);
            if(length && length<80){d.related=true;d.name.assign(name,length);}
        }
    }
    return d;
}
static const Officer &read(Obj root,Obj person) { COST_SCOPE("ro_trait_read");
    auto result=officers.try_emplace(person);
    Officer &c=result.first->second;
    std::array<unsigned short,9> slots;
    memcpy(slots.data(),person+0x118,sizeof(slots));
    if(!result.second && slots==c.slots)return c;
    c.slots=slots;c.text.clear();c.revision=++revision;
    std::array<unsigned,9> seen{};unsigned count=0;
    for(unsigned slot=0;slot<slots.size();++slot) {
        unsigned id=slots[slot];if(!id || id>=definitions.size())continue;
        const auto &d=definition(root,id);if(!d.related)continue;
        // Only on a changed snapshot, and only for relevant slots. Preserve
        // native DLC/collaboration availability and the installed 5/9 limit.
        if(std::find(seen.begin(),seen.begin()+count,id)!=seen.begin()+count)continue;
        if(fn<unsigned(__fastcall *)(Obj,unsigned)>(0x2305D0)(person,slot)!=id)continue;
        seen[count++]=id;
        if(!c.text.empty())c.text+=L" · ";
        c.text+=d.name;
    }
    return c;
}
}
static void clear_trait_cache(){traits::clear();view_cache=ViewCache{};}
static Obj make_text(Obj layout,unsigned color) {
    Obj text=fn<Obj(__fastcall *)(size_t)>(0x3F6BC0)(0x140);
    if(!text)return nullptr;
    fn<Obj(__fastcall *)(Obj)>(0x864EA0)(text);
    int ok=fn<int(__fastcall *)(Obj,int,int,int,int,Obj)>(0x86CEC0)(text,0,0,640,24,layout);
    // Same as before: a failed control is hidden and never retried (no partial teardown).
    if(!ok || !pointer(text,0x130)) {fn<void(__fastcall *)(Obj,int)>(0x8741D0)(text,0);return nullptr;}
    fn<void(__fastcall *)(Obj,int)>(0x8740B0)(text,kTraitFont);
    fn<void(__fastcall *)(Obj,int)>(0x8759E0)(text,0); // native left/top alignment
    fn<void(__fastcall *)(Obj,unsigned)>(0x873960)(text,color);
    fn<Frame>(0x866EF0)(text); // CCmnText, NOT parts-layout begin
    return text;
}
static bool create_text(Panel &p,Obj layout) { COST_SCOPE("ro_heavy_create_text5");HeavyTimer timer;
    p.text_attempted=true;
    // Outline copies first: later children draw over earlier ones.
    for(Obj &copy:p.outline)copy=make_text(layout,0xFF000000);
    p.text=make_text(layout,0xFFFFFFFF);
    p.text_begun=true;p.text_visible=true;
    if(!p.text) {show_outline(p,false);return false;}
    return true;
}
static void show_outline(Panel &p,bool value) {
    // Copies follow the main text's state; native calls only on a change.
    if(!p.text_attempted || p.outline_visible==value)return;
    p.outline_visible=value;
    for(Obj copy:p.outline)if(copy)fn<void(__fastcall *)(Obj,int)>(0x8741D0)(copy,value?1:0);
}
static float geometry_float(Obj p,unsigned offset) {
    float value;memcpy(&value,p+offset,sizeof(value));return value;
}
static bool name_end(Obj layout,int &x,int &y) {
    // Native 1A40B0 writes the region name to child +208, part 22.
    // 1A8420 resizes that font node to the actual name, not the label box.
    Obj name=pointer(layout,0x208);
    if(!name || (dword(name,0x40)&3)!=3)return false;
    Obj nodes=pointer(name,0x138),font=nodes?pointer(nodes,22*8):nullptr;
    Obj matrix=font?pointer(font,0xE8):nullptr;
    if(!matrix)return false;
    int width=static_cast<short>(word(font,0xF0)),height=static_cast<short>(word(font,0xF2));
    float w=width*geometry_float(font,0xF4),h=height*geometry_float(font,0xF8);
    float px=geometry_float(matrix,0x30),py=geometry_float(matrix,0x34);
    if(!(w>0 && w<1024 && h>0 && h<256 && px>-4096 && px<4096 && py>-4096 && py<4096))return false;
    unsigned align=font[0x10C];
    // Same local geometry/alignment arithmetic as native 92C980.
    int left=static_cast<int>(px)-static_cast<int>((align>>4)*w*0.5f);
    int top=-static_cast<int>(py)+static_cast<int>((align&15)*h*0.5f);
    x=static_cast<int>(dword(name,0x20))+left+static_cast<int>(w)+4;
    y=static_cast<int>(dword(name,0x24))+top+(static_cast<int>(h)-kTraitFont-4)/2;
    return true;
}
static void update_traits(Panel &p,Obj root,Obj person,Obj layout,int fixed_x,int fixed_y) {
    p.text_wanted=false;
    if(!person || !mod_features.region_officer_traits)return;
    const auto &c=traits::read(root,person);
    if(c.text.empty())return;
    int x=fixed_x,y=fixed_y;
    if(fixed_x==INT_MIN && !name_end(layout,x,y))return;
    if(!p.text_attempted && (!heavy_budget(layout) || !create_text(p,layout)))return;
    if(!p.text || !p.text_begun)return;
    if(p.text_revision!=c.revision) { COST_SCOPE("ro_set_trait_text");
        for(Obj copy:p.outline)if(copy)fn<void(__fastcall *)(Obj,const wchar_t *)>(0x8759A0)(copy,c.text.c_str());
        fn<void(__fastcall *)(Obj,const wchar_t *)>(0x8759A0)(p.text,c.text.c_str());
        p.text_revision=c.revision;
    }
    if(x!=p.text_x || y!=p.text_y) {
        static const int dx[kTraitMain]={-1,1,-1,1},dy[kTraitMain]={-1,-1,1,1};
        for(unsigned i=0;i<kTraitMain;++i)if(p.outline[i])fn<void(__fastcall *)(Obj,int,int)>(0x874650)(p.outline[i],x+dx[i],y+dy[i]);
        fn<void(__fastcall *)(Obj,int,int)>(0x874650)(p.text,x,y);p.text_x=x;p.text_y=y;
    }
    p.text_wanted=true;
}
