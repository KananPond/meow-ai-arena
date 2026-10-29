// v16.cpp —— team2 队 B 位球员程序（阶跃 Step 5）
// 与队友（Kimi K3.1 Pro）定稿的配合协议：
//   角色：每步用共享局面的确定性截球时间选唯一 chaser（平局 IDX 小者）。
//         T_i = min{k : Dt[k] + KICK_REACH >= dist(P_i, B_k)}，
//         B_k = soccer::predict_ball 预测的球位；Dt[k] = 从静止 k 步最大位移。
//   消息：0..127 = 本步有效踢球方向 code = round(atan2(ky,kx)*64/pi) mod 128（右0 下32 左64 上96）
//         128 = 我 CHASE；129 = 我 DEFEND/SUPPORT；255 = 紧急回防；130 保留。

#include "soccer.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include <algorithm>

using soccer::i64;
using soccer::BallPt;

static soccer::Config CFG;
static i64 FW = 12000, FH = 8000;
static i64 Dt[230];  // Dt[k]：从静止出发 k 步内最大位移

static void init_tables() {
    i64 v = 0;
    Dt[0] = 0;
    for (int k = 1; k < 230; ++k) {
        v = soccer::tdiv(v * 9, 10) + 40;
        Dt[k] = Dt[k - 1] + v;
    }
}

static inline i64 d2(i64 x1, i64 y1, i64 x2, i64 y2) {
    i64 dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}
static inline i64 dd(i64 x1, i64 y1, i64 x2, i64 y2) { return soccer::isqrt(d2(x1, y1, x2, y2)); }
static inline i64 clampi(i64 v, i64 lo, i64 hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Ply { i64 x, y, vx, vy, cd; };

// 截球时间：从 (px,py)（视为静止）出发，最早第几步能到达距球轨迹不超 reach 的位置
static int itime(i64 px, i64 py, i64 bx, i64 by, const std::vector<BallPt>& pb, i64 reach) {
    if (d2(px, py, bx, by) <= reach * reach) return 0;
    const int n = (int)pb.size();
    for (int k = 1; k <= n; ++k) {
        const BallPt& p = pb[k - 1];
        const i64 r = Dt[k] + reach;
        if (d2(px, py, p.x, p.y) <= r * r) return k;
    }
    return 1000000;
}
// 次级指标：到球轨迹的最近距离
static i64 imind(i64 px, i64 py, i64 bx, i64 by, const std::vector<BallPt>& pb) {
    i64 best = d2(px, py, bx, by);
    for (const auto& p : pb) best = std::min(best, d2(px, py, p.x, p.y));
    return soccer::isqrt(best);
}
// 从静止到达某点所需步数
static int steps_to(i64 px, i64 py, i64 tx, i64 ty) {
    const i64 r = dd(px, py, tx, ty);
    for (int k = 0; k < 230; ++k)
        if (Dt[k] >= r) return k;
    return 1000000;
}

// 踢球候选评估：把球以 (kx,ky) 踢出后的局面评分
static i64 eval_kick(i64 kx, i64 ky, const Ply& me, const Ply& mate, const Ply& o0, const Ply& o1, i64 bx, i64 by) {
    std::vector<BallPt> path;
    const int g = soccer::predict_ball(CFG, bx, by, kx, ky, 170, path);
    const int n = (int)path.size();
    const int eo = std::min(itime(o0.x, o0.y, bx, by, path, 450), itime(o1.x, o1.y, bx, by, path, 450));
    const int em = itime(mate.x, mate.y, bx, by, path, 450);
    int goal_step = 1000000;
    if (g != 0) goal_step = n;  // path 最后一项即越线时刻
    int lim = n;
    if (eo < lim) lim = eo;
    if (lim < 1) lim = 1;
    const BallPt& eff = path[lim - 1];  // 有效终点（被截断或自然停止）
    const int eo_end = std::min(steps_to(o0.x, o0.y, eff.x, eff.y), steps_to(o1.x, o1.y, eff.x, eff.y));
    const int em_end = steps_to(mate.x, mate.y, eff.x, eff.y);
    const int my_end = steps_to(me.x, me.y, eff.x, eff.y);
    i64 score;
    if (g == 1) {
        score = (eo < goal_step) ? 800 : 5000;  // 进对方门
    } else if (g == -1) {
        score = -100000;  // 乌龙
    } else {
        score = clampi((eff.x - 6000) / 3, -1200, 1400);  // 向对方场推进（封顶，避免偏爱朝墙角乱踢）
        if (em < 1000000 && (eo == 1000000 || em < eo)) score += 350;  // 队友先碰到
        if (eo < 1000000) score -= 300;                                // 对方能拦
        else if (eo_end < em_end) score -= 300;                        // 落点归对方
        else score += 150;                                             // 落点归我方
        // 保球权：对方拦不到且我比对手先到落点（带球推进的核心）
        if (eo == 1000000 && my_end < eo_end) score += 500;
        // 球停在死角（贴角）是坏局
        if (eff.y < 1300 || eff.y > 6700 || eff.x > 11300) score -= 500;
    }
    return score;
}

struct Cand { i64 kx, ky, score; int order; };

// 生成候选并评估，返回最优（scores 全部很低也返回最优，调用方总得踢）
static Cand choose_kick(const Ply& me, const Ply& mate, const Ply& o0, const Ply& o1, i64 bx, i64 by,
                        bool prefer_shot) {
    std::vector<Cand> cs;
    auto add = [&](i64 tx, i64 ty, int order) {
        i64 dx = tx - bx, dy = ty - by;
        i64 kx = dx, ky = dy;
        soccer::clamp_len(kx, ky, CFG.kick_max);
        if (kx == 0 && ky == 0) return;
        cs.push_back(Cand{kx, ky, eval_kick(kx, ky, me, mate, o0, o1, bx, by), order});
    };
    auto addp = [&](i64 tx, i64 ty, i64 power, int order) {
        i64 dx = tx - bx, dy = ty - by;
        i64 kx = dx, ky = dy;
        soccer::clamp_len(kx, ky, power);
        if (kx == 0 && ky == 0) return;
        cs.push_back(Cand{kx, ky, eval_kick(kx, ky, me, mate, o0, o1, bx, by), order});
    };
    // 射门（含两个角）
    add(FW - 60, 3200, 0);
    add(FW - 60, 4000, 0);
    add(FW - 60, 4800, 0);
    add(FW - 60, 3080, 0);
    add(FW - 60, 4920, 0);
    // 带球推进：轻力度向前踢（球速≈300<人类极速391，可以跟着球走），方向朝对方球门并略微收 y
    addp(bx + 2200, by + (4000 - by) / 5, 300, 5);
    addp(bx + 1600, by + (4000 - by) / 4, 260, 5);
    addp(bx + 2600, by + (4000 - by) / 3, 340, 5);
    // 宽处转移 / 解围
    add(10800, 500, 1);
    add(10800, 7500, 1);
    add(10800, 2500, 1);
    add(10800, 5500, 1);
    add(9500, 400, 1);
    add(9500, 7600, 1);
    // 传给队友（带提前量）
    add(mate.x + mate.vx * 8, mate.y + mate.vy * 8, 2);
    add(mate.x + mate.vx * 14, mate.y + mate.vy * 14, 2);
    // 回传安全球
    add(1400, 4000, 3);
    add(1400, 4000 + (by >= 4000 ? 1500 : -1500), 3);
    // 被逼抢时：朝远离最近对手的方向
    i64 nd = std::min(dd(o0.x, o0.y, bx, by), dd(o1.x, o1.y, bx, by));
    if (nd < 1600) {
        const Ply& no = (dd(o0.x, o0.y, bx, by) <= dd(o1.x, o1.y, bx, by)) ? o0 : o1;
        add(bx + (bx - no.x) * 2, by + (by - no.y) * 2, 4);
        add(bx + (bx - no.x) * 3, by + (by - no.y) * 3, 4);
    }
    if (prefer_shot) {
        for (auto& c : cs)
            if (c.order == 0) c.score += 250;
    }
    Cand best = cs[0];
    for (const auto& c : cs)
        if (c.score > best.score || (c.score == best.score && c.order < best.order)) best = c;
    return best;
}

static int dir_code(i64 kx, i64 ky) {
    // 约定：code = round(atan2(ky,kx) * 64 / pi)，右=0 下=32 左=64 上=96（y 向下）
    const double a = std::atan2((double)ky, (double)kx) * 64.0 / 3.14159265358979323846;
    int c = (int)std::lround(a);
    c = ((c % 128) + 128) % 128;
    return c;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::string magic;
    i64 ver;
    if (!(std::cin >> magic >> ver)) return 0;
    i64 GOAL, CH, RP, RB;
    std::cin >> FW >> FH >> GOAL >> CH >> RP >> RB;
    CFG.w = FW; CFG.h = FH; CFG.goal = GOAL; CFG.chamfer = CH; CFG.rp = RP; CFG.rb = RB;
    i64 a1, a2, a3, a4, a5;
    std::cin >> a1 >> a2 >> a3 >> a4 >> a5;
    CFG.acc_max = a1; CFG.pf_n = a2; CFG.pf_d = a3; CFG.bf_n = a4; CFG.bf_d = a5;
    i64 k1, k2, k3;
    std::cin >> k1 >> k2 >> k3;
    CFG.kick_max = k1; CFG.kick_reach = k2; CFG.kick_cooldown = k3;
    i64 e1, e2, e3, e4, e5, e6;
    std::cin >> e1 >> e2 >> e3 >> e4 >> e5 >> e6;
    CFG.e_wall_n = e1; CFG.e_wall_d = e2; CFG.e_ball_n = e3; CFG.e_ball_d = e4; CFG.e_pp_n = e5; CFG.e_pp_d = e6;
    i64 s1, s2, s3;
    std::cin >> s1 >> s2 >> s3;
    CFG.steps = s1; CFG.stall = s2; CFG.msg_max = s3;
    i64 IDX, STEP_MS, FIRST_MS;
    std::cin >> IDX >> STEP_MS >> FIRST_MS;
    init_tables();

    i64 T, LEFT, MY, OPP, MSG;
    i64 bx, by, bvx, bvy;
    Ply pl[4];
    i64 prev_my = -1, prev_opp = -1;
    std::vector<BallPt> pb;

    while (std::cin >> T >> LEFT >> MY >> OPP >> MSG >> bx >> by >> bvx >> bvy) {
        for (auto& p : pl) std::cin >> p.x >> p.y >> p.vx >> p.vy >> p.cd;
        const Ply me = pl[0], mate = pl[1], o0 = pl[2], o1 = pl[3];

        // 球轨迹预测
        pb.clear();
        const int pg = soccer::predict_ball(CFG, bx, by, bvx, bvy, 170, pb);

        // ---- 角色（与队友完全一致的确定性规则）----
        const int tm = itime(me.x, me.y, bx, by, pb, CFG.kick_reach);
        const int tn = itime(mate.x, mate.y, bx, by, pb, CFG.kick_reach);
        bool chase;
        if (tm < tn) chase = true;
        else if (tn < tm) chase = false;
        else {
            const i64 dm = imind(me.x, me.y, bx, by, pb);
            const i64 dn = imind(mate.x, mate.y, bx, by, pb);
            if (dm < dn) chase = true;
            else if (dn < dm) chase = false;
            else chase = (IDX == 0);
        }

        // ---- 开球检测 ----
        bool kickoff = false;
        if (T == 0) kickoff = true;
        else if (MY != prev_my || OPP != prev_opp) kickoff = true;
        else {
            const bool still = bvx == 0 && bvy == 0 && me.vx == 0 && me.vy == 0 && mate.vx == 0 && mate.vy == 0 &&
                               o0.vx == 0 && o0.vy == 0 && o1.vx == 0 && o1.vy == 0;
            if (still && dd(bx, by, 6000, 4000) <= 700) kickoff = true;
        }
        prev_my = MY;
        prev_opp = OPP;

        i64 ax = 0, ay = 0, kx = 0, ky = 0, msg = 0;
        i64 tx = bx, ty = by;

        const bool can_kick = (me.cd == 0 && d2(me.x, me.y, bx, by) <= CFG.kick_reach * CFG.kick_reach);

        if (kickoff) {
            // 找开球者（距球约 500 的人）
            const Ply all[4] = {me, mate, o0, o1};
            int nearest = 0;
            i64 nd = dd(all[0].x, all[0].y, bx, by);
            for (int i = 1; i < 4; ++i) {
                const i64 d = dd(all[i].x, all[i].y, bx, by);
                if (d < nd) { nd = d; nearest = i; }
            }
            const bool we_kick = (nearest == 0 || nearest == 1);
            if (we_kick && IDX == 0) {
                // 我方开球，我是踢球者：立刻选最优方向踢
                const Cand c = choose_kick(me, mate, o0, o1, bx, by, true);
                kx = c.kx; ky = c.ky;
                ax = 0; ay = 0;
                msg = dir_code(kx, ky);
            } else if (we_kick) {
                // 队友开球：前插支援（远柱侧）
                tx = clampi(7000, 500, FW - 500);
                ty = 4000 + (by >= 4000 ? -1700 : 1700);
            } else if (IDX == 0) {
                // 对方开球，我压迫：朝球当前偏前位置跑，下一步再按真实轨迹修正
                tx = 5600; ty = by;
            } else {
                // 对方开球，我守门
                tx = 850;
                ty = 4000 + clampi(by - 4000, -700, 700) * 3 / 10;
            }
        } else if (chase) {
            // ---- 追球/进攻 ----
            if (can_kick) {
                const Cand c = choose_kick(me, mate, o0, o1, bx, by, false);
                kx = c.kx; ky = c.ky;
                msg = dir_code(kx, ky);
                // 踢球同时适当跟进球方向
                tx = bx + kx / 4; ty = by + ky / 4;
            } else if (tm < 1000000) {
                if (tm == 0) { tx = bx; ty = by; }
                else { tx = pb[tm - 1].x; ty = pb[tm - 1].y; }
                // 冷却中且球在脚边：先绕到球后方（进攻反向一侧）
                if (me.cd > 0 && d2(me.x, me.y, bx, by) <= 900 * 900) {
                    tx = bx - 520; ty = by;
                }
            } else if (pg == -1 && !pb.empty()) {
                // 追不上且球将飞进本方球门：去门线封堵预测入门点
                tx = 620;
                ty = clampi(pb.back().y, 3100, 4900);
            } else {
                // 追不上：去球的预计终点/落点
                if (!pb.empty()) { tx = pb.back().x; ty = pb.back().y; }
                tx = clampi(tx, 500, FW - 500);
                ty = clampi(ty, 500, FH - 500);
            }
        } else {
            // ---- 防守/支援 ----
            if (can_kick) {
                const Cand c = choose_kick(me, mate, o0, o1, bx, by, false);
                kx = c.kx; ky = c.ky;
                msg = dir_code(kx, ky);
                tx = bx + kx / 4; ty = by + ky / 4;
            } else {
                // 威胁判断：只有快速飞向我方/已在本方半场/预测进球才回撤，慢速回退球仍可上抢
                bool threat = (bvx < -250) || (bx < 5200) || (pg == -1);
                if (MSG == 255) threat = true;  // 队友紧急回防
                if (!threat) {
                    const int to0 = itime(o0.x, o0.y, bx, by, pb, 450);
                    const int to1 = itime(o1.x, o1.y, bx, by, pb, 450);
                    const bool winnable = (tm + 2 <= to0 && tm + 2 <= to1);
                    if (winnable && tm < 1000000) {
                        // 球权在我：第二人上抢/接应（自然落在主追者后面）
                        if (tm == 0) { tx = bx; ty = by; }
                        else { tx = pb[tm - 1].x; ty = pb[tm - 1].y; }
                    } else {
                        // 球权在彼：提前回撤防守带
                        ty = clampi(4000 + clampi(by - 4000, -1400, 1400) * 6 / 10, 3200, 4800);
                        tx = clampi(bx * 35 / 100, 900, 2700);
                    }
                } else {
                    if (pg == -1 && !pb.empty()) {
                        // 球将进本方门：封远侧门柱（chaser 去预测入门点）
                        tx = 620;
                        ty = clampi(4000 + (pb.back().y >= 4000 ? -900 : 900), 3100, 4900);
                    } else {
                        // 滑动中央站位：y 跟球但向中心收缩，x 收窄（来得及回门线）
                        const i64 f = (bx < 5000) ? 75 : 55;
                        ty = 4000 + clampi(by - 4000, -1400, 1400) * f / 100;
                        ty = clampi(ty, 3200, 4800);
                        tx = clampi(bx * 32 / 100, 900, 2700);
                        if (bx < 2600) tx = clampi(bx - 650, 450, 2400);
                    }
                }
            }
        }

        if (kickoff && IDX == 0 && !(bx == 6000)) { /* 占位防止误用 */ }
        tx = clampi(tx, 350, FW - 350);
        ty = clampi(ty, 350, FH - 350);

        // ---- 加速度：朝目标全速（已有速度同向且够快则滑行）----
        {
            i64 dx = tx - me.x, dy = ty - me.y;
            const i64 len = dd(0, 0, dx, dy);
            if (len > 0) {
                const i64 dot = me.vx * dx + me.vy * dy;
                if (dot > 0 && len < 6 * dd(0, 0, me.vx, me.vy) && dd(0, 0, me.vx, me.vy) > 250) {
                    ax = 0; ay = 0;  // 滑行
                } else {
                    ax = soccer::tdiv(dx * CFG.acc_max, len);
                    ay = soccer::tdiv(dy * CFG.acc_max, len);
                    if (ax == 0 && ay == 0) {
                        ax = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
                        ay = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
                    }
                }
            }
        }

        if (kx == 0 && ky == 0) {
            if (chase) msg = 128;
            else msg = (MSG == 255) ? 129 : 129;
            // 紧急回防提示
            if (!chase && bvx < -300 && bx < 4500) msg = 255;
        }

        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << msg << std::endl;
    }
    return 0;
}
