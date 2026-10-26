#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// SAN14_Convenience.dll only. Starts the GPU pass-timing diagnostic when
// AI_TacticsMod\기능설정.ini [diagnostics] gpu_profile=1 (default 0, restart to apply).
extern "C" void gpu_profile_start(HMODULE self);
