// Historical local1 Windows prototype, retained for comparison; NOT included in the current build.
// Active game-native editor: native_ccz_editor.inl (user correction 2026-11-02).
// Owned, modal settings dialog inside the game process. Open only from the title UI frame.
// No native game function is called by its Windows callbacks. It edits one INI profile.
#include "ccz_appearance_io.h"
#include "ccz_appearance_labels.h"
namespace ccz_settings {
enum : int {FormTab=100,GeneralTab,Rows,Low,High,Class0,Class1,Class2,Mask0=120,All=140,None,Example,Reset,Help};
struct Dialog {
    ccz_appearance::Settings value;std::wstring before;wchar_t path[MAX_PATH]={};
    HWND window=nullptr,list=nullptr,low=nullptr,high=nullptr,choices[3]={},masks[8]={};
    std::vector<HWND> common,unique;unsigned selected=0;bool personal=false,loading=false;
};
static HWND control(Dialog &d,const wchar_t *cls,const wchar_t *text,DWORD style,int id,int x,int y,int w,int h){
    HWND c=CreateWindowExW((!wcscmp(cls,L"LISTBOX") || !wcscmp(cls,L"COMBOBOX"))?WS_EX_CLIENTEDGE:0,cls,text,
        WS_CHILD|WS_VISIBLE|style,x,y,w,h,d.window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),self_module,nullptr);
    SendMessageW(c,WM_SETFONT,SendMessageW(d.window,WM_GETFONT,0,0),TRUE);return c;
}
static void enable_page(Dialog &d){for(HWND c:d.common)ShowWindow(c,d.personal?SW_HIDE:SW_SHOW);for(HWND c:d.unique)ShowWindow(c,d.personal?SW_SHOW:SW_HIDE);}
static void store_selection(Dialog &d){
    if(d.loading)return;
    if(d.personal){if(d.selected>=ccz_appearance::general_count)return;unsigned bits=0;for(unsigned f=0;f<8;++f)if(SendMessageW(d.masks[f],BM_GETCHECK,0,0)==BST_CHECKED)bits|=1u<<f;d.value.forms[d.selected]=static_cast<unsigned char>(bits);}
    else if(d.selected<ccz_appearance::rows)for(unsigned t=0;t<3;++t){LRESULT at=SendMessageW(d.choices[t],CB_GETCURSEL,0,0);if(at!=CB_ERR)d.value.mapping[d.selected][t]=static_cast<unsigned char>(at==ccz_appearance::classes?255:at);}
}
static bool editable(unsigned row){return row<=10 || row==19 || row>=21;}
static void load_selection(Dialog &d){
    d.loading=true;
    if(d.personal){for(unsigned f=0;f<8;++f)SendMessageW(d.masks[f],BM_SETCHECK,(d.value.forms[d.selected]&(1u<<f))?BST_CHECKED:BST_UNCHECKED,0);}
    else for(unsigned t=0;t<3;++t){unsigned v=d.value.mapping[d.selected][t];SendMessageW(d.choices[t],CB_SETCURSEL,v==255?ccz_appearance::classes:v,0);EnableWindow(d.choices[t],editable(d.selected));}
    d.loading=false;
}
static void page(Dialog &d,bool personal){
    store_selection(d);d.personal=personal;d.selected=0;d.loading=true;SendMessageW(d.list,LB_RESETCONTENT,0,0);
    unsigned count=personal?static_cast<unsigned>(ccz_appearance::general_count):ccz_appearance::rows;
    for(unsigned i=0;i<count;++i)SendMessageW(d.list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(personal?ccz_appearance::general_labels[i]:ccz_appearance::formation_labels[i]));
    SendMessageW(d.list,LB_SETCURSEL,0,0);d.loading=false;enable_page(d);load_selection(d);
}
static bool read_threshold(Dialog &d){
    BOOL a=FALSE,b=FALSE;UINT low=GetDlgItemInt(d.window,Low,&a,FALSE),high=GetDlgItemInt(d.window,High,&b,FALSE);
    if(!a || !b || low>=high || high>255){MessageBoxW(d.window,L"통솔 경계는 0~255이며 첫 경계가 두 번째 경계보다 작아야 합니다.",L"조조전",MB_OK|MB_ICONINFORMATION);return false;}
    d.value.low=low;d.value.high=high;return true;
}
static void create(Dialog &d){
    RECT r={};GetClientRect(d.window,&r);int w=r.right,h=r.bottom;
    int left=w*29/100,rx=left+24,rw=w-rx-20;
    control(d,L"BUTTON",L"진형 · 통솔",BS_PUSHBUTTON|WS_TABSTOP,FormTab,16,12,128,30);
    control(d,L"BUTTON",L"고유 무장",BS_PUSHBUTTON|WS_TABSTOP,GeneralTab,152,12,128,30);
    d.list=control(d,L"LISTBOX",L"",LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_TABSTOP,Rows,16,54,left-16,h-124);
    d.common.push_back(control(d,L"STATIC",L"선택한 진형에서 사용할 공통 병종",0,0,rx,60,rw,24));
    for(unsigned t=0;t<3;++t){
        const wchar_t *label=t==0?L"통솔 1단계":t==1?L"통솔 2단계":L"통솔 3단계";
        d.common.push_back(control(d,L"STATIC",label,0,0,rx,94+static_cast<int>(t)*64,110,24));
        d.choices[t]=control(d,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,Class0+static_cast<int>(t),rx+116,90+static_cast<int>(t)*64,rw-116,300);
        d.common.push_back(d.choices[t]);
        for(unsigned c=0;c<ccz_appearance::classes;++c)SendMessageW(d.choices[t],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(ccz_appearance::class_labels[c]));
        SendMessageW(d.choices[t],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"원작 3D 모형"));
    }
    d.common.push_back(control(d,L"STATIC",L"통솔 경계 (모든 진형 · 군주 · 군사에 공통)",0,0,rx,300,rw,26));
    d.low=control(d,L"EDIT",L"",ES_NUMBER|WS_BORDER|WS_TABSTOP,Low,rx,334,72,28);
    d.high=control(d,L"EDIT",L"",ES_NUMBER|WS_BORDER|WS_TABSTOP,High,rx+156,334,72,28);
    d.common.push_back(d.low);d.common.push_back(d.high);
    SendMessageW(d.low,EM_SETLIMITTEXT,3,0);SendMessageW(d.high,EM_SETLIMITTEXT,3,0);
    SetDlgItemInt(d.window,Low,d.value.low,FALSE);SetDlgItemInt(d.window,High,d.value.high,FALSE);
    d.common.push_back(control(d,L"STATIC",L"미만 1단계 / 두 경계 사이 2단계 / 이상 3단계\n기본 통솔을 사용합니다. 기본 경계는 70 / 90입니다.\n수상·특수 진형의 원작 렌더링 제한은 유지됩니다.",0,0,rx,378,rw,78));
    d.unique.push_back(control(d,L"STATIC",L"고유 그림을 사용할 진형을 체크하세요.\n체크하지 않은 진형은 공통 병종 설정을 따릅니다.\n군주·군사도 고유 그림을 허용한 진형에서만 우선합니다.",0,0,rx,60,rw,76));
    for(unsigned f=0;f<8;++f){int col=static_cast<int>(f%2),row=static_cast<int>(f/2);d.masks[f]=control(d,L"BUTTON",ccz_appearance::formation_labels[f],BS_AUTOCHECKBOX|WS_TABSTOP,Mask0+static_cast<int>(f),rx+col*(rw/2),150+row*38,rw/2,30);d.unique.push_back(d.masks[f]);}
    d.unique.push_back(control(d,L"BUTTON",L"모든 진형에서 고유",BS_PUSHBUTTON|WS_TABSTOP,All,rx,324,rw/2-6,32));
    d.unique.push_back(control(d,L"BUTTON",L"공통 조형만 사용",BS_PUSHBUTTON|WS_TABSTOP,None,rx+rw/2+6,324,rw/2-6,32));
    d.unique.push_back(control(d,L"BUTTON",L"장료 예시: 어린 · 봉시만 고유",BS_PUSHBUTTON|WS_TABSTOP,Example,rx,370,rw,34));
    d.unique.push_back(control(d,L"STATIC",L"고유 그림은 기존 92명입니다. 정란·투석·수송과\n수상/특수 진형에는 기존 공통/원작 처리를 유지합니다.",0,0,rx,422,rw,50));
    control(d,L"STATIC",L"병종 그림만 바뀝니다. 능력치 · 전투 규칙 · 세이브는 바꾸지 않습니다.",0,Help,16,h-64,w-32,22);
    control(d,L"BUTTON",L"기본값 복원",BS_PUSHBUTTON|WS_TABSTOP,Reset,16,h-38,128,30);
    control(d,L"BUTTON",L"저장",BS_DEFPUSHBUTTON|WS_TABSTOP,IDOK,w-220,h-38,94,30);
    control(d,L"BUTTON",L"취소",BS_PUSHBUTTON|WS_TABSTOP,IDCANCEL,w-116,h-38,94,30);
    page(d,false);
}
static INT_PTR CALLBACK procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    Dialog *d=reinterpret_cast<Dialog *>(GetWindowLongPtrW(hwnd,DWLP_USER));
    if(message==WM_INITDIALOG){d=reinterpret_cast<Dialog *>(lp);d->window=hwnd;SetWindowLongPtrW(hwnd,DWLP_USER,lp);create(*d);return TRUE;}
    if(!d)return FALSE;
    if(message==WM_CLOSE){EndDialog(hwnd,0);return TRUE;}
    if(message!=WM_COMMAND)return FALSE;
    unsigned id=LOWORD(wp),event=HIWORD(wp);
    if(id==IDCANCEL){EndDialog(hwnd,0);return TRUE;}
    if(id==FormTab || id==GeneralTab){page(*d,id==GeneralTab);return TRUE;}
    if(id==Rows && event==LBN_SELCHANGE){store_selection(*d);LRESULT row=SendMessageW(d->list,LB_GETCURSEL,0,0);if(row!=LB_ERR){d->selected=static_cast<unsigned>(row);load_selection(*d);}return TRUE;}
    if(id>=Class0 && id<=Class2 && event==CBN_SELCHANGE){store_selection(*d);return TRUE;}
    if(id>=Mask0 && id<Mask0+8){store_selection(*d);return TRUE;}
    if(id==All || id==None){d->value.forms[d->selected]=id==All?255:0;load_selection(*d);return TRUE;}
    if(id==Example){
        store_selection(*d);size_t at=ccz_appearance::general_index(663);if(at<ccz_appearance::general_count){d->selected=static_cast<unsigned>(at);d->value.forms[at]=(1u<<1)|(1u<<2);SendMessageW(d->list,LB_SETCURSEL,at,0);load_selection(*d);}return TRUE;
    }
    if(id==Reset){d->value=ccz_appearance::Settings{};SetDlgItemInt(hwnd,Low,70,FALSE);SetDlgItemInt(hwnd,High,90,FALSE);load_selection(*d);return TRUE;}
    if(id==IDOK){
        store_selection(*d);if(!read_threshold(*d))return TRUE;
        if(!ccz_appearance::save_profile(d->path,d->before,d->value)){MessageBoxW(hwnd,L"설정을 저장하지 못했습니다. 다른 설정창에서 같은 값을 바꿨거나 파일을 쓸 수 없습니다. 취소 후 다시 열어 주세요.",L"조조전",MB_OK|MB_ICONEXCLAMATION);return TRUE;}
        EndDialog(hwnd,1);return TRUE;
    }
    return FALSE;
}
static DLGTEMPLATE *dialog_template(unsigned char *raw){
    auto t=reinterpret_cast<DLGTEMPLATE *>(raw);
    t->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT|DS_CENTER;t->cx=550;t->cy=350;
    WORD *p=reinterpret_cast<WORD *>(raw+sizeof(DLGTEMPLATE));*p++=0;*p++=0;
    const wchar_t caption[]=L"조조전 · 병종과 고유 조형 설정";memcpy(p,caption,sizeof(caption));p+=sizeof(caption)/sizeof(WORD);
    *p++=9;const wchar_t font[]=L"맑은 고딕";memcpy(p,font,sizeof(font));
    return t;
}
static bool show(HWND owner){
    Dialog d;if(!mod_ini_path(d.path,L"기능설정.ini"))return false;
    d.before=ccz_appearance::read_profile(d.path);ccz_appearance::decode(d.before.c_str(),d.value);
    // In-process modal child of the game window; the native title frame is suspended until it closes.
    alignas(4) unsigned char raw[256]={};auto t=dialog_template(raw);
    INT_PTR result=DialogBoxIndirectParamW(self_module,t,owner,&procedure,reinterpret_cast<LPARAM>(&d));
    if(result==1){ccz_module::reload_appearance();log_event("ccz_appearance_saved",d.value.low,d.value.high,0);return true;}
    if(result==-1)log_event("ccz_appearance_dialog_failed",GetLastError(),0,0);return false;
}
}
