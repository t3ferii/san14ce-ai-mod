#pragma once
static Hex tile(unsigned x,unsigned y) {return y*220+x;}
struct MapPort final:MovementPort {
    ArmyGroup army;FieldSnapshot frame;MoveBatch last;
    std::map<Handle,std::vector<Hex>> orders;
    std::map<Handle,unsigned> captures;
    std::set<Hex> blocked;
    unsigned publications=0,releases=0;
    bool narrow=false,owned=false,fail=false,gate=false,delay=false;
    ReleaseReason reason=ReleaseReason::original_stop;
    explicit MapPort(unsigned count=7) {
        army.context={1,1,1,2,3,true};army.group=10;army.count=count;army.leader=100;
        frame.world=1;frame.group=10;frame.target=3;frame.city_region=10;frame.city_hex=tile(62,45);
        frame.revision=1;frame.hostile_city=frame.automatic=frame.mission_unchanged=true;
        frame.count=count;frame.flat_cost=10;frame.points_per_base_day=1;
        const auto facing=face_toward(tile(40,45),frame.city_hex);
        for(unsigned i=0;i<count;++i) {
            army.members[i]={1000+i,2000+i,100+i,4000};
            frame.members[i]={1000+i,100+i,4000,formation_hex(tile(40,45),i,facing),10+i,false};
        }
    }
    bool snapshot(const ArmyGroup &,FieldSnapshot &out) noexcept override {out=frame;return true;}
    std::vector<Approach> approaches(const FieldSnapshot &) override {
        return {{2,tile(40,43),true},{1,tile(44,45),true}};
    }
    unsigned region_of(Hex h) const noexcept override {
        if(h==tile(40,43))return 2;
        return h%220<=46?1u:(h%220>=53?10u:3u);
    }
    bool region_adjacent(unsigned city,unsigned base) const noexcept override {return city==10 && base==1;}
    bool owned_by_group(unsigned r) const noexcept override {return r==1 && owned;}
    bool cell_open(unsigned,Hex h) const noexcept override {
        const unsigned x=h%220,y=h/220;
        return h<no_hex && x>=35 && x<=68 && y>=35 && y<=55 && h!=frame.city_hex &&
            !blocked.count(h) && (!narrow || x<47 || x>52 || y==45);
    }
    bool edge_open(unsigned i,Hex a,Hex b) const noexcept override {
        return hex_distance(a,b)==1 && cell_open(i,b) && !(gate && i==frame.count-1 && b%220==47);
    }
    unsigned travel_cost(unsigned i,Hex,Hex b) const noexcept override {
        return b/220==42+i%3?1u:4u+i;
    }
    bool publish(const FieldSnapshot &s,const MoveBatch &batch) noexcept override {
        if(fail)return false;
        check(s.revision==frame.revision,"publication has fresh snapshot");
        std::set<Hex> admitted;
        for(const auto &order:batch.orders) {
            unsigned i=0;while(i<frame.count && frame.members[i].army!=order.army)++i;
            check(i<frame.count && !order.path.empty() && order.path.front()==frame.members[i].hex,"identity and route origin checked");
            for(std::size_t j=1;j<order.path.size();++j)check(edge_open(i,order.path[j-1],order.path[j]),"all generated edges are native legal");
            if(batch.mode==MoveMode::cohesive || batch.mode==MoveMode::column) {
                check(batch.common_base==10 && batch.common_cost==10 && order.path.size()==2,"minimum base common cost and single edge");
                for(unsigned j=0;j<frame.count;++j)if(j!=i)check(frame.members[j].hex!=order.path.back(),"scheduler does not enter occupied cohort cell");
                check(admitted.insert(order.path.back()).second,"same batch destinations unique");
            }
        }
        last=batch;++publications;
        for(const auto &order:batch.orders) {orders[order.army]=order.path;captures[order.army]=order.capture_region;}
        return true;
    }
    void release(const ArmyGroup &,ReleaseReason r) noexcept override {++releases;reason=r;orders.clear();}
    void advance() {
        for(unsigned i=0;i<frame.count;++i) {
            if(delay && i==frame.count-1 && frame.day%3)continue;
            auto &route=orders[frame.members[i].army];
            if(route.size()<2)continue;
            check(route.front()==frame.members[i].hex,"pending native origin follows actual cell");
            bool occupied=false;for(unsigned j=0;j<frame.count;++j)if(j!=i && frame.members[j].hex==route[1])occupied=true;
            if(occupied)continue;
            frame.members[i].hex=route[1];route.erase(route.begin());++frame.revision;
            if(route.size()==1 && captures[frame.members[i].army]==1)owned=true;
        }
        std::set<Hex> positions;for(unsigned i=0;i<frame.count;++i)check(positions.insert(frame.members[i].hex).second,"native simulation has no overlap");
    }
};
