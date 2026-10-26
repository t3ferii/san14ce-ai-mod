namespace unified_native {
static bool post_available(unsigned id) {
    Obj post=table(0x784F8,id,255);if(!valid(post) || id==73)return false;
    unsigned type=opened_force[0x48],flags=post[0x3D];
    // Same title/special-force branches as 72D895..72D96D.
    if(type==6)return (flags&1)!=0;
    if(type==44)return (flags&2)!=0;
    if(type==45)return (flags&4)!=0;
    Obj rank=table(0x78490,opened_force[0x47],12);
    return valid(rank) && !(flags&7) && static_cast<int>(post[0x37])>static_cast<int>(rank[0xAC])-1;
}
static bool post_occupied(unsigned id,Obj except=nullptr) {
    for(unsigned officer=1;officer<=6000;++officer) {
        Obj p=table(0x158,officer,6000);if(serving(p) && p!=except && p[0xE7]==id)return true;
    }
    return false;
}
static Picker post_picker() {
    Picker p;p.title=L"빈 관직 선택 — 공적이 높은 무장부터 지휘병사가 높은 관직 수여";
    for(unsigned id=0;id<=255;++id)if(post_available(id) && !post_occupied(id)) {
        Obj post=table(0x784F8,id,255);
        p.ids.push_back(id);p.labels.push_back(copy_text(reinterpret_cast<const wchar_t *>(post+0x10))+
            L" · 지휘병사 "+std::to_wstring(award_post_capacity(id)));
    }
    return p;
}
struct NativePostMap {Obj unused=nullptr;std::map<Obj,unsigned> entries;};
static BatchResult give_posts(const std::vector<unified::Officer> &chosen,const std::vector<unsigned> &ids) {
    BatchResult result;auto roster=chosen;auto posts=ids;
    std::sort(roster.begin(),roster.end(),[](const auto &a,const auto &b){
        if(a.merit!=b.merit)return a.merit>b.merit;
        if(a.abilities[0]!=b.abilities[0])return a.abilities[0]>b.abilities[0];return a.id<b.id;
    });
    std::sort(posts.begin(),posts.end(),[](unsigned a,unsigned b){unsigned x=award_post_capacity(a),y=award_post_capacity(b);return x!=y?x>y:a<b;});
    NativeList eligible;award_post_people(opened_force,&eligible.value);std::set<Obj> allowed;
    for(Link *l=list_first(eligible.value);l;l=l->next)allowed.insert(l->value);
    size_t at=0;
    for(const auto &person:roster) {
        if(at>=posts.size()){++result.skipped;continue;}
        Obj p=table(0x158,person.id,6000);unsigned post_id=posts[at++];
        if(!context_valid() || !serving(p) || !allowed.count(p) || !post_available(post_id) ||
           post_occupied(post_id,p) || p[0xE7]==post_id ||
           !awards::may_post(award_active(p),award_deployed(p),post_id,award_post_capacity(post_id),award_post_capacity(p[0xE7]))) {++result.skipped;continue;}
        unsigned before=orders();if(!before){++result.no_orders;continue;}
        NativePostMap pending;pending.entries.emplace(p,post_id);
        // Native nodes: left/parent/right +0/+8/+10, is_nil +19,
        // pair<Obj,unsigned> +20/+28; header pointer at map+8, size+10.
        award_post_batch(reinterpret_cast<Obj>(&pending));
        if(p[0xE7]==post_id){set_orders(before-1);++result.changed;}else ++result.failed;
    }
    return result;
}
struct TradeResources {int gold=1000,grain=0,soldiers=0;};
static TradeResources trade_resources;
static std::wstring trade_summary(unsigned count) {
    return L"\n금: 1명당 "+std::to_wstring(trade_resources.gold)+L" / 최대 합계 "+std::to_wstring(static_cast<uint64_t>(trade_resources.gold)*count)+
        L"\n군량: 1명당 "+std::to_wstring(trade_resources.grain)+L" / 최대 합계 "+std::to_wstring(static_cast<uint64_t>(trade_resources.grain)*count)+
        L"\n병사: 1명당 "+std::to_wstring(trade_resources.soldiers)+L" / 최대 합계 "+std::to_wstring(static_cast<uint64_t>(trade_resources.soldiers)*count);
}
struct TradeEditor {TradeResources pending;std::function<void(bool)> done;};
static void trade_resource_menu(std::shared_ptr<TradeEditor> editor) {
    Picker p;p.multiple=false;p.title=L"무장 1명당 교역 공물";p.ids={0,1,2,3,4};
    p.labels={L"금  "+std::to_wstring(editor->pending.gold),L"군량  "+std::to_wstring(editor->pending.grain),
        L"병사  "+std::to_wstring(editor->pending.soldiers),L"공물 확정",L"취소"};
    pick(std::move(p),[editor](bool ok,const std::vector<unsigned> &ids){
        if(!ok || ids.empty() || !context_valid()){editor->done(false);return;}unsigned id=ids[0];
        if(id==4){editor->done(false);return;}
        if(id==3){
            if(!editor->pending.gold && !editor->pending.grain && !editor->pending.soldiers){notice(L"공물을 하나 이상 지정하세요.",L"교역");editor->done(false);return;}
            trade_resources=editor->pending;editor->done(true);return;
        }
        if(id>2){editor->done(false);return;}
        int initial=id==0?editor->pending.gold:id==1?editor->pending.grain:editor->pending.soldiers;
        unified_dialogs::number(initial,0,9999999,[editor,id](bool accepted,int value){
            if(!context_valid()){editor->done(false);return;}
            if(accepted){if(id==0)editor->pending.gold=value;else if(id==1)editor->pending.grain=value;else editor->pending.soldiers=value;}
            trade_resource_menu(editor);
        });
    });
}
static void choose_trade_resources(std::function<void(bool)> done) {
    auto editor=std::make_shared<TradeEditor>();editor->pending=trade_resources;editor->done=std::move(done);trade_resource_menu(std::move(editor));
}
static bool trade_route(unsigned id) {
    Obj foreign=table(0x85A20,id,4);
    return id>=1 && id<=4 && valid(foreign) && owned_city(native_city(foreign[0x31]));
}
static Picker trade_picker() {
    Picker p;p.title=L"교역 국가 선택 — 교역 거점을 소유한 국가";p.multiple=false;
    for(unsigned id=1;id<=4;++id)if(trade_route(id)) {
        Obj foreign=table(0x85A20,id,4);p.ids.push_back(id);
        p.labels.push_back(copy_text(reinterpret_cast<const wchar_t *>(foreign+0x10))+
            L" · "+city_name(foreign[0x31]));
    }
    return p;
}
static BatchResult trade(const std::vector<unified::Officer> &chosen,unsigned country) {
    BatchResult result;
    for(const auto &person:chosen) {
        Obj p=table(0x158,person.id,6000);
        if(!context_valid() || !idle(p) || !trade_route(country)){++result.skipped;continue;}
        unsigned before=orders();if(!before){++result.no_orders;continue;}
        // Native 1F44A0 charges the issuing force's 22E8C0 base, and the
        // officer's source district. Preserve one central order per task.
        Obj capital=fn<Obj(__fastcall *)(Obj)>(0x22E8C0)(opened_force);
        if(!valid(capital)){++result.failed;continue;}
        auto gold=*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(capital,0)+0x90);
        auto food=*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(capital,0)+0xA0);
        if(gold(capital)<trade_resources.gold || food(capital)<trade_resources.grain ||
            dword(capital,0x3C)<static_cast<unsigned>(trade_resources.soldiers)){++result.skipped;continue;}
        NativeList actors;if(!list_add(actors.value,p)){++result.failed;continue;}
        struct TradeCommand {GameList actors;unsigned country,padding;Obj gift;int gold,grain,soldiers,padding2;};
        static_assert(sizeof(TradeCommand)==0x30,"native trade command ABI");
        TradeCommand command{actors.value,country,0,nullptr,trade_resources.gold,trade_resources.grain,trade_resources.soldiers,0};
        if(!fn<int(__fastcall *)(TradeCommand *)>(0x1EFAB0)(&command)){++result.skipped;continue;}
        Obj district=personnel_order_district(p),central=order_district();
        unsigned old_origin=valid(district)?district[0x14]:0;
        fn<void(__fastcall *)(TradeCommand *,int)>(0x1EA8C0)(&command,1);
        if(award_active(p)) {
            if(valid(district) && district!=central)district[0x14]=static_cast<unsigned char>(old_origin);
            set_orders(before-1);++result.changed;
        }else ++result.failed;
    }
    return result;
}
}
