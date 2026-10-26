#pragma once
#include <algorithm>
#include <cstdlib>
#include <vector>
namespace combat_v2 {
using Hex = unsigned;
constexpr Hex no_hex = 48400;
inline Hex neighbor(Hex h, unsigned d) noexcept {
    constexpr int dx[6] = {0,1,1,0,-1,-1};
    constexpr int dy[2][6] = {{-1,0,1,1,1,0},{-1,-1,0,1,0,-1}};
    if(h >= no_hex || d >= 6)return no_hex;
    const int column = static_cast<int>(h % 220);
    const int x = column + dx[d], y = static_cast<int>(h / 220) + dy[column & 1][d];
    return x >= 0 && x < 220 && y >= 0 && y < 220 ? static_cast<Hex>(y * 220 + x) : no_hex;
}
inline unsigned hex_distance(Hex a, Hex b) noexcept {
    if(a >= no_hex || b >= no_hex)return no_hex;
    const int ax = static_cast<int>(a % 220), bx = static_cast<int>(b % 220);
    const int ay = static_cast<int>(a / 220) - (ax + (ax & 1)) / 2;
    const int by = static_cast<int>(b / 220) - (bx + (bx & 1)) / 2;
    return static_cast<unsigned>((std::abs(ax-bx)+std::abs(ay-by)+std::abs(ax-bx+ay-by))/2);
}
inline unsigned face_toward(Hex from, Hex to) noexcept {
    unsigned best = 0, range = no_hex;
    for(unsigned d = 0; d < 6; ++d) {
        const auto distance = hex_distance(neighbor(from,d),to);
        if(distance < range) {range = distance; best = d;}
    }
    return best;
}
// Native candidate 0 is the center commander. Fill the forward half first,
// then the flanks and rear; this is geometry, not a new officer/trait rule.
inline Hex formation_hex(Hex center, unsigned member, unsigned facing) noexcept {
    constexpr unsigned seats[6] = {0,1,5,2,4,3};
    if(!member)return center;
    return member <= 6 ? neighbor(center,(seats[member-1]+facing)%6) : no_hex;
}
inline std::vector<Hex> radius_cells(Hex center, unsigned radius) {
    std::vector<Hex> cells;
    if(center >= no_hex)return cells;
    const int x = static_cast<int>(center%220), y = static_cast<int>(center/220);
    for(int row = (std::max)(0,y-static_cast<int>(radius)-1); row <= (std::min)(219,y+static_cast<int>(radius)+1); ++row)
        for(int col = (std::max)(0,x-static_cast<int>(radius)); col <= (std::min)(219,x+static_cast<int>(radius)); ++col) {
            const auto h = static_cast<Hex>(row*220+col);
            if(hex_distance(center,h) <= radius)cells.push_back(h);
        }
    std::sort(cells.begin(),cells.end(),[center](Hex a,Hex b) {
        const auto da=hex_distance(a,center),db=hex_distance(b,center);
        return da==db?a<b:da<db;
    });
    return cells;
}
} // namespace combat_v2
