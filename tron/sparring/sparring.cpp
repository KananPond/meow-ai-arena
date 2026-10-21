// sparring.cpp —— 陪练（不进选手包，只发编译好的程序）。逐行移植自原型 bots.py：
//   2 人：vor1_eg   1 层同时行动极小极大（我先选、对手看到再选，偏悲观），Voronoi 地盘差评估；
//                   与对手分离后改为终局填格搜索。
//   多人：vor4_eg   对每个方向算多人 Voronoi（对手原地），避开对手下一步可能进的格；
//                   与所有对手分离后改为终局填格搜索。
// 终局填格：迭代加深穷举，叶子用贴墙走到底（Warnsdorff）的步数，固定 3000 个节点，不看时间，结果可复现。
// 平手时的方向顺序与原型一致：先直行，再按 上、右、下、左。
#include "tron.hpp"

#include <vector>

namespace {

constexpr int INF = 1000000000;
constexpr int WIN = 1000000;
constexpr int EG_BUDGET = 3000;

int W, H;
std::vector<char> F;    // 空地（不含任何人的头部）

struct Timeout {};

inline int tgt(int p, int d) {    // 走一步后的格子编号，出界为 -1
    const int x = p % W + tron::DX[d], y = p / W + tron::DY[d];
    return (x < 0 || x >= W || y < 0 || y >= H) ? -1 : y * W + x;
}
inline bool fr(const std::vector<char>& f, int t) { return t >= 0 && f[size_t(t)]; }

// 从 p 出发（p 本身不在 f 里）能到达的格子
int flood(int p, const std::vector<char>& f, std::vector<char>& reg) {
    reg.assign(f.size(), 0);
    std::vector<int> q{p};
    int n = 0;
    for (size_t h = 0; h < q.size(); ++h)
        for (int d = 0; d < 4; ++d) {
            const int t = tgt(q[h], d);
            if (fr(f, t) && !reg[size_t(t)]) {
                reg[size_t(t)] = 1;
                ++n;
                q.push_back(t);
            }
        }
    return n;
}

int fill_bound(int p, const std::vector<char>& f) {
    std::vector<char> reg;
    if (!flood(p, f, reg)) return 0;
    int e = 0, o = 0;
    for (size_t i = 0; i < reg.size(); ++i)
        if (reg[i]) (((i % size_t(W)) + (i / size_t(W))) % 2 == 0 ? e : o)++;
    const bool pe = ((p % W) + (p / W)) % 2 == 0;
    const int first = pe ? o : e, second = pe ? e : o;
    return first > second ? 2 * second + 1 : 2 * first;
}

// a 能到达的区域里有没有挨着 others 中某个头部的格子
bool touches(int a, const std::vector<int>& others, const std::vector<char>& f) {
    std::vector<char> reg;
    flood(a, f, reg);
    for (int b : others)
        for (int d = 0; d < 4; ++d) {
            const int t = tgt(b, d);
            if (t >= 0 && reg[size_t(t)]) return true;
        }
    return false;
}

// 多人 Voronoi：每人严格先到的格子数（同时到达的不算，也不再向外扩）
std::vector<int> voronoi(const std::vector<int>& heads, const std::vector<char>& f) {
    const size_t n = heads.size();
    std::vector<char> seen(f.size(), 0);
    std::vector<std::vector<int>> front(n);
    for (size_t k = 0; k < n; ++k) {
        front[k] = {heads[k]};
        seen[size_t(heads[k])] = 1;
    }
    std::vector<int> cnt(n, 0);
    std::vector<int> owner(f.size(), -1);
    for (;;) {
        std::vector<std::vector<int>> nf(n);
        std::vector<int> touched;
        for (size_t k = 0; k < n; ++k)
            for (int p : front[k])
                for (int d = 0; d < 4; ++d) {
                    const int t = tgt(p, d);
                    if (!fr(f, t) || seen[size_t(t)]) continue;
                    if (owner[size_t(t)] == -1) {
                        owner[size_t(t)] = int(k);
                        touched.push_back(t);
                    } else if (owner[size_t(t)] != int(k)) owner[size_t(t)] = -2;
                }
        if (touched.empty()) break;
        for (int t : touched) {
            seen[size_t(t)] = 1;
            if (owner[size_t(t)] >= 0) {
                nf[size_t(owner[size_t(t)])].push_back(t);
                ++cnt[size_t(owner[size_t(t)])];
            }
            owner[size_t(t)] = -1;
        }
        front.swap(nf);
    }
    return cnt;
}

// ---------------------------------------------------------------- 终局填格
long nodes, limit;
bool exhausted;

int rollout(int p, std::vector<char> f) {
    int steps = 0;
    for (;;) {
        int best_t = -1, best_deg = 9;
        for (int d = 0; d < 4; ++d) {
            const int t = tgt(p, d);
            if (!fr(f, t)) continue;
            f[size_t(t)] = 0;
            int deg = 0;
            for (int e = 0; e < 4; ++e) deg += fr(f, tgt(t, e));
            f[size_t(t)] = 1;
            if (deg == 0) deg = 5;
            if (deg < best_deg) best_t = t, best_deg = deg;
        }
        if (best_t < 0) break;
        p = best_t;
        f[size_t(p)] = 0;
        ++steps;
    }
    nodes += 1 + steps / 4;
    return steps;
}

int fill_dfs(int p, std::vector<char>& f, int depth) {
    if (++nodes > limit) throw Timeout{};
    if (depth == 0) {
        exhausted = false;
        return rollout(p, f);
    }
    int best = 0;
    const int ub = fill_bound(p, f);
    for (int d = 0; d < 4; ++d) {
        const int t = tgt(p, d);
        if (!fr(f, t)) continue;
        f[size_t(t)] = 0;
        const int v = 1 + fill_dfs(t, f, depth - 1);
        f[size_t(t)] = 1;
        if (v > best) {
            best = v;
            if (best >= ub) break;
        }
    }
    return best;
}

std::vector<int> order_straight(int last) {
    std::vector<int> o{last};
    for (int d = 0; d < 4; ++d)
        if (d != last) o.push_back(d);
    return o;
}

int fill_move(int a, int last) {
    std::vector<int> moves;
    for (int d : order_straight(last))
        if (fr(F, tgt(a, d))) moves.push_back(d);
    if (moves.empty()) return last;
    if (moves.size() == 1) return moves[0];
    nodes = 0;
    limit = EG_BUDGET;
    int best_d = moves[0], best_v = -1;
    const int ub_total = fill_bound(a, F);
    for (int depth = 1; depth < 200; ++depth) {
        exhausted = true;
        try {
            int cur_d = -1, cur_v = -1;
            for (int d : moves) {
                const int t = tgt(a, d);
                F[size_t(t)] = 0;
                int v;
                try {
                    v = 1 + fill_dfs(t, F, depth - 1);
                } catch (Timeout&) {
                    F[size_t(t)] = 1;
                    throw;
                }
                F[size_t(t)] = 1;
                if (v > cur_v) cur_v = v, cur_d = d;
            }
            best_d = cur_d;
            best_v = cur_v;
            std::vector<int> re{best_d};
            for (int m : moves)
                if (m != best_d) re.push_back(m);
            moves = re;
        } catch (Timeout&) {
            break;
        }
        if (best_v >= ub_total || exhausted) break;
    }
    return best_d;
}

// ---------------------------------------------------------------- 2 人：vor1_eg
int g_me, g_turn;

int evaluate(int a, int b, const std::vector<char>& f) {
    if (!touches(a, {b}, f)) {
        const int fa = fill_bound(a, f), fb = fill_bound(b, f);
        if (fa > fb) return WIN / 2 + (fa - fb);
        if (fa < fb) return -WIN / 2 + (fa - fb);
        return 0;
    }
    auto c = voronoi({a, b}, f);
    return c[0] - c[1];
}

int vor1(const tron::State& s) {
    const int me = s.me, op = 1 - me, last = s.last_dir[me];
    const int a = s.y[me] * W + s.x[me], b = s.y[op] * W + s.x[op];
    if (!touches(a, {b}, F)) return fill_move(a, last);
    std::vector<int> mine, ops;
    for (int d : order_straight(last))
        if (fr(F, tgt(a, d))) mine.push_back(d);
    for (int d = 0; d < 4; ++d)
        if (fr(F, tgt(b, d))) ops.push_back(d);
    if (ops.empty()) ops.push_back(tron::U);
    if (mine.empty()) return last;
    int best = -INF, bestd = mine[0];
    for (int da : mine) {
        int worst = INF;
        for (int db : ops) {
            const int ta = tgt(a, da), tb = tgt(b, db);
            bool deadA = !fr(F, ta), deadB = !fr(F, tb);
            if (ta == tb && !deadA) {
                if (g_turn % 2 == me) deadB = true;
                else deadA = true;
            }
            int v;
            if (deadA || deadB) v = (deadA && deadB) ? 0 : deadA ? -WIN : WIN;
            else {
                F[size_t(ta)] = 0;
                F[size_t(tb)] = 0;
                v = evaluate(ta, tb, F);
                F[size_t(ta)] = 1;
                F[size_t(tb)] = 1;
            }
            if (v < worst) worst = v;
            if (worst <= best) break;
        }
        if (worst > best) best = worst, bestd = da;
    }
    return bestd;
}

// ---------------------------------------------------------------- 多人：vor4_eg
int vor4(const tron::State& s) {
    const int me = s.me, last = s.last_dir[me];
    const int a = s.y[me] * W + s.x[me];
    std::vector<int> others;
    for (int k = 0; k < s.N; ++k)
        if (k != me && s.alive[k]) others.push_back(s.y[k] * W + s.x[k]);
    if (!touches(a, others, F)) return fill_move(a, last);
    std::vector<char> danger(F.size(), 0);
    for (int o : others)
        for (int d = 0; d < 4; ++d) {
            const int t = tgt(o, d);
            if (fr(F, t)) danger[size_t(t)] = 1;
        }
    bool have = false;
    int bd = last, kr = 0, ks = 0, kreach = 0;
    std::vector<char> reg;
    for (int d : order_straight(last)) {
        const int t = tgt(a, d);
        if (!fr(F, t)) continue;
        F[size_t(t)] = 0;
        std::vector<int> heads{t};
        heads.insert(heads.end(), others.begin(), others.end());
        const auto cnt = voronoi(heads, F);
        const int reach = flood(t, F, reg);
        F[size_t(t)] = 1;
        const int score = reach > 0 ? cnt[0] : -1, r = -int(danger[size_t(t)]);
        if (!have || r > kr || (r == kr && (score > ks || (score == ks && reach > kreach)))) {
            have = true;
            bd = d, kr = r, ks = score, kreach = reach;
        }
    }
    return bd;
}

}  // namespace

int decide(const tron::State& s) {
    W = s.W;
    H = s.H;
    g_me = s.me;
    g_turn = s.turn;
    F.assign(size_t(W * H), 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) F[size_t(y * W + x)] = s.cell[y][x] == tron::EMPTY;
    return s.N == 2 ? vor1(s) : vor4(s);
}
