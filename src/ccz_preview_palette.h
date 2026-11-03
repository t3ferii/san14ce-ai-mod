#pragma once
#include <algorithm>
// Same material correction / red reference colour as the renderer's palette row63.
// The editor has no selected force. In the field the existing force-colour palette is used.
namespace ccz_catalog {
inline unsigned representative_colour(const unsigned char *palette,unsigned index){
    if(!index)return 0;const auto *c=palette+index*3;
    float r=c[0]/255.0f,g=c[1]/255.0f,b=c[2]/255.0f;
    float high=(std::max)(r,(std::max)(g,b)),low=(std::min)(r,(std::min)(g,b)),delta=high-low;
    float saturation=high>0?delta/high:0,hue=0;
    if(delta>0){if(high==r)hue=(g-b)/delta+(g<b?6.0f:0.0f);else if(high==g)hue=(b-r)/delta+2;else hue=(r-g)/delta+4;hue/=6;}
    if(palette[768+index]){
        if(!(saturation>0.85f && hue>=0.30f && hue<=0.43f)){float s=saturation*0.55f;r=high;g=high*(1-s*0.58f);b=high*(1-s);}
        else {float v=high/0.8f;v=v<0.25f?0.25f:v>1.25f?1.25f:v;r=(std::min)(1.0f,0.86f*v);g=(std::min)(1.0f,0.12f*v);b=(std::min)(1.0f,0.08f*v);}
    }
    return 0xFF000000u|(static_cast<unsigned>(r*255+.5f)<<16)|(static_cast<unsigned>(g*255+.5f)<<8)|static_cast<unsigned>(b*255+.5f);
}
}
