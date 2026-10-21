// sparring_hard.cpp —— 强陪练（不进选手包，只发编译好的程序 bots/sparring_hard.exe / .dll）。
//   2 人：同时行动近似成"我先选、对手看到再选"（偏悲观）的 alpha-beta，抢格按回合优先级精确结算；
//         迭代加深 + 置换表（跨回合保留）。叶子评估 = Voronoi 地盘（同时到达的格按优先级判），地盘按关节点
//         估计"真正走得到"的格数（死胡同只算最好的一条），再加地盘里的连边数。
//         双方分离时按奇偶上界比谁走得久。
//   多人：选最近的一个对手做同样的搜索，其余对手原地不动、当作 Voronoi 的源；
//         第一步避开"本回合比我优先的其余对手下一步可能进的格"。评估 = 我的地盘 − 对手地盘 / 2。
//   与所有人分离后：终局填格，迭代加深穷举，叶子用关节点估计与奇偶上界取小。
// 不看时间：每步的搜索量按工作量固定，结果可复现。强度数据见 说明.md。
#include "tron.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

constexpr int MAXP = 43 * 43;
constexpr int INF = 1 << 30;
constexpr int WIN = 1 << 26;

// 每步的搜索量按"工作量"固定，不看时间：工作量 ≈ BFS / DFS 访问的格子数 + 每个节点、每次评估的固定开销，
// 与耗时大致成正比（静音模式下约 22 毫秒 / 百万）。对战搜索每步 65 万，终局填格每步 80 万。
constexpr long SEARCH_BUDGET = 650000;
constexpr long FILL_BUDGET = 800000;
long work = 0;    // 累计工作量（只增不减；每步的上限 = 开始时的值 + 预算）

int W, H, PW, N, ME;
int DV[4];
uint8_t blk[MAXP];      // 1 = 不能走：界外、障碍、任何轨迹（含头部）
uint8_t col[MAXP];      // 棋盘格颜色 (x + y) & 1
int head[tron::MAXN];
bool alv[tron::MAXN];

inline int prio(int k, int turn) { return ((k - turn) % N + N) % N; }

// ---------------------------------------------------------------- Voronoi
uint32_t vstamp = 0;
uint32_t seen[MAXP];
int lay[MAXP];
int8_t own[MAXP];
uint16_t msk[MAXP];
int qa[MAXP], qb[MAXP];
int vid[tron::MAXN], vturn;    // 各源的玩家编号、BFS 第 0 层对应的回合（同时到达时按优先级判给谁）

struct Vor {
    int cnt[tron::MAXN];
    int edges[tron::MAXN];
    int par[tron::MAXN][2];     // 各人地盘里两种颜色的格数
    uint16_t conn;              // 与我（src 0）相遇过的人（位）
    int meet[tron::MAXN];       // 与我第一次相遇的层数
};

// srcs[0] 是"我"，vid[] 是各源的编号。同时出发的 BFS：严格先到的格子归他；
// 同时到的格子按到达那一回合的抢格优先级记给优先者，但不再向外扩。
void voronoi(const int* srcs, int ns, Vor& v) {
    if (++vstamp == 0) {
        std::memset(seen, 0, sizeof seen);
        vstamp = 1;
    }
    for (int i = 0; i < ns; ++i) {
        v.cnt[i] = v.edges[i] = v.par[i][0] = v.par[i][1] = 0;
        v.meet[i] = INF;
    }
    v.conn = 0;
    int* cur = qa;
    int* nxt = qb;
    int nc = 0;
    for (int i = 0; i < ns; ++i) {
        const int p = srcs[i];
        cur[nc++] = p;
        own[p] = int8_t(i);
        seen[p] = vstamp;
        lay[p] = -1;
        msk[p] = uint16_t(1u << i);
    }
    for (int L = 0; nc; ++L) {
        int nn = 0;
        for (int i = 0; i < nc; ++i) {
            const int p = cur[i];
            const int o = own[p];
            const uint16_t ob = uint16_t(1u << o);
            for (int d = 0; d < 4; ++d) {
                const int t = p + DV[d];
                if (blk[t]) continue;
                if (seen[t] != vstamp) {
                    seen[t] = vstamp;
                    lay[t] = L;
                    own[t] = int8_t(o);
                    msk[t] = ob;
                    nxt[nn++] = t;
                } else if (!(msk[t] & ob)) {
                    const uint16_t m = msk[t];
                    if (o == 0) {
                        v.conn |= m;
                        for (int j = 1; j < ns; ++j)
                            if ((m >> j & 1) && v.meet[j] == INF) v.meet[j] = L;
                    } else if (m & 1) {
                        v.conn |= ob;
                        if (v.meet[o] == INF) v.meet[o] = L;
                    }
                    if (lay[t] == L) {
                        msk[t] |= ob;
                        own[t] = -1;
                    }
                }
            }
        }
        int k = 0;
        for (int i = 0; i < nn; ++i) {
            const int t = nxt[i];
            const int o = own[t];
            if (o < 0) {
                int w = -1;
                for (int j = 0; j < ns; ++j)
                    if ((msk[t] >> j & 1) && (w < 0 || prio(vid[j], vturn + L) < prio(vid[w], vturn + L))) w = j;
                ++v.cnt[w];
                ++v.par[w][col[t]];
                continue;
            }
            ++v.cnt[o];
            ++v.par[o][col[t]];
            int e = 0;
            for (int d = 0; d < 4; ++d) e += !blk[t + DV[d]];
            v.edges[o] += e;
            nxt[k++] = t;
        }
        nc = k;
        work += nn;
        int* tmp = cur;
        cur = nxt;
        nxt = tmp;
    }
}

// 地盘里"真正走得到"的格数（关节点估计）：在 voronoi 之后调用，只走 own == i 的格子。
// 从头部出发做 Tarjan：子树被关节点隔开时，走进去就回不来，只能选一个这样的死胡同走到底；
// 其余的（能绕回来的）都能走到。返回 可回来的格数 + 最好的一条死胡同。
uint32_t cst[MAXP], cstamp = 0;
int dfn[MAXP], lowv[MAXP], ctimer;
int cown;
struct AB {
    int a, b;
};
AB chamber_dfs(int v) {
    int A = 1, B = 0;
    for (int d = 0; d < 4; ++d) {
        const int t = v + DV[d];
        if (blk[t] || seen[t] != vstamp || own[t] != cown) continue;
        if (cst[t] != cstamp) {
            cst[t] = cstamp;
            dfn[t] = lowv[t] = ++ctimer;
            const AB r = chamber_dfs(t);
            if (lowv[t] < lowv[v]) lowv[v] = lowv[t];
            if (lowv[t] >= dfn[v]) {
                if (r.a + r.b > B) B = r.a + r.b;
            } else {
                A += r.a;
                if (r.b > B) B = r.b;
            }
        } else if (dfn[t] < lowv[v]) lowv[v] = dfn[t];
    }
    return {A, B};
}
int chamber(int h, int i) {
    if (++cstamp == 0) {
        std::memset(cst, 0, sizeof cst);
        cstamp = 1;
    }
    cown = i;
    cst[h] = cstamp;
    dfn[h] = lowv[h] = ctimer = 0;
    const AB r = chamber_dfs(h);
    return r.a - 1 + r.b;
}

// 奇偶上界：从 h 出发能走的最多步数（区域里两种颜色的格数为 par）
inline int parity_bound(int h, const int par[2]) {
    const int first = par[col[h] ^ 1], second = par[col[h]];
    return first > second ? 2 * second + 1 : 2 * first;
}

// ---------------------------------------------------------------- 单人填格
long flimit;
bool fexhausted;    // 本层搜索没有碰到深度上限（已经穷举）
struct FillTimeout {};

// 迭代加深穷举，叶子用"关节点估计与奇偶上界取小"（是可走步数的上界，也是不错的估计）；
// 同分时先走出口少的格（贴墙）。工作量到 FILL_BUDGET 就停，用上一层搜完的结果。
int fill_est(int p) {
    int src[1] = {p};
    Vor v;
    voronoi(src, 1, v);
    work += v.cnt[0];
    return std::min(chamber(p, 0), parity_bound(p, v.par[0]));
}
int order_moves(int p, int* mv) {    // 出口少的在前；同样多时按 上右下左
    int n = 0, deg[4];
    for (int d = 0; d < 4; ++d) {
        const int t = p + DV[d];
        if (blk[t]) continue;
        int g = 0;
        for (int e = 0; e < 4; ++e) g += !blk[t + DV[e]];
        int i = n++;
        while (i > 0 && deg[i - 1] > g) mv[i] = mv[i - 1], deg[i] = deg[i - 1], --i;
        mv[i] = d;
        deg[i] = g;
    }
    return n;
}
int fill2_dfs(int p, int depth) {
    if (work > flimit) throw FillTimeout{};
    work += 4;
    const int ub = fill_est(p);
    if (depth == 0 || ub == 0) {
        if (ub > 0) fexhausted = false;
        return ub;
    }
    int mv[4];
    const int n = order_moves(p, mv);
    int best = 0;
    for (int i = 0; i < n; ++i) {
        const int t = p + DV[mv[i]];
        blk[t] = 1;
        int v;
        try {
            v = 1 + fill2_dfs(t, depth - 1);
        } catch (FillTimeout&) {
            blk[t] = 0;
            throw;
        }
        blk[t] = 0;
        if (v > best) {
            best = v;
            if (best >= ub) break;
        }
    }
    return best;
}
int fill_move(int a, int last) {
    int moves[4];
    const int nm = order_moves(a, moves);
    if (nm == 0) return last;
    if (nm == 1) return moves[0];
    flimit = work + FILL_BUDGET;
    int best_d = moves[0];
    for (int depth = 0; depth < 400; ++depth) {
        fexhausted = true;
        int cur_d = -1, cur_v = -1;
        try {
            for (int i = 0; i < nm; ++i) {
                const int t = a + DV[moves[i]];
                blk[t] = 1;
                int v;
                try {
                    v = 1 + fill2_dfs(t, depth);
                } catch (FillTimeout&) {
                    blk[t] = 0;
                    throw;
                }
                blk[t] = 0;
                if (v > cur_v) cur_v = v, cur_d = moves[i];
            }
        } catch (FillTimeout&) {
            break;
        }
        best_d = cur_d;
        for (int i = 0; i < nm; ++i)
            if (moves[i] == best_d) {
                for (int j = i; j > 0; --j) moves[j] = moves[j - 1];
                moves[0] = best_d;
                break;
            }
        if (fexhausted) break;
    }
    return best_d;
}
// ---------------------------------------------------------------- 双人搜索（2 人局，或多人局里我对最近的对手）
// 局面：blk、我的头 a、对手的头 b、回合 turn。其余对手（多人局）原地不动。
int RIV;                    // 对手编号
int others[tron::MAXN], othid[tron::MAXN], nothers;    // 其余活着的对手的头
long limit;
bool aborted;
constexpr int K_CELL = 10, K_EDGE = 1;   // 评估：每格 10 分，地盘里每条连边 1 分

int srcbuf[tron::MAXN];

int evaluate(int a, int b, int turn, bool riv_alive) {
    work += 40;
    int ns = 0;
    vid[ns] = ME;
    srcbuf[ns++] = a;
    if (riv_alive) vid[ns] = RIV, srcbuf[ns++] = b;
    for (int i = 0; i < nothers; ++i) vid[ns] = othid[i], srcbuf[ns++] = others[i];
    vturn = turn;
    Vor v;
    voronoi(srcbuf, ns, v);
    if (N == 2) {
        if (!(v.conn & 2)) {    // 分离：比谁能走得更久
            const int fa = parity_bound(a, v.par[0]), fb = parity_bound(b, v.par[1]);
            if (fa > fb) return WIN / 2 + (fa - fb) * 16;
            if (fa < fb) return -WIN / 2 + (fa - fb) * 16;
            return 0;
        }
        const int c0 = chamber(a, 0), c1 = chamber(b, 1);
        work += v.cnt[0] + v.cnt[1];
        return K_CELL * (c0 - c1) + K_EDGE * (v.edges[0] - v.edges[1]);
    }
    // 多人：我的地盘；与谁都不相连时按奇偶上界
    int mine;
    const int myc = v.cnt[0];
    if (v.conn == 0) mine = K_CELL * std::min(myc, parity_bound(a, v.par[0])) + K_EDGE * v.edges[0];
    else mine = K_CELL * myc + K_EDGE * v.edges[0];
    int theirs = riv_alive ? K_CELL * v.cnt[1] + K_EDGE * v.edges[1] : 0;
    return mine - theirs / 2 + (riv_alive ? 0 : 3000);
}

// 置换表
struct TT {
    uint64_t key;
    int val;
    int8_t depth, flag, ma, mb;
};
constexpr int TTSIZE = 1 << 18;
TT tt[TTSIZE];
uint64_t ZC[MAXP], ZA[MAXP], ZB[MAXP], ZT[tron::MAXN], ZR[tron::MAXN];
uint64_t root_hash;    // 根局面所有轨迹格的 ZC 异或（置换表跨回合保留）
bool zinit = false;

uint64_t sm(uint64_t& s) {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

int root_turn;

// 置换表里的胜负分按"离本节点几步"存，取出时换回"离根几步"
constexpr int MATE = WIN - 10000;
inline int to_tt(int v, int ply) { return v >= MATE ? v + ply : v <= -MATE ? v - ply : v; }
inline int from_tt(int v, int ply) { return v >= MATE ? v - ply : v <= -MATE ? v + ply : v; }

// 我走 da、对手走 db 之后的结果。返回 0 = 都活；否则 1 我死、2 对手死、3 都死
inline int outcome(int a, int b, int da, int db, int turn) {
    const int ta = a + DV[da], tb = b + DV[db];
    bool dA = blk[ta], dB = blk[tb];
    if (!dA && !dB && ta == tb) {
        if (prio(ME, turn) < prio(RIV, turn)) dB = true;
        else dA = true;
    }
    return (dA ? 1 : 0) | (dB ? 2 : 0);
}

int search(int a, int b, int depth, int alpha, int beta, int ply, uint64_t hash) {
    const int turn = root_turn + ply;
    if (depth <= 0) return evaluate(a, b, turn, true);
    if (work >= limit) {
        aborted = true;
        return 0;
    }
    work += 10;
    int ma[4], nma = 0, mb[4], nmb = 0;
    for (int d = 0; d < 4; ++d) {
        if (!blk[a + DV[d]]) ma[nma++] = d;
        if (!blk[b + DV[d]]) mb[nmb++] = d;
    }
    if (nma == 0) return nmb == 0 ? (N == 2 ? 0 : -WIN + ply) : -WIN + ply;
    if (nmb == 0) {
        if (N == 2) return WIN - ply;
        // 多人：对手死了，看我剩下的地盘
        return evaluate(a, b, turn, false);
    }
    const uint64_t key = hash ^ ZA[a] ^ ZB[b] ^ ZT[turn % N] ^ ZR[RIV];
    TT& e = tt[key & (TTSIZE - 1)];
    int hint_a = -1, hint_b = -1;
    if (e.key == key) {
        if (e.depth >= depth) {
            const int ev = from_tt(e.val, ply);
            if (e.flag == 0) return ev;
            if (e.flag == 1 && ev >= beta) return ev;
            if (e.flag == 2 && ev <= alpha) return ev;
        }
        hint_a = e.ma;
        hint_b = e.mb;
    }
    if (hint_a >= 0)
        for (int i = 0; i < nma; ++i)
            if (ma[i] == hint_a) {
                ma[i] = ma[0];
                ma[0] = hint_a;
                break;
            }
    const int a0 = alpha;
    int best = -INF, bma = ma[0], bmb = -1;
    for (int i = 0; i < nma; ++i) {
        const int da = ma[i], ta = a + DV[da];
        int order[4], no = 0;
        if (hint_b >= 0)
            for (int j = 0; j < nmb; ++j)
                if (mb[j] == hint_b) order[no++] = hint_b;
        for (int j = 0; j < nmb; ++j)
            if (mb[j] != hint_b) order[no++] = mb[j];
        int worst = INF, wr = order[0];
        for (int j = 0; j < no; ++j) {
            const int db = order[j], tb = b + DV[db];
            const int oc = outcome(a, b, da, db, turn);
            int v;
            if (oc == 1) v = -WIN + ply;
            else if (oc == 3) v = N == 2 ? 0 : -WIN + ply;
            else if (oc == 2) {
                if (N == 2) v = WIN - ply;
                else {
                    blk[ta] = 1;
                    v = evaluate(ta, b, turn + 1, false);
                    blk[ta] = 0;
                }
            } else {
                blk[ta] = blk[tb] = 1;
                v = search(ta, tb, depth - 1, alpha, worst < beta ? worst : beta, ply + 1, hash ^ ZC[ta] ^ ZC[tb]);
                blk[ta] = blk[tb] = 0;
            }
            if (aborted) return 0;
            if (v < worst) worst = v, wr = db;
            if (worst <= alpha) break;
        }
        if (worst > best) best = worst, bma = da, bmb = wr;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    e.key = key;
    e.val = to_tt(best, ply);
    e.depth = int8_t(depth);
    e.flag = best <= a0 ? 2 : best >= beta ? 1 : 0;
    e.ma = int8_t(bma);
    e.mb = int8_t(bmb);
    return best;
}

// 第一步的风险：目标格挨着一个本回合比我优先的"其余对手"（他走进来我就死）
int first_step_risk(int t, int turn) {
    int r = 0;
    for (int k = 0; k < N; ++k) {
        if (k == ME || k == RIV || !alv[k]) continue;
        if (prio(k, turn) > prio(ME, turn)) continue;
        for (int d = 0; d < 4; ++d)
            if (head[k] + DV[d] == t) ++r;
    }
    return r;
}

int duel(int last, int turn) {
    const int a = head[ME], b = head[RIV];
    int ma[4], nma = 0;
    ma[nma++] = last;
    for (int d = 0; d < 4; ++d)
        if (d != last) ma[nma++] = d;
    int k = 0;
    for (int i = 0; i < nma; ++i)
        if (!blk[a + DV[ma[i]]]) ma[k++] = ma[i];
    nma = k;
    if (nma == 0) return last;
    int mb[4], nmb = 0;
    for (int d = 0; d < 4; ++d)
        if (!blk[b + DV[d]]) mb[nmb++] = d;
    limit = work + SEARCH_BUDGET;
    root_turn = turn;
    root_hash = 0;
    for (int p = 0; p < (H + 2) * PW; ++p)
        if (blk[p]) root_hash ^= ZC[p];
    int best_move = ma[0];
    int risk[4];
    for (int i = 0; i < nma; ++i) risk[i] = first_step_risk(a + DV[ma[i]], turn);
    for (int depth = 1; depth <= 60; ++depth) {
        aborted = false;
        int best = -INF, bm = ma[0];
        for (int i = 0; i < nma; ++i) {
            const int da = ma[i], ta = a + DV[da];
            int worst = INF;
            if (nmb == 0) {
                blk[ta] = 1;
                worst = N == 2 ? WIN : evaluate(ta, b, turn + 1, false);
                blk[ta] = 0;
            }
            for (int j = 0; j < nmb; ++j) {
                const int db = mb[j], tb = b + DV[db];
                const int oc = outcome(a, b, da, db, turn);
                int v;
                if (oc == 1) v = -WIN;
                else if (oc == 3) v = N == 2 ? 0 : -WIN;
                else if (oc == 2) {
                    if (N == 2) v = WIN;
                    else {
                        blk[ta] = 1;
                        v = evaluate(ta, b, turn + 1, false);
                        blk[ta] = 0;
                    }
                } else {
                    blk[ta] = blk[tb] = 1;
                    v = search(ta, tb, depth - 1, best, worst, 1, root_hash ^ ZC[ta] ^ ZC[tb]);
                    blk[ta] = blk[tb] = 0;
                }
                if (aborted) break;
                if (v < worst) worst = v;
                if (worst <= best) break;
            }
            if (aborted) break;
            if (risk[i] && worst > -WIN / 2) worst -= WIN / 4 * risk[i];
            if (worst > best) best = worst, bm = da;
        }
        if (aborted) break;    // 本层没搜完：用上一层的结果
        best_move = bm;
        // 上一层的最好走法排到最前
        for (int i = 0; i < nma; ++i)
            if (ma[i] == bm) {
                for (int j = i; j > 0; --j) ma[j] = ma[j - 1], risk[j] = risk[j - 1];
                ma[0] = bm;
                risk[0] = first_step_risk(a + DV[bm], turn);
                break;
            }
        if (best >= WIN / 2 || best <= -WIN / 2) {
            if (best >= WIN - 100 || best <= -WIN + 100) break;
        }
    }
    return best_move;
}

}  // namespace

int decide(const tron::State& s) {
    if (!zinit) {
        uint64_t seed = 12345;
        for (int i = 0; i < MAXP; ++i) ZC[i] = sm(seed), ZA[i] = sm(seed), ZB[i] = sm(seed);
        for (int k = 0; k < tron::MAXN; ++k) ZT[k] = sm(seed), ZR[k] = sm(seed);
        zinit = true;
    }
    W = s.W;
    H = s.H;
    N = s.N;
    ME = s.me;
    PW = W + 2;
    DV[tron::U] = -PW;
    DV[tron::R] = 1;
    DV[tron::D] = PW;
    DV[tron::L] = -1;
    std::memset(blk, 1, sizeof blk);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int p = (y + 1) * PW + x + 1;
            blk[p] = s.cell[y][x] != tron::EMPTY;
            col[p] = uint8_t((x + y) & 1);
        }
    for (int k = 0; k < N; ++k) {
        head[k] = (s.y[k] + 1) * PW + s.x[k] + 1;
        alv[k] = s.alive[k];
    }
    const int last = s.last_dir[ME];
    // 看我和谁相连、谁最近
    int srcs[tron::MAXN], who[tron::MAXN], ns = 0;
    srcs[ns] = head[ME];
    who[ns++] = ME;
    for (int k = 0; k < N; ++k)
        if (k != ME && alv[k]) srcs[ns] = head[k], who[ns++] = k;
    for (int i = 0; i < ns; ++i) vid[i] = who[i];
    vturn = s.turn;
    Vor v;
    voronoi(srcs, ns, v);
    if (v.conn == 0) return fill_move(head[ME], last);
    int bi = -1;
    for (int i = 1; i < ns; ++i)
        if ((v.conn >> i & 1) && (bi < 0 || v.meet[i] < v.meet[bi])) bi = i;
    RIV = who[bi];
    nothers = 0;
    for (int i = 1; i < ns; ++i)
        if (i != bi) othid[nothers] = who[i], others[nothers++] = srcs[i];
    return duel(last, s.turn);
}
