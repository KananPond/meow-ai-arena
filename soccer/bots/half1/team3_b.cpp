// submission/player.cpp —— 豆包 (Doubao-Seed-Evolving)
// 2v2 简化足球。视角永远"自己向右进攻"。
// 战术（与队友 Gemini 约定）：
//   动态角色 C（主抢/主攻）/ D（协防/门将），用两边完全相同的确定性
//   截球步数函数 calc_intercept_t 判定，带 4 步滞回，MSG bit7 同步角色。
//   踢球：枚举候选方向×力度，用 soccer::predict_ball 精确模拟球路，
//   再用四人的截球步数打分（必进 > 我方先控且推进 > 解围 >> 乌龙）。
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

// 角度查找表（1000 为模），36 个方向每 10 度
static i64 CTAB[36], STAB[36];
// 球员纯加速 k 步可达半径增量 R_acc(k)（不含起步余量）
static i64 RACC[61];

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
// p+=v, v=v*9/10；不模拟墙。一旦 |p-bk[k]|<=450（rp+rb）即可触球。
// use_cd=false 时忽略踢球冷却——角色选举只看"谁先到位"。
static int intercept_t(const P& p0, int cd, const i64 bk[61][2], bool use_cd) {
    i64 x = p0.x, y = p0.y, vx = p0.vx, vy = p0.vy;
    for (int k = 1; k <= 60; ++k) {
        // 先决定朝 bk[k] 的加速
        i64 tx = bk[k][0] - x, ty = bk[k][1] - y;
        i64 td = isqrt(tx * tx + ty * ty);
        if (td > 0) {
            // 归一到 40（与引擎限长同样整数化）
            i64 ax = tx * 40 / td, ay = ty * 40 / td;
            if (ax == 0 && ay == 0) { if (tx > 0) ax = 1; else if (tx < 0) ax = -1; }
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

// PD 式追点：远处尽快达到极速，近点快速刹车（v*0.9 的刹车余量很大）
static void steer(i64 tx, i64 ty, const P& me, i64& ax, i64& ay) {
    i64 dx = tx - me.x, dy = ty - me.y;
    i64 d = isqrt(dx * dx + dy * dy);
    i64 dvx, dvy;
    if (d <= 1) {
        dvx = 0; dvy = 0;
    } else {
        // 期望速率：远 ~380，到点平滑降到 0
        i64 want = clampi(d / 3, 0, 380);
        dvx = dx * want / d;
        dvy = dy * want / d;
    }
    ax = clampi(dvx - me.vx, -40, 40);
    ay = clampi(dvy - me.vy, -40, 40);
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
    {
        i64 r = 0, v = 0;
        RACC[0] = 480;
        for (int k = 1; k <= 60; ++k) {
            v += 40;
            r += v;
            v = v * 9 / 10;
            RACC[k] = r + 480;
        }
    }

    i64 T, LEFT, MY, OPP, MATE_MSG;
    i64 bx, by, bvx, bvy;
    P pl[4];  // 0 自己，1 队友，2、3 对手

    bool prev_c = (IDX == 0);
    bool forced_c = false;  // 开球强制
    int lock = 0;           // 角色夺权锁（防对称翻转）：>0 时保持现角色

    while (std::cin >> T >> LEFT >> MY >> OPP >> MATE_MSG >> bx >> by >> bvx >> bvy) {
        for (auto& q : pl) std::cin >> q.x >> q.y >> q.vx >> q.vy >> q.cd;
        const P& me = pl[0];
        const P& mate = pl[1];

        bool kickoff = (bvx == 0 && bvy == 0 && bx == W / 2);

        // ---------- 球轨迹与四人截球步数 ----------
        i64 bk[61][2];
        int goal_dir;
        ball_points(bx, by, bvx, bvy, bk, goal_dir);

        // 角色选举用"到位时间"（忽略 cd）；踢球可行性的 cd 单独看 can_reach
        int tme = intercept_t(me, 0, bk, false);
        int tmate = intercept_t(mate, 0, bk, false);
        int to0 = intercept_t(pl[2], (int)pl[2].cd, bk, true);
        int to1 = intercept_t(pl[3], (int)pl[3].cd, bk, true);

        // ---------- 角色判定（对称稳定：不会两人同时翻面） ----------
        bool is_c;
        bool mate_prev_c = ((MATE_MSG >> 7) & 1) != 0;
        if (lock > 0) --lock;
        if (kickoff) {
            is_c = (IDX == 0);
            forced_c = is_c;
            lock = 0;
        } else if (lock > 0 && prev_c != mate_prev_c) {
            is_c = prev_c;  // 夺权确认后的稳定期保持角色
        } else {
            if (prev_c != mate_prev_c) {
                // 上一步一 C 一 D：D 快 2 步以上就夺权（对称：两人判断一致，不会同时翻）
                bool take;
                if (prev_c) take = (tmate + 2 <= tme);
                else take = (tme + 2 <= tmate);
                if (take) {
                    is_c = !prev_c;
                    lock = 6;  // 短锁，只用于扛过确认那一步的瞬时反复
                } else {
                    is_c = prev_c;
                }
            } else {
                // 冲突（两人都C，夺权确认中）或初始（都D）：纯选举，等 t 时 idx0 当 C
                is_c = (tme < tmate) || (tme == tmate && IDX == 0);
                lock = 0;
            }
        }

        // ---------- 踢球候选评估 ----------
        i64 d2me = (bx - me.x) * (bx - me.x) + (by - me.y) * (by - me.y);
        bool can_reach = (me.cd == 0 && d2me <= KICK_REACH * KICK_REACH);
        bool danger = (bx < 3000);  // 我方禁区附近
        // C 可以踢；D 只在自家危险区踢（解围），或对手正贴脸
        bool opp_on_ball = std::min(isqrt((bx - pl[2].x) * (bx - pl[2].x) + (by - pl[2].y) * (by - pl[2].y)),
                                    isqrt((bx - pl[3].x) * (bx - pl[3].x) + (by - pl[3].y) * (by - pl[3].y))) < 650;
        bool may_kick = can_reach && (is_c || danger || forced_c || (!is_c && opp_on_ball && bx < 4500));

        i64 best_score = -(1LL << 60);
        i64 best_kx = 0, best_ky = 0;
        int best_kind = 0;  // 1 射门/中路 2 上 3 下 4 传
        bool will_kick = false;

        if (may_kick) {
            auto eval = [&](i64 kx, i64 ky, bool soft) {
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
                if (g == -1) return -(1LL << 50);  // 乌龙，绝对禁止

                // 踢球当步我仍按当前速度移动（评估时保守忽略本步加速），之后 cd=5
                P meAfter;
                meAfter.x = me.x + me.vx;
                meAfter.y = me.y + me.vy;
                meAfter.vx = me.vx * 9 / 10;
                meAfter.vy = me.vy * 9 / 10;
                meAfter.cd = (int)KICK_CD;
                int tm0 = intercept_t(meAfter, (int)KICK_CD, pk, true);  // cd=5
                int tm1 = intercept_t(pl[1], (int)pl[1].cd, pk, true);
                int t0 = intercept_t(pl[2], (int)pl[2].cd, pk, true);
                int t1 = intercept_t(pl[3], (int)pl[3].cd, pk, true);
                int myBest = std::min(tm0, tm1);
                int oppBest = std::min(t0, t1);

                if (g == 1) {
                    // 对手能否在球越线前先碰到？用完整运动学截球时间判断
                    int gk = (int)o2.size();
                    int tb0 = intercept_t(pl[2], (int)pl[2].cd, pk, true);
                    int tb1 = intercept_t(pl[3], (int)pl[3].cd, pk, true);
                    int tb = std::min(tb0, tb1);
                    if (tb > gk) return 1000000000LL - gk;  // 必进
                }
                i64 score;
                // 推进分：终点越靠右、越贴门越好
                i64 fx = lx, fy = ly;
                score = fx - std::abs(fy - H / 2) * 2;
                if (myBest <= oppBest) score += 4000 + (oppBest - myBest) * 150;
                else score -= 20000 + (myBest - oppBest) * 500;  // 对手先拿到球=送球权，重罚
                // 终点在对方禁区附近加分
                if (fx > 9000 && std::abs(fy - H / 2) < 1300) score += 3000;
                if (g == 1) score += 200000;  // 可能被封堵的射门仍给较高分
                // 后场必须踢向右前方
                if (danger) {
                    if (kx <= 0) score -= 200000;
                    if (fx < bx + 600) score -= 100000;
                    if (myBest > oppBest && bx < 3500) score -= 80000;
                }
                (void)soft;
                return score;
            };

            // 开球：把球踢向对方半场的空角（远离两名对手），自己追
            if (forced_c && IDX == 0) {
                static const i64 KOS[][2] = {{700, 380}, {700, -380}, {800, 260}, {800, -260}, {600, 450}, {600, -450}};
                for (auto& v : KOS) {
                    i64 s = eval(v[0], v[1], false);
                    if (s > best_score) { best_score = s; best_kx = v[0]; best_ky = v[1]; }
                }
            }

            // 36 方向 × 3 力度
            for (int pw : {800, 600, 400}) {
                if (!is_c && pw < 600) continue;  // D 只大脚解围
                for (int a = 0; a < 36; ++a) {
                    i64 kx = pw * CTAB[a] / 1000;
                    i64 ky = pw * STAB[a] / 1000;
                    if (kx == 0 && ky == 0) continue;
                    if (!is_c && kx <= 0) continue;
                    i64 s = eval(kx, ky, pw <= 400);
                    if (s > best_score) {
                        best_score = s;
                        best_kx = kx;
                        best_ky = ky;
                    }
                }
            }
            // 瞄远门柱/近门柱的精细射门角
            for (i64 gy = 2950; gy <= 5050; gy += 260) {
                i64 dx = W - bx, dy = gy - by;
                i64 d = isqrt(dx * dx + dy * dy);
                if (d <= 0) continue;
                i64 kx = dx * 800 / d, ky = dy * 800 / d;
                i64 s = eval(kx, ky, false);
                if (s > best_score) { best_score = s; best_kx = kx; best_ky = ky; }
            }

            // 决定是否真踢：踢球总比用身体乱撞强，只要不是乌龙候选就踢
            if (forced_c && IDX == 0) {
                will_kick = true;  // 开球必踢
            } else if (!is_c) {
                // D（只在危险区获得踢球权）：向右前方大脚解围
                will_kick = (best_score > -(1LL << 45) && best_kx > 0);
            } else {
                will_kick = (best_score > -(1LL << 40));
            }

            if (will_kick) {
                if (best_ky <= -120) best_kind = 2;
                else if (best_ky >= 120) best_kind = 3;
                else best_kind = 1;
                i64 pw2 = isqrt(best_kx * best_kx + best_ky * best_ky);
                if (pw2 <= 500 && is_c) best_kind = 4;  // 轻推视作传递
            }
        }

        // ---------- 跑位目标 ----------
        i64 tx, ty;
        if (is_c) {
            if (will_kick) {
                tx = bx;
                ty = by;
            } else {
                int k = clampi(tme, 0, 60);
                tx = bk[k][0];
                ty = bk[k][1];
                // 球基本静止且我已贴球：让自己落在球的左后侧，便于下一脚朝右踢（盘带）
                if (me.cd == 0 && std::abs(bvx) + std::abs(bvy) < 60) {
                    i64 ex = bx - me.x, ey = by - me.y;
                    if (ex * ex + ey * ey < 600LL * 600LL) {
                        tx = bx - 200;
                        ty = by;
                    }
                }
            }
        } else {
            // D：永远先保证门，不随紧急逻辑去当 C（角色选举已保证唯一的 C）
            if (goal_dir == -1) {
                // 球正飞向自家门：回门线按来球高度封堵
                i64 cy = by;
                for (int k = 1; k <= 60; ++k) {
                    if (bk[k][0] <= 0) { cy = bk[k][1]; break; }
                    cy = bk[k][1];
                }
                tx = 330;
                ty = clampi(cy, 3020, 4980);
            } else if (kickoff && T < 40) {
                // 开球后保持纵深接应，不前压过中线
                tx = 3600;
                ty = H / 2 + ((by > H / 2) ? 700 : -700);
            } else if (bx < 5200) {
                // 门将/清道夫：贴门站位，y 跟球高度；球远时最多小幅前移
                tx = clampi(250 + bx / 8, 300, 900);
                ty = H / 2 + (by - H / 2) * 55 / 100;
                ty = clampi(ty, 3020, 4980);
            } else {
                // 球压在对方半场：前压到中圈附近、另一侧边路接应
                tx = clampi(bx - 3200, 2600, 5200);
                ty = (by < H / 2) ? 5700 : 2300;
            }
        }

        // 避免和队友挤同一个点：近了就沿垂直方向让一点
        {
            i64 mdx = me.x - mate.x, mdy = me.y - mate.y;
            if (mdx * mdx + mdy * mdy < 700LL * 700LL) {
                i64 side = (IDX == 0) ? -1 : 1;
                tx += side * 220;
                ty += side * 120;
            }
        }

        i64 ax, ay;
        steer(tx, ty, me, ax, ay);

        i64 kx = 0, ky = 0;
        if (will_kick) { kx = best_kx; ky = best_ky; }

        int msg = (is_c ? 128 : 0) | (will_kick ? best_kind : 0);

        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << msg << std::endl;
        prev_c = is_c;
    }
    return 0;
}
