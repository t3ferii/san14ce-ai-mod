// 4계절 패치 G1a bake pass. The game's ground shaders (near d3e1f71e, far 9addb6cb) write G-buffer albedo
//   o0 = t10 * base,  base = near detail blend (far: t4, the far albedo 4096^2 BC3)
// and the Season Map adds o0' = A + o0 * M. Baked into the two map textures:
//   mode 0  t10' = t10 * M + A / t4                (near and far; clipped at 1)
//   mode 1  t4'  = (t4*t10*M + A) / t10'           (far: t4' * t10' = the Season Map sum; = t4 where t10' did not clip)
// Near the camera the detail replaces t4, which averages to about t4, so A shows there too except where t10' clipped.
#include "season_terrain.hlsl"
cbuffer Bake : register(b0) {
    float4 bk0;   // x,y: size of the mip being baked; z,w: tile origin in texels
    float4 bk1;   // x: mip level of the originals; y: mode (0 t10, 1 t4)
};
Texture2D<float4> OrigMul : register(t20);   // original t10 (multiplier, sRGB view)
Texture2D<float4> OrigFar : register(t21);   // original t4 (far albedo, sRGB view)
float4 bake_map(float4 pos : SV_Position) : SV_Target
{
    float2 uv = (pos.xy + bk0.zw) / bk0.xy;
    float4 mul = OrigMul.SampleLevel(sClamp, uv, bk1.x);
    float4 farc = OrigFar.SampleLevel(sClamp, uv, bk1.x);
    float4 orig = bk1.y > 0.5 ? farc : mul;
    sanInit();
    float h = LandHeight.SampleLevel(sClamp, uv, 0);
    int2 hp = clamp(int2(saturate(uv) * 512.0), 0, 511);
    [branch] if (h < 740.0 && TexHazard.Load(int3(hp, 0)).g < 0.1) return bk1.y > 2.5 ? 0.0 : orig;   // sea floor: game colour
    float3 A, M;
    season(uv, float3(0.0, 1.0, 0.0), uv * MAPW, h, h, A, M);
    // D (patched ground shaders, o0 = o0 * (1 - D) + A): mode 3 bakes D = 1 - M, mode 4 bakes A. 0 = the original.
    [branch] if (bk1.y > 3.5) return float4(saturate(A), 1.0);
    [branch] if (bk1.y > 2.5) return float4(saturate(1.0 - M), 1.0);
    // G1c: the added part A goes into t10 as far as it fits, divided by the far albedo (the near detail averages to
    // about t4), so the near camera also sees it; only what does not fit (bright snow on dark ground) goes into t4.
    float3 target = farc.rgb * mul.rgb * M + A;                       // the Season Map picture at distance
    float3 mulP = saturate(mul.rgb * M + A / max(farc.rgb, 0.03));
    [branch] if (bk1.y < 0.5) return float4(mulP, mul.a);
    [branch] if (bk1.y < 1.5) return float4(saturate(target / max(mulP, 0.02)), farc.a);
    return float4(saturate(target), 1.0);                                // mode 2: fixture reference only
}
float4 bake_vs(uint id : SV_VertexID) : SV_Position
{
    float2 p = float2((id << 1) & 2, id & 2);
    return float4(p * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
