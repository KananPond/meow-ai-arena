// submission/player.cpp —— 豆包 (Doubao-Seed-Evolving)，下半场版
// 2v2 简化足球。视角永远"自己向右进攻"。
// 与队友 Gemini 的约定：动态 C（主抢/主攻）/ D（协防/门将），
// 相同的确定性截球步数 + 2 步裕量夺权，MSG bit7 同步角色。
#include "soccer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using namespace soccer;
using i64 = long long;

struct P {
    i64 x, y, vx, vy, cd;
};

static Config CFG;
static i64 W, H;
static int IDX;

static i64 CTAB[36], STAB[36];  // 每 10 度一个方向

static i64 clampi(i64 v, i64 lo, i64 hi) { return v < lo ? lo : (v > hi ? hi : v); }

// 球未来 60 步轨迹点 bk[0..60]：bk[0]=当前，停下/进球后保持最后一点
static void ball_points(i64 bx, i64 by, i64 bvx, i64 bvy, i64 bk[61][2], int& goal) {
    std::vector<BallPt> out;
    goal = predict_ball(CFG, bx, by, bvx, bvy, 60, out);
    bk[0][0] = bx;
    bk[0][1] = by;
    i64 lx = bx, ly = by;
    if (!out.empty()) {
        lx = out.back().x;
        ly = out.back().y;
    }
    for (int k = 1; k <= 60; ++k) {
        if (k - 1 < (int)out.size()) {
            bk[k][0] = out[k - 1].x;
            bk[k][1] = out[k - 1].y;
        } else {
            bk[k][0] = lx;
            bk[k][1] = ly;
        }
    }
}

// 截球步数：完整运动学模拟。每步朝 bk[k] 全力加速 40（整数归一），
// p+=v, v=v*9/10；不模拟墙。|p-bk[k]|<=460 即触球。
// use_cd=false 时忽略踢球冷却——角色选举只看"谁先到位"。
static int intercept_t(const P& p0, int cd, const i64 bk[61][2], bool use_cd) {
    i64 x = p0.x, y = p0.y, vx = p0.vx, vy = p0.vy;
    for (int k = 1; k <= 60; ++k) {
        i64 tx = bk[k][0] - x, ty = bk[k][1] - y;
        i64 td = isqrt(tx * tx + ty * ty);
        if (td > 0) {
            i64 ax = tx * 40 / td, ay = ty * 40 / td;
            if (ax == 0 && ay == 0) {
                if (tx > 0) ax = 1;
                else if (tx < 0) ax = -1;
            }
            vx += ax;
            vy += ay;
        }
        x += vx;
        y += vy;
        vx = vx * 9 / 10;
        vy = vy * 9 / 10;
        if (!use_cd || k >= cd) {
            i64 ex = bk[k][0] - x, ey = bk[k][1] - y;
            if (ex * ex + ey * ey <= 460LL * 460LL) return k;
        }
    }
    i64 ex = bk[0][0] - p0.x, ey = bk[0][1] - p0.y;
    return 61 + isqrt(ex * ex + ey * ey) / 350;
}

// 跑向定点：期望速度随距离衰减，误差向量整体限长 40（不单独钳 x/y，避免斜跑偏角）
static void goto_point(i64 tx, i64 ty, const P& me, i64& ax, i64& ay) {
    i64 dx = tx - me.x, dy = ty - me.y;
    i64 dvx = clampi(dx / 3, -350, 350);
    i64 dvy = clampi(dy / 3, -350, 350);
    ax = dvx - me.vx;
    ay = dvy - me.vy;
    clamp_len(ax, ay, 40);
}

// 截球移动：朝第 k 步球的位置跑（k 步预测点），用漂移补偿做到精确碰面
static void goto_ball_at(int k, const i64 bk[61][2], const P& me, i64& ax, i64& ay) {
    k = clampi(k, 1, 60);
    // 无加速漂移 k 步后的位置
    i64 drx = me.x, dry = me.y, dvx = me.vx, dvy = me.vy;
    for (int i = 0; i < k; ++i) {
        drx += dvx;
        dry += dvy;
        dvx = dvx * 9 / 10;
        dvy = dvy * 9 / 10;
    }
    ax = bk[k][0] - drx;
    ay = bk[k][1] - dry;
    if (k > 1) {
        i64 d = isqrt(ax * ax + ay * ay);
        if (d > 0) {
            ax = ax * 40 / d;
            ay = ay * 40 / d;
        }
    }
    clamp_len(ax, ay, 40);
}

int main() {
    std::ios::sync_with_stdio(false);
    std::string magic;
    long long ver;
    long long GOALv, CHv, RPv, RBv;
    long long ACC_MAX, PF_N, PF_D, BF_N, BF_D;
    long long KICK_MAX, KICK_REACH, KICK_CD;
    long long ew_n, ew_d, eb_n, eb_d, ep_n, ep_d;
    long long STEPS, STALL, MSG_MAX;
    long long STEP_MS, FIRST_MS;
    if (!(std::cin >> magic >> ver)) return 0;
    std::cin >> W >> H >> GOALv >> CHv >> RPv >> RBv;
    std::cin >> ACC_MAX >> PF_N >> PF_D >> BF_N >> BF_D;
    std::cin >> KICK_MAX >> KICK_REACH >> KICK_CD;
    std::cin >> ew_n >> ew_d >> eb_n >> eb_d >> ep_n >> ep_d;
    std::cin >> STEPS >> STALL >> MSG_MAX;
    std::cin >> IDX >> STEP_MS >> FIRST_MS;
    CFG.w = W; CFG.h = H; CFG.goal = GOALv; CFG.chamfer = CHv; CFG.rp = RPv; CFG.rb = RBv;
    CFG.acc_max = ACC_MAX; CFG.pf_n = PF_N; CFG.pf_d = PF_D; CFG.bf_n = BF_N; CFG.bf_d = BF_D;
    CFG.kick_max = KICK_MAX; CFG.kick_reach = KICK_REACH; CFG.kick_cooldown = KICK_CD;
    CFG.e_wall_n = ew_n; CFG.e_wall_d = ew_d; CFG.e_ball_n = eb_n; CFG.e_ball_d = eb_d;
    CFG.e_pp_n = ep_n; CFG.e_pp_d = ep_d; CFG.steps = STEPS; CFG.stall = STALL; CFG.msg_max = MSG_MAX;

    for (int i = 0; i < 36; ++i) {
        double a = i * 10.0 * 3.14159265358979323846 / 180.0;
        CTAB[i] = (i64)std::llround(std::cos(a) * 1000.0);
        STAB[i] = (i64)std::llround(std::sin(a) * 1000.0);
    }

    i64 T, LEFT, MY, OPP, MATE_MSG;
    i64 bx, by, bvx, bvy;
    P pl[4];  // 0 自己，1 队友，2、3 对手

    bool prev_c = (IDX == 0);
    bool forced_c = false;
    int ko_lock = 0;       // 开球后角色锁定剩余步数
    bool ko_i_am_c = false;  // 本次开球锁定中我是不是 C

    while (std::cin >> T >> LEFT >> MY >> OPP >> MATE_MSG >> bx >> by >> bvx >> bvy) {
        for (auto& q : pl) std::cin >> q.x >> q.y >> q.vx >> q.vy >> q.cd;
        const P& me = pl[0];
        const P& mate = pl[1];
        bool kickoff = (bvx == 0 && bvy == 0 && bx == W / 2);

        // ---------- 球轨迹与截球步数 ----------
        i64 bk[61][2];
        int goal_dir;
        ball_points(bx, by, bvx, bvy, bk, goal_dir);

        int tme = intercept_t(me, 0, bk, false);
        int tmate = intercept_t(mate, 0, bk, false);
        int to0 = intercept_t(pl[2], (int)pl[2].cd, bk, true);
        int to1 = intercept_t(pl[3], (int)pl[3].cd, bk, true);
        int to = std::min(to0, to1);

        // ---------- 角色（无状态对称选举：两个相同程序也能正确分工） ----------
        // 每人独立算同一件事：截球 t 小者当 C；平局用开球时离球更近者破。
        bool is_c;
        if (kickoff) {
            i64 dm = (bx - me.x) * (bx - me.x) + (by - me.y) * (by - me.y);
            i64 dt = (bx - mate.x) * (bx - mate.x) + (by - mate.y) * (by - mate.y);
            ko_i_am_c = (dm <= dt);
            is_c = ko_i_am_c;
            forced_c = is_c;
        } else {
            forced_c = false;
            is_c = (tme < tmate) || (tme == tmate && ko_i_am_c);
        }

        // ---------- 踢球候选评估 ----------
        i64 d2me = (bx - me.x) * (bx - me.x) + (by - me.y) * (by - me.y);
        bool can_reach = (me.cd == 0 && d2me <= KICK_REACH * KICK_REACH);
        bool danger = (bx < 3000);
        // 够得着就踢：C 全局可踢；D 在我方半场可踢（解围）。强制开球者必踢。
        bool may_kick = can_reach && (is_c || forced_c || bx < 5200);

        i64 best_score = -(1LL << 60);
        i64 best_kx = 0, best_ky = 0;
        bool will_kick = false;

        if (may_kick) {
            auto eval = [&](i64 kx, i64 ky) -> i64 {
                clamp_len(kx, ky, KICK_MAX);
                std::vector<BallPt> o2;
                int g = predict_ball(CFG, bx, by, kx, ky, 60, o2);
                i64 pk[61][2];
                pk[0][0] = bx; pk[0][1] = by;
                i64 lx = bx, ly = by;
                if (!o2.empty()) { lx = o2.back().x; ly = o2.back().y; }
                for (int k = 1; k <= 60; ++k) {
                    if (k - 1 < (int)o2.size()) { pk[k][0] = o2[k - 1].x; pk[k][1] = o2[k - 1].y; }
                    else { pk[k][0] = lx; pk[k][1] = ly; }
                }
                if (g == -1) return -(1LL << 50);  // 乌龙绝对禁止

                // 踢球当步我仍按当前速度滑一步，之后 cd=5
                P meAfter;
                meAfter.x = me.x + me.vx;
                meAfter.y = me.y + me.vy;
                meAfter.vx = me.vx * 9 / 10;
                meAfter.vy = me.vy * 9 / 10;
                meAfter.cd = (int)KICK_CD;
                int tm0 = intercept_t(meAfter, (int)KICK_CD, pk, true);
                int tm1 = intercept_t(pl[1], (int)pl[1].cd, pk, true);
                int t0 = intercept_t(pl[2], (int)pl[2].cd, pk, true);
                int t1 = intercept_t(pl[3], (int)pl[3].cd, pk, true);
                int myBest = std::min(tm0, tm1);
                int oppBest = std::min(t0, t1);

                if (g == 1) {
                    int gk = (int)o2.size();
                    if (oppBest > gk) return 1000000000LL - gk;  // 对手拦不住，必进
                }
                i64 fx = lx, fy = ly;
                i64 score = fx - std::abs(fy - H / 2) * 2;
                if (myBest <= oppBest) score += 4000 + (oppBest - myBest) * 150;
                else score -= 20000 + (myBest - oppBest) * 500;  // 送球权重罚
                if (fx > 9000 && std::abs(fy - H / 2) < 1300) score += 3000;
                if (g == 1) score += 200000;  // 可能被挡的射门仍优先
                if (danger) {
                    if (kx <= 0) score -= 200000;
                    if (fx < bx + 600) score -= 100000;
                    if (myBest > oppBest && bx < 3500) score -= 80000;
                }
                return score;
            };

            if (forced_c) {
                // 开球：正前方中等力度推，自己锁定为 C 追上去
                best_score = 1LL << 40;
                best_kx = 460;
                best_ky = 0;
            }

            // 36 方向 × 3 力度（D 只大脚向右前解围）
            for (i64 pw : {800LL, 600LL, 400LL}) {
                if (!is_c && pw < 600) continue;
                for (int a = 0; a < 36; ++a) {
                    i64 kx = pw * CTAB[a] / 1000;
                    i64 ky = pw * STAB[a] / 1000;
                    if (kx == 0 && ky == 0) continue;
                    if (!is_c && kx < 200) continue;
                    i64 s = eval(kx, ky);
                    if (s > best_score) { best_score = s; best_kx = kx; best_ky = ky; }
                }
            }
            // 精细射门角：瞄门柱之间一系列点
            for (i64 gy = 2950; gy <= 5050; gy += 260) {
                i64 dx = W - bx, dy = gy - by;
                i64 d = isqrt(dx * dx + dy * dy);
                if (d <= 0) continue;
                i64 kx = dx * 800 / d, ky = dy * 800 / d;
                i64 s = eval(kx, ky);
                if (s > best_score) { best_score = s; best_kx = kx; best_ky = ky; }
            }

            if (forced_c) {
                will_kick = true;
            } else if (!is_c) {
                will_kick = (best_score > -(1LL << 45) && best_kx > 0);
            } else {
                will_kick = (best_score > -(1LL << 40));
            }
        }

        // ---------- 跑位 ----------
        i64 ax, ay;
        int best_kind = (best_ky <= -120) ? 2 : (best_ky >= 120 ? 3 : 1);

        if (is_c) {
            if (will_kick) {
                // 踢球步：基本停在原地，给一点点向球的修正
                goto_point(bx, by, me, ax, ay);
            } else if (can_reach && me.cd == 0 && std::abs(bvx) + std::abs(bvy) < 60) {
                // 静止球且够得着：落到球左后侧，下一脚向右踢
                goto_point(bx - 200, by, me, ax, ay);
            } else if (to < tme && to <= 15) {
                // 对手先碰到球：不追虚幻点，去封堵对手触球点朝我方门的一侧
                i64 sx = bk[clampi(to, 0, 60)][0] - 400;
                i64 sy = bk[clampi(to, 0, 60)][1];
                goto_point(sx, sy, me, ax, ay);
            } else {
                // 追球：朝球的短预测点直接跑（朴素、不绕路）
                i64 bdx = bx - me.x, bdy = by - me.y;
                i64 bd = isqrt(bdx * bdx + bdy * bdy);
                int lk = bd > 1400 ? 3 : (bd > 700 ? 2 : 1);
                i64 tx = bk[lk][0], ty = bk[lk][1];
                i64 dx = tx - me.x, dy = ty - me.y, d = isqrt(dx * dx + dy * dy);
                if (d > 0) {
                    ax = dx * 40 / d;
                    ay = dy * 40 / d;
                    if (ax == 0 && ay == 0) ax = dx > 0 ? 1 : -1;
                    clamp_len(ax, ay, 40);
                } else { ax = 0; ay = 0; }
            }
        } else {
            // D：默认贴门线当门将，只在我方稳控前场球时前压
            bool secure_attack = (tmate < to && bx > 7000 && bvx >= -50);
            // 球快速逼近我方门、且 C 明显追不上 → 门将沿球路前出拦截
            bool rush_save = false;
            int rush_k = 0;
            if (bvx < -100 && bx < 5600 && tmate > tme + 1) {
                // 球到 x≈700 的那一步；我 k 步能跑到就出击
                for (int k = 2; k <= 45; ++k) {
                    if (bk[k][0] > 700) continue;
                    i64 gx = bk[k][0] < 150 ? bk[k - 1][0] : bk[k][0];
                    i64 gy = bk[k][0] < 150 ? bk[k - 1][1] : bk[k][1];
                    i64 dx = gx - me.x, dy = gy - me.y;
                    i64 d = isqrt(dx * dx + dy * dy);
                    if (d <= 300LL * k && gx > 200) { rush_save = true; rush_k = k; }
                    break;
                }
            }
            if (kickoff && T < 25) {
                goto_point(3200, H / 2 + ((by > H / 2) ? 600 : -600), me, ax, ay);
            } else if (rush_save) {
                goto_ball_at(rush_k, bk, me, ax, ay);
            } else if (can_reach && me.cd == 0) {
                goto_point(bx, by, me, ax, ay);  // 球到脚下直接解围
            } else if (secure_attack) {
                i64 sx = clampi(bx - 3000, 3200, 5200);
                i64 sy = (by < H / 2) ? 5600 : 2400;
                goto_point(sx, sy, me, ax, ay);
            } else {
                // 门将站位：球与门心(0,4000)连线上，深度随球 x 前出
                // 取预测球点（12 步后或到门线）作为"球"
                int lk = 8;
                i64 qx = bk[lk][0], qy = bk[lk][1];
                i64 tx, ty;
                if (qx <= 400) {
                    tx = 400;
                    ty = qy;
                } else {
                    // 在 (0,4000)→(qx,qy) 线段上，取 x = depth
                    i64 depth = clampi(300 + qx / 6, 380, 1100);
                    tx = depth;
                    ty = H / 2 + (qy - H / 2) * depth / qx;
                }
                goto_point(tx, clampi(ty, 2950, 5050), me, ax, ay);
            }
        }

        // 避免和队友叠在一起
        {
            i64 mdx = me.x - mate.x, mdy = me.y - mate.y;
            if (mdx * mdx + mdy * mdy < 650LL * 650LL) {
                i64 side = (IDX == 0) ? -1 : 1;
                ax += side * 14;
                ay += side * 10;
                clamp_len(ax, ay, 40);
            }
        }

        i64 kx = will_kick ? best_kx : 0;
        i64 ky = will_kick ? best_ky : 0;
        int msg = (is_c ? 128 : 0) | (will_kick ? best_kind : 0);
        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << msg << std::endl;
        prev_c = is_c;
    }
    return 0;
}
