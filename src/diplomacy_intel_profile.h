#pragma once
#include "game_addresses.h"
// Original (근거: 1a4a447d 이전 기록, docs/HISTORY.md), confirmed against retained runtime .text.
namespace diplomacy_intel {
const unsigned final_tail=GA(GA_818EC6),final_resume=GA(GA_818ECB),final_cleanup=GA(GA_818F76);
static const unsigned char final_tail_bytes[]={0xBA,1,0,0,0}; // one whole MOV EDX,1
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    {GA(GA_8133D0),0x4AA,GH(0x3FBFA1F2F5E1B5B5ull)},
    {GA(GA_812D70),0x1CC,GH(0x288CA184FE3FAFE9ull)},
    {GA(GA_813940),0x21B,GH(0xC17F8E7E0A6F4AB3ull)},
    {GA(GA_8180C0),0x509,GH(0x1893A8E667F8319Dull)},
    {GA(GA_818600),0xA00,GH(0xE3C884A87CC0638Eull)},
    {GA(GA_82C240),0x674,GH(0x65A79D2564DDB752ull)},
    {GA(GA_82EB90),0x449,GH(0x8A62A706F13EEBB1ull)},
    {GA(GA_82F030),0x3FC,GH(0xDB253558E6228B9Cull)},
    {GA(GA_82F490),0x402,GH(0x009B99142026F40Aull)},
    {GA(GA_82F8F0),0x3FC,GH(0x886CEC5E709937F3ull)},
    {GA(GA_830410),0x307,GH(0x37F5557A7993EB9Cull)},
    {GA(GA_82FD50),0x305,GH(0x8D53E3ADA4EC3DF7ull)},
    {GA(GA_8300B0),0x305,GH(0xC5FB044EBC26AA4Cull)},
    {GA(GA_324BC0),0x21F,GH(0xED681F2C42523980ull)},
    {GA(GA_326ED0),0xFE,GH(0x5296296C509DCECFull)},
    {GA(GA_806DA0),0x36E,GH(0xB73B0B8E87B01B74ull)},
    {GA(GA_231770),0x4B5,GH(0x6DAE62DCF48ACB3Bull)},
    {GA(GA_235150),0x59,GH(0x92EE204438A18200ull)},
    {GA(GA_826F70),0x89,GH(0x8DB4B8E861AD9B7Cull)},
    {GA(GA_809060),0x52,GH(0xECC636E43F2B9CD3ull)},
    {GA(GA_843680),0xDB,GH(0x3151C6539772611Bull)},
    {GA(GA_858910),0x17E,GH(0x92BDE9D5CED9AC82ull)},
    {GA(GA_83E4A0),0x134,GH(0x0268B9BBDA33271Aull)},
    {GA(GA_85D490),0x7,GH(0xE79A5C99C5896991ull)},
    {GA(GA_85D4D0),0x10,GH(0xB904A9AE37C96A76ull)},
    {GA(GA_85F2B0),0x10,GH(0x2497AFADFF8A0A9Eull)},
    {GA(GA_92F390),0xA0,GH(0xBFFB2AB655CAD528ull)},
    {GA(GA_9332B0),0xDC,GH(0xB9B034AAFC67EA57ull)},
    {GA(GA_931D40),0x60,GH(0xD756A57C9E6DDC07ull)},
    {GA(GA_931DA0),0x77,GH(0xF75449EC868C05BFull)},
    {GA(GA_807710),0x28,GH(0x2FF5498767FD9756ull)},
#ifndef NO_RUNTIME_LOG
    {GA(GA_3FDAF0),0x40,GH(0x0E4B260EF2CB0585ull)},
#endif
};
struct Slot {unsigned rva,original;};
static const Slot slots[]={
    {GA(GA_1460C70),GA(GA_813940)},
    {GA(GA_1460C50),GA(GA_8133D0)},
    {GA(GA_1460C58),GA(GA_812D70)},
    {GA(GA_1462A50),GA(GA_807710)},
    {GA(GA_14622F0),GA(GA_82F030)},
    {GA(GA_14622F8),GA(GA_82FD50)},
    {GA(GA_14624D8),GA(GA_82EB90)},
    {GA(GA_14624E0),GA(GA_830410)},
    {GA(GA_14628A8),GA(GA_82F490)},
    {GA(GA_14628B0),GA(GA_8300B0)},
    {GA(GA_14626C0),GA(GA_82F8F0)},
    {GA(GA_14626C8),GA(GA_830410)},
};
struct Site {unsigned rva,original;};
static const Site sites[]={
    {GA(GA_830583),GA(GA_324BC0)},
    {GA(GA_82FEC3),GA(GA_324BC0)},
    {GA(GA_830223),GA(GA_324BC0)},
    {GA(GA_818A0E),GA(GA_324BC0)},
    {GA(GA_818A4D),GA(GA_806DA0)},
    {GA(GA_818E96),GA(GA_326ED0)},
    {GA(GA_813A6D),GA(GA_818600)},
#ifndef NO_RUNTIME_LOG
    {GA(GA_326F43),GA(GA_3FDAF0)}, // Original six-argument draw, diagnostic pass-through only.
#endif
};
}
