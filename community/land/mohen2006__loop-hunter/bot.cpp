// 圈地对战 bot（land）—— 独立实现。
//
// 核心思路：
//   1. 每回合重新规划"出圈—圈地—回家"的环路：环路由若干直线段组成
//      （Γ 形、U 形、矩形等），用引擎同规则模拟走位，模拟失败（撞墙 /
//      撞自己轨迹 / 无法闭合）的候选直接丢弃。
//   2. 用位并行泛洪（每行一个 uint64）从棋盘边界 flood，精确计算闭合后
//      被围住的格数作为收益；中性格计 1，对手领地计 EW，对手保护区不计。
//   3. 安全模型：对每个活跃对手从其头部做 (格, 方向) 状态 BFS（不穿过
//      它自己的轨迹，不能掉头），得到它到达每一格的最早回合；我轨迹上的
//      每一格，对手到达时间必须晚于我闭合环路的回合，否则候选不安全。
//   4. 切断：对手在圈外带轨迹时，若我能比它回家更快赶到它的某段轨迹、
//      且沿途安全，则主动切断（它清空轨迹、回出生点）。
//   5. 粘性：已在执行的计划有小幅加成，避免来回摇摆。
//   6. 保险层：最终落子前保证不撞墙、不撞自己轨迹，并尽量避开与对手
//      同格 / 互换位置的撞头。
#include <bits/stdc++.h>
using namespace std;

using u64 = unsigned long long;

static const int DX[4] = {0, 0, -1, 1};   // U D L R
static const int DY[4] = {-1, 1, 0, 0};
static const char DC[4] = {'U', 'D', 'L', 'R'};
static const int INF = 1e9;
static const int MAXC = 48 * 48;

// ---------------- 可调参数 ----------------
#ifndef P_C0_DUEL
#define P_C0_DUEL 20.0     // 单挑环路成本常数：评分 = 收益 / (长度 + C0)
#endif
#ifndef P_C0_MELEE
#define P_C0_MELEE 16.0    // 混战环路成本常数
#endif
#ifndef P_EW_DUEL
#define P_EW_DUEL 2.0      // 1v1 中对手领地格的权重
#endif
#ifndef P_EW_MELEE
#define P_EW_MELEE 1.2     // 混战中对手领地格的权重
#endif
#ifndef P_TOPK
#define P_TOPK 3000        // 进入精确泛洪评估的候选数
#endif
#ifndef P_A1
#define P_A1 26            // 第一段最大长度
#endif
#ifndef P_B1
#define P_B1 18            // 侧向段最大长度
#endif
#ifndef P_MS_TOTAL
#define P_MS_TOTAL 12.0     // 每回合规划总预算（ms）
#endif
#ifndef P_MS_EXACT
#define P_MS_EXACT 6.0     // 精确评估阶段预算（ms）
#endif
#ifndef P_MS_HILL
#define P_MS_HILL 3.5      // 变异刷新阶段预算（ms）
#endif
#ifndef P_STICK
#define P_STICK 1.03       // 沿用旧计划的评分加成
#endif
#ifndef P_MARGIN_DUEL
#define P_MARGIN_DUEL 0    // 1v1 安全余量
#endif
#ifndef P_MARGIN_MELEE
#define P_MARGIN_MELEE 0   // 混战安全余量
#endif
#ifndef P_HUNT_KB
#define P_HUNT_KB 6.0      // 切断价值常数
#endif
#ifndef P_HUNT_KW
#define P_HUNT_KW 1.5      // 切断价值随对手轨迹长度线性增长
#endif
#ifndef P_HUNT_RETW
#define P_HUNT_RETW 0.0    // 切断成本中回家距离的权重（0 = 只算去程）
#endif
#ifndef P_HUNT_LIMIT_DUEL
#define P_HUNT_LIMIT_DUEL 24
#endif
#ifndef P_HUNT_LIMIT_MELEE
#define P_HUNT_LIMIT_MELEE 10
#endif
#ifndef P_HUNT_OW
#define P_HUNT_OW 100000   // 其他对手威胁窗口 = 去程距离 + 此值（大 = 看到我回家）
#endif

// ---------------- 全局状态 ----------------
int W, H, MT, N, ME, MOVE_MS, INIT_MS, S;
int spx[4], spy[4];
int own[MAXC];              // -1 中性，0..3 玩家
int trl[MAXC];              // -1 无，否则轨迹主人
int prot[MAXC];             // -1 或保护区主人
struct Pl { int x, y, dir, tl, area, deaths; } pl[4];
int turnNo;
bool outP[4];               // 已判负出局（不再移动）
int lastPos[4], lastDeaths[4];

int deE[4][MAXC];           // 对手 e 到每一格的最早到达回合
int threat[MAXC];           // min over 活跃对手
int hd[MAXC];               // 每一格到我方领地的最短距离（避开我的轨迹）
int ehome[4];               // 对手 e 回到自己领地的最短距离
double EW;
double C0;   // 环路成本常数（按赛制）

static int qbuf[MAXC];
static int ps[MAXC + 64];   // own==ME 的二维前缀和，(W+1) 为跨度
u64 rNeu[64], rEne[64];     // 每行：可争夺的中性格 / 对手领地格（不含他人保护区）

chrono::steady_clock::time_point T0;
double elapsedMs() {
    return chrono::duration<double, milli>(chrono::steady_clock::now() - T0).count();
}

int dirIndex(char c) { return c == 'U' ? 0 : c == 'D' ? 1 : c == 'L' ? 2 : 3; }
inline bool inb(int x, int y) { return (unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H; }
inline int cid(int x, int y) { return y * W + x; }

// ---------------- 输入 ----------------
bool readState() {
    string tok;
    if (!(cin >> tok)) return false;
    cin >> turnNo;
    for (int i = 0; i < N; i++) {
        int id; char d;
        cin >> tok >> id >> pl[id].x >> pl[id].y >> d >> pl[id].tl >> pl[id].area >> pl[id].deaths;
        pl[id].dir = dirIndex(d);
    }
    cin >> tok;  // OWNER
    string row;
    for (int y = 0; y < H; y++) {
        cin >> row;
        for (int x = 0; x < W; x++) {
            char c = row[x];
            own[y * W + x] = c == '.' ? -1 : c - '0';
        }
    }
    cin >> tok;  // TRAIL
    for (int y = 0; y < H; y++) {
        cin >> row;
        for (int x = 0; x < W; x++) {
            char c = row[x];
            trl[y * W + x] = c == '.' ? -1 : c - '0';
        }
    }
    cin >> tok;  // END
    return true;
}

// ---------------- 对手到达时间 BFS ----------------
// 状态 = (格, 到达方向)；不能掉头，不能踩自己的轨迹。其他人的轨迹可以踩
// （死的是对方），所以不设障碍。这样得到的是对手到达每一格的最早回合。
void bfsEnemy(int e, int* dist) {
    static int sdist[MAXC * 4];
    static int sq[MAXC * 4];
    for (int i = 0; i < S * 4; i++) sdist[i] = INF;
    for (int k = 0; k < S; k++) dist[k] = INF;
    int sx = pl[e].x, sy = pl[e].y;
    int qh = 0, qt = 0;
    for (int d = 0; d < 4; d++) {
        if (d == (pl[e].dir ^ 1)) continue;
        int nx = sx + DX[d], ny = sy + DY[d];
        if (!inb(nx, ny)) continue;
        int nk = cid(nx, ny);
        if (trl[nk] == e) continue;
        int st = nk * 4 + d;
        if (sdist[st] != INF) continue;
        sdist[st] = 1;
        sq[qt++] = st;
        dist[nk] = 1;
    }
    while (qh < qt) {
        int st = sq[qh++];
        int k = st >> 2, d = st & 3;
        int x = k % W, y = k / W;
        int nd = sdist[st] + 1;
        for (int d2 = 0; d2 < 4; d2++) {
            if (d2 == (d ^ 1)) continue;
            int nx = x + DX[d2], ny = y + DY[d2];
            if (!inb(nx, ny)) continue;
            int nk = cid(nx, ny);
            if (trl[nk] == e) continue;
            int nst = nk * 4 + d2;
            if (sdist[nst] != INF) continue;
            sdist[nst] = nd;
            sq[qt++] = nst;
            if (dist[nk] > nd) dist[nk] = nd;
        }
    }
    dist[cid(sx, sy)] = 0;
}

// 对手 e 回到自己领地的最短距离（不穿自己轨迹）
int enemyHomeDist(int e) {
    if (own[cid(pl[e].x, pl[e].y)] == e) return 0;
    static int dist[MAXC];
    for (int k = 0; k < S; k++) dist[k] = INF;
    int qh = 0, qt = 0;
    int s = cid(pl[e].x, pl[e].y);
    dist[s] = 0;
    qbuf[qt++] = s;
    while (qh < qt) {
        int k = qbuf[qh++];
        int x = k % W, y = k / W;
        int nd = dist[k] + 1;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!inb(nx, ny)) continue;
            int nk = cid(nx, ny);
            if (trl[nk] == e || dist[nk] != INF) continue;
            dist[nk] = nd;
            if (own[nk] == e) return nd;
            qbuf[qt++] = nk;
        }
    }
    return INF;
}

// ---------------- 环路模拟 ----------------
struct Leg { int d, len; };   // len < 0 表示"一直到闭合为止"
struct Sim {
    int n = 0;                // 已走步数
    bool cap = false;         // 是否闭合（走进自己领地且有轨迹）
    int L = 0;                // 闭合时的步数
    int nt = 0;               // 新增轨迹格数
    int tcells[300];
    int minx, maxx, miny, maxy;
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
        int lim = len < 0 ? 70 : len;   // 开口段最多 70 步
        int cnt = 0;
        while (cnt < lim) {
            if (first && d == (pl[ME].dir ^ 1)) return false;  // 不能掉头
            first = false;
            x += DX[d]; y += DY[d];
            if (!inb(x, y)) return false;                      // 撞墙
            int k = cid(x, y);
            if (trl[k] == ME || visStamp[k] == stampCtr) return false;  // 撞自己轨迹
            if (s.nt >= 280 || s.n >= 400) return false;   // 防御：环路不可能更长
            s.n++;
            if (own[k] == ME) {
                if (hasTrail) { s.cap = true; s.L = s.n; return true; }  // 闭合
                // 在自己领地内移动，不产生轨迹
            } else {
                visStamp[k] = stampCtr;
                s.tcells[s.nt++] = k;
                hasTrail = true;
                if (x < s.minx) s.minx = x;
                if (x > s.maxx) s.maxx = x;
                if (y < s.miny) s.miny = y;
                if (y > s.maxy) s.maxy = y;
            }
            cnt++;
        }
    }
    s.L = s.n;
    return false;   // 段走完还没闭合
}

// ---------------- 位并行泛洪：精确收益 ----------------
inline u64 hfillL(u64 seed, u64 free) {   // 向高位（x+1）方向填
    u64 g = seed, p = free;
    g |= p & (g << 1); p &= p << 1;
    g |= p & (g << 2); p &= p << 2;
    g |= p & (g << 4); p &= p << 4;
    g |= p & (g << 8); p &= p << 8;
    g |= p & (g << 16); p &= p << 16;
    g |= p & (g << 32);
    return g;
}
inline u64 hfillR(u64 seed, u64 free) {   // 向低位（x-1）方向填
    u64 g = seed, p = free;
    g |= p & (g >> 1); p &= p >> 1;
    g |= p & (g >> 2); p &= p >> 2;
    g |= p & (g >> 4); p &= p >> 4;
    g |= p & (g >> 8); p &= p >> 8;
    g |= p & (g >> 16); p &= p >> 16;
    g |= p & (g >> 32);
    return g;
}

double constGain;   // 已有轨迹格的价值（闭合时一并变成领地）

double exactGain(const Sim& s) {
    static u64 wall[64], freeRow[64], reach[64];
    u64 full = (W == 64) ? ~0ULL : ((1ULL << W) - 1);
    for (int y = 0; y < H; y++) {
        u64 w = 0;
        int base = y * W;
        for (int x = 0; x < W; x++) {
            int k = base + x;
            if (own[k] == ME || trl[k] == ME) w |= 1ULL << x;
        }
        wall[y] = w;
    }
    double g = constGain;
    for (int i = 0; i < s.nt; i++) {
        int k = s.tcells[i];
        wall[k / W] |= 1ULL << (k % W);
        if (prot[k] != -1 && prot[k] != ME) continue;
        g += own[k] == -1 ? 1.0 : EW;   // own[k]!=ME 时是对手领地
    }
    for (int y = 0; y < H; y++) {
        u64 fr = ~wall[y] & full;
        freeRow[y] = fr;
        reach[y] = (y == 0 || y == H - 1) ? fr : (fr & (1ULL | (1ULL << (W - 1))));
    }
    bool ch = true;
    while (ch) {
        ch = false;
        for (int y = 0; y < H; y++) {
            u64 fr = freeRow[y];
            u64 sd = reach[y] | (fr & (y > 0 ? reach[y - 1] : 0)) | (fr & (y < H - 1 ? reach[y + 1] : 0));
            u64 nr = hfillL(sd, fr) | hfillR(sd, fr);
            if (nr != reach[y]) { reach[y] = nr; ch = true; }
        }
        for (int y = H - 1; y >= 0; y--) {
            u64 fr = freeRow[y];
            u64 sd = reach[y] | (fr & (y > 0 ? reach[y - 1] : 0)) | (fr & (y < H - 1 ? reach[y + 1] : 0));
            u64 nr = hfillL(sd, fr) | hfillR(sd, fr);
            if (nr != reach[y]) { reach[y] = nr; ch = true; }
        }
    }
    for (int y = 0; y < H; y++) {
        u64 enc = freeRow[y] & ~reach[y];
        if (enc) g += __builtin_popcountll(enc & rNeu[y]) + EW * __builtin_popcountll(enc & rEne[y]);
    }
    return g;
}

// 快速估计：轨迹外接矩形减去其中的已有我方格（用于候选排序）
double estGain(const Sim& s) {
    int x0 = s.minx, x1 = s.maxx, y0 = s.miny, y1 = s.maxy;
    int W1 = W + 1;
    int mine = ps[(y1 + 1) * W1 + x1 + 1] - ps[y0 * W1 + x1 + 1] - ps[(y1 + 1) * W1 + x0] + ps[y0 * W1 + x0];
    return (double)((x1 - x0 + 1) * (y1 - y0 + 1) - mine);
}

// ---------------- 计划（跨回合保留） ----------------
static Leg planLegs[8];
static int planNl = 0;
static int planExpX = -1, planExpY = -1, planDeaths = -1;

// 撞头风险：目标格是否是某活跃对手下一步可能进入的格（同格而死），
// 或我与它互换位置。对手自己的轨迹格不算——踩上去死的是它。
bool headOnRisk(int tx, int ty, int mx, int my) {
    for (int e = 0; e < N; e++) {
        if (e == ME || outP[e]) continue;
        for (int d = 0; d < 4; d++) {
            if (d == (pl[e].dir ^ 1)) continue;
            int nx = pl[e].x + DX[d], ny = pl[e].y + DY[d];
            if (!inb(nx, ny)) continue;
            if (trl[cid(nx, ny)] == e) continue;      // 它不会自杀
            if (nx == tx && ny == ty) return true;    // 同格
            if (nx == mx && ny == my && pl[e].x == tx && pl[e].y == ty) return true;  // 互换
        }
    }
    return false;
}

// ---------------- 主决策 ----------------
int decide() {
    T0 = chrono::steady_clock::now();
    const Pl& me = pl[ME];
    int remaining = MT - turnNo;
    int margin = (N == 2) ? P_MARGIN_DUEL : P_MARGIN_MELEE;

    // 出局检测：位置不变 + 无轨迹 + 阵亡数不变 -> 已判负
    for (int j = 0; j < N; j++) {
        if (j != ME && turnNo > 1 && pl[j].x == lastPos[j] % W && pl[j].y == lastPos[j] / W
            && pl[j].tl == 0 && pl[j].deaths == lastDeaths[j] && lastPos[j] >= 0) {
            outP[j] = true;
        }
        lastPos[j] = cid(pl[j].x, pl[j].y);
        lastDeaths[j] = pl[j].deaths;
    }

    // 前缀和（own==ME）
    int W1 = W + 1;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            ps[(y + 1) * W1 + x + 1] = ps[y * W1 + x + 1] + ps[(y + 1) * W1 + x] - ps[y * W1 + x]
                                       + (own[y * W + x] == ME ? 1 : 0);
        }
    }
    // 每行可争夺格
    for (int y = 0; y < H; y++) {
        u64 n = 0, e = 0;
        int base = y * W;
        for (int x = 0; x < W; x++) {
            int k = base + x;
            if (prot[k] != -1 && prot[k] != ME) continue;   // 他人保护区永不可能夺到
            if (own[k] == -1) n |= 1ULL << x;
            else if (own[k] != ME) e |= 1ULL << x;
        }
        rNeu[y] = n; rEne[y] = e;
    }
    // 对手到达时间场
    for (int k = 0; k < S; k++) threat[k] = INF;
    for (int e = 0; e < N; e++) {
        if (e == ME || outP[e]) { ehome[e] = 0; continue; }
        bfsEnemy(e, deE[e]);
        for (int k = 0; k < S; k++) threat[k] = min(threat[k], deE[e][k]);
        ehome[e] = (pl[e].tl > 0) ? enemyHomeDist(e) : 0;
    }
    // 已有轨迹的价值 + 轨迹上对手的最近到达时间
    constGain = 0;
    int minTrailThreat = INF;
    for (int k = 0; k < S; k++) {
        if (trl[k] != ME) continue;
        if (prot[k] == -1 || prot[k] == ME) constGain += own[k] == -1 ? 1.0 : EW;
        minTrailThreat = min(minTrailThreat, threat[k]);
    }
    // 到我方领地的距离场（避开我的轨迹）
    {
        int qh = 0, qt = 0;
        for (int k = 0; k < S; k++) {
            hd[k] = INF;
            if (own[k] == ME) { hd[k] = 0; qbuf[qt++] = k; }
        }
        while (qh < qt) {
            int k = qbuf[qh++];
            int x = k % W, y = k / W;
            int nd = hd[k] + 1;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inb(nx, ny)) continue;
                int nk = cid(nx, ny);
                if (trl[nk] == ME || hd[nk] != INF) continue;
                hd[nk] = nd;
                qbuf[qt++] = nk;
            }
        }
    }

    // 计划失效判定
    if (me.tl == 0 || me.deaths != planDeaths) { planNl = 0; }
    planDeaths = me.deaths;

    auto pathSafe = [&](const Sim& s) -> bool {
        int L = s.L;
        if (minTrailThreat <= L + margin) return false;
        for (int i = 0; i < s.nt; i++)
            if (threat[s.tcells[i]] <= L + margin) return false;
        return true;
    };

    double bestRate = -1e18;
    int bestDir = -1;
    Leg bestLegs[8]; int bestNl = 0;

    // ---- 1. 沿用旧计划（小幅加成） ----
    if (planNl > 0 && me.x == planExpX && me.y == planExpY && me.tl > 0) {
        Leg lg[8]; int nl = 0;
        for (int j = 0; j < planNl; j++) lg[nl++] = planLegs[j];
        if (lg[0].len > 0) {
            lg[0].len--;
            if (lg[0].len == 0) {
                for (int j = 1; j < nl; j++) lg[j - 1] = lg[j];
                nl--;
            }
        }
        Sim s;
        if (nl > 0 && simulate(lg, nl, s) && s.cap && s.L <= remaining && pathSafe(s)) {
            double rate = exactGain(s) / (s.L + C0) * P_STICK;
            bestRate = rate; bestDir = lg[0].d; bestNl = nl;
            for (int j = 0; j < nl; j++) bestLegs[j] = lg[j];
        }
    }

    // ---- 2. 枚举环路候选 ----
    struct Cand { double est; Leg legs[6]; int nl; };
    static vector<Cand> cands;
    cands.clear();
    Sim s;
    int A = P_A1, B = P_B1;
    for (int d1 = 0; d1 < 4; d1++) {
        if (d1 == (me.dir ^ 1)) continue;
        for (int a = 1; a <= A; a++) {
            for (int si = 0; si < 2; si++) {
                int sd = (d1 < 2 ? 2 : 0) + si;      // 垂直于 d1
                int sdR = sd ^ 1;
                // Γ 形：d1 走 a，sd 一路回家
                {
                    Cand c; c.nl = 2;
                    c.legs[0] = {d1, a}; c.legs[1] = {sd, -1};
                    if (simulate(c.legs, c.nl, s) && s.cap && s.L <= remaining && pathSafe(s)) {
                        c.est = estGain(s) / (s.L + C0);
                        cands.push_back(c);
                    }
                }
                for (int b = 1; b <= B; b++) {
                    // U 形：d1 a、sd b、d1^1 一路回家
                    {
                        Cand c; c.nl = 3;
                        c.legs[0] = {d1, a}; c.legs[1] = {sd, b}; c.legs[2] = {d1 ^ 1, -1};
                        if (simulate(c.legs, c.nl, s) && s.cap && s.L <= remaining && pathSafe(s)) {
                            c.est = estGain(s) / (s.L + C0);
                            cands.push_back(c);
                        }
                    }
                    // 矩形：d1 a、sd b、d1^1 a、sd^1 一路回家
                    {
                        Cand c; c.nl = 4;
                        c.legs[0] = {d1, a}; c.legs[1] = {sd, b}; c.legs[2] = {d1 ^ 1, a}; c.legs[3] = {sdR, -1};
                        if (simulate(c.legs, c.nl, s) && s.cap && s.L <= remaining && pathSafe(s)) {
                            c.est = estGain(s) / (s.L + C0);
                            cands.push_back(c);
                        }
                    }
                }
            }
        }
        if (elapsedMs() > P_MS_TOTAL * 0.45) break;   // 枚举本身别超时
    }

    // ---- 3. 估计值排序后精确评估 ----
    int K = min<int>(P_TOPK, (int)cands.size());
    if (K > 0) {
        partial_sort(cands.begin(), cands.begin() + K, cands.end(),
                     [](const Cand& x, const Cand& y) { return x.est > y.est; });
        for (int i = 0; i < K; i++) {
            if (elapsedMs() > P_MS_EXACT) break;
            Cand& c = cands[i];
            if (!simulate(c.legs, c.nl, s) || !s.cap) continue;
            double rate = exactGain(s) / (s.L + C0);
            if (rate > bestRate) {
                bestRate = rate; bestDir = c.legs[0].d; bestNl = c.nl;
                for (int j = 0; j < c.nl; j++) bestLegs[j] = c.legs[j];
            }
        }
    }

    // ---- 4. 以当前最优为中心做随机变异 ----
    if (bestNl > 0) {
        static uint64_t rs = 0x9E3779B97F4A7C15ULL;
        auto rnd = [&]() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (unsigned)(rs >> 11); };
        int iters = 0;
        while (elapsedMs() < P_MS_HILL && iters < 4000) {
            iters++;
            Leg lg[8]; int nl;
            if (rnd() & 1) {
                // 变异当前最优
                nl = bestNl;
                for (int j = 0; j < nl; j++) lg[j] = bestLegs[j];
                int m = -1;
                for (int t = 0; t < 6; t++) {
                    int c = rnd() % nl;
                    if (lg[c].len > 0) { m = c; break; }
                }
                if (m < 0) continue;
                int delta = (int)(rnd() % 7) - 3;
                if (delta == 0) delta = 1;
                lg[m].len += delta;
                if (lg[m].len < 1 || lg[m].len > 48) continue;
                if ((rnd() & 1) && nl >= 2) {   // 偶尔动两段
                    int m2 = rnd() % nl;
                    if (lg[m2].len > 0) {
                        lg[m2].len += (int)(rnd() % 5) - 2;
                        if (lg[m2].len < 1 || lg[m2].len > 48) continue;
                    }
                }
            } else {
                // 随机新形状
                nl = 2 + rnd() % 3;
                int d = rnd() % 4;
                if (d == (me.dir ^ 1)) d = me.dir;
                lg[0] = {d, 1 + (int)(rnd() % A)};
                for (int j = 1; j < nl; j++) {
                    int pd = lg[j - 1].d;
                    int nd = ((pd < 2) ? 2 : 0) + (rnd() & 1);
                    lg[j] = {nd, j == nl - 1 ? -1 : 1 + (int)(rnd() % B)};
                }
            }
            if (!simulate(lg, nl, s) || !s.cap || s.L > remaining || !pathSafe(s)) continue;
            double rate = exactGain(s) / (s.L + C0);
            if (rate > bestRate) {
                bestRate = rate; bestDir = lg[0].d; bestNl = nl;
                for (int j = 0; j < nl; j++) bestLegs[j] = lg[j];
            }
        }
    }

    // ---- 5. 切断机会 ----
    {
        // 从头部 BFS（避自己的轨迹），记录每格的首步方向和父格
        static int dm[MAXC], par[MAXC], pred[MAXC];
        for (int k = 0; k < S; k++) dm[k] = INF;
        int qh = 0, qt = 0;
        int st = cid(me.x, me.y);
        dm[st] = 0;
        for (int d = 0; d < 4; d++) {
            if (d == (me.dir ^ 1)) continue;
            int nx = me.x + DX[d], ny = me.y + DY[d];
            if (!inb(nx, ny)) continue;
            int k = cid(nx, ny);
            if (trl[k] == ME || dm[k] != INF) continue;
            dm[k] = 1; par[k] = d; pred[k] = st; qbuf[qt++] = k;
        }
        while (qh < qt) {
            int k = qbuf[qh++];
            int x = k % W, y = k / W;
            int nd = dm[k] + 1;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inb(nx, ny)) continue;
                int nk = cid(nx, ny);
                if (trl[nk] == ME || dm[nk] != INF) continue;
                dm[nk] = nd; par[nk] = par[k]; pred[nk] = k; qbuf[qt++] = nk;
            }
        }
        bool inside = own[st] == ME;
        int limit = (N == 2) ? (inside ? P_HUNT_LIMIT_DUEL : P_HUNT_LIMIT_DUEL / 2)
                             : (inside ? P_HUNT_LIMIT_MELEE : P_HUNT_LIMIT_MELEE / 2);
        double bestHuntRate = bestRate;
        int huntDir = -1;
        for (int e = 0; e < N; e++) {
            if (e == ME || outP[e] || pl[e].tl == 0) continue;
            // 找我能最快到达的它的轨迹格
            int bk = -1, bd = INF;
            for (int k = 0; k < S; k++) {
                if (trl[k] != e) continue;
                if (dm[k] < bd) { bd = dm[k]; bk = k; }
            }
            if (bk < 0 || bd > limit || bd > ehome[e]) continue;   // 到得比它回家还慢就没戏
            // 回溯路径
            int path[130]; int np = 0;
            int c = bk;
            while (c != st && np < 128) {
                path[np++] = c;
                c = pred[c];
            }
            int back = hd[bk] >= INF ? 60 : hd[bk];     // 切断后回家距离
            int T = bd + back + 2;
            int Tow = min(T, bd + P_HUNT_OW);           // 其他对手的威胁窗口
            bool safe = true;
            // 路径上的格：只有走出领地后的格才有轨迹、才怕被踩。
            // 领地内的格对手随便走都无所谓（踩不到我的轨迹）。
            for (int i = 0; i < np && safe; i++) {
                int k = path[i];
                if (own[k] == ME) continue;
                if (deE[e][k] <= bd) safe = false;
                for (int o = 0; o < N && safe; o++) {
                    if (o == ME || o == e || outP[o]) continue;
                    if (deE[o][k] <= Tow) safe = false;
                }
            }
            // 我已有的轨迹要能活到我回家
            if (safe && me.tl > 0) {
                for (int k = 0; k < S && safe; k++)
                    if (trl[k] == ME && threat[k] <= T) safe = false;
            }
            if (!safe) continue;
            double V = P_HUNT_KB + P_HUNT_KW * pl[e].tl;
            double rate = V / (bd + P_HUNT_RETW * back + 2.0);
            if (rate > bestHuntRate) { bestHuntRate = rate; huntDir = par[bk]; }
        }
        if (huntDir >= 0) {
            bestDir = huntDir; bestNl = 0;   // 切断是短计划，不设粘性
        }
    }

    // ---- 6. 落子前保险：避开撞头 ----
    if (bestDir >= 0) {
        int tx = me.x + DX[bestDir], ty = me.y + DY[bestDir];
        if (!inb(tx, ty) || trl[cid(tx, ty)] == ME || headOnRisk(tx, ty, me.x, me.y)) {
            int alt = -1, altScore = -INF;
            for (int d = 0; d < 4; d++) {
                if (d == (me.dir ^ 1)) continue;
                int nx = me.x + DX[d], ny = me.y + DY[d];
                if (!inb(nx, ny)) continue;
                int k = cid(nx, ny);
                if (trl[k] == ME) continue;
                if (headOnRisk(nx, ny, me.x, me.y)) continue;
                int sc = (own[k] == ME ? 30 : 0) + min(threat[k], 40);
                if (sc > altScore) { altScore = sc; alt = d; }
            }
            if (alt >= 0) {
                if (alt != bestDir) planNl = 0;
                bestDir = alt;
            }
            // 找不到安全替代就按原方向（横竖都是死）
        }
    }

    // ---- 7. 记录计划并返回 ----
    if (bestDir >= 0) {
        if (bestNl > 0) {
            planNl = bestNl;
            for (int j = 0; j < bestNl; j++) planLegs[j] = bestLegs[j];
            planExpX = me.x + DX[bestDir];
            planExpY = me.y + DY[bestDir];
        } else {
            planNl = 0;
        }
#ifdef DBG
        if (turnNo % 25 == 0)
            cerr << "t" << turnNo << " tl" << me.tl << " pos" << me.x << "," << me.y
                 << " d" << DC[bestDir] << " rate" << bestRate << " cands" << (int)cands.size()
                 << " ms" << elapsedMs() << endl;
#endif
        return bestDir;
    }
    planNl = 0;

    // ---- 8. 兜底 ----
    // 8a. 在领地内且无计划：尽量在领地内多走（深度受限）
    if (me.tl == 0) {
        int bestD = -1, bestScore = -1;
        for (int d = 0; d < 4; d++) {
            if (d == (me.dir ^ 1)) continue;
            int nx = me.x + DX[d], ny = me.y + DY[d];
            if (!inb(nx, ny) || trl[cid(nx, ny)] == ME) continue;
            int score = 0;
            if (own[cid(nx, ny)] == ME) {
                score = 1;
                // 深度 6：还能在领地内走几步
                function<int(int, int, int, int)> go = [&](int x, int y, int dir, int depth) -> int {
                    if (depth == 0) return 0;
                    int best = 0;
                    for (int e = 0; e < 4; e++) {
                        if (e == (dir ^ 1)) continue;
                        int ax = x + DX[e], ay = y + DY[e];
                        if (!inb(ax, ay) || own[cid(ax, ay)] != ME) continue;
                        best = max(best, 1 + go(ax, ay, e, depth - 1));
                    }
                    return best;
                };
                score += go(nx, ny, d, 6);
            }
            if (score > bestScore) { bestScore = score; bestD = d; }
        }
        if (bestD >= 0) return bestD;
    }
    // 8b. 在圈外：沿 hd 梯度最快回家
    {
        int bd = -1, bv = INF;
        for (int d = 0; d < 4; d++) {
            if (d == (me.dir ^ 1)) continue;
            int nx = me.x + DX[d], ny = me.y + DY[d];
            if (!inb(nx, ny)) continue;
            int k = cid(nx, ny);
            if (trl[k] == ME) continue;
            int v = hd[k];
            if (v < bv) { bv = v; bd = d; }
        }
        if (bd >= 0) return bd;
    }
    // 8c. 任何合法方向
    for (int d = 0; d < 4; d++) {
        if (d == (me.dir ^ 1)) continue;
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (inb(nx, ny) && trl[cid(nx, ny)] != ME) return d;
    }
    return me.dir;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string tok;
    cin >> tok;  // INIT
    cin >> W >> H >> MT >> N >> ME >> MOVE_MS >> INIT_MS;
    S = W * H;
    for (int i = 0; i < N; i++) {
        int id, x, y;
        cin >> tok >> id >> x >> y;
        spx[id] = x; spy[id] = y;
    }
    for (int k = 0; k < S; k++) { own[k] = -1; trl[k] = -1; prot[k] = -1; }
    for (int i = 0; i < N; i++)
        for (int y = spy[i] - 1; y <= spy[i] + 1; y++)
            for (int x = spx[i] - 1; x <= spx[i] + 1; x++)
                if (inb(x, y)) prot[cid(x, y)] = i;
    EW = (N == 2) ? P_EW_DUEL : P_EW_MELEE;
    C0 = (N == 2) ? P_C0_DUEL : P_C0_MELEE;
    for (int i = 0; i < N; i++) { lastPos[i] = -1; lastDeaths[i] = -1; outP[i] = false; }
    while (readState()) {
        int d = decide();
        cout << turnNo << ' ' << DC[d] << '\n' << flush;
    }
    return 0;
}
