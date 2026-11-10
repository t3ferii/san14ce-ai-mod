#pragma once
// [combat] siege_guard (user 2026-11-10, local17): pure per-army guard test for computer siege groups.
// Every field-formation army (1-6) of a computer siege group whose commander's effective intelligence
// is at least 80 intercepts player armies within 5 hexes. Siege-weapon (8-10), march-column (7) and
// navy (11-13) formations and every player-force army (direct or delegated corps) never guard.
// The hook lives in src/native_siege_guard.inl; this header is fixture-testable without game memory.
namespace siege_guard {

constexpr int kRadius = 5;                    // player armies this close to a guard are intercepted
constexpr unsigned kMinIntelligence = 80;     // commander's effective intelligence (지력)

// Formations 1-6 fight in the field; 7 is the march column, 8-10 siege weapons, 11-13 navy.
inline bool field_formation(unsigned formation) { return formation >= 1 && formation <= 6; }

inline bool guard_capable(unsigned formation, unsigned intelligence, bool player) {
    return !player && field_formation(formation) && intelligence >= kMinIntelligence;
}

}   // namespace siege_guard
