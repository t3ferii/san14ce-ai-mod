#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include "stat_growth.h"
static unsigned checks;
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
    printf("{\"status\":\"modpack_fixture_passed\",\"checks\":%u,\"gameplay_verified\":false}\n",checks);
    return 0;
}
