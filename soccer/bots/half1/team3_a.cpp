#include "soccer.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using soccer::i64;

static constexpr int MAX_H = 75;

struct DriftTable {
    i64 dx[4][MAX_H + 1];
    i64 dy[4][MAX_H + 1];
    i64 r_acc[MAX_H + 1];
};

inline void build_drift_table(const soccer::Obs& obs, DriftTable& dt) {
    i64 r = 0, v = 0;
    dt.r_acc[0] = 0;
    for (int k = 1; k <= MAX_H; ++k) {
        v += 40;
        r += v;
        v = soccer::tdiv(v * 9, 10);
        dt.r_acc[k] = r;
    }
    for (int p = 0; p < 4; ++p) {
        i64 x = obs.pl[p].x, y = obs.pl[p].y;
        i64 vx = obs.pl[p].vx, vy = obs.pl[p].vy;
        dt.dx[p][0] = x;
        dt.dy[p][0] = y;
        for (int k = 1; k <= MAX_H; ++k) {
            x += vx;
            y += vy;
            vx = soccer::tdiv(vx * 9, 10);
            vy = soccer::tdiv(vy * 9, 10);
            dt.dx[p][k] = x;
            dt.dy[p][k] = y;
        }
    }
}

// 角色选举专用截球步数估算（与队友代码逐位对齐：60步，余量480，use_cd 控制是否看冷却）
inline int calc_intercept_t(const soccer::PlayerView& p, i64 bx, i64 by, const std::vector<soccer::BallPt>& traj,
                            bool use_cd = true) {
    i64 dx = p.x, dy = p.y, dvx = p.vx, dvy = p.vy;
    i64 r = 0, v = 0;
    for (int k = 0; k <= 60; ++k) {
        i64 bkx = bx, bky = by;
        if (k >= 1 && !traj.empty()) {
            int idx = std::min((int)traj.size() - 1, k - 1);
            bkx = traj[idx].x;
            bky = traj[idx].y;
        }
        i64 rk = (k == 0 && use_cd) ? 500 : (r + 480);
        if (!use_cd || k >= p.cd) {
            i64 ddx = bkx - dx, ddy = bky - dy;
            if (ddx * ddx + ddy * ddy <= rk * rk) return k;
        }
        dx += dvx;
        dy += dvy;
        dvx = soccer::tdiv(dvx * 9, 10);
        dvy = soccer::tdiv(dvy * 9, 10);
        v += 40;
        r += v;
        v = soccer::tdiv(v * 9, 10);
    }
    i64 ddx = bx - p.x, ddy = by - p.y;
    return 61 + (int)(soccer::isqrt(ddx * ddx + ddy * ddy) / 350);
}

// 对手最早触球（踢球或身体封堵）步数
inline int calc_opp_touch_t(const soccer::Obs& obs, const DriftTable& dt, int p, const std::vector<soccer::BallPt>& traj) {
    const auto& pl = obs.pl[p];
    if (pl.cd == 0) {
        i64 dx0 = obs.bx - pl.x, dy0 = obs.by - pl.y;
        if (dx0 * dx0 + dy0 * dy0 <= 500 * 500) return 0;
    }
    int n = (int)traj.size();
    for (int k = 1; k <= 60; ++k) {
        i64 bkx = obs.bx, bky = obs.by;
        if (n > 0) {
            int idx = std::min(n - 1, k - 1);
            bkx = traj[idx].x;
            bky = traj[idx].y;
        }
        i64 rk = dt.r_acc[k] + (k >= pl.cd ? 500 : 450);
        i64 ddx = bkx - dt.dx[p][k], ddy = bky - dt.dy[p][k];
        if (ddx * ddx + ddy * ddy <= rk * rk) return k;
    }
    i64 ddx = obs.bx - pl.x, ddy = obs.by - pl.y;
    return 61 + (int)(soccer::isqrt(ddx * ddx + ddy * ddy) / 350);
}

inline void clamp_field_pos(i64& tx, i64& ty) {
    tx = std::clamp(tx, 340LL, 11660LL);
    ty = std::clamp(ty, 340LL, 7660LL);
    if (tx + ty < 1440) {
        i64 diff = 1440 - (tx + ty);
        tx += (diff + 1) / 2;
        ty += (diff + 1) / 2;
    }
    if ((12000 - tx) + ty < 1440) {
        i64 diff = 1440 - ((12000 - tx) + ty);
        tx -= (diff + 1) / 2;
        ty += (diff + 1) / 2;
    }
    if (tx + (8000 - ty) < 1440) {
        i64 diff = 1440 - (tx + (8000 - ty));
        tx += (diff + 1) / 2;
        ty -= (diff + 1) / 2;
    }
    if ((12000 - tx) + (8000 - ty) < 1440) {
        i64 diff = 1440 - ((12000 - tx) + (8000 - ty));
        tx -= (diff + 1) / 2;
        ty -= (diff + 1) / 2;
    }
}

// 计算朝向 k 步后的目标点 (tx, ty) 所需的本步加速度 (ax, ay)
inline void steer_to_k(const soccer::PlayerView& me, const DriftTable& dt, int k, i64 tx, i64 ty, i64& ax, i64& ay) {
    clamp_field_pos(tx, ty);
    k = std::clamp(k, 1, MAX_H);
    i64 ddx = tx - dt.dx[0][k];
    i64 ddy = ty - dt.dy[0][k];
    if (ddx == 0 && ddy == 0) {
        ax = 0;
        ay = 0;
        return;
    }
    if (k == 1) {
        ax = ddx;
        ay = ddy;
        soccer::clamp_len(ax, ay, 40);
        return;
    }
    double d = std::hypot((double)ddx, (double)ddy);
    if (d < 1.0) {
        ax = 0;
        ay = 0;
        return;
    }
    ax = std::llround(ddx / d * 40.0);
    ay = std::llround(ddy / d * 40.0);
    soccer::clamp_len(ax, ay, 40);
}

// 定点跑位并平滑减速停住
inline void steer_to_stop(const soccer::PlayerView& me, i64 tx, i64 ty, i64& ax, i64& ay) {
    clamp_field_pos(tx, ty);
    i64 dx1 = tx - (me.x + me.vx);
    i64 dy1 = ty - (me.y + me.vy);
    if (dx1 * dx1 + dy1 * dy1 <= 40 * 40 && me.vx * me.vx + me.vy * me.vy <= 70 * 70) {
        ax = dx1;
        ay = dy1;
        soccer::clamp_len(ax, ay, 40);
        return;
    }

    double ddx = double(tx - me.x) - 3.0 * double(me.vx);
    double ddy = double(ty - me.y) - 3.0 * double(me.vy);
    double d = std::hypot(ddx, ddy);
    if (d < 1.0) {
        ax = 0;
        ay = 0;
        return;
    }
    double dist_now = std::hypot(double(tx - me.x), double(ty - me.y));
    double mag = (dist_now > 140.0) ? 40.0 : std::min(40.0, d * 0.6);
    ax = std::llround(ddx / d * mag);
    ay = std::llround(ddy / d * mag);
    soccer::clamp_len(ax, ay, 40);
}

struct KickChoice {
    i64 kx = 0, ky = 0;
    int msg_low = 0;
    double score = -1e18;
    bool self_follow = false;
    int follow_step = 6;
    i64 follow_x = 6000, follow_y = 4000;
};

inline bool would_self_bump_badly(const soccer::Config& cfg, const soccer::Obs& obs, i64 ax, i64 ay, i64 kx, i64 ky) {
    const auto& me = obs.pl[0];
    i64 px1 = me.x + me.vx + ax;
    i64 py1 = me.y + me.vy + ay;
    i64 pvx1 = me.vx + ax;
    i64 pvy1 = me.vy + ay;
    i64 bx1 = obs.bx + kx;
    i64 by1 = obs.by + ky;
    i64 dx = bx1 - px1, dy = by1 - py1;
    i64 R = cfg.rp + cfg.rb;
    if (dx * dx + dy * dy >= R * R) return false;
    i64 rvx = kx - pvx1, rvy = ky - pvy1;
    return (rvx * dx + rvy * dy) < 0;
}

inline KickChoice choose_kick(const soccer::Config& cfg, const soccer::Obs& obs, const DriftTable& dt,
                              i64 plan_ax, i64 plan_ay) {
    KickChoice best;
    const auto& me = obs.pl[0];
    const auto& mate = obs.pl[1];

    i64 ddx0 = obs.bx - me.x, ddy0 = obs.by - me.y;
    if (me.cd > 0 || ddx0 * ddx0 + ddy0 * ddy0 > cfg.kick_reach * cfg.kick_reach) {
        return best;
    }

    bool opp_can_kick_now = false;
    for (int p = 2; p < 4; ++p) {
        i64 odx = obs.bx - obs.pl[p].x, ody = obs.by - obs.pl[p].y;
        if (obs.pl[p].cd == 0 && odx * odx + ody * ody <= cfg.kick_reach * cfg.kick_reach) {
            opp_can_kick_now = true;
        }
    }

    std::vector<std::pair<i64, i64>> cands;
    cands.reserve(280);

    auto add_dir_speed = [&](double dx, double dy, double spd) {
        double g = std::hypot(dx, dy);
        if (g < 1e-6) return;
        i64 kx = std::llround(dx / g * spd);
        i64 ky = std::llround(dy / g * spd);
        soccer::clamp_len(kx, ky, cfg.kick_max);
        if (kx == 0 && ky == 0) return;
        cands.push_back({kx, ky});
    };

    // 1. 直接瞄准对方球门各点 + 上下墙一次反弹镜像球门点（满力 800）
    for (i64 gy = 3060; gy <= 4940; gy += 110) {
        add_dir_speed(double(cfg.w - obs.bx), double(gy - obs.by), 800.0);
        for (double scale : {0.8, 1.0, 1.25}) {
            double vgy_top = 150.0 - (gy - 150.0) * scale;
            add_dir_speed(double(cfg.w - obs.bx), vgy_top - double(obs.by), 800.0);
            double vgy_bot = 7850.0 + (7850.0 - gy) * scale;
            add_dir_speed(double(cfg.w - obs.bx), vgy_bot - double(obs.by), 800.0);
        }
    }

    // 2. 72 个均匀角度满力 800
    constexpr double PI = 3.14159265358979323846;
    for (int i = 0; i < 72; ++i) {
        double ang = (2.0 * PI * i) / 72.0;
        add_dir_speed(std::cos(ang), std::sin(ang), 800.0);
    }

    // 3. 中小力度传球/盘带推进（180/240 力度实现6步冷却期满速自我跟进盘带！）
    if (!opp_can_kick_now && obs.bx > 2000) {
        for (double spd : {580.0, 420.0, 290.0, 220.0, 170.0}) {
            for (int i = 0; i < 36; ++i) {
                double ang = (2.0 * PI * i) / 36.0;
                if (obs.bx < 4800 && std::cos(ang) < 0.0) continue;
                if (obs.bx >= 4800 && std::cos(ang) < -0.55) continue;
                add_dir_speed(std::cos(ang), std::sin(ang), spd);
            }
            add_dir_speed(double(mate.x + 900 - obs.bx), double(mate.y - obs.by), spd);
            add_dir_speed(double(mate.x + 1800 - obs.bx), double(mate.y - obs.by), spd);
        }
    }

    soccer::Wall walls[8];
    const int nwalls = soccer::make_walls(cfg, walls);
    const i64 gy0 = cfg.gy0(), gy1 = cfg.gy1();

    i64 px1[4], py1[4], pvx1[4], pvy1[4];
    px1[0] = me.x + me.vx + plan_ax;
    py1[0] = me.y + me.vy + plan_ay;
    pvx1[0] = me.vx + plan_ax;
    pvy1[0] = me.vy + plan_ay;
    for (int p = 1; p < 4; ++p) {
        double ddx = double(obs.bx - obs.pl[p].x), ddy = double(obs.by - obs.pl[p].y);
        double d = std::hypot(ddx, ddy);
        i64 oax = d > 1.0 ? std::llround(ddx / d * 40.0) : 0;
        i64 oay = d > 1.0 ? std::llround(ddy / d * 40.0) : 0;
        soccer::clamp_len(oax, oay, 40);
        pvx1[p] = obs.pl[p].vx + oax;
        pvy1[p] = obs.pl[p].vy + oay;
        px1[p] = obs.pl[p].x + pvx1[p];
        py1[p] = obs.pl[p].y + pvy1[p];
    }

    std::vector<soccer::BallPt> traj;
    traj.reserve(MAX_H);

    for (const auto& kv : cands) {
        i64 kx = kv.first, ky = kv.second;
        if (obs.bx < 3200 && kx < 80) continue;
        if (obs.bx < 4800 && kx < -50) continue;

        i64 bx = obs.bx + kx, by = obs.by + ky;
        i64 bvx = kx, bvy = ky;

        // 检查第 0 步 ball_players 碰撞（对手贴身封堵）
        const i64 R = cfg.rp + cfg.rb;
        const i64 x0 = bx, y0 = by, vx0 = bvx, vy0 = bvy;
        bool hit_opp_step0 = false;
        for (int p = 1; p < 4; ++p) {
            i64 dx = x0 - px1[p], dy = y0 - py1[p];
            i64 d2 = dx * dx + dy * dy;
            if (d2 >= R * R) continue;
            if (d2 == 0) dx = 0, dy = 1, d2 = 1;
            const i64 d = soccer::isqrt(d2);
            const i64 pen = R - d + 1;
            const i64 rvx = vx0 - pvx1[p], rvy = vy0 - pvy1[p];
            const i64 dot = rvx * dx + rvy * dy;
            bx += soccer::tdiv(pen * dx, d);
            by += soccer::tdiv(pen * dy, d);
            if (dot < 0) {
                bvx -= soccer::tdiv((cfg.e_ball_d + cfg.e_ball_n) * dot * dx, cfg.e_ball_d * d2);
                bvy -= soccer::tdiv((cfg.e_ball_d + cfg.e_ball_n) * dot * dy, cfg.e_ball_d * d2);
                if (p >= 2) hit_opp_step0 = true;
            }
        }
        if (would_self_bump_badly(cfg, obs, plan_ax, plan_ay, kx, ky)) {
            if (would_self_bump_badly(cfg, obs, 0, 0, kx, ky)) continue;
        }

        int goal_res = 0;
        int goal_step = 999;
        if (gy0 <= by && by <= gy1) {
            if (bx < 0) goal_res = -1, goal_step = 1;
            else if (bx > cfg.w) goal_res = 1, goal_step = 1;
        }
        if (goal_res == -1) continue;

        traj.clear();
        if (goal_res == 0) {
            soccer::wall_collide(walls, nwalls, gy0, gy1, true, cfg.rb, cfg.e_wall_n, cfg.e_wall_d, bx, by, bvx, bvy);
            bvx = soccer::tdiv(bvx * cfg.bf_n, cfg.bf_d);
            bvy = soccer::tdiv(bvy * cfg.bf_n, cfg.bf_d);
            traj.push_back({bx, by, bvx, bvy});

            i64 x = bx, y = by, vx = bvx, vy = bvy;
            for (int k = 1; k < MAX_H; ++k) {
                x += vx;
                y += vy;
                if (gy0 <= y && y <= gy1 && (x < 0 || x > cfg.w)) {
                    traj.push_back({x, y, 0, 0});
                    goal_res = (x > cfg.w) ? 1 : -1;
                    goal_step = k + 1;
                    break;
                }
                soccer::wall_collide(walls, nwalls, gy0, gy1, true, cfg.rb, cfg.e_wall_n, cfg.e_wall_d, x, y, vx, vy);
                vx = soccer::tdiv(vx * cfg.bf_n, cfg.bf_d);
                vy = soccer::tdiv(vy * cfg.bf_n, cfg.bf_d);
                traj.push_back({x, y, vx, vy});
                if (vx == 0 && vy == 0) break;
            }
        } else {
            traj.push_back({bx, by, 0, 0});
        }

        if (goal_res == -1) continue;

        int t_me = 999, t_mate = 999, t_opp = 999;
        double min_opp_dist_margin = 1e9;
        int n_pts = (int)traj.size();

        for (int step = 1; step <= MAX_H; ++step) {
            const auto& bp = traj[std::min(step - 1, n_pts - 1)];
            if (bp.x <= cfg.w && bp.x >= 0) {
                for (int p = 2; p < 4; ++p) {
                    i64 reach = dt.r_acc[step] + (step >= obs.pl[p].cd ? 500 : 450);
                    i64 ddx = bp.x - dt.dx[p][step], ddy = bp.y - dt.dy[p][step];
                    i64 d2 = ddx * ddx + ddy * ddy;
                    if (d2 <= reach * reach) {
                        if (step < t_opp) t_opp = step;
                    }
                    if (goal_res == 1 && step < goal_step) {
                        double margin = std::sqrt((double)d2) - double(reach);
                        if (margin < min_opp_dist_margin) min_opp_dist_margin = margin;
                    }
                }
            }
            if (t_mate == 999 && step >= mate.cd && bp.x <= cfg.w) {
                i64 reach = dt.r_acc[step] + 465;
                i64 ddx = bp.x - dt.dx[1][step], ddy = bp.y - dt.dy[1][step];
                if (ddx * ddx + ddy * ddy <= reach * reach) t_mate = step;
            }
            if (t_me == 999 && step >= 6 && bp.x <= cfg.w) {
                i64 reach = dt.r_acc[step] + 465;
                i64 ddx = bp.x - dt.dx[0][step], ddy = bp.y - dt.dy[0][step];
                if (ddx * ddx + ddy * ddy <= reach * reach) t_me = step;
            }
        }
        if (hit_opp_step0) t_opp = 1;

        double sc = 0.0;
        int msg_low = 1;
        bool self_follow = false;
        int follow_step = 6;
        i64 follow_x = obs.bx, follow_y = obs.by;

        if (goal_res == 1 && t_opp >= goal_step) {
            // 必进球！
            sc = 1000000.0 - goal_step * 2500.0 + std::clamp(min_opp_dist_margin, 0.0, 1500.0) * 5.0;
            msg_low = 1;
        } else if (goal_res == 1 && t_opp >= goal_step - 2 && obs.bx > 4200) {
            // 高威胁射门
            sc = 260000.0 + (t_opp - goal_step) * 35000.0 - goal_step * 1500.0 + min_opp_dist_margin * 10.0;
            msg_low = 1;
        } else {
            int t_us = std::min(t_me, t_mate);
            int t_first = std::min({t_us, t_opp, MAX_H});
            const auto& target_pt = traj[std::min(std::max(0, t_first - 1), n_pts - 1)];
            i64 ix = target_pt.x, iy = target_pt.y;

            if (t_us < t_opp) {
                int margin = std::min(8, t_opp - t_us);
                sc = 150000.0 + margin * 2400.0;
                double dist_goal = std::hypot(double(12000 - ix), double(4000 - iy));
                sc += ix * 8.5 - dist_goal * 5.0 - t_us * 350.0;

                if (ix > 7200 && ix < 11200 && iy > 1600 && iy < 6400) {
                    sc += 6500.0;
                }
                if (ix > 11100 && (iy < 1500 || iy > 6500)) sc -= 15000.0;
                if (ix < 3500 && std::abs(iy - 4000) < 2400) sc -= 30000.0;

                if (t_mate < t_me) {
                    msg_low = 4;  // 传给队友
                    if (ix > me.x + 800) sc += 3000.0;
                } else {
                    self_follow = true;
                    follow_step = t_me;
                    follow_x = ix;
                    follow_y = iy;
                    msg_low = (ky < -150) ? 2 : (ky > 150 ? 3 : 1);
                }
            } else {
                // 受迫解围（绝不能高于我方控球分！）
                sc = -200000.0 + ix * 10.0 + t_opp * 1500.0 + std::abs(iy - 4000) * 2.0;
                if (ix < 4800 && std::abs(iy - 4000) < 2500) sc -= 45000.0;
                msg_low = (ky < -150) ? 2 : (ky > 150 ? 3 : 1);
            }
        }

        if (sc > best.score) {
            best.score = sc;
            best.kx = kx;
            best.ky = ky;
            best.msg_low = msg_low;
            best.self_follow = self_follow;
            best.follow_step = follow_step;
            best.follow_x = follow_x;
            best.follow_y = follow_y;
        }
    }
    return best;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string magic;
    i64 ver;
    soccer::Config cfg;
    i64 step_ms, first_ms;
    int my_idx = 0;

    if (!(std::cin >> magic >> ver)) return 0;
    std::cin >> cfg.w >> cfg.h >> cfg.goal >> cfg.chamfer >> cfg.rp >> cfg.rb;
    std::cin >> cfg.acc_max >> cfg.pf_n >> cfg.pf_d >> cfg.bf_n >> cfg.bf_d;
    std::cin >> cfg.kick_max >> cfg.kick_reach >> cfg.kick_cooldown;
    std::cin >> cfg.e_wall_n >> cfg.e_wall_d >> cfg.e_ball_n >> cfg.e_ball_d >> cfg.e_pp_n >> cfg.e_pp_d;
    std::cin >> cfg.steps >> cfg.stall >> cfg.msg_max;
    std::cin >> my_idx >> step_ms >> first_ms;

    bool prev_is_c = (my_idx == 0);
    int role_lock = 0;
    soccer::Obs obs;
    obs.idx = my_idx;

    std::vector<soccer::BallPt> ball_traj;
    ball_traj.reserve(MAX_H);
    DriftTable dt;

    while (std::cin >> obs.t >> obs.left >> obs.my_score >> obs.opp_score >> obs.mate_msg >> obs.bx >> obs.by >>
           obs.bvx >> obs.bvy) {
        for (int i = 0; i < 4; ++i) {
            std::cin >> obs.pl[i].x >> obs.pl[i].y >> obs.pl[i].vx >> obs.pl[i].vy >> obs.pl[i].cd;
        }

        const auto& me = obs.pl[0];
        const auto& mate = obs.pl[1];

        build_drift_table(obs, dt);

        int pred_res = soccer::predict_ball(cfg, obs.bx, obs.by, obs.bvx, obs.bvy, MAX_H, ball_traj);

        // 1. 角色选举（与队友代码逐位对齐：use_cd = false, margin = 10, lock = 30）
        int t_me_role = calc_intercept_t(me, obs.bx, obs.by, ball_traj, false);
        int t_mate_role = calc_intercept_t(mate, obs.bx, obs.by, ball_traj, false);

        // 真实可踢截球步数（含 cd）
        int t_me = calc_intercept_t(me, obs.bx, obs.by, ball_traj, true);
        int t_mate = calc_intercept_t(mate, obs.bx, obs.by, ball_traj, true);
        int t_opp0 = calc_opp_touch_t(obs, dt, 2, ball_traj);
        int t_opp1 = calc_opp_touch_t(obs, dt, 3, ball_traj);
        int t_opp = std::min(t_opp0, t_opp1);

        bool is_kickoff = (obs.bx == 6000 && obs.bvx == 0 && obs.bvy == 0);
        bool is_c = false;
        bool mate_prev_c = ((obs.mate_msg >> 7) & 1) != 0;
        if (role_lock > 0) --role_lock;
        if (is_kickoff || obs.t == 0) {
            is_c = (my_idx == 0);
            role_lock = 0;
        } else if (role_lock > 0 && prev_is_c != mate_prev_c) {
            is_c = prev_is_c;
        } else {
            if (prev_is_c != mate_prev_c) {
                bool take = prev_is_c ? (t_mate_role + 10 <= t_me_role) : (t_me_role + 10 <= t_mate_role);
                if (take) {
                    is_c = !prev_is_c;
                    role_lock = 30;
                } else {
                    is_c = prev_is_c;
                }
            } else {
                if (t_me_role != t_mate_role) is_c = (t_me_role < t_mate_role);
                else is_c = (my_idx == 0);
                role_lock = 0;
            }
        }
        prev_is_c = is_c;

        int mate_intent = (int)(obs.mate_msg & 7);
        bool emergency_save = (pred_res == -1);

        // 门将防守站位计算辅助函数
        auto steer_goalie = [&](i64 dbx, i64 dby, i64& out_ax, i64& out_ay) {
            dbx = std::max(dbx, 350LL);
            i64 gk_x = std::clamp(dbx / 5 + 420, 580LL, 1550LL);
            i64 gk_y = 4000 + soccer::tdiv((dby - 4000) * gk_x, dbx);
            gk_y = std::clamp(gk_y, 2250LL, 5750LL);
            steer_to_stop(me, gk_x, gk_y, out_ax, out_ay);
        };

        // 2. 决定本步跑位 (ax, ay)
        i64 ax = 0, ay = 0;

        if (emergency_save) {
            // 球正飞向我方球门！找越线前 (bp.x >= 0) 最早能拦住的轨迹点
            int n_traj = (int)ball_traj.size();
            int max_save_k = std::max(1, n_traj - 1);
            int best_k = -1;

            for (int k = 1; k <= max_save_k && k <= MAX_H; ++k) {
                const auto& bp = ball_traj[k - 1];
                if (bp.x < 0) break;
                i64 reach = dt.r_acc[k] + (k >= me.cd ? 495 : 445);
                i64 ddx = bp.x - dt.dx[0][k], ddy = bp.y - dt.dy[0][k];
                if (ddx * ddx + ddy * ddy <= reach * reach) {
                    best_k = k;
                    break;
                }
            }
            if (best_k != -1) {
                const auto& bp = ball_traj[best_k - 1];
                steer_to_k(me, dt, best_k, std::max(320LL, bp.x), bp.y, ax, ay);
            } else {
                // 如果直接轨迹暂时够不着，全力冲向球门线前的落点！(绝不要往旁边的过路点偏航)
                const auto& goal_bp = ball_traj[std::max(0, max_save_k - 1)];
                i64 gy_save = std::clamp(goal_bp.y, 3020LL, 4980LL);
                steer_to_k(me, dt, std::clamp(max_save_k, 1, MAX_H), 420, gy_save, ax, ay);
            }
        } else if (is_c || (mate_intent == 4 && t_me <= t_mate + 4)) {
            // 我是主抢球手 C
            double d_me_goal = std::hypot(double(me.x - 700), double(me.y - 4000));
            double d_mate_goal = std::hypot(double(mate.x - 700), double(mate.y - 4000));
            bool mate_guarding_goal = (mate.x < 2200 && mate.y > 2500 && mate.y < 5500);

            if (t_opp < t_me) {
                // 对手会比我先碰到球！
                int opp_k = std::clamp(t_opp, 1, std::max(1, (int)ball_traj.size()));
                i64 obx = ball_traj.empty() ? obs.bx : ball_traj[opp_k - 1].x;
                i64 oby = ball_traj.empty() ? obs.by : ball_traj[opp_k - 1].y;

                // 计算封堵点（在对手触球点朝我方球门方向 420 处）
                double gx = double(0 - obx), gy = double(4000 - oby);
                double gd = std::hypot(gx, gy);
                i64 block_x = obx, block_y = oby;
                if (gd > 1.0) {
                    block_x = obx + std::llround(gx / gd * 420.0);
                    block_y = oby + std::llround(gy / gd * 420.0);
                }

                // 检查我能否在 opp_k + 2 步内赶到封堵点贴身封堵
                int check_k = std::clamp(opp_k + 1, 1, MAX_H);
                i64 bdx = block_x - dt.dx[0][check_k], bdy = block_y - dt.dy[0][check_k];
                i64 breach = dt.r_acc[check_k] + 550;
                bool can_block_in_time = (bdx * bdx + bdy * bdy <= breach * breach);

                if (d_me_goal < d_mate_goal && !mate_guarding_goal && obx > 1600) {
                    // 我是最后一名防守人且队友没在门前：留在门前封堵射门扇面！
                    steer_goalie(obx, oby, ax, ay);
                } else if (can_block_in_time) {
                    // 来得及贴身封堵对手出球！
                    steer_to_k(me, dt, opp_k, block_x, block_y, ax, ay);
                } else if (!mate_guarding_goal) {
                    // 来不及贴身封堵对手，且队友不在门前（例如队友跑去了中场边路）：我立刻回撤到门前中路保护球门！
                    steer_goalie(obx, oby, ax, ay);
                } else {
                    // 队友已经在守门，而我来不及贴身抢下对手这一脚：退守到射门走廊中段拦截对手的射门/传球！
                    i64 contain_x = std::clamp(obx - 2800, 1800LL, 6800LL);
                    i64 contain_y = 4000 + soccer::tdiv((oby - 4000) * contain_x, std::max(obx, contain_x + 1));
                    contain_y = std::clamp(contain_y, 2000LL, 6000LL);
                    steer_to_stop(me, contain_x, contain_y, ax, ay);
                }
            } else {
                // 我能比对手先（或同时）拿到球！
                int target_k = std::clamp(t_me, 1, MAX_H);
                i64 tbx = obs.bx, tby = obs.by;
                if (!ball_traj.empty() && t_me >= 1) {
                    int idx = std::min((int)ball_traj.size() - 1, target_k - 1);
                    tbx = ball_traj[idx].x;
                    tby = ball_traj[idx].y;
                }

                i64 next_bx = obs.bx + obs.bvx, next_by = obs.by + obs.bvy;
                double dist_now = std::hypot(double(next_bx - me.x), double(next_by - me.y));
                bool can_kick_now = (me.cd == 0 &&
                                     (obs.bx - me.x) * (obs.bx - me.x) + (obs.by - me.y) * (obs.by - me.y) <= 500 * 500);

                if (!can_kick_now && dist_now < 720.0 && me.x > next_bx - 80 && obs.bx < 9500) {
                    i64 dy_ball = me.y - next_by;
                    i64 offset_y = (dy_ball >= 0) ? 500 : -500;
                    if (next_by < 900) offset_y = 500;
                    if (next_by > 7100) offset_y = -500;
                    steer_to_k(me, dt, 2, next_bx - 350, next_by + offset_y, ax, ay);
                } else if (me.cd > 0 && t_me <= me.cd + 1 && t_opp > me.cd + 2) {
                    steer_to_k(me, dt, target_k, tbx - 430, tby, ax, ay);
                } else {
                    steer_to_k(me, dt, target_k, tbx, tby, ax, ay);
                }
            }
        } else {
            // 协防 / 接应手 D
            bool we_attacking = (t_mate < t_opp && obs.bx > 6600 && obs.bvx >= -80);
            if (we_attacking) {
                // 即使队友在前场控球，D 也保持在后场中路偏接应侧 (x <= 3200, y in [2800, 5200])，绝不丢空门！
                i64 sup_x = std::clamp(obs.bx - 4200, 1400LL, 3200LL);
                i64 sup_y = (obs.by < 4000) ? 4600 : 3400;
                steer_to_stop(me, sup_x, sup_y, ax, ay);
            } else {
                int look_k = std::min({t_opp, t_mate, 14});
                look_k = std::clamp(look_k, 1, std::max(1, (int)ball_traj.size()));
                i64 dbx = ball_traj.empty() ? obs.bx : ball_traj[look_k - 1].x;
                i64 dby = ball_traj.empty() ? obs.by : ball_traj[look_k - 1].y;
                steer_goalie(dbx, dby, ax, ay);
            }
        }

        // 3. 决定踢球 (kx, ky)
        KickChoice kc = choose_kick(cfg, obs, dt, ax, ay);
        i64 kx = kc.kx, ky = kc.ky;

        if (kx != 0 || ky != 0) {
            if (kc.self_follow && is_c) {
                steer_to_k(me, dt, kc.follow_step, kc.follow_x, kc.follow_y, ax, ay);
            }
            if (would_self_bump_badly(cfg, obs, ax, ay, kx, ky)) {
                i64 bx1 = obs.bx + kx, by1 = obs.by + ky;
                i64 px1 = me.x + me.vx, py1 = me.y + me.vy;
                double ddx = double(px1 - bx1), ddy = double(py1 - by1);
                double d = std::hypot(ddx, ddy);
                if (d > 1.0) {
                    ax = std::llround(ddx / d * 40.0);
                    ay = std::llround(ddy / d * 40.0);
                    soccer::clamp_len(ax, ay, 40);
                } else {
                    ax = 0;
                    ay = 0;
                }
            }
        }

        // 4. 组装 MSG（bit7 = is_c，低3位 = 动作类型）
        i64 msg = (is_c ? 128 : 0) | (kc.msg_low & 7);
        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << msg << std::endl;
    }
    return 0;
}
