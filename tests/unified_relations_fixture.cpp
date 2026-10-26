// Exercise the production sortie relation adapter without loading the game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <vector>
using Obj=unsigned char *;
static std::vector<unsigned char> image_bytes(0x7D6AE0);
static Obj image=image_bytes.data();
static unsigned checks,comparisons,captions;
static Obj last_person,last_reference;
static unsigned char people[4][16];
static void *mock_function(unsigned);
template<class T>static T fn(unsigned rva){return reinterpret_cast<T>(mock_function(rva));}
#include "../src/native_unified_relations.inl"
static void check(bool ok,const char *message){++checks;if(!ok){fprintf(stderr,"%s\n",message);ExitProcess(1);}}
static unified_relations::NativeValue *__fastcall compare(unified_relations::NativeValue *out,Obj person,Obj reference) {
    ++comparisons;last_person=person;last_reference=reference;
    if(person==people[3]){out->kind=2;out->grade=0x7FFFFFFF;}
    else {out->kind=0;out->grade=person==reference?2:person[0];}
    return out;
}
static const wchar_t *__fastcall caption(int grade) {
    ++captions;
    static const wchar_t *values[]={L"",L"×",L"△",L"○",L"<blue>◎</blue>",L"<gold>◎</gold>"};
    return values[grade];
}
static void *mock_function(unsigned rva) {
    if(rva==0x7D6AC0)return reinterpret_cast<void *>(&compare);
    if(rva==0x30BED0)return reinterpret_cast<void *>(&caption);
    fprintf(stderr,"Unexpected game call: %X\n",rva);ExitProcess(1);
}
int main() {
    using namespace unified_relations;
    const unsigned char compare_bytes[]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x48};
    const unsigned char caption_bytes[]={0x8D,0x41,0xFF,0x83,0xF8,0x04,0x77,0x2E,0x8D,0x91,0x18,0x08,0x00,0x00,0x81,0xFA};
    check(!available(),"unrecognized native entry disables relation reuse");
    memcpy(image+0x7D6AC0,compare_bytes,16);memcpy(image+0x30BED0,caption_bytes,16);
    check(available(),"both current native entries recognized");
    image[0x30BED0]^=1;check(!available(),"caption mismatch is rejected too");image[0x30BED0]^=1;
    people[0][0]=4;people[1][0]=5;people[2][0]=1;
    set_reference(nullptr);check(std::wstring(text(people[0])).empty() && comparisons==0,"no comparison without a chosen reference");
    set_reference(people[1]);
    check(comparable(people[0]),"native comparable result accepted");
    check(last_person==people[0] && last_reference==people[1],"preserve sortie ABI and comparison direction");
    check(std::wstring(text(people[0]))==L"<blue>◎</blue>","preserve native text including color markup");
    NativeValue sorted;check(sort_value(&sorted,people[0])==&sorted && sorted.kind==0 && sorted.grade==4,"native typed sort value is returned");
    check(comparisons==1 && captions==1,"render and sort reuse one cached game calculation");
    check(!comparable(people[1]),"reference itself is not a related search result");
    check(std::wstring(text(people[1]))==L"△","native self display is retained in an unfiltered list");
    check(!comparable(people[3]) && std::wstring(text(people[3])).empty(),"invalid native comparison is blank and excluded");
    sort_value(&sorted,people[3]);check(sorted.kind==2 && sorted.grade==0x7FFFFFFF,"invalid native sort sentinel is retained");
    unsigned previous=comparisons;set_reference(people[2]);text(people[0]);
    check(comparisons==previous+1 && last_reference==people[2],"changing reference discards old pair calculations");
    check(comparable(people[2])==false,"new reference is also excluded from related results");
    set_reference(nullptr);previous=comparisons;text(people[0]);
    check(comparisons==previous && cache.size()==1,"closing or world change clears pair state without game calls");
    printf("{\"checks\":%u,\"native_game_executed\":false}\n",checks);
}
