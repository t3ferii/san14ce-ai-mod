// Generated from the SHA-256 checked CE plain copy.
#pragma once
#include "game_addresses.h"
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    {GA(GA_1F91A0),0x8A,GH(0x9049AF136676CEB0ull)},
    {GA(GA_1F9DE0),0x1363,GH(0x30373A53223F4916ull)},
    {GA(GA_1EB980),0x21C,GH(0xC74D90B0C2A44B5Dull)},
    {GA(GA_1F42B0),0x92,GH(0xCD4CCE4E0E924056ull)},
    {GA(GA_294B90),0x5C0,GH(0xAA079D3E52482679ull)},
    {GA(GA_229EA0),0x2C,GH(0xA2E199FE100A4E74ull)},
    {GA(GA_2CA280),0x30,GH(0x7221823C76F2F29Full)},
    {GA(GA_2C5650),0x86,GH(0x1713BE6413D071A6ull)},
    {GA(GA_22CA00),0x75,GH(0x880B7C7EC5215155ull)},
};
static unsigned guard_failed_row;   // index of the first guard whose hash differed (diagnostic only)
// local11 (docs/INSTALL_LOCAL2_TRIAGE.md section 18): row 3 (GA_1F42B0, the durability setter this feature only CALLS) is also traced by San14Sim's
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
// local10 G2 3.2 (docs/LOCAL10_PORT_PLAN.md): shape of the final-damage capture point GA_1FA607 (plain memcmp; the 1F9DE0 guard hash pins
// the whole function). 1 = 1fbe6be7 `mov [rbp-10],edx; mov r12d,[rsp+78]`, 2 = 1a4a447d two ADDs, `mov [rbp-70],edx`, `mov [rbp-30],r12d`
// (the CALL replaces the first five ADD bytes, the sixth byte becomes a NOP; the bridge replays both ADDs first), 0 = unknown.
static constexpr unsigned char capture_form_a[]={0x89,0x55,0xF0,0x44,0x8B,0x64,0x24,0x78};
static constexpr unsigned char capture_form_b[]={0x03,0x55,0x90,0x03,0x55,0xE8,0x89,0x55,0x90,0x44,0x89,0x65,0xD0};
static unsigned capture_form() {
    const unsigned site=GA(GA_1FA607);
    if(!site)return 0;
    if(!memcmp(image+site,capture_form_a,sizeof(capture_form_a)))return 1;
    if(!memcmp(image+site,capture_form_b,sizeof(capture_form_b)))return 2;
    return 0;
}
