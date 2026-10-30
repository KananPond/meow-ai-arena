// 2v2 soccer player - DeepSeek V4.1 Flash 1017
// 视角：自己向右进攻，己方门在 x=0 (y 3000..5000)，对方门在 x=W。
// 与队友约定的协议：近球者=进攻者(MSG 1)，另一人=防守者(MSG 2)；同一步只一人踢。
// 本版要点：门将站在"球门中心->球"的连线上、离门 800；进攻者追预测球位（从冷却结束起算），
// 球慢时绕到球的己方后侧；能踢就一脚 800 射向对方两人封堵最差的球门位置。
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "soccer.hpp"

using i64 = long long;
using soccer::isqrt;

static i64 W, H, GOAL, CHAMFER, RP, RB;
static i64 ACC_MAX, PF_N, PF_D, BF_N, BF_D;
static i64 KICK_MAX, KICK_REACH, KICK_CD;
static i64 STEPS, STALL, MSG_MAX, IDX;
static soccer::Config CFG;

struct P {
    i64 x = 0, y = 0, vx = 0, vy = 0, cd = 0;
};
struct Sc {
    i64 t = 0, left = 0, my = 0, opp = 0, mate_msg = 0;
    i64 bx = 0, by = 0, bvx = 0, bvy = 0;
    P pl[4];
};

static inline i64 clampi(i64 v, i64 a, i64 b) { return v < a ? a : (v > b ? b : v); }
static inline i64 dst2(i64 ax, i64 ay, i64 bx, i64 by) {
    const i64 dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

// 点 (px,py) 到线段 (ax,ay)-(bx,by) 的距离平方（只看大小，粗略取整）
static i64 seg_dist2(i64 ax, i64 ay, i64 bx, i64 by, i64 px, i64 py) {
    const i64 vx = bx - ax, vy = by - ay;
    const i64 wx = px - ax, wy = py - ay;
    const i64 vv = vx * vx + vy * vy;
    const i64 tt = wx * vx + wy * vy;
    if (vv <= 0 || tt <= 0) return wx * wx + wy * wy;
    if (tt >= vv) {
        const i64 dx = px - bx, dy = py - by;
        return dx * dx + dy * dy;
    }
    const i64 ww = wx * wx + wy * wy;
    return (ww * vv - tt * tt) / vv;
}

// 走位：期望速度 = 到目标的距离 / 3，限 380；加速度 = 期望速度/0.9 - 当前速度（浮点计算后取整）。
static void steer(const P& me, i64 tx, i64 ty, i64& ax, i64& ay) {
    double vdx = double(tx - me.x) / 3.0, vdy = double(ty - me.y) / 3.0;
    const double vl = std::hypot(vdx, vdy);
    if (vl > 380.0) {
        vdx *= 380.0 / vl;
        vdy *= 380.0 / vl;
    }
    double adx = vdx / 0.9 - double(me.vx);
    double ady = vdy / 0.9 - double(me.vy);
    const double al = std::hypot(adx, ady);
    if (al > double(ACC_MAX)) {
        adx *= double(ACC_MAX) / al;
        ady *= double(ACC_MAX) / al;
    }
    ax = i64(std::llround(adx));
    ay = i64(std::llround(ady));
}

// 射门：候选 y in [3100,4900]，选对方两人到球->(W,y) 路线最小距离最大的 y；
// 并列时选离对方"门将"(x 更大的那个) y 更远的。
static void aim_shot(const Sc& s, i64& kx, i64& ky) {
    const P& o0 = s.pl[2];
    const P& o1 = s.pl[3];
    const P& keeper = (o0.x >= o1.x) ? o0 : o1;
    i64 best_y = 4000, best_gap = -1;
    i64 best_score = -1;
    for (i64 y = 3100; y <= 4900; y += 200) {
        const i64 d0 = seg_dist2(s.bx, s.by, W, y, o0.x, o0.y);
        const i64 d1 = seg_dist2(s.bx, s.by, W, y, o1.x, o1.y);
        const i64 sc = std::min(d0, d1);
        const i64 gap = std::abs(y - keeper.y);
        if (sc > best_score || (sc == best_score && gap > best_gap)) {
            best_score = sc;
            best_gap = gap;
            best_y = y;
        }
    }
    kx = W - s.bx;
    ky = best_y - s.by;
    soccer::clamp_len(kx, ky, KICK_MAX);
}

// 从静止全速跑 k 步大约能跑的距离
static i64 reach_dist(i64 k) {
    i64 f = 1000;
    for (i64 i = 0; i < k && i < 40; ++i) f = f * 9 / 10;
    return 400 * k - 3600 * (1000 - f) / 1000;
}

// 进攻者：球正朝我方门飞（vx<-120）时先迎面拦截最近的轨迹点；
// 否则球速 >=150 时追预测球位（从踢球冷却结束那步起算）；球慢时绕到球的己方后侧 400。
static void attacker_target(const Sc& s, const P& me, i64& tx, i64& ty) {
    bool found = false;
    const i64 bsp2 = s.bvx * s.bvx + s.bvy * s.bvy;
    const bool incoming = (s.bvx < -120);
    if (incoming || bsp2 >= 150 * 150) {
        std::vector<soccer::BallPt> tr;
        soccer::predict_ball(CFG, s.bx, s.by, s.bvx, s.bvy, 40, tr);
        if (incoming) {
            for (i64 k = 0; k < i64(tr.size()); ++k) {
                const i64 d = isqrt(dst2(me.x, me.y, tr[k].x, tr[k].y));
                if (d <= reach_dist(k + 1) + 150) {
                    tx = tr[k].x;
                    ty = tr[k].y;
                    found = true;
                    break;
                }
            }
            if (!found && !tr.empty()) {
                tx = tr.back().x;
                ty = tr.back().y;
                found = true;
            }
        } else {
            const i64 k0 = (me.cd > 0) ? (me.cd - 1) : 0;
            for (i64 k = k0; k < i64(tr.size()); ++k) {
                const i64 d = isqrt(dst2(me.x, me.y, tr[k].x, tr[k].y));
                if (d - 400 <= 350 * (k + 1)) {
                    tx = tr[k].x;
                    ty = tr[k].y;
                    found = true;
                    break;
                }
            }
        }
    }
    if (!found) {
        const i64 gx = W - s.bx, gy = 4000 - s.by;
        const i64 g = isqrt(gx * gx + gy * gy);
        if (g > 1) {
            tx = s.bx - 400 * gx / g;
            ty = s.by - 400 * gy / g;
        } else {
            tx = s.bx;
            ty = s.by;
        }
    }
    tx = clampi(tx, RP, W - RP);
    ty = clampi(ty, RP, H - RP);
}

// 防守者 = 门将：站在球门中心 (0,4000) 与球的连线上、离门 800 处；不出去追球。
static void defender_target(const Sc& s, i64& tx, i64& ty) {
    const i64 gx = s.bx, gy = s.by - 4000;
    const i64 gl = isqrt(gx * gx + gy * gy);
    i64 sx = 800, sy = 4000;
    if (gl > 1) {
        sx = 800 * gx / gl;
        sy = 4000 + 800 * gy / gl;
    }
    tx = clampi(sx, RP, 1500);
    ty = clampi(sy, 3000 - 150, 5000 + 150);
    // ball flying toward our goal: move to predicted crossing at x=700
    if (s.bvx < -200) {
        std::vector<soccer::BallPt> tr;
        soccer::predict_ball(CFG, s.bx, s.by, s.bvx, s.bvy, 80, tr);
        for (size_t k = 0; k < tr.size(); ++k) {
            if (tr[k].x <= 700) {
                if (tr[k].y >= 2400 && tr[k].y <= 5600) {
                    tx = 650;
                    ty = clampi(tr[k].y, 1200, 6800);
                }
                break;
            }
        }
    }
}

int main() {
    std::ios::sync_with_stdio(false);
    std::string magic;
    i64 ver = 0;
    if (!(std::cin >> magic >> ver)) return 0;
    std::cin >> W >> H >> GOAL >> CHAMFER >> RP >> RB;
    std::cin >> ACC_MAX >> PF_N >> PF_D >> BF_N >> BF_D;
    std::cin >> KICK_MAX >> KICK_REACH >> KICK_CD;
    i64 ew_n, ew_d, eb_n, eb_d, ep_n, ep_d;
    std::cin >> ew_n >> ew_d >> eb_n >> eb_d >> ep_n >> ep_d;
    std::cin >> STEPS >> STALL >> MSG_MAX;
    i64 step_ms = 10, first_ms = 1000;
    std::cin >> IDX >> step_ms >> first_ms;

    CFG.w = W;
    CFG.h = H;
    CFG.goal = GOAL;
    CFG.chamfer = CHAMFER;
    CFG.rp = RP;
    CFG.rb = RB;
    CFG.acc_max = ACC_MAX;
    CFG.pf_n = PF_N;
    CFG.pf_d = PF_D;
    CFG.bf_n = BF_N;
    CFG.bf_d = BF_D;
    CFG.kick_max = KICK_MAX;
    CFG.kick_reach = KICK_REACH;
    CFG.kick_cooldown = KICK_CD;
    CFG.e_wall_n = ew_n;
    CFG.e_wall_d = ew_d;
    CFG.e_ball_n = eb_n;
    CFG.e_ball_d = eb_d;
    CFG.e_pp_n = ep_n;
    CFG.e_pp_d = ep_d;
    CFG.steps = STEPS;
    CFG.stall = STALL;
    CFG.msg_max = MSG_MAX;
    const i64 reach2 = KICK_REACH * KICK_REACH;

    Sc s;
    while (std::cin >> s.t >> s.left >> s.my >> s.opp >> s.mate_msg >> s.bx >> s.by >> s.bvx >> s.bvy) {
        for (int i = 0; i < 4; ++i) {
            P& p = s.pl[i];
            std::cin >> p.x >> p.y >> p.vx >> p.vy >> p.cd;
        }
        const P& me = s.pl[0];
        const P& mate = s.pl[1];

        const i64 d_me = dst2(me.x, me.y, s.bx, s.by);
        const i64 d_mate = dst2(mate.x, mate.y, s.bx, s.by);
        const bool attacker = (d_me < d_mate) || (d_me == d_mate && IDX == 0);

        auto can_kick = [&](const P& p) { return p.cd == 0 && dst2(p.x, p.y, s.bx, s.by) <= reach2; };
        const bool att_can = can_kick(attacker ? me : mate);

        bool kick = false;
        if (attacker)
            kick = can_kick(me);
        else
            kick = !att_can && s.bx < 3000 && can_kick(me);

        i64 tx = s.bx, ty = s.by;
        if (attacker)
            attacker_target(s, me, tx, ty);
        else
            defender_target(s, tx, ty);

        i64 ax = 0, ay = 0;
        steer(me, tx, ty, ax, ay);

        i64 kx = 0, ky = 0;
        if (kick) aim_shot(s, kx, ky);

        const i64 msg = attacker ? 1 : 2;
        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << msg << '\n';
        std::cout.flush();
    }
    return 0;
}


