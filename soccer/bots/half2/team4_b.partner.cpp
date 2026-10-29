#include "soccer.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace std;
using soccer::i64;
using soccer::PlayerView;

struct Vec {
    double x = 0, y = 0;
    Vec operator+(Vec b) const { return {x + b.x, y + b.y}; }
    Vec operator-(Vec b) const { return {x - b.x, y - b.y}; }
    Vec operator*(double a) const { return {x * a, y * a}; }
};

static double len(Vec a) { return sqrt(a.x * a.x + a.y * a.y); }
static Vec limit(Vec a, double m) {
    double d = len(a);
    return d > m && d > 0 ? a * (m / d) : a;
}
static Vec pos(const PlayerView& p) { return {double(p.x), double(p.y)}; }
static Vec vel(const PlayerView& p) { return {double(p.vx), double(p.vy)}; }
static double dist(Vec a, Vec b) { return len(a - b); }
static double bound(double x, double lo, double hi) { return max(lo, min(hi, x)); }

struct BallPath {
    vector<soccer::BallPt> points; // current position at index 0
    soccer::BallPt terminal{};
    int goal = 0, goal_time = 1000;
};

static BallPath forecast(const soccer::Config& c, i64 bx, i64 by, i64 vx, i64 vy, int n) {
    vector<soccer::BallPt> next;
    int goal = soccer::predict_ball(c, bx, by, vx, vy, n, next);
    BallPath p;
    p.goal = goal;
    p.points.push_back({bx, by, vx, vy});
    if (goal) { p.goal_time = int(next.size()); p.terminal = next.back(); }
    for (int k = 0; k < int(next.size()); ++k) {
        if (goal && k + 1 == int(next.size())) break;
        p.points.push_back(next[k]);
    }
    if (!goal) {
        while (int(p.points.size()) <= n) p.points.push_back(p.points.back());
    }
    return p;
}

static Vec ballpos(const BallPath& b, int k) {
    k = min(k, int(b.points.size()) - 1);
    return {double(b.points[k].x), double(b.points[k].y)};
}

// Identical team role convention: free-ball trajectory, reachability, two-step hysteresis.
static int intercept_eta(const PlayerView& p, const BallPath& path) {
    double q = 1.0;
    for (int t = 0; t <= 40 && t < int(path.points.size()); ++t) {
        double s = 10.0 * (1.0 - q);
        double radius = 400.0 * t - 3600.0 * (1.0 - q) + 500.0;
        Vec c = pos(p) + vel(p) * s;
        Vec b = ballpos(path, t);
        double dx = b.x - c.x, dy = b.y - c.y;
        if (t >= p.cd && dx * dx + dy * dy <= radius * radius) return t;
        q *= 0.9;
    }
    return 99;
}

static bool can_kick(const PlayerView& p, i64 bx, i64 by, i64 reach) {
    i64 dx = p.x - bx, dy = p.y - by;
    return p.cd == 0 && dx * dx + dy * dy <= reach * reach;
}

static Vec steer(const PlayerView& p, Vec target, double max_speed = 355.0) {
    Vec delta = target - pos(p);
    double d = len(delta);
    if (d < 0.01) return limit(vel(p) * -1.0, 40.0);
    // Begin braking when the remaining distance is comparable to the stopping distance.
    double desired = min(max_speed, sqrt(70.0 * d));
    Vec target_v = delta * (desired / d);
    return limit(target_v * (1.0 / 0.9) - vel(p), 40.0);
}

static Vec offensive_target(const soccer::Obs& o, const BallPath& path, int eta) {
    int t = eta == 99 ? min(12, int(path.points.size()) - 1) : min(40, eta + 2);
    Vec b = ballpos(path, t);
    // Approach from the goal side whenever it does not meaningfully delay interception.
    double behind = eta > 8 ? 100.0 : 250.0;
    Vec goal = {12000.0, 4000.0};
    Vec dir = goal - b;
    double d = len(dir);
    if (d > 1) b = b - dir * (behind / d);
    b.x = bound(b.x, 320, 11680);
    b.y = bound(b.y, 320, 7680);
    return b;
}

static Vec chase_acceleration(const soccer::Obs& o, const BallPath& path, int eta) {
    const auto& p = o.pl[0];
    if (path.points.size() <= 1) return steer(p, ballpos(path, 0));
    int n = eta;
    if (n == 99) {
        double best_gap = numeric_limits<double>::infinity();
        n = 1;
        for (int t = 1; t < int(path.points.size()) && t <= 40; ++t) {
            double q = pow(0.9, t), s = 10.0 * (1.0 - q);
            double r = 400.0 * t - 3600.0 * (1.0 - q);
            double gap = dist(ballpos(path, t), pos(p) + vel(p) * s) - r;
            if (gap < best_gap) { best_gap = gap; n = t; }
        }
    }
    n = max(1, min(n, int(path.points.size()) - 1));
    Vec target = ballpos(path, n);
    if (p.x <= target.x + 200) target.x -= 200;
    target.x = bound(target.x, 320, 11680);
    target.y = bound(target.y, 320, 7680);
    if (n <= 5 && hypot(double(path.points[n].vx), double(path.points[n].vy)) < 130)
        return steer(p, target);
    double q = pow(0.9, n), inertia = 10.0 * (1.0 - q);
    double control = (400.0 * n - 3600.0 * (1.0 - q)) / 40.0;
    Vec need = target - (pos(p) + vel(p) * inertia);
    return limit(need * (1.0 / max(1.0, control)), 40.0);
}

static Vec defensive_target(const soccer::Obs& o, const BallPath& path) {
    double x = bound(double(o.bx) * 0.400, 450, 1600);
    double y = 4000.0 + (double(o.by) - 4000.0) * x / max(900.0, double(o.bx));
    y = bound(y, 2700.0, 5300.0);
    return {x, y};
}

// Once a shot is heading into the goal, aim for a reachable future collision point.
static bool shot_block(const soccer::Obs& o, const BallPath& path, Vec& acc) {
    if (path.goal != -1) return false;
    const auto& p = o.pl[0];
    int best_t = 1;
    double best_gap = numeric_limits<double>::infinity();
    for (int t = 1; t <= path.goal_time; ++t) {
        Vec b = t == path.goal_time ? Vec{double(path.terminal.x), double(path.terminal.y)} : ballpos(path, t);
        double q = pow(0.9, t), s = 10.0 * (1.0 - q);
        double r = 400.0 * t - 3600.0 * (1.0 - q);
        double gap = dist(b, pos(p) + vel(p) * s) - r - 410.0;
        if (gap < best_gap) { best_gap = gap; best_t = t; }
        if (gap <= 0) { best_t = t; break; }
    }
    Vec b = best_t == path.goal_time ? Vec{double(path.terminal.x), double(path.terminal.y)} : ballpos(path, best_t);
    double q = pow(0.9, best_t), s = 10.0 * (1.0 - q);
    double control = (400.0 * best_t - 3600.0 * (1.0 - q)) / 40.0;
    Vec delta = b - (pos(p) + vel(p) * s);
    double d = len(delta);
    if (d > 200) delta = delta * ((d - 200) / d);
    else delta = {0, 0};
    acc = limit(delta * (1.0 / max(1.0, control)), 40.0);
    return true;
}

static soccer::Game make_game(const soccer::Config& c, const soccer::Obs& o) {
    soccer::Config cfg = c;
    cfg.stall = 0;
    soccer::Game g(cfg, 0, 0, false);
    g.t = o.t;
    g.score[0] = o.my_score;
    g.score[1] = o.opp_score;
    g.bx = o.bx; g.by = o.by; g.bvx = o.bvx; g.bvy = o.bvy;
    int order[4] = {o.idx, 1 - o.idx, 2, 3};
    for (int i = 0; i < 4; ++i) {
        int j = order[i];
        g.px[j] = o.pl[i].x; g.py[j] = o.pl[i].y;
        g.pvx[j] = o.pl[i].vx; g.pvy[j] = o.pl[i].vy; g.cd[j] = o.pl[i].cd;
    }
    return g;
}

// Estimate whether an opponent can reach a sample of a candidate trajectory.
static int threat_time(const soccer::Game& g, const BallPath& path, int max_t) {
    double q = 1.0;
    for (int t = 0; t <= max_t && t < int(path.points.size()); ++t) {
        double s = 10.0 * (1.0 - q);
        double r = 400.0 * t - 3600.0 * (1.0 - q) + 500.0;
        Vec b = ballpos(path, t);
        for (int i = 2; i < 4; ++i) {
            if (g.cd[i] > t) continue;
            Vec center = {g.px[i] + g.pvx[i] * s, g.py[i] + g.pvy[i] * s};
            Vec d = b - center;
            if (d.x * d.x + d.y * d.y <= r * r) return t;
        }
        q *= 0.9;
    }
    return 99;
}

static double kick_value(const soccer::Game& initial, const soccer::Obs& o, Vec acc,
                         i64 kx, i64 ky) {
    soccer::Game g = initial;
    soccer::Action a[4];
    a[o.idx].ax = llround(acc.x); a[o.idx].ay = llround(acc.y);
    a[o.idx].kx = kx; a[o.idx].ky = ky;
    for (int j = 2; j < 4; ++j) {
        const PlayerView& p = o.pl[j];
        Vec move = steer(p, {double(o.bx + o.bvx), double(o.by + o.bvy)});
        a[j].ax = -llround(move.x); a[j].ay = llround(move.y);
        if (can_kick(p, o.bx, o.by, g.cfg.kick_reach)) {
            Vec kick = limit({-double(o.bx) - 200, 4000.0 - o.by}, 800);
            a[j].kx = -llround(kick.x); a[j].ky = llround(kick.y);
        }
    }
    int goal = g.step(a);
    if (goal == 0) return 15000;
    if (goal == 1) return -20000;
    BallPath p = forecast(g.cfg, g.bx, g.by, g.bvx, g.bvy, 30);
    PlayerView players[4];
    for (int i = 0; i < 4; ++i)
        players[i] = {g.px[i], g.py[i], g.pvx[i], g.pvy[i], g.cd[i]};
    int ours = min(intercept_eta(players[0], p), intercept_eta(players[1], p));
    int theirs = min(intercept_eta(players[2], p), intercept_eta(players[3], p));
    int sample = min({20, int(p.points.size()) - 1, ours, theirs});
    Vec endpoint = ballpos(p, sample);
    double result = (endpoint.x - o.bx) * 0.45;
    if (ours < theirs) {
        result += 1400.0 + 110.0 * min(8, theirs - ours);
    } else if (theirs < 99) {
        result -= 1100.0 + 130.0 * min(8, ours - theirs);
        if (endpoint.x < 5000) result -= (5000 - endpoint.x) * 0.35;
    }
    if (p.goal != 0) {
        int first = p.goal > 0 ? 2 : 0;
        double closest = 1000000.0;
        for (int t = 0; t < int(p.points.size()); ++t) {
            double q = pow(0.9, t), s = 10.0 * (1.0 - q);
            double reach = 400.0 * t - 3600.0 * (1.0 - q);
            Vec b = ballpos(p, t);
            for (int j = first; j < first + 2; ++j) {
                double r = reach + (players[j].cd <= t ? 500.0 : 450.0);
                double gap = dist(b, pos(players[j]) + vel(players[j]) * s) - r;
                closest = min(closest, gap);
            }
        }
        double chance = bound(0.48 + closest / 1800.0, 0.10, 0.98);
        if (p.goal > 0) result += 6500.0 * chance;
        else result -= 18000.0 * chance;
    }
    if (g.bvx < -100 && g.bx < 5000) result -= 350;
    if (kx == 0 && ky == 0 && hypot(double(o.bvx), double(o.bvy)) < 80) result -= 200;
    return result;
}

static Vec choose_kick(const soccer::Config& cfg, const soccer::Obs& o, Vec acc) {
    soccer::Game initial = make_game(cfg, o);
    vector<Vec> shots;
    // Sample the mouth directly, then open angles including passes and wall rebounds.
    for (int y = 3150; y <= 4850; y += 170) {
        Vec d = {double(cfg.w - o.bx + 300), double(y - o.by)};
        double n = len(d);
        if (n > 0) shots.push_back(d * (double(cfg.kick_max) / n));
    }
    const double PI = acos(-1.0);
    for (int power : {800, 550, 300, 160}) {
        for (int k = 0; k < 48; ++k) {
            double a = k * PI / 24.0;
            shots.push_back({power * cos(a), power * sin(a)});
        }
    }
    for (int y = 3200; y <= 4800; y += 200) {
        double dx = double(cfg.w - o.bx + 300);
        double top = -(double(o.by - 150) + (y - 150.0) / 0.8);
        double bottom = double(7850 - o.by) + (7850.0 - y) / 0.8;
        Vec a = {dx, top}, b = {dx, bottom};
        double na = len(a), nb = len(b);
        if (na > 0) shots.push_back(a * (double(cfg.kick_max) / na));
        if (nb > 0) shots.push_back(b * (double(cfg.kick_max) / nb));
    }
    // A pass into the teammate's path can be a safe release under pressure.
    Vec pass = pos(o.pl[1]) + vel(o.pl[1]) * 4.0 - Vec{double(o.bx), double(o.by)};
    if (pass.x > -500 && len(pass) > 300) shots.push_back(limit(pass, double(cfg.kick_max)));
    Vec best = {0, 0};
    double best_value = kick_value(initial, o, acc, 0, 0);
    for (Vec s : shots) {
        i64 x = llround(s.x), y = llround(s.y);
        double value = kick_value(initial, o, acc, x, y);
        if (value > best_value) { best_value = value; best = {double(x), double(y)}; }
    }
    return best;
}

// Counterfactual kickoff: opponents pursue, defend the goal, or run to the ball's lead.
// If these different reactions all leave an open lane, try the direct shot.
static bool open_kickoff_lane(const soccer::Config& cfg, const soccer::Obs& o,
                              Vec self_acc, Vec kick, int scenario) {
    soccer::Game g = make_game(cfg, o);
    g.cfg.steps = o.t + 40;
    for (int t = 0; t < 16; ++t) {
        soccer::Action a[4];
        if (t == 0) {
            a[o.idx].ax = llround(self_acc.x);
            a[o.idx].ay = llround(self_acc.y);
            a[o.idx].kx = llround(kick.x);
            a[o.idx].ky = llround(kick.y);
        }
        for (int j = 2; j < 4; ++j) {
            PlayerView p{g.px[j], g.py[j], g.pvx[j], g.pvy[j], g.cd[j]};
            if (can_kick(p, g.bx, g.by, cfg.kick_reach)) return false;
            Vec target;
            if (j == 3 && scenario != 2) {
                double x = 12000.0 - bound((12000.0 - g.bx) * 0.4, 450.0, 1600.0);
                double y = 4000.0 + (g.by - 4000.0) * (12000.0 - x) / max(900.0, 12000.0 - g.bx);
                target = {x, bound(y, 2700.0, 5300.0)};
            } else {
                double lead = scenario == 0 ? 0.0 : 2.0;
                target = {double(g.bx) + lead * g.bvx, double(g.by) + lead * g.bvy};
            }
            Vec move = steer(p, target);
            a[j].ax = -llround(move.x);
            a[j].ay = llround(move.y);
        }
        int goal = g.step(a);
        if (goal >= 0) return goal == 0;
        if (g.bvx < 200 || g.bx < 5500) return false;
    }
    return false;
}

static Vec smart_kickoff(const soccer::Config& cfg, const soccer::Obs& o, Vec acc) {
    int side = o.pl[2].y <= o.by ? 1 : -1;
    Vec fallback{500.0, double(side) * 250.0};
    Vec best = fallback;
    double best_value = -1e9;
    for (int y = 3200; y <= 4800; y += 100) {
        Vec dir{double(cfg.w - o.bx + 400), double(y - o.by)};
        double d = len(dir);
        Vec kick = dir * (800.0 / max(1.0, d));
        int success = 0;
        for (int scenario = 0; scenario < 3; ++scenario)
            success += open_kickoff_lane(cfg, o, acc, kick, scenario);
        if (success < 3) continue;
        // Prefer a lane away from both opponents and nearer the post.
        double value = success * 10000.0 +
                       0.3 * abs(double(o.pl[2].y) - (o.by + 2 * kick.y)) +
                       0.2 * abs(double(o.pl[3].y) - (o.by + 6 * kick.y));
        if (value > best_value) { best_value = value; best = kick; }
    }
    return best;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    soccer::Config c;
    string magic; i64 version, idx, step_ms, first_ms;
    if (!(cin >> magic >> version)) return 0;
    cin >> c.w >> c.h >> c.goal >> c.chamfer >> c.rp >> c.rb;
    cin >> c.acc_max >> c.pf_n >> c.pf_d >> c.bf_n >> c.bf_d;
    cin >> c.kick_max >> c.kick_reach >> c.kick_cooldown;
    cin >> c.e_wall_n >> c.e_wall_d >> c.e_ball_n >> c.e_ball_d >> c.e_pp_n >> c.e_pp_d;
    cin >> c.steps >> c.stall >> c.msg_max;
    cin >> idx >> step_ms >> first_ms;
    soccer::Obs o; o.idx = int(idx);
    int owner = -1;
    i64 old_my = -1, old_opp = -1;
    while (cin >> o.t >> o.left >> o.my_score >> o.opp_score >> o.mate_msg
               >> o.bx >> o.by >> o.bvx >> o.bvy) {
        for (auto& p : o.pl) cin >> p.x >> p.y >> p.vx >> p.vy >> p.cd;
        BallPath path = forecast(c, o.bx, o.by, o.bvx, o.bvy, 40);
        PlayerView team[2];
        team[o.idx] = o.pl[0]; team[1 - o.idx] = o.pl[1];
        int eta[2] = {intercept_eta(team[0], path), intercept_eta(team[1], path)};
        if (owner < 0 || old_my != o.my_score || old_opp != o.opp_score)
            owner = eta[0] <= eta[1] ? 0 : 1;
        else if (eta[1 - owner] + 2 <= eta[owner]) owner = 1 - owner;
        old_my = o.my_score; old_opp = o.opp_score;
        bool primary = owner == o.idx;
        bool me_can = can_kick(o.pl[0], o.bx, o.by, c.kick_reach);
        bool mate_can = can_kick(o.pl[1], o.bx, o.by, c.kick_reach);
        bool may_kick = me_can && (primary || !mate_can);
        bool self_back = o.pl[0].x < o.pl[1].x;
        int back_idx = self_back ? o.idx : 1 - o.idx;
        int opp_eta = min(intercept_eta(o.pl[2], path), intercept_eta(o.pl[3], path));
        bool personal = true;
        bool gap = abs(o.pl[0].x - o.pl[1].x) > 1200 &&
                   min(o.pl[0].x, o.pl[1].x) < 5000 && (o.bx > 2500 || personal);
        bool chasing_danger = gap && opp_eta <= eta[back_idx] + 2;
        bool chase = chasing_danger ? !self_back : primary;
        Vec acc = chase ? chase_acceleration(o, path, eta[o.idx])
                        : limit((limit((defensive_target(o, path) - pos(o.pl[0])) * 0.28, 345.0)
                                 - vel(o.pl[0])) * 0.70, 40.0);
        Vec block;
        if (shot_block(o, path, block)) acc = block;
        bool kickoff = o.bx == 6000 && o.bvx == 0 && o.bvy == 0 &&
                       o.pl[0].x == 5500 && o.pl[0].y == o.by &&
                       o.pl[0].vx == 0 && o.pl[0].vy == 0;
        Vec kick = {};
        if (may_kick) kick = kickoff ? smart_kickoff(c, o, acc) : choose_kick(c, o, acc);
        int msg = 240 + (kick.x != 0 || kick.y != 0 ? 3 : (primary ? 1 : 2));
        cout << llround(acc.x) << ' ' << llround(acc.y) << ' '
             << llround(kick.x) << ' ' << llround(kick.y) << ' ' << msg << endl;
    }
    return 0;
}
