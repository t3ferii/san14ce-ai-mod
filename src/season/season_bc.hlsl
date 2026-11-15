// 4계절 패치 G1a: GPU block compression of one baked tile (range fit, one thread per 4x4 block).
// Src is the tile through a UNORM view, so the stored (sRGB-encoded) bytes are compressed as they are.
Texture2D<float4> Src : register(t0);
RWTexture2D<uint2> OutBC1 : register(u0);
RWTexture2D<uint4> OutBC3 : register(u0);
cbuffer C : register(b0) { uint4 cc; };   // x, y: blocks across, down
uint pack565(float3 c) { uint3 q = uint3(round(saturate(c) * float3(31.0, 63.0, 31.0))); return (q.x << 11) | (q.y << 5) | q.z; }
float3 unpack565(uint v) { return float3((v >> 11) & 31u, (v >> 5) & 63u, v & 31u) / float3(31.0, 63.0, 31.0); }
void load_block(uint2 b, out float4 px[16])
{
    [unroll] for (uint i = 0; i < 16; ++i) px[i] = Src.Load(int3(b * 4u + uint2(i & 3u, i >> 2), 0));
}
uint2 color_block(float4 px[16])
{
    float3 lo = 1.0, hi = 0.0;
    [unroll] for (uint i = 0; i < 16; ++i) { lo = min(lo, px[i].rgb); hi = max(hi, px[i].rgb); }
    float3 inset = (hi - lo) / 16.0; lo = saturate(lo + inset); hi = saturate(hi - inset);
    uint c0 = pack565(hi), c1 = pack565(lo);
    if (c0 < c1) { uint t = c0; c0 = c1; c1 = t; }
    if (c0 == c1) return uint2(c0 | (c1 << 16), 0u);
    float3 e0 = unpack565(c0), e1 = unpack565(c1), d = e0 - e1;
    float dd = max(dot(d, d), 1e-8);
    uint idx = 0;
    [unroll] for (uint j = 0; j < 16; ++j) {
        uint q = (uint)round(saturate(dot(px[j].rgb - e1, d) / dd) * 3.0);   // 3 = c0 ... 0 = c1
        uint code = q == 3u ? 0u : q == 2u ? 2u : q == 1u ? 3u : 1u;
        idx |= code << (2u * j);
    }
    return uint2(c0 | (c1 << 16), idx);
}
[numthreads(8, 8, 1)]
void bc1_main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= cc.x || id.y >= cc.y) return;
    float4 px[16]; load_block(id.xy, px);
    OutBC1[id.xy] = color_block(px);
}
[numthreads(8, 8, 1)]
void bc3_main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= cc.x || id.y >= cc.y) return;
    float4 px[16]; load_block(id.xy, px);
    float lo = 1.0, hi = 0.0;
    [unroll] for (uint i = 0; i < 16; ++i) { lo = min(lo, px[i].a); hi = max(hi, px[i].a); }
    uint a0 = (uint)round(hi * 255.0), a1 = (uint)round(lo * 255.0);
    uint lowBits = 0, highBits = 0;   // 48 bits of 3-bit indices
    if (a0 > a1) {
        [unroll] for (uint j = 0; j < 16; ++j) {
            uint q = (uint)round(saturate((px[j].a * 255.0 - a1) / (float)(a0 - a1)) * 7.0);   // 7 = a0 ... 0 = a1
            uint code = q == 7u ? 0u : q == 0u ? 1u : 8u - q;
            uint bit = 3u * j;
            if (bit < 32u) { lowBits |= code << bit; if (bit > 29u) highBits |= code >> (32u - bit); }
            else highBits |= code << (bit - 32u);
        }
    }
    uint2 c = color_block(px);
    OutBC3[id.xy] = uint4(a0 | (a1 << 8) | ((lowBits & 0xFFFFu) << 16), (lowBits >> 16) | ((highBits & 0xFFFFu) << 16), c.x, c.y);
}
