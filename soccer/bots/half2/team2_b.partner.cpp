// partner.cpp —— 个人队专用第二程序（阶跃 Step 5 编写）
// 定位：专职清道夫门将，与 player.cpp（进攻/逼抢手）互补。
// 设计要点（与 player.cpp 不同的一套代码）：
//   1) 球在本方半场且我比队友近 -> 主动出击断球/大脚解围
//   2) 否则守"清扫线"：x≈900，y 跟踪 predict_ball 预测的入门点（含撞墙折射），
//      球不会进门时跟球 y 但向中心收缩
//   3) 拿到球就用独立实现的简版解围评估（避免乌龙、避开对手、尽量踢向对方半场/边线）
//   4) 开球按 IDX 分工（与 player.cpp 一致：开球方0号踢/1号支援，非开球方0号压/1号守）

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
static i64 Dt[230];  // Dt[k]：从静止 k 步最大位移（与 player.cpp 同一递推，保证截球判断一致）

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

// 截球时间（与 player.cpp 相同定义，但这里自己实现一遍）
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
static int steps_to(i64 px, i64 py, i64 tx, i64 ty) {
    const i64 r = dd(px, py, tx, ty);
    for (int k = 0; k < 230; ++k)
        if (Dt[k] >= r) return k;
    return 1000000;
}

// 简版解围评估：安全性优先（不乌龙、不被对手直接截），其次向对方半场踢远
static void pick_clear(const Ply& me, const Ply& mate, const Ply& o0, const Ply& o1, i64 bx, i64 by, i64& kx,
                       i64& ky) {
    i64 best_kx = 0, best_ky = 0, best_sc = -1000000000;
    auto consider = [&](i64 tx, i64 ty) {
        i64 cx = tx - bx, cy = ty - by;
        soccer::clamp_len(cx, cy, CFG.kick_max);
        if (cx == 0 && cy == 0) return;
        std::vector<BallPt> path;
        const int g = soccer::predict_ball(CFG, bx, by, cx, cy, 150, path);
        if (g == -1) return;  // 乌龙直接排除
        const int eo = std::min(itime(o0.x, o0.y, bx, by, path, 450), itime(o1.x, o1.y, bx, by, path, 450));
        const int em = itime(mate.x, mate.y, bx, by, path, 450);
        i64 sc = 0;
        if (g == 1) sc = 4000;  // 直接进对方门
        else {
            const i64 ex = path.empty() ? bx : path.back().x;
            const i64 ey = path.empty() ? by : path.back().y;
            sc = (ex - 6000) / 2;  // 向对方场踢远
            if (eo < 1000000) sc -= 250;
            if (em < eo) sc += 200;
            if (ey < 1000 || ey > 7000) sc -= 150;  // 别往死角踢
        }
        if (sc > best_sc) { best_sc = sc; best_kx = cx; best_ky = cy; }
    };
    consider(FW - 100, 3300);
    consider(FW - 100, 4700);
    consider(FW - 100, 4000);
    consider(10000, 800);
    consider(10000, 7200);
    consider(9500, 2500);
    consider(9500, 5500);
    consider(mate.x + mate.vx * 10, mate.y + mate.vy * 10);
    consider(3000, 4000);
    if (best_sc <= -1000000000) { best_kx = CFG.kick_max; best_ky = 0; }
    kx = best_kx;
    ky = best_ky;
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

        pb.clear();
        const int pg = soccer::predict_ball(CFG, bx, by, bvx, bvy, 170, pb);

        // 开球检测
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

        i64 ax = 0, ay = 0, kx = 0, ky = 0, msg = 129;
        i64 tx = bx, ty = by;
        const bool can_kick = (me.cd == 0 && d2(me.x, me.y, bx, by) <= CFG.kick_reach * CFG.kick_reach);
        const int tm = itime(me.x, me.y, bx, by, pb, CFG.kick_reach);
        const int tn = itime(mate.x, mate.y, bx, by, pb, CFG.kick_reach);

        if (kickoff) {
            const Ply all[4] = {me, mate, o0, o1};
            int nearest = 0;
            i64 nd = dd(all[0].x, all[0].y, bx, by);
            for (int i = 1; i < 4; ++i) {
                const i64 d = dd(all[i].x, all[i].y, bx, by);
                if (d < nd) { nd = d; nearest = i; }
            }
            const bool we_kick = (nearest == 0 || nearest == 1);
            if (we_kick && IDX == 0) {
                // 我方开球且我是踢球者：大力踢向对方球门方向（简单：选最优点）
                i64 cxk = 0, cyk = 0;
                pick_clear(me, mate, o0, o1, bx, by, cxk, cyk);
                kx = cxk; ky = cyk;
            } else if (we_kick) {
                // 队友开球：门将稍上前封折射，不前插太深
                tx = 2100;
                ty = 4000 + clampi(by - 4000, -900, 900) / 2;
            } else if (IDX == 0) {
                // 对方开球，我压迫
                tx = 5600; ty = by;
            } else {
                // 对方开球，我守门
                tx = 800;
                ty = 4000 + clampi(by - 4000, -700, 700) * 3 / 10;
            }
        } else if (can_kick) {
            // 拿到球：大脚解围（简版评估）
            pick_clear(me, mate, o0, o1, bx, by, kx, ky);
            tx = bx + kx / 4; ty = by + ky / 4;
        } else if (bx < 4300 && bvx < 120 && tm <= 10 && tm < itime(o0.x, o0.y, bx, by, pb, 450) &&
                   tm < itime(o1.x, o1.y, bx, by, pb, 450)) {
            // 出击断球：仅当球在本方半场、在向我方运动/慢速、我是四方里最快的且不太远
            if (tm < 1000000) {
                if (tm == 0) { tx = bx; ty = by; }
                else { tx = pb[tm - 1].x; ty = pb[tm - 1].y; }
            } else {
                tx = clampi(bx, 500, 3000);
                ty = clampi(by, 500, FH - 500);
            }
        } else {
            // ---- 门将清扫线 ----
            if (pg == -1 && !pb.empty()) {
                // 球将进本方门：站到预测入门点（predict_ball 已含撞墙折射）
                tx = 760;
                ty = clampi(pb.back().y, 3150, 4850);
            } else {
                // 跟踪球 y（向中心收缩），x 保持清扫线
                tx = 900;
                ty = clampi(4000 + clampi(by - 4000, -1500, 1500) * 55 / 100, 3200, 4800);
                // 球在对方半场深处且速度朝对方球门：可稍微上前支援解围
                if (bx > 7500 && bvx > 200) tx = 1300;
            }
        }

        tx = clampi(tx, 350, FW - 350);
        ty = clampi(ty, 350, FH - 350);

        // 加速度：朝目标全速
        {
            i64 dx = tx - me.x, dy = ty - me.y;
            const i64 len = dd(0, 0, dx, dy);
            if (len > 0) {
                ax = soccer::tdiv(dx * CFG.acc_max, len);
                ay = soccer::tdiv(dy * CFG.acc_max, len);
                if (ax == 0 && ay == 0) {
                    ax = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
                    ay = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
                }
            }
        }

        if (kx != 0 || ky != 0) {
            // 解围方向消息（与 player.cpp 同协议：0..127 = 方向角）
            const double a = std::atan2((double)ky, (double)kx) * 64.0 / 3.14159265358979323846;
            int c = (int)std::lround(a);
            msg = ((c % 128) + 128) % 128;
        } else {
            msg = 129;  // DEFEND/SUPPORT
        }

        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << msg << std::endl;
    }
    return 0;
}
