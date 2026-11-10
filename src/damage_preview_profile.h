// Generated from the verified CE plain copy; do not hand-edit addresses.
#pragma once
#include "game_addresses.h"
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    {GA(GA_7FC700),0x533,GH(0x553866FDBFA88EEAull)},
    {GA(GA_7D2810),0x17D,GH(0xF9449107F862AA21ull)},
    {GA(GA_7F7AA0),0x1C5,GH(0xBD6415CC11811600ull)},
    {GA(GA_16B180),0x5B0,GH(0x95F2647C654B973Dull)},
    {GA(GA_2B4460),0x2A0,GH(0xE22FB9DD8D3431D4ull)},
    {GA(GA_2B4750),0x244,GH(0x3D36A2090EC06DE7ull)},
    {GA(GA_2AC0A0),0x2D3,GH(0x7A69ED6C468793F0ull)},
    {GA(GA_2AC5A0),0x283,GH(0x99563BCBA92BD324ull)},
    {GA(GA_191130),0x15D,GH(0x980696A7D579BDB6ull)},
    {GA(GA_931DA0),0x76,GH(0x2914D5583B28ED06ull)},
    {GA(GA_22E110),0x53,GH(0xDEB53A37DA8F9528ull)},
    {GA(GA_192AF0),0x4C8,GH(0xECD26F395DA09FECull)},
};
// local10 G4 5.1: lives here (not in the .inl) so tests/address_guard_fixture.cpp can run the very same guard against both builds.
static inline bool guards_match() {
    for(const auto &g:guards) {
        unsigned long long h=0xCBF29CE484222325ull;
        for(unsigned i=0;i<g.size;++i)h=(h^image[g.rva+i])*0x100000001B3ull;
        if(h!=g.hash)return false;
    }
    return true;
}
