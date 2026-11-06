#pragma once
#include "game_addresses.h"
// Decoded CE KO 1fbe6be7; retained .text independently checked by the fixture.
namespace advisor_briefing {
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    // User 2026-11-02: headless final report; exact caller and native data ABI.
    {GA(GA_671950),0xB5,GH(0x74E3247405593EE6ull)},
    {GA(GA_676D0D),0x5,GH(0x4518FFC00815F7DEull)},
    {GA(GA_67AE80),0xC4,GH(0x6E551AF4FA4BFEAAull)},
    {GA(GA_2213C0),0x84,GH(0x9F4151F2F33C6416ull)},
    {GA(GA_2220A0),0x20,GH(0x566641AC944BF594ull)},
    {GA(GA_92EF10),0x84,GH(0x898FD7EFA62C2FCDull)},
    {GA(GA_92CEE0),0x2A,GH(0x3F5EB08BB653F311ull)},
    {GA(GA_304640),0xB9,GH(0xB7BC7DC9C6A64EC3ull)},
    {GA(GA_451FF0),0xF,GH(0xAEE5EBB108CF720Bull)},
    {GA(GA_43FF90),0xC,GH(0x99B5448DC3A91CBEull)},
    {GA(GA_842A90),0x30,GH(0xEC324A3DDAA32856ull)},
    {GA(GA_835230),0x30,GH(0xDEA85AF2B9BF30E3ull)},
    {GA(GA_6C43B0),0x92,GH(0x98ADEEDDF572D31Aull)},
    {GA(GA_6C4670),0x92,GH(0x9DF77FEDBF7DE264ull)},
    {GA(GA_41D550),0xCB,GH(0xB22ECE0F18101E79ull)},
    {GA(GA_3F5A0),0x2A,GH(0xC91B1B4F18C4D6A4ull)},
    {GA(GA_925B0),0x2D,GH(0x2CA692BB7D3CB2F8ull)},
    {GA(GA_1F0AD0),0x2FA,GH(0x685355CEA78F2233ull)},
    {GA(GA_1F09E0),0xE9,GH(0xDC99B37A38475A43ull)},
    {GA(GA_1F0DD0),0x1D4,GH(0x863FD6D8757688A0ull)},
    {GA(GA_332E90),0x39,GH(0xA2521E4DD50F954Full)},
    {GA(GA_44A0F0),0x30,GH(0xD25C0C429C3F6802ull)},
    {GA(GA_448E40),0x30,GH(0x3B3E7B6B24799976ull)},
    {GA(GA_448A70),0x30,GH(0x226B18362CB27251ull)},
    {GA(GA_862E40),0x30,GH(0xC66784648A0B537Dull)},
    {GA(GA_1F15F0),0x30,GH(0x4435611EFD5950CFull)},
    {GA(GA_4F7170),0x30,GH(0x3C937BC1FEA2320Full)},
    {GA(GA_67B420),0x30,GH(0xC8C15F1E6759F421ull)},
    {GA(GA_67B600),0x30,GH(0x9273D5886D8E0802ull)},
    {GA(GA_67B1E0),0x30,GH(0x78701124C9429BC4ull)},
    {GA(GA_67C990),0x30,GH(0x4EB7B1CC4CFF3FD3ull)},
    {GA(GA_676870),0x30,GH(0x458D80B2DB8D7B84ull)},
    {GA(GA_677D70),0x30,GH(0x4CD7D2DDF6B96B6Dull)},
    {GA(GA_66F7C0),0x30,GH(0x2D1B77E922AB0EBEull)},
    {GA(GA_88C1C0),0x30,GH(0xF76339E333DC9127ull)},
    {GA(GA_88D5A0),0x30,GH(0x9BE2533CF444F51Cull)},
    {GA(GA_2417A0),0x30,GH(0x88696CA8D446F155ull)},
    {GA(GA_92CB10),0x30,GH(0x859E8B9133D235DAull)},
    {GA(GA_92CF90),0x30,GH(0x2BE0A085C7A3A46Full)},
    {GA(GA_930D20),0x30,GH(0xE8519F2627931699ull)},
    {GA(GA_56FE50),0x3F,GH(0x9F11875143756B9Bull)},
    {GA(GA_56FC20),0x30,GH(0x221409446F18F1ADull)},
    {GA(GA_32E8D0),0x30,GH(0xEC77CF8C54A5ECBEull)},
    {GA(GA_233750),0x30,GH(0x51FEBF11B58D7C3Dull)},
    {GA(GA_22DBC0),0x30,GH(0x60E677EF6BF1AC9Full)},
    {GA(GA_874650),0x30,GH(0x4A4A3F922C1B87CAull)},
    {GA(GA_110E0),0x30,GH(0x38B11CA89519DC56ull)},
    {GA(GA_12540),0x30,GH(0x748089E0B3EAAC89ull)},
    {GA(GA_861480),0x28,GH(0xFA240DEFD52D4F83ull)},
    {GA(GA_8631F0),0x79,GH(0x57BF0E884218FD83ull)},
    {GA(GA_92CBA0),0xDB,GH(0x12F1E50010B8BB0Cull)},
    {GA(GA_67D1F0),0x12B2,GH(0x67335967463EC984ull)},
    {GA(GA_30CF80),0x30,GH(0x22A9B9D965B33389ull)},
    {GA(GA_8108C0),0xA1,GH(0x1E182ECCC5C2A7EBull)},
    // 81133D's five-byte CALL belongs to diplomacy_intel. Guard its owner
    // separately so an already-published local counterturn is compatible.
    {GA(GA_811210),0x12D,GH(0xCCF4A2755C00091Bull)},
    {GA(GA_811342),0xE9,GH(0x49FF241C21BBB1FAull)},
    {GA(GA_841660),0xCD,GH(0x567EF57C896446CEull)},
    {GA(GA_83BE30),0x76,GH(0xBD967541EFE52F88ull)},
    {GA(GA_861380),0xC7,GH(0x8F466E51B24BFF96ull)},
    {GA(GA_861F30),0x243,GH(0xD81D91F19BDF0D59ull)},
    {GA(GA_863270),0x70,GH(0x404C9D5BA002ED7Dull)},
    {GA(GA_863C70),0x81,GH(0x683ECE4BC6117BFEull)},
    {GA(GA_44CAF0),0x5B4,GH(0xDE27A5275C13AC34ull)},
    {GA(GA_22ECC0),0x30,GH(0xA0C2657FCE320466ull)},
    {GA(GA_22CA00),0x30,GH(0xDB324EB60C8358D8ull)},
    {GA(GA_733D30),0x442,GH(0x9D9777BB933B44F2ull)},
    {GA(GA_842380),0x1E6,GH(0x4418509E2CEDF835ull)},
    {GA(GA_22EEC0),0x30,GH(0x0575C1D6718A59D2ull)},
    {GA(GA_1F16A0),0xA1,GH(0x783B155FC33B0394ull)},
    {GA(GA_861450),0x28,GH(0x779EF135D4C3D51Bull)},
    {GA(GA_860D50),0x7F,GH(0x48D670F2DE39815Full)},
    {GA(GA_844030),0xC3,GH(0x15EC1F60D62BE81Dull)},
};
// Whole instructions, no RIP-relative operands; relay returns at entry+size.
static const unsigned char message_prologue[]={0x48,0x8B,0xC4,0x55,0x48,0x8D,0x68,0xA1};
static const unsigned char narration_prologue[]={0x48,0x8B,0xC4,0x57,0x48,0x81,0xEC,0x80,0,0,0};
static const unsigned char person_prologue[]={0x48,0x8B,0xC4,0x55,0x48,0x8D,0x68,0xA1};
static const unsigned char event_prologue[]={0x40,0x55,0x56,0x57,0x41,0x54};
static const unsigned char scene_prologue[]={0x40,0x57,0x48,0x83,0xEC,0x70};
static const unsigned char scenery_prologue[]={0x48,0x8B,0xC4,0x41,0x56};
// 2026-11-05 local5, shared since local11 (docs/INSTALL_LOCAL2_TRIAGE.md section 18): a read-only observer (San14Sim's version.dll trace hook)
// replaces the first bytes of 448E40 / 44A0F0 with E9 rel32 + NOPs. Those two rows only prove the function identity (virtual callees reached
// through the slots below, never patched here), so guards_match() judges the row hash with ga::hash_ok: the plain hash, or the hash with an
// OBSERVED prefix (E9 leaving the image, NOP padding, length and original bytes from ga_observers) put back. The whole 0x30 bytes stay covered.
// Complete tail wrappers, including rel32; never copy these into a trampoline.
static const unsigned char picture_show_bytes[]={0x48,0x8B,0x89,0xB8,1,0,0,0x45,0x33,0xC0,0xE9,0x91,0x0A,0x3F,0};
static const unsigned char picture_close_bytes[]={0x48,0x8B,0x89,0xB8,1,0,0,0xE9,0x94,0x52,0x3F,0};
// Exact primary vtables, independently resolved via PE RTTI COL/type descriptor.
static const unsigned event_owners[]={
    GA(GA_14471E8), // CSearchDiscoveryState, frame 731EA0
    GA(GA_1440FF8), // CStrategyEmployTalkState, frame 733D30
    GA(GA_143BF50), // CStrategyAdviceState, frame 731F70
    GA(GA_14653E8), // CEventModalState, frame 83A880
    GA(GA_1447C70), // CStrategyCreditRankupState, frame 7334A0 (844030 direct path remains native)
};
// 677D70 copies eight IDs from 14311A0 and appends literal 0x394.
static const unsigned district_labels[]={909,909,910,911,912,913,914,915,916};
// Optional CALL sites: validate/chain/skip independently, never patch the callee.
struct SceneSite {unsigned rva,original;};
static const SceneSite scene_sites[]={
    {GA(GA_28F346),GA(GA_6C4310)},
    {GA(GA_29840B),GA(GA_243A60)},{GA(GA_298413),GA(GA_571EF0)},
    {GA(GA_298805),GA(GA_243A60)},{GA(GA_29880D),GA(GA_571EF0)},
};
struct Slot {unsigned rva,original;};
static const Slot slots[]={
    {GA(GA_13D63A8),GA(GA_44A0F0)},
    {GA(GA_13D63B0),GA(GA_448E40)},
    {GA(GA_13D68D0),GA(GA_448A70)},
    {GA(GA_13A2EB0),GA(GA_862E40)},
    {GA(GA_13A2F08),GA(GA_1F15F0)},
    {GA(GA_13A2F28),GA(GA_92AD0)},
    {GA(GA_142EEA0),GA(GA_67B420)},
    {GA(GA_142EEC0),GA(GA_67B600)},
    {GA(GA_142EEA8),GA(GA_67B1E0)},
    {GA(GA_142EE40),GA(GA_67B7C0)},
    {GA(GA_142D1A0),GA(GA_66F7C0)},
    {GA(GA_142D1B8),GA(GA_66F3E0)},
    {GA(GA_1474800),GA(GA_88C1C0)},
    {GA(GA_1474820),GA(GA_893720)},
    {GA(GA_145F9F0),GA(GA_8108C0)},
    {GA(GA_145FA10),GA(GA_92AD0)},
    {GA(GA_1465288),GA(GA_83BE30)},
    {GA(GA_13D63C8),GA(GA_44CAF0)},
    {GA(GA_13A2FA0),GA(GA_1F16A0)},
    {GA(GA_13A2FC0),GA(GA_92AD0)},
};
}
