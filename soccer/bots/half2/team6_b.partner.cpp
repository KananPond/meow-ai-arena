// partner.cpp —— 个人队的第二名球员（Claude Haiku 5.5 另写，与 player.cpp 配合）。
//
// 分工约定（与 player.cpp 必须一致，两份程序同时按同一公式判断）：
//   · 近球者（到球心距离平方较小；平局取 IDX 小的）是进攻者，另一人是防守者；
//   · 同一步只一人踢：进攻者能踢就踢；防守者只在进攻者踢不到、球在我方三分之一场内、自己能踢时解围；
//   · 射门一律全力（800），瞄准门口 y ∈ {3100, …, 4900} 中对方两人离球路最远的那个；
//   · MSG：1 = 进攻者，2 = 防守者。
//
// 走位：
//   · 防守者守在“球门中心 → 球”的连线上、离球门 800；
//   · 进攻者球速 ≥ 150 时追预测球位（从踢球冷却结束那刻起算，取最早能赶到的点），否则绕到球的己方后侧 400。

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "soccer.hpp"

namespace {

struct Body {
    long long x, y, vx, vy, cd;
};

// 两点距离的平方：整数运算，踢球距离的判断不受浮点误差影响
long long sq_dist(long long ax, long long ay, long long bx, long long by) {
    const long long dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

double clamp_to(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

// 点 (px, py) 到线段 (sx, sy)-(ex, ey) 的距离
double dist_to_segment(double px, double py, double sx, double sy, double ex, double ey) {
    const double dx = ex - sx, dy = ey - sy;
    const double len2 = dx * dx + dy * dy;
    double t = 0;
    if (len2 > 0) t = clamp_to(((px - sx) * dx + (py - sy) * dy) / len2, 0, 1);
    return std::hypot(px - (sx + t * dx), py - (sy + t * dy));
}

// 朝 (tx, ty) 走：先算出想要的速度（距离的 1/3），再反推加速度，使摩擦后的速度接近这个值
void walk_toward(const Body& me, double tx, double ty, double friction, long long acc_max, long long& ax,
                 long long& ay) {
    double want_x = (tx - me.x) / 3.0, want_y = (ty - me.y) / 3.0;
    const double want_len = std::hypot(want_x, want_y);
    if (want_len > 380) {
        want_x *= 380 / want_len;
        want_y *= 380 / want_len;
    }
    double need_x = want_x / friction - me.vx, need_y = want_y / friction - me.vy;
    const double need_len = std::hypot(need_x, need_y);
    if (need_len > acc_max) {
        need_x *= acc_max / need_len;
        need_y *= acc_max / need_len;
    }
    ax = std::llround(need_x);
    ay = std::llround(need_y);
}

// 射门向量：瞄准门口 y（3100..4900）中，对方两人离球路最远的那个；并列取离对方门将 y 更远的；限长到 kick_max
void aim_shot(const soccer::Config& cf, long long bx, long long by, const Body& foe_a, const Body& foe_b,
              long long& kx, long long& ky) {
    const Body& keeper = foe_a.x >= foe_b.x ? foe_a : foe_b;  // 对方门将 = 对方两人中 x 更大者
    double best_clear = -1, best_gap = -1;
    long long best_y = cf.h / 2;
    for (long long y = cf.gy0() + 100; y <= cf.gy1() - 100; y += 200) {
        const double clear = std::min(dist_to_segment(foe_a.x, foe_a.y, bx, by, cf.w, y),
                                      dist_to_segment(foe_b.x, foe_b.y, bx, by, cf.w, y));
        const double gap = std::fabs(double(y - keeper.y));
        const bool better = clear > best_clear + 1e-9 || (clear > best_clear - 1e-9 && gap > best_gap);
        if (better) {
            best_clear = clear;
            best_gap = gap;
            best_y = y;
        }
    }
    kx = cf.w - bx;
    ky = best_y - by;
    soccer::clamp_len(kx, ky, cf.kick_max);
}

}  // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::string magic;
    long long version;
    if (!(std::cin >> magic >> version)) return 0;

    long long W, H, GOAL, CHAMFER, RP, RB;
    long long ACC_MAX, PF_N, PF_D, BF_N, BF_D;
    long long KICK_MAX, KICK_REACH, KICK_COOLDOWN;
    long long ew_n, ew_d, eb_n, eb_d, ep_n, ep_d;
    long long STEPS, STALL, MSG_MAX;
    long long IDX, STEP_MS, FIRST_MS;
    std::cin >> W >> H >> GOAL >> CHAMFER >> RP >> RB;
    std::cin >> ACC_MAX >> PF_N >> PF_D >> BF_N >> BF_D;
    std::cin >> KICK_MAX >> KICK_REACH >> KICK_COOLDOWN;
    std::cin >> ew_n >> ew_d >> eb_n >> eb_d >> ep_n >> ep_d;
    std::cin >> STEPS >> STALL >> MSG_MAX;
    std::cin >> IDX >> STEP_MS >> FIRST_MS;

    soccer::Config cf;
    cf.w = W;
    cf.h = H;
    cf.goal = GOAL;
    cf.chamfer = CHAMFER;
    cf.rp = RP;
    cf.rb = RB;
    cf.acc_max = ACC_MAX;
    cf.pf_n = PF_N;
    cf.pf_d = PF_D;
    cf.bf_n = BF_N;
    cf.bf_d = BF_D;
    cf.kick_max = KICK_MAX;
    cf.kick_reach = KICK_REACH;
    cf.kick_cooldown = KICK_COOLDOWN;
    cf.e_wall_n = ew_n;
    cf.e_wall_d = ew_d;
    cf.e_ball_n = eb_n;
    cf.e_ball_d = eb_d;
    cf.e_pp_n = ep_n;
    cf.e_pp_d = ep_d;
    cf.steps = STEPS;
    cf.stall = STALL;
    cf.msg_max = MSG_MAX;

    const double friction = double(PF_N) / double(PF_D);
    const long long reach_sq = KICK_REACH * KICK_REACH;
    const double goal_mid_y = cf.h / 2.0;  // 己方球门中心 (0, goal_mid_y)

    long long T, LEFT, MY, OPP, MATE_MSG;
    long long bx, by, bvx, bvy;
    Body bodies[4];  // 0 自己，1 队友，2、3 对手
    std::vector<soccer::BallPt> path;
    while (std::cin >> T >> LEFT >> MY >> OPP >> MATE_MSG >> bx >> by >> bvx >> bvy) {
        for (auto& b : bodies) std::cin >> b.x >> b.y >> b.vx >> b.vy >> b.cd;
        const Body& me = bodies[0];
        const Body& mate = bodies[1];

        // 分工：近球者是进攻者
        const long long my_d2 = sq_dist(me.x, me.y, bx, by);
        const long long mate_d2 = sq_dist(mate.x, mate.y, bx, by);
        const bool i_attack = my_d2 < mate_d2 || (my_d2 == mate_d2 && IDX == 0);
        const Body& attacker = i_attack ? me : mate;

        auto can_kick = [&](const Body& b) { return b.cd == 0 && sq_dist(b.x, b.y, bx, by) <= reach_sq; };

        // 踢球权：同一步只一人踢
        bool do_kick = false;
        if (i_attack) {
            do_kick = can_kick(me);
        } else {
            do_kick = !can_kick(attacker) && bx < 3000 && can_kick(me);
        }

        // 防守位：球门中心 → 球 的连线上，离球门 800
        double goal_x = 800, goal_y = goal_mid_y;
        const double to_ball_x = bx, to_ball_y = by - goal_mid_y;
        const double to_ball_len = std::hypot(to_ball_x, to_ball_y);
        if (to_ball_len > 1) {
            goal_x = 800 * to_ball_x / to_ball_len;
            goal_y = goal_mid_y + 800 * to_ball_y / to_ball_len;
        }
        goal_x = clamp_to(goal_x, RP, 1500);
        goal_y = clamp_to(goal_y, cf.gy0() - 150, cf.gy1() + 150);
        double target_x = goal_x, target_y = goal_y;

        if (i_attack) {
            const double ball_speed = std::hypot(double(bvx), double(bvy));
            bool have_target = false;
            if (ball_speed >= 150) {
                // 球在动：从踢球冷却结束那一步起，取最早能赶到的预测球位
                soccer::predict_ball(cf, bx, by, bvx, bvy, 40, path);
                for (size_t k = size_t(std::max(me.cd - 1, 0LL)); k < path.size() && !have_target; ++k) {
                    const double reach_dist = std::hypot(double(path[k].x - me.x), double(path[k].y - me.y));
                    if (reach_dist - 400 <= 350.0 * double(k + 1)) {
                        target_x = path[k].x;
                        target_y = path[k].y;
                        have_target = true;
                    }
                }
            }
            if (!have_target) {
                // 球慢：绕到球的己方后侧（相对对方门中心），距离 400
                const double back_x = cf.w - bx, back_y = goal_mid_y - by;
                const double back_len = std::hypot(back_x, back_y);
                target_x = bx;
                target_y = by;
                if (back_len > 1) {
                    target_x = bx - 400 * back_x / back_len;
                    target_y = by - 400 * back_y / back_len;
                }
            }
        }
        target_x = clamp_to(target_x, RP, W - RP);
        target_y = clamp_to(target_y, RP, H - RP);

        long long ax = 0, ay = 0;
        walk_toward(me, target_x, target_y, friction, ACC_MAX, ax, ay);

        long long kx = 0, ky = 0;
        if (do_kick) aim_shot(cf, bx, by, bodies[2], bodies[3], kx, ky);

        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << (i_attack ? 1 : 2) << std::endl;
    }
    return 0;
}
