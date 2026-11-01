// 조조전 병종 빌보드 (2026-10-31). CPU가 클립 좌표·아틀라스 화소 좌표를 계산한다.
// t0 = 칸 아틀라스(R8_UINT 색 번호, 0 = 투명), t1 = 팔레트(256 x 64행, RGBA). 행 0 = 중립, 1+k = 세력색 k.
// 팔레트 0번 = 그 행의 테두리 색(세력색과 대비). 투명 화소 중 이웃에 그림이 있으면 테두리로 칠한다.
// team 0x100 = 선형 색 장면 타깃(float/sRGB)에 그릴 때 sRGB 팔레트를 선형으로 바꾼다. 0x200 = 회심 섬광. 0x400 = 상태 글자(t0 = 글자 마스크), 0x800 = 회심 글자.
Texture2D<uint> atlas : register(t0);
Texture2D<float4> palette : register(t1);
struct Vertex {float4 position:SV_POSITION;float2 texel:TEXCOORD0;nointerpolation uint team:TEXCOORD1;};
Vertex vs_main(float4 position:POSITION,float2 texel:TEXCOORD0,uint team:TEXCOORD1) {
    Vertex v;v.position=position;v.texel=texel;v.team=team;return v;
}
float4 ps_main(Vertex v):SV_TARGET {
    int2 at=int2(floor(v.texel));
    uint index=atlas.Load(int3(at,0));
    if(index==0) {
        uint near=atlas.Load(int3(at+int2(1,0),0))|atlas.Load(int3(at-int2(1,0),0))|
                  atlas.Load(int3(at+int2(0,1),0))|atlas.Load(int3(at-int2(0,1),0));
        if(near==0)discard;
    }
    float3 rgb;
    if(v.team&0x400)rgb=index==0?float3(0.03,0.03,0.03):((v.team&0x800)?float3(1.0,0.78,0.12):float3(0.45,0.85,1.0));   // 상태 글자: 막기 하늘색, 회심 금색, 검은 테두리
    else rgb=palette.Load(int3(int(index),int(v.team&0xFF),0)).rgb;
    if(v.team&0x200)rgb=lerp(rgb,float3(1,1,1),0.65);
    if(v.team&0x100)rgb=pow(rgb,2.2);
    return float4(rgb,1);
}
