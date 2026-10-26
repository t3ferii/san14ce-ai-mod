#pragma once
#include <array>
#include <vector>
#include <map>
#include <stdint.h>
#include <algorithm>

// Supplemental relationships. Person+C0's eight native uint16_t values
// remain in place; no part of Person+D0 (dislikes) or later fields is used.
namespace friendship {
constexpr unsigned Capacity=500,NativeCapacity=8,MaxOfficer=6000;
using Binding=std::array<uint8_t,32>;
using Native=std::array<uint16_t,NativeCapacity>;
using NativeReader=Native (*)(unsigned,void *);
enum class AddResult {Added,Existing,Invalid,Full};
inline bool valid_id(unsigned id) {return id>0 && id<=MaxOfficer;}
class Store {
    std::map<unsigned,std::vector<uint16_t>> extra_;
    static uint32_t checksum(const uint8_t *data,size_t size) {
        uint32_t value=~0u;
        for(size_t i=0;i<size;++i) {
            value^=data[i];
            for(unsigned bit=0;bit<8;++bit)value=(value>>1)^(0xEDB88320u&(0u-(value&1u)));
        }
        return ~value;
    }
    static void put16(std::vector<uint8_t> &out,unsigned value) {
        out.push_back(static_cast<uint8_t>(value));out.push_back(static_cast<uint8_t>(value>>8));
    }
    static void put32(std::vector<uint8_t> &out,uint32_t value) {
        for(unsigned i=0;i<4;++i)out.push_back(static_cast<uint8_t>(value>>(i*8)));
    }
    static unsigned get16(const uint8_t *p) {return p[0]|(static_cast<unsigned>(p[1])<<8);}
    static uint32_t get32(const uint8_t *p) {
        return get16(p)|(static_cast<uint32_t>(get16(p+2))<<16);
    }
public:
    void clear() {extra_.clear();}
    void erase_person(unsigned id) {
        extra_.erase(id);
        for(auto row=extra_.begin();row!=extra_.end();) {
            auto &ids=row->second;
            ids.erase(std::remove(ids.begin(),ids.end(),id),ids.end());
            if(ids.empty())row=extra_.erase(row);else ++row;
        }
    }
    std::vector<uint16_t> list(unsigned owner,const Native &native) const {
        std::vector<uint16_t> result;result.reserve(Capacity);
        auto append=[&](uint16_t id) {
            if(valid_id(id) && id!=owner && std::find(result.begin(),result.end(),id)==result.end())
                result.push_back(id);
        };
        for(uint16_t id:native)append(id);
        auto row=extra_.find(owner);
        if(row!=extra_.end())for(uint16_t id:row->second)append(id);
        return result;
    }
    bool contains(unsigned owner,unsigned target,const Native &native) const {
        if(!valid_id(owner) || !valid_id(target) || owner==target)return false;
        if(std::find(native.begin(),native.end(),target)!=native.end())return true;
        auto row=extra_.find(owner);
        return row!=extra_.end() && std::find(row->second.begin(),row->second.end(),target)!=row->second.end();
    }
    AddResult can_add(unsigned owner,unsigned target,const Native &native) const {
        if(!valid_id(owner) || !valid_id(target) || owner==target)return AddResult::Invalid;
        if(contains(owner,target,native))return AddResult::Existing;
        return list(owner,native).size()>=Capacity?AddResult::Full:AddResult::Added;
    }
    AddResult add(unsigned owner,unsigned target,const Native &native) {
        AddResult result=can_add(owner,target,native);
        if(result==AddResult::Added)extra_[owner].push_back(static_cast<uint16_t>(target));
        return result;
    }
    bool add_mutual(unsigned first,unsigned second,const Native &first_native,const Native &second_native) {
        auto a=can_add(first,second,first_native),b=can_add(second,first,second_native);
        if(a==AddResult::Full || b==AddResult::Full || a==AddResult::Invalid || b==AddResult::Invalid)return false;
        // Prepare copies before publication: allocation failure cannot leave
        // a one-way relation from the requested mutual military merit award.
        auto next=extra_;
        if(a==AddResult::Added)next[first].push_back(static_cast<uint16_t>(second));
        if(b==AddResult::Added)next[second].push_back(static_cast<uint16_t>(first));
        extra_.swap(next);return true;
    }
    bool remove(unsigned owner,unsigned target) {
        auto row=extra_.find(owner);if(row==extra_.end())return false;
        auto &ids=row->second;auto at=std::find(ids.begin(),ids.end(),target);
        if(at==ids.end())return false;ids.erase(at);
        if(ids.empty())extra_.erase(row);return true;
    }
    // The binding must be the hash of the exact native save, supplied by a
    // separately verified save/load hook. A global per-scenario file is not
    // sufficient: loading an earlier save must not import a later relation.
    std::vector<uint8_t> encode(const Binding &binding) const {
        std::vector<uint8_t> out={'S','1','4','F','R','5','0','0'};
        put32(out,1);out.insert(out.end(),binding.begin(),binding.end());
        put32(out,static_cast<uint32_t>(extra_.size()));
        for(const auto &row:extra_) {
            put16(out,row.first);put16(out,static_cast<unsigned>(row.second.size()));
            for(uint16_t id:row.second)put16(out,id);
        }
        put32(out,checksum(out.data(),out.size()));return out;
    }
    bool decode(const uint8_t *data,size_t size,const Binding &binding,NativeReader native,void *context) {
        constexpr size_t Header=48,MaxSize=Header+MaxOfficer*(4+Capacity*2)+4;
        const uint8_t magic[]={'S','1','4','F','R','5','0','0'};
        if(!data || !native || size<Header+4 || size>MaxSize ||
           !std::equal(magic,magic+8,data) || get32(data+8)!=1 ||
           !std::equal(binding.begin(),binding.end(),data+12) ||
           checksum(data,size-4)!=get32(data+size-4))return false;
        unsigned rows=get32(data+44);if(rows>MaxOfficer)return false;
        std::map<unsigned,std::vector<uint16_t>> replacement;size_t offset=Header;
        for(unsigned i=0;i<rows;++i) {
            if(offset+4>size-4)return false;
            unsigned owner=get16(data+offset),count=get16(data+offset+2);offset+=4;
            if(!valid_id(owner) || count==0 || count>Capacity || replacement.count(owner) ||
               offset+count*2>size-4)return false;
            auto &ids=replacement[owner];ids.reserve(count);
            for(unsigned j=0;j<count;++j) {
                unsigned id=get16(data+offset);offset+=2;
                if(!valid_id(id) || id==owner || std::find(ids.begin(),ids.end(),id)!=ids.end())return false;
                ids.push_back(static_cast<uint16_t>(id));
            }
        }
        if(offset!=size-4)return false;
        Store next;next.extra_.swap(replacement);
        for(const auto &row:next.extra_)
            if(next.list(row.first,native(row.first,context)).size()>Capacity)return false;
        extra_.swap(next.extra_);return true;
    }
};
}
