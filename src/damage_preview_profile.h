// Generated from the verified CE plain copy; do not hand-edit addresses.
#pragma once
#include "game_addresses.h"
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    {GA(GA_7FA230),0x533,GH(0xFA0E6E9FA81368AEull)},
    {GA(GA_7D0460),0x17D,GH(0x27A85A72FD78E104ull)},
    {GA(GA_7F55D0),0x1C5,GH(0x8999CD3A739DF67Dull)},
    {GA(GA_16A1D0),0x5B0,GH(0x6258DD38B405FA38ull)},
    {GA(GA_2B21A0),0x2A0,GH(0x3DAA963C35BDFC51ull)},
    {GA(GA_2B2440),0x244,GH(0xA61EC6CEBEFA084Aull)},
    {GA(GA_2A9E50),0x2D3,GH(0x7E749534421219C6ull)},
    {GA(GA_2AA2E0),0x283,GH(0x28D3F3D39B51787Full)},
    {GA(GA_18FDA0),0x15D,GH(0x43FA58B396A6750Cull)},
    {GA(GA_92F810),0x76,GH(0x05424D9381749391ull)},
    {GA(GA_22C6D0),0x53,GH(0xE8370CA5854F2A1Full)},
    {GA(GA_191760),0x4C8,GH(0xA71EAF6B3CE0D745ull)},
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
