// bot.cpp —— 光轮对战（Tron）bot
// 思路：
//   1) 每个合法落点做 flood fill：进去后属于我的可达空间（生存安全）
//   2) 多源 BFS 算 Voronoi 领土：我能先到的格 vs 对手能先到的格
//   3) 抢格危险度：落点与本回合优先级更高（能抢赢我）的对手相邻 => 可能被撞死
//   4) 蒙特卡洛 rollout：把候选落点固定，双方按"封顶 flood fill 贪心"策略推演到终局/水平线，
//      统计我的存活回合与淘汰人数，量化"我能活多久、挤掉几个人"
#include "tron.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace {

constexpr int ST = tron::MAXW;
constexpr int NC = tron::MAXW * tron::MAXH;

int g_stamp = 0;
int g_seen[NC];
int g_qq[NC];
int g_qd[NC];
int gpos_x[NC], gpos_y[NC];
struct TabInit {
    TabInit() {
        for (int i = 0; i < NC; ++i) {
            gpos_x[i] = i % ST;
            gpos_y[i] = i / ST;
        }
    }
} g_tabinit_;

// ---- flood fill：从 (sx,sy)（该格视为已占）可达空地数；到 cap 提前退出 ----
inline int floodfill(const tron::State& s, int sx, int sy, int cap) {
    if ((unsigned)sx >= (unsigned)s.W || (unsigned)sy >= (unsigned)s.H) return 0;
    ++g_stamp;
    int head = 0, tail = 0;
    const int stt = sy * ST + sx;
    g_seen[stt] = g_stamp;
    g_qq[tail++] = stt;
    int cnt = 0;
    while (head < tail) {
        const int cur = g_qq[head++];
        const int cx = gpos_x[cur], cy = gpos_y[cur];
        for (int d = 0; d < 4; ++d) {
            const int nx = cx + tron::DX[d], ny = cy + tron::DY[d];
            if ((unsigned)nx >= (unsigned)s.W || (unsigned)ny >= (unsigned)s.H) continue;
            const int n = ny * ST + nx;
            if (g_seen[n] == g_stamp) continue;
            if (s.cell[ny][nx] != tron::EMPTY) continue;
            g_seen[n] = g_stamp;
            g_qq[tail++] = n;
            if (++cnt >= cap) return cnt;
        }
    }
    return cnt;
}

// ---- Voronoi：我的头放到 (myx,myy)，其余活人在各自头格 ----
int v_stamp = 0;
int v_seen[NC];
int v_dist[NC];
int v_own[NC];
int v_qq[NC];

inline void voronoi(const tron::State& s, int myx, int myy, int& mine, int& maxopp) {
    ++v_stamp;
    int head = 0, tail = 0;
    {
        const int stt = myy * ST + myx;
        v_seen[stt] = v_stamp;
        v_dist[stt] = 0;
        v_own[stt] = s.me;
        v_qq[tail++] = stt;
    }
    for (int k = 0; k < s.N; ++k) {
        if (k == s.me || !s.alive[k]) continue;
        const int stt = s.y[k] * ST + s.x[k];
        if (v_seen[stt] == v_stamp) continue;
        v_seen[stt] = v_stamp;
        v_dist[stt] = 0;
        v_own[stt] = k;
        v_qq[tail++] = stt;
    }
    while (head < tail) {
        const int cur = v_qq[head++];
        if (v_own[cur] < 0) continue;    // 同时到达的争议格：双方都不算，也不再扩展
        const int nd = v_dist[cur] + 1;
        const int o = v_own[cur];
        const int cx = gpos_x[cur], cy = gpos_y[cur];
        for (int d = 0; d < 4; ++d) {
            const int nx = cx + tron::DX[d], ny = cy + tron::DY[d];
            if ((unsigned)nx >= (unsigned)s.W || (unsigned)ny >= (unsigned)s.H) continue;
            const int n = ny * ST + nx;
            if (s.cell[ny][nx] != tron::EMPTY) continue;
            if (v_seen[n] == v_stamp) {
                if (v_dist[n] == nd && v_own[n] >= 0 && v_own[n] != o) v_own[n] = -1;
            } else {
                v_seen[n] = v_stamp;
                v_dist[n] = nd;
                v_own[n] = o;
                v_qq[tail++] = n;
            }
        }
    }
    int cnt[tron::MAXN];
    for (int k = 0; k < tron::MAXN; ++k) cnt[k] = 0;
    for (int i = 0; i < tail; ++i) {
        const int cur = v_qq[i];
        const int o = v_own[cur];
        if (o < 0) continue;
        const int cx = gpos_x[cur], cy = gpos_y[cur];
        if (s.cell[cy][cx] != tron::EMPTY) continue;
        ++cnt[o];
    }
    mine = cnt[s.me];
    int oppmax = 0;
    for (int k = 0; k < s.N; ++k)
        if (k != s.me && s.alive[k]) oppmax = std::max(oppmax, cnt[k]);
    maxopp = oppmax;
}

// ---- 出路检查：我走 d 之后，下一步的出口是否被下一回合优先级更高的对手卡死 ----
// 返回 2 = 所有出口都致命 / 无路可走；1 = 部分出口致命；0 = 安全
inline int exit_lethal(const tron::State& s, int d) {
    tron::State s2 = s;
    int mvs[tron::MAXN];
    for (int k = 0; k < s.N; ++k) mvs[k] = (k == s.me) ? d : s.last_dir[k];
    tron::step(s2, mvs);
    if (!s2.alive[s2.me]) return 2;
    const int me = s2.me;
    const int pme = tron::priority(s2, me);
    int total = 0, lethal = 0;
    for (int e = 0; e < 4; ++e) {
        if (!tron::can_move(s2, me, e)) continue;
        ++total;
        const int tx = s2.x[me] + tron::DX[e], ty = s2.y[me] + tron::DY[e];
        for (int k = 0; k < s2.N; ++k) {
            if (k == me || !s2.alive[k]) continue;
            if (std::abs(s2.x[k] - tx) + std::abs(s2.y[k] - ty) != 1) continue;
            if (tron::priority(s2, k) < pme) {
                ++lethal;
                break;
            }
        }
    }
    if (total == 0 || lethal >= total) return 2;
    return lethal;
}

// ---- 与最近的活对手"还差几回合相邻"（1 = 落点本身就贴着对手）----
inline int contact_dist(const tron::State& s, int sx, int sy, int capd) {
    ++g_stamp;
    int head = 0, tail = 0;
    const int stt = sy * ST + sx;
    g_seen[stt] = g_stamp;
    g_qq[tail++] = stt;
    g_qd[stt] = 0;
    while (head < tail) {
        const int cur = g_qq[head++];
        const int dd = g_qd[cur];
        if (dd >= capd) continue;
        const int cx = gpos_x[cur], cy = gpos_y[cur];
        for (int k = 0; k < s.N; ++k) {
            if (k == s.me || !s.alive[k]) continue;
            if (std::abs(s.x[k] - cx) + std::abs(s.y[k] - cy) == 1) return dd + 1;
        }
        const int nd = dd + 1;
        for (int d = 0; d < 4; ++d) {
            const int nx = cx + tron::DX[d], ny = cy + tron::DY[d];
            if ((unsigned)nx >= (unsigned)s.W || (unsigned)ny >= (unsigned)s.H) continue;
            const int n = ny * ST + nx;
            if (g_seen[n] == g_stamp) continue;
            if (s.cell[ny][nx] != tron::EMPTY) continue;
            g_seen[n] = g_stamp;
            g_qd[n] = nd;
            g_qq[tail++] = n;
        }
    }
    return capd + 1;
}

// ================= 蒙特卡洛 rollout =================

// 策略：会恶意抢格（本回合优先级更高且有人相邻）的封顶 flood fill 贪心
uint32_t g_rng = 0x9E3779B9u;
inline uint32_t rnd() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

inline int policy_move(const tron::State& s, int k, int cap, bool allow_kill = true) {
    const int pk = tron::priority(s, k);
    int killd = -1;
    if (allow_kill) {
        for (int d = 0; d < 4; ++d) {
            if (!tron::can_move(s, k, d)) continue;
            const int tx = s.x[k] + tron::DX[d], ty = s.y[k] + tron::DY[d];
            for (int j = 0; j < s.N; ++j) {
                if (j == k || !s.alive[j]) continue;
                if (std::abs(s.x[j] - tx) + std::abs(s.y[j] - ty) != 1) continue;
                if (tron::priority(s, j) > pk) {
                    killd = d;
                    break;
                }
            }
            if (killd >= 0) break;
        }
        if (killd >= 0 && (rnd() & 100) < 88) return killd;
    }

    const int straight = s.last_dir[k];
    int bestv = -1, bestd = -1;
    for (int i = 0; i < 4; ++i) {
        const int d = (i == 0) ? straight : ((straight + i) & 3);
        if (!tron::can_move(s, k, d)) continue;
        const int v = floodfill(s, s.x[k] + tron::DX[d], s.y[k] + tron::DY[d], cap);
        if (v > bestv) {
            bestv = v;
            bestd = d;
        }
    }
    return bestd < 0 ? straight : bestd;
}

// 固定我的第一步为 myfirst，推演到终局或 horizon；返回我这局的"价值"
inline double rollout_value(const tron::State& s0, int myfirst, int horizon, int cap, double deadline) {
    tron::State s = s0;
    int mv[tron::MAXN];
    for (int t = 0; t < horizon && !s.over; ++t) {
        if ((t & 15) == 15 && tron::budget_left() <= deadline) break;    // 预算守卫
        for (int k = 0; k < s.N; ++k) {
            if (!s.alive[k]) continue;
            mv[k] = (t == 0 && k == s.me) ? myfirst : policy_move(s, k, cap);
        }
        tron::step(s, mv);
    }
    const int me = s0.me;
    if (!s.alive[me]) return double(s.death_turn[me]);
    int dead = 0;
    for (int k = 0; k < s.N; ++k)
        if (k != me && !s.alive[k]) ++dead;
    double v = double(horizon);
    v += 60.0 * double(dead) / double(std::max(1, s.N - 1));
    v += 0.12 * double(floodfill(s, s.x[me], s.y[me], 500));
    return v;
}

// ---- 静态评估局面 s：我的头就是 (s.x[me], s.y[me]) ----
inline double eval_pos(const tron::State& s) {
    const int me = s.me, N = s.N, W = s.W, H = s.H;
    const double WH = double(W) * double(H);
    const int mx = s.x[me], my = s.y[me];
    const int pme = tron::priority(s, me);
    int mine = 0, maxopp = 0;
    voronoi(s, mx, my, mine, maxopp);
    const int sp = floodfill(s, mx, my, 3000);
    int danger = 0, kill = 0, forced = 0;
    for (int k = 0; k < N; ++k) {
        if (k == me || !s.alive[k]) continue;
        if (std::abs(s.x[k] - mx) + std::abs(s.y[k] - my) != 1) continue;
        if (tron::priority(s, k) < pme) {
            ++danger;
            int opts = 0;
            for (int e = 0; e < 4; ++e)
                if (tron::can_move(s, k, e)) ++opts;
            if (opts <= 1) ++forced;
        } else {
            ++kill;
        }
    }
    int lx = 0;
    int total = 0, lethal = 0;
    for (int e = 0; e < 4; ++e) {
        if (!tron::can_move(s, me, e)) continue;
        ++total;
        const int tx = mx + tron::DX[e], ty = my + tron::DY[e];
        for (int k = 0; k < N; ++k) {
            if (k == me || !s.alive[k]) continue;
            if (std::abs(s.x[k] - tx) + std::abs(s.y[k] - ty) != 1) continue;
            if (tron::priority(s, k) < pme) {
                ++lethal;
                break;
            }
        }
    }
    if (total == 0 || lethal >= total) lx = 2;
    else lx = lethal;
    const int contact = contact_dist(s, mx, my, 12);

    double sc = 100.0 * double(mine - maxopp) / WH;
    sc += 60.0 * double(std::min(sp, 220)) / WH;
    if (forced) sc -= 900.0 * forced;
    if (danger) sc -= (N == 2 ? 900.0 : 260.0) * danger;
    if (lx >= 2) sc -= 900.0;
    else sc -= 120.0 * lx;
    sc += 1.5 * kill;
    if (N == 2) {
        if (mine - maxopp > 8) sc -= 2.5 * double(std::max(0, 8 - contact));
        else if (mine - maxopp < -8) sc += 2.5 * double(std::max(0, 10 - contact));
    } else if (contact <= 2) {
        sc -= 2.0 * (3 - contact);
    }
    return sc;
}

// ---- duel：三层 minimax。我 d -> 对手最佳 r -> 我最佳 d2 -> 静态评估 ----
inline double duel_score3(const tron::State& s, int d) {
    const int me = s.me, N = s.N, opp = 1 - me;
    double worst = 1e30;
    int replies = 0;
    for (int r = 0; r < 4; ++r) {
        if (s.alive[opp] && !tron::can_move(s, opp, r)) continue;
        ++replies;
        tron::State s2 = s;
        int mvs[tron::MAXN];
        for (int k = 0; k < N; ++k) mvs[k] = (k == me) ? d : (k == opp ? r : s.last_dir[k]);
        tron::step(s2, mvs);
        if (!s2.alive[me]) return -900.0;
        double bestv = -1e30;
        int cnt2 = 0;
        for (int d2 = 0; d2 < 4; ++d2) {
            if (!tron::can_move(s2, me, d2)) continue;
            ++cnt2;
            tron::State s3 = s2;
            int m3[tron::MAXN];
            for (int k = 0; k < N; ++k) m3[k] = (k == me) ? d2 : policy_move(s2, k, 100000, false);
            tron::step(s3, m3);
            const double v = s3.alive[me] ? eval_pos(s3) : -900.0;
            if (v > bestv) bestv = v;
        }
        if (cnt2 == 0) return -900.0;    // 走进去就没路了
        if (bestv < worst) worst = bestv;
    }
    if (replies == 0) return -900.0;
    return worst;
}

// ---- duel：二层 minimax。对手从合法走法里选让我最差的 ----
inline double refute_score(const tron::State& s, int d) {
    const int me = s.me, N = s.N, opp = 1 - me;
    double worst = 1e30;
    int replies = 0;
    for (int r = 0; r < 4; ++r) {
        if (s.alive[opp] && !tron::can_move(s, opp, r)) continue;
        ++replies;
        tron::State s2 = s;
        int mvs[tron::MAXN];
        for (int k = 0; k < N; ++k) mvs[k] = (k == me) ? d : (k == opp ? r : s.last_dir[k]);
        tron::step(s2, mvs);
        if (!s2.alive[me]) return -900.0;    // 被抢格/撞死
        const double v = eval_pos(s2);
        if (v < worst) worst = v;
    }
    if (replies == 0) {    // 对手无路可走（理论上已死）
        tron::State s2 = s;
        int mvs[tron::MAXN];
        for (int k = 0; k < N; ++k) mvs[k] = (k == me) ? d : s.last_dir[k];
        tron::step(s2, mvs);
        return s2.alive[me] ? eval_pos(s2) : -900.0;
    }
    return worst;
}

// ---- duel：minimax（同时决策游戏，未轮到决策的玩家按策略模型走）----
inline double duel_minimax(const tron::State& s, int depth, int cap) {
    if (s.over || depth <= 0) {
        if (!s.alive[s.me]) return -900.0;
        if (tron::alive_count(s) <= 1) return 1000.0;
        return eval_pos(s);
    }
    const int me = s.me, opp = 1 - me;
    const bool myturn = (depth & 1) != 0;
    const int who = myturn ? me : opp;
    double best = myturn ? -1e30 : 1e30;
    int cnt = 0;
    for (int d = 0; d < 4; ++d) {
        if (!tron::can_move(s, who, d)) continue;
        ++cnt;
        tron::State s2 = s;
        int mvs[tron::MAXN];
        for (int k = 0; k < s.N; ++k) mvs[k] = (k == who) ? d : policy_move(s, k, cap, false);
        tron::step(s2, mvs);
        const double v = duel_minimax(s2, depth - 1, cap);
        if (myturn ? (v > best) : (v < best)) best = v;
    }
    if (cnt == 0) return myturn ? -900.0 : 1000.0;
    return best;
}

// ---- 3~4 人局：枚举所有活对手的走法组合，取对我最差的联合应手 ----
inline double multi_refute(const tron::State& s, int d) {
    const int me = s.me, N = s.N;
    int opps[tron::MAXN], nopp = 0;
    for (int k = 0; k < N; ++k)
        if (k != me && s.alive[k]) opps[nopp++] = k;
    if (nopp == 0) {    // 只剩我
        tron::State s2 = s;
        int mvs[tron::MAXN];
        for (int k = 0; k < N; ++k) mvs[k] = (k == me) ? d : s.last_dir[k];
        tron::step(s2, mvs);
        return s2.alive[me] ? eval_pos(s2) : -900.0;
    }
    const int total = 1 << (2 * nopp);
    double worst = 1e30;
    for (int code = 0; code < total; ++code) {
        int mvs[tron::MAXN];
        bool ok = true;
        for (int i = 0; i < nopp; ++i) {
            const int r = (code >> (2 * i)) & 3;
            if (!tron::can_move(s, opps[i], r)) {
                ok = false;
                break;
            }
            mvs[opps[i]] = r;
        }
        if (!ok) continue;
        for (int k = 0; k < N; ++k)
            if (k != me && !s.alive[k]) mvs[k] = s.last_dir[k];
        mvs[me] = d;
        tron::State s2 = s;
        tron::step(s2, mvs);
        if (!s2.alive[me]) return -900.0;
        const double v = eval_pos(s2);
        if (v < worst) worst = v;
        if (tron::budget_left() < 14.0) break;    // 预算守卫
    }
    if (worst > 1e29) {    // 没人能应手
        tron::State s2 = s;
        int mvs[tron::MAXN];
        for (int k = 0; k < N; ++k) mvs[k] = (k == me) ? d : s.last_dir[k];
        tron::step(s2, mvs);
        return s2.alive[me] ? eval_pos(s2) : -900.0;
    }
    return worst;
}

}    // namespace

int decide(const tron::State& s) {
    const int me = s.me, N = s.N, W = s.W, H = s.H;
    int mv[4], nmv = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, me, d)) mv[nmv++] = d;
    if (nmv == 0) return tron::U;
    if (nmv == 1) return mv[0];

    const double WH = double(W) * double(H);
    const int pme = tron::priority(s, me);

    if (N == 2) {    // duel：三层 minimax，不用 rollout
        double best = -1e30;
        int bestd = mv[0];
        for (int i = 0; i < nmv; ++i) {
            const double sc = duel_score3(s, mv[i]) + 0.001 * double(i);
            if (sc > best) {
                best = sc;
                bestd = mv[i];
            }
        }
        return bestd;
    }
    if (N <= 4) {    // 3~4 人局：多对手联合应手，不用 rollout
        double best = -1e30;
        int bestd = mv[0];
        for (int i = 0; i < nmv; ++i) {
            const double sc = multi_refute(s, mv[i]) + 0.001 * double(i);
            if (sc > best) {
                best = sc;
                bestd = mv[i];
            }
        }
        return bestd;
    }

    if (N == 2) {    // duel：三层 minimax，不用 rollout
        double best = -1e30;
        int bestd = mv[0];
        for (int i = 0; i < nmv; ++i) {
            const double sc = duel_score3(s, mv[i]) + 0.001 * double(i);
            if (sc > best) {
                best = sc;
                bestd = mv[i];
            }
        }
        return bestd;
    }

    double heur[4];
    int spv[4], vor_mine[4], vor_maxopp[4], dg[4], fc[4], kl[4], lxv[4];
    for (int i = 0; i < nmv; ++i) {
        const int d = mv[i];
        const int tx = s.x[me] + tron::DX[d], ty = s.y[me] + tron::DY[d];
        const int sp = floodfill(s, tx, ty, 3000);
        int mine = 0, maxopp = 0;
        voronoi(s, tx, ty, mine, maxopp);
        int danger = 0, kill = 0, forced = 0;
        for (int k = 0; k < N; ++k) {
            if (k == me || !s.alive[k]) continue;
            if (std::abs(s.x[k] - tx) + std::abs(s.y[k] - ty) != 1) continue;
            if (tron::priority(s, k) < pme) {
                ++danger;
                int opts = 0;
                for (int e = 0; e < 4; ++e)
                    if (tron::can_move(s, k, e)) ++opts;
                if (opts <= 1) ++forced;
            } else {
                ++kill;
            }
        }
        const int contact = contact_dist(s, tx, ty, 12);
        const int lx = exit_lethal(s, d);
        spv[i] = sp;
        vor_mine[i] = mine;
        vor_maxopp[i] = maxopp;
        dg[i] = danger;
        fc[i] = forced;
        kl[i] = kill;
        lxv[i] = lx;

        double sc = 100.0 * double(mine - maxopp) / WH;                 // 领土
        sc += 60.0 * double(std::min(sp, 220)) / WH;                    // 空间安全
        if (forced) sc -= 900.0 * forced;
        if (danger) sc -= (N == 2 ? 900.0 : 260.0) * danger;    // 对手机型会恶意抢格，多数局按致命处理
        if (lx >= 2) sc -= 900.0;
        else sc -= 120.0 * lx;                                          // 出路被封
        sc += 1.5 * kill;
        if (N == 2) {
            //  duel：领土领先就避开对手（防抢格），落后就逼近抢领土
            if (mine - maxopp > 8) sc -= 2.5 * double(std::max(0, 8 - contact));
            else if (mine - maxopp < -8) sc += 2.5 * double(std::max(0, 10 - contact));
        } else if (contact <= 2) {
            sc -= 2.0 * (3 - contact);    // 多人局别贴人
        }
        heur[i] = sc;
    }

    // ---- 蒙特卡洛：均匀分给每个候选，预算用完为止 ----
    const int horizon = (N == 2) ? 170 : (N <= 4 ? 150 : 175);
    const int cap = (N == 2) ? 100000 : (N <= 4 ? 400 : 60);
    double sum[4] = {0, 0, 0, 0};
    int cnt[4] = {0, 0, 0, 0};
    double bl = tron::budget_left();
    if (bl > 1e6) bl = 45.0;                      // 没有主循环时（自测）给个默认值
    const double stop_at = 16.0;                  // 留足余量：正式模式超时会被判罚
    int guard = 0;
    while (tron::budget_left() > stop_at) {
        // 轮转：谁 rollout 少先给谁
        int pick = 0;
        for (int i = 1; i < nmv; ++i)
            if (cnt[i] < cnt[pick]) pick = i;
        sum[pick] += rollout_value(s, mv[pick], horizon, cap, stop_at);
        ++cnt[pick];
        if (++guard > 20000) break;
    }
    for (int i = 0; i < nmv; ++i)
        if (cnt[i] > 0) heur[i] += (N == 2 ? 0.0 : 0.5) * (sum[i] / double(cnt[i]));

    int best = 0;
    for (int i = 1; i < nmv; ++i)
        if (heur[i] > heur[best]) best = i;
    return mv[best];
}
