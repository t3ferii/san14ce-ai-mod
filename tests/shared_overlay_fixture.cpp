#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <cstdio>
// User/Claude 2026-10-29: standalone declarations for adapter instrumentation.
#define COST_SCOPE(name) do {} while(0)
#ifndef NO_RUNTIME_LOG
namespace terrain_trees {alignas(8) static volatile LONG64 perf_frame_us,perf_total_us,perf_calls,perf_max_us;}
namespace log_perf {alignas(8) static volatile LONG64 calls,total_us,max_us;}
static void log_event(const char *,unsigned,unsigned,unsigned) {}
#endif
// User 2026-10-29: standalone overlay fixture uses a no-op terrain observer.
struct ID3D11Device;
struct ID3D11DeviceContext;
namespace terrain_render {static void on_device(ID3D11Device *){} static void on_present(ID3D11DeviceContext *){} static bool needs_present(){return false;}}
#include "../src/native_unified_overlay.inl"
#include "../src/native_ccz_preview.inl"
// User 2026-10-31: 조조전 병종 그림 draws from the same Present hook; no-op here.
namespace ccz_sprites {static bool needs_present(){return false;} static void on_present(ID3D11Device *,ID3D11DeviceContext *,ID3D11Texture2D *,const D3D11_TEXTURE2D_DESC &){}}
// W4-3: the draw loop is driven by the overlay service; the fixture joins the same rows production generates (module_overlay_subscriptions.inl).
// local298: the relay duel round number (row duel_digits) is drawn by its own self-contained pass.
#include "../src/native_duel_digits.inl"
static struct {bool ccz_sprites=true,advisor_briefing=true,ccz_bands=false,duel_relay=true;} mod_features;   // ccz_bands: idle band flags share the 조조전 row (local248)
#include "../src/module_overlay_subscriptions.inl"
static unsigned checks;
static void check(bool ok,const char *label){++checks;if(!ok){fprintf(stderr,"%s\n",label);ExitProcess(1);}}
int main() {
    MODULE_SUBSCRIBE_OVERLAY();
    using namespace unified_overlay;
    Bounds full=bounds(1920,1080);check(full.x==218 && full.y==156 && full.width==164 && full.height==42,"native UI anchor at full HD");
    Bounds letter=bounds(1280,800);check(letter.x==145 && letter.y==144 && letter.width==109 && letter.height==28,"windowed letterboxing preserves native UI position");
    check(!contains({},0,0) && !client_hit(full,100,100,0,0,1920,1080),"invalid geometry cannot consume input");
    check(client_hit(full,220,158,1920,1080,1920,1080),"client-to-backbuffer hit mapping");
    check(!contains(full,full.x+full.width,full.y),"right edge belongs to the game");
    check(eligible(true,true,true,false) && !eligible(true,true,true,true) && !eligible(true,true,false,false),"native map and popup gate");
    Pointer pointer;bool fire=false;
    check(!pointer.down(true,false,1) && !pointer.up(true,true,1,fire) && !fire,"outside clicks pass through");
    check(pointer.down(true,true,1) && pointer.up(true,true,1,fire) && fire,"inside click activates once on release");
    check(!pointer.up(true,true,1,fire) && !fire,"duplicate releases do not reopen the dialog");
    pointer.down(true,true,1);check(pointer.up(true,false,1,fire) && !fire,"dragging off the button cancels");
    pointer.down(true,true,1);check(pointer.up(true,true,2,fire) && !fire,"state transitions cancel a pending click");
    HWND target=CreateWindowExW(0,L"STATIC",L"personnel-overlay-fixture",WS_OVERLAPPEDWINDOW,0,0,1280,830,nullptr,nullptr,nullptr,nullptr);
    check(target!=nullptr,"hidden fixture window");
    DXGI_SWAP_CHAIN_DESC desc={};desc.BufferCount=1;desc.BufferDesc.Width=1280;desc.BufferDesc.Height=800;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow=target;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain *chain=nullptr;ID3D11Device *test_device=nullptr;ID3D11DeviceContext *test_context=nullptr;
    HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&test_device,nullptr,&test_context);
    check(SUCCEEDED(result) && chain,"real D3D11 fixture device");
    install();check(original_present && (*reinterpret_cast<void ***>(chain))[8]==reinterpret_cast<void *>(&present),"reference-style shared Present hook is actually installed");
    ID3D11Texture2D *back=nullptr;check(SUCCEEDED(chain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&back))),"fixture backbuffer");
    ID3D11RenderTargetView *view=nullptr;check(SUCCEEDED(test_device->CreateRenderTargetView(back,nullptr,&view)),"fixture render target");
    const float clear[]={16.0f/255,32.0f/255,48.0f/255,1};test_context->ClearRenderTargetView(view,clear);
    show(true);draw(chain,0);check(read_flag(&ready)==1 && texture && window==target,"production renderer binds and draws into the game-like backbuffer");
    D3D11_TEXTURE2D_DESC staging_desc={};back->GetDesc(&staging_desc);staging_desc.Usage=D3D11_USAGE_STAGING;
    staging_desc.BindFlags=0;staging_desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;staging_desc.MiscFlags=0;
    ID3D11Texture2D *staging=nullptr;check(SUCCEEDED(test_device->CreateTexture2D(&staging_desc,nullptr,&staging)),"readback texture");
    test_context->CopyResource(staging,back);D3D11_MAPPED_SUBRESOURCE mapped={};check(SUCCEEDED(test_context->Map(staging,0,D3D11_MAP_READ,0,&mapped)),"read rendered pixels");
    auto bytes=static_cast<const unsigned char *>(mapped.pData);
    unsigned outside=*reinterpret_cast<const unsigned *>(bytes);
    unsigned inside=*reinterpret_cast<const unsigned *>(bytes+(letter.y+3)*mapped.RowPitch+(letter.x+3)*4);
    check(outside==0xff302010u && inside!=outside,"only the small button rectangle changes; existing game pixels are preserved");test_context->Unmap(staging,0);
    auto cached=texture;draw(chain,0);check(texture==cached,"unchanged frames reuse the cached small texture");
    RECT client={};GetClientRect(target,&client);int x=(letter.x+letter.width/2)*client.right/1280,y=(letter.y+letter.height/2)*client.bottom/800;
    SendMessageW(target,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(target,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    check(take_click() && !take_click(),"real window messages enqueue one UI-thread request");
    show(false);SendMessageW(target,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(target,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    check(!take_click(),"hidden overlay cannot open a dialog");
#ifndef NO_RUNTIME_LOG
    {   // local298: the relay duel round number through the same Present loop: "12" over the medallion (967.5, 101 of 1920 x 1080 ->
        // 645, 107 at 1280 x 800 letterboxed), the glyph's alpha kept (game pixels around it untouched), the game's state back after
        test_context->ClearRenderTargetView(view,clear);
        test_context->OMSetRenderTargets(1,&view,nullptr);
        const D3D11_VIEWPORT game_vp{0.0f,0.0f,1280.0f,800.0f,0.0f,1.0f};test_context->RSSetViewports(1,&game_vp);
        check(!duel_digits::gate(),"no round number, no overlay work");
        duel_digits::publish(12,967.5f,101.0f);
        check(duel_digits::gate(),"a published round number opens the overlay gate");
        draw(chain,0);
        check(!duel_digits::broken() && duel_digits::built && duel_digits::draws==1,"round number glyphs build and draw on a real device");
        ID3D11RenderTargetView *bound=nullptr;test_context->OMGetRenderTargets(1,&bound,nullptr);
        check(bound==view,"the game's render target comes back after the number");if(bound)bound->Release();
        UINT vps=1;D3D11_VIEWPORT got{};test_context->RSGetViewports(&vps,&got);
        check(vps==1 && got.Width==1280.0f && got.Height==800.0f,"the game's viewport comes back after the number");
        test_context->CopyResource(staging,back);D3D11_MAPPED_SUBRESOURCE map={};
        check(SUCCEEDED(test_context->Map(staging,0,D3D11_MAP_READ,0,&map)),"read the number's pixels");
        auto at=[&](int px,int py){return *reinterpret_cast<const unsigned *>(static_cast<const unsigned char *>(map.pData)+py*map.RowPitch+px*4);};
        unsigned changed=0,gold=0,dark=0,far_changed=0;
        for(int py=0;py<800;++py)for(int px=0;px<1280;++px) {
            const unsigned c=at(px,py);
            const bool near_box=px>=585 && px<=705 && py>=55 && py<=160;
            if(c==0xff302010u)continue;
            if(!near_box){++far_changed;continue;}
            ++changed;
            const unsigned r=c&255u,g=(c>>8)&255u,b=(c>>16)&255u;
            if(r>200 && g>110 && b<170 && r>g && g>b)++gold;
            if(r<70 && g<40 && b<40)++dark;
        }
        test_context->Unmap(staging,0);
        check(far_changed==0,"nothing drawn away from the medallion");
        check(changed>800 && gold>60 && dark>20,"gold fill, dark stroke and glow around the number");
        duel_digits::publish(0,0.0f,0.0f);
        check(!duel_digits::gate(),"number 0 closes the overlay gate");
        printf("{\"duel_digits\":{\"changed\":%u,\"gold\":%u,\"dark\":%u}}\n",changed,gold,dark);
    }
    {   // local298 투지 bars (same row): left 200 of 300 with 100 the list will spend (red), right 150 filling from the right; at 1280 x 800
        // the left bar is x 260.7-582, the right 698-1019.3, y 116-121.3; nothing drawn elsewhere, the number word stays 0
        test_context->ClearRenderTargetView(view,clear);
        duel_digits::place_bar(0,391.0f,114.0f,482.0f,8.0f);duel_digits::place_bar(1,1047.0f,114.0f,482.0f,8.0f);
        const int morale[2]={200,150},red[2]={100,0};
        duel_digits::publish_bars(true,morale,red);
        check(duel_digits::gate(),"투지 bars open the overlay gate");
        const unsigned before=duel_digits::draws;
        draw(chain,0);
        check(!duel_digits::broken() && duel_digits::draws==before+1,"투지 bars draw on a real device");
        test_context->CopyResource(staging,back);D3D11_MAPPED_SUBRESOURCE map={};
        check(SUCCEEDED(test_context->Map(staging,0,D3D11_MAP_READ,0,&map)),"read the bars' pixels");
        auto at=[&](int px,int py){return *reinterpret_cast<const unsigned *>(static_cast<const unsigned char *>(map.pData)+py*map.RowPitch+px*4);};
        auto yellow=[&](int px,int py){const unsigned c=at(px,py),r=c&255u,g=(c>>8)&255u,b=(c>>16)&255u;return r>200 && g>140 && b<120;};
        auto redish=[&](int px,int py){const unsigned c=at(px,py),r=c&255u,g=(c>>8)&255u,b=(c>>16)&255u;return r>150 && g<120 && b<100;};
        auto dark=[&](int px,int py){const unsigned c=at(px,py),r=c&255u,g=(c>>8)&255u,b=(c>>16)&255u;return r<50 && g<40 && b<40;};
        unsigned far_changed=0;
        for(int py=0;py<800;++py)for(int px=0;px<1280;++px) {
            const bool box=py>=112 && py<=125 && ((px>=256 && px<=587) || (px>=693 && px<=1024));
            if(!box && at(px,py)!=0xff302010u)++far_changed;
        }
        const bool left_ok=yellow(300,118) && redish(420,118) && dark(530,118),right_ok=yellow(950,118) && dark(750,118);
        test_context->Unmap(staging,0);
        check(far_changed==0,"nothing drawn away from the two bars");
        check(left_ok,"left bar: yellow 투지, red planned spend, dark track");
        check(right_ok,"right bar fills from the right");
        duel_digits::publish_bars(false,morale,red);
        check(!duel_digits::gate(),"bars off closes the overlay gate");
    }
    {   // local299 relay command window, local305 strip and hold list (same row) on a 1920 x 1080 frame over a scene-like gradient: the
        // strip alone draws only inside its panel (0, 760, 520 x 320) with the chosen stance lit and 필살 gold; a full hold with the 필살
        // list only inside (0, 520, 520 x 560), the picked one gold, an allowed one red, 「확정」 gold; SAN14_DUEL_PREVIEW=<file> also writes
        // a frame with the round number and both 투지 bars as raw RGBA 1920 x 1080 (the preview picture), SAN14_DUEL_PREVIEW_HOLD=<file>
        // the 필살 hold frame
        const int W=1920,H=1080;
        std::vector<unsigned> bg(static_cast<size_t>(W)*H);
        for(int py=0;py<H;++py) {
            const float t=static_cast<float>(py)/static_cast<float>(H-1);
            const unsigned r=static_cast<unsigned>(78.0f+58.0f*t),g=static_cast<unsigned>(88.0f+34.0f*t),b=static_cast<unsigned>(96.0f-36.0f*t);
            for(int px=0;px<W;++px)bg[static_cast<size_t>(py)*W+px]=r|(g<<8)|(b<<16)|0xff000000u;
        }
        D3D11_TEXTURE2D_DESC fd{};fd.Width=W;fd.Height=H;fd.MipLevels=fd.ArraySize=1;fd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;fd.SampleDesc.Count=1;
        fd.Usage=D3D11_USAGE_DEFAULT;fd.BindFlags=D3D11_BIND_RENDER_TARGET;
        const D3D11_SUBRESOURCE_DATA init{bg.data(),static_cast<UINT>(W*4),0};
        ID3D11Texture2D *frame=nullptr,*frame_read=nullptr;
        check(SUCCEEDED(test_device->CreateTexture2D(&fd,&init,&frame)),"1920 x 1080 frame");
        D3D11_TEXTURE2D_DESC rd=fd;rd.Usage=D3D11_USAGE_STAGING;rd.BindFlags=0;rd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        check(SUCCEEDED(test_device->CreateTexture2D(&rd,nullptr,&frame_read)),"1920 x 1080 readback");
        namespace dd=duel_digits;
        dd::WinView sv;sv.stance=1;sv.now=2;sv.spc=1;sv.swp=0;   // 방어중시 chosen, 투지중시 in play, 필살 on, no waiting member
        {
            dd::WinView fv;fv.mode=4;fv.list=2;fv.next=17;fv.now=3;fv.stance=2;fv.spc=2;fv.swp=3;fv.autoplay=true;fv.ok=true;fv.item[0]=fv.item[4]=true;
            fv.pick=4;fv.member[0]=2;fv.member[1]=0;
            const LONG64 st=dd::window_state(fv);
            check((st&1) && ((st>>dd::WMode)&7)==4 && ((st>>dd::WList)&3)==2 && ((st>>dd::WNext)&127)==17 && ((st>>dd::WNow)&3)==3 &&
                  ((st>>dd::WStance)&3)==2 && ((st>>dd::WSpc)&3)==2 && ((st>>dd::WSwap)&3)==3 && ((st>>dd::WAuto)&1) && ((st>>dd::WOk)&1) &&
                  ((st>>dd::WItem)&63)==17 && ((st>>dd::WPick)&7)==5 && ((st>>dd::WMember)&15)==(3u|(1u<<2)),"the window state packs every field");
            check(((st>>dd::WRet)&3)==0 && ((st>>dd::WStat)&1)==0 && ((st>>dd::WCrit)&0x7FF)==0,"local306 fields stay 0 when unset");
            // local306: 퇴각 on the 교대 place, the status line (critical charge 0-6, own / other guard 합, clamped)
            dd::WinView rv=fv;rv.ret=2;rv.stat=true;rv.crit=6;rv.guard=11;rv.foe=4;
            const LONG64 rs=dd::window_state(rv);
            check(((rs>>dd::WRet)&3)==2 && ((rs>>dd::WStat)&1) && ((rs>>dd::WCrit)&7)==6 && ((rs>>dd::WGuard)&15)==11 && ((rs>>dd::WFoe)&15)==4 &&
                  (rs&((1LL<<dd::WRet)-1))==(st&((1LL<<dd::WRet)-1)),"the window state packs 퇴각 and the status line without touching the rest");
            rv.crit=9;rv.guard=20;rv.foe=-3;
            const LONG64 cs=dd::window_state(rv);
            check(((cs>>dd::WCrit)&7)==6 && ((cs>>dd::WGuard)&15)==15 && ((cs>>dd::WFoe)&15)==0,"the status numbers clamp to their bits");
            rv.stat=false;
            check(((dd::window_state(rv)>>dd::WStat)&0x7FF)==0,"no status line: its numbers are not packed");
            rv.list=3;rv.mode=5;
            check(((dd::window_state(rv)>>dd::WList)&3)==3 && ((dd::window_state(rv)>>dd::WMode)&7)==5,"the 퇴각 hold: list 3, mode 5");
        }
        dd::publish_window(dd::window_state(sv));
        check(dd::gate(),"the strip opens the overlay gate");
        const LONG before=dd::window_draws();
        dd::draw(test_device,test_context,frame,fd);
        check(!dd::broken() && !dd::window_broken() && dd::window_draws()==before+1,"the strip builds and draws on a real device");
        test_context->CopyResource(frame_read,frame);D3D11_MAPPED_SUBRESOURCE map={};
        check(SUCCEEDED(test_context->Map(frame_read,0,D3D11_MAP_READ,0,&map)),"read the strip's pixels");
        auto at=[&](int px,int py){return *reinterpret_cast<const unsigned *>(static_cast<const unsigned char *>(map.pData)+py*map.RowPitch+px*4);};
        auto rgb=[&](int px,int py,unsigned &r,unsigned &g,unsigned &b){const unsigned c=at(px,py);r=c&255u;g=(c>>8)&255u;b=(c>>16)&255u;};
        auto changed=[&](int x0,int y0,int x1,int y1,unsigned &in,unsigned &out) {
            in=out=0;
            for(int py=0;py<H;++py)for(int px=0;px<W;++px) {
                const bool same=at(px,py)==bg[static_cast<size_t>(py)*W+px];
                if(px>=x0 && px<x1 && py>=y0 && py<y1){if(!same)++in;}
                else if(!same)++out;
            }
        };
        // a plate's colour: pixels of a kind inside its 214 x 52 place (red: the lit / 필살 plates, gold also matches red, blue)
        struct Hue {unsigned red,gold,blue;};
        auto plate_hue=[&](int x0,int y0) {
            Hue h{0,0,0};
            for(int py=y0;py<y0+52;++py)for(int px=x0;px<x0+214;++px) {
                const unsigned c=at(px,py),cr=c&255u,cg=(c>>8)&255u,cb=(c>>16)&255u;
                if(cr>100 && cr>2*cg && cr>cb)++h.red;
                if(cr>90 && cr>cg && cg>cb && cb<80)++h.gold;
                if(cb>cr+20 && cb>cg)++h.blue;
            }
            return h;
        };
        auto gold_only=[](Hue h){return h.gold>5000 && h.red<500;};
        unsigned inside_changed=0,far_changed=0,r,g,b;
        changed(0,760,520,1080,inside_changed,far_changed);
        const Hue h_spc=plate_hue(276,818),h_chosen=plate_hue(36,876),h_other=plate_hue(36,818);
        const bool gold=gold_only(h_spc),lit=h_chosen.red>5000,dark=h_other.red<500 && h_other.gold<1000;
        if(!gold || !lit || !dark)printf("{\"duel_strip_hues\":[%u,%u,%u,%u,%u]}\n",h_spc.gold,h_spc.red,h_chosen.red,h_other.red,h_other.gold);
        test_context->Unmap(frame_read,0);
        check(far_changed==0,"the strip draws nothing outside its panel");
        check(inside_changed>80000,"the strip panel is drawn");
        check(gold && lit && dark,"the strip: 필살 gold, the chosen stance lit, the others dark");
        unsigned hold_inside=0;
        {   // the 필살 hold: list (필살기 picked, 급소 allowed, 무쌍 not), 「확정」
            test_context->UpdateSubresource(frame,0,nullptr,bg.data(),static_cast<UINT>(W*4),0);
            dd::WinView hv=sv;hv.mode=2;hv.list=1;hv.next=12;hv.ok=true;hv.item[0]=hv.item[1]=hv.item[3]=hv.item[4]=true;hv.pick=0;
            dd::publish_window(dd::window_state(hv));
            dd::draw(test_device,test_context,frame,fd);
            test_context->CopyResource(frame_read,frame);
            check(SUCCEEDED(test_context->Map(frame_read,0,D3D11_MAP_READ,0,&map)),"read the hold's pixels");
            unsigned far2=0;
            changed(0,520,520,1080,hold_inside,far2);
            const Hue h_pick=plate_hue(36,548),h_crit=plate_hue(36,606),h_musou=plate_hue(36,664),h_kiai=plate_hue(276,548),h_ok=plate_hue(261,992);
            const bool picked=gold_only(h_pick),red=h_crit.red>5000,off=h_musou.red<500 && h_musou.gold<1000 && h_musou.blue<500;
            const bool blue_on=h_kiai.blue>5000,ok_gold=gold_only(h_ok);
            if(!picked || !red || !off || !blue_on || !ok_gold)
                printf("{\"duel_hold_hues\":[%u,%u,%u,%u,%u,%u,%u]}\n",h_pick.gold,h_pick.red,h_crit.red,h_musou.gold,h_kiai.blue,h_ok.gold,h_ok.red);
            char hold_out[MAX_PATH]={};
            if(GetEnvironmentVariableA("SAN14_DUEL_PREVIEW_HOLD",hold_out,MAX_PATH)>0) {
                FILE *f=nullptr;
                if(fopen_s(&f,hold_out,"wb")==0 && f) {
                    for(int py=0;py<H;++py)fwrite(static_cast<const unsigned char *>(map.pData)+py*map.RowPitch,4,W,f);
                    fclose(f);
                }
            }
            test_context->Unmap(frame_read,0);
            check(far2==0 && hold_inside>inside_changed,"a full hold draws only inside the big panel, more than the strip");
            check(picked && red && off && blue_on && ok_gold,"the hold: the picked 필살 gold, allowed ones in their colours (red, blue), 무쌍 off dim, 「확정」 gold");
        }
        {   // local301: the 교대 list (two members; their names drawn with GDI at run time), then the join sticker on the left half;
            // SAN14_DUEL_PREVIEW_PICK=<file> writes that frame as raw RGBA 1920 x 1080
            test_context->UpdateSubresource(frame,0,nullptr,bg.data(),static_cast<UINT>(W*4),0);
            dd::NameSet ns{};
            memcpy(ns.name[0],L"유비",sizeof(L"유비"));memcpy(ns.name[1],L"조운",sizeof(L"조운"));memcpy(ns.name[2],L"제갈량",sizeof(L"제갈량"));
            ns.hp[0]=80;ns.hp[1]=62;ns.hp[2]=45;
            dd::publish_names(ns);
            dd::WinView pv=sv;pv.mode=3;pv.list=2;pv.next=8;pv.ok=true;pv.swp=1;pv.member[0]=1;pv.member[1]=2;
            dd::publish_window(dd::window_state(pv));
            dd::publish_sticker(dd::SJoin,-1,-1);
            check(dd::gate(),"the 교대 list and a join sticker open the overlay gate");
            dd::draw(test_device,test_context,frame,fd);
            check(!dd::broken() && dd::names_built>=0 && dd::name_tex[1].v && dd::label_tex[1].v && dd::label_tex[2].v,"member names build with GDI");
            check(dd::sticker_made==3 && !dd::sticker_broken() && dd::sticker_ready(dd::SJoin) && dd::sticker_ready(dd::SSpecial) && !dd::sticker_ready(dd::SCrit),
                  "the join sticker builds first, one kind a frame since the window's first frame (and says so to the UI thread)");
            test_context->CopyResource(frame_read,frame);
            check(SUCCEEDED(test_context->Map(frame_read,0,D3D11_MAP_READ,0,&map)),"read the 교대 list frame");
            const bool blue=plate_hue(36,548).blue>5000;
            const bool stuck=at(500,330)!=bg[static_cast<size_t>(330)*W+500],right_clear=at(1420,330)==bg[static_cast<size_t>(330)*W+1420];
            char pick_out[MAX_PATH]={};
            if(GetEnvironmentVariableA("SAN14_DUEL_PREVIEW_PICK",pick_out,MAX_PATH)>0) {
                FILE *f=nullptr;
                if(fopen_s(&f,pick_out,"wb")==0 && f) {
                    for(int py=0;py<H;++py)fwrite(static_cast<const unsigned char *>(map.pData)+py*map.RowPitch,4,W,f);
                    fclose(f);
                }
            }
            test_context->Unmap(frame_read,0);
            check(blue,"a 교대 member on the blue plate");
            check(stuck && right_clear,"the sticker on the joiner's half only");
            const LONG64 st=dd::sticker_state(dd::SJoin,-1,-1,5000);
            check(dd::sticker_on(st,5000) && dd::sticker_on(st,6599) && !dd::sticker_on(st,6600) && !dd::sticker_on(0,5000),"the sticker lasts kStickerMs");
            check(dd::sticker_scale(0)>1.6f && dd::sticker_scale(800)==1.0f && dd::sticker_scale(1600)==0.0f,"the sticker pops in and shrinks away");
            check(dd::sticker_state(-1,-1,-1,5000)==0 && ((dd::sticker_state(dd::SSwap,dd::SDraw,dd::SWin,5000)>>1)&0xFFF)==(2u|(9u<<4)|(8u<<8)),
                  "the sticker state: a kind + 1 per place, nothing = 0");
            for(int i=0;i<dd::SKinds+2 && dd::sticker_made<dd::SKinds;++i)dd::draw(test_device,test_context,frame,fd);
            check(dd::sticker_made==dd::SKinds && dd::sticker_ready(dd::SDraw) && !dd::sticker_broken(),"every sticker kind builds, one a frame");
            dd::publish_window(0);
            {   // local303: each kind's colours on its place (교대 green, 필살기 / 급소 red, 무쌍 purple, 기합 / 견수 blue, 판정승 gold, 난입
                // red-orange; local306: 연속 공격 orange, 자동 견수 teal, 투지 상승 amber, 퇴각 slate; 무승부 in the centre only);
                // SAN14_DUEL_PREVIEW_STICKERS=<file> writes the seven frames' band y 120-539 stacked as raw RGBA 1920 x 2940 (the preview picture)
                static const int kFrames=7;
                static const int lay[kFrames][3]={{dd::SSwap,dd::SSpecial,-1},{dd::SMusou,dd::SKiai,-1},{dd::SGuard,dd::SCrit,-1},{dd::SWin,dd::SJoin,-1},
                                                  {dd::SCombo,dd::SAutoGuard,-1},{dd::SSpirit,dd::SRetreat,-1},{-1,-1,dd::SDraw}};
                std::vector<unsigned> band(static_cast<size_t>(W)*420*kFrames);
                unsigned hue[kFrames][2]={};
                bool centre_ok=false;
                for(int f=0;f<kFrames;++f) {
                    test_context->UpdateSubresource(frame,0,nullptr,bg.data(),static_cast<UINT>(W*4),0);
                    InterlockedExchange64(&dd::sticker,dd::sticker_state(lay[f][0],lay[f][1],lay[f][2],GetTickCount64()-800));
                    dd::draw(test_device,test_context,frame,fd);
                    test_context->CopyResource(frame_read,frame);
                    check(SUCCEEDED(test_context->Map(frame_read,0,D3D11_MAP_READ,0,&map)),"read a sticker frame");
                    for(int py=120;py<540;++py)memcpy(&band[(static_cast<size_t>(f)*420+py-120)*W],static_cast<const unsigned char *>(map.pData)+py*map.RowPitch,W*4);
                    for(int half=0;half<2;++half)for(int py=150;py<510;++py)for(int px=half?1170:250;px<(half?1670:750);++px) {
                        rgb(px,py,r,g,b);
                        const int k=lay[f][half];
                        const bool want=k==dd::SSwap?g>r+40 && g>b+40:k==dd::SSpecial || k==dd::SCrit?r>g+90 && r>b+90:k==dd::SMusou?r>g+40 && b>g+60:
                                        k==dd::SKiai || k==dd::SGuard?b>r+60 && b>g+20:k==dd::SWin?r>180 && g>120 && b<90:k==dd::SJoin?r>180 && b<70:
                                        k==dd::SCombo?r>180 && r>g+60 && b<90:k==dd::SAutoGuard?g>r+50 && b>r+50:
                                        k==dd::SSpirit?r>170 && g>95 && r>g+30 && b<80:k==dd::SRetreat?b>g+25 && b>r+15:false;
                        if(want)++hue[f][half];
                    }
                    if(f==kFrames-1)centre_ok=at(960,330)!=bg[static_cast<size_t>(330)*W+960] && at(300,330)==bg[static_cast<size_t>(330)*W+300] &&
                                      at(1620,330)==bg[static_cast<size_t>(330)*W+1620];
                    test_context->Unmap(frame_read,0);
                }
                bool hues=true;
                for(int f=0;f<kFrames-1;++f)for(int half=0;half<2;++half)if(hue[f][half]<3000)hues=false;
                if(!hues)printf("{\"duel_sticker_hues\":[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]}\n",hue[0][0],hue[0][1],hue[1][0],hue[1][1],hue[2][0],hue[2][1],
                                hue[3][0],hue[3][1],hue[4][0],hue[4][1],hue[5][0],hue[5][1]);
                check(hues,"each sticker kind in its colours on its half");
                check(centre_ok,"「무승부!」 in the centre, both halves clear");
                char stk_out[MAX_PATH]={};
                if(GetEnvironmentVariableA("SAN14_DUEL_PREVIEW_STICKERS",stk_out,MAX_PATH)>0) {
                    FILE *f=nullptr;
                    if(fopen_s(&f,stk_out,"wb")==0 && f){fwrite(band.data(),4,band.size(),f);fclose(f);}
                }
            }
            dd::clear_sticker();dd::publish_window(0);
            check(!dd::gate(),"sticker 0 and window 0 close the gate");
            {   // local313: the 연장 badge alone (window and stickers off): made on its first frame after the stickers, an upright crimson
                // plate with cream text only inside its place right above the skip buttons (x 1668-1876, y 883-950 at 1920 x 1080, well
                // right of the window panel 0-520); it stays until clear_badge. SAN14_DUEL_PREVIEW_BADGE=<file> writes x 1500-1919,
                // y 800-1079 as raw RGBA 420 x 280 (the preview picture)
                check(!dd::badge_ready(),"no badge before one is shown");
                test_context->UpdateSubresource(frame,0,nullptr,bg.data(),static_cast<UINT>(W*4),0);
                InterlockedExchange64(&dd::badge,static_cast<LONG64>((GetTickCount64()-1000)|1ull));   // past its pop
                check(dd::gate(),"a shown badge opens the overlay gate");
                dd::draw(test_device,test_context,frame,fd);
                check(dd::badge_ready() && dd::badge_wh[0]>150 && dd::badge_wh[0]<260 && dd::badge_wh[1]>50 && dd::badge_wh[1]<90,"the badge plate builds on a real device");
                const int bx1=1876,by1=950,bx0=bx1-dd::badge_wh[0],by0=by1-dd::badge_wh[1];
                test_context->CopyResource(frame_read,frame);
                check(SUCCEEDED(test_context->Map(frame_read,0,D3D11_MAP_READ,0,&map)),"read the badge's pixels");
                unsigned in=0,out=0,crimson=0,cream=0,black=0;
                for(int py=0;py<H;++py)for(int px=0;px<W;++px) {
                    if(at(px,py)==bg[static_cast<size_t>(py)*W+px])continue;
                    if(px<bx0-1 || px>bx1+1 || py<by0-1 || py>by1+1){++out;continue;}
                    ++in;rgb(px,py,r,g,b);
                    if(r>100 && r>g+60 && r>b+20)++crimson;
                    if(r>220 && g>200 && b>160)++cream;
                    if(r<30 && g<30 && b<30)++black;
                }
                std::vector<unsigned> crop(static_cast<size_t>(420)*280);
                for(int py=800;py<1080;++py)memcpy(&crop[static_cast<size_t>(py-800)*420],static_cast<const unsigned char *>(map.pData)+py*map.RowPitch+1500*4,420*4);
                test_context->Unmap(frame_read,0);
                if(out || crimson<4000 || cream<300 || black<400)printf("{\"duel_badge\":{\"in\":%u,\"out\":%u,\"crimson\":%u,\"cream\":%u,\"black\":%u}}\n",in,out,crimson,cream,black);
                check(out==0 && bx0>520,"the badge draws only in its place, right of the window panel");
                check(crimson>=4000 && cream>=300 && black>=400,"a crimson plate, cream text and a black rim / edge");
                check(dd::badge_scale(0)>1.3f && dd::badge_scale(5000)==1.0f && dd::badge_scale(600000)==1.0f,"the badge pops in and stays");
                char badge_out[MAX_PATH]={};
                if(GetEnvironmentVariableA("SAN14_DUEL_PREVIEW_BADGE",badge_out,MAX_PATH)>0) {
                    FILE *f=nullptr;
                    if(fopen_s(&f,badge_out,"wb")==0 && f){fwrite(crop.data(),4,crop.size(),f);fclose(f);}
                }
                dd::clear_badge();
                check(!dd::gate(),"clear_badge closes the gate");
            }
            dd::publish_window(dd::window_state(sv));
        }
        {   // local306: 퇴각 on the 교대 place (on red, pending blue, short of 투지 dark), the status line under the strip, the 퇴각 hold
            // (퇴각 picked gold, its hint on row 1, 「돌아가기」); SAN14_DUEL_PREVIEW_RETREAT=<file> writes the strip with the status line, then the
            // hold, as raw RGBA 1920 x 2160
            std::vector<unsigned> base(static_cast<size_t>(W)*H),shot(static_cast<size_t>(W)*H);
            auto take=[&](const dd::WinView &v,std::vector<unsigned> &out) {
                test_context->UpdateSubresource(frame,0,nullptr,bg.data(),static_cast<UINT>(W*4),0);
                dd::publish_window(dd::window_state(v));
                dd::draw(test_device,test_context,frame,fd);
                test_context->CopyResource(frame_read,frame);
                check(SUCCEEDED(test_context->Map(frame_read,0,D3D11_MAP_READ,0,&map)),"read a 퇴각 frame");
                for(int py=0;py<H;++py)memcpy(&out[static_cast<size_t>(py)*W],static_cast<const unsigned char *>(map.pData)+py*map.RowPitch,W*4);
                test_context->Unmap(frame_read,0);
            };
            auto vhue=[&](const std::vector<unsigned> &v,int x0,int y0) {
                Hue h{0,0,0};
                for(int py=y0;py<y0+52;++py)for(int px=x0;px<x0+214;++px) {
                    const unsigned c=v[static_cast<size_t>(py)*W+px],cr=c&255u,cg=(c>>8)&255u,cb=(c>>16)&255u;
                    if(cr>100 && cr>2*cg && cr>cb)++h.red;
                    if(cr>90 && cr>cg && cg>cb && cb<80)++h.gold;
                    if(cb>cr+20 && cb>cg)++h.blue;
                }
                return h;
            };
            auto diff=[&](int x0,int y0,int x1,int y1,unsigned &in,unsigned &out) {
                in=out=0;
                for(int py=0;py<H;++py)for(int px=0;px<W;++px) {
                    const size_t i=static_cast<size_t>(py)*W+px;
                    if(base[i]==shot[i])continue;
                    if(px>=x0 && px<x1 && py>=y0 && py<y1)++in;else ++out;
                }
            };
            dd::WinView tv=sv;tv.ret=1;
            take(tv,base);
            const Hue h_on=vhue(base,271,876);
            tv.stat=true;tv.crit=6;tv.guard=3;tv.foe=12;
            take(tv,shot);
            unsigned stat_in=0,stat_out=0;
            diff(0,1046,520,1080,stat_in,stat_out);
            const std::vector<unsigned> stat_shot=shot;
            tv.stat=false;tv.ret=2;take(tv,shot);
            const Hue h_pend=vhue(shot,271,876);
            tv.ret=3;take(tv,shot);
            const Hue h_short=vhue(shot,271,876);
            const bool on_red=h_on.red>5000,pend_blue=h_pend.blue>5000,short_dark=h_short.red<500 && h_short.gold<1000 && h_short.blue<500;
            if(!on_red || !pend_blue || !short_dark)
                printf("{\"duel_retreat_hues\":[%u,%u,%u,%u,%u]}\n",h_on.red,h_pend.blue,h_short.red,h_short.gold,h_short.blue);
            check(on_red && pend_blue && short_dark,"the 교대 place as 퇴각: on red, pending blue, short of 투지 dark");
            check(stat_in>300 && stat_out==0,"the status line draws under the strip only");
            dd::WinView qv=sv;qv.mode=5;qv.next=9;qv.ok=true;
            take(qv,base);   // the 퇴각 hold before its list
            qv.list=3;qv.pick=0;qv.item[0]=true;
            take(qv,shot);
            const bool ret_gold=gold_only(vhue(shot,36,548));
            unsigned hint_in=0,rest=0,back_in=0,empty_in=0,empty2_in=0;
            diff(36,600,520,662,hint_in,rest);
            diff(266,664,480,716,back_in,rest);
            diff(36,664,250,716,empty_in,rest);
            diff(276,548,490,590,empty2_in,rest);   // the no-list hold's own hint (y 608) reaches row 0's bottom edge
            if(!ret_gold || hint_in<300 || back_in<3000 || empty_in || empty2_in)
                printf("{\"duel_retreat_hold\":[%u,%u,%u,%u]}\n",hint_in,back_in,empty_in,empty2_in);
            check(ret_gold,"the 퇴각 hold: 퇴각 picked gold");
            check(hint_in>=300 && back_in>=3000 && empty_in==0 && empty2_in==0,"the 퇴각 hold: the hint on row 1, 「돌아가기」, the other places empty");
            char ret_out[MAX_PATH]={};
            if(GetEnvironmentVariableA("SAN14_DUEL_PREVIEW_RETREAT",ret_out,MAX_PATH)>0) {
                FILE *f=nullptr;
                if(fopen_s(&f,ret_out,"wb")==0 && f){fwrite(stat_shot.data(),4,stat_shot.size(),f);fwrite(shot.data(),4,shot.size(),f);fclose(f);}
            }
            dd::publish_window(dd::window_state(sv));
        }
        char out[MAX_PATH]={};
        if(GetEnvironmentVariableA("SAN14_DUEL_PREVIEW",out,MAX_PATH)>0) {
            test_context->UpdateSubresource(frame,0,nullptr,bg.data(),static_cast<UINT>(W*4),0);
            dd::publish(12,967.5f,101.0f);
            dd::place_bar(0,391.0f,114.0f,482.0f,8.0f);dd::place_bar(1,1047.0f,114.0f,482.0f,8.0f);
            const int morale[2]={200,0},red[2]={100,0};
            dd::publish_bars(true,morale,red);
            dd::draw(test_device,test_context,frame,fd);
            test_context->CopyResource(frame_read,frame);
            check(SUCCEEDED(test_context->Map(frame_read,0,D3D11_MAP_READ,0,&map)),"read the preview frame");
            FILE *f=nullptr;
            if(fopen_s(&f,out,"wb")==0 && f) {
                for(int py=0;py<H;++py)fwrite(static_cast<const unsigned char *>(map.pData)+py*map.RowPitch,4,W,f);
                fclose(f);
            }
            test_context->Unmap(frame_read,0);
            dd::publish(0,0.0f,0.0f);dd::publish_bars(false,morale,red);
        }
        dd::publish_window(0);
        check(!dd::gate(),"window 0 closes the overlay gate");
        frame_read->Release();frame->Release();
        printf("{\"duel_window\":{\"strip\":%u,\"hold\":%u,\"build_ms\":%u}}\n",inside_changed,hold_inside,dd::win_build_ms);
    }
    {   // local308 helper chips (mock C): the rings stay under the 투지 bars (y 122 at the original gauge) and clear of both main portraits
        // (discs to x 386 / from x 1556 with their frames); on a frame: a full ring blue all round, a half ring blue on the right and dark on
        // the left, a dark disc where no portrait scaled, the portrait's place left alone, 「name 무력」 under the ring, nothing elsewhere;
        // SAN14_DUEL_PREVIEW_CHIPS=<file> writes the band y 100-300 as raw RGBA 1920 x 200
        namespace dd=duel_digits;
        const int W=1920,H=1080;
        check(dd::kChipCY-dd::kChipOuter>122.0f,"the chip rings stay under the 투지 bars");
        check(dd::kChipX[0][0]-dd::kChipOuter>386.0f && dd::kChipX[1][0]+dd::kChipOuter<1556.0f,"the chips stay clear of both main portraits");
        check(dd::kChipX[0][1]-dd::kChipX[0][0]>2.0f*dd::kChipOuter && dd::kChipX[1][0]-dd::kChipX[1][1]>2.0f*dd::kChipOuter,"the chips of one side do not touch");
        std::vector<unsigned> bg(static_cast<size_t>(W)*H);
        for(int py=0;py<H;++py)for(int px=0;px<W;++px)bg[static_cast<size_t>(py)*W+px]=0xff000000u|static_cast<unsigned>(110+py/40)|(static_cast<unsigned>(90+py/60)<<8)|(70u<<16);
        D3D11_TEXTURE2D_DESC fd{};fd.Width=W;fd.Height=H;fd.MipLevels=fd.ArraySize=1;fd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;fd.SampleDesc.Count=1;
        fd.Usage=D3D11_USAGE_DEFAULT;fd.BindFlags=D3D11_BIND_RENDER_TARGET;
        const D3D11_SUBRESOURCE_DATA init{bg.data(),static_cast<UINT>(W*4),0};
        ID3D11Texture2D *frame=nullptr,*frame_read=nullptr;
        check(SUCCEEDED(test_device->CreateTexture2D(&fd,&init,&frame)),"chip frame");
        D3D11_TEXTURE2D_DESC rd=fd;rd.Usage=D3D11_USAGE_STAGING;rd.BindFlags=0;rd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        check(SUCCEEDED(test_device->CreateTexture2D(&rd,nullptr,&frame_read)),"chip readback");
        dd::ChipSet cs;memset(&cs,0,sizeof cs);
        const float r=dd::kFaceR*dd::kChipScale;
        cs.chip[0].on=true;cs.chip[0].face=true;cs.chip[0].cx=dd::kChipX[0][0];cs.chip[0].cy=dd::kChipCY;cs.chip[0].r=r;cs.chip[0].hp=100;cs.chip[0].power=79;
        memcpy(cs.chip[0].name,L"유비",sizeof(L"유비"));
        cs.chip[1].on=true;cs.chip[1].face=false;cs.chip[1].cx=dd::kChipX[1][0];cs.chip[1].cy=dd::kChipCY;cs.chip[1].r=r;cs.chip[1].hp=50;cs.chip[1].power=64;
        memcpy(cs.chip[1].name,L"만총",sizeof(L"만총"));
        check(!dd::gate(),"no chips, no overlay work");
        dd::publish_chips(cs);
        check(dd::gate(),"published chips open the gate");
        dd::draw(test_device,test_context,frame,fd);
        check(!dd::broken() && dd::chip_ring[0].v && dd::chip_ring[1].v && dd::chip_name[0].v && dd::chip_power[1].v,"the rings and texts build on a real device");
        test_context->CopyResource(frame_read,frame);D3D11_MAPPED_SUBRESOURCE map={};
        check(SUCCEEDED(test_context->Map(frame_read,0,D3D11_MAP_READ,0,&map)),"read the chips' pixels");
        auto at=[&](float px,float py){const int x=static_cast<int>(px),y=static_cast<int>(py);return *reinterpret_cast<const unsigned *>(static_cast<const unsigned char *>(map.pData)+y*map.RowPitch+x*4);};
        auto blue=[&](float px,float py){const unsigned c=at(px,py),cr=c&255u,cg=(c>>8)&255u,cb=(c>>16)&255u;return cb>200 && cg>140 && cr<120;};
        auto track=[&](float px,float py){const unsigned c=at(px,py),cr=c&255u,cg=(c>>8)&255u,cb=(c>>16)&255u;return cr<70 && cg<70 && cb<80 && cb>=cr;};
        auto dark=[&](float px,float py){const unsigned c=at(px,py),cr=c&255u,cg=(c>>8)&255u,cb=(c>>16)&255u;return cr<80 && cg<70 && cb<70;};
        auto same_bg=[&](float px,float py){return at(px,py)==bg[static_cast<size_t>(static_cast<int>(py))*W+static_cast<int>(px)];};
        const float rm=r+dd::kRingIn+dd::kRingW*0.5f;
        const float ax=dd::kChipX[0][0],bx=dd::kChipX[1][0],cy=dd::kChipCY;
        const bool ring_full=blue(ax,cy-rm) && blue(ax+rm,cy) && blue(ax,cy+rm) && blue(ax-rm,cy);
        const bool halfway=blue(bx,cy-rm) && blue(bx+rm,cy) && track(bx-rm,cy) && track(bx-rm*0.7f,cy-rm*0.7f);
        const bool disc=dark(bx,cy) && same_bg(ax,cy) && same_bg(ax+r*0.5f,cy-r*0.5f);
        unsigned text=0,far_changed=0;
        const float top=cy-dd::kChipOuter-1.0f,bottom=cy+r+60.0f;
        for(int py=0;py<H;++py)for(int px=0;px<W;++px) {
            if(same_bg(static_cast<float>(px),static_cast<float>(py)))continue;
            const float fx=static_cast<float>(px),fy=static_cast<float>(py);
            const bool box=fy>=top && fy<=bottom && ((fx>=ax-70.0f && fx<=ax+70.0f) || (fx>=bx-70.0f && fx<=bx+70.0f));
            if(!box)++far_changed;
            else if(fy>cy+dd::kChipOuter+1.0f)++text;
        }
        char chip_out[MAX_PATH]={};
        if(GetEnvironmentVariableA("SAN14_DUEL_PREVIEW_CHIPS",chip_out,MAX_PATH)>0) {
            FILE *f=nullptr;
            if(fopen_s(&f,chip_out,"wb")==0 && f){for(int py=100;py<300;++py)fwrite(static_cast<const unsigned char *>(map.pData)+py*map.RowPitch,4,W,f);fclose(f);}
        }
        test_context->Unmap(frame_read,0);
        if(!ring_full || !halfway || !disc)printf("{\"duel_chip_px\":[%08x,%08x,%08x,%08x,%08x]}\n",at(ax,cy-rm),at(bx-rm,cy),at(bx+rm,cy),at(bx,cy),at(ax,cy));
        check(ring_full,"a full ring is blue all round");
        check(halfway,"half the HP: blue from the top to the bottom on the right, the track on the left");
        check(disc,"no portrait: a dark disc; a portrait's place stays the game's");
        check(text>150 && far_changed==0,"「name 무력」 under the rings, nothing drawn elsewhere (nothing over the 투지 bars)");
        memset(&cs,0,sizeof cs);dd::publish_chips(cs);
        check(!dd::gate(),"no chips on closes the gate");
        frame_read->Release();frame->Release();
        printf("{\"duel_chips\":{\"text_px\":%u,\"builds\":%u}}\n",text,dd::chip_builds);
    }
#endif
    staging->Release();view->Release();back->Release();test_context->Release();test_device->Release();chain->Release();DestroyWindow(target);
    printf("{\"status\":\"shared_overlay_fixture_passed\",\"checks\":%u,\"real_d3d11_pixel_readback\":true,\"gameplay_verified\":false}\n",checks);
}
