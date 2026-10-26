#pragma once
#include <stdint.h>
namespace campaign {
enum Role : unsigned char {front,middle,support,siege};
enum Phase : unsigned char {approach,rally,prepare,assault};
inline unsigned reserve(unsigned stock,unsigned threat) {(void)threat;return stock<4000?stock:4000;}
inline unsigned available(unsigned stock) {return stock-reserve(stock,0);}
inline bool siege_departure(unsigned available_troops,unsigned target_troops) {
    return available_troops>=40000 ||
        (available_troops>=20000 && static_cast<uint64_t>(target_troops)<=static_cast<uint64_t>(available_troops)+5000);
}
inline bool enough(uint64_t attack,uint64_t enemy) {return attack && attack*2>=enemy*3;}
inline bool assaults(Role role,unsigned troops) {return role==front || role==siege || (role==middle && troops>=5000);}
inline bool required(Role role,unsigned troops) {return role==front || role==siege || (role==middle && troops>=5000);}
}
