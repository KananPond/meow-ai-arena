// 光轮对战（Tron）提交 bot
//
// 策略要点：
//  1. 安全优先：绝不选会立刻自杀的 move（除非所有 move 都死，且能拉一个垫背 = 并列）；
//  2. 面积保全：主指标是从我头部出发的可达空地（flood fill），加上"出口数"（下一步还有几路可走）；
//  3. 战术击杀：对手被逼（只剩 1 个合法方向）且本回合优先级比我高时，抢占他唯一的出口 → 对手死；
//     反过来，若对手优先级更高且可能与我同抢一格，避开该格；
//  4. 对抗前瞻：对手按"不自杀 / 最大化面积 / 能杀我则杀我"模拟（每人取前 1~2 个候选方向），
//     我的根 move 按对手所有响应组合下的最差局面取（minimax，深度 1~3，自适应宽度）；
//  5. 残局（存活 ≤ 4）：alpha-beta 搜索强制击杀序列（mate-in-k），含拉人垫背的止损选择。
//
// 时间管理：tron::budget_left() 硬停 + 节点数上限；任何情况下都保证返回一个当前合法的 move。

#include "tron.hpp"

#include <climits>
#include <cstdint>
#include <cstring>
#include <functional>

namespace {

constexpr int PW = 43, PH = 43, PC = PW * PH;  // 带 1 格障碍边框的棋盘（41+2）
constexpr int KILLV = 10000;

struct W {
    int W_ = 0, H_ = 0, N = 0, turn = 0, me = 0;
    int8_t c[PC];
    int px[12], py[12];
    uint8_t alive[12], last[12];
};

W g;
uint16_t vis[PC];
uint16_t stamp = 1;
int qb[PC];

inline int idxx(int x, int y) { return (y + 1) * PW + (x + 1); }
inline int prio(const W& w, int k) { return ((k - w.turn) % w.N + w.N) % w.N; }

inline int alive_count(const W& w) {
    int c = 0;
    for (int k = 0; k < w.N; ++k) c += w.alive[k];
    return c;
}

void load(const tron::State& s) {
    g.W_ = s.W; g.H_ = s.H; g.N = s.N; g.turn = s.turn; g.me = s.me;
    std::memset(g.c, tron::WALL, sizeof g.c);
    for (int y = 0; y < s.H; ++y)
        for (int x = 0; x < s.W; ++x) g.c[idxx(x, y)] = s.cell[y][x];
    for (int k = 0; k < s.N; ++k) {
        g.px[k] = s.x[k]; g.py[k] = s.y[k];
        g.alive[k] = s.alive[k]; g.last[k] = s.last_dir[k];
    }
}

// 区域分析：从 (sx,sy)（该格当墙）出发的连通空地。
//   shape   宽处（≥2 空邻居）每格 3 分、窄道每格 1 分
//   raw     格数
//   opp_adj 区域内格子与对手轨迹相邻的次数（区域被对手切得多碎）
struct AreaInfo { int shape = 0, raw = 0, opp_adj = 0; };
static AreaInfo last_area;
AreaInfo area_full(const W& w, int sx, int sy) {
    if (++stamp == 0) { std::memset(vis, 0, sizeof vis); stamp = 1; }
    int h = 0, t = 0, s0 = idxx(sx, sy);
    vis[s0] = stamp; qb[t++] = s0;
    AreaInfo ai;
    while (h < t) {
        int p = qb[h++];
        int np[4] = {p - PW, p + PW, p - 1, p + 1};
        if (w.c[p] == tron::EMPTY) {  // 起点（我的头）当墙：不计数，照常扩散
            int open = 0, oa = 0;
            for (int i = 0; i < 4; ++i) {
                int8_t c = w.c[np[i]];
                if (c == tron::EMPTY) ++open;
                else if (c >= 0 && c != w.me) ++oa;
            }
            ++ai.raw;
            ai.shape += (open >= 2) ? 3 : 1;
            ai.opp_adj += oa;
        }
        for (int i = 0; i < 4; ++i)
            if (w.c[np[i]] == tron::EMPTY && vis[np[i]] != stamp) { vis[np[i]] = stamp; qb[t++] = np[i]; }
    }
    last_area = ai;
    return ai;
}
int area_shape(const W& w, int sx, int sy) { return area_full(w, sx, sy).shape; }

// 活着的对手里，头挨着我区域（vis 必须是 area_full 刚打的戳）的个数
int opps_in_region(const W& w) {
    int c = 0;
    for (int k = 0; k < w.N; ++k) {
        if (k == w.me || !w.alive[k]) continue;
        int p = idxx(w.px[k], w.py[k]);
        int np[4] = {p - PW, p + PW, p - 1, p + 1};
        for (int i = 0; i < 4; ++i)
            if (w.c[np[i]] == tron::EMPTY && vis[np[i]] == stamp) { ++c; break; }
    }
    return c;
}

// (x,y) 周围 9×9 的空地数（便宜的空间代理）
int space9(const W& w, int x, int y) {
    int c = 0;
    for (int dy = -4; dy <= 4; ++dy)
        for (int dx = -4; dx <= 4; ++dx) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= w.W_ || ny < 0 || ny >= w.H_) continue;
            if (w.c[idxx(nx, ny)] == tron::EMPTY) ++c;
        }
    return c;
}

inline int legal_moves(const W& w, int k, int out[4]) {
    int n = 0;
    for (int d = 0; d < 4; ++d) {
        int nx = w.px[k] + tron::DX[d], ny = w.py[k] + tron::DY[d];
        if (w.c[idxx(nx, ny)] == tron::EMPTY) out[n++] = d;
    }
    return n;
}

// 与 tron::step 完全一致的结算（在 W 上）
void step_wk(W& w, const int mv[12], int8_t why[12]) {
    int tx[12], ty[12];
    for (int k = 0; k < w.N; ++k) {
        why[k] = 0;
        if (!w.alive[k]) continue;
        int d = mv[k];
        if (d < 0 || d > 3) d = w.last[k];
        w.last[k] = d;
        int nx = w.px[k] + tron::DX[d], ny = w.py[k] + tron::DY[d];
        int8_t c = w.c[idxx(nx, ny)];
        if (nx < 0 || nx >= w.W_ || ny < 0 || ny >= w.H_) why[k] = 1;
        else if (c == tron::WALL) why[k] = 2;
        else if (c == k) why[k] = 3;
        else if (c >= 0) why[k] = 4;
        else { tx[k] = nx; ty[k] = ny; }
    }
    for (int k = 0; k < w.N; ++k) {
        if (!w.alive[k] || why[k]) continue;
        int best = k;
        for (int j = k + 1; j < w.N; ++j) {
            if (!w.alive[j] || why[j] || tx[j] != tx[k] || ty[j] != ty[k]) continue;
            if (prio(w, j) < prio(w, best)) best = j;
        }
        if (best != k) why[k] = 5;
    }
    for (int k = 0; k < w.N; ++k) {
        if (!w.alive[k]) continue;
        if (why[k]) w.alive[k] = 0;
        else { w.px[k] = tx[k]; w.py[k] = ty[k]; w.c[idxx(tx[k], ty[k])] = (int8_t)k; }
    }
    ++w.turn;
}

// ---------------------------------------------------------------- 对手模型
// 对手走方向 d 的评分（假设我的目标格是 mx,my）：不自杀、保面积、能抢杀我则抢杀
int opp_score(const W& w, int k, int d, int mx, int my) {
    int nx = w.px[k] + tron::DX[d], ny = w.py[k] + tron::DY[d];
    if (w.c[idxx(nx, ny)] != tron::EMPTY) return -1000000;  // 自杀
    int sc = space9(w, nx, ny);
    int na = 0;
    for (int d2 = 0; d2 < 4; ++d2)
        if (w.c[idxx(nx + tron::DX[d2], ny + tron::DY[d2])] == tron::EMPTY) ++na;
    sc += 40 * (na >= 2) + 20 * (na >= 3) - 200 * (na == 0);
    if (nx == mx && ny == my)
        sc += (prio(w, k) < prio(w, w.me)) ? 4000 : -1000000;  // 抢杀我 / 自己死
    return sc;
}

// 对手 k 的候选方向（前 2 个）
struct Cands {
    int cand[12][4];
    uint8_t ncand[12];
    int oplist[12], na;
    int product;
};

void build_cands(const W& w, int mx, int my, Cands& cd, int cap) {
    cd.na = 0;
    cd.product = 1;
    for (int k = 0; k < w.N; ++k) {
        cd.ncand[k] = 0;
        if (k == w.me || !w.alive[k]) continue;
        cd.oplist[cd.na++] = k;
        int lm[4]; int n = legal_moves(w, k, lm);
        if (n == 0) { cd.cand[k][0] = w.last[k]; cd.ncand[k] = 1; cd.product = 1; continue; }
        int best[4] = {-1, -1, -1, -1}, bs[4] = {INT_MIN, INT_MIN, INT_MIN, INT_MIN};
        for (int i = 0; i < n; ++i) {
            int d = lm[i];
            int sc = opp_score(w, k, d, mx, my);
            for (int r = 0; r < 4; ++r)
                if (sc > bs[r]) {
                    for (int r2 = 3; r2 > r; --r2) { bs[r2] = bs[r2 - 1]; best[r2] = best[r2 - 1]; }
                    bs[r] = sc; best[r] = d;
                    break;
                }
        }
        bool solo_opp = (alive_count(w) == 2);
        cd.cand[k][0] = best[0];
        int nc = 1;
        if (best[1] >= 0 && bs[1] > -500000) {
            int dx = w.px[k] - w.px[w.me], dy = w.py[k] - w.py[w.me];
            bool danger = (dx < 0 ? -dx : dx) <= 6 && (dy < 0 ? -dy : dy) <= 6;
            for (int i = 0; i < n; ++i)
                if (w.px[k] + tron::DX[lm[i]] == mx && w.py[k] + tron::DY[lm[i]] == my) danger = true;
            if (solo_opp) { cd.cand[k][1] = best[1]; nc = 2; }
            else if (danger) { cd.cand[k][1] = best[1]; nc = 2; }
        }
        if (solo_opp) {
            // 1v1：对手所有非自杀方向都要考虑（对方可能在用牺牲空间的方式逼角）
            if (best[2] >= 0 && bs[2] > -500000) { cd.cand[k][2] = best[2]; nc = 3; }
            if (best[3] >= 0 && bs[3] > -500000) { cd.cand[k][3] = best[3]; nc = 4; }
        }
        cd.ncand[k] = (uint8_t)nc;
        cd.product *= nc;
    }
    // 控制组合数：把最远的"双候选"对手降为单候选
    while (cd.product > cap) {
        int far = -1, fd = -1;
        for (int i = 0; i < cd.na; ++i) {
            int k = cd.oplist[i];
            if (cd.ncand[k] < 2) continue;
            int dx = w.px[k] - w.px[w.me], dy = w.py[k] - w.py[w.me];
            int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            if (dist > fd) { fd = dist; far = i; }
        }
        if (far < 0) break;
        cd.product /= cd.ncand[cd.oplist[far]];
        cd.ncand[cd.oplist[far]] = 1;
    }
}

// 对手响应组合（每个对手取 cand 里的第 sel 个）
struct Combo { uint8_t sel[12]; };

void gen_combos(const Cands& cd, int cap, Combo out[]) {
    int n = 0;
    int sel[12] = {};
    // 递归展开（na ≤ 11，深度可控）
    std::function<void(int)> rec = [&](int i) {
        if (n >= cap) return;
        if (i == cd.na) {
            for (int t = 0; t < cd.na; ++t) out[n].sel[t] = (uint8_t)sel[t];
            ++n;
            return;
        }
        int k = cd.oplist[i];
        for (int s = 0; s < cd.ncand[k]; ++s) {
            sel[i] = s;
            rec(i + 1);
            if (n >= cap) return;
        }
    };
    rec(0);
}

int combo_count(const Cands& cd, int cap) {
    int p = 1;
    for (int i = 0; i < cd.na; ++i) p *= cd.ncand[cd.oplist[i]];
    return p > cap ? cap : p;
}

// ---------------------------------------------------------------- 局面评估
// 拥塞：附近对手头与对手轨迹的密度（12 人局里大家挤在一起就是危险源）
int congestion(const W& w, int cx, int cy) {
    int sc = 0;
    for (int k = 0; k < w.N; ++k) {
        if (k == w.me || !w.alive[k]) continue;
        int dx = w.px[k] - cx, dy = w.py[k] - cy;
        if ((dx < 0 ? -dx : dx) <= 5 && (dy < 0 ? -dy : dy) <= 5) sc += 100;
    }
    int trail = 0;
    for (int dy = -6; dy <= 6 && trail < 120; ++dy)
        for (int dx = -6; dx <= 6; ++dx) {
            int nx = cx + dx, ny = cy + dy;
            if (nx < 0 || nx >= w.W_ || ny < 0 || ny >= w.H_) continue;
            int8_t c = w.c[idxx(nx, ny)];
            if (c >= 0 && c != w.me) ++trail;
        }
    sc += trail * 2;
    return sc > 400 ? 400 : sc;
}

// 轮到"我"时的局面价值（残局 hunt 用更粗的终端值，见 hunt）
int eval_leaf(const W& w) {
    int lm[4]; int n = legal_moves(w, w.me, lm);
    if (n == 0) return -5000;  // 下回合必死
    int mx = w.px[w.me], my = w.py[w.me];
    AreaInfo ai = area_full(w, mx, my);
    int sc = ai.shape - 2 * ai.opp_adj + 40 * (n >= 2) + 20 * (n >= 3) - 250 * (n == 1)
             - congestion(w, mx, my);
    int pm = prio(w, w.me);
    // 威胁：我的哪些出口会被"优先级高于我"的对手同抢
    int threat = 0;
    for (int i = 0; i < n; ++i) {
        int tx = mx + tron::DX[lm[i]], ty = my + tron::DY[lm[i]];
        for (int k = 0; k < w.N; ++k) {
            if (k == w.me || !w.alive[k] || prio(w, k) >= pm) continue;
            int ol[4]; int on = legal_moves(w, k, ol);
            for (int j = 0; j < on; ++j) {
                if (w.px[k] + tron::DX[ol[j]] == tx && w.py[k] + tron::DY[ol[j]] == ty) { threat += 120; break; }
            }
        }
    }
    sc -= threat;
    // 机会：对手被逼、我占优
    for (int k = 0; k < w.N; ++k) {
        if (k == w.me || !w.alive[k]) continue;
        int ol[4]; int on = legal_moves(w, k, ol);
        if (on == 0) continue;
        if (on == 1) {
            int cx = w.px[k] + tron::DX[ol[0]], cy = w.py[k] + tron::DY[ol[0]];
            if (cx == mx && cy == my) { sc += 150; continue; }  // 他唯一的路撞我的头 → 白送
            if (prio(w, k) > pm) {
                if (std::abs(cx - mx) + std::abs(cy - my) == 1) sc += 400;  // 下回合可杀
                else sc += 60;
            }
        } else if (alive_count(w) == 2 && on == 2) {
            sc += 60;  // 1v1：把对手逼到只剩两条路，离逼杀更近
        }
    }
    return sc;
}

// 自我延续：从 w 出发模拟 steps 个完整回合。每回合对手走 top-1（opp_score），
// 我走 space9+出口数 最大的方向（便宜代理）。返回最后一步后的形状分；我死则 INT_MIN。
// 用途：区分"同一连通区里不同方向"的长期价值 —— 静态面积分做不到这一点。
int self_cont(W w, int steps) {
    for (int s = 0; s < steps; ++s) {
        int lm[4]; int n = legal_moves(w, w.me, lm);
        if (n == 0) return INT_MIN;
        int best = -1, bs = INT_MIN;
        W best_w;
        for (int i = 0; i < n; ++i) {
            int d = lm[i];
            W w2 = w;
            int mv[12];
            int mx = w2.px[w2.me] + tron::DX[d], my = w2.py[w2.me] + tron::DY[d];
            for (int k = 0; k < w2.N; ++k) {
                if (k == w2.me) { mv[k] = d; continue; }
                if (!w2.alive[k]) { mv[k] = w2.last[k]; continue; }
                int ol[4]; int on = legal_moves(w2, k, ol);
                if (on == 0) { mv[k] = w2.last[k]; continue; }
                int b = ol[0], bsc = INT_MIN;
                for (int j = 0; j < on; ++j) {
                    int sc = opp_score(w2, k, ol[j], mx, my);
                    if (sc > bsc) { bsc = sc; b = ol[j]; }
                }
                mv[k] = b;
            }
            int8_t why[12];
            step_wk(w2, mv, why);
            if (!w2.alive[w2.me]) continue;
            int nx = w2.px[w2.me], ny = w2.py[w2.me];
            int sc = space9(w2, nx, ny);
            int na = 0;
            for (int d2 = 0; d2 < 4; ++d2)
                if (w2.c[idxx(nx + tron::DX[d2], ny + tron::DY[d2])] == tron::EMPTY) ++na;
            sc += 40 * (na >= 2) + 20 * (na >= 3) - 200 * (na == 0);
            if (sc > bs) { bs = sc; best = d; best_w = w2; }
        }
        if (best < 0) return INT_MIN;
        w = best_w;
    }
    return area_shape(w, w.px[w.me], w.py[w.me]);
}

// 我走 d（对手走各自的 top-1 预测）一整回合后的分数 —— 根节点启发式
int root_score(const W& w, int d) {
    AreaInfo a_old = area_full(w, w.px[w.me], w.py[w.me]);  // 走之前的区域
    int mv[12];
    for (int k = 0; k < w.N; ++k) {
        if (k == w.me) mv[k] = d;
        else if (w.alive[k]) {
            int lm[4]; int n = legal_moves(w, k, lm);
            if (n == 0) mv[k] = w.last[k];
            else {
                int mx = w.px[w.me] + tron::DX[d], my = w.py[w.me] + tron::DY[d];
                int best = lm[0], bs = INT_MIN;
                for (int i = 0; i < n; ++i) {
                    int sc = opp_score(w, k, lm[i], mx, my);
                    if (sc > bs) { bs = sc; best = lm[i]; }
                }
                mv[k] = best;
            }
        } else mv[k] = w.last[k];
    }
    W w2 = w;
    int8_t why[12];
    step_wk(w2, mv, why);
    if (!w2.alive[w2.me]) {
        int deaths = 0;
        for (int k = 0; k < w2.N; ++k)
            if (k != w2.me && w.alive[k] && !w2.alive[k]) ++deaths;
        return -1000 + 350 * deaths;  // 垫背：同回合并列
    }
    int sc = eval_leaf(w2);
    int split = a_old.raw - last_area.raw - 1;  // 这一步把自己区域切掉的格数
    if (split > 0) sc -= 5 * split;
    sc -= 90 * opps_in_region(w2);  // 区域内挤着的对手
    sc -= congestion(w2, w2.px[w2.me], w2.py[w2.me]);
    for (int k = 0; k < w2.N; ++k)
        if (k != w2.me && w.alive[k] && !w2.alive[k])
            sc += (why[k] == 5) ? 250 : 50;  // 抢格击杀 / 白送
    // 自我延续 6 回合后的形状：区分同区域内不同方向（对手也按 top-1 走）
    int cont = self_cont(w2, 6);
    if (cont > -100000) sc += 3 * cont / 4;
    return sc;
}

// ---------------------------------------------------------------- 搜索
struct Ctx {
    long long nodes = 0;
    long long node_cap = 1000000;
    bool budgeted = false;
    double stop_ms = 8.0;
    bool stop = false;
    int maxdepth = 0;
};

bool tick(Ctx& c) {
    if ((++c.nodes & 15) == 0) {
        if (c.nodes > c.node_cap) return c.stop = true;
        if (c.budgeted && tron::budget_left() < c.stop_ms) return c.stop = true;
    }
    return c.stop;
}

// 我走 d、对手任意响应组合（≤ cap）后的最差值；h 为剩余"我方回合"层数
int value_move(const W& w, int d, int h, Ctx& c, int opcap, int mycap) {
    int mx = w.px[w.me] + tron::DX[d], my = w.py[w.me] + tron::DY[d];
    AreaInfo a_old = area_full(w, w.px[w.me], w.py[w.me]);
    Cands cd;
    build_cands(w, mx, my, cd, opcap);
    int nc = combo_count(cd, opcap);
    Combo local[64];
    gen_combos(cd, nc, local);
    int worst = INT_MAX;
    int mv[12];
    for (int k = 0; k < w.N; ++k) mv[k] = (k == w.me) ? d : w.last[k];
    for (int ci = 0; ci < nc; ++ci) {
        if (tick(c)) break;
        const Combo& cb = local[ci];
        for (int t = 0; t < cd.na; ++t) mv[cd.oplist[t]] = cd.cand[cd.oplist[t]][cb.sel[t]];
        W w2 = w;
        int8_t why[12];
        step_wk(w2, mv, why);
        int v;
        if (!w2.alive[w2.me]) {
            int deaths = 0;
            for (int k = 0; k < w2.N; ++k)
                if (k != w2.me && w.alive[k] && !w2.alive[k]) ++deaths;
            v = -1000 + 350 * deaths;
        } else if (h == 1) {
            v = eval_leaf(w2);
            int split = a_old.raw - last_area.raw - 1;  // 区域被切掉
            if (split > 0) v -= 5 * split;
            v -= 90 * opps_in_region(w2);
            if (nc <= 4) {  // 对手影响小时，看自我延续 4 回合后的形状
                int cont = self_cont(w2, 4);
                if (cont > -100000) v += 3 * cont / 4;
            }
        } else {
            int lm2[4]; int n2 = legal_moves(w2, w2.me, lm2);
            if (n2 == 0) v = -5000;
            else {
                // 我方下一手：按便宜代理分排序，取前 mycap
                int ord[4], sv[4];
                for (int i = 0; i < n2; ++i) {
                    ord[i] = lm2[i];
                    int nx = w2.px[w2.me] + tron::DX[lm2[i]], ny = w2.py[w2.me] + tron::DY[lm2[i]];
                    int na = 0;
                    for (int d2 = 0; d2 < 4; ++d2)
                        if (w2.c[idxx(nx + tron::DX[d2], ny + tron::DY[d2])] == tron::EMPTY) ++na;
                    sv[i] = space9(w2, nx, ny) + 40 * (na >= 2) + 20 * (na >= 3) - 200 * (na == 0);
                }
                for (int i = 1; i < n2; ++i)
                    for (int j = i; j > 0 && sv[j] > sv[j - 1]; --j) { std::swap(sv[j], sv[j - 1]); std::swap(ord[j], ord[j - 1]); }
                v = INT_MIN;
                int lim = n2 < mycap ? n2 : mycap;
                for (int i = 0; i < lim; ++i) {
                    if (tick(c)) break;
                    int v2 = value_move(w2, ord[i], h - 1, c, opcap / 2 + 8, 2);
                    if (v2 > v) v = v2;
                }
            }
        }
        if (v < worst) worst = v;
    }
    if (worst == INT_MAX) worst = -2000;
    return worst;
}

// 常规搜索：按 root_score 排序遍历我方全部合法方向
int search_my(const W& w, int h, Ctx& c, int opcap, int* best_d) {
    int lm[4]; int n = legal_moves(w, w.me, lm);
    if (n == 0) {
        *best_d = w.last[w.me];
        int deaths = 0;
        for (int k = 0; k < w.N; ++k)
            if (k != w.me && w.alive[k]) { int ol[4]; if (legal_moves(w, k, ol) == 0) ++deaths; }
        return -1000 + 350 * deaths;
    }
    int ord[4], sv[4];
    for (int i = 0; i < n; ++i) { ord[i] = lm[i]; sv[i] = root_score(w, lm[i]); }
    // 稳定排序（降分；同分保持 U R D L 相对顺序）
    for (int i = 1; i < n; ++i)
        for (int j = i; j > 0 && sv[j] > sv[j - 1]; --j) { std::swap(sv[j], sv[j - 1]); std::swap(ord[j], ord[j - 1]); }
    int best = INT_MIN, bd = ord[0];
    for (int i = 0; i < n; ++i) {
        if (tick(c)) break;
        int v = value_move(w, ord[i], h, c, opcap, 2);
        if (v > best) { best = v; bd = ord[i]; }
    }
    *best_d = bd;
    return best;
}

// ---------------------------------------------------------------- 残局 hunt
// 对抗式：我方 max、对手 min（对手目标 = 杀我 / 保命保面积）
// 值：杀一个对手 +700（存活到终局者额外高）；我死 −2*KILLV（独自）/ −KILLV+200*deaths（垫背）
int hunt_resp(const W& w, int d, int depth, Ctx& c, int alpha, int beta);

int hunt_my(const W& w, int depth, Ctx& c, int alpha, int beta, int* best_d) {
    int lm[4]; int n = legal_moves(w, w.me, lm);
    if (n == 0) {
        if (best_d) *best_d = w.last[w.me];
        int deaths = 0;
        for (int k = 0; k < w.N; ++k)
            if (k != w.me && w.alive[k]) { int ol[4]; if (legal_moves(w, k, ol) == 0) ++deaths; }
        return -2 * KILLV + 200 * deaths;
    }
    // 排序：先试"能立刻杀"的，再按便宜的空间代理（防自困）
    int ord[4], key[4][3];
    for (int i = 0; i < n; ++i) {
        int d = lm[i];
        int mx = w.px[w.me] + tron::DX[d], my = w.py[w.me] + tron::DY[d];
        Cands cd;
        build_cands(w, mx, my, cd, 1);  // top-1 探针
        int mv[12];
        for (int k = 0; k < w.N; ++k) mv[k] = (k == w.me) ? d : w.last[k];
        for (int t = 0; t < cd.na; ++t) mv[cd.oplist[t]] = cd.cand[cd.oplist[t]][0];
        W w2 = w;
        int8_t why[12];
        step_wk(w2, mv, why);
        int k0 = 0, k1 = 0, k2 = 0, k0b = 0;
        if (!w2.alive[w2.me]) {
            k2 = INT_MIN; k0b = INT_MAX;  // 自杀排最后
        } else {
            for (int k = 0; k < w2.N; ++k)
                if (k != w2.me && w.alive[k] && !w2.alive[k]) ++k0;
            int nx = w2.px[w2.me], ny = w2.py[w2.me];
            int na = 0;
            for (int d2 = 0; d2 < 4; ++d2)
                if (w2.c[idxx(nx + tron::DX[d2], ny + tron::DY[d2])] == tron::EMPTY) ++na;
            k1 = space9(w2, nx, ny) + 40 * (na >= 2) + 20 * (na >= 3) - 200 * (na == 0);
            for (int k = 0; k < w2.N; ++k)
                if (k != w2.me && w2.alive[k]) { int ol[4]; k0b += legal_moves(w2, k, ol); }
        }
        key[i][0] = k0; key[i][1] = k0b; key[i][2] = k1;
        ord[i] = d;
    }
    for (int i = 1; i < n; ++i)
        for (int j = i; j > 0; --j) {
            bool better = key[j][0] != key[j - 1][0] ? key[j][0] > key[j - 1][0]
                       : key[j][1] != key[j - 1][1] ? key[j][1] < key[j - 1][1]
                       : key[j][2] > key[j - 1][2];
            if (better) { std::swap(key[j], key[j - 1]); std::swap(ord[j], ord[j - 1]); }
        }
    int best = INT_MIN, bd = ord[0];
    for (int i = 0; i < n; ++i) {
        if (c.stop) break;
        if (tick(c)) break;
        int v = hunt_resp(w, ord[i], depth, c, alpha, beta);
        if (v > best) { best = v; bd = ord[i]; }
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
        if (best >= KILLV) break;  // 已找到强制杀光
    }
    if (best_d) *best_d = bd;
    return best;
}

int hunt_resp(const W& w, int d, int depth, Ctx& c, int alpha, int beta) {
    int mx = w.px[w.me] + tron::DX[d], my = w.py[w.me] + tron::DY[d];
    Cands cd;
    build_cands(w, mx, my, cd, 16);
    int nc = combo_count(cd, 16);
    Combo local[16];
    gen_combos(cd, nc, local);
    int worst = INT_MAX;
    int mv[12];
    for (int k = 0; k < w.N; ++k) mv[k] = (k == w.me) ? d : w.last[k];
    for (int ci = 0; ci < nc; ++ci) {
        if (c.stop) break;
        if (tick(c)) break;
        const Combo& cb = local[ci];
        for (int t = 0; t < cd.na; ++t) mv[cd.oplist[t]] = cd.cand[cd.oplist[t]][cb.sel[t]];
        W w2 = w;
        int8_t why[12];
        step_wk(w2, mv, why);
        int v;
        if (!w2.alive[w2.me]) {
            int deaths = 0;
            for (int k = 0; k < w2.N; ++k)
                if (k != w2.me && w.alive[k] && !w2.alive[k]) ++deaths;
            v = deaths > 0 ? -KILLV + 200 * deaths : -2 * KILLV;
        } else {
            int dead_opp = 0;
            for (int k = 0; k < w2.N; ++k)
                if (k != w2.me && w.alive[k] && !w2.alive[k]) ++dead_opp;
            if (alive_count(w2) == 1) v = KILLV * 2;  // 1v1 获胜
            else {
                v = 700 * dead_opp;
                if (depth > 1) v += hunt_my(w2, depth - 1, c, alpha, beta, nullptr);
                else {
                    v += eval_leaf(w2);
                    if (tron::budget_left() > 12) {
                        int cont = self_cont(w2, 3);
                        if (cont > -100000) v += cont / 2;
                    }
                }
            }
        }
        if (v < worst) worst = v;
        if (worst <= alpha) break;
    }
    if (worst == INT_MAX) worst = 0;
    return worst;
}

}  // namespace

int decide(const tron::State& s) {
    load(s);
    int lm[4]; int n = legal_moves(g, g.me, lm);
    if (n == 0) return 0;  // 四向皆死，随便给

    const int alive = alive_count(g);
    const bool first = (s.turn == 0);
    const double budget = tron::budget_left();
    const bool budgeted = budget < 1e10;

    Ctx c;
    c.budgeted = budgeted;
    if (alive <= 4) {
        // 残局 hunt
        int depth = alive == 2 ? 26 : alive == 3 ? 14 : 10;
        if (first) depth += 8;
        c.maxdepth = depth;
        c.node_cap = first ? 5000000 : (alive == 2 ? 25000 : 8000);
        c.stop_ms = first ? 1500 : 27;
    } else {
        c.node_cap = first ? 2000000 : 60000;
        c.stop_ms = first ? 1800 : 30;
    }

    int bd = lm[0];
    int val = root_score(g, lm[0]);
    if (alive <= 4) {
        val = hunt_my(g, c.maxdepth, c, INT_MIN, INT_MAX, &bd);
    } else {
        int h = 1;
        if (budgeted && budget > 40) h = 2;
        if (first && budget > 800) h = 3;
        if (!budgeted) h = 2;  // 快速模式（无判罚）：默认多算一层
        val = search_my(g, h, c, 48, &bd);
    }

    // 安全兜底：保证返回当前合法方向
    if (bd < 0 || bd > 3) bd = lm[0];
    bool legal = false;
    for (int i = 0; i < n; ++i) if (lm[i] == bd) legal = true;
    if (!legal) {
        int best = lm[0], bs = INT_MIN;
        for (int i = 0; i < n; ++i) {
            int sc = root_score(g, lm[i]);
            if (sc > bs) { bs = sc; best = lm[i]; }
        }
        bd = best;
    }

    // 止损：若所选 move 大概率导致我下一回合就死（只剩一条路），
    // 找一个能拉对手垫背的 move（同回合并列 = 名次分更高）
    {
        int lmb[4]; int nb = legal_moves(g, g.me, lmb);
        if (nb == 1) {
            // 唯一的路：若它是"死路"（走过去后无出口），考虑垫背
            int d0 = lmb[0];
            W w2 = g;
            int mv[12];
            int mx = w2.px[w2.me] + tron::DX[d0], my = w2.py[w2.me] + tron::DY[d0];
            for (int k = 0; k < w2.N; ++k) {
                if (k == w2.me) { mv[k] = d0; continue; }
                if (!w2.alive[k]) { mv[k] = w2.last[k]; continue; }
                int ol[4]; int on = legal_moves(w2, k, ol);
                if (on == 0) { mv[k] = w2.last[k]; continue; }
                int b = ol[0], bsc = INT_MIN;
                for (int j = 0; j < on; ++j) { int sc = opp_score(w2, k, ol[j], mx, my); if (sc > bsc) { bsc = sc; b = ol[j]; } }
                mv[k] = b;
            }
            int8_t why[12];
            step_wk(w2, mv, why);
            if (w2.alive[w2.me]) {
                int lm2[4]; int n2 = legal_moves(w2, w2.me, lm2);
                if (n2 == 0) {
                    // 当前选择会让我下回合必死。找一个同回合能杀死对手的 move
                    int best_k = -1, best_kd = 0;
                    for (int i = 0; i < n; ++i) {
                        int d = lm[i];
                        W w3 = g;
                        int mv3[12];
                        int mx3 = w3.px[w3.me] + tron::DX[d], my3 = w3.py[w3.me] + tron::DY[d];
                        for (int k = 0; k < w3.N; ++k) {
                            if (k == w3.me) { mv3[k] = d; continue; }
                            if (!w3.alive[k]) { mv3[k] = w3.last[k]; continue; }
                            int ol[4]; int on = legal_moves(w3, k, ol);
                            if (on == 0) { mv3[k] = w3.last[k]; continue; }
                            int b = ol[0], bsc = INT_MIN;
                            for (int j = 0; j < on; ++j) { int sc = opp_score(w3, k, ol[j], mx3, my3); if (sc > bsc) { bsc = sc; b = ol[j]; } }
                            mv3[k] = b;
                        }
                        int8_t why3[12];
                        step_wk(w3, mv3, why3);
                        int kd = 0;
                        bool i_die = !w3.alive[w3.me];
                        for (int k = 0; k < w3.N; ++k)
                            if (k != w3.me && g.alive[k] && !w3.alive[k]) ++kd;
                        // 我死且拉到对手 = 并列；我不死但杀到对手 = 更好
                        int score = (i_die ? kd * 2 : kd * 3) + (i_die ? 0 : 100);
                        if (score > best_kd) { best_kd = score; best_k = d; }
                    }
                    if (best_k >= 0 && best_kd >= 2) bd = best_k;  // 至少能拉一个
                }
            }
        }
    }

    std::fprintf(stderr, "t=%d N=%d alive=%d d=%c v=%d nodes=%lld ms_left=%.1f\n",
                 s.turn, s.N, alive, tron::DIR_CHAR[bd], val, c.nodes, tron::budget_left());
    return bd;
}
