#pragma once
// 개성 9칸 범위 B (traits9_screens, SAN14_Convenience.dll, 2026-11-18 local236): 원작이 아직 5칸으로 그리는 화면에 9칸을 붙인다.
// 출처: 통합패치(san14ce-integrated-patch) SAN14_Traits9_UI.dll v2.6의 모든 화면 9칸(설치 3C10), 편집기 3x3(1930), 유닛 편성 탭
// 라벨 배치(39C0). 그 DLL은 실행하지 않고 읽기 전용으로 조사해(research/traits9_v2/ui_dll_inventory.md, 디컴파일
// research/ghidra_refs/SAN14_Traits9_UI_49fb15b4e757d7a4, 현재 빌드 주소는 같은 DLL 새 판 7A82493F의 표) 같은 동작을 소스로 다시 썼다.
//  - 화면 갱신 함수가 끝난 뒤, 개성이 6개 이상이거나 6번째 이후 칸이 쓰이면 원작 라벨 5개의 높이·행 위치를 재서 같은 부모에
//    원작 표식(CCmnMrker) 9개를 새로 만들어 채우고 원작 5개를 숨긴다. 아니면 원작 그대로.
//  - 무장 상세 기본·능력 탭과 무장일람은 범위 A(native_traits9.inl, 레이아웃 노드·표 확장)가 이미 9칸이라 여기서 다루지 않는다.
// 이 헤더는 게임 함수를 부르지 않는 계산과 자리 표만 둔다. 계산은 tests/address_guard_fixture.cpp의 traits9_screens 줄이 검사한다.
#include <stdint.h>
namespace t9s {
struct Rect {int x,y,w,h;};

// 6칸 이상이거나, 5칸 이하라도 마지막 개성이 6번째 칸(번호 5) 이후에 있으면 9칸으로 그린다(26B0, 1930/16A0 공통).
inline bool wants_grid(unsigned n,const int *slots) {return n>5 || (n!=0 && slots[n-1]>4);}

// 모든 화면: 원작 라벨 5개를 재서 9칸 배치(2280). 5개 모두 폭 20~2400, 높이 10~500, 높이 같음(부모 일치는 호출자가 본다).
// 서로 다른 y = 행(오름차순), 같은 행에서 오른쪽 이웃까지의 가장 좁은 양의 틈 = 원작 칸 사이. 행 사이가 높이보다 좁으면 실패.
// 행 R개면 열 C=(R+8)/R(1행 9열, 2행 5열, 3·4행 3열, 5행 2열). 새 틈 = 원작 틈의 절반(올림, 틈이 없으면 높이/10, 최소 2),
// 새 폭 = (가장 오른쪽 끝 - 가장 왼쪽 시작 - 틈 합) / C, 30 미만이면 실패.
inline bool measure_grid(const Rect in[5],Rect out[9]) {
    const int h=in[0].h;
    int min_x=in[0].x,max_r=in[0].x+in[0].w,rows[5],row_count=0,gap=2401;
    for(int i=0;i<5;++i) {
        const Rect &r=in[i];
        if(r.w<20 || r.w>2400 || r.h<10 || r.h>500 || r.h!=h)return false;
        if(r.x<min_x)min_x=r.x;
        if(r.x+r.w>max_r)max_r=r.x+r.w;
        int k=0;while(k<row_count && rows[k]<r.y)++k;              // 행 목록: 오름차순, 같은 y는 한 번
        if(k==row_count || rows[k]!=r.y) {
            for(int m=row_count;m>k;--m)rows[m]=rows[m-1];
            rows[k]=r.y;++row_count;
        }
        for(int j=0;j<5;++j) {                                      // 같은 행 안의 양의 틈 중 가장 좁은 것
            if(in[j].y!=r.y)continue;
            const int g=in[j].x-(r.x+r.w);
            if(g>=1 && g<gap)gap=g;
        }
    }
    for(int k=0;k+1<row_count;++k)if(rows[k+1]-rows[k]<h)return false;
    const int cols=(row_count+8)/row_count;
    int half=gap<2401?(gap+1)/2:h/10;
    if(half<2)half=2;
    const int width=((max_r-min_x)-(cols-1)*half)/cols;
    if(width<30)return false;
    for(int i=0;i<9;++i)out[i]={(i%cols)*(half+width)+min_x,rows[i/cols],width,h};
    return true;
}

// 편집기(장수 편집 탭): 원작 2열×3행(5칸) → 3열×3행(1420). 기준 라벨0, 폭 30~1000, 높이 10~300, 열 간격 dx(폭 < dx ≤ 2×폭),
// 행 간격 dy(높이 ≤ dy ≤ 3×높이), 5칸 모두 같은 폭·높이이고 x = 홀수 칸이면 라벨1.x 아니면 라벨0.x, y = (칸/2)×dy + 라벨0.y.
// 원작 틈 g = dx - 폭: g<3이면 새 틈 2(전체에서 4 뺌), 아니면 (g+1)/2. 세 칸 합 폭 = 폭 + dx - 2×새 틈 ≥ 90, 새 폭 = 합/3.
inline bool editor_grid(const Rect in[5],Rect out[9]) {
    const int x0=in[0].x,y0=in[0].y,w=in[0].w,h=in[0].h;
    if(w<30 || w>1000 || h<10 || h>300)return false;
    const int dx=in[1].x-x0,dy=in[2].y-y0;
    if(!(w<dx && dx<=w*2 && h<=dy && dy<=h*3))return false;
    for(int i=0;i<5;++i) {
        if(in[i].w!=w || in[i].h!=h)return false;
        if(in[i].x!=((i&1)?in[1].x:x0) || in[i].y!=(i>>1)*dy+y0)return false;
    }
    const int g=dx-w;
    int cut,half;
    if(g<3) {cut=4;half=2;}
    else {cut=(g+1)&~1;half=(g+1)>>1;}
    const int total=w+dx-cut;
    if(total<90)return false;
    const int width=total/3;
    for(int i=0;i<9;++i)out[i]={(i%3)*(half+width)+x0,(i/3)*dy+y0,width,h};
    return true;
}

// 유닛 편성 탭(CStragegyUnitMakePTab +0x178, CStrategyUnitMakeAUTab +0x220): 원작은 개성 표식 9개를 만들되 y = (칸%5)×행 간격으로
// 놓아 6~9번째가 1~4번째 위에 겹친다. 만들 때(가상 +0x138 = 8534E0 직전) 2열×5행으로 옮긴다(39C0). u = 배열 안 위치,
// first/second = 이미 만든 배열[0]/배열[2](없으면 null). 폭 62~2396·높이 10~500 밖이거나 칸이 안 맞으면 원래 인자 그대로(false).
inline bool unit_tab(unsigned u,const Rect *first,const Rect *second,int &x,int &y,int &w,int h) {
    if(static_cast<unsigned>(w-62)>=0x923u || static_cast<unsigned>(h-10)>=0x1EBu)return false;
    int x0=x,y0=y,pitch=h,gap=2,width=w;
    bool from_first=u!=0 && first,fits=true;
    if(from_first) {
        x0=first->x;y0=first->y;
        const int col=static_cast<int>(u%5);
        if(col==0) {
            if(second)pitch=second->y-y0;                            // 새 줄 첫 칸: 이미 옮긴 배열[2]가 한 행 아래
            else from_first=false;                                   // 배열[2]가 없으면 행 간격 = 높이(x0/y0는 배열[0])
        } else pitch=(y-y0)/col;                                     // 원작 y에서 행 간격
        if(from_first)fits=h<=pitch;
        else pitch=h;
    }
    if(!from_first && h<=0x13) {width=w-2;gap=2;}                   // 낮은 칸: 검사 없이 2 줄여 반으로
    else {
        gap=h>0x13?h/10:2;
        width=w-gap;
        if(width<60 || !fits || h*3<pitch)return false;
    }
    w=width>>1;
    x=((u&1)?gap+w:0)+x0;
    y=static_cast<int>(u>>1)*pitch+y0;
    return true;
}
}   // namespace t9s
