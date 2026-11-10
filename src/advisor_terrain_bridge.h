#pragma once
// Shared by the military report (writer) and terrain edit (reader). Owned by
// advisor-briefing so the code dependency direction matches the installer
// Requires row: terrain edit needs the report UI that arms a city.
#include <stdint.h>
namespace terrain_edit {
inline void *advisor_city_pending=nullptr;
inline void *advisor_area_pending=nullptr;
inline uint64_t advisor_city_armed_ms=0;
}
