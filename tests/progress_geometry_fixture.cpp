#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include "../src/progress_geometry.h"
#include "../src/native_progress_profile.h"
using Distance=int(__fastcall *)(int,int);
static Distance original;
static progress_geometry::Grid grid;
static __declspec(noinline) int __fastcall candidate(int a,int b) {
    if(static_cast<unsigned>(a)>=progress_geometry::cells || static_cast<unsigned>(b)>=progress_geometry::cells)return original(a,b);
    return grid.distance(static_cast<unsigned>(a),static_cast<unsigned>(b));
}
struct Pair {int a,b;};
static volatile uint64_t sink;
static double measure(Distance function,const std::vector<Pair> &pairs) {
    LARGE_INTEGER begin,end,frequency;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
    uint64_t sum=0;
    for(unsigned repeat=0;repeat<20;++repeat)for(const Pair &pair:pairs)sum+=static_cast<unsigned>(function(pair.a,pair.b));
    QueryPerformanceCounter(&end);sink=sum;
    return static_cast<double>(end.QuadPart-begin.QuadPart)*1000.0/static_cast<double>(frequency.QuadPart);
}
int main() {
    static_assert(sizeof(progress_ai_entry_bytes)==32,"AI entry guard size");
    auto code=static_cast<unsigned char *>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!code)return 1;memcpy(code,progress_distance_bytes,sizeof(progress_distance_bytes));
    DWORD old;if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old))return 2;
    FlushInstructionCache(GetCurrentProcess(),code,sizeof(progress_distance_bytes));original=reinterpret_cast<Distance>(code);grid.initialize();
    uint64_t comparisons=0;
    const unsigned anchors[]={0,1,2,219,220,221,439,440,441,48399,48180,48181,24200,24201,24309,24090};
    for(unsigned h=0;h<progress_geometry::cells;++h)for(unsigned anchor:anchors) {
        int expected=original(static_cast<int>(h),static_cast<int>(anchor));
        if(candidate(static_cast<int>(h),static_cast<int>(anchor))!=expected || candidate(static_cast<int>(anchor),static_cast<int>(h))!=expected)return 3;
        comparisons+=2;
    }
    uint32_t random=0x63C47A15;
    for(unsigned i=0;i<1000000;++i) {
        random=random*1664525u+1013904223u;int a=static_cast<int>(random%48400);
        random=random*1664525u+1013904223u;int b=static_cast<int>(random%48400);
        if(candidate(a,b)!=original(a,b))return 4;++comparisons;
    }
    const int unusual[]={-1,-220,-48400,48400,48401,65535,INT32_MIN,INT32_MAX};
    for(int a:unusual)for(int b:unusual)if(candidate(a,b)!=original(a,b))return 5;
    printf("{\"status\":\"progress_geometry_fixture_passed\",\"comparisons\":%llu,\"checks\":5,\"benchmarks\":[",static_cast<unsigned long long>(comparisons));
    for(unsigned mode=0;mode<3;++mode) {
        std::vector<Pair> pairs;random=0x9C8D32F1;
        for(unsigned i=0;i<100000;++i) {
            random=random*1664525u+1013904223u;int a=static_cast<int>(random%48400);
            random=random*1664525u+1013904223u;int b=mode==0?static_cast<int>(random%48400):mode==1?24200:(a+1)%48400;
            pairs.push_back({a,b});
        }
        measure(original,pairs);measure(candidate,pairs);
        std::vector<double> before,after;
        for(unsigned trial=0;trial<7;++trial) {
            if(trial&1) {after.push_back(measure(candidate,pairs));before.push_back(measure(original,pairs));}
            else {before.push_back(measure(original,pairs));after.push_back(measure(candidate,pairs));}
        }
        std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
        printf("%s{\"mode\":%u,\"original_ms\":%.3f,\"candidate_ms\":%.3f,\"ratio\":%.3f}",mode?",":"",mode,before[3],after[3],before[3]/after[3]);
    }
    printf("],\"gameplay_verified\":false}\n");VirtualFree(code,0,MEM_RELEASE);return 0;
}
