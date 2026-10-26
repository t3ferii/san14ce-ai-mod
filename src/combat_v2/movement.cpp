#include "movement.h"
#include <cmath>
#include <limits>
#include <queue>
#include <tuple>

namespace combat_v2 {
namespace {
struct Route {std::vector<Hex> cells; std::uint64_t cost = 0;};
template<class Goal,class Heuristic,class Edge>
Route search(Hex from,Goal goal,Heuristic heuristic,Edge edge) {
    if(from >= no_hex)return {};
    constexpr auto infinity = (std::numeric_limits<std::uint64_t>::max)();
    std::vector<std::uint64_t> cost(no_hex,infinity);
    std::vector<Hex> parent(no_hex,no_hex);
    using Item = std::pair<std::uint64_t,Hex>;
    std::priority_queue<Item,std::vector<Item>,std::greater<Item>> open;
    cost[from]=0; parent[from]=from; open.push({heuristic(from),from});
    Hex end=no_hex;
    while(!open.empty()) {
        const auto item=open.top(); open.pop(); const Hex here=item.second;
        if(item.first!=cost[here]+heuristic(here))continue;
        if(goal(here)) {end=here; break;}
        for(unsigned d=0;d<6;++d) {
            const Hex next=neighbor(here,d); if(next>=no_hex)continue;
            const unsigned charge=edge(here,next);
            if(!charge || charge>1000000u)continue;
            const auto value=cost[here]+charge;
            if(value<cost[next]) {cost[next]=value;parent[next]=here;open.push({value+heuristic(next),next});}
        }
    }
    if(end>=no_hex)return {};
    Route result; result.cost=cost[end];
    for(Hex h=end;;h=parent[h]) {result.cells.push_back(h);if(h==from)break;}
    std::reverse(result.cells.begin(),result.cells.end()); return result;
}
bool contains(const std::vector<Hex> &cells,Hex h) {return std::find(cells.begin(),cells.end(),h)!=cells.end();}
Route individual(MovementPort &port,const FieldSnapshot &s,unsigned i,Hex target,
                 const std::vector<Hex> &excluded={}) {
    if(target>=no_hex || target==s.city_hex || !port.cell_open(i,target))return {};
    return search(s.members[i].hex,[target](Hex h){return h==target;},
        [target](Hex h){return hex_distance(h,target);},[&](Hex a,Hex b) {
            if(b==s.city_hex || contains(excluded,b) || !port.cell_open(i,b) || !port.edge_open(i,a,b))return 0u;
            return port.travel_cost(i,a,b);
        });
}
bool footprint(MovementPort &port,const FieldSnapshot &s,Hex center,unsigned facing) {
    for(unsigned i=0;i<s.count;++i) {
        const auto h=formation_hex(center,i,facing);
        if(h>=no_hex || h==s.city_hex || !port.cell_open(i,h))return false;
    }
    return true;
}
bool all_at(const FieldSnapshot &s,const std::array<Hex,max_units> &targets) {
    for(unsigned i=0;i<s.count;++i)if(s.members[i].hex!=targets[i])return false;
    return true;
}
bool vacant(const FieldSnapshot &s,Hex target,unsigned member) {
    for(unsigned i=0;i<s.count;++i)if(i!=member && s.members[i].hex==target)return false;
    return true;
}
void append(std::vector<Hex> &into,const std::vector<Hex> &more,std::size_t start=0) {
    for(std::size_t i=start;i<more.size();++i)if(into.empty() || into.back()!=more[i])into.push_back(more[i]);
}
bool assemble_paths(MovementPort &port,const FieldSnapshot &s,Hex around,unsigned facing,bool at_base,
                    unsigned base,std::array<std::vector<Hex>,max_units> &paths,Hex &center) {
    for(Hex candidate:radius_cells(around,3)) {
        if(at_base && port.region_of(candidate)!=base)continue;
        if(!footprint(port,s,candidate,facing))continue;
        std::array<std::vector<Hex>,max_units> trial;
        bool possible=true;
        for(unsigned i=0;i<s.count;++i) {
            std::vector<Hex> other_seats;
            // Center commander arrives before the ring closes around it.
            for(unsigned j=0;i && j<s.count;++j)if(j!=i)other_seats.push_back(formation_hex(candidate,j,facing));
            auto route=individual(port,s,i,formation_hex(candidate,i,facing),other_seats);
            if(route.cells.empty()) {possible=false;break;}
            trial[i]=std::move(route.cells);
        }
        if(possible) {center=candidate;paths=std::move(trial);return true;}
    }
    return false;
}
bool select_base(MovementPort &port,const FieldSnapshot &s,Approach &chosen,
                 std::array<std::vector<Hex>,max_units> &paths,std::array<Hex,max_units> &ends,
                 unsigned fixed_region,Hex fixed_hex) {
    std::uint64_t best=(std::numeric_limits<std::uint64_t>::max)();
    for(const auto &candidate:port.approaches(s)) {
        if(fixed_region && (candidate.region!=fixed_region || candidate.center!=fixed_hex))continue;
        if(!candidate.ordinary_base || candidate.center>=no_hex || !candidate.region ||
           candidate.region==s.city_region || port.region_of(candidate.center)!=candidate.region ||
           !port.region_adjacent(s.city_region,candidate.region))continue;
        const auto facing=face_toward(candidate.center,s.city_hex);
        std::array<Hex,max_units> trial_ends{};
        std::array<std::vector<Hex>,max_units> trial_paths;
        std::uint64_t score=0; bool possible=true;
        const auto nearby=radius_cells(candidate.center,3);
        for(unsigned i=0;i<s.count;++i) {
            auto options=nearby;
            const Hex preferred=formation_hex(candidate.center,i,facing);
            if(i==0)options={candidate.center};
            else {
                auto it=std::find(options.begin(),options.end(),preferred);
                if(it!=options.end())std::rotate(options.begin(),it,it+1);
            }
            bool placed=false;
            for(Hex h:options) {
                if(port.region_of(h)!=candidate.region)continue;
                bool used=false;for(unsigned j=0;j<i;++j)if(trial_ends[j]==h)used=true;
                for(unsigned j=0;j<i;++j)if(contains(trial_paths[j],h))used=true;
                if(used)continue;
                auto route=individual(port,s,i,h);
                if(route.cells.empty())continue;
                score+=route.cost;trial_ends[i]=h;trial_paths[i]=std::move(route.cells);placed=true;break;
            }
            if(!placed) {possible=false;break;}
        }
        if(possible) {
            score=0;
            for(unsigned i=0;i<s.count;++i) {
                std::vector<Hex> other_ends;
                for(unsigned j=0;i && j<s.count;++j)if(i!=j)other_ends.push_back(trial_ends[j]);
                auto route=individual(port,s,i,trial_ends[i],other_ends);
                if(route.cells.empty()) {possible=false;break;}
                score+=route.cost;trial_paths[i]=std::move(route.cells);
            }
        }
        if(possible && score<best) {best=score;chosen=candidate;paths=std::move(trial_paths);ends=trial_ends;}
    }
    return chosen.region!=0;
}
Route shared_path(MovementPort &port,const FieldSnapshot &s,Hex from,unsigned facing,bool rigid) {
    return search(from,[&](Hex h){return hex_distance(h,s.city_hex)==2 && footprint(port,s,h,facing);},
        [&](Hex h){const auto d=hex_distance(h,s.city_hex);return d>2?d-2:0;},[&](Hex a,Hex b) {
            for(unsigned i=0;i<s.count;++i) {
                const auto x=rigid?formation_hex(a,i,facing):a;
                const auto y=rigid?formation_hex(b,i,facing):b;
                if(y>=no_hex || y==s.city_hex || !port.cell_open(i,y) || !port.edge_open(i,x,y))return 0u;
            }
            return 1u; // neutral common cost: terrain remains a legality gate
        });
}
bool column_paths(MovementPort &port,const FieldSnapshot &s,const std::vector<Hex> &shared,
                  std::array<std::vector<Hex>,max_units> &paths,std::array<Hex,max_units> &ends) {
    if(shared.size()<2)return false;
    const Hex receiving=shared.back();auto parking=radius_cells(receiving,3);
    // Leave the center and incoming corridor clear. All parking is on the
    // outer ring BEYOND the entrance, so early arrivals cannot plug the exit.
    for(unsigned i=0;i<s.count;++i) {
        std::vector<Hex> prefix;std::size_t join=shared.size();
        for(std::size_t k=0;k<shared.size();++k)if(shared[k]==s.members[i].hex) {join=k;break;}
        if(join==shared.size()) {
            auto merge=search(s.members[i].hex,[&](Hex h){return h==shared.front();},
                [&](Hex h){return hex_distance(h,shared.front());},[&](Hex a,Hex b) {
                    return b!=s.city_hex && port.cell_open(i,b) && port.edge_open(i,a,b)?1u:0u;
                });
            if(merge.cells.empty())return false;
            prefix=std::move(merge.cells);join=0;
        }
        append(prefix,shared,join);
        bool found=false;
        for(Hex target:parking) {
            if(hex_distance(receiving,target)!=3 || contains(shared,target) ||
               hex_distance(target,shared.front())<=hex_distance(receiving,shared.front()) || !port.cell_open(i,target))continue;
            bool used=false;for(unsigned j=0;j<i;++j)if(ends[j]==target)used=true;
            if(used)continue;
            auto tail=search(receiving,[target](Hex h){return h==target;},
                [target](Hex h){return hex_distance(h,target);},[&](Hex a,Hex b) {
                    if(b==s.city_hex || contains(shared,b) || hex_distance(receiving,b)>3 ||
                       !port.cell_open(i,b) || !port.edge_open(i,a,b))return 0u;
                    for(unsigned j=0;j<i;++j)if(b==ends[j])return 0u;
                    return 1u;
                });
            if(tail.cells.empty())continue;
            bool plugs=false;
            for(unsigned j=0;j<i;++j)if(contains(paths[j],target))plugs=true;
            if(plugs)continue;
            paths[i]=prefix;append(paths[i],tail.cells);ends[i]=target;found=true;break;
        }
        if(!found)return false;
    }
    return true;
}
unsigned slowest_base(const FieldSnapshot &s) noexcept {
    unsigned minimum=31;
    for(unsigned i=0;i<s.count;++i) {
        if(s.members[i].base_movement<1 || s.members[i].base_movement>30)return 0;
        minimum=(std::min)(minimum,s.members[i].base_movement);
    }
    return minimum;
}
} // namespace

MovementController::MovementController(const ArmyGroup &army) noexcept:army_(army) {
    state_.info.active=army.context.automatic && army.context.world && army.group && army.count>=2 && army.count<=max_units;
    state_.endpoints.fill(no_hex);state_.pending_hex.fill(no_hex);
}
void MovementController::resume_after_detour() noexcept {
    if(!state_.info.active)return;
    if(state_.info.stage==TravelStage::cohesive_approach) {
        state_.info.stage=TravelStage::assemble;state_.info.mode=MoveMode::assemble;
    }
    state_.planned=state_.pending=false;state_.credit=0;state_.member_credit.fill(0);
    state_.pending_hex.fill(no_hex);
}
MoveStatus MovementController::tick(MovementPort &port) noexcept {
    if(!state_.info.active)return MoveStatus::released;
    FieldSnapshot s;
    if(!port.snapshot(army_,s))return MoveStatus::invalid_snapshot;
    bool matching=s.world==army_.context.world && s.group==army_.group && s.target==army_.context.target &&
        s.count==army_.count && s.city_hex<no_hex && s.city_region && s.automatic;
    for(unsigned i=0;matching && i<s.count;++i) {
        const auto &m=s.members[i];
        matching=m.army==army_.members[i].army && m.officer==army_.members[i].officer && m.soldiers && m.hex<no_hex;
        for(unsigned j=0;j<i;++j)if(s.members[j].hex==m.hex)matching=false;
    }
    if(!matching || (state_.planned && s.day<state_.day)) {
        port.release(army_,ReleaseReason::identity_changed);state_.info.active=false;return MoveStatus::released;
    }
    bool stopped=!s.hostile_city || !s.mission_unchanged;
    for(unsigned i=0;i<s.count;++i)stopped=stopped || s.members[i].original_stop;
    if(stopped) {port.release(army_,ReleaseReason::original_stop);state_.info.active=false;return MoveStatus::released;}
    try {
        State next=state_;MoveBatch batch;
        const auto result=update(port,s,next,batch);
        if(result==MoveStatus::invalid_snapshot) {
            port.release(army_,ReleaseReason::original_stop);state_.info.active=false;return result;
        }
        if(next.info.active && (!batch.orders.empty() || next.info.mode!=state_.info.mode) && !port.publish(s,batch))
            return MoveStatus::native_refusal;
        state_=std::move(next);return result;
    } catch(...) {return MoveStatus::no_route;}
}
MoveStatus MovementController::update(MovementPort &port,const FieldSnapshot &s,State &n,MoveBatch &batch) {
    if(n.info.stage==TravelStage::to_approach) {
        batch.mode=MoveMode::individual;
        if(!n.planned) {
            Approach chosen;
            if(!select_base(port,s,chosen,n.routes,n.endpoints,n.info.approach_region,n.info.approach_hex))return MoveStatus::no_route;
            n.info.approach_region=chosen.region;n.info.approach_hex=chosen.center;
            n.info.rally_center=chosen.center;n.facing=face_toward(chosen.center,s.city_hex);
            n.day=s.day;n.planned=true;
            for(unsigned i=0;i<s.count;++i)batch.orders.push_back({s.members[i].army,n.routes[i],i==0?chosen.region:0});
            return MoveStatus::issued;
        }
        if(!all_at(s,n.endpoints) || !port.owned_by_group(n.info.approach_region))return MoveStatus::waiting;
        n.info.stage=TravelStage::assemble;n.info.mode=MoveMode::assemble;n.planned=false;
        batch.mode=MoveMode::assemble;return MoveStatus::waiting;
    }
    if(n.info.stage==TravelStage::assemble) {
        batch.mode=MoveMode::assemble;
        if(!n.planned) {
            const bool at_base=n.centers.empty();Hex center=no_hex;
            if(!assemble_paths(port,s,n.info.rally_center,n.facing,at_base,n.info.approach_region,n.routes,center))return MoveStatus::no_route;
            n.info.rally_center=center;
            for(unsigned i=0;i<s.count;++i) {
                n.endpoints[i]=formation_hex(center,i,n.facing);
                // Avoid closing a six-member ring while its commander is
                // still outside after a narrow passage/construction detour.
                batch.orders.push_back({s.members[i].army,i && s.members[0].hex!=center?
                    std::vector<Hex>{s.members[i].hex}:n.routes[i],0});
            }
            n.day=s.day;n.planned=s.members[0].hex==center;return MoveStatus::issued;
        }
        if(!all_at(s,n.endpoints))return MoveStatus::waiting;
        n.info.stage=TravelStage::cohesive_approach;n.info.mode=MoveMode::cohesive;n.planned=false;
        n.pending=false;n.credit=0;n.member_credit.fill(0);n.pending_hex.fill(no_hex);n.day=s.day;
    }
    batch.mode=n.info.mode;
    const auto base=slowest_base(s);
    if(!base || !s.flat_cost || s.flat_cost>1000 || !(s.points_per_base_day>0) ||
       !std::isfinite(s.points_per_base_day))return MoveStatus::invalid_snapshot;
    batch.common_base=base;batch.common_cost=s.flat_cost;
    if(!n.planned) {
        if(hex_distance(n.info.rally_center,s.city_hex)<=2) {
            port.release(army_,ReleaseReason::city_assault);n.info.stage=TravelStage::native_combat;n.info.active=false;
            return MoveStatus::finished;
        }
        auto rigid=shared_path(port,s,n.info.rally_center,n.facing,true);
        if(!rigid.cells.empty()) {
            n.centers=std::move(rigid.cells);n.center_index=0;n.info.mode=MoveMode::cohesive;
        } else {
            auto line=shared_path(port,s,n.info.rally_center,n.facing,false);
            if(line.cells.empty() || !column_paths(port,s,line.cells,n.routes,n.endpoints))return MoveStatus::no_route;
            n.centers=std::move(line.cells);n.cursor.fill(0);n.info.mode=MoveMode::column;
        }
        n.planned=true;n.day=s.day;batch.mode=n.info.mode;
    }
    const auto elapsed=static_cast<double>(static_cast<std::int64_t>(s.day)-n.day);
    const double earned=base*s.points_per_base_day*elapsed;
    n.day=s.day;
    if(n.info.mode==MoveMode::cohesive) {
        if(n.pending && all_at(s,n.pending_hex)) {n.pending=false;++n.center_index;n.info.rally_center=n.centers[n.center_index];}
        if(n.center_index+1>=n.centers.size()) {
            port.release(army_,ReleaseReason::city_assault);n.info.stage=TravelStage::native_combat;n.info.active=false;
            return MoveStatus::finished;
        }
        if(!n.pending) {
            n.credit=(std::min)(static_cast<double>(s.flat_cost),n.credit+earned);
            if(n.credit<s.flat_cost)return MoveStatus::waiting;
            const auto next=n.centers[n.center_index+1];
            for(unsigned i=0;i<s.count;++i)n.pending_hex[i]=formation_hex(next,i,n.facing);
            for(unsigned i=0;i<s.count;++i) {
                const auto from=formation_hex(n.centers[n.center_index],i,n.facing),to=n.pending_hex[i];
                if(s.members[i].hex!=from || !port.cell_open(i,to) || !port.edge_open(i,from,to))return MoveStatus::invalid_snapshot;
                for(unsigned j=0;j<i;++j)if(to==formation_hex(n.centers[n.center_index],j,n.facing) &&
                    from==n.pending_hex[j])return MoveStatus::no_route;
            }
            n.credit-=s.flat_cost;n.pending=true;
        }
        for(unsigned i=0;i<s.count;++i) {
            const auto h=s.members[i].hex,target=n.pending_hex[i];
            if(h==target)continue;
            if(h!=formation_hex(n.centers[n.center_index],i,n.facing))return MoveStatus::invalid_snapshot;
            if(vacant(s,target,i) && port.cell_open(i,target) && port.edge_open(i,h,target))
                batch.orders.push_back({s.members[i].army,{h,target},0});
        }
    } else {
        if(all_at(s,n.endpoints)) {
            n.info.rally_center=n.centers.back();n.info.stage=TravelStage::assemble;n.info.mode=MoveMode::assemble;
            n.planned=false;n.pending_hex.fill(no_hex);batch.mode=MoveMode::assemble;batch.common_base=batch.common_cost=0;
            return MoveStatus::waiting;
        }
        for(unsigned i=0;i<s.count;++i) {
            auto &cursor=n.cursor[i];const auto &route=n.routes[i];const auto actual=s.members[i].hex;
            if(cursor+1<route.size() && actual==route[cursor+1]) {++cursor;n.pending_hex[i]=no_hex;}
            if(actual!=route[cursor])return MoveStatus::invalid_snapshot;
            if(cursor+1==route.size())continue;
            if(n.pending_hex[i]>=no_hex)n.member_credit[i]=(std::min)(static_cast<double>(s.flat_cost),n.member_credit[i]+earned);
            const auto target=route[cursor+1];
            if(n.member_credit[i]<s.flat_cost && n.pending_hex[i]>=no_hex)continue;
            if(!vacant(s,target,i) || !port.cell_open(i,target) || !port.edge_open(i,actual,target))continue;
            bool reserved=false;for(const auto &order:batch.orders)if(order.path.back()==target)reserved=true;
            if(reserved)continue;
            if(n.pending_hex[i]>=no_hex) {n.member_credit[i]-=s.flat_cost;n.pending_hex[i]=target;}
            batch.orders.push_back({s.members[i].army,{actual,target},0});
        }
    }
    return batch.orders.empty()?MoveStatus::waiting:MoveStatus::issued;
}
} // namespace combat_v2
