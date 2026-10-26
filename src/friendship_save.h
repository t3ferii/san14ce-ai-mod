#pragma once
#include "friendship_store.h"
#include <string>

namespace friendship {
// Used under the adapter's lock. Worker FileRead/FileWrite observations are
// bound to the corresponding native host job, never to a global scenario file.
template<class Stored> class BasicSaveCoordinator {
public:
    struct Completed {std::string name;Binding binding;Stored store;};
private:
    uint64_t serial_=0,save_epoch_=0,load_epoch_=0;
    std::string save_name_,load_name_;
    Stored snapshot_;
    Binding save_hash_={},load_hash_={};
    bool save_observed_=false,load_observed_=false;
public:
    static bool valid_name(const std::string &name) {
        if(name.size()<5 || name.size()>64 || name.substr(name.size()-4)!=".s14")return false;
        for(size_t i=0;i<name.size()-4;++i) {
            unsigned char c=static_cast<unsigned char>(name[i]);
            if(!((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_'))return false;
        }
        return true;
    }
    uint64_t begin_save(const std::string &name,Stored store) {
        ++serial_;save_epoch_=0;load_epoch_=0;save_observed_=false;load_observed_=false;
        if(!valid_name(name))return 0;
        save_name_=name;snapshot_=std::move(store);save_epoch_=serial_;return serial_;
    }
    uint64_t begin_load(const std::string &name) {
        ++serial_;save_epoch_=0;load_epoch_=0;save_observed_=false;load_observed_=false;
        if(!valid_name(name))return 0;
        load_name_=name;load_epoch_=serial_;return serial_;
    }
    uint64_t save_token(const std::string &name) const {
        return save_epoch_==serial_ && name==save_name_?save_epoch_:0;
    }
    uint64_t load_token(const std::string &name) const {
        return load_epoch_==serial_ && name==load_name_?load_epoch_:0;
    }
    bool observe_save(uint64_t epoch,const std::string &name,const Binding &hash,bool success) {
        if(!epoch || epoch!=save_token(name) || !success)return false;
        save_hash_=hash;save_observed_=true;return true;
    }
    bool observe_load(uint64_t epoch,const std::string &name,const Binding &hash,bool complete) {
        if(!epoch || epoch!=load_token(name) || !complete)return false;
        load_hash_=hash;load_observed_=true;return true;
    }
    bool finish_save(uint64_t epoch,bool success,Completed &out) {
        if(!epoch || epoch!=save_epoch_ || epoch!=serial_)return false;
        save_epoch_=0;
        if(!success || !save_observed_)return false;
        out={save_name_,save_hash_,std::move(snapshot_)};return true;
    }
    bool finish_load(uint64_t epoch,bool success,Completed &out) {
        if(!epoch || epoch!=load_epoch_ || epoch!=serial_)return false;
        load_epoch_=0;
        if(!success || !load_observed_)return false;
        out={load_name_,load_hash_,{}};return true;
    }
};
using SaveCoordinator=BasicSaveCoordinator<Store>;
}
