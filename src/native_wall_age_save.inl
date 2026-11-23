// [combat] wall_age_pick / wall_age_siege, the save side (user 2026-11-21 「중립건물의 id 같은거에 우리 모드로 타겟된 횟수를 기억하고
// 건설되고 경과된 순을 기록하는거야 (세이브 파일에 저장)」, docs/NEUTRAL_WALL_AGE.md A-2). SAN14_CombatAI.dll owns and writes the record
// table (wall_age_shared.h) and exports it as SAN14CombatWallAge; this convenience module only saves and restores it in the
// sidecar <save>.<sha256>.wallage (plugin order 90). No export (combat DLL absent, both switches off, other version) = the
// plugin is inactive and the records live in memory only; the original save is never touched.
#include "wall_age_shared.h"
namespace wall_age_save {
static WallAgeTable *shared;
static bool looked;
// The combat DLL is loaded before the first save or load; asked once, then kept.
static WallAgeTable *table_now() {
    if (shared || looked) return shared;
    HMODULE combat = GetModuleHandleW(L"SAN14_CombatAI.dll");
    if (!combat) return nullptr;   // not loaded yet: ask again next time
    looked = true;
    auto get = reinterpret_cast<GetWallAgeTable>(GetProcAddress(combat, "SAN14CombatWallAge"));
    WallAgeTable *t = get ? get(kWallAgeVersion) : nullptr;
    if (t && t->size == sizeof(WallAgeTable) && t->version == kWallAgeVersion) shared = t;
    log_event(shared ? "wall_age_save_ready" : "wall_age_save_missing", get ? 1u : 0u, t ? 1u : 0u, 0);
    return shared;
}
static std::vector<WallAgeRecord> saved;
static LONG saved_day = -1;
static SidecarCache side_cache;
// Save phase 1: the live records under the table lock (no game calls).
static void side_snapshot() { COST_SCOPE("wall_age_save:snapshot");
    saved.assign(kWallAgeIds, WallAgeRecord{});
    saved_day = -1;
    WallAgeTable *t = table_now();
    if (!t) return;
    AcquireSRWLockShared(&t->lock);
    memcpy(saved.data(), t->rec, sizeof(t->rec));
    saved_day = t->last_day;
    ReleaseSRWLockShared(&t->lock);
}
static bool side_write(const merit::SaveCoordinator::Completed &job, save_host::Written &out) { COST_SCOPE("wall_age_save:write");
    if (saved.size() != kWallAgeIds) saved.assign(kWallAgeIds, WallAgeRecord{});
    const std::vector<uint8_t> bytes = wall_age_codec::encode(job.binding.data(), saved_day, saved.data(), kWallAgeIds);
    return save_host::store(self_module, save_host::Job{job.name, job.binding}, L"wallage", bytes, side_cache, out);
}
// Load phase 1: the records of the world being left are dropped.
static void side_begin_load() {
    side_cache.clear();
    WallAgeTable *t = table_now();
    if (!t) return;
    AcquireSRWLockExclusive(&t->lock);
    for (WallAgeRecord &r : t->rec) r = WallAgeRecord{};
    t->last_day = -1;
    InterlockedIncrement(&t->epoch);
    ReleaseSRWLockExclusive(&t->lock);
}
// Load phase 3: filled only when the binding matches; last_day comes back too, so the next reconcile (same day or later) keeps
// the records instead of clearing them as an earlier date.
static bool side_restore(const merit::SaveCoordinator::Completed *job) { COST_SCOPE("wall_age_save:restore");
    WallAgeTable *t = table_now();
    bool restored = false;
    unsigned count = 0;
    if (job && t) {
        wchar_t path[MAX_PATH];
        if (save_host::path(self_module, save_host::Job{job->name, job->binding}, L"wallage", path, MAX_PATH, false)) {
            HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER length = {};
                const LONGLONG limit = static_cast<LONGLONG>(wall_age_codec::kHead + kWallAgeIds * wall_age_codec::kEntry);
                if (GetFileSizeEx(file, &length) && length.QuadPart >= static_cast<LONGLONG>(wall_age_codec::kHead) && length.QuadPart <= limit) {
                    std::vector<uint8_t> bytes(static_cast<size_t>(length.QuadPart), 0);
                    DWORD got = 0;
                    if (ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &got, nullptr) && got == bytes.size()) {
                        std::vector<WallAgeRecord> rec(kWallAgeIds);
                        int32_t last_day = -1;
                        if (wall_age_codec::decode(bytes.data(), bytes.size(), job->binding.data(), last_day, rec.data(), kWallAgeIds)) {
                            for (const WallAgeRecord &r : rec) if (r.kind) ++count;
                            AcquireSRWLockExclusive(&t->lock);
                            memcpy(t->rec, rec.data(), sizeof(t->rec));
                            t->last_day = last_day;
                            ReleaseSRWLockExclusive(&t->lock);
                            restored = true;
                        }
                    }
                }
                CloseHandle(file);
            }
        }
    }
    log_event("wall_age_load", count, restored ? 1u : 0u, t ? 1u : 0u);
    return restored;
}
static const save_host::SidecarPlugin sidecar_plugin = {"wall_age_save", L"wallage", 1, "wall_age_save_written", "wall_age_save_write_failed",
    []() { return table_now() != nullptr; },
    []() { side_snapshot(); },
    [](const save_host::Job &job, save_host::Written &out) { return side_write(*static_cast<const merit::SaveCoordinator::Completed *>(job.source), out); },
    []() { side_begin_load(); },
    [](const save_host::Job *job) { return side_restore(job ? static_cast<const merit::SaveCoordinator::Completed *>(job->source) : nullptr); }};
}   // namespace wall_age_save
