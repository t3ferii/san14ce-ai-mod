#pragma once
#include <algorithm>
namespace unified_overlay {
inline bool eligible(bool world,bool force,bool parent,bool modal){return world && force && parent && !modal;}
struct Bounds {int x=0,y=0,width=0,height=0;};
inline Bounds bounds(int width,int height) {
    if(width<=0 || height<=0)return {};
    double scale=(std::min)(width/1920.0,height/1080.0);
    auto pixel=[&](double n){return static_cast<int>(n+0.5);};
    return {pixel((width-1920*scale)/2+218*scale),pixel((height-1080*scale)/2+156*scale),
        (std::max)(1,pixel(164*scale)),(std::max)(1,pixel(42*scale))};
}
inline bool contains(const Bounds &b,int x,int y){return b.width>0 && b.height>0 && x>=b.x && y>=b.y && x<b.x+b.width && y<b.y+b.height;}
inline bool client_hit(const Bounds &b,int x,int y,int client_width,int client_height,int buffer_width,int buffer_height) {
    if(client_width<=0 || client_height<=0 || buffer_width<=0 || buffer_height<=0)return false;
    return contains(b,static_cast<int>(static_cast<long long>(x)*buffer_width/client_width),
        static_cast<int>(static_cast<long long>(y)*buffer_height/client_height));
}
struct Pointer {
    bool armed=false;unsigned generation=0;
    bool down(bool visible,bool inside,unsigned epoch){armed=visible && inside;generation=epoch;return armed;}
    bool up(bool visible,bool inside,unsigned epoch,bool &fire){bool handled=armed;fire=armed && visible && inside && generation==epoch;armed=false;return handled;}
};
}
