// The same comparison used by the sortie officer list's nine Link columns.
// Own the reference and cache locally; never write the sortie selection globals.
#include <map>
#include <string>
namespace unified_relations {
struct NativeValue {int kind=2;unsigned padding=0;int grade=0x7FFFFFFF;};
static_assert(sizeof(NativeValue)==12,"native relation sort value ABI");
struct Value {NativeValue native;std::wstring text;};
static Obj reference;
static std::map<Obj,Value> cache;
static bool available() {
    static const unsigned char compare[]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x48};
    static const unsigned char caption[]={0x8D,0x41,0xFF,0x83,0xF8,0x04,0x77,0x2E,0x8D,0x91,0x18,0x08,0x00,0x00,0x81,0xFA};
    return !memcmp(image+0x7D6AC0,compare,sizeof(compare)) && !memcmp(image+0x30BED0,caption,sizeof(caption));
}
static void set_reference(Obj person) {reference=person;cache.clear();}
static std::wstring grade_text(int grade) {
    const wchar_t *s=fn<const wchar_t *(__fastcall *)(int)>(0x30BED0)(grade);
    return s?std::wstring(s,wcsnlen(s,256)):std::wstring();
}
static const Value &get(Obj person) {
    auto found=cache.find(person);if(found!=cache.end())return found->second;
    Value value;
    if(reference && person) {
        fn<NativeValue *(__fastcall *)(NativeValue *,Obj,Obj)>(0x7D6AC0)(&value.native,person,reference);
        if(value.native.kind==0 && value.native.grade>=1 && value.native.grade<=5) {
            value.text=grade_text(value.native.grade);
        }
    }
    return cache.emplace(person,std::move(value)).first->second;
}
static const wchar_t *__fastcall text(Obj person) {return get(person).text.c_str();}
static NativeValue *__fastcall sort_value(NativeValue *out,Obj person) {*out=get(person).native;return out;}
static bool comparable(Obj person) {return reference && person!=reference && get(person).native.kind==0;}
}
