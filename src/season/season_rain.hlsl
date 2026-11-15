// 4계절 패치 (wprkfdid12-ux) SeasonRain.hlsl v26k52, vendored unchanged for AI_TacticsMod (free use). Drawn by native_season_bake.inl after the game's lighting pass.
// v26k38: no ghost outlines (one brightness for the whole picture), a soft half-dense cloud, the curtain soft between rain amounts
// SAN14 season map v26k17 (smooth curtain and cloud outline): the rain layer over the whole picture - troops, horses, trees and walls included - drawn after the
// v26k30: the game's own hex grid; the rain (haze, curtain, lightning) exactly on the rules' rain cells - only the cloud keeps its soft outline
// game's lighting (post of the water-layer lighting pass 70b6fa39daa0880e), before the interface. Each pixel's world position
// comes from the depth buffer and that pass's own clip -> world matrix. Every colour drawn here is taken RELATIVE to the lit
// picture (a copy of it, d3dx.ini: ResourceSAN14Scene) so the layer looks right whatever brightness scale the game is using
// (fixed brightness terms turned into solid white blobs in the whole-map view).
// v26k - rain SYSTEMS: the rules module now sends rain as district-sized masses (not speckle), and this layer draws, pulled
// back, what the user asked for: a translucent rain CLOUD floating over each mass (half opaque at its heart, lit on one
// flank, grey underneath; neighbouring masses join into one larger cloud), a grey rain CURTAIN of fine streaks hanging from
// the cloud down to the ground, and (in the terrain shaders) a faint shadow on the land. Up close the cloud fades away and
// only the streaks, drops and splashes remain (SeasonRainDrops.hlsl), as before.
// Zoom scale used everywhere below: pxN = world units one screen pixel covers at the screen centre, as on a 2160-line screen
// (close-up about 1, a city in view about 3, a province about 10, the whole map about 40-50).
Texture2D<float4> Depth : register(t4);        // the lighting pass's sDepth
Texture2D<float4> Scene : register(t101);      // copy of the lit picture (d3dx.ini: ResourceSAN14Scene)
Texture1D<float4> IniParams : register(t120);
cbuffer Globals : register(b0) { float4 cb0[31]; }   // [13] camera position, [14] camera direction, [23..26] clip -> world (rows)

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
static const float LOWLAND = 1250.0;            // the lowland's height
static const float CLOUD_H = 7000.0;            // (old) the cloud base over the lowland
static const float CLOUD_UP = 4500.0;           // v26k38b2: the cloud sheet, FIXED over the land: 4500 over the lowland (about six tiles),
                                                //           1500 over any higher ground - it never moves with the camera
// v26k38b2: the cloud's footprint = the rules' own rain cells under it (five look-ups, 0.35 of a cell apart): 1 inside a rain cell,
//           0 a third of a cell past its rim, soft in between - no cloud anywhere else
float rainCover(float2 q)
{
    float2 o1 = float2(CW_, CH_) * 0.35, o2 = float2(CW_, -CH_) * 0.35;
    float c = step(0.5, wxBits(cellWx(hexIdxAt(q)), 0.0, 2.0)) * 2.0;
    c += step(0.5, wxBits(cellWx(hexIdxAt(q + o1)), 0.0, 2.0)) + step(0.5, wxBits(cellWx(hexIdxAt(q - o1)), 0.0, 2.0));
    c += step(0.5, wxBits(cellWx(hexIdxAt(q + o2)), 0.0, 2.0)) + step(0.5, wxBits(cellWx(hexIdxAt(q - o2)), 0.0, 2.0));
    return c / 6.0;
}

float hash21(float2 p) { p = frac(p * float2(123.34, 456.21)); p += dot(p, p + 45.32); return frac(p.x * p.y); }
float vnoise(float2 p)
{
    float2 i = floor(p), f = frac(p); float2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i), b = hash21(i + float2(1, 0)), c = hash21(i + float2(0, 1)), d = hash21(i + float2(1, 1));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}
float fbm2(float2 p) { return vnoise(p) * 0.62 + vnoise(p * 2.13 + 7.1) * 0.38; }
float billow(float2 p) { return 1.0 - abs(2.0 * fbm2(p) - 1.0); }                   // rounded lumps
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
float rainTierRaw(float i, float j)                  // v26k38: the tier alone (the cloud's outline needs nothing else)
{
    float idx = clamp(j, 0.0, 54.0) * 55.0 + clamp(i, 0.0, 54.0);
    float4 v4 = IniParams.Load(int2(150 + (int)floor((idx + 0.5) / 4.0), 0)); float v = v4[(int)(idx - floor((idx + 0.5) / 4.0) * 4.0)];
    v -= floor(v / 100000.0 + 1e-4) * 100000.0; v -= floor(v / 10000.0 + 1e-4) * 10000.0;
    return floor(v / 1000.0 + 1e-4);
}
float rainTierAt(float2 wp)
{
    float gx = (wp.x - C0_.x) / (CW_ * 4.0) - 0.5, gy = (wp.y - C0_.y) / (CH_ * 4.0) - 0.5;
    float x0 = floor(gx), y0 = floor(gy); float fx = gx - x0, fy = gy - y0;
    return lerp(lerp(rainTierRaw(x0, y0), rainTierRaw(x0 + 1.0, y0), fx), lerp(rainTierRaw(x0, y0 + 1.0), rainTierRaw(x0 + 1.0, y0 + 1.0), fx), fy);
}
float wobbleAt(float2 wp, float tm) { return (vnoise(wp / 2600.0 + float2(tm * 0.02, 0.0)) - 0.5) * 0.3; }   // a small edge wobble only: no rain where the rules send none
float3 worldAt(float2 ndc, float d)
{
    float4 hw = ndc.x * cb0[23] + ndc.y * cb0[24] + d * cb0[25] + cb0[26];
    return hw.xyz / (abs(hw.w) > 1e-20 ? hw.w : 1e-20);
}

float lumOf(float3 c) { return dot(c, float3(0.3, 0.59, 0.11)); }
float lumAt(float2 p, float2 scr)                    // the lit picture's brightness under a screen pixel
{
    int2 q = clamp(int2(p), int2(0, 0), int2(scr) - 1);
    return lumOf(Scene.Load(int3(q, 0)).rgb);
}
// v26k38: one brightness for the whole picture - 8 fixed points spread over the screen, the same for every pixel (the mean of the
//         brighter half, so a dark sea or a bright snowfield in a corner moves it only a little). Before, every pixel measured the
//         picture 14 and 80 px around itself, and wherever those points fell on a border line, an icon or a number, the cloud and
//         the curtain grew brighter in exactly that shape: copies of the borders shifted sideways, layer on layer ("ghost outlines").
float sceneRef(float2 sscr)
{
    float l[8]; float s = 0.0;
    [unroll] for (int j = 0; j < 8; j++) {
        float2 f = float2(((float)(j % 4) + 0.5) / 4.0, ((float)(j / 4) + 0.5) / 2.0 + (float)(j % 2) * 0.12 - 0.06) * 0.84 + 0.08;
        l[j] = lumAt(f * sscr, sscr); s += l[j]; }
    float mean = s / 8.0, su = 0.0, nu = 0.0;
    [unroll] for (int k = 0; k < 8; k++) { float u = step(mean, l[k]); su += l[k] * u; nu += u; }
    return max(su / max(nu, 1.0), 1e-4);
}
// v26k38: the cloud's density at a world point - heaped lumps at three sizes (a cauliflower edge, no drawn rim). The same function and
//         drift as the shadow in the terrain shaders, so the shadow lies right under the cloud.
float cloudDens(float2 q)
{
    return fbm2(q / 3200.0 + 1.0) * 0.68 + billow(q / 1150.0 + 2.0) * 0.22 + billow(q / 520.0 + 3.0) * 0.10;   // v26k38g: round puffs (fbm), lumpy rims
}

void main(float4 pos : SV_Position, float4 ndc4 : TEXCOORD0, out float4 o0 : SV_Target0)
{
    o0 = 0;
    float4 ip = IniParams.Load(int2(0, 0)), ip2 = IniParams.Load(int2(2, 0));
    float4 wctl = IniParams.Load(int2(100, 0)), dlk = IniParams.Load(int2(60, 0));
    float wOn = (wctl.w > 0.5 && abs(wctl.z - 26.0) < 0.5 && dlk.w > 0.5) ? 1.0 : 0.0;
    float rLevel = wOn > 0.5 ? clamp(round(wctl.y), 0.0, 3.0) : 0.0;
    if (rLevel < 1.5 || ip.x < 0.5 || ip.y < 0.5) discard;
    gridInit();                                                   // v26k30: the game's own hex grid
    // v26k: every lookup goes by the pixel's screen fraction (ndc), never by its pixel number - while the camera moves the game
    //       renders the depth at another size than the picture (the layer vanished whenever the camera moved: pixel numbers ran
    //       off the smaller depth texture)
    float2 ndc = ndc4.xy;
    float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    uint W, H; Depth.GetDimensions(W, H);
    uint SW, SH; Scene.GetDimensions(SW, SH);
    int2 px = clamp(int2(uv * float2((float)W, (float)H)), int2(0, 0), int2((int)W - 1, (int)H - 1));
    float2 spos = uv * float2((float)SW, (float)SH);                // this pixel in the picture copy
    int2 spx = clamp(int2(spos), int2(0, 0), int2((int)SW - 1, (int)SH - 1));
    float d = Depth.Load(int3(px, 0)).x;
    if (d <= 0.0 || d >= 0.99999) discard;
    float3 P = worldAt(ndc, d);
    float3 E = cb0[13].xyz;
#ifdef CLOUD_ONLY
    if (!(E.y > 1000.0 && E.y < 400000.0)) discard;   // AI_TacticsMod local152: not this pass's constants
#endif
    float2 wp = P.xz;
    { float2 wpV = wp - sanSH(); if (wpV.x < 0.0 || wpV.x > MAPW || wpV.y < 0.0 || wpV.y > MAPW) discard; }   // v26k48: the old map's place
#ifdef CLOUD_ONLY   // AI_TacticsMod local156 (user 2026-11-14 "구름 ... 순마다 바뀌게 ... 순이 바뀔때 한번에 계산"): the clouds stand still
    float tm = ((ip.x - 1.0) * 3.0 + IniParams.Load(int2(1, 0)).x) * 60.0;   // within a 순 and step once (60 s of the old drift) when it turns
#else
    float tm = ip2.y;
#endif
    float2 scr = float2((float)W, (float)H);
    float2 sscr = float2((float)SW, (float)SH);
    float kk = sscr.y / 1400.0;
    float camH = max(E.y - LOWLAND, 500.0);                       // the camera's height over the lowland
    float pxG;
    {   // one pixel size for the whole screen (the angle of one pixel x the camera's height): the same grid everywhere on the screen
        float3 r0 = worldAt(float2(0.0, 0.0), 0.001) - worldAt(float2(0.0, 0.0), 0.5), r1 = worldAt(float2(0.0, 2.0 / scr.y), 0.001) - worldAt(float2(0.0, 2.0 / scr.y), 0.5);
        float ang = length(normalize(r1) - normalize(r0));
        pxG = max(camH * ang, 0.01);
    }
    float pxN = pxG * kk / 1.54;                                  // the same number at any screen size (as on a 2160-line screen)
    // v26k: the zoom scale (pxN: close-up ~1, a province ~10, the whole map ~40-50 - the old 85..130 'whole map' was never reached)
    float zf = smoothstep(5.0, 16.0, pxN);                        // streaks shorten, the haze thins
    float zw = smoothstep(16.0, 32.0, pxN);
    float whole = smoothstep(22.0, 40.0, pxN);                    // the whole map: no haze, no lightning, no single streaks
    // v26k38d: the cloud shows at every zoom step but the closest (it only fades going down to that one). Before it showed only between
    //          pxN 6..24 AND with the camera well above a sheet 7000 high - no resting zoom step was in that window, so it flashed up
    //          only while the view moved between two steps (the blink).
    // v26k38h: and none at the strategic whole-map steps (camera 90000+): full at the top-down step (pxN 26.7), gone by pxN 40 (camera
    //          ~82000) - there the picture under it is the black faction map and the fixed grey-white painted white blotches over it
    // v26k38b2: the cloud is a sheet fixed over the land (CLOUD_UP): it shows while the camera is well above it and fades as the camera
    //           comes down to it (the closest zoom step, camera ~6400: gone; the next one, ~12400: full). Before, the sheet kept a quarter
    //           of the camera's height and came down with every zoom step ("the cloud follows the zoom down").
    float cloudA = smoothstep(1500.0, 4000.0, E.y - (LOWLAND + CLOUD_UP));
    float cloudFar = 1.0 - smoothstep(30.0, 40.0, pxN);           // v26k38h: the cloud body only
    // the view ray above this point (the curtain samples and the cloud lean along it toward the camera)
    float3 toE = E - P; float Lh = max(length(toE.xz), 1.0); float2 dirH = toE.xz / Lh; float tanE = max(toE.y / Lh, 0.08);
    // ---- the cloud geometry: a flat sheet at CLOUD_H over the lowland (lifted a little over high ground so no peak pokes through)
    // v26k38d: the sheet keeps a quarter of the camera's height (never above 5600): always well under the camera at every zoom, and the
    //          drops and flakes (SeasonRainDrops.hlsl, the same rule) start at it - the rain comes out of the cloud
    float yc = max(LOWLAND + CLOUD_UP, P.y + 1500.0);            // v26k38b2: fixed - the same for every camera
    float Hcl = yc - P.y;                                         // the curtain's height here
    // v26k38c: the cloud sits on the map (no lean along the view): the camera tilts with every zoom step, and a cloud drawn where the
    //          line of sight crossed a sheet 7000 high slid several cells across the map at each step - it seemed to blink. Its shadow on
    //          the land (terrain shaders) is set off toward the south-east, away from the sun, so it still reads as floating.
    float2 Qc = wp + dirH * (Hcl / tanE);                         // where this pixel's line of sight crosses the fixed sheet (true 3D: the cloud stays put)
    float wxB = cellWx(hexIdxAt(wp));
    float4 R = float4(wxBits(wxB, 0.0, 2.0), 0.0, wxBits(wxB, 2.0, 1.0), 0.0);   // v26k30: this cell's own rain (the rules'), exactly
    float tn = wobbleAt(wp, tm);
    float tierE = R.x;                                   // v26k30: no edge wobble - the rain is on the rain cells, nowhere else
    float rainI = smoothstep(0.35, 0.95, tierE);
    // v26k38b2: the cloud over exactly the rules' rain cells (the cell under the sheet point, softened a third of a cell) - not the 4x4
    //           rain grid spread 2.8 cells outward (that put cloud over clear cells, and rain seemed to fall from nowhere next to it)
    float cov = 0.0, wxQ = 0.0;
    [branch] if (cloudA > 0.01) { cov = rainCover(Qc); wxQ = cellWx(hexIdxAt(Qc)); }
    float tierC = max(wxBits(wxQ, 0.0, 2.0), cov);
    float cf = cov * cloudA * cloudFar;                           // the cloud's footprint
    float strengthC = saturate((wxBits(wxQ, 0.0, 2.0) - 0.6) / 2.0);
    float Hs = lerp(900.0, Hcl, cloudA);                          // the curtain: a tile high up close, up to the cloud when pulled back
    // nothing here at all? (no rain under the pixel, none along the ray up to the cloud, no cloud over it)
#ifdef CLOUD_ONLY
    if (cf < 0.002) discard;
#else
    [branch] if (rainI < 0.004 && cf < 0.002) {
        float Rm = wxBits(cellWx(hexIdxAt(wp + dirH * (Hs * 0.5 / tanE))), 0.0, 2.0), Rt = wxBits(cellWx(hexIdxAt(wp + dirH * (Hs / tanE))), 0.0, 2.0);
        if (Rm < 0.5 && Rt < 0.5) discard;
    }
#endif
    float strength = saturate((tierE - 0.6) / 2.0);
    float snowing = smoothstep(0.3, 0.7, max(R.z, wxBits(wxQ, 2.0, 1.0) * cloudA));
    float3 scene = Scene.Load(int3(spx, 0)).rgb;
    float sl = lumOf(scene);
    float3 PE = P - E; float camD = max(length(PE), 1.0);
    // the local brightness of the picture (this pixel and four around it): the only scale the colours below are drawn against
    // v26k38: this pixel against the whole picture (no points 14 px away any more: they copied border lines into the haze and curtain)
    // v26k38f: every colour from this pixel's own brightness only. The picture is an HDR buffer (ground 3-15, glints / snow / labels
    //          50-120): measured against the whole picture (k38-k38e) the cloud came out 2-4x the ground's brightness, white paint
    //          over the map, and jumped with whatever the reference points hit
    float ref = max(sl, 1e-4);
    // the colour rule: a pale grey-white line (lighter than the picture, less saturated) on ground, grass, trees, dark roofs;
    // a darker line only where the surface is itself pale and bright (snow, pale rock, white walls)
    float satS = (max(scene.r, max(scene.g, scene.b)) - min(scene.r, min(scene.g, scene.b))) / max(max(scene.r, max(scene.g, scene.b)), 1e-4);
    float paleHere = smoothstep(0.40, 0.15, satS) * smoothstep(0.95, 1.35, sl / ref);   // 1 = a pale bright surface
    float3 grey = lerp(scene, sl.xxx, 0.7);
    float3 liftC = lerp(grey * 1.0 + ref * 0.25, scene * 0.70, paleHere);                // the line / the curtain (<= 1.25x the ground)
    float3 curtainC = lerp(grey * 1.0 + ref * 0.22, scene * 0.72, paleHere) * float3(0.94, 0.97, 1.04);   // the curtain under a cloud: a touch cooler
    float3 Cp = 0; float Aacc = 0;                                // the layers, back to front ("over")
    // v26k38e: how much of this pixel's line of sight, from the ground up to the cloud sheet, runs over rain cells (the curtain's own
    //          eight look-ups, no extra cost): the cloud covers that whole column too - the rain on the ground and the lower part of
    //          the drops no longer stick out under the cloud's edge ("rain falling outside the cloud")
    float colC = 0.0;
#ifndef CLOUD_ONLY   // AI_TacticsMod local152 (user 2026-11-14 "구름만 진행"): the cloud body alone, no rain picture
    // 1) a thin rain haze with the distance (never a sheet): a share of the local brightness, blue-grey; gone at the whole map
    [branch] if (rLevel > 1.5 && whole < 0.99) {
        float hz = rainI * (0.05 + 0.07 * strength) * smoothstep(1500.0, 12000.0, camD) * lerp(1.0, 0.5, zw) * (1.0 - whole) * (1.0 - 0.5 * cloudA);
        // v26k6: falling snow whitens the air near and far - sleet none, snow 8%, a snowstorm 22% (rain unchanged)
        float sSleet = 1.0 - smoothstep(1.3, 1.7, tierE), sStorm = smoothstep(2.3, 2.7, tierE);
        float hzS = rainI * (1.0 - sSleet) * lerp(0.08, 0.22, sStorm) * (1.0 - whole) * (1.0 - 0.5 * cloudA);
        hz = lerp(hz, hzS, smoothstep(0.3, 0.7, R.z));
        float3 hazeC = sl * lerp(0.85, 1.1, snowing) * lerp(float3(0.97, 0.99, 1.02), float3(1.0, 1.0, 1.02), snowing);   // AI_TacticsMod local132 (user 2026-11-14 "빗줄기 그래픽 너무 구리다"): neutral, no tint
        Cp = Cp * (1 - hz) + hazeC * hz; Aacc = Aacc * (1 - hz) + hz;
        // local132: the rain cells read as wet - the ground a little darker and duller under the rain (not a colour wash)
        float wetA = rainI * (1.0 - snowing) * (0.10 + 0.08 * strength) * (1.0 - whole);
        float3 wetC = lerp(scene, sl.xxx, 0.35) * 0.72;
        Cp = Cp * (1 - wetA) + wetC * wetA; Aacc = Aacc * (1 - wetA) + wetA;
    }
    // 2) the curtain: a translucent column over every raining tile with fine streaks running down it, sampled at five heights up
    //    this pixel's own view ray. Up close a tile high and thin (as before); pulled back it reaches the cloud and is the grey
    //    veil of rain seen hanging under it.
    [branch] if (rLevel > 1.5) {
        float2 perp = float2(dirH.y, -dirH.x);
        float shaft = 0.0; colC = 0.0;
        // v26k17: eight heights, shifted a little per pixel (a fine dither fixed to the ground) - five fixed heights drew five separate
        //         copies of the rain area stepped along the view, seen as stretched, stair-stepped ghosts at the cloud's edges and over
        //         the sea, sliding when the camera moved; now the curtain is one soft, continuous veil
        float jit = hash21(floor(wp / max(pxG * 1.5, 1.0)) + 0.37);
        [unroll] for (int k = 0; k < 8; k++) {
            float hk = ((float)k + jit) / 8.0 * Hs;
            float2 Q = wp + dirH * (hk / tanE);
            // v26k38: pulled back (where the cloud shows) each of the eight heights looks up the rain cell a little to its own side (a
            //         fixed spiral, up to 0.7 of a cell, growing with the cloud): the eight hex edges no longer fall on one another as
            //         stacked wavy outlines, they spread into one soft edge - the same eight look-ups as before (no extra cost)
            float ka = (float)k * 2.39996; float2 ko = float2(cos(ka), sin(ka)) * (sqrt(((float)k + 0.5) / 8.0) * 0.7 * CW_ * cloudA);
            float rk = step(0.5, wxBits(cellWx(hexIdxAt(Q + ko)), 0.0, 2.0));   // v26k30: the curtain hangs only over rain cells
            colC += rk;
            float u = dot(Q, perp);
            float st = vnoise(float2(u / (3.5 * pxG), (hk + tm * 2200.0) / (55.0 * pxG)));
            float st2 = vnoise(float2(u / (9.0 * pxG) + 7.0, (hk + tm * 1600.0) / (140.0 * pxG)));
            float wgt = lerp(1.0 - hk / Hs, 0.55 + 0.45 * hk / Hs, cloudA);            // up close denser at the ground; under a cloud denser toward its base
            shaft += rk * wgt * (0.25 + 0.75 * smoothstep(0.42, 0.80, st * 0.6 + st2 * 0.4));
        }
        shaft *= 0.125; colC *= 0.125;
        float ash = shaft * (0.05 * (1.0 - cloudA) + 0.03 * cloudA) * (1.0 - 0.4 * snowing);   // AI_TacticsMod local132 (user 2026-11-14 "빗줄기 그래픽 너무 구리다"): only a faint veil, no columns   // v26k38g: the veil under the cloud 0.10 (the gaps between the cloud lumps stay clear)
        float3 cc = lerp(liftC, curtainC, cloudA);
        Cp = Cp * (1 - ash) + cc * ash; Aacc = Aacc * (1 - ash) + ash;
    }
#endif
    // 3) the cloud, on top (v26k38b2): one sheet over the rain cells only, no clear gaps inside them - heaped lighter and denser lumps
    //    (30% at the thinnest, 60% at a lump's heart, so the map always shows), the rim soft within a third of a cell past the rain
    //    and wobbling with the lumps. ONE FIXED grey-white colour (HDR ~24) - nothing taken from the picture, so it cannot jump.
    [branch] if (cf > 0.002) {
        float2 q = Qc + float2(tm * 45.0, tm * 28.0);                           // the lumps drift; the footprint does not
        float D = cloudDens(q), DL = cloudDens(q + float2(-300.0, 300.0));      // toward the light: the north-west flank
        float e = saturate(cov + (D - 0.62) * 4.8 * cov * (1.0 - cov));         // the rim wobbles with the lumps; inside a rain cell 1
        float lump = smoothstep(0.50, 0.75, D), litS = smoothstep(-0.04, 0.05, D - DL);
        float a = e * (0.12 + 0.16 * lump) * cloudA * cloudFar;   // AI_TacticsMod local132 (user 2026-11-14 "빗줄기 그래픽 너무 구리다"): a light cloud, the map shows
        float wC = 24.0 * (1.0 - 0.25 * strengthC);                              // a downpour's cloud a little greyer
        float3 litC = wC * float3(1.0, 1.0, 1.02), shC = wC * 0.62 * float3(0.92, 0.95, 1.03);
        float3 col = lerp(shC, litC, saturate(0.35 + 0.65 * litS - 0.25 * lump * (1.0 - litS)));
        Cp = Cp * (1 - a) + col * a; Aacc = Aacc * (1 - a) + a;
    }
    // 4) v26k38f: no lightning (a flash over the whole rain area every few seconds read as the layer blinking)
    // v26k38c: none of the weather on the game's black faction map (the whole-map overlay drawn into the picture while zooming out to
    //          the top): the cloud showed there as grey patches popping in and out
    Aacc *= smoothstep(0.005, 0.02, sl);                         // v26k38h: 10x stricter (the faction map comes in at 0.0001-0.004; real dark ground is 0.03+)
    if (Aacc < 0.002) discard;
    // v26k38g: at most 65% anywhere (the map always shows through)
    float Aout = min(Aacc, 0.65);
    float3 outC = Cp / max(Aacc, 1e-4);
    o0 = float4(outC, Aout);
}
