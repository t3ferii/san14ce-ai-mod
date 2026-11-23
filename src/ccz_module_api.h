#pragma once
// SAN14_CCZ.dll (조조전 탭, user 2026-11-01). SAN14_Convenience.dll loads it only when a [ccz] feature is on,
// keeps the Present hook and the 169F8B CALL publication, and forwards to these entries.
// The module owns only its army-display vtable slot (13A03D0) and its render-target hook.
#include <d3d11.h>
static const unsigned ccz_module_abi=10; // 10 (2026-11-22): band duel entry 2CCC60 (the duel generator sees bands as bandits); 9 (2026-11-20): band capture entry 2801C0 + bands->exchange/fire (kind traits on normal and fire damage); 8 (2026-11-20): band max morale entry 230460 + bands->power (morale/kind combat factor); 7 (2026-11-19): command-window entry 4522B0 (bands leave the selection); 6 (2026-11-19): install_idle_flags (idle band flags without 조조전 병종 그림); 5 (2026-11-18): band flag portrait (face texture entry 8AD530); 4: bands; 3: far_keep/sprite_fallback.
using CczLog=void (__cdecl *)(const char *,unsigned,unsigned,unsigned);
// 종군 중립군(무리): the convenience module publishes these original entries and jumps to hooks[i]; the gates it hands to
// configure() run the displaced prologue and continue in the original. Order = native_ccz_bands.inl band_entries.
// kBandFaceTexture (8AD530, face image from zp1 bytes) is published only in portrait mode (조조전 병종 그림 not installed).
enum CczBandHook : unsigned {kBandArmyForce,kBandPaint,kBandSupply,kBandRemove,kBandPaintWrite,kBandMorale,kBandDayMove,
                             kBandSetAction,kBandSetTarget,kBandFaceTexture,kBandUnitTarget,kBandMaxMorale,kBandCapture,kBandDuel,kBandHookCount};
// kBandDuel (2CCC60, the once-a-day duel generator, 2026-11-22) is always published with the bands: inside it a band army answers its
// raw force 51, so the original 1..45 filter keeps band commanders out of every duel as candidate, opponent and substitute (user
// 「의용병무리는 일기토에서 아예빼버려 걸지도 걸리지도 않게」).
// kBandCapture (2801C0, capture roll (commander, captor) -> bool, 2026-11-20) is always published with the bands: a band commander
// is never captured, so 25A880 deletes the generic officer (user "포로나 그런거 되면 야전에서 즉시 사라져야해").
// kBandUnitTarget (4522B0, the selection list -> CStrategyUnitTargetState command window) is always published with the bands.
// kBandMaxMorale (230460, army -> max morale, 2026-11-20) is always published with the bands: elite bands answer 130.
struct CczBands {
    void *hooks[kBandHookCount];
    // flags: 1 on, 2 diagnostic idle record, 4 setter hooks published, 8 flag portrait (face texture hook published),
    // bits 8..11 [ccz] band_troops 1..3 (0 = 500->5000; an older module ignores them, so the ABI stays)
    // bits 12..15 [ccz] band_morale 1..3 (kill/rout morale x0.5 / x1 / x1.5; 0 = x1)
    bool (__cdecl *configure)(void *const *gates,unsigned flags);
    unsigned (__cdecl *save)(unsigned char *out,unsigned cap);       // bytes written, 0 = buffer too small
    bool (__cdecl *load)(const unsigned char *in,unsigned length);
    void (__cdecl *reset)();
    // 2026-11-20: factor on the original common combat multiplier 2AAF30 (attack and defense input) for one army; 1 for every
    // army that is not a live band. Called by the convenience module's 2AAF30 hook (battle_rules) on the game threads.
    float (__cdecl *power)(unsigned char *army);
    // 2026-11-20 kind traits. exchange: after the original one-direction normal damage 16B180 of the exchange stage 16B730 (CALL
    // 16B86C), may scale *damage (cavalry terrain, rattan armour) and confuse the target (poison/sorcery); from/to are the
    // participant objects (an army only when it is one). fire: tile-fire troop damage of an army (CALL 43EB3E -> 179DE0),
    // returns the amount to apply. Both run on the game's day-frame thread.
    void (__cdecl *exchange)(unsigned char *from,unsigned char *to,int *damage);
    int (__cdecl *fire)(unsigned char *army,int amount);
};
struct CczApi {
    unsigned size,abi;
    void (__cdecl *initialize)(unsigned char *image,CczLog log,void (__cdecl *overlay_install)());
    bool (__cdecl *sprites_guards)();
    bool (__cdecl *install_sprites)(bool hide,bool depth,bool far_keep,bool fallback);
    bool (__cdecl *needs_present)();
    void (__cdecl *on_present)(ID3D11Device *,ID3D11DeviceContext *,ID3D11Texture2D *,const D3D11_TEXTURE2D_DESC &);
    void (__cdecl *configure_combat)(bool block,bool critical);
    int (__fastcall *combat_fix)(unsigned char *manager,unsigned char *pair,unsigned char *output);
    const CczBands *bands;
    // 2026-11-19 의용병 무리 with 조조전 병종 그림 off: the renderer draws only the idle band flags (no army-display hook, native models).
    bool (__cdecl *install_idle_flags)(bool depth);
};
using GetCczApi=const CczApi *(__cdecl *)(unsigned);
