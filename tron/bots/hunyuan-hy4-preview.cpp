// 光轮对战 bot：Voronoi 地盘估值 + 序贯近似 Alpha-Beta 搜索 + 迭代加深 + 收官填格。
#include "tron.hpp"

#include <algorithm>
#include <cstring>

namespace {

constexpr int WINV = 1000000;
constexpr int MAXC = tron::MAXW * tron::MAXH;

// ---------------------------------------------------------------- 邻接表
int16_t nb_of[4][MAXC];
int nbW = 0, nbH = 0;

inline void ensure_nb(int W, int H) {
    if (nbW == W && nbH == H) return;
    nbW = W;
    nbH = H;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int c = y * W + x;
            nb_of[0][c] = y > 0 ? c - W : -1;
            nb_of[1][c] = x < W - 1 ? c + 1 : -1;
            nb_of[2][c] = y < H - 1 ? c + W : -1;
            nb_of[3][c] = x > 0 ? c - 1 : -1;
        }
}

// ---------------------------------------------------------------- Voronoi 地盘
// 多源 BFS：每人一格发现的先到先看所有权；同一层多人同时到达的格子判为争议（谁都不算）。
// 争议格仍会被各方继续扩散（用 bitmask 记录到达者）。
uint16_t vown[MAXC];
int vlev[MAXC];
int vcur[MAXC], vnxt[MAXC];

void voronoi(const tron::State& s, int counts[]) {
    const int W = s.W, H = s.H, N = s.N, CN = W * H;
    for (int i = 0; i < CN; ++i) {
        vown[i] = 0;
        vlev[i] = -1;
    }
    const int8_t* cc = &s.cell[0][0];
    int* cur = vcur;
    int* nxt = vnxt;
    int cn = 0;
    for (int k = 0; k < N; ++k) {
        if (!s.alive[k]) continue;
        const int c = s.y[k] * W + s.x[k];
        vown[c] = uint16_t(1u << k);
        vlev[c] = 0;
        cur[cn++] = c;
    }
    for (int level = 0; cn > 0; ++level) {
        int nn = 0;
        for (int i = 0; i < cn; ++i) {
            const int c = cur[i];
            const uint16_t m = vown[c];
            if (m == 0 || (m & (m - 1)) != 0) {
                // 争议格仍继续向外扩散（各自成正）
            }
            for (int d = 0; d < 4; ++d) {
                const int nc = nb_of[d][c];
                if (nc < 0 || cc[nc] != tron::EMPTY) continue;
                const uint16_t om = vown[nc];
                if (om == 0) {
                    vown[nc] = m;
                    vlev[nc] = level + 1;
                    nxt[nn++] = nc;
                } else if (vlev[nc] == level + 1) {
                    vown[nc] = uint16_t(om | m);
                }
            }
        }
        int* t = cur;
        cur = nxt;
        nxt = t;
        cn = nn;
    }
    for (int k = 0; k < N; ++k) counts[k] = 0;
    for (int i = 0; i < CN; ++i) {
        uint16_t m = vown[i];
        if (m == 0 || (m & (m - 1)) != 0) continue;
        int k = 0;
        while ((m & 1u) == 0) {
            m = uint16_t(m >> 1);
            ++k;
        }
        if (k < N) ++counts[k];
    }
}

// ---------------------------------------------------------------- 搜索
struct Searcher {
    int N = 0, me = 0;
    int mv[tron::MAXN];
    long long nodes = 0;
    bool timedOut = false;
    double stopMs = 8;
    int lastVal = 0;

    int evaluate(const tron::State& s) {
        if (!s.alive[me]) return -WINV + s.turn;
        int others = 0;
        for (int k = 0; k < N; ++k)
            if (k != me && s.alive[k]) ++others;
        if (others == 0) return WINV - s.turn;
        int counts[tron::MAXN];
        voronoi(s, counts);
        const int myC = counts[me];
        int best = 0, sum = 0, cnt = 0;
        for (int k = 0; k < N; ++k)
            if (k != me && s.alive[k]) {
                if (counts[k] > best) best = counts[k];
                sum += counts[k];
                ++cnt;
            }
        if (cnt == 0) return WINV - s.turn;
        if (N <= 2) return myC - best;
        const int avg = sum / cnt;
        return 2 * myC - best - avg;
    }

    // 不参与分支的人怎么走：能直行就直行，否则挑空邻居最多的方向
    int policy(const tron::State& s, int k) const {
        const int straight = s.last_dir[k];
        if (tron::can_move(s, k, straight)) return straight;
        int bd = -1, bv = -1;
        for (int d = 0; d < 4; ++d) {
            if (!tron::can_move(s, k, d)) continue;
            const int nx = s.x[k] + tron::DX[d], ny = s.y[k] + tron::DY[d];
            int f = 0;
            for (int e = 0; e < 4; ++e)
                if (tron::is_free(s, nx + tron::DX[e], ny + tron::DY[e])) ++f;
            if (f > bv) {
                bv = f;
                bd = d;
            }
        }
        return bd >= 0 ? bd : tron::U;
    }

    inline void tick() {
        if ((++nodes & 127) == 0 && tron::budget_left() < stopMs) timedOut = true;
    }

    // 一回合：本回合轮到的每个人依次给方向（前 nBra 个做分支），全部定下后结算。
    // 注意 mv 是共享的：更深回合的搜索会把本回合已填好的方向写脏，所以每层退出时都要还原。
    int assign(const tron::State& s, const int* order, int nBra, int norder, int idx, int depth, int alpha, int beta) {
        if (idx == norder) {
            // 只拷贝棋盘真正用到的那部分（State 是固定 41×41 的，全拷要贵好几倍）
            tron::State t;
            t.W = s.W;
            t.H = s.H;
            t.N = s.N;
            t.me = s.me;
            t.turn = s.turn;
            t.max_turns = s.max_turns;
            t.over = s.over;
            for (int y = 0; y < s.H; ++y) std::memcpy(t.cell[y], s.cell[y], size_t(s.W));
            for (int k = 0; k < s.N; ++k) {
                t.x[k] = s.x[k];
                t.y[k] = s.y[k];
                t.alive[k] = s.alive[k];
                t.last_dir[k] = s.last_dir[k];
                t.death_turn[k] = s.death_turn[k];
                t.cause[k] = s.cause[k];
            }
            tron::step(t, mv);
            return rec(t, depth - 1, alpha, beta);
        }
        const int k = order[idx];
        const int saved = mv[k];
        const int r = assignBody(s, order, nBra, norder, idx, depth, alpha, beta);
        mv[k] = saved;
        return r;
    }

    int assignBody(const tron::State& s, const int* order, int nBra, int norder, int idx, int depth, int alpha,
                   int beta) {
        const bool myTurn = idx == 0;
        const bool branch = idx < nBra;
        if (!branch) {
            mv[order[idx]] = policy(s, order[idx]);
            return assign(s, order, nBra, norder, idx + 1, depth, alpha, beta);
        }
        if (myTurn) {
            int cand[4], nc = 0;
            int sc[4];
            for (int d = 0; d < 4; ++d) {
                if (!tron::can_move(s, me, d)) continue;
                const int nx = s.x[me] + tron::DX[d], ny = s.y[me] + tron::DY[d];
                int f = 0;
                for (int e = 0; e < 4; ++e)
                    if (tron::is_free(s, nx + tron::DX[e], ny + tron::DY[e])) ++f;
                sc[d] = (d == s.last_dir[me] ? 100 : 0) + f;
            }
            // 候选方向排序：直行优先 + 周围空邻居多
            int ord[4], no = 0;
            for (int d = 0; d < 4; ++d)
                if (tron::can_move(s, me, d)) ord[no++] = d;
            nc = no;
            for (int i = 1; i < no; ++i)
                for (int j = i; j > 0 && sc[ord[j]] > sc[ord[j - 1]]; --j) {
                    int t = ord[j];
                    ord[j] = ord[j - 1];
                    ord[j - 1] = t;
                }
            nc = no;
            for (int i = 0; i < nc; ++i) cand[i] = ord[i];
            int bestV = -2 * WINV;
            for (int i = 0; i < nc; ++i) {
                mv[me] = cand[i];
                const int v = assign(s, order, nBra, norder, idx + 1, depth, alpha, beta);
                if (timedOut) return 0;
                if (v > bestV) bestV = v;
                if (bestV > alpha) alpha = bestV;
                if (alpha >= beta) break;
            }
            return bestV;
        }
        // 对手节点：偏执模型（对手联合让我最差），天然可以用 alpha-beta 剪枝
        const int k = order[idx];
        int cand[4], nc = 0, sc[4];
        for (int d = 0; d < 4; ++d) {
            if (!tron::can_move(s, k, d)) continue;
            const int nx = s.x[k] + tron::DX[d], ny = s.y[k] + tron::DY[d];
            int f = 0;
            for (int e = 0; e < 4; ++e)
                if (tron::is_free(s, nx + tron::DX[e], ny + tron::DY[e])) ++f;
            sc[d] = (d == s.last_dir[k] ? 100 : 0) + f;
            cand[nc++] = d;
        }
        if (nc == 0) {
            mv[k] = tron::U;
            return assign(s, order, nBra, norder, idx + 1, depth, alpha, beta);
        }
        for (int i = 1; i < nc; ++i)
            for (int j = i; j > 0 && sc[cand[j]] > sc[cand[j - 1]]; --j) {
                int t = cand[j];
                cand[j] = cand[j - 1];
                cand[j - 1] = t;
            }
        int bestV = 2 * WINV;
        for (int i = 0; i < nc; ++i) {
            mv[k] = cand[i];
            const int v = assign(s, order, nBra, norder, idx + 1, depth, alpha, beta);
            if (timedOut) return 0;
            if (v < bestV) bestV = v;
            if (bestV < beta) beta = bestV;
            if (alpha >= beta) break;
        }
        return bestV;
    }

    int rec(const tron::State& s, int depth, int alpha, int beta) {
        tick();
        if (timedOut) return 0;
        if (s.over || depth <= 0) return evaluate(s);
        // 组顺序：我第一（悲观/安全），然后按到我的曼哈顿距离排序的对手
        int opps[tron::MAXN], key[tron::MAXN], nop = 0;
        for (int k = 0; k < N; ++k)
            if (k != me && s.alive[k]) {
                opps[nop] = k;
                key[nop] = std::abs(s.x[k] - s.x[me]) + std::abs(s.y[k] - s.y[me]);
                ++nop;
            }
        for (int i = 1; i < nop; ++i)
            for (int j = i; j > 0 && key[j] < key[j - 1]; --j) {
                int t = opps[j];
                opps[j] = opps[j - 1];
                opps[j - 1] = t;
                t = key[j];
                key[j] = key[j - 1];
                key[j - 1] = t;
            }
        int order[tron::MAXN], norder = 0;
        order[norder++] = me;
        int nBra = 1;
        const int reach = 2 * depth + 6;
        int maxOpp = N <= 2 ? 1 : (N <= 4 ? N - 1 : 3);
        int used = 0;
        for (int i = 0; i < nop; ++i)
            if (used < maxOpp && key[i] <= reach) {
                order[norder++] = opps[i];
                ++used;
                ++nBra;
            }
        for (int i = 0; i < nop; ++i)
            if (!orderContains(order, norder, opps[i])) order[norder++] = opps[i];
        return assign(s, order, nBra, norder, 0, depth, alpha, beta);
    }

    static bool orderContains(const int* order, int n, int k) {
        for (int i = 0; i < n; ++i)
            if (order[i] == k) return true;
        return false;
    }

    // 根：对每个合法方向做一次完整回合搜索
    int root(const tron::State& s, int depth, const int* rootOrder, int nl) {
        int order[tron::MAXN], norder = 0, nBra = 1;
        order[norder++] = me;
        int opps[tron::MAXN], key[tron::MAXN], nop = 0;
        for (int k = 0; k < N; ++k)
            if (k != me && s.alive[k]) {
                opps[nop] = k;
                key[nop] = std::abs(s.x[k] - s.x[me]) + std::abs(s.y[k] - s.y[me]);
                ++nop;
            }
        for (int i = 1; i < nop; ++i)
            for (int j = i; j > 0 && key[j] < key[j - 1]; --j) {
                int t = opps[j];
                opps[j] = opps[j - 1];
                opps[j - 1] = t;
                t = key[j];
                key[j] = key[j - 1];
                key[j - 1] = t;
            }
        const int reach = 2 * depth + 6;
        const int maxOpp = N <= 2 ? 1 : (N <= 4 ? N - 1 : 3);
        int used = 0;
        for (int i = 0; i < nop; ++i)
            if (used < maxOpp && key[i] <= reach) {
                order[norder++] = opps[i];
                ++used;
                ++nBra;
            }
        for (int i = 0; i < nop; ++i)
            if (!orderContains(order, norder, opps[i])) order[norder++] = opps[i];

        int bestD = rootOrder[0], bestV = -2 * WINV;
        for (int i = 0; i < nl; ++i) {
            const int d = rootOrder[i];
            mv[me] = d;
            const int v = assign(s, order, nBra, norder, 1, depth, bestV, 2 * WINV);
            if (timedOut) break;
            if (v > bestV) {
                bestV = v;
                bestD = d;
            }
        }
        lastVal = bestV;
        return bestD;
    }
};

// ---------------------------------------------------------------- 连通性 / 收官
int fvis[MAXC];
int fstamp = 0;
int fq[MAXC];

// 从 (sx, sy) 出发能到的空地数（该格本身已被占用，不算在内）
int floodCount(const tron::State& s, int sx, int sy) {
    ++fstamp;
    const int W = s.W;
    const int8_t* cc = &s.cell[0][0];
    const int start = sy * W + sx;
    int head = 0, tail = 0;
    fq[tail++] = start;
    fvis[start] = fstamp;
    int cnt = 0;
    while (head < tail) {
        const int c = fq[head++];
        for (int d = 0; d < 4; ++d) {
            const int nc = nb_of[d][c];
            if (nc < 0 || cc[nc] != tron::EMPTY || fvis[nc] == fstamp) continue;
            fvis[nc] = fstamp;
            ++cnt;
            fq[tail++] = nc;
        }
    }
    return cnt;
}

// 我和所有活着的对手是否已经被完全隔开（我的连通块里没有别人的头）
bool separated(const tron::State& s, int me) {
    ++fstamp;
    const int W = s.W;
    const int8_t* cc = &s.cell[0][0];
    const int start = s.y[me] * W + s.x[me];
    int head = 0, tail = 0;
    fq[tail++] = start;
    fvis[start] = fstamp;
    while (head < tail) {
        const int c = fq[head++];
        for (int d = 0; d < 4; ++d) {
            const int nc = nb_of[d][c];
            if (nc < 0 || cc[nc] != tron::EMPTY || fvis[nc] == fstamp) continue;
            fvis[nc] = fstamp;
            fq[tail++] = nc;
        }
    }
    // 别人的头本身是轨迹，不会被洪水波及：看它四周有没有属于我这一块的格子
    for (int k = 0; k < s.N; ++k) {
        if (k == me || !s.alive[k]) continue;
        for (int d = 0; d < 4; ++d) {
            const int cx = s.x[k] + tron::DX[d], cy = s.y[k] + tron::DY[d];
            if (!tron::inside(s, cx, cy)) continue;
            if (fvis[cy * W + cx] == fstamp) return false;
        }
    }
    return true;
}

inline void myMove(tron::State& s, int me, int d) {
    s.last_dir[me] = d;
    s.x[me] += tron::DX[d];
    s.y[me] += tron::DY[d];
    s.cell[s.y[me]][s.x[me]] = int8_t(me);
}

// 贪心填格：每步选"不切断自己空间、且落点周围空邻居最少"的方向，直到走不动。
// 返回还能再走多少步。
int greedyFill(tron::State& s, int me, int cap, double lowMs) {
    int steps = 0;
    while (steps < cap) {
        if (tron::budget_left() < lowMs) break;
        const int x0 = s.x[me], y0 = s.y[me];
        const int base = floodCount(s, x0, y0);
        int bd = -1, bestLoss = 1 << 30, bestFree = 1 << 30;
        for (int d = 0; d < 4; ++d) {
            const int nx = x0 + tron::DX[d], ny = y0 + tron::DY[d];
            if (!tron::is_free(s, nx, ny)) continue;
            s.cell[ny][nx] = int8_t(me);
            const int r = floodCount(s, nx, ny);
            s.cell[ny][nx] = tron::EMPTY;
            int f = 0;
            for (int e = 0; e < 4; ++e)
                if (tron::is_free(s, nx + tron::DX[e], ny + tron::DY[e])) ++f;
            const int loss = (base - 1) - r;
            if (loss < bestLoss || (loss == bestLoss && f < bestFree)) {
                bestLoss = loss;
                bestFree = f;
                bd = d;
            }
        }
        if (bd < 0) break;
        myMove(s, me, bd);
        ++steps;
    }
    return steps;
}

// 收官：挑"预计还能活最久"的第一步
int endgameMove(const tron::State& s, int me, int legal[], int nl) {
    int bestD = legal[0], bestSteps = -1;
    for (int i = 0; i < nl; ++i) {
        const int d = legal[i];
        tron::State t = s;
        myMove(t, me, d);
        const int steps = 1 + greedyFill(t, me, 400, 6.0);
        if (steps > bestSteps) {
            bestSteps = steps;
            bestD = d;
        }
        if (tron::budget_left() < 8.0) break;
    }
    return bestD;
}

}  // namespace

int decide(const tron::State& s) {
    ensure_nb(s.W, s.H);
    const int me = s.me;
    int legal[4], nl = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, me, d)) legal[nl++] = d;
    if (nl == 0) return tron::U;
    if (nl == 1) return legal[0];

    // 和所有人都隔开之后，胜负只取决于谁把自己那块地填得更久：换成专门的填格搜索
    const int myArea = floodCount(s, s.x[me], s.y[me]);
    int shownDepth = 0, shownVal = 0, shownEnd = 0;
    if (myArea <= 500 && separated(s, me)) {
        return endgameMove(s, me, legal, nl);
    }

    Searcher S;
    S.N = s.N;
    S.me = me;
    S.stopMs = 8.0;
    int rootOrder[4];
    for (int i = 0; i < nl; ++i) rootOrder[i] = legal[i];
    int bestD = legal[0];
    const int maxD = s.N == 2 ? 40 : (s.N <= 4 ? 24 : 16);
    for (int depth = 1; depth <= maxD; ++depth) {
        const int d = S.root(s, depth, rootOrder, nl);
        if (S.timedOut) break;
        bestD = d;
        shownDepth = depth;
        shownVal = S.lastVal;
        for (int i = 0; i < nl; ++i)
            if (rootOrder[i] == bestD) {
                for (int j = i; j > 0; --j) rootOrder[j] = rootOrder[j - 1];
                rootOrder[0] = bestD;
                break;
            }
        if (S.lastVal >= WINV - MAXC) break;    // 已经算出必胜，不必再深挖
    }
    if (!tron::can_move(s, me, bestD)) bestD = legal[0];
    return bestD;
}
