// tactic_priority pure engine v2 (docs/TACTIC_PRIORITY_V2_PLAN.md 7 WP-1). No game image: kind_of, select, select_remembered,
// classify, hint fitting, frame height, rank operations, grid layout, profile v2 round trip / v1 migration / broken text / limits.
#include "../src/tactic_priority_profile.h"
#include <cstdio>
static int checks=0,failures=0;
static void ok(bool v,const char *what) {++checks;if(!v){++failures;std::printf("FAIL %s\n",what);}}
typedef std::vector<unsigned> V;
static bool eq(const tp::Three &a,unsigned x,unsigned y,unsigned z) {return a[0]==x && a[1]==y && a[2]==z;}
static void test_kind() {
    ok(tp::kind_of(1)==tp::Kind::Common && tp::kind_of(24)==tp::Kind::Common,"kind common 1..24");
    ok(tp::kind_of(25)==tp::Kind::Trade && tp::kind_of(26)==tp::Kind::Trade && tp::kind_of(136)==tp::Kind::Trade && tp::kind_of(139)==tp::Kind::Trade,"kind trade");
    ok(tp::kind_of(27)==tp::Kind::Unique && tp::kind_of(35)==tp::Kind::Unique && tp::kind_of(135)==tp::Kind::Unique && tp::kind_of(140)==tp::Kind::Unique && tp::kind_of(200)==tp::Kind::Unique,"kind unique");
    ok(tp::kind_of(0)==tp::Kind::None && tp::kind_of(201)==tp::Kind::None && tp::kind_of(-1)==tp::Kind::None,"kind none");
}
static void test_select() {
    const std::vector<int> prio={tp::kUnique,137,1,17,6};
    ok(eq(tp::select(V{1,17,35,137,6},prio),35,137,1),"select unique, 137, 1");
    ok(eq(tp::select(V{35,35,137,137,1},prio),35,137,1),"select dedupes");
    ok(eq(tp::select(V{6,17,2,3},prio),17,6,0),"select: unranked 2,3 are never added, slot stays empty");
    ok(eq(tp::select(V{40,41,42,43},{tp::kUnique}),40,41,42),"select several unique in slot order, max 3");
    ok(eq(tp::select(V{5,9},prio),0,0,0),"select: nothing ranked is usable -> all empty (caller keeps the original)");
    ok(!tp::any(tp::select(V{5,9},prio)) && tp::any(tp::select(V{1},prio)),"any()");
    ok(eq(tp::select(V{},prio),0,0,0),"select empty eligible");
    ok(eq(tp::select(V{2,3,4,5},{}),0,0,0),"select empty ranked = nothing (caller keeps the original)");
    ok(eq(tp::select(V{2,3,4,5},{99,3,tp::kUnique}),3,0,0),"select ranked id not held is skipped, no filling");
    ok(eq(tp::select(V{1,17},{17}),17,0,0),"select ranked fewer than 3 even with more usable");
    ok(eq(tp::select(V{1,17,6,2},{2,6,17,1}),2,6,17),"select keeps ranked order, max 3");
    ok(tp::martial(60,60) && tp::martial(80,70) && !tp::martial(70,80),"tie is martial");
}
static void test_restore() {
    ok(eq(tp::select_remembered(tp::Three{17,35,0},V{1,17,35,137,6}),17,35,0),"remembered: exactly 2, third stays empty");
    ok(eq(tp::select_remembered(tp::Three{99,17,35},V{1,17,137,6}),0,17,0),"remembered drops unusable ones only, slots kept");
    ok(eq(tp::select_remembered(tp::Three{},V{1,17,35,137}),0,0,0),"no remembered tactic stays empty");
    ok(eq(tp::select_remembered(tp::Three{17,17,17},V{1,17,35,137}),17,0,0),"remembered duplicates");
    ok(eq(tp::select_remembered(tp::Three{17,0,0},V{17}),17,0,0),"remembered one tactic");
    ok(eq(tp::select_remembered(tp::Three{0,17,0},V{17}),0,17,0),"remembered keeps its slot");
    ok(eq(tp::select_remembered(tp::Three{6,17,1},V{1,6,17,35}),6,17,1),"remembered full three kept in order");
    for(unsigned count=0;count<=10;++count) {   // never an ineligible id, never more than were remembered
        V el;for(unsigned i=0;i<count;++i)el.push_back(30+i);
        tp::Three r=tp::select_remembered(tp::Three{32,99,0},el);
        unsigned n=0;bool bad=false;for(unsigned id:r){if(id)++n;if(id && !tp::contains(el,id))bad=true;}
        ok(!bad && n==(count>2?1u:0u),"select_remembered 0..10 eligible");
    }
}
static void test_classify() {
    tp::Memo m;m.person=100;m.formation=4;m.generation=7;
    ok(tp::classify(nullptr,100,4,7)==tp::Event::Person,"classify no memo");
    ok(tp::classify(&m,101,4,7)==tp::Event::Person,"classify person changed");
    ok(tp::classify(&m,100,5,7)==tp::Event::Formation,"classify formation changed");
    ok(tp::classify(&m,100,4,8)==tp::Event::Settings,"classify generation changed");
    ok(tp::classify(&m,100,5,8)==tp::Event::Formation,"classify formation wins over generation");
    ok(tp::classify(&m,100,4,7)==tp::Event::None,"classify unchanged");
}
static void test_ranks() {
    const std::vector<int> none;
    const auto tiles=tp::default_list(true,none);
    std::vector<int> r;
    ok(tp::rank_of(r,17)==0,"rank_of empty");
    ok(tp::toggle_rank(r,17,tiles) && tp::toggle_rank(r,tp::kUnique,tiles) && tp::toggle_rank(r,1,tiles) && r==std::vector<int>({17,tp::kUnique,1}),"toggle appends in click order");
    ok(tp::rank_of(r,17)==1 && tp::rank_of(r,tp::kUnique)==2 && tp::rank_of(r,1)==3 && tp::rank_of(r,6)==0,"rank_of 1-based");
    ok(tp::toggle_rank(r,tp::kUnique,tiles) && r==std::vector<int>({17,1}) && tp::rank_of(r,1)==2,"toggle off pulls later ranks up");
    ok(tp::toggle_rank(r,17,tiles) && r==std::vector<int>({1}),"toggle off first");
    ok(!tp::toggle_rank(r,999,tiles) && !tp::toggle_rank(r,0,tiles) && !tp::toggle_rank(r,35,tiles) && r==std::vector<int>({1}),"toggle of non-tile ids is refused");
    tp::clear(r);ok(r.empty(),"clear");
    ok(tp::preset(true,none)==tiles && tp::preset(false,none)==tp::default_list(false,none),"preset = default list");
    const auto eff=tp::effective({6,tp::kUnique},tiles);
    ok(eff.size()==tiles.size() && eff[0]==6 && eff[1]==tp::kUnique && eff[2]==137 && eff[3]==1 && eff[4]==17,"effective ranked then defaults");
    ok(tp::effective({},tiles)==tiles,"effective of empty = defaults");
    ok(tp::effective({999,17,17},tiles)[0]==17 && tp::effective({999,17,17},tiles).size()==tiles.size(),"effective drops unknown and duplicates");
    ok(tp::effective(tiles,tiles)==tiles,"effective of full defaults");
    std::vector<int> catalog={1,2,3,6,17,137};
    std::vector<int> l={17,17,999,35,1,tp::kUnique,tp::kUnique,0};
    tp::sanitize(l,catalog);
    ok(l==std::vector<int>({17,1,tp::kUnique}),"sanitize dedupes, drops unknown/unique-kind ids");
    ok(tp::ranked_from_full(tiles,tiles).empty(),"ranked_from_full of defaults is empty");
    std::vector<int> full=tiles;std::swap(full[3],full[4]);
    const auto rf=tp::ranked_from_full(full,tiles);
    ok(tp::effective(rf,tiles)==full && rf.size()==4,"ranked_from_full keeps the minimal prefix");
}
typedef std::vector<tp::Tile> Tiles;
static std::wstring catalog_name(int id) {   // fixture catalog: the listed names plus siege, naval and one unlisted name
    static const wchar_t *const names[]={L"?",L"분전",L"돌격",L"급습",L"연노",L"제사",L"매성",L"업화",L"화시",L"철벽",L"경파",L"치료",L"진정",L"격려",L"도발",L"고무",
        L"대갈",L"교란",L"견제",L"저지",L"아수라진",L"안식사법",L"구갑대열",L"정란",L"충차",L"투석",L"전상돌격",L"구축",L"강습",L"시람",L"알수없음"};
    return id>=1 && id<=30?names[id]:names[0];
}
static void test_groups() {
    ok(tp::hidden_name(L"정란") && tp::hidden_name(L"충차") && tp::hidden_name(L"투석") && tp::hidden_name(L"전상돌격") &&
       tp::hidden_name(L"구축") && tp::hidden_name(L"강습") && tp::hidden_name(L"시람") && !tp::hidden_name(L"분전") && !tp::hidden_name(L""),"hidden siege/naval names");
    unsigned o=9;
    ok(tp::tile_group(L"아수라진",&o)==1 && o==0 && tp::tile_group(L"구갑대열",&o)==1 && o==2,"group 1 order");
    ok(tp::tile_group(L"분전",&o)==2 && o==0 && tp::tile_group(L"제사",&o)==2 && o==4,"group 2");
    ok(tp::tile_group(L"매성",&o)==3 && tp::tile_group(L"화시",&o)==3 && o==2,"group 3");
    ok(tp::tile_group(L"철벽",&o)==4 && tp::tile_group(L"고무",&o)==4 && o==6,"group 4");
    ok(tp::tile_group(L"대갈",&o)==5 && tp::tile_group(L"저지",&o)==5 && o==3,"group 5");
    ok(tp::tile_group(L"충차")==6 && tp::tile_group(L"뭐지")==6 && tp::tile_group(L"")==6,"unlisted -> other");
    std::vector<int> ids{tp::kUnique};for(int i=30;i>=1;--i)ids.push_back(i);   // scrambled incoming order, 30 = unlisted name
    const Tiles t=tp::build_tiles(ids,catalog_name);
    ok(t.size()==1+30-7,"7 siege/naval tactics removed");
    ok(t[0].id==tp::kUnique && t[0].group==0,"unique tile first");
    bool sorted=true,no_hidden=true;
    for(size_t i=1;i<t.size();++i) {if(t[i].group<t[i-1].group)sorted=false;if(tp::hidden_name(catalog_name(t[i].id)))no_hidden=false;}
    ok(sorted && no_hidden && t.back().group==6 && t.back().id==30,"groups ascending, no hidden tile, unlisted last");
    std::vector<std::wstring> g4;for(const auto &x:t)if(x.group==4)g4.push_back(catalog_name(x.id));
    ok(g4==std::vector<std::wstring>({L"철벽",L"경파",L"치료",L"진정",L"격려",L"도발",L"고무"}),"group order = listed order");
    std::vector<std::wstring> g1;for(const auto &x:t)if(x.group==1)g1.push_back(catalog_name(x.id));
    ok(g1==std::vector<std::wstring>({L"아수라진",L"안식사법",L"구갑대열"}),"group 1 order");
    ok(tp::tile_ids(t).size()==t.size(),"tile_ids");
}
static void test_grid() {
    std::vector<int> ids{tp::kUnique};for(int i=1;i<=30;++i)ids.push_back(i);
    const Tiles t=tp::build_tiles(ids,catalog_name);
    std::vector<unsigned> groups;for(const auto &x:t)groups.push_back(x.group);
    for(int w:{954,1094}) {
        const auto g=tp::layout_groups(groups,w);
        bool good=g.cells.size()==t.size() && g.tile_w==164 && g.tile_h==48 && g.gap_x>=12 && g.gap_x<=24 && g.w==w && g.h<=560;
        for(size_t i=0;i<g.cells.size();++i) {
            const auto &c=g.cells[i];
            if(c.x<tp::kMargin || c.x+g.tile_w>g.w-tp::kMargin || c.y<0 || c.y+g.tile_h>g.h)good=false;
            if(i && groups[i]!=groups[i-1] && !(c.y>=g.cells[i-1].y+g.tile_h+g.gap_y && c.x==g.cells[0].x))good=false;   // a group starts a new row at the left edge, same row gap
            for(size_t j=i+1;j<g.cells.size();++j) {
                const auto &d=g.cells[j];
                if(c.x<d.x+g.tile_w && d.x<c.x+g.tile_w && c.y<d.y+g.tile_h && d.y<c.y+g.tile_h)good=false;
            }
        }
        ok(good,("grouped grid valid w="+std::to_string(w)).c_str());
    }
    {   // group sizes 1,3,5,3,7,4: rows 1,1,1,1,2,1
        const auto g=tp::layout_groups(std::vector<unsigned>{0,1,1,1,2,2,2,2,2,3,3,3,4,4,4,4,4,4,4,5,5,5,5},954);
        ok(g.rows==7 && g.cells[0].y==0 && g.cells[1].y==62,"row count and the even row gap (48+14)");
        ok(g.cells[17].y==g.cells[12].y+62 && g.cells[12].x==g.cells[0].x && g.cells[17].x==g.cells[12].x+0 && g.cells[16].x==g.cells[12].x+4*(164+g.gap_x),"a group wraps after 5 columns inside the group");
        ok(g.cells[12].y>g.cells[11].y+48+10,"next group starts below with the group gap");
    }
    {const auto g=tp::layout_groups(std::vector<unsigned>{},954);ok(g.cells.empty() && g.rows==0 && g.h==0,"empty grid");}
    {const auto g=tp::layout_groups(std::vector<unsigned>{6,6,6,6,6,6},954);ok(g.rows==2 && g.cells[5].y==62 && g.cells[5].x==g.cells[0].x,"single group wraps at five columns");}
}
static void test_hint_and_frame() {
    ok(tp::text_width(L"ab")==26 && tp::text_width(L"무력")==48 && tp::text_width(L"")==0,"text_width");
    const std::vector<std::wstring> c{L"무력형 3개 지정 · 무력≥지력→무력형 / 지력>무력→지력형",L"무력형 3개 지정 · 무력≥지력→무력형",L"무력형 3개 지정"};
    ok(tp::fit_hint(c,5000)==c[0],"fit_hint takes the longest when it fits");
    ok(tp::fit_hint(c,tp::text_width(c[1])*103/100)==c[1],"fit_hint shortens");
    ok(tp::fit_hint(c,10)==c[2] && tp::fit_hint({},100).empty(),"fit_hint falls back to the last / empty");
    const int hint_w=1094-416;   // type 1 hint node at (377,18,W-416,28)
    ok(tp::text_width(tp::fit_hint(c,hint_w))*103/100<=hint_w || tp::fit_hint(c,hint_w)==c[2],"chosen hint fits the node");
    ok(tp::frame_height(0)==tp::kFrameMinH && tp::frame_height(400)==tp::kFrameMinH,"small grid keeps the original window height");
    // realistic worst case: unique + groups 3,5,3,7,4 + about 10 unlisted tactics -> 9 rows
    std::vector<unsigned> groups{0,1,1,1,2,2,2,2,2,3,3,3,4,4,4,4,4,4,4,5,5,5,5,6,6,6,6,6,6,6,6,6,6};
    const auto g=tp::layout_groups(groups,1094);
    const int fh=tp::frame_height(g.h),hint_y=tp::kGridTopY+g.h+tp::kBlockGap,button_y=fh-tp::kButtonRowH-tp::kButtonBottomGap;
    ok(g.rows==9 && fh>=tp::kFrameMinH && hint_y+2*tp::kLineH+tp::kLineGap<=button_y && tp::kGridTopY+g.h<=hint_y && fh<=1080,"frame grows so the grid, two hint lines and the buttons never overlap");
    ok(tp::frame_height(g.h+58)>=tp::frame_height(g.h)+58 || tp::frame_height(g.h)==tp::kFrameMinH,"frame height is monotonic in the grid height");
}
static bool has_all_once(const std::vector<int> &l,const std::vector<int> &catalog) {
    int unique=0;for(int id:l)if(id==tp::kUnique)++unique;
    if(unique!=1)return false;
    for(int id:catalog)if(tp::known(id,catalog) && std::count(l.begin(),l.end(),id)!=1)return false;
    return true;
}
static void test_profile() {
    const std::vector<int> none;
    const auto dm=tp::default_list(true,none),di=tp::default_list(false,none);
    ok(dm.size()>3 && dm[0]==tp::kUnique && dm[1]==137 && dm[2]==1 && dm[3]==17 && dm[4]==6,"default martial head");
    ok(di[0]==tp::kUnique && di[1]==137 && di[2]==17 && di[3]==6,"default intellect head");
    ok(std::count(di.begin(),di.end(),1)==1 && di.size()==dm.size() && has_all_once(dm,none),"default intellect holds 1 later, same size");
    ok(dm.size()==1+24+6,"default size = U + 24 common + 6 trade");
    tp::Settings s;s.martial={17,tp::kUnique,1};s.intellect={};
    const std::string text=tp::encode(s);
    ok(text=="v=2;martial=17,U,1;intellect=","encode v2 text");
    tp::Settings back;int ver=0;bool restore=true;
    ok(tp::decode(text,none,back,&ver,&restore) && ver==2 && !restore && back.martial==s.martial && back.intellect.empty(),"v2 round trip with empty list");
    ok(text.size()<=tp::kMaxProfile,"encoded length within limit");
    tp::Settings keep;keep.martial={6};keep.intellect={17};
    const char *bad[]={"","garbage","v=3;martial=U;intellect=U","v=2;martial=U","v=2;martial=U,x;intellect=U","v=2;martial=U,201;intellect=U","v=2;martial=U,1,;intellect=U",
        "v=2;martial=U;intellect=U;extra=1","v=2;v=2;martial=U;intellect=U","v=2;restore=0;martial=U;intellect=U","martial=U;intellect=U;v=2","v=1;martial=U;intellect=U",
        "v=1;restore=2;martial=U,1;intellect=U,1","v=1;restore=0;martial=;intellect=U","v=1;restore=0;martial=U,1","v=1;restore=0;martial=U,x;intellect=U,1","v=2;martial=U,,1;intellect="};
    for(const char *b:bad) {tp::Settings t=keep;ok(!tp::decode(b,none,t) && t.martial==keep.martial && t.intellect==keep.intellect,b);}
    std::string longtext="v=2;martial=U";while(longtext.size()<2100)longtext+=",1";
    tp::Settings t;ok(!tp::decode(longtext+";intellect=",none,t),"decode > 2047 chars rejected");
    ok(tp::decode("v=2;martial=;intellect=",none,t) && t.martial.empty() && t.intellect.empty(),"decode minimal v2");
    ok(!tp::decode("v=2;martial=U;intellect=\xEB\xB0\x98",none,t),"decode non-ascii rejected");
    std::vector<int> catalog={1,2,3,6,17,137};
    ok(tp::decode("v=2;martial=17,17,25,35,U,U;intellect=2",catalog,t) && t.martial==std::vector<int>({17,tp::kUnique}) && t.intellect==std::vector<int>({2}),"v2 drops duplicates and ids outside the catalog");
    tp::Settings old;old.martial=dm;old.intellect=di;
    std::string v1="v=1;restore=1;martial="+tp::encode_list(old.martial)+";intellect="+tp::encode_list(old.intellect);
    ok(tp::decode(v1,none,t,&ver,&restore) && ver==1 && restore && t.martial.empty() && t.intellect.empty(),"v1 default profile migrates to empty ranked lists");
    old.martial=dm;std::swap(old.martial[3],old.martial[4]);
    v1="v=1;restore=0;martial="+tp::encode_list(old.martial)+";intellect="+tp::encode_list(di);
    ok(tp::decode(v1,none,t,&ver,&restore) && ver==1 && !restore && t.martial.size()==4 && tp::effective(t.martial,dm)==old.martial && t.intellect.empty(),"v1 custom order migrates to a shorter ranked list");
    ok(tp::decode("v=1;restore=0;martial=U,1;intellect=U,17",none,t) && tp::effective(t.martial,dm)==tp::effective({tp::kUnique,1},dm) && tp::effective(t.intellect,di)==tp::effective({tp::kUnique,17},di),"v1 short list migrates");
    ok(tp::decode("v=1;restore=0;martial=6,U;intellect=U",none,t) && t.martial==std::vector<int>({6}),"v1 6,U migrates to the minimal prefix 6");
    ok(tp::decode(tp::encode(t),none,back) && back.martial==t.martial,"migrated profile re-encodes as v2");
}
int main() {
    test_kind();test_select();test_restore();test_classify();test_ranks();test_groups();test_grid();test_hint_and_frame();test_profile();
    std::printf("{\"checks\":%d,\"failures\":%d}\n",checks,failures);
    return failures?1:0;
}
