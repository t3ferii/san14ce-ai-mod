#pragma once
#include "game_addresses.h"
// Decoded CE KO (근거: 1a4a447d 이전 기록, docs/HISTORY.md); retained .text independently checked by the fixture.
namespace advisor_briefing {
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    // User 2026-11-02: headless final report; exact caller and native data ABI.
    {GA(GA_674020),0xB5,GH(0x737121B16692C076ull)},
    {GA(GA_6793CD),0x5,GH(0x4518FFC00815F7DEull)},
    {GA(GA_67D540),0xC4,GH(0xBE8F06CF7F3EE82Full)},
    {GA(GA_222E30),0x84,GH(0xD15ECA74E5757839ull)},
    {GA(GA_223AD0),0x20,GH(0x3AFD45DC2CA5F084ull)},
    {GA(GA_9314A0),0x84,GH(0x3A1ABA67AACB5122ull)},
    {GA(GA_92F390),0x2A,GH(0xD3DEE020886CCAA8ull)},
    {GA(GA_306A30),0xB9,GH(0xE2C1F972B2A0A893ull)},
    {GA(GA_4541E0),0xF,GH(0xA6A6142BA09CFD46ull)},
    {GA(GA_4421F0),0xC,GH(0xA5611762ADFDE513ull)},
    {GA(GA_844F70),0x30,GH(0x0696A77B498C0425ull)},
    {GA(GA_837710),0x30,GH(0x045E9CF4B5EBD11Full)},
    {GA(GA_6C6B10),0x92,GH(0x78D0D5A298A64342ull)},
    {GA(GA_6C6DD0),0x92,GH(0x82A091CE73565A9Full)},
    {GA(GA_41F8C0),0xCB,GH(0xB70122A792C1219Eull)},
    {GA(GA_40060),0x2A,GH(0x34EDAC209D392918ull)},
    {GA(GA_935F0),0x2D,GH(0xFF30B2842B9C2E18ull)},
    {GA(GA_1F1E90),0x2FA,GH(0x31AB3508B5BB0C45ull)},
    {GA(GA_1F1DA0),0xE9,GH(0x33C6CB6F805140B1ull)},
    {GA(GA_1F2190),0x1D4,GH(0xDBF2388C63688455ull)},
    {GA(GA_335110),0x39,GH(0x16B534D88DF2511Aull)},
    {GA(GA_44C340),0x30,GH(0xD25C0C429C3F6802ull)},
    {GA(GA_44B090),0x30,GH(0xFA4BBD2C9272C8CCull)},
    {GA(GA_44ACC0),0x30,GH(0x1969650DF80458FEull)},
    {GA(GA_865170),0x30,GH(0x144147F3D6F7C85Bull)},
    {GA(GA_1F29B0),0x30,GH(0xAFBA45C88E3216CDull)},
    {GA(GA_4F99D0),0x30,GH(0x3E1F0743FF071052ull)},
    {GA(GA_67DB40),0x30,GH(0xEB2927F21C8B3C47ull)},
    {GA(GA_67DD20),0x30,GH(0x6FE25D4F6E3204E4ull)},
    {GA(GA_67D8A0),0x30,GH(0xFDC901BAA8892681ull)},
    {GA(GA_67F0B0),0x30,GH(0x4EB7B1CC4CFF3FD3ull)},
    {GA(GA_678F30),0x30,GH(0x458D80B2DB8D7B84ull)},
    {GA(GA_67A430),0x30,GH(0x4F2DB8DDF8B5350Full)},
    {GA(GA_671E90),0x30,GH(0x2D1B77E922AB0EBEull)},
    {GA(GA_88E4B0),0x30,GH(0xEB98543885CA6897ull)},
    {GA(GA_88F890),0x30,GH(0x9BE2533CF444F51Cull)},
    {GA(GA_243140),0x30,GH(0x23482F0BFF727A28ull)},
    {GA(GA_92EFC0),0x30,GH(0x859E8B9133D235DAull)},
    {GA(GA_92F440),0x30,GH(0x2BE0A085C7A3A46Full)},
    {GA(GA_9332B0),0x30,GH(0xD6EC5BA439C4F3CCull)},
    {GA(GA_5726B0),0x3F,GH(0x9F11875143756B9Bull)},
    {GA(GA_572480),0x30,GH(0x221409446F18F1ADull)},
    {GA(GA_330B50),0x30,GH(0x72BDE4D4E92C54E7ull)},
    {GA(GA_2350F0),0x30,GH(0xDDFDA12F6622C7E2ull)},
    {GA(GA_22F650),0x30,GH(0xCE305C5B27DB6925ull)},
    {GA(GA_876AD0),0x30,GH(0x4AF87CE7A9E9C780ull)},
    {GA(GA_11040),0x30,GH(0x66627F75D3A2ECDEull)},
    {GA(GA_124A0),0x30,GH(0x748089E0B3EAAC89ull)},
    {GA(GA_8637B0),0x28,GH(0x79A1DCBCB9548E43ull)},
    {GA(GA_865520),0x79,GH(0x14B8986E118B4A5Eull)},
    {GA(GA_92F050),0xDB,GH(0x83C5D3296505F6DAull)},
    {GA(GA_67F910),0x12B2,GH(0x6E1099CD13AF9FFDull)},
    {GA(GA_30F010),0x30,GH(0xC85B7D9D7944997Full)},
    {GA(GA_812FF0),0xA1,GH(0xFB907615E14E6724ull)},
    // 81133D's five-byte CALL belongs to diplomacy_intel. Guard its owner
    // separately so an already-published local counterturn is compatible.
    {GA(GA_813940),0x12D,GH(0x4130C31D1E9E11D9ull)},
    {GA(GA_813A72),0xE9,GH(0x6EE877F21B2F39A0ull)},
    {GA(GA_843B60),0xCD,GH(0x8AB91C3DF4384075ull)},
    {GA(GA_83E300),0x76,GH(0x8D2780573573FD26ull)},
    {GA(GA_8636B0),0xC7,GH(0xF34539001EC9251Dull)},
    {GA(GA_864260),0x243,GH(0xF8454AD2EE500069ull)},
    {GA(GA_8655A0),0x70,GH(0x8458E8F2384667B8ull)},
    {GA(GA_865FC0),0x81,GH(0x1874890D4CABFCF9ull)},
    {GA(GA_44ED30),0x5B4,GH(0x7F6577E4FD08C879ull)},
    {GA(GA_230750),0x30,GH(0xB85C8C95763E1ADCull)},
    {GA(GA_22E450),0x30,GH(0x5DED0658BDA83FB0ull)},
    {GA(GA_7360C0),0x442,GH(0xDF29E6C290A042DEull)},
    {GA(GA_844860),0x1E6,GH(0xD38A14EC122A25B8ull)},
    {GA(GA_230950),0x30,GH(0x64581DB2C14B06E4ull)},
    {GA(GA_1F2A60),0xA1,GH(0x7F065698BD9EEE8Eull)},
    {GA(GA_863780),0x28,GH(0xF81F2268F099305Bull)},
    {GA(GA_863080),0x7F,GH(0x1F0CF2C8422B3606ull)},
    {GA(GA_846510),0xC3,GH(0x1A203E2E6A08E7BDull)},
};
// Whole instructions, no RIP-relative operands; relay returns at entry+size.
static const unsigned char message_prologue[]={0x48,0x8B,0xC4,0x55,0x48,0x8D,0x68,0xA1};
static const unsigned char narration_prologue[]={0x48,0x8B,0xC4,0x57,0x48,0x81,0xEC,0x80,0,0,0};
static const unsigned char person_prologue[]={0x48,0x8B,0xC4,0x55,0x48,0x8D,0x68,0xA1};
static const unsigned char event_prologue[]={0x40,0x55,0x56,0x57,0x41,0x54};
static const unsigned char scene_prologue[]={0x40,0x57,0x48,0x83,0xEC,0x70};
static const unsigned char scenery_prologue[]={0x48,0x8B,0xC4,0x41,0x56};
// 2026-11-05 local5, shared since local11 (docs/HISTORY.md section 18): a read-only observer (San14Sim's version.dll trace hook)
// replaces the first bytes of 448E40 / 44A0F0 with E9 rel32 + NOPs. Those two rows only prove the function identity (virtual callees reached
// through the slots below, never patched here), so guards_match() judges the row hash with ga::hash_ok: the plain hash, or the hash with an
// OBSERVED prefix (E9 leaving the image, NOP padding, length and original bytes from ga_observers) put back. The whole 0x30 bytes stay covered.
// Complete tail wrappers, including rel32; never copy these into a trampoline.
static const unsigned char picture_show_bytes[]={0x48,0x8B,0x89,0xB8,1,0,0,0x45,0x33,0xC0,0xE9,0x81,0x0D,0x3F,0};
static const unsigned char picture_close_bytes[]={0x48,0x8B,0x89,0xB8,1,0,0,0xE9,0x14,0x55,0x3F,0};
// Exact primary vtables, independently resolved via PE RTTI COL/type descriptor.
static const unsigned event_owners[]={
    GA(GA_144A270), // CSearchDiscoveryState, frame 731EA0
    GA(GA_1444080), // CStrategyEmployTalkState, frame 733D30
    GA(GA_143EFD8), // CStrategyAdviceState, frame 731F70
    GA(GA_1468478), // CEventModalState, frame 83A880
    GA(GA_144ACF8), // CStrategyCreditRankupState, frame 7334A0 (844030 direct path remains native)
};
// 677D70 copies eight IDs from 14311A0 and appends literal 0x394.
static const unsigned district_labels[]={909,909,910,911,912,913,914,915,916};
// Optional CALL sites: validate/chain/skip independently, never patch the callee.
struct SceneSite {unsigned rva,original;};
static const SceneSite scene_sites[]={
    {GA(GA_291256),GA(GA_6C6A70)},
    {GA(GA_29A31B),GA(GA_2454C0)},{GA(GA_29A323),GA(GA_574750)},
    {GA(GA_29A715),GA(GA_2454C0)},{GA(GA_29A71D),GA(GA_574750)},
};
struct Slot {unsigned rva,original;};
static const Slot slots[]={
    {GA(GA_13D9438),GA(GA_44C340)},
    {GA(GA_13D9440),GA(GA_44B090)},
    {GA(GA_13D9960),GA(GA_44ACC0)},
    {GA(GA_13A5EF0),GA(GA_865170)},
    {GA(GA_13A5F48),GA(GA_1F29B0)},
    {GA(GA_13A5F68),GA(GA_8CA1C0)},
    {GA(GA_1431F30),GA(GA_67DB40)},
    {GA(GA_1431F50),GA(GA_67DD20)},
    {GA(GA_1431F38),GA(GA_67D8A0)},
    {GA(GA_1431ED0),GA(GA_67DEE0)},
    {GA(GA_1430230),GA(GA_671E90)},
    {GA(GA_1430248),GA(GA_671AB0)},
    {GA(GA_1477890),GA(GA_88E4B0)},
    {GA(GA_14778B0),GA(GA_895A10)},
    {GA(GA_1462A80),GA(GA_812FF0)},
    {GA(GA_1462AA0),GA(GA_8CA1C0)},
    {GA(GA_1468318),GA(GA_83E300)},
    {GA(GA_13D9458),GA(GA_44ED30)},
    {GA(GA_13A5FE0),GA(GA_1F2A60)},
    {GA(GA_13A6000),GA(GA_8CA1C0)},
};
}
