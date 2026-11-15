/* vendored by tools/vendor_season_rules.py - see season_engine.c */
/* ======================================================================================================
 * v26: the living climate - everything is a cause and a result, every roll independent
 *   year   : rain / cold anomaly fields (coarse: regions move together + local), monsoon strength (winter, summer)
 *   turn   : temperature, dryness, wind field (terrain-shaped), lying snow and river ice (the "normal year" tables
 *            stay exactly as approved; the year's anomaly only adds or takes away), fire / flood / slide rolls
 *   day    : rain systems drifting with the wind (3 strengths, rain shadow behind the high ranges, snow in the
 *            cold), wet ground, drought cracks; rain puts fires out, feeds the rivers, loosens slopes,
 *            slows armies and weakens 화계
 * ====================================================================================================== */
#define GX 9
#define GN 81
#define RG 55
#define CL(c) (cl + (u64)(c) * 64)
static u8 *cl, *shNW, *shSE, *fuelT, *peopleT, *roughT, *naltT; static int v2;
#define MAXS 48
#pragma pack(push, 1)
typedef struct { u8 river, dn; u16 mid, nstem, pad; float B, r2; } SDesc;
#pragma pack(pop)
typedef struct { int L, below, hold, jamL, jamDays, frozenRun, wasFrozen, jamDone; float S, Q, Qin, Qnext, G, inflow; } Stretch;
typedef struct { int active, maxL, kind, cell, days, drained; u32 seq, day0, drainDay; } RiverEp;
static SDesc *sdesc; static int NSt, NSt0; static u8 *fstr, *fneed, *fcatch, *fstem;
static Stretch sx[MAXS + 1]; static RiverEp rep_[9];   /* local156: rivers 4..8 (the side rivers) */
static u16 swe[NCELL];                              /* snow lying on the ground as water (it reaches the river when it melts) */
static float sRun[MAXS + 1], sRech[MAXS + 1];
static float cl_alt(int c) { const u8 *r = CL(c); return (float)(r[1] | (r[2] << 8)); }
static float cl_T0(int c, int m) { return CL(c)[16 + m] / 5.0f - 20.0f; }
static float cl_P(int c, int m) { float s = CL(c)[28 + m] / 14.0f; return s * s; }
static float cl_RD(int c, int m) { return CL(c)[40 + m] / 10.0f; }
static float cl_RH(int c, int m) { return (float)CL(c)[52 + m]; }
static int cl_land(int c) { u8 t = vterr[c]; return !(t == 0 || t == 12 || t == 13 || t == 14 || t == 24 || (t >= 19 && t <= 23)); }

/* ---- small maths (no C library here) ---- */
static float fsqrt(float x) { if (x <= 0.0f) return 0.0f; union { float f; u32 i; } u; u.f = x; u.i = 0x1fbd1df5 + (u.i >> 1); float r = u.f; r = 0.5f * (r + x / r); r = 0.5f * (r + x / r); r = 0.5f * (r + x / r); return r; }
static float fexp(float x) { if (x > 60.0f) x = 60.0f; if (x < -60.0f) x = -60.0f; float k = x * 1.4426950f; int n = (int)(k >= 0 ? k + 0.5f : k - 0.5f); float y = x - n * 0.69314718f;
    float p = 1.0f + y * (1.0f + y * (0.5f + y * (0.16666667f + y * (0.041666667f + y * 0.0083333f)))); union { float f; u32 i; } u; u.i = (u32)(n + 127) << 23; return p * u.f; }
static float flog(float x) { if (x <= 1e-30f) return -69.0f; union { float f; u32 i; } u; u.f = x; int e = (int)((u.i >> 23) & 255) - 127; u.i = (u.i & 0x007fffff) | 0x3f800000; float m = u.f;
    float t = (m - 1.0f) / (m + 1.0f), t2 = t * t; return e * 0.69314718f + 2.0f * t * (1.0f + t2 * (0.33333333f + t2 * (0.2f + t2 * 0.142857f))); }
static float fpow(float x, float p) { return x <= 0.0f ? 0.0f : fexp(p * flog(x)); }
static float fcos(float x) { while (x > 3.14159265f) x -= 6.2831853f; while (x < -3.14159265f) x += 6.2831853f; float x2 = x * x; return 1.0f - x2 * (0.5f - x2 * (0.041666667f - x2 * (0.0013888889f - x2 * 0.0000248016f))); }
static float fsin(float x) { return fcos(x - 1.57079633f); }
static float sm(float e0, float e1, float x) { float t = (x - e0) / (e1 - e0); t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3.0f - 2.0f * t); }
static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float frand(void) { return (float)(rnd() & 0xffffff) / 16777216.0f; }
static float gauss(void) { return (frand() + frand() + frand() + frand() - 2.0f) * 1.7320508f; }   /* near-normal, within +-3.5 */
static float fabsf_(float x) { return x < 0 ? -x : x; }

/* ---- cell geometry ---- */
static float MX(int c) { return 375.5f + 7.0f * (c % 220); }
static float MY(int c) { int col = c % 220; return 138.5f + 8.0829f * ((c / 220) - 0.5f * (col & 1)); }
static int nb6(int c, int k)      /* the six neighbours (flat-top hexes, odd columns half a row up) */
{
    int x = c % 220, y = c / 220, nx, ny;
    static const int E[6][2] = { {0,-1},{0,1},{-1,0},{1,0},{-1,1},{1,1} }, O[6][2] = { {0,-1},{0,1},{-1,-1},{1,-1},{-1,0},{1,0} };
    if (x & 1) { nx = x + O[k][0]; ny = y + O[k][1]; } else { nx = x + E[k][0]; ny = y + E[k][1]; }
    if (nx < 0 || nx >= 220 || ny < 0 || ny >= 220) return -1; return ny * 220 + nx;
}
/* the four weather nodes around a cell and their weights */
static void node_mix(int c, int *n, float *w)
{
    float fx = (c % 220) / 219.0f * (GX - 1), fy = (c / 220) / 219.0f * (GX - 1);
    int x0 = (int)fx, y0 = (int)fy; if (x0 > GX - 2) x0 = GX - 2; if (y0 > GX - 2) y0 = GX - 2;
    float ax = fx - x0, ay = fy - y0;
    n[0] = y0 * GX + x0; n[1] = n[0] + 1; n[2] = n[0] + GX; n[3] = n[2] + 1;
    w[0] = (1 - ax) * (1 - ay); w[1] = ax * (1 - ay); w[2] = (1 - ax) * ay; w[3] = ax * ay;
}

/* ---- the year ---- */
typedef struct { float zr[GN], zt[GN], mw, ms; float rj[36][GN], tt[36][GN], wdx[36][GN], wdy[36][GN]; int year, valid; } Year;
static Year Y;
static void year_field(float *out)
{
    float c[16]; for (int k = 0; k < 16; k++) c[k] = gauss();
    for (int j = 0; j < GX; j++) for (int i = 0; i < GX; i++) {
        float fx = i / (float)(GX - 1) * 3.0f, fy = j / (float)(GX - 1) * 3.0f; int x0 = (int)fx, y0 = (int)fy; if (x0 > 2) x0 = 2; if (y0 > 2) y0 = 2;
        float ax = fx - x0, ay = fy - y0;
        float cv = c[y0 * 4 + x0] * (1 - ax) * (1 - ay) + c[y0 * 4 + x0 + 1] * ax * (1 - ay) + c[(y0 + 1) * 4 + x0] * (1 - ax) * ay + c[(y0 + 1) * 4 + x0 + 1] * ax * ay;
        out[j * GX + i] = (0.75f * cv / 0.72f + 0.66f * gauss()) / 1.0f; }
}
static const char *GRADE[5] = { "big drought", "dry", "normal", "wet", "very wet" };
static int grade_of(float z) { return z < -1.17f ? 0 : z < -0.33f ? 1 : z <= 0.33f ? 2 : z <= 1.17f ? 3 : 4; }
static void year_roll(int year)
{
    Y.year = year; Y.valid = 1;
    year_field(Y.zr); year_field(Y.zt);
    Y.mw = clampf(gauss(), -1.5f, 1.5f); Y.ms = clampf(gauss(), -1.5f, 1.5f);      /* winter / summer monsoon strength */
    for (int k = 0; k < GN; k++) { float northK = 1.0f - (k / GX) / 8.0f;            /* row 0 = north */
        Y.zt[k] -= 0.45f * Y.mw * northK;                                              /* a strong winter monsoon: the north colder */
        Y.zr[k] += 0.35f * Y.ms * (1.0f - northK) - 0.15f * Y.mw * northK; }        /* strong summer monsoon: the south wetter; strong winter monsoon: the north drier */
    for (int t = 0; t < 36; t++) { int m = t / 3 + 1; int winter = (m >= 9 || m <= 1), summer = (m >= 4 && m <= 6);
        float base = winter ? 2.3562f : summer ? -0.7854f : -9.0f;                   /* blowing toward: NW wind -> SE, SE wind -> NW; else any */
        float mon = winter ? Y.mw : summer ? Y.ms : 0.0f;
        for (int k = 0; k < GN; k++) { Y.rj[t][k] = 1.0f + 0.2f * (frand() * 2 - 1); Y.tt[t][k] = 0.8f * (frand() * 2 - 1);
            float a = base < -8.0f ? frand() * 6.2831853f : base + gauss() * 0.6f;
            float s = (0.35f + 0.65f * frand()) * (1.0f + 0.3f * mon) * (1.0f - 0.55f * roughT[k] / 255.0f);
            Y.wdx[t][k] = fcos(a) * s; Y.wdy[t][k] = fsin(a) * s; } }
    /* neighbouring nodes share their wind (one broad flow, not 81 separate gusts) */
    for (int t = 0; t < 36; t++) { float bx[GN], by[GN]; for (int k = 0; k < GN; k++) { float sx = 0, sy = 0, sw = 0; int kx = k % GX, ky = k / GX;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int x = kx + dx, y = ky + dy; if (x < 0 || x >= GX || y < 0 || y >= GX) continue; float w = (dx || dy) ? 0.5f : 1.0f; sx += Y.wdx[t][y * GX + x] * w; sy += Y.wdy[t][y * GX + x] * w; sw += w; }
            bx[k] = sx / sw; by[k] = sy / sw; }
        for (int k = 0; k < GN; k++) { Y.wdx[t][k] = bx[k]; Y.wdy[t][k] = by[k]; } }
    { char b[200], *p = b; p = pstr(p, "year "); p = pint(p, year); p = pstr(p, ": winter monsoon "); p = pint(p, (int)(Y.mw * 100)); p = pstr(p, "%, summer monsoon "); p = pint(p, (int)(Y.ms * 100));
      p = pstr(p, "%; rain grades N/C/S "); p = pstr(p, GRADE[grade_of(Y.zr[1 * GX + 5])]); *p++ = '/'; p = pstr(p, GRADE[grade_of(Y.zr[4 * GX + 4])]); *p++ = '/'; p = pstr(p, GRADE[grade_of(Y.zr[7 * GX + 4])]);
      p = pstr(p, "; cold anomaly N "); p = pint(p, (int)(Y.zt[1 * GX + 5] * 100)); *p = 0; logs(b); }
#ifdef HOSTTEST
    if (getenv("NORMALYEAR")) { for (int k = 0; k < GN; k++) { Y.zr[k] = 0; Y.zt[k] = 0; } }
#endif
}

/* ---- the turn's climate ---- */
static float cT[NCELL], cMult[NCELL], cP10[NCELL], cD[NCELL], cCR[NCELL], cWX[NCELL], cWY[NCELL];
static u8 cSnow[NCELL], willCov[NCELL]; static int swe_init;                           /* lying snow this turn: 0 none, 1 plain, 2 deep (hills / high) - the table unless the year's cold changes it */
static u8 cFrz[NCELL / 8 + 1], cFrzN[NCELL / 8 + 1];
static float ndT[GN], nMult[GN];                  /* node values of the turn (also sent to the map) */
static int cur_turn = -1;
static float TfY = 1.3085f, TfL = -4.907f;        /* freezing thresholds: Yellow River, Liaodong west coast (from the ice calibration) */
static int nw_hill(int c) { const u8 *r = CL(c); return r[4] >= 128 && ((vterr[c] >= 8 && vterr[c] <= 11) || r[5] >= 128 || cl_alt(c) > 600.0f); }
/* v26k12: the map's own snow strength (the picture's formula: a smooth 0..1 from the temperature, thin on the northwest plains) -
   used to find where the white cover is about to spread, so the snow is seen FALLING there first */
static float vis_snow(int t, float dT, int c)
{
    float tpos = (t / 3) + 2 + ((t % 3) - 1) / 3.0f; int tf = (int)tpos; float f = tpos - tf;
    int i0 = ((tf % 12) + 12) % 12, i1 = (i0 + 1) % 12;
    const u8 *r = CL(c); float nZ = r[3] / 255.0f, nw = r[4] / 255.0f, imp = r[5] / 255.0f, alt = cl_alt(c);
    float lapse = 0.0045f * alt + 1.2f * sm(0.25f, 0.75f, imp);
    float Ts = cl_T0(c, i0) * (1 - f) + cl_T0(c, i1) * f - lapse + dT - 4.0f * nZ; float nf = 1.0f - fabsf_(tpos - 10.0f) / 0.67f; if (nf < 0) nf = 0;
    if (nf > 0) { float Td = cl_T0(c, 11) - lapse + dT - 4.0f * nZ; float m = Ts < 0.5f * (Ts + Td) ? Ts : 0.5f * (Ts + Td); Ts = Ts * (1 - nf) + m * nf; }
    float s = sm(2.0f, -9.0f, Ts); if (nw > 0.02f && !nw_hill(c)) s *= 1.0f - 0.55f * nw;
    return s;
}
static void snow_ice_of(int t, float dT, int c, u8 *snow, u8 *frozen)
{
    float tpos = (t / 3) + 2 + ((t % 3) - 1) / 3.0f; int tf = (int)tpos; float f = tpos - tf; if (tpos < 0) { tf = (int)tpos - 1; f = tpos - tf; }
    int i0 = ((tf % 12) + 12) % 12, i1 = (i0 + 1) % 12, a0 = (i0 + 1) % 12, b0 = (i0 + 11) % 12, a1 = (i1 + 1) % 12, b1 = (i1 + 11) % 12;
    const u8 *r = CL(c); float nZ = r[3] / 255.0f, nw = r[4] / 255.0f, imp = r[5] / 255.0f, fm = r[6] / 255.0f, alt = cl_alt(c);
    float lapse = 0.0045f * alt + 1.2f * sm(0.25f, 0.75f, imp);
    float Tc = cl_T0(c, i0) * (1 - f) + cl_T0(c, i1) * f - lapse + dT;
    float gr = (cl_T0(c, a0) - cl_T0(c, b0)) * (1 - f) + (cl_T0(c, a1) - cl_T0(c, b1)) * f;
    float rising = sm(-1.5f, 1.5f, gr);
    *snow = 0; *frozen = 0;
    if (cl_land(c)) { float Ts = Tc - 4.0f * nZ - 0.5f; float nf = 1.0f - fabsf_(tpos - 10.0f) / 0.67f; if (nf < 0) nf = 0;
        if (nf > 0) { float Td = cl_T0(c, 11) - lapse + dT - 4.0f * nZ - 0.5f; float m = Ts < 0.5f * (Ts + Td) ? Ts : 0.5f * (Ts + Td); Ts = Ts * (1 - nf) + m * nf; }
        float need = rising > 0.5f ? 0.80f : 0.62f;
        if ((nw < 0.5f || nw_hill(c)) && sm(2.0f, -9.0f, Ts) >= need) *snow = ((vterr[c] >= 8 && vterr[c] <= 11) || alt > 900.0f) ? 2 : 1; }   /* v26k12: the dry northwest's foothills and highlands get snow too (its plains stay bare) */
    if (vterr[c] == T_GREATRIVER) { float Ti = Tc - 2.0f * nZ - 2.0f * rising * (1.0f - sm(4.0f, 8.0f, Tc)); int ice = (r[7] >> 5) & 3;
        if (ice == 1) *frozen = Ti < TfY; else if (ice == 2) *frozen = Ti < TfL; else *frozen = (fm * 3.0f > 1.0f ? 1.0f : fm * 3.0f) * sm(1.5f, -5.0f, Ti) >= 0.30f; }
}
static int frozen_dyn(int t, int c) { u8 s0, f0, s1, f1; int n[4]; float w[4]; node_mix(c, n, w); float dT = 0; for (int k = 0; k < 4; k++) dT += ndT[n[k]] * w[k];
    snow_ice_of(t, 0.0f, c, &s0, &f0); snow_ice_of(t, dT, c, &s1, &f1); return f0 == f1 ? frozen_wanted(t, c) : f1; }
/* v26k15: where in a region the rain falls more or less: damp air more, the lee of a high range less. Divided by the region's own
   mean, so it only shifts the rain inside the region - before, it cut the dry north twice (its rain days already say it is dry) */
static float provFac[18];
static float loc_fac(int c, int t)
{
    float tpos = (t / 3) + 2 + ((t % 3) - 1) / 3.0f; int tf = (int)tpos; float f = tpos - tf; int i0 = ((tf % 12) + 12) % 12, i1 = (i0 + 1) % 12;
    int m = t / 3 + 1; int winterW = (m >= 9 || m <= 1), summerW = (m >= 4 && m <= 6);
    float rh = cl_RH(c, i0) * (1 - f) + cl_RH(c, i1) * f; float a = clampf(0.8f + 0.4f * (rh - 60.0f) / 30.0f, 0.6f, 1.3f);
    float sh = winterW ? shNW[c] / 255.0f : summerW ? shSE[c] / 255.0f : 0.5f * (shNW[c] + shSE[c]) / 255.0f;
    return a * (1.0f - 0.7f * sh);
}
static int clim_gen;
static void climate_turn(int t)
{
    cur_turn = t; clim_gen++;
    { float sum[18]; int n[18]; for (int k = 0; k < 18; k++) { sum[k] = 0; n[k] = 0; }
      for (int c = 0; c < NCELL; c += 2) { if (!cl_land(c)) continue; int pv = prov[c] < 17 ? prov[c] : 17; sum[pv] += loc_fac(c, t); n[pv]++; }
      for (int k = 0; k < 18; k++) provFac[k] = n[k] ? sum[k] / n[k] : 1.0f; }
    static int swe_ok_dummy; (void)swe_ok_dummy;
    for (int k = 0; k < GN; k++) { nMult[k] = clampf(1.0f + 0.30f * Y.zr[k], 0.4f, 1.8f) * Y.rj[t][k]; ndT[k] = 0.9f * Y.zt[k] + Y.tt[t][k]; }
    float tpos = (t / 3) + 2 + ((t % 3) - 1) / 3.0f; int tf = (int)tpos; float f = tpos - tf;
    int i0 = ((tf % 12) + 12) % 12, i1 = (i0 + 1) % 12;
    int nextt = t == 35 ? 0 : t + 1;
    memset(cFrz, 0, sizeof(cFrz)); memset(cFrzN, 0, sizeof(cFrzN));
    for (int c = 0; c < NCELL; c++) {
        int n[4]; float w[4]; node_mix(c, n, w); float m = 0, dT = 0, wx = 0, wy = 0;
        for (int k = 0; k < 4; k++) { m += nMult[n[k]] * w[k]; dT += ndT[n[k]] * w[k]; wx += Y.wdx[t][n[k]] * w[k]; wy += Y.wdy[t][n[k]] * w[k]; }
        const u8 *r = CL(c); float imp = r[5] / 255.0f, alt = cl_alt(c);
        float lapse = 0.0045f * alt + 1.2f * sm(0.25f, 0.75f, imp);
        float Tc = cl_T0(c, i0) * (1 - f) + cl_T0(c, i1) * f - lapse + dT; cT[c] = Tc; cMult[c] = m;
        float Pm = (cl_P(c, i0) * (1 - f) + cl_P(c, i1) * f) * m; cP10[c] = Pm / 3.0f;
        cWX[c] = wx; cWY[c] = wy;
        if (cl_land(c)) { float days = (cl_RD(c, i0) * (1 - f) + cl_RD(c, i1) * f) * fsqrt(m);
            float rh = clampf((cl_RH(c, i0) * (1 - f) + cl_RH(c, i1) * f) + (m - 1) * 12.0f, 15.0f, 98.0f);
            float PET = 13.0f * (Tc > 0 ? Tc : 0) * (1.0f - rh / 100.0f); if (PET < 15.0f) PET = 15.0f;
            cD[c] = 0.6f * clampf(1.0f - Pm / PET, 0, 1) + 0.4f * clampf(1.0f - days / 12.0f, 0, 1); }
        else cD[c] = 0;
        u8 s0, f0, s1, f1; snow_ice_of(t, 0.0f, c, &s0, &f0); snow_ice_of(t, dT, c, &s1, &f1);
        cSnow[c] = (s0 == s1 && !nw_hill(c)) ? snw[t][c] : s1;                    /* the approved normal-year table; the anomaly only where it flips the state (v26k12: the northwest's hills have no table - the rule decides) */
        willCov[c] = 0;
        if (cl_land(c)) { float sC = vis_snow(t, dT, c), sN = vis_snow(nextt, dT, c);   /* v26k12: the white cover spreads here during this turn - the snow falls first */
            if (sN >= 0.20f && (sC < 0.20f ? sN - sC > 0.02f : sN - sC >= 0.15f)) willCov[c] = 1; }
        if (vterr[c] == T_GREATRIVER) { int coast = ((r[7] >> 5) & 3) == 2;         /* the Liaodong west coast is new: no table for it, the rule decides */
            if (coast ? f1 : (f0 == f1 ? frozen_wanted(t, c) : f1)) cFrz[c >> 3] |= (u8)(1 << (c & 7));
            u8 s2, f2, s3, f3; snow_ice_of(nextt, 0.0f, c, &s2, &f2); snow_ice_of(nextt, dT, c, &s3, &f3); if (coast ? f3 : (f2 == f3 ? frozen_wanted(nextt, c) : f3)) cFrzN[c >> 3] |= (u8)(1 << (c & 7)); }
        if (!swe_init && cSnow[c] && NSt && fcatch[c]) swe[c] = cSnow[c] >= 2 ? 80 : 30;   /* v26k14: a load in winter starts with the snow already lying */
        if (cSnow[c]) YS.snowA++; if (nw_hill(c) ? s0 : snw[t][c]) YS.snowN++;   /* v26k12: the northwest hills' normal is the rule's normal year */   /* v26k11 ledger */
        if (vterr[c] == T_GREATRIVER && ((r[7] >> 5) & 3) == 1) { if (frz_cur(c)) YS.iceA++; if (frozen_wanted(t, c)) YS.iceN++; }
        float wl = fsqrt(wx * wx + wy * wy);
        cCR[c] = (cSnow[c] || !cl_land(c)) ? 0.0f : fpow(cD[c], 1.3f) * sm(-3.0f, 10.0f, Tc) * (0.75f + 0.5f * wl);
    }
    swe_init = 1;
    { int nw2 = 0; for (int c = 0; c < NCELL; c++) nw2 += willCov[c]; if (nw2) { char b[80], *p = b; p = pstr(p, "first snow before the cover: cells "); p = pint(p, nw2); *p = 0; logs(b); } }   /* v26k12 */
}
static int frz_cur(int c) { return (cFrz[c >> 3] >> (c & 7)) & 1; }
static int frz_next(int c) { return (cFrzN[c >> 3] >> (c & 7)) & 1; }
static int van_turn = -1, van_val;
static int vanilla_now(u8 *d) { if (van_turn == cur_turn && cur_turn >= 0) return van_val; u8 *date = *(u8 **)(d + DB_DATE); u32 today = *(u32 *)(date + 0x450);
    TB(t_news); van_val = vanilla_disaster_recent(d, today); TE(t_news, "check of the game's own news", 0); van_turn = cur_turn; return van_val; }

/* ---- the days: rain systems drifting with the wind ---- */
static u8 wet[NCELL], crack[NCELL], heavyDays[NCELL], rainRun[NCELL]; static u16 dryd[NCELL];   /* rainRun: v26k10 days of the current spell */   /* rain[] and snowing[] are declared ahead of the speed hook in season_rules_v26.c */
/* ---- v26k11: the climate ledger - every year's counts next to their normal range (from many simulated years), written to
   SeasonStats.txt each turn; a value outside its range is marked, two years in a row the same way is marked "확인 필요".
   A brake stops a disaster for the rest of the year when its count runs far past the normal range (and says so). ---- */
static void stats_reset(void) { memset(&YS, 0, sizeof(YS)); memset(YSH, 0, sizeof(YSH)); nYSH = 0; YS.year = -1; }
static float latA[64 * 64], latB[64 * 64], latC[28 * 28], latD[28 * 28], latE[8 * 8], latF[8 * 8]; static float lphase; static float ox, oy;
static u8 ucdf[256]; static int ucdf_ok;
static float lat_at(const float *L, int n, float x, float y)
{
    float fx = x - (float)(int)(x / n) * n; if (fx < 0) fx += n; float fy = y - (float)(int)(y / n) * n; if (fy < 0) fy += n;
    int x0 = (int)fx, y0 = (int)fy; float ax = fx - x0, ay = fy - y0; int x1 = (x0 + 1) % n, y1 = (y0 + 1) % n;
    return L[y0 * n + x0] * (1 - ax) * (1 - ay) + L[y0 * n + x1] * ax * (1 - ay) + L[y1 * n + x0] * (1 - ax) * ay + L[y1 * n + x1] * ax * ay;
}
static void lat_fill(float *L, int n) { for (int i = 0; i < n * n; i++) L[i] = frand(); }
static float noise_cell(int c)
{
    float x = (c % 220) / 220.0f * 64.0f + ox, y = (c / 220) / 220.0f * 64.0f + oy;
    /* v26k: rain comes as SYSTEMS, not speckle - a regional tendency (about 27 cells across), the systems themselves (about
       8 cells, a district), and a little fine detail for the edges; neighbouring systems join into one larger mass */
    float a = lat_at(latA, 64, x, y) * (1 - lphase) + lat_at(latB, 64, x, y) * lphase;
    float b = lat_at(latC, 28, x * 0.4375f + 3.1f, y * 0.4375f + 1.7f) * (1 - lphase) + lat_at(latD, 28, x * 0.4375f + 3.1f, y * 0.4375f + 1.7f) * lphase;
    float e = lat_at(latE, 8, x * 0.125f + 0.7f, y * 0.125f + 2.3f) * (1 - lphase) + lat_at(latF, 8, x * 0.125f + 0.7f, y * 0.125f + 2.3f) * lphase;
    return a * 0.15f + b * 0.43f + e * 0.42f;
}
/* v26k27 (speed): noise_cell's lattice lookups split into a column part and a row part, computed once per day for the 220
   columns and 220 rows instead of once per cell - the same arithmetic in the same order, so the same values bit for bit */
typedef struct { int i0, i1; float a, b; } LatAx;     /* a = frac, b = 1 - frac */
static LatAx nzX[3][220], nzY[3][220];
static void lat_axis(LatAx *A, int n, float v)
{
    float fv = v - (float)(int)(v / n) * n; if (fv < 0) fv += n;
    int v0 = (int)fv; A->a = fv - v0; A->b = 1 - A->a; A->i0 = v0; A->i1 = (v0 + 1) % n;
}
static void noise_prep(void)
{
    for (int i = 0; i < 220; i++) {
        float x = i / 220.0f * 64.0f + ox, y = i / 220.0f * 64.0f + oy;
        lat_axis(&nzX[0][i], 64, x); lat_axis(&nzY[0][i], 64, y);
        lat_axis(&nzX[1][i], 28, x * 0.4375f + 3.1f); lat_axis(&nzY[1][i], 28, y * 0.4375f + 1.7f);
        lat_axis(&nzX[2][i], 8, x * 0.125f + 0.7f); lat_axis(&nzY[2][i], 8, y * 0.125f + 2.3f); }
}
static float lat_fast(const float *L, int n, const LatAx *X, const LatAx *Y)
{
    return L[Y->i0 * n + X->i0] * X->b * Y->b + L[Y->i0 * n + X->i1] * X->a * Y->b + L[Y->i1 * n + X->i0] * X->b * Y->a + L[Y->i1 * n + X->i1] * X->a * Y->a;
}
static float noise_fast(int c)
{
    int col = c % 220, row = c / 220;
    float a = lat_fast(latA, 64, &nzX[0][col], &nzY[0][row]) * (1 - lphase) + lat_fast(latB, 64, &nzX[0][col], &nzY[0][row]) * lphase;
    float b = lat_fast(latC, 28, &nzX[1][col], &nzY[1][row]) * (1 - lphase) + lat_fast(latD, 28, &nzX[1][col], &nzY[1][row]) * lphase;
    float e = lat_fast(latE, 8, &nzX[2][col], &nzY[2][row]) * (1 - lphase) + lat_fast(latF, 8, &nzX[2][col], &nzY[2][row]) * lphase;
    return a * 0.15f + b * 0.43f + e * 0.42f;
}
static void build_cdf(void)
{
    noise_prep();
    int h[256]; memset(h, 0, sizeof(h)); for (int c = 0; c < NCELL; c += 6) { int b = (int)(noise_fast(c) * 255.99f); if (b < 0) b = 0; if (b > 255) b = 255; h[b]++; }
    int tot = 0; for (int b = 0; b < 256; b++) tot += h[b]; int acc = 0;
    for (int b = 0; b < 256; b++) { acc += h[b]; ucdf[b] = (u8)(tot ? (acc * 255) / tot : b); } ucdf_ok = 1;
}
/* v26k12: drought cracks by each region's OWN normal (asked for): the dry-spell length that a normal climate reaches on only one
   warm dry day in ten there (simulated years, per weather node and season: spring / summer / autumn / winter of the game's calendar).
   A dry north needs a far longer spell (무위 200 days in spring) than the wet south (형주 30 days in autumn); 255 = never seen there -
   then 90 days. At least 30 days everywhere. */
static const u8 DRYTHR[81 * 4] = {
    255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 156,98,111,255, 107,106,71,255, 134,30,84,255, 122,142,63,255, 165,255,53,255,
    134,110,67,255, 117,96,71,255, 255,255,255,255, 145,154,136,255, 129,125,89,255, 133,132,76,255, 126,118,73,255, 104,112,63,255, 114,255,62,255,
    124,119,82,255, 98,90,44,255, 92,73,255,255, 102,115,67,255, 118,118,74,255, 121,134,77,255, 103,101,73,255, 138,164,73,255, 255,255,255,255,
    255,255,255,255, 113,116,255,255, 88,78,255,255, 95,75,59,255, 94,104,67,255, 92,99,77,255, 94,93,77,255, 98,87,77,255, 255,255,255,255,
    255,255,255,255, 53,30,30,255, 79,255,255,255, 59,255,255,255, 87,255,44,255, 54,255,77,255, 35,255,54,255, 65,255,62,255, 255,255,255,255,
    79,255,43,255, 76,255,61,255, 39,255,255,255, 255,255,30,255, 255,255,30,255, 255,255,85,255, 255,255,39,255, 255,255,255,255, 255,255,70,255,
    66,255,87,255, 62,255,83,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,49,255, 255,255,77,255, 255,255,30,255, 255,255,30,255,
    78,255,255,124, 75,255,255,255, 255,255,255,255, 255,255,98,255, 255,255,64,255, 255,255,73,66, 255,255,80,30, 255,255,78,44, 255,255,255,255,
    129,255,78,177, 80,255,255,255, 255,255,93,255, 255,255,97,134, 255,255,90,82, 255,255,69,71, 255,255,255,255, 255,255,255,255, 255,255,255,255,
};
static int dry_need(int c, int t) { int nx = (int)((c % 220) / 219.0f * (GX - 1) + 0.5f), ny = (int)((c / 220) / 219.0f * (GX - 1) + 0.5f); int v = DRYTHR[(ny * GX + nx) * 4 + (t / 9)]; return v == 255 ? 90 : v < 30 ? 30 : v; }
static float RIacc[4]; static float RInorm[4]; static int RIdays;
static int fire_cells_total, rain_cells_today, snow_cells_today; static float rain_expected;
static u16 cityCellL[128]; static int ncityL; static u8 heavyCity[128], snowCity[128]; static u32 wLast[2][128]; static int wTrack[2] = { -1, -1 }; static u8 wGap[2];   /* v26k13: the last day of a downpour / snowstorm over each city; the notice being followed to its end */   /* v26k6: days of a downpour / snowstorm over each city this turn (the one-line weather notice) */   /* v26k: the climate's own share, to check the field against */
static float pq[NCELL], hvq[NCELL]; static int pq_t = -1, pq_gen = -1;
static void rain_day(int t, int day_in_turn)
{
    /* the field drifts with the mean wind (4 cells a day per unit of wind), and slowly changes shape */
    float mx = 0, my = 0; for (int k = 0; k < GN; k++) { mx += Y.wdx[t][k]; my += Y.wdy[t][k]; } mx /= GN; my /= GN;
    ox -= mx * 3.0f * (64.0f / 220.0f); oy -= my * 3.0f * (64.0f / 220.0f);   /* v26k10: systems drift and change a little slower (longer spells) */
    lphase += 0.14f; if (lphase >= 1.0f) { lphase -= 1.0f; memcpy(latA, latB, sizeof(latA)); lat_fill(latB, 64); memcpy(latC, latD, sizeof(latC)); lat_fill(latD, 28); memcpy(latE, latF, sizeof(latE)); lat_fill(latF, 8); }
    build_cdf();                                      /* v26k: today's field ranked afresh (about 0.6 ms): the share of raining cells is the climate's every day, however the big systems fall */
    float tpos = (t / 3) + 2 + ((t % 3) - 1) / 3.0f; int tf = (int)tpos; float f = tpos - tf; int i0 = ((tf % 12) + 12) % 12, i1 = (i0 + 1) % 12;
    int m = t / 3 + 1; int winterW = (m >= 9 || m <= 1), summerW = (m >= 4 && m <= 6);
    int meiyu = (m == 4 || m == 5), shuAut = (m == 7 || m == 8);
    rain_cells_today = snow_cells_today = 0; rain_expected = 0; float rsrc[4] = { 0, 0, 0, 0 }; int nsrc[4] = { 0, 0, 0, 0 };
    /* v26k27 (speed): the chance and the downpour share of each cell, worked out once per turn (same formulas, same order) */
    if (pq_t != t || pq_gen != clim_gen) { pq_t = t; pq_gen = clim_gen;
        for (int c = 0; c < NCELL; c++) {
            if (!cl_land(c) && vterr[c] != T_GREATRIVER && vterr[c] != 13) continue;
            const u8 *r = CL(c);
            float days = cl_RD(c, i0) * (1 - f) + cl_RD(c, i1) * f; float mlt = cMult[c];
            float p = days / 30.0f * 0.6f * fsqrt(mlt);
            p *= loc_fac(c, t) / provFac[prov[c] < 17 ? prov[c] : 17];
            float lat = 45.3f - 25.2f * (MY(c) / 2048.0f);
            if (meiyu && lat > 27.5f && lat < 33.5f) p *= 1.25f;
            if (shuAut && r[11] == 11) p *= 1.2f;
            if (p > 0.95f) p = 0.95f; pq[c] = p;
            float hv = 0.18f * fpow(mlt, 0.8f); if (meiyu && lat > 27.5f && lat < 33.5f) hv *= 1.3f; if (shuAut && r[11] == 11) hv *= 1.3f; hvq[c] = clampf(hv, 0.10f, 0.30f); } }
    for (int c = 0; c < NCELL; c++) {
        if (!cl_land(c) && vterr[c] != T_GREATRIVER && vterr[c] != 13) { rain[c] = 0; continue; }
        const u8 *r = CL(c);
        /* a rain day here = real rain, so about 3/5 of the climate's rain days; v26k15 humid air / rain shadow move it within the
           region; 장마 band along the Yangtze x1.25, Sichuan's autumn rains x1.2 (all in pq[], worked out once per turn above) */
        float p = pq[c]; rain_expected += p; p *= 0.99f;   /* v26k10: the two-day minimum adds a few rain days back */
        float u = ucdf[(int)clampf(noise_fast(c) * 255.0f, 0, 255)] / 255.0f;
        /* v26k5: the QUALITY of the rain follows the year and the season - the downpour share of a system is 18% in a normal
           year, up to 27% in a very wet year (and x1.3 in the 장마 band / Sichuan's autumn rains), down to 10% in a big drought */
        float hv = hvq[c];
        u8 tier = 0; if (u < p) { float q = u / p; tier = q < hv ? 3 : q < hv + 0.34f ? 2 : 1; }
        if (!tier && rainRun[c] == 1) tier = rain[c] ? rain[c] : 1;      /* v26k10: no one-day showers - a spell lasts at least two days */
        if (!tier && willCov[c] && day_in_turn <= 1) tier = frand() < 0.7f ? 2 : 1;   /* v26k12: where the white cover spreads this turn, snow falls in its first two days */
        rainRun[c] = tier ? (u8)(rainRun[c] < 250 ? rainRun[c] + 1 : 250) : 0;
        rain[c] = tier; snowing[c] = (tier && (cT[c] < 1.0f || willCov[c])) ? 1 : 0;
        if (NSt) { int s = fcatch[c]; if (s >= 1 && s <= NSt) {   /* v26k14: the day's water on the land that drains to a river stretch */
            float P = tier == 3 ? 25.0f : tier == 2 ? 8.0f : tier == 1 ? 2.0f : 0.0f, Wt = 0.0f;
            if (snowing[c]) { int v = swe[c] + (int)P; swe[c] = (u16)(v > 60000 ? 60000 : v); } else Wt = P;   /* snow waits on the ground */
            if (swe[c] && cT[c] > 0.0f) { float m = 3.0f * cT[c] + 0.5f; if (m > swe[c]) m = swe[c]; swe[c] = (u16)(swe[c] - (int)m); Wt += (float)(int)m; }   /* it melts with the warmth */
            float k = 0.10f + 0.60f * wet[c] / 255.0f;              /* soaked ground sends most of it to the river, dry ground drinks it */
            sRun[s] += Wt * k; sRech[s] += Wt * (1.0f - k) * 0.25f; } }
        if (tier) { rain_cells_today++; if (snowing[c]) snow_cells_today++; int add = tier == 3 ? 70 : tier == 2 ? 40 : 10; int wv = wet[c] + add; wet[c] = (u8)(wv > 255 ? 255 : wv); dryd[c] = 0; if (tier >= 2 && !snowing[c]) heavyDays[c] = (u8)(heavyDays[c] + (tier == 3 ? 2 : 1)); }   /* v26k12: snow lies on the slope, it does not wash it down (the thaw counts later) */   /* v26k5: half-days - rain counts half, a downpour a full day */
        else { int dec = 18 + (int)(30.0f * cD[c]); wet[c] = (u8)(wet[c] > dec ? wet[c] - dec : 0); if (dryd[c] < 60000) dryd[c]++; }
        /* drought cracks: a long dry spell in warm, dry weather; closed again by real rain */
        if (tier >= 2) crack[c] = 0;
        else if (!crack[c] && cl_land(c) && dryd[c] >= 30 && cT[c] > 12.0f && cD[c] > 0.55f && (vterr[c] == 1 || vterr[c] == 2 || vterr[c] == 3 || vterr[c] == 15) && dryd[c] >= dry_need(c, t)) crack[c] = 1;   /* v26k12: the region's own normal */
        u8 sb = r[15]; if (sb) { float wgt = snowing[c] ? 0.0f : tier == 3 ? 3.0f : tier == 2 ? 1.5f : tier == 1 ? 0.5f : 0.0f;   /* v26k12: snowfall reaches the river when it melts (the thaw), not twice */ for (int rv = 1; rv <= 3; rv++) if (sb & (1 << (rv - 1))) { rsrc[rv] += wgt; nsrc[rv]++; } }
    }
#ifdef HOSTTEST
    if (getenv("DRYDUMP")) { static FILE *df; if (!df) df = fopen(getenv("DRYDUMP"), "ab"); int ss = (t / 3) / 3;
        for (int c = 0; c < NCELL; c += 3) { if (!cl_land(c) || !(cT[c] > 12.0f && cD[c] > 0.55f && (vterr[c] == 1 || vterr[c] == 2 || vterr[c] == 3 || vterr[c] == 15))) continue;
            int nx = (int)((c % 220) / 219.0f * (GX - 1) + 0.5f), ny = (int)((c / 220) / 219.0f * (GX - 1) + 0.5f); unsigned char rec[4] = { (unsigned char)(ny * GX + nx), (unsigned char)ss, (unsigned char)(dryd[c] > 250 ? 250 : dryd[c]), 0 }; fwrite(rec, 1, 4, df); } }
    if (getenv("REGDBG")) { static int a[17], sn[17], rn[17], hv[17], tb[17], cv[17], cr[17]; static float tt[17], fr[17]; memset(a, 0, sizeof a); memset(sn, 0, sizeof sn); memset(rn, 0, sizeof rn); memset(hv, 0, sizeof hv); memset(tb, 0, sizeof tb); memset(cv, 0, sizeof cv); memset(cr, 0, sizeof cr); memset(tt, 0, sizeof tt); memset(fr, 0, sizeof fr);
        for (int c = 0; c < NCELL; c++) { int pv = prov[c]; if (pv > 16 || !cl_land(c)) continue; a[pv]++; if (snowing[c]) sn[pv]++; else if (rain[c]) rn[pv]++; if (rain[c] >= 3) hv[pv]++; if (snw[t][c]) tb[pv]++; if (cSnow[c]) cv[pv]++; if (crack[c]) cr[pv]++; tt[pv] += cT[c]; fr[pv] += cCR[c]; }
        for (int pv = 0; pv < 17; pv++) if (a[pv]) printf("reg t%d p%d n %d sn %d rn %d hv %d tb %d cv %d cr %d T %.1f fr %.3f\n", t, pv, a[pv], sn[pv], rn[pv], hv[pv], tb[pv], cv[pv], cr[pv], tt[pv] / a[pv], fr[pv] / a[pv]); }
    if (getenv("SOUTHSNOW")) { for (int c = 0; c < NCELL; c++) if (snowing[c] && prov[c] >= 13 && prov[c] <= 16 && cl_land(c) && (rnd() % 50) == 0) printf("ss p%d c%d alt %.0f T %.1f terr %d\n", prov[c], c, cl_alt(c), cT[c], vterr[c]); }
    if (getenv("SNOWDBG")) { int pv = atoi(getenv("SNOWDBG")); int a = 0, sn = 0, rn = 0, hv = 0, tb = 0, cv = 0, cr = 0; float tt = 0, dd = 0, ww = 0, cr2 = 0; for (int c = 0; c < NCELL; c++) { if (prov[c] != pv || !cl_land(c)) continue; a++; if (snowing[c]) sn++; else if (rain[c]) rn++; if (rain[c] >= 3) hv++; if (snw[t][c]) tb++; if (cSnow[c]) cv++; if (crack[c]) cr++; tt += cT[c]; dd += cD[c]; ww += wet[c]; cr2 += cCR[c]; } printf("snowdbg t%d cells %d snow %d rain %d heavy %d table %d cover %d crack %d T %.1f dry %.2f wet %.0f firerisk %.3f\n", t, a, sn, rn, hv, tb, cv, cr, tt / a, dd / a, ww / a, cr2 / a); }
    if (getenv("RAINPROV")) { static FILE *rp; if (!rp) rp = fopen(getenv("RAINPROV"), "w"); int cnt[18] = {0}, sn = 0; for (int c = 0; c < NCELL; c++) { if (!cl_land(c) || !rain[c]) continue; cnt[prov[c] < 17 ? prov[c] : 17]++; if (snowing[c]) sn++; }
        fprintf(rp, "%d %d", g_syear, t); for (int k = 0; k < 18; k++) fprintf(rp, " %d", cnt[k]); fprintf(rp, " %d %.0f\n", sn, rain_expected); }
    { static FILE *rm; if (!rm && getenv("RAINMAP")) rm = fopen(getenv("RAINMAP"), "wb"); if (rm) { unsigned char hd[4] = { (unsigned char)t, (unsigned char)day_in_turn, (unsigned char)grade_of(Y.zr[40]), 0 }; fwrite(hd, 1, 4, rm); for (int c = 0; c < NCELL; c++) { unsigned char v = (unsigned char)(rain[c] | (snowing[c] << 2)); fwrite(&v, 1, 1, rm); } fflush(rm); } }
    { static FILE *rdd; if (!rdd && getenv("RAINDAILY")) rdd = fopen(getenv("RAINDAILY"), "wb"); if (rdd) { fwrite(rain, 1, NCELL, rdd); fflush(rdd); } }
    if (getenv("CRACKREG")) { static long cd[17], sd[17], fs[17]; static int yr0 = -1; static int cmx[17]; { int cn[17] = {0}; for (int c = 0; c < NCELL; c++) if (crack[c] && prov[c] < 17) cn[prov[c]]++; for (int p2 = 0; p2 < 17; p2++) if (cn[p2] > cmx[p2]) cmx[p2] = cn[p2]; } for (int c = 0; c < NCELL; c++) { if (prov[c] > 16) continue; if (crack[c]) cd[prov[c]]++; if (snowing[c]) { sd[prov[c]]++; if (willCov[c] && day_in_turn <= 1) fs[prov[c]]++; } }
        if (t == 35 && day_in_turn >= 8) { if (yr0 != g_year) { yr0 = g_year; printf("CRACKREG"); for (int p2 = 0; p2 < 17; p2++) printf(" %ld", cd[p2]); printf("\nCRACKMAX"); for (int p2 = 0; p2 < 17; p2++) printf(" %d", cmx[p2]); memset(cmx, 0, sizeof cmx); printf("\nSNOWREG"); for (int p2 = 0; p2 < 17; p2++) printf(" %ld/%ld", sd[p2], fs[p2]); printf("\n"); memset(cd, 0, sizeof cd); memset(sd, 0, sizeof sd); memset(fs, 0, sizeof fs); } } }
    if (getenv("RAINDBG")) printf("day t%d: rain %d expected %d lphase %.2f\n", t, rain_cells_today, (int)rain_expected, lphase);
#endif
    for (int rv = 1; rv <= 3; rv++) if (nsrc[rv]) RIacc[rv] += rsrc[rv] / nsrc[rv];
    RIdays++;
    { YS.rainAct += rain_cells_today; YS.rainExp += rain_expected; int ncr = 0; for (int c = 0; c < NCELL; c++) if (crack[c]) ncr++; if (ncr > YS.crackMax) YS.crackMax = ncr; }   /* v26k11 ledger */
}
static void city_weather_count(void)   /* v26k6: counted after the day's rain is final (the test's forced rain included) */
{ u32 now = today_day();
  for (int k = 0; k < ncityL && k < 128; k++) { int c = cityCellL[k]; if (rain[c] >= 3) { if (snowing[c]) snowCity[k]++; else heavyCity[k]++; wLast[snowing[c] ? 1 : 0][k] = now; } }
  for (int kind = 0; kind < 2; kind++) { int k = wTrack[kind]; if (k < 0 || k >= ncityL) continue; int c = cityCellL[k];   /* v26k13 (asked for): the day it stops is said */
      if (rain[c] >= 3 && (snowing[c] ? 1 : 0) == kind) { wGap[kind] = 0; continue; }
      if (++wGap[kind] < 2) continue;
      u8 *d = db(); u8 *f = d ? nearest_city(d, c, 2 * 2) : 0; rset(L"[날씨] "); if (f) raddname(f); radd(kind ? L" 일대의 눈보라가 그쳤습니다." : L" 일대의 폭우가 그쳤습니다."); radd_when(0); report_ex(c, 0); wTrack[kind] = -1; } }
static void ri_norm_init(void)          /* the river's own normal peak: the mean of its three wettest turns in a normal year (expected rain weight per cell-day) */
{
    for (int rv = 1; rv <= 3; rv++) { float v[36]; for (int t = 0; t < 36; t++) { float tpos = (t / 3) + 2 + ((t % 3) - 1) / 3.0f; int tf = (int)tpos; float f = tpos - tf; int i0 = ((tf % 12) + 12) % 12, i1 = (i0 + 1) % 12;
            float s = 0; int n = 0; for (int c = 0; c < NCELL; c += 2) { if (!(CL(c)[15] & (1 << (rv - 1)))) continue; float days = cl_RD(c, i0) * (1 - f) + cl_RD(c, i1) * f; float p = days / 30.0f; if (p > 0.95f) p = 0.95f; s += p * 1.35f; n++; }
            v[t] = n ? s / n : 0; }
        for (int i = 0; i < 36; i++) for (int j = i + 1; j < 36; j++) if (v[j] > v[i]) { float x = v[i]; v[i] = v[j]; v[j] = x; }
        RInorm[rv] = (v[0] + v[1] + v[2]) / 3.0f; if (RInorm[rv] < 0.05f) RInorm[rv] = 0.05f; }
}

/* ---- fires: start small, every turn one independent roll - out / as it is / spreading (uphill, downwind, into fuel) ---- */
typedef struct { u16 cells[64]; int n, age, start, active, tier, ember, hurtTurns; u32 seq; int city; int doused, rainSaid; u32 day0; } Fire;   /* v26k2: doused = cells the rain put out */
static Fire fires[12]; static int nfires_total;
static u8 ash[NCELL], ashLeft[NCELL];
static float K_IGN = 0.12f, K_CRH = 0.22f;
static int fire_has(Fire *F, int c) { for (int i = 0; i < F->n; i++) if (F->cells[i] == c) return 1; return 0; }
static int burnable(int c) { return fuelT[c] > 0 && !cSnow[c] && !ash[c] && !burning[c] && !flooded[c]; }   /* v26e: not into flood water */
static void fire_spread(Fire *F, int target)
{
    int guard = 0;
    while (F->n < target && F->n < 60 && guard++ < 400) {
        int fr[256]; float fw[256]; int nf2 = 0; float tot = 0;
        for (int i = 0; i < F->n && nf2 < 240; i++) { int c = F->cells[i];
            for (int k = 0; k < 6; k++) { int n = nb6(c, k); if (n < 0 || fire_has(F, n) || !burnable(n) || rain[n] >= 2) continue;   /* v26k5: no spreading into rain */
                float up = cl_alt(n) > cl_alt(c) + 60.0f ? 2.0f : 1.0f; float dx = MX(n) - MX(c), dy = MY(n) - MY(c); float dl = fsqrt(dx * dx + dy * dy); if (dl < 1) dl = 1;
                float wl = fsqrt(cWX[c] * cWX[c] + cWY[c] * cWY[c]); float wind = 1.0f + 0.8f * (wl > 0 ? (dx * cWX[c] + dy * cWY[c]) / dl : 0);
                float w = fuelT[n] / 60.0f * up * (wind < 0.2f ? 0.2f : wind) * (rain[n] == 1 ? 0.5f : 1.0f); fr[nf2] = n; fw[nf2] = w; nf2++; tot += w; } }   /* v26k5: drizzle halves it */
        if (!nf2 || tot <= 0) break;
        float u = frand() * tot; int k = 0; while (k < nf2 - 1 && u > fw[k]) { u -= fw[k]; k++; }
        F->cells[F->n++] = (u16)fr[k]; burning[fr[k]] = 1;
    }
}
static void fire_ember(Fire *F)          /* a spark carried 2-4 cells downwind */
{
    int c = F->cells[rnd() % (u32)F->n]; if (rain[c] >= 3) return;   /* v26k5: no sparks fly in a downpour */
    float wl = fsqrt(cWX[c] * cWX[c] + cWY[c] * cWY[c]);
    float ang = frand() * 6.2831853f; float dx = wl > 0.01f ? cWX[c] / wl : fcos(ang), dy = wl > 0.01f ? cWY[c] / wl : fsin(ang);
    float dist = 2.0f + frand() * 2.0f; float rr = (frand() - 0.5f) * 1.2f;                /* a little off the wind line */
    float ex = dx * fcos(rr) - dy * fsin(rr), ey = dx * fsin(rr) + dy * fcos(rr);
    int x = c % 220 + (int)(ex * dist + (ex > 0 ? 0.5f : -0.5f)), y = c / 220 + (int)(ey * dist + (ey > 0 ? 0.5f : -0.5f)); if (x < 0 || x >= 220 || y < 0 || y >= 220) return;
    int n = y * 220 + x; if (fire_has(F, n) || !burnable(n) || rain[n] >= 2 || F->n >= 60) return; F->cells[F->n++] = (u16)n; burning[n] = 1; F->ember++;
}
static const u16 *fire_name(int n) { return n < 10 ? L"산불" : n < 30 ? L"큰 산불" : L"대산불"; }
static int fire_tier(int n) { return n < 10 ? 0 : n < 30 ? 1 : 2; }
static void fire_news_start(u8 *d, Fire *F)
{
    u8 *f = nearest_city(d, F->cells[0], 30 * 30);
    rset(L"[재해] "); if (f) { raddname(f); radd(L" 부근 "); } radd(L"산에 산불이 났습니다. ("); raddn(F->n); radd(L"칸)"); report(F->cells[0]);
}
static u8 *area_city(u8 *d, u8 *area)                 /* the city object an area belongs to */
{
    int fid = area[0x35]; if (fid > 0x33) return 0; u8 *f = *(u8 **)(d + DB_FOOTHOLD + 8 * (u64)fid); if (!rok(f, 0x60)) return 0;
    void **vt = *(void ***)f; if ((u8 *)vt != game + VT_FOOTHOLD && (u8 *)vt != game + VT_CITYDATA) return 0; return f;
}
static int area_faction(u8 *d, u8 *area) { u8 *c = area_city(d, area); return c ? *(u16 *)(c + 0x4e) : -1; }
static void fire_end(u8 *d, Fire *F, int t, int byrain)
{
    int left = 3 + (int)(rnd() % 5); int n = F->n + F->doused;                       /* v26k2: the whole burnt area, the rained-out cells included */
    YS.fireCells += n; if (n >= 30) YS.fireGreat++; else if (n >= 10) YS.fireBig++;   /* v26k11 ledger */
    for (int i = 0; i < F->n; i++) { int c = F->cells[i]; burning[c] = 0; ash[c] = 1; ashLeft[c] = (u8)left; }
    int tier = fire_tier(n);
    u8 *f = nearest_city(d, F->cells[0], 30 * 30);
    /* v26k13 (asked for): every fire's end is said (the small ones too), with the day and how long it burned */
    if (byrain) { rset(L"[재해] 비가 내려 "); if (f) { raddname(f); radd(L" 부근 "); } radd(fire_name(n)); radd(L"이 꺼졌습니다."); radd_when(F->day0); report_ex(F->cells[0], tier >= 1); }
    else { rset(L"[재해] "); if (f) { raddname(f); radd(L" 부근 "); } radd(fire_name(n)); radd(L"이 꺼졌습니다. 탄 자리 "); raddn(n); radd(L"칸"); radd_when(F->day0); report_ex(F->cells[0], 0); }
    /* the people: the nearest base loses part of its population; most move to the nearest city of the same lord, the rest are gone */
    { int bestA = 0, bd = 400; for (int i = 1; i <= 500; i++) { u8 *a = foot(d, i); if (!a) continue; int dd = cdist2(*(u16 *)(a + 0x3a), F->cells[0]); if (dd < bd) { bd = dd; bestA = i; } }
      if (bestA) { u8 *a = foot(d, bestA); int pop = *(u16 *)(a + 0x52); int pct = tier == 0 ? 1 : tier == 1 ? 3 : 6; int lose = pop * pct / 100;
        if (sr_domestic && lose > 0 && pop > 100) { int fac = area_faction(d, a); int moved = lose * 9 / 10; sr_cb_population(a, pop - lose);
            u8 *dst = 0; int bdd = 1 << 30;
            for (int i = 1; i <= 500; i++) { if (i == bestA) continue; u8 *b = foot(d, i); if (!b) continue; u8 *h = cellp(d, *(u16 *)(b + 0x3a)); if (!h || h[0x10] != 16) continue;
                if (fac >= 0 && area_faction(d, b) != fac) continue; int dd = cdist2(*(u16 *)(b + 0x3a), *(u16 *)(a + 0x3a)); if (dd < bdd) { bdd = dd; dst = b; } }
            if (dst) { int p2 = *(u16 *)(dst + 0x52); int np = p2 + moved; sr_cb_population(dst, np); }
            char b[200], *p = b; p = pstr(p, "fire end: base "); p = putf8(p, (u16 *)(a + 0x10), 4); p = pstr(p, " pop "); p = pint(p, pop); p = pstr(p, " -"); p = pint(p, lose); p = pstr(p, " moved "); p = pint(p, dst ? moved : 0);
            if (dst) { p = pstr(p, " to "); p = putf8(p, (u16 *)(dst + 0x10), 4); } p = pstr(p, " faction "); p = pint(p, fac); *p = 0; logs(b);
            if (tier >= 1) { rset(L"[재해] 산불로 "); raddname(a); radd(L"의 백성 "); raddn(lose); radd(L"명이 "); if (dst) { raddname(dst); radd(L"(으)로 옮겨 갔습니다."); } else radd(L"흩어졌습니다."); report_ex(*(u16 *)(a + 0x3a), 0); } } } }
    F->active = 0; F->n = 0;
    char b[120], *p = b; p = pstr(p, "fire ended: cells "); p = pint(p, n); p = pstr(p, " turns "); p = pint(p, F->age); p = pstr(p, byrain ? " (rain)" : ""); *p = 0; logs(b); (void)t;
}
static void fire_hurt(u8 *d, Fire *F)
{
    int tier = fire_tier(F->n); int lo = tier == 0 ? 5 : tier == 1 ? 10 : 15, hi = lo + 5;
    for (int i = 0; i < F->n; i++) { int c = F->cells[i]; if (!occ[c]) continue; u8 *a = army(d, occ[c]); if (a && chance(7000)) hurt(a, rint(lo, hi), rint(lo, hi), "fire"); }
}
static int fire_start_at(u8 *d, int c0, int want, int t, int forced)
{
    if (!forced && vanilla_now(d)) return 0;                      /* the game's own disaster is in the news: ours waits */
    Fire *F = 0; for (int i = 0; i < 12; i++) if (!fires[i].active) { F = &fires[i]; break; } if (!F) return 0;
    memset(F, 0, sizeof(*F)); F->active = 1; F->start = t; F->day0 = today_day(); F->n = 1; F->cells[0] = (u16)c0; burning[c0] = 1; ev_seq++; F->seq = ev_seq;
    fire_spread(F, want); if (F->n < 2 && !forced) { for (int i = 0; i < F->n; i++) burning[F->cells[i]] = 0; F->active = 0; F->n = 0; return 0; }
    nfires_total++; YS.fires++; fire_hurt(d, F); fire_news_start(d, F);
    for (int i = 0; i < F->n && i < 8; i++) vis_add(1, F->cells[i], t, 3);
    char b[100], *p = b; p = pstr(p, "fire start cell "); p = pint(p, c0); p = pstr(p, " cells "); p = pint(p, F->n); *p = 0; logs(b);
    return 1;
}
static void fires_turn(u8 *d, int t)
{
    /* ash fades */
    for (int c = 0; c < NCELL; c++) if (ashLeft[c]) { ashLeft[c]--; if (!ashLeft[c]) ash[c] = 0; }
    for (int i = 0; i < 12; i++) { Fire *F = &fires[i]; if (!F->active) continue; F->age++;
        float r = 0; for (int j = 0; j < F->n; j++) r += cCR[F->cells[j]]; r = clampf(r / F->n / K_CRH, 0, 1);
        float wetf = 0; for (int j = 0; j < F->n; j++) wetf += wet[F->cells[j]] / 255.0f; wetf /= F->n;
        float pO = 0.70f + (0.15f - 0.70f) * r + 0.25f * wetf, pH = 0.25f + (0.35f - 0.25f) * r; float u = frand();   /* v26k4: the wet-ground term stays (asked for), on top of the cell-by-cell rain dousing */
        int out = F->age >= 3 || u < pO;
        if (!out && u >= pO + pH) { int before = F->n; int target = (int)(F->n * (1.5f + 1.5f * frand()) + 0.5f); if (target > 60) target = 60; fire_spread(F, target); if (frand() < 0.20f) fire_ember(F);
            int t0 = fire_tier(before), t1 = fire_tier(F->n);
            if (t1 > t0) { u8 *f = nearest_city(d, F->cells[0], 30 * 30); rset(L"[재해] "); if (f) { raddname(f); radd(L" 부근 "); } radd(L"산불이 "); radd(fire_name(F->n)); radd(L"로 번졌습니다. ("); raddn(F->n); radd(L"칸)"); report(F->cells[0]);
                for (int j = 0; j < F->n && j < 8; j++) { int dup = 0; for (int s = 0; s < 16; s++) if (vis[s].type == 1 && vis[s].cell == F->cells[j]) dup = 1; if (!dup) vis_add(1, F->cells[j], t, 3); }
                /* a big fire hurts the nearest city: public order; the great fire also its food store */
                if (f) { int fc = *(u16 *)(f + 0x3a); u8 *h = cellp(d, fc); if (h && h[0x10] == 16) { int o0 = f[0x54]; int dec = t1 == 1 ? rint(5, 10) : rint(10, 20); int o1 = o0 - dec; if (o1 < 0) o1 = 0; if (o0 <= 100) { sr_cb_order(f, o1); o1 = f[0x54]; } else o1 = o0;
                        int took = t1 == 2 ? city_take_food(d, f, rint(3, 6)) : 0;
                        if (o1 != o0) { rset(L"[재해] 산불로 "); raddname(f); radd(L"의 치안이 "); raddn(o0 - o1); if (took) { radd(L", 병량이 "); raddn(took); } radd(L" 줄었습니다."); report_ex(fc, 0); } else if (took) { rset(L"[재해] 산불로 "); raddname(f); radd(L"의 병량이 "); raddn(took); radd(L" 줄었습니다."); report_ex(fc, 0); } } } } }
        if (out) fire_end(d, F, t, 0); else fire_hurt(d, F); }
    /* new fires: climate (dry, warm, wind) x fuel x people; never on the first turn of 1/4/7/10월; not on snow, not on fresh ash, not on wet ground */
    if (!banned_turn(t) && YS.fires >= 15) { if (!YS.brake[BR_FIRE]) { YS.brake[BR_FIRE] = 1; logs("ledger brake: fires"); } }   /* v26k11: far past the normal range */
    else if (!banned_turn(t)) { int made = 0;
        for (int c = 0; c < NCELL && made < 3; c += 1) { if (!fuelT[c] || cSnow[c] || ash[c] || burning[c] || cCR[c] <= 0) continue;
            float fu = fuelT[c] / 60.0f; float wetf = 1.0f - wet[c] / 255.0f;
            int dry = crack[c]; for (int k = 0; k < 6 && !dry; k++) { int q = nb6(c, k); if (q >= 0 && crack[q]) dry = 1; }   /* v26k6: cracked (drought) ground: x1.5 */
            float p = K_IGN * cCR[c] * (fu * fu / 9.0f) * (peopleT[c] / 100.0f) * 0.001f * wetf * wetf * (dry ? 1.5f : 1.0f);
            if (frand() < p) { if (fire_start_at(d, c, 2 + (int)(rnd() % 3), t, 0)) made++; } } }
    fire_cells_total = 0; for (int i = 0; i < 12; i++) if (fires[i].active) fire_cells_total += fires[i].n;
}
static void fires_day(u8 *d, int t)       /* v26k2: rain puts out the CELLS it falls on (asked for) - a downpour 60% a day each, rain 30%, drizzle nothing; the fire ends only when no cell is left */
{
    for (int i = 0; i < 12; i++) { Fire *F = &fires[i]; if (!F->active) continue;
        int out = 0, ended = 0;
        for (int j = 0; j < F->n; ) { int c = F->cells[j]; u8 rt = rain[c]; float p = rt >= 3 ? 0.60f : rt == 2 ? 0.30f : 0.0f;
            if (p <= 0.0f || frand() >= p) { j++; continue; }
            if (F->n == 1) { fire_end(d, F, t, 1); ended = 1; out++; break; }      /* the last cell: the fire is out */
            burning[c] = 0; ash[c] = 1; ashLeft[c] = (u8)(3 + (int)(rnd() % 5)); F->cells[j] = F->cells[F->n - 1]; F->n--; F->doused++; out++; }
        if (!out) continue;
        if (!ended && !F->rainSaid && F->n + F->doused >= 10 && F->doused >= 3 && F->doused * 5 >= F->n + F->doused) {   /* said once, when a fifth (at least 3 cells) is out */ F->rainSaid = 1; u8 *f = nearest_city(d, F->cells[0], 30 * 30);
            rset(L"[재해] 비가 내려 "); if (f) { raddname(f); radd(L" 부근 "); } radd(fire_name(F->n + F->doused)); radd(L"이 "); raddn(F->doused); radd(L"칸 꺼졌습니다. (타는 중 "); raddn(F->n); radd(L"칸)"); report_ex(F->cells[0], 0); }
        char b[100], *q = b; q = pstr(q, "rain on fire: cells out "); q = pint(q, out); q = pstr(q, ended ? " (fire ended)" : " left "); if (!ended) q = pint(q, F->n); *q = 0; logs(b); }
}

/* ---- v26k14 floods: the water itself. Each of the three rivers (main stems only, end to end) is cut into stretches. Every day the
   rain and the melting snow on the land that drains to a stretch run off into it - more of it when the ground is already soaked,
   little on dry ground; snow waits on the ground until it melts. The water flows down stretch by stretch (a day each), joined by
   what comes from above. Where it rises over the banks the stretch floods - the lowest ground first, the higher the water the
   further it spreads; high ground stays dry. When it falls, the higher ground comes out first. The banks are as high as the
   flow a normal climate brings about once in two years (the rule natural rivers follow), raised a quarter by the dikes. How
   often and when floods come is not set anywhere: it is what the rain does. ---- */
static const float LEVEE = 1.25f, BIG = 1.5f, GREAT = 1.8f;   /* over the banks the water spreads fast: a little more flow covers much more of the flat land */
static const u16 *FLN[4] = { L"", L"홍수", L"큰 홍수", L"대홍수" }; static const u16 *RIVB[8] = { L"황하", L"장강", L"한수", L"사수", L"여수", L"원수·상수", L"상수", L"반강" };
static int newsDay[9]; static int newsJoin[9][4], nJoin[9];
static void fire_douse_cell(int c)                /* v26e: water over a burning cell: that cell goes out (ash), the fire shrinks */
{
    for (int i = 0; i < 12; i++) { Fire *F = &fires[i]; if (!F->active) continue;
        for (int j = 0; j < F->n; j++) if (F->cells[j] == c) { F->cells[j] = F->cells[F->n - 1]; F->n--; break; }
        if (F->n == 0 && F->active) { F->active = 0; u8 *d = db(); u8 *f = d ? nearest_city(d, c, 30 * 30) : 0;   /* v26k13: said too */
            rset(L"[재해] 홍수 물에 "); if (f) { raddname(f); radd(L" 부근 "); } radd(fire_name(F->doused + 1)); radd(L"이 꺼졌습니다."); radd_when(F->day0); report_ex(c, 0); } }
    burning[c] = 0; ash[c] = 1; ashLeft[c] = 3;
}
static void flood_walls(u8 *d, int s)              /* earthen / stone walls standing in the water hold it back from the ground behind them up to a big flood */
{
    int rv = sdesc[s - 1].river, L = sx[s].L;
    for (int i = 1; i <= 3000; i++) { u8 *b = *(u8 **)(d + DB_BUILD + 8 * (u64)i); if (!b) continue; if (b != bld_ok[i]) { if (!rok(b, 0x20)) continue; bld_ok[i] = b; }
        int kind = *(u16 *)(b + 0x12); if (kind != 9 && kind != 10) continue; int wc = *(u16 *)(b + 0x10); if (wc >= NCELL || fstr[wc] != s || !flooded[wc]) continue;
        int e0 = *(u16 *)(b + 0x14); if (e0 <= 0 || e0 > 20000 || L > 2) continue; int wz = fneed[wc]; flooded[wc] = 0;
        for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) { int x = wc % 220 + dx, y = wc / 220 + dy; if (x < 0 || x >= 220 || y < 0 || y >= 220) continue; int q = y * 220 + x;
            if (flooded[q] && fstr[q] == s && fneed[q] >= wz && fneed[q] > 1) flooded[q] = 0; } }
    (void)rv;
}
static void flood_paint_s(u8 *d, int s)
{
    int L = sx[s].L;
    for (int c = 0; c < NCELL; c++) { if (fstr[c] != s) continue; int under = L > 0 && fneed[c] <= L;
        if (flooded[c] && !under) mud[c] = 2; flooded[c] = under ? (u8)L : 0;      /* drained ground is mud for two turns */
        if (flooded[c] && burning[c]) fire_douse_cell(c); }
    if (d && L) flood_walls(d, s);
}
/* v26k38c (asked for): a flood hurts an army once, when it comes into the water from dry ground or the water comes to it - troops and
   morale -20% / -30% / -40% (flood / big flood / great flood, the level of its cell). Moving on in the water or staying in it costs
   nothing more; out on dry ground and back in is a new hit. One bit per army slot, cleared when another officer takes the slot.
   Looked at once a day, after the day's water. The first look after loading a save only notes who stands in water. */
static u8 inw[512]; static u16 inw_off[512]; static int inw_prime = 1;
static void flood_entry(u8 *d)
{
    int hit = 0, prime = inw_prime; inw_prime = 0;
    for (int i = 1; i <= 500; i++) { u8 *a = army(d, i); if (!a) { inw[i] = 0; continue; }
        u16 off = *(u16 *)(a + 0x12); if (inw_off[i] != off) { inw_off[i] = off; inw[i] = 0; }
        int c = *(u16 *)(a + 0x2a); int L = c < NCELL ? flooded[c] : 0;
        if (!L) { inw[i] = 0; continue; } if (inw[i]) continue; inw[i] = 1; if (prime) continue;
        int pct = L >= 3 ? 40 : L == 2 ? 30 : 20; hurt_pct(a, pct, pct, L >= 3 ? "great flood" : L == 2 ? "big flood" : "flood"); hit++; }
    if (hit) { char b[80], *p = b; p = pstr(p, "flood: armies caught by the water "); p = pint(p, hit); *p = 0; logs(b); }
}
static u32 base_flood_seq[512];                   /* which flood already charged this base (once per base per flood) */
static void flood_food(u8 *d, int rv)
{
    /* v26k2 (asked for): the water TOUCHING a base counts as that base flooded - its hex or one of the six around it under water.
       Every flooded village / port / gate costs its city 5% of the food store, the city itself under water 20%; the base is
       charged once per flood (a rising flood that reaches new bases charges those too). */
    if (rv > 0) return;   /* local132: a flood costs no food or order */
    RiverEp *E = &rep_[rv];
    int pct[0x34]; int nb[0x34]; int cityHit[0x34]; u8 *repf[0x34]; int repc[0x34];
    for (int k = 0; k < 0x34; k++) { pct[k] = 0; nb[k] = 0; cityHit[k] = 0; repf[k] = 0; repc[k] = 0; }
    for (int i = 1; i <= 500; i++) { u8 *f = foot(d, i); if (!f) continue; int fc = *(u16 *)(f + 0x3a); if (fc >= NCELL) continue; u8 *h = cellp(d, fc); if (!h) continue;
        int tt = h[0x10]; if (tt != 15 && tt != 16 && tt != 17 && tt != 18) continue;
        int fp[24], nfp = 1, touched = 0; fp[0] = fc;
        for (int i2 = 0; i2 < nfp && nfp < 24; i2++) for (int k = 0; k < 6 && nfp < 24; k++) { int q = nb6(fp[i2], k); if (q < 0 || vterr[q] != vterr[fc] || cdist2(q, fc) > 9) continue; int dup = 0; for (int j = 0; j < nfp; j++) if (fp[j] == q) dup = 1; if (!dup) fp[nfp++] = q; }
        for (int i2 = 0; i2 < nfp && !touched; i2++) { int q0 = fp[i2]; if (flooded[q0] && (floodz[q0] >> 4) == rv) touched = 1;
            for (int k = 0; k < 6 && !touched; k++) { int q = nb6(q0, k); if (q >= 0 && flooded[q] && (floodz[q] >> 4) == rv) touched = 1; } }
        if (!touched) continue;
        if (base_flood_seq[i] == E->seq) continue; base_flood_seq[i] = E->seq;
        if (tt == 16 && f[0x54] <= 100) {          /* v26k38c (asked for): the city itself under water - its order -20% / -30% / -40% of what it is */
            int lv = 0; for (int i2 = 0; i2 < nfp; i2++) { int q0 = fp[i2]; if ((floodz[q0] >> 4) == rv && flooded[q0] > lv) lv = flooded[q0];
                for (int k = 0; k < 6; k++) { int q = nb6(q0, k); if (q >= 0 && (floodz[q] >> 4) == rv && flooded[q] > lv) lv = flooded[q]; } }
            int P = lv >= 3 ? 40 : lv == 2 ? 30 : 20, o0 = f[0x54], o1 = o0 - o0 * P / 100;
            o1 = o1 < 0 ? 0 : o1; if (o1 != o0) sr_cb_order(f, o1); o1 = f[0x54]; if (o1 != o0) { rset(L"[재해] 홍수로 "); raddname(f); radd(L"의 치안이 "); raddn(o0 - o1); radd(L" 떨어졌습니다."); report_ex(fc, 0);
                char b[100], *q = b; q = pstr(q, "flood: city order "); q = pint(q, o0); q = pstr(q, "->"); q = pint(q, o1); q = pstr(q, " (level "); q = pint(q, lv); q = pstr(q, ")"); *q = 0; logs(b); } }
        int fid = f[0x35]; if (fid > 0x33) continue;
        if (tt == 16) { pct[fid] += 20; cityHit[fid] = 1; repf[fid] = f; repc[fid] = fc; } else { pct[fid] += 5; nb[fid]++; if (!repf[fid]) { repf[fid] = f; repc[fid] = fc; } } }
    for (int fid = 0; fid <= 0x33; fid++) { if (!pct[fid] || !repf[fid]) continue;
        u8 *cityA = 0; for (int i = 1; i <= 500 && !cityA; i++) { u8 *f = foot(d, i); if (!f || f[0x35] != fid) continue; u8 *h = cellp(d, *(u16 *)(f + 0x3a)); if (h && h[0x10] == 16) cityA = f; }
        int took = city_take_food(d, repf[fid], pct[fid] > 60 ? 60 : pct[fid]); if (!took) continue; YS.floodFood += took;
        rset(L"[재해] 홍수로 "); if (cityA) raddname(cityA); else raddname(repf[fid]);
        if (cityHit[fid] && nb[fid]) { radd(L"이(가) 물에 잠기고 거점 "); raddn(nb[fid]); radd(L"곳이 침수되어 병량이 "); }
        else if (cityHit[fid]) radd(L"이(가) 물에 잠겨 병량이 ");
        else { radd(L"의 거점 "); raddn(nb[fid]); radd(L"곳이 침수되어 병량이 "); }
        raddn(took); radd(L" 줄었습니다."); report_ex(repc[fid], 0);
        char b[120], *q = b; q = pstr(q, "flood: city foothold "); q = pint(q, fid); q = pstr(q, " food -"); q = pint(q, took); q = pstr(q, " ("); q = pint(q, pct[fid]); q = pstr(q, "%: city "); q = pint(q, cityHit[fid]); q = pstr(q, ", bases "); q = pint(q, nb[fid]); q = pstr(q, ")"); *q = 0; logs(b); }
}
static u8 *place_of(u8 *d, int s) { return d ? nearest_city(d, sdesc[s - 1].mid, 60 * 60) : 0; }
static void radd_place(u8 *d, int s) { u8 *f = place_of(d, s); if (f) { raddname(f); radd(L" 부근"); } else radd(L"상류"); }
/* local156 (user 2026-11-14 "하비소패, 여남, 무릉장사 영릉계양, 강주, 건녕 ... 이 강들은 2칸정도만 더 확장되도록하자 좌우로"): the data's
   33 stretches cover only the 황하, the middle and lower 장강 and the 한수. Six side stretches are added once the game's city table can
   be read: the river water (여울 / 강 / 대하) within 7 hexes of the named cities that no data stretch holds, and the flat open land
   (평지 / 가도 / 모래 / 습지) 1-2 hexes beside it (need 1 / 2; never another stretch's ground, cities or hills). Their water is the
   day's rain on the land within 12 hexes of those cities (as the data stretches' run-off), and their bank takes the ratio of bank to
   catchment of the data's head stretches (no inflow from above), so they rise with heavy local rain like the others. */
static int minor_done; static SDesc sd2[MAXS]; static u16 *mcatch[MAXS + 1]; static int mncatch[MAXS + 1];
static int minor_city(u8 *d, const u16 *name, int any_base)
{
    for (int i = 1; i <= 500; i++) { u8 *f = foot(d, i); if (!f) continue; const u16 *n = (const u16 *)(f + 0x10);
        int same = 1; for (int k = 0; k < 8; k++) { if (n[k] != name[k]) { same = 0; break; } if (!name[k]) break; }   /* local184: the whole name (유수항) */
        if (!same) continue; int c = *(u16 *)(f + 0x3a); if (c >= NCELL) continue;
        u8 *h = cellp(d, c); if (!h || (!any_base && h[0x10] != 16)) continue; return c; }
    return -1;
}
static void sr_minor_rivers(u8 *d)
{
    if (minor_done || !d || !NSt0 || !fstr || !fneed || !fcatch || !fstem) return;
    /* local168 (user 2026-11-15 "수춘 하비여남의 저 소호가 범람했으면 ... 여남은 습지지대로 폭우로 자주 강이 되기도 했어"): the 소호 lake and
       the 습지 around it get a stretch of their own, found by the 합비 / 역양 bases (any base, radius 9); it shares the 회수 river (5)
       with 여남. Both take bank x0.6 so heavy local rain floods them more often (the flood width by stage stays as before).
       local173 (user 2026-11-15 "여남 홍수 습지도 여강으로 바꿔야하는데"): the user meant 여강, not 여남 - the 여남 stretch is replaced by
       one around 여강 (장강 2, bank x0.6); its own 장강 water stays with the data stretches (20 / 21), the rest of the water near it is
       this stretch's. */
    /* local184 (user 2026-11-15 "유수항쪽도 범람되서 바다랑 연결되어야 하는거 아닌가? ... 유수항 변경안도 적용해줘"): the 소호 stretch takes
       유수항 as a third anchor (radius 6): the water near it - the 유수 way down to the 장강 included, even a data stretch's - seeds the
       stretch, and the open land 1-2 hexes beside it moves to the 소호 stretch even when a data stretch (the 장강) held it, so a 소호
       flood reaches 유수항 and the 장강. Other side stretches' ground is never taken. */
    static const struct { const u16 *a, *b; u8 rv, any, r; float bank; const u16 *c; u8 rc; } G[7] = { { L"하비", L"소패", 4, 0, 7, 1.0f, 0, 0 }, { L"여강", 0, 2, 0, 7, 0.6f, 0, 0 },
        { L"무릉", L"장사", 6, 0, 7, 1.0f, 0, 0 }, { L"영릉", L"계양", 7, 0, 7, 1.0f, 0, 0 }, { L"강주", 0, 2, 0, 7, 1.0f, 0, 0 }, { L"건녕", 0, 8, 0, 7, 1.0f, 0, 0 },
        { L"합비", L"역양", 5, 1, 9, 0.6f, L"유수항", 6 } };
    int cities[7][3], found = 0;
    for (int g = 0; g < 7; g++) { cities[g][0] = minor_city(d, G[g].a, G[g].any); cities[g][1] = G[g].b ? minor_city(d, G[g].b, G[g].any) : -1;
        cities[g][2] = G[g].c ? minor_city(d, G[g].c, 1) : -1; if (cities[g][0] >= 0 || cities[g][1] >= 0) found++; }
    if (!found) return;   /* the city table is not up yet: next day */
    minor_done = 1;
    for (int c = 0; c < NCELL; c++) if (fstr[c] > NSt0) { fstr[c] = 0; fneed[c] = 0; floodz[c] = 0; }
    NSt = NSt0; memcpy(sd2, sdesc, sizeof(SDesc) * (u32)NSt0); sdesc = sd2;
    /* the bank per catchment cell of the head stretches */
    static int ncat[MAXS + 1], fed[MAXS + 1]; memset(ncat, 0, sizeof(ncat)); memset(fed, 0, sizeof(fed));
    for (int c = 0; c < NCELL; c++) { int s = fcatch[c]; if (s >= 1 && s <= NSt0) ncat[s]++; }
    for (int s = 1; s <= NSt0; s++) { int dn = sdesc[s - 1].dn; if (dn >= 1 && dn <= NSt0) fed[dn] = 1; }
    float rs[MAXS]; int nr = 0; for (int s = 1; s <= NSt0; s++) if (!fed[s] && ncat[s] > 20 && sdesc[s - 1].B > 0) rs[nr++] = sdesc[s - 1].B / (float)ncat[s];
    for (int i = 0; i < nr; i++) for (int j = i + 1; j < nr; j++) if (rs[j] < rs[i]) { float x = rs[i]; rs[i] = rs[j]; rs[j] = x; }
    float ratio = nr ? rs[nr / 2] : 0.0f;
    static short dist[NCELL]; static int q[NCELL];
    for (int g = 0; g < 7 && NSt < MAXS; g++) {
        const int R2 = G[g].r * G[g].r;
        int c0 = cities[g][0], c1 = cities[g][1], c2 = cities[g][2]; if (c0 < 0 && c1 < 0) continue;
        const int RC2 = G[g].rc * G[g].rc;
        int s = NSt + 1, head = 0, tail = 0, nw = 0, ng = 0, moved = 0, mid = c0 >= 0 ? c0 : c1;
        for (int c = 0; c < NCELL; c++) dist[c] = -1;
        for (int c = 0; c < NCELL; c++) { int v = vterr[c]; if (v < 12 || v > 14) continue;
            int at2 = c2 >= 0 && cdist2(c, c2) <= RC2;
            int near = (c0 >= 0 && cdist2(c, c0) <= R2) || (c1 >= 0 && cdist2(c, c1) <= R2) || at2; if (!near) continue;
            if (!at2 && ((fstr[c] && fstr[c] <= NSt) || fstem[c])) continue;   /* a data stretch's own river (not by the third anchor) */
            dist[c] = 0; q[tail++] = c; nw++; }
        if (!nw) continue;
        while (head < tail) { int c = q[head++]; if (dist[c] >= 2) continue;
            for (int k = 0; k < 6; k++) { int n = nb6(c, k); if (n < 0 || dist[n] >= 0) continue; int v = vterr[n]; if (v < 1 || v > 4) continue;
                if (fstr[n] && fneed[n]) {   /* another stretch's ground: only a data stretch's, by the third anchor, moves over */
                    if (c2 < 0 || fstr[n] > NSt0 || cdist2(n, c2) > RC2) continue;
                    moved++; }
                dist[n] = (short)(dist[c] + 1); fstr[n] = (u8)s; fneed[n] = (u8)dist[n]; floodz[n] = (u8)((G[g].rv << 4) | dist[n]); ng++; q[tail++] = n; } }
        if (!ng) continue;
        int nc = 0; for (int c = 0; c < NCELL; c++) if (cl_land(c) && ((c0 >= 0 && cdist2(c, c0) <= 12 * 12) || (c1 >= 0 && cdist2(c, c1) <= 12 * 12))) nc++;
        u16 *cat = (u16 *)VirtualAlloc(0, sizeof(u16) * (u32)(nc + 1), 0x3000, 4); if (!cat) continue; int k2 = 0;
        for (int c = 0; c < NCELL; c++) if (cl_land(c) && ((c0 >= 0 && cdist2(c, c0) <= 12 * 12) || (c1 >= 0 && cdist2(c, c1) <= 12 * 12))) cat[k2++] = (u16)c;
        if (mcatch[s]) VirtualFree(mcatch[s], 0, 0x8000);
        mcatch[s] = cat; mncatch[s] = k2;
        memset(&sd2[s - 1], 0, sizeof(SDesc)); sd2[s - 1].river = G[g].rv; sd2[s - 1].mid = (u16)mid; sd2[s - 1].B = ratio * (float)k2 * G[g].bank;
        memset(&sx[s], 0, sizeof(Stretch)); NSt = s;
        { char b[140], *p = b; p = pstr(p, "engine: side stretch "); p = pint(p, s); p = pstr(p, " river "); p = pint(p, G[g].rv); p = pstr(p, " water "); p = pint(p, nw);
          p = pstr(p, " ground "); p = pint(p, ng); p = pstr(p, " catch "); p = pint(p, k2); p = pstr(p, " bank x1000 "); p = pint(p, (int)(sd2[s - 1].B * 1000.0f));
          if (G[g].c) { p = pstr(p, " anchor3 "); p = pint(p, c2); p = pstr(p, " moved "); p = pint(p, moved); } *p = 0; logs(b); }
    }
}
static void minor_runoff(void)   /* the day's rain on the side stretches' land (as rain_day does through fcatch for the data stretches) */
{
    for (int s = NSt0 + 1; s <= NSt; s++) { const u16 *cat = mcatch[s]; if (!cat) continue;
        for (int i = 0; i < mncatch[s]; i++) { int c = cat[i], tier = rain[c]; if (!tier || snowing[c]) continue;
            float P = tier == 3 ? 25.0f : tier == 2 ? 8.0f : 2.0f, k = 0.10f + 0.60f * wet[c] / 255.0f;
            sRun[s] += P * k; sRech[s] += P * (1.0f - k) * 0.25f; } }
}
/* the water of one day: called after the day's rain (rain_day has added each stretch's run-off) */
static void hydro_day(void)
{
    if (!NSt) return;
    minor_runoff();
    for (int s = 1; s <= NSt; s++) sx[s].Qin = sx[s].Qnext, sx[s].Qnext = 0;
    for (int s = 1; s <= NSt; s++) { Stretch *X = &sx[s];
        X->G += sRech[s]; float base = X->G * 0.02f; X->G -= base;
        X->inflow = sRun[s] + base + X->Qin; X->S += X->inflow; X->Q = X->S / 1.5f; X->S -= X->Q;
        int dn = sdesc[s - 1].dn; if (dn >= 1 && dn <= NSt) sx[dn].Qnext += X->Q;
        sRun[s] = 0; sRech[s] = 0; }
#ifdef HOSTTEST
    if (getenv("HYDROCAL")) { static float amax[MAXS + 1]; static int yr = -1; static FILE *hf; if (!hf) hf = fopen(getenv("HYDROCAL"), "w");
        if (yr != g_syear) { if (yr >= 0) { fprintf(hf, "Y %d", yr); for (int s = 1; s <= NSt; s++) fprintf(hf, " %.1f", amax[s]); fprintf(hf, "\n"); fflush(hf); } yr = g_syear; memset(amax, 0, sizeof amax); }
        for (int s = 1; s <= NSt; s++) if (sx[s].Q > amax[s]) amax[s] = sx[s].Q; }
    if (getenv("HYDRODAY")) { static FILE *hd; if (!hd) hd = fopen(getenv("HYDRODAY"), "w"); fprintf(hd, "%d %d", g_syear, cur_turn); for (int s = 1; s <= NSt; s++) fprintf(hd, " %.0f", sx[s].Q); fprintf(hd, "\n"); }
#endif
}
static void ep_final(int rv)          /* the flood season of this river is over (no new rise for 15 days): into the ledger */
{
    RiverEp *E = &rep_[rv]; if (!E->active) return; E->active = 0;
    { int m = E->maxL < 1 ? 1 : E->maxL > 3 ? 3 : E->maxL; YS.fl[m]++; YS.flTurns[m] += (E->days + 9) / 10; }   /* the ledger (turns) */
#ifdef HOSTTEST
    if (getenv("FLOODLOG")) printf("FLOODEND river %d max %d days %d kind %d\n", rv, E->maxL, E->days, E->kind);
#endif
}
static void ep_end(u8 *d, int rv)            /* the water is gone: said, with the day */
{
    RiverEp *E = &rep_[rv]; if (!E->active || E->drained) return; E->drained = 1; E->drainDay = today_day();
    for (int i = 0; i < 16; i++) if (vis[i].type == 2 && vis[i].cell == rv) vis[i].type = 0;
    rset(L"[재해] "); radd(RIVB[rv - 1]); radd(L" 물이 빠졌습니다."); radd_when(E->day0); report_ex(E->cell, 0);
    logs("flood ended"); (void)d;
}
/* a stretch's level changes (rises: the water reaches new ground - news, people swept, food lost; falls: higher ground dries) */
static void stretch_set(u8 *d, int s, int L, int kind, int t)
{
    Stretch *X = &sx[s]; int rv = sdesc[s - 1].river, old = X->L; if (L == old) return;
#ifdef HOSTTEST
    if (getenv("STRUP") && L > old) printf("STRUP y%d t%d s%d L%d\n", g_syear, t, s, L);
#endif
    RiverEp *E = &rep_[rv];
    if (L > old) {
        if (E->active && E->drained && today_day() >= E->drainDay + 15) ep_final(rv);
        if (E->active && E->drained) {          /* v26k15: the same flood season - the river rises again within 15 days of draining: one flood, said as "again" */
            E->drained = 0; X->L = L; if (L > E->maxL) E->maxL = L; flood_paint_s(d, s); flood_food(d, rv); if (rv <= 3) vis_add(2, rv, L >= 2 ? 1 : 0, 3);
            rset(L"[재해] "); if (kind == 2) radd(L"얼음에 막혀 "); else if (kind == 0) radd(L"폭우로 "); radd(RIVN[rv - 1]); radd(L" "); radd_place(d, s); radd(L"에서 다시 범람했습니다.");
            if (L >= 2) { radd(L" ("); radd(FLN[L]); radd(L")"); } report(sdesc[s - 1].mid); nJoin[rv] = 0; return; }
        if (!E->active) {
            if (kind != 9 && kind != 2 && 0) { if (!YS.brake[BR_FLOOD]) { YS.brake[BR_FLOOD] = 1; logs("ledger brake: floods"); } return; }
            if (kind != 9 && vanilla_now(d)) { logs("flood: the game's own disaster is in the news - waits"); return; }
            memset(E, 0, sizeof(*E)); E->active = 1; ev_seq++; E->seq = ev_seq; E->day0 = today_day(); E->kind = kind; E->cell = sdesc[s - 1].mid; E->maxL = L; YS.flStart++;
            X->L = L; flood_paint_s(d, s); flood_food(d, rv);
            if (rv <= 3) vis_add(2, rv, L >= 2 ? 1 : 0, 3);
            rset(L"[재해] "); if (kind == 2) radd(L"얼음에 막혀 "); else if (kind == 0) radd(L"폭우로 "); radd(RIVN[rv - 1]); radd(L" "); radd_place(d, s); radd(L"에서 범람했습니다.");
            if (L >= 2) { radd(L" ("); radd(FLN[L]); radd(L")"); } report(E->cell);
#ifdef HOSTTEST
            if (getenv("FLOODLOG")) printf("FLOODSTART y%d t%d river %d level %d kind %d stretch %d\n", g_year, t, rv, L, kind, s);
#endif
            { char b[100], *p = b; p = pstr(p, "flood start river "); p = pint(p, rv); p = pstr(p, " level "); p = pint(p, L); p = pstr(p, " kind "); p = pint(p, kind); p = pstr(p, " stretch "); p = pint(p, s); *p = 0; logs(b); }
            nJoin[rv] = 0; newsDay[rv] = (int)today_day(); return; }
        X->L = L; flood_paint_s(d, s); flood_food(d, rv);
        if (L > E->maxL) { E->maxL = L; for (int i = 0; i < 16; i++) if (vis[i].type == 2 && vis[i].cell == rv) { vis[i].p = L >= 2 ? 1 : 0; vis[i].left = 3; }
            rset(L"[재해] "); radd(RIVB[rv - 1]); radd(L" "); radd_place(d, s); if (L == 3) radd(L" 둑이 터져 일대가 물에 잠겼습니다. (대홍수)"); else radd(L" 물이 더 불어 큰 홍수가 되었습니다."); report(sdesc[s - 1].mid); }
        else if (old == 0 && nJoin[rv] < 4) newsJoin[rv][nJoin[rv]++] = s;          /* more of the river under water: said once a day */
        return; }
    X->L = L; flood_paint_s(d, s);
    int any = 0; for (int q = 1; q <= NSt; q++) if (sdesc[q - 1].river == rv && sx[q].L > 0) any = 1;
    if (!any) ep_end(d, rv);
    (void)t;
}
#ifdef SEASON_TEST
static int test_reflood;   /* 1 = the Yellow River's test flood should come back once after it drains ("다시 범람") */
static void force_flood(u8 *d, int rv, int L, int nearCell, int t, int kind, int holdDays);
#endif
static int sr_hist(int s);
static void sr_hist_extend(void);
static void floods_day(u8 *d, int t)
{
    if (!NSt) return;
    sr_minor_rivers(d);
#ifdef SEASON_TEST
    if (test_reflood && rep_[1].active && rep_[1].drained && today_day() >= rep_[1].drainDay + 4) { test_reflood = 0; force_flood(d, 1, 2, 14882, t, 0, 10); }
#endif
    for (int s = 1; s <= NSt; s++) { Stretch *X = &sx[s]; float B = sdesc[s - 1].B; int Lt = 0, kind = 0;
        if (B > 0) { float r = X->Q / B * (sr_hist(s) ? 1.22f : 1.1f); Lt = r > GREAT ? 3 : r > BIG ? 2 : r > LEVEE ? 1 : 0; }
        if (X->jamDays > 0) { X->jamDays--; if (X->jamL > Lt) { Lt = X->jamL; kind = 2; } }
        if (X->hold > 0) { X->hold--; if (Lt < X->L) Lt = X->L; }
        if (Lt > X->L) { X->below = 0; stretch_set(d, s, Lt, kind, t); if (X->L == Lt && X->hold < 15 * Lt) X->hold = 15 * Lt; }
        else if (Lt < X->L) { if (++X->below >= 3) { X->below = 0; stretch_set(d, s, X->L - 1, 0, t); } }   /* the water falls a stage at most every three days */
        else X->below = 0; }
    for (int rv = 1; rv <= 8; rv++) { RiverEp *E = &rep_[rv]; if (!E->active) continue;
        if (E->drained) { u32 now = today_day(); if (now >= E->drainDay + 15 || now < E->drainDay) ep_final(rv); continue; }
        E->days++;
        for (int i = 0; i < 16; i++) if (vis[i].type == 2 && vis[i].cell == rv) vis[i].left = 3;
        if (nJoin[rv]) { rset(L"[재해] "); radd(RIVB[rv - 1]); radd(L" 물이 "); for (int k = 0; k < nJoin[rv] && k < 2; k++) { if (k) radd(L"·"); radd_place(d, newsJoin[rv][k]); }
            if (nJoin[rv] > 2) { radd(L" 외 "); raddn(nJoin[rv] - 2); radd(L"곳"); } radd(L"까지 넘쳤습니다."); report_ex(sdesc[newsJoin[rv][0] - 1].mid, 0); nJoin[rv] = 0; } }
}
static u8 prevSnowT[NCELL]; static int prevSnowOk; static u8 meltNow[NCELL];   /* v26k12: snow that melted at this turn change (1 plain, 2 deep) - the landslides use it */
static void floods_turn(u8 *d, int t)
{
    for (int c = 0; c < NCELL; c++) meltNow[c] = (prevSnowOk && prevSnowT[c] && !cSnow[c]) ? prevSnowT[c] : 0;
    memcpy(prevSnowT, cSnow, sizeof(prevSnowT)); prevSnowOk = 1;
    if (!NSt) return;
    /* walls in the water: -10 endurance a turn (v26k6); 0 = broken */
    if (0) { int walls = 0, broke = 0, cell = -1;   /* local132: walls do not wash away */
      for (int i = 1; i <= 3000; i++) { u8 *b = *(u8 **)(d + DB_BUILD + 8 * (u64)i); if (!b) continue; if (b != bld_ok[i]) { if (!rok(b, 0x20)) continue; bld_ok[i] = b; }
        int kind = *(u16 *)(b + 0x12); if (kind != 9 && kind != 10) continue; int wc = *(u16 *)(b + 0x10); if (wc >= NCELL || !fstr[wc] || !sx[fstr[wc]].L || fneed[wc] > sx[fstr[wc]].L) continue;
        int e0 = *(u16 *)(b + 0x14); if (e0 <= 0 || e0 > 20000) continue; int e1 = e0 - 10; if (e1 < 0) e1 = 0; sr_cb_durability(b, e1); e1 = *(u16 *)(b + 0x14); walls++; cell = wc; if (e1 == 0) broke++; }
      if (walls) { char b[100], *p = b; p = pstr(p, "flood: walls in the water "); p = pint(p, walls); p = pstr(p, ", broken "); p = pint(p, broke); *p = 0; logs(b);
        if (broke) { rset(L"[재해] 물에 잠긴 토성·석벽 "); raddn(broke); radd(L"곳이 무너졌습니다."); report_ex(cell, 0); } } }
    /* the Yellow River's ice: a stretch that breaks up while the stretch below it is still frozen sends its ice into it - the ice
       jams and the water backs up over the banks. Once a winter per stretch, and only with water enough to push the ice. */
    for (int s = 1; s <= NSt; s++) { if (sdesc[s - 1].river != 1) continue; Stretch *X = &sx[s]; int n = 0, fz = 0;
        for (int c = 0; c < NCELL; c++) if (fstem[c] == s && vterr[c] == T_GREATRIVER) { n++; if (frz_cur(c)) fz++; }
        int frozen = n > 0 && fz * 2 > n;
        if (frozen) { if (!X->wasFrozen) X->jamDone = 0; X->frozenRun++; }
        else if (X->wasFrozen && X->frozenRun >= 4 && !X->jamDone) { X->jamDone = 1; int dn = sdesc[s - 1].dn;
            int dnFrozen = 0; if (dn >= 1 && dn <= NSt) { int n2 = 0, f2 = 0; for (int c = 0; c < NCELL; c++) if (fstem[c] == dn && vterr[c] == T_GREATRIVER) { n2++; if (frz_cur(c)) f2++; } dnFrozen = n2 > 0 && f2 * 2 > n2; }
            float B = sdesc[s - 1].B, push = B > 0 ? X->Q / (0.3f * B) : 0; if (push > 1) push = 1;
            if (dnFrozen && frand() < 0.6f * push) { X->jamL = X->Q > 0.6f * B ? 2 : 1; X->jamDays = 8 + (int)(rnd() % 10); logs("ice jam on the Yellow River"); } }
        if (!frozen) X->frozenRun = 0; X->wasFrozen = frozen; }
    (void)t;
}
#ifdef SEASON_TEST
static void force_flood(u8 *d, int rv, int L, int nearCell, int t, int kind, int holdDays)   /* the test: one stretch of each river at a chosen stage, held a while */
{
    int best = -1, bd = 1 << 30; for (int s = 1; s <= NSt; s++) { if (sdesc[s - 1].river != rv) continue; int dd = cdist2(sdesc[s - 1].mid, nearCell); if (dd < bd) { bd = dd; best = s; } }
    if (best < 0) return; sx[best].hold = holdDays; stretch_set(d, best, L, kind, t);
}
#endif
/* ---- landslides from the rain: heavy rain days on the steep road cells of the slide regions; burnt slopes x3 ---- */
static float K_SL = 0.0015f;
static void slides_turn(u8 *d, int t)
{
    build_near_gate(); int made = 0;
    if (YS.slides >= 10) { if (!YS.brake[BR_SLIDE]) { YS.brake[BR_SLIDE] = 1; logs("ledger brake: landslides"); } made = 3; }   /* v26k11 */
    for (int c = 0; c < NCELL && made < 3; c++) { if (!slidez[c] || near_gate[c]) continue; float rainD = heavyDays[c] * 0.5f; float meltD = meltNow[c] >= 2 ? 2.0f : meltNow[c] ? 1.0f : 0.0f;
        float hd = rainD + meltD; if (hd < 2.5f) continue;   /* v26k5: in days (half-day steps); v26k12: the spring thaw soaks the slope too - deep snow gone = 2 days of heavy rain, thin snow 1 */
        float s = CL(c)[14] * 10.0f; float st = s < 300.0f ? 0.3f : (s - 300.0f) / 900.0f; if (st > 1) st = 1;
        float heavy = (hd - 2.0f) * 0.5f; float p = K_SL * heavy * heavy * st * (ash[c] ? 3.0f : 1.0f);
        if (frand() >= p) continue;
        Event *e = (nev < 48) ? &ev[nev++] : 0; if (!e) break; memset(e, 0, sizeof(*e)); e->day0 = today_day(); e->type = EV_SLIDE; e->region = slidez[c]; e->start = t; e->cell = c; ev_seq++;
        slide_at(d, e, c, season_of(t)); e->turns_left = 3; e->active = 1; made++; YS.slides++;
        for (int i = 0; i < 16; i++) if (vis[i].type == 3 && vis[i].left == 0) vis[i].left = e->turns_left;
        u8 *f = nearest_city(d, c, 30 * 30); rset(meltD > rainD ? L"[재해] 눈 녹은 물로 " : meltD > 0 ? L"[재해] 폭우와 눈 녹은 물로 " : L"[재해] 폭우로 "); if (meltD > 0) YS.slideMelt++; radd(SLIDEN[e->region - 1]); radd(L" "); if (f) { raddname(f); radd(L" 부근 "); } radd(L"산길이 산사태로 막혔습니다."); report(c); }
#ifdef HOSTTEST
    if (getenv("CRACKREG")) { int m1 = 0, m2 = 0, mr = 0; for (int c = 0; c < NCELL; c++) if (slidez[c] && !near_gate[c]) { if (meltNow[c] == 1) m1++; if (meltNow[c] == 2) m2++; if (meltNow[c] && heavyDays[c]) mr++; } if (m1 + m2) printf("SLMELT t%d plain %d deep %d withrain %d\n", t, m1, m2, mr); }
#endif
    memset(heavyDays, 0, sizeof(heavyDays));
}

/* ---- drought news: once a season per region when the ground starts to crack ---- */
static u8 crack_said[17], drought_on[17]; static u32 drought_day0[17];   /* v26k13: the drought news given, so its end is said too */
static void drought_turn(u8 *d, int t)
{
    int cnt[17]; memset(cnt, 0, sizeof(cnt)); for (int c = 0; c < NCELL; c++) if (crack[c] && prov[c] < 17) cnt[prov[c]]++;
    int s = season_of(t);
    /* v26k12: the cracks now mean "drier than this region's normal", so the news comes when 15% of the region's open ground has cracked
       (at least 40 cells) - the regions differ in size (was 400 cells everywhere) */
    static int elig[17], elig_ok; if (!elig_ok) { elig_ok = 1; for (int c = 0; c < NCELL; c++) if (prov[c] < 17 && cl_land(c) && (vterr[c] == 1 || vterr[c] == 2 || vterr[c] == 3 || vterr[c] == 15)) elig[prov[c]]++; }
    for (int r = 0; r < 17; r++) { int need = elig[r] * 15 / 100; if (need < 40) need = 40;
        if (cnt[r] < need) { if (cnt[r] < need / 4) { crack_said[r] = 0;
                if (drought_on[r]) { drought_on[r] = 0; rset(L"[재해] 비가 내려 "); radd(PROVN[r]); radd(L"의 가뭄이 풀렸습니다."); radd_when(drought_day0[r]); int pick = -1; for (int c = 0; c < NCELL && pick < 0; c++) if (prov[c] == r && cl_land(c)) pick = c; report_ex(pick, 0); } }   /* v26k13 (asked for) */
            continue; }
        if (drought_on[r]) continue; drought_on[r] = 1; drought_day0[r] = today_day(); crack_said[r] = (u8)(s + 4 * (g_year & 63));   /* v26k15: once a drought, not again every season while it lasts */
        int pick = -1, n = 0; for (int c = 0; c < NCELL; c++) if (crack[c] && prov[c] == r) { n++; if (rnd() % (u32)n == 0) pick = c; }
        u8 *f = nearest_city(d, pick, 40 * 40); rset(L"[재해] 가뭄: "); radd(PROVN[r]); radd(L" "); if (f) { raddname(f); radd(L" 부근 "); } radd(L"땅이 쩍쩍 갈라졌습니다."); report_ex(pick, 0); }
}

/* ---- the map: rain grid, node anomalies and the per-cell state, written straight into the graphics tool's list ---- */
#define IP_CTRL 100
#define IP_DT 101
#define IP_MULT 122
#define IP_RAIN 150
#define IP_STATE 1000
#define IP_GRID 3420     /* v26k30: the game's own hex grid: x first column's x, y the z origin, z column step, w hex radius */
#define IP_WX 3424       /* v26k30: per cell one byte, three cells a float: rain tier (2 bits), snowing, lying snow (2), river ice, shaken, mud */
#define IP_END 7460
#define IP_DAY 99
static float g_dayfrac;
#ifdef SEASON_TEST
/* v26k16 test: the snow and ice preview - whatever the season of the save, 34 days run the snow through 9월 하순 .. 3월 상순 (two days a
   turn): first snow, the cover spreading, deep winter (Hebei white, the Yellow River frozen), the melt. Picture only - the rules keep
   the real season. */
static int prev_on, prev_day, prev_step = -1;
#endif
static int rain_level = 3;                        /* 0 off, 1 wet ground, 2 + clouds and curtains, 3 + streaks */
static volatile int shown_level = 3;              /* what the map is told right now (the calibration switches it off for a few seconds at a time) */
static u8 cstate[NCELL];
static int sr_flood_tiles;   /* defined in season_engine.c (flag 64) */
static void state_build(void)
{
    for (int c = 0; c < NCELL; c++) { u8 s = 0;
        if (flooded[c] && !sr_flood_tiles) s = (u8)(((floodz[c] >> 4) == 1 ? 9 : 3) + flooded[c]);   /* v26k7: 10..12 the Yellow River (muddy), 4..6 the others */ else if (mine[c] == 2) s = 7;   /* v26k38c: a blocked road shows its landslide first (the map draws it from this state now) */ else if (burning[c]) s = 1; else if (mud[c]) s = (u8)(mud[c] >= 2 ? 13 : 14); else if (ash[c]) s = ashLeft[c] >= 3 ? 2 : 3;   /* v26k30b: 13 fresh mud, 14 drying */ else if (crack[c]) s = 9;   /* v26e: water over fire */
        cstate[c] = s; }
    /* v26k7: the river's own cells beside the flood carry the flood picture too (picture only): the bank slope inside a river
       cell and the shore under the game's ripples are painted with the same water, so the flood joins the river with no dry gap.
       v26k12 (asked for): three cells out into the river (was one) - the flood always overlaps the game's own water */
    for (int pass = 0; pass < 3; pass++) { static u8 nx[NCELL]; memset(nx, 0, sizeof(nx));
        for (int c = 0; c < NCELL; c++) { if ((vterr[c] != 13 && vterr[c] != 14) || cstate[c]) continue; u8 best = 0;
            for (int k = 0; k < 6; k++) { int n = nb6(c, k); if (n < 0) continue; u8 v = cstate[n]; if ((v >= 4 && v <= 6) || (v >= 10 && v <= 12)) { int lv = v >= 10 ? v - 9 : v - 3, lb = !best ? 0 : best >= 10 ? best - 9 : best - 3; if (lv > lb) best = v; } }
            nx[c] = best; }
        for (int c = 0; c < NCELL; c++) if (nx[c]) cstate[c] = nx[c]; }
#ifdef SEASON_TEST
    if (NSt) { static u32 lastDay; u32 now = today_day(); if (now != lastDay) { lastDay = now; int bad = 0, mudn = 0;   /* v26k14: the Yellow River's mud must never show on the Yangtze or the Han */
        for (int c = 0; c < NCELL; c++) { if (cstate[c] < 10 || cstate[c] > 12) continue; mudn++; int s = fstr[c] ? fstr[c] : fstem[c]; if (s >= 1 && s <= NSt && sdesc[s - 1].river != 1) bad++; }
        if (mudn) { char b[100], *p = b; p = pstr(p, "flood colour check: muddy cells "); p = pint(p, mudn); p = pstr(p, ", off the Yellow River "); p = pint(p, bad); *p = 0; logs(b); } } }
#endif
}
static float *dl_list(int need);
/* v26k30: the game's hex grid. Its cell -> world function (official 0x188700, rollback 0x187f50): x = col * CW + X0,
   z = row * sqrt(3) * R + (even column ? sqrt(3) * R / 2 : 0) + Z0 with X0 at 0x1a20578 (data, 37594) and Z0 at 0x2141a80 (set at start-up:
   sqrt(3) * R / 2 + 12930), CW 702 and R 468 in the read-only data. Read from the running game so a map mod that moves the grid is followed. */
static void grid_params(float *g)
{
    g[0] = 37594.0f; g[1] = 13335.2998f; g[2] = 702.0f; g[3] = 468.0f;
    if (!gver) return;
    u32 rcw = RVA_GRID_CW, rr = RVA_GRID_R;   /* v26k41: moved in the 2026-11-05 update */
    u32 rx0 = RVA_GRID_X0, rz0 = RVA_GRID_Z0; if (!rcw || !rr || !rx0 || !rz0) return;
    float *x0 = (float *)(game + rx0), *z0 = (float *)(game + rz0), *cw = (float *)(game + rcw), *r = (float *)(game + rr);
    if (!readable(x0, 4) || !readable(z0, 4) || !readable(cw, 4) || !readable(r, 4)) return;
    if (!(*cw > 500.0f && *cw < 900.0f && *r > 300.0f && *r < 600.0f && *x0 > -50000.0f && *x0 < 1000000.0f && *z0 > -50000.0f && *z0 < 1000000.0f)) return;   /* v26k48: the map mod (맵 확장 137) moves Z0 by +117000 (130335) */
    g[0] = *x0; g[1] = *z0; g[2] = *cw; g[3] = *r;
}
static int cell_wx(int c, int t)
{
    int tier = rain[c] > 3 ? 3 : rain[c]; int sn = (tier && snowing[c]) ? 1 : 0;
    return tier | (sn << 2) | (cover_lvl(c, t) << 3) | ((mine[c] == 1 ? 1 : 0) << 5) | (((quaked[c] & 3) ? 1 : 0) << 6) | ((mud[c] ? 1 : 0) << 7);
}
static void weather_write(void)
{
    if (dl_state != 1) return;
    while (__sync_lock_test_and_set(&dl_lock, 1)) Sleep(0);
    float *L = dl_list(IP_END);
    if (!L) { __sync_lock_release(&dl_lock); return; }
    /* v26k29: list entry 97.y = 1 while any rain grid cell has a rain tier. With none the rain layer (picture copy, drops, haze)
       draws exactly nothing, so d3dx.ini leaves it out. Set before the grid when rain comes, cleared after it when the rain is gone,
       so no frame ever misses a drop. */
    int anyTier = 0; for (int c = 0; c < NCELL && !anyTier; c++) if (rain[c] > 0) anyTier = 1;
    if (anyTier) { L[97 * 4 + 1] = 1.0f; __sync_synchronize(); }
    for (int k = 0; k < GN; k++) { L[IP_DT * 4 + k] = ndT[k]; L[IP_MULT * 4 + k] = nMult[k]; }
    for (int j = 0; j < RG; j++) for (int i = 0; i < RG; i++) { int tmax = 0, sn = 0, cr = 0, n = 0; float ws = 0;
        for (int dy = 0; dy < 4; dy++) for (int dx = 0; dx < 4; dx++) { int x = i * 4 + dx, y = j * 4 + dy; if (x >= 220 || y >= 220) continue; int c = y * 220 + x; n++;
            if (rain[c] > tmax) tmax = rain[c]; if (snowing[c]) sn = 1; if (crack[c]) cr++; ws += wet[c]; }
        float v = (n ? (float)(int)(ws / n / 255.0f * 100.0f) : 0.0f) + tmax * 1000.0f + sn * 10000.0f + (n && cr * 2 >= n ? 100000.0f : 0.0f);
        L[IP_RAIN * 4 + j * RG + i] = v; }
    state_build();
    for (int q = 0; q < (NCELL + 4) / 5; q++) { float v = 0, m = 1; for (int k = 0; k < 5; k++) { int c = q * 5 + k; if (c < NCELL) v += cstate[c] * m; m *= 16.0f; } L[IP_STATE * 4 + q] = v; }
    /* v26k30: the game's own hex grid, read from the game itself (its cell -> world function uses these four numbers), and the
       per-cell weather the map must draw exactly where the rules have it */
    { float g[4]; grid_params(g); for (int k = 0; k < 4; k++) L[IP_GRID * 4 + k] = g[k]; }
    { int t = cur_turn < 0 ? 0 : cur_turn;
      for (int q = 0; q < (NCELL + 2) / 3; q++) { float v = 0, m = 1; for (int k = 0; k < 3; k++) { int c = q * 3 + k; if (c < NCELL) v += (float)cell_wx(c, t) * m; m *= 256.0f; } L[IP_WX * 4 + q] = v; } }
    L[IP_CTRL * 4 + 1] = (float)shown_level; L[IP_CTRL * 4 + 2] = 26.0f; L[IP_CTRL * 4 + 3] = 1.0f;
    L[IP_DAY * 4 + 0] = g_dayfrac; L[IP_DAY * 4 + 1] = (float)(cur_turn + 1); L[IP_DAY * 4 + 2] = 0.0f;
#ifdef SEASON_TEST
    if (prev_on) { int step = prev_day / 2; L[IP_DAY * 4 + 0] = ((prev_day % 2) + 0.5f) / 2.0f; L[IP_DAY * 4 + 1] = (float)((26 + step) % 36 + 1); L[IP_DAY * 4 + 2] = 1.0f; }
#endif   /* v26k12: how far into the turn (0..1) and the rules' own turn: the snow cover grows / melts day by day */
    if (!anyTier) { __sync_synchronize(); L[97 * 4 + 1] = 0.0f; }
    dl_epoch_bump(L);                          /* v26k29: after the day's data: the ground shader's kept colours of yesterday are not used again */
    __sync_lock_release(&dl_lock);
}
/* v26h: frame-rate watch, no blinking. Every second the graphics tool's frame counter (list entry 100.x, counted by d3dx.ini)
   is read. Seconds are not counted while the game window is not in front, within 2.5 s of a day tick or a turn change
   (the game itself stalls there), or when under 2 frames came (loading, alt-tab). "No rain" seconds = fewer than 200 raining
   cells today; "rain" seconds = 200 or more and the rain picture at level 2 or 3 (clouds drawn). The rain picture is lowered
   one level only when, over at least 30 counted rainy seconds, the average is under 30 frames/s AND more than 10% under the
   no-rain average of the last 60 counted seconds; after 60 rainy seconds without that, it is kept. The verdict is written to
   SeasonPerf.txt next to the log and is not measured again on this computer (the installer removes the file, so a new version
   measures afresh). Overall averages and the slowest second go to the log for the test summary. */
static int cal_state;                              /* 0 measuring, 2 done */
static u64 cal_next; static float cal_last; static volatile u64 day_tick_at;
static float fr_base[60]; static int fr_bn, fr_bi;
static float fr_rain_sum; static int fr_rain_n;
static float fr_all_sum, fr_all_min = 1e9f; static int fr_all_n, fr_all_rn; static float fr_all_rsum; static u64 fr_log_next;
static u16 perfpath[600];
static void perf_path(void) { if (perfpath[0]) return; int n = wlen(logpath); while (n && logpath[n - 1] != '\\') n--; for (int i = 0; i < n; i++) perfpath[i] = logpath[i]; perfpath[n] = 0; wcat(perfpath, L"SeasonPerf.txt"); }
static void perf_save(const char *why, float ravg, float bavg)
{
    perf_path(); HANDLE h = CreateFileW(perfpath, 0x40000000, 1, 0, 2, 0x80, 0); if (h == (HANDLE)(i64)-1) return;
    char b[200], *q = b; q = pstr(q, "level="); q = pint(q, rain_level); q = pstr(q, "\r\nrain_fps="); q = pint(q, (int)ravg); q = pstr(q, "\r\nbase_fps="); q = pint(q, (int)bavg);
    q = pstr(q, "\r\nwhy="); q = pstr(q, why); q = pstr(q, "\r\n"); DWORD w; WriteFile(h, b, (DWORD)(q - b), &w, 0); CloseHandle(h);
}
static int perf_load(void)                         /* 1 = a verdict exists: rain_level set from it, no measuring */
{
    perf_path(); HANDLE h = CreateFileW(perfpath, 0x80000000, 1, 0, 3, 0x80, 0); if (h == (HANDLE)(i64)-1) return 0;
    char b[200]; DWORD n = 0; ReadFile(h, b, 199, &n, 0); CloseHandle(h); b[n] = 0;
    if (n < 7 || b[0] != 'l' || b[1] != 'e' || b[2] != 'v' || b[3] != 'e' || b[4] != 'l' || b[5] != '=' || b[6] < '1' || b[6] > '3') return 0;
    /* v26k38d: the weather is never lowered by itself - a saved level from an older version is read only to say it is not used */
    char l[140], *q = l; q = pstr(q, "rain picture: SeasonPerf.txt says level "); q = pint(q, b[6] - '0'); q = pstr(q, " - ignored, the weather is always drawn in full (level 3)"); *q = 0; logs(l);
    return 1;
}
static volatile int perf_on;                       /* v26k27: 1 while the speed test switches the ways */
static void rain_calibrate(void)
{
    if (dl_state != 1 || perf_on) return; u64 now = GetTickCount64(); if (now < cal_next) return;
    float *L = dl_list(IP_END); if (!L) return; float fc = L[IP_CTRL * 4];
    if (cal_next == 0) { cal_last = fc; cal_next = now + 1000; return; }
    float frames = fc - cal_last; cal_last = fc; cal_next = now + 1000;
    int valid = frames >= 2.0f && frames < 1000.0f && game_in_front() && now >= quiet_after && (day_tick_at == 0 || now - day_tick_at > 2500);
    if (!valid) return;
    float fps = frames;                            /* one second */
    fr_all_sum += fps; fr_all_n++; if (fps < fr_all_min) fr_all_min = fps;
    int rainy = rain_cells_today >= 200 && shown_level >= 2;
    if (rainy) { fr_all_rsum += fps; fr_all_rn++; }
    if (fr_log_next == 0) fr_log_next = now + 60000;
    if (now >= fr_log_next) { fr_log_next = now + 60000;
        char b[200], *q = b; q = pstr(q, "fps: avg "); q = pint(q, (int)(fr_all_sum / fr_all_n)); q = pstr(q, " min "); q = pint(q, (int)fr_all_min); q = pstr(q, " seconds "); q = pint(q, fr_all_n);
        q = pstr(q, " rain avg "); q = pint(q, fr_all_rn ? (int)(fr_all_rsum / fr_all_rn) : 0); q = pstr(q, " rain seconds "); q = pint(q, fr_all_rn); q = pstr(q, " level "); q = pint(q, rain_level); *q = 0; logs(b); }
    if (cal_state == 2) return;
    if (!rainy) { if (rain_cells_today < 200) { fr_base[fr_bi] = fps; fr_bi = (fr_bi + 1) % 60; if (fr_bn < 60) fr_bn++; } return; }
    fr_rain_sum += fps; fr_rain_n++;
    if (fr_rain_n < 30 || fr_bn < 10) return;
    float ravg = fr_rain_sum / fr_rain_n, bsum = 0; for (int i = 0; i < fr_bn; i++) bsum += fr_base[i]; float bavg = bsum / fr_bn;
    int bad = ravg < 30.0f && ravg < bavg * 0.9f;
    char b[200], *q = b; q = pstr(q, "rain picture: rain avg "); q = pint(q, (int)ravg); q = pstr(q, " no-rain avg "); q = pint(q, (int)bavg); q = pstr(q, " at level "); q = pint(q, rain_level);
    if (bad && rain_level > 1) { rain_level--; shown_level = rain_level; L[IP_CTRL * 4 + 1] = (float)shown_level; q = pstr(q, " -> reduced to "); q = pint(q, rain_level); *q = 0; logs(b);
        fr_rain_sum = 0; fr_rain_n = 0; perf_save("reduced", ravg, bavg); return; }
    if (bad) { q = pstr(q, " (kept at the lowest level)"); *q = 0; logs(b); cal_state = 2; perf_save("lowest", ravg, bavg); return; }
    if (fr_rain_n >= 60) { q = pstr(q, " - fine, kept"); *q = 0; logs(b); cal_state = 2; perf_save("fine", ravg, bavg); }
}

/* v26k37: weather-layer watch (self-diagnosis, reads three counters once a second, writes a line only when the state changes):
   d3dx.ini counts in 96.x the frames the game's lighting pass ran (the weather layer hangs on it) and in 96.y the frames the layer
   was drawn. "lighting pass missing" = the game drew that picture another way (a camera / zoom step) and the rain / snow could not
   be put on it; "not drawn" = the pass ran but the layer's own conditions were off. */
static void wl_watch(void)
{
    static u64 next; static float lf = -1, ll, ld; static int state = -1, lines; static u64 since;
    if (dl_state != 1) return; u64 now = GetTickCount64(); if (now < next) return; next = now + 1000;
    float *L = dl_list(IP_END); if (!L) return;
    float fc = L[IP_CTRL * 4], lit = L[96 * 4 + 0], dr = L[96 * 4 + 1];
    if (lf < 0 || fc < lf || lit < ll || dr < ld) { lf = fc; ll = lit; ld = dr; return; }
    float df = fc - lf, dl = lit - ll, dd = dr - ld; lf = fc; ll = lit; ld = dr;
    if (df < 2.0f || df > 1000.0f || !game_in_front()) return;
    int want = L[97 * 4 + 1] > 0.5f && L[IP_CTRL * 4 + 1] > 1.5f;
    int st = dl < df * 0.9f ? 0 : (!want ? 1 : (dd >= df * 0.9f ? 2 : 3));
    if (st == state) return;
    static const char *T[4] = { "lighting pass MISSING - the game drew the picture another way, no rain / snow layer possible",
                                "lighting pass ok, no rain anywhere (layer off)", "lighting pass ok, rain / snow layer drawn", "lighting pass ok but the layer NOT drawn" };
    if (lines++ < 400) { char b[260], *q = b; q = pstr(q, "weather layer: "); q = pstr(q, T[st]); q = pstr(q, " | frames/s "); q = pint(q, (int)df);
        q = pstr(q, ", pass "); q = pint(q, (int)dl); q = pstr(q, ", layer "); q = pint(q, (int)dd);
        if (state >= 0) { q = pstr(q, " | previous state lasted s "); q = pint(q, (int)((now - since) / 1000)); } *q = 0; logs(b); }
    state = st; since = now;
}

#ifdef PERF_AB
#ifndef PF_CYCLES
#define PF_CYCLES 36
#endif
/* ---- v26k27 speed test (test build only): the map is drawn three ways in turn, 3 seconds each, for the first minutes after a
   save is loaded, and the frame time of each way is written to the log:
     way 0 new  - the ground drawn once with the season picture inside, winter water without an extra pass per piece
     way 1 old  - as in v26k23 (the ground drawn twice, the winter water redrawn piece by piece)
     way 2 game - the game alone (no season picture on the ground, water, trees, no rain picture)
     way 3 new without the rain picture (clouds, curtains, drops) - how much the rain costs
   The first 0.5 s after a switch are not counted; a window with the game away from the front or a stall (no new frame for
   0.3 s: loading, the game's own turn work) is thrown away. Each cycle runs the three ways in a different order. ---- */
static int pf_state, pf_way, pf_cycle, pf_pos, pf_bad, pf_progress, pf_done_cycles;
static u64 pf_t0, pf_last_frame_t; static float pf_f0, pf_last_fc; static u64 pf_day0; static u64 pf_front_ms, pf_prev_now;
static float pf_ms[4]; static int pf_valid[4];
static double pf_sum[3][4]; static int pf_n[3][4];     /* [group: all, progress, heavy][way] */
static double pf_min_ms[4] = { 1e9, 1e9, 1e9, 1e9 }, pf_max_ms[4];
static const int PF_ORDER[4][4] = { { 0, 1, 2, 3 }, { 1, 3, 0, 2 }, { 2, 0, 3, 1 }, { 3, 2, 1, 0 } };
/* v26k38c: the ways are the route cost - 0 and 2 the new one (each cell weighed), 1 and 3 the old one. The picture is the same in all four
   (the ground's binding stays the new one: y98 is left at 0). Per way also: the route steps weighed per second and the windows lost to a stall. */
static u64 pf_rc0; static double pf_rcs[4]; static int pf_rcn[4], pf_stall[4];
static void pf_set(float *L, int way) { (void)L; rc_off = way & 1; pf_rc0 = rc_calls; }
static void perf_summary(void)
{
    static const char *G[3] = { "all", "during progress", "heavy view (old binds under 60 frames/s)" };
    for (int g = 0; g < 3; g++) { if (!pf_n[g][1]) continue;
        char b[260], *q = b; q = pstr(q, "perf summary ["); q = pstr(q, G[g]); q = pstr(q, "] cycles "); q = pint(q, pf_n[g][1]);
        static const char *W[4] = { " | route new ", " | route old ", " | route new (2) ", " | route old (2) " };   /* v26k38c */
        for (int w = 0; w < 4; w++) { double ms = pf_n[g][w] ? pf_sum[g][w] / pf_n[g][w] : 0; q = pstr(q, W[w]); q = pint(q, (int)(ms * 100.0 + 0.5)); q = pstr(q, "/100 ms ("); q = pint(q, ms > 0 ? (int)(1000.0 / ms + 0.5) : 0); q = pstr(q, " fps)"); }
        *q = 0; logs(b); }
    { char b[200], *q = b; q = pstr(q, "perf range (ms per frame, per 2.5 s window): new "); for (int w = 0; w < 4; w++) { if (w == 1) q = pstr(q, " old "); if (w == 2) q = pstr(q, " new2 "); if (w == 3) q = pstr(q, " old2 "); q = pint(q, (int)(pf_min_ms[w] * 10)); q = pstr(q, ".."); q = pint(q, (int)(pf_max_ms[w] * 10)); q = pstr(q, "/10"); } *q = 0; logs(b); }
    { char b[260], *q = b; q = pstr(q, "perf route: steps weighed per s (new / old / new2 / old2) ");
      for (int w = 0; w < 4; w++) { if (w) q = pstr(q, " / "); q = pint(q, pf_rcn[w] ? (int)(pf_rcs[w] / pf_rcn[w] + 0.5) : 0); }
      q = pstr(q, " | windows lost to a stall "); for (int w = 0; w < 4; w++) { if (w) q = pstr(q, " / "); q = pint(q, pf_stall[w]); }
      q = pstr(q, " | cells weighed dearer "); q = pint(q, (int)(rc_mulled > 2000000000ull ? 2000000000ull : rc_mulled)); *q = 0; logs(b); }
}
static void perf_ab(void)
{
    if (pf_state == 2) return;
    if (dl_state != 1 || day_tick_at == 0) return;   /* a save is running and the map link is direct */
    float *L = dl_list(IP_END); if (!L) return;
    u64 now = GetTickCount64(); float fc = L[IP_CTRL * 4];
    int front = game_in_front();
    if (pf_prev_now && front) pf_front_ms += now - pf_prev_now; pf_prev_now = now;
    if (pf_state == 0) { pf_state = 1; perf_on = 1; pf_cycle = 0; pf_pos = 0; pf_way = PF_ORDER[0][0]; pf_set(L, pf_way); pf_t0 = now; pf_bad = 0; pf_progress = 0; pf_day0 = day_tick_at; pf_last_fc = fc; pf_last_frame_t = now;
        logs("perf test (v26k38c): the route cost - new (each cell weighed) and old - take turns every 3 s for a few minutes; frames, route steps and stalls per way"); return; }
    if (fc != pf_last_fc) { pf_last_fc = fc; pf_last_frame_t = now; } else if (now - pf_last_frame_t >= 300) { if (!(pf_bad & 2) && front) pf_stall[pf_way]++; pf_bad |= 3; }
    if (!front) pf_bad = 1;
    if (day_tick_at != pf_day0) pf_progress = 1;
    u64 el = now - pf_t0;
    if (el >= 500 && pf_f0 == 0.0f) { pf_f0 = fc; pf_bad = 0; pf_progress = 0; pf_day0 = day_tick_at; pf_t0 = now - 500; pf_rc0 = rc_calls; return; }   /* counting starts after the settle */
    if (el < 3000) return;
    float frames = fc - pf_f0; int ok = !pf_bad && pf_f0 > 0.0f && frames >= 5.0f;
    { u64 st = rc_calls - pf_rc0; if (front) { pf_rcs[pf_way] += (double)st / ((double)(el - 500) / 1000.0); pf_rcn[pf_way]++; } }
    pf_ms[pf_way] = ok ? 2500.0f / frames : 0; pf_valid[pf_way] = ok; if (pf_progress) pf_valid[pf_way] |= 2;
    pf_pos++;
    if (pf_pos == 4) {                              /* a cycle is complete */
        int all = (pf_valid[0] & 1) && (pf_valid[1] & 1) && (pf_valid[2] & 1) && (pf_valid[3] & 1), prog = (pf_valid[0] | pf_valid[1] | pf_valid[2] | pf_valid[3]) & 2;
        char b[200], *q = b; q = pstr(q, "perf cycle "); q = pint(q, pf_cycle + 1); q = pstr(q, prog ? " progress" : " still");
        q = pstr(q, all ? "" : " (thrown away)"); q = pstr(q, rain_cells_today >= 200 && shown_level >= 2 ? " rain" : " dry");
        static const char *W[4] = { ": new ", " old ", " new2 ", " old2 " };
        for (int w = 0; w < 4; w++) { q = pstr(q, W[w]); q = pint(q, (int)(pf_ms[w] * 100.0f + 0.5f)); q = pstr(q, "/100 ms"); }
        *q = 0; logs(b);
        if (all) { pf_done_cycles++;
            int heavy = pf_ms[1] > 16.7f;
            for (int w = 0; w < 4; w++) { pf_sum[0][w] += pf_ms[w]; pf_n[0][w]++; if (prog) { pf_sum[1][w] += pf_ms[w]; pf_n[1][w]++; } if (heavy) { pf_sum[2][w] += pf_ms[w]; pf_n[2][w]++; }
                if (pf_ms[w] < pf_min_ms[w]) pf_min_ms[w] = pf_ms[w]; if (pf_ms[w] > pf_max_ms[w]) pf_max_ms[w] = pf_ms[w]; } }
        pf_cycle++; pf_pos = 0; pf_valid[0] = pf_valid[1] = pf_valid[2] = pf_valid[3] = 0;
        if (pf_done_cycles >= PF_CYCLES || pf_front_ms >= 8u * 60u * 1000u) {
            pf_set(L, 0); pf_state = 2; perf_on = 0; perf_summary(); logs("perf test: done - the new way stays"); return; }
        if (pf_cycle % 8 == 0) perf_summary();
    }
    pf_way = PF_ORDER[pf_cycle % 4][pf_pos]; pf_set(L, pf_way); pf_t0 = now; pf_f0 = 0.0f; pf_bad = 0; pf_progress = 0; pf_day0 = day_tick_at;
}
#else
static void perf_ab(void) {}
#endif
#ifdef DIAG28
/* ---- v26k28 test build: where the turn's time goes. Every 5 s: frames, calls to the movement-speed hook and the time they take
   (one call in 1024 timed), the game's own part of it, the fire-rate hook calls. For the first minutes after a save is loaded
   the hook takes turns every 10 s: 0 table (new), 1 worked out per call (as v26k27), 2 off (the game's own speed, no slow-downs). ---- */
#ifndef DIAG_SEQ
#define DIAG_SEQ 0   /* v26k28d: the k28c sequence (F8 records, cost monitor, way switching) is off; the GPU clock in SeasonSync does the work */
#endif
static int ab_state, ab_way, ab_n[3][2], ab_sum[3][2], ab_min[3][2];   /* v26k28c way comparison: 0 not yet, 1 running, 2 done */
static void diag_tick(void)
{
    static u64 next, t_start, last_calls, last_samp, last_tot, last_orig, last_fr, front_ms, prev; static float last_fc; static u64 last_day_tick; static int phase, done;
    static i64 qf; if (!qf) QueryPerformanceFrequency(&qf);
    if (dl_state != 1 || day_tick_at == 0) return;
    float *L = dl_list(IP_END); if (!L) return;
    u64 now = GetTickCount64(); if (prev && game_in_front()) front_ms += now - prev; prev = now;
    if (!next) { next = now + 5000; t_start = now; last_fc = L[IP_CTRL * 4]; last_calls = sp_calls; last_samp = sp_samp; last_tot = sp_tot; last_orig = sp_orig; last_fr = fr_calls; last_day_tick = day_tick_at;
        logs("diag: frame timing every 5 s, with our own time-step work; GPU time per shader is in SeasonSync_log.txt"); return; }
    hook_mode = 0; (void)done; (void)t_start;
    /* v26k28c: (1) the graphics tool's frame record (F8, call list only) once standing still and twice on the march;
       (2) its own cost monitor (Ctrl+F9 on, Shift+F9 = freeze and write to its log, Ctrl+F9 off) on the march;
       (3) then, on the march, 5-second windows take turns: way 0 (the season map) / way 2 (the game's own pictures, season
           pictures and rain off) - so the log shows how much of the march's frame time is the season pictures and how much
           is the graphics tool being there at all. Afterwards way 0 stays. */
    { static int st, mon_on; static u64 still_since, last_tick_seen, loaded_at, t_mark; if (!loaded_at) loaded_at = now;
      if (day_tick_at != last_tick_seen) { last_tick_seen = day_tick_at; still_since = now; }
      int moving = now - still_since < 1500, still = !moving && now - still_since > 6000 && now - loaded_at > 15000;
      if (DIAG_SEQ && game_in_front() && ab_state == 0) {
        const char *what = 0;
        if (!(st & 1) && still) { st |= 1; key_tap(0x77); what = "standing still"; }
        else if (!(st & 2) && moving && now - loaded_at > 20000) { st |= 2; key_tap(0x77); what = "on the march (1)"; t_mark = now; }
        else if ((st & 2) && !(st & 4) && moving && now - t_mark > 20000) { st |= 4; key_tap(0x77); what = "on the march (2)"; t_mark = now; }
        else if ((st & 4) && !(st & 8) && moving && now - t_mark > 5000) { st |= 8; key_combo(0x11, 0x78); mon_on = 1; t_mark = now; logs("diag: cost monitor on (Ctrl+F9)"); }
        else if ((st & 8) && !(st & 16) && now - t_mark > 8000) { st |= 16; key_combo(0x10, 0x78); Sleep(500); key_combo(0x11, 0x78); mon_on = 0; t_mark = now; logs("diag: cost monitor written to the graphics tool's log (Shift+F9) and off"); }
        else if ((st & 16) && now - t_mark > 3000) { ab_state = 1; logs("diag: way comparison starts on the march (0 season map / 2 game's own pictures, 5 s each)"); }
        if (what) { char b[80], *q = b; q = pstr(q, "diag: frame record (F8) "); q = pstr(q, what); *q = 0; logs(b); } }
      (void)mon_on; }
    if (now < next) return;
    float fc = L[IP_CTRL * 4]; u64 calls = sp_calls, samp = sp_samp, tot = sp_tot, orig = sp_orig, frc = fr_calls;
    double secs = (now - next + 5000) / 1000.0; double frames = fc - last_fc;
    u64 dc = calls - last_calls, ds = samp - last_samp; double ttot = ds ? (double)(tot - last_tot) / ds : 0, torig = ds ? (double)(orig - last_orig) / ds : 0;
    double us_tot = ttot * 1e6 / qf * dc / secs, us_orig = torig * 1e6 / qf * dc / secs;   /* microseconds per second */
    int prog = day_tick_at != last_day_tick;
    char b[300], *q = b; q = pstr(q, "diag "); q = pstr(q, prog ? "progress" : "still"); q = pstr(q, " way "); q = pint(q, (int)(L[98 * 4] + 0.5));
    q = pstr(q, ": fps "); q = pint(q, (int)(frames / secs + 0.5)); q = pstr(q, " | speed hook calls/s "); q = pint(q, (int)(dc / secs));
    q = pstr(q, " time ms/s "); q = pint(q, (int)(us_tot / 1000.0)); q = pstr(q, " (game's own part "); q = pint(q, (int)(us_orig / 1000.0)); q = pstr(q, ")");
    q = pstr(q, " | fire-rate calls/s "); q = pint(q, (int)((frc - last_fr) / secs));
    { static i64 l_tt; static u64 l_tn; i64 tt = th_ticks; u64 tn = th_n; i64 mx = th_maxt; th_maxt = 0;
      q = pstr(q, " | our time-step work ms/s x10 "); q = pint(q, (int)((tt - l_tt) * 10000.0 / qf / secs)); q = pstr(q, " calls/s "); q = pint(q, (int)((tn - l_tn) / secs));
      q = pstr(q, " longest ms x10 "); q = pint(q, (int)(mx * 10000 / qf)); l_tt = tt; l_tn = tn; }
    *q = 0; logs(b);
    if (ab_state == 1) {
        int ab_fps = (int)(frames / secs + 0.5);
        if (frames > 0 && ab_fps < 1000) { ab_n[ab_way][prog]++; ab_sum[ab_way][prog] += ab_fps; if (ab_fps < ab_min[ab_way][prog] || !ab_min[ab_way][prog]) ab_min[ab_way][prog] = ab_fps; }
        if (ab_n[0][1] >= 6 && ab_n[2][1] >= 6) {
            ab_state = 2; ab_way = 0; L[98 * 4] = 0.0f;
            for (int w = 0; w <= 2; w += 2) { char s[200], *r = s; r = pstr(r, "diag summary way "); r = pint(r, w); r = pstr(r, w ? " (game's own pictures)" : " (season map)");
                for (int g = 1; g >= 0; g--) { r = pstr(r, g ? " | march: windows " : " | standing: windows "); r = pint(r, ab_n[w][g]);
                    r = pstr(r, " avg fps "); r = pint(r, ab_n[w][g] ? ab_sum[w][g] / ab_n[w][g] : 0); r = pstr(r, " lowest "); r = pint(r, ab_min[w][g]); }
                *r = 0; logs(s); }
            logs("diag: way comparison done - the season map stays (way 0). The test is complete.");
        } else { ab_way = ab_way ? 0 : 2; L[98 * 4] = (float)ab_way; }
    }
    last_fc = fc; last_calls = calls; last_samp = samp; last_tot = tot; last_orig = orig; last_fr = frc; last_day_tick = day_tick_at; next = now + 5000;
}
#else
static void diag_tick(void) {}
#endif

/* ---- the game's own fires (화계) in the rain, and the rain's hand on the game ---- */
/* RVA_FIRERATE: int FireRate(officer*, hex*, showMsg) -> 0..100, the one place the 화계 success rate comes from (command, AI, screen);
   RVA_SETHEXFIRE: void SetHexFire(hex*, hours): hex+0x16 = clamp(hours, 0, 720), 0 removes the fire picture (both from the version table) */
typedef int (GAMEABI *FireRateFn)(void *, void *, int);
static FireRateFn firerate_orig;
static int GAMEABI FireRateHook(void *off, void *hex, int show)
{
    fr_calls++;
    int r = firerate_orig(off, hex, show); if (r <= 0 || !v2) return r;
    u8 *d = *dbp(); if (!d) return r; u8 *h0 = *(u8 **)(d + DB_HEX); if (!h0 || !hex) return r;
    i64 o = (u8 *)hex - h0; if (o < 0 || o >= (i64)NCELL * 32 || (o & 31)) return r;
    int c = (int)(o >> 5); u8 rt = rain[c];
    if (rt == 1) r = r * 4 / 5; else if (rt == 2) r = r / 2; else if (rt >= 3) r = 0;
    return r;
}
static int install_firerate(void)
{
    static const u8 head[6] = { 0x40, 0x56, 0x41, 0x54, 0x41, 0x57 };   /* push rsi; push r12; push r15 */
    u8 *at = game + RVA_FIRERATE;
    if (!readable(at, 16)) return 0; for (int i = 0; i < 6; i++) if (at[i] != head[i]) return 0;
    if (at[6] != 0x48 || at[7] != 0x81 || at[8] != 0xec) return 0;
    u8 *tr = (u8 *)VirtualAlloc(0, 4096, 0x3000, 0x40); if (!tr) return 0;
    memcpy(tr, head, 6); tr[6] = 0xFF; tr[7] = 0x25; *(u32 *)(tr + 8) = 0; *(u64 *)(tr + 12) = (u64)(at + 6);
    FlushInstructionCache(GetCurrentProcess(), tr, 32);
    firerate_orig = (FireRateFn)tr;
    u8 *relay = api->MakeRelay(at, (void *)FireRateHook); if (!relay) { VirtualFree(tr, 0, 0x8000); firerate_orig = 0; return 0; }
    i64 dl = relay - (at + 5); if (dl < -0x7fffffffLL || dl > 0x7fffffffLL) { api->FreeRelay(relay); VirtualFree(tr, 0, 0x8000); firerate_orig = 0; return 0; }
    S14_Hook h; memset(&h, 0, sizeof(h)); h.target = at; h.length = 6; memcpy(h.expected, head, 6);
    h.replacement[0] = 0xE9; i32 dd = (i32)dl; memcpy(h.replacement + 1, &dd, 4); h.replacement[5] = 0x90;
    if (!api->InstallHooks("SAN14CE.SeasonRules.FireRate", &h, 1)) { api->FreeRelay(relay); VirtualFree(tr, 0, 0x8000); firerate_orig = 0; return 0; }
    return 1;
}
static int sethexfire_ok;
static void game_fires_day(u8 *d)         /* the game's own burning hexes under rain: shorter, a downpour puts them out */
{
    if (!sethexfire_ok) return; u8 *h0 = *(u8 **)(d + DB_HEX); if (d != hex_db) hex_check(d); if (!hex0) return; (void)h0;
    typedef void (GAMEABI *SetFire)(void *, int); int n = 0;
    for (int c = 0; c < NCELL && n < 64; c++) { u8 *h = hex0 + 32 * (u64)c; int v = *(u16 *)(h + 0x16); if (!v || v > 720) continue; u8 rt = rain[c]; if (rt < 1) continue;
        int nv = rt >= 3 ? 0 : rt == 2 ? v / 2 : v * 3 / 4; if (nv == v) continue; ((SetFire)(game + RVA_SETHEXFIRE))(h, nv); n++; }
    if (n) { char b[80], *p = b; p = pstr(p, "rain on the game's fires: "); p = pint(p, n); p = pstr(p, " hex(es)"); *p = 0; logs(b); }
}

/* ---- the forced test: every disaster in every size, rain in three strengths, dry cracked ground ---- */
#ifdef SEASON_TEST
static int force_snow_k = -1;   /* v26k6: spots 4..6 are snow whatever the temperature (the test only) */
static void force_rain_at(int cx, int cy, int R, int tier)
{
    for (int y = cy - R; y <= cy + R; y++) for (int x = cx - R; x <= cx + R; x++) { if (x < 0 || x >= 220 || y < 0 || y >= 220) continue; int dx = x - cx, dy = y - cy; if (dx * dx + dy * dy > R * R) continue;
        int c = y * 220 + x; if (!cl_land(c) && vterr[c] != T_GREATRIVER) continue; rain[c] = (u8)tier; snowing[c] = (tier && (cT[c] < 1.0f || force_snow_k > 0)) ? 1 : 0; wet[c] = (u8)(tier == 3 ? 220 : tier == 2 ? 150 : 90); if (tier >= 2) heavyDays[c] = 3; }
}
static int force_rain_on; static int force_rain_cx[10], force_rain_cy[10], force_rain_r[10], force_rain_t[10];   /* v26k7: 4..6 snow row, 7..9 rain row */   /* v26k6: spots 5..7 three snow strengths in the north, 8 a downpour on a city */
static void force_fire_size(u8 *d, int region_cell, int size, int t)
{
    int c0 = -1; int n = 0; for (int c = 0; c < NCELL; c++) { if (!fuelT[c] || burning[c] || ash[c] || cSnow[c] || occ[c]) continue; int dd = cdist2(c, region_cell); if (dd > 12 * 12) continue; n++; if (rnd() % (u32)n == 0) c0 = c; }
    if (c0 < 0) return;
    Fire *F = 0; for (int i = 0; i < 12; i++) if (!fires[i].active) { F = &fires[i]; break; } if (!F) return;
    memset(F, 0, sizeof(*F)); F->active = 1; F->start = t; F->day0 = today_day(); F->n = 1; F->cells[0] = (u16)c0; burning[c0] = 1; ev_seq++; F->seq = ev_seq; F->age = -3;   /* stays 3 turns beyond the usual */
    fire_spread(F, size); nfires_total++; fire_news_start(d, F);
    for (int i = 0; i < F->n && i < 8; i++) vis_add(1, F->cells[i], t, 3);
    char b[100], *p = b; p = pstr(p, "TEST fire cells "); p = pint(p, F->n); p = pstr(p, " wanted "); p = pint(p, size); *p = 0; logs(b);
}
#endif

/* ---- load and reset ---- */
/* v26k7: the flood rises from the river outward. The zones of the data file (drawn from a hazard picture) skipped the bank cells
   right at the water (slopes, cliffs) and left patches cut off from the river; here every river's zone is rebuilt as rings
   counted from its own water: the bank cells next to the water are ring 1 and always go under first, then the land behind.
   z 1 = rings 1-2, z 2 = ring 3 (a flood covers z 1-2), z 3 = rings 4.. inside the old zone (a big / great flood). */
static int test_relabel_cell = -1;                 /* v26k12: one cell that was wrongly the Yellow River's (the test points to it) */
static void flood_rings(void)
{
    static u8 seen[NCELL]; static u16 q[NCELL]; int added = 0, dropped = 0, kept = 0, moved = 0;
    /* v26k12: a few cells far south of the Yellow River carried its number in the data (its muddy flood showed on the Han and the
       Yangtze). Every such cell goes to the nearest other river's zone within 6 cells, or out of the zones. */
    for (int c = 0; c < NCELL; c++) { if ((floodz[c] >> 4) != 1 || !(floodz[c] & 15) || c / 220 <= 85) continue;
        int best = -1, bd = 37; for (int q2 = 0; q2 < NCELL; q2++) { int r2 = floodz[q2] >> 4; if (r2 < 2 || !(floodz[q2] & 15)) continue; int dd = cdist2(c, q2); if (dd < bd) { bd = dd; best = q2; } }
        if (best >= 0) { floodz[c] = (u8)((floodz[best] & 0xf0) | (floodz[c] & 15)); if (test_relabel_cell < 0 || (floodz[c] >> 4) == 3) test_relabel_cell = c; } else floodz[c] = 0; moved++; }
    for (int rv = 1; rv <= 3; rv++) {
        static u8 inzone[NCELL], water[NCELL]; memset(inzone, 0, sizeof(inzone)); memset(water, 0, sizeof(water)); memset(seen, 0, sizeof(seen));
        for (int c = 0; c < NCELL; c++) if ((floodz[c] >> 4) == rv && (floodz[c] & 15)) inzone[c] = 1;
        /* the river's water along its flood stretch: the great river's water within three steps of the zone (v26k12: was two), side streams two */
        { static u8 dz[NCELL]; memset(dz, 255, sizeof(dz)); int h2 = 0, t2 = 0; for (int c = 0; c < NCELL; c++) if (inzone[c]) { dz[c] = 0; q[t2++] = (u16)c; }
          while (h2 < t2) { int c = q[h2++]; if (dz[c] >= 3) continue; for (int k = 0; k < 6; k++) { int n = nb6(c, k); if (n < 0 || dz[n] != 255) continue; dz[n] = (u8)(dz[c] + 1); q[t2++] = (u16)n; } }
          for (int c = 0; c < NCELL; c++) if ((vterr[c] == 14 && dz[c] <= 3) || (vterr[c] == 13 && dz[c] <= 2)) water[c] = 1; }   /* side streams: as before */
        /* v26k12 (asked for): the bank always goes under first - EVERY land cell at that water is ring 1, and the cell behind it ring 2,
           whether the old hazard picture had them or not; the land further back (rings 3..) only inside the old zone */
        int head = 0, tail = 0;
        for (int c = 0; c < NCELL; c++) if (water[c]) for (int k = 0; k < 6; k++) { int n = nb6(c, k); if (n < 0 || seen[n] || water[n] || vterr[n] == 13 || vterr[n] == 14 || vterr[n] == 0 || !cl_land(n)) continue;
            if (!inzone[n] && (vterr[n] == 16 || vterr[n] == 17)) continue;   /* a city or gate beside the water is not added (only touched) */
            if ((floodz[n] >> 4) && (floodz[n] >> 4) != rv && (floodz[n] & 15)) continue;   /* another river's zone */
            seen[n] = 1; q[tail++] = (u16)n; }
        static u8 ring[NCELL]; memset(ring, 0, sizeof(ring)); for (int i = 0; i < tail; i++) ring[q[i]] = 1;
        while (head < tail) { int c = q[head++]; for (int k = 0; k < 6; k++) { int n = nb6(c, k); if (n < 0 || seen[n]) continue;
                int ok = inzone[n] || (ring[c] == 1 && cl_land(n) && vterr[n] != 13 && vterr[n] != 14 && vterr[n] != 16 && vterr[n] != 17 && !((floodz[n] >> 4) && (floodz[n] & 15)));
                if (!ok) continue; seen[n] = 1; ring[n] = (u8)(ring[c] + 1); q[tail++] = (u16)n; } }
        for (int c = 0; c < NCELL; c++) {
            int was = inzone[c];
            if (!seen[c]) { if (was) { floodz[c] = 0; dropped++; } continue; }
            u8 z = ring[c] <= 2 ? 1 : ring[c] == 3 ? 2 : 3;
            if (!was) added++; else kept++;
            floodz[c] = (u8)((rv << 4) | z); } }
#ifdef HOSTTEST
    if (getenv("FLOODDUMP")) { FILE *f = fopen(getenv("FLOODDUMP"), "wb"); fwrite(floodz, 1, NCELL, f); fclose(f); }
#endif
    char b[120], *p = b; p = pstr(p, "flood rings: bank cells added "); p = pint(p, added); p = pstr(p, ", cells kept "); p = pint(p, kept); p = pstr(p, ", cut-off cells dropped "); p = pint(p, dropped); p = pstr(p, ", wrong-river cells moved "); p = pint(p, moved); *p = 0; logs(b);
}
static void engine_load(void)
{
    v2 = 0; if (!v2base) return;
    u8 *p = v2base; cl = p; p += (u64)NCELL * 64; shNW = p; p += NCELL; shSE = p; p += NCELL; fuelT = p; p += NCELL; peopleT = p; p += NCELL; roughT = p; p += 81; naltT = p; p += 81;
    v2 = 1; ri_norm_init();
    { u32 need2 = 24 + NCELL * 5 + 36 * (NCELL / 8) + 36 * NCELL + NCELL * 64 + NCELL * 4 + 81 * 2, off = (need2 + 15) & ~15u; u32 have = 24 + NCELL * 5 + 36 * (NCELL / 8) + 36 * NCELL + v2len;   /* v26k14: the rivers' stretches */
      u8 *q = dat + off; NSt = 0;
      if (off + 8 <= have && q[0] == 'F' && q[1] == 'L' && q[2] == 'D' && q[3] == '3') { int ns = *(u32 *)(q + 4);
          if (ns > 0 && ns <= MAXS && off + 8 + (u32)ns * 16 + 4u * NCELL <= have) { sdesc = (SDesc *)(q + 8); u8 *r = q + 8 + ns * 16; fstr = r; fneed = r + NCELL; fcatch = r + 2 * NCELL; fstem = r + 3 * NCELL; NSt = ns; NSt0 = ns; minor_done = 0; sr_hist_extend();
              for (int c = 0; c < NCELL; c++) { int s = fstr[c]; floodz[c] = (s >= 1 && s <= ns && fneed[c]) ? (u8)((sdesc[s - 1].river << 4) | fneed[c]) : 0; } } }
      char b[120], *p = b; p = pstr(p, "engine: river stretches "); p = pint(p, NSt); if (NSt) { int nb = 0; for (int s = 0; s < NSt; s++) if (sdesc[s].B > 0) nb++; p = pstr(p, ", with banks "); p = pint(p, nb); } *p = 0; logs(b);
      if (!NSt) flood_rings(); }
    perf_load(); cal_state = 2;                     /* v26k38d: no automatic lowering of the rain picture - the frame watch only counts */
    { int add = 0; for (int c = 0; c < NCELL; c++) { if (vterr[c] != T_GREATRIVER || ((CL(c)[7] >> 5) & 3) != 2) continue; int dup = 0; for (int k = 0; k < nf; k++) if (fcells[k] == c) { dup = 1; break; } if (!dup && nf < NCELL) { fcells[nf++] = (u16)c; add++; } }
      char b[80], *q = b; q = pstr(q, "engine: Liaodong coast ice cells added "); q = pint(q, add); *q = 0; logs(b); }
    char b[120], *q = b; q = pstr(q, "engine: river rain norms x1000 "); q = pint(q, (int)(RInorm[1] * 1000)); *q++ = '/'; q = pint(q, (int)(RInorm[2] * 1000)); *q++ = '/'; q = pint(q, (int)(RInorm[3] * 1000)); *q = 0; logs(b);
}
static void engine_reset(void)
{
    inw_prime = 1;                             /* v26k38c: the first flood look after a load only notes who stands in water */
    stats_reset();   /* v26k11: a reload starts a new ledger (every roll is new) */
    memset(rain, 0, sizeof(rain)); memset(wet, 0, sizeof(wet)); memset(snowing, 0, sizeof(snowing)); memset(crack, 0, sizeof(crack)); memset(heavyDays, 0, sizeof(heavyDays)); memset(rainRun, 0, sizeof(rainRun)); memset(dryd, 0, sizeof(dryd));
    memset(ash, 0, sizeof(ash)); memset(ashLeft, 0, sizeof(ashLeft)); memset(fires, 0, sizeof(fires)); memset(base_flood_seq, 0, sizeof(base_flood_seq)); prevSnowOk = 0; memset(heavyCity, 0, sizeof(heavyCity)); memset(snowCity, 0, sizeof(snowCity)); memset(cstate, 0, sizeof(cstate)); memset(crack_said, 0, sizeof(crack_said)); memset(drought_on, 0, sizeof(drought_on)); wTrack[0] = wTrack[1] = -1; memset(wLast, 0, sizeof(wLast));
    RIacc[1] = RIacc[2] = RIacc[3] = 0; RIdays = 0; memset(sx, 0, sizeof(sx)); memset(rep_, 0, sizeof(rep_)); memset(swe, 0, sizeof(swe)); swe_init = 0; memset(sRun, 0, sizeof(sRun)); memset(sRech, 0, sizeof(sRech)); memset(nJoin, 0, sizeof(nJoin)); cur_turn = -1; van_turn = -1; Y.valid = 0;
    ox = oy = 0; lphase = 0; lat_fill(latA, 64); lat_fill(latB, 64); lat_fill(latC, 28); lat_fill(latD, 28); lat_fill(latE, 8); lat_fill(latF, 8); ucdf_ok = 0;
    shown_level = rain_level; fr_rain_sum = 0; fr_rain_n = 0;   /* v26h: the frame watch keeps running across loads; only the rainy tally restarts */
    for (int c = 0; c < NCELL; c++) dryd[c] = (u16)(rnd() % 20);        /* the dry spell counters do not all start at zero */
}
