// 4계절 패치 (wprkfdid12-ux): season picture code from the Season Map v26k52 (SeasonTerrainNear.hlsl lines 1-1261,
// author's code under its no-conditions licence, 사용허락.txt). Unchanged except that main() is left out: the
// bake pass (season_bake.hlsl) calls season() once per map texel instead of once per screen pixel.
// SAN14 Season overlay v26 - shared code (the living climate: rain, snow, anomalies, per-cell disasters)
// v26k30: the game's own hex grid; rain, lying snow and river ice exactly on the rules' cells
// Output is combined by dual-source blending:  final = A + dst * M   (RGB only)
Texture1D<float4> IniParams : register(t120);
Texture2D<float4> LandControl : register(t3);
Texture2D<float>  LandHeight  : register(t110);
Texture2D<float4> TexSnow     : register(t111);
Texture2D<float4> TexVeg      : register(t112);
Texture2D<float4> TexFreeze   : register(t113);   // r: rivers that freeze (water shader), g: river + banks (land)
                                                  // b: impassable mountain cells, a: passable hill cells (v13)
Texture2D<float4> TexRegion   : register(t114);   // v14: bare-rock colour by real geology (rgb), layering strength (a)
Texture2D<float4> TexRock     : register(t115);   // v15: 4 rock surface photos, brightness only (r granite, g layered cliff, b limestone cliff, a red soil)
Texture2D<float4> TexLayer    : register(t116);   // v15: which of the 4 photos each region uses
Texture2DArray<float4> TexGround : register(t117); // v17: plain ground photos: 0 loess 1 dry 2 dark soil 3 red soil 4 lush grass 5 sparse grass
Texture2D<float4> TexSoil     : register(t118);   // v17: plain soil by region (r loess, g dry, b dark, a red)
Texture2D<float4> TexForest   : register(t119);   // v18: forest tiles (r) - keep the game's forest floor there
Texture2DArray<float4> TexClim : register(t109);  // v23: regional climate (real city temperatures), slice = month-1:
                                                  //      r sea-level temperature, g warming trend, b dryness, a coldest month
Texture2D<float4> TexZone     : register(t108);   // v24: r north of the Yellow River (snow weight), g northwest dry-cold (Liangzhou),
                                                  //      b castle surroundings, a Hexi gravel desert
Texture2D<float4> TexFlood    : register(t106);   // v25.7: flood water photo (rgb, sRGB) + ripple pattern (a)
Texture2D<float4> TexHazard   : register(t107);   // v24: r flood order from the big rivers (0 = river edge .. 1 = never), g river id
Texture2D<float4> TexRock2    : register(t105);   // v26: 4 more rock photos (r light granite, g red sandstone, b mossy rock, a sheer face)
Texture2D<float4> TexRock3    : register(t104);   // v26: 4 more (r layered cliff, g loess wall, b snow-dusted rock, a grey limestone)
Texture2D<float4> TexRelief   : register(t102);   // v26k19: rock relief normals (rg side-on rock face, ba broken rock from above)
Texture2D<float4> TexTags     : register(t103);   // v26: the named mountain sites (r id/255, g blend weight, b fine-relief level/10)
// v24: TexGround slices 9 patchy melting snow, 10 burnt ground, 11 loess (NW), 12 gravel desert, 13 sand + gravel
// v23: TexGround slices 6 dry straw grass, 7 green-brown grass (waking / dry season), 8 leaf litter
SamplerState sClamp : register(s3);
SamplerState sWrap  : register(s7);

static const float PI2 = 6.2831853;
static const float MAPW = 204800.0;

// v26k48: a map mod that makes the map bigger (맵 확장 137: 4097 samples, the old land moved down by 1170 samples = +117000 in z)
//         moves the game's hex grid by the same amount (list 3420 = the grid the rules module reads from the game). The season
//         picture is worked out in the old map's place (SAN_SH taken off the world point, the season maps are the old map's);
//         the game's own maps (land height, control map) are read at the game's size (SAN_MW). Land outside the old map keeps the
//         game's own picture. With no such mod the shift is 0 and the map 204800 wide: SAN_ON stays false, every path as before.
static float2 SAN_SH = float2(0.0, 0.0); static float SAN_MW = 204800.0; static bool SAN_ON = false; static bool SAN_SKIP = false; static int SAN_LC = 2048;
void sanInit()
{
    float4 g = IniParams.Load(int2(3420, 0));
    bool gok = g.z > 500.0 && g.z < 900.0 && g.w > 300.0 && g.w < 600.0;
    SAN_SH = (g.z > 500.0 && g.z < 900.0 && g.w > 300.0 && g.w < 600.0) ? float2(g.x - 37594.0, g.y - 13335.2998) : float2(0.0, 0.0);
    if (abs(SAN_SH.x) < 1.0 && abs(SAN_SH.y) < 1.0) SAN_SH = float2(0.0, 0.0);
    uint lw, lh; LandHeight.GetDimensions(lw, lh);
    SAN_MW = lw < 3000u ? 204800.0 : (float)(lw - 1u) * 100.0;
    SAN_ON = SAN_SH.x != 0.0 || SAN_SH.y != 0.0 || SAN_MW != MAPW;
    SAN_SKIP = !gok && lw >= 3000u;                             // a bigger map but the grid not known yet: draw nothing rather than misplace it
    uint cw0, ch0; LandControl.GetDimensions(cw0, ch0);
    SAN_LC = SAN_ON ? (int)cw0 - 1 : 2048;
}
float landS(float2 u) { return LandHeight.SampleLevel(sClamp, SAN_ON ? (u * MAPW + SAN_SH) / SAN_MW : u, 0); }   // old-map uv -> the game's height map

// material id (control map R/G) -> detail slice (from the game's material table)
static const int MAT2SLICE[18] = {12,16,15,10,9,11,8,17,13,14,18,0,4,3,2,5,6,7};   // local141: ids 4 / 10 as the current game table (season_mat_layers)
// per detail slice: how much of it is vegetation, and whether it is farmland
static const float VEG[19]  = {0.9,0.0,0.05,0.35,0.7,0.55,0.7,0.9,0.5,1.0,0.8,0.85,1.0,0.7,0.8,0.0,0.3,0.6,0.55};
static const float FARM[19] = {0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1};

float hash21(float2 p) { p = frac(p * float2(123.34, 456.21)); p += dot(p, p + 45.32); return frac(p.x * p.y); }
float vnoise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i), b = hash21(i + float2(1, 0)), c = hash21(i + float2(0, 1)), d = hash21(i + float2(1, 1));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}
float fbm(float2 p) { return vnoise(p) * 0.55 + vnoise(p * 2.13 + 7.1) * 0.3 + vnoise(p * 4.37 + 3.3) * 0.15; }
float fbm2(float2 p) { return vnoise(p) * 0.62 + vnoise(p * 2.13 + 7.1) * 0.38; }
float billow(float2 p) { return 1.0 - abs(2.0 * fbm2(p) - 1.0); }   // v26k: the rain cloud's lumps (as in SeasonRain.hlsl)
float3 lin(float3 c) { return pow(saturate(c), 2.2); }
float bell(float x, float c, float w) { float d = (x - c) / w; return exp(-d * d); }

void matinfo(float2 uv, out float veg, out float farm, out float bare)
{
    float2 t = SAN_ON ? (uv * MAPW + SAN_SH) * 0.01 : uv * 2048.0;   // v26k48: the game's control map (100 world units a texel)
    float2 f = frac(t);
    int2 b = int2(floor(t));
    veg = 0; farm = 0; bare = 0;
    [unroll] for (int k = 0; k < 4; k++) {
        int2 o = int2(k & 1, k >> 1);
        float wgt = (o.x ? f.x : 1 - f.x) * (o.y ? f.y : 1 - f.y);
        float4 c = LandControl.Load(int3(clamp(b + o, 0, SAN_LC), 0));
        int ia = min((int)round(c.r * 255.0), 17), ib = min((int)round(c.g * 255.0), 17);
        int sa = MAT2SLICE[ia], sb = MAT2SLICE[ib];
        float bw = saturate(c.b);
        veg  += wgt * lerp(VEG[sa], VEG[sb], bw);
        farm += wgt * lerp(FARM[sa], FARM[sb], bw);
        bare += wgt * lerp(VEG[sa] < 0.1 ? 1.0 : 0.0, VEG[sb] < 0.1 ? 1.0 : 0.0, bw);   // roads, sand: keep the game's look
    }
}


// v23: bilinear read of the small climate / castle maps without relying on the game's samplers
float4 climAt(float2 uv, int sl)
{
    float2 p = saturate(uv) * 128.0 - 0.5;
    int2 b = int2(floor(p)); float2 f = p - b;
    int2 b0 = clamp(b, 0, 127), b1 = clamp(b + 1, 0, 127);
    float4 a = TexClim.Load(int4(b0.x, b0.y, sl, 0)), c = TexClim.Load(int4(b1.x, b0.y, sl, 0));
    float4 d = TexClim.Load(int4(b0.x, b1.y, sl, 0)), e = TexClim.Load(int4(b1.x, b1.y, sl, 0));
    return lerp(lerp(a, c, f.x), lerp(d, e, f.x), f.y);
}
float4 zoneAt(float2 uv)
{
    float2 p = saturate(uv) * 512.0 - 0.5;
    int2 b = int2(floor(p)); float2 f = p - b;
    int2 b0 = clamp(b, 0, 511), b1 = clamp(b + 1, 0, 511);
    float4 a = TexZone.Load(int3(b0.x, b0.y, 0)), c = TexZone.Load(int3(b1.x, b0.y, 0));
    float4 d = TexZone.Load(int3(b0.x, b1.y, 0)), e = TexZone.Load(int3(b1.x, b1.y, 0));
    return lerp(lerp(a, c, f.x), lerp(d, e, f.x), f.y);
}
float4 hazAt(float2 uv)
{
    float2 p = saturate(uv) * 512.0 - 0.5;
    int2 b = int2(floor(p)); float2 f = p - b;
    int2 b0 = clamp(b, 0, 511), b1 = clamp(b + 1, 0, 511);
    float4 a = TexHazard.Load(int3(b0.x, b0.y, 0)), c = TexHazard.Load(int3(b1.x, b0.y, 0));
    float4 d = TexHazard.Load(int3(b0.x, b1.y, 0)), e = TexHazard.Load(int3(b1.x, b1.y, 0));
    return lerp(lerp(a, c, f.x), lerp(d, e, f.x), f.y);
}

float2 tdirW(float3 n) { return normalize(float2(-n.z, n.x) + float2(0.0001, 0.0)); }   // along the slope contour
float mtn0(float alt) { return smoothstep(150.0, 700.0, alt); }
// spring flower palette: pink, white, yellow, purple, red-magenta, light blue, orange
// (mountains lean to azalea pinks/magentas, plains to the full mix)
float3 flowercol(float h, float mtn)
{
    float3 c;
    float k = h * 7.0;
    if (k < 1.0)      c = float3(0.72, 0.26, 0.40);   // peach pink
    else if (k < 2.0) c = float3(0.68, 0.67, 0.64);   // pear / plum white
    else if (k < 3.0) c = float3(0.70, 0.53, 0.03);   // forsythia / rapeseed yellow
    else if (k < 4.0) c = float3(0.34, 0.13, 0.52);   // violet purple
    else if (k < 5.0) c = float3(0.64, 0.08, 0.30);   // azalea magenta
    else if (k < 6.0) c = float3(0.30, 0.46, 0.72);   // light blue
    else              c = float3(0.72, 0.30, 0.05);   // orange
    float3 az = (frac(h * 3.7) < 0.5) ? float3(0.70, 0.20, 0.40) : float3(0.64, 0.08, 0.30);
    return lerp(c, az, mtn * 0.6);
}

// ---- v26 helpers: the rules module's weather data in the number list, and the disaster drawings as functions ----
// v26k30: the game's own hex grid (list 3420: x = first column's x, y = z origin, z = column step, w = hex radius - read from the
//         game by the rules module; the game's own numbers until then). Centre of cell (col,row): x = X0 + col*CW, z = Z0 + row*H +
//         (even column ? H/2 : 0), H = sqrt(3)*R; written below as C0 + (col*CW, (row - 0.5*odd)*CH) with C0 = (X0, Z0 + H/2).
//         (The grid used before - 700 x 808.29 from (37550, 13850) - drifted up to 0.7 of a cell off the game's toward the east and south.)
static float CW_ = 702.0, CH_ = 810.6002;
static float2 C0_ = float2(37594.0, 13740.6);
void gridInit()
{
    float4 g = IniParams.Load(int2(3420, 0));
    if (!(g.z > 500.0 && g.z < 900.0 && g.w > 300.0 && g.w < 600.0)) g = float4(37594.0, 13335.2998, 702.0, 468.0);
    g.xy -= SAN_SH;                                   // v26k48: in the old map's place (0 without a map mod)
    CW_ = g.z; CH_ = g.w * 1.7320508; C0_ = float2(g.x, g.y + CH_ * 0.5);
}
float hexIdxAt(float2 wp)                            // the cell under a world point (the game's nearest-centre rule), -1 off the map
{
    float c0 = floor((wp.x - C0_.x) / CW_); float bestD = 1e12, best = -1.0;
    [unroll] for (int kq = 0; kq < 2; kq++) {
        float col1 = c0 + kq; float codd = col1 - floor(col1 * 0.5) * 2.0;
        float row1 = round((wp.y - C0_.y) / CH_ + 0.5 * codd);
        float2 cand = C0_ + float2(col1 * CW_, (row1 - 0.5 * codd) * CH_);
        float dd = dot(wp - cand, wp - cand);
        if (dd < bestD) { bestD = dd; best = (col1 >= 0.0 && col1 < 220.0 && row1 >= 0.0 && row1 < 220.0) ? row1 * 220.0 + col1 : -1.0; }
    }
    return best;
}
// v26k30: the rules' own weather of a cell (list 3424 on, one byte a cell, three cells a float): bits 0-1 rain tier (1 drizzle / sleet,
//         2 rain / snow, 3 downpour / snowstorm), 2 snowing, 3-4 lying snow (1 thin, 2 deep - the value the speed and the tile info use),
//         5 river ice, 6 shaken ground, 7 mud. The map draws rain, snow and ice exactly on the cells that have them.
float cellWx(float idx)
{
    if (idx < 0.0) return 0.0;
    float q = floor((idx + 0.5) / 3.0); float k = idx - q * 3.0;
    float4 f4 = IniParams.Load(int2(3424 + (int)floor((q + 0.5) / 4.0), 0)); float f = f4[(int)(q - floor((q + 0.5) / 4.0) * 4.0)];
    float d = floor(f * exp2(-8.0 * k) + 1e-3); return d - floor(d / 256.0 + 1e-4) * 256.0;
}
float wxBits(float b, float lo, float n) { float d = floor(b * exp2(-lo) + 1e-3); float m = exp2(n); return d - floor(d / m + 1e-4) * m; }
// v26k38b2: the cloud's footprint (SeasonRain.hlsl rainCover, the same five look-ups): the rules' rain cells, soft a third of a cell past
float rainCover(float2 q)
{
    float2 o1 = float2(CW_, CH_) * 0.35, o2 = float2(CW_, -CH_) * 0.35;
    float c = step(0.5, wxBits(cellWx(hexIdxAt(q)), 0.0, 2.0)) * 2.0;
    c += step(0.5, wxBits(cellWx(hexIdxAt(q + o1)), 0.0, 2.0)) + step(0.5, wxBits(cellWx(hexIdxAt(q - o1)), 0.0, 2.0));
    c += step(0.5, wxBits(cellWx(hexIdxAt(q + o2)), 0.0, 2.0)) + step(0.5, wxBits(cellWx(hexIdxAt(q - o2)), 0.0, 2.0));
    return c / 6.0;
}
float cellState(float cellIdx)                       // 4 bits per cell, 5 cells a float, from list entry 1000 on
{
    float q = floor((cellIdx + 0.5) / 5.0); float k = cellIdx - q * 5.0;
    float4 f4 = IniParams.Load(int2(1000 + (int)floor((q + 0.5) / 4.0), 0)); float f = f4[(int)(q - floor((q + 0.5) / 4.0) * 4.0)];
    float d = floor(f * exp2(-4.0 * k) + 1e-3);
    return d - floor(d / 16.0 + 1e-4) * 16.0;
}
float4 rainCellRaw(float i, float j)                 // one 55x55 grid value -> (tier, wet, snowing, cracked)
{
    float idx = clamp(j, 0.0, 54.0) * 55.0 + clamp(i, 0.0, 54.0);
    float4 v4 = IniParams.Load(int2(150 + (int)floor((idx + 0.5) / 4.0), 0)); float v = v4[(int)(idx - floor((idx + 0.5) / 4.0) * 4.0)];
    float cr = floor(v / 100000.0 + 1e-4); v -= cr * 100000.0;
    float sn = floor(v / 10000.0 + 1e-4); v -= sn * 10000.0;
    float tier = floor(v / 1000.0 + 1e-4); v -= tier * 1000.0;
    return float4(tier, saturate(v / 100.0), sn, cr);
}
float4 rainAt(float2 wp)                             // bilinear over the 4x4-cell grid
{
    float gx = (wp.x - C0_.x) / (CW_ * 4.0) - 0.5, gy = (wp.y - C0_.y) / (CH_ * 4.0) - 0.5;
    float x0 = floor(gx), y0 = floor(gy); float fx = gx - x0, fy = gy - y0;
    float4 a = rainCellRaw(x0, y0), b = rainCellRaw(x0 + 1.0, y0), c = rainCellRaw(x0, y0 + 1.0), d = rainCellRaw(x0 + 1.0, y0 + 1.0);
    return lerp(lerp(a, b, fx), lerp(c, d, fx), fy);
}
float nodeAt(int base, float2 uv)                    // the 9x9 weather nodes (list entries base..base+20)
{
    float fx = saturate(uv.x) * 8.0, fy = saturate(uv.y) * 8.0; float x0 = min(floor(fx), 7.0), y0 = min(floor(fy), 7.0); float ax = fx - x0, ay = fy - y0;
    float k00 = y0 * 9.0 + x0, k10 = k00 + 1.0, k01 = k00 + 9.0, k11 = k01 + 1.0;
    float4 q00 = IniParams.Load(int2(base + (int)floor((k00 + 0.5) / 4.0), 0)); float v00 = q00[(int)(k00 - floor((k00 + 0.5) / 4.0) * 4.0)];
    float4 q10 = IniParams.Load(int2(base + (int)floor((k10 + 0.5) / 4.0), 0)); float v10 = q10[(int)(k10 - floor((k10 + 0.5) / 4.0) * 4.0)];
    float4 q01 = IniParams.Load(int2(base + (int)floor((k01 + 0.5) / 4.0), 0)); float v01 = q01[(int)(k01 - floor((k01 + 0.5) / 4.0) * 4.0)];
    float4 q11 = IniParams.Load(int2(base + (int)floor((k11 + 0.5) / 4.0), 0)); float v11 = q11[(int)(k11 - floor((k11 + 0.5) / 4.0) * 4.0)];
    return lerp(lerp(v00, v10, ax), lerp(v01, v11, ax), ay);
}
float hazRiverId(float2 uv)                          // the river id of the nearest hazard texel (never blended between rivers)
{
    int2 p = clamp(int2(saturate(uv) * 512.0), 0, 511);
    float g = TexHazard.Load(int3(p, 0)).g;
    return g < 0.15 ? 0.0 : (g < 0.5 ? 1.0 : (g < 0.8 ? 3.0 : 2.0));
}
// ---- forest fire in one cell: charred ground, flames, embers, smoke; burntOut = ash only, fade = how much is left to see ----
void drawFire(inout float3 A, inout float3 M, float2 wp, float2 rel, float hexn, float incell, float vegF, float foot, float tm, float burntOut, float fade, float smokeOn)
{
    float a;
    float grow = burntOut < 0.5 ? 1.0 : 1.0;
    float burnt = incell * vegF * fade * grow;
    [branch] if (burnt > 0.002) {
        float3 ash = lin(TexGround.SampleLevel(sWrap, float3(wp / 1400.0, 10), clamp(log2(max(foot, 1.0) / 1.4), 0.0, 9.0)).rgb);
        ash = lerp(ash * 0.6, float3(0.08, 0.08, 0.08), smoothstep(0.55, 0.9, vnoise(wp / 260.0 + 2.0)) * 0.6);
        a = burnt * 0.94;
        M *= 1 - a; A = A * (1 - a) + ash * a;
        if (burntOut < 0.5) {
            float gl = smoothstep(0.86, 0.97, vnoise(wp / 90.0 + 11.0)) * burnt * (0.6 + 0.4 * sin(tm * 6.0 + wp.x * 0.01));
            M *= 1 - gl; A = A * (1 - gl) + float3(0.9, 0.25, 0.03) * gl;
            float2 fw = wp / 70.0 + float2(0.0, -tm * 2.6);
            float f1 = vnoise(fw), f2 = vnoise(wp / 24.0 + float2(tm * 3.5, -tm * 8.0) + 3.0), f3 = vnoise(wp / 150.0 + float2(0.0, -tm * 1.2));
            float fl = f1 * 0.45 + f2 * 0.35 + f3 * 0.2 + incell * 0.22;
            float flame = smoothstep(0.52, 0.80, fl) * incell * vegF;
            float core = smoothstep(0.78, 0.96, fl) * incell;
            float3 fcol = lerp(float3(0.85, 0.18, 0.01), float3(1.0, 0.62, 0.08), smoothstep(0.55, 0.85, fl));
            fcol = lerp(fcol, float3(1.0, 0.92, 0.55), core);
            M *= 1 - flame; A = A * (1 - flame) + fcol * flame * 1.6;
            float2 ew = wp + float2(tm * 60.0, -tm * 140.0);
            float ember = step(0.975, hash21(floor(ew / 30.0) + floor(tm * 3.0))) * smoothstep(1.6, 0.6, hexn) * vegF;
            M *= 1 - ember; A = A * (1 - ember) + float3(1.0, 0.55, 0.1) * ember * 1.5;
        }
    }
    [branch] if (smokeOn > 0.5) {
        float2 sd = normalize(float2(0.55, -0.83));
        float su = dot(rel, sd), sv = dot(rel, float2(-sd.y, sd.x));
        float sw = 300.0 + su * 0.45 + (vnoise(float2(su, sv) / 220.0 + tm * 0.3) - 0.5) * 260.0;
        float smoke = smoothstep(sw, sw * 0.3, abs(sv)) * smoothstep(-350.0, 150.0, su) * smoothstep(2300.0, 500.0, su);
        smoke *= 0.35 + 0.65 * vnoise((float2(su, sv) + float2(-tm * 180.0, 0.0)) / 160.0);
        smoke *= (burntOut < 0.5 ? 0.75 : 0.35 * fade) * vegF;
        float3 scol = lerp(float3(0.16, 0.15, 0.14), float3(0.55, 0.52, 0.48), smoothstep(400.0, 2000.0, su));
        M *= 1 - smoke; A = A * (1 - smoke) + scol * smoke;
    }
}
// ---- flood water inside one hex (wet = this cell under water, nwet = the neighbour across the nearest edge), L = 1 normal 2 big 3 great ----
void drawFlood(inout float3 A, inout float3 M, float2 wp, float2 dl, float hexn, float wet, float nwet, float L, float yel, float foot, float tm, float above, float hzr)
{
    float a;
    float edgeN = (vnoise(wp / 140.0 + 2.0) - 0.5) * 0.30;
    float rimW = clamp(foot * 2.5 / (CW_ / 1.5), 0.05, 0.10);
    float tW = wet > 0.5 ? (nwet > 0.5 ? 1.0 : (1.0 - hexn - edgeN)) : (nwet > 0.5 ? (hexn - 1.0 - edgeN) : -1.0);
    float riverEdge = smoothstep(0.03, 0.0, hzr) * wet;
    float fl = max(smoothstep(-0.02, 0.04, tW), riverEdge) * (1.0 - 0.3 * above);   // v26k7: bare riverbank slopes fill too
    float boundary = (wet != nwet) ? 1.0 : 0.0;
    float big = L > 1.5 ? 1.0 : 0.0, great = L > 2.5 ? 1.0 : 0.0;
    [branch] if (fl > 0.001) {
        float2 flow = float2(0.8, 0.3) * tm * (90.0 + 40.0 * great);
        float n1 = vnoise((wp + flow) / 150.0), n2 = vnoise((wp - flow * 0.6) / 55.0 + 5.0), n3 = vnoise((wp + flow * 1.4) / 26.0 + 9.0);
        float churn = n1 * 0.5 + n2 * 0.3 + n3 * 0.2;
        float lodF = clamp(log2(max(foot, 1.0) / (1500.0 / 1024.0)), 0.0, 10.0);
        float rip = TexFlood.SampleLevel(sWrap, (wp + flow * 0.6) / 1500.0, lodF).a * 0.6
                  + TexFlood.SampleLevel(sWrap, (wp - flow * 0.4) / 2300.0 + 0.37, clamp(lodF - 0.6, 0.0, 10.0)).a * 0.4;
        float3 deepC = lerp(float3(0.018, 0.045, 0.115), float3(0.035, 0.085, 0.175), churn);
        float3 mudC  = lerp(float3(0.110, 0.055, 0.020), float3(0.175, 0.095, 0.035), churn);
        float3 water = lerp(deepC, mudC, yel);
        float swirl = smoothstep(0.58, 0.88, n1) * (0.35 + 0.3 * big + 0.15 * great);
        water = lerp(water, lerp(float3(0.010, 0.025, 0.070), float3(0.30, 0.19, 0.08), yel), swirl);
        float2 fd = float2(0.936, 0.351);
        float al = dot(wp, fd), ac = dot(wp, float2(-fd.y, fd.x));
        float streak = smoothstep(0.60, 0.88, vnoise(float2((al - tm * (120.0 + 80.0 * great)) / 260.0, ac / max(24.0, foot * 2.0))));
        water = lerp(water, lerp(float3(0.10, 0.20, 0.32), float3(0.34, 0.24, 0.12), yel), streak * (0.5 + 0.2 * great));
        water += lerp(float3(0.05, 0.08, 0.11), float3(0.05, 0.04, 0.025), yel) * smoothstep(0.55, 0.85, rip) * (1.0 - smoothstep(200.0, 700.0, foot));
        float foam = smoothstep(0.68 - 0.1 * great, 0.82, n2 * 0.6 + n3 * 0.4) * (0.5 + 0.25 * great) * (1.0 - smoothstep(150.0, 500.0, foot));
        float rim = smoothstep(rimW * 2.5, rimW * 0.5, tW) * boundary;
        water = lerp(water, float3(0.62, 0.60, 0.54), saturate(foam + rim * 0.95));
        float2 dg = (wp + flow * 0.9) / 46.0; float2 di = floor(dg), dfr = frac(dg) - 0.5;
        float dh = hash21(di + 41.0);
        float2 dq = dfr - (float2(hash21(di + 7.0), hash21(di + 13.0)) - 0.5) * 0.5;
        float da = (hash21(di + 3.0) - 0.5) * 0.8;
        float2 dax = float2(fd.x * cos(da) - fd.y * sin(da), fd.x * sin(da) + fd.y * cos(da));
        float dl1 = dot(dq, dax) / 0.36, dl2 = dot(dq, float2(-dax.y, dax.x)) / 0.055;
        float debris = step(0.965 - 0.02 * great, dh) * smoothstep(1.0, 0.7, dl1 * dl1 + dl2 * dl2) * (1.0 - smoothstep(40.0, 140.0, foot));
        water = lerp(water, lerp(float3(0.07, 0.045, 0.02), float3(0.33, 0.27, 0.13), step(0.985, dh)), debris * 0.9);
        a = fl * 0.97;
        M *= 1 - a; A = A * (1 - a) + water * a;
    }
    float haloW = max(0.45, foot * 6.0 / (CW_ / 1.5));
    float halo = boundary * smoothstep(-haloW, -0.02, tW) * smoothstep(0.04, -0.02, tW) * (1.0 - above);
    [branch] if (halo > 0.001) {
        a = halo * 0.75;
        M *= 1 - a; A = A * (1 - a) + float3(0.03, 0.026, 0.02) * a;
    }
}
// ---- landslide in one cell (unchanged picture from v25.8) ----
void drawSlide(inout float3 A, inout float3 M, float2 wp, float2 cw, float2 rel, float hexn, float wob, float foot, float2 uv)
{
    float a;
    float2 luv = cw / MAPW;
    float lgx = landS(luv + float2(4.0 / 2048.0, 0)) - landS(luv - float2(4.0 / 2048.0, 0));
    float lgz = landS(luv + float2(0, 4.0 / 2048.0)) - landS(luv - float2(0, 4.0 / 2048.0));
    float2 dn = -normalize(float2(lgx, lgz) + float2(0.0001, 0.0));
    float lu = dot(rel, dn), lv = dot(rel, float2(-dn.y, dn.x));
    float3 rk = TexRegion.SampleLevel(sClamp, uv, 0).rgb;
    float3 soilF = float3(0.085, 0.058, 0.034);
    float3 scarF = float3(0.30, 0.165, 0.07);
    float3 rockF = lerp(rk * 0.8, float3(0.26, 0.25, 0.23), 0.5);
    const float2 Ld = float2(-0.6, -0.8);
    float scw = 450.0 + (vnoise(float2(lu, lv) / 170.0 + 4.0) - 0.5) * 300.0 + lu * 0.10;
    float scar = smoothstep(scw, scw * 0.7, abs(lv)) * smoothstep(-1900.0, -1450.0, lu) * smoothstep(80.0, -200.0, lu);
    [branch] if (scar > 0.002) {
        float2 g = float2(lu, lv) / 95.0; float2 gi = floor(g), gf = frac(g);
        float f1 = 9.0, f2 = 9.0, hb = 0.0; float2 bd = 0;
        [unroll] for (int oy = -1; oy <= 1; oy++) [unroll] for (int ox = -1; ox <= 1; ox++) {
            float2 o = float2(ox, oy); float hh = hash21(gi + o + 3.0);
            float2 dd = gf - (o + float2(hh, hash21(gi + o + 11.0)));
            float d = length(dd);
            if (d < f1) { f2 = f1; f1 = d; bd = dd; hb = hh; } else if (d < f2) f2 = d;
        }
        float plate = smoothstep(0.55, 0.80, hb);
        float3 sc = lerp(scarF * lerp(0.75, 1.15, vnoise(wp / 40.0)), rockF * lerp(0.75, 1.2, hb), plate);
        sc *= 1.0 - 0.55 * plate * (1.0 - smoothstep(0.02, 0.08, f2 - f1));
        sc *= 0.8 + 0.35 * vnoise(float2(lu, lv) / 60.0 + 9.0);
        sc *= 0.8 + 0.4 * plate * saturate(dot(normalize(bd + 0.0001), Ld) * 0.5 + 0.5);
        a = scar * 0.96;
        M *= 1 - a; A = A * (1 - a) + sc * a;
    }
    float toeR = 0.55 + (vnoise(float2(lv / 160.0, 3.0)) - 0.5) * 0.6;
    float bigcell = smoothstep(1.38, 1.05, hexn + wob);
    float heap = max(bigcell, smoothstep(-100.0, 150.0, lu) * smoothstep(1000.0 * toeR + 450.0, 350.0, lu) * smoothstep(700.0, 330.0, abs(lv)));
    float thrown = smoothstep(350.0, 650.0, lu) * smoothstep(1900.0, 900.0, lu) * smoothstep(1000.0, 420.0, abs(lv)) * (1.0 - heap);
    [branch] if (heap > 0.002 || thrown > 0.002) {
        float3 deb = soilF * lerp(0.6, 1.1, vnoise(wp / 25.0) * 0.6 + vnoise(wp / 9.0 + 4.0) * 0.4);
        float occ = 0.0, shadow = 0.0;
        static const float2 PENT[5] = { float2(1, 0), float2(0.309, 0.951), float2(-0.809, 0.588), float2(-0.809, -0.588), float2(0.309, -0.951) };
        [loop] for (int bi = 0; bi < 3; bi++) {
            int bk = bi == 0 ? 1 : (bi == 1 ? 2 : 0);
            float cs = bk == 0 ? 220.0 : (bk == 1 ? 95.0 : 150.0);
            float keep = bk == 0 ? 1.0 : (bk == 1 ? heap * 0.95 : thrown * 0.45);
            [branch] if (keep > 0.002) {
                float2 g = wp / cs; float2 gi = floor(g), gf = frac(g);
                [unroll] for (int oy = -1; oy <= 1; oy++) [unroll] for (int ox = -1; ox <= 1; ox++) {
                    float2 o = float2(ox, oy); float2 id = gi + o + bk * 57.0;
                    float h1 = hash21(id), h2 = hash21(id + 19.0), h3 = hash21(id + 37.0);
                    if (bk == 0) {
                        float2 bcw = (gi + o + 0.2 + float2(h1, h2) * 0.6) * cs - cw; float2 brn = bcw / (CW_ / 1.5);
                        keep = 0.8 * smoothstep(1.25, 0.85, max(abs(brn.y) * 1.1547, abs(brn.x) + abs(brn.y) * 0.57735)); }
                    if (h3 > keep) continue;
                    float2 d = gf - (o + 0.2 + float2(h1, h2) * 0.6);
                    float rad = (bk == 0 ? 0.62 : (bk == 1 ? 0.50 : 0.28)) * (0.8 + h2 * 0.4);
                    float an = h1 * 6.2831853, ca = cos(an), sa = sin(an);
                    float poly = 0.0, face = 0.0, fb = -9.0;
                    [unroll] for (int k = 0; k < 5; k++) {
                        float2 nk = float2(PENT[k].x * ca - PENT[k].y * sa, PENT[k].x * sa + PENT[k].y * ca);
                        float pk = dot(d, nk) / (rad * (0.8 + 0.4 * hash21(id + k * 7.0)));
                        poly = max(poly, pk);
                        if (pk > fb) { fb = pk; face = dot(nk, Ld); }
                    }
                    float inside = smoothstep(1.0, 0.88, poly);
                    float top = smoothstep(0.7, 0.3, poly);
                    float lit = lerp(0.30 + 1.0 * saturate(face * 0.5 + 0.5), 1.25, top * 0.6);
                    float3 bc = lerp(rockF, soilF * 1.25, step(0.72, h2) * 0.55) * lerp(0.8, 1.15, h3) * lit;
                    deb = lerp(deb, bc, inside);
                    occ = max(occ, inside);
                    shadow = max(shadow, smoothstep(rad * 1.15, rad * 0.7, length(d + Ld * rad * 0.5)) * (1.0 - inside));
                }
            }
        }
        deb *= 1.0 - 0.85 * shadow;
        deb = lerp(deb, float3(0.11, 0.095, 0.075), smoothstep(40.0, 160.0, foot) * 0.6);
        a = max(heap * 0.98, occ * thrown + shadow * thrown * 0.7);
        M *= 1 - a; A = A * (1 - a) + deb * a;
    }
}
// ---- v26k30b: the shaken ground over a whole shaken cell: a fine net of fresh cracks and grey dust, stronger the stronger the quake
//      (qt 0 지진, 1 강한 지진, 2 대지진); edgeN = the cell-edge measure toward a neighbour that was not shaken (0 = no edge there) ----
void drawShaken(inout float3 A, inout float3 M, float2 wp, float qt, float foot, float edgeN)
{
    float px = max(foot, 1.0);
    float k = qt < 0.5 ? 0.55 : (qt < 1.5 ? 0.75 : 0.95);
    float inc = smoothstep(1.0, 0.86, edgeN);
    float sz = 170.0 * max(1.0, px / 6.0);
    float2 g = wp / sz; float2 gi = floor(g), gf = frac(g); float f1 = 9.0, f2 = 9.0;
    [unroll] for (int oy = -1; oy <= 1; oy++) [unroll] for (int ox = -1; ox <= 1; ox++) {
        float2 o = float2(ox, oy); float2 pc = o + 0.1 + float2(hash21(gi + o + 71.0), hash21(gi + o + 83.0)) * 0.8;
        float d = length(gf - pc); if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d; }
    float edge = (f2 - f1) * sz, cwd = max(0.045 * sz, 1.0 * px);
    float fis = smoothstep(cwd, cwd * 0.4, edge) * smoothstep(0.30, 0.55, vnoise(wp / 520.0 + 31.0));   // broken here and there, not a full grid
    float dust = (0.20 + 0.10 * vnoise(wp / 260.0 + 5.0)) * k * inc;
    M *= 1 - dust; A = A * (1 - dust) + float3(0.20, 0.18, 0.15) * dust;          // grey dust over the shaken ground
    float a = fis * k * inc * (1.0 - smoothstep(600.0, 1400.0, px));
    M *= 1 - a; A = A * (1 - a) + float3(0.020, 0.017, 0.014) * a;
}
// ---- earthquake along a fault through the epicentre (unchanged picture from v25.9) ----
void drawQuake(inout float3 A, inout float3 M, float2 wp, float2 cw, float2 rel, float nb, float foot, float2 uv)
{
    float a;
    float qt = floor((nb + 0.5) / 12.0), qdir = nb - qt * 12.0;
    float qs = qt < 0.5 ? 0.6 : (qt < 1.5 ? 1.0 : 1.4);
    float qw = qt < 0.5 ? 0.55 : (qt < 1.5 ? 1.0 : 1.35);
    float angq = qdir * 0.2618;
    float2 fd = float2(cos(angq), sin(angq));
    float qa = dot(rel, fd), qn = dot(rel, float2(-fd.y, fd.x));
    float qL = 3600.0 * qs;
    if (abs(qa) > qL + 200.0) return;
    float wig = (vnoise(float2(qa / 650.0, 1.7)) - 0.5) * 420.0 + (vnoise(float2(qa / 140.0, 3.1)) - 0.5) * 90.0;
    float sd = qn - wig, qd = abs(sd);
    float taper = saturate(1.0 - abs(qa) / qL), inL = step(abs(qa), qL);
    float px = max(foot, 1.0);
    float wq = max((40.0 + 140.0 * taper) * qw, 3.0 * px);
    float crack = smoothstep(wq, wq * 0.55, qd) * inL;
    [unroll] for (int bi = 0; bi < 5; bi++) {
        if (qt < 0.5 && (bi == 0 || bi == 4)) continue;
        float ofs = (bi - 2.0) * 1300.0 * qs, ba = ((bi & 1) == 0 ? 0.6 : -0.55);
        float2 bo = cw + fd * ofs + float2(-fd.y, fd.x) * ((vnoise(float2(ofs / 650.0, 1.7)) - 0.5) * 420.0 + (vnoise(float2(ofs / 140.0, 3.1)) - 0.5) * 90.0);
        float2 bd = normalize(fd * cos(ba) + float2(-fd.y, fd.x) * sin(ba) * ((bi & 2) == 0 ? 1.0 : -1.0));
        float2 br = wp - bo; float bu = dot(br, bd), bv = dot(br, float2(-bd.y, bd.x));
        float bL = 1500.0 * qs * (1.0 - abs(bi - 2.0) * 0.2);
        bv -= (vnoise(float2(bu / 200.0, bi * 7.0)) - 0.5) * 160.0;
        float bw = max((20.0 + 60.0 * saturate(1.0 - bu / bL)) * qw, 2.0 * px);
        crack = max(crack, smoothstep(bw, bw * 0.5, abs(bv)) * step(0.0, bu) * step(bu, bL));
    }
    float fw = max((150.0 + 120.0 * taper) * qw, 6.0 * px);
    float face = smoothstep(wq * 0.7, wq, sd) * smoothstep(wq + fw, wq + fw * 0.6, sd) * inL * (0.5 + 0.5 * taper);
    float lip = smoothstep(wq * 0.7, wq, -sd) * smoothstep(wq + fw * 0.6, wq + fw * 0.3, -sd) * inL * (0.4 + 0.6 * taper);
    float bandW = (900.0 * taper + 200.0) * qs;
    float band = smoothstep(bandW, bandW * 0.4, qd) * inL * (0.3 + 0.7 * taper);
    float2 bg = wp / 75.0;
    float lineW = max(0.06, px * 1.2 / 75.0);
    float blocks = smoothstep(lineW, 0.0, 0.5 - max(abs(frac(bg.x + vnoise(bg) * 0.6) - 0.5), abs(frac(bg.y + vnoise(bg + 4.0) * 0.6) - 0.5)));
    float rubble = band * blocks * (1.0 - crack) * (1.0 - face);
    [branch] if (band + crack + face + lip > 0.001) {
        float3 rkq = TexRegion.SampleLevel(sClamp, uv, 0).rgb;
        a = band * 0.30;
        M *= 1 - a; A = A * (1 - a) + float3(0.06, 0.05, 0.04) * a;
        a = rubble * 0.85;
        M *= 1 - a; A = A * (1 - a) + float3(0.025, 0.02, 0.017) * a;
        float fstr = vnoise(float2(qa / 40.0, sd / 260.0) + 5.0);
        float3 faceC = lerp(float3(0.56, 0.46, 0.33), rkq * 1.3 + 0.08, 0.3) * lerp(0.8, 1.15, fstr);
        a = face * 0.95;
        M *= 1 - a; A = A * (1 - a) + faceC * a;
        a = lip * 0.85;
        M *= 1 - a; A = A * (1 - a) + float3(0.42, 0.35, 0.26) * a;
        a = crack;
        M *= 1 - a; A = A * (1 - a) + float3(0.008, 0.007, 0.006) * a;
    }
}

// v26b: the terrain vertex shader's constants (d3dx.ini: ps-cb13 = vs-cb0): rows 0..3 = world -> clip (row vectors)
cbuffer CamVS : register(b13) { float4 cvM[5]; }
float3 eyePos()                                      // the camera: the world point whose clip x, y and w are all zero
{
    float3 a0 = float3(cvM[0].x, cvM[0].y, cvM[0].w), a1 = float3(cvM[1].x, cvM[1].y, cvM[1].w), a2 = float3(cvM[2].x, cvM[2].y, cvM[2].w);
    float3 b = -float3(cvM[3].x, cvM[3].y, cvM[3].w);
    float3 c0 = cross(a1, a2), c1 = cross(a2, a0), c2 = cross(a0, a1);
    float det = dot(a0, c0);
    return abs(det) > 1e-18 ? float3(dot(b, c0), dot(b, c1), dot(b, c2)) / det : float3(0.0, -1e9, 0.0);
}
// v26b: seamless rock photo mapping - three projections (from above and the two vertical planes) blended by the slope
//       direction, each sampled with its own true screen derivatives (no smearing, no sparkle on steep faces)
#define TRIROCK(T, p, w, dpx, dpy, S) (T.SampleGrad(sWrap, (p).xz / (S), (dpx).xz / (S), (dpy).xz / (S)) * (w).y \
                                     + T.SampleGrad(sWrap, (p).zy / (S), (dpx).zy / (S), (dpy).zy / (S)) * (w).x \
                                     + T.SampleGrad(sWrap, (p).xy / (S), (dpx).xy / (S), (dpy).xy / (S)) * (w).z)
// uv: map uv (0..1, v=0 north)   nrm: terrain normal (y up)   wxz: world xz   h: land height   wy: world height (near: the mesh's own)
void season(float2 uv, float3 nrm, float2 wxz, float h, float wy, out float3 A, out float3 M)
{
    A = 0; M = 1;
    float4 ip = IniParams.Load(int2(0, 0));
    float month = ip.x;
    if (month < 0.5 || ip.y < 0.5) discard;
    gridInit();                                                   // v26k30: the game's own hex grid

    // v26: nothing is painted on the sea floor - that test is in main() (a second discard inside this function makes the game's
    //      shader compiler fail with "internal error: argument pulled into unrelated predicate")
    float veg, farm, bare; matinfo(uv, veg, farm, bare);
    // v23: slope from the height map itself (the same near and far; the far terrain's own normal changes with zoom)
    float2 e1 = float2(1.0 / 2048.0, 0), e2 = float2(0, 1.0 / 2048.0);
    float gx = (landS(uv + e1) - landS(uv - e1)) / 200.0;
    float gz = (landS(uv + e2) - landS(uv - e2)) / 200.0;
    float3 nn = normalize(float3(-gx, 1.0, -gz));
    float ny = nn.y;

    // ---- climate: v23 real regional climate (city temperatures, 1 C colder than today), altitude, month ----
    float lat = 45.3 - 25.2 * uv.y;
    // v24: 36 turns a year - the turn (first / middle / last ten days) blends between the monthly means
    float4 ip1 = IniParams.Load(int2(1, 0));
    float xun = (ip1.x >= 0.0 && ip1.x <= 2.0) ? ip1.x : 1.0;
    // v25.9i: the game's calendar keeps KOEI's seasons (spring = 1-3월): game month m shows the real climate of month m+2
    float tpos = clamp(round(month), 1.0, 12.0) + 1.0 + (round(xun) - 1.0) / 3.0;   // 0 = mid January of the real climate
    float tfl = floor(tpos);
    int mi0 = ((int)tfl + 12) % 12, mi1 = ((int)tfl + 13) % 12;
    float4 cA = climAt(uv, mi0), cB = climAt(uv, mi1);
    float4 cl = lerp(cA, cB, tpos - tfl);
    float alt = max(h - 750.0, 0.0);
    float lapse = 0.0045 * alt;
    // v26: the year's weather from the rules module - temperature anomaly and rain multiplier of the weather nodes (the "normal year" stays the climate itself)
    float4 wctl = IniParams.Load(int2(100, 0));
    float wValid = (wctl.w > 0.5 && abs(wctl.z - 26.0) < 0.5) ? 1.0 : 0.0;
    float wdT = 0.0, wM = 1.0;                                                               // v26h: read only when valid (same values)
    [branch] if (wValid > 0.5) { wdT = clamp(nodeAt(101, uv), -4.0, 4.0); wM = clamp(nodeAt(122, uv), 0.4, 1.8); }
    float T = cl.r - lapse + wdT;
    // v26h: the rain grid (tier, wet, snowing, cracked) is read once here; the rain picture below reuses it. The ground's wetness
    //       from recent rain stands in for humidity: it slows the drying of the grass a little (nothing else uses it).
    float4 dlk0 = IniParams.Load(int2(60, 0));
    float wOn0 = (wValid > 0.5 && dlk0.w > 0.5) ? 1.0 : 0.0;
    float4 R0 = 0; float tierG = 0.0, wxB = 0.0, wxOn = 0.0;   // v26k30: tierG the rain grid's (the cloud's soft outline); R0.x / R0.z this cell's own
    [branch] if (wOn0 > 0.5) { R0 = rainAt(wxz); tierG = R0.x; wxB = cellWx(hexIdxAt(wxz)); wxOn = 1.0; R0.x = wxBits(wxB, 0.0, 2.0); R0.z = wxBits(wxB, 2.0, 1.0); }
    // v13: impassable mountains read as high, cold, rocky massifs; passable hills stay wooded and soft
    float4 cm = TexFreeze.SampleLevel(sClamp, uv, 0);
    float imp = smoothstep(0.25, 0.75, cm.b), hillC = smoothstep(0.25, 0.75, cm.a) * (1.0 - imp);
    T -= 1.2 * imp;
    float rising = smoothstep(-1.5, 1.5, cl.g);           // 1 = getting warmer (late winter / spring), 0 = cooling (autumn)
    float warm = rising, cool = 1.0 - rising;
    float dry = saturate(cl.b * (1.0 - 0.35 * (wM - 1.0)));        // v26: a wet year is a little less dry, a dry year more
    float dryG = dry * (1.0 - 0.2 * R0.y);                            // v26h: humid (recently rained-on) ground: the grass dries a little slower
    float Tmin = cl.a - lapse - 1.2 * imp;                // coldest month here
    float ev = smoothstep(2.0, 6.5, Tmin);                // mild winters: evergreen country, grass never fully dies
    // grass life 0 (dry straw) .. 1 (green): wakes ~3-9 C in spring, browns ~15-7 C in autumn, dry seasons brown it
    float G = lerp(smoothstep(7.0, 15.0, T), smoothstep(3.0, 9.0, T), rising) * (1.0 - 0.75 * dryG);
    G = max(G, ev * 0.55 * (1.0 - 0.5 * dryG));
    // spring flush of new leaves; in the evergreen south it comes later (March-April), not in the mild winter
    float fresh = rising * smoothstep(6.0 + 8.0 * ev, 10.0 + 8.0 * ev, T) * (1.0 - smoothstep(16.0 + 5.0 * ev, 21.0 + 5.0 * ev, T)) * (1.0 - 0.9 * dry);
    float olv = ev * (1.0 - smoothstep(12.0, 17.0, T)) * (1.0 - fresh);
    float4 zone = zoneAt(uv);
    float castle = zone.b, northZ = zone.r, nwZ = zone.g, gobiZ = zone.a;

    // ---- relief measures (used by the treeline and the shading) ----
    float2 t1 = float2(3.0 / 2048.0, 0), t2 = float2(0, 3.0 / 2048.0);
    float2 u1 = float2(9.0 / 2048.0, 0), u2 = float2(0, 9.0 / 2048.0);
    float2 w1 = float2(20.0 / 2048.0, 0), w2 = float2(0, 20.0 / 2048.0);
    float hn = (landS(uv + t1) + landS(uv - t1)
              + landS(uv + t2) + landS(uv - t2)) * 0.25;
    float hw = (landS(uv + u1) + landS(uv - u1)
              + landS(uv + u2) + landS(uv - u2)) * 0.25;
    float hW = (landS(uv + w1) + landS(uv - w1)
              + landS(uv + w2) + landS(uv - w2)) * 0.25;
    float cav = (hn - h) * 0.6 + (hw - h) * 0.4;          // > 0 in valleys, < 0 on crests (small scale)
    float cavW = hW - h;                                   // saddles between summits (large scale)
    float reliefF = smoothstep(80.0, 600.0, alt + abs(hw - h) * 2.0);

    // ---- treeline (game height): north ~2310, Qinling ~2730, far south ~3150 (v12, matched to 1.2x mountains)
    //      natural edge: sunny south slopes higher, shady north slopes lower, rock runs down the ridges,
    //      forest climbs up the gullies, ragged patches instead of a regular zigzag
    float tl = 2730.0 + (34.0 - lat) * (lat < 34.0 ? 46.7 : 70.0);
    float southF = nn.z / max(length(nn.xz), 0.05) * saturate(1.0 - nn.y) ;   // +1 facing south (world +z)
    tl += 230.0 * southF;
    tl += clamp(cav * 0.9, -380.0, 380.0) + clamp(cavW * 0.35, -260.0, 260.0);
    float above, cliffR;
    // v26h (frame): the noise terms can lower the treeline by at most 410 and raise 'above' by at most 0.35; on passable ground
    //       far enough below that bound 'above' is exactly 0 whatever the noise - the six noise reads are skipped there.
    //       (imp = 0 there, so the cliff term is 0 too.) Everything else is computed exactly as before.
    float aboveMax = (h - (tl - 480.0 * imp + 260.0 * hillC - 410.0)) / 360.0 + 0.85;
    [branch] if (imp <= 0.0 && aboveMax < -0.01) { above = 0.0; cliffR = 0.0; }
    else {
    float2 wq = wxz / 1400.0; wq += float2(vnoise(wq * 0.7 + 3.3), vnoise(wq * 0.7 + 8.1)) * 1.6;     // warped, not periodic
    float tln = vnoise(wq) * 0.5 + vnoise(wq * 2.7 + 5.2) * 0.3 + vnoise(wxz / 160.0 + 13.0) * 0.2;
    tl += (tln - 0.5) * 420.0 - (1.0 - smoothstep(0.55, 0.85, ny)) * 200.0;
    tl += -480.0 * imp + 260.0 * hillC;
    float patchN = vnoise(wxz / 75.0 + 31.0);
    above = saturate((h - tl) / 360.0 + 0.5 + (patchN - 0.5) * 0.7);   // 1 = no plants, bare rock (ragged patches)
    // impassable: steep faces are bare cliffs even below the treeline (ragged, follows the slope)
    cliffR = imp * smoothstep(0.82, 0.50, ny + (patchN - 0.5) * 0.18);
    above = saturate(max(above, cliffR * 0.85));
    }
    float vegB = veg * (1.0 - above);                              // vegetation that can exist here
    // plains only (flat and low): where spring flowers may grow
    float plainF = smoothstep(0.88, 0.96, ny) * (1.0 - smoothstep(900.0, 1600.0, alt));
    float forestF = smoothstep(0.15, 0.6, TexForest.SampleLevel(sClamp, uv, 0).r);   // v18: no ground photos / flower meadows under trees
    float slopeF = 1.0 - plainF;                                   // hills and mountain sides

    float2 dx = ddx(wxz), dy = ddy(wxz);
    float dhx = ddx(wy), dhy = ddy(wy);                // v26b: for the triplanar rock sampling
    float foot = max(length(dx), length(dy)) + 1e-7 * abs(dhx + dhy);   // world units per pixel (the tiny term keeps the derivatives out of the branches)
    float3 wp3 = float3(wxz.x, wy, wxz.y), dp3x = float3(dx.x, dhx, dx.y), dp3y = float3(dy.x, dhy, dy.y);
    float3 triW = pow(abs(nrm) + 1e-4, 4.0); triW /= (triW.x + triW.y + triW.z);
    float n1 = fbm(wxz / 9000.0), n2 = vnoise(wxz / 1500.0 + 11.0);
    float n3 = lerp(vnoise(wxz / 380.0 + 29.0), 0.5, smoothstep(90.0, 300.0, foot));
    float3 vegTex = lin(TexVeg.Sample(sWrap, wxz / 4200.0).rgb);
    float detail = dot(vegTex, float3(0.3, 0.59, 0.11)) / 0.128;  // ~1 on average (v19: true mean of the photo)
    detail = lerp(clamp(detail, 0.45, 1.8), 1.0, smoothstep(250.0, 800.0, foot));   // same average near and far

    float3 col; float a;

    // ---- v17: plain ground by region: real soil and grass photos replace the game's blurry plain ground ----
    float a0P = plainF * (1.0 - bare) * (1.0 - farm * 0.75) * (1.0 - forestF) * 0.95;          // v25.3: weights first ...
    float hillP = (1.0 - plainF) * smoothstep(0.55, 0.80, ny) * (1.0 - above) * (1.0 - forestF) * (1.0 - bare) * smoothstep(0.15, 0.55, veg) * 0.72;
    [branch] if (max(a0P, hillP) > 0.0 || castle > 0.02)                                         // ... photos only where they show
    {
        float4 sw4 = TexSoil.SampleLevel(sClamp, uv, 0); sw4 /= max(dot(sw4, 1.0.xxxx), 0.001);
        float2 g1 = wxz / 2600.0, g2 = float2(wxz.x * 0.8 - wxz.y * 0.6, wxz.x * 0.6 + wxz.y * 0.8) / 6100.0;   // two scales: no visible tiling
        float lodG = clamp(log2(max(foot, 1.0) / (2600.0 / 1024.0)), 0.0, 8.0), lodG2 = clamp(lodG - 1.2, 0.0, 8.0);
        float3 soil = 0;
        [unroll] for (int si = 0; si < 4; si++) {
            [branch] if (sw4[si] > 0.0) {                                                        // v26h (frame): a zero weight adds exactly nothing
            float3 t = TexGround.SampleLevel(sWrap, float3(g1, si), lodG).rgb * 0.6 + TexGround.SampleLevel(sWrap, float3(g2, si), lodG2).rgb * 0.4;
            soil += lin(t) * sw4[si];
            }
        }
        // v24: northwest (Liangzhou): fine loess in Longxi/Tianshui, gravel desert (gobi) on the Hexi plains
        [branch] if (nwZ > 0.02) {
            float3 loess = lin(TexGround.SampleLevel(sWrap, float3(g1 * 1.1, 11), lodG).rgb) * 0.6 + lin(TexGround.SampleLevel(sWrap, float3(g2, 11), lodG2).rgb) * 0.4;
            float3 gob = lin(TexGround.SampleLevel(sWrap, float3(g1 * 1.3, 12), lodG).rgb);
            float3 sand = lin(TexGround.SampleLevel(sWrap, float3(g1 * 1.2, 13), lodG).rgb);
            float3 desert = lerp(gob, sand, smoothstep(0.45, 0.75, vnoise(wxz / 3000.0 + 61.0)));
            soil = lerp(soil, lerp(loess, desert, gobiZ), saturate(nwZ * 1.1));
        }
        float gm = saturate(veg * 1.25 - 0.25 + (vnoise(wxz / 900.0 + 77.0) - 0.5) * 0.6);   // grass where the game has plants, patchy
        gm *= 1.0 - 0.7 * gobiZ;                                                               // v24: sparse grass on the gravel desert
        float3 baseC = soil;
        // v26h (frame): the grass photos are read only where grass shows (gm > 0), and each photo only when its blend weight
        //       is not exactly 0 (lerp(x, y, 0) is exactly x). Same picture, fewer texture reads on bare ground and in winter.
        [branch] if (gm > 0.0) {
        float lushG = saturate(sw4.b + sw4.a * 0.9 + sw4.r * 0.35);
        float g1w = saturate(G * 2.0), g2w = saturate(G * 2.0 - 1.0);
        float3 grGreen = 0;
        [branch] if (g2w > 0.0) {
            float3 gr5 = lin(TexGround.SampleLevel(sWrap, float3(g1 * 1.45, 5), lodG).rgb);
            grGreen = gr5;
            [branch] if (lushG > 0.0) grGreen = lerp(gr5, lin(TexGround.SampleLevel(sWrap, float3(g1 * 1.45, 4), lodG).rgb), lushG);
        }
        // v23: the grass itself follows the season: dry straw -> green-brown -> green (evergreen country: olive in winter)
        float3 grStraw = lin(TexGround.SampleLevel(sWrap, float3(g1 * 1.3, 6), lodG).rgb);
        float3 grMix = 0;
        [branch] if (g1w > 0.0 || olv > 0.0) grMix = lin(TexGround.SampleLevel(sWrap, float3(g1 * 0.9, 7), lodG).rgb);
        float3 gr = lerp(lerp(grStraw, grMix, g1w), grGreen, g2w);
        gr = lerp(gr, grMix, olv * 0.45);
        gr *= lerp(1.0.xxx, float3(1.06, 1.16, 0.80), fresh * 0.8);                  // spring: fresh light green
        baseC = lerp(soil, gr, gm);
        }
        float a0 = a0P;
        // v19: green hill sides below the treeline get the same grass/soil photos, so near and far look alike
        a0 = max(a0, hillP);
        // v23: castle surroundings: fine, flat packed earth (lighter, low contrast) instead of big dark clods
        [branch] if (castle > 0.02) {
            float3 packed = 0, soilMean = 0;
            [unroll] for (int sj = 0; sj < 4; sj++) {
                packed   += lin(TexGround.SampleLevel(sWrap, float3(g1 * 3.1, sj), max(lodG - 1.6, 0.0)).rgb) * sw4[sj];
                soilMean += lin(TexGround.SampleLevel(sWrap, float3(0.5, 0.5, sj), 9.0).rgb) * sw4[sj];
            }
            packed = lerp(soilMean, packed, 0.45) * 1.22;
            float cA = smoothstep(0.05, 0.6, castle) * 0.9;
            baseC = lerp(baseC, packed, cA); a0 = max(a0, cA);
        }
        M *= 1 - a0; A = A * (1 - a0) + baseC * a0;
    }

    // ---- relief shading: darker valley floors, folds and saddles so each summit and ridgeline stands out ----
    {
        float noC = 1.0 - smoothstep(0.02, 0.4, castle);            // v23: no dark ring around castles
        float dark = (saturate(cav / 240.0) * 0.40 + saturate(cavW / 500.0) * 0.18) * reliefF * (1.0 + 0.45 * imp - 0.25 * hillC) * noC;
        M *= 1.0 - saturate(dark); A *= 1.0 - saturate(dark);
        float crest = saturate(-cav / 240.0) * 0.10 * reliefF * (1.0 + 1.2 * imp) * noC;   // crests: a touch of light rock
        A = A * (1 - crest) + float3(0.30, 0.29, 0.27) * crest; M *= 1 - crest;
        // v26: a lone pine on the summits of the low hills (the painting look), only up close
        float pineF = hillC * saturate(-cav / 160.0) * (1.0 - smoothstep(35.0, 110.0, foot)) * noC;
        [branch] if (pineF > 0.02) {
            float2 pg = wxz / 260.0; float2 pid = floor(pg); float hp = hash21(pid + 61.0);
            float2 pc = (pid + 0.5 + (float2(hash21(pid + 3.0), hash21(pid + 5.0)) - 0.5) * 0.5) * 260.0;
            float d = length(wxz - pc); float rad = 34.0 + 18.0 * hash21(pid + 9.0);
            float tree = step(hp, 0.55) * smoothstep(rad, rad * 0.55, d) * pineF;
            float3 pine = lerp(float3(0.045, 0.085, 0.035), float3(0.09, 0.14, 0.06), smoothstep(rad * 0.6, 0.0, d));
            M *= 1 - tree; A = A * (1 - tree) + pine * tree;
            float shade = step(hp, 0.55) * smoothstep(rad * 1.3, rad * 0.6, length(wxz - pc - float2(18.0, 22.0))) * (1.0 - tree) * pineF * 0.35;
            M *= 1 - shade; A *= 1 - shade;
        }
    }

    // ---- summer: lush deep green (wet summers only) ----
    float lush = smoothstep(17.0, 23.0, T) * (1.0 - 0.7 * dry) * vegB;
    M *= lerp(1.0.xxx, float3(0.84, 1.0, 0.74), lush * 0.75); A *= lerp(1.0.xxx, float3(0.84, 1.0, 0.74), lush * 0.75);

    // ---- spring: fresh green (v23: from the real warming, not the calendar) ----
    a = fresh * vegB * 0.30 * (1.0 - plainF * 0.6);          // plains already got it in their grass photo
    col = float3(0.075, 0.17, 0.03) * detail;
    M *= 1 - a; A = A * (1 - a) + col * a;

    // multi-coloured blossom: individual flowers up close, colour clusters at mid range,
    // a patchwork of differently coloured meadows from far away
    // ---- v23: flowers by species, each opening at its own temperature in its own region ----
    //  0 plum (white-pink, late winter: the coldest month just turning; few, only where winters are mild)
    //  1 peach / apricot pink (spring ~10-14 C)   2 forsythia yellow bushes (~8-11 C, north of the south hills)
    //  3 azalea magenta (hill feet, ~13-18 C)   4 violet (meadows, spring)   5 summer meadow flowers (north) / autumn asters
    float patch = smoothstep(0.405, 0.705, n1 * 0.6 + n2 * 0.4);   // meadow patches (v9: 30% fewer)
    float sw[6] = { 0, 0, 0, 0, 0, 0 };
    // v26h (frame): the common factor of every species (plants, no forest, no castle) and an upper bound of the weights are
    //       checked first; where they are 0 (or below 1e-12: the same picture in 32-bit float) the six noise reads are skipped
    float swCF = vegB * (1.0 - forestF) * (1.0 - smoothstep(0.1, 0.5, castle));
    float swUB = 0.0;
    [branch] if (swCF > 0.0)
    {
        float hillFoot = smoothstep(0.97, 0.86, ny) * smoothstep(0.70, 0.80, ny) * (1.0 - smoothstep(600.0, 1300.0, alt));
        float meadow = patch * (1.0 - farm);
        float spring = rising * (1.0 - 0.7 * dry);
        float notSouth = 1.0 - ev * 0.8;                         // evergreen south: hardly any spring meadows
        float plum = smoothstep(-1.5, 1.5, Tmin) * bell(T - Tmin, 2.0, 2.0) * smoothstep(-0.5, 0.5, cl.g + 0.3);
        float summerN = smoothstep(17.0, 20.0, T) * (1.0 - smoothstep(24.0, 27.0, T)) * smoothstep(33.0, 36.0, lat) * (1.0 - 0.6 * dry);
        float autumnF = cool * bell(T, 15.0, 3.5) * (1.0 - 0.5 * dry);
        float b1 = bell(T, 12.0, 2.5), b2 = bell(T, 9.5, 2.2), b3 = bell(T, 15.5, 2.6), b4 = bell(T, 12.5, 3.0);
        swUB = (plum * 1.4 * 0.45 * plainF + spring * (b1 * notSouth * (1.35 + meadow * 0.12) * plainF + b2 * notSouth * 0.75 * max(plainF, hillFoot)
              + b3 * max(plainF * 0.35, hillFoot * 1.2) + b4 * notSouth * meadow * 0.28 * plainF) + max(summerN, autumnF) * meadow * 0.40 * plainF) * swCF;
        [branch] if (swUB > 1e-12) {
        float orchard = smoothstep(0.55, 0.72, vnoise(wxz / 650.0 + 41.0));
        float bushA = smoothstep(0.60, 0.74, vnoise(wxz / 240.0 + 53.0));
        float bushB = smoothstep(0.60, 0.74, vnoise(wxz / 240.0 + 67.0));
        sw[0] = plum * orchard * (0.4 + farm) * 0.45 * plainF;
        sw[1] = spring * bell(T, 12.0, 2.5) * notSouth * (orchard * (0.5 + farm) * 0.9 + meadow * 0.12) * plainF;
        sw[2] = spring * bell(T, 9.5, 2.2) * notSouth * bushA * (0.35 + farm * 0.4) * max(plainF, hillFoot);
        sw[3] = spring * bell(T, 15.5, 2.6) * bushB * max(plainF * 0.35, hillFoot * 1.2);
        sw[4] = spring * bell(T, 12.5, 3.0) * notSouth * meadow * 0.28 * plainF;
        sw[5] = max(summerN, autumnF) * meadow * 0.40 * plainF;
        [unroll] for (int q = 0; q < 6; q++) sw[q] *= vegB * (1.0 - forestF) * (1.0 - smoothstep(0.1, 0.5, castle));
        }
    }
    float swT = sw[0] + sw[1] + sw[2] + sw[3] + sw[4] + sw[5];
    float c1 = 46.0;
    [branch] if (swUB > 1e-12) {                                                                 // v26h (frame): see above
    // close range: one flower per jittered cell on two offset grids, species picked by the local mix
    float d1 = 0.0; float3 k1 = 0;
    float aa = max(foot / c1, 0.02) * 2.0;                               // edge softness (no shimmer)
    // v26h: swT > 1e-4 - the cell hash is exactly 0 in about one cell in 800, and such a cell drew a full flower for ANY
    //       swT > 0 (a lone flower in midwinter or midsummer up close); below a 1-in-6000 density nothing is drawn now
    [branch] if (foot < c1 * 1.1 && swT > 1e-4)                                                  // v25.3
    [unroll] for (int g = 0; g < 2; g++) {
        float2 p = wxz / c1 + (g ? float2(0.37, 0.71) : 0.0);
        float2 id = floor(p) + g * 101.0;
        float hp = hash21(id + 9.3);
        if (hp < saturate(swT * 1.6)) {
            float pick = hash21(id + 17.7) * swT, acc = 0.0; int spc = 0;
            [unroll] for (int q = 0; q < 6; q++) { if (pick > acc) spc = q; acc += sw[q]; }
            static const float3 SC[6] = { float3(0.76,0.64,0.66), float3(0.80,0.33,0.45), float3(0.80,0.58,0.03),
                                          float3(0.62,0.07,0.30), float3(0.30,0.12,0.55), float3(0.72,0.71,0.64) };
            static const float3 CC[6] = { float3(0.62,0.48,0.12), float3(0.70,0.45,0.10), float3(0.75,0.40,0.02),
                                          float3(0.35,0.02,0.12), float3(0.80,0.78,0.35), float3(0.80,0.55,0.02) };
            static const float  PN[6] = { 5, 5, 4, 5, 5, 16 };
            static const float  PW[6] = { 0.6, 0.6, 3.0, 0.35, 1.2, 6.0 };
            static const float  RD[6] = { 1, 1, 0, 1, 0, 0 };
            static const float  SZ[6] = { 0.30, 0.31, 0.27, 0.34, 0.20, 0.24 };
            static const float  CR[6] = { 0.13, 0.13, 0.08, 0.10, 0.10, 0.24 };
            float2 jit = float2(hash21(id + 5.5), hash21(id + 8.1)) - 0.5;
            float rad = SZ[spc] * lerp(0.75, 1.15, hash21(id + 2.2));
            float2 v = (frac(p) - 0.5 - jit * 0.45) / rad;
            float r = length(v), an = atan2(v.y, v.x) + hash21(id + 3.7) * 6.2831853;
            float kp = abs(cos(an * PN[spc] * 0.5));
            float lim = RD[spc] > 0.5 ? pow(sqrt(kp), PW[spc]) : pow(kp, PW[spc]);
            if (spc == 4) lim *= 1.0 - 0.22 * pow(sin(an * 15.0), 2.0);          // ragged violet petals
            float inside = smoothstep(lim + aa / rad, lim - aa / rad, r);
            float3 pc = SC[spc] * lerp(1.12, 0.78, saturate(r / max(lim, 0.05)));   // lighter centre, deeper tips
            pc = lerp(pc, CC[spc], smoothstep(CR[spc] + aa / rad, CR[spc] - aa / rad, r));
            k1 = inside > d1 ? pc : k1; d1 = max(d1, inside);
        }
    }
    // mid / far range: each meadow area shows the colour of the species growing there
    float3 kc[6] = { float3(0.76,0.64,0.66), float3(0.80,0.33,0.45), float3(0.80,0.58,0.03),
                     float3(0.62,0.07,0.30), float3(0.30,0.12,0.55), float3(0.72,0.71,0.64) };
    float3 k2 = 0; float hc = frac(vnoise(wxz / 420.0 + 3.0) * 2.3 + vnoise(wxz / 1700.0 + 5.0) * 1.7) * swT, acc2 = 0.0;
    float3 kAvg = 0;
    [unroll] for (int q = 0; q < 6; q++) { kAvg += kc[q] * sw[q]; if (hc > acc2) k2 = kc[q]; acc2 += sw[q]; }
    kAvg /= max(swT, 1e-4);
    float l23 = smoothstep(60.0, 200.0, foot);
    k2 = lerp(k2, kAvg, l23);
    float d2 = saturate(swT * 1.6) * lerp(smoothstep(0.30, 0.75, vnoise(wxz / 160.0 + 7.0)) * 0.55, 0.30, l23);   // v19: same flower cover as up close
    float l12 = smoothstep(c1 * 0.30, c1 * 1.1, foot);
    float dotm = lerp(d1, d2, l12);
    float3 fc = lerp(k1, k2, l12);
    a = saturate(dotm * 1.05);
    M *= 1 - a; A = A * (1 - a) + fc * a;
    }
    float far = smoothstep(c1 * 0.30, c1 * 1.1, foot);                                         // (= the l12 of the flower block)

    // ---- v23: fields through the year ----
    float patch2 = smoothstep(0.255, 0.575, n1 * 0.5 + n2 * 0.5);   // v9: rapeseed only on ~70% of farmland
    // v24: snow counts 4 C colder north of the Yellow River; in November part of December's cold comes early (first snow)
    // v26k12: the snow follows the rules' own turn and the day within it (list entry 99: x = 0..1 through the turn, y = turn + 1):
    //         the white cover grows and melts day by day toward the next turn's, at the pace of the temperature, instead of
    //         jumping at the turn change (the snow itself has fallen first - the rules make it snow where the cover is spreading)
    float tposS = tpos, Tsn = T, iceT = T;
    [branch] if (wValid > 0.5) {
        float4 dq = IniParams.Load(int2(99, 0));
        [branch] if (dq.y > 0.5 && dq.y < 36.5) {
            float tr = round(dq.y) - 1.0, mq = floor(tr / 3.0 + 0.01);
            float tq = mq + 2.0 + (tr - 3.0 * mq - 1.0) / 3.0 + saturate(dq.x) / 3.0;
            float dS = tq - tpos; dS -= 12.0 * round(dS / 12.0); tposS = tpos + dS;
            float fS = tposS - tfl, rS = cl.r;
            [branch] if (fS >= 0.0 && fS <= 1.0) rS = lerp(cA.r, cB.r, fS);
            else { float tf2 = floor(tposS); int s0 = ((int)tf2 % 12 + 12) % 12, s1 = (s0 + 1) % 12; rS = lerp(climAt(uv, s0).r, climAt(uv, s1).r, tposS - tf2); }
            Tsn = T + (rS - cl.r);
            if (dq.z > 0.5) iceT = Tsn;                                // v26k16: the test build's preview runs the river ice with the snow
        }
    }
    float Ts = Tsn - 4.0 * northZ;
    float nf = saturate(1.0 - abs(tposS - 10.0) / 0.67);
    [branch] if (nf > 0.0) {
        float Tdec = climAt(uv, 11).r - lapse - 1.2 * imp - 4.0 * northZ;
        Ts = lerp(Ts, min(Ts, 0.5 * (Ts + Tdec)), nf);
    }
    float snowT = smoothstep(2.0, -9.0, Ts);
    // v26k30: the lying snow is the rules' own per cell (thin / deep, the value the speed and the tile info use): none where they have none
    float covL = wxBits(wxB, 3.0, 2.0);
    float snowS = wxOn > 0.5 ? (covL > 1.5 ? 1.0 : (covL > 0.5 ? 0.62 : 0.0)) : snowT;
    float gateS = wxOn > 0.5 ? step(0.5, covL) : 1.0;
    float fieldF = farm * plainF * (1.0 - smoothstep(0.1, 0.5, castle));
    [branch] if (fieldF > 0.0)                                                                   // v26h (frame): every field weight is fieldF * ...
    {
        // winter wheat / barley: grey-green through the winter wherever winters are not too hard
        float wc = fieldF * (1.0 - G) * (1.0 - snowT) * smoothstep(-7.0, -4.0, Tmin) * smoothstep(0.30, 0.55, vnoise(wxz / 1100.0 + 91.0));
        col = float3(0.075, 0.095, 0.050) * lerp(0.85, 1.1, n3);
        a = wc * 0.6;
        M *= 1 - a; A = A * (1 - a) + col * a;
        // rapeseed in flower (Sichuan first, then the Yangtze; not in the cold north)
        float rape = rising * bell(T, 10.5, 2.5) * smoothstep(-2.0, 1.0, Tmin) * (1.0 - 0.5 * dry);
        a = rape * fieldF * 0.75 * patch2;
        col = float3(0.58, 0.44, 0.02) * lerp(0.8, 1.15, n3);
        M *= 1 - a; A = A * (1 - a) + col * a;
        // rice paddies just flooded (late spring, rice country)
        float flood = fieldF * smoothstep(0.0, 3.0, Tmin) * rising * bell(T, 21.0, 2.2) * (1.0 - patch2 * 0.5);
        col = float3(0.060, 0.085, 0.090);
        a = flood * 0.45;
        M *= 1 - a; A = A * (1 - a) + col * a;
        // ripe grain before the harvest
        float ripe = fieldF * cool * cool * smoothstep(14.0, 19.0, T) * (1.0 - smoothstep(26.0, 29.0, T));
        col = float3(0.30, 0.21, 0.04) * lerp(0.85, 1.1, n3);
        a = ripe * 0.5;
        M *= 1 - a; A = A * (1 - a) + col * a;
    }

    // ---- autumn: golden plains, red/orange/yellow mountains (not in the evergreen south) ----
    float af = cool * bell(T, 12.0, 6.0) * vegB * (1.0 - 0.85 * ev);
    float mtn = smoothstep(120.0, 650.0, alt);
    float3 gold = float3(0.30, 0.19, 0.03), brown = float3(0.20, 0.12, 0.04);
    float3 red = float3(0.27, 0.035, 0.012), orange = float3(0.34, 0.11, 0.015), yel = float3(0.33, 0.24, 0.02);
    float3 plainC = lerp(gold, brown, n2 * 0.7);
    float pick = n3 * 0.65 + n2 * 0.35;
    float3 mtnC = pick < 0.38 ? red : pick < 0.62 ? orange : yel;
    mtnC = lerp(mtnC, orange, far * 0.4);
    col = lerp(plainC, mtnC, mtn) * detail;
    a = af * lerp(0.35, 0.8, mtn);
    M *= 1 - a; A = A * (1 - a) + col * a;

    // ---- late autumn / winter dormancy: leafless woods on the slopes (plains already show dry straw) ----
    float dor = (1.0 - G) * vegB * (1 - snowT) * (1.0 - ev);
    [branch] if (dor > 0.0) {                                                                     // v25.3
    float twig = lerp(vnoise(wxz / 55.0 + 23.0), 0.5, smoothstep(40.0, 160.0, foot));
    float3 bareC = lerp(float3(0.085, 0.070, 0.058), float3(0.20, 0.17, 0.14), smoothstep(0.35, 0.70, twig));
    {
        float lodL = clamp(log2(max(foot, 1.0) / (1800.0 / 1024.0)), 0.0, 9.0);
        float3 litter = lin(TexGround.SampleLevel(sWrap, float3(wxz / 1800.0, 8), lodL).rgb);
        bareC = lerp(bareC, litter, 0.5);
    }
    col = lerp(float3(0.17, 0.12, 0.06), bareC, slopeF) * detail;
    a = dor * lerp(0.25, 0.60, slopeF);
    M *= 1 - a; A = A * (1 - a) + col * a;
    }

    // ---- v24: northwest (Liangzhou) winter: little snow, but frozen hard - grey frozen ground, frost, ice in the gullies,
    //      ice specks in the furrows, wind-blown snow streaks, ice streaks down shaded gorge walls ----
    float wf = nwZ * smoothstep(5.0, -3.0, T);
    [branch] if (wf > 0.01) {
        float lowA = 1.0 - smoothstep(1500.0, 2600.0, alt);                 // high mountains keep their normal snow
        float k = wf * lowA;
        float3 cold = float3(0.86, 0.90, 0.98) * 0.93;                       // frozen ground: the warm cast drains away
        M *= lerp(1.0.xxx, cold, 0.55 * k);
        A = lerp(A, dot(A, float3(0.3, 0.59, 0.11)) * float3(0.95, 0.98, 1.05), 0.5 * k);
        // frost: a fine silver-grey grain on the ground and dry grass
        float fg = lerp(vnoise(wxz / 22.0 + 5.0), 0.5, smoothstep(30.0, 120.0, foot));
        a = k * 0.30 * smoothstep(0.45, 0.70, fg + (vnoise(wxz / 300.0) - 0.5) * 0.4) * (1.0 - forestF * 0.6);
        col = float3(0.44, 0.47, 0.52);
        M *= 1 - a; A = A * (1 - a) + col * a;
        // ice along the gully floors and small streams
        float gul = smoothstep(25.0, 110.0, cav) * (1.0 - forestF) * smoothstep(0.40, 0.65, vnoise(wxz / 260.0 + 17.0));
        float iceN = vnoise(wxz / 90.0 + 3.0);
        col = lerp(float3(0.46, 0.57, 0.68), float3(0.72, 0.76, 0.80), smoothstep(0.35, 0.75, iceN));
        a = k * gul * 0.85;
        M *= 1 - a; A = A * (1 - a) + col * a;
        // ice specks in the field furrows
        a = k * farm * smoothstep(0.80, 0.90, lerp(vnoise(wxz / 38.0 + 9.0), 0.5, smoothstep(40.0, 150.0, foot))) * 0.6;
        M *= 1 - a; A = A * (1 - a) + float3(0.62, 0.68, 0.74) * a;
        // wind-blown snow: long streaks along the north-west wind, drifts on the lee (south-east facing) slopes
        float2 wdir = float2(0.7071, 0.7071);
        float along = dot(wxz, wdir), across = dot(wxz, float2(-wdir.y, wdir.x));
        float st = vnoise(float2(along / 1500.0, across / 240.0) + 23.0) * 0.7 + vnoise(float2(along / 700.0, across / 110.0) + 41.0) * 0.3;   // v26b: softer streaks
        st = lerp(st, 0.5, smoothstep(25.0, 90.0, foot));                  // v24 test2: streaks only up close (from afar they made a dark hatch)
        float lee = saturate(dot(nn.xz, wdir) * 3.0) * smoothstep(0.40, 0.70, vnoise(wxz / 400.0 + 8.0));
        float drift = (smoothstep(0.62, 0.80, st) * 0.45 + lee * 0.55) * smoothstep(1.0, -5.0, Ts) * gateS;   // v26k30: blown snow only on snow cells
        a = k * saturate(drift) * 0.85;
        M *= 1 - a; A = A * (1 - a) + float3(0.70, 0.72, 0.76) * a;
        // shaded gorge walls (Wudu): frozen water streaks hanging down the cliffs
        float wall = (1.0 - smoothstep(0.45, 0.72, ny)) * saturate(-southF * 2.0 + 0.3);
        float streak = smoothstep(0.88, 0.97, vnoise(float2(dot(wxz, tdirW(nn)) / 45.0, h / 400.0) + 51.0));
        a = nwZ * smoothstep(6.0, 0.0, T) * wall * streak * 0.7;
        M *= 1 - a; A = A * (1 - a) + float3(0.74, 0.78, 0.83) * a;
    }

    // ---- above the treeline: bare rock in every season (snow comes on top in the cold months) ----
    // v14: rock colour of the real local geology (loess, red sandstone, granite, limestone ...), with layered cliffs
    float expoR = saturate(above + (1.0 - vegB) * mtn * 0.5);
    [branch] if (above > 0.0 || expoR > 0.0) {                                                   // v25.3
    float4 rg = TexRegion.SampleLevel(sClamp, uv, 0);
    [branch] if (above > 0.0) {                                                                  // v26h (frame): the photo weight is 'above' - 0 means it is not read
    // rock surface photo of the local rock type: seen from above on gentle ground, from the side on cliffs
    float4 lw = TexLayer.SampleLevel(sClamp, uv, 0); lw /= max(dot(lw, 1.0.xxxx), 0.001);
    float4 dTri = TRIROCK(TexRock, wp3, triW, dp3x, dp3y, 2000.0);                       // v26b: triplanar, true derivatives
    float rl = dot(dTri, lw) * 2.0;
    // v26: the named mountains get their own rock photo (Taishan / Huangshan granite, Danxia red sandstone, Stone Forest moss,
    //      Huashan / Jianmen sheer faces, Taihang bands, loess walls, Qilian snow-dusted rock, Guilin limestone) and their own hue
    float3 tagHue = 1.0.xxx;
    {
        int2 tp = clamp(int2(uv * 512.0), 0, 511);
        float4 tg = TexTags.Load(int3(tp, 0));
        float tid = round(tg.r * 255.0), tw = TexTags.SampleLevel(sClamp, uv, 0).g;           // v26b: the weight bilinear - no blocky edge
        [branch] if (tid > 0.5 && tw > 0.01) {
            float4 r2 = TRIROCK(TexRock2, wp3, triW, dp3x, dp3y, 1900.0), r3 = TRIROCK(TexRock3, wp3, triW, dp3x, dp3y, 1900.0);
            float4 w2 = 0, w3 = 0;
            if (tid == 1.0 || tid == 2.0) { w2.b = 1.0; tagHue = float3(0.86, 0.92, 0.84); }                   // 석림, 장가계: mossy grey pillars
            else if (tid == 3.0 || tid == 12.0) { w3.a = 1.0; tagHue = float3(0.90, 0.92, 0.94); }             // 계림, 오대산: grey limestone
            else if (tid == 4.0 || tid == 8.0 || tid == 11.0 || tid == 14.0) { w2.a = 1.0; tagHue = float3(0.92, 0.92, 0.93); }   // 화산, 검문관, 무협, 아미산: sheer faces
            else if (tid == 5.0 || tid == 6.0 || tid == 7.0 || tid == 16.0) { w2.r = 1.0; tagHue = float3(1.02, 1.0, 0.98); }    // 태산, 황산, 무당산, 삼청산: light granite
            else if (tid == 9.0 || tid == 15.0) { w2.g = 1.0; tagHue = float3(1.25, 0.78, 0.62); }                              // 단하산, 무이산: red sandstone
            else if (tid == 10.0 || tid == 18.0) { w3.r = 1.0; tagHue = float3(0.98, 0.95, 0.90); }                             // 태항대협곡, 숭산: layered cliff bands
            else if (tid == 13.0) { w3.g = 1.0; tagHue = float3(1.12, 1.02, 0.80); }                                            // 황토고원: loess walls
            else if (tid == 17.0) { w3.b = 1.0; tagHue = float3(0.96, 0.98, 1.04); }                                            // 기련산: snow-dusted rock
            float rl2 = (dot(r2, w2) + dot(r3, w3)) * 2.0;
            rl2 = 1.0 + (rl2 - 1.0) * 1.6;                                                  // v26b: the site photo shows clearly
            tw = smoothstep(0.0, 0.35, tw);
            rl = lerp(rl, rl2, tw); tagHue = lerp(1.0.xxx, tagHue, tw);
        }
    }
    rl = lerp(clamp(rl, 0.30, 1.75), 1.0, smoothstep(350.0, 900.0, foot));                 // v26b: keeps its texture a little further out
    float lay = sin(h / 34.0 + (vnoise(wxz / 700.0) - 0.5) * 9.0 + vnoise(wxz / 2500.0 + 4.0) * 6.0);
    float strata = smoothstep(-0.3, 0.5, lay) * rg.a * (1.0 - smoothstep(0.80, 0.95, ny));   // layers show on steep faces
    float3 rockC = rg.rgb * tagHue * lerp(0.82, 1.12, n3) * rl;
    rockC = lerp(rockC, rockC * float3(0.80, 0.86, 0.92) * 0.80, strata * 0.35);
    rockC *= lerp(1.0, 0.90, imp);
    a = above * lerp(0.80, 0.86, imp);
    M *= 1 - a; A = A * (1 - a) + rockC * a;
    }
    {   // whatever bare ground still shows through takes the local rock hue (no more glaring white-grey)
        float expo = expoR;
        float3 hue = rg.rgb / max(dot(rg.rgb, float3(0.3, 0.59, 0.11)), 0.05);
        M *= lerp(1.0.xxx, hue * 0.82, 0.45 * expo); A *= lerp(1.0.xxx, hue * 0.82, 0.45 * expo);
    }
    }

    // ---- v24: thaw - wet dark ground right after the snow has gone (only where winters are snowy) ----
    {
        float TminS = Tmin - 4.0 * northZ;
        float wet = rising * smoothstep(0.5, -4.0, TminS) * smoothstep(-4.0, 1.0, Ts) * (1.0 - smoothstep(4.0, 9.0, Ts)) * (1.0 - 0.7 * nwZ);
        wet *= (1.0 - above) * smoothstep(0.55, 0.85, ny);
        M *= lerp(1.0.xxx, float3(0.78, 0.80, 0.85), wet * 0.8); A *= lerp(1.0.xxx, float3(0.78, 0.80, 0.85), wet * 0.8);
    }

    // ---- snow ----
    float s = snowS * (1.0 - smoothstep(0.4, 0.9, castle) * 0.15) * smoothstep(0.50, 0.86, ny);   // v26b: no snow on near-vertical faces   // v24 test2: city grounds get snow too (only a little trodden)
    s *= wxOn > 0.5 ? 1.0 : lerp(1.0, 0.45, nwZ * (1.0 - max(smoothstep(300.0, 900.0, alt), max(hillC, imp))));   // v26k30: with the rules the cell's own thin / deep decides   // v24: dry northwest plains: thin snow only (v26k12: its foothills, hills and highlands get the full cover)
    s += rising * (0.16 * saturate(-southF) - 0.18 * saturate(southF)) * step(0.02, snowS);   // v24: melts first on sunny slopes
    float3 sn = lin(TexSnow.Sample(sWrap, wxz / 3200.0).rgb);
    float snl = dot(sn, float3(0.3, 0.59, 0.11));
    float cover = saturate((s * 1.3 - 0.12 - (n2 * 0.6 + n3 * 0.4) * 0.5 + (snl - 0.45) * 0.4) * 2.4) * gateS;
    [branch] if (gateS > 0.5 && (cover > 0.0 || (farm * rising * snowS > 0.0 && foot < 90.0))) {                // v25.3
    // v24: patchy edges taken from the melting-snow photo; field furrows keep strips of snow while it melts
    float lodP = clamp(log2(max(foot, 1.0) / (1500.0 / 1024.0)), 0.0, 9.0);
    float3 p15 = lin(TexGround.SampleLevel(sWrap, float3(wxz / 1500.0, 9), lodP).rgb);
    float m15 = smoothstep(0.25, 0.60, dot(p15, float3(0.3, 0.59, 0.11)));
    float edgeS = saturate(1.0 - abs(cover * 2.0 - 1.0));
    cover = saturate(cover + (m15 - 0.5) * 0.9 * edgeS);
    float furrow = smoothstep(0.55, 0.85, sin(dot(wxz, float2(0.8, 0.6)) / 28.0) * 0.5 + 0.5) * (1.0 - smoothstep(30.0, 90.0, foot));
    cover = max(cover, farm * rising * furrow * saturate(snowS * 2.5) * 0.85);
    col = lerp(float3(0.62, 0.64, 0.68), sn * 1.05, 0.55);
    col = lerp(col, p15 * 1.08, edgeS * 0.45 * rising);
    float thin = wxOn > 0.5 ? (covL < 1.5 ? 1.0 : 0.0) : cool * (1.0 - smoothstep(0.35, 0.70, snowT));   // v26k30: 얕은 눈 lets the ground show through                     // v24: first snow is thin - the ground shows through
    a = cover * (1.0 - 0.40 * thin);
    M *= 1 - a; A = A * (1 - a) + col * a;
    }

    // ---- frozen river banks / islands: same ice as the water surface, reaching onto the shore ----
    float fm = TexFreeze.SampleLevel(sClamp, wxz / 204800.0, 0).g;
    if (fm > 0.002) {
        float Ti = iceT - 2.0 * northZ - 2.0 * rising * (1.0 - smoothstep(4.0, 8.0, iceT));     // v24: 2 C colder north of the Yellow River                                   // ice lingers into early spring
        float fr = fm * smoothstep(1.5, -5.0, Ti);
        if (wxOn > 0.5) fr = wxBits(wxB, 5.0, 1.0) > 0.5 ? max(fr, fm * 0.9) : 0.0;   // v26k30: ice exactly on the cells the rules froze
        float ni = vnoise(wxz / 900.0) * 0.55 + vnoise(wxz / 230.0 + 3.1) * 0.30 + vnoise(wxz / 60.0 + 7.7) * 0.15;
        float ci = saturate(((fr - 0.12) / 0.18 * 1.2 - ni) * 8.0);   // solid sheet once cold enough
        float si = vnoise(wxz / 140.0 + 11.0) * 0.6 + vnoise(wxz / 35.0 + 5.0) * 0.4;
        col = lerp(float3(0.36, 0.46, 0.56), float3(0.74, 0.77, 0.81), smoothstep(0.40, 0.75, si));
        col = lerp(col, float3(0.74, 0.77, 0.81), 0.35);             // banks: more snow on the ice
        a = ci;
        M *= 1 - a; A = A * (1 - a) + col * a;
    }

    // ======================================================================================================
    // v26: disasters and weather. The rules module writes straight into the graphics tool's number list:
    //   100   x frame counter (d3dx.ini), y rain picture level 0..3, z 26, w 1 = weather data valid
    //   101.. 81 weather-node temperature anomalies (C)      122.. 81 node rain multipliers
    //   150.. 55x55 rain grid (4x4 cells): v = wet% + tier*1000 + snowing*10000 + cracked*100000
    //   1000.. per-cell state, 5 cells a float (4 bits each): 1 burning, 2 fresh ash, 3 old ash, 4/5/6 flood (normal/big/great),
    //          7 landslide (also a slot), 9 drought cracks
    //   60..95 the 16 picture slots (landslide, earthquake; and fire / flood when the direct link is off: key presses)
    // ======================================================================================================
    float4 ip2 = IniParams.Load(int2(2, 0));
    float4 dlk = IniParams.Load(int2(60, 0));                // the direct link (w = 1 while the rules module writes the slots itself)
    float dlon = dlk.w > 0.5 ? 1.0 : 0.0, dlbank = floor((dlk.z + 0.5) / 256.0);
    float tm = ip2.y, evn = dlon > 0.5 ? dlk.z - dlbank * 256.0 : ip2.w;
    float tcur = (clamp(round(ip.x), 1.0, 12.0) - 1.0) * 3.0 + clamp(round(ip1.x), 0.0, 2.0);   // this turn, 0..35 (same count as the module)
    float CW = CW_, CH = CH_; float2 C0 = C0_;              // v26k30: centre of cell (0,0) on the game's own grid (gridInit)
    float2 wp = wxz;
    float wOn = (wctl.w > 0.5 && abs(wctl.z - 26.0) < 0.5 && dlon > 0.5) ? 1.0 : 0.0;
    float rLevel = wOn > 0.5 ? clamp(round(wctl.y), 0.0, 3.0) : 0.0;
    // ---- the hex under this pixel: nearest cell centre, its index, position inside it, the neighbour across the nearest edge ----
    float c0 = floor((wp.x - C0.x) / CW);
    float2 cc = 0; float bestD = 1e12; float ccol = 0, crow = 0;
    [unroll] for (int kq = 0; kq < 2; kq++) {
        float col1 = c0 + kq; float codd = col1 - floor(col1 * 0.5) * 2.0;
        float row1 = round((wp.y - C0.y) / CH + 0.5 * codd);
        float2 cand = C0 + float2(col1 * CW, (row1 - 0.5 * codd) * CH);
        float dd = dot(wp - cand, wp - cand);
        if (dd < bestD) { bestD = dd; cc = cand; ccol = col1; crow = row1; }
    }
    float inMap = (ccol >= 0.0 && ccol < 220.0 && crow >= 0.0 && crow < 220.0) ? 1.0 : 0.0;
    float cellIdx = clamp(crow, 0.0, 219.0) * 220.0 + clamp(ccol, 0.0, 219.0);
    float2 dl = wp - cc;
    float2 rl = dl / (CW / 1.5);                             // in units of the hex radius
    float hexn = max(abs(rl.y) * 1.1547, abs(rl.x) + abs(rl.y) * 0.57735);   // flat-top hexagon, 1 at the edge
    float sx = dl.x < 0.0 ? -1.0 : 1.0, sy = dl.y < 0.0 ? -1.0 : 1.0;
    float2 nA = float2(0.0, sy), nB = float2(0.8660254 * sx, 0.5 * sy);
    float2 nE = dot(dl, nA) > dot(dl, nB) ? nA : nB;         // direction to the neighbour across the nearest edge
    float2 ncc = cc + nE * CH;                               // its centre (approximate for the diagonal ones, exact enough for the state lookup)
    float ncol = round((ncc.x - C0.x) / CW); float nodd = ncol - floor(ncol * 0.5) * 2.0; float nrow = round((ncc.y - C0.y) / CH + 0.5 * nodd);
    float ncellIdx = clamp(nrow, 0.0, 219.0) * 220.0 + clamp(ncol, 0.0, 219.0);
    float st = 0.0, nst = 0.0;
    [branch] if (wOn > 0.5 && inMap > 0.5) { st = cellState(cellIdx); nst = cellState(ncellIdx); }
    float wob = 0.0;                                                                          // v26h (frame): read only where a cell picture is drawn
    [branch] if ((st >= 0.5 && st <= 3.5) || (st > 6.5 && st < 7.5) || (st > 8.5 && st < 9.5) || (st > 12.5 && st < 14.5) || evn > 0.5) wob = (vnoise(wp / 120.0 + 7.0) - 0.5) * 0.3;
    float vegF = saturate(veg * 1.6 + 0.25) * (1.0 - above * 0.7);
    // ---- per-cell: fire, ash ----
    [branch] if (st >= 0.5 && st <= 3.5) {
        float incell = smoothstep(1.15, 0.85, hexn + wob);
        float burntOut = st > 1.5 ? 1.0 : 0.0; float fade = st < 1.5 ? 1.0 : (st < 2.5 ? 0.9 : 0.45);
        drawFire(A, M, wp, dl, hexn, incell, vegF, foot, tm, burntOut, fade, st < 2.5 ? 1.0 : 0.0);
    }
    // ---- per-cell: flood water (the cell's own state, and the neighbour's for the shared edge) ----
    // v26k7: the river travels with the cell state - 4..6 a blue flood (Yangtze, Han), 10..12 the Yellow River's muddy flood
    //        (the colour no longer comes from the hazard texture, so the new riverbank cells get the right one)
    float stF = (st >= 9.5 && st <= 12.5) ? st - 6.0 : st, nstF = (nst >= 9.5 && nst <= 12.5) ? nst - 6.0 : nst;
    [branch] if (stF >= 3.5 && stF <= 6.5) {                // v26k30: water only inside a flooded cell (none lapping into the dry cell beside it)
        float wet = (stF >= 3.5 && stF <= 6.5) ? 1.0 : 0.0, nwet = (nstF >= 3.5 && nstF <= 6.5) ? 1.0 : 0.0;
        float L = max(stF, nstF) - 3.0;                      // 1 normal, 2 big, 3 great
        float yel = (st >= 9.5 || (wet < 0.5 && nst >= 9.5)) ? 1.0 : 0.0;
        drawFlood(A, M, wp, dl, hexn, wet, nwet, L, yel, foot, tm, above, hazAt(uv).r);
    }
    // ---- per-cell: drought cracks ----
    [branch] if (st > 8.5 && st < 9.5) {
        // v26b (bold, asked for): the whole cell a pale bleached clay floor broken into a few big plates by wide black fissures;
        //       the fissures keep at least three screen pixels of width however far the camera is pulled back, and from very far
        //       the cell stays a pale, dark-veined patch - "the ground cracked from drought" reads at a glance
        float px = max(foot, 1.0);
        float plateS = 230.0 * max(1.0, px / 6.0);                                        // v26f: plates never under ~38 screen pixels - the net looks the same at every zoom
        float lodD = smoothstep(260.0, 520.0, px);                                        // v26e: the same picture at every zoom; only the whole-map view softens it
        float2 g = wp / plateS; float2 gi = floor(g), gf = frac(g);
        float f1 = 9.0, f2 = 9.0;
        [unroll] for (int oy = -1; oy <= 1; oy++) [unroll] for (int ox = -1; ox <= 1; ox++) {
            float2 o = float2(ox, oy); float2 pcen = o + 0.1 + float2(hash21(gi + o + 5.0), hash21(gi + o + 13.0)) * 0.8;
            float d = length(gf - pcen); if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d;
        }
        float edge = (f2 - f1) * plateS;                                                  // ~ distance to the plate boundary
        float cwid = max(0.07 * plateS, 1.3 * px);                                        // v26f: fissures a fixed share (~14%) of the plate, so pulled back they do not fatten into a dark mass
        float fis = smoothstep(cwid, cwid * 0.45, edge);
        float rim = smoothstep(cwid * 2.4, cwid * 1.05, edge) * (1.0 - fis);              // the plate edges curl up: a darker rim
        float lodC = clamp(log2(px / (900.0 / 1024.0)), 0.0, 9.0);
        float3 crk = lin(TexGround.SampleLevel(sWrap, float3(wp / 900.0, 14), lodC).rgb);
        float lum = saturate(dot(crk, float3(0.3, 0.59, 0.11)) / 0.22);
        // v26e: the plates keep the ground's own colour (farmland stays brown, dry grass stays straw) - dried, dusty, a little
        //       paler - with the photo's grain on them; only the fissures are painted (black up close, fainter from afar).
        //       No fixed pale plate colour any more: that read as white tiles with black grout.
        float3 dusty = lerp(A, dot(A, float3(0.3, 0.59, 0.11)).xxx, 0.40) * float3(1.08, 1.02, 0.86) + float3(0.015, 0.012, 0.008);
        float3 plate = dusty * (0.88 + 0.20 * lum) * (0.92 + 0.16 * vnoise(wp / 520.0 + 41.0));      // stays the ground's colour, just dried
        plate *= 1.0 - 0.25 * rim * (1.0 - lodD);
        float3 cr = lerp(plate, float3(0.012, 0.010, 0.008), fis * lerp(1.0, 0.6, lodD));
        float inc = smoothstep(1.32, 0.75, hexn + wob * 2.0);                            // v26c: soft, wide cell edge: neighbouring cells merge, a lone cell is a blot, not a hexagon
        float ca = inc * (1.0 - above) * (1.0 - forestF * 0.85) * smoothstep(0.50, 0.80, ny) * lerp(0.97, 0.80, lodD);
        ca *= 1.0 - saturate(cover * 1.6);                                               // v26d: not on snow-covered ground (the cracks are under the snow)
        M *= 1 - ca; A = A * (1 - ca) + cr * ca;
    }
    // ---- v26k30b: per-cell mud where the flood water has just gone (13 the first turn, 14 drying the second) - inside the cell only ----
    [branch] if (st > 12.5 && st < 14.5) {
        float fresh = st < 13.5 ? 1.0 : 0.0;
        float nmud = (nst > 12.5 && nst < 14.5) ? 1.0 : 0.0;
        float incM = nmud > 0.5 ? 1.0 : smoothstep(1.0, 0.82, hexn + max(wob, 0.0));          // a soft rim inside the cell, none toward a muddy neighbour
        float mn1 = vnoise(wp / 190.0 + 3.0), mn2 = vnoise(wp / 47.0 + 9.0), mn3 = vnoise(wp / 13.0 + 21.0);
        float3 mudW = lerp(float3(0.070, 0.050, 0.034), float3(0.115, 0.084, 0.055), mn2 * 0.7 + mn3 * 0.3);   // soaked dark silt
        float3 mudD = lerp(float3(0.150, 0.118, 0.080), float3(0.205, 0.165, 0.115), mn2 * 0.7 + mn3 * 0.3);   // drying, paler
        float3 mudC = lerp(mudD, mudW, fresh);
        float pud = smoothstep(0.60, 0.70, mn1) * lerp(0.35, 1.0, fresh) * (1.0 - smoothstep(300.0, 900.0, foot));   // standing water in the hollows
        mudC = lerp(mudC, float3(0.035, 0.045, 0.055) + 0.04 * mn3, pud);
        float aM = incM * (1.0 - above) * lerp(0.62, 0.85, fresh) * (1.0 - forestF * 0.4);
        M *= 1 - aM; A = A * (1 - aM) + mudC * aM;
    }
    float shakenDone = 0.0;
    // ---- v26k38c: a landslide from the per-cell state (7) with the direct link - every blocked cell shows, however many (the 16 slots
    //      ran out when a great quake brought down 10 roads); the same picture as the slot drew (inside its own cell). One call after
    //      the slots for both ways (the slot one is only for the key-press way), so the shader holds the picture once.
    float slideHere = (st > 6.5 && st < 7.5) ? 1.0 : 0.0;
    // ---- the picture slots: earthquakes always; landslide / fire / flood only when the direct link is off (key presses) ----
    float4 hzP = 0, hzC = 0; float hzDone = 0;
    [branch] if (evn > 0.5)
    [loop] for (int si = 0; si < 16; si++) {
        if ((float)si >= evn) break;
        float v, chk;
        [branch] if (dlon > 0.5) { v = round(IniParams.Load(int2(64 + (int)dlbank * 16 + si / 4, 0))[si % 4]); chk = -1.0; }
        else { float4 evq = IniParams.Load(int2(3 + si / 4, 0)), ecq = IniParams.Load(int2(7 + si / 4, 0)); v = round(evq[si % 4]); chk = round(ecq[si % 4]); }
        if (v < 1.0) continue;
        if (chk >= 0.0) {
        float ds = v + si * 7919.0; ds -= floor((ds + 4986.5) / 9973.0) * 9973.0; if (ds < -0.5) ds += 9973.0;
        if (abs(ds - chk) > 0.5) continue; }
        float code = floor((v + 0.5) / 48400.0), cell = v - code * 48400.0;
        float ty = floor((code + 0.5) / 36.0), nb = code - ty * 36.0;
        if (ty == 1.0 || ty == 3.0 || ty == 5.0) { float ag = tcur - nb; if (ag < 0.0) ag += 36.0; if (ag > 30.0) ag = 0.0; nb = ag; }
        if (wOn > 0.5 && (ty == 1.0 || ty == 2.0 || ty == 3.0 || ty == 5.0)) continue;   // drawn from the per-cell state instead
        if (ty == 2.0) {
            if (hzDone < 0.5) { hzP = hazAt(uv); hzC = hazAt(cc / MAPW); hzDone = 1; }
            float rid = hazRiverId(uv);
            if (abs(rid - cell) > 0.5) continue;
            if (rid == 2.0 && (uv.x * 16.0 < 6.8 || uv.x * 16.0 > 10.5)) continue;
            float big = nb, L = big > 0.5 ? 0.45 : 0.20;
            float wet = (hzC.r <= L && hzC.g > 0.15) ? 1.0 : 0.0;
            float nwet = 0.0;
            [branch] if (hexn > 0.55) { float4 hzN = hazAt(ncc / MAPW); nwet = (hzN.r <= L && hzN.g > 0.15) ? 1.0 : 0.0; }
            drawFlood(A, M, wp, dl, hexn, wet, nwet, big > 0.5 ? 2.0 : 1.0, abs(cell - 1.0) < 0.5 ? 1.0 : 0.0, foot, tm, above, hzP.r);
            continue;
        }
        float col = cell - floor((cell + 0.5) / 220.0) * 220.0, row = floor((cell + 0.5) / 220.0);
        float odd = col - floor(col * 0.5) * 2.0;
        float2 cw = C0 + float2(col * CW, (row - 0.5 * odd) * CH);
        float2 rel = wp - cw;
        float dist = length(rel);
        if (dist > (ty == 4.0 ? 12500.0 : 3000.0)) continue;                         // v26k30b: a great quake shakes cells up to ~11000 out
        float2 rn = rel / (CW / 1.5);
        float hexs = max(abs(rn.y) * 1.1547, abs(rn.x) + abs(rn.y) * 0.57735);
        float incs = smoothstep(1.15, 0.85, hexs + wob);
        if (ty == 3.0 && abs(cell - cellIdx) > 0.5) continue;                       // v26k30: a landslide stays inside its own cell
        if (ty == 4.0 && wxOn > 0.5 && wxBits(wxB, 6.0, 1.0) < 0.5) continue;        // v26k30: the quake's cracks only on the cells the rules shook
        if (ty == 1.0 || ty == 5.0) {
            float fade = ty == 1.0 ? 1.0 : saturate(1.0 - nb / 5.0);
            drawFire(A, M, wp, rel, hexs, incs, vegF, foot, tm, ty == 5.0 ? 1.0 : 0.0, fade, (ty == 1.0 || nb < 3.0) ? 1.0 : 0.0);
        } else if (ty == 3.0) {
            slideHere = 1.0;                                                         // its own cell (checked above): cw = cc, rel = dl
        } else if (ty == 4.0) {
            [branch] if (wxOn > 0.5 && shakenDone < 0.5) {                          // v26k30b: every shaken cell shows it (the fault line alone covered only part)
                shakenDone = 1.0;
                float nq = wxBits(cellWx(ncellIdx), 6.0, 1.0);
                drawShaken(A, M, wp, floor((nb + 0.5) / 12.0), foot, nq > 0.5 ? 0.0 : hexn);
            }
            drawQuake(A, M, wp, cw, rel, nb, foot, uv);
        }
    }
    [branch] if (slideHere > 0.5) drawSlide(A, M, wp, cc, dl, hexn, wob, foot, uv);
    // ---- the weather picture: wet ground, drifting rain clouds with curtains beneath (pulled back), thin streaks (up close) ----
    [branch] if (rLevel > 0.5 && inMap > 0.5) {
        float4 R = R0;                                       // tier 0..3, wet 0..1, snowing 0..1, cracked 0..1 (v26h: read once above)
        float tn = (vnoise(wp / 2600.0 + float2(tm * 0.02, 0.0)) - 0.5) * 0.3;   // v26k: a small edge wobble only - no wet ground or shadow where the rules send no rain
        float tierE = R.x;                                   // v26k30: this cell's own rain tier (the rules'), no edge wobble: rain exactly on the rain cells
        float tierCl = tierG + tn;                           // the cloud (and its shadow) keeps the rain grid's soft outline
        float rainI = smoothstep(0.35, 0.95, tierE);
        float strength = saturate((tierE - 0.6) / 2.0);
        float snowing = smoothstep(0.3, 0.7, R.z);
        float landF = (1.0 - above * 0.5) * (fm > 0.002 ? 0.6 : 1.0);
        // v26d: the ground itself is left almost alone (a painted ground traps the rain inside the ground picture, and a blue
        //       wash read as snow); the rain lives in the air: streaks, haze, clouds, lightning. Only a faint wet darkening below.
        // 1) wet ground: darker, a touch colder; soaked plains show dark mud up close
        float wetG = saturate(R.y * 1.3 * step(0.5, R.x) + rainI * 0.5 * (1.0 - snowing)) * landF * (1.0 - snowS);   // v26k30: wet only while it rains on this cell
        [branch] if (wetG > 0.01) {
            float3 wetC = float3(0.72, 0.74, 0.80);
            M *= lerp(1.0.xxx, wetC, wetG * 0.4); A *= lerp(1.0.xxx, wetC, wetG * 0.4);
        }
        // 1b) v26k6: falling snow dusts the ground while it falls - snow 14%, a snowstorm 36% (sleet none); the lying snow of the
        //     turn (the approved tables) is drawn elsewhere and untouched
        [branch] if (snowing > 0.01 && rainI > 0.01) {
            float sSleet = 1.0 - smoothstep(1.3, 1.7, tierE), sStorm = smoothstep(2.3, 2.7, tierE);
            float dust = snowing * rainI * (1.0 - sSleet) * lerp(0.14, 0.36, sStorm) * (1.0 - above * 0.5);
            float3 snowC = float3(0.80, 0.82, 0.86);
            M *= 1 - dust; A = A * (1 - dust) + snowC * dust;
        }
        // v26d: the rain haze, the drops and the lightning are drawn over the whole picture by SeasonRain.hlsl (d3dx.ini)
        // 2) v26k: the shadow of the rain cloud on the land - 15% at most, exactly under the cloud SeasonRain.hlsl draws
        //    pulled back (the same outline: the rain's footprint a little past its rim, cut by the same drifting lumps).
        //    Up close there is no cloud, so no shadow (asked for: 90% transparent / nothing painted on the ground).
        // v26k38b2: under exactly the cloud SeasonRain.hlsl draws now - the rain cells' own footprint (no clear gaps inside, lighter
        //           and darker lumps), set off a cell toward the south-east (the sun is north-west), nothing past a third of a cell
        [branch] if (rLevel > 1.5 && tierG > 0.001) {                             // the rain grid (max of each 4x4 block) reaches 4 cells round any rain cell
            float farF = smoothstep(2.2, 6.0, foot);                                // with the cloud - every zoom step but the closest
            [branch] if (farF > 0.01) {
                float2 qs = wp + float2(-700.0, 700.0);                                   // the cloud over the north-west casts it here
                float cov = rainCover(qs);
                [branch] if (cov > 0.01) {
                    float2 q = qs + float2(tm * 45.0, tm * 28.0);
                    float D = fbm2(q / 3200.0 + 1.0) * 0.68 + billow(q / 1150.0 + 2.0) * 0.22 + 0.075;   // the cloud's lumps (cloudDens, two sizes)
                    float e = saturate(cov + (D - 0.62) * 4.8 * cov * (1.0 - cov));
                    float lump = smoothstep(0.50, 0.75, D);
                    float3 cloudC = lerp(float3(0.070, 0.072, 0.080), float3(0.10, 0.10, 0.11), snowing);   // a shadow on the land
                    float aw1 = e * (0.12 + 0.10 * lump) * farF;
                    M *= 1 - aw1; A = A * (1 - aw1) + cloudC * aw1;
                }
            }
        }
        // v26g: the splashes are drawn by SeasonRain.hlsl (by the slope of the ground, hidden behind what stands in front)
    }

    // ---- v26k19: rock relief on the mountain flanks - the picture side of the fine relief (the height grid has only 7-8 points a
    //      tile, so the grain finer than that is drawn): real rock scans, side-on on the flanks (vertical erosion grooves), from
    //      above on gentler mountain ground; lit from the game's sun (north-west), creases darker. Strength = the regional level
    //      1..10 (TexTags.b = level / 10, 0 off the mountains), fading out as the view pulls back.
    {
        float lvl = TexTags.Load(int3(clamp(int2(uv * 512.0), 0, 511), 0)).b;
        float nearR = 1.0 - smoothstep(450.0, 1300.0, foot);                     // v26k21: kept out to the usual playing distance
        [branch] if (lvl > 0.02 && nearR > 0.01) {
            float steepR = 1.0 - smoothstep(0.50, 0.92, ny);
            float2 tdR = tdirW(nn);
            float2 dnR = normalize(nn.xz + float2(0.0001, 0.0));                  // downhill
            float lodR = clamp(log2(max(foot, 1.0) / (1100.0 / 1024.0)), 0.0, 10.0);
            float4 rS = TexRelief.SampleLevel(sWrap, float2(dot(wxz, tdR), -h * 1.6) / 1100.0, lodR);
            float4 rT = TexRelief.SampleLevel(sWrap, wxz / 1300.0, lodR);
            float lodR2 = clamp(log2(max(foot, 1.0) / (3000.0 / 1024.0)), 0.0, 10.0);
            float4 rS2 = TexRelief.SampleLevel(sWrap, float2(dot(wxz, tdR), -h * 1.6) / 3000.0 + 0.37, lodR2);   // v26k21: a second, bigger grain
            float4 rT2 = TexRelief.SampleLevel(sWrap, wxz / 3400.0 + 0.61, lodR2);
            float2 pS = (rS.rg * 2.0 - 1.0) * 0.65 + (rS2.rg * 2.0 - 1.0) * 0.65, pT = (rT.ba * 2.0 - 1.0) * 0.65 + (rT2.ba * 2.0 - 1.0) * 0.65;
            float2 tilt = lerp(float2(pT.x, -pT.y), tdR * pS.x - dnR * pS.y * 0.8, steepR);
            float shR = dot(tilt, float2(-0.6, -0.8));                             // + turned to the sun, - away from it
            float crease = saturate(cav / 45.0) - 0.5 * saturate(-cav / 45.0);    // gullies darker, edges a little lighter
            float kR = lvl * nearR * (0.50 + 0.50 * steepR);
            float fR = clamp(1.0 + kR * (1.35 * shR - 0.65 * crease), 0.30, 1.65);   // v26k21: stronger (asked for: clearly visible, not faint)
            A *= fR; M *= fR;
        }
    }
    // v26k44: poison swamp (독늪) stays exactly the game's own ground - the game paints these cells purple, a sign the player needs
    //         on the map. Everything the season picture drew on them (ground photos, plants, snow, rock colour, and also the weather
    //         and disaster marks: cloud shadow, wet ground, flood water, fire) is taken off again. Rain, snow and clouds in the air
    //         (SeasonRain) are unchanged; the rules (speed, damage) are unchanged. Footprint = the game's own swamp ground
    //         (season_forest.dds green, from the terrain data); outside it the value is exactly 0 and this is skipped.
    float swampK = TexForest.SampleLevel(sClamp, uv, 0).g;
    [branch] if (swampK > 0.002) { swampK = smoothstep(0.15, 0.75, swampK); A *= 1.0 - swampK; M = lerp(M, 1.0.xxx, swampK); }
}

