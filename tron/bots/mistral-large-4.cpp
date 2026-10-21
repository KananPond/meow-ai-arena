// 光轮对战 bot
//
// 思路：
//   1v1（占正式分 50%）：迭代加深 alpha-beta 极小化极大搜索，每回合用 tron::step 精确
//        结算（同时移动、抢格优先级、对撞与真规则一致）。叶子估值用 Voronoi：从双方头部
//        多源 BFS，每个空格归属先到达的人（并列平分）——衡量谁实际控制多少棋盘；被困死
//        的人份额为 0，估值自然就是 0/1。浅层用 flood 面积排序，深层用廉价启发式排序。
//   3/4/12 人：安全过滤（躲开高优先级对手能同时走进的格子）+ 逐招推演估值。
//        对手用 Voronoi-greedy 模型（走一步后自己的棋盘份额最大）；我落子后所有人（含我）
//        按 flood-greedy 走 rollout。估值 = 预期排名比我低的人数（已死者精确按死亡回合，
//        存活者按 Voronoi 份额估计谁活得更久）。
#include "tron.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <climits>

namespace {

using tron::State;

// ---------------- flood fill ----------------
uint32_t g_seen[tron::MAXH][tron::MAXW];
uint32_t g_stamp = 0;

// 从 (sx,sy) 出发（该格可以是头部）能到达的空格数。用 stamp 避免每次清棋盘。
inline int flood(const State& s, int sx, int sy) {
    if (!tron::inside(s, sx, sy)) return 0;
    const uint32_t st = ++g_stamp;
    int qx[tron::MAXH * tron::MAXW], qy[tron::MAXH * tron::MAXW];
    int head = 0, tail = 0, count = 0;
    for (int d = 0; d < 4; ++d) {
        const int nx = sx + tron::DX[d], ny = sy + tron::DY[d];
        if (tron::is_free(s, nx, ny) && g_seen[ny][nx] != st) {
            g_seen[ny][nx] = st;
            qx[tail] = nx; qy[tail] = ny; ++tail; ++count;
        }
    }
    while (head < tail) {
        const int x = qx[head], y = qy[head]; ++head;
        for (int d = 0; d < 4; ++d) {
            const int nx = x + tron::DX[d], ny = y + tron::DY[d];
            if (tron::is_free(s, nx, ny) && g_seen[ny][nx] != st) {
                g_seen[ny][nx] = st;
                qx[tail] = nx; qy[tail] = ny; ++tail; ++count;
            }
        }
    }
    return count;
}

// ---------------- Voronoi（多源 BFS） ----------------
// 每个活人头部是一个源，沿空地 BFS 得各人到每格的最短距离；每个空格归属距离最小的
// 人（并列平分）。share[k] = k 的份额。被墙围死的人份额为 0。
struct VQ { int x, y, k, d; };
VQ g_vq[tron::MAXN * tron::MAXH * tron::MAXW];
uint32_t g_vseen[tron::MAXN][tron::MAXH][tron::MAXW];
int g_vdist[tron::MAXN][tron::MAXH][tron::MAXW];
uint32_t g_vstamp = 0;

void voronoi_shares(const State& s, double share[tron::MAXN]) {
    const int N = s.N;
    const uint32_t st = ++g_vstamp;
    for (int k = 0; k < N; ++k) share[k] = 0.0;
    int head = 0, tail = 0;
    for (int k = 0; k < N; ++k) {
        if (!s.alive[k]) continue;
        for (int d = 0; d < 4; ++d) {
            const int nx = s.x[k] + tron::DX[d], ny = s.y[k] + tron::DY[d];
            if (tron::is_free(s, nx, ny) && g_vseen[k][ny][nx] != st) {
                g_vseen[k][ny][nx] = st;
                g_vdist[k][ny][nx] = 1;
                g_vq[tail++] = {nx, ny, k, 1};
            }
        }
    }
    while (head < tail) {
        const VQ cur = g_vq[head++];
        for (int d = 0; d < 4; ++d) {
            const int nx = cur.x + tron::DX[d], ny = cur.y + tron::DY[d];
            if (tron::is_free(s, nx, ny) && g_vseen[cur.k][ny][nx] != st) {
                g_vseen[cur.k][ny][nx] = st;
                g_vdist[cur.k][ny][nx] = cur.d + 1;
                g_vq[tail++] = {nx, ny, cur.k, cur.d + 1};
            }
        }
    }
    for (int y = 0; y < s.H; ++y) {
        for (int x = 0; x < s.W; ++x) {
            if (s.cell[y][x] != tron::EMPTY) continue;
            int best = INT_MAX;
            for (int k = 0; k < N; ++k) {
                if (!s.alive[k]) continue;
                if (g_vseen[k][y][x] == st && g_vdist[k][y][x] < best) best = g_vdist[k][y][x];
            }
            if (best == INT_MAX) continue;    // 谁都到不了的格子不计
            int cnt = 0;
            for (int k = 0; k < N; ++k) {
                if (!s.alive[k]) continue;
                if (g_vseen[k][y][x] == st && g_vdist[k][y][x] == best) ++cnt;
            }
            const double w = 1.0 / cnt;
            for (int k = 0; k < N; ++k) {
                if (!s.alive[k]) continue;
                if (g_vseen[k][y][x] == st && g_vdist[k][y][x] == best) share[k] += w;
            }
        }
    }
}

inline int legal_moves(const State& s, int k, int out[4]) {
    int n = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, k, d)) out[n++] = d;
    return n;
}

// greedy.cpp 同款走法：可达空格最多；并列先直行，再按 U,R,D,L。
inline int greedy_move(const State& s, int k) {
    const int straight = s.last_dir[k];
    int order[4] = {straight, 0, 0, 0}, n = 1;
    for (int d = 0; d < 4; ++d)
        if (d != straight) order[n++] = d;
    int best = -1, best_score = -1;
    for (int i = 0; i < n; ++i) {
        const int d = order[i];
        if (!tron::can_move(s, k, d)) continue;
        const int sc = flood(s, s.x[k] + tron::DX[d], s.y[k] + tron::DY[d]);
        if (sc > best_score) { best_score = sc; best = d; }
    }
    return best;    // -1 = 无路可走（走 last_dir 必死）
}

// Voronoi-greedy：走一步后自己的棋盘份额最大；并列先直行，再按 U,R,D,L。
inline int voronoi_move(const State& s, int k) {
    const int straight = s.last_dir[k];
    int order[4] = {straight, 0, 0, 0}, n = 1;
    for (int d = 0; d < 4; ++d)
        if (d != straight) order[n++] = d;
    int best = -1;
    double best_score = -1.0;
    for (int i = 0; i < n; ++i) {
        const int d = order[i];
        if (!tron::can_move(s, k, d)) continue;
        State c = s;
        c.x[k] += tron::DX[d];
        c.y[k] += tron::DY[d];
        c.cell[c.y[k]][c.x[k]] = int8_t(k);
        c.last_dir[k] = d;
        double share[tron::MAXN];
        voronoi_shares(c, share);
        if (share[k] > best_score) { best_score = share[k]; best = d; }
    }
    return best;
}

// 按"走一步后的可达空格数"降序排列
inline void order_by_area(const State& s, int k, int* mv, int* score, int n) {
    for (int i = 0; i < n; ++i)
        score[i] = flood(s, s.x[k] + tron::DX[mv[i]], s.y[k] + tron::DY[mv[i]]);
    for (int i = 1; i < n; ++i) {
        const int d = mv[i], sc = score[i];
        int j = i - 1;
        while (j >= 0 && score[j] < sc) { mv[j + 1] = mv[j]; score[j + 1] = score[j]; --j; }
        mv[j + 1] = d; score[j + 1] = sc;
    }
}

// 排序：浅层用 flood 面积（准确），深层用廉价启发式（目标格自由邻居数 + 直行偏好）
inline void order_moves(const State& s, int k, int* mv, int* score, int n, bool use_flood) {
    if (use_flood) {
        order_by_area(s, k, mv, score, n);
        return;
    }
    const int straight = s.last_dir[k];
    for (int i = 0; i < n; ++i) {
        const int d = mv[i];
        const int nx = s.x[k] + tron::DX[d], ny = s.y[k] + tron::DY[d];
        int sc = (d == straight) ? 64 : 0;
        for (int dd = 0; dd < 4; ++dd)
            if (tron::is_free(s, nx + tron::DX[dd], ny + tron::DY[dd])) sc += 8;
        score[i] = sc;
    }
    for (int i = 1; i < n; ++i) {
        const int d = mv[i], sc = score[i];
        int j = i - 1;
        while (j >= 0 && score[j] < sc) { mv[j + 1] = mv[j]; score[j + 1] = score[j]; --j; }
        mv[j + 1] = d; score[j + 1] = sc;
    }
}

inline double sigmoid(double x) { return 1.0 / (1.0 + std::exp(-x)); }

// 固定搜索量开关（比较版本时用；默认按 CPU 预算走）
long long g_node_cap = -1;    // 1v1 节点上限，-1 = 按时间
long long g_roll_cap = -1;    // 多人 rollout 轮数上限，-1 = 按时间
int g_debug = 0;

// wall clock guard: stop searching past a wall-clock cap (formal: 2s per turn, 4s at turn 0)
std::chrono::steady_clock::time_point g_t0;
double g_wall_budget = 1e18;
inline bool wall_over() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g_t0).count() >
           g_wall_budget;
}

// ================= 1v1：alpha-beta 搜索 =================
int g_me = 0;
long long g_nodes = 0;
bool g_abort = false;
constexpr int ORDER_FLOOD_DEPTH = 4;    // 深度 >= 此值用 flood 排序，否则用廉价排序

// 叶子估值：从我的视角，[0,1]。Voronoi 份额天然处理各种终局：
// 我被困死 share=0 → 0；对手被困死 → 1；同死 0/0 → 0.5。
inline double eval_leaf(const State& s) {
    double share[tron::MAXN];
    voronoi_shares(s, share);
    const double denom = share[g_me] + share[1 - g_me];
    return denom > 1e-9 ? share[g_me] / denom : 0.5;
}

double my_node(const State& s, int depth, double alpha, double beta);

// 对手回合：固定我的招 my_move，对手任选一招（极小化我的估值）。beta 是本层的局部量。
double opp_node(const State& s, int my_move, int depth, double alpha, double beta) {
    const int me = g_me, opp = 1 - me;
    int om[4], osc[4];
    const int no = legal_moves(s, opp, om);
    if (no == 0) return 1.0;    // 对手无路可走：本回合必死，我必胜
    order_moves(s, opp, om, osc, no, depth >= ORDER_FLOOD_DEPTH);
    double worst = 1e18;
    for (int j = 0; j < no; ++j) {
        State c = s;
        int mv[2];
        mv[me] = my_move;
        mv[opp] = om[j];
        tron::step(c, mv);
        const double v = my_node(c, depth - 1, alpha, beta);
        if (v < worst) worst = v;
        if (worst < beta) beta = worst;
        if (beta <= alpha) break;
    }
    return worst;
}

// 我的回合节点：depth 还剩几回合。返回我的估值。
double my_node(const State& s, int depth, double alpha, double beta) {
    if (g_abort) return 0.0;
    if ((++g_nodes & 127) == 0) {
        if (g_node_cap >= 0 ? g_nodes > g_node_cap : tron::budget_left() < 10 || wall_over()) {
            g_abort = true;
            return 0.0;
        }
    }
    const int me = g_me, opp = 1 - me;
    if (s.over) {
        if (!s.alive[me]) return s.alive[opp] ? 0.0 : 0.5;
        return 1.0;
    }
    if (depth <= 0) return eval_leaf(s);
    int mm[4], msc[4];
    const int nm = legal_moves(s, me, mm);
    if (nm == 0) {    // 我无路可走：本回合必死
        int om[4];
        return legal_moves(s, opp, om) == 0 ? 0.5 : 0.0;
    }
    order_moves(s, me, mm, msc, nm, depth >= ORDER_FLOOD_DEPTH);
    double best = -1e18;
    for (int i = 0; i < nm; ++i) {
        const double v = opp_node(s, mm[i], depth, alpha, beta);
        if (v > best) best = v;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return best;
}

int decide_1v1(const State& s) {
    g_me = s.me;
    g_nodes = 0;
    const int me = s.me, opp = 1 - s.me;
    int mm[4], msc[4];
    const int nm = legal_moves(s, me, mm);
    if (nm == 0) return s.last_dir[me];
    order_by_area(s, me, mm, msc, nm);
    {
        int om[4];
        if (legal_moves(s, opp, om) == 0) return mm[0];    // 对手本回合必死
    }
    int best_move = mm[0];
    int best_depth = 0;
    double best_val = -1e18;
    const int MAXD = 24;
    for (int depth = 1; depth <= MAXD; ++depth) {
        g_abort = false;
        double alpha = -1e18;
        int dm = mm[0];
        double dv = -1e18;
        for (int i = 0; i < nm; ++i) {
            if (g_abort) break;
            const double v = opp_node(s, mm[i], depth, alpha, 1e18);
            if (v > dv) { dv = v; dm = mm[i]; }
            if (dv > alpha) alpha = dv;
        }
        if (!g_abort) {
            best_move = dm;
            best_depth = depth;
            best_val = dv;
            if (dv >= 0.999 || dv <= 0.001) break;    // 胜负已定
        }
        if (g_node_cap >= 0 ? g_nodes > g_node_cap : tron::budget_left() < 12 || wall_over()) break;
    }
    if (g_debug)
        std::fprintf(stderr, "[1v1] turn %d me %d depth %d val %.3f nodes %lld move %c\n", s.turn, me,
                     best_depth, best_val, g_nodes, tron::DIR_CHAR[best_move]);
    return best_move;
}

// ================= 多人（3/4/12）：推演 + rollout 估值 =================

// 估值：预期"排名比我低"的对手人数（越大越好）。
// - 已死的对手：按死亡回合精确比较。
// - 都活着：P(我活得比 j 久) = sigmoid((share_j - my_share) / 40)，share 为 Voronoi 份额。
double score_state(const State& c, int me) {
    const int N = c.N;
    double share[tron::MAXN];
    voronoi_shares(c, share);
    const bool i_alive = c.alive[me];
    const int my_death = i_alive ? (1 << 29) : c.death_turn[me];
    double sc = 0.0;
    for (int j = 0; j < N; ++j) {
        if (j == me) continue;
        if (!c.alive[j]) {
            const int dj = c.death_turn[j];
            if (!i_alive) {
                if (dj < my_death) sc += 1.0;
                else if (dj == my_death) sc += 0.5;
            } else {
                sc += 1.0;    // j 死在我前（我还活着），必然排名比我低
            }
        } else {
            if (!i_alive) continue;    // 我死了，活着的对手都排我前面
            sc += sigmoid((share[j] - share[me]) / 40.0);
        }
    }
    if (i_alive) sc += 0.001 * share[me];    // 微小的绝对空间偏好，用于并列时的稳定选择
    return sc;
}

// 从 c 出发所有人（含我）按 flood-greedy 走 rollout，返回估值。
double rollout_eval(State& c, int me, int max_rounds, double allowance) {
    const int N = c.N;
    const double t0 = tron::budget_used();
    int r = 0;
    while (r < max_rounds && tron::alive_count(c) > 1 && c.alive[me]) {
        if (g_roll_cap >= 0 ? r >= g_roll_cap : tron::budget_used() - t0 > allowance || wall_over()) break;
        int mv[tron::MAXN];
        for (int k = 0; k < N; ++k)
            mv[k] = c.alive[k] ? greedy_move(c, k) : 0;
        tron::step(c, mv);
        ++r;
    }
    return score_state(c, me);
}

int decide_multi(const State& s) {
    const int me = s.me, N = s.N;
    int mm[4];
    const int nm = legal_moves(s, me, mm);
    if (nm == 0) return s.last_dir[me];

    // 对手模型：Voronoi-greedy（本回合同时决策，基于当前棋盘）
    int model[tron::MAXN];
    for (int k = 0; k < N; ++k)
        model[k] = (k == me || !s.alive[k]) ? 0 : voronoi_move(s, k);

    // 安全过滤：若某高优先级对手能一步走进我想到的格子，则该格危险（被抢格必死）。
    const int my_prio = tron::priority(s, me);
    bool safe[4];
    bool any_safe = false;
    for (int i = 0; i < nm; ++i) {
        const int cx = s.x[me] + tron::DX[mm[i]], cy = s.y[me] + tron::DY[mm[i]];
        safe[i] = true;
        for (int j = 0; j < N; ++j) {
            if (j == me || !s.alive[j]) continue;
            if (tron::priority(s, j) >= my_prio) continue;    // 优先级不高于我的，撞死的是他
            if (std::abs(s.x[j] - cx) + std::abs(s.y[j] - cy) == 1) { safe[i] = false; break; }
        }
        if (safe[i]) any_safe = true;
    }
    int cand[4], nc = 0;
    for (int i = 0; i < nm; ++i)
        if (!any_safe || safe[i]) cand[nc++] = mm[i];

    // 逐招推演：我走 cand[i]，对手走模型招，结算一回合后全员 greedy rollout
    const int max_rounds = (N >= 12) ? 100 : 200;
    int best_move = cand[0];
    double best_score = -1e18;
    for (int i = 0; i < nc; ++i) {
        // 每个候选招平分剩余时间（留 12ms 余量）
        const double allowance = std::max(1.0, (tron::budget_left() - 12.0) / (nc - i));
        State c = s;
        int mv[tron::MAXN];
        for (int k = 0; k < N; ++k) mv[k] = (k == me) ? cand[i] : model[k];
        tron::step(c, mv);
        const double sc = rollout_eval(c, me, max_rounds, allowance);
        if (g_debug)
            std::fprintf(stderr, "[multi] turn %d N %d cand %c score %.3f\n", s.turn, N,
                         tron::DIR_CHAR[cand[i]], sc);
        if (sc > best_score) { best_score = sc; best_move = cand[i]; }
    }
    return best_move;
}

}  // namespace

int decide(const tron::State& s) {
    if (const char* e = std::getenv("TRON_FIXED_NODES")) {
        g_node_cap = g_roll_cap = std::atoll(e);
    }
    if (const char* e = std::getenv("TRON_DEBUG")) {
        g_debug = std::atoi(e);
    }
    g_t0 = std::chrono::steady_clock::now();
    g_wall_budget = (s.turn == 0 ? 3000.0 : 1500.0);
    if (tron::alive_count(s) <= 1) return s.last_dir[s.me];
    if (s.N == 2) return decide_1v1(s);
    return decide_multi(s);
}
