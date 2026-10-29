// bots.hpp —— 基准 / 陪练 bot（原型 bots.py 的 C++ 移植，决策用浮点，物理预测用 soccer.hpp）。
// 不进选手包（选手只拿到编译好的程序）。
//
//   Roles    分工：离球近的冲球大脚射门，另一人守门前（原型 roles）
//   Planner  规划：够得着球时枚举 ndir 个方向 × 若干力度，前向模拟球，按"谁先截到、在哪截到、能否直接进"打分；
//            depth2 > 0 时对前 depth2 个"自己人先拿到"的踢法再模拟一脚射门（原型 planN / pro / ace）。
//            没球时：两人用同一个确定性函数决定谁去抢（截球时间小者，平手队内 0 号），另一人守门 / 接应。
// 全部确定性：同样的输入得到同样的输出，不看时间。
#pragma once

#include "../workspace/soccer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sbot {

using soccer::i64;
constexpr double PI = 3.14159265358979323846;

inline long long iround(double v) { return (long long)std::nearbyint(v); }  // 与 Python round 一样：四舍六入五成双

struct V2 { double x, y; };

inline V2 lim(double x, double y, double m) {
    const double d = std::hypot(x, y);
    if (d > m && d > 0) return {x * m / d, y * m / d};
    return {x, y};
}

struct PV { double x, y, vx, vy; int cd; };

struct Phys {
    soccer::Config cfg;
    double W, H, A, f, reach, gy0, gy1, vmax;
    std::vector<double> S1, S2;
    explicit Phys(const soccer::Config& c) : cfg(c) {
        W = double(c.w);
        H = double(c.h);
        A = double(c.acc_max);
        f = double(c.pf_n) / double(c.pf_d);
        const int N = 260;
        S1.assign(N, 0);
        S2.assign(N, 0);
        double s1 = 0, s2 = 0;
        for (int t = 1; t < N; ++t) {
            s1 += std::pow(f, t - 1);
            s2 += (1 - std::pow(f, t)) / (1 - f);
            S1[size_t(t)] = s1;
            S2[size_t(t)] = s2;
        }
        reach = double(c.kick_reach - 30);
        gy0 = (H - double(c.goal)) / 2;
        gy1 = gy0 + double(c.goal);
        vmax = A * f / (1 - f) + A;
    }
    // 球员 p 最早第几步能踢到沿 traj 走的球（traj[0] 是当前位置）
    int tau(const PV& p, const std::vector<V2>& traj, int maxT, int cd) const {
        const int L = int(traj.size());
        for (int t = cd; t < maxT; ++t) {
            const V2& q = t < L ? traj[size_t(t)] : traj[size_t(L - 1)];
            const double dx = q.x - p.x - S1[size_t(t)] * p.vx;
            const double dy = q.y - p.y - S1[size_t(t)] * p.vy;
            if (std::hypot(dx, dy) - reach <= A * S2[size_t(t)]) return t;
        }
        return maxT;
    }
    int tau(const PV& p, const std::vector<V2>& traj, int maxT) const { return tau(p, traj, maxT, p.cd); }
    V2 steer_to(const PV& p, double qx, double qy, int t) const {
        t = std::max(1, std::min(t, int(S1.size()) - 1));
        const double ax = (qx - p.x - S1[size_t(t)] * p.vx) / S2[size_t(t)];
        const double ay = (qy - p.y - S1[size_t(t)] * p.vy) / S2[size_t(t)];
        return lim(ax, ay, A);
    }
    V2 arrive(const PV& p, double qx, double qy, double gain = 0.12) const {
        V2 v = lim((qx - p.x) * gain, (qy - p.y) * gain, vmax);
        return lim(v.x - p.vx, v.y - p.vy, A);
    }
};

// 球轨迹：traj[0] = 当前位置，traj[k] = k 步后。goal = +1 / -1 / 0，gstep = 进球那一步（traj 的最后一项）
inline void ball_traj(const soccer::Config& cfg, i64 bx, i64 by, i64 bvx, i64 bvy, int n, std::vector<V2>& traj, int& goal,
                      int& gstep) {
    static thread_local std::vector<soccer::BallPt> out;
    goal = soccer::predict_ball(cfg, bx, by, bvx, bvy, n, out);
    traj.clear();
    traj.push_back({double(bx), double(by)});
    for (auto& o : out) traj.push_back({double(o.x), double(o.y)});
    gstep = goal ? int(out.size()) : -1;
}

struct Act { long long ax = 0, ay = 0, kx = 0, ky = 0, msg = 0; };

struct Options {
    int ndir = 16;
    std::vector<int> powers = {400, 800};
    int depth2 = 0;
    double bias = 2.0;
    bool safe = false, goalside = false, push = true;
    bool fixed_roles = false;  // 固定分工：队内 0 号主攻，1 号守门
    double keep = 900;
    int horizon = 90;
};

class Bot {
public:
    virtual ~Bot() = default;
    virtual Act act(const soccer::Obs& o) = 0;
};

inline PV pv(const soccer::PlayerView& p) { return {double(p.x), double(p.y), double(p.vx), double(p.vy), int(p.cd)}; }

// ------------------------------------------------------------------ roles

class RolesBot : public Bot {
public:
    explicit RolesBot(const soccer::Config& c) : ph(c) {}
    Act act(const soccer::Obs& o) override {
        const PV me = pv(o.pl[0]), mate = pv(o.pl[1]);
        const double bx = double(o.bx), by = double(o.by);
        const double dm = std::hypot(bx - me.x, by - me.y), dt = std::hypot(bx - mate.x, by - mate.y);
        const bool attacker = dm < dt || (dm == dt && o.idx == 0);
        V2 a;
        if (attacker) a = lim((bx - me.x) * 1000, (by - me.y) * 1000, ph.A);
        else {
            const double gx = 0, gy = ph.H / 2;
            const double dx = bx - gx, dy = by - gy;
            double d = std::hypot(dx, dy);
            if (d == 0) d = 1;
            const double k = std::min(1500.0, d * 0.5) / d;
            a = ph.arrive(me, gx + dx * k, gy + dy * k);
        }
        Act r;
        r.ax = iround(a.x);
        r.ay = iround(a.y);
        const i64 ddx = o.bx - o.pl[0].x, ddy = o.by - o.pl[0].y;
        if (o.pl[0].cd == 0 && ddx * ddx + ddy * ddy <= ph.cfg.kick_reach * ph.cfg.kick_reach) {
            const double dx = ph.W - bx, dy = ph.H / 2 - by;
            double d = std::hypot(dx, dy);
            if (d == 0) d = 1;
            r.kx = iround(dx * double(ph.cfg.kick_max) / d);
            r.ky = iround(dy * double(ph.cfg.kick_max) / d);
        }
        return r;
    }

private:
    Phys ph;
};

// ------------------------------------------------------------------ planner

class PlannerBot : public Bot {
public:
    PlannerBot(const soccer::Config& c, const Options& op) : ph(c), cfg(c), opt(op) {
        for (int i = 0; i < opt.ndir; ++i) {
            const double ang = 2 * PI * i / opt.ndir;
            for (int pw : opt.powers) cands.push_back({iround(pw * std::cos(ang)), iround(pw * std::sin(ang))});
        }
    }

    Act act(const soccer::Obs& o) override {
        const PV me = pv(o.pl[0]);
        const double bx = double(o.bx), by = double(o.by);
        ball_traj(cfg, o.bx, o.by, o.bvx, o.bvy, opt.horizon, traj, goal_, gstep_);
        own_threat = goal_ == -1 ? gstep_ : -1;
        int taus[4];
        for (int i = 0; i < 4; ++i) taus[i] = ph.tau(pv(o.pl[i]), traj, opt.horizon);
        const bool go = chaser(o, taus);
        Act r;
        const i64 ddx = o.bx - o.pl[0].x, ddy = o.by - o.pl[0].y;
        if (o.pl[0].cd == 0 && ddx * ddx + ddy * ddy <= cfg.kick_reach * cfg.kick_reach) choose_kick(o, r.kx, r.ky);
        const int L = int(traj.size());
        if (go && opt.goalside && std::min(taus[2], taus[3]) + 3 < taus[0]) {
            const int k = std::min(taus[2], taus[3]);
            const V2 q = traj[size_t(std::min(k, L - 1))];
            const double gx = 0, gy = ph.H / 2;
            const double dx = q.x - gx, dy = q.y - gy;
            double d = std::hypot(dx, dy);
            if (d == 0) d = 1;
            if (std::hypot(me.x - gx, me.y - gy) > d - 300) {
                const double rr = std::max(600.0, d - 1200);
                const V2 a = ph.arrive(me, gx + dx * rr / d, gy + dy * rr / d, 0.3);
                r.ax = iround(a.x);
                r.ay = iround(a.y);
                return r;
            }
        }
        V2 a;
        if (go) {
            const int t = taus[0];
            if (t >= opt.horizon) {
                a = lim(bx - me.x, by - me.y, ph.A);
                a = lim(a.x * 1000, a.y * 1000, ph.A);
            } else if (t <= 1) {
                const V2 q = traj[size_t(std::min(1, L - 1))];
                a = ph.steer_to(me, q.x, q.y, 1);
            } else {
                const V2 q = traj[size_t(std::min(t, L - 1))];
                a = ph.steer_to(me, q.x, q.y, t);
            }
        } else {
            const bool ours = std::min(taus[0], taus[1]) < std::min(taus[2], taus[3]);
            a = support(o, taus, ours);
        }
        r.ax = iround(a.x);
        r.ay = iround(a.y);
        return r;
    }

private:
    Phys ph;
    soccer::Config cfg;
    Options opt;
    std::vector<std::pair<long long, long long>> cands;
    std::vector<V2> traj;
    int goal_ = 0, gstep_ = -1, own_threat = -1;
    bool has_last = false;
    int last_us = 0, last_opp = 0;

    bool chaser(const soccer::Obs& o, const int* taus) const {
        const int idx = o.idx;
        if (opt.fixed_roles) {
            if (idx == 0) return true;
            const V2 q = traj[size_t(std::min(taus[0], int(traj.size()) - 1))];
            return q.x < ph.W / 3 && taus[0] + 3 < taus[1];
        }
        return taus[0] < taus[1] || (taus[0] == taus[1] && idx < 1 - idx);
    }

    V2 support(const soccer::Obs& o, const int* taus, bool ours) const {
        const PV me = pv(o.pl[0]);
        const double bx = double(o.bx), by = double(o.by);
        const double W = ph.W, H = ph.H;
        const int L = int(traj.size());
        if (own_threat >= 0 && taus[0] <= own_threat) {
            const int t = std::max(1, taus[0]);
            const V2 q = traj[size_t(std::min(t, L - 1))];
            return ph.steer_to(me, q.x, q.y, t);
        }
        if (opt.safe) {
            const int lead = std::min(taus[2], taus[3]) - std::min(taus[0], taus[1]);
            if (!(lead >= 8 && bx > W * 0.6)) ours = false;
        }
        if (ours && !opt.fixed_roles && bx > W * 0.55 && opt.push) {
            const double tx = std::min(bx + 1500, W - 1500);
            const double ty = H / 2 + (by < H / 2 ? 1800 : -1800);
            return ph.arrive(me, tx, ty);
        }
        const int k = std::min(std::min(taus[2], taus[3]), L - 1);
        const V2 q = traj[size_t(std::min(k, L - 1))];
        const double gx = 0, gy = H / 2;
        const double dx = q.x - gx, dy = q.y - gy;
        double d = std::hypot(dx, dy);
        if (d == 0) d = 1;
        const double r = std::min(opt.keep, d * 0.5);
        return ph.arrive(me, gx + dx * r / d, gy + dy * r / d, 0.25);
    }

    double evaluate(const soccer::Obs& o, const std::vector<V2>& tr, int goal, int gstep, int kicker_cd) {
        const int T = opt.horizon;
        const int lim_t = goal ? gstep + 1 : T;
        const int t_me = ph.tau(pv(o.pl[0]), tr, lim_t, std::max(1, kicker_cd));
        const int t_mate = ph.tau(pv(o.pl[1]), tr, lim_t);
        const int t_opp = std::min(ph.tau(pv(o.pl[2]), tr, lim_t), ph.tau(pv(o.pl[3]), tr, lim_t));
        const int t_us = std::min(t_me, t_mate);
        const int L = int(tr.size());
        has_last = false;
        if (goal == 1) {
            if (t_opp >= gstep) return 100000.0 - gstep * 10;
            const double p = 1 / (1 + std::exp(-(t_opp - gstep + opt.bias) / 1.5));
            const V2 q = tr[size_t(std::min(t_opp, L - 1))];
            return p * (100000.0 - gstep * 10) + (1 - p) * (-20000 + q.x);
        }
        if (goal == -1 && t_us >= gstep) return -100000;
        has_last = true;
        last_us = t_us;
        last_opp = t_opp;
        if (t_us < t_opp) {
            const V2 q = tr[size_t(std::min(t_us, L - 1))];
            return 20000 + q.x + std::min(t_opp - t_us, 25) * 120.0 - t_us * 5.0;
        }
        const V2 q = tr[size_t(std::min(t_opp, L - 1))];
        return -20000 + q.x - std::min(t_us - t_opp, 25) * 60.0;
    }

    bool second_touch(const soccer::Obs& o, const std::vector<V2>& tr, int t_us, double& best) {
        const double W = ph.W, H = ph.H;
        const V2 q = tr[size_t(std::min(t_us, int(tr.size()) - 1))];
        const double K = double(cfg.kick_max);
        bool any = false;
        static thread_local std::vector<soccer::BallPt> out;
        static thread_local std::vector<V2> full;
        const double fr[5] = {0.15, 0.35, 0.5, 0.65, 0.85};
        for (double frac : fr) {
            const double ty = ph.gy0 + (ph.gy1 - ph.gy0) * frac;
            const double aims[3] = {ty, -ty, 2 * H - ty};
            for (double aim_y : aims) {
                const double dx = W + 200 - q.x, dy = aim_y - q.y;
                double d = std::hypot(dx, dy);
                if (d == 0) d = 1;
                const long long kx = iround(dx * K / d), ky = iround(dy * K / d);
                const int goal = soccer::predict_ball(cfg, iround(q.x), iround(q.y), kx, ky, 60, out);
                if (goal != 1) continue;
                full.assign(tr.begin(), tr.begin() + std::min(size_t(t_us + 1), tr.size()));
                for (auto& x : out) full.push_back({double(x.x), double(x.y)});
                const int gt = int(full.size()) - 1;
                const int t_opp = std::min(ph.tau(pv(o.pl[2]), full, gt + 1), ph.tau(pv(o.pl[3]), full, gt + 1));
                const double pg = t_opp >= gt ? 1.0 : 1 / (1 + std::exp(-(t_opp - gt + opt.bias) / 1.5));
                const V2 qq = full[size_t(std::min(t_opp, gt))];
                const double v = pg * (100000.0 - gt * 10 - 3000) + (1 - pg) * (-20000 + qq.x);
                if (!any || v > best) best = v;
                any = true;
            }
        }
        return any;
    }

    void choose_kick(const soccer::Obs& o, long long& okx, long long& oky) {
        struct Cand { double v; long long kx, ky; std::vector<V2> tr; int t_us; };
        std::vector<Cand> pool;
        double bv = 0;
        long long bkx = 0, bky = 0;
        bool have = false;
        std::vector<V2> tr;
        int goal = 0, g = -1;
        for (auto& c : cands) {
            ball_traj(cfg, o.bx, o.by, c.first, c.second, opt.horizon, tr, goal, g);
            const double v = evaluate(o, tr, goal, g, int(cfg.kick_cooldown));
            if (opt.depth2 && has_last && last_us < last_opp) pool.push_back({v, c.first, c.second, tr, last_us});
            if (!have || v > bv) bv = v, bkx = c.first, bky = c.second, have = true;
        }
        if (opt.depth2 && bv < 95000) {
            std::stable_sort(pool.begin(), pool.end(), [](const Cand& a, const Cand& b) { return a.v > b.v; });
            for (size_t i = 0; i < pool.size() && int(i) < opt.depth2; ++i) {
                double v2 = 0;
                if (second_touch(o, pool[i].tr, pool[i].t_us, v2) && v2 > bv) bv = v2, bkx = pool[i].kx, bky = pool[i].ky;
            }
        }
        bool opp_reach = false;
        for (int k = 2; k < 4; ++k) {
            const i64 dx = o.bx - o.pl[k].x, dy = o.by - o.pl[k].y;
            if (dx * dx + dy * dy <= cfg.kick_reach * cfg.kick_reach) opp_reach = true;
        }
        if (!opp_reach) {
            ball_traj(cfg, o.bx, o.by, o.bvx, o.bvy, opt.horizon, tr, goal, g);
            const double v = evaluate(o, tr, goal, g, 0) - 50;
            if (v > bv) {
                okx = oky = 0;
                return;
            }
        }
        okx = bkx;
        oky = bky;
    }
};

// ------------------------------------------------------------------ 工厂

// 名字：roles、plan8、plan16、plan32、pro（plan32 + 第二脚，depth2=6）、ace（pro + 稳妥站位 + 回防）；
// 后缀 _bX 射门胆量，_fx 固定分工，_kN 守门距离，_hN 预测步数，_dN 第二脚候选数，_nN 方向数
inline Bot* make_bot(const std::string& name, const soccer::Config& cfg) {
    std::vector<std::string> parts;
    size_t s = 0;
    for (;;) {
        const size_t e = name.find('_', s);
        parts.push_back(name.substr(s, e == std::string::npos ? std::string::npos : e - s));
        if (e == std::string::npos) break;
        s = e + 1;
    }
    const std::string base = parts[0];
    if (base == "roles") return new RolesBot(cfg);
    Options op;
    auto P = [&](double f) { return int(iround(double(cfg.kick_max) * f)); };
    if (base.rfind("pro", 0) == 0 || base.rfind("ace", 0) == 0) {
        op.ndir = 32;
        op.powers = {P(0.375), P(0.7), P(1)};
        op.depth2 = base.size() > 3 ? std::atoi(base.c_str() + 3) : 6;
        if (base.rfind("ace", 0) == 0) op.safe = op.goalside = true;
    } else if (base.rfind("plan", 0) == 0) {
        op.ndir = std::atoi(base.c_str() + 4);
        if (op.ndir == 4) op.powers = {P(1)};
        else if (op.ndir == 8) op.powers = {P(0.6), P(1)};
        else if (op.ndir == 16) op.powers = {P(0.5), P(1)};
        else if (op.ndir == 32) op.powers = {P(0.375), P(0.7), P(1)};
        else op.powers = {P(0.3), P(0.5), P(0.7), P(0.85), P(1)};
    } else {
        return nullptr;
    }
    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string& p = parts[i];
        if (p == "fx") op.fixed_roles = true;
        else if (p == "safe") op.safe = true;
        else if (p == "gs") op.goalside = true;
        else if (p == "nopush") op.push = false;
        else if (p.size() > 1 && p[0] == 'b') op.bias = std::atof(p.c_str() + 1);
        else if (p.size() > 1 && p[0] == 'k') op.keep = std::atof(p.c_str() + 1);
        else if (p.size() > 1 && p[0] == 'h') op.horizon = std::atoi(p.c_str() + 1);
        else if (p.size() > 1 && p[0] == 'd') op.depth2 = std::atoi(p.c_str() + 1);
        else if (p.size() > 1 && p[0] == 'n') op.ndir = std::atoi(p.c_str() + 1);
        else if (p.size() > 1 && p[0] == 'p') {  // 力度档：p3 = 3 档均匀
            const int n = std::atoi(p.c_str() + 1);
            op.powers.clear();
            for (int k = 1; k <= n; ++k) op.powers.push_back(P(double(k) / n));
        }
    }
    return new PlannerBot(cfg, op);
}

}  // namespace sbot
