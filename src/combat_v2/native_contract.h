#pragma once
#include "sortie.h"

// ABI notes for the FUTURE NativePort implementation. No address is invoked,
// patched or bound here. Existing native evidence is the source of these
// signatures, not the player FormationMove UI or the old AI behavior rules.
namespace combat_v2::native_contract {
using Object = unsigned char *;
struct GameList {Object vtable; unsigned *handle;};
using BuildCommand = void(__fastcall *)(Object node, Object target, Object person,
                                       unsigned *command, unsigned maximum);
using RefreshCommand = void(__fastcall *)(Object person, Object formation, unsigned *command);
using Dispatch = Object(__fastcall *)(unsigned *command, int spend);
using FormGroup = Object(__fastcall *)(GameList *created_armies);
using CancelGroup = void(__fastcall *)(Object group);
using DisposeEmptyGroup = void(__fastcall *)(Object group);
using CommandCapacity = int(__fastcall *)(Object person);
using FormationOwned = int(__fastcall *)(Object person, unsigned formation);
static_assert(sizeof(Command) == 0x78, "private command storage matches native allocation");
static_assert(sizeof(GameList) == 16, "x64 native list header");
// Verified current-game call records, only documentary constants:
constexpr unsigned build_command = 0x42E70, refresh_command = 0xD0090;
constexpr unsigned dispatch = 0x1EC890, form_group = 0x2D9260;
constexpr unsigned cancel_group = 0x9E900, dispose_empty_group = 0x282540;
constexpr unsigned command_capacity = 0x2311A0, formation_owned = 0x232350;
// Integration still requires current image/byte guards; AI parent result-list
// and power bookkeeping; native object ownership/cancellation verification;
// revalidation immediately before spending; and original AI thread execution.
// Player/UI selection callbacks are not an AI admission condition.
} // namespace combat_v2::native_contract
