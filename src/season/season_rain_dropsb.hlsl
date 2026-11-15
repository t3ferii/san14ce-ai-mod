// 4계절 패치 (wprkfdid12-ux) SeasonRainDropsB.hlsl v26k52, vendored unchanged for AI_TacticsMod (free use). Drawn by native_season_bake.inl after the game's lighting pass.
// v26k38b: LAYOUT B - made from SeasonRainDrops.hlsl by gen_rainB.py (the same picture) for the game's other lighting pass 6642ae48461b1425
//   (used whenever lit points are on screen - towns, fires; the weather vanished there): its depth is t5, its constants
//   carry the camera at [16] and clip -> world at [26..29] (three rows later than 70b6fa39daa0880e).
// SAN14 season map v26k13 (no camera angle without rain/snow; v26k: zoom scale and rain-system edges; v26j: geometry drops): the falling drops, the snowflakes and the splashes as real geometry (one small quad each), drawn right
// v26k30: the game's own hex grid; a drop / flake falls only onto a rain cell, with that cell's strength
// after the game's lighting (post of the water-layer lighting pass 70b6fa39daa0880e, after SeasonRain.hlsl's haze), with the
// game's own depth test, so a drop vanishes exactly where a wall, a roof, a ridge or a horse stands in front of it.
// Every drop belongs to a cell of a grid fixed on the ground (no sliding when the camera pans). The screen is cut into
// 192 x 108 tiles; each tile draws the drops of the ground cell whose centre falls on it (one owner per cell), so the work is
// the same at every zoom and the picture is the same at every zoom: lines of a fixed size on the screen - up close one line is
// one drop, from afar one line stands for a whole shower over several tiles (the curtains under the clouds), and at the
// whole-map view nothing is drawn at all (the ground keeps its faint cloud shadow). Colours are taken relative to the lit
// picture around the pixel (a copy of it, d3dx.ini): lighter and greyer than ground, grass and dark roofs, darker than snow,
// pale rock and white walls - never a fixed brightness, so no exposure can turn them into white blobs.
Texture2D<float4> Depth : register(t5);        // the lighting pass's depth copy (d3dx.ini: bound for the vertex shader too)
Texture1D<float4> IniParams : register(t120);
#ifdef VERTEX_SHADER
cbuffer Lit : register(b13) { float4 cb0[34]; }   // the lighting pass's pixel constants (d3dx.ini: vs-cb13 = ps-cb0): [13] camera, [23..26] clip -> world
#else
cbuffer Lit : register(b0) { float4 cb0[34]; }    // the same buffer as the pixel shader sees it
#endif
Texture2D<float4> Scene : register(t101);      // copy of the lit picture (d3dx.ini: ResourceSAN14Scene)

static const float MAPW = 204800.0;
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
    CW_ = g.z; CH_ = g.w * 1.7320508; C0_ = float2(g.x, g.y + CH_ * 0.5);
}
float2 sanSH()                                       // v26k48: how far a map mod moved the old land (list 3420 against the game's own grid; 0 without one)
{
    float4 g = IniParams.Load(int2(3420, 0));
    float2 s = (g.z > 500.0 && g.z < 900.0 && g.w > 300.0 && g.w < 600.0) ? float2(g.x - 37594.0, g.y - 13335.2998) : float2(0.0, 0.0);
    return (abs(s.x) < 1.0 && abs(s.y) < 1.0) ? float2(0.0, 0.0) : s;
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
static const uint NX = 192, NY = 108, NC = 2;   // screen tiles, candidates per ground cell (d3dx.ini: draw = 192*108*2*6 = 248832)
                                                // v26k29: 2 candidates (was 3): fewer drops, the strengths kept apart by the density factors below

float hash21(float2 p) { p = frac(p * float2(123.34, 456.21)); p += dot(p, p + 45.32); return frac(p.x * p.y); }
float vnoise(float2 p)
{
    float2 i = floor(p), f = frac(p); float2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i), b = hash21(i + float2(1, 0)), c = hash21(i + float2(0, 1)), d = hash21(i + float2(1, 1));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}
float4 rainCellRaw(float i, float j)
{
    float idx = clamp(j, 0.0, 54.0) * 55.0 + clamp(i, 0.0, 54.0);
    float4 v4 = IniParams.Load(int2(150 + (int)floor((idx + 0.5) / 4.0), 0)); float v = v4[(int)(idx - floor((idx + 0.5) / 4.0) * 4.0)];
    float cr = floor(v / 100000.0 + 1e-4); v -= cr * 100000.0;
    float sn = floor(v / 10000.0 + 1e-4); v -= sn * 10000.0;
    float tier = floor(v / 1000.0 + 1e-4); v -= tier * 1000.0;
    return float4(tier, saturate(v / 100.0), sn, cr);
}
float4 rainAt(float2 wp)
{
    float gx = (wp.x - C0_.x) / (CW_ * 4.0) - 0.5, gy = (wp.y - C0_.y) / (CH_ * 4.0) - 0.5;
    float x0 = floor(gx), y0 = floor(gy); float fx = gx - x0, fy = gy - y0;
    float4 a = rainCellRaw(x0, y0), b = rainCellRaw(x0 + 1.0, y0), c = rainCellRaw(x0, y0 + 1.0), d = rainCellRaw(x0 + 1.0, y0 + 1.0);
    return lerp(lerp(a, b, fx), lerp(c, d, fx), fy);
}
float3 worldAt(float2 ndc, float d)
{
    float4 hw = ndc.x * cb0[26] + ndc.y * cb0[27] + d * cb0[28] + cb0[29];
    return hw.xyz / (abs(hw.w) > 1e-20 ? hw.w : 1e-20);
}
float4x4 inv4(float4x4 m)
{
    float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3];
    float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3];
    float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3];
    float a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3];
    float b00 = a00 * a11 - a01 * a10, b01 = a00 * a12 - a02 * a10, b02 = a00 * a13 - a03 * a10;
    float b03 = a01 * a12 - a02 * a11, b04 = a01 * a13 - a03 * a11, b05 = a02 * a13 - a03 * a12;
    float b06 = a20 * a31 - a21 * a30, b07 = a20 * a32 - a22 * a30, b08 = a20 * a33 - a23 * a30;
    float b09 = a21 * a32 - a22 * a31, b10 = a21 * a33 - a23 * a31, b11 = a22 * a33 - a23 * a32;
    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    float id = abs(det) > 1e-30 ? 1.0 / det : 0.0;
    float4x4 r;
    r[0] = float4(a11 * b11 - a12 * b10 + a13 * b09, a02 * b10 - a01 * b11 - a03 * b09, a31 * b05 - a32 * b04 + a33 * b03, a22 * b04 - a21 * b05 - a23 * b03);
    r[1] = float4(a12 * b08 - a10 * b11 - a13 * b07, a00 * b11 - a02 * b08 + a03 * b07, a32 * b02 - a30 * b05 - a33 * b01, a20 * b05 - a22 * b02 + a23 * b01);
    r[2] = float4(a10 * b10 - a11 * b08 + a13 * b06, a01 * b08 - a00 * b10 - a03 * b06, a30 * b04 - a31 * b02 + a33 * b00, a21 * b02 - a20 * b04 - a23 * b00);
    r[3] = float4(a11 * b07 - a10 * b09 - a12 * b06, a00 * b09 - a01 * b07 + a02 * b06, a31 * b01 - a30 * b03 - a32 * b00, a20 * b03 - a21 * b01 + a22 * b00);
    return r * id;
}
float lumOf(float3 c) { return dot(c, float3(0.3, 0.59, 0.11)); }
struct V2P
{
    float4 pos : SV_Position;
    noperspective float4 t0 : TEXCOORD0;     // screen-space quantities: interpolated straight, not by depth
    noperspective float4 t1 : TEXCOORD1;
    noperspective float4 t2 : TEXCOORD2;
    noperspective float4 t3 : TEXCOORD3;
};

#ifdef VERTEX_SHADER
float depthAtPx(float2 p, float2 scr)
{
    int2 q = clamp(int2(p), int2(0, 0), int2(scr) - 1);
    return Depth.Load(int3(q, 0)).x;
}
float3 worldAtPx(float2 p, float2 scr, out float dd)
{
    int2 q = clamp(int2(p), int2(0, 0), int2(scr) - 1);
    dd = Depth.Load(int3(q, 0)).x;
    float2 n = float2((q.x + 0.5) / scr.x * 2.0 - 1.0, 1.0 - (q.y + 0.5) / scr.y * 2.0);
    return worldAt(n, dd);
}
float4 clipOf(float4x4 Mi, float3 w) { return mul(float4(w, 1.0), Mi); }
float2 pxOf(float4 c, float2 scr) { float iw = abs(c.w) > 1e-20 ? 1.0 / c.w : 0.0; return float2((c.x * iw * 0.5 + 0.5) * scr.x, (0.5 - c.y * iw * 0.5) * scr.y); }

V2P main(uint vid : SV_VertexID)
{
    V2P O; O.pos = float4(0, 0, 0, 0); O.t0 = 0; O.t1 = 0; O.t2 = 0; O.t3 = 0;   // a collapsed quad draws nothing
    float4 o0 = O.pos, t0 = 0, t1 = 0, t2 = 0, t3 = 0;
    float4 ip = IniParams.Load(int2(0, 0)), ip2 = IniParams.Load(int2(2, 0));
    float4 wctl = IniParams.Load(int2(100, 0)), dlk = IniParams.Load(int2(60, 0));
    float wOn = (wctl.w > 0.5 && abs(wctl.z - 26.0) < 0.5 && dlk.w > 0.5) ? 1.0 : 0.0;
    float rLevel = wOn > 0.5 ? clamp(round(wctl.y), 0.0, 3.0) : 0.0;
    if (rLevel < 2.5 || ip.x < 0.5 || ip.y < 0.5) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    gridInit();                                                   // v26k30: the game's own hex grid
    uint W, H; Depth.GetDimensions(W, H);
    float2 scr = float2((float)W, (float)H);
    float kk = scr.y / 1400.0;
    float tm = ip2.y;
    uint corner = vid % 6u; uint q = vid / 6u;
    uint cand = q % NC; uint tile = q / NC;
    if (tile >= NX * NY) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    uint tx = tile % NX, ty = tile / NX;
    float2 tsz = scr / float2((float)NX, (float)NY);
    float2 tc = (float2((float)tx, (float)ty) + 0.5) * tsz;                    // this tile's centre on the screen
    float dT; float3 PT = worldAtPx(tc, scr, dT);
    if (dT <= 0.0 || dT >= 0.99999) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }                                   // sky or nothing under this tile
    float2 PTV = PT.xz - sanSH(); if (PTV.x < 0.0 || PTV.x > MAPW || PTV.y < 0.0 || PTV.y > MAPW) {   // v26k48: the old map's place
    O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    float3 E = cb0[16].xyz;
    float camH = max(E.y - 1250.0, 500.0);
    float pxG;
    {
        float3 r0 = worldAt(float2(0.0, 0.0), 0.001) - worldAt(float2(0.0, 0.0), 0.5), r1 = worldAt(float2(0.0, 2.0 / scr.y), 0.001) - worldAt(float2(0.0, 2.0 / scr.y), 0.5);
        float ang = length(normalize(r1) - normalize(r0));
        pxG = max(camH * ang, 0.01);
    }
    float pxN = pxG * kk / 1.54;                                            // the same number at any screen size (as on a 2160-line screen)
    // v26k: the zoom scale as in SeasonRain.hlsl (pxN: close-up ~1, a province ~10, the whole map ~40-50)
    float zf = smoothstep(5.0, 16.0, pxN);
    float zw = smoothstep(16.0, 32.0, pxN);
    float whole = smoothstep(22.0, 40.0, pxN);                               // at the whole map only the cloud, its curtain and shadow remain
    if (whole > 0.99) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    float Gpx = 48.0 * lerp(1.0, 0.9, zw);
    float Gt = Gpx * kk * pxG;
    float G = 64.0 * exp2(floor(log2(Gt / 64.0)));                            // the grid in steps of two (no swimming while zooming)
    float keep = (G / Gt) * (G / Gt);
    // v26k29: no rain tier on any rain-grid point that a drop of this tile could land near -> every candidate here would be
    //         folded away below (rain strength 0 there whatever the time wobble). Decided now, before the costly part. The drop's
    //         landing point lies within 2 cells (2G) of this tile's ground point; one grid point more on each side for safety.
    {
        float2 gsz = float2(CW_ * 4.0, CH_ * 4.0);
        float2 gmin = (PT.xz - 2.0 * G - C0_) / gsz - 0.5, gmax = (PT.xz + 2.0 * G - C0_) / gsz - 0.5;
        int2 n0 = int2(floor(gmin)) - 1, n1 = int2(floor(gmax)) + 2;
        [branch] if (n1.x - n0.x <= 12 && n1.y - n0.y <= 12) {
            float mxT = 0.0;
            [loop] for (int gj = n0.y; gj <= n1.y; gj++) [loop] for (int gi = n0.x; gi <= n1.x; gi++) mxT = max(mxT, rainCellRaw((float)gi, (float)gj).x);
            if (mxT <= 0.0) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
        }
    }
    float2 cell0 = floor(PT.xz / G);
    float4x4 Mw = float4x4(cb0[26], cb0[27], cb0[28], cb0[29]);
    float4x4 Mi = inv4(Mw);
    // one owner per cell: the tile its centre (at this tile's ground height) lands on. This tile looks at the cell under its
    // centre and the eight around it and takes the first whose centre lands here (cells smaller than a tile, far away, may
    // share a tile: then only one of them is drawn there).
    float2 cell = cell0; float found = 0.0;
    [unroll] for (int oy = -1; oy <= 1; oy++) [unroll] for (int ox = -1; ox <= 1; ox++) {
        float2 cc2 = cell0 + float2((float)ox, (float)oy);
        float4 cc = clipOf(Mi, float3((cc2.x + 0.5) * G, PT.y, (cc2.y + 0.5) * G));
        float2 cp = clamp(pxOf(cc, scr), float2(0, 0), scr - 1.0);
        if (found < 0.5 && cc.w > 0.0 && (uint)(cp.x / tsz.x) == tx && (uint)(cp.y / tsz.y) == ty) { cell = cc2; found = 1.0; }
    }
    if (found < 0.5) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    // this candidate
    float2 cj = cell + (float)cand * 53.0;
    float h1 = hash21(cj + 3.1), h2 = hash21(cj + 17.7), h3 = hash21(cj + 29.3), h4 = hash21(cj + 41.9), h5 = hash21(cj + 57.3);
    float2 wdir = normalize(float2(0.6, 0.35));
    float gust = 0.5 + 0.5 * sin(dot((cell + 0.5) * G, wdir) / (G * 9.0) - tm * 1.3);
    float2 Lxz = float2((cell.x + 0.1 + 0.8 * h1) * G, (cell.y + 0.1 + 0.8 * h2) * G);   // the landing point
    float wxL = cellWx(hexIdxAt(Lxz));
    float4 R = float4(wxBits(wxL, 0.0, 2.0), 0.0, wxBits(wxL, 2.0, 1.0), 0.0);   // v26k30: the landing cell's own rain (the rules'), exactly
    float tn = (vnoise(Lxz / 2600.0 + float2(tm * 0.02, 0.0)) - 0.5) * 0.3;         // v26k: a small edge wobble only - no drops where the rules send no rain
    float tierE = R.x;
    float rainI = smoothstep(0.35, 0.95, tierE);
    if (rainI < 0.01) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    float snowing = smoothstep(0.3, 0.7, R.z);
    // v26k6: three snowfalls by the strength of the system - sleet (drizzle tier): many hard grains, fast short slanted ticks;
    //        snow (rain tier): soft round flakes, slow, swaying; a snowstorm (downpour tier): big flakes driven along the wind
    float sleet = 1.0 - smoothstep(1.3, 1.7, tierE), storm = smoothstep(2.3, 2.7, tierE);
    float snowD = lerp(lerp(0.55, 0.80, sleet), 1.00, storm);                 // v26k19: snow 0.55, sleet 0.80, snowstorm the densest
    // v26k7: rain by strength - drizzle sparse and slow, rain more and faster, a downpour the most and the fastest
    float rDriz = 1.0 - smoothstep(1.3, 1.7, tierE), rHeavy = smoothstep(2.3, 2.7, tierE);
    float rainD = lerp(lerp(0.75, 0.35, rDriz), 1.00, rHeavy);                // v26k19: drizzle clearly sparse, a downpour full
    float dens = lerp(lerp(0.85, 0.70, zf) * lerp(1.0, 0.75, zw) * rainD, snowD, snowing) * keep * (1.0 - whole);
    // v26k29: with 2 candidates instead of 3 the counts per strength become (asked for: fewer drops, the strengths still clearly apart):
    //         drizzle -35%, rain -30%, downpour -20%; sleet -30%, snow -35%, snowstorm -20% (the weaker the fewer)
    dens *= lerp(lerp(lerp(1.005, 0.905, rDriz), 1.195, rHeavy), lerp(lerp(0.962, 1.039, sleet), 1.258, storm), snowing);
    if (h3 > dens * lerp(0.55, 1.0, snowing) * (0.45 + 0.75 * gust * gust)) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    float fallP = lerp(0.72, 0.90, snowing);
    // v26k19: every rain falls fast (drizzle 420, rain 780, downpour 1200 px a second up close); every snow falls slowly as round
    //         grains / flakes (sleet 170, snow 55, snowstorm 95 but driven sideways by the wind) - the slowest rain is 2.5x the fastest snow
    float spd = lerp(lerp(2.0, 1.5, zf) * lerp(lerp(0.94, 0.50, rDriz), 1.44, rHeavy), lerp(lerp(0.165, 0.51, sleet), 0.285, storm), snowing);
    float ph = frac(tm * spd + h4);
    // the landing surface under that point: whatever the picture shows there (ground, a roof, a wall, a horse), found from the
    // depth under its screen position (two rounds, the second one for roofs and walls)
    float Ly = PT.y; float bad = 0.0;
    [unroll] for (int it = 0; it < 2; it++) {
        float4 lc = clipOf(Mi, float3(Lxz.x, Ly, Lxz.y));
        float2 lp = pxOf(lc, scr);
        float dd; float3 Pl = worldAtPx(lp, scr, dd);
        if (lc.w <= 0.0 || lp.x < -tsz.x || lp.x > scr.x + tsz.x || lp.y < -tsz.y || lp.y > scr.y + tsz.y || dd <= 0.0 || dd >= 0.99999) bad = 1.0;
        else Ly = Pl.y;
    }
    if (bad > 0.5) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    float3 Lw = float3(Lxz.x, Ly, Lxz.y);
    float4 Lc = clipOf(Mi, Lw); float2 Lpx = pxOf(Lc, scr);
    float3 PE = Lw - E; float camD = max(length(PE), 1.0);
    // v26k13: the drops fade with the distance ONLY where the cloud and the grey curtain under it take their place (the same weight
    //         SeasonRain.hlsl gives the cloud: pulled back, the camera above the cloud). Before, they faded by distance alone (3 to
    //         5.5 times the camera's height) while no cloud was drawn yet - a tilted camera at a middle height showed nothing, and
    //         turning it a little made the snow and the rain vanish over the same place (양평). Now some picture is always there.
    // v26k38b2: the cloud sheet of SeasonRain.hlsl - fixed over the land (4500 over the lowland, 1500 over higher ground), shown
    //           while the camera is well above it (the same rule)
    float ycl = max(1250.0 + 4500.0, Ly + 1500.0);
    float cloudV = smoothstep(1500.0, 4000.0, E.y - (1250.0 + 4500.0));
    float farD = 1.0 - cloudV * smoothstep(3.0, 5.5, camD / camH);
    float strength = saturate((tierE - 0.6) / 2.0);
    float aK = (0.55 + 0.4 * h5) * rainI * farD * (1.0 - whole);
    aK *= lerp(lerp(lerp(0.55, 0.35, rDriz), 0.70, rHeavy), 1.0, snowing);   // AI_TacticsMod local132 (user 2026-11-14 "빗줄기 그래픽 너무 구리다"): rain short, thin, sparse, faint, slanted       // v26k19: drizzle faint, a downpour strong
    float2 cx = float2((corner == 1u || corner == 3u || corner == 4u) ? 1.0 : 0.0, (corner >= 2u && corner != 3u) ? 1.0 : 0.0);   // (0,0)(1,0)(0,1)(1,0)(1,1)(0,1)
    float2 ndcPx = 2.0 / scr;                                                 // one pixel in ndc units
    [branch] if (ph < fallP) {
        // ---- in the air: a streak (rain) or a flake (snow) ----
        float4 s0 = Lc, s1 = clipOf(Mi, Lw + float3(0.0, 1000.0, 0.0));
        float dpx = max(length(pxOf(s1, scr) - Lpx), 0.5);
        float vpx = clamp(1000.0 / dpx, 0.05, 4000.0);                        // world units per screen pixel, vertical, here
        float Hpx = 300.0 * kk;
        float Lp = lerp(lerp(42.0, 26.0, zf) * lerp(1.0, 0.8, zw) * lerp(lerp(0.84, 0.36, rDriz), 1.58, rHeavy), lerp(lerp(6.0, 7.0, sleet), 16.0, storm), snowing) * (0.6 + 0.8 * h5) * kk;
        // v26k7: the flake grows with the snowfall - sleet a fine grain, snow a soft flake, a snowstorm big flakes
        float wpx = lerp(lerp(1.6, 1.3, zf) * lerp(lerp(0.80, 0.43, rDriz), 1.0, rHeavy), lerp(lerp(2.4, 1.0, sleet), 3.6, storm), snowing) * kk;
        float Hf = Hpx * vpx, Ld = Lp * vpx;
        // v26k: rain starts at the cloud, never above it: the fall begins just under the cloud's base sheet (SeasonRain.hlsl:
        //       LOWLAND + CLOUD_H over the lowland, lifted over high ground), so pulled back the streaks hang under the cloud
        // v26k38b2: every drop starts at the cloud's base right above its own rain cell and falls straight onto that cell; up close
        //           (the camera down at the cloud, the cloud faded) they come from above the screen, never from above the sheet
        Hf = min(lerp(Hf, ycl - Ly, cloudV), ycl - Ly);
        float hgt = (1.0 - ph / fallP) * max(Hf - Ld, Hf * 0.5);              // v26k38b2: the streak's tail never above the cloud's base
        float2 wdF = normalize(float2(0.6, 0.35));
        float driftF = lerp(lerp(0.0, 0.35, sleet), 1.6, storm) * snowing;      // v26k19: sleet a little slanted, a snowstorm blown along the wind
        driftF *= min(1.0, 500.0 / max(Hf * driftF, 1.0));                    // v26k38b2: blown at most 500 sideways over the whole fall (stays under its cloud)
        float3 Ah = Lw + float3(-wdF.x * hgt * driftF, hgt, -wdF.y * hgt * driftF);
        float2 wsl = normalize(float2(0.6, 0.35)) * lerp(lerp(0.0, 0.45, sleet), 0.8, storm) * snowing;   // v26k6: sleet and snowstorm come slanted with the wind
        float rsl = lerp(lerp(0.30, 0.18, rDriz), 0.50, rHeavy);                // v26k19: drizzle nearly straight, a downpour slanted
        float3 At = Lw + float3(-rsl * Ld - wsl.x * Ld, hgt + Ld, -wsl.y * Ld);
        float4 cH = clipOf(Mi, Ah), cT = clipOf(Mi, At);
        if (cH.w <= 0.0 || cT.w <= 0.0) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
        float2 pH = pxOf(cH, scr), pT = pxOf(cT, scr);
        float ext = wpx + 2.0;                                                 // the soft edge around the line
        [branch] if (snowing > 0.5) {      // v26k19: every snowfall is round - sleet small hard grains, snow soft flakes, a snowstorm big flakes
            float rad = wpx * (1.2 + 0.6 * h5) + 1.0;
            float2 off = (cx * 2.0 - 1.0) * (rad + 2.0 * kk);                  // a square around the flake and its rim
            float sway = sin(tm * 1.3 + h4 * 6.2832) * lerp(lerp(11.0, 2.0, sleet), 6.0, storm) * kk;
            float4 c = cH; c.xy += float2(off.x + sway, -off.y) * ndcPx * c.w;
            o0 = c;
            t0 = float4(off.x, off.y, 2.0, strength);
            t1 = float4(aK * 0.9, rad, 0.0, snowing);
            t2 = float4(pH, wpx, 0.0);
            t3 = float4(kk, 0, 0, 0);
            { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
        }
        float2 dir = pT - pH; float len = max(length(dir), 1.0); dir /= len;
        float2 perp = float2(-dir.y, dir.x);
        float along = lerp(-ext, len + ext, cx.y);                             // pixels from the head, along the line
        float across = (cx.x * 2.0 - 1.0) * ext;                               // pixels across
        float2 ppx = pH + dir * along + perp * across;
        float a01 = saturate(along / len);
        float4 c = lerp(cH, cT, a01);                                          // depth along the line
        c.xy = ((ppx / scr) * 2.0 - 1.0) * float2(1.0, -1.0) * c.w;
        o0 = c;
        t0 = float4(across, along / len, snowing > 0.5 ? 4.0 : 1.0, strength);
        t1 = float4(snowing > 0.5 ? aK * lerp(0.95, 0.85, storm) : aK, wpx, len, snowing);
        t2 = float4(pH, 0.0, 0.0);
        t3 = float4(kk, 0, 0, 0);
        { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    }
    // ---- landing now: the splash on the surface (rain only) ----
    if (snowing > 0.5) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    float nearF = 1.0 - smoothstep(8.0, 18.0, pxN);
    if (nearF < 0.01) { O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O; }
    float qf = saturate((ph - fallP) / (1.0 - fallP));                        // 0 the moment it lands .. 1 gone
    // the slope under the landing point from the depth around it: steepness and the downhill direction on the screen
    float o = 5.0 * kk; float d1, d2, d3, d4;
    float3 Pr = worldAtPx(Lpx + float2(o, 0), scr, d1), Pl2 = worldAtPx(Lpx - float2(o, 0), scr, d2);
    float3 Pd = worldAtPx(Lpx + float2(0, o), scr, d3), Pu = worldAtPx(Lpx - float2(0, o), scr, d4);
    float3 Nn = normalize(cross(Pd - Pu, Pr - Pl2) + float3(0.0, 1e-6, 0.0)); if (Nn.y < 0.0) Nn = -Nn;
    float steep = 1.0 - smoothstep(0.55, 0.95, Nn.y);
    float2 Dh = normalize(Nn.xz + float2(1e-5, 0.0));                          // downhill (horizontal, world)
    float4 cd = clipOf(Mi, Lw + float3(Dh.x, 0.0, Dh.y) * 200.0);
    float2 dsc = pxOf(cd, scr) - Lpx; float dl = length(dsc); dsc = dl > 0.5 ? dsc / dl : float2(0.0, 1.0);
    if (steep > 0.85) dsc = float2(0.0, 1.0);                                  // a wall: straight down the face
    float S = 30.0 * kk;                                                       // the quad's half size (pixels)
    float2 off = (cx * 2.0 - 1.0) * S;
    float4 c = Lc; c.z = min(c.z * 1.03 + 0.001 * c.w, c.w);                  // a touch nearer than the surface (reversed depth)
    c.xy += float2(off.x, -off.y) * ndcPx * c.w;
    o0 = c;
    // the phase-derived sizes are worked out here, per splash (d3dcompiler_46 falls over when the pixel shader derives them)
    float bq = saturate(qf / 0.35), fade = (1.0 - qf) * (1.0 - qf), rr = (3.0 + 15.0 * qf) * kk;
    float runL = lerp(6.0, 26.0, steep) * kk * qf, ringIn = smoothstep(0.0, 0.25, qf);
    t0 = float4(off.x, off.y, 3.0, strength);
    t1 = float4(rainI * farD * nearF * (1.0 - whole), fade, steep, ringIn);
    t2 = float4(Lpx, dsc);
    t3 = float4(kk, bq, rr, runL);
    O.pos = o0; O.t0 = t0; O.t1 = t1; O.t2 = t2; O.t3 = t3; return O;
}
#endif

#ifdef PIXEL_SHADER
float lumAt(float2 p)
{
    uint W, H; Scene.GetDimensions(W, H);
    int2 q = clamp(int2(p), int2(0, 0), int2(W, H) - 1);
    return lumOf(Scene.Load(int3(q, 0)).rgb);
}
// the splash of one landing drop: a burst (a bright dot with three ticks thrown up) then a ring opening and fading; on a
// slope the ring slides a little downhill and the water runs a short way down; on a steep face (a wall) it runs down the face
float splashOf(float2 v, float fade, float steep, float ringIn, float2 dsc, float kk, float bq, float rr, float runL, out float isBurst)
{
    float th = 1.6 * kk;
    float dotR = (2.6 + 2.0 * bq) * kk;
    float bu = smoothstep(dotR, dotR * 0.3, length(v)) * (1.0 - 0.6 * bq);
    float tl = (3.0 + 10.0 * bq) * kk;
    float tw = (1.0 - bq) * 0.95;
    float al0 = -v.y, ac0 = abs(v.x);                                           // three ticks thrown up: straight, left, right
    float al1 = dot(v, float2(-0.62, -0.78)), ac1 = abs(dot(v, float2(0.78, -0.62)));
    float al2 = dot(v, float2(0.62, -0.78)), ac2 = abs(dot(v, float2(0.78, 0.62)));
    bu = max(bu, smoothstep(th, th * 0.3, ac0) * step(0.0, al0) * step(al0, tl) * tw);
    bu = max(bu, smoothstep(th, th * 0.3, ac1) * step(0.0, al1) * step(al1, tl) * tw);
    bu = max(bu, smoothstep(th, th * 0.3, ac2) * step(0.0, al2) * step(al2, tl) * tw);
    float2 ctr = dsc * rr * 0.35 * steep;                                       // the ring slides a little downhill on a slope
    float2 e = v - ctr; float ringD = length(float2(e.x, e.y / 0.5));            // an ellipse: the ring lies flat on the ground
    float ring = smoothstep(th, th * 0.3, abs(ringD - rr)) * (1.0 - smoothstep(0.55, 0.85, steep)) * ringIn;
    float alr = dot(v, dsc), acr = abs(dot(v, float2(-dsc.y, dsc.x)));
    float run = smoothstep(th, th * 0.3, acr) * step(0.0, alr) * step(alr, runL) * smoothstep(0.15, 0.4, steep);
    float spl = max(ring, run * 0.9) * fade;
    float ab = bu * 0.95, as2 = spl * 0.80;
    isBurst = step(as2, ab);
    return max(ab, as2);
}
void main(V2P I, out float4 o0 : SV_Target0)
{
    o0 = 0;
    float4 pos = I.pos, t0 = I.t0, t1 = I.t1, t2 = I.t2, t3 = I.t3;
    float kind = t0.z; float kk = t3.x;
    int2 px = int2(pos.xy);
    float3 scene = Scene.Load(int3(px, 0)).rgb;
    float sl = lumOf(scene);
    float o = 14.0 * kk;
    float ref = (sl * 2.0 + lumAt(pos.xy + float2(o, 0)) + lumAt(pos.xy - float2(o, 0)) + lumAt(pos.xy + float2(0, o)) + lumAt(pos.xy - float2(0, o))) / 6.0;
    ref = max(ref, 1e-4);
    // the colour rule (see SeasonRain.hlsl): pale grey-white on ground, grass and dark roofs; darker on pale bright surfaces
    float satS = (max(scene.r, max(scene.g, scene.b)) - min(scene.r, min(scene.g, scene.b))) / max(max(scene.r, max(scene.g, scene.b)), 1e-4);
    float paleHere = smoothstep(0.40, 0.15, satS) * smoothstep(0.95, 1.35, sl / ref);
    float3 grey = lerp(scene, sl.xxx, 0.7);
    float3 liftC = lerp(grey * 1.12 + ref * 0.26, scene * 0.70, paleHere);
    float3 liftH = lerp(grey * 1.05 + ref * 0.25, scene * 0.58, paleHere);   // v26k38f: <= 1.3x the ground
    float3 rimC = scene * 0.78;
    float a = 0.0; float3 col = liftC;
    [branch] if (kind < 1.5) {
        // a streak: t0.x pixels across, t0.y 0..1 along (0 = the leading end), t1 = (strength, width, length)
        float wpx = t1.y; float dd = abs(t0.x); float al = t0.y;
        float lenF = smoothstep(1.0, 0.7, al) * smoothstep(-0.03, 0.0, al);
        float c = smoothstep(wpx, wpx * 0.3, dd) * lenF;
        float r = smoothstep(wpx + 1.6, wpx * 0.9, dd) * lenF * (1.0 - c) * (0.5 + 0.4 * smoothstep(0.40, 0.15, satS));
        float hd = smoothstep(0.30, 0.0, al);
        col = c > 0.001 ? lerp(liftC, liftH, hd) : rimC;
        a = max(c, r * 0.55) * t1.x;
        if (c <= 0.001) a = r * 0.55 * t1.x;
    } else if (kind > 3.5) {
        // v26k6: a snow streak (sleet grain / snowstorm flake drawn out by the wind): white, soft ends
        float wpx = t1.y; float dd = abs(t0.x); float al = t0.y;
        float lenF = smoothstep(1.0, 0.6, al) * smoothstep(-0.05, 0.1, al);
        float c = smoothstep(wpx, wpx * 0.25, dd) * lenF;
        float r = smoothstep(wpx + 1.6, wpx * 0.9, dd) * lenF * (1.0 - c) * (0.35 + 0.5 * paleHere);   // v26k9: rim on pale ground
        col = c > 0.02 ? max(ref, sl) * 1.15 + scene * 0.15 : scene * 0.60;
        a = (c > 0.02 ? c : r) * t1.x;
    } else if (kind < 2.5) {
        // a snowflake: a soft dot
        float rad = t1.y; float dd = length(t0.xy);
        float c = smoothstep(rad, rad * 0.35, dd);
        // v26k9: a thin darker rim, stronger on pale ground (snow-covered land, pale rock) so the flake shows on white too
        float r = smoothstep(rad + 1.8 * kk, rad * 0.9, dd) * (1.0 - c) * (0.35 + 0.5 * paleHere);
        col = c > 0.02 ? max(ref, sl) * 1.15 + scene * 0.15 : scene * 0.60;
        a = (c > 0.02 ? c : r) * t1.x;
    } else {
        // a splash: local pixel coordinates t0.xy (x right, y down); t1 = (strength, fade, steep, ring-in), t2.zw downhill on
        // the screen, t3 = (kk, burst phase, ring radius, run length)
        float isB; float sa = splashOf(t0.xy, t1.y, t1.z, t1.w, t2.zw, kk, t3.y, t3.z, t3.w, isB);
        a = sa * t1.x; col = lerp(liftC, liftH, isB);
    }
    if (a < 0.003) discard;
    o0 = float4(col, saturate(a));
}
#endif
