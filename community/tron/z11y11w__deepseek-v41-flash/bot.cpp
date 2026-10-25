// 光轮对战 bot。
// 思路：
//   2 人局：精确的最大-最小搜索（每回合先我选、再对手选，用真正的 step 结算抢格），
//           叶子用"我比对手多能霸占多少格"（多源 BFS 的势力范围）评估，迭代加深。
//   3 人以上：我分支、对手按"能占最多空地"的贪心策略预测，逐步推演，叶子同样用势力范围评估。
// 所有搜索都有时间闸：留 6 毫秒余量（真实比赛按 CPU 毫秒；设了 TRON_MS 环境变量时按墙上时钟，方便快速测试）。
#include "tron.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

using tron::State;

constexpr int MAXN = tron::MAXN;
constexpr int MAXC = tron::MAXH * tron::MAXW;
constexpr double INF = 1e18;
constexpr double DEAD = -1e5;   // 我死了
constexpr double WIN = 1e5;     // 对手死的死、我笑到最后

int gMe = 0;
long long gNodes = 0;
bool gAbort = false;
double gWallLimit = 0.0;   // >0 时按墙上毫秒停（测试用）
double gHardWall = 350.0;  // 墙上时钟保险
std::chrono::steady_clock::time_point gT0;

int gDst[MAXC], gOwn[MAXC];
int gQx[MAXC], gQy[MAXC];
int gArea[MAXN];
int gStamp[MAXC];
int gStampV = 0;

inline double wallMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - gT0).count();
}

// 真正查时间（比较贵，只由 checkTime 每 32 拍调一次）
inline bool timeUp() {
    if (gAbort) return true;
    const double w = wallMs();
    if ((gWallLimit > 0.0 && w > gWallLimit) || w > gHardWall || tron::budget_left() < 6.0) {
        gAbort = true;
        return true;
    }
    return false;
}

// 每 32 拍查一次：拍子在一整块大活（一次洪泛、一次评估、一个分支）前打点，粒度约 0.1~0.3ms
inline bool checkTime() {
    if (gAbort) return true;
    if ((++gNodes & 31) != 0) return false;
    return timeUp();
}

// ---------------- 局面评估 ----------------
// 多源 BFS：所有人同时向外扩，谁先到算谁的（势力范围）。比只看自己的连通块更贴近真实博弈。
void computeAreas(const State& s) {
    const int W = s.W, H = s.H, n = W * H;
    for (int i = 0; i < n; ++i) {
        gDst[i] = -1;
        gOwn[i] = -1;
    }
    int head = 0, tail = 0;
    for (int k = 0; k < s.N; ++k) {
        if (!s.alive[k]) continue;
        const int idx = s.y[k] * W + s.x[k];
        if (gDst[idx] < 0) {
            gDst[idx] = 0;
            gOwn[idx] = k;
            gQx[tail] = s.x[k];
            gQy[tail] = s.y[k];
            ++tail;
        }
    }
    while (head < tail) {
        const int x = gQx[head], y = gQy[head];
        ++head;
        const int nd = gDst[y * W + x] + 1;
        const int o = gOwn[y * W + x];
        for (int d = 0; d < 4; ++d) {
            const int nx = x + tron::DX[d], ny = y + tron::DY[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            const int ni = ny * W + nx;
            if (gDst[ni] >= 0) continue;
            if (s.cell[ny][nx] != tron::EMPTY) continue;
            gDst[ni] = nd;
            gOwn[ni] = o;
            gQx[tail] = nx;
            gQy[tail] = ny;
            ++tail;
        }
    }
    for (int k = 0; k < MAXN; ++k) gArea[k] = 0;
    for (int i = 0; i < n; ++i)
        if (gOwn[i] >= 0) ++gArea[gOwn[i]];
}

// 评估：我和每个对手的势力范围差（比只看自己连通块更贴近真实博弈）
inline double evaluate(const State& s) {
    if (!s.alive[gMe]) return DEAD;
    if (checkTime()) return DEAD;
    computeAreas(s);
    if (s.over) return WIN + gArea[gMe];
    const double denom = s.N > 1 ? double(s.N - 1) : 1.0;
    double sum = 0;
    for (int k = 0; k < s.N; ++k)
        if (k != gMe) sum += gArea[k];
    return double(gArea[gMe]) - sum / denom;
}

// 从 (sx, sy) 出发能到达的空地数（不含起点）
int floodArea(const State& s, int sx, int sy) {
    if (checkTime()) return -1;
    if (!tron::inside(s, sx, sy) || s.cell[sy][sx] != tron::EMPTY) return -1;
    ++gStampV;
    const int W = s.W;
    int head = 0, tail = 0;
    gStamp[sy * W + sx] = gStampV;
    gQx[tail] = sx;
    gQy[tail] = sy;
    ++tail;
    int cnt = 0;
    while (head < tail) {
        const int x = gQx[head], y = gQy[head];
        ++head;
        for (int d = 0; d < 4; ++d) {
            const int nx = x + tron::DX[d], ny = y + tron::DY[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= s.H) continue;
            const int ni = ny * W + nx;
            if (gStamp[ni] == gStampV) continue;
            if (s.cell[ny][nx] != tron::EMPTY) continue;
            gStamp[ni] = gStampV;
            gQx[tail] = nx;
            gQy[tail] = ny;
            ++tail;
            ++cnt;
        }
    }
    return cnt;
}

// 方向生成：第 0 回合可以掉头，之后不能走进上一步的格子；能直走就先直走，再看 U R D L
int genMoves(const State& s, int k, int* out) {
    const int first = (s.turn == 0) ? s.last_dir[k] : s.last_dir[k];
    const int rev = (s.turn == 0) ? -1 : tron::opposite(s.last_dir[k]);
    int n = 0;
    if (first != rev && tron::can_move(s, k, first)) out[n++] = first;
    for (int d = 0; d < 4; ++d) {
        if (d == rev || d == first) continue;
        if (tron::can_move(s, k, d)) out[n++] = d;
    }
    return n;
}

// 这一格会不会被"抢格优先级比我高"的活人抢走？会的话走过去就是送死（除非实在没别的路）。
inline bool canBeContested(const State& s, int k, int d) {
    const int tx = s.x[k] + tron::DX[d];
    const int ty = s.y[k] + tron::DY[d];
    const int pk = tron::priority(s, k);
    for (int j = 0; j < s.N; ++j) {
        if (j == k || !s.alive[j]) continue;
        if (tron::priority(s, j) > pk) continue;  // 优先级比我低的抢不走
        const int rev = (s.turn == 0) ? -1 : tron::opposite(s.last_dir[j]);
        for (int d2 = 0; d2 < 4; ++d2) {
            if (d2 == rev) continue;
            if (s.x[j] + tron::DX[d2] != tx || s.y[j] + tron::DY[d2] != ty) continue;
            if (tron::can_move(s, j, d2)) return true;
        }
    }
    return false;
}

// 对手预测：挑走过去之后自己地盘最大的方向（和 greedy 一样的策略）
int predictMove(const State& s, int k) {
    int mv[4];
    const int n = genMoves(s, k, mv);
    if (n == 0) return 9;  // 非法值 → step 里按"沿上一回合方向"，通常撞死
    int best = mv[0], bestS = -2;
    for (int i = 0; i < n; ++i) {
        const int d = mv[i];
        const int sc = floodArea(s, s.x[k] + tron::DX[d], s.y[k] + tron::DY[d]);
        if (sc > bestS) {
            bestS = sc;
            best = d;
        }
    }
    return best;
}

// ---------------- 2 人局：精确最大-最小 ----------------
double searchMax(State& s, int depth, double alpha, double beta);

double searchMin(State& s, int myMove, int depth, double alpha, double beta) {
    const int opp = 1 - gMe;
    int mv[4];
    const int n = genMoves(s, opp, mv);
    if (n == 0) {  // 对手没路可走：他必死
        State t = s;
        int m[MAXN] = {0};
        m[gMe] = myMove;
        m[opp] = 9;
        tron::step(t, m);
        return evaluate(t);
    }
    double best = INF;
    for (int i = 0; i < n; ++i) {
        State t = s;
        int m[MAXN] = {0};
        m[gMe] = myMove;
        m[opp] = mv[i];
        tron::step(t, m);
        double v;
        if (t.over || depth <= 1) {
            v = evaluate(t);
        } else {
            v = searchMax(t, depth - 1, alpha, best < INF ? std::min(beta, best) : beta);
        }
        if (v < best) best = v;
        if (best <= alpha) break;
        if (checkTime()) break;
    }
    return best;
}

double searchMax(State& s, int depth, double alpha, double beta) {
    int mv[4];
    const int n = genMoves(s, gMe, mv);
    if (n == 0) return DEAD + 1000.0;  // 我这一回合必死
    if (depth <= 0) return evaluate(s);
    double best = -INF;
    for (int i = 0; i < n; ++i) {
        const double v = searchMin(s, mv[i], depth, best > -INF ? std::max(alpha, best) : alpha, beta);
        if (v > best) best = v;
        if (best >= beta) break;
        if (checkTime()) break;
    }
    return best;
}

int rootSearch2(const State& s, const int* mv, int n) {
    int bestMove = mv[0];
    double bestVal = -INF;
    for (int depth = 2; depth <= 30; ++depth) {
        double alpha = -INF;
        int bm = mv[0];
        double bv = -INF;
        for (int i = 0; i < n; ++i) {
            const double v = searchMin(const_cast<State&>(s), mv[i], depth, alpha, INF);
            if (gAbort) break;
            if (v > bv) {
                bv = v;
                bm = mv[i];
            }
            if (bv > alpha) alpha = bv;
        }
        if (gAbort) break;
        bestMove = bm;
        bestVal = bv;
        if (bv > WIN * 0.5 || bv < DEAD * 0.5) break;  // 已经证明输/赢，不用再深
    }
    (void)bestVal;
    return bestMove;
}

// ---------------- 3 人以上：我分支、对手贪心预测 ----------------
// 过滤掉"走过去会被优先级更高的人抢走"的方向；全都不安全时才退而求其次
int safeMoves(const State& s, const int* mv, int n, int* out) {
    if (s.N >= 8) {  // 12 人局里几乎总有人"能"来抢，硬过滤会把好方向全禁掉
        for (int i = 0; i < n; ++i) out[i] = mv[i];
        return n;
    }
    int cnt = 0;
    for (int i = 0; i < n; ++i)
        if (!canBeContested(s, gMe, mv[i])) out[cnt++] = mv[i];
    if (cnt > 0) return cnt;
    for (int i = 0; i < n; ++i) out[i] = mv[i];
    return n;
}

double multiSearch(State& s, int depth) {
    int raw[4];
    const int rn = genMoves(s, gMe, raw);
    if (rn == 0) return DEAD + 1000.0;
    if (depth <= 0) return evaluate(s);
    int mv[4];
    const int n = safeMoves(s, raw, rn, mv);
    double best = -INF;
    for (int i = 0; i < n; ++i) {
        State t = s;
        int m[MAXN] = {0};
        m[gMe] = mv[i];
        for (int k = 0; k < s.N; ++k)
            if (k != gMe && s.alive[k]) m[k] = predictMove(s, k);
        tron::step(t, m);
        const double v = multiSearch(t, depth - 1);
        if (v > best) best = v;
        if (checkTime()) break;
    }
    return best;
}

int rootSearchMulti(const State& s, const int* mvIn, int nIn) {
    int mv[4];
    const int n = safeMoves(s, mvIn, nIn, mv);
    int bestMove = mv[0];
    for (int depth = 1; depth <= 24; ++depth) {
        double bv = -INF;
        int bm = mv[0];
        for (int i = 0; i < n; ++i) {
            State t = s;
            int m[MAXN] = {0};
            m[gMe] = mv[i];
            for (int k = 0; k < s.N; ++k)
                if (k != gMe && s.alive[k]) m[k] = predictMove(s, k);
            tron::step(t, m);
            const double v = (depth <= 1) ? evaluate(t) : multiSearch(t, depth - 1);
            if (gAbort) break;
            if (v > bv) {
                bv = v;
                bm = mv[i];
            }
        }
        if (gAbort) break;
        bestMove = bm;
    }
    return bestMove;
}

}  // namespace

int decide(const tron::State& s) {
    gMe = s.me;
    gNodes = 0;
    gAbort = false;
    gT0 = std::chrono::steady_clock::now();
    gWallLimit = (s.turn == 0) ? 150.0 : 0.0;  // 第 0 回合不必想太久
    gHardWall = (s.turn == 0) ? 1500.0 : 350.0;
    if (const char* e = std::getenv("TRON_MS")) {
        const double v = std::atof(e);
        if (v > 0) {
            gWallLimit = v;
            gHardWall = v + 50.0;
        }
    }

    int mv[4];
    const int n = genMoves(s, gMe, mv);
    if (n <= 0) return (s.turn == 0) ? tron::U : s.last_dir[gMe];
    if (n == 1) return mv[0];
    return (s.N == 2) ? rootSearch2(s, mv, n) : rootSearchMulti(s, mv, n);
}
