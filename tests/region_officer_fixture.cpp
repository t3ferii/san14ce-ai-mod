#define REGION_OFFICER_FIXTURE 1
// The actual display adapter, against fake UI callbacks; no live game access.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
using Obj=unsigned char *;
static Obj image,fixture_world;
static bool enabled=true;
static struct Features {bool region_officers=true,region_officer_traits=true;} mod_features;
static unsigned dword(Obj p,unsigned o){return *reinterpret_cast<unsigned *>(p+o);}
static unsigned word(Obj p,unsigned o){return *reinterpret_cast<unsigned short *>(p+o);}
static Obj pointer(Obj p,unsigned o){return *reinterpret_cast<Obj *>(p+o);}
static void put(Obj p,unsigned o,Obj value){*reinterpret_cast<Obj *>(p+o)=value;}
static void number(Obj p,unsigned o,unsigned value){*reinterpret_cast<unsigned *>(p+o)=value;}
static void short_number(Obj p,unsigned o,unsigned short value){*reinterpret_cast<unsigned short *>(p+o)=value;}
static Obj world(){return fixture_world;}
static Obj table(unsigned o,unsigned id,unsigned max){return fixture_world && id<=max?pointer(fixture_world,o+id*8):nullptr;}
static unsigned checks,frames,allocations,created,portraits,colors,moves,shows,starts,ends,freed,native_leaves,native_destroys;
static unsigned last_color,last_id,last_flags,direct_native_calls;
static int portrait_x,portrait_y;
static Obj last_parent,fixture_popup;
static bool bad_resource;
static Obj latest_text;
static unsigned char fixture_text_root[0x108]{},fixture_font[0x108]{};
static unsigned text_created,text_freed,text_started,text_ended,trait_reads,text_sets;
static std::wstring text_value;
static float fixture_zoom;
static Obj fixture_camera;
static unsigned patch_calls,fail_patch;
static void check(bool value,const char *label){++checks;if(!value){fprintf(stderr,"%s\n",label);exit(1);}}
static Obj __fastcall allocate(size_t n){++allocations;return new unsigned char[n]{};}
static Obj __fastcall construct(Obj p){return p;}
static void __fastcall initialize(Obj){}
static int __fastcall create_face(Obj p,int,int,Obj parent,unsigned){
    ++created;last_parent=parent;put(p,8,parent);number(p,0x40,3);number(p,0x28,64);number(p,0x2C,72);
    put(p,0x138,bad_resource?nullptr:p+0x100);return 1;
}
static void __fastcall scale(Obj,int w,int h){check(w==48 && h==54,"portrait uses native aspect at 75 percent");}
static void __fastcall begin_face(Obj){++starts;}
static void __fastcall end_face(Obj){++ends;}
static void __fastcall show_face(Obj,int){++shows;}
static void __fastcall position(Obj p,int x,int y){++moves;if(p!=latest_text){portrait_x=x;portrait_y=y;}number(p,0x20,x);number(p,0x24,y);}
static LONGLONG fixture_clock,fixture_portrait_cost;
static LONGLONG fixture_now(){return fixture_clock;}
static void __fastcall set_person(Obj,Obj p){fixture_clock+=fixture_portrait_cost;++portraits;last_id=p?word(p,0xA6):0;}
static unsigned __fastcall color(Obj,int){return 0xFF114477;}
static void __fastcall set_color(Obj,int node,unsigned c){check(node==5,"use existing native frame color");++colors;last_color=c;}
static void set_zoom(float value){fixture_zoom=value;number(image+128,0x230,static_cast<unsigned>(value));number(image+128,0x234,static_cast<unsigned>((value-static_cast<unsigned>(value))*10000));}
static void set_popup(Obj value){fixture_popup=value;put(image+32,0x48,value);}
static int __fastcall create_text(Obj p,int,int,int width,int height,Obj parent){
    ++text_created;latest_text=p;put(p,8,parent);put(p,0x130,fixture_font);number(p,0x40,3);
    put(p,0xA0,fixture_text_root);put(p,0x90,fixture_text_root);
    number(p,0x28,width);number(p,0x2C,height);number(p,0x30,255);
    check(width==640 && height==24,"bounded text box");return 1;
}
static void __fastcall font_size(Obj,int value){check(value==18,"readable trait font (native default size)");}
static void __fastcall align_text(Obj,int value){check(value==0,"left aligned text");}
static unsigned outline_colors,white_colors;
static void __fastcall color_text(Obj,unsigned value){check(value==0xFF000000 || value==0xFFFFFFFF,"black outline copies and white text");++(value==0xFF000000?outline_colors:white_colors);}
static void __fastcall set_text(Obj,const wchar_t *value){++text_sets;text_value=value;}
static void __fastcall begin_text(Obj){++text_started;}
static void __fastcall end_text(Obj){++text_ended;}
static Obj __fastcall destroy_text(Obj p,unsigned){++text_freed;delete[] p;latest_text=nullptr;return nullptr;}
static unsigned __fastcall get_trait(Obj p,unsigned slot){++trait_reads;return word(p,0x118+slot*2);}
static Obj __fastcall camera_manager(){return fixture_camera;}
static float __fastcall zoom_level(Obj){return fixture_zoom;}
static int __fastcall visible(Obj p){return p && (dword(p,0x40)&3)==3 && (!pointer(p,8) || visible(pointer(p,8)));}
static int absolute_x(Obj p){return static_cast<int>(dword(p,0x20))+(pointer(p,8)?absolute_x(pointer(p,8)):0);}
static Obj __fastcall popup_manager(){return image+32;}
static Obj __fastcall current_popup(Obj){return fixture_popup;}
static void __fastcall frame(Obj){++frames;}
static void __fastcall face_leave(Obj,Obj){}
static void __fastcall native_leave(Obj,Obj){++native_leaves;}
static Obj __fastcall destroy_face(Obj p,unsigned f){check(f==1,"game allocator owns the face");delete[] p;++freed;return nullptr;}
static Obj __fastcall native_destroy(Obj p,unsigned f){++native_destroys;last_flags=f;return p;}
static void *function(unsigned r) {
    if(r!=0x1A7F50 && r!=0x1A7EA0)++direct_native_calls; // fixture only; no counter in the shipped adapter
    switch(r){
    case 0x864EA0:return reinterpret_cast<void *>(&construct);
    case 0x86CEC0:return reinterpret_cast<void *>(&create_text);
    case 0x8740B0:return reinterpret_cast<void *>(&font_size);
    case 0x8759E0:return reinterpret_cast<void *>(&align_text);
    case 0x873960:return reinterpret_cast<void *>(&color_text);
    case 0x8759A0:return reinterpret_cast<void *>(&set_text);
    case 0x866EF0:return reinterpret_cast<void *>(&begin_text);
    case 0x869850:return reinterpret_cast<void *>(&end_text);
    case 0x877DB0:return reinterpret_cast<void *>(&face_leave);
    case 0x865F30:return reinterpret_cast<void *>(&destroy_text);
    case 0x2305D0:return reinterpret_cast<void *>(&get_trait);
    case 0x3F6BC0:return reinterpret_cast<void *>(&allocate);
    case 0x864C50:return reinterpret_cast<void *>(&construct);
    case 0x92AB70:return reinterpret_cast<void *>(&initialize);
    case 0x1B2E80:return reinterpret_cast<void *>(&create_face);
    case 0x2AD010:return reinterpret_cast<void *>(&color);
    case 0x875B00:return reinterpret_cast<void *>(&scale);
    case 0x866E80:return reinterpret_cast<void *>(&begin_face);
    case 0x869820:return reinterpret_cast<void *>(&end_face);
    case 0x8782E0:return reinterpret_cast<void *>(&face_leave);
    case 0x198BE0:return reinterpret_cast<void *>(&destroy_face);
    case 0x8741D0:return reinterpret_cast<void *>(&show_face);
    case 0x874650:return reinterpret_cast<void *>(&position);
    case 0x1B91E0:return reinterpret_cast<void *>(&set_person);
    case 0x92FA60:return reinterpret_cast<void *>(&set_color);
    case 0x11170:return reinterpret_cast<void *>(&camera_manager);
    case 0xEDE0:return reinterpret_cast<void *>(&zoom_level);
    case 0x86F250:return reinterpret_cast<void *>(&visible);
    case 0x110E0:return reinterpret_cast<void *>(&popup_manager);
    case 0x56FC20:return reinterpret_cast<void *>(&current_popup);
    case 0x1A7F50:return reinterpret_cast<void *>(&frame);
    case 0x1A74D0:return reinterpret_cast<void *>(&native_leave);
    case 0x19B1D0:return reinterpret_cast<void *>(&native_destroy);
    case 0x1A7EA0:return reinterpret_cast<void *>(&frame);
    case 0x1A75A0:return reinterpret_cast<void *>(&native_leave);
    case 0x19B350:return reinterpret_cast<void *>(&native_destroy);
    default:check(false,"unexpected native call");return nullptr;
    }
}
template<class T>static T fn(unsigned r){return reinterpret_cast<T>(function(r));}
namespace region_officers {static void __fastcall frame(Obj);}
static bool patch_pointer(unsigned slot,unsigned original,void *p){
    if(++patch_calls==fail_patch)return false;
    check(pointer(image,slot)==image+original,"only original slot may publish");put(image,slot,reinterpret_cast<Obj>(p));
    if(patch_calls==1)region_officers::frame(nullptr); // publication gate must not touch UI state
    return true;
}
static void restore_pointer(unsigned slot,unsigned original,void *p){if(pointer(image,slot)==p)put(image,slot,image+original);}
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
#include "../src/native_region_officers.inl"
static void seed_profile(const char *path) {
    std::ifstream f(path,std::ios::binary);check(f.good(),"plain current EXE supplied");
    std::vector<char> data((std::istreambuf_iterator<char>(f)),{});
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(data.data());
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64 *>(data.data()+dos->e_lfanew);
    auto sections=IMAGE_FIRST_SECTION(nt);
    for(const auto &p:region_officers::profile) {
        bool found=false;
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
            const auto &s=sections[i];
            if(p.rva>=s.VirtualAddress && p.rva+p.size<=s.VirtualAddress+s.SizeOfRawData){
                memcpy(image+p.rva,data.data()+s.PointerToRawData+p.rva-s.VirtualAddress,p.size);found=true;break;
            }
        }
        check(found,"native guard has file-backed bytes");
    }
    for(const auto &h:region_officers::hooks)put(image,h.slot,image+h.original);
}
int main(int argc,char **argv) {
    check(argc==2,"current plain PE argument required");
    std::vector<unsigned char> bytes(0x2800000),state(0x88000);image=bytes.data();fixture_world=state.data();
    seed_profile(argv[1]);
    region_officers::heavy_clock=&fixture_now; // manual clock: heavy steps cost 0 unless a test says so
    check(region_officers::guards_match(),"current native byte and slot guards pass");
    image[0x1B2E80]^=1;check(!region_officers::guards_match(),"changed face ABI bytes reject installation");image[0x1B2E80]^=1;
    fail_patch=2;check(!region_officers::install(),"partial hook publication fails");
    for(const auto &h:region_officers::hooks)check(pointer(image,h.slot)==image+h.original,"failed publication restores original ownership");
    fail_patch=0;check(region_officers::install(),"all six slots publish once");
    check(!region_officers::install(),"double installation rejected");
    unsigned char context[128]{},area[160]{},tile[64]{},person[512]{},layout[0x230]{},parent[128]{},owner[64]{};
    put(fixture_world,0x871E8,context);context[0x3A]=1;put(fixture_world,0xE270+80*8,tile);tile[0x14]=1;
    put(fixture_world,0xDF30+8,owner);short_number(area,0x3A,80);put(layout,0x1F0,area);put(layout,8,parent);
    number(layout,0x20,400);number(layout,0x24,400);number(layout,0x28,100);number(layout,0x2C,24);
    unsigned char name_label[0x140]{},name_nodes[23*8]{},name_font[0x110]{},name_matrix[0x40]{};
    put(layout,0x208,name_label);put(name_label,0x138,name_nodes);put(name_nodes,22*8,name_font);put(name_font,0xE8,name_matrix);
    number(name_label,0x40,3);short_number(name_font,0xF0,52);short_number(name_font,0xF2,26);
    auto set_float=[](Obj p,unsigned o,float value){memcpy(p+o,&value,sizeof(value));};
    set_float(name_font,0xF4,1);set_float(name_font,0xF8,1);set_float(name_matrix,0x30,24);set_float(name_matrix,0x34,-3);
    number(layout,0x40,3);number(parent,0x40,3);fixture_camera=image+128;
    auto tick=[&](){region_officers::frame(layout);};
    tile[0x14]=2;tick();check(!created,"enemy region creates no resource");tile[0x14]=1;
    number(layout,0x20,2000);tick();check(!created,"offscreen region creates no resource");number(layout,0x20,400);
    set_popup(image+64);tick();check(!created,"modal screens create no resource");set_popup(nullptr);
    number(image,0x21431FC,3);tick();check(!created,"domestic command mode creates no extra portrait");number(image,0x21431FC,0);
    set_zoom(5);tick();check(!created,"far zoom never creates resources");set_zoom(0);
    tick();check(created==1 && region_officers::panels.at(layout).visible,"owned vacant region displays");
    check(last_parent==layout,"portrait is a child of the native region label");
    check(!last_id && last_color==0xFF808080,"vacancy clears the portrait and uses neutral gray frame");
    check(portrait_x==-52 && portrait_y==-15,"portrait sits immediately left of region label and shares its vertical center");
    unsigned baseline_alloc=allocations,baseline_portraits=portraits,baseline_colors=colors,baseline_moves=moves;
    number(layout,0x28,180);tick();
    check(portrait_x==-52 && portrait_y==-15 && moves==baseline_moves,"longer region names do not shift the portrait");
    number(layout,0x28,100);
    for(unsigned i=0;i<10000;++i)tick();
    check(allocations==baseline_alloc && portraits==baseline_portraits && colors==baseline_colors && moves==baseline_moves,
          "steady frames allocate and rebuild no assets or transforms");
    short_number(person,0xA6,321);put(fixture_world,0x158+10*8,person);
    tick();check(portraits==baseline_portraits,"uncommitted officer selection is ignored");
    short_number(area,0x36,10);tick();check(portraits==baseline_portraits+1 && last_id==321,"committed officer change updates once");
    short_number(person,0xA6,322);tick();check(last_id==322,"portrait edit invalidates cached image");
    short_number(area,0x36,0);tick();check(!last_id && last_color==0xFF808080,"removal clears previous officer image");
    number(layout,0x20,410);unsigned before_move=moves;tick();
    check(moves==before_move && absolute_x(region_officers::panels.at(layout).face)==358,"native parent motion needs no copied transform");
    number(layout,0x20,510);
    check(absolute_x(region_officers::panels.at(layout).face)==458,"late camera projection moves child before next adapter tick");
    number(layout,0x40,2);
    check(!visible(region_officers::panels.at(layout).face),"native label hide also hides child before next adapter tick");
    tick();check(!region_officers::panels.at(layout).visible,"hidden label suppresses adapter work");number(layout,0x40,3);
    for(int z=0;z<=4;++z){set_zoom(static_cast<float>(z));tick();check(region_officers::panels.at(layout).visible,"maximum zoom through four wheel-out steps visible");}
    set_zoom(4.01f);tick();check(!region_officers::panels.at(layout).visible,"fifth zoom transition hides immediately");
    set_zoom(6);tick();check(!region_officers::panels.at(layout).visible,"distant overview stays hidden");
    set_zoom(4);tick();check(region_officers::panels.at(layout).visible && allocations==baseline_alloc,"zoom return reuses existing face");
    fixture_camera=nullptr;region_officers::view_cache.ready=false;tick();check(!region_officers::panels.at(layout).visible,"missing camera hides safely");fixture_camera=image+128;set_zoom(0);
    tile[0x14]=2;tick();check(!region_officers::panels.at(layout).visible,"ownership loss hides old portrait");tile[0x14]=1;
    number(image,0x21431FC,7);tick();check(!region_officers::panels.at(layout).visible,"entering regional domestic hides read-only portrait");
    number(image,0x21431FC,0);tick();check(region_officers::panels.at(layout).visible,"return to world map reuses cached portrait");
    unsigned char agriculture[0xE8]{},combat[0xE8]{},commerce[0xE8]{};
    wcscpy_s(reinterpret_cast<wchar_t *>(agriculture+0x10),70,L"농정");agriculture[0xB6]=33;
    wcscpy_s(reinterpret_cast<wchar_t *>(combat+0x10),70,L"맹장");combat[0xB6]=35;
    wcscpy_s(reinterpret_cast<wchar_t *>(commerce+0x10),70,L"징세");commerce[0xBA]=39;
    put(fixture_world,0x7DD18+114*8,agriculture);put(fixture_world,0x7DD18+90*8,combat);put(fixture_world,0x7DD18+113*8,commerce);
    short_number(area,0x36,10);short_number(person,0x118,114);short_number(person,0x11A,90);short_number(person,0x128,113);
    tick();check(text_created==0 && last_id==322,"portrait load spends this frame; trait text resource waits");
    tick();check(text_created==5 && outline_colors==4 && white_colors==1 && text_value==L"농정 · 징세","native relevant types and slot nine appear, combat traits excluded");
    auto &panel=region_officers::panels.at(layout);
    check(pointer(panel.text,8)==layout && static_cast<int>(dword(panel.text,0x20))==80,"text follows actual native name glyph box with four pixel gap");
    check(trait_reads==2,"only relevant changed slots invoke native availability getter");
    check(panel.outline[0] && static_cast<int>(dword(panel.outline[0],0x20))==79 && static_cast<int>(dword(panel.outline[3],0x20))==81,"outline copies sit one pixel around the white text");
    unsigned stable_text=text_sets,stable_reads=trait_reads,stable_calls=direct_native_calls,stable_frames=frames;
    for(unsigned i=0;i<10000;++i)tick();
    check(direct_native_calls==stable_calls,"ten thousand steady callbacks make zero additional direct native calls");
    check(text_sets==stable_text && trait_reads==stable_reads,"stable traits do not rebuild or query native getters");
    check(frames-stable_frames==10000,"original frames counted in fixture only");
    short_number(person,0x11A,114);tick();check(text_value==L"농정 · 징세","duplicate trait names are not repeated");
    number(layout,0x28,180);tick();check(dword(panel.text,0x20)==80,"outer label whitespace does not move traits");number(layout,0x28,100);
    short_number(name_font,0xF0,78);tick();check(dword(panel.text,0x20)==106,"longer native name moves traits to its actual end");short_number(name_font,0xF0,52);
    name_font[0x10C]=0x10;set_float(name_matrix,0x30,50);tick();check(dword(panel.text,0x20)==80,"center aligned native name has same visible right edge");
    name_font[0x10C]=0;set_float(name_matrix,0x30,24);
    number(name_label,0x40,2);tick();check(!panel.text_visible,"hidden native name hides related text");number(name_label,0x40,3);
    put(name_font,0xE8,nullptr);tick();check(!panel.text_visible,"missing name geometry safely hides traits");put(name_font,0xE8,name_matrix);tick();
    set_zoom(5);tick();check(!panel.visible && !panel.text_visible,"far zoom hides portrait and text together");set_zoom(0);tick();
    mod_features.region_officer_traits=false;tick();check(panel.visible && !panel.text_visible,"traits switch hides only the related-trait text");
    mod_features.region_officer_traits=true;tick();check(panel.text_visible,"traits switch on shows text again");
    short_number(area,0x36,0);tick();check(!panel.text_visible && panel.visible,"vacancy keeps portrait and hides stale trait text");
    enabled=false;tick();check(!region_officers::panels.at(layout).visible,"disabled module hides portrait");enabled=true;
    region_officers::leave(layout,image+88);check(text_freed==5 && text_started==text_ended,"text and outline lifecycle is balanced");check(freed==1 && starts==ends && native_leaves==1 && region_officers::panels.empty(),"leave unregisters and destroys exactly once");
    region_officers::destroy(layout,3);check(freed==1 && native_destroys==1 && last_flags==3,"deleting destructor preserves flags and avoids double free");
    region_officers::last_creation=0;bad_resource=true;tick();
    check(!region_officers::panels.at(layout).visible,"failed asset remains hidden");unsigned failed_count=created;
    for(unsigned i=0;i<100;++i)tick();check(created==failed_count,"failed assets do not retry per frame");
    region_officers::destroy(layout,1);check(freed==2 && starts==ends,"unbegun failed resource has no unmatched unregister");
    check(word(area,0x36)==0 && tile[0x14]==1 && word(person,0xA6)==322,"display adapter never writes appointment or game data");
    check(frames>=10100,"original frame callback always executes");
    // Heavy native UI work is split: one face/text resource or portrait load per native frame.
    unsigned char layout2[0x230]{},layout3[0x230]{};memcpy(layout2,layout,sizeof(layout2));memcpy(layout3,layout,sizeof(layout3));
    bad_resource=false;short_number(area,0x36,10);
    auto &panels=region_officers::panels;
    unsigned c0=created,p0=portraits,t0=text_created;
    region_officers::last_creation=0;region_officers::frame(layout2);
    check(created==c0+1 && portraits==p0 && !panels.at(layout2).visible,"new label creates only its face in the first frame");
    region_officers::frame(layout2);
    check(portraits==p0+1 && text_created==t0 && panels.at(layout2).visible,"portrait image loads on the next frame; text waits");
    region_officers::frame(layout2);
    check(text_created==t0+5 && panels.at(layout2).text_visible,"trait text resource is created on a later frame");
    region_officers::last_creation=0;region_officers::frame(layout3);
    check(created==c0+1,"another label waits while this frame's budget is spent");
    for(int i=0;i<3;++i){region_officers::frame(layout2);region_officers::frame(layout3);}
    check(created==c0+2 && text_created==t0+10 && panels.at(layout3).text_visible,"second label completes in three frames");
    auto pass=[&](){region_officers::frame(layout2);region_officers::frame(layout3);};
    short_number(person,0xA6,323);unsigned p1=portraits;
    pass();check(portraits==p1+1 && panels.at(layout2).visible!=panels.at(layout3).visible,"two portrait loads never share one native frame");
    pass();pass();check(portraits==p1+2 && panels.at(layout3).visible && panels.at(layout2).visible,"deferred portrait lands within two frames");
    // v12.19 spacing: a heavy step that took 25 units blocks the next one for 50 units.
    fixture_portrait_cost=25;short_number(person,0xA6,324);unsigned q0=portraits;
    pass();check(portraits==q0+1,"first slow portrait load runs");
    pass();pass();check(portraits==q0+1,"next portrait waits while the cooldown runs");
    fixture_clock+=49;pass();check(portraits==q0+1,"cooldown lasts twice the measured load");
    fixture_clock+=1;pass();pass();check(portraits==q0+2,"after the cooldown the deferred portrait loads");
    fixture_portrait_cost=0;fixture_clock+=1000;
    unsigned budget_calls=direct_native_calls;
    for(unsigned i=0;i<5000;++i)pass();
    check(direct_native_calls==budget_calls,"budget bookkeeping adds no steady native calls");
    region_officers::leave(layout2,image+88);region_officers::leave(layout3,image+88);
    check(panels.empty() && starts==ends && text_started==text_ended && text_freed==15,"split resources release once");
    // City label (CFootholdInfoLayout): +1F0 object +4E base -> building +10 hex -> tile +12 area -> officer.
    unsigned char city_layout[0x230]{},base_obj[0x60]{},building[0x20]{},city_tile[0x20]{},city_area[0x60]{};
    memcpy(city_layout,layout2,sizeof(city_layout));put(city_layout,0,image+0x13999B8);put(city_layout,0x1F0,base_obj);
    short_number(base_obj,0x4E,7);put(fixture_world,0x6DA98+7*8,building);short_number(building,0x10,90);
    put(fixture_world,0xE270+90*8,city_tile);city_tile[0x14]=1;short_number(city_tile,0x12,33);
    put(fixture_world,0x6CAF0+33*8,city_area);short_number(city_area,0x36,10);short_number(city_area,0x3A,90);
    region_officers::last_creation=0;
    for(int i=0;i<8;++i)region_officers::city_frame(city_layout);
    check(panels.count(city_layout) && panels.at(city_layout).visible && last_id==324,"city label shows the city area's development officer");
    check(panels.at(city_layout).text_visible && static_cast<int>(dword(panels.at(city_layout).text,0x20))==panels.at(city_layout).x,"city traits sit under the portrait");
    city_tile[0x14]=2;region_officers::city_frame(city_layout);check(!panels.at(city_layout).visible,"enemy city hides");city_tile[0x14]=1;
    short_number(base_obj,0x4E,0x34);region_officers::city_frame(city_layout);check(!panels.at(city_layout).visible,"gates are excluded");
    region_officers::city_leave(city_layout,image+88);check(!panels.count(city_layout) && starts==ends,"city label leave releases once");
    region_officers::city_destroy(city_layout,1);
    printf("{\"status\":\"region_officer_fixture_passed\",\"checks\":%u,\"steady_frames\":10000,\"steady_asset_rebuilds\":0,\"live_game_tested\":false}\n",checks);
}
