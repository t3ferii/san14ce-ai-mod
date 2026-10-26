// Exercise the production callbacks with native UI helpers substituted.
#include <string>
static unsigned expert_test_force=2,expert_test_score=600,expert_test_trait;
static float expert_test_result;
static float expert_test_native_base=177.0f;
static std::vector<unsigned> expert_test_ids;
static Obj expert_test_row,expert_test_layout;
static unsigned expert_test_native_events,expert_test_populates,expert_test_popup_key;
static float expert_test_scroll_position;
static unsigned expert_test_difficulty;
static std::map<Obj,std::wstring> expert_test_text;
static std::vector<Obj> expert_test_buttons;
static bool expert_test_initialized;
static std::vector<unsigned> expert_test_registered_nodes;
static unsigned __fastcall expert_test_get(Obj options,unsigned key,unsigned preset) {
    if(key==43)return expert_test_difficulty;
    return key==4 || key==5?dword(expert_option(options,key),preset*4):0;
}
static float __fastcall expert_test_adjacency(Obj,int force,int *score) {
    *score=static_cast<int>(expert_test_score);
    unsigned key=static_cast<unsigned>(force)==player_force()?4:5;
    unsigned scale=expert_native_adjacency_option(image+0x1A46CA8,key,0);
    return 1.0f+static_cast<float>(expert_test_score*(4+expert_test_trait))*0.0001f*static_cast<float>(scale)*0.01f;
}
static void __fastcall expert_test_pair(Obj,Obj,Obj) {
    int score=0;expert_test_result=contextual_adjacency(table(0xE270,100,48399),static_cast<int>(expert_test_force),&score);
}
static float __fastcall expert_test_power(Obj army,Obj,int,int) {
    int score=0;
    unsigned force=valid(army)?army_force(army):expert_test_force;
    return expert_test_native_base*contextual_adjacency(table(0xE270,100,48399),static_cast<int>(force),&score);
}
static Obj __fastcall expert_test_field(Obj context,Obj entries) {
    expert_test_result=expert_test_power(pointer(context,0x98),pointer(context,0xA0),1,1);
    return entries;
}
static void __fastcall expert_test_model(Obj,GameList *list) {
    expert_test_ids.clear();
    for(Link *p=list_first(*list);p;p=p->next)expert_test_ids.push_back(dword(p->value,0));
}
static void __fastcall expert_test_scroll(Obj,float position,int) {expert_test_scroll_position=position;}
static void __fastcall expert_test_population(Obj,Obj,Obj,Obj) {++expert_test_populates;}
static void __fastcall expert_test_event(Obj,Obj *) {++expert_test_native_events;}
static void __fastcall expert_test_number(Obj,Obj *,Obj,Obj) {++expert_test_native_events;}
static Obj __fastcall expert_test_source(Obj,Obj item) {return item;}
static Obj __fastcall expert_test_row_at(Obj,unsigned) {return expert_test_row;}
static void __fastcall expert_test_marker(Obj control,int value) {*reinterpret_cast<int *>(control+0x10)=value;}
static void __fastcall expert_test_bind(Obj row,Obj record,Obj,int,int) {
    memcpy(row+0x220,record,0x60);
    // Even a native control that emits while binding cannot overwrite settings.
    unsigned char callback[16]={};putptr(callback,8,expert_test_layout);
    *reinterpret_cast<unsigned *>(row+0x88)=dword(record,0);
    *reinterpret_cast<unsigned *>(row+0x80)=199;
    Obj event=row;expert_slider_event(callback,&event);
}
static void __fastcall expert_test_popup(Obj layout,Obj event,Obj,Obj) {
    expert_test_popup_key=dword(event,0x88);
    *reinterpret_cast<unsigned *>(layout+0x1A4)=expert_test_popup_key;
    *reinterpret_cast<int *>(layout+0x1A8)=expert_number_current(pointer(layout,0x8C0),expert_test_popup_key);
}
static Obj __fastcall expert_test_destroy(Obj layout,unsigned) {return layout;}
static Obj __fastcall expert_test_button_factory(Obj table,int id,int style,int kind,int visible,int flag) {
    if(id==7) {
        check(style==3 && kind==0 && visible==1 && flag==0,
              "2026-10-24: inline factory forwards all original number-button arguments");
        expert_test_initialized=true;
        return pointer(pointer(pointer(table,0x18),8),0x198);
    }
    check(expert_test_initialized && id==48 && dword(table,0x10)==53,
          "2026-10-25: the cycle button follows the original numeric control factory (v6 node48)");
    check(style==3 && kind==0 && visible==1 && flag==0,
          "2026-10-25: the cycle button is a plain kind 0 button with the standard style 3 (style 5 crashed on click)");
    expert_test_registered_nodes.push_back(static_cast<unsigned>(id));
    Obj object=static_cast<Obj>(calloc(1,0x198));expert_test_buttons.push_back(object);
    Obj parent=table-0x130;
    putptr(object,8,parent);putptr(object,0xA0,object);
    *reinterpret_cast<int *>(object+0x20)=969;
    *reinterpret_cast<int *>(object+0x28)=112;*reinterpret_cast<int *>(object+0x2C)=48;return object;
}
static void __fastcall expert_test_leave(Obj,Obj) {}
static void __fastcall expert_test_callback(Obj,int,Obj) {}
static void __fastcall expert_test_text_set(Obj object,const wchar_t *text) {expert_test_text[object]=text;}
static void __fastcall expert_test_enable(Obj object,int value) {*reinterpret_cast<int *>(object+0x104)=value;}
static unsigned expert_test_hides;
static void __fastcall expert_test_visible(Obj object,int value) {*reinterpret_cast<int *>(object+0x108)=value;if(!value)++expert_test_hides;}
static unsigned expert_test_radio_calls;
static void __fastcall expert_test_select(Obj,int) {++expert_test_radio_calls;}
static bool expert_near(float a,float b) {float delta=a-b;return delta>-0.00001f && delta<0.00001f;}
static void expert_settings_tests() {
    for(unsigned id:{4u,5u,61u,62u}) {
        unsigned lines=1;
        for(const wchar_t *text=expert_tooltip(id);*text;++text)if(*text==L'\n')++lines;
        check(lines==3,"expert help stays within the native three-line panel without scrolling");
    }
    auto saved_functions=functions;
    Adjacency saved_adjacency=original_adjacency;
    ExpertPopulate saved_population=original_expert_populate;
    ExpertUnitPower saved_power=original_expert_unit_power;
    unsigned saved_human=dword(expert_option(image+0x1A46CA8,4),0);
    unsigned saved_ai=dword(expert_option(image+0x1A46CA8,5),0),saved_mode=dword(image,0x1A47AF8);
    bool roundtrip=true;
    for(unsigned base=50;base<=200;++base)for(unsigned rate=0;rate<=200;++rate) {
        unsigned packed=expert_combat::encode({base,rate});auto decoded=expert_combat::decode(packed);
        roundtrip=roundtrip && expert_combat::encoded(packed) && decoded.base==base && decoded.region==rate;
    }
    check(roundtrip,"all 30,351 base/rate combinations survive the native 32-bit setting representation");
    bool decimals=true;
    for(unsigned base=50;base<=200;++base)for(unsigned rate=0;rate<=2000;++rate) {
        auto packed=expert_combat::encode({base,rate/10,rate%10});auto decoded=expert_combat::decode(packed);
        decimals=decimals && expert_combat::encoded(packed) && decoded.base==base &&
            decoded.region*10+decoded.tenth==rate;
    }
    check(decimals,"2026-10-23 user presets: all 302,151 tenth-percent values roundtrip exactly");
    check(!expert_combat::encoded(expert_combat::decimal_tag|(49u<<11)|392) &&
          !expert_combat::encoded(expert_combat::decimal_tag|(100u<<11)|2001) &&
          expert_combat::tagged(expert_combat::decimal_tag|(100u<<11)|392),
          "decimal tags reject malformed values and are normalized before native multiplication");
    check(expert_near(expert_combat::multiplier({100,39,2},600),3.352f) &&
          expert_near(expert_combat::multiplier({100,62,5},600),4.75f),"39.2 and 62.5 percent retain their fractional combat contribution");
    check(!expert_combat::encoded(expert_combat::tag|(49u<<8)|4) &&
          !expert_combat::encoded(expert_combat::tag|(100u<<8)|201),"malformed packed values are not accepted");
    check(expert_combat::decode(100).region==4 && expert_combat::decode(200).region==8 &&
          expert_combat::decode(50).region==2,"legacy save multiplier values have an explicit UI migration");
    BIND(0x3EA400,expert_test_get);BIND(0x16A030,expert_test_pair);
    original_adjacency=expert_test_adjacency;
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,4))=expert_combat::encode({50,0});
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,5))=expert_combat::encode({200,8});
    *reinterpret_cast<unsigned *>(image+0x1A47AF8)=0;
    unsigned char pair[64]={},first[32]={};putptr(pair,0,first);put16(first,0,1);
    *reinterpret_cast<unsigned *>(first+4)=27;put16(pair+8,0,3);*reinterpret_cast<unsigned *>(pair+12)=27;
    expert_test_force=2;expert_test_score=600;expert_test_trait=0;
    combat_pair_scores(pair,nullptr,nullptr);
    check(expert_near(expert_test_result,2.48f),"AI unit against player uses 200/100 + 8/100 * six regions");
    expert_test_force=1;combat_pair_scores(pair,nullptr,nullptr);
    check(expert_near(expert_test_result,0.5f),"player base 50 and regional rate zero apply independently");
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,4))=expert_combat::encode({100,8});
    combat_pair_scores(pair,nullptr,nullptr);
    check(expert_near(expert_test_result,1.48f),"1000 base with six 8-percent regions produces 1480");
    expert_test_score=245;combat_pair_scores(pair,nullptr,nullptr);
    check(expert_near(expert_test_result,1.196f),"partial occupation fractions are preserved");
    expert_test_score=600;expert_test_trait=2;combat_pair_scores(pair,nullptr,nullptr);
    check(expert_near(expert_test_result,1.6f),"native force trait addition survives changing the regional base rate");
    expert_test_trait=0;expert_test_force=2;put16(pair+8,0,2);combat_pair_scores(pair,nullptr,nullptr);
    check(expert_near(expert_test_result,1.24f),"AI versus AI keeps the existing fixed four percent regardless of custom values");
    int score=0;check(expert_near(contextual_adjacency(table(0xE270,100,48399),2,&score),1.24f),
        "unscoped estimates never apply the player-battle custom base or rate");
    put16(pair+8,0,3);*reinterpret_cast<unsigned *>(image+0x1A47AF8)=1;combat_pair_scores(pair,nullptr,nullptr);
    check(expert_near(expert_test_result,1.24f),"normal scenario mode cannot activate the expert custom settings");
    *reinterpret_cast<unsigned *>(image+0x1A47AF8)=0;
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,5))=150;combat_pair_scores(pair,nullptr,nullptr);
    check(expert_near(expert_test_result,1.36f),"legacy save retains the exact native multiplier before any edit");
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,5))=expert_combat::tag|(49u<<8)|4;
    combat_pair_scores(pair,nullptr,nullptr);
    check(expert_near(expert_test_result,1.24f),"malformed mod-tagged settings cannot become a gigantic native multiplier");
    check(!combat_scope.player_battle && !combat_scope.fixed,"new player combat scope restores TLS after a battle");
    original_expert_unit_power=expert_test_power;BIND(0x16B880,expert_test_field);
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,4))=expert_combat::encode({50,200});
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,5))=expert_combat::encode({200,8});
    Obj human_army=table(0x7E838,3,500),ai_army=table(0x7E838,1,500),other_ai=table(0x7E838,2,500);
    expert_test_score=400;expert_test_native_base=177.0f;
    float four=expert_unit_power(human_army,nullptr,1,1);
    expert_test_score=100;float one=expert_unit_power(human_army,nullptr,1,1);
    check(expert_near(four,1504.5f) && expert_near(one,442.5f),
        "unit display uses its native base 177 with base50/rate200: four regions8.5 versus one region2.5");
    expert_test_native_base=403.0f;
    check(expert_near(expert_unit_power(human_army,nullptr,1,1),1007.5f),
        "different native officer/formation stats remain different; no fixed 1000 or common unit base");
    expert_test_force=2;expert_test_native_base=100.0f;expert_test_score=600;
    check(expert_near(expert_unit_power(nullptr,goal,1,1),124.0f),
        "city-only power queries retain native regional multipliers instead of receiving a unit base setting");
    unsigned char field[0xB8]={};putptr(field,0x98,ai_army);putptr(field,0xA8,other_ai);
    check(expert_field_stats(field,field)==field && expert_near(expert_test_result,124.0f),
        "the second native regional consumer retains four percent for an actual AI-versus-AI pair");
    putptr(field,0xA8,human_army);expert_field_stats(field,field);
    check(expert_near(expert_test_result,248.0f),
        "the native field stat consumer applies AI base200/rate8 against a player");
    putptr(field,0x98,human_army);putptr(field,0xA8,ai_army);expert_test_score=400;
    expert_field_stats(field,field);
    check(expert_near(expert_test_result,850.0f),
        "the native field stat consumer applies player base50/rate200 without replacing its native base");
    check(!expert_unit_scope.active && !combat_scope.first,
        "display and field computation restore unit and opponent scopes after native calls");
    putptr(field,0x98,ai_army);putptr(field,0xA8,human_army);expert_test_score=600;
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,5))=expert_combat::encode({100,39,2});
    expert_field_stats(field,field);
    check(expert_near(expert_test_result,335.2f) && expert_native_adjacency_option(image+0x1A46CA8,5,0)==100,
          "2026-10-23 decimal preset reaches native combat and is normalized before the native option consumer");
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,5))=expert_combat::encode({100,62,5});
    expert_field_stats(field,field);
    check(expert_near(expert_test_result,475.0f),"62.5 percent reaches the real production field-stat callback");
    putptr(field,0xA8,other_ai);expert_field_stats(field,field);
    check(expert_near(expert_test_result,124.0f),"decimal presets also keep AI-versus-AI fixed at four percent");
    BIND(0x466220,expert_test_model);BIND(0x875BC0,expert_test_scroll);
    BIND(0x640F20,expert_test_bind);BIND(0x85C860,expert_test_marker);
    BIND(0x86B270,expert_test_source);BIND(0x86B6F0,expert_test_row_at);
    BIND(0x65E810,expert_test_event);BIND(0x65F890,expert_test_number);
    BIND(0x65EDD0,expert_test_event);
    BIND(0x636250,expert_test_popup);BIND(0x5A73A0,expert_test_destroy);
    BIND(0x92E430,expert_test_button_factory);
    BIND(0x1A3CB0,expert_test_callback);BIND(0x85C9C0,expert_test_text_set);
    BIND(0x6475C0,expert_test_leave);BIND(0x5A7450,expert_test_destroy);
    BIND(0x141E0,expert_test_enable);BIND(0x8741D0,expert_test_visible);
    BIND(0x85AD00,expert_test_select);BIND(0x85AD20,expert_test_select);expert_test_radio_calls=0;
    expert_test_initialized=false;expert_test_registered_nodes.clear();
    Obj layout=static_cast<Obj>(calloc(1,0x1FB0)),list=object(0),row=static_cast<Obj>(calloc(1,0x300)),slider=object(0);
    Obj parent=object(0),standard=object(0),number=object(0);
    putptr(row,0x178,parent);putptr(row,0x1A0,standard);putptr(row,0x198,number);
    putptr(row,0,image+0x141BCF8);putptr(parent,8,row);putptr(parent,0x148,parent);
    *reinterpret_cast<int *>(standard+0x20)=876;*reinterpret_cast<int *>(standard+0x28)=100;
    *reinterpret_cast<int *>(standard+0x2C)=40;
    *reinterpret_cast<unsigned *>(parent+0x140)=53;
    expert_test_hides=0;
    check(expert_number_factory(parent+0x130,7,3,0,1,0)==number,
          "inline creation preserves the original factory return for native row+198 assignment");
    check(expert_test_hides==0,"2026-10-24 local10: creation never hides the presets (hide->show left skin parts hidden)");
    putptr(layout,0,image+0x141BE88);putptr(layout,0x8C0,list);putptr(list,0x1C0,layout);putptr(row,0x190,slider);
    *reinterpret_cast<unsigned *>(layout+0x1AC)=1;
    unsigned ids[]={2,3,11,4,5,6,7,16};
    for(unsigned i=0;i<8;++i)*reinterpret_cast<unsigned *>(layout+0xC30+i*0x60)=ids[i];
    *reinterpret_cast<unsigned *>(expert_option(layout+0x1C8,4))=expert_combat::encode({150,8});
    *reinterpret_cast<unsigned *>(expert_option(layout+0x1C8,5))=expert_combat::encode({100,4});
    original_expert_populate=expert_test_population;expert_test_layout=layout;expert_test_row=row;
    expert_populate(layout,nullptr,nullptr,nullptr);
    check(expert_test_ids==std::vector<unsigned>({2,3,11,61,62,4,5,6,7,16}) && dword(list,0x1B8)==10,
        "expert settings2 inserts exactly two rows between surround capture and adjacency and exposes ten scroll rows");
    auto &rows=expert_rows.at(layout);
    check(wcscmp(reinterpret_cast<const wchar_t *>(pointer(rows[0].bytes,0x18)),L"자세력 기본 공/방")==0 &&
          wcscmp(reinterpret_cast<const wchar_t *>(pointer(rows[1].bytes,0x18)),L"타세력 기본 공/방")==0,
        "new rows use the requested Korean player and computer labels");
    expert_list_bind(list,rows[0].bytes,0);
    check(dword(row,0x220)==61 && dword(row,0x224)==150 && dword(slider,0x10)==100,
        "base row binds its own value and standard marker while retaining its virtual event id");
    expert_list_bind(list,layout+0xD50,0);
    check(dword(row,0x220)==4 && dword(row,0x224)==8 && dword(slider,0x10)==4 &&
          expert_ui_settings(layout,4).region==8,"regional row displays direct percent and binding events cannot mutate it");
    auto &preset_panel=expert_preset_panels.at(row);
    Obj cycle=preset_panel.button;
    check(expert_test_buttons.size()==1 && !dword(standard,0x104) && expert_test_text[cycle]==L"소·중·대" &&
          dword(cycle,0x80)==0 && dword(cycle,0x88)==expert_choice_key+4 && dword(cycle,0x20)==969,
          "2026-10-25: one cycle button sits right of standard, starts unselected and offers 소 first");
    check(expert_test_registered_nodes==std::vector<unsigned>({48}),
          "2026-10-25: the cycle button is the layout v6 node48 in the pane's native registry");
    unsigned char callback[16]={},event[0x90]={};putptr(callback,8,layout);Obj event_ptr=event;
    *reinterpret_cast<unsigned *>(event+0x88)=61;*reinterpret_cast<unsigned *>(event+0x80)=200;
    expert_slider_event(callback,&event_ptr);
    check(expert_ui_settings(layout,61).base==200 && expert_ui_settings(layout,4).region==8,
        "base slider updates only the base component in the cached native option");
    *reinterpret_cast<unsigned *>(event+0x88)=4;*reinterpret_cast<unsigned *>(event+0x80)=0;
    expert_slider_event(callback,&event_ptr);
    check(expert_ui_settings(layout,4).region==0 && expert_ui_settings(layout,61).base==200,
        "regional slider accepts zero and preserves the base component");
    check(!expert_ui_store(layout,61,49) && !expert_ui_store(layout,4,201) && !expert_ui_store(layout,4,-1),
        "out-of-range UI values cannot enter the saved native settings");
    *reinterpret_cast<float *>(list+0x170)=0.375f;
    *reinterpret_cast<int *>(event+0x80)=-1000;expert_number_event(callback,&event_ptr,nullptr,nullptr);
    check(expert_ui_settings(layout,4).region==0,"2026-10-23: standard without a choice cannot alter the regional setting");
    expert_test_difficulty=4;
    *reinterpret_cast<unsigned *>(event+0x88)=expert_choice_key+4;*reinterpret_cast<int *>(event+0x80)=1;
    expert_choice_event(callback,&event_ptr);expert_list_bind(list,layout+0xD50,0);
    check(expert_ui_settings(layout,4).region==0 && dword(standard,0x104)==1 &&
          expert_test_text[cycle]==L"중" && dword(cycle,0x80)==2 && !expert_test_radio_calls,
          "choosing medium shows 중, offers 대 next and enables standard");
    *reinterpret_cast<unsigned *>(event+0x88)=4;*reinterpret_cast<int *>(event+0x80)=-1000;
    expert_number_event(callback,&event_ptr,nullptr,nullptr);
    check(expert_ui_settings(layout,4).region==25,"ultimate player medium standard applies 25 percent");
    expert_list_bind(list,layout+0xDB0,0);
    check(dword(standard,0x104)==0,"other-force choice begins independently unselected");
    *reinterpret_cast<unsigned *>(event+0x88)=expert_choice_key+5;*reinterpret_cast<int *>(event+0x80)=1;
    expert_choice_event(callback,&event_ptr);
    *reinterpret_cast<unsigned *>(event+0x88)=5;*reinterpret_cast<int *>(event+0x80)=-1000;
    expert_number_event(callback,&event_ptr,nullptr,nullptr);
    check(expert_ui_settings(layout,5).region==49 && expert_ui_settings(layout,4).region==25,
          "ultimate other-force medium applies 49 percent independently of player medium");
    // Acceptance table from the user's tooltips and 2026-10-23 beginner/intermediate decision.
    const unsigned expected_enemy[5][3]={{40,250,460},{40,250,460},{40,250,460},{160,392,625},{240,490,740}};
    const unsigned expected_player[3]={40,250,460};
    for(unsigned difficulty=0;difficulty<5;++difficulty)for(unsigned id=4;id<=5;++id)for(unsigned choice=0;choice<3;++choice) {
        expert_test_difficulty=difficulty;expert_choices[layout].selected[id-4]=static_cast<int>(choice);
        check(expert_apply_preset(layout,id) && expert_ui_settings(layout,id).region*10+expert_ui_settings(layout,id).tenth==
              (id==4?expected_player[choice]:expected_enemy[difficulty][choice]),
              "all five difficulties and both forces apply the user's exact tooltip values");
    }
    expert_test_difficulty=3;expert_choices[layout].selected[1]=1;expert_apply_preset(layout,5);
    expert_list_bind(list,layout+0xDB0,0);
    check(expert_test_text[number]==L"39.2" && expert_ui_settings(layout,5).tenth==2,
          "expert other-force medium displays and stores 39.2 without rounding");
    expert_ui_store(layout,62,150);
    check(expert_ui_settings(layout,5).tenth==2,"base edits preserve the fractional regional preset");
    unsigned before_invalid=dword(expert_option(layout+0x1C8,5),0);
    expert_test_difficulty=5;
    check(!expert_apply_preset(layout,5) && dword(expert_option(layout+0x1C8,5),0)==before_invalid,
          "unknown difficulty cannot apply a fallback preset");
    expert_test_difficulty=3;*reinterpret_cast<unsigned *>(layout+0x19C)=1;
    check(!expert_apply_preset(layout,5),"read-only scenario settings reject preset writes");
    *reinterpret_cast<unsigned *>(layout+0x19C)=0;
    expert_ui_store(layout,5,4);expert_ui_store(layout,4,4);
    expert_test_difficulty=4;
    expert_list_bind(list,rows[0].bytes,0);
    expert_choices[layout].selected[0]=2;expert_list_bind(list,layout+0xD50,0);
    check(expert_test_text[cycle]==L"대" && dword(cycle,0x80)==0,"2026-10-25: after 대 the cycle returns to 소");
    expert_list_bind(list,rows[0].bytes,0);
    check(dword(cycle,0x108)==0,"base rows hide recycled preset controls");
    expert_row_leave(row,nullptr);
    check(expert_preset_panels.find(row)==expert_preset_panels.end() && expert_choices[layout].selected[1]==1,
          "row leave discards native child pointers while the dialog keeps each force selection");
    expert_number_factory(parent+0x130,7,3,0,1,0);expert_list_bind(list,layout+0xDB0,0);
    check(expert_test_buttons.size()==2 && dword(standard,0x104)==1,
          "reinitialized row creates new controls and restores its independent selection");
    expert_row_destroy(row,0);
    check(expert_preset_panels.find(row)==expert_preset_panels.end(),"row destruction cannot leave stale control addresses");
    for(unsigned nodes:{48u,51u,63u}) {
        *reinterpret_cast<unsigned *>(parent+0x140)=nodes;
        expert_number_factory(parent+0x130,7,3,0,1,0);
    }
    check(expert_preset_panels.find(row)==expert_preset_panels.end() && expert_test_buttons.size()==2,
          "2026-10-25: unpatched, v4 and 48-wide v5 layouts create no controls");
    *reinterpret_cast<unsigned *>(parent+0x140)=53;
    expert_number_factory(parent+0x130,7,3,0,1,0);expert_list_bind(list,layout+0xDB0,0);
    check(expert_near(expert_test_scroll_position,0.375f),"native scroll callback receives the fractional position in XMM1 after standard button refresh");
    *reinterpret_cast<unsigned *>(event+0x88)=61;*reinterpret_cast<int *>(event+0x80)=300;
    expert_number_event(callback,&event_ptr,nullptr,nullptr);
    check(expert_test_popup_key==4 && dword(layout,0x1A4)==61 && dword(layout,0x1A8)==200 && !expert_popup_layout,
        "number popup uses native 50..200 bounds with the real base initial value and restores popup scope");
    *reinterpret_cast<unsigned *>(layout+0x1A8)=123;expert_number_confirm(callback);
    check(expert_ui_settings(layout,61).base==123 && expert_ui_settings(layout,4).region==4,
        "numeric confirmation updates the intended virtual base setting");
    *reinterpret_cast<unsigned *>(event+0x88)=5;expert_number_event(callback,&event_ptr,nullptr,nullptr);
    check(expert_test_popup_key==7 && dword(layout,0x1A4)==5 && dword(layout,0x1A8)==4,
        "regional number popup uses native 0..200 bounds and its own current percent");
    *reinterpret_cast<unsigned *>(layout+0x1A8)=200;expert_number_confirm(callback);
    check(expert_ui_settings(layout,5).region==200,"numeric confirmation accepts regional 200 percent");
    *reinterpret_cast<unsigned *>(layout+0x1AC)=0;expert_test_ids.clear();expert_populate(layout,nullptr,nullptr,nullptr);
    check(expert_test_ids.empty(),"other expert settings tabs do not gain the extra rows");
    unsigned prior_populates=expert_test_populates;LONG prior_enabled=enabled;
    enabled=0;expert_populate(nullptr,nullptr,nullptr,nullptr);
    enabled=1;expert_populate(nullptr,nullptr,nullptr,nullptr);enabled=prior_enabled;
    check(expert_test_populates==prior_populates+2,"disabled or absent expert screens forward without reading a null layout");
    expert_destroy(layout,0);check(expert_rows.find(layout)==expert_rows.end() && expert_choices.empty() && expert_preset_panels.empty(),
        "closing the expert dialog releases row and selection state; native parents own the buttons");
    check(expert_test_registered_nodes.size()==3,"native row reinitialization registers exactly one fresh set of nodes");
    free(layout);free(row);free(slider);free(list);free(parent);free(standard);free(number);
    for(Obj button:expert_test_buttons)free(button);expert_test_buttons.clear();expert_test_text.clear();expert_test_ids.clear();
    expert_test_registered_nodes.clear();
    functions=std::move(saved_functions);original_adjacency=saved_adjacency;original_expert_populate=saved_population;
    original_expert_unit_power=saved_power;
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,4))=saved_human;
    *reinterpret_cast<unsigned *>(expert_option(image+0x1A46CA8,5))=saved_ai;
    *reinterpret_cast<unsigned *>(image+0x1A47AF8)=saved_mode;
}
