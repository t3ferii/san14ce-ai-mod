#pragma once
// Eleven observations span ten native 10-day turns. No forecast is inferred.
struct ObstacleHistory {
    unsigned short hex[11], goal;
    int day[11], range[11], progress;
    unsigned count;
};
static void obstacle_sample(ObstacleHistory &h,int day,unsigned hex,unsigned goal,int range,int progress) {
    if(h.count && (h.goal!=goal || h.progress!=progress || day<h.day[h.count-1] ||
                  day-h.day[h.count-1]>10))h.count=0;
    h.goal=static_cast<unsigned short>(goal);h.progress=progress;
    if(h.count && day-h.day[h.count-1]<10)return;
    if(h.count==11) {
        for(unsigned i=1;i<11;++i) {h.hex[i-1]=h.hex[i];h.day[i-1]=h.day[i];h.range[i-1]=h.range[i];}
        --h.count;
    }
    unsigned i=h.count++;h.hex[i]=static_cast<unsigned short>(hex);h.day[i]=day;h.range[i]=range;
}
static bool obstacle_oscillating(const ObstacleHistory &h) {
    if(h.count!=11 || h.day[10]-h.day[0]<100 || h.range[0]-h.range[10]>2)return false;
    unsigned reversals=0,revisits=0,moves=0;int previous=0,lo=h.range[0],hi=lo;
    for(unsigned i=1;i<11;++i) {
        if(h.range[i]<lo)lo=h.range[i];if(h.range[i]>hi)hi=h.range[i];
        int change=h.range[i]-h.range[i-1],sign=(change>0)-(change<0);
        if(sign) {if(previous && sign!=previous)++reversals;previous=sign;++moves;}
        for(unsigned j=0;j+1<i;++j)if(h.hex[i]==h.hex[j]) {++revisits;break;}
    }
    return moves>=4 && reversals>=3 && revisits>=3 && hi-lo<=12;
}
