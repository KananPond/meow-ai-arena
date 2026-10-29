#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "soccer.hpp"

using i64 = long long;

struct Player {
    i64 x, y, vx, vy, cd;
};

struct State {
    i64 t, left, myScore, oppScore, mateMsg;
    i64 bx, by, bvx, bvy;
    Player p[4];
};

static soccer::Config cfg;
static int IDX;
static std::array<i64, 81> travel;

static i64 dist2(i64 x1, i64 y1, i64 x2, i64 y2) {
    i64 dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

static i64 distance(i64 x1, i64 y1, i64 x2, i64 y2) {
    return soccer::isqrt(dist2(x1, y1, x2, y2));
}

static i64 clampValue(i64 x, i64 lo, i64 hi) {
    return std::max(lo, std::min(hi, x));
}

static std::pair<i64, i64> unitVector(i64 dx, i64 dy, i64 length) {
    if (dx == 0 && dy == 0) return {length, 0};
    i64 d = soccer::isqrt(dx * dx + dy * dy);
    return {dx * length / d, dy * length / d};
}

static int arrival(const Player& p, const std::vector<soccer::BallPt>& path) {
    for (int k = 1; k <= (int)path.size(); ++k) {
        const auto& b = path[k - 1];
        if (distance(p.x, p.y, b.x, b.y) <= travel[k] + 650) return k;
    }
    return 1000;
}

static int firstIntercept(const Player& p, const std::vector<soccer::BallPt>& path) {
    for (int k = 1; k <= (int)path.size(); ++k) {
        const auto& b = path[k - 1];
        if (distance(p.x, p.y, b.x, b.y) <= travel[k] + 360) return k;
    }
    return 0;
}

static std::pair<i64, i64> accelerationTo(const Player& p, i64 tx, i64 ty) {
    i64 dx = tx - p.x, dy = ty - p.y;
    i64 d = soccer::isqrt(dx * dx + dy * dy);
    if (d == 0) {
        auto a = unitVector(-p.vx, -p.vy, 40);
        return a;
    }
    double speed = std::min(385.0, std::max(0.0, (double)(d - 30) * 0.72));
    double dvx = dx * speed / d - p.vx;
    double dvy = dy * speed / d - p.vy;
    i64 ax = (i64)std::llround(dvx), ay = (i64)std::llround(dvy);
    soccer::clamp_len(ax, ay, 40);
    return {ax, ay};
}

struct KickAnswer {
    i64 x = 0, y = 0;
    long long score = std::numeric_limits<long long>::min();
};

static KickAnswer chooseKick(const State& s, bool chaser) {
    KickAnswer best;
    if (s.p[0].cd != 0 || dist2(s.p[0].x, s.p[0].y, s.bx, s.by) > 500 * 500) return best;

    std::vector<std::pair<i64, i64>> kicks;
    const int goalY[] = {3120, 3350, 3650, 4000, 4350, 4650, 4880};
    for (int y : goalY) kicks.push_back(unitVector(cfg.w + 300 - s.bx, y - s.by, 800));

    const int laneY[] = {900, 1700, 2500, 3300, 4000, 4700, 5500, 6300, 7100};
    i64 clearSpeed = (s.bx < 4300 || s.bvx < -120) ? 800 : 610;
    for (int y : laneY) kicks.push_back(unitVector(cfg.w - s.bx, y - s.by, clearSpeed));

    i64 mateLeadX = s.p[1].x + s.p[1].vx * 5;
    i64 mateLeadY = s.p[1].y + s.p[1].vy * 5;
    kicks.push_back(unitVector(mateLeadX - s.bx, mateLeadY - s.by, 720));
    kicks.push_back(unitVector(mateLeadX + 900 - s.bx, mateLeadY - s.by, 720));

    for (int j = 0; j < 16; ++j) {
        double a = 2.0 * 3.14159265358979323846 * j / 16.0;
        kicks.push_back({(i64)std::llround(std::cos(a) * clearSpeed),
                         (i64)std::llround(std::sin(a) * clearSpeed)});
    }

    std::vector<soccer::BallPt> path;
    for (size_t z = 0; z < kicks.size(); ++z) {
        i64 kx = kicks[z].first, ky = kicks[z].second;
        if (kx == 0 && ky == 0) continue;
        int goal = soccer::predict_ball(cfg, s.bx, s.by, kx, ky, 85, path);
        int block = 1000;
        i64 nearest = std::numeric_limits<i64>::max();
        for (int k = 1; k <= (int)path.size(); ++k) {
            const auto& b = path[k - 1];
            for (int o = 2; o <= 3; ++o) {
                i64 d = distance(s.p[o].x, s.p[o].y, b.x, b.y);
                nearest = std::min(nearest, d);
                if (d <= travel[std::min(k, 80)] + 430) block = std::min(block, k);
            }
        }
        int goalStep = 1000;
        if (goal != 0) {
            goalStep = (int)path.size();
            if (goal < 0) continue;
        }

        long long score;
        if (goal > 0) {
            if (block <= goalStep) continue;
            score = 900000000LL - goalStep * 1000LL + std::min<i64>(nearest, 10000);
        } else {
            int last = std::min<int>((int)path.size(), std::min(block, 36));
            if (last < 1) last = std::min<int>(path.size(), 8);
            i64 bestX = s.bx;
            i64 endY = s.by;
            for (int k = 0; k < last; ++k) {
                bestX = std::max(bestX, path[k].x);
                endY = path[k].y;
            }
            score = bestX * 10000LL + std::min(block, 55) * 180000LL;
            score += std::min<i64>(nearest, 5000) * 30LL;
            if (kx < 0) score -= (s.bx < 3500 ? 1000000LL : 18000000LL);
            if (s.bx < 4300) score += (bestX - s.bx) * 12000LL;
            i64 md = distance(s.p[1].x, s.p[1].y, path[std::max(0, last - 1)].x, endY);
            if (s.p[1].x > s.bx + 500 && md < 1400) score += 1600000LL - md * 500LL;
            if (bestX > cfg.w - 1800 && (endY < 2700 || endY > 5300)) score -= 1500000LL;
            if (!chaser && s.bx > 5000) score -= 500000LL;
        }
        if (score > best.score) best = {kx, ky, score};
    }
    return best;
}

static int directionMessage(i64 kx, i64 ky) {
    if (kx == 0 && ky == 0) return 0;
    double a = std::atan2((double)ky, (double)kx);
    int code = (int)std::llround(a * 64.0 / 3.14159265358979323846);
    code %= 128;
    if (code < 0) code += 128;
    return code;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string magic;
    long long version;
    if (!(std::cin >> magic >> version)) return 0;
    std::cin >> cfg.w >> cfg.h >> cfg.goal >> cfg.chamfer >> cfg.rp >> cfg.rb;
    std::cin >> cfg.acc_max >> cfg.pf_n >> cfg.pf_d >> cfg.bf_n >> cfg.bf_d;
    std::cin >> cfg.kick_max >> cfg.kick_reach >> cfg.kick_cooldown;
    std::cin >> cfg.e_wall_n >> cfg.e_wall_d >> cfg.e_ball_n >> cfg.e_ball_d >> cfg.e_pp_n >> cfg.e_pp_d;
    std::cin >> cfg.steps >> cfg.stall >> cfg.msg_max;
    long long stepMs, firstMs;
    std::cin >> IDX >> stepMs >> firstMs;

    travel[0] = 0;
    i64 speed = 0;
    for (int k = 1; k <= 80; ++k) {
        speed = speed * cfg.pf_n / cfg.pf_d + cfg.acc_max;
        travel[k] = travel[k - 1] + speed;
    }

    State s;
    while (std::cin >> s.t >> s.left >> s.myScore >> s.oppScore >> s.mateMsg
                    >> s.bx >> s.by >> s.bvx >> s.bvy) {
        for (auto& p : s.p) std::cin >> p.x >> p.y >> p.vx >> p.vy >> p.cd;

        std::vector<soccer::BallPt> path;
        soccer::predict_ball(cfg, s.bx, s.by, s.bvx, s.bvy, 60, path);
        int ta = arrival(s.p[0], path), tb = arrival(s.p[1], path);
        int t0 = IDX == 0 ? ta : tb;
        int t1 = IDX == 0 ? tb : ta;
        bool backChaser = s.bx < 3000 && t1 + 3 < t0;
        bool chaser = IDX == 0 ? !backChaser : backChaser;
        if (IDX == 0 && s.mateMsg == 128) chaser = false;
        if (s.bvx < 80 || s.bx < 5200) {
            chaser = s.p[0].x != s.p[1].x ? s.p[0].x > s.p[1].x : IDX == 0;
        }

        i64 tx, ty;
        if (chaser) {
            int k = firstIntercept(s.p[0], path);
            if (k > 0) {
                tx = path[k - 1].x;
                ty = path[k - 1].y;
            } else {
                tx = s.bx + s.bvx * 3;
                ty = s.by + s.bvy * 3;
            }
        } else {
            int oppA = arrival(s.p[2], path), oppB = arrival(s.p[3], path);
            bool danger = s.bx < 5600 || s.bvx < -80 || std::min(oppA, oppB) <= t0 + 2;
            if (danger) {
                if (s.bvx < -120 && s.bx < 6500) {
                    tx = 800;
                    ty = 4000;
                    for (const auto& b : path) {
                        if (b.x < 1150) {
                            ty = b.y;
                            break;
                        }
                    }
                } else {
                    tx = clampValue(s.bx - 1800, 800, 2200);
                    i64 denominator = std::max<i64>(s.bx, 900);
                    ty = 4000 + (s.by - 4000) * tx / denominator;
                }
                ty = clampValue(ty, 3100, 4900);
            } else {
                tx = clampValue(s.bx - 1100, 2600, 4500);
                ty = clampValue(4000 + (s.by - 4000) / 3, 3000, 5000);
            }
        }

        tx = clampValue(tx, 350, cfg.w - 350);
        ty = clampValue(ty, 350, cfg.h - 350);
        auto acc = accelerationTo(s.p[0], tx, ty);
        KickAnswer kick = chooseKick(s, chaser);
        int msg = (kick.x || kick.y) ? directionMessage(kick.x, kick.y) : (chaser ? 128 : 129);
        if (!chaser && (s.bx < 1800 || (s.bvx < -350 && s.bx < 4000))) msg = 255;
        if (kick.x || kick.y) msg = directionMessage(kick.x, kick.y);

        std::cout << acc.first << ' ' << acc.second << ' ' << kick.x << ' ' << kick.y << ' ' << msg << std::endl;
    }
    return 0;
}
