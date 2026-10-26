/* Exercises the actual adapter with simulated native helpers. Never attaches
 * to a game process. Passing these tests does not establish the game's ABI. */
#include <vector>
#include <map>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
struct FixtureFlush {const void *address;SIZE_T size;};
static std::vector<FixtureFlush> fixture_flushes;
static BOOL WINAPI fixture_flush(HANDLE process,LPCVOID address,SIZE_T size) {
    fixture_flushes.push_back({address,size});
    return FlushInstructionCache(process,address,size);
}
#define FlushInstructionCache fixture_flush
#include "../src/native_adapter.cpp"
#undef FlushInstructionCache
static std::map<unsigned, void *> functions;
static std::map<Obj, unsigned> army_ids;
static void *fixture_function(unsigned rva) {
    auto found = functions.find(rva);
    if (found == functions.end()) { fprintf(stderr, "Missing fixture RVA %X\n", rva); abort(); }
    return found->second;
}
static Obj root, player, area, origin, district, goal, current_army, goal_owner;
static Obj heads[4096];
static unsigned next_handle, frees, native_cuts, original_nodes, finalized, selected_target;
static unsigned legal_facilities = ~0u;
static int money = 10000;
static bool cut_available;
static bool cap_reached;
static std::vector<unsigned> candidates_requested;
static std::map<Obj,Obj> tile_areas;
static std::vector<Obj> region_tiles;
static int fixture_day=100;
static bool region_available=true;
static bool path_available=true, wrong_waypoint=false, field_alternative=false;
static unsigned build_permission_calls;
static Obj permission_force,permission_tile,permission_definition;
static int permission_policy;
static bool __fastcall mock_build_permission(Obj force,Obj tile,Obj definition,int policy) {
    ++build_permission_calls;permission_force=force;permission_tile=tile;
    permission_definition=definition;permission_policy=policy;return policy==7;
}
static void put16(Obj p, unsigned offset, unsigned value) { *reinterpret_cast<unsigned short *>(p + offset) = static_cast<unsigned short>(value); }
static void putptr(Obj p, unsigned offset, void *value) { *reinterpret_cast<void **>(p + offset) = value; }
static Obj object(unsigned id) { Obj p = static_cast<Obj>(calloc(1, 0x400)); put16(p, 0x10, id); return p; }
static void slot(unsigned offset, unsigned id, Obj obj) { putptr(root, offset + id * 8, obj); }
static int __fastcall mock_valid(Obj p) { return p && word(p, 0x10); }
static unsigned __fastcall mock_force(Obj army) { return army[0x150]; }
static Obj __fastcall mock_player(Obj) { return player; }
static unsigned __fastcall mock_id(Obj obj) { return word(obj, 0x10); }
static unsigned __fastcall mock_army_id(Obj obj) { return army_ids.at(obj); }
static int __fastcall mock_resource(Obj) { return money; }
static int __fastcall mock_hostile(unsigned a, unsigned b) { return a != b; }
static Obj __fastcall mock_area(Obj tile) { auto found=tile_areas.find(tile);return found==tile_areas.end()?area:found->second; }
static int __fastcall mock_distance(int a, int b) { return abs(a - b); }
static Obj __fastcall mock_origin(Obj) { return origin; }
static Obj __fastcall mock_district(Obj) { return district; }
static Obj __fastcall mock_goal(Obj, unsigned) { return goal; }
static Obj __fastcall mock_goal_force(Obj) { return goal_owner; }
static int __fastcall mock_day(Obj) { return fixture_day; }
static unsigned __fastcall mock_goal_hex(Obj) { return 200; }
static float __fastcall mock_route_speed(Obj,Obj,int,int,Obj tile) {return region_available && (!tile || valid(tile))?1.0f:0.0f;}
static int __fastcall mock_daily_points(Obj army) {float s=*reinterpret_cast<float *>(army+0x180);return s>0?static_cast<int>(s*100):100;}
static Obj __fastcall mock_terrain(Obj tile) {return tile;}
static int __fastcall mock_hex_cost(Obj) {return 100;}
static int __fastcall mock_zone_flag(Obj a) { return a && a[0x34]?1:0; }
static bool __fastcall mock_cap(Obj) {return cap_reached;}
static float __fastcall mock_adjacency(Obj,int,int *score) {*score=300;return 1.72f;}
static float __fastcall mock_partial_adjacency(Obj,int,int *score) {*score=245;return 1.588f;}
static float __fastcall mock_medium_adjacency(Obj,int,int *score) {*score=300;return 2.064f;}
static float observed_adjacency;
static void __fastcall mock_pair(Obj,Obj,Obj) {int score=0;observed_adjacency=contextual_adjacency(table(0xE270,100,48399),2,&score);}
static Obj active_formation;
static bool apply_actual_damage=true;
static unsigned damage_calls;
static uint64_t __fastcall mock_damage(Obj manager,Obj pair,Obj output) {
    ++damage_calls;
    if(manager!=output)abort(); // three argument forwarding, including null
    if(apply_actual_damage && pair && dword(pair+8,4)==27) {
        Obj victim=table(0x7E838,word(pair+8,0),500);
        if(valid(victim) && word(victim,0x16)>10)put16(victim,0x16,word(victim,0x16)-10);
    }
    return 0x123456789ABCDEF0ull;
}
static Obj __fastcall mock_active_formation(Obj) {return active_formation;}
static void __fastcall mock_nearby(Obj anchor,int radius,GameList *out) {
    if(radius==1) {
        unsigned hex=mock_id(anchor);
        Obj adjacent=table(0xE270,hex-1,48399);
        if(valid(adjacent))list_add(*out,adjacent);
        return; // The native ring excludes its anchor center.
    }
    for(Obj tile:region_tiles)if(tile!=anchor)list_add(*out,tile);
}
static int __fastcall mock_region(Obj,Obj army,Obj tile,int radius,int mode,int apply) {
    if(radius!=1 || mode!=0 || apply!=1)abort();
    if(!region_available)return 0;
    unsigned selected=mock_id(tile)+1;
    Obj center=table(0xE270,selected,48399);
    if(!valid(center) || center[0x14]==mock_force(army))return 0;
    put16(army,0x2C,wrong_waypoint?101:selected);return 1;
}
static void __fastcall mock_init(GameList *list) { list->handle = new unsigned(++next_handle); heads[next_handle] = nullptr; }
static Obj __fastcall mock_append(Obj, unsigned handle, unsigned char) {
    Link *link = new Link{nullptr, nullptr};
    auto end = reinterpret_cast<Link **>(&heads[handle]);
    while (*end) end = &(*end)->next;
    *end = link; return reinterpret_cast<Obj>(link);
}
static void __fastcall mock_free(GameList *list) {
    if (!list->handle) return;
    auto link = reinterpret_cast<Link *>(heads[*list->handle]);
    while (link) { auto next = link->next; delete link; link = next; }
    heads[*list->handle] = nullptr; delete list->handle; list->handle = nullptr; ++frees;
}
static int __fastcall mock_predicate(unsigned, GameList *, GameList *enemies, Obj *target) {
    ++native_cuts; auto first = list_first(*enemies);
    if (!cut_available || !first) return 0;
    for (auto link = first; link; link = link->next) {
        if (in_cut && mock_force(link->value) != 1) abort();
    }
    *target = first->value; selected_target = mock_army_id(*target); return 1;
}
static int __fastcall mock_cut(Obj army, GameList *route, Obj *target) {
    GameList enemies; list_init(enemies);
    for (unsigned i = 1; i <= 500; ++i) {
        Obj candidate = table(0x7E838, i, 500);
        if (valid(candidate) && candidate != army) list_add(enemies, candidate);
    }
    int result = cut_player_predicate(mock_force(army), route, &enemies, target);
    if(result)put16(army,0x2C,104);
    list_free(enemies); return result;
}
static void __fastcall mock_clear(Obj army) {
    for(unsigned j=0;j<5;++j)put16(army,0x2C+j*2,48400);
    *reinterpret_cast<unsigned *>(army+0x1F0)=0;
}
static void __fastcall mock_waypoint(Obj army,int index,unsigned hex) {put16(army,0x2C+index*2,hex);}
static void __fastcall mock_action(Obj army, int action) { army[0x37] = static_cast<unsigned char>(action); }
static void __fastcall mock_target(Obj army, int type, int id) { army[0x38] = static_cast<unsigned char>(type); put16(army, 0x3A, static_cast<unsigned>(id)); }
static void __fastcall mock_finalize(Obj army) {
    ++finalized;bool permitted=path_available && region_available;unsigned target=word(army,0x2C);
    if(target>=48400 && army[0x38]==9)target=word(army,0x3A);
    unsigned here=word(army,0x2A),next=target;
    if(target<48400 && distance(here,target)>1) {
        int range=distance(here,target);
        for(unsigned d=0;d<6;++d) {unsigned h=sortie::neighbor(here,d);if(h<48400 && distance(h,target)<range) {next=h;range=distance(h,target);}}
    }
    if(campaign_native_trace && permitted && !wrong_waypoint && target<48400) {
        campaign_native_trace->clear();campaign_native_trace->push_back(here);unsigned h=here;
        for(unsigned step=0;h!=target && step<48400;++step) {
            unsigned best=h;int range=distance(h,target);
            for(unsigned d=0;d<6;++d) {unsigned cell=sortie::neighbor(h,d);if(cell<48400 && distance(cell,target)<range) {best=cell;range=distance(cell,target);}}
            if(best==h)break;h=best;campaign_native_trace->push_back(h);
        }
    }
    put16(army,0x48,permitted?(wrong_waypoint?48400:next):48400);
}
static int __fastcall mock_cost(Obj, Obj) { return 100; }
static int __fastcall mock_possible(GameList *people, Obj, Obj def) {
    return list_first(*people) && (legal_facilities & (1u << word(def, 0x10)));
}
static void __fastcall mock_candidates(Obj, Obj, Obj def, GameList *out, Obj, Obj, Obj next) {
    if (next) abort(); candidates_requested.push_back(word(def, 0x10)); list_add(*out, table(0xE270, 105, 48399));
}
static Obj building;
static Obj __fastcall mock_building(Obj) { return building; }
static int __fastcall mock_maximum(Obj) { return 1000; }
static uint64_t __fastcall mock_original(Obj, Obj, Obj) { ++original_nodes; return 2; }
static uint64_t __fastcall mock_city_capture(Obj, Obj, Obj) {
    table(0xE270,100,48399)[0x14] = 2; ++original_nodes; return 1;
}
static uint64_t __fastcall mock_alternative(Obj, Obj, Obj army) {
    if(!field_alternative)return 2;
    mock_action(army,1);mock_target(army,9,150);put16(army,0x2C,150);mock_finalize(army);return 1;
}
static Obj __fastcall mock_resolve(Obj, Obj supplied) { return supplied; }
static Obj __fastcall mock_blackboard(Obj context) { return context; }
static void __fastcall mock_variable(Obj, unsigned, Obj *out, int) { *out = current_army; }
#define BIND(rva, name) functions[rva] = reinterpret_cast<void *>(&name)
static void setup() {
    image = static_cast<Obj>(VirtualAlloc(nullptr, 0x2800000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    root = static_cast<Obj>(calloc(1, 0x90000)); putptr(image, 0x277DF18, root);
    putptr(image, 0x27D10E8, image); putptr(image, 0x27D10F0, heads);
    *reinterpret_cast<unsigned *>(image + 0x27D1120) = 4096;
    wcscpy_s(log_path, L"native_fixture.log");
    player = object(1); player[0x3A]=1; Obj forcevt = static_cast<Obj>(calloc(1, 0x100)); putptr(forcevt, 0x60, reinterpret_cast<void *>(&mock_id)); putptr(player, 0, forcevt);
    putptr(root, 0x871E8, player);Obj ownforce=object(2);putptr(ownforce,0,forcevt);slot(0xDF30,2,ownforce);goal_owner=player;
    area = object(1); put16(area, 0x3A, 100);slot(0x6CAF0,1,area);
    origin = object(1); Obj originvt = static_cast<Obj>(calloc(1, 0x100)); putptr(originvt, 0x90, reinterpret_cast<void *>(&mock_resource)); putptr(origin, 0, originvt);
    district = object(1); goal = object(1);*reinterpret_cast<unsigned *>(goal+0x3C)=1000000; building = object(1);put16(origin,0x4E,9);
    for (unsigned i=100;i<=119;++i) { Obj tile = object(i);put16(tile,0x12,1);tile[0x14] = 2; slot(0xE270, i, tile); }
    for (unsigned i : {6u,4u,7u,3u,11u,12u,14u,13u}) slot(0x73958, i, object(i));
    slot(0x158, 1, object(1)); Obj troops = object(1); put16(troops, 0x14, 8); put16(troops, 0x16, 1); slot(0x7F8D8, 1, troops);
    BIND(0x32F500,mock_valid); BIND(0x22CC40,mock_force); BIND(0x32E910,mock_player); BIND(0x2CA280,mock_hostile);
    BIND(0x229EA0,mock_area); BIND(0x2AA850,mock_distance); BIND(0x21E2B0,mock_init); BIND(0x18DF0,mock_append); BIND(0x424D60,mock_free);
    BIND(0xC0C80,mock_predicate); BIND(0xD06D0,mock_cut); BIND(0x22DA10,mock_army_id); BIND(0x22DDA0,mock_id);
    BIND(0xA2410,mock_clear); BIND(0x2EAAE0,mock_action); BIND(0x2EAB40,mock_target); BIND(0x26C940,mock_finalize);
    BIND(0x2EE1E0,mock_waypoint);
    BIND(0x22A5A0,mock_origin); BIND(0x22BAA0,mock_district); BIND(0x32C130,mock_goal); BIND(0x22C990,mock_goal_force);
    BIND(0x2AB390,mock_cost); BIND(0x278FA0,mock_possible); BIND(0xB9070,mock_candidates); BIND(0x22FB10,mock_building); BIND(0x2AC120,mock_maximum);
    BIND(0x201E0,mock_original); BIND(0x1F3A0,mock_original); BIND(0x68CE0,mock_original); BIND(0xB6070,mock_resolve);
    BIND(0xBF7A40,mock_blackboard); BIND(0xB26470,mock_variable);
    BIND(0x61120,mock_original); BIND(0x32BA50,mock_day); BIND(0x22D740,mock_goal_hex);
    BIND(0x2A7AA0,mock_nearby); BIND(0x232680,mock_zone_flag); BIND(0x232BF0,mock_zone_flag); BIND(0xCC560,mock_region);
    original_damage=mock_damage;BIND(0x22B090,mock_active_formation);BIND(0x2B0290,mock_route_speed);
    BIND(0x269150,mock_daily_points);BIND(0x2318B0,mock_terrain);BIND(0xBE120,mock_hex_cost);
    BIND(0x2C9E30,mock_cap);BIND(0x16A030,mock_pair);original_adjacency=mock_adjacency;
    Obj manager=object(1);putptr(manager,0x28,manager);putptr(image,0x1B9A4A0,manager);
    for(unsigned rva : {0x20060u,0x1FFE0u,0x1FF60u,0x1F120u,0x1FEA0u,0x1FF00u}) BIND(rva,mock_alternative);
    enabled = 1;
}
static unsigned checks,failures;
static void check(bool condition, const char *description) {
    if (!condition) { fprintf(stderr, "FAIL: %s\n", description); ++failures; }
    ++checks;
}
#include "domestic_fixture.inl"
#include "obstacle_fixture.inl"
#include "campaign_revision_fixture.inl"
#include "campaign_support_fixture.inl"
#include "campaign_operation_fixture.inl"
#include "crash_diagnostic_fixture.inl"
#include "expert_settings_fixture.inl"
#ifdef SORTIE_FORMATION
#include "sortie_fixture.inl"
#include "district_intercept_fixture.inl"
#endif
// attack_recheck_fix: blackboard with two ints; originals replay #10114 (assign) and #10115 (A -= B).
static int recheck_vars[2];static bool recheck_set[2];
static int recheck_index(unsigned key) {return key==0x47EAF02Bu?0:key==0xB57EAF65u?1:-1;}
static Obj __fastcall recheck_blackboard(Obj) {static unsigned char board[8];return board;}
static char __fastcall recheck_read(Obj,unsigned key,int *out,int) {int i=recheck_index(key);if(i<0||!recheck_set[i])return 0;*out=recheck_vars[i];return 1;}
static char __fastcall recheck_write(Obj,unsigned key,int v) {int i=recheck_index(key);if(i<0)return 0;recheck_vars[i]=v;recheck_set[i]=true;return 1;}
static uint64_t __fastcall recheck_strength(Obj node,Obj,Obj) {recheck_write(nullptr,*reinterpret_cast<unsigned *>(node+0x3C),3000);return 1;}
static uint64_t __fastcall recheck_minus(Obj node,Obj,Obj) {
    int a=0,b=0;recheck_read(nullptr,*reinterpret_cast<unsigned *>(node+0x38),&a,0);recheck_read(nullptr,*reinterpret_cast<unsigned *>(node+0x3C),&b,0);
    recheck_write(nullptr,*reinterpret_cast<unsigned *>(node+0x38),a-b);return 1;
}
// mission_v2 corridor: S(401 city) .. G(402 castle); 403 own, 404/405 hostile, 406 hostile port,
// 410 gate with 411 behind it, 407-408-412-409 a two-region detour.
static Obj corridor_origin_city;
static int __fastcall corridor_core(Obj a) {return a && a[0x34]==1;}
static Obj __fastcall corridor_origin(Obj) {return corridor_origin_city;}
static unsigned __fastcall corridor_hex(Obj o) {return word(o,0x20);}
static void corridor_tests() {
    BIND(0x232680,corridor_core);BIND(0x22A5A0,corridor_origin);BIND(0x22D740,corridor_hex);
    const unsigned edges[][2]={{401,403},{401,406},{401,410},{403,404},{403,405},{404,402},{404,406},{405,402},
        {410,411},{411,402},{401,407},{407,408},{408,412},{412,409},{409,402}};
    Obj areas[13];
    for(unsigned i=0;i<13;++i) {unsigned id=400+i;Obj a=object(id);put16(a,0x3A,40000+i);slot(0x6CAF0,id,a);areas[i]=a;
        Obj t=object(40000+i);put16(t,0x12,id);t[0x14]=2;slot(0xE270,40000+i,t);}
    for(auto &e:edges)for(unsigned k=0;k<2;++k) {Obj a=areas[e[k]-400];unsigned n=a[0x67];
        put16(a,0x56+n*2,e[1-k]);a[0x66]|=static_cast<unsigned char>(1u<<n);a[0x67]=static_cast<unsigned char>(n+1);}
    areas[1][0x34]=1;areas[2][0x34]=1;areas[10][0x34]=1;areas[6][0x34]=2;   // cities, gate, port
    table(0xE270,40001,48399)[0x14]=1;table(0xE270,40003,48399)[0x14]=1;  // departure city, own region
    corridor_origin_city=object(1);put16(corridor_origin_city,0x20,40001);
    Obj lead=object(77),troops=object(5);lead[0x150]=1;put16(lead,0x58,5);put16(troops,0x16,9);
    for(unsigned i=0;i<10;++i)put16(troops,0x18+i*2,48400);put16(troops,0x18,40099);
    mission_route_corridor(troops,lead,40002);
    check(word(troops,0x18)==40099 && word(troops,0x1A)==40006 && word(troops,0x1C)==40004 && word(troops,0x1E)==40005 &&
          word(troops,0x20)==48400,"mission_v2 corridor: shortest+1 regions fill empty slots after the original entry, port included, gate and detour excluded");
    for(unsigned i=1;i<10;++i)put16(troops,0x18+i*2,48400);
    mission_route_corridor(troops,lead,40002);
    check(word(troops,0x1A)==48400,"mission_v2 corridor: offered once per goal, the original list consumption is not refilled");
    put16(troops,0x16,8);mission_route_corridor(troops,lead,40002);
    check(word(troops,0x1A)==40006,"mission_v2 corridor: a new goal gets a fresh offer");
    for(unsigned i=0;i<13;++i) {slot(0x6CAF0,400+i,nullptr);slot(0xE270,40000+i,nullptr);}
    BIND(0x232680,mock_zone_flag);BIND(0x22A5A0,mock_origin);BIND(0x22D740,mock_goal_hex);
}
static void recheck_tests() {
    BIND(0xBF7A40,recheck_blackboard);BIND(0xB26300,recheck_read);BIND(0xB26990,recheck_write);
    BIND(0x7DC00,recheck_strength);BIND(0x91030,recheck_minus);
    Obj strength=object(1),minus=object(1);
    *reinterpret_cast<unsigned *>(strength+0x3C)=0x47EAF02Bu;*reinterpret_cast<unsigned *>(strength+0x40)=5;
    *reinterpret_cast<unsigned *>(minus+0x38)=0xB57EAF65u;*reinterpret_cast<unsigned *>(minus+0x3C)=0x47EAF02Bu;
    for(int fix=0;fix<2;++fix) {
        mod_features.attack_recheck_fix=fix!=0;
        recheck_write(nullptr,0x47EAF02Bu,40000);recheck_write(nullptr,0xB57EAF65u,30000);
        for(int base=0;base<2;++base) {foothold_strength_node(strength,strength,strength);variable_minus_node(minus,minus,minus);}
        if(fix) check(recheck_vars[0]==40000 && recheck_vars[1]==24000,"attack_recheck_fix: base power still reduces enemy, friendly attack sum survives the base loop");
        else check(recheck_vars[0]==3000 && recheck_vars[1]==24000,"attack_recheck_fix off: original overwrites the friendly sum with one base");
    }
    mod_features.attack_recheck_fix=false;
}
int main() {
    setup();
    crash_diagnostic_tests();
    Obj army = object(1); army[0x150] = 2; put16(army,0x12,1); put16(army,0x58,1); put16(army,0x2A,101); slot(0x7E838,1,army); current_army = army;
    put16(army,0x16,6000);
    Obj ai_enemy = object(2); ai_enemy[0x150] = 3; slot(0x7E838,2,ai_enemy);
    Obj human = object(3); human[0x150] = 1; slot(0x7E838,3,human);
    army_ids[army]=1;army[0x1A]=100; army_ids[ai_enemy]=2;ai_enemy[0x1A]=100; army_ids[human]=3;human[0x1A]=100;
    put16(army,0x10,1); put16(ai_enemy,0x10,1); put16(human,0x10,1);
    expert_settings_tests();
    cut_available = true;
    check(try_cut(army) && selected_target == 3, "cut candidates exclude other computers");
    check((dword(army,0x1F0)&8) && dword(army,0x1F4)==3,"cut flags and target assigned");
    unsigned calls = native_cuts;
    check(!try_cut(human) && calls==native_cuts,"player orders preserved");
    check(cut_node(nullptr,nullptr,human)==2 && original_nodes==1,"player cut node invokes original");
    cut_available = false;
    check(!try_cut(army),"no feasible native cut falls through");
    auto p = observe_capture(army); check(!p->credit,"existing friendly region does not grant capture");
    auto other_progress = observe_capture(ai_enemy);
    check(p!=other_progress && p->army==army && other_progress->army==ai_enemy,
          "units with the same state field keep distinct capture histories");
    check(observe_capture(army)==p && p->army==army,
          "observing another unit does not reset the first unit's history");
    auto center = table(0xE270,100,48399); center[0x14] = 3; observe_capture(army); center[0x14]=2; observe_capture(army);
    check(p->credit,"ownership transition near army grants capture");
    legal_facilities = (1u<<4)|(1u<<7)|(1u<<3);
    check(try_build(army,*p) && p->facility==4,"unavailable catapult falls through to fortress");
    check(!p->credit && army[0x37]==8 && army[0x38]==9 && word(army,0x42)==105,"build consumes one capture and sets native target");
    check(continue_build(army,*p),"keep planned construction before city attack");
    auto tile = table(0xE270,105,48399); put16(tile,0x1A,1); building[0x12]=4; building[0x13]=2; put16(building,0x14,500);
    check(continue_build(army,*p),"unfinished building continues");
    put16(building,0x14,1000); check(!continue_build(army,*p) && !p->facility,"completed building releases army"); put16(tile,0x1A,0);
    p->credit=true; money=0; check(!try_build(army,*p) && p->credit,"insufficient money does not issue building"); money=10000;
    fixture_day+=10;legal_facilities=~0u; check(try_build(army,*p) && p->facility==6,"catapult has first priority after temporary resource retry");
    p->facility=0; p->credit=true; put16(ai_enemy,0x2A,105);
    check(!try_build(army,*p),"occupied hex is rejected"); put16(ai_enemy,0x2A,0);
    army[0x37]=4; army[0x38]=5; p->credit=false;
    check(apply_priority(army,nullptr,nullptr) && p->build_chain && army[0x37]!=11,"temporarily blocked captured-base construction cannot fall through into native retreat");
    Obj node=object(1); cut_available=true;
    check(first_army_node(node,node,army)==1,"army decision returns success for player cut");
    cut_available=false;
    Obj troops2=object(1); put16(troops2,0x14,8); put16(troops2,0x16,1); slot(0x7F8D8,2,troops2);
    put16(army,0x58,2); army[0x37]=4; army[0x38]=5;
    memset(&progress[1],0,sizeof(progress[1])); center[0x14]=3;
    check(belongs_to_troops(army,troops2) && !belongs_to_troops(ai_enemy,troops2),
          "troop membership resolves object identity despite shared state fields");
    BIND(0x68CE0,mock_city_capture);
    check(city_node(node,node,troops2)==1 && army[0x37]==8 && army[0x46]==6,
          "city hook observes capture and builds for the actual troop group");
    BIND(0x68CE0,mock_original);
    // A new operation after the previous construction test. Target city 9
    // remains the strategic goal even when native construction changes action.
    put16(goal,0x4E,9);put16(troops2,0x16,9);put16(troops2,0x14,10);put16(army,0x16,6000);
    memset(&progress[1],0,sizeof(progress[1]));*reinterpret_cast<unsigned *>(army+0x1F0)=0;p=observe_capture(army);remember_campaign(army,*p);
    check(p->campaign && p->city==9,"offensive goal remembered independently of construction action");
    Obj region1=object(110),region2=object(120);region1[0x14]=3;region2[0x14]=3;
    slot(0xE270,110,region1);slot(0xE270,120,region2);
    Obj area1=object(1),area2=object(1);put16(area1,0x3A,110);put16(area2,0x3A,120);
    tile_areas[region1]=area1;tile_areas[region2]=area2;
    slot(0x6CAF0,1,area);slot(0x6CAF0,2,area1);slot(0x6CAF0,3,area2);
    Obj city_area=object(4);city_area[0x34]=1;put16(city_area,0x3A,200);slot(0x6CAF0,4,city_area);
    put16(area,0x56,2);area[0x66]=1;
    put16(area1,0x56,1);put16(area1,0x58,3);area1[0x66]=3;
    put16(area2,0x56,2);put16(area2,0x58,4);area2[0x66]=3;
    put16(city_area,0x56,3);city_area[0x66]=1;
    for(unsigned h=100;h<=300;++h) {
        Obj t=table(0xE270,h,48399);if(!valid(t)) {t=object(h);t[0x14]=2;slot(0xE270,h,t);}
        unsigned id=h<110?1:h<120?2:h==200?4:3;put16(t,0x12,id);
        tile_areas[t]=table(0x6CAF0,id,500);
    }

    region_tiles={region1,region2};
    check(campaign_step(army,*p) && p->region_target==110 && word(army,0x2C)==48400 && word(army,0x3A)==110,
          "nearest enemy region chosen before a city attack");
    check(campaign_step(army,*p) && p->region_target==110,"unfinished region mission retained");
    region1[0x14]=2;put16(army,0x2A,110);observe_capture(army);
    check(p->credit,"next captured region grants fresh construction credit");
    check(try_build(army,*p) && p->facility==6,"newly captured region repeats construction");
    put16(tile,0x1A,1);building[0x12]=6;building[0x13]=2;put16(building,0x14,1000);
    check(!continue_build(army,*p) && p->finished_build,"finished construction keeps remembered campaign");
    check(campaign_step(army,*p) && p->region_target==120 && p->city==9,
          "construction completion proceeds to another region instead of returning home");
    region2[0x14]=2;region_tiles.clear();p->region_target=48400;p->facility=0;p->credit=false;p->build_chain=false;p->staging_hex=48400;
    put16(army,0x2A,195);put16(troops2,0x14,10);fixture_day=100;
    check(campaign_step(army,*p) && army[0x37]==4 && word(army,0x3A)==9,
          "no feasible field tasks permit city fallback without any assembly wait");
    fixture_day=120;
    check(campaign_step(army,*p) && army[0x37]==4,"later callback cannot reintroduce a rally hold");
    // Use two additional independent troop groups to verify the order batch
    // can coordinate troops, rather than only members of one native group.
    Obj partners[2];
    for(unsigned j=0;j<2;++j) {
        unsigned id=j+4;Obj mate=object(1);partners[j]=mate;army_ids[mate]=id;mate[0x1A]=100;slot(0x7E838,id,mate);
        mate[0x150]=2;put16(mate,0x12,1);put16(mate,0x16,5000);put16(mate,0x2A,194-j);put16(mate,0x58,3+j);
        Obj troop=object(1);put16(troop,0x14,10);put16(troop,0x16,9);slot(0x7F8D8,3+j,troop);
        auto mp=observe_capture(mate);remember_campaign(mate,*mp);mp->staging_hex=static_cast<unsigned short>(194-j);
    }
    check(campaign_step(army,*p) && army[0x37]==4 && partners[0][0x37]==0 && partners[1][0x37]==0,
          "one army decides independently without forcing other troop groups into an assault batch");
    p->built_once=true;p->finished_build=true;p->return_deadline_day=fixture_day+10;put16(troops2,0x14,7);put16(troops2,0x16,101);
    remember_campaign(army,*p);
    check(p->campaign && word(troops2,0x14)==10 && word(troops2,0x16)==9,
          "construction-triggered return goal is replaced with the remembered enemy city");
    calls=original_nodes;
    check(retreat_node(node,node,army)==1 && original_nodes==calls,
          "completed campaign construction does not enter native return node");
    put16(army,0x16,999);calls=original_nodes;
    check(retreat_node(node,node,army)==2 && original_nodes==calls+1,
          "depleted army retains native retreat behavior");
    remember_campaign(army,*p);check(!p->campaign,"depleted army leaves the assembly roster");
    put16(army,0x16,6000);remember_campaign(army,*p);goal_owner=table(0xDF30,2,51);
    check(!hostile_city(army,9),"captured or friendly city is not a campaign target");
    remember_campaign(army,*p);
    check(!p->campaign,"city capture ends the operation and clears its release state");
    goal_owner=player;remember_campaign(army,*p);
    for(Obj mate:partners)put16(mate,0x16,2999);
    put16(army,0x2A,195);p->region_target=48400;p->facility=0;p->credit=false;p->build_chain=false;p->staging_hex=48400;
    fixture_day=200;
    check(campaign_step(army,*p) && army[0x37]==4,"no field work does not invent an unreachable rally");
    fixture_day=220;
    check(campaign_step(army,*p) && army[0x37]==4 && word(army,0x3A)==9,
          "city fallback remains independent of a twenty-day deadline");
    p->motion_day=fixture_day;
    p->built_once=true;p->finished_build=true;p->return_deadline_day=219;calls=original_nodes;
    check(retreat_node(node,node,army)==1 && original_nodes==calls,
          "healthy army remains useful after construction protection expires");
    // Continuous construction, actual cap vs no money, ownership/threat gates.
    put16(army,0x2A,101);p->build_center=100;p->build_chain=true;p->cycle_mask=1u<<4;p->credit=false;
    legal_facilities=(1u<<4)|(1u<<7);put16(tile,0x1A,0);region_tiles.clear();
    check(try_build(army,*p) && p->facility==7,"completed fortress continues into music platform without another capture");
    p->facility=0;p->build_chain=true;put16(human,0x2A,104);
    check(try_build(army,*p) && p->build_chain,"nearby player presence alone cannot cancel a captured-base construction obligation");put16(human,0x2A,0);
    p->credit=true;p->build_chain=true;cap_reached=true;
    p->facility=0;army[0x46]=0;
    check(!try_build(army,*p) && !p->credit && !p->build_chain,"allied capacity completion releases the captured-base obligation");
    Obj native_builder=object(1);army_ids[native_builder]=6;native_builder[0x1A]=100;slot(0x7E838,6,native_builder);
    native_builder[0x150]=2;put16(native_builder,0x12,1);put16(native_builder,0x2A,101);
    Progress *np=observe_capture(native_builder);check(!np->facility,"idle native unit has no construction assignment");
    native_builder[0x37]=9;native_builder[0x38]=11;native_builder[0x46]=4;
    put16(native_builder,0x42,105);put16(native_builder,0x3A,99);
    Obj partial_fort=object(105);partial_fort[0x12]=4;partial_fort[0x13]=2;put16(partial_fort,0x14,400);slot(0x6DA98,99,partial_fort);
    put16(tile,0x1A,99);
    observe_capture(native_builder);
    check(np->facility==4 && np->built_once,"native construction that starts later is adopted and excluded from rally readiness");
    native_builder[0x37]=4;native_builder[0x38]=5;put16(native_builder,0x3A,9);put16(native_builder,0x2A,104);
    np->facility=0;np->build_chain=false;np->finished_build=true;
    observe_capture(native_builder);
    check(np->facility==4 && np->build_chain && !np->finished_build,
          "native city reset before observation cannot abandon a nearby assigned unfinished facility");
    np->facility=0;np->build_chain=false;put16(native_builder,0x2A,108);observe_capture(native_builder);
    check(!np->facility,"stale construction fields cannot reclaim a distant abandoned building");
    put16(native_builder,0x2A,104);partial_fort[0x13]=1;observe_capture(native_builder);
    check(!np->facility,"retained fields cannot resume a building now owned by the player");
    partial_fort[0x13]=2;partial_fort[0x12]=7;observe_capture(native_builder);
    check(!np->facility,"retained fields cannot adopt a replacement of another facility type");
    partial_fort[0x12]=4;put16(partial_fort,0x14,1000);observe_capture(native_builder);
    check(!np->facility,"a completed facility cannot restart work after native city reset");
    put16(partial_fort,0x14,0);observe_capture(native_builder);
    check(!np->facility,"destroyed construction cannot be reclaimed from stale fields");
    put16(partial_fort,0x14,400);put16(partial_fort,0x10,106);observe_capture(native_builder);
    check(!np->facility,"reused building ID at a different hex cannot be adopted");
    put16(tile,0x1A,0);observe_capture(native_builder);
    check(!np->facility,"missing facility cannot resume an abandoned city-order construction");
    slot(0x6DA98,99,nullptr);slot(0x7E838,6,nullptr);
    Obj wall=object(110);put16(wall,0x10,105);wall[0x12]=10;wall[0x13]=0;put16(wall,0x14,1000);slot(0x6DA98,110,wall);
    p->destroy_id=0;p->obstacle={};p->progress_day=fixture_day;
    cap_reached=true;
    check(!try_destroy(army,*p),"area cap alone never orders neutral demolition");
    mock_action(army,6);mock_target(army,11,110);
    check(cancel_unqualified_neutral_order(army,*p) && army[0x37]!=6,"unqualified neutral demolition already stored in an old save is cancelled");
    cap_reached=false;fixture_day+=100;
    check(!try_destroy(army,*p),"ten turns without oscillation never orders neutral demolition");
    p->campaign=true;p->returning=false;
    for(unsigned i=0;i<10;++i)obstacle_sample(p->obstacle,fixture_day+static_cast<int>(i)*10,101+i%2,200,99-static_cast<int>(i%2),p->progress_day);
    fixture_day+=90;
    check(!try_destroy(army,*p),"nine complete turns of oscillation do not qualify");
    fixture_day+=10;obstacle_sample(p->obstacle,fixture_day,101,200,99,p->progress_day);
    check(try_destroy(army,*p) && army[0x37]==6 && army[0x38]==11 && word(army,0x3A)==110,
          "ten turns of same-goal oscillation select a blocking neutral stonewall");
    check(try_destroy(army,*p) && p->destroy_id==110,"validated neutral demolition remains a native attack mission");
    check(!cancel_unqualified_neutral_order(army,*p),"qualified saved native attack survives the order cancellation guard");
    put16(army,0x2A,104);p->obstacle.count=0;
    check(try_destroy(army,*p),"approaching an already authorized obstacle does not erase its mission");
    put16(army,0x2A,101);
    p->returning=true;check(!try_destroy(army,*p) && !p->destroy_id && army[0x37]!=6,"withdrawal cancels obsolete neutral demolition order");p->returning=false;
    wall[0x13]=2;check(!destructible(army,wall),"own facility cannot be a demolition target");
    wall[0x13]=1;cap_reached=true;
    check(try_destroy(army,*p),"enemy-owned facility still eligible at native area cap");
    check(!cancel_unqualified_neutral_order(army,*p),"neutral order guard never cancels enemy-owned facility attacks");
    p->destroy_id=0;cap_reached=false;p->progress_day=fixture_day;
    fixture_day+=59;check(!try_destroy(army,*p),"legacy enemy facility timeout does not trigger early");
    fixture_day+=1;check(try_destroy(army,*p),"legacy enemy facility timeout unchanged at six turns");
    p->destroy_id=0;wall[0x13]=0;wall[0x12]=3;
    check(!destructible(army,wall),"neutral non-barrier facility is never targeted");wall[0x12]=16;
    check(destructible(army,wall),"neutral iron chain participates in obstacle demolition");
    p->obstacle={};p->campaign=false;put16(army,0x2A,110);put16(human,0x2A,100);
    Obj hit_pair=object(1),hit_source=object(1);putptr(hit_pair,0,hit_source);
    put16(hit_source,0,3);*reinterpret_cast<unsigned *>(hit_source+4)=27;
    put16(hit_pair+8,0,1);*reinterpret_cast<unsigned *>(hit_pair+12)=27;
    slot(0x77428,3,object(3));slot(0x77428,13,object(13));active_formation=table(0x77428,3,20);
    ranged_hits[1]={};apply_actual_damage=false;
    check(combat_apply_damage(nullptr,hit_pair,nullptr)==0x123456789ABCDEF0ull && !ranged_hits[1].present,
          "forecast or cancelled damage forwards native result without recording a hit");
    apply_actual_damage=true;
    check(combat_apply_damage(nullptr,hit_pair,nullptr)==0x123456789ABCDEF0ull && ranged_hits[1].present,
          "actual player active-arrow formation damage records defender identity");
    check(try_destroy(army,*p,true) && p->destroy_id==110,"actual ranged hit enables emergency iron chain demolition without waiting ten turns");
    fixture_day+=11;check(try_destroy(army,*p,true),"authorized firing barrier mission continues during its bounded travel window");
    put16(wall,0x10,115);p->destroy_id=0;
    check(!try_destroy(army,*p),"nearby wall beyond victim is not mistaken for a firing barrier");put16(wall,0x10,105);
    fixture_day+=11;check(!try_destroy(army,*p),"old ranged hit expires after one native turn");
    active_formation=table(0x77428,13,20);ranged_hits[1]={};combat_apply_damage(nullptr,hit_pair,nullptr);
    check(ranged_hits[1].present && try_destroy(army,*p),"actual active water tower ship hit qualifies for neutral chain demolition");
    ranged_hits[1]={};p->destroy_id=0;active_formation=object(1);human[0x1C]=13;
    combat_apply_damage(nullptr,hit_pair,nullptr);
    check(!ranged_hits[1].present,"equipped tower ship on land does not qualify when active formation differs");
    active_formation=table(0x77428,3,20);put16(hit_source,0,2);
    combat_apply_damage(nullptr,hit_pair,nullptr);check(!ranged_hits[1].present,"AI ranged attacker never creates player defensive-wall exception");
    put16(hit_source,0,3);combat_apply_damage(nullptr,hit_pair,nullptr);
    put16(army,0x12,99);check(!ranged_wall(army,wall),"reused defender slot cannot inherit another commander's hit");put16(army,0x12,1);
    p->campaign=true;p->obstacle={};
    for(unsigned i=0;i<11;++i)obstacle_sample(p->obstacle,fixture_day-100+static_cast<int>(i)*10,101+i%2,200,99-static_cast<int>(i%2),p->progress_day);
    ranged_hits[1]={};put16(army,0x2A,101);
    put16(wall,0x14,0);p->destroy_id=110;p->destroy_type=16;p->destroy_owner=0;p->destroy_hex=105;
    check(!try_destroy(army,*p) && !p->destroy_id && p->finished_demolition,
          "destroyed barrier releases mission without directly deleting a facility");
    put16(human,0x2A,0);human[0x1C]=0;
    cap_reached=false;slot(0x6DA98,110,nullptr);p->build_chain=false;p->credit=false;p->facility=0;
    // A group release cannot overwrite a just-selected logistics cut.
    put16(army,0x2A,195);put16(troops2,0x14,10);remember_campaign(army,*p);cut_available=true;
    check(try_cut(army),"player cut is feasible for assembly regression");remember_cut(army,*p);
    check(priority_mission(army,*p),"player cut marks army ineligible for group city assault");
    unsigned saved_goal=word(army,0x3A);mock_clear(army);mock_action(army,4);mock_target(army,5,9);
    check(restore_cut(army,*p) && word(army,0x2C)==104 && word(army,0x3A)==saved_goal,
          "later native order reset restores validated player logistics route in the same decision day");
    cut_available=false;for(Obj mate:partners)put16(mate,0x16,5000);
    auto mp=observe_capture(partners[0]);remember_campaign(partners[0],*mp);mp->staging_hex=word(partners[0],0x2A);
    p->staging_hex=195;
    check(campaign_step(partners[0],*mp) && (dword(army,0x1F0)&8) && word(army,0x2C)==104,
          "independent city fallback cannot overwrite another army logistics mission");
    // Combat-scoped adjacency: same terrain, different actual opponent.
    unsigned char pair[64]={},first[32]={};putptr(pair,0,first);put16(first,0,1);
    *reinterpret_cast<unsigned *>(first+4)=27;put16(pair+8,0,2);*reinterpret_cast<unsigned *>(pair+12)=27;
    combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency>1.119f && observed_adjacency<1.121f,"AI vs AI three full regions use 12 percent in actual pair scope");
    check(!combat_scope.fixed,"combat TLS restored after the native pair evaluation");
    int adj_score=0;
    check(contextual_adjacency(center,2,&adj_score)==1.72f,"screen and unscoped requests retain difficulty bonus");
    put16(pair+8,0,3);combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency==1.72f,"same AI unit facing actual player retains native 24 percent per region");
    put16(pair+8,0,1);combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency==1.72f,"same force or invalid combat pair never enables AI enemy normalization");
    put16(pair+8,0,2);original_adjacency=mock_partial_adjacency;combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency>1.097f && observed_adjacency<1.099f,"partial occupancy is preserved when replacing the regional rate");
    original_adjacency=mock_medium_adjacency;combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency>1.119f && observed_adjacency<1.121f,"AI pair ignores medium or large difficulty settings for its fixed rate");
    put16(pair+8,0,3);combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency==2.064f,"player battle keeps native whole-multiplier medium setting");
    Obj fort=object(100);fort[0x12]=4;fort[0x13]=3;put16(fort,0x14,1000);slot(0x6DA98,100,fort);
    put16(pair+8,0,100);*reinterpret_cast<unsigned *>(pair+12)=11;combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency>1.119f && observed_adjacency<1.121f,"actual AI facility opponent also resolves to its owner");
    fort[0x13]=1;combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency==2.064f,"actual player facility opponent keeps configured adjacency");
    original_adjacency=mock_adjacency;
    // Demo has a loaded context with controlling-force byte 0. It still runs
    // computer construction/campaign work, but has no player logistics target.
    Obj saved_player=player,context=pointer(root,0x871E8);
    check(!computer_army(human),"normal play continues to exclude the human force");
    player=object(0);context[0x3A]=0;
    check(observer_mode() && computer_army(army) && computer_army(human),
          "explicit zero controlling force recognizes all valid demo armies as computer");
    calls=native_cuts;cut_available=true;
    check(!try_cut(army) && native_cuts==calls,"demo never invokes player logistics search without a player");
    check(!restore_cut(army,*p) && !priority_mission(army,*p),"old player cut does not reserve a demo army");
    cut_available=false;cap_reached=false;legal_facilities=~0u;money=10000;
    put16(army,0x2A,101);army[0x37]=4;army[0x38]=5;army[0x46]=0;put16(army,0x42,48400);
    memset(&progress[1],0,sizeof(progress[1]));center[0x14]=3;
    p=observe_capture(army);center[0x14]=2;observe_capture(army);
    check(p->credit && try_build(army,*p) && p->facility==6,"demo region capture grants credit and starts construction");
    check(apply_priority(army,node,node) && army[0x37]==8,"demo priority callback preserves ongoing construction");
    put16(pair+8,0,2);*reinterpret_cast<unsigned *>(pair+12)=27;combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency>1.119f && observed_adjacency<1.121f,"demo AI battle uses four percent per full region");
    put16(pair+8,0,3);combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency>1.119f && observed_adjacency<1.121f,"formerly human force is AI when demo has no controlling player");
    check(!combat_scope.fixed && contextual_adjacency(center,2,&adj_score)==1.72f,
          "demo restores combat scope and keeps screen calculation native");
    context[0x3A]=52;
    check(!observer_mode() && !computer_army(army),"invalid controlling-force value is not mistaken for demo");
    combat_pair_scores(pair,nullptr,nullptr);
    check(observed_adjacency==1.72f,"unknown control context preserves native battle adjacency");
    putptr(root,0x871E8,nullptr);
    check(!observer_mode() && !computer_army(army),"missing game context cannot enable demo commands");
    putptr(root,0x871E8,context);context[0x3A]=1;player=saved_player;
    check(!computer_army(human) && computer_army(army),"returning from demo restores human order exclusion");
    // Late-loaded friendly region is a construction opportunity without
    // inventing a capture, and released groups must recheck regional work.
    Obj recovery=object(1);army_ids[recovery]=7;recovery[0x1A]=100;slot(0x7E838,7,recovery);
    recovery[0x150]=2;put16(recovery,0x12,1);put16(recovery,0x16,6000);
    put16(recovery,0x58,7);put16(recovery,0x2A,101);
    Obj recovery_troops=object(1);put16(recovery_troops,0x14,10);put16(recovery_troops,0x16,9);slot(0x7F8D8,7,recovery_troops);
    auto rp=observe_capture(recovery);remember_campaign(recovery,*rp);
    check(rp->campaign && !rp->credit,"late-loaded offensive unit has a goal without fictitious capture credit");
    prepare_local_build(recovery,*rp);
    check(rp->build_chain && !rp->credit,"friendly offensive region permits a fresh native construction review");
    {   // [combat] mission_v2 (2026-10-21): passing a friendly region away from the castle builds nothing,
        // and a native order on the troops' hostile goal city counts as on-mission.
        Progress saved=*rp;mod_features.mission_v2=true;
        rp->build_chain=false;rp->credit=false;rp->work_reviewed_center=48400;prepare_local_build(recovery,*rp);
        check(!rp->build_chain && !rp->credit,"mission_v2: a friendly region far from the siege castle grants no construction");
        unsigned char a37=recovery[0x37],a38=recovery[0x38];unsigned short a3a=word(recovery,0x3A);
        recovery[0x37]=4;recovery[0x38]=5;put16(recovery,0x3A,9);
        bool on=native_on_mission(recovery);put16(recovery,0x3A,8);bool off=native_on_mission(recovery);
        check(on==hostile_city(recovery,9) && !off,"mission_v2: only an attack on the troops' own goal city is on-mission");
        recovery[0x37]=a37;recovery[0x38]=a38;put16(recovery,0x3A,a3a);
        // Option B: a stationary army attacking a hostile city is besieging and is not sent home.
        Progress siege_state=*rp;rp->forced_return=false;rp->motion_day=fixture_day-60;
        recovery[0x37]=4;recovery[0x38]=5;put16(recovery,0x3A,9);
        if(hostile_city(recovery,9)) {campaign_individual_priority(recovery,*rp);
            check(!rp->forced_return && rp->motion_day==fixture_day,"mission_v2: a castle assault never triggers the forty-day stationary retreat");}
        recovery[0x37]=a37;recovery[0x38]=a38;put16(recovery,0x3A,a3a);*rp=siege_state;
        mod_features.mission_v2=false;*rp=saved;
        // ai_max_aggression: the behaviour-tree "AI setting stage == n" condition answers as stage 4 only.
        mod_features.ai_max_aggression=true;int stage[1]={4},low[1]={1};
        check(bt_condition(0x73,0,nullptr,nullptr,stage,1)==1 && bt_condition(0x73,0,nullptr,nullptr,low,1)==0,
            "ai_max_aggression: stage condition reads as 궁극(4)");
        mod_features.ai_max_aggression=false;
    }
    army[0x46]=0;army[0x37]=4;progress[1].facility=0;put16(army,0x42,48400);
    cap_reached=false;legal_facilities=1u<<4;money=10000;cut_available=false;
    check(try_build(recovery,*rp) && rp->facility==4,"already-owned region can begin an eligible fortress after loading");
    legal_facilities=(1u<<6)|(1u<<4)|(1u<<7)|(1u<<3);rp->facility=0;rp->build_chain=true;rp->cycle_mask=0;
    check(try_build(recovery,*rp) && rp->facility==6,"all four facilities available always select catapult before camp");
    legal_facilities=(1u<<4)|(1u<<7)|(1u<<3);rp->facility=0;rp->build_chain=true;
    check(try_build(recovery,*rp) && rp->facility==4,"unavailable catapult still selects fortress before music and camp");
    legal_facilities=(1u<<7)|(1u<<3);rp->facility=0;rp->build_chain=true;
    check(try_build(recovery,*rp) && rp->facility==7,"camp cannot precede an eligible music platform");
    pause_assembly(*rp);
    check(rp->staging_hex==48400,"construction keeps no artificial staging marker");
    rp->facility=0;rp->build_chain=false;rp->credit=false;recovery[0x46]=0;put16(recovery,0x42,48400);
    for(Obj mate:partners)put16(mate,0x2A,0);
    put16(recovery,0x2A,195);region1[0x14]=3;region_tiles={region1};
    progress[1].region_target=48400;
    check(campaign_step(recovery,*rp) && rp->region_target==110 && recovery[0x37]==1 && word(recovery,0x48)<48400,
          "native ring excluding its center still produces exact regional movement and next step");
    rp->region_target=48400;progress[1].region_target=110;
    check(next_region(recovery,*rp,200)==1,"sole ally-reserved center receives cooperative occupation instead of waiting");
    check(campaign_step(recovery,*rp) && recovery[0x37]==1 && rp->region_target==110,
          "reserved region causes active regional movement, never a rally hold");
    progress[1].region_target=48400;rp->region_target=48400;region_available=false;
    check(next_region(recovery,*rp,200)==2,"failed native region path is not mistaken for absence of work");
    region_available=true;region1[0x14]=0;
    check(next_region(recovery,*rp,200)==1 && rp->region_owner==0,"neutral regional centers are also selected before siege");
    put16(recovery,0x2A,110);region1[0x14]=2;observe_capture(recovery);
    check(rp->credit,"captured neutral mission grants construction review credit");
    rp->credit=false;rp->build_chain=false;rp->region_target=48400;put16(recovery,0x2A,195);
    region1[0x14]=3;region_available=false;
    Obj home=object(200);put16(home,0x12,4);tile_areas[home]=city_area;home[0x14]=2;slot(0xE270,200,home);
    fixture_day+=20;field_alternative=true;
    check(campaign_step(recovery,*rp,node,node) && !rp->returning && recovery[0x37]==1 && word(recovery,0x3A)==150,
          "blocked original route reconsiders native field work instead of indefinitely holding");
    field_alternative=false;
    region_available=true;region_tiles.clear();rp->returning=false;rp->region_blocked_day=-1;
    recovery[0x37]=13;recovery[0x38]=255;put16(recovery,0x3A,0);mock_clear(recovery);
    put16(recovery,0x16,999);
    check(recover_idle(recovery,*rp) && rp->returning && recovery[0x37]==11 && word(recovery,0x3A)==9,
          "goal-less depleted army immediately receives a friendly return HEX");
    recovery[0x37]=4;recovery[0x38]=5;put16(recovery,0x3A,9);
    check(recover_idle(recovery,*rp) && recovery[0x37]==11 && word(recovery,0x3A)==9,
          "later native city order cannot erase a validated recovery return");
    {   // A gate home uses the original gate-retreat form (action 12, kind 6, gate number) (2026-10-22).
        unsigned short saved_base=word(origin,0x4E);put16(origin,0x4E,54);rp->returning=false;
        recovery[0x37]=13;recovery[0x38]=255;put16(recovery,0x3A,0);mock_clear(recovery);
        check(recover_idle(recovery,*rp) && recovery[0x37]==12 && recovery[0x38]==6 && word(recovery,0x3A)==3,
              "return to a gate home writes action 12 / gate kind 6 / gate number");
        put16(origin,0x4E,saved_base);rp->returning=false;
        recovery[0x37]=13;recovery[0x38]=255;put16(recovery,0x3A,0);mock_clear(recovery);
        check(recover_idle(recovery,*rp) && recovery[0x37]==11 && word(recovery,0x3A)==9,"city home keeps the city return form");
    }
    home[0x14]=3;
    check(!recover_idle(recovery,*rp),"a captured origin cannot remain a return destination");
    home[0x14]=2;rp->returning=false;put16(recovery,0x16,6000);
    recovery[0x37]=13;recovery[0x38]=255;put16(recovery,0x3A,0);mock_clear(recovery);
    rp->motion_day=fixture_day;
    check(!recover_idle(recovery,*rp),"fresh stationary army is not immediately retargeted");
    put16(partners[0],0x16,5000);put16(partners[0],0x2A,194);partners[0][0x37]=4;partners[0][0x38]=5;
    fixture_day+=10;
    check(!recover_idle(recovery,*rp) && rp->campaign && rp->city==9 && word(recovery_troops,0x16)==9,
          "one stationary turn joins a nearby healthy ally's offensive city goal");
    check(recovery[0x37]!=4,"joining a strategic goal does not issue an immediate city assault");
    recovery[0x38]=9;put16(recovery,0x3A,195);rp->motion_day=fixture_day-100;
    check(!recover_idle(recovery,*rp),"valid HEX command does not trigger idle recovery timers");
    recovery[0x38]=255;put16(recovery,0x3A,0);put16(recovery,0x2C,196);
    check(!recover_idle(recovery,*rp),"a valid movement waypoint is not treated as idle");
    mock_clear(recovery);put16(partners[0],0x16,2999);put16(partners[1],0x16,2999);
    put16(army,0x16,2999);rp->motion_day=fixture_day-20;
    check(!recover_idle(recovery,*rp) && !rp->returning,"healthy army is never sent home just because two idle turns elapsed");
    put16(recovery,0x2A,200);
    check(!recover_idle(recovery,*rp) && !rp->returning,"reaching home releases recovery to the native controller");
    put16(recovery,0x2A,195);home[0x14]=3;recovery[0x38]=255;put16(recovery,0x3A,0);
    check(!recover_idle(recovery,*rp),"no safe origin leaves native handling in control");
    home[0x14]=2;put16(partners[0],0x16,5000);put16(partners[0],0x58,7);
    partners[0][0x37]=4;partners[0][0x38]=5;put16(recovery_troops,0x16,7);
    rp->returning=false;rp->motion_day=fixture_day-20;
    check(!recover_idle(recovery,*rp) && word(recovery_troops,0x16)==7,
          "active shared troop member blocks retargeting and keeps its strategic goal unchanged");
    put16(partners[0],0x58,3);
    home[0x14]=2;slot(0x7E838,7,nullptr);
    // Regression: old CC560(center,1) misses the center. Also reproduce the
    // user's saved action 13 / HEX / empty route without an elapsed timer.
    for(Obj mate:partners)put16(mate,0x2A,0);
    region1[0x14]=3;region2[0x14]=2;region_tiles={region1};
    Obj active=object(1);army_ids[active]=8;active[0x1A]=100;slot(0x7E838,8,active);active[0x150]=2;
    put16(active,0x12,1);put16(active,0x16,8000);put16(active,0x58,8);put16(active,0x2A,195);
    Obj active_troops=object(1);put16(active_troops,0x14,10);put16(active_troops,0x16,9);slot(0x7F8D8,8,active_troops);
    active[0x37]=13;active[0x38]=9;put16(active,0x3A,195);mock_clear(active);
    auto ap=observe_capture(active);remember_campaign(active,*ap);
    check(!mock_region(nullptr,active,region1,1,0,1),"faithful native ring cannot choose its excluded anchor center");
    progress[1].region_target=48400;cap_reached=false;cut_available=false;money=10000;legal_facilities=~0u;
    bool active_changed=apply_priority(active,node,node);
    check(active_changed && active[0x37]==1 && word(active,0x3A)==110 && word(active,0x48)<48400,
          "saved eight-thousand-soldier HEX hold immediately becomes active region movement without waiting");
    put16(active,0x2A,110);region1[0x14]=2;observe_capture(active);
    check(ap->credit && word(active,0x2A)==110,"regional movement reaches its center and records observed ownership change");
    check(apply_priority(active,node,node) && active[0x37]==8 && ap->facility==6,
          "occupation leads into eligible construction before any city assault");
    ap->facility=0;ap->credit=false;ap->build_chain=false;active[0x46]=0;put16(active,0x42,48400);
    region2[0x14]=3;region_tiles={region1,region2};
    check(campaign_step(active,*ap) && ap->region_target==120 && active[0x37]==1,
          "completed regional work continues to another center without a rally or return");
    ap->region_target=48400;region1[0x14]=3;region_tiles={region1};
    put16(active,0x2A,101);
    unsigned char prior_order[0x34];memcpy(prior_order,active+0x2C,sizeof(prior_order));
    wrong_waypoint=true;
    check(!assign_region(active,*ap,110) && !memcmp(prior_order,active+0x2C,sizeof(prior_order)),
          "a successful planner with a different waypoint is rejected and original orders restored");
    wrong_waypoint=false;path_available=false;
    check(!assign_region(active,*ap,110) && !memcmp(prior_order,active+0x2C,sizeof(prior_order)),
          "missing native next step cannot be reported as an active region mission");
    path_available=true;region_tiles.clear();slot(0x7E838,8,nullptr);
    campaign_revision_tests();
    campaign_support_tests();
    campaign_operation_tests();
    // Wall prohibition identifies the actor, not the tile owner. Human walls
    // and every non-wall facility retain the exact native permission result.
    Obj ai_force=table(0xDF30,2,51),earth=object(9),stone=object(10);
    slot(0x73958,9,earth);slot(0x73958,10,stone);slot(0xDF30,1,player);
    original_build_permission=mock_build_permission;enabled=1;
    calls=build_permission_calls;
    check(!build_permission(ai_force,center,earth,7) && build_permission_calls==calls,
          "AI earthen wall denied before native candidates or construction validation");
    check(!build_permission(ai_force,center,stone,7) && build_permission_calls==calls,
          "AI stone wall denied without calling original eligibility");
    check(build_permission(player,center,earth,7) && build_permission_calls==calls+1,
          "player earthen wall eligibility is forwarded");
    check(permission_force==player && permission_tile==center && permission_definition==earth && permission_policy==7,
          "human construction forwards force tile definition and fourth argument exactly");
    check(!build_permission(player,center,stone,0),"human illegal stone wall remains illegal under native rules");
    check(build_permission(player,center,stone,7),"player legal stone wall remains buildable");
    for(unsigned facility:{6u,4u,7u,3u,11u,12u,14u,13u})
        check(build_permission(ai_force,center,table(0x73958,facility,30),7),"AI non-wall native eligibility preserved");
    center[0x14]=1;
    check(!build_permission(ai_force,center,earth,7),"AI wall stays blocked on a player-owned tile");
    center[0x14]=2;
    check(build_permission(player,center,earth,7),"human wall forwarding does not depend on tile ownership");
    Obj unregistered_force=object(51);
    check(build_permission(unregistered_force,center,earth,7),"unknown force cannot be mistaken for AI");
    Obj wall_context=pointer(root,0x871E8);wall_context[0x3A]=0;Obj normal_player=player;player=object(0);
    check(!build_permission(ai_force,center,earth,7) && !build_permission(normal_player,center,stone,7),
          "explicit observer mode blocks walls for every actual AI force");
    wall_context[0x3A]=52;
    check(build_permission(ai_force,center,earth,7),"invalid control context preserves native wall eligibility");
    putptr(root,0x871E8,nullptr);
    check(build_permission(ai_force,center,stone,7),"missing context does not block construction on guessed control state");
    putptr(root,0x871E8,wall_context);wall_context[0x3A]=1;player=normal_player;enabled=0;
    check(build_permission(ai_force,center,earth,7),"disabled policy safely forwards native wall eligibility");enabled=1;
    Obj wall_builder=object(1);army_ids[wall_builder]=9;wall_builder[0x1A]=100;slot(0x7E838,9,wall_builder);
    wall_builder[0x150]=2;put16(wall_builder,0x12,1);put16(wall_builder,0x16,5000);
    put16(wall_builder,0x58,2);put16(wall_builder,0x2A,101);
    wall_builder[0x37]=8;wall_builder[0x38]=9;wall_builder[0x46]=9;put16(wall_builder,0x42,105);
    auto wp=observe_capture(wall_builder);wp->campaign=true;wp->city=9;
    check(cancel_wall_order(wall_builder,*wp) && !wp->facility && wall_builder[0x46]==0 &&
          wall_builder[0x37]==13 && wp->campaign && wp->city==9,
          "existing AI wall travel order cancelled without discarding its offensive city");
    wall_builder[0x37]=9;wall_builder[0x46]=10;put16(wall_builder,0x42,105);
    check(cancel_wall_order(wall_builder,*wp),"existing AI stone wall construction order cancelled");
    wall_builder[0x150]=1;wall_builder[0x37]=8;wall_builder[0x46]=9;
    check(!cancel_wall_order(wall_builder,*wp) && wall_builder[0x37]==8 && wall_builder[0x46]==9,
          "player wall order is never cancelled");
    wall_builder[0x150]=2;wall_builder[0x46]=4;
    check(!cancel_wall_order(wall_builder,*wp),"AI fortress order is never cancelled");
    wall_builder[0x37]=4;wall_builder[0x46]=9;
    check(!cancel_wall_order(wall_builder,*wp),"stale wall field cannot cancel a city attack");
    check(!set_build(wall_builder,9,105) && !set_build(wall_builder,10,105),
          "mod direct build setter independently rejects AI walls");
    Obj neutral_wall=object(1);neutral_wall[0x12]=9;neutral_wall[0x13]=0;put16(neutral_wall,0x14,1000);put16(neutral_wall,0x10,105);
    check(destructible(wall_builder,neutral_wall),"neutral earthen wall destruction remains available");
    neutral_wall[0x12]=10;check(destructible(wall_builder,neutral_wall),"neutral stone wall destruction remains available");
    neutral_wall[0x13]=1;check(destructible(wall_builder,neutral_wall),"hostile player wall destruction remains available");
    slot(0x7E838,9,nullptr);
    unsigned allocated=next_handle;
    check(frees==allocated,"all temporary native lists released");
    obstacle_policy_tests();
    domestic_tests();
    recheck_tests();
    corridor_tests();
#ifdef SORTIE_FORMATION
    sortie_tests();
    district_intercept_tests();
#endif
    enabled=0;
    memcpy(image+0xD2486,kCutCallBytes,5);
    memcpy(image+0x1749A0,kDamageEntryBytes,5);
    memcpy(image+0x169F44,kPairCallBytes,5);memcpy(image+0x2B07C0,kAdjacencyEntryBytes,5);
    memcpy(image+0x279190,kBuildPermissionEntryBytes,5);
    memcpy(image+0x295160,kDomesticTickEntryBytes,5);
    memcpy(image+0x36423,kDomesticBudgetBranchBytes,6);
    memcpy(image+0x1ED475,kDomesticCostLoadBytes,6);memcpy(image+0x302980,population_entry,16);
    putptr(image,0x1344518,image+0x201E0); putptr(image,0x1343C98,image+0x1F3A0); putptr(image,0x134B4F8,image+0x68CE0);
    putptr(image,0x134AF00,image+0x61120);
    putptr(image,0x1346E70,image+0x36320);putptr(image,0x134C8F8,image+0x7A110);
    image[0x1749A0]=0xCC;
    check(!install() && !enabled && image[0xD2486]==0xE8,"unexpected damage call rejects installation before publishing any hooks");
    memcpy(image+0x1749A0,kDamageEntryBytes,5);
    image[0x2B07C0]=0xCC;
    check(!install() && !enabled && image[0xD2486]==0xE8 && pointer(image,0x1344518)==image+0x201E0,
          "unexpected adjacency entry rejects the complete installation without changing other links");
    memcpy(image+0x2B07C0,kAdjacencyEntryBytes,5);
    image[0x279190]=0xCC;
    check(!install() && !enabled && pointer(image,0x1344518)==image+0x201E0,
          "unexpected build permission entry rejects installation before any patch is published");
    memcpy(image+0x279190,kBuildPermissionEntryBytes,5);
    image[0x295160]=0xCC;
    check(!install() && !enabled && pointer(image,0x1346E70)==image+0x36320,"unexpected domestic tick signature rejects all new hooks");
    memcpy(image+0x295160,kDomesticTickEntryBytes,5);
    image[0x302980]=0xCC;
    check(!install() && !enabled && pointer(image,0x1344518)==image+0x201E0,
          "changed monthly population entry rejects installation before publishing any hooks");
    memcpy(image+0x302980,population_entry,16);
    const unsigned char population_tail[]={0x48,0x8B,0xC1,0x5F,0x5E,0x5B,0x5D,0xC3};
    DWORD population_old=0;
    check(VirtualProtect(image+0x302000,0x1000,PAGE_EXECUTE_READWRITE,&population_old)!=0,
          "synthetic population continuation page executable");
    // Original prologue writes saved RBP into the caller's shadow space.
    // The synthetic tail tests the fourth integer argument and restores RBP.
    unsigned char build_tail[]={0x41,0x83,0xF9,7,0x0F,0x94,0xC0,0x48,0x8B,0x6C,0x24,0x10,0xC3};
    memcpy(image+0x279195,build_tail,sizeof(build_tail));
    // The exact stolen store saves the third argument in the caller's shadow
    // space; synthetic native tail returns preview+11 and writes the output.
    unsigned char domestic_tail[]={0x41,0xC7,0,0x7B,0,0,0,0x8D,0x42,0x0B,0xC3};
    memcpy(image+0x295165,domestic_tail,sizeof(domestic_tail));
    unsigned char damage_tail[]={0x41,0xC7,0,0x7B,0,0,0,0x48,0xB8,0xF0,0xDE,0xBC,0x9A,0x78,0x56,0x34,0x12,0x41,0x5E,0x5F,0x5D,0xC3};
    memcpy(image+0x1749A5,damage_tail,sizeof(damage_tail));
    DWORD damage_old=0;
    check(VirtualProtect(image+0x174000,0x1000,PAGE_EXECUTE_READWRITE,&damage_old)!=0,"synthetic damage page executable");
    DWORD domestic_old=0;
    check(VirtualProtect(image+0x295000,0x1000,PAGE_EXECUTE_READWRITE,&domestic_old)!=0,"synthetic domestic page executable");
    DWORD build_old=0;
    check(VirtualProtect(image+0x279000,0x1000,PAGE_EXECUTE_READWRITE,&build_old)!=0,
          "synthetic native build permission page made executable for ABI test");
    // Execute a synthetic native tail with the exact stolen PUSH prologue.
    // This checks actual trampoline/argument/stack behavior, not just opcodes.
    unsigned char tail[]={0x41,0xC7,0,0x2C,1,0,0,0xB8,0,0,0,0,0x66,0x0F,0x6E,0xC0,0x41,0x5F,0x5D,0x5B,0xC3};
    float native_value=1.72f;memcpy(tail+8,&native_value,4);memcpy(image+0x2B07C5,tail,sizeof(tail));
    DWORD executable_old=0;check(VirtualProtect(image+0x2B0000,0x10000,PAGE_EXECUTE_READWRITE,&executable_old)!=0,
                              "synthetic native function page made executable for ABI test");
    check(install() && enabled,"atomic code and vtable installation on fixture image");
    bool population_flushed=false;
    for(const auto &call:fixture_flushes) {
        uintptr_t begin=reinterpret_cast<uintptr_t>(call.address);
        uintptr_t end=begin+call.size;
        uintptr_t trampoline=reinterpret_cast<uintptr_t>(original_population_growth);
        if(begin<=trampoline-32 && end>=trampoline+19)population_flushed=true;
    }
    check(population_flushed,"production install flushes both monthly population relay and full trampoline before publication");
    memcpy(image+0x302985,population_tail,sizeof(population_tail));
    FlushInstructionCache(GetCurrentProcess(),image+0x302980,32);
    enabled=0;
    check(original_population_growth(origin)==reinterpret_cast<uint64_t>(origin) &&
          reinterpret_cast<PopulationGrowth>(image+0x302980)(origin)==reinterpret_cast<uint64_t>(origin),
          "monthly growth trampoline and installed entry preserve argument, 64 bit return and all four pushed registers");
    enabled=1;
    int damage_output=0;
    check(original_damage(nullptr,nullptr,reinterpret_cast<Obj>(&damage_output))==0x123456789ABCDEF0ull && damage_output==123,
          "damage trampoline preserves third argument pointer, 64-bit return and nonvolatile PUSH registers");
    original_damage=mock_damage; // wrapper relay forwarding uses the private native mock
    intptr_t damage_delta=*reinterpret_cast<int32_t *>(image+0x1749A1);
    auto installed_damage=reinterpret_cast<uint64_t(__fastcall *)(Obj,Obj,Obj)>(image+0x1749A5+damage_delta);
    check(installed_damage(nullptr,nullptr,nullptr)==0x123456789ABCDEF0ull,"installed damage relay preserves three arguments and 64-bit native return");
    check(pointer(image,0x1344518)==reinterpret_cast<Obj>(&first_army_node),"first node hook installed");
    check(pointer(image,0x134AF00)==reinterpret_cast<Obj>(&retreat_node),"construction return hook installed");
    check(image[0x169F44]==0xE8 && image[0x2B07C0]==0xE9,"combat scope and adjacency trampoline links installed atomically");
    check(image[0x279190]==0xE9,"central native construction permission hook installed");
    check(pointer(image,0x1346E70)==reinterpret_cast<Obj>(&domestic_officer_node) &&
          pointer(image,0x134C8F8)==reinterpret_cast<Obj>(&domestic_selection_node),"automatic officer and selection vtable hooks installed");
    check(image[0x295160]==0xE9 && image[0x1ED475]==0xE8 && image[0x1ED47A]==0x90 &&
          memcmp(image+0x36423,"\x90\x90\x90\x90\x90\x90",6)==0,"complete domestic code links installed");
    int domestic_output=0;
    check(original_domestic_tick(nullptr,7,&domestic_output)==18 && domestic_output==123,
          "domestic tick trampoline preserves integer argument output pointer and shadow space");
    domestic_output=0;
    check(reinterpret_cast<DomesticTick>(image+0x295160)(nullptr,7,&domestic_output)==18 && domestic_output==123,
          "installed domestic entry forwards preview without synchronizing or corrupting arguments");
    check(original_build_permission(player,center,earth,7) && !original_build_permission(player,center,earth,0),
          "native build trampoline preserves RBP shadow space fourth argument and boolean return");
    auto installed_permission=reinterpret_cast<BuildPermission>(image+0x279190);
    check(!installed_permission(ai_force,center,earth,7) && !installed_permission(ai_force,center,stone,7),
          "actual installed native entry refuses both AI wall definitions");
    check(installed_permission(player,center,earth,7) && !installed_permission(player,center,stone,0),
          "actual installed native entry preserves human eligibility and native rejection");
    check(installed_permission(ai_force,center,table(0x73958,6,30),7),
          "actual installed native entry preserves AI catapult eligibility");
    adj_score=0;check(original_adjacency(center,2,&adj_score)==1.72f && adj_score==300,
                      "native trampoline preserves original return value, output pointer and stack");
    combat_scope={2,3,true};
    float hooked=reinterpret_cast<Adjacency>(image+0x2B07C0)(center,2,&adj_score);
    check(hooked>1.119f && hooked<1.121f,"actual installed entry jump calls contextual modifier and native trampoline");
    combat_scope={};
#ifdef SORTIE_FORMATION
    sortie_abi_tests();
#endif
    check(!install(),"installation refuses duplicate or unexpected bytes");
#ifdef SORTIE_FORMATION
    printf("{\"status\":\"sortie_fixture_passed\",\"checks\":%u,\"failures\":%u,\"gameplay_validated\":false}\n",checks,failures);
#else
    printf("{\"status\":\"native_adapter_fixture_passed\",\"checks\":%u,\"failures\":%u,\"gameplay_validated\":false}\n",checks,failures);
#endif
    return failures?1:0;
}

