// player.cpp —— Claude Haiku 5.5 的球员程序（第三版：进攻 + 防守，角色按共同局面分配）。
//
// 与队友约定（两人看到的球和四名球员坐标相同，所以各自算出的角色一致）：
//   · 近球者（到球心距离平方较小的我方球员；平局取 IDX 小的）是进攻者，另一人是防守者。
//   · 同一步只允许一人踢：进攻者能踢就踢；防守者只在进攻者不能踢、球在我方三分之一场内时解围。
//   · MSG：1 = 我是进攻者，2 = 我是防守者（只用于复盘对照）。
//   · 射门瞄准：y ∈ {3100, 3300, …, 4900}，选对方两人到球路线距离最大的那个。
//
// 本版的个人改动（不影响角色和踢球权的约定）：
//   · 进攻者：球速 ≥ 150 时追预测球位（从踢球冷却结束那刻起算，取最早能赶到的点），否则绕到球的己方后侧；一律全力踢。
//   · 防守者守在“球门中心 → 球”的连线上、离球门 800，不出门去截球。

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "soccer.hpp"

namespace {

struct P {
    long long x, y, vx, vy, cd;
};

long long dist2(long long ax, long long ay, long long bx, long long by) {
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
}

double clampd(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

// 点 (px, py) 到线段 (ax, ay)-(bx, by) 的距离
double seg_dist(double px, double py, double ax, double ay, double bx, double by) {
    double dx = bx - ax, dy = by - ay;
    double len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? clampd(((px - ax) * dx + (py - ay) * dy) / len2, 0, 1) : 0;
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

// 走向 (tx, ty)：目标速度 = 距离 / 3（约 3 步到），换算成加速度（下一步速度 ≈ fr × (v + a)）
void steer(const P& me, double tx, double ty, double fr, long long acc_max, long long& ax, long long& ay) {
    double vdx = (tx - me.x) / 3.0, vdy = (ty - me.y) / 3.0;
    double vl = std::hypot(vdx, vdy);
    if (vl > 380) {
        vdx *= 380 / vl;
        vdy *= 380 / vl;
    }
    double adx = vdx / fr - me.vx, ady = vdy / fr - me.vy;
    double al = std::hypot(adx, ady);
    if (al > acc_max) {
        adx *= acc_max / al;
        ady *= acc_max / al;
    }
    ax = std::llround(adx);
    ay = std::llround(ady);
}

// 踢球向量：瞄准 (w, y)，y 取 3100..4900 中对方两人离球路最远的那个（并列取离对方门将 y 更远的），限长到 power
void shot(const soccer::Config& cf, long long bx, long long by, const P& o0, const P& o1, long long power,
          long long& kx, long long& ky) {
    const P& keeper = o0.x >= o1.x ? o0 : o1;  // 对方门将 = 对方两人中 x 更大者
    double best = -1, best_gap = -1;
    long long best_y = cf.h / 2;
    for (long long y = cf.gy0() + 100; y <= cf.gy1() - 100; y += 200) {
        double s = std::min(seg_dist(o0.x, o0.y, bx, by, cf.w, y), seg_dist(o1.x, o1.y, bx, by, cf.w, y));
        double gap = std::fabs(double(y - keeper.y));
        if (s > best + 1e-9 || (s > best - 1e-9 && gap > best_gap)) {
            best = s;
            best_gap = gap;
            best_y = y;
        }
    }
    kx = cf.w - bx;
    ky = best_y - by;
    soccer::clamp_len(kx, ky, power);
}

}  // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::string magic;
    long long ver;
    if (!(std::cin >> magic >> ver)) return 0;
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

    soccer::Config cf;  // 用开局参数覆盖默认值（给 predict_ball 用）
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
    const double fr = double(PF_N) / double(PF_D);
    const long long reach2 = KICK_REACH * KICK_REACH;
    const long long gy0 = cf.gy0(), gy1 = cf.gy1();
    const double gcy = cf.h / 2.0;  // 己方球门中心 (0, gcy)

    long long T, LEFT, MY, OPP, MATE_MSG;
    long long bx, by, bvx, bvy;
    P pl[4];  // 0 自己，1 队友，2、3 对手
    std::vector<soccer::BallPt> tr;
    while (std::cin >> T >> LEFT >> MY >> OPP >> MATE_MSG >> bx >> by >> bvx >> bvy) {
        for (auto& p : pl) std::cin >> p.x >> p.y >> p.vx >> p.vy >> p.cd;
        const P& me = pl[0];
        const P& mate = pl[1];

        // 角色：近球者是进攻者（平局取 IDX 小的）
        const long long dme = dist2(me.x, me.y, bx, by);
        const long long dmate = dist2(mate.x, mate.y, bx, by);
        const bool attacker = dme < dmate || (dme == dmate && IDX == 0);
        auto can_kick = [&](const P& p) { return p.cd == 0 && dist2(p.x, p.y, bx, by) <= reach2; };
        const bool att_can_kick = can_kick(attacker ? me : mate);

        // 踢球权：同一步只一人踢（进攻者能踢就踢；防守者只在进攻者踢不到时解围）
        bool kick;
        if (attacker) {
            kick = can_kick(me);
        } else {
            kick = !att_can_kick && bx < 3000 && can_kick(me);  // 球在我方三分之一场内，解围
        }

        // 走位目标：默认守门位（球门中心 → 球 的连线上，离球门 800）
        double spot_x = 800, spot_y = gcy;
        double gux = bx, guy = by - gcy;
        double gul = std::hypot(gux, guy);
        if (gul > 1) {
            spot_x = 800 * gux / gul;
            spot_y = gcy + 800 * guy / gul;
        }
        spot_x = clampd(spot_x, RP, 1500);
        spot_y = clampd(spot_y, gy0 - 150, gy1 + 150);
        double tx = spot_x, ty = spot_y;

        if (attacker) {
            const double bspeed = std::hypot(double(bvx), double(bvy));
            bool found = false;
            if (bspeed >= 150) {
                // 球在动：追预测球位，取最早能赶到的那个（从踢球冷却结束那一刻起算）
                soccer::predict_ball(cf, bx, by, bvx, bvy, 40, tr);
                for (size_t k = size_t(std::max(me.cd - 1, 0LL)); k < tr.size() && !found; ++k) {
                    double d = std::hypot(double(tr[k].x - me.x), double(tr[k].y - me.y));
                    if (d - 400 <= 350.0 * double(k + 1)) {
                        tx = tr[k].x;
                        ty = tr[k].y;
                        found = true;
                    }
                }
            }
            if (!found) {
                // 球慢：绕到球的己方后侧（相对对方门中心）
                double gx = cf.w - bx, gy = gcy - by;
                double g = std::hypot(gx, gy);
                tx = bx;
                ty = by;
                if (g > 1) {
                    tx = bx - 400 * gx / g;
                    ty = by - 400 * gy / g;
                }
            }
        }
        tx = clampd(tx, RP, W - RP);
        ty = clampd(ty, RP, H - RP);

        long long ax, ay;
        steer(me, tx, ty, fr, ACC_MAX, ax, ay);
        long long kx = 0, ky = 0;
        if (kick) shot(cf, bx, by, pl[2], pl[3], KICK_MAX, kx, ky);
        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << (attacker ? 1 : 2) << std::endl;
    }
    return 0;
}
