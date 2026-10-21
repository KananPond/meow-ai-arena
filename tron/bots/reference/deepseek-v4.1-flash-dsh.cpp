// Tron bot: 空间/Voronoi 评估 + 联合回合搜索。
//   1v1:   迭代加深的联合动作 alpha-beta（我走一步、对手应一步，精确结算抢格）。
//   3~12:  用贪心空间模型预测每个对手的走法，再做多回合前瞻；对"高优先级对手贴脸抢格"
//          的风险做扣分。
// 评估：BFS 算每格最近归属（严格更近才算领地）与各人可达连通块大小。
#include "tron.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using namespace tron;

constexpr int CELLS = MAXH * MAXW;
constexpr double SPACE_W = 0.35;      // 可达块大小在评估里的权重
constexpr double ROLL_W = 0.45;       // 长线推演结果的权重（多人局）
constexpr double ROLL1_W = 0.25;      // 长线推演结果的权重（1v1）
constexpr double TIME_RESERVE = 8.0;  // 每回合留出的 CPU 毫秒余量

// ===========================================================================
// 评估
// ===========================================================================
// 评估用临时空间。索引统一用 y*MAXW+x（= State::cell 的行宽），与 W 无关。
struct EvalScratch {
    int8_t board[CELLS];
    int dist[CELLS];
    int8_t own[CELLS];
    int16_t comp[CELLS];
    int compSize[CELLS];
    int16_t qx[CELLS], qy[CELLS];
};
EvalScratch g_ev;

struct Metrics {
    double q[MAXN];
    int area[MAXN];
    int space[MAXN];
};

// 连通块标记（只标 EMPTY 格子），结果放在 g_ev.comp / compSize
int computeComps(const State& s) {
    EvalScratch& e = g_ev;
    const int W = s.W, H = s.H;
    for (int y = 0; y < H; ++y) {
        std::memcpy(e.board + y * MAXW, s.cell[y], sizeof(int8_t) * size_t(W));
        std::memset(e.comp + y * MAXW, 0xFF, sizeof(int16_t) * size_t(W));
    }
    int nc = 0;
    for (int y0 = 0; y0 < H; ++y0)
        for (int x0 = 0; x0 < W; ++x0) {
            const int i0 = y0 * MAXW + x0;
            if (e.board[i0] != EMPTY || e.comp[i0] >= 0) continue;
            int h = 0, t = 0;
            e.qx[t] = int16_t(x0);
            e.qy[t] = int16_t(y0);
            ++t;
            e.comp[i0] = int16_t(nc);
            while (h < t) {
                const int cx = e.qx[h], cy = e.qy[h];
                ++h;
                for (int d = 0; d < 4; ++d) {
                    const int nx = cx + DX[d], ny = cy + DY[d];
                    if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                    const int ni = ny * MAXW + nx;
                    if (e.board[ni] != EMPTY || e.comp[ni] >= 0) continue;
                    e.comp[ni] = int16_t(nc);
                    e.qx[t] = int16_t(nx);
                    e.qy[t] = int16_t(ny);
                    ++t;
                }
            }
            e.compSize[nc] = t;
            ++nc;
        }
    return nc;
}

inline int cellSpace(const State& s, int x, int y) {
    const int c = g_ev.comp[y * MAXW + x];
    return c >= 0 ? g_ev.compSize[c] : 0;
}

void computeMetrics(const State& s, Metrics& m, bool needSpace) {
    EvalScratch& e = g_ev;
    const int W = s.W, H = s.H, N = s.N;
    for (int y = 0; y < H; ++y) {
        std::memcpy(e.board + y * MAXW, s.cell[y], sizeof(int8_t) * size_t(W));
        std::memset(e.dist + y * MAXW, 0xFF, sizeof(int) * size_t(W));
        std::memset(e.own + y * MAXW, 0xFF, sizeof(int8_t) * size_t(W));
    }
    int head = 0, tail = 0;
    for (int k = 0; k < N; ++k) {
        if (!s.alive[k]) continue;
        const int i = s.y[k] * MAXW + s.x[k];
        e.dist[i] = 0;
        e.own[i] = int8_t(k);
        e.qx[tail] = int16_t(s.x[k]);
        e.qy[tail] = int16_t(s.y[k]);
        ++tail;
    }
    while (head < tail) {
        const int x = e.qx[head], y = e.qy[head];
        ++head;
        const int ci = y * MAXW + x;
        const int nd = e.dist[ci] + 1;
        const int o = e.own[ci];
        for (int d = 0; d < 4; ++d) {
            const int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            const int ni = ny * MAXW + nx;
            if (e.board[ni] != EMPTY) continue;
            if (e.dist[ni] < 0) {
                e.dist[ni] = nd;
                e.own[ni] = int8_t(o);
                e.qx[tail] = int16_t(nx);
                e.qy[tail] = int16_t(ny);
                ++tail;
            } else if (e.dist[ni] == nd && e.own[ni] >= 0 && int(e.own[ni]) != o) {
                e.own[ni] = -2;
            }
        }
    }
    for (int k = 0; k < MAXN; ++k) {
        m.area[k] = 0;
        m.space[k] = 0;
        m.q[k] = 0.0;
    }
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int i = y * MAXW + x;
            const int o = e.own[i];
            if (o >= 0 && e.board[i] == EMPTY) ++m.area[o];
        }
    if (needSpace) {
        computeComps(s);
        for (int k = 0; k < N; ++k) {
            if (!s.alive[k]) continue;
            int best = 0;
            const int cx = s.x[k], cy = s.y[k];
            for (int d = 0; d < 4; ++d) {
                const int nx = cx + DX[d], ny = cy + DY[d];
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                const int ni = ny * MAXW + nx;
                if (e.board[ni] != EMPTY) continue;
                const int c = e.comp[ni];
                if (c >= 0 && e.compSize[c] > best) best = e.compSize[c];
            }
            m.space[k] = best;
        }
    }
    for (int k = 0; k < N; ++k) m.q[k] = double(m.area[k]) + SPACE_W * double(m.space[k]);
}

inline int aliveCount(const State& s) { return alive_count(s); }

// 已死亡时的最终名次分（存活者都排在我前面，同回合死的取平均）
double deadScore(const State& s, int k) {
    const int N = s.N;
    if (!s.alive[k] && s.death_turn[k] < 0) return 0.0;
    int above = 0, ties = 0;
    for (int j = 0; j < N; ++j) {
        if (j == k) continue;
        if (s.alive[j]) ++above;
        else if (s.death_turn[j] > s.death_turn[k]) ++above;
        else if (s.death_turn[j] == s.death_turn[k]) ++ties;
    }
    const double rank0 = double(above) + 0.5 * double(ties);
    return (double(N) - 1.0 - rank0) / double(N - 1);
}

// 存活时的估值：把名次分下限（已死的人都在我下面）与领地占比混合。
double leafScore(const State& s, const Metrics& m) {
    const int me = s.me, N = s.N;
    if (!s.alive[me]) return deadScore(s, me);
    int alive = 0;
    double den = 0.0;
    for (int k = 0; k < N; ++k)
        if (s.alive[k]) {
            ++alive;
            den += m.q[k];
        }
    if (alive <= 1) return 1.0;
    const double lo = double(N - alive) / double(N - 1);
    const double qq = den > 1e-9 ? m.q[me] / den : 1.0 / double(alive);
    double v = lo + (1.0 - lo) * qq;
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    return v;
}

// ===========================================================================
// 搜索上下文
// ===========================================================================
struct SearchCtx {
    int me = 0;
    long long nodes = 0;
    bool stop = false;
    double stopAt = 0.0;  // budget_left() 低于这个值就收手
};
SearchCtx g_sc;
double g_roll[4] = {0, 0, 0, 0};  // 每个方向的长线推演分
int g_dbgDepth = 0;
double g_dbgValue = 0.0;
double g_dbgVals[4] = {0, 0, 0, 0};
int g_dbgCnt = 0;

inline bool timeUp() {
    if ((++g_sc.nodes & 63) == 0 && budget_left() < g_sc.stopAt) g_sc.stop = true;
    return g_sc.stop;
}

// 合法走法（目标格是空地），按"目标格周围空地数"降序排
int orderMoves(const State& s, int k, int out[4]) {
    int cand[4];
    int sc[4];
    int n = 0;
    const int straight = s.last_dir[k];
    for (int d = 0; d < 4; ++d) {
        const int nx = s.x[k] + DX[d], ny = s.y[k] + DY[d];
        if (!is_free(s, nx, ny)) continue;
        int freeN = 0;
        for (int e = 0; e < 4; ++e)
            if (is_free(s, nx + DX[e], ny + DY[e])) ++freeN;
        cand[n] = d;
        sc[n] = freeN * 2 + (d == straight ? 1 : 0);
        ++n;
    }
    for (int i = 1; i < n; ++i) {  // 插入排序（稳定、n<=4）
        const int c = cand[i], v = sc[i];
        int j = i - 1;
        while (j >= 0 && sc[j] < v) {
            cand[j + 1] = cand[j];
            sc[j + 1] = sc[j];
            --j;
        }
        cand[j + 1] = c;
        sc[j + 1] = v;
    }
    for (int i = 0; i < n; ++i) out[i] = cand[i];
    return n;
}

// ===========================================================================
// 1v1：联合动作 minimax（同回合双方各走一步，用 step 精确结算）
// ===========================================================================
double exactLeaf(const State& s) {
    const int me = g_sc.me;
    if (!s.alive[me]) return deadScore(s, me);
    if (aliveCount(s) <= 1) return 1.0;
    Metrics m;
    computeMetrics(s, m, false);
    return leafScore(s, m);
}

double searchMe(const State& s, int rounds, double alpha, double beta);
double rolloutScore(const State& s, int firstMove);

double searchOpp(const State& s, int myMove, int rounds, double alpha, double beta) {
    const int me = g_sc.me;
    const int opp = 1 - me;
    int legal[4];
    const int n = orderMoves(s, opp, legal);
    int moves[MAXN] = {0};
    if (timeUp()) return 0.5;
    if (n == 0) {  // 对手无路可走，本回合必死
        State t = s;
        moves[me] = myMove;
        moves[opp] = s.last_dir[opp];
        step(t, moves);
        return exactLeaf(t);
    }
    double best = 2.0;
    for (int i = 0; i < n; ++i) {
        State t = s;
        moves[me] = myMove;
        moves[opp] = legal[i];
        step(t, moves);
        const double v = t.over ? exactLeaf(t) : searchMe(t, rounds - 1, alpha, std::min(beta, best));
        if (g_sc.stop) return best < 2.0 ? best : 0.5;
        if (v < best) best = v;
        if (best < beta) beta = best;
        if (alpha >= beta) break;
    }
    return best;
}

double searchMe(const State& s, int rounds, double alpha, double beta) {
    const int me = g_sc.me;
    if (!s.alive[me]) return deadScore(s, me);
    if (aliveCount(s) <= 1) return 1.0;
    if (timeUp()) return 0.5;
    if (rounds <= 0) {
        Metrics m;
        computeMetrics(s, m, false);
        return leafScore(s, m);
    }
    int mv[4];
    const int n = orderMoves(s, me, mv);
    if (n == 0) {
        State t = s;
        int moves[MAXN] = {0};
        moves[me] = s.last_dir[me];
        moves[1 - me] = s.last_dir[1 - me];
        step(t, moves);
        return deadScore(t, me);
    }
    double best = -1.0;
    for (int i = 0; i < n; ++i) {
        const double v = searchOpp(s, mv[i], rounds, alpha, beta);
        if (g_sc.stop) return best >= 0.0 ? best : 0.5;
        if (v > best) best = v;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return best;
}

int choose1v1(const State& s) {
    const int me = s.me;
    int mv[4];
    const int n = orderMoves(s, me, mv);
    if (n == 0) return s.last_dir[me];
    for (int d = 0; d < 4; ++d) g_roll[d] = 0.0;
    int bestMove = mv[0];
    int prevBest = mv[0];
    for (int rounds = 1; rounds <= 24; ++rounds) {
        g_sc.stop = false;
        double alpha = -1.0;
        double bestV = -1e9;
        int bm = -1;
        int ord[4];
        int cnt = 0;
        ord[cnt++] = prevBest;
        for (int i = 0; i < n; ++i)
            if (mv[i] != prevBest) ord[cnt++] = mv[i];
        for (int i = 0; i < cnt; ++i) {
            const double v = searchOpp(s, ord[i], rounds, alpha, 2.0);
            if (g_sc.stop) break;
            if (v > bestV) {
                bestV = v;
                bm = ord[i];
            }
            if (bestV > alpha) alpha = bestV;
        }
        if (g_sc.stop || bm < 0) break;
        bestMove = bm;
        prevBest = bm;
        if (bestV >= 0.9999) break;  // 已看到必胜
    }
    return bestMove;
}

// ===========================================================================
// 多人：用贪心空间预测对手，再做多回合前瞻
// ===========================================================================
void predictMoves(const State& s, int pred[MAXN], int me) {
    computeComps(s);
    for (int k = 0; k < s.N; ++k) {
        pred[k] = s.last_dir[k] & 3;
        if (!s.alive[k]) continue;
        const int straight = s.last_dir[k];
        int bestD = -1, bestS = -1;
        for (int i = 0; i < 4; ++i) {
            const int d = (straight + i) & 3;
            const int nx = s.x[k] + DX[d], ny = s.y[k] + DY[d];
            if (!is_free(s, nx, ny)) continue;
            const int sc = cellSpace(s, nx, ny);
            if (sc > bestS) {
                bestS = sc;
                bestD = d;
            }
        }
        if (bestD >= 0) pred[k] = bestD;
    }
}

// 全体按"贪心空间"策略把整局打完，返回我最终的名次分（0..1）。时间不够返回 -1。
double rolloutScore(const State& s, int firstMove) {
    const int me = g_sc.me, N = s.N;
    State t = s;
    int pred[MAXN];
    int moves[MAXN];
    for (int turnCount = 0; turnCount < 260; ++turnCount) {
        if (t.over) break;
        if (!t.alive[me]) return deadScore(t, me);
        if ((turnCount & 15) == 0 && budget_left() < g_sc.stopAt + 2.0) return -1.0;
        predictMoves(t, pred, me);
        for (int k = 0; k < N; ++k) moves[k] = t.alive[k] ? pred[k] : 0;
        if (turnCount == 0) moves[me] = firstMove;
        step(t, moves);
    }
    if (!t.alive[me]) {
        if (N <= 2) {  // 1v1：用"我比对手多活几回合"做细一点的信号
            const int opp = 1 - me;
            const int myD = t.death_turn[me];
            const int opD = t.alive[opp] ? 1000 : t.death_turn[opp];
            return 0.5 + 0.5 * std::tanh(double(myD - opD) / 15.0);
        }
        return deadScore(t, me);
    }
    if (aliveCount(t) <= 1) return 1.0;
    Metrics m;
    computeMetrics(t, m, true);
    return leafScore(t, m);
}

// 高优先级对手贴脸抢我目标格的风险扣分
double riskPenalty(const State& s, int myMove) {
    const int me = s.me;
    const int tx = s.x[me] + DX[myMove], ty = s.y[me] + DY[myMove];
    if (!is_free(s, tx, ty)) return 0.0;
    const int myPrio = priority(s, me);
    int threats = 0;
    for (int k = 0; k < s.N; ++k) {
        if (k == me || !s.alive[k]) continue;
        if (priority(s, k) >= myPrio) continue;
        const int dx = s.x[k] - tx, dy = s.y[k] - ty;
        if ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) == 1) ++threats;
    }
    if (threats == 0) return 0.0;
    // 有先手优势的对手正好贴着我的目标格：被抢就必死，按人数给不同权重的扣分。
    const double base = 0.30;
    return base + 0.08 * double(threats - 1);
}

double multiValue(const State& s, int depth, int* rootBest) {
    const int me = g_sc.me;
    if (!s.alive[me]) return deadScore(s, me);
    if (aliveCount(s) <= 1) return 1.0;
    if (timeUp()) return 0.5;
    int mv[4];
    const int n = orderMoves(s, me, mv);
    int pred[MAXN];
    if (n == 0) {
        State t = s;
        int moves[MAXN];
        predictMoves(s, pred, me);
        for (int k = 0; k < s.N; ++k) moves[k] = s.alive[k] ? (k == me ? (s.last_dir[me] & 3) : pred[k]) : 0;
        step(t, moves);
        return deadScore(t, me);
    }
    if (depth <= 0) {
        Metrics m;
        computeMetrics(s, m, true);
        return leafScore(s, m);
    }
    predictMoves(s, pred, me);
    double best = -1.0;
    int bm = mv[0];
    const int N = s.N;
    for (int i = 0; i < n; ++i) {
        State t = s;
        int moves[MAXN];
        for (int k = 0; k < N; ++k) moves[k] = s.alive[k] ? (k == me ? mv[i] : pred[k]) : 0;
        step(t, moves);
        double v = multiValue(t, depth - 1, nullptr);
        if (g_sc.stop) break;
        v -= riskPenalty(s, mv[i]);
        if (rootBest) v += ROLL_W * g_roll[mv[i]];
        if (v > best) {
            best = v;
            bm = mv[i];
        }
    }
    if (rootBest && !g_sc.stop && best > -0.5) *rootBest = bm;
    return best;
}

int chooseMulti(const State& s) {
    const int me = s.me;
    int mv[4];
    const int n = orderMoves(s, me, mv);
    if (n == 0) return s.last_dir[me];
    for (int d = 0; d < 4; ++d) g_roll[d] = 0.0;
    // 12 人局人太多、短期搜索看不到被围死的结局，用"全体贪心"把整局推演一遍。
    if (s.N >= 5 && budget_left() > g_sc.stopAt + 16.0) {
        for (int i = 0; i < n; ++i) {
            const double r = rolloutScore(s, mv[i]);
            if (r < 0.0) return mv[0];  // 时间不够，走排序第一
            g_roll[mv[i]] = r;
        }
    }
    int best = mv[0];
    for (int depth = 1; depth <= 8; ++depth) {
        g_sc.stop = false;
        int bm = -1;
        multiValue(s, depth, &bm);
        if (g_sc.stop || bm < 0) break;
        best = bm;
    }
    return best;
}

}  // namespace

// ===========================================================================
int decide(const tron::State& s) {
    if (s.N < 2 || s.W <= 0 || s.over) return tron::U;
    g_sc.me = s.me;
    g_sc.nodes = 0;
    g_sc.stop = false;
    const double left0 = budget_left();
    // 每回合计划花掉的时间（CPU 毫秒）：首回合预算很大，也只花 110ms；常规回合花 40ms。
    double spend = left0 - TIME_RESERVE;
    if (spend > 110.0) spend = 110.0;
    if (spend < 2.0) spend = 2.0;
    g_sc.stopAt = left0 - spend;
    if (s.N == 2) return choose1v1(s);
    return chooseMulti(s);
}
