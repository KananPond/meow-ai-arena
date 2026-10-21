// Tron bot:
//  - 1v1（含多人局后期只剩 1 个对手）：交替行动近似的 alpha-beta + 置换表，
//    叶节点用 Voronoi 领地 + 双方独立 flood fill 评估，迭代加深到预算用尽。
//  - 3/4/12 人：对手用"贪心+避战"策略建模，我方做束搜索（beam），
//    评估 = 自己的 Voronoi 领地（独立 flood 兜底）+ 最大对手领地差 + 最近活敌距离 + 击杀数。
//  - 多人根节点过滤被高优先级对手抢占的格子；多人推演走 tron::step 精确结算。
#include "tron.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace tron;

namespace {

constexpr int INF = 1 << 28;

// ---------------------------------------------------------------- 杂项
inline int cell_idx(int x, int y) { return y * MAXW + x; }

uint64_t rng_state = 0x123456789ABCDEF0ULL;
inline uint64_t xrng() {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint64_t HCELL[MAXH * MAXW][MAXN];
uint64_t HX[MAXN][MAXW];
uint64_t HY[MAXN][MAXH];
uint64_t HALIVE[MAXN];
uint64_t HTURN[8192];
bool hash_ready = false;
int g_last_depth = 0;

void hash_init() {
    for (int i = 0; i < MAXH * MAXW; ++i)
        for (int j = 0; j < MAXN; ++j) HCELL[i][j] = xrng();
    for (int k = 0; k < MAXN; ++k) {
        for (int x = 0; x < MAXW; ++x) HX[k][x] = xrng();
        for (int y = 0; y < MAXH; ++y) HY[k][y] = xrng();
        HALIVE[k] = xrng();
    }
    for (int i = 0; i < 8192; ++i) HTURN[i] = xrng();
    hash_ready = true;
}

uint64_t state_key(const State& s) {
    uint64_t h = 0;
    for (int y = 0; y < s.H; ++y)
        for (int x = 0; x < s.W; ++x) {
            int c = s.cell[y][x];
            if (c >= 0) h ^= HCELL[cell_idx(x, y)][c];
        }
    for (int k = 0; k < s.N; ++k) {
        h ^= HX[k][s.x[k]] ^ HY[k][s.y[k]];
        if (s.alive[k]) h ^= HALIVE[k];
    }
    h ^= HTURN[s.turn & 8191];
    return h;
}

int g_stamp[MAXH * MAXW];
int g_stamp_cur = 0;

// 从 (sx,sy) 出发的独立 flood fill 数可达空格；distbuf 非空时记录步数（头的邻格=1）。
int flood_count(const State& s, int sx, int sy, int* distbuf = nullptr) {
    ++g_stamp_cur;
    int cnt = 0, qh = 0, qt = 0;
    static int q[MAXH * MAXW];
    for (int d = 0; d < 4; ++d) {
        int nx = sx + DX[d], ny = sy + DY[d];
        if (is_free(s, nx, ny) && g_stamp[cell_idx(nx, ny)] != g_stamp_cur) {
            g_stamp[cell_idx(nx, ny)] = g_stamp_cur;
            q[qt++] = cell_idx(nx, ny);
            if (distbuf) distbuf[cell_idx(nx, ny)] = 1;
            ++cnt;
        }
    }
    while (qh < qt) {
        int cur = q[qh++];
        int cx = cur % MAXW, cy = cur / MAXW;
        for (int d = 0; d < 4; ++d) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (is_free(s, nx, ny) && g_stamp[cell_idx(nx, ny)] != g_stamp_cur) {
                g_stamp[cell_idx(nx, ny)] = g_stamp_cur;
                q[qt++] = cell_idx(nx, ny);
                if (distbuf) distbuf[cell_idx(nx, ny)] = distbuf[cur] + 1;
                ++cnt;
            }
        }
    }
    return cnt;
}

// Voronoi 多源 BFS（所有活人的头）。own[k]=严格最近格数，tie=等距格数，
// nearest=最近活敌的曼哈顿距离。dist_out/owner_out 可选输出（仅 EMPTY 可达格写）。
void voronoi(const State& s, long own[MAXN], int& tie, int& nearest,
             int* dist_out = nullptr, int8_t* owner_out = nullptr) {
    for (int k = 0; k < s.N; ++k) own[k] = 0;
    tie = 0;
    nearest = 9999;
    static int dist[MAXH * MAXW];
    static int8_t owner[MAXH * MAXW];
    for (int i = 0, e = s.H * MAXW; i < e; ++i) { dist[i] = -1; owner[i] = -2; }
    static int q[MAXH * MAXW];
    int qh = 0, qt = 0;
    for (int k = 0; k < s.N; ++k)
        if (s.alive[k]) {
            int ci = cell_idx(s.x[k], s.y[k]);
            dist[ci] = 0; owner[ci] = (int8_t)k;
            q[qt++] = ci;
        }
    int my = s.me;
    while (qh < qt) {
        int ci = q[qh++];
        int cx = ci % MAXW, cy = ci / MAXW;
        int cd = dist[ci], ow = owner[ci];
        for (int d = 0; d < 4; ++d) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!inside(s, nx, ny) || s.cell[ny][nx] != EMPTY) continue;
            int ni = cell_idx(nx, ny);
            if (dist[ni] == -1) {
                dist[ni] = cd + 1;
                owner[ni] = (int8_t)(ow == -1 ? -1 : ow);
                q[qt++] = ni;
            } else if (dist[ni] == cd + 1 && owner[ni] >= 0 && ow >= 0 && owner[ni] != ow) {
                owner[ni] = -1;
            }
        }
    }
    for (int y = 0; y < s.H; ++y)
        for (int x = 0; x < s.W; ++x) {
            int ci = cell_idx(x, y);
            if (s.cell[y][x] != EMPTY || dist[ci] < 0) continue;
            int ow = owner[ci];
            if (ow == -1) ++tie;
            else if (ow >= 0) ++own[ow];
            if (dist_out) dist_out[ci] = dist[ci];
            if (owner_out) owner_out[ci] = owner[ci];
        }
    for (int k = 0; k < s.N; ++k)
        if (s.alive[k] && k != my)
            nearest = std::min(nearest, std::abs(s.x[k] - s.x[my]) + std::abs(s.y[k] - s.y[my]));
}

int openness(const State& s, int k, int d) {
    int nx = s.x[k] + DX[d], ny = s.y[k] + DY[d];
    int v = 0;
    for (int e = 0; e < 4; ++e)
        if (is_free(s, nx + DX[e], ny + DY[e])) ++v;
    return v;
}

// 搜索用：只移动 actor 一个人（交替行动近似）。d 合法时移动，否则 actor 死。
inline void move_actor(State& s, int k, int d) {
    int nx = s.x[k] + DX[d], ny = s.y[k] + DY[d];
    s.last_dir[k] = d;
    if (is_free(s, nx, ny)) {
        s.x[k] = nx; s.y[k] = ny;
        s.cell[ny][nx] = (int8_t)k;
    } else {
        s.alive[k] = false;
        s.death_turn[k] = s.turn;
    }
    ++s.turn;
}

// ============================================================ 1v1 alpha-beta
struct TTEntry {
    uint64_t key;
    int value;
    int16_t turn;
    int8_t depth;
    int8_t bound;   // 0 exact, 1 lower, 2 upper
};
uint32_t TT_BITS = 20, TT_SIZE = 1u << 20;
TTEntry* tt = nullptr;

struct AbCtx {
    int me, foe;
    uint64_t nodes;
    double stop_ms;
    bool stop;
};

// ------------------------------------------------ 交替行动 alpha-beta（深度优先）
int duel_eval(const State& s, int me, int foe);
struct ACtx {
    int me, foe;
    uint64_t nodes;
    double stop_ms;
    bool stop;
};

// ply 偶=我方(max)，奇=对手(min)。只移动当前 actor（交替近似），以换取搜索深度。
int ab_search(State& s, int depth, int alpha, int beta, int ply, ACtx& c, uint64_t key) {
    if ((++c.nodes & 1023) == 0 && budget_left() < c.stop_ms) { c.stop = true; return 0; }

    if (!s.alive[c.me] || !s.alive[c.foe]) return duel_eval(s, c.me, c.foe);
    if (depth <= 0) return duel_eval(s, c.me, c.foe);

    const int actor = (ply & 1) ? c.foe : c.me;
    uint32_t slot = (uint32_t)(key ^ (uint64_t(ply & 1) << 63)) & (TT_SIZE - 1);
    const TTEntry& te = tt[slot];
    int a0 = alpha, b0 = beta;
    if (te.key == key && te.turn == (int16_t)(s.turn & 0xFFFF) && te.depth == (int8_t)depth) {
        if (te.bound == 0) return te.value;
        if (te.bound == 1) alpha = std::max(alpha, te.value);
        else beta = std::min(beta, te.value);
        if (alpha >= beta) return te.value;
    }

    int dirs[4], nd = 0;
    for (int d = 0; d < 4; ++d)
        if (can_move(s, actor, d)) dirs[nd++] = d;
    int straight = s.last_dir[actor];
    std::sort(dirs, dirs + nd, [&](int a, int b) {
        int oa = (a == straight ? 4 : 0) + openness(s, actor, a);
        int ob = (b == straight ? 4 : 0) + openness(s, actor, b);
        return oa > ob;
    });
    if (nd == 0) dirs[nd++] = straight;

    int best = (ply & 1) ? INF : -INF;
    int ox = s.x[actor], oy = s.y[actor];
    for (int i = 0; i < nd; ++i) {
        State ns = s;
        bool lived;
        {
            int nx = ox + DX[dirs[i]], ny = oy + DY[dirs[i]];
            ns.last_dir[actor] = dirs[i];
            lived = is_free(ns, nx, ny);
            if (lived) {
                ns.x[actor] = nx; ns.y[actor] = ny;
                ns.cell[ny][nx] = (int8_t)actor;
            } else {
                ns.alive[actor] = false;
                ns.death_turn[actor] = ns.turn;
            }
            ++ns.turn;
        }
        uint64_t nk = key ^ HTURN[s.turn & 8191] ^ HTURN[ns.turn & 8191];
        if (lived)
            nk ^= HCELL[cell_idx(ns.x[actor], ns.y[actor])][actor]
                  ^ HX[actor][ox] ^ HX[actor][ns.x[actor]]
                  ^ HY[actor][oy] ^ HY[actor][ns.y[actor]];
        else
            nk ^= HALIVE[actor];

        int v = ab_search(ns, depth - 1, alpha, beta, ply ^ 1, c, nk);
        if (c.stop) return 0;
        if (ply & 1) {
            if (v < best) best = v;
            if (v < beta) beta = v;
        } else {
            if (v > best) best = v;
            if (v > alpha) alpha = v;
        }
        if (alpha >= beta) break;
    }

    int8_t bound = (best <= a0) ? 2 : (best >= b0) ? 1 : 0;
    TTEntry ne{key, best, (int16_t)(s.turn & 0xFFFF), (int8_t)depth, (int8_t)bound};
    tt[slot] = ne;
    return best;
}

// 对手贪心策略：在合法动作里选走后 Voronoi 领地（近似：flood 空间 - 贴我惩罚）最大的。
int foe_greedy(const State& s) {
    int foe = -1;
    // 该函数只在 1v1 计划里调用：从存活者推断
    int me = s.me;
    for (int k = 0; k < s.N; ++k)
        if (s.alive[k] && k != me) { foe = k; break; }
    if (foe < 0) return s.last_dir[me];
    int dirs[4], nd = 0;
    for (int d = 0; d < 4; ++d)
        if (can_move(s, foe, d)) dirs[nd++] = d;
    if (nd == 0) return s.last_dir[foe];
    int best = dirs[0], bv = -1 << 30;
    int pm = priority(s, foe), pme = priority(s, me);
    for (int i = 0; i < nd; ++i) {
        int d = dirs[i];
        int nx = s.x[foe] + DX[d], ny = s.y[foe] + DY[d];
        int v = flood_count(s, nx, ny) * 4;
        // 抢占：能抢到我可能走的格则加分
        for (int e = 0; e < 4; ++e)
            if (can_move(s, me, e) && s.x[me] + DX[e] == nx && s.y[me] + DY[e] == ny)
                v += (pm < pme ? 60 : -60);
        if (d == s.last_dir[foe]) v += 1;
        if (v > bv) { bv = v; best = d; }
    }
    return best;
}

// 模型规划：每回合对手按 foe_greedy 走（精确 step，含抢格），我方分支取最大。
struct PCtx { int me, foe; uint64_t nodes; double stop_ms; bool stop; };
int plan_search(State& s, int depth, int alpha, int beta, PCtx& c, uint64_t key,
                int* ptt_val, int8_t* ptt_dep, uint16_t* ptt_key, uint32_t ptt_mask) {
    if ((++c.nodes & 1023) == 0 && budget_left() < c.stop_ms) { c.stop = true; return 0; }
    if (!s.alive[c.me] || !s.alive[c.foe]) return duel_eval(s, c.me, c.foe);
    if (depth <= 0) return duel_eval(s, c.me, c.foe);

    uint32_t slot = (uint32_t)key & ptt_mask;
    if (ptt_key[slot] == (uint16_t)(key >> 48) && ptt_dep[slot] == (int8_t)depth)
        return ptt_val[slot];

    int fd = foe_greedy(s);
    int dirs[4], nd = 0;
    for (int d = 0; d < 4; ++d)
        if (can_move(s, c.me, d)) dirs[nd++] = d;
    int straight = s.last_dir[c.me];
    std::sort(dirs, dirs + nd, [&](int a, int b) {
        return ((a == straight ? 4 : 0) + openness(s, c.me, a)) >
               ((b == straight ? 4 : 0) + openness(s, c.me, b));
    });
    if (nd == 0) dirs[nd++] = straight;

    int moves[MAXN], best = -INF;
    for (int i = 0; i < nd; ++i) {
        State ns = s;
        moves[c.me] = dirs[i];
        moves[c.foe] = fd;
        step(ns, moves);
        uint64_t nk = key ^ HTURN[s.turn & 8191] ^ HTURN[ns.turn & 8191];
        for (int k = 0; k < ns.N; ++k) {
            if (ns.alive[k]) {
                if (ns.x[k] != s.x[k] || ns.y[k] != s.y[k])
                    nk ^= HCELL[cell_idx(ns.x[k], ns.y[k])][k]
                          ^ HX[k][s.x[k]] ^ HX[k][ns.x[k]]
                          ^ HY[k][s.y[k]] ^ HY[k][ns.y[k]];
            } else if (s.alive[k]) nk ^= HALIVE[k];
        }
        int v = plan_search(ns, depth - 1, alpha, beta, c, nk, ptt_val, ptt_dep, ptt_key, ptt_mask);
        if (c.stop) return 0;
        if (v > best) best = v;
        if (v > alpha) alpha = v;
        if (alpha >= beta) break;
    }
    if (ptt_key[slot] != (uint16_t)(key >> 48) || ptt_dep[slot] < depth) {
        ptt_key[slot] = (uint16_t)(key >> 48); ptt_dep[slot] = (int8_t)depth; ptt_val[slot] = best;
    }
    return best;
}

// 前 joint_plies 层用精确联合结算（max 我 / min 对手，双方同时走）；之后转入深度交替搜索。
// joint_plies 在此层走完后减到 0 时调用 ab_search。
int joint_dfs(State& s, int depth, int alpha, int beta, int joint_plies, ACtx& c, uint64_t key) {
    if ((++c.nodes & 1023) == 0 && budget_left() < c.stop_ms) { c.stop = true; return 0; }
    if (!s.alive[c.me] || !s.alive[c.foe]) return duel_eval(s, c.me, c.foe);
    if (joint_plies <= 0)
        return depth <= 0 ? duel_eval(s, c.me, c.foe) : ab_search(s, depth, alpha, beta, 0, c, key);
    if (depth <= 0) return duel_eval(s, c.me, c.foe);

    int md[4], fd[4], nm = 0, nf = 0;
    for (int d = 0; d < 4; ++d) {
        if (can_move(s, c.me, d)) md[nm++] = d;
        if (can_move(s, c.foe, d)) fd[nf++] = d;
    }
    if (nm == 0) md[nm++] = s.last_dir[c.me];
    if (nf == 0) fd[nf++] = s.last_dir[c.foe];
    int msd = s.last_dir[c.me], fsd = s.last_dir[c.foe];
    std::sort(md, md + nm, [&](int a, int b) {
        return ((a == msd ? 4 : 0) + openness(s, c.me, a)) > ((b == msd ? 4 : 0) + openness(s, c.me, b));
    });
    std::sort(fd, fd + nf, [&](int a, int b) {
        return ((a == fsd ? 4 : 0) + openness(s, c.foe, a)) > ((b == fsd ? 4 : 0) + openness(s, c.foe, b));
    });

    int a0 = alpha, b0 = beta;
    int moves[MAXN];
    int best = -INF;
    for (int i = 0; i < nm; ++i) {
        int worst = INF;
        for (int j = 0; j < nf; ++j) {
            State ns = s;
            moves[c.me] = md[i];
            moves[c.foe] = fd[j];
            step(ns, moves);
            uint64_t nk = key ^ HTURN[s.turn & 8191] ^ HTURN[ns.turn & 8191];
            for (int k = 0; k < ns.N; ++k) {
                if (ns.alive[k]) {
                    if (ns.x[k] != s.x[k] || ns.y[k] != s.y[k]) {
                        nk ^= HCELL[cell_idx(ns.x[k], ns.y[k])][k]
                              ^ HX[k][s.x[k]] ^ HX[k][ns.x[k]]
                              ^ HY[k][s.y[k]] ^ HY[k][ns.y[k]];
                    }
                } else if (s.alive[k]) nk ^= HALIVE[k];
            }
            int v;
            if (!ns.alive[c.me] || !ns.alive[c.foe]) v = duel_eval(ns, c.me, c.foe);
            else if (depth - 2 <= 0) v = duel_eval(ns, c.me, c.foe);
            else v = joint_dfs(ns, depth - 2, alpha, beta, joint_plies - 1, c, nk);
            if (c.stop) return 0;
            if (v < worst) worst = v;
        }
        if (worst > best) best = worst;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    (void)a0; (void)b0;
    return best;
}

// 局面分析：独立 flood + 是否被分隔。返回 rme/rfo，separated 表示两人可达区不相交。
void duel_analysis(const State& s, int me, int foe, int& rme, int& rfo, bool& separated) {
    // 我方可达标记
    ++g_stamp_cur;
    int sm = g_stamp_cur;
    static int q[MAXH * MAXW];
    int qt = 0;
    rme = 0;
    for (int d = 0; d < 4; ++d) {
        int nx = s.x[me] + DX[d], ny = s.y[me] + DY[d];
        if (is_free(s, nx, ny) && g_stamp[cell_idx(nx, ny)] != sm) {
            g_stamp[cell_idx(nx, ny)] = sm; q[qt++] = cell_idx(nx, ny); ++rme;
        }
    }
    for (int h = 0; h < qt; ++h) {
        int ci = q[h], cx = ci % MAXW, cy = ci / MAXW;
        for (int d = 0; d < 4; ++d) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (is_free(s, nx, ny) && g_stamp[cell_idx(nx, ny)] != sm) {
                g_stamp[cell_idx(nx, ny)] = sm; q[qt++] = cell_idx(nx, ny); ++rme;
            }
        }
    }
    // 对手可达，用另一个 stamp；遇到我方标记则记相交
    ++g_stamp_cur;
    int sf = g_stamp_cur;
    qt = 0; rfo = 0; separated = true;
    for (int d = 0; d < 4; ++d) {
        int nx = s.x[foe] + DX[d], ny = s.y[foe] + DY[d];
        int ni = cell_idx(nx, ny);
        if (is_free(s, nx, ny) && g_stamp[ni] != sf) {
            if (g_stamp[ni] == sm) separated = false;
            g_stamp[ni] = sf; q[qt++] = ni; ++rfo;
        }
    }
    for (int h = 0; h < qt; ++h) {
        int ci = q[h], cx = ci % MAXW, cy = ci / MAXW;
        for (int d = 0; d < 4; ++d) {
            int nx = cx + DX[d], ny = cy + DY[d];
            int ni = cell_idx(nx, ny);
            if (is_free(s, nx, ny) && g_stamp[ni] != sf) {
                if (g_stamp[ni] == sm) separated = false;
                g_stamp[ni] = sf; q[qt++] = ni; ++rfo;
            }
        }
    }
}

int duel_eval(const State& s, int me, int foe) {
    if (!s.alive[me]) return -200000 + s.turn * 5;
    if (!s.alive[foe]) return 200000 - s.turn * 5;
    int rme, rfo; bool separated;
    duel_analysis(s, me, foe, rme, rfo, separated);
    if (rme == 0) return -120000 + s.turn * 5;
    if (rfo == 0) return 120000 - s.turn * 5;
    int d = rme - rfo;
    if (separated) {
        return (d > 0 ? 60000 : d < 0 ? -60000 : 0) + d * 50;
    }
    long own[MAXN];
    int tie, nearest;
    voronoi(s, own, tie, nearest);
    long vme = own[me] + tie / 2, vfo = own[foe] + tie / 2;
    int v = int(vme - vfo) * 100;
    v += std::max(-40, std::min(40, d)) * 2;
    return v;
}

int duel_root(const State& s0) {
    ACtx c;
    c.me = s0.me;
    c.foe = -1;
    for (int k = 0; k < s0.N; ++k)
        if (s0.alive[k] && k != c.me) c.foe = k;
    c.nodes = 0; c.stop = false;
    c.stop_ms = s0.turn == 0 ? 400.0 : 12.0;

    int dirs[4], nd = 0;
    for (int d = 0; d < 4; ++d)
        if (can_move(s0, c.me, d)) dirs[nd++] = d;
    if (nd == 0) return s0.last_dir[c.me];
    if (nd == 1) return dirs[0];
    int straight = s0.last_dir[c.me];
    std::sort(dirs, dirs + nd, [&](int a, int b) {
        int oa = (a == straight ? 4 : 0) + openness(s0, c.me, a);
        int ob = (b == straight ? 4 : 0) + openness(s0, c.me, b);
        return oa > ob;
    });

    // 根节点过滤：本回合会被更高优先级对手抢占的格（仅近距离时）
    int mh = std::abs(s0.x[c.me] - s0.x[c.foe]) + std::abs(s0.y[c.me] - s0.y[c.foe]);
    if (mh <= 4 && priority(s0, c.foe) < priority(s0, c.me)) {
        bool threatened[MAXH * MAXW] = {};
        for (int d = 0; d < 4; ++d)
            if (can_move(s0, c.foe, d))
                threatened[cell_idx(s0.x[c.foe] + DX[d], s0.y[c.foe] + DY[d])] = true;
        int safe[4], ns2 = 0;
        for (int i = 0; i < nd; ++i)
            if (!threatened[cell_idx(s0.x[c.me] + DX[dirs[i]], s0.y[c.me] + DY[dirs[i]])])
                safe[ns2++] = dirs[i];
        if (ns2 > 0) { std::memcpy(dirs, safe, sizeof(int) * ns2); nd = ns2; }
        if (nd == 1) return dirs[0];
    }

    uint64_t base = state_key(s0);
    int scores[4];
    for (int i = 0; i < nd; ++i) scores[i] = -INF;
    int chosen = dirs[0];

    // 模型规划的置换表（每次 decide 清空）
    static constexpr uint32_t PT_BITS = 19, PT_SIZE = 1u << PT_BITS, PT_MASK = PT_SIZE - 1;
    static int* ptt_val = nullptr; static int8_t* ptt_dep; static uint16_t* ptt_key;
    if (!ptt_val) {
        ptt_val = new int[PT_SIZE]; ptt_dep = new int8_t[PT_SIZE]; ptt_key = new uint16_t[PT_SIZE];
    }
    std::memset(ptt_key, 0, sizeof(uint16_t) * PT_SIZE);
    std::memset(ptt_dep, 0, sizeof(int8_t) * PT_SIZE);

    PCtx pc; pc.me = c.me; pc.foe = c.foe; pc.nodes = 0; pc.stop = false;
    pc.stop_ms = c.stop_ms;
    int max_depth = s0.turn == 0 ? 80 : 60;
    int moves[MAXN];
    for (int depth = 1; depth <= max_depth; ++depth) {
        int local_best_dir = chosen, local_best_val = -INF;
        int alpha = -INF;
        bool aborted = false;
        int fd = foe_greedy(s0);
        for (int i = 0; i < nd; ++i) {
            if (budget_left() < c.stop_ms + 2.0) { aborted = true; break; }
            State ns = s0;
            moves[c.me] = dirs[i];
            moves[c.foe] = fd;
            step(ns, moves);
            if (!ns.alive[c.me]) { scores[i] = -200000 + ns.turn * 5; continue; }
            uint64_t nk = base ^ HTURN[s0.turn & 8191] ^ HTURN[ns.turn & 8191];
            for (int k = 0; k < ns.N; ++k) {
                if (ns.alive[k]) {
                    if (ns.x[k] != s0.x[k] || ns.y[k] != s0.y[k])
                        nk ^= HCELL[cell_idx(ns.x[k], ns.y[k])][k]
                              ^ HX[k][s0.x[k]] ^ HX[k][ns.x[k]]
                              ^ HY[k][s0.y[k]] ^ HY[k][ns.y[k]];
                } else nk ^= HALIVE[k];
            }
            int v = ns.alive[c.foe]
                        ? plan_search(ns, depth - 1, -INF, INF, pc, nk,
                                      ptt_val, ptt_dep, ptt_key, PT_MASK)
                        : (200000 - ns.turn * 5);
            if (pc.stop) { aborted = true; break; }
            scores[i] = v;
            if (v > local_best_val) { local_best_val = v; local_best_dir = dirs[i]; }
            if (v > alpha) alpha = v;
        }
        if (aborted) break;
        chosen = local_best_dir;
        for (int i = 1; i < nd; ++i) {
            int sd = dirs[i], sv = scores[i], j = i - 1;
            while (j >= 0 && scores[j] < sv) {
                dirs[j + 1] = dirs[j]; scores[j + 1] = scores[j]; --j;
            }
            dirs[j + 1] = sd; scores[j + 1] = sv;
        }
        if (local_best_val > 100000 || local_best_val < -100000) break;
        double need = s0.turn == 0 ? 600.0 : 20.0;
        if (budget_left() < need) break;
        g_last_depth = depth;
    }
    if (s0.turn < 6 || (s0.turn & 15) == 0)
        fprintf(stderr, "1v1 t=%d depth=%d nodes=%llu left=%.0f d=%d\n",
                s0.turn, g_last_depth, (unsigned long long)c.nodes, budget_left(), chosen);
    return chosen;
}

// ============================================================ 多人束搜索
struct BNode {
    State st;
    int root_dir;
    double score;
};

// 对手策略：近邻（曼哈顿≤9）精确 flood，远邻看局部开阔度；惩罚贴活人头；直行平局偏好。
int opp_policy(const State& s, int k) {
    int dirs[4], nd = 0;
    for (int d = 0; d < 4; ++d)
        if (can_move(s, k, d)) dirs[nd++] = d;
    if (nd == 0) return s.last_dir[k];
    int mdist = std::abs(s.x[k] - s.x[s.me]) + std::abs(s.y[k] - s.y[s.me]);
    bool near = mdist <= 9;
    int best = dirs[0], bestv = -1000000;
    for (int i = 0; i < nd; ++i) {
        int d = dirs[i];
        int nx = s.x[k] + DX[d], ny = s.y[k] + DY[d];
        int v;
        if (near) v = flood_count(s, nx, ny) * 4;
        else {
            v = 0;
            for (int e = 0; e < 4; ++e)
                if (is_free(s, nx + DX[e], ny + DY[e])) v += 5;
        }
        for (int e = 0; e < 4; ++e) {
            int ax = nx + DX[e], ay = ny + DY[e];
            if (!inside(s, ax, ay)) continue;
            int cc = s.cell[ay][ax];
            if (cc >= 0 && cc != k && s.alive[cc]) v -= 40;
        }
        if (d == s.last_dir[k]) v += 1;
        if (v > bestv) { bestv = v; best = d; }
    }
    return best;
}

double ffa_eval(const State& s) {
    if (!s.alive[s.me]) return -1000000.0 + s.turn;
    long own[MAXN];
    int tie, nearest;
    voronoi(s, own, tie, nearest);
    long myspace = own[s.me];
    if (myspace == 0) {
        int fc = flood_count(s, s.x[s.me], s.y[s.me]);
        if (fc == 0) return -500000.0 + s.turn;
        myspace = fc;
    }
    long best_other = 0;
    int alive_n = 0;
    for (int k = 0; k < s.N; ++k)
        if (s.alive[k]) {
            ++alive_n;
            if (k != s.me) best_other = std::max(best_other, own[k]);
        }
    double v = myspace * 10.0;
    v += double(myspace - best_other) * 1.5;
    v += std::min(nearest, 12) * 6.0;
    v += (s.N - alive_n) * 25.0;
    return v;
}

int ffa_root(const State& s0) {
    const int me = s0.me;
    int dirs[4], nd = 0;
    for (int d = 0; d < 4; ++d)
        if (can_move(s0, me, d)) dirs[nd++] = d;
    if (nd == 0) return s0.last_dir[me];
    if (nd == 1) return dirs[0];

    // 根节点过滤：本回合会被更高优先级对手抢占的格
    static bool threatened[MAXH * MAXW];
    std::memset(threatened, 0, sizeof(bool) * s0.H * MAXW);
    int pm = priority(s0, me);
    for (int k = 0; k < s0.N; ++k) {
        if (!s0.alive[k] || k == me || priority(s0, k) >= pm) continue;
        for (int d = 0; d < 4; ++d)
            if (can_move(s0, k, d))
                threatened[cell_idx(s0.x[k] + DX[d], s0.y[k] + DY[d])] = true;
    }
    int safe[4], nsafe = 0;
    for (int i = 0; i < nd; ++i)
        if (!threatened[cell_idx(s0.x[me] + DX[dirs[i]], s0.y[me] + DY[dirs[i]])])
            safe[nsafe++] = dirs[i];
    if (nsafe > 0) { std::memcpy(dirs, safe, sizeof(int) * nsafe); nd = nsafe; }

    int straight = s0.last_dir[me];
    std::sort(dirs, dirs + nd, [&](int a, int b) {
        int oa = (a == straight ? 4 : 0) + openness(s0, me, a);
        int ob = (b == straight ? 4 : 0) + openness(s0, me, b);
        return oa > ob;
    });

    const int K = s0.N >= 8 ? 10 : 16;
    const int MAXD = s0.turn == 0 ? 24 : 22;
    const double stop = s0.turn == 0 ? 400.0 : 13.0;

    std::vector<BNode> beam, nxt;
    beam.reserve(64);
    nxt.reserve(80);
    double root_best[4];
    for (int i = 0; i < nd; ++i) root_best[i] = -1e18;

    int moves[MAXN];
    for (int i = 0; i < nd; ++i) {
        BNode bn;
        bn.st = s0;
        for (int k = 0; k < s0.N; ++k) moves[k] = k == me ? dirs[i] : s0.last_dir[k];
        step(bn.st, moves);
        bn.root_dir = dirs[i];
        bn.score = ffa_eval(bn.st);
        beam.push_back(std::move(bn));
        root_best[i] = beam.back().score;
    }

    int chosen = dirs[0];
    double chosen_score = root_best[0];
    for (int i = 1; i < nd; ++i)
        if (root_best[i] > chosen_score) { chosen_score = root_best[i]; chosen = dirs[i]; }

    for (int depth = 2; depth <= MAXD; ++depth) {
        nxt.clear();
        for (const BNode& bn : beam) {
            const State& s = bn.st;
            if (!s.alive[me] || s.over) { nxt.push_back(bn); continue; }
            int pol[MAXN];
            for (int k = 0; k < s.N; ++k)
                pol[k] = (s.alive[k] && k != me) ? opp_policy(s, k) : s.last_dir[k];

            int md[4], m = 0;
            for (int d = 0; d < 4; ++d)
                if (can_move(s, me, d)) md[m++] = d;
            if (m == 0) md[m++] = s.last_dir[me];
            for (int i = 0; i < m; ++i) {
                BNode c2;
                c2.st = s;
                for (int k = 0; k < s.N; ++k) moves[k] = k == me ? md[i] : pol[k];
                step(c2.st, moves);
                c2.root_dir = bn.root_dir;
                c2.score = ffa_eval(c2.st);
                nxt.push_back(std::move(c2));
            }
        }
        std::sort(nxt.begin(), nxt.end(), [](const BNode& a, const BNode& b) { return a.score > b.score; });
        int keep = std::min(K, (int)nxt.size());
        beam.clear();
        for (int i = 0; i < keep; ++i) beam.push_back(std::move(nxt[i]));

        for (int i = 0; i < nd; ++i) root_best[i] = -1e18;
        for (const BNode& bn : beam)
            for (int i = 0; i < nd; ++i)
                if (dirs[i] == bn.root_dir) root_best[i] = std::max(root_best[i], bn.score);
        int bd = chosen; double bs = -1e18;
        for (int i = 0; i < nd; ++i)
            if (root_best[i] > bs) { bs = root_best[i]; bd = dirs[i]; }
        chosen = bd;

        bool any_alive = false;
        for (const BNode& bn : beam)
            if (bn.st.alive[me] && !bn.st.over) { any_alive = true; break; }
        if (!any_alive) break;
        if (budget_left() < stop) break;
        if (bs < -200000) break;
    }
    return chosen;
}

}  // namespace

int decide(const tron::State& s) {
    if (!hash_ready) {
        hash_init();
        if (!tt) {
            tt = (TTEntry*)std::malloc(size_t(TT_SIZE) * sizeof(TTEntry));
            std::memset(tt, 0, size_t(TT_SIZE) * sizeof(TTEntry));
        }
    }
    int alive_foes = 0;
    for (int k = 0; k < s.N; ++k)
        if (s.alive[k] && k != s.me) ++alive_foes;
    if (alive_foes == 0) {
        for (int d = 0; d < 4; ++d)
            if (can_move(s, s.me, d)) return d;
        return s.last_dir[s.me];
    }
    int r;
    if (alive_foes == 1) r = duel_root(s);
    else r = ffa_root(s);
    if (r < 0 || r > 3 || !can_move(s, s.me, r)) {
        for (int d = 0; d < 4; ++d)
            if (can_move(s, s.me, d)) return d;
        return s.last_dir[s.me];
    }
    return r;
}
