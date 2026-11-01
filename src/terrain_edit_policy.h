#pragma once
#include <array>
#include <vector>
#include <stdint.h>
#include <string.h>

namespace terrain_edit {
constexpr unsigned HexCount=48400, MaxChanges=4096;
constexpr size_t HeaderSize=42, MaxEncodedSize=HeaderSize+MaxChanges*4;
using Binding=std::array<uint8_t,32>;
// Stable rejection codes used by terrain_edit_reject. Unchanged is a no-op.
enum class Result : unsigned { Applied=0, Disabled=1, HexRange=2, Terrain=3,
    Unchanged=4, Occupied=5, Full=6, Conflict=7, MissingTile=8 };
inline bool allowed(unsigned terrain) {
    return (terrain>=1 && terrain<=14) || (terrain>=19 && terrain<=21) || terrain==24;
}
inline Result validate(bool active,unsigned hex,unsigned original,unsigned next,bool occupied) {
    if(!active)return Result::Disabled;
    if(hex>=HexCount)return Result::HexRange;
    if(!allowed(original) || !allowed(next))return Result::Terrain;
    if(original==next)return Result::Unchanged;
    if(occupied)return Result::Occupied;
    return Result::Applied;
}
struct Change {uint16_t hex;uint8_t next,original;};
class Changes {
    std::array<Change,MaxChanges> entries_={};
    unsigned count_=0;
public:
    unsigned size() const {return count_;}
    const Change &operator[](unsigned index) const {return entries_[index];}
    void clear() {count_=0;}
    Result edit(bool active,unsigned hex,unsigned current,unsigned next,bool occupied) {
        Result result=validate(active,hex,current,next,occupied);
        if(result!=Result::Applied)return result;
        unsigned i=0;while(i<count_ && entries_[i].hex!=hex)++i;
        if(i<count_) {
            if(current!=entries_[i].next)return Result::Conflict;
            if(next==entries_[i].original) {entries_[i]=entries_[--count_];}
            else entries_[i].next=static_cast<uint8_t>(next);
        } else {
            if(count_==MaxChanges)return Result::Full;
            entries_[count_++]={static_cast<uint16_t>(hex),static_cast<uint8_t>(next),static_cast<uint8_t>(current)};
        }
        return Result::Applied;
    }
    // Load may find native tiles already serialized with the new value. Do not
    // apply the UI occupancy rule: units/buildings may have arrived after edit.
    template<class Tile> unsigned restore(const Changes &saved,Tile tile,unsigned &failed) {
        clear();failed=0;
        for(unsigned i=0;i<saved.size();++i) {
            const Change &c=saved[i];uint8_t *value=tile(c.hex);
            if(!value || (*value!=c.original && *value!=c.next)) {++failed;continue;}
            entries_[count_++]=c;*value=c.next;
        }
        return count_;
    }
    // Drop entries whose live tile no longer holds our value (e.g. a new game
    // reinitialised the map without a load host); notify(change,current) each.
    template<class Tile,class Notify> unsigned reconcile(Tile tile,Notify notify) {
        unsigned dropped=0;
        for(unsigned i=0;i<count_;) {
            const Change c=entries_[i];uint8_t *value=tile(c.hex);
            if(value && *value==c.next){++i;continue;}
            entries_[i]=entries_[--count_];++dropped;notify(c,value?static_cast<unsigned>(*value):static_cast<unsigned>(c.original));
        }
        return dropped;
    }
    template<class Tile> void rollback(Tile tile) {
        for(unsigned i=0;i<count_;++i) {
            const Change &c=entries_[i];uint8_t *value=tile(c.hex);
            if(value && *value==c.next)*value=c.original;
        }
        clear();
    }
    std::vector<uint8_t> encode(const Binding &binding) const {
        std::vector<uint8_t> bytes(HeaderSize+count_*4);
        memcpy(bytes.data(),"TERR\1\0\0\0",8);memcpy(bytes.data()+8,binding.data(),32);
        bytes[40]=static_cast<uint8_t>(count_);bytes[41]=static_cast<uint8_t>(count_>>8);
        for(unsigned i=0;i<count_;++i) {
            const Change &c=entries_[i];size_t at=HeaderSize+i*4;
            bytes[at]=static_cast<uint8_t>(c.hex);bytes[at+1]=static_cast<uint8_t>(c.hex>>8);
            bytes[at+2]=c.next;bytes[at+3]=c.original;
        }
        return bytes;
    }
    bool decode(const uint8_t *bytes,size_t size,const Binding &binding) {
        if(!bytes || size<HeaderSize || size>MaxEncodedSize ||
           memcmp(bytes,"TERR\1\0\0\0",8) || memcmp(bytes+8,binding.data(),32))return false;
        unsigned count=bytes[40]|unsigned(bytes[41])<<8;
        if(count>MaxChanges || size!=HeaderSize+count*4)return false;
        Changes replacement;std::array<bool,HexCount> seen={};
        for(unsigned i=0;i<count;++i) {
            size_t at=HeaderSize+i*4;unsigned hex=bytes[at]|unsigned(bytes[at+1])<<8;
            unsigned next=bytes[at+2],original=bytes[at+3];
            if(hex>=HexCount || seen[hex] || !allowed(next) || !allowed(original) || next==original)return false;
            seen[hex]=true;replacement.entries_[replacement.count_++]={static_cast<uint16_t>(hex),static_cast<uint8_t>(next),static_cast<uint8_t>(original)};
        }
        *this=replacement;return true;
    }
};
}
