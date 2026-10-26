/* Shared native UI calls. Include after Obj/fn definitions.
 * Call only from the game's native UI callback, never a loader worker or
 * remote thread. See docs/NATIVE_FUNCTIONS.md for ABI and evidence.
 */
namespace native_ui {
static void officer_notice(unsigned speaker,const wchar_t *text) {
    // 1F09E0 copies UTF-16 text synchronously before queuing the dialog.
    fn<void(__fastcall *)(unsigned,const wchar_t *)>(0x1F09E0)(speaker,text);
}
}
