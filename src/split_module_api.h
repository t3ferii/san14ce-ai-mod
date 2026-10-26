#pragma once
// One coordinator validates the original image before either module publishes.
// These calls manage hooks only; no worker calls a game function.
static const unsigned split_module_abi=1;
// AI-CONTRACT [ABI]: Field order, calling convention, sizeof checks and split_module_abi are a cross-DLL contract. Change both producer and consumer together; test bad-ABI and partial-publication rollback cases.
struct SplitCombatApi {
    unsigned size,abi;
    bool (__cdecl *prepare)(unsigned char *);
    bool (__cdecl *publish)();
    void (__cdecl *rollback)();
    void (__cdecl *activate)();
    void (__cdecl *departure_scope)(int);
};
using GetSplitCombatApi=const SplitCombatApi *(__cdecl *)(unsigned);
using InitializeSplitConvenience=bool (__cdecl *)(unsigned,const SplitCombatApi *);
#ifdef SPLIT_MODULE_FIXTURE
struct SplitFixtureApi {
    bool (__cdecl *prepare)(unsigned char *,const SplitCombatApi *);
    bool (__cdecl *publish)();void (__cdecl *rollback)();
    void (__cdecl *activate)();
    unsigned (__cdecl *scope)();
    unsigned long long (__cdecl *departure)();
    void (__cdecl *fail_after)(unsigned);
};
#endif
