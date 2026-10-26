#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <initializer_list>
#include "stat_growth.h"
using Obj=unsigned char *;
struct ImportedSection {unsigned offset,size,protection;};
struct ImportedCode {Obj base;unsigned size,entry,exceptions,count;const ImportedSection *sections;size_t section_count;};
#include "modpack_payloads.h"
static unsigned checks;
extern "C" void SAN14ModpackCollabBootstrap() {}
static bool check(bool value) {++checks;return value;}
int main() {
    if(!check(statgrowth::threshold(100,9800,10000)==10000) ||
       !check(statgrowth::threshold(101,9800,10000)==10200) ||
       !check(statgrowth::threshold(200,9800,10000)==30000) ||
       !check(statgrowth::advance(100,10199,9800,10000)==100) ||
       !check(statgrowth::advance(100,10200,9800,10000)==101) ||
       !check(statgrowth::advance(100,30000,9800,10000)==200) ||
       !check(statgrowth::advance(199,29999,9800,10000)==199) ||
       !check(statgrowth::advance(200,UINT32_MAX,9800,10000)==200) ||
       !check(statgrowth::threshold(200,0,UINT32_MAX)>UINT32_MAX))return 1;
    for(const auto &code:{hover_code,deaths_code}) {
        if(!check(RtlAddFunctionTable(reinterpret_cast<PRUNTIME_FUNCTION>(code.base+code.exceptions),
                  code.count,reinterpret_cast<DWORD64>(code.base)-0x1000)!=FALSE))return 2;
        for(size_t n=0;n<code.section_count;++n) {
            DWORD old=0;const auto &s=code.sections[n];
            if(s.size && !check(VirtualProtect(code.base+s.offset,s.size,s.protection,&old)!=FALSE))return 3;
        }
        // The actual linked implementation must reject this unsupported test EXE.
        if(!check(reinterpret_cast<unsigned(__cdecl *)()>(code.base+code.entry)()==0))return 4;
    }
    printf("{\"status\":\"modpack_fixture_passed\",\"checks\":%u,\"gameplay_verified\":false}\n",checks);
    return 0;
}
