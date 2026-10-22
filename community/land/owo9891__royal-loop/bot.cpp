// Territory bot: loop planner with enemy-arrival safety model + opportunistic trail cutting.
#include <bits/stdc++.h>
using namespace std;

#ifndef DBG_T0
#define DBG_T0 250
#endif
#ifndef DBG_T1
#define DBG_T1 300
#endif
#ifndef P_EWM
#define P_EWM 2.5
#endif
#ifndef P_EW2
#define P_EW2 2.5
#endif
#ifndef P_MARGIN
#define P_MARGIN 0
#endif
#ifndef P_RET_MODE
#define P_RET_MODE 2
#endif
#ifndef P_DEF_CUR
#define P_DEF_CUR 1
#endif
#ifndef P_DEF_FORMULA
#define P_DEF_FORMULA 1
#endif
#ifndef P_DEF_MINN
#define P_DEF_MINN 3
#endif
#ifndef P_ATK_TRIP
#define P_ATK_TRIP 0
#endif
#ifndef P_DEF_W
#define P_DEF_W 1.5
#endif
#ifndef P_DEF_ON
#define P_DEF_ON 0
#endif
#ifndef P_DEF_K
#define P_DEF_K 3.0
#endif
#ifndef P_DEF_THRESH
#define P_DEF_THRESH 50
#endif
#ifndef P_DEF_RISKY
#define P_DEF_RISKY 0
#endif
#ifndef P_C0
#define P_C0 20.0
#endif
#ifndef P_KB
#define P_KB 6.0
#endif
#ifndef P_KW
#define P_KW 1.5
#endif
#ifndef P_BUDGET_MS
#define P_BUDGET_MS 8.0
#endif
#ifndef P_RAND_ITERS
#define P_RAND_ITERS 3000
#endif
#ifndef P_RAND_MS
#define P_RAND_MS 5.5
#endif
#ifndef P_TOPK
#define P_TOPK 6000
#endif
// ---- two-loop lookahead (exit-position awareness) ----
#ifndef P2_ON
#define P2_ON 1
#endif
#ifndef P2_MAXN
#define P2_MAXN 16
#endif
#ifndef P2_DEADLINE
#define P2_DEADLINE 24.0
#endif
#ifndef P2_W2
#define P2_W2 1.0
#endif
// ---- contested-cell weighting ----
#ifndef P_CW
#define P_CW 1.2      // extra value of a cell the opponent can reach soon
#endif
#ifndef P_CW_SCALE
#define P_CW_SCALE 36.0
#endif
#ifndef P_ATK_CURRENCY
#define P_ATK_CURRENCY 1
#endif
#ifndef P_CW_NORM
#define P_CW_NORM 0     // 1 = divide gain by mean cell weight (keep C0 meaningful)
#endif
#ifndef P_CWM
#define P_CWM 0.0     // P_CW used in melee (N==4); 0 = no contested-cell weighting
#endif

const int DX[4] = {0, 0, -1, 1};
const int DY[4] = {-1, 1, 0, 0};
const char DC[4] = {'U', 'D', 'L', 'R'};
const int INF = 1e9;
const int MAXC = 48 * 48;

int W, H, MAX_TURNS, N, ME, MOVE_MS, INIT_MS;
int spx[4], spy[4];
int own[MAXC], trl[MAXC], prot[MAXC];
struct Player { int x, y, dir, tl, area, deaths; } pl[4];
int turnNo;
int fbCount = 0, outTurns = 0;
int deE[4][MAXC], deMin[MAXC], Te[4];
int hd[MAXC];      // distance to own territory
int ps[49 * 49];   // prefix sums of own==ME
vector<int> myTrail;
int minDeEx;
double EW;
int committed = INF;        // remaining length of the plan chosen last turn (after that move)
struct Leg { int d, len; };  // len<0: until captured
Leg planLegs[6]; int planNl = 0; int planExpX = -1, planExpY = -1; int planDeaths = 0;

// ---- two-loop lookahead state ----
struct EvTop { double r, g; int L, nl; Leg legs[6]; };
static EvTop evTop[P2_MAXN];
int evN = 0;
static int p2Enc[MAXC];          // enclosed cells of the first loop
static int savOwn[MAXC], savTrl[MAXC];
static double savCG;
static Player savMe;
double p2Deadline = P2_DEADLINE;

int dirIndex(char c) { return string("UDLR").find(c); }
inline bool inb(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }

chrono::steady_clock::time_point t0;
double elapsedMs() { return chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count(); }

bool readState() {
    string tok;
    if (!(cin >> tok)) return false;
    cin >> turnNo;
    for (int i = 0; i < N; i++) {
        int id; char d; Player q;
        cin >> tok >> id >> q.x >> q.y >> d >> q.tl >> q.area >> q.deaths;
        q.dir = dirIndex(d);
        pl[id] = q;
    }
    string row;
    cin >> tok;
    for (int y = 0; y < H; y++) {
        cin >> row;
        for (int x = 0; x < W; x++) own[y * W + x] = row[x] == '.' ? -1 : row[x] - '0';
    }
    cin >> tok;
    for (int y = 0; y < H; y++) {
        cin >> row;
        for (int x = 0; x < W; x++) trl[y * W + x] = row[x] == '.' ? -1 : row[x] - '0';
    }
    cin >> tok;
    return true;
}

static int qbuf[MAXC];
// BFS from (sx,sy) heading dir; cannot reverse on first step; cells with trl==blockOwner are blocked.
void bfsFrom(int sx, int sy, int dir, int blockOwner, int* dist) {
    for (int i = 0; i < W * H; i++) dist[i] = INF;
    int qh = 0, qt = 0;
    int s = sy * W + sx;
    dist[s] = 0;
    for (int d = 0; d < 4; d++) {
        if (d == (dir ^ 1)) continue;
        int nx = sx + DX[d], ny = sy + DY[d];
        if (!inb(nx, ny)) continue;
        int k = ny * W + nx;
        if (trl[k] == blockOwner || dist[k] != INF) continue;
        dist[k] = 1;
        qbuf[qt++] = k;
    }
    while (qh < qt) {
        int k = qbuf[qh++];
        int x = k % W, y = k / W;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!inb(nx, ny)) continue;
            int nk = ny * W + nx;
            if (trl[nk] == blockOwner || dist[nk] != INF) continue;
            dist[nk] = dist[k] + 1;
            qbuf[qt++] = nk;
        }
    }
}

// ---------- path simulation ----------
struct Sim {
    int n, L;
    bool cap;
    int cells[260];
    int nt;
    int tcells[260];
    int minx, maxx, miny, maxy;
    int lastDir;   // direction of the move that closed the loop
};
static int visStamp[MAXC];
static int stampCtr = 0;

bool simulate(const Leg* legs, int nl, Sim& s) {
    stampCtr++;
    int x = pl[ME].x, y = pl[ME].y;
    bool hasTrail = pl[ME].tl > 0;
    s.n = 0; s.nt = 0; s.cap = false;
    s.minx = s.maxx = x; s.miny = s.maxy = y;
    bool first = true;
    for (int li = 0; li < nl; li++) {
        int d = legs[li].d;
        int len = legs[li].len;
        int cnt = 0;
        int lim = len < 0 ? 45 : len;
        while (cnt < lim) {
            if (first && d == (pl[ME].dir ^ 1)) return false;
            first = false;
            x += DX[d]; y += DY[d];
            if (!inb(x, y)) return false;
            int k = y * W + x;
            if (trl[k] == ME || visStamp[k] == stampCtr) return false;
            if (s.n >= 250) return false;
            s.cells[s.n++] = k;
            cnt++;
            if (x < s.minx) s.minx = x;
            if (x > s.maxx) s.maxx = x;
            if (y < s.miny) s.miny = y;
            if (y > s.maxy) s.maxy = y;
            if (own[k] == ME) {
                if (hasTrail) { s.cap = true; s.lastDir = d; s.L = s.n; return true; }
            } else {
                visStamp[k] = stampCtr;
                s.tcells[s.nt++] = k;
                hasTrail = true;
            }
        }
    }
    s.L = s.n;
    return false;
}

bool pathSafe(const Sim& s) {
    int L = s.L + P_MARGIN;
    if (pl[ME].tl > 0 && minDeEx <= L) return false;
    for (int i = 0; i < s.nt; i++)
        if (deMin[s.tcells[i]] <= L) return false;
    return true;
}

typedef unsigned long long u64;
u64 rOwnMe[48], rTrlMe[48], rNeuNP[48], rEneNP[48], rFull;
u64 rNeuB[4][48], rEneB[4][48];   // neutral / enemy cells split by contestedness bucket
static const double CBW[4] = {1.0, 1.3, 1.6, 2.0};
double cellW[MAXC];
double meanCellW = 1.0;
double constGainW;
double constGain;  // value of my existing trail cells
inline double cellVal(int k) {
    if (prot[k] != -1 && prot[k] != ME) return 0;
    if (own[k] == ME) return 0;
    return own[k] == -1 ? 1.0 : EW;
}
inline u64 hfill(u64 seed, u64 free) {
    u64 g = seed, p = free;
    g |= p & (g << 1); p &= p << 1;
    g |= p & (g << 2); p &= p << 2;
    g |= p & (g << 4); p &= p << 4;
    g |= p & (g << 8); p &= p << 8;
    g |= p & (g << 16); p &= p << 16;
    g |= p & (g << 32);
    return g;
}
inline u64 hfillR(u64 seed, u64 free) {
    u64 g = seed, p = free;
    g |= p & (g >> 1); p &= p >> 1;
    g |= p & (g >> 2); p &= p >> 2;
    g |= p & (g >> 4); p &= p >> 4;
    g |= p & (g >> 8); p &= p >> 8;
    g |= p & (g >> 16); p &= p >> 16;
    g |= p & (g >> 32);
    return g;
}
inline double cellValW(int k) {
    if (prot[k] != -1 && prot[k] != ME) return 0;
    if (own[k] == ME) return 0;
    double base = (own[k] == -1) ? 1.0 : EW;
    return base * cellW[k];
}
// how contested is each cell? enemy arrival time -> weight bucket
void buildWeightMasks() {
    const double cw = (N == 2) ? P_CW : P_CWM;
    int n = W * H;
    for (int k = 0; k < n; k++) {
        double d = (deMin[k] >= (int)P_CW_SCALE) ? P_CW_SCALE : (double)deMin[k];
        if (d < 0) d = P_CW_SCALE;
        double t = 1.0 - d / P_CW_SCALE;
        cellW[k] = 1.0 + cw * t * t;
    }
    for (int y = 0; y < H; y++) {
        u64 nb[4] = {0, 0, 0, 0}, eb[4] = {0, 0, 0, 0};
        for (int x = 0; x < W; x++) {
            int k = y * W + x;
            if (prot[k] != -1 && prot[k] != ME) continue;
            if (own[k] == ME) continue;
            int b = 0;
            double w = cellW[k];
            if (w >= 1.75) b = 3; else if (w >= 1.45) b = 2; else if (w >= 1.15) b = 1;
            if (own[k] == -1) nb[b] |= 1ULL << x;
            else eb[b] |= 1ULL << x;
        }
        for (int b = 0; b < 4; b++) { rNeuB[b][y] = nb[b]; rEneB[b][y] = eb[b]; }
    }
    double sw = 0; int cnt = 0;
    for (int k = 0; k < n; k++) {
        if (prot[k] != -1 && prot[k] != ME) continue;
        if (own[k] == ME) continue;
        sw += cellW[k]; cnt++;
    }
    meanCellW = cnt ? (sw / cnt) : 1.0;
    if (meanCellW < 0.2) meanCellW = 1.0;
    constGainW = 0;
    for (int k : myTrail) constGainW += cellValW(k);
}
void buildRows() {
    rFull = (W == 64) ? ~0ULL : ((1ULL << W) - 1);
    for (int y = 0; y < H; y++) {
        u64 o = 0, t = 0, n = 0, e = 0;
        for (int x = 0; x < W; x++) {
            int k = y * W + x;
            if (own[k] == ME) o |= 1ULL << x;
            if (trl[k] == ME) t |= 1ULL << x;
            if (!(prot[k] != -1 && prot[k] != ME)) {
                if (own[k] == -1) n |= 1ULL << x;
                else if (own[k] != ME) e |= 1ULL << x;
            }
        }
        rOwnMe[y] = o; rTrlMe[y] = t; rNeuNP[y] = n; rEneNP[y] = e;
    }
    constGain = 0;
    for (int k : myTrail) constGain += cellVal(k);
}
// exact weighted gain of completing the sim'd loop (bit-parallel flood fill)
// if encOut/encNOut are given, the enclosed cell list is written out (for applying the capture)
double exactGain(const Sim& s, int* encOut = nullptr, int* encNOut = nullptr) {
    u64 wall[48], reach[48];
    for (int y = 0; y < H; y++) { wall[y] = rOwnMe[y] | rTrlMe[y]; reach[y] = 0; }
    double g = constGainW;
    for (int i = 0; i < s.nt; i++) {
        int k = s.tcells[i];
        wall[k / W] |= 1ULL << (k % W);
        g += cellValW(k);
    }
    u64 edge = 1ULL | (1ULL << (W - 1));
    for (int y = 0; y < H; y++) {
        u64 fr = ~wall[y] & rFull;
        reach[y] = (y == 0 || y == H - 1) ? fr : (fr & edge);
    }
    bool ch = true;
    while (ch) {
        ch = false;
        for (int y = 0; y < H; y++) {
            u64 fr = ~wall[y] & rFull;
            u64 sd = reach[y] | (fr & ((y > 0 ? reach[y - 1] : 0) | (y < H - 1 ? reach[y + 1] : 0)));
            u64 nr = hfill(sd, fr) | hfillR(sd, fr);
            if (nr != reach[y]) { reach[y] = nr; ch = true; }
        }
        for (int y = H - 1; y >= 0; y--) {
            u64 fr = ~wall[y] & rFull;
            u64 sd = reach[y] | (fr & ((y > 0 ? reach[y - 1] : 0) | (y < H - 1 ? reach[y + 1] : 0)));
            u64 nr = hfill(sd, fr) | hfillR(sd, fr);
            if (nr != reach[y]) { reach[y] = nr; ch = true; }
        }
    }
    for (int y = 0; y < H; y++) {
        u64 enc = ~wall[y] & rFull & ~reach[y];
        if (enc) {
            for (int b = 0; b < 4; b++)
                g += CBW[b] * (__builtin_popcountll(enc & rNeuB[b][y]) + EW * __builtin_popcountll(enc & rEneB[b][y]));
            if (encOut) {
                u64 e = enc;
                while (e) {
                    int b = __builtin_ctzll(e);
                    e &= e - 1;
                    encOut[(*encNOut)++] = y * W + b;
                }
            }
        }
    }
#if P_CW_NORM
    g /= meanCellW;
#endif
    return g;
}

double estGain(const Sim& s) {
    int x0 = s.minx, x1 = s.maxx, y0 = s.miny, y1 = s.maxy;
    int area = (x1 - x0 + 1) * (y1 - y0 + 1);
    int W1 = W + 1;
    int mine = ps[(y1 + 1) * W1 + x1 + 1] - ps[y0 * W1 + x1 + 1] - ps[(y1 + 1) * W1 + x0] + ps[y0 * W1 + x0];
    return area - mine;
}

// ---------------------------------------------------------------- two-loop lookahead
void resetTop() { evN = 0; }
void pushTop(double g, int L, const Leg* legs, int nl) {
    if (L <= 0) return;
    double r = g / (L + P_C0);
    int slot = -1;
    if (evN < P2_MAXN) slot = evN++;
    else {
        int w = 0;
        for (int i = 1; i < evN; i++) if (evTop[i].r < evTop[w].r) w = i;
        if (r <= evTop[w].r) return;
        slot = w;
    }
    EvTop& e = evTop[slot];
    e.r = r; e.g = g; e.L = L; e.nl = nl;
    for (int j = 0; j < nl; j++) e.legs[j] = legs[j];
}

void saveBoard() {
    memcpy(savOwn, own, sizeof(int) * W * H);
    memcpy(savTrl, trl, sizeof(int) * W * H);
    savCG = constGain;
    savMe = pl[ME];
}
void rebuildMine() {
    myTrail.clear();
    for (int k = 0; k < W * H; k++) if (trl[k] == ME) myTrail.push_back(k);
    buildRows();
    buildWeightMasks();
}
void restoreBoard() {
    memcpy(own, savOwn, sizeof(int) * W * H);
    memcpy(trl, savTrl, sizeof(int) * W * H);
    constGain = savCG;
    pl[ME] = savMe;
    rebuildMine();
}

// Apply the first loop's capture to the live board (so the second loop can be searched from the exit).
void applyCapture(const Sim& s, int encN) {
    for (int k = 0; k < W * H; k++)
        if (trl[k] == ME) {
            if (prot[k] == -1 || prot[k] == ME) own[k] = ME;
            trl[k] = -1;
        }
    for (int i = 0; i < s.nt; i++) {
        int k = s.tcells[i];
        if (prot[k] == -1 || prot[k] == ME) own[k] = ME;
        trl[k] = -1;
    }
    for (int i = 0; i < encN; i++) {
        int k = p2Enc[i];
        if (prot[k] == -1 || prot[k] == ME) own[k] = ME;
    }
    int lastK = s.cells[s.n - 1];
    pl[ME].x = lastK % W;
    pl[ME].y = lastK / W;
    pl[ME].dir = s.lastDir;
    pl[ME].tl = 0;
    rebuildMine();
    // refresh prefix sums for estGain under the new board
    int W1 = W + 1;
    for (int x = 0; x <= W; x++) ps[x] = 0;
    for (int y = 0; y <= H; y++) ps[y * W1] = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            ps[(y + 1) * W1 + x + 1] = ps[y * W1 + x + 1] + ps[(y + 1) * W1 + x] - ps[y * W1 + x] + (own[y * W + x] == ME);
}

// Coarse second-loop search from the current (post-capture) position.
// returns the best second-loop gain and its length.
double searchSecond(int rem2, int& outL) {
    static const int ST[] = {2, 4, 6, 8, 11, 15, 20};
    const int NS = 7;
    struct SC { double est; Leg legs[4]; int nl; };
    static SC scs[420];
    int nsc = 0;
    Sim s2;
    int d0 = pl[ME].dir;
    for (int d1 = 0; d1 < 4; d1++) {
        if (d1 == (d0 ^ 1)) continue;
        for (int ia = 0; ia < NS; ia++) {
            int a = ST[ia];
            for (int si = 0; si < 2; si++) {
                int sd = (d1 < 2 ? 2 : 0) + si;
                Leg lg[4];
                lg[0] = {d1, a}; lg[1] = {sd, -1};
                if (simulate(lg, 2, s2) && s2.cap && s2.L <= rem2 && pathSafe(s2) && nsc < 418) {
                    SC& q = scs[nsc++];
                    q.est = estGain(s2) / (s2.L + 1.0); q.nl = 2;
                    q.legs[0] = lg[0]; q.legs[1] = lg[1];
                }
                for (int ib = 0; ib < NS; ib++) {
                    int b = ST[ib];
                    lg[0] = {d1, a}; lg[1] = {sd, b}; lg[2] = {d1 ^ 1, -1};
                    if (simulate(lg, 3, s2) && s2.cap && s2.L <= rem2 && pathSafe(s2) && nsc < 418) {
                        SC& q = scs[nsc++];
                        q.est = estGain(s2) / (s2.L + 1.0); q.nl = 3;
                        q.legs[0] = lg[0]; q.legs[1] = lg[1]; q.legs[2] = lg[2];
                    }
                }
            }
        }
    }
    outL = 0;
    if (!nsc) return 0;
    int K = nsc < 50 ? nsc : 50;
    nth_element(scs, scs + K, scs + nsc, [](const SC& x, const SC& y) { return x.est > y.est; });
    double best = 0;
    for (int i = 0; i < K; i++) {
        if (simulate(scs[i].legs, scs[i].nl, s2) && s2.cap) {
            double g = exactGain(s2);
            if (g > best) { best = g; outL = s2.L; }
        }
    }
    return best;
}

// combined two-loop score of a first-loop candidate
double twoLoopScore(const EvTop& e1, int remaining, int& outL2) {
    Sim s1;
    if (!simulate(e1.legs, e1.nl, s1) || !s1.cap) return -1;
    int encN = 0;
    double g1 = exactGain(s1, p2Enc, &encN);
    int rem2 = remaining - s1.L;
    double g2 = 0; int L2 = 0;
    if (rem2 > 3) {
        saveBoard();
        applyCapture(s1, encN);
        g2 = searchSecond(rem2, L2);
        restoreBoard();
    }
    outL2 = L2;
    return (g1 + P2_W2 * g2) / (s1.L + L2 + P_C0);
}

struct Cand { double est; Leg legs[4]; int nl; };

// pick a move that stays inside own territory as long as possible
int stayInside() {
    const Player& me = pl[ME];
    int bestD = -1, bestScore = -1;
    for (int d = 0; d < 4; d++) {
        if (d == (me.dir ^ 1)) continue;
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (!inb(nx, ny) || trl[ny * W + nx] == ME) continue;
        int score = 0;
        if (own[ny * W + nx] == ME) {
            score = 1;
            // depth-limited lookahead on staying inside
            function<int(int, int, int, int)> go = [&](int x, int y, int dir, int depth) -> int {
                if (depth == 0) return 0;
                int best = 0;
                for (int e = 0; e < 4; e++) {
                    if (e == (dir ^ 1)) continue;
                    int ax = x + DX[e], ay = y + DY[e];
                    if (!inb(ax, ay) || own[ay * W + ax] != ME) continue;
                    best = max(best, 1 + go(ax, ay, e, depth - 1));
                }
                return best;
            };
            score += go(nx, ny, d, 6);
        }
        if (score > bestScore) { bestScore = score; bestD = d; }
    }
    return bestD;
}


// How many of MY cells would enemy `e` capture if it closed its current loop now?
// Walls = e's territory + e's trail + e's shortest return path; flood from the border.
int projectedSteal(int e) {
    if (pl[e].tl <= 0) return 0;
    int n = W * H;
    static int dist[MAXC], par[MAXC], qq[MAXC];
    for (int k = 0; k < n; k++) dist[k] = INF;
    int st = pl[e].y * W + pl[e].x;
    dist[st] = 0;
    int qh = 0, qt = 0;
    qq[qt++] = st;
    int goal = -1;
    while (qh < qt) {
        int k = qq[qh++];
        if (own[k] == e) { goal = k; break; }
        int x = k % W, y = k / W;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            int nk = ny * W + nx;
            if (trl[nk] == e || dist[nk] != INF) continue;
            dist[nk] = dist[k] + 1;
            par[nk] = k;
            qq[qt++] = nk;
        }
    }
    if (goal < 0) return 0;
    static unsigned char wall[MAXC], vis[MAXC];
    for (int k = 0; k < n; k++) wall[k] = (own[k] == e || trl[k] == e) ? 1 : 0;
    for (int c = goal; c != st; c = par[c]) wall[c] = 1;
    for (int k = 0; k < n; k++) vis[k] = 0;
    qh = 0; qt = 0;
    for (int k = 0; k < n; k++) {
        int x = k % W;
        bool border = (x == 0 || x == W - 1 || k < W || k >= n - W);
        if (border && !wall[k] && !vis[k]) { vis[k] = 1; qq[qt++] = k; }
    }
    while (qh < qt) {
        int k = qq[qh++];
        int x = k % W, y = k / W;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            int nk = ny * W + nx;
            if (wall[nk] || vis[nk]) continue;
            vis[nk] = 1;
            qq[qt++] = nk;
        }
    }
    int cnt = 0;
    for (int k = 0; k < n; k++) if (!wall[k] && !vis[k] && own[k] == ME) cnt++;
    return cnt;
}

int decide() {
    t0 = chrono::steady_clock::now();
    const Player& me = pl[ME];
    int WH = W * H;
    int remaining = MAX_TURNS - turnNo;

    // prefix sums
    int W1 = W + 1;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            ps[(y + 1) * W1 + x + 1] = ps[y * W1 + x + 1] + ps[(y + 1) * W1 + x] - ps[y * W1 + x] + (own[y * W + x] == ME);
    myTrail.clear();
    for (int k = 0; k < WH; k++) if (trl[k] == ME) myTrail.push_back(k);
    buildRows();

    // enemy distance maps
    for (int k = 0; k < WH; k++) deMin[k] = INF;
    for (int e = 0; e < N; e++) {
        if (e == ME) continue;
        bfsFrom(pl[e].x, pl[e].y, pl[e].dir, e, deE[e]);
        for (int k = 0; k < WH; k++) deMin[k] = min(deMin[k], deE[e][k]);
        int t = INF;
        for (int k = 0; k < WH; k++) if (own[k] == e) t = min(t, deE[e][k]);
        Te[e] = (own[pl[e].y * W + pl[e].x] == e) ? 0 : t;
    }
    buildWeightMasks();
    minDeEx = INF;
    for (int k : myTrail) minDeEx = min(minDeEx, deMin[k]);
    {   // distance to own territory, avoiding my trail
        int qh = 0, qt = 0;
        for (int k = 0; k < WH; k++) { hd[k] = INF; if (own[k] == ME) { hd[k] = 0; qbuf[qt++] = k; } }
        while (qh < qt) {
            int k = qbuf[qh++];
            int x = k % W, y = k / W;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inb(nx, ny)) continue;
                int nk = ny * W + nx;
                if (trl[nk] == ME || hd[nk] != INF) continue;
                hd[nk] = hd[k] + 1;
                qbuf[qt++] = nk;
            }
        }
    }
    // commitment: while outside, the plan may never get longer than what is left of the last one
    if (me.tl == 0 || me.deaths != planDeaths) { committed = INF; planNl = 0; }
    planDeaths = me.deaths;
    int limitL = (me.tl > 0) ? committed - 1 : INF;
#ifndef USE_LIMIT
    limitL = INF;
#endif
    if (limitL > remaining) limitL = remaining;

    // ---- loop candidates ----
    vector<Cand> cands;
    cands.reserve(4000);
    Sim s;
    int A = 26, B = 20;
    for (int d1 = 0; d1 < 4; d1++) {
        if (d1 == (me.dir ^ 1)) continue;
        for (int a = 1; a <= A; a++) {
            for (int si = 0; si < 2; si++) {
                int sd = (d1 < 2 ? 2 : 0) + si;
                // 2-leg
                {
                    Cand c; c.nl = 2;
                    c.legs[0] = {d1, a}; c.legs[1] = {sd, -1};
                    if (simulate(c.legs, c.nl, s) && s.cap && s.L <= limitL && pathSafe(s)) {
                        c.est = estGain(s) / (s.L + P_C0);
                        cands.push_back(c);
                    }
                }
                for (int b = 1; b <= B; b++) {
                    Cand c; c.nl = 3;
                    c.legs[0] = {d1, a}; c.legs[1] = {sd, b}; c.legs[2] = {d1 ^ 1, -1};
                    if (simulate(c.legs, c.nl, s) && s.cap && s.L <= limitL && pathSafe(s)) {
                        c.est = estGain(s) / (s.L + P_C0);
                        cands.push_back(c);
                    }
                }
            }
        }
    }
    int K = min<int>(P_TOPK, cands.size());
    if (K > 0) {
        partial_sort(cands.begin(), cands.begin() + K, cands.end(), [](const Cand& a, const Cand& b) { return a.est > b.est; });
    }
    double bestRate = 0;
    int bestDir = -1;
    Leg bestLegs[6]; int bestNl = 0;
    resetTop();
    if (planNl > 0 && me.x == planExpX && me.y == planExpY && me.tl > 0) {
        Leg lg[6]; int nl = 0;
        for (int j = 0; j < planNl; j++) lg[nl++] = planLegs[j];
        if (lg[0].len > 0) { lg[0].len--; if (lg[0].len == 0) { for (int j = 1; j < nl; j++) lg[j - 1] = lg[j]; nl--; } }
        if (nl > 0 && simulate(lg, nl, s) && s.cap && s.L <= limitL && pathSafe(s)) {
            double g = exactGain(s);
            double rate = g / (s.L + P_C0);
            bestRate = rate; bestDir = lg[0].d; bestNl = nl; for (int j = 0; j < nl; j++) bestLegs[j] = lg[j];
            bestRate = rate * 1.02;  // slight stickiness
            pushTop(g, s.L, lg, nl);
        }
    }
    for (int i = 0; i < K; i++) {
        if (elapsedMs() > P_BUDGET_MS) break;
        Cand& c = cands[i];
        if (!simulate(c.legs, c.nl, s)) continue;
        double g = exactGain(s);
        double rate = g / (s.L + P_C0);
        pushTop(g, s.L, c.legs, c.nl);
        if (rate > bestRate) {
            bestRate = rate; bestDir = c.legs[0].d;
            bestNl = c.nl; for (int j = 0; j < c.nl; j++) bestLegs[j] = c.legs[j];
        }
    }
    // random / hill-climbing refinement with the remaining budget
    {
        static uint64_t rs = 88172645463325252ULL;
        auto rnd = [&]() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (unsigned)(rs >> 11); };
        int iters = 0;
        while (iters < P_RAND_ITERS && elapsedMs() < P_RAND_MS) {
            iters++;
            Leg lg[6]; int nl;
            if (bestNl > 0 && (rnd() & 1)) {
                nl = bestNl;
                for (int j = 0; j < nl; j++) lg[j] = bestLegs[j];
                int m = rnd() % nl;
                if (lg[m].len < 0) m = (nl >= 2) ? nl - 2 : 0;
                if (lg[m].len < 0) continue;
                int delta = (int)(rnd() % 7) - 3;
                if (delta == 0) delta = 1;
                lg[m].len += delta;
                if (lg[m].len < 1) continue;
                if (rnd() % 3 == 0 && nl >= 2) {  // second mutation
                    int m2 = rnd() % nl;
                    if (lg[m2].len > 0) { lg[m2].len += (int)(rnd() % 5) - 2; if (lg[m2].len < 1) continue; }
                }
            } else {
                nl = 3 + rnd() % 3;
                int d = rnd() % 4;
                if (d == (me.dir ^ 1)) d = me.dir;
                lg[0] = {d, 1 + (int)(rnd() % 22)};
                for (int j = 1; j < nl; j++) {
                    int pd = lg[j - 1].d;
                    int nd = ((pd < 2) ? 2 : 0) + (rnd() & 1);
                    lg[j] = {nd, j == nl - 1 ? -1 : 1 + (int)(rnd() % 14)};
                }
            }
            if (!simulate(lg, nl, s) || !s.cap || s.L > limitL || !pathSafe(s)) continue;
            double g = exactGain(s);
            double rate = g / (s.L + P_C0);
            if (rate > bestRate) {
                bestRate = rate; bestDir = lg[0].d;
                bestNl = nl; for (int j = 0; j < nl; j++) bestLegs[j] = lg[j];
            }
        }
    }

    int bestL = 0;
    if (bestDir >= 0) { if (simulate(bestLegs, bestNl, s)) bestL = s.L; else bestL = INF; }
    // ---- two-loop lookahead: prefer first loops that leave a good follow-up ----
#if P2_ON
    if (evN > 0 && elapsedMs() < p2Deadline) {
        // make sure the current incumbent (possibly from random refinement) is in the pool
        if (bestNl > 0 && bestDir >= 0) {
            if (simulate(bestLegs, bestNl, s)) pushTop(exactGain(s), s.L, bestLegs, bestNl);
        }
        double bestComb = -1;
        Leg bl[6]; int bnl = 0, bL1 = 0;
        int rem = remaining;
        for (int i = 0; i < evN; i++) {
            if (elapsedMs() > p2Deadline) break;
            int L2 = 0;
            double sc = twoLoopScore(evTop[i], rem, L2);
            if (sc > bestComb) {
                bestComb = sc;
                bnl = evTop[i].nl;
                for (int j = 0; j < bnl; j++) bl[j] = evTop[i].legs[j];
                bL1 = evTop[i].L;
            }
        }
        if (bestComb > 0 && bnl > 0) {
            bestDir = bl[0].d; bestNl = bnl; bestL = bL1;
            for (int j = 0; j < bnl; j++) bestLegs[j] = bl[j];
        }
    }
#endif
    // ---- attack candidates ----
    {
        static int dm[MAXC], par[MAXC];
        // BFS from my head, blocked by my own trail; keep parent
        for (int k = 0; k < WH; k++) dm[k] = INF;
        int qh = 0, qt = 0;
        int st = me.y * W + me.x;
        dm[st] = 0;
        for (int d = 0; d < 4; d++) {
            if (d == (me.dir ^ 1)) continue;
            int nx = me.x + DX[d], ny = me.y + DY[d];
            if (!inb(nx, ny)) continue;
            int k = ny * W + nx;
            if (trl[k] == ME || dm[k] != INF) continue;
            dm[k] = 1; par[k] = d; qbuf[qt++] = k;
        }
        while (qh < qt) {
            int k = qbuf[qh++];
            int x = k % W, y = k / W;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inb(nx, ny)) continue;
                int nk = ny * W + nx;
                if (trl[nk] == ME || dm[nk] != INF) continue;
                dm[nk] = dm[k] + 1; par[nk] = d; qbuf[qt++] = nk;
            }
        }
        for (int e = 0; e < N; e++) {
            if (e == ME || pl[e].tl == 0) continue;
            int bk = -1, bd = INF;
            for (int k = 0; k < WH; k++)
                if (trl[k] == e && dm[k] < bd) { bd = dm[k]; bk = k; }
            if (bk < 0 || bd > Te[e]) continue;
            int T = bd + (hd[bk] >= INF ? 1000 : hd[bk]);
            if (T > limitL) continue;
            int dirsBuf[128], n = 0;
            int c = bk;
            while (c != st && n < 120) {
                int d = par[c];
                dirsBuf[n++] = d;
                c = (c % W - DX[d]) + (c / W - DY[d]) * W;
            }
            reverse(dirsBuf, dirsBuf + n);
            bool safe = true;
            int x = me.x, y = me.y;
            for (int i = 0; i < n && safe; i++) {
                x += DX[dirsBuf[i]]; y += DY[dirsBuf[i]];
                int k = y * W + x;
                if (i == n - 1) break;
                if (own[k] == ME) continue;
                if (deE[e][k] <= bd) safe = false;
                for (int o = 0; o < N && safe; o++) {
                    if (o == ME || o == e) continue;
                    if (deE[o][k] <= T) safe = false;
                }
            }
            if (safe && me.tl > 0) {
                for (int k : myTrail) {
                    if (deE[e][k] <= bd) { safe = false; break; }
                    for (int o = 0; o < N && safe; o++)
                        if (o != ME && o != e && deE[o][k] <= T) safe = false;
                    if (!safe) break;
                }
            }
            int defRisk = 0;
#if P_DEF_ON
            if (N == 2) defRisk = projectedSteal(e);
#endif
            if (!safe) {
#if P_DEF_RISKY > 0
                if (N != 2 || defRisk < P_DEF_THRESH) continue;
#else
                continue;
#endif
            }
            double V = P_KB + P_KW * pl[e].tl;
#if P_DEF_ON
            if (N == 2 && defRisk >= P_DEF_THRESH) V += P_DEF_K * defRisk;
#endif
            // currency fix: denied cells worth the same weighted amount as enclosed cells
#if P_ATK_CURRENCY
            V *= meanCellW;
#endif
            // --- user's defence formulation -------------------------------------
            // defence time = (head -> intercept) + (intercept -> my border)
            int backHome = (hd[bk] >= INF ? 60 : hd[bk]);
            int headHome = (hd[me.y * W + me.x] >= INF ? 60 : hd[me.y * W + me.x]);
            if (P_RET_MODE == 0) backHome = 0;
            else if (P_RET_MODE == 2) { backHome = backHome - headHome; if (backHome < 0) backHome = 0; }
            double rate;
#if P_ATK_TRIP
            rate = V / (bd + backHome + 2.0);
#else
            rate = V / (bd + 2.0);
#endif
#if P_DEF_FORMULA
            if (N == 2) {
                int saved = projectedSteal(e);
#if P_DEF_CUR
                double defRate = P_DEF_W * meanCellW * saved / (double)(bd + backHome + 2);
#else
                double defRate = P_DEF_W * saved / (double)(bd + backHome + 2);
#endif
                if (defRate > rate) rate = defRate;
            }
#endif
            // --------------------------------------------------------------------
            if (rate > bestRate) { bestRate = rate; bestDir = dirsBuf[0]; bestNl = 0; bestL = T; }
        }
    }

#ifdef DEBUG
    if (turnNo >= DBG_T0 && turnNo < DBG_T1) cerr << "E" << pl[1-ME].x << ',' << pl[1-ME].y << 'd' << pl[1-ME].dir << " etl" << pl[1-ME].tl << " " << "t" << turnNo << " tl" << me.tl << " pos " << me.x << "," << me.y << " best " << bestDir << " rate " << bestRate << " nl " << bestNl << " cands " << cands.size() << " minDeEx " << minDeEx << " limitL " << limitL << " bestL " << bestL << endl;
#endif
    if (bestDir >= 0) {
        if (bestNl > 0) { planNl = bestNl; for (int j = 0; j < bestNl; j++) planLegs[j] = bestLegs[j]; }
        else planNl = 0;
        committed = bestL;
        planExpX = me.x + DX[bestDir]; planExpY = me.y + DY[bestDir];
        return bestDir;
    }
    planNl = 0;

    // ---- fallbacks ----
    fbCount++;
#ifdef DEBUG
    if (turnNo % 5 == 0 && me.tl == 0) cerr << "FB t" << turnNo << " tl " << me.tl << " pos " << me.x << ',' << me.y << " cands " << cands.size() << " limitL " << limitL << endl;
#endif
    if (me.tl == 0) {
        int d = stayInside();
        if (d >= 0) return d;
    }
    // outside or no inside move: go home by shortest path
    {
        static int dist[MAXC];
        // BFS backwards from own territory: distance to nearest own cell avoiding my trail
        for (int k = 0; k < WH; k++) dist[k] = INF;
        int qh = 0, qt = 0;
        for (int k = 0; k < WH; k++) if (own[k] == ME) { dist[k] = 0; qbuf[qt++] = k; }
        while (qh < qt) {
            int k = qbuf[qh++];
            int x = k % W, y = k / W;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inb(nx, ny)) continue;
                int nk = ny * W + nx;
                if (trl[nk] == ME || dist[nk] != INF) continue;
                dist[nk] = dist[k] + 1;
                qbuf[qt++] = nk;
            }
        }
        int bd = -1, bv = INF;
        for (int d = 0; d < 4; d++) {
            if (d == (me.dir ^ 1)) continue;
            int nx = me.x + DX[d], ny = me.y + DY[d];
            if (!inb(nx, ny) || trl[ny * W + nx] == ME) continue;
            int v = dist[ny * W + nx];
            if (v < bv) { bv = v; bd = d; }
        }
        if (bd >= 0) return bd;
    }
    for (int d = 0; d < 4; d++) {
        if (d == (me.dir ^ 1)) continue;
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (inb(nx, ny)) return d;
    }
    return me.dir;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string tok;
    cin >> tok;
    cin >> W >> H >> MAX_TURNS >> N >> ME >> MOVE_MS >> INIT_MS;
    for (int i = 0; i < N; i++) {
        int id, x, y;
        cin >> tok >> id >> x >> y;
        spx[id] = x; spy[id] = y;
    }
    for (int k = 0; k < W * H; k++) prot[k] = -1;
    for (int i = 0; i < N; i++)
        for (int y = spy[i] - 1; y <= spy[i] + 1; y++)
            for (int x = spx[i] - 1; x <= spx[i] + 1; x++)
                if (inb(x, y)) prot[y * W + x] = i;
    EW = (N == 2) ? P_EW2 : P_EWM;
    while (readState()) {
        int d = decide();
#ifdef DEBUG
        if (pl[ME].tl > 0) outTurns++;
        if (turnNo % 50 == 0 || (turnNo>=280 && turnNo<=288)) { cerr << "t" << turnNo; for (int i = 0; i < N; i++) cerr << " [" << pl[i].area << " d" << pl[i].deaths << "]"; cerr << endl; }
        if (turnNo == MAX_TURNS - 1) cerr << "fallback " << fbCount << " outside " << outTurns << " deaths " << pl[ME].deaths << " area " << pl[ME].area << endl;
#endif
        cout << turnNo << ' ' << DC[d] << endl;
    }
}
