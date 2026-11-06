#pragma once
#include "game_addresses.h"
// Original decoded 1fbe6be7..., confirmed against retained runtime .text.
namespace diplomacy_intel {
const unsigned final_tail=GA(GA_816756),final_resume=GA(GA_81675B),final_cleanup=GA(GA_816806);
static const unsigned char final_tail_bytes[]={0xBA,1,0,0,0}; // one whole MOV EDX,1
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    {GA(GA_810CA0),0x4AA,GH(0xF93E4D48ACB7A0FBull)},
    {GA(GA_810640),0x1CC,GH(0x14FA6C2C2983796Dull)},
    {GA(GA_811210),0x21B,GH(0x0282A4B773813717ull)},
    {GA(GA_815970),0x509,GH(0x907F7119BBC28C6Bull)},
    {GA(GA_815EA0),0xA00,GH(0xE3ED444B9D02F3AAull)},
    {GA(GA_829BD0),0x674,GH(0xE553753EB1B2E1C0ull)},
    {GA(GA_82C510),0x449,GH(0x0922B94B6EDD5BBCull)},
    {GA(GA_82C960),0x3FC,GH(0x76D83CC326E1D30Bull)},
    {GA(GA_82CD60),0x402,GH(0x2974F41CC9CCDAA8ull)},
    {GA(GA_82D170),0x3FC,GH(0x23619AB5A2FD6DA0ull)},
    {GA(GA_82D570),0x307,GH(0x6F0FF5D35548B54Cull)},
    {GA(GA_82D880),0x305,GH(0xAC0290C8CF9368FBull)},
    {GA(GA_82DB90),0x305,GH(0xD5C375E82B7225F7ull)},
    {GA(GA_322B30),0x21F,GH(0x13E1AEB63B0C9CB4ull)},
    {GA(GA_324E20),0xFE,GH(0xE26D07C6C76FABB5ull)},
    {GA(GA_804770),0x36E,GH(0x7D7F4ABABF33F9DEull)},
    {GA(GA_22FCE0),0x4B5,GH(0xB1692F55E1895A25ull)},
    {GA(GA_2337B0),0x59,GH(0xE7CEF884DD150FF1ull)},
    {GA(GA_824880),0x89,GH(0xC2331598513E40A5ull)},
    {GA(GA_806950),0x52,GH(0x4444CED6F456EF3Bull)},
    {GA(GA_841180),0xDB,GH(0x3151C6539772611Bull)},
    {GA(GA_856620),0x17E,GH(0x7401C846799E089Aull)},
    {GA(GA_83BFD0),0x134,GH(0x044BDA4035D33931ull)},
    {GA(GA_85B160),0x7,GH(0xE79A5C99C5896991ull)},
    {GA(GA_85B1A0),0x10,GH(0xB904A9AE37C96A76ull)},
    {GA(GA_85CF80),0x10,GH(0x2497AFADFF8A0A9Eull)},
    {GA(GA_92CEE0),0xA0,GH(0x5F49261B4DED1CCDull)},
    {GA(GA_930D20),0xDC,GH(0xF5883D13105A27C4ull)},
    {GA(GA_92F7B0),0x60,GH(0x81C5EF3F68A59802ull)},
    {GA(GA_92F810),0x77,GH(0x643D23A4F9165456ull)},
    {GA(GA_805010),0x28,GH(0x5E88D2E55819EF51ull)},
#ifndef NO_RUNTIME_LOG
    {GA(GA_3FB600),0x40,GH(0x00272F28DF444A6Full)},
#endif
};
struct Slot {unsigned rva,original;};
static const Slot slots[]={
    {GA(GA_145DBE0),GA(GA_811210)},
    {GA(GA_145DBC0),GA(GA_810CA0)},
    {GA(GA_145DBC8),GA(GA_810640)},
    {GA(GA_145F9C0),GA(GA_805010)},
    {GA(GA_145F260),GA(GA_82C960)},
    {GA(GA_145F268),GA(GA_82D880)},
    {GA(GA_145F448),GA(GA_82C510)},
    {GA(GA_145F450),GA(GA_82D570)},
    {GA(GA_145F818),GA(GA_82CD60)},
    {GA(GA_145F820),GA(GA_82DB90)},
    {GA(GA_145F630),GA(GA_82D170)},
    {GA(GA_145F638),GA(GA_82D570)},
};
struct Site {unsigned rva,original;};
static const Site sites[]={
    {GA(GA_82D688),GA(GA_322B30)},
    {GA(GA_82D998),GA(GA_322B30)},
    {GA(GA_82DCA8),GA(GA_322B30)},
    {GA(GA_8162A2),GA(GA_322B30)},
    {GA(GA_8162E1),GA(GA_804770)},
    {GA(GA_816726),GA(GA_324E20)},
    {GA(GA_81133D),GA(GA_815EA0)},
#ifndef NO_RUNTIME_LOG
    {GA(GA_324E93),GA(GA_3FB600)}, // Original six-argument draw, diagnostic pass-through only.
#endif
};
}
