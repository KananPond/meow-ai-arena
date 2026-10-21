// 强光轮（Tron） bot。
//
// 策略：
//   - 1v1（2 人）：alpha-beta 极大极小（PVS + 迭代加深 + 上一层的走法排序）搜索，
//     评估 = 自己可达空格数 - 对手可达空格数。对手按"最坏情况"处理。
//   - 3～4 人：单步期望值搜索，对手走法按 softmax 概率模型组合；结局分 =
//     我的面积 - 最大对手面积，对手阵亡给奖励，自己阵亡重罚。
//   - 12 人：两层前瞻，各对手用贪心（最大化自己面积）预测，同上的结局分。
#include "tron.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

using tron::State;

constexpr double BIG = 1e7;
constexpr int MAX_DEPTH_1V1 = 12;
constexpr double TIME_MARGIN = 6.0;
constexpr double SOFTMAX_T = 12.0;
constexpr double KILL_BONUS = 1000.0;
constexpr int OPP_MOVE_LIMIT = 2;   // 对手节点最多考虑面积最大的几个走法
constexpr int HIST_W = 3;            // 历史启发权重（排序用）

bool g_abort = false;
int g_nodes = 0;
int hist[2][4];                      // 历史启发：剪枝产生方加成

inline void checkBudget() {
    if ((++g_nodes & 0xFF) == 0 && tron::budget_left() < TIME_MARGIN) g_abort = true;
}

// ------------------------------------------------------------------ flood fill
static int areaFrom(const State& s, int sx, int sy) {
    static bool seen[tron::MAXH][tron::MAXW];
    static int qx[tron::MAXH * tron::MAXW], qy[tron::MAXH * tron::MAXW];
    for (int y = 0; y < s.H; ++y) std::memset(seen[y], 0, s.W);
    int head = 0, tail = 0, cnt = 0;
    qx[tail] = sx; qy[tail] = sy; ++tail;
    seen[sy][sx] = true;
    while (head < tail) {
        const int x = qx[head], y = qy[head];
        ++head;
        for (int d = 0; d < 4; ++d) {
            const int nx = x + tron::DX[d], ny = y + tron::DY[d];
            if (nx >= 0 && nx < s.W && ny >= 0 && ny < s.H && !seen[ny][nx] &&
                s.cell[ny][nx] == tron::EMPTY) {
                seen[ny][nx] = true;
                qx[tail] = nx; qy[tail] = ny;
                ++tail;
                ++cnt;
            }
        }
    }
    return cnt;
}

static void orderMovesByArea(const State& s, int k, int moves[], int n, int scores[]) {
    for (int i = 0; i < n; ++i)
        scores[i] = areaFrom(s, s.x[k] + tron::DX[moves[i]], s.y[k] + tron::DY[moves[i]]) +
                    HIST_W * hist[k][moves[i]];
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (scores[j] > scores[i]) {
                std::swap(moves[i], moves[j]);
                std::swap(scores[i], scores[j]);
            }
}

// ------------------------------------------------------------ 1v1 搜索

inline double evalLeaf1(const State& s) {
    const int me = s.me, opp = 1 - s.me;
    const bool a1 = s.alive[me], a2 = s.alive[opp];
    if (!a1 && !a2) return 0.0;
    if (!a1) return -BIG;
    if (!a2) return BIG;
    return double(areaFrom(s, s.x[me], s.y[me]) - areaFrom(s, s.x[opp], s.y[opp]));
}

double search1(State& s, int depth, double alpha, double beta);

// 对手节点（min）：我们已定 m。
double oppNode1(const State& s, int m, int depth, double alpha, double beta) {
    checkBudget();
    const int me = s.me, opp = 1 - s.me;
    int om[4], nom = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, opp, d)) om[nom++] = d;
    if (nom == 0) {
        State s2 = s;
        int mv[2] = {m, s.last_dir[opp]};
        tron::step(s2, mv);
        return s2.alive[me] ? BIG : 0.0;
    }
    int osc[4];
    orderMovesByArea(s, opp, om, nom, osc);
    if (nom > OPP_MOVE_LIMIT) nom = OPP_MOVE_LIMIT;
    double res = 1e30;
    for (int i = 0; i < nom; ++i) {
        if (g_abort) break;
        State s2 = s;
        int mv[2] = {m, om[i]};
        tron::step(s2, mv);
        double v;
        if (i == 0) {
            v = (depth <= 1) ? evalLeaf1(s2) : search1(s2, depth - 1, alpha, beta);
        } else {
            v = (depth <= 1) ? evalLeaf1(s2) : search1(s2, depth - 1, beta - 1, beta);
            if (v < beta && v > alpha && depth > 1)
                v = search1(s2, depth - 1, alpha, v);
        }
        if (v < res) res = v;
        if (res < beta) beta = res;
        if (alpha >= beta) { hist[opp][om[i]] += 1 << (depth > 8 ? 8 : depth); break; }
    }
    return res;
}

// 我方节点（max）。
double search1(State& s, int depth, double alpha, double beta) {
    checkBudget();
    const int me = s.me, opp = 1 - s.me;
    if (!s.alive[me]) return -BIG;
    if (!s.alive[opp]) return BIG;
    if (depth <= 0) return evalLeaf1(s);
    int mm[4], nmm = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, me, d)) mm[nmm++] = d;
    if (nmm == 0) return -BIG;
    int sc[4];
    orderMovesByArea(s, me, mm, nmm, sc);
    double best = -1e30;
    for (int i = 0; i < nmm; ++i) {
        if (g_abort) break;
        double v;
        if (i == 0) {
            v = oppNode1(s, mm[i], depth, alpha, beta);
        } else {
            v = oppNode1(s, mm[i], depth, alpha, alpha + 1);
            if (v > alpha && v < beta) v = oppNode1(s, mm[i], depth, v, beta);
        }
        if (v > best) best = v;
        if (best > alpha) alpha = best;
        if (alpha >= beta) { hist[me][mm[i]] += 1 << (depth > 8 ? 8 : depth); break; }
    }
    return best;
}

int root1(const State& s0) {
    const State s = s0;
    const int me = s.me;
    int mm[4], nmm = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, me, d)) mm[nmm++] = d;
    if (nmm == 0) return s.last_dir[me];
    if (nmm == 1) return mm[0];
    int sc[4];
    orderMovesByArea(s, me, mm, nmm, sc);

    double mvVal[4];
    int order[4];
    for (int i = 0; i < nmm; ++i) { mvVal[i] = double(sc[i]); order[i] = i; }

    int bestDir = mm[0];
    for (int depth = 1; depth <= MAX_DEPTH_1V1 && tron::budget_left() > TIME_MARGIN; ++depth) {
        g_abort = false;
        // 按上一层价值排序（初始按面积）
        for (int i = 0; i < nmm; ++i)
            for (int j = i + 1; j < nmm; ++j)
                if (mvVal[order[j]] > mvVal[order[i]]) std::swap(order[i], order[j]);

        double alpha = -1e30, beta = 1e30;
        double newVal[4];
        int curDir = -1;
        double curBest = -1e30;
        for (int k = 0; k < nmm; ++k) {
            const int idx = order[k];
            const int m = mm[idx];
            double v;
            if (k == 0) {
                v = oppNode1(s, m, depth, alpha, beta);
            } else {
                v = oppNode1(s, m, depth, alpha, alpha + 1);
                if (v > alpha && v < beta) v = oppNode1(s, m, depth, v, beta);
            }
            newVal[idx] = v;
            if (v > curBest) { curBest = v; curDir = m; }
            if (v > alpha) alpha = v;
        }
        if (g_abort) break;                 // 本层未完成，丢弃
        for (int i = 0; i < nmm; ++i) mvVal[i] = newVal[i];
        if (curDir >= 0) bestDir = curDir;
        if (curBest >= BIG - 1e3) break;    // 必胜
    }
    return bestDir;
}

// ------------------------------------------------------- 多人共用

inline void oppProbs(const State& s, int k, double probs[4]) {
    double gains[4];
    double mx = -1e30;
    for (int d = 0; d < 4; ++d) {
        gains[d] = -1.0;
        if (tron::can_move(s, k, d)) {
            gains[d] = double(areaFrom(s, s.x[k] + tron::DX[d], s.y[k] + tron::DY[d]));
            if (gains[d] > mx) mx = gains[d];
        }
    }
    double sum = 0.0;
    for (int d = 0; d < 4; ++d) {
        if (gains[d] >= 0) probs[d] = std::exp((gains[d] - mx) / SOFTMAX_T);
        else probs[d] = 0.0;
        sum += probs[d];
    }
    if (!(sum > 0.0)) {
        for (int d = 0; d < 4; ++d) probs[d] = 0.0;
        probs[s.last_dir[k]] = 1.0;
        return;
    }
    for (int d = 0; d < 4; ++d) probs[d] /= sum;
}

double expectMultiRec(const State& s, int me, const int opps[], int nopp, int idx, double probSoFar,
                      int mv[]) {
    if (idx == nopp) {
        State s2 = s;
        int mvs[tron::MAXN];
        for (int k = 0; k < s.N; ++k) mvs[k] = s.last_dir[k];
        mvs[me] = mv[me];
        for (int i = 0; i < nopp; ++i) mvs[opps[i]] = mv[opps[i]];
        tron::step(s2, mvs);
        if (!s2.alive[me]) return probSoFar * (-BIG);
        const int myArea = areaFrom(s2, s2.x[me], s2.y[me]);
        int maxOpp = -1, killed = 0;
        for (int i = 0; i < nopp; ++i) {
            const int k = opps[i];
            if (s2.alive[k]) {
                const int a = areaFrom(s2, s2.x[k], s2.y[k]);
                if (a > maxOpp) maxOpp = a;
            } else if (s.alive[k]) {
                ++killed;
            }
        }
        double val = double(myArea) - double(std::max(0, maxOpp));
        if (killed > 0) val += KILL_BONUS * double(killed);
        return probSoFar * val;
    }
    const int k = opps[idx];
    double probs[4];
    oppProbs(s, k, probs);
    int legal[4], nlegal = 0;
    for (int d = 0; d < 4; ++d)
        if (probs[d] > 0.0) legal[nlegal++] = d;
    double acc = 0.0;
    if (nlegal == 0) {
        mv[k] = s.last_dir[k];
        acc = expectMultiRec(s, me, opps, nopp, idx + 1, probSoFar, mv);
    } else {
        for (int i = 0; i < nlegal; ++i) {
            mv[k] = legal[i];
            acc += expectMultiRec(s, me, opps, nopp, idx + 1, probSoFar * probs[legal[i]], mv);
        }
    }
    return acc;
}

int rootMulti(const State& s0) {
    const State s = s0;
    const int me = s.me;
    int opps[tron::MAXN], nopp = 0;
    for (int k = 0; k < s.N; ++k)
        if (k != me && s.alive[k]) opps[nopp++] = k;
    int mm[4], nmm = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, me, d)) mm[nmm++] = d;
    if (nmm == 0) return s.last_dir[me];
    if (nmm == 1) return mm[0];
    int sc[4];
    orderMovesByArea(s, me, mm, nmm, sc);
    double bestVal = -1e30;
    int bestDir = mm[0];
    int mv[tron::MAXN];
    for (int i = 0; i < nmm; ++i) {
        if (tron::budget_left() < TIME_MARGIN) break;
        mv[me] = mm[i];
        const double v = expectMultiRec(s, me, opps, nopp, 0, 1.0, mv);
        if (v > bestVal) { bestVal = v; bestDir = mm[i]; }
    }
    return bestDir;
}

// -------------------------------------------------------------- 12 人

int greedyOppMove(const State& s, int k) {
    const int straight = s.last_dir[k];
    const int order[4] = {straight, (straight + 1) & 3, (straight + 2) & 3, (straight + 3) & 3};
    int best = straight, bestSc = -1;
    for (int i = 0; i < 4; ++i) {
        const int d = order[i];
        if (!tron::can_move(s, k, d)) continue;
        const int sc = areaFrom(s, s.x[k] + tron::DX[d], s.y[k] + tron::DY[d]);
        if (sc > bestSc) { bestSc = sc; best = d; }
    }
    return best;
}

double evalState12(const State& s, const State& before) {
    const int me = s.me;
    if (!s.alive[me]) return -BIG;
    const int myArea = areaFrom(s, s.x[me], s.y[me]);
    int maxOpp = -1, killed = 0;
    for (int k = 0; k < s.N; ++k) {
        if (k == me || !before.alive[k]) continue;
        if (s.alive[k]) {
            const int a = areaFrom(s, s.x[k], s.y[k]);
            if (a > maxOpp) maxOpp = a;
        } else {
            ++killed;
        }
    }
    double val = double(myArea) - double(std::max(0, maxOpp));
    if (killed > 0) val += KILL_BONUS * double(killed);
    return val;
}

int root12(const State& s0) {
    const State s = s0;
    const int me = s.me;
    int mm[4], nmm = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, me, d)) mm[nmm++] = d;
    if (nmm == 0) return s.last_dir[me];
    if (nmm == 1) return mm[0];
    int sc[4];
    orderMovesByArea(s, me, mm, nmm, sc);
    double bestVal = -1e30;
    int bestDir = mm[0];
    for (int i = 0; i < nmm; ++i) {
        if (tron::budget_left() < TIME_MARGIN) break;
        State s2 = s;
        int mvs[tron::MAXN];
        for (int k = 0; k < s.N; ++k)
            mvs[k] = (k == me) ? mm[i] : (s.alive[k] ? greedyOppMove(s, k) : s.last_dir[k]);
        tron::step(s2, mvs);
        double bestNext = -1e30;
        for (int d = 0; d < 4; ++d) {
            if (!tron::can_move(s2, me, d)) continue;
            State s3 = s2;
            int mvs2[tron::MAXN];
            for (int k = 0; k < s.N; ++k)
                mvs2[k] = (k == me) ? d : (s2.alive[k] ? greedyOppMove(s2, k) : s2.last_dir[k]);
            tron::step(s3, mvs2);
            const double v = evalState12(s3, s2);
            if (v > bestNext) bestNext = v;
        }
        if (bestNext > bestVal) { bestVal = bestNext; bestDir = mm[i]; }
    }
    return bestDir;
}

}  // namespace

int decide(const tron::State& s) {
    if (s.N == 2) return root1(s);
    if (s.N <= 4) return rootMulti(s);
    return root12(s);
}
