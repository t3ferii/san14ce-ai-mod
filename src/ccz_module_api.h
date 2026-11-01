#pragma once
// SAN14_CCZ.dll (조조전 탭, user 2026-11-01). SAN14_Convenience.dll loads it only when a [ccz] feature is on,
// keeps the Present hook and the 169F8B CALL publication, and forwards to these entries.
// The module owns only its army-display vtable slot (13A03D0) and its render-target hook.
#include <d3d11.h>
static const unsigned ccz_module_abi=1;
using CczLog=void (__cdecl *)(const char *,unsigned,unsigned,unsigned);
struct CczApi {
    unsigned size,abi;
    void (__cdecl *initialize)(unsigned char *image,CczLog log,void (__cdecl *overlay_install)());
    bool (__cdecl *sprites_guards)();
    bool (__cdecl *install_sprites)(bool hide,bool depth);
    bool (__cdecl *needs_present)();
    void (__cdecl *on_present)(ID3D11Device *,ID3D11DeviceContext *,ID3D11Texture2D *,const D3D11_TEXTURE2D_DESC &);
    void (__cdecl *configure_combat)(bool block,bool critical);
    int (__fastcall *combat_fix)(unsigned char *manager,unsigned char *pair,unsigned char *output);
};
using GetCczApi=const CczApi *(__cdecl *)(unsigned);
