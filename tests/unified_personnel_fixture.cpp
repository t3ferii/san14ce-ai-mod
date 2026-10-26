#include "../src/unified_personnel.h"
#include <cstdio>
#include <cstdlib>
static unsigned checks;
static void check(bool value,const char *name) {
    ++checks;if(!value){std::fprintf(stderr,"FAILED: %s\n",name);std::exit(1);}
}
int main() {
    using namespace unified;
    Officer a;a.id=1;a.name=L"조운";a.abilities={92,96,77,65,91};a.merit=12000;
    a.facets[City]={L"허창"};a.facets[Trait]={L"동찰",L"담력",L"9번째 개성"};
    a.facets[Policy]={L"병참개혁"};a.tactics={1,6,137,35,17};
    Filter f;check(matches(a,f),"empty criteria");f.name=L"조";check(matches(a,f),"Korean name search");
    f.name=L"관";check(!matches(a,f),"nonmatching name");f.name.clear();
    f.facets[City].values={L"허창",L"진류"};f.facets[Trait].values={L"9번째 개성"};
    check(matches(a,f),"city OR and ninth trait AND");f.facets[Policy].values={L"화계연구"};
    check(!matches(a,f),"cross-category intersection");f.facets[Policy].values.clear();
    f.facets[Trait]={{L"동찰",L"담력"},true};check(matches(a,f),"all selected traits");
    f.facets[Trait].values.push_back(L"위풍");check(!matches(a,f),"missing required trait");
    f.facets[Trait].all=false;check(matches(a,f),"any selected trait");
    f.minimum[1]=97;check(!matches(a,f),"ability minimum");f.minimum[1]=96;
    f.maximum[1]=96;check(matches(a,f),"inclusive ability boundaries");
    f.merit_min=12001;check(!matches(a,f),"merit minimum");f.merit_min=0;f.merit_max=11999;
    check(!matches(a,f),"merit maximum");
    check(knows(a,137) && !knows(a,0) && !knows(a,2),"known tactic IDs");
    Officer b=a;b.id=2;b.tactics={1};
    auto plan=plan_learning({a,b,b},{{1,1,3},{2,6,1},{28,137,1}});
    check(plan.work.size()==2,"officer by book cross product");
    check(plan.known==4,"known tactics skipped before consumption");
    check(plan.work[0].officer==2 && plan.work[0].tactic==6,"known officer does not reserve stock");
    check(plan.work[1].officer==2 && plan.work[1].tactic==137,"multiple books for one officer");
    Officer c=b;c.id=3;
    plan=plan_learning({b,c},{{2,6,1}});check(plan.work.size()==1 && plan.stock_missing==1,"shared limited inventory");
    plan=plan_learning({b},{{2,6,1},{3,6,1}});check(plan.work.size()==1 && plan.known==1,"duplicate book tactic");
    c.tactics={1,2,3,4,5,6,7,8,9,10};
    plan=plan_learning({c},{{28,137,1}});check(plan.work.empty() && plan.capacity_missing==1,"full slots never overwrite");
    plan=plan_learning({b},{{28,137,0}});check(plan.work.empty() && plan.stock_missing==1,"no-stock plan");
    std::vector<TacticInfo> catalog{{1,TacticKind::Common},{6,TacticKind::Common},
        {17,TacticKind::Common},{35,TacticKind::Unique},{137,TacticKind::Trade}};
    auto top=select_tactics(a,catalog,{Unique,137,1,17,6});
    check(top==std::array<unsigned,3>{35,137,1},"unique then Asura then common priority");
    top=select_tactics(a,catalog,{137,137,999,6});
    check(top==std::array<unsigned,3>{137,6,1},"deduplicate and skip unowned then fallback");
    top=select_tactics(b,catalog,{Trade,Unique});
    check(top==std::array<unsigned,3>{1,0,0},"fewer than three known tactics");
    top=select_tactics(a,catalog,{});check(top==std::array<unsigned,3>{1,6,137},"stable native learned-order fallback");
    Priorities priority;priority.martial={137};priority.intellect={6};
    check(priority.for_person(a)==priority.martial,"martial profile");a.abilities[2]=96;
    check(priority.for_person(a)==priority.martial,"tie uses martial profile");a.abilities[2]=97;
    check(priority.for_person(a)==priority.intellect,"intellect profile");
    std::printf("{\"passed\":true,\"checks\":%u}\n",checks);return 0;
}
