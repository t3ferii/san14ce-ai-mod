#pragma once
#include "friendship_save.h"
#include <set>

// One merit-origin record per awarded officer, with a reciprocal ruler view.
// Native slot ownership is persisted only for mod-inserted ruler relations.
namespace merit {
using Binding=friendship::Binding;
constexpr unsigned MaxOfficer=6000,MaxForce=51;
struct Context {unsigned force=0,ruler=0,grade=0;bool alive=false;};
struct Bond {uint16_t officer,ruler;uint8_t force;uint8_t native_slot=0;};
using Reader=Context (*)(unsigned,void *);
using Cleanup=void (*)(const Bond &);
class Store {
    std::map<unsigned,Bond> bonds_;
    std::map<unsigned,std::set<unsigned>> rulers_;
    void erase(std::map<unsigned,Bond>::iterator it,Cleanup clean=nullptr) {
        if(clean)clean(it->second);
        auto index=rulers_.find(it->second.ruler);
        if(index!=rulers_.end()) {index->second.erase(it->first);if(index->second.empty())rulers_.erase(index);}
        bonds_.erase(it);
    }
    static bool valid(const Bond &b,Reader read,void *context) {
        if(!read)return false;
        Context officer=read(b.officer,context),ruler=read(b.ruler,context);
        return officer.alive && ruler.alive && officer.grade==3 &&
            officer.force==b.force && ruler.force==b.force &&
            officer.ruler==b.ruler && ruler.ruler==b.ruler;
    }
    static void put16(std::vector<uint8_t> &out,unsigned v) {
        out.push_back(static_cast<uint8_t>(v));out.push_back(static_cast<uint8_t>(v>>8));
    }
    static void put32(std::vector<uint8_t> &out,uint32_t v) {
        for(unsigned i=0;i<4;++i)out.push_back(static_cast<uint8_t>(v>>(i*8)));
    }
    static unsigned get16(const uint8_t *p) {return p[0]|(static_cast<unsigned>(p[1])<<8);}
    static uint32_t get32(const uint8_t *p) {return get16(p)|(static_cast<uint32_t>(get16(p+2))<<16);}
    static uint32_t checksum(const uint8_t *p,size_t size) {
        uint32_t v=~0u;
        for(size_t i=0;i<size;++i) {
            v^=p[i];for(unsigned bit=0;bit<8;++bit)v=(v>>1)^(0xEDB88320u&(0u-(v&1u)));
        }
        return ~v;
    }
public:
    static constexpr size_t MaxEncodedSize=52+MaxOfficer*6;
    void clear() {bonds_.clear();rulers_.clear();}
    size_t size() const {return bonds_.size();}
    const Bond *bond(unsigned officer) const {auto it=bonds_.find(officer);return it==bonds_.end()?nullptr:&it->second;}
    template<class Visit> void each(Visit visit) {for(auto &row:bonds_)visit(row.second);}
    void erase_person(unsigned id,Cleanup clean=nullptr) {
        auto own=bonds_.find(id);if(own!=bonds_.end())erase(own,clean);
        auto index=rulers_.find(id);
        if(index!=rulers_.end()) {auto ids=index->second;for(unsigned officer:ids)erase(bonds_.find(officer),clean);}
    }
    void prune(Reader read,void *context,Cleanup clean=nullptr) {
        for(auto it=bonds_.begin();it!=bonds_.end();)
            if(!valid(it->second,read,context)) {auto old=it++;erase(old,clean);}else ++it;
    }
    void prune_pair(unsigned owner,unsigned other,Reader read,void *context,Cleanup clean=nullptr) {
        for(unsigned id:{owner,other}) {
            auto it=bonds_.find(id);
            if(it!=bonds_.end() && (it->second.ruler==owner || it->second.ruler==other) && !valid(it->second,read,context))erase(it,clean);
        }
    }
    void prune_person(unsigned id,Reader read,void *context,Cleanup clean=nullptr) {
        auto owner=bonds_.find(id);
        if(owner!=bonds_.end() && !valid(owner->second,read,context))erase(owner,clean);
        auto index=rulers_.find(id);
        if(index!=rulers_.end()) {auto ids=index->second;for(unsigned officer:ids) {
            auto it=bonds_.find(officer);if(!valid(it->second,read,context))erase(it,clean);
        }}
    }
    bool award(unsigned officer,unsigned previous_grade,Reader read,void *context) {
        if(!friendship::valid_id(officer) || previous_grade>=3 || !read)return false;
        Context now=read(officer,context);
        if(!friendship::valid_id(now.ruler) || officer==now.ruler ||
            now.force==0 || now.force>MaxForce)return false;
        Bond bond{static_cast<uint16_t>(officer),static_cast<uint16_t>(now.ruler),
                  static_cast<uint8_t>(now.force)};
        if(!valid(bond,read,context))return false;
        auto old=bonds_.find(officer);
        auto index=rulers_.find(now.ruler);
        if((old==bonds_.end() || old->second.ruler!=now.ruler) && index!=rulers_.end() && index->second.size()>=500)return false;
        if(old!=bonds_.end()) {
            if(old->second.ruler==bond.ruler)bond.native_slot=old->second.native_slot;
            erase(old);
        }
        // Publication of one record establishes both directions together.
        bonds_.insert_or_assign(officer,bond);rulers_[bond.ruler].insert(officer);return true;
    }
    void set_native_slot(unsigned officer,unsigned slot) {auto it=bonds_.find(officer);if(it!=bonds_.end() && slot<=8)it->second.native_slot=static_cast<uint8_t>(slot);}
    bool contains(unsigned owner,unsigned other,Reader read,void *context) const {
        if(!friendship::valid_id(owner) || !friendship::valid_id(other) || owner==other)return false;
        auto it=bonds_.find(owner);
        if(it!=bonds_.end() && it->second.ruler==other && valid(it->second,read,context))return true;
        it=bonds_.find(other);
        return it!=bonds_.end() && it->second.ruler==owner && valid(it->second,read,context);
    }
    std::vector<uint16_t> list(unsigned owner,Reader read,void *context) const {
        std::vector<uint16_t> ids;
        if(!friendship::valid_id(owner))return ids;
        auto it=bonds_.find(owner);
        if(it!=bonds_.end() && valid(it->second,read,context))ids.push_back(it->second.ruler);
        auto index=rulers_.find(owner);
        if(index!=rulers_.end())for(unsigned officer:index->second) {
            const Bond &b=bonds_.at(officer);if(valid(b,read,context))ids.push_back(b.officer);
        }
        return ids;
    }
    std::vector<uint8_t> encode(const Binding &hash) const {
        std::vector<uint8_t> out={'S','1','4','M','B','O','N','D'};
        put32(out,2);out.insert(out.end(),hash.begin(),hash.end());put32(out,static_cast<uint32_t>(bonds_.size()));
        for(const auto &row:bonds_) {
            const Bond &b=row.second;put16(out,b.officer);put16(out,b.ruler);
            out.push_back(b.force);out.push_back(b.native_slot);
        }
        put32(out,checksum(out.data(),out.size()));return out;
    }
    bool decode(const uint8_t *p,size_t size,const Binding &hash) {
        const uint8_t magic[]={'S','1','4','M','B','O','N','D'};
        if(!p || size<52 || size>MaxEncodedSize || !std::equal(magic,magic+8,p) ||
            (get32(p+8)!=1 && get32(p+8)!=2) || !std::equal(hash.begin(),hash.end(),p+12) ||
            checksum(p,size-4)!=get32(p+size-4))return false;
        unsigned count=get32(p+44);
        if(count>MaxOfficer || size!=52+static_cast<size_t>(count)*6)return false;
        std::map<unsigned,Bond> replacement;
        for(unsigned i=0;i<count;++i) {
            const uint8_t *row=p+48+i*6;
            Bond b{static_cast<uint16_t>(get16(row)),static_cast<uint16_t>(get16(row+2)),row[4]};
            if(!friendship::valid_id(b.officer) || !friendship::valid_id(b.ruler) ||
                  b.officer==b.ruler || b.force==0 || b.force>MaxForce || row[5]>8 || (get32(p+8)==1 && row[5]!=0) ||
                !replacement.emplace(b.officer,b).second)return false;
            replacement.at(b.officer).native_slot=row[5];
        }
        std::map<unsigned,std::set<unsigned>> index;
        for(const auto &row:replacement)index[row.second.ruler].insert(row.first);
        bonds_.swap(replacement);rulers_.swap(index);return true;
    }
};
using SaveCoordinator=friendship::BasicSaveCoordinator<Store>;
}
