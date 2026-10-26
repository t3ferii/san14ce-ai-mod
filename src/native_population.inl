// Actual monthly growth includes native randomness, security, population
// taper and Culture 35. Multiply only its positive delta, preserving cap.
using PopulationGrowth=uint64_t(__fastcall *)(Obj);
static PopulationGrowth original_population_growth;
static uint64_t __fastcall population_growth(Obj city) {
    NativePhase phase("monthly_population");
    if(!enabled || !valid(city) || word(city,0x4E)>51)return original_population_growth(city);
    struct Sample {Obj area;unsigned before;};std::vector<Sample> samples;
    for(unsigned i=1;i<=500;++i) {
        Obj area=table(0x6CAF0,i,500);
        if(valid(area) && area[0x35]==word(city,0x4E))samples.push_back({area,word(area,0x52)});
    }
    uint64_t result=original_population_growth(city);
    for(const auto &s:samples) {
        unsigned now=word(s.area,0x52);
        if(now>s.before) {
            unsigned value=(std::min)(60000u,s.before+4u*(now-s.before));
            *reinterpret_cast<unsigned short *>(s.area+0x52)=static_cast<unsigned short>(value);
        }
    }
    return result;
}
static const unsigned char population_entry[16]={0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x8D,0x6C,0x24,0xD9};
