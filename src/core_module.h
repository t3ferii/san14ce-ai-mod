#pragma once
// Module descriptors of the convenience module (docs/MODULARIZATION_PLAN.md W4-0). One row per install step; the activation table
// (src/module_table_convenience.inl) is generated from development/module_registry.json. Build/activation time only: nothing here
// runs per frame.
struct ModuleDesc {
    const char *name;
    unsigned char off_id;       // diagnostic feature id of the refusal log (0 = none), mirrors the id in `refused`
    bool (*wanted)();           // the former guard of the statement (evaluated when the row is reached)
    bool (*install)();          // false = refused
    void (*refused)();          // the former failure handling (feature_off / split_disable / log)
};
// FNV-1a over the row name plus a terminator: feature_order log and tests/module_registry_fixture.cpp use the same function.
static const unsigned kModuleHashSeed=2166136261u;
inline unsigned module_name_hash(const char *name,unsigned hash) {
    for(;*name;++name)hash=(hash^static_cast<unsigned char>(*name))*16777619u;
    return (hash^0xFFu)*16777619u;
}
