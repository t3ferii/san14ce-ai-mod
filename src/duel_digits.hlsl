// Relay duel round number (local298, [diagnostics] duel_manual_test): one premultiplied RGBA glyph per draw, the viewport is the
// glyph's screen rectangle (the advisor overlay's full-viewport triangle). Headers duel_digits_vs.h / duel_digits_ps.h:
//   fxc /nologo /O3 /T vs_4_0 /E vs_main /Vn duel_digits_vs /Fh src\duel_digits_vs.h src\duel_digits.hlsl
//   fxc /nologo /O3 /T ps_4_0 /E ps_main /Vn duel_digits_ps /Fh src\duel_digits_ps.h src\duel_digits.hlsl
Texture2D glyph : register(t0);
SamplerState linear_sampler : register(s0);
struct Vertex {float4 position:SV_POSITION;float2 texel:TEXCOORD0;};
Vertex vs_main(uint id:SV_VertexID) {
    Vertex v;
    float2 t=float2((id<<1)&2,id&2);
    v.position=float4(t*float2(2,-2)+float2(-1,1),0,1);
    v.texel=t;
    return v;
}
float4 ps_main(Vertex v):SV_TARGET {return glyph.Sample(linear_sampler,v.texel);}
