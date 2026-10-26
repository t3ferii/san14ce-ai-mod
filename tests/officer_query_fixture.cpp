#include "../src/officer_query.h"
#include <cassert>
#include <cstdio>
using namespace officer_query;
static Condition C(uint16_t f,Op op,int64_t a){Condition c;c.field=f;c.op=op;c.a=a;return c;}
static int fails=0;
#define CHECK(x) do{if(!(x)){std::printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#x);++fails;}}while(0)
int main(){
    Catalog cat;Labels labels;
    cat.add(1,Kind::Text,L"이름",L"기본");
    cat.add(10,Kind::Number,L"무력",L"능력",0,200);
    cat.add(11,Kind::Number,L"지력",L"능력",0,200);
    cat.add(12,Kind::Number,L"충성",L"기본",0,100);
    cat.add(20,Kind::Label,L"소재",L"기본");
    cat.add(30,Kind::Set,L"개성",L"개성");
    cat.add(31,Kind::Set,L"전법",L"전법");
    auto mk=[&](unsigned id,const wchar_t*n,int64_t war,int64_t intel,int64_t loy,const wchar_t*city,std::vector<uint32_t> tr,std::vector<uint32_t> ta){
        Record r=cat.blank();r.id=id;r.name=n;r.numbers[0]=war;r.numbers[1]=intel;r.numbers[2]=loy;
        r.labels[0]=labels.intern(20,city);std::sort(tr.begin(),tr.end());std::sort(ta.begin(),ta.end());r.sets[0]=tr;r.sets[1]=ta;return r;};
    std::vector<Record> rs={mk(1,L"관우",97,75,100,L"허창",{5,7},{1,2}),mk(2,L"제갈량",38,100,kUnknown,L"성도",{9},{3}),
        mk(3,L"장비",98,30,90,L"허창",{5},{2}),mk(4,L"조운",96,76,95,L"양평관",{},{1})};
    uint32_t xu=labels.find(20,L"허창");CHECK(xu==1);
    // (무력>=95 AND 개성 has 5) OR (지력>=90 AND 전법 has 3)
    Group q;q.any=true;
    Group a;a.conditions.push_back(C(10,Op::AtLeast,95));Condition t;t.field=30;t.op=Op::HasAny;t.values={5};a.conditions.push_back(t);
    Group b;b.conditions.push_back(C(11,Op::AtLeast,90));Condition u;u.field=31;u.op=Op::HasAny;u.values={3};b.conditions.push_back(u);
    q.groups={a,b};
    auto res=evaluate(cat,q,rs);CHECK((res==std::vector<size_t>{0,1,2}));
    // Unknown never matches, even negated
    Group g;g.conditions.push_back(C(12,Op::AtLeast,50));CHECK(evaluate(cat,g,rs).size()==3);
    g.conditions[0].negate=true;CHECK(evaluate(cat,g,rs).empty());
    Condition k;k.field=12;k.op=Op::Known;Group gk;gk.conditions={k};CHECK(evaluate(cat,gk,rs).size()==3);
    // OR with unknown and true -> true; AND unknown + false -> false
    Group o;o.any=true;o.conditions.push_back(C(12,Op::AtLeast,0));o.conditions.push_back(C(11,Op::AtLeast,99));
    CHECK(matches(cat,o,rs[1]));
    Group n;n.conditions.push_back(C(12,Op::AtLeast,0));n.conditions.push_back(C(11,Op::AtMost,10));
    CHECK(test(cat,n,rs[1])==0);
    Group n2;n2.conditions.push_back(C(12,Op::AtLeast,0));CHECK(test(cat,n2,rs[1])==-1);
    // HasAll / Label AnyOf / Between / Contains / negate group
    Condition all;all.field=30;all.op=Op::HasAll;all.values={5,7};Group ga;ga.conditions={all};CHECK((evaluate(cat,ga,rs)==std::vector<size_t>{0}));
    Condition city;city.field=20;city.op=Op::AnyOf;city.values={xu};Group gc;gc.conditions={city};CHECK((evaluate(cat,gc,rs)==std::vector<size_t>{0,2}));
    Condition bt;bt.field=10;bt.op=Op::Between;bt.a=98;bt.b=96;Group gb;gb.conditions={bt};CHECK((evaluate(cat,gb,rs)==std::vector<size_t>{0,2,3}));
    Condition nm;nm.field=1;nm.op=Op::Contains;nm.text=L"제갈";Group gn;gn.conditions={nm};CHECK((evaluate(cat,gn,rs)==std::vector<size_t>{1}));
    gc.negate=true;CHECK((evaluate(cat,gc,rs)==std::vector<size_t>{1,3}));
    Condition none;none.field=30;none.op=Op::HasAny;none.values={5};none.negate=true;Group gno;gno.conditions={none};
    CHECK((evaluate(cat,gno,rs)==std::vector<size_t>{1,3}));
    Group empty;CHECK(evaluate(cat,empty,rs).size()==4);
    Group orEmpty;orEmpty.any=true;orEmpty.groups.push_back(Group());orEmpty.conditions.push_back(C(10,Op::AtLeast,98));
    CHECK((evaluate(cat,orEmpty,rs)==std::vector<size_t>{2}));
    Group onlyEmpty;onlyEmpty.any=true;onlyEmpty.groups.push_back(Group());CHECK(evaluate(cat,onlyEmpty,rs).size()==4);
    // describe
    auto member=[&](uint16_t f,uint32_t v)->std::wstring{return f==20?labels.text(f,v):L"#"+std::to_wstring(v);};
    std::wstring d=describe(cat,q,member);CHECK(d.find(L"또는")!=std::wstring::npos && d.find(L"무력 95 이상")!=std::wstring::npos);
    // save/load roundtrip with labels by text
    q.groups[1].negate=true;q.groups[0].conditions.push_back(city);
    std::wstring text=save(cat,labels,q);
    Labels other;other.intern(20,L"성도");other.intern(20,L"허창"); // different ids
    Group back;unsigned dropped=9;CHECK(load(cat,other,text,back,dropped));CHECK(dropped==0);
    CHECK(back.any && back.groups.size()==2 && back.groups[1].negate && back.groups[0].conditions.size()==3);
    CHECK(back.groups[0].conditions[2].values==std::vector<uint32_t>{2});
    Labels missing;Group back2;CHECK(load(cat,missing,text,back2,dropped));CHECK(dropped==1 && back2.groups[0].conditions.size()==2);
    CHECK(!load(cat,other,L"G\t0\t0\nC\tx\n",back2,dropped));
    CHECK(!load(cat,other,L"G\t0\t0\n",back2,dropped));
    std::wstring weird=L"a\tb\\c\nd";CHECK(unescape(escape(weird))==weird);
    // performance: 6000 records x nested query
    std::vector<Record> big;for(unsigned i=0;i<6000;++i){Record r=rs[i%4];r.id=i;big.push_back(r);}
    for(int i=0;i<50;++i)evaluate(cat,q,big);
    std::printf(fails?"FAILED %d\n":"ALL PASS\n",fails);return fails!=0;
}
