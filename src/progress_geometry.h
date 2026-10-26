#pragma once
#include <stdint.h>
namespace progress_geometry {
constexpr unsigned cells=220u*220u;
struct Coordinate {int16_t column,doubled_row;};
static_assert(sizeof(Coordinate)==4,"Native kernel coordinate stride");
struct Grid {
    Coordinate coordinates[cells];
    void initialize() {
        for(unsigned h=0;h<cells;++h) {
            unsigned x=h%220,y=h/220;
            coordinates[h]={static_cast<int16_t>(x),static_cast<int16_t>(2*y+((x&1u)==0))};
        }
    }
    int distance(unsigned a,unsigned b) const {
        const Coordinate &left=coordinates[a],&right=coordinates[b];
        int dx=right.column-left.column,dy=right.doubled_row-left.doubled_row;
        if(dx<0)dx=-dx;if(dy<0)dy=-dy;
        return dy>dx?dx+((dy-dx)>>1):dx;
    }
};
}
