#pragma once
namespace sortie {
constexpr unsigned none=48400, size=7;
// Slots are clockwise from north; slot 6 is the center.
inline unsigned neighbor(unsigned hex,unsigned direction) {
    static const int dx[6]={0,1,1,0,-1,-1};
    static const int dy[2][6]={{-1,0,1,1,1,0},{-1,-1,0,1,0,-1}};
    if(hex>=none || direction>=6)return none;
    int x=static_cast<int>(hex%220),y=static_cast<int>(hex/220);
    y+=dy[x&1][direction];x+=dx[direction];
    return x>=0 && x<220 && y>=0 && y<220?static_cast<unsigned>(y*220+x):none;
}
inline unsigned slot_hex(unsigned center,unsigned slot,unsigned facing) {
    return slot==6?center:neighbor(center,(slot+facing)%6);
}
inline bool siege(unsigned formation) {return formation>=8 && formation<=10;}
inline unsigned role(unsigned slot) {return slot==0 || slot==1 || slot==5?0:slot==3?2:1;}
struct Officer {unsigned id,mask,capacity,support;int score;bool gate=false;};
inline bool accepts(const Officer &p,unsigned slot) {
    unsigned rear=(1u<<3)|(1u<<7)|(p.gate?(7u<<8):0u)|(slot==6?((1u<<1)|(1u<<4)):0u);
    return role(slot)==0?p.capacity>=3000 && (p.mask&((1u<<1)|(1u<<4)|(1u<<7)))!=0:
        role(slot)==1?p.capacity>=1500 && (p.mask&rear)!=0:p.support && p.capacity>=1000 && (p.mask&0xfe)!=0;
}
inline bool match(unsigned slot,const Officer *people,unsigned count,int *owners,bool *seen) {
    for(unsigned i=0;i<count;++i)if(!seen[i] && accepts(people[i],slot)) {
        seen[i]=true;
        if(owners[i]<0 || match(static_cast<unsigned>(owners[i]),people,count,owners,seen)) {
            owners[i]=static_cast<int>(slot);return true;
        }
    }
    return false;
}
inline bool roster(const Officer *people,unsigned count,unsigned *selected,unsigned budget=0xFFFFFFFFu,unsigned limit=7) {
    if(count<2 || count>128)return false;
    int owners[128];for(unsigned i=0;i<count;++i)owners[i]=-1;
    for(unsigned i=0;i<7;++i)selected[i]=128;
    const unsigned order[7]={0,6,1,5,2,4,3};
    for(unsigned slot:order) {bool seen[128]={};match(slot,people,count,owners,seen);}
    for(unsigned i=0;i<count;++i)if(owners[i]>=0)selected[owners[i]]=i;
    if(selected[0]>=count || selected[6]>=count || limit<2)return false;
    unsigned minimum=3000+1500,number=2;
    if(budget<minimum)return false;
    const unsigned priority[5]={1,5,3,2,4};
    for(unsigned slot:priority)if(selected[slot]<count) {
        bool wanted=slot==1?budget>=9000:slot==5?budget>=16000:slot==2?budget>=24000:slot==4?budget>=32000:true;
        unsigned base=slot==3?1000:role(slot)==0?3000:1500;
        if(!wanted || number>=limit || budget-minimum<base)selected[slot]=128;
        else {minimum+=base;++number;}
    }
    return true;
}
inline unsigned formation(const Officer &p,unsigned slot) {
    if(role(slot)==1 && p.gate)for(unsigned f=8;f<=10;++f)if(p.mask&(1u<<f))return f;
    if(role(slot)==1 && (p.mask&(1u<<3)))return 3;
    if(p.mask&(1u<<4))return 4;
    if(p.mask&(1u<<1))return 1;
    if(p.mask&(1u<<3))return 3;
    for(unsigned f=2;f<=7;++f)if(p.mask&(1u<<f))return f;
    return 0;
}
inline bool allocate(const unsigned *capacity,unsigned available,unsigned *out,unsigned fronts=(1u<<0)|(1u<<1)|(1u<<5)) {
    unsigned total=0,minimum=0;
    for(unsigned i=0;i<7;++i) {
        out[i]=0;if(!capacity[i])continue;
        unsigned base=i==3?1000:(fronts&(1u<<i))?3000:1500;
        if(capacity[i]<base)return false;
        total+=i==3?1000:capacity[i];out[i]=base;minimum+=base;
    }
    if(available<minimum)return false;
    unsigned remaining=(total<available?total:available)-minimum;
    while(remaining) {
        bool changed=false;
        for(unsigned i=0;i<7 && remaining;++i) {
            if(!capacity[i] || i==3)continue;
            unsigned grant=(fronts&(1u<<i))?250:100;
            if(grant>capacity[i]-out[i])grant=capacity[i]-out[i];
            if(grant>remaining)grant=remaining;
            out[i]+=grant;remaining-=grant;changed|=grant!=0;
        }
        if(!changed)return false;
    }
    return true;
}
}
