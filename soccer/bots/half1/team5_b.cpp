// submission/player.cpp —— team5 B 位（GLM 5.3）
//
// 与队友（A 位 Claude Opus 5.5）约定的队内协议：
//   MSG（每步 1 字节）：
//     bit0-5 = min(T,62)+1，T = 本程序算出的"自己最早能踢到球的步数"（0 = 无信息，仅第 0 步）
//     bit6   = 我当前是抢球者
//     bit7   = 本步我要有效踢球
//   抢球者判定【用上一步双方广播的值】（两人手里的数相同 → 结论一致）：
//     effMe = myPrevT - (myPrevChaser?3:0)，effMate = mateT - (mateChaser?3:0)，小者抢，平手 0 号抢
//     第 0 步（MSG=0）：0 号抢、1 号守；队友持续无信息时自己替他算。
//
// 踢球：枚举（朝门多目标 + 全方向多力度），对每个候选用 predict_ball 精确模拟，
//       按"进球概率 / 谁先拿到球 / 落点价值"打分取最优（思路与队友约定的一致，实现各自独立）。
// 移动：抢球者朝拦截点全速（考虑当前速度漂移）；防守者站球-门连线 / 堵射门轨迹 / 转换时回防。

#include "soccer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using i64 = long long;
using soccer::clamp_len;
using soccer::isqrt;
using soccer::tdiv;

static soccer::Config CF;
static i64 W, H, ACC, KMAX, KREACH, RP, RB, GCY;
static int IDX;

static const int HOR = 100;
static double PD[HOR + 2];  // PD[k]：现有速度在 k 步里的位移系数（v*PD[k]）
static double PA[HOR + 2];  // PA[k]：从静止全速加速 k 步的最大位移

static void init_tables() {
    double v = 0, disp = 0, p = 1.0;
    PD[0] = 0;
    for (int k = 1; k <= HOR + 1; ++k) {
        p *= 0.9;
        PD[k] = 10.0 * (1.0 - p);  // sum_{j<k} 0.9^j * 1... = 10*(1-0.9^k)
        v += 40;
        disp += v;
        v *= 0.9;
        PA[k] = disp;
    }
}

struct PV {
    i64 x, y, vx, vy, cd;
};

static inline i64 d2(i64 dx, i64 dy) { return dx * dx + dy * dy; }
static inline i64 dist(i64 ax, i64 ay, i64 bx, i64 by) { return isqrt(d2(bx - ax, by - ay)); }
static inline i64 clmp(i64 v, i64 lo, i64 hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline i64 iabs(i64 v) { return v < 0 ? -v : v; }
static inline i64 mn(i64 a, i64 b) { return a < b ? a : b; }

struct Traj {
    std::vector<i64> x, y;  // x[k] = k 步后球的位置（k=0 现在）
    int n = 0;
    int goal = 0;  // +1 进右门（我们得分），-1 进左门（对方得分）
};

static Traj predict(i64 bx, i64 by, i64 bvx, i64 bvy, int n) {
    Traj t;
    t.x.push_back(bx);
    t.y.push_back(by);
    std::vector<soccer::BallPt> out;
    t.goal = soccer::predict_ball(CF, bx, by, bvx, bvy, n, out);
    for (const auto& p : out) {
        t.x.push_back(p.x);
        t.y.push_back(p.y);
    }
    t.n = (int)t.x.size() - 1;
    if (t.goal == 0 && t.n < n) {  // 球停住：位置保持
        while (t.n < n) {
            t.x.push_back(t.x.back());
            t.y.push_back(t.y.back());
            ++t.n;
        }
    }
    return t;
}

// 球员最早能到 Traj 球 500 范围内的步数（线性动力学可达集，略保守）；够不着返回大数
static int reach_first(const PV& p, const Traj& t, int kmin, double R) {
    int k0 = std::max(1, kmin);
    for (int k = k0; k <= t.n; ++k) {
        const double ex = (double)p.x + p.vx * PD[k];
        const double ey = (double)p.y + p.vy * PD[k];
        const double dx = (double)t.x[k] - ex, dy = (double)t.y[k] - ey;
        if (std::sqrt(dx * dx + dy * dy) <= PA[k] + R) return k;
    }
    return 1000;
}

// 全速追 Traj 的球（备用口径，用于角色广播的 T）
static int pursuit_time(const Traj& tr, i64 px, i64 py, i64 pvx, i64 pvy, int maxk) {
    for (int k = 0;; ++k) {
        const int bk = std::min(k, tr.n);
        if (d2(tr.x[bk] - px, tr.y[bk] - py) <= KREACH * KREACH) return k;
        if (k >= maxk) return maxk + 1;
        const int tk = std::min(k + 1, tr.n);
        i64 ax = tr.x[tk] - px - pvx, ay = tr.y[tk] - py - pvy;
        clamp_len(ax, ay, ACC);
        pvx += ax;
        pvy += ay;
        px += pvx;
        py += pvy;
        if (px < RP) {
            px = RP;
            if (pvx < 0) pvx = 0;
        }
        if (px > W - RP) {
            px = W - RP;
            if (pvx > 0) pvx = 0;
        }
        if (py < RP) {
            py = RP;
            if (pvy < 0) pvy = 0;
        }
        if (py > H - RP) {
            py = H - RP;
            if (pvy > 0) pvy = 0;
        }
        pvx = tdiv(pvx * 9, 10);
        pvy = tdiv(pvy * 9, 10);
    }
}

// 朝拦截点全速（考虑当前速度漂移）；已经能准时到则稳住
static void chase_to(const PV& me, i64 tx, i64 ty, int k, i64& ax, i64& ay) {
    if (k < 1) k = 1;
    const double ex = me.x + me.vx * PD[k];
    const double ey = me.y + me.vy * PD[k];
    const double dx = (double)tx - ex, dy = (double)ty - ey;
    const double d = std::sqrt(dx * dx + dy * dy);
    if (d < 40) {  // 能准时到：直接驶向目标（带减速）
        ax = tx - me.x - me.vx;
        ay = ty - me.y - me.vy;
        clamp_len(ax, ay, ACC);
        return;
    }
    ax = (i64)std::llround(dx / d * (double)ACC);
    ay = (i64)std::llround(dy / d * (double)ACC);
}

// 到目标点停靠（带速度补偿）
static void aim_accel(const PV& me, i64 tx, i64 ty, i64& ax, i64& ay) {
    ax = tx - me.x - me.vx;
    ay = ty - me.y - me.vy;
    clamp_len(ax, ay, ACC);
}

// 踢球评估：球从 (bx,by) 以 (vx,vy) 出发后的发展
static double eval_kick(i64 bx, i64 by, i64 vx, i64 vy, const PV& me, const PV& mate, const PV opp[2], Traj& T) {
    T = predict(bx, by, vx, vy, HOR);
    int tUs = mn(reach_first(me, T, 6, 470), reach_first(mate, T, (int)std::max(1LL, mate.cd), 470));
    const int tOpp = mn(reach_first(opp[0], T, (int)std::max(1LL, opp[0].cd), 480),
                        reach_first(opp[1], T, (int)std::max(1LL, opp[1].cd), 480));
    if (T.goal == 1) {
        // 进球概率：看对手在球进门途中能不能拦（沿轨迹的可达亏量）
        double dmin = 1e18;
        for (int o = 0; o < 2; ++o)
            for (int k = 1; k <= T.n; ++k) {
                const double ex = opp[o].x + opp[o].vx * PD[k];
                const double ey = opp[o].y + opp[o].vy * PD[k];
                const double dx = (double)mn(T.x[k], 11850LL) - ex, dy = (double)T.y[k] - ey;
                dmin = std::min(dmin, std::sqrt(dx * dx + dy * dy) - PA[k]);
            }
        double pg = 0.78 + dmin / 700.0;
        if (pg < 0.05) pg = 0.05;
        if (pg > 0.97) pg = 0.97;
        return 2.5 * pg - (1.0 - pg) - 0.0008 * T.n;
    }
    if (T.goal == -1) return -2.5;  // 自家门方向，直接否决
    const double stopX = T.x[T.n], stopY = T.y[T.n];
    const double cy = std::fabs(stopY - (double)GCY) / (H / 2.0);
    if (tUs >= 1000 && tOpp >= 1000) return 0.1 + 0.5 * (stopX / (double)W) - 0.05 * cy;
    double pu;
    if (tUs >= 1000) pu = 0.0;
    else if (tOpp >= 1000) pu = 1.0;
    else pu = 1.0 / (1.0 + std::exp(-(double)(tOpp - tUs) / 1.5));
    const int tu = mn(tUs, T.n), to = mn(tOpp, T.n);
    const double ux = (tUs < 1000 ? T.x[tu] : stopX), uy = (tUs < 1000 ? T.y[tu] : stopY);
    const double ox = (tOpp < 1000 ? T.x[to] : stopX), oy = (tOpp < 1000 ? T.y[to] : stopY);
    const double vUs = 0.15 + 0.5 * (ux / (double)W) - 0.04 * (std::fabs(uy - GCY) / (H / 2.0));
    const double vOpp = -0.15 - 0.5 * (1.0 - ox / (double)W) + 0.04 * (std::fabs(oy - GCY) / (H / 2.0));
    return pu * vUs + (1.0 - pu) * vOpp;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::string magic;
    i64 ver;
    if (!(std::cin >> magic >> ver)) return 0;
    i64 goalw, cham, pf_n, pf_d, bf_n, bf_d, kcd, ewn, ewd, ebn, ebd, epn, epd, steps, stall, msgmax, idx, sms, fms;
    std::cin >> W >> H >> goalw >> cham >> RP >> RB >> ACC >> pf_n >> pf_d >> bf_n >> bf_d >> KMAX >> KREACH >> kcd >>
        ewn >> ewd >> ebn >> ebd >> epn >> epd >> steps >> stall >> msgmax >> idx >> sms >> fms;
    CF.w = W;
    CF.h = H;
    CF.goal = goalw;
    CF.chamfer = cham;
    CF.rp = RP;
    CF.rb = RB;
    CF.acc_max = ACC;
    CF.pf_n = pf_n;
    CF.pf_d = pf_d;
    CF.bf_n = bf_n;
    CF.bf_d = bf_d;
    CF.kick_max = KMAX;
    CF.kick_reach = KREACH;
    CF.kick_cooldown = kcd;
    CF.e_wall_n = ewn;
    CF.e_wall_d = ewd;
    CF.e_ball_n = ebn;
    CF.e_ball_d = ebd;
    CF.e_pp_n = epn;
    CF.e_pp_d = epd;
    CF.steps = steps;
    CF.stall = stall;
    CF.msg_max = msgmax;
    IDX = (int)idx;
    GCY = H / 2;
    init_tables();

    i64 T, LEFT, MY, OPP, MATE_MSG, bx, by, bvx, bvy;
    PV pl[4];
    bool have_prev = false;
    int myPrevT = 0;
    bool myPrevChaser = false;

    while (std::cin >> T >> LEFT >> MY >> OPP >> MATE_MSG >> bx >> by >> bvx >> bvy) {
        for (auto& p : pl) std::cin >> p.x >> p.y >> p.vx >> p.vy >> p.cd;
        const PV me = pl[0], mate = pl[1];
        const PV opp[2] = {pl[2], pl[3]};

        const Traj tr = predict(bx, by, bvx, bvy, HOR);
        const int myT = pursuit_time(tr, me.x, me.y, me.vx, me.vy, HOR);
        const int oppT0 = pursuit_time(tr, opp[0].x, opp[0].y, opp[0].vx, opp[0].vy, HOR);
        const int oppT1 = pursuit_time(tr, opp[1].x, opp[1].y, opp[1].vx, opp[1].vy, HOR);
        const int oppBest = mn(oppT0, oppT1);

        // ---- 角色：用上一步双方广播的值（与队友约定，保证两人结论一致）----
        const int mateEnc = (int)(MATE_MSG & 63);
        const bool mateCh = (MATE_MSG & 64) != 0;
        bool chaser;
        if (!have_prev || mateEnc == 0) {
            if (!have_prev) {
                chaser = (IDX == 0);  // 第 0 步：0 号抢、1 号守
            } else {
                // 队友无信息：自己替他算
                const int mt = mn(pursuit_time(tr, mate.x, mate.y, mate.vx, mate.vy, HOR), 62);
                const int meE = mn(myT, 62);
                chaser = (meE < mt) || (meE == mt && IDX == 0);
            }
        } else {
            const int meEff = myPrevT - (myPrevChaser ? 3 : 0);
            const int mateEff = (mateEnc - 1) - (mateCh ? 3 : 0);
            chaser = (meEff < mateEff) || (meEff == mateEff && IDX == 0);
        }

        i64 ax = 0, ay = 0, kx = 0, ky = 0;
        bool will_kick = false;
        const bool in_reach = (me.cd == 0 && d2(bx - me.x, by - me.y) <= KREACH * KREACH);
        const i64 mateBall = dist(mate.x, mate.y, bx, by);
        const bool mateCanKick = (mateBall <= KREACH && mate.cd == 0);

        // ---- 踢球决策：枚举 + 模拟评分 ----
        Traj bestT;
        bool haveKick = false;
        if (in_reach && (chaser || !mateCanKick)) {
            double bestV = -1e18;
            i64 bkx = 0, bky = 0;
            auto tryK = [&](double ang, double sp) {
                i64 cx = (i64)std::llround(std::cos(ang) * sp), cy = (i64)std::llround(std::sin(ang) * sp);
                clamp_len(cx, cy, KMAX);
                if (cx == 0 && cy == 0) return;
                Traj T2;
                const double v = eval_kick(bx, by, cx, cy, me, mate, opp, T2);
                if (v > bestV) {
                    bestV = v;
                    bkx = cx;
                    bky = cy;
                    bestT = T2;
                }
            };
            const bool desperate = (LEFT < 250 && MY < OPP);
            const double PI = 3.14159265358979323846;
            for (i64 gy = 3150; gy <= 4850; gy += 170) tryK(std::atan2((double)gy - by, (double)W - bx), (double)KMAX);
            const double speeds[] = {800, 620, 460, 330, 220};
            for (int a = 0; a < 24; ++a) {
                const double ang = a * (2 * PI / 24);
                for (double sp : speeds) tryK(ang, sp);
            }
            // 不踢的选项：球已经在往对方门里走
            double vNo = -1e18;
            if (tr.goal == 1) vNo = 1.5;
            if (!desperate && bestV > vNo) {
                kx = bkx;
                ky = bky;
                will_kick = true;
                haveKick = true;
            } else if (desperate && bestV > -2.4) {  // 搏命：几乎总是踢
                kx = bkx;
                ky = bky;
                will_kick = true;
                haveKick = true;
            }
        }

        // ---- 移动 ----
        if (haveKick) {
            // 踢完跟上自己的球
            const int k = reach_first(me, bestT, 6, 470);
            if (k < 1000) chase_to(me, bestT.x[k], bestT.y[k], k, ax, ay);
            else chase_to(me, bestT.x[mn(bestT.n, 12)], bestT.y[mn(bestT.n, 12)], mn(bestT.n, 12), ax, ay);
            chaser = true;
        } else if (chaser) {
            // 球冲着我方门来且追不上：堵轨迹
            int cross = -1;
            if (bvx < -120) {
                for (int i = 0; i <= tr.n; ++i)
                    if (tr.x[i] <= 1800) {
                        cross = i;
                        break;
                    }
            }
            if (cross >= 0 && myT > cross + 2) {
                aim_accel(me, std::max(tr.x[cross], 500LL), clmp(tr.y[cross], 500, H - 500), ax, ay);
            } else if (oppBest < myT && tr.goal != -1) {
                // 对手比我先拿到球：退回门前，站 拿球点->我方门中心 连线上距门 ~1000
                const int oppTb = (oppT0 <= oppT1) ? oppT0 : oppT1;
                const int bk = mn(oppTb, tr.n);
                const i64 gx = tr.x[bk], gy = tr.y[bk];
                i64 dx = -gx, dy = GCY - gy;
                i64 L = isqrt(d2(dx, dy));
                if (L < 1) {
                    dx = -1;
                    dy = 0;
                    L = 1;
                }
                const i64 off = clmp(tdiv(L * 25, 100), 700LL, 1200LL);
                aim_accel(me, clmp(gx + tdiv(dx * off, L), RP, W - RP), clmp(gy + tdiv(dy * off, L), RP, H - RP), ax,
                          ay);
            } else if (me.cd > 0 && me.x > bx + 150 && bvx > -50) {
                // 球在我前方且暂时不能踢：绕到球后
                aim_accel(me, bx - 800, by + (me.y > by ? 400 : -400), ax, ay);
            } else {
                const int k = mn(myT, tr.n);
                chase_to(me, tr.x[k], tr.y[k], k, ax, ay);
            }
        } else {
            // ---- 防守 / 接应 ----
            const i64 nOppBall = mn(dist(opp[0].x, opp[0].y, bx, by), dist(opp[1].x, opp[1].y, bx, by));
            const bool poss = (nOppBall > 1200);
            const bool ball_coming = (bvx < -120);
            if (tr.goal == -1 && myT < tr.n) {
                // 球奔我方门且我能截住：上！
                const int k = mn(myT, tr.n);
                chase_to(me, tr.x[k], tr.y[k], k, ax, ay);
            } else if (tr.goal == -1) {
                // 堵轨迹与 x<=1000 的交点
                int k = -1;
                for (int i = 0; i <= tr.n; ++i)
                    if (tr.x[i] <= 1000) {
                        k = i;
                        break;
                    }
                if (k < 0) k = tr.n;
                aim_accel(me, std::max(tr.x[k], 420LL), clmp(tr.y[k], 500, H - 500), ax, ay);
            } else if (ball_coming) {
                // 球在回来：抢在轨迹 x<=1600 的点回防
                int k = -1;
                for (int i = 0; i <= tr.n; ++i)
                    if (tr.x[i] <= 1600) {
                        k = i;
                        break;
                    }
                if (k >= 0) aim_accel(me, std::max(tr.x[k], 500LL), clmp(tr.y[k], 500, H - 500), ax, ay);
                else {
                    const i64 d = 800;
                    i64 dx = bx, dy = by - GCY;
                    i64 L = isqrt(d2(dx, dy));
                    if (L < 1) {
                        dx = 1;
                        dy = 0;
                        L = 1;
                    }
                    aim_accel(me, std::max(tdiv(dx * d, L), 420LL), GCY + tdiv(dy * d, L), ax, ay);
                }
            } else if (poss && bx > 5200 && bvx > 150) {
                // 我方明显控球且球快速压向对方门：短暂接应（球后 2400，y 折中，不过半场太深）
                aim_accel(me, clmp(bx - 2400, 2400, 5400), (by + GCY) / 2, ax, ay);
            } else {
                // 常规防守：球-门连线上，距门 clamp(0.3*距离, 800, 2400)
                const i64 bd = dist(bx, by, 0, GCY);
                const i64 d = clmp(bd * 3 / 10, 800, 2400);
                i64 dx = bx, dy = by - GCY;
                i64 L = isqrt(d2(dx, dy));
                if (L < 1) {
                    dx = 1;
                    dy = 0;
                    L = 1;
                }
                aim_accel(me, std::max(tdiv(dx * d, L), 420LL), GCY + tdiv(dy * d, L), ax, ay);
            }
        }

        // ---- 消息 ----
        int bT = mn(myT, 62);
        // 我是后卫且球有争抢（非我方门危险）：广播加罚，让前面的人去抢
        if (me.x < mate.x - 1000 && oppBest <= myT + 2 && tr.goal != -1 && myT > 0) bT = mn(62, bT + 4);
        int msg = (bT + 1) | (chaser ? 64 : 0) | (will_kick ? 128 : 0);
        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << msg << std::endl;

        myPrevT = bT;
        myPrevChaser = chaser;
        have_prev = true;
    }
    return 0;
}
