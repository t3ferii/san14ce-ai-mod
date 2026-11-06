#pragma once
#include "game_addresses.h"
// P3 hook registry (docs/LAYOUT_DLL_COMPAT_DESIGN.md section 8.1, user 2026-11-05 Layout compatibility design).
// One row per UI hook handed to SAN14_Layout.dll's hook manager. Data only: owner, kind, RVA, original target and
// manager flags. The replacement bodies stay in SAN14_Convenience.dll (design C-1, Q7) and register by RVA through
// layout_client::vt/call/jump. The Layout fixture registers every row against a synthetic image, and
// tests/verify_layout_registry.py checks the table against the original unpacked EXE, the blocked-entry list and
// the whitelist rows. Do not put game logic here.
#include <stdint.h>
#include "layout_api.h"
enum LayoutRowKind : unsigned char {LR_VT=1,LR_CALL=2,LR_JUMP=3};
struct LayoutRow {const char *owner;unsigned char kind;uint32_t rva,original,flags;};
// Flag shorthands (design 8.1: C chain, Y yield to a same-function peer, R refuse foreign, W whitelist required).
enum : uint32_t {
    LF_C=LAYOUT_CHAIN_OK|LAYOUT_ALL_OR_NONE,
    LF_CY=LF_C|LAYOUT_YIELD_IF_PEER,
    LF_CW=LF_C|LAYOUT_NEED_WHITELIST,
    LF_CYW=LF_CY|LAYOUT_NEED_WHITELIST,
    LF_Y=LAYOUT_YIELD_IF_PEER|LAYOUT_ALL_OR_NONE            // yield to the same-function peer, never chain (imported/own byte patches)
};
static const LayoutRow layout_rows[]={
    // 1-4 speed_buttons (Q8: yields to the R1 integrated progress-speed extension, which patches the same 898957 jump)
    {"speed_buttons",LR_VT,  GA(GA_1473F50),GA(GA_882540),LF_CYW},   // 1  CChangeSpeedLayout +188; yuga checks this slot (VirtualQuery + Convenience)
    {"speed_buttons",LR_VT,  GA(GA_13D6698),GA(GA_44BCA0),LF_CY},    // 2  day progress frame
    {"speed_buttons",LR_JUMP,GA(GA_898957), GA(GA_891720),LF_CY},    // 3  std::function tail jump (E9, block offset 7)
    {"speed_buttons",LR_VT,  GA(GA_1474100),GA(GA_88C2E0),LF_CY},    // 4  popup hover
    // 5-6 lord_change
    {"lord_change",LR_CALL,GA(GA_503C9C),GA(GA_85FEF0),LF_C},
    {"lord_change",LR_VT,  GA(GA_13E5088),GA(GA_533570),LF_C},
    // 7-12 region_officers (fog 109-slot table: all six slots must live in our DLL; yields only for R1 RegionOfficer Face!=0)
    {"region_officers",LR_VT,GA(GA_139A338),GA(GA_1A7F50),LF_CYW},
    {"region_officers",LR_VT,GA(GA_139A350),GA(GA_1A74D0),LF_CYW},
    {"region_officers",LR_VT,GA(GA_139A328),GA(GA_19B1D0),LF_CYW},
    {"region_officers",LR_VT,GA(GA_13999C8),GA(GA_1A7EA0),LF_CYW},
    {"region_officers",LR_VT,GA(GA_13999E0),GA(GA_1A75A0),LF_CYW},
    {"region_officers",LR_VT,GA(GA_13999B8),GA(GA_19B350),LF_CYW},
    // 13-15 ccz_title (+ the frame slot 13E4E30 the module already hooks, not in the 8.1 table: reported)
    {"ccz_title",LR_CALL,GA(GA_4E83F3), GA(GA_866230),LF_C},
    {"ccz_title",LR_VT,  GA(GA_13E4E10),GA(GA_504670),LF_C},
    {"ccz_title",LR_VT,  GA(GA_13E4E18),GA(GA_4F9310),LF_C},
    {"ccz_title",LR_VT,  GA(GA_13E4E30),GA(GA_50B800),LF_C},
    // 16 ccz_sprites (fog 109-slot table)
    {"ccz_sprites",LR_VT,GA(GA_13A03D0),GA(GA_1C6EC0),LF_CW},
    // 17-19 layout_wait; the 86E0C0 entry patch is gone (8.3), layout_keep waits for 86E320 observation (P4)
    {"layout_wait",LR_CALL,GA(GA_676767),GA(GA_86C570),LF_C},
    {"layout_wait",LR_CALL,GA(GA_86CFA5),GA(GA_86C570),LF_C},
    {"layout_wait",LR_CALL,GA(GA_86DEC9),GA(GA_86C570),LF_C},
    // 21-23 fast_menu (8.7: yields to R1 FastMenu)
    {"fast_menu",LR_VT,GA(GA_13D63D0),GA(GA_56FF60),LF_CY},
    {"fast_menu",LR_VT,GA(GA_143C4C8),GA(GA_56FF60),LF_CY},
    {"fast_menu",LR_VT,GA(GA_1457238),GA(GA_56FF60),LF_CY},
    // 24 stat_exp text call (R1 StatusColor identifies it by the 85C5DC return address: tail jump keeps it)
    {"stat_exp",LR_CALL,GA(GA_85C5D7),GA(GA_930D20),LF_C},
    // 25 stat_exp: 15 (22FCE0 stat read, 85C420 chart) pairs in three windows
    {"stat_exp",LR_CALL,GA(GA_9073F6),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_907406),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_907421),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_907434),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_90744F),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_907462),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_90747D),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_907490),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_9074AB),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_9074BE),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_907F79),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_907F89),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_907FA5),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_907FB8),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_907FD4),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_907FE7),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_908003),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_908016),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_908032),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_908045),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F5A8C),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_7F5A9C),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F5AB5),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_7F5AC6),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F5ADF),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_7F5AF0),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F5B09),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_7F5B1A),GA(GA_85C420),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F5B33),GA(GA_22FCE0),LF_C},{"stat_exp",LR_CALL,GA(GA_7F5B44),GA(GA_85C420),LF_C},
    // 26 dispatch_days
    {"dispatch_days",LR_CALL,GA(GA_898083),GA(GA_930D20),LF_C},
    // 27 damage_preview: the 7FA230 entry patch is replaced by its two callers (8.3; 7FA1FD crosses the block)
    {"damage_preview",LR_CALL,GA(GA_7F5781),GA(GA_7FA230),LF_C},
    {"damage_preview",LR_CALL,GA(GA_7FA1FD),GA(GA_7FA230),LF_C},
    // 28-32 expert_links: the 6566E0 entry patch is replaced by its three callers (8.3)
    {"expert_links",LR_CALL,GA(GA_5F10F7),GA(GA_6566E0),LF_C},
    {"expert_links",LR_CALL,GA(GA_5F5B27),GA(GA_6566E0),LF_C},
    {"expert_links",LR_CALL,GA(GA_65EAAD),GA(GA_6566E0),LF_C},
    {"expert_links",LR_CALL,GA(GA_6362DB),GA(GA_5BEC40),LF_C},
    {"expert_links",LR_CALL,GA(GA_5F14DA),GA(GA_92E430),LF_C},
    {"expert_links",LR_VT,GA(GA_141BCC8),GA(GA_5B9110),LF_C},
    {"expert_links",LR_VT,GA(GA_1429728),GA(GA_65EDD0),LF_C},
    {"expert_links",LR_VT,GA(GA_14296B8),GA(GA_65E810),LF_C},
    {"expert_links",LR_VT,GA(GA_14296F0),GA(GA_65F890),LF_C},
    {"expert_links",LR_VT,GA(GA_1429798),GA(GA_65E8C0),LF_C},
    {"expert_links",LR_VT,GA(GA_141BD20),GA(GA_6475C0),LF_C},
    {"expert_links",LR_VT,GA(GA_141BCF8),GA(GA_5A7450),LF_C},
    {"expert_links",LR_VT,GA(GA_141BE88),GA(GA_5A73A0),LF_C},
    // 33-35 unified_dialogs
    {"unified_dialogs",LR_VT,GA(GA_13AC588),GA(GA_2FB440),LF_C},
    {"unified_dialogs",LR_VT,GA(GA_13AC5B0),GA(GA_258170),LF_C},
    {"unified_dialogs",LR_VT,GA(GA_13AC578),GA(GA_256970),LF_C},
    // 36 advisor_hud
    {"advisor_hud",LR_VT,GA(GA_14751D8),GA(GA_894420),LF_C},
    {"advisor_hud",LR_VT,GA(GA_14751F0),GA(GA_8782E0),LF_C},
    // 37-40 recruit_ui
    {"recruit_ui",LR_VT,GA(GA_13AC708),GA(GA_6BE8E0),LF_C},
    {"recruit_ui",LR_VT,GA(GA_13AC770),GA(GA_2C0820),LF_C},
    {"recruit_ui",LR_VT,GA(GA_1440FE8),GA(GA_6F1D10),LF_C},
    {"recruit_ui",LR_VT,GA(GA_1440DF0),GA(GA_733BD0),LF_C},
    // 41-44 merit. merit_save keeps the warehouse sidecar independent of merit gameplay (existing guard split);
    // merit_prisoner is split out of the bundle (G4 exception) and yields to R1 PrisonerReturn.
    {"merit_save",LR_VT,GA(GA_13E69A0),GA(GA_50B670),LF_C},
    {"merit_save",LR_VT,GA(GA_13E60F8),GA(GA_509580),LF_C},
    {"merit_prisoner",LR_VT,GA(GA_13A6BB0),GA(GA_2390B0),LAYOUT_CHAIN_OK|LAYOUT_YIELD_IF_PEER},
    {"merit",LR_VT,GA(GA_1413600),GA(GA_908820),LF_CW},               // yuga waits for this slot and chains behind it
    {"merit",LR_VT,GA(GA_1413488),GA(GA_878810),LF_C},
    // (the traits9 captive CALL row, former 47, was removed with the captive_dialog feature, 2026-11-06 local10)
    // 49 collab (R1 ModPackBridge Collab) / 50 scenery (R1 SettingsHelpScenic): modpack record and scenery slots, Y only
    {"collab",LR_VT,GA(GA_13A7220),GA(GA_23EB50),LF_Y},
    {"collab",LR_VT,GA(GA_13A6DE8),GA(GA_23FB90),LF_Y},
    {"scenery",LR_VT,GA(GA_13D2F00),GA(GA_421990),LF_Y},
    {"scenery",LR_VT,GA(GA_14471F0),GA(GA_7229C0),LF_Y},
    // 52 domestic_selection (domestic AI node slots)
    {"domestic_selection",LR_VT,GA(GA_1346E70),GA(GA_36320),LF_CY},
    {"domestic_selection",LR_VT,GA(GA_134C8F8),GA(GA_7A110),LF_CY},
    // 53 mediation_plus E8 sites (6D7D90 is handed to batteki/yuga, its filter moves to the 70E9C1 block hook)
    {"mediation_plus",LR_CALL,GA(GA_70EADC),GA(GA_711920),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_6BA697),GA(GA_796110),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_7971B3),GA(GA_796110),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_6BA69F),GA(GA_78D920),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_7971BB),GA(GA_78D920),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_73502B),GA(GA_6D1A50),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_735035),GA(GA_6D0550),LF_C},
    // trait_hover: 152 vtable slots in the ModPack TraitHover blob table order (0x62A0) + 4 930D20 duel label calls (docs/TRAIT_HOVER_PLAN.md 1.2/1.7/3.2). N = row index inside the owner.
    {"trait_hover",LR_VT,GA(GA_13ACF38),GA(GA_92AD0),LF_CY}, // 1 CPersonData 표 tick
    {"trait_hover",LR_VT,GA(GA_13ACF70),GA(GA_7C9080),LF_CY}, // 2 CPersonData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13AD8A8),GA(GA_92AD0),LF_CY}, // 3 CKingdomData 표 tick
    {"trait_hover",LR_VT,GA(GA_13AD8E0),GA(GA_7C9080),LF_CY}, // 4 CKingdomData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13AD8C8),GA(GA_8FBD40),LF_CY}, // 5 CKingdomData 표 종료
    {"trait_hover",LR_VT,GA(GA_13D7E30),GA(GA_92AD0),LF_CY}, // 6 CCityData 표 tick
    {"trait_hover",LR_VT,GA(GA_13D7E68),GA(GA_7C9080),LF_CY}, // 7 CCityData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13D7E50),GA(GA_8FBD40),LF_CY}, // 8 CCityData 표 종료
    {"trait_hover",LR_VT,GA(GA_13EFF18),GA(GA_92AD0),LF_CY}, // 9 CForceData 표 tick
    {"trait_hover",LR_VT,GA(GA_13EFF50),GA(GA_7C9080),LF_CY}, // 10 CForceData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13EFF38),GA(GA_8FBD40),LF_CY}, // 11 CForceData 표 종료
    {"trait_hover",LR_VT,GA(GA_13EFD28),GA(GA_92AD0),LF_CY}, // 12 CEditPersonData 표 tick
    {"trait_hover",LR_VT,GA(GA_13EFD60),GA(GA_7C9080),LF_CY}, // 13 CEditPersonData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13EFB38),GA(GA_92AD0),LF_CY}, // 14 CItemData 표 tick
    {"trait_hover",LR_VT,GA(GA_13EFB70),GA(GA_7C9080),LF_CY}, // 15 CItemData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13EFB58),GA(GA_8FBD40),LF_CY}, // 16 CItemData 표 종료
    {"trait_hover",LR_VT,GA(GA_13EF948),GA(GA_92AD0),LF_CY}, // 17 CPersonalityData 표 tick
    {"trait_hover",LR_VT,GA(GA_13EF980),GA(GA_7C9080),LF_CY}, // 18 CPersonalityData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13EF968),GA(GA_8FBD40),LF_CY}, // 19 CPersonalityData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F2D10),GA(GA_92AD0),LF_CY}, // 20 CAdvantageData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F2D48),GA(GA_7C9080),LF_CY}, // 21 CAdvantageData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F2D30),GA(GA_8FBD40),LF_CY}, // 22 CAdvantageData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F3AA0),GA(GA_92AD0),LF_CY}, // 23 CTacticsData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F3AD8),GA(GA_7C9080),LF_CY}, // 24 CTacticsData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F3AC0),GA(GA_8FBD40),LF_CY}, // 25 CTacticsData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F38B0),GA(GA_92AD0),LF_CY}, // 26 CFormationData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F38E8),GA(GA_7C9080),LF_CY}, // 27 CFormationData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F38D0),GA(GA_8FBD40),LF_CY}, // 28 CFormationData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F36C0),GA(GA_92AD0),LF_CY}, // 29 CEditNewItemData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F36F8),GA(GA_7C9080),LF_CY}, // 30 CEditNewItemData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F36E0),GA(GA_8FBD40),LF_CY}, // 31 CEditNewItemData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F34D0),GA(GA_92AD0),LF_CY}, // 32 CFootholdData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F3508),GA(GA_7C9080),LF_CY}, // 33 CFootholdData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F34F0),GA(GA_8FBD40),LF_CY}, // 34 CFootholdData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F32E0),GA(GA_92AD0),LF_CY}, // 35 CEditPersonalityData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F3318),GA(GA_7C9080),LF_CY}, // 36 CEditPersonalityData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F3300),GA(GA_8FBD40),LF_CY}, // 37 CEditPersonalityData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F30F0),GA(GA_92AD0),LF_CY}, // 38 CEditPersonalityConditionData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F3128),GA(GA_7C9080),LF_CY}, // 39 CEditPersonalityConditionData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F3110),GA(GA_8FBD40),LF_CY}, // 40 CEditPersonalityConditionData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F2F00),GA(GA_92AD0),LF_CY}, // 41 CEditPersonalityEffectData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F2F38),GA(GA_7C9080),LF_CY}, // 42 CEditPersonalityEffectData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F2F20),GA(GA_8FBD40),LF_CY}, // 43 CEditPersonalityEffectData 표 종료
    {"trait_hover",LR_VT,GA(GA_1422028),GA(GA_92AD0),LF_CY}, // 44 CRankData 표 tick
    {"trait_hover",LR_VT,GA(GA_1422060),GA(GA_7C9080),LF_CY}, // 45 CRankData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1422048),GA(GA_8FBD40),LF_CY}, // 46 CRankData 표 종료
    {"trait_hover",LR_VT,GA(GA_1421E38),GA(GA_92AD0),LF_CY}, // 47 CPoemData 표 tick
    {"trait_hover",LR_VT,GA(GA_1421E70),GA(GA_7C9080),LF_CY}, // 48 CPoemData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1421E58),GA(GA_8FBD40),LF_CY}, // 49 CPoemData 표 종료
    {"trait_hover",LR_VT,GA(GA_1421C48),GA(GA_92AD0),LF_CY}, // 50 CSceneryData 표 tick
    {"trait_hover",LR_VT,GA(GA_1421C80),GA(GA_7C9080),LF_CY}, // 51 CSceneryData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1421C68),GA(GA_8FBD40),LF_CY}, // 52 CSceneryData 표 종료
    {"trait_hover",LR_VT,GA(GA_1421A58),GA(GA_92AD0),LF_CY}, // 53 CAreaData 표 tick
    {"trait_hover",LR_VT,GA(GA_1421A90),GA(GA_7C9080),LF_CY}, // 54 CAreaData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1421A78),GA(GA_8FBD40),LF_CY}, // 55 CAreaData 표 종료
    {"trait_hover",LR_VT,GA(GA_1424240),GA(GA_92AD0),LF_CY}, // 56 CDistrictData 표 tick
    {"trait_hover",LR_VT,GA(GA_1424278),GA(GA_7C9080),LF_CY}, // 57 CDistrictData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1424260),GA(GA_8FBD40),LF_CY}, // 58 CDistrictData 표 종료
    {"trait_hover",LR_VT,GA(GA_1424050),GA(GA_92AD0),LF_CY}, // 59 SResolutionDispInfo 표 tick
    {"trait_hover",LR_VT,GA(GA_1424088),GA(GA_7C9080),LF_CY}, // 60 SResolutionDispInfo 표 숨김
    {"trait_hover",LR_VT,GA(GA_1424070),GA(GA_8FBD40),LF_CY}, // 61 SResolutionDispInfo 표 종료
    {"trait_hover",LR_VT,GA(GA_1423E60),GA(GA_92AD0),LF_CY}, // 62 CDoctrineData 표 tick
    {"trait_hover",LR_VT,GA(GA_1423E98),GA(GA_7C9080),LF_CY}, // 63 CDoctrineData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1423E80),GA(GA_8FBD40),LF_CY}, // 64 CDoctrineData 표 종료
    {"trait_hover",LR_VT,GA(GA_1423C70),GA(GA_92AD0),LF_CY}, // 65 CPolicyData 표 tick
    {"trait_hover",LR_VT,GA(GA_1423CA8),GA(GA_7C9080),LF_CY}, // 66 CPolicyData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1423C90),GA(GA_8FBD40),LF_CY}, // 67 CPolicyData 표 종료
    {"trait_hover",LR_VT,GA(GA_14491D0),GA(GA_92AD0),LF_CY}, // 68 CCampaignData 표 tick
    {"trait_hover",LR_VT,GA(GA_1449208),GA(GA_7C9080),LF_CY}, // 69 CCampaignData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14491F0),GA(GA_8FBD40),LF_CY}, // 70 CCampaignData 표 종료
    {"trait_hover",LR_VT,GA(GA_1448F70),GA(GA_92AD0),LF_CY}, // 71 CPersonGroupUIData 표 tick
    {"trait_hover",LR_VT,GA(GA_1448FA8),GA(GA_7C9080),LF_CY}, // 72 CPersonGroupUIData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1448F90),GA(GA_8FBD40),LF_CY}, // 73 CPersonGroupUIData 표 종료
    {"trait_hover",LR_VT,GA(GA_1445798),GA(GA_92AD0),LF_CY}, // 74 CPostData 표 tick
    {"trait_hover",LR_VT,GA(GA_14457D0),GA(GA_7C9080),LF_CY}, // 75 CPostData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14457B8),GA(GA_8FBD40),LF_CY}, // 76 CPostData 표 종료
    {"trait_hover",LR_VT,GA(GA_144AE08),GA(GA_92AD0),LF_CY}, // 77 CDoctrinePanelData 표 tick
    {"trait_hover",LR_VT,GA(GA_144AE40),GA(GA_7C9080),LF_CY}, // 78 CDoctrinePanelData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144AE28),GA(GA_8FBD40),LF_CY}, // 79 CDoctrinePanelData 표 종료
    {"trait_hover",LR_VT,GA(GA_144B3D8),GA(GA_92AD0),LF_CY}, // 80 CForcePolicyData 표 tick
    {"trait_hover",LR_VT,GA(GA_144B410),GA(GA_7C9080),LF_CY}, // 81 CForcePolicyData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144B3F8),GA(GA_8FBD40),LF_CY}, // 82 CForcePolicyData 표 종료
    {"trait_hover",LR_VT,GA(GA_144B1E8),GA(GA_92AD0),LF_CY}, // 83 CGemData 표 tick
    {"trait_hover",LR_VT,GA(GA_144B220),GA(GA_7C9080),LF_CY}, // 84 CGemData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144B208),GA(GA_8FBD40),LF_CY}, // 85 CGemData 표 종료
    {"trait_hover",LR_VT,GA(GA_144AFF8),GA(GA_92AD0),LF_CY}, // 86 CForeignData 표 tick
    {"trait_hover",LR_VT,GA(GA_144B030),GA(GA_7C9080),LF_CY}, // 87 CForeignData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144B018),GA(GA_8FBD40),LF_CY}, // 88 CForeignData 표 종료
    {"trait_hover",LR_VT,GA(GA_145A450),GA(GA_92AD0),LF_CY}, // 89 CArmyUnitData 표 tick
    {"trait_hover",LR_VT,GA(GA_145A488),GA(GA_7C9080),LF_CY}, // 90 CArmyUnitData 표 숨김
    {"trait_hover",LR_VT,GA(GA_145A470),GA(GA_8FBD40),LF_CY}, // 91 CArmyUnitData 표 종료
    {"trait_hover",LR_VT,GA(GA_145A640),GA(GA_92AD0),LF_CY}, // 92 CFacilityData 표 tick
    {"trait_hover",LR_VT,GA(GA_145A678),GA(GA_7C9080),LF_CY}, // 93 CFacilityData 표 숨김
    {"trait_hover",LR_VT,GA(GA_145A660),GA(GA_8FBD40),LF_CY}, // 94 CFacilityData 표 종료
    {"trait_hover",LR_VT,GA(GA_14B03C8),GA(GA_92AD0),LF_CY}, // 95 CGateData 표 tick
    {"trait_hover",LR_VT,GA(GA_14B0400),GA(GA_7C9080),LF_CY}, // 96 CGateData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14B03E8),GA(GA_8FBD40),LF_CY}, // 97 CGateData 표 종료
    {"trait_hover",LR_VT,GA(GA_14B01D8),GA(GA_92AD0),LF_CY}, // 98 CForeignTechnologyData 표 tick
    {"trait_hover",LR_VT,GA(GA_14B0210),GA(GA_7C9080),LF_CY}, // 99 CForeignTechnologyData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14B01F8),GA(GA_8FBD40),LF_CY}, // 100 CForeignTechnologyData 표 종료
    {"trait_hover",LR_VT,GA(GA_14AFFE8),GA(GA_92AD0),LF_CY}, // 101 CCreditRankData 표 tick
    {"trait_hover",LR_VT,GA(GA_14B0020),GA(GA_7C9080),LF_CY}, // 102 CCreditRankData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14B0008),GA(GA_8FBD40),LF_CY}, // 103 CCreditRankData 표 종료
    {"trait_hover",LR_VT,GA(GA_13AD160),GA(GA_2DC340),LF_CY}, // 104 CPersonData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13ADA98),GA(GA_2DC340),LF_CY}, // 105 CKingdomData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13D8058),GA(GA_2DC340),LF_CY}, // 106 CCityData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F0108),GA(GA_2DC340),LF_CY}, // 107 CForceData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F02F0),GA(GA_2DC340),LF_CY}, // 108 CEditPersonData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F04D8),GA(GA_2DC340),LF_CY}, // 109 CItemData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F06C0),GA(GA_2DC340),LF_CY}, // 110 CPersonalityData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F49E8),GA(GA_2DC340),LF_CY}, // 111 CAdvantageData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F3C90),GA(GA_2DC340),LF_CY}, // 112 CTacticsData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F3E78),GA(GA_2DC340),LF_CY}, // 113 CFormationData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F4060),GA(GA_2DC340),LF_CY}, // 114 CEditNewItemData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F4248),GA(GA_2DC340),LF_CY}, // 115 CFootholdData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F4430),GA(GA_2DC340),LF_CY}, // 116 CEditPersonalityData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F4618),GA(GA_2DC340),LF_CY}, // 117 CEditPersonalityConditionData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F4800),GA(GA_2DC340),LF_CY}, // 118 CEditPersonalityEffectData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1422218),GA(GA_2DC340),LF_CY}, // 119 CRankData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1422400),GA(GA_2DC340),LF_CY}, // 120 CPoemData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14225E8),GA(GA_2DC340),LF_CY}, // 121 CSceneryData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14227D0),GA(GA_2DC340),LF_CY}, // 122 CAreaData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1424430),GA(GA_2DC340),LF_CY}, // 123 CDistrictData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1424618),GA(GA_2DC340),LF_CY}, // 124 SResolutionDispInfo 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1424800),GA(GA_2DC340),LF_CY}, // 125 CDoctrineData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14249E8),GA(GA_2DC340),LF_CY}, // 126 CPolicyData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1448D88),GA(GA_2DC340),LF_CY}, // 127 CCampaignData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1449430),GA(GA_2DC340),LF_CY}, // 128 CPersonGroupUIData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1449688),GA(GA_2DC340),LF_CY}, // 129 CPostData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144BB80),GA(GA_2DC340),LF_CY}, // 130 CDoctrinePanelData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144B5C8),GA(GA_2DC340),LF_CY}, // 131 CForcePolicyData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144B7B0),GA(GA_2DC340),LF_CY}, // 132 CGemData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144B998),GA(GA_2DC340),LF_CY}, // 133 CForeignData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_145AA18),GA(GA_2DC340),LF_CY}, // 134 CArmyUnitData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_145A830),GA(GA_2DC340),LF_CY}, // 135 CFacilityData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14B05B8),GA(GA_2DC340),LF_CY}, // 136 CGateData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14B07A0),GA(GA_2DC340),LF_CY}, // 137 CForeignTechnologyData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14B0988),GA(GA_2DC340),LF_CY}, // 138 CCreditRankData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_145FFE0),GA(GA_92AD0),LF_CY}, // 139 CDuelPersonInfoLayout 일기토 tick
    {"trait_hover",LR_VT,GA(GA_14604C0),GA(GA_92AD0),LF_CY}, // 140 CDuelGaugeLayout 일기토 tick
    {"trait_hover",LR_VT,GA(GA_14B8CE8),GA(GA_92AD0),LF_CY}, // 141 CInfoSummaryPersonality 요약 tick
    {"trait_hover",LR_VT,GA(GA_14B8D08),GA(GA_924B20),LF_CY}, // 142 CInfoSummaryPersonality 요약 종료
    {"trait_hover",LR_VT,GA(GA_14B8D20),GA(GA_9143C0),LF_CY}, // 143 CInfoSummaryPersonality 요약 숨김
    {"trait_hover",LR_VT,GA(GA_13EC0D8),GA(GA_92AD0),LF_CY}, // 144 CEditNewPersonalityLayout 편집기 tick
    {"trait_hover",LR_VT,GA(GA_13EC110),GA(GA_869820),LF_CY}, // 145 CEditNewPersonalityLayout 편집기 숨김
    {"trait_hover",LR_VT,GA(GA_13EC0F8),GA(GA_8782E0),LF_CY}, // 146 CEditNewPersonalityLayout 편집기 종료
    {"trait_hover",LR_VT,GA(GA_13ACF58),GA(GA_8FBD40),LF_CY}, // 147 CPersonData 표 종료
    {"trait_hover",LR_VT,GA(GA_13EFD48),GA(GA_8FBD40),LF_CY}, // 148 CEditPersonData 표 종료
    {"trait_hover",LR_VT,GA(GA_1460000),GA(GA_8782E0),LF_CY}, // 149 CDuelPersonInfoLayout 일기토 종료
    {"trait_hover",LR_VT,GA(GA_1460018),GA(GA_869820),LF_CY}, // 150 CDuelPersonInfoLayout 일기토 숨김
    {"trait_hover",LR_VT,GA(GA_14604E0),GA(GA_8782E0),LF_CY}, // 151 CDuelGaugeLayout 일기토 종료
    {"trait_hover",LR_VT,GA(GA_14604F8),GA(GA_869820),LF_CY}, // 152 CDuelGaugeLayout 일기토 숨김
    {"trait_hover",LR_CALL,GA(GA_806D51),GA(GA_930D20),LF_CY}, // 153 CDuelPersonInfoLayout 갱신 개성 라벨 B
    {"trait_hover",LR_CALL,GA(GA_8070B1),GA(GA_930D20),LF_CY}, // 154 CDuelPersonInfoLayout 갱신 개성 라벨 B
    {"trait_hover",LR_CALL,GA(GA_807D92),GA(GA_930D20),LF_CY}, // 155 CDuelGaugeLayout 갱신 개성 라벨 A
    {"trait_hover",LR_CALL,GA(GA_80807C),GA(GA_930D20),LF_CY}, // 156 CDuelGaugeLayout 갱신 개성 라벨 A
    // battle_deaths (docs/BATTLE_DEATHS_PLAN.md): cut-in visibility (368360) x2 + death record (295AA0) x2; the 295C5F branch is layout_battle_deaths_span.
    {"battle_deaths",LR_CALL,GA(GA_259682),GA(GA_368360),LF_CY}, // 157 258EE0 전투 전사 컷인 화면 판정
    {"battle_deaths",LR_CALL,GA(GA_27D976),GA(GA_368360),LF_CY}, // 158 27D4B0 전사 컷인 화면 판정
    {"battle_deaths",LR_CALL,GA(GA_25974B),GA(GA_295AA0),LF_CY}, // 159 258EE0 전사 기록 호출(범위)
    {"battle_deaths",LR_CALL,GA(GA_27DA09),GA(GA_295AA0),LF_CY}, // 160 27D4B0 전사 기록 호출(범위)
};
static const unsigned layout_row_count=sizeof(layout_rows)/sizeof(layout_rows[0]);
// Block / span / byte rows that are not a vtable slot or call site. expect[] is the original bytes.
struct LayoutPatchRow {const char *owner;uint32_t rva,len;uint32_t flags;};
// 53: 70E9C1 `cmp word [rdx+0BCh],0 ; jnz 70EA11` inside the 70E9A0 candidate filter. Refuse when the bytes differ.
static const uint8_t layout_mediation_expect[10]={0x66,0x83,0xBA,0xBC,0x00,0x00,0x00,0x00,0x75,0x46};
static const LayoutPatchRow layout_mediation_row={"mediation_plus",GA(GA_70E9C1),10,LAYOUT_REFUSE_IF_FOREIGN|LAYOUT_ALL_OR_NONE};
// battle_deaths: 295C5F `test ebx,ebx ; je +8CE` (8 bytes, offset 0xF in its 16-byte line, so api_hook_block would refuse it). Written as a span (E9 + NOP3).
static const uint8_t layout_battle_deaths_expect[8]={0x85,0xDB,0x0F,0x84,0xCE,0x08,0x00,0x00};   // test ebx,ebx; je +8CE (both builds)
static const LayoutPatchRow layout_battle_deaths_span={"battle_deaths",GA(GA_295C5F),8,
    LAYOUT_YIELD_IF_PEER|LAYOUT_REFUSE_IF_FOREIGN|LAYOUT_ALL_OR_NONE};
// Dynamic-source patches (manifest tables of the converted traits9 mod): spans are E9 + NOP fill written block by block,
// bytes are plain value patches. Owner traits9, YIELD_IF_PEER (R1 Traits9 writes the same places), never chained.
enum LayoutPatchKind : unsigned char {LP_SPAN=4,LP_BYTES=5};
struct LayoutPatchSite {unsigned char kind;uint32_t rva,len;};
static const LayoutPatchSite layout_traits9_sites[]={
    {LP_BYTES,GA(GA_4A6CF3),8},{LP_BYTES,GA(GA_4A7313),8},{LP_BYTES,GA(GA_22558F),4},{LP_BYTES,GA(GA_23064A),3},{LP_BYTES,GA(GA_232D97),4},
    {LP_SPAN,GA(GA_8DF198),7},{LP_SPAN,GA(GA_8DF1F5),6},{LP_SPAN,GA(GA_907294),17},
    {LP_SPAN,GA(GA_8CDD40),14},{LP_SPAN,GA(GA_8E0148),10},{LP_SPAN,GA(GA_8E01D0),6},{LP_SPAN,GA(GA_907D9C),16},
    {LP_SPAN,GA(GA_2BF033),15}
};
static const unsigned layout_traits9_site_count=sizeof(layout_traits9_sites)/sizeof(layout_traits9_sites[0]);
// Machine code of the 70E9C1 relay (design 8.4), 64 bytes: `cmp byte [flag],0 ; jne pass ; cmp word [rdx+0BCh],0 ;
// jne reject ; pass: jmp [pass_target] ; reject: jmp [reject_target]`. Data cells at +32 (flag), +40, +48.
// RDX holds the candidate person at 70E9B2 and is not touched. Shared by SAN14_Convenience.dll and the fixtures.
static void layout_build_mediation_relay(unsigned char *relay,bool on,void *pass_target,void *reject_target) {
    static const unsigned char code[]={0x80,0x3D,0,0,0,0,0x00, 0x75,0x0A, 0x66,0x83,0xBA,0xBC,0x00,0x00,0x00,0x00, 0x75,0x06,
        0xFF,0x25,0,0,0,0, 0xFF,0x25,0,0,0,0};
    for(unsigned i=0;i<64;++i)relay[i]=0xCC;
    for(unsigned i=0;i<sizeof(code);++i)relay[i]=code[i];
    unsigned char *flag=relay+32,*pass=relay+40,*reject=relay+48;
    *flag=on?1:0;
    *reinterpret_cast<void **>(pass)=pass_target;*reinterpret_cast<void **>(reject)=reject_target;
    *reinterpret_cast<int32_t *>(relay+2)=static_cast<int32_t>(flag-(relay+7));
    *reinterpret_cast<int32_t *>(relay+21)=static_cast<int32_t>(pass-(relay+25));
    *reinterpret_cast<int32_t *>(relay+27)=static_cast<int32_t>(reject-(relay+31));
}
// Registry entry points that must never be hooked at their entry (design C-4); the fixture and the verify script
// check that no row above starts inside one of these.
static const uint32_t layout_blocked_entries[]={GA(GA_930D20),GA(GA_841660),GA(GA_6566E0),GA(GA_86E0C0),GA(GA_86C570),GA(GA_86E140),GA(GA_7FA230),GA(GA_302980),
    GA(GA_304640),GA(GA_1F09E0),GA(GA_1F0AD0),GA(GA_1F0DD0),GA(GA_6C43B0),GA(GA_6C4670)};
