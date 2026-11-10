// Generated from the SHA-256 checked CE plain copy.
#pragma once
#include "game_addresses.h"
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    {GA(GA_1FA530),0x8A,GH(0x7B57E80254C04EE9ull)},
    {GA(GA_1FB170),0x1363,GH(0x9535F607A84BFC7Bull)},
    {GA(GA_1ECD20),0x21C,GH(0xBCABEE4B3A6A8167ull)},
    {GA(GA_1F5680),0x92,GH(0x07F3CBA83C476463ull)},
    {GA(GA_296AA0),0x5C0,GH(0x453521D2C78D6EE9ull)},
    {GA(GA_22B8E0),0x2C,GH(0x5520AF45E33CAAA0ull)},
    {GA(GA_2CC630),0x30,GH(0xAE2882DA8EB1870Eull)},
    {GA(GA_2C7A00),0x86,GH(0xA5247E8372095055ull)},
    {GA(GA_22E450),0x75,GH(0x0421AA7E267EE1E5ull)},
};
static unsigned guard_failed_row;   // index of the first guard whose hash differed (diagnostic only)
// local11 (docs/HISTORY.md section 18): row 3 (GA_1F5680, the durability setter this feature only CALLS) is also traced by San14Sim's
// version.dll, which replaces its first 5 bytes by `E9 rel32` leaving the image (local10 game log: building_destruction_guard_failed 3). The row
// hash is therefore computed by ga::hash_ok: the plain hash, or the hash with an OBSERVED prefix put back to the original bytes (a patch inside the
// game, other padding, or any changed byte behind the prefix still fails). Feature code 50 = building_destruction_all (the addr_unresolved gate id).
static bool guards_match() {
    unsigned row=0;
    for(const auto &g:guards) {
        ga::ObserverSet used;
        if(!ga::hash_ok(image,g.rva,g.size,g.hash,&used)) {guard_failed_row=row;return false;}
        for(unsigned k=0;k<used.count;++k)log_event("observer_hook_tolerated",50,used.rva[k]);
        ++row;
    }
    return true;
}
// local10 G2 3.2 (docs/HISTORY.md): shape of the final-damage capture point GA_1FB9E6 (plain memcmp; the 1F9DE0 guard hash pins
// the whole function): two ADDs, `mov [rbp-70],edx`, `mov [rbp-30],r12d` (the CALL replaces the first five ADD bytes, the sixth byte becomes
// a NOP; the bridge replays both ADDs first). The capture form of the previous build was removed at the 1a4a447d re-base (W1-F).
static constexpr unsigned char capture_shape[]={0x03,0x55,0x90,0x03,0x55,0xE8,0x89,0x55,0x90,0x44,0x89,0x65,0xD0};
static bool capture_shape_ok() {
    const unsigned site=GA(GA_1FB9E6);
    return site && !memcmp(image+site,capture_shape,sizeof(capture_shape));
}
