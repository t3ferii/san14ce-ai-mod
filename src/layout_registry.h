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
    {"speed_buttons",LR_VT,  GA(GA_1476FE0),GA(GA_8848E0),LF_CYW},   // 1  CChangeSpeedLayout +188; yuga checks this slot (VirtualQuery + Convenience)
    {"speed_buttons",LR_VT,  GA(GA_13D9728),GA(GA_44DEE0),LF_CY},    // 2  day progress frame
    {"speed_buttons",LR_JUMP,GA(GA_89ABF7), GA(GA_893A10),LF_CY},    // 3  std::function tail jump (E9, block offset 7)
    {"speed_buttons",LR_VT,  GA(GA_1477190),GA(GA_88E5D0),LF_CY},    // 4  popup hover
    // 5-6 lord_change
    {"lord_change",LR_CALL,GA(GA_5064FC),GA(GA_862220),LF_C},
    {"lord_change",LR_VT,  GA(GA_13E8118),GA(GA_535D90),LF_C},
    // 7-12 region_officers (fog 109-slot table: all six slots must live in our DLL; yields only for R1 RegionOfficer Face!=0)
    {"region_officers",LR_VT,GA(GA_139D378),GA(GA_1A9250),LF_CYW},
    {"region_officers",LR_VT,GA(GA_139D390),GA(GA_1A87D0),LF_CYW},
    {"region_officers",LR_VT,GA(GA_139D368),GA(GA_19C4A0),LF_CYW},
    {"region_officers",LR_VT,GA(GA_139CA08),GA(GA_1A91A0),LF_CYW},
    {"region_officers",LR_VT,GA(GA_139CA20),GA(GA_1A88A0),LF_CYW},
    {"region_officers",LR_VT,GA(GA_139C9F8),GA(GA_19C620),LF_CYW},
    // 13-15 ccz_title (+ the frame slot 13E4E30 the module already hooks, not in the 8.1 table: reported)
    {"ccz_title",LR_CALL,GA(GA_4EAC33), GA(GA_8685C0),LF_C},
    {"ccz_title",LR_VT,  GA(GA_13E7EA0),GA(GA_506ED0),LF_C},
    {"ccz_title",LR_VT,  GA(GA_13E7EA8),GA(GA_4FBB70),LF_C},
    {"ccz_title",LR_VT,  GA(GA_13E7EC0),GA(GA_50DEC0),LF_C},
    // 16 ccz_sprites (fog 109-slot table)
    {"ccz_sprites",LR_VT,GA(GA_13A3410),GA(GA_1C8240),LF_CW},
    // 17-19 layout_wait; the 86E0C0 entry patch is gone (8.3), layout_keep waits for 86E320 observation (P4)
    {"layout_wait",LR_CALL,GA(GA_678E27),GA(GA_86E920),LF_C},
    {"layout_wait",LR_CALL,GA(GA_86F355),GA(GA_86E920),LF_C},
    {"layout_wait",LR_CALL,GA(GA_870279),GA(GA_86E920),LF_C},
    // 19b layout_keep_wrap (user 2026-11-10 U35: the day-stall CPU trace had ~20% of each burst in loads through the 870470
    // thin wrapper, which lost layout_keep with 8.3): the E8 inside the wrapper (offset 2C, outside the 16-byte entry window).
    // R1 IC keeps the 0x33-byte wrapper body, so this owner yields whenever an R1 mod folder is present.
    {"layout_keep_wrap",LR_CALL,GA(GA_87049C),GA(GA_86E920),LF_CY},
    // U35 local29: native kind-10 face setters in selection and strategy UI.
    {"face_keep_portrait",LR_CALL,GA(GA_892234),GA(GA_85AE70),LF_CY},
    {"face_keep_portrait",LR_CALL,GA(GA_670AF1),GA(GA_85AE70),LF_CY},
    {"face_keep_portrait",LR_CALL,GA(GA_670B53),GA(GA_85AE70),LF_CY},
    {"face_keep_portrait",LR_CALL,GA(GA_670BA2),GA(GA_85AE70),LF_CY},
    // 21-23 fast_menu (8.7: yields to R1 FastMenu)
    {"fast_menu",LR_VT,GA(GA_13D9460),GA(GA_5727C0),LF_CY},
    {"fast_menu",LR_VT,GA(GA_143F550),GA(GA_5727C0),LF_CY},
    {"fast_menu",LR_VT,GA(GA_145A2C8),GA(GA_5727C0),LF_CY},
    // 24 stat_exp text call (R1 StatusColor identifies it by the 85C5DC return address: tail jump keeps it)
    {"stat_exp",LR_CALL,GA(GA_85E907),GA(GA_9332B0),LF_C},
    // 25 stat_exp: 15 (22FCE0 stat read, 85C420 chart) pairs in three windows
    {"stat_exp",LR_CALL,GA(GA_909896),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_9098A6),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_9098C1),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_9098D4),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_9098EF),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_909902),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_90991D),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_909930),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_90994B),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_90995E),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_90A419),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_90A429),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_90A445),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_90A458),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_90A474),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_90A487),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_90A4A3),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_90A4B6),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_90A4D2),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_90A4E5),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F7F5C),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_7F7F6C),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F7F85),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_7F7F96),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F7FAF),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_7F7FC0),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F7FD9),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_7F7FEA),GA(GA_85E750),LF_C},
    {"stat_exp",LR_CALL,GA(GA_7F8003),GA(GA_231770),LF_C},{"stat_exp",LR_CALL,GA(GA_7F8014),GA(GA_85E750),LF_C},
    // 26 dispatch_days
    {"dispatch_days",LR_CALL,GA(GA_89A373),GA(GA_9332B0),LF_C},
    // 27 damage_preview: the 7FA230 entry patch is replaced by its two callers (8.3; 7FA1FD crosses the block)
    {"damage_preview",LR_CALL,GA(GA_7F7C51),GA(GA_7FC700),LF_C},
    {"damage_preview",LR_CALL,GA(GA_7FC6CD),GA(GA_7FC700),LF_C},
    // 28-32 expert_links: the 6566E0 entry patch is replaced by its three callers (8.3)
    {"expert_links",LR_CALL,GA(GA_5F3957),GA(GA_658DF0),LF_C},
    {"expert_links",LR_CALL,GA(GA_5F8357),GA(GA_658DF0),LF_C},
    {"expert_links",LR_CALL,GA(GA_6610BD),GA(GA_658DF0),LF_C},
    {"expert_links",LR_CALL,GA(GA_638B8B),GA(GA_5C15D0),LF_C},
    {"expert_links",LR_CALL,GA(GA_5F3D3A),GA(GA_9309C0),LF_C},
    {"expert_links",LR_VT,GA(GA_141ED58),GA(GA_5BBA10),LF_C},
    {"expert_links",LR_VT,GA(GA_142C7B8),GA(GA_661470),LF_C},
    {"expert_links",LR_VT,GA(GA_142C748),GA(GA_660E20),LF_C},
    {"expert_links",LR_VT,GA(GA_142C780),GA(GA_661ED0),LF_C},
    {"expert_links",LR_VT,GA(GA_142C828),GA(GA_660F00),LF_C},
    {"expert_links",LR_VT,GA(GA_141EDB0),GA(GA_649CD0),LF_C},
    {"expert_links",LR_VT,GA(GA_141ED88),GA(GA_5A9A60),LF_C},
    {"expert_links",LR_VT,GA(GA_141EF18),GA(GA_5A9950),LF_C},
    // local17 expert_ingame (user 2026-11-09): CSetUpScenarioSeniorState slot 1 enter / slot 5 frame and the footer
    // button call inside the enter (862280(footer,1,1,-1)) where the 확인 button is added. Registered only when the key is on.
    {"expert_links",LR_VT,GA(GA_141F250),GA(GA_609100),LF_C},
    {"expert_links",LR_VT,GA(GA_141F270),GA(GA_60EF50),LF_C},
    {"expert_links",LR_CALL,GA(GA_609543),GA(GA_862280),LF_C},
    // 33-35 unified_dialogs
    {"unified_dialogs",LR_VT,GA(GA_13AF618),GA(GA_2FD830),LF_C},
    {"unified_dialogs",LR_VT,GA(GA_13AF640),GA(GA_259B10),LF_C},
    {"unified_dialogs",LR_VT,GA(GA_13AF608),GA(GA_2583C0),LF_C},
    // 36 advisor_hud
    {"advisor_hud",LR_VT,GA(GA_1478268),GA(GA_896710),LF_C},
    {"advisor_hud",LR_VT,GA(GA_1478280),GA(GA_87A690),LF_C},
    // 37-40 recruit_ui
    {"recruit_ui",LR_VT,GA(GA_13AF798),GA(GA_6C1040),LF_C},
    {"recruit_ui",LR_VT,GA(GA_13AF800),GA(GA_2C2BA0),LF_C},
    {"recruit_ui",LR_VT,GA(GA_1444070),GA(GA_6F3FF0),LF_C},
    {"recruit_ui",LR_VT,GA(GA_1443E78),GA(GA_735F60),LF_C},
    // 41-44 merit. merit_save keeps the warehouse sidecar independent of merit gameplay (existing guard split);
    // merit_prisoner is split out of the bundle (G4 exception) and yields to R1 PrisonerReturn.
    {"merit_save",LR_VT,GA(GA_13E9A30),GA(GA_50DD30),LF_C},
    {"merit_save",LR_VT,GA(GA_13E9188),GA(GA_50BD20),LF_C},
    {"merit_prisoner",LR_VT,GA(GA_13A9BF0),GA(GA_23AA60),LAYOUT_CHAIN_OK|LAYOUT_YIELD_IF_PEER},
    {"merit",LR_VT,GA(GA_1416690),GA(GA_90ACC0),LF_CW},               // yuga waits for this slot and chains behind it
    {"merit",LR_VT,GA(GA_1416518),GA(GA_87ABC0),LF_C},
    // (the traits9 captive CALL row, former 47, was removed with the captive_dialog feature, 2026-11-06 local10)
    // 49 collab (R1 ModPackBridge Collab) / 50 scenery (R1 SettingsHelpScenic): modpack record and scenery slots, Y only
    {"collab",LR_VT,GA(GA_13AA260),GA(GA_2404F0),LF_Y},
    {"collab",LR_VT,GA(GA_13A9E28),GA(GA_241530),LF_Y},
    {"scenery",LR_VT,GA(GA_13D5F90),GA(GA_423D00),LF_Y},
    {"scenery",LR_VT,GA(GA_144A278),GA(GA_724E40),LF_Y},
    // 52 domestic_selection (domestic AI node slots)
    {"domestic_selection",LR_VT,GA(GA_1349E98),GA(GA_36E80),LF_CY},
    {"domestic_selection",LR_VT,GA(GA_134F918),GA(GA_7B0B0),LF_CY},
    // 53 mediation_plus E8 sites (6D7D90 is handed to batteki/yuga, its filter moves to the 70E9C1 block hook)
    {"mediation_plus",LR_CALL,GA(GA_710DEC),GA(GA_713C30),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_6BCDF7),GA(GA_798260),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_799303),GA(GA_798260),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_6BCDFF),GA(GA_78FA70),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_79930B),GA(GA_78FA70),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_7373BB),GA(GA_6D3E70),LF_C},
    {"mediation_plus",LR_CALL,GA(GA_7373C5),GA(GA_6D2970),LF_C},
    // trait_hover: 152 vtable slots in the ModPack TraitHover blob table order (0x62A0) + 4 930D20 duel label calls (docs/HISTORY.md 1.2/1.7/3.2). N = row index inside the owner.
    {"trait_hover",LR_VT,GA(GA_13AFFA8),GA(GA_8CA1C0),LF_CY}, // 1 CPersonData 표 tick
    {"trait_hover",LR_VT,GA(GA_13AFFE0),GA(GA_5B6E10),LF_CY}, // 2 CPersonData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13B0918),GA(GA_8CA1C0),LF_CY}, // 3 CKingdomData 표 tick
    {"trait_hover",LR_VT,GA(GA_13B0950),GA(GA_5B6E10),LF_CY}, // 4 CKingdomData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13B0938),GA(GA_546A70),LF_CY}, // 5 CKingdomData 표 종료
    {"trait_hover",LR_VT,GA(GA_13DAEC0),GA(GA_8CA1C0),LF_CY}, // 6 CCityData 표 tick
    {"trait_hover",LR_VT,GA(GA_13DAEF8),GA(GA_5B6E10),LF_CY}, // 7 CCityData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13DAEE0),GA(GA_546A70),LF_CY}, // 8 CCityData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F2FA8),GA(GA_8CA1C0),LF_CY}, // 9 CForceData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F2FE0),GA(GA_5B6E10),LF_CY}, // 10 CForceData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F2FC8),GA(GA_546A70),LF_CY}, // 11 CForceData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F2DB8),GA(GA_8CA1C0),LF_CY}, // 12 CEditPersonData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F2DF0),GA(GA_5B6E10),LF_CY}, // 13 CEditPersonData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F2BC8),GA(GA_8CA1C0),LF_CY}, // 14 CItemData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F2C00),GA(GA_5B6E10),LF_CY}, // 15 CItemData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F2BE8),GA(GA_546A70),LF_CY}, // 16 CItemData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F29D8),GA(GA_8CA1C0),LF_CY}, // 17 CPersonalityData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F2A10),GA(GA_5B6E10),LF_CY}, // 18 CPersonalityData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F29F8),GA(GA_546A70),LF_CY}, // 19 CPersonalityData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F5DA0),GA(GA_8CA1C0),LF_CY}, // 20 CAdvantageData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F5DD8),GA(GA_5B6E10),LF_CY}, // 21 CAdvantageData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F5DC0),GA(GA_546A70),LF_CY}, // 22 CAdvantageData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F6B30),GA(GA_8CA1C0),LF_CY}, // 23 CTacticsData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F6B68),GA(GA_5B6E10),LF_CY}, // 24 CTacticsData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F6B50),GA(GA_546A70),LF_CY}, // 25 CTacticsData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F6940),GA(GA_8CA1C0),LF_CY}, // 26 CFormationData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F6978),GA(GA_5B6E10),LF_CY}, // 27 CFormationData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F6960),GA(GA_546A70),LF_CY}, // 28 CFormationData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F6750),GA(GA_8CA1C0),LF_CY}, // 29 CEditNewItemData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F6788),GA(GA_5B6E10),LF_CY}, // 30 CEditNewItemData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F6770),GA(GA_546A70),LF_CY}, // 31 CEditNewItemData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F6560),GA(GA_8CA1C0),LF_CY}, // 32 CFootholdData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F6598),GA(GA_5B6E10),LF_CY}, // 33 CFootholdData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F6580),GA(GA_546A70),LF_CY}, // 34 CFootholdData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F6370),GA(GA_8CA1C0),LF_CY}, // 35 CEditPersonalityData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F63A8),GA(GA_5B6E10),LF_CY}, // 36 CEditPersonalityData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F6390),GA(GA_546A70),LF_CY}, // 37 CEditPersonalityData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F6180),GA(GA_8CA1C0),LF_CY}, // 38 CEditPersonalityConditionData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F61B8),GA(GA_5B6E10),LF_CY}, // 39 CEditPersonalityConditionData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F61A0),GA(GA_546A70),LF_CY}, // 40 CEditPersonalityConditionData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F5F90),GA(GA_8CA1C0),LF_CY}, // 41 CEditPersonalityEffectData 표 tick
    {"trait_hover",LR_VT,GA(GA_13F5FC8),GA(GA_5B6E10),LF_CY}, // 42 CEditPersonalityEffectData 표 숨김
    {"trait_hover",LR_VT,GA(GA_13F5FB0),GA(GA_546A70),LF_CY}, // 43 CEditPersonalityEffectData 표 종료
    {"trait_hover",LR_VT,GA(GA_14250B8),GA(GA_8CA1C0),LF_CY}, // 44 CRankData 표 tick
    {"trait_hover",LR_VT,GA(GA_14250F0),GA(GA_5B6E10),LF_CY}, // 45 CRankData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14250D8),GA(GA_546A70),LF_CY}, // 46 CRankData 표 종료
    {"trait_hover",LR_VT,GA(GA_1424EC8),GA(GA_8CA1C0),LF_CY}, // 47 CPoemData 표 tick
    {"trait_hover",LR_VT,GA(GA_1424F00),GA(GA_5B6E10),LF_CY}, // 48 CPoemData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1424EE8),GA(GA_546A70),LF_CY}, // 49 CPoemData 표 종료
    {"trait_hover",LR_VT,GA(GA_1424CD8),GA(GA_8CA1C0),LF_CY}, // 50 CSceneryData 표 tick
    {"trait_hover",LR_VT,GA(GA_1424D10),GA(GA_5B6E10),LF_CY}, // 51 CSceneryData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1424CF8),GA(GA_546A70),LF_CY}, // 52 CSceneryData 표 종료
    {"trait_hover",LR_VT,GA(GA_1424AE8),GA(GA_8CA1C0),LF_CY}, // 53 CAreaData 표 tick
    {"trait_hover",LR_VT,GA(GA_1424B20),GA(GA_5B6E10),LF_CY}, // 54 CAreaData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1424B08),GA(GA_546A70),LF_CY}, // 55 CAreaData 표 종료
    {"trait_hover",LR_VT,GA(GA_14272D0),GA(GA_8CA1C0),LF_CY}, // 56 CDistrictData 표 tick
    {"trait_hover",LR_VT,GA(GA_1427308),GA(GA_5B6E10),LF_CY}, // 57 CDistrictData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14272F0),GA(GA_546A70),LF_CY}, // 58 CDistrictData 표 종료
    {"trait_hover",LR_VT,GA(GA_14270E0),GA(GA_8CA1C0),LF_CY}, // 59 SResolutionDispInfo 표 tick
    {"trait_hover",LR_VT,GA(GA_1427118),GA(GA_5B6E10),LF_CY}, // 60 SResolutionDispInfo 표 숨김
    {"trait_hover",LR_VT,GA(GA_1427100),GA(GA_546A70),LF_CY}, // 61 SResolutionDispInfo 표 종료
    {"trait_hover",LR_VT,GA(GA_1426EF0),GA(GA_8CA1C0),LF_CY}, // 62 CDoctrineData 표 tick
    {"trait_hover",LR_VT,GA(GA_1426F28),GA(GA_5B6E10),LF_CY}, // 63 CDoctrineData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1426F10),GA(GA_546A70),LF_CY}, // 64 CDoctrineData 표 종료
    {"trait_hover",LR_VT,GA(GA_1426D00),GA(GA_8CA1C0),LF_CY}, // 65 CPolicyData 표 tick
    {"trait_hover",LR_VT,GA(GA_1426D38),GA(GA_5B6E10),LF_CY}, // 66 CPolicyData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1426D20),GA(GA_546A70),LF_CY}, // 67 CPolicyData 표 종료
    {"trait_hover",LR_VT,GA(GA_144C258),GA(GA_8CA1C0),LF_CY}, // 68 CCampaignData 표 tick
    {"trait_hover",LR_VT,GA(GA_144C290),GA(GA_5B6E10),LF_CY}, // 69 CCampaignData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144C278),GA(GA_546A70),LF_CY}, // 70 CCampaignData 표 종료
    {"trait_hover",LR_VT,GA(GA_144BFF8),GA(GA_8CA1C0),LF_CY}, // 71 CPersonGroupUIData 표 tick
    {"trait_hover",LR_VT,GA(GA_144C030),GA(GA_5B6E10),LF_CY}, // 72 CPersonGroupUIData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144C018),GA(GA_546A70),LF_CY}, // 73 CPersonGroupUIData 표 종료
    {"trait_hover",LR_VT,GA(GA_1448820),GA(GA_8CA1C0),LF_CY}, // 74 CPostData 표 tick
    {"trait_hover",LR_VT,GA(GA_1448858),GA(GA_5B6E10),LF_CY}, // 75 CPostData 표 숨김
    {"trait_hover",LR_VT,GA(GA_1448840),GA(GA_546A70),LF_CY}, // 76 CPostData 표 종료
    {"trait_hover",LR_VT,GA(GA_144DE90),GA(GA_8CA1C0),LF_CY}, // 77 CDoctrinePanelData 표 tick
    {"trait_hover",LR_VT,GA(GA_144DEC8),GA(GA_5B6E10),LF_CY}, // 78 CDoctrinePanelData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144DEB0),GA(GA_546A70),LF_CY}, // 79 CDoctrinePanelData 표 종료
    {"trait_hover",LR_VT,GA(GA_144E460),GA(GA_8CA1C0),LF_CY}, // 80 CForcePolicyData 표 tick
    {"trait_hover",LR_VT,GA(GA_144E498),GA(GA_5B6E10),LF_CY}, // 81 CForcePolicyData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144E480),GA(GA_546A70),LF_CY}, // 82 CForcePolicyData 표 종료
    {"trait_hover",LR_VT,GA(GA_144E270),GA(GA_8CA1C0),LF_CY}, // 83 CGemData 표 tick
    {"trait_hover",LR_VT,GA(GA_144E2A8),GA(GA_5B6E10),LF_CY}, // 84 CGemData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144E290),GA(GA_546A70),LF_CY}, // 85 CGemData 표 종료
    {"trait_hover",LR_VT,GA(GA_144E080),GA(GA_8CA1C0),LF_CY}, // 86 CForeignData 표 tick
    {"trait_hover",LR_VT,GA(GA_144E0B8),GA(GA_5B6E10),LF_CY}, // 87 CForeignData 표 숨김
    {"trait_hover",LR_VT,GA(GA_144E0A0),GA(GA_546A70),LF_CY}, // 88 CForeignData 표 종료
    {"trait_hover",LR_VT,GA(GA_145D4E0),GA(GA_8CA1C0),LF_CY}, // 89 CArmyUnitData 표 tick
    {"trait_hover",LR_VT,GA(GA_145D518),GA(GA_5B6E10),LF_CY}, // 90 CArmyUnitData 표 숨김
    {"trait_hover",LR_VT,GA(GA_145D500),GA(GA_546A70),LF_CY}, // 91 CArmyUnitData 표 종료
    {"trait_hover",LR_VT,GA(GA_145D6D0),GA(GA_8CA1C0),LF_CY}, // 92 CFacilityData 표 tick
    {"trait_hover",LR_VT,GA(GA_145D708),GA(GA_5B6E10),LF_CY}, // 93 CFacilityData 표 숨김
    {"trait_hover",LR_VT,GA(GA_145D6F0),GA(GA_546A70),LF_CY}, // 94 CFacilityData 표 종료
    {"trait_hover",LR_VT,GA(GA_14B3458),GA(GA_8CA1C0),LF_CY}, // 95 CGateData 표 tick
    {"trait_hover",LR_VT,GA(GA_14B3490),GA(GA_5B6E10),LF_CY}, // 96 CGateData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14B3478),GA(GA_546A70),LF_CY}, // 97 CGateData 표 종료
    {"trait_hover",LR_VT,GA(GA_14B3268),GA(GA_8CA1C0),LF_CY}, // 98 CForeignTechnologyData 표 tick
    {"trait_hover",LR_VT,GA(GA_14B32A0),GA(GA_5B6E10),LF_CY}, // 99 CForeignTechnologyData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14B3288),GA(GA_546A70),LF_CY}, // 100 CForeignTechnologyData 표 종료
    {"trait_hover",LR_VT,GA(GA_14B3078),GA(GA_8CA1C0),LF_CY}, // 101 CCreditRankData 표 tick
    {"trait_hover",LR_VT,GA(GA_14B30B0),GA(GA_5B6E10),LF_CY}, // 102 CCreditRankData 표 숨김
    {"trait_hover",LR_VT,GA(GA_14B3098),GA(GA_546A70),LF_CY}, // 103 CCreditRankData 표 종료
    {"trait_hover",LR_VT,GA(GA_13B01D0),GA(GA_5112F0),LF_CY}, // 104 CPersonData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13B0B08),GA(GA_5112F0),LF_CY}, // 105 CKingdomData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13DB0E8),GA(GA_5112F0),LF_CY}, // 106 CCityData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F3198),GA(GA_5112F0),LF_CY}, // 107 CForceData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F3380),GA(GA_5112F0),LF_CY}, // 108 CEditPersonData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F3568),GA(GA_5112F0),LF_CY}, // 109 CItemData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F3750),GA(GA_5112F0),LF_CY}, // 110 CPersonalityData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F7A78),GA(GA_5112F0),LF_CY}, // 111 CAdvantageData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F6D20),GA(GA_5112F0),LF_CY}, // 112 CTacticsData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F6F08),GA(GA_5112F0),LF_CY}, // 113 CFormationData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F70F0),GA(GA_5112F0),LF_CY}, // 114 CEditNewItemData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F72D8),GA(GA_5112F0),LF_CY}, // 115 CFootholdData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F74C0),GA(GA_5112F0),LF_CY}, // 116 CEditPersonalityData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F76A8),GA(GA_5112F0),LF_CY}, // 117 CEditPersonalityConditionData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_13F7890),GA(GA_5112F0),LF_CY}, // 118 CEditPersonalityEffectData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14252A8),GA(GA_5112F0),LF_CY}, // 119 CRankData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1425490),GA(GA_5112F0),LF_CY}, // 120 CPoemData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1425678),GA(GA_5112F0),LF_CY}, // 121 CSceneryData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1425860),GA(GA_5112F0),LF_CY}, // 122 CAreaData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14274C0),GA(GA_5112F0),LF_CY}, // 123 CDistrictData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14276A8),GA(GA_5112F0),LF_CY}, // 124 SResolutionDispInfo 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1427890),GA(GA_5112F0),LF_CY}, // 125 CDoctrineData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1427A78),GA(GA_5112F0),LF_CY}, // 126 CPolicyData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144BE10),GA(GA_5112F0),LF_CY}, // 127 CCampaignData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144C4B8),GA(GA_5112F0),LF_CY}, // 128 CPersonGroupUIData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144C710),GA(GA_5112F0),LF_CY}, // 129 CPostData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144EC08),GA(GA_5112F0),LF_CY}, // 130 CDoctrinePanelData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144E650),GA(GA_5112F0),LF_CY}, // 131 CForcePolicyData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144E838),GA(GA_5112F0),LF_CY}, // 132 CGemData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_144EA20),GA(GA_5112F0),LF_CY}, // 133 CForeignData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_145DAA8),GA(GA_5112F0),LF_CY}, // 134 CArmyUnitData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_145D8C0),GA(GA_5112F0),LF_CY}, // 135 CFacilityData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14B3648),GA(GA_5112F0),LF_CY}, // 136 CGateData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14B3830),GA(GA_5112F0),LF_CY}, // 137 CForeignTechnologyData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_14B3A18),GA(GA_5112F0),LF_CY}, // 138 CCreditRankData 표 본문 tick
    {"trait_hover",LR_VT,GA(GA_1463070),GA(GA_8CA1C0),LF_CY}, // 139 CDuelPersonInfoLayout 일기토 tick
    {"trait_hover",LR_VT,GA(GA_1463550),GA(GA_8CA1C0),LF_CY}, // 140 CDuelGaugeLayout 일기토 tick
    {"trait_hover",LR_VT,GA(GA_14BBD78),GA(GA_8CA1C0),LF_CY}, // 141 CInfoSummaryPersonality 요약 tick
    {"trait_hover",LR_VT,GA(GA_14BBD98),GA(GA_926FE0),LF_CY}, // 142 CInfoSummaryPersonality 요약 종료
    {"trait_hover",LR_VT,GA(GA_14BBDB0),GA(GA_916890),LF_CY}, // 143 CInfoSummaryPersonality 요약 숨김
    {"trait_hover",LR_VT,GA(GA_13EF168),GA(GA_8CA1C0),LF_CY}, // 144 CEditNewPersonalityLayout 편집기 tick
    {"trait_hover",LR_VT,GA(GA_13EF1A0),GA(GA_86BBD0),LF_CY}, // 145 CEditNewPersonalityLayout 편집기 숨김
    {"trait_hover",LR_VT,GA(GA_13EF188),GA(GA_87A690),LF_CY}, // 146 CEditNewPersonalityLayout 편집기 종료
    {"trait_hover",LR_VT,GA(GA_13AFFC8),GA(GA_546A70),LF_CY}, // 147 CPersonData 표 종료
    {"trait_hover",LR_VT,GA(GA_13F2DD8),GA(GA_546A70),LF_CY}, // 148 CEditPersonData 표 종료
    {"trait_hover",LR_VT,GA(GA_1463090),GA(GA_87A690),LF_CY}, // 149 CDuelPersonInfoLayout 일기토 종료
    {"trait_hover",LR_VT,GA(GA_14630A8),GA(GA_86BBD0),LF_CY}, // 150 CDuelPersonInfoLayout 일기토 숨김
    {"trait_hover",LR_VT,GA(GA_1463570),GA(GA_87A690),LF_CY}, // 151 CDuelGaugeLayout 일기토 종료
    {"trait_hover",LR_VT,GA(GA_1463588),GA(GA_86BBD0),LF_CY}, // 152 CDuelGaugeLayout 일기토 숨김
    {"trait_hover",LR_CALL,GA(GA_809461),GA(GA_9332B0),LF_CY}, // 153 CDuelPersonInfoLayout 갱신 개성 라벨 B
    {"trait_hover",LR_CALL,GA(GA_8097C1),GA(GA_9332B0),LF_CY}, // 154 CDuelPersonInfoLayout 갱신 개성 라벨 B
    {"trait_hover",LR_CALL,GA(GA_80A4A2),GA(GA_9332B0),LF_CY}, // 155 CDuelGaugeLayout 갱신 개성 라벨 A
    {"trait_hover",LR_CALL,GA(GA_80A78C),GA(GA_9332B0),LF_CY}, // 156 CDuelGaugeLayout 갱신 개성 라벨 A
    // battle_deaths (docs/HISTORY.md): cut-in visibility (368360) x2 + death record (295AA0) x2; the 295C5F branch is layout_battle_deaths_span.
    {"battle_deaths",LR_CALL,GA(GA_25B022),GA(GA_36A850),LF_CY}, // 157 258EE0 전투 전사 컷인 화면 판정
    {"battle_deaths",LR_CALL,GA(GA_27F716),GA(GA_36A850),LF_CY}, // 158 27D4B0 전사 컷인 화면 판정
    {"battle_deaths",LR_CALL,GA(GA_25B0EB),GA(GA_2979B0),LF_CY}, // 159 258EE0 전사 기록 호출(범위)
    {"battle_deaths",LR_CALL,GA(GA_27F7A9),GA(GA_2979B0),LF_CY}, // 160 27D4B0 전사 기록 호출(범위)
    // 258-261 tactic_priority (sortie setup footer button, per-frame pick, leave, departure record)
    {"tactic_priority",LR_CALL,GA(GA_7DE5BD),GA(GA_862220),LF_C},
    {"tactic_priority",LR_VT,  GA(GA_145B8D8),GA(GA_7E18E0),LF_C},
    {"tactic_priority",LR_VT,  GA(GA_145B8C0),GA(GA_7DC200),LF_C},
    {"tactic_priority",LR_CALL,GA(GA_7EE1C4),GA(GA_1EDC30),LF_C},
    // local17 drag_learn_multi (user 2026-11-09): CStrategyLearnState slot 5 frame. The wrapper runs the original frame
    // first and then rewrites the 습득 preview pane (외 N명 name, 전법서/명령서 totals) of a dragged multi selection.
    {"drag_learn",LR_VT,GA(GA_1447540),GA(GA_737050),LF_C},
// local17 auto_buttons (PW36b, user 2026-11-09): the two toggles on the main screen 00_02_00_mein_pk (CGameLayout).
// Slot 49 setup = 4452B0, slot 0 destroy = 43BE70, and the two native icon builds at 44616D / 445FA6 (8685C0).
// Registered only while auto_search or auto_reward is on (native_auto_commands.inl install()).
{"auto_buttons",LR_VT,GA(GA_13D9268),GA(GA_4452B0),LF_C},
{"auto_buttons",LR_VT,GA(GA_13D90E0),GA(GA_43BE70),LF_C},
{"auto_buttons",LR_CALL,GA(GA_44616D),GA(GA_8685C0),LF_C},
{"auto_buttons",LR_CALL,GA(GA_445FA6),GA(GA_8685C0),LF_C},
};
static const unsigned layout_row_count=sizeof(layout_rows)/sizeof(layout_rows[0]);
// Block / span / byte rows that are not a vtable slot or call site. expect[] is the original bytes.
struct LayoutPatchRow {const char *owner;uint32_t rva,len;uint32_t flags;};
// 53: 70E9C1 `cmp word [rdx+0BCh],0 ; jnz 70EA11` inside the 70E9A0 candidate filter. Refuse when the bytes differ.
static const uint8_t layout_mediation_expect[10]={0x66,0x83,0xBA,0xBC,0x00,0x00,0x00,0x00,0x75,0x46};
static const LayoutPatchRow layout_mediation_row={"mediation_plus",GA(GA_710CD1),10,LAYOUT_REFUSE_IF_FOREIGN|LAYOUT_ALL_OR_NONE};
// battle_deaths: 295C5F `test ebx,ebx ; je +8CE` (8 bytes, offset 0xF in its 16-byte line, so api_hook_block would refuse it). Written as a span (E9 + NOP3).
static const uint8_t layout_battle_deaths_expect[8]={0x85,0xDB,0x0F,0x84,0xCE,0x08,0x00,0x00};   // test ebx,ebx; je +8CE (both builds)
static const LayoutPatchRow layout_battle_deaths_span={"battle_deaths",GA(GA_297B6F),8,
    LAYOUT_YIELD_IF_PEER|LAYOUT_REFUSE_IF_FOREIGN|LAYOUT_ALL_OR_NONE};
// Dynamic-source patches (manifest tables of the converted traits9 mod): spans are E9 + NOP fill written block by block,
// bytes are plain value patches. Owner traits9, YIELD_IF_PEER (R1 Traits9 writes the same places), never chained.
enum LayoutPatchKind : unsigned char {LP_SPAN=4,LP_BYTES=5};
struct LayoutPatchSite {unsigned char kind;uint32_t rva,len;};
static const LayoutPatchSite layout_traits9_sites[]={
    {LP_BYTES,GA(GA_4A90B3),8},{LP_BYTES,GA(GA_4A96D3),8},{LP_BYTES,GA(GA_226FBF),4},{LP_BYTES,GA(GA_2320AA),3},{LP_BYTES,GA(GA_234737),4},
    {LP_SPAN,GA(GA_8E1628),7},{LP_SPAN,GA(GA_8E1685),6},{LP_SPAN,GA(GA_909734),17},
    {LP_SPAN,GA(GA_8D0320),14},{LP_SPAN,GA(GA_8E25D8),10},{LP_SPAN,GA(GA_8E2660),6},{LP_SPAN,GA(GA_90A23C),16},
    {LP_SPAN,GA(GA_2C13B3),15}
};
static const unsigned layout_traits9_site_count=sizeof(layout_traits9_sites)/sizeof(layout_traits9_sites[0]);
// Machine code of the 70E9C1 relay (design 8.4), 64 bytes: `cmp byte [flag],0 ; jne pass ; cmp word [rdx+0BCh],0 ;
// jne reject ; pass: jmp [pass_target] ; reject: jmp [reject_target]`. Data cells at +32 (flag), +40, +48.
// RDX holds the candidate person at 70E9B2 and is not touched. Shared by SAN14_Convenience.dll and the fixtures.
inline void layout_build_mediation_relay(unsigned char *relay,bool on,void *pass_target,void *reject_target) {
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
static const uint32_t layout_blocked_entries[]={GA(GA_9332B0),GA(GA_843B60),GA(GA_658DF0),GA(GA_870470),GA(GA_86E920),GA(GA_8704F0),GA(GA_7FC700),GA(GA_304D70),
    GA(GA_306A30),GA(GA_1F1DA0),GA(GA_1F1E90),GA(GA_1F2190),GA(GA_6C6B10),GA(GA_6C6DD0)};
