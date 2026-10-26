// [diagnostics] cost_profile only (2026-10-25, user: 0-day wait after 전략중 in 영웅난무). Times the
// original face setter 1B91E0(face node, person) at each of its 7 direct call sites; results go to the
// function_cost CSV as face_1B91E0_<site> (calls, total, max per 1 s window). No behaviour change.
// Evidence (REA carrier REA_Analysis/turnstart_20261025, unpacked 1fbe6be7...): 1B91E0 sets five face
// images (sizes 7/0x10/0x18/0x1E/0x20) through 9307B0; CPU samples of the 0-day wait put ~0.9 s of
// 4.7 s in its 9307B0->930840->872170->8A6DE0->8AB113 path. Army flag panels reach it via 1B3523.
// Sites (caller): 1990EF/199203 (198FA0), 199727 (1995E0), 19A0A1/19A0C1 (19A080), 1A95CB (1A957D
// chunk), 1B3523 (1B3450). Args: RCX, RDX only (1B91E0 prologue); callers ignore RAX.
namespace face_timing {
static const unsigned kFace=0x1B91E0,kSites[7]={0x1990EF,0x199203,0x199727,0x19A0A1,0x19A0C1,0x1A95CB,0x1B3523};
using Face=void(__fastcall *)(Obj,Obj);
static void __fastcall s0(Obj f,Obj p) {COST_SCOPE("face_1B91E0_1990EF");fn<Face>(kFace)(f,p);}
static void __fastcall s1(Obj f,Obj p) {COST_SCOPE("face_1B91E0_199203");fn<Face>(kFace)(f,p);}
static void __fastcall s2(Obj f,Obj p) {COST_SCOPE("face_1B91E0_199727");fn<Face>(kFace)(f,p);}
static void __fastcall s3(Obj f,Obj p) {COST_SCOPE("face_1B91E0_19A0A1");fn<Face>(kFace)(f,p);}
static void __fastcall s4(Obj f,Obj p) {COST_SCOPE("face_1B91E0_19A0C1");fn<Face>(kFace)(f,p);}
static void __fastcall s5(Obj f,Obj p) {COST_SCOPE("face_1B91E0_1A95CB");fn<Face>(kFace)(f,p);}
static void __fastcall s6(Obj f,Obj p) {COST_SCOPE("face_1B91E0_1B3523");fn<Face>(kFace)(f,p);}
static CallLink links[7];
static bool install() {
    // 1B3523 belongs to face_keep when it is installed (its wrapper keeps the same timing key).
    const unsigned count=face_keep::installed?6:7;
    for(unsigned i=0;i<count;++i) {const unsigned site=kSites[i];
        if(image[site]!=0xE8 || site+5+*reinterpret_cast<int *>(image+site+1)!=kFace) {log_event("face_timing_guard_rejected",site);return false;}}
    Obj relay=allocate_near(image+kFace);if(!relay)return false;
    const Face wrappers[7]={s0,s1,s2,s3,s4,s5,s6};
    DWORD old;bool ok=true;
    for(unsigned i=0;i<count && ok;++i) {absolute_jump(relay+32*i,reinterpret_cast<void *>(wrappers[i]));ok=call_prepare(links[i],kSites[i],relay+32*i);}
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,count*32)) {
        VirtualFree(relay,0,MEM_RELEASE);log_event("face_timing_prepare_rejected");return false;
    }
    for(unsigned i=0;i<count;++i)if(!call_publish(links[i])) {log_event("face_timing_publish_rejected",i);while(i)call_restore(links[--i]);return false;}
    log_event("face_timing_installed");return true;
}
}
