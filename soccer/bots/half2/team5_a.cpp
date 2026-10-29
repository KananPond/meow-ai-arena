// Opus（A 位）球员程序：用 soccer.hpp 精确预测球，按“谁先够到球”分配抢球 / 防守，
// 踢球时枚举方向和力度，模拟球路评估（进球概率、谁先拿到球、落点位置）。
// 队内消息（与队友约定）：bit0-5 = min(T,62)+1（T = 我算的自己的拦截步数，0 = 无信息），
// bit6 = 我是抢球者，bit7 = 本步我有效踢球。
#include "soccer.hpp"
#include <bits/stdc++.h>
using namespace std;
typedef long long ll;

struct Pl { ll x, y, vx, vy, cd; };

static soccer::Config CFG;
static const int HOR = 100;
static double Dk[HOR + 2], Ak[HOR + 2];
static int IDX = 0;
#ifndef BACKPEN_V
#define BACKPEN_V 4
#endif
#ifndef PG0
#define PG0 0.72
#endif
#ifndef PGS
#define PGS 600.0
#endif
#ifndef KEEPF
#define KEEPF 0.3
#endif
#ifndef KEEPMAX
#define KEEPMAX 2400.0
#endif
#ifndef GSOFF
#define GSOFF 1300.0
#endif
#ifndef REACTS
#define REACTS 1
#endif
#ifndef REACTD
#define REACTD 0
#endif
#ifndef CYW
#define CYW 0.06
#endif
#ifndef GSM
#define GSM 0
#endif
static int BACKPEN = BACKPEN_V;

static void init_tables() {
    double p = 1.0, s = 0;
    Dk[0] = Ak[0] = 0;
    for (int k = 1; k <= HOR + 1; ++k) {
        p *= 0.9;
        Dk[k] = 10.0 * (1.0 - p);
        s += 10.0 * (1.0 - p);
        Ak[k] = 40.0 * s * 0.985;
    }
}

// 球的轨迹：tx[k], ty[k] = k 步后的位置（k>=1）；goalStep = 进门那一步（res=±1），否则 0
struct Traj {
    int n = 0;          // 有效长度（k = 1..n）
    int res = 0;        // +1 进对方门，-1 进自家门
    double x[HOR + 2], y[HOR + 2];
};

static vector<soccer::BallPt> g_out;

static void make_traj(ll bx, ll by, ll vx, ll vy, Traj& T) {
    int res = soccer::predict_ball(CFG, bx, by, vx, vy, HOR, g_out);
    int n = (int)g_out.size();
    T.res = res;
    if (n == 0) {
        for (int k = 1; k <= HOR; ++k) T.x[k] = bx, T.y[k] = by;
        T.n = HOR;
        return;
    }
    for (int k = 1; k <= n; ++k) T.x[k] = g_out[k - 1].x, T.y[k] = g_out[k - 1].y;
    if (res == 0) {
        for (int k = n + 1; k <= HOR; ++k) T.x[k] = T.x[n], T.y[k] = T.y[n];
        T.n = HOR;
    } else {
        T.n = n;
    }
}

// 球员 p 在 k 步后能否到达 (tx,ty) 的 R 范围内（线性动力学下的精确可达集，略保守）
static inline double deficit_late(const Pl& p, int k, double tx, double ty, double R, int late = REACTD) {
    double ex = p.x + p.vx * Dk[k], ey = p.y + p.vy * Dk[k];
    double dx = tx - ex, dy = ty - ey;
    return sqrt(dx * dx + dy * dy) - (Ak[k - late] + R);
}
static inline double deficit(const Pl& p, int k, double tx, double ty, double R) {
    double ex = p.x + p.vx * Dk[k], ey = p.y + p.vy * Dk[k];
    double dx = tx - ex, dy = ty - ey;
    return sqrt(dx * dx + dy * dy) - (Ak[k] + R);
}

// 球员最早能控球的步数（kmin 起），够不着返回 INF
static const int INF = 1000;
static int first_reach(const Pl& p, const Traj& T, int kmin, double R) {
    if (kmin < 1) kmin = 1;
    for (int k = kmin; k <= T.n; ++k)
        if (deficit(p, k, T.x[k], T.y[k], R) <= 0) return k;
    return INF;
}

static double pos_value(int side, double x, double y) {
    double cy = fabs(y - 4000.0) / 4000.0;
    if (side > 0) return 0.25 + 0.45 * (x / 12000.0) - CYW * cy;
    return -0.25 - 0.45 * (1.0 - x / 12000.0) + CYW * cy;
}

static Pl P[4];  // 0 我 1 队友 2,3 对手
static const double R_US = 470, R_OPP = 480;

// 评估：球从 (bx,by) 以速度 (vx,vy) 出发（本步已踢）。kminMe/kminMate = 我/队友最早能再踢的步数
static double eval_ball(ll bx, ll by, ll vx, ll vy, int kminMe, int kminMate, Traj& T) {
    make_traj(bx, by, vx, vy, T);
    int tUs = INF, tOpp = INF;
    tUs = min(first_reach(P[0], T, kminMe, R_US), first_reach(P[1], T, kminMate, R_US));
    for (int o = 2; o < 4; ++o)
        for (int k = 1; k <= T.n && k < tOpp; ++k)
            if (deficit_late(P[o], k, T.x[k], T.y[k], R_OPP) <= 0) { tOpp = k; break; }
    if (T.res == 1) {
        int g = T.n;
        double dmin = 1e18;
        for (int o = 2; o < 4; ++o)
            for (int k = 1; k <= g; ++k) {
                double tx = T.x[k];
                if (tx > 11850) tx = 11850;
                dmin = min(dmin, deficit_late(P[o], k, tx, T.y[k], R_OPP, REACTS));
            }
        double pg = PG0 + dmin / PGS;
        pg = max(0.03, min(0.97, pg));
        double vb = pos_value(-1, 11300, 4000);
        return pg * 1.0 + (1 - pg) * vb - 0.001 * g;
    }
    if (T.res == -1) {
        int g = T.n;
        if (tUs >= g || tOpp < tUs) return -1.0 - 0.001 * (100 - g);
    }
    double vu = 0, vo = 0, pu;
    if (tUs >= INF && tOpp >= INF) return pos_value(1, T.x[T.n], T.y[T.n]) * 0.0;
    if (tUs < INF) vu = pos_value(1, T.x[tUs], T.y[tUs]);
    if (tOpp < INF) vo = pos_value(-1, T.x[tOpp], T.y[tOpp]);
    if (tUs >= INF) pu = 0;
    else if (tOpp >= INF) pu = 1;
    else pu = 1.0 / (1.0 + exp(-(double)(tOpp - tUs) / 1.5));
    return pu * vu + (1 - pu) * vo;
}

#ifndef SMARTKEEP
#define SMARTKEEP 1
#endif
// 智能门将：从威胁点 (sx,sy) 枚举 800 力度的射门，找出会进我方门的球路；
// 在门前网格里选能封住最多球路的站位（同分取离默认点 (tx,ty) 近的）
static void smart_keeper(double sx, double sy, double& tx, double& ty) {
    static vector<soccer::BallPt> o;
    static double SX[80][45], SY[80][45];
    static int SN[80];
    int ns = 0;
    for (int a = 0; a < 72; ++a) {
        double ang = a * (2 * 3.14159265358979323846 / 72);
        ll vx = llround(cos(ang) * 800), vy = llround(sin(ang) * 800);
        int res = soccer::predict_ball(CFG, (ll)sx, (ll)sy, vx, vy, 40, o);
        if (res != -1) continue;
        int n = min((int)o.size(), 40);
        for (int k = 0; k < n; ++k) SX[ns][k + 1] = o[k].x, SY[ns][k + 1] = o[k].y;
        SN[ns] = n;
        ++ns;
    }
    if (ns == 0) return;
    double bestS = -1e18, bx = tx, by = ty;
    static const double CXS[] = {305, 550, 850, 1200, 1600, 2100, 2700};
    for (double cx : CXS)
        for (double cy = 2700; cy <= 5300; cy += 200) {
            Pl kp{(ll)cx, (ll)cy, 0, 0, 0};
            int cover = 0;
            for (int s = 0; s < ns; ++s) {
                bool ok = false;
                for (int k = 1; k <= SN[s]; ++k) {
                    double ddx = SX[s][k] - cx, ddy = SY[s][k] - cy;
                    double r = Ak[k] + 450;
                    if (ddx * ddx + ddy * ddy <= r * r) { ok = true; break; }
                }
                if (ok) ++cover;
            }
            double dd = sqrt((cx - tx) * (cx - tx) + (cy - ty) * (cy - ty));
            double sc = cover * 1000.0 - dd;
            if (sc > bestS) bestS = sc, bx = cx, by = cy;
        }
    tx = bx;
    ty = by;
}

static void acc_toward(double dx, double dy, ll& ax, ll& ay) {
    double d = sqrt(dx * dx + dy * dy);
    if (d < 1e-6) { ax = ay = 0; return; }
    ax = llround(dx / d * 40.0);
    ay = llround(dy / d * 40.0);
}

// 以“到达后停住”的方式移动到目标点
static void move_to(const Pl& me, double tx, double ty, ll& ax, ll& ay) {
    double dx = tx - me.x, dy = ty - me.y;
    double d = sqrt(dx * dx + dy * dy);
    double sp = min(395.0, d * 0.35);
    double vdx = 0, vdy = 0;
    if (d > 1e-6) vdx = dx / d * sp, vdy = dy / d * sp;
    double ex = vdx - me.vx, ey = vdy - me.vy;
    double e = sqrt(ex * ex + ey * ey);
    if (e > 40) ex = ex / e * 40, ey = ey / e * 40;
    ax = llround(ex);
    ay = llround(ey);
    // 限长后裁判还会再限一次，llround 可能让长度略超 40，没关系（裁判按比例缩短）
}

// 朝拦截点全速
static void chase(const Pl& me, const Traj& T, int k, ll& ax, ll& ay) {
    if (k < 1) k = 1;
    if (k > T.n) k = T.n;
    double ex = me.x + me.vx * Dk[k], ey = me.y + me.vy * Dk[k];
    double dx = T.x[k] - ex, dy = T.y[k] - ey;
    double d = sqrt(dx * dx + dy * dy);
    if (d < 30) {
        // 已经能准时到：顺带减速，免得冲过头
        move_to(me, T.x[k], T.y[k], ax, ay);
        return;
    }
    acc_toward(dx, dy, ax, ay);
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    init_tables();
    string magic;
    ll ver;
    ll W, H, GOAL, CHAMFER, RP, RB, ACC_MAX, PF_N, PF_D, BF_N, BF_D, KICK_MAX, KICK_REACH, KICK_COOLDOWN;
    ll ewn, ewd, ebn, ebd, epn, epd, STEPS, STALL, MSG_MAX, idx, STEP_MS, FIRST_MS;
    if (!(cin >> magic >> ver)) return 0;
    cin >> W >> H >> GOAL >> CHAMFER >> RP >> RB >> ACC_MAX >> PF_N >> PF_D >> BF_N >> BF_D >> KICK_MAX >> KICK_REACH >>
        KICK_COOLDOWN >> ewn >> ewd >> ebn >> ebd >> epn >> epd >> STEPS >> STALL >> MSG_MAX >> idx >> STEP_MS >> FIRST_MS;
    IDX = (int)idx;

    bool wasChaser = false;
    int prevMyT = -1;
    int zeroMsg = 0;
    static Traj cur, tmp, best;
    ll t, left, mys, ops, mmsg, bx, by, bvx, bvy;
    while (cin >> t >> left >> mys >> ops >> mmsg >> bx >> by >> bvx >> bvy) {
        for (int i = 0; i < 4; ++i) cin >> P[i].x >> P[i].y >> P[i].vx >> P[i].vy >> P[i].cd;
        auto stepStart = std::chrono::steady_clock::now();
        const Pl& me = P[0];
        make_traj(bx, by, bvx, bvy, cur);

        // 自己的拦截步数
        ll dbx = bx - me.x, dby = by - me.y;
        bool inReach = dbx * dbx + dby * dby <= 500LL * 500LL;
        int myT;
        if (inReach && me.cd == 0) myT = 0;
        else myT = first_reach(me, cur, (int)me.cd, R_US);

        // 角色：两人都用“上一步双方发出的 T 和抢球标志”比较，手里的数相同 → 结论一致
        bool chaser;
        if (mmsg == 0) ++zeroMsg; else zeroMsg = 0;
        if (mmsg == 0 && zeroMsg >= 3) {
            // 队友不发消息（不按协议）：自己估算两人的拦截步数
            ll mdx0 = bx - P[1].x, mdy0 = by - P[1].y;
            int mateT = (mdx0 * mdx0 + mdy0 * mdy0 <= 250000 && P[1].cd == 0) ? 0
                                                                                : first_reach(P[1], cur, (int)P[1].cd, R_US);
            mateT = min(mateT, 62);
            // 不按协议的队友多半见球就扑：我偏向留守，明显更近才去抢
            int myEff = min(myT, 62) - (wasChaser ? 3 : 0) + 5;
            chaser = myEff < mateT;
        } else if (mmsg == 0 || prevMyT < 0) {
            chaser = (IDX == 0);
        } else {
            int mateT = (int)(mmsg & 63) - 1;
            bool mateChaser = (mmsg & 64) != 0;
            int myEff = prevMyT - (wasChaser ? 3 : 0);
            int mateEff = mateT - (mateChaser ? 3 : 0);
            chaser = myEff < mateEff || (myEff == mateEff && IDX == 0);
        }

        // 对手最早拿球
        int oppT = INF;
        for (int o = 2; o < 4; ++o) oppT = min(oppT, first_reach(P[o], cur, 1, R_OPP));

        ll ax = 0, ay = 0, kx = 0, ky = 0;
        bool kicked = false;

        // 踢球决策
        ll mdx = bx - P[1].x, mdy = by - P[1].y;
        bool mateCanKick = (mdx * mdx + mdy * mdy <= 250000) && P[1].cd == 0;
        if (inReach && me.cd == 0 && (chaser || !mateCanKick)) {
            double bestV = -1e18;
            ll bkx = 0, bky = 0;
            int kminMate = (int)max<ll>(1, P[1].cd);
            auto tryKick = [&](double ang, double sp) {
                ll cx = llround(cos(ang) * sp), cy = llround(sin(ang) * sp);
                soccer::clamp_len(cx, cy, 800);
                if (cx == 0 && cy == 0) return;
                double v = eval_ball(bx, by, cx, cy, 6, kminMate, tmp);
                if (v > bestV) {
                    bestV = v;
                    bkx = cx;
                    bky = cy;
                }
            };
            for (int gy = 3150; gy <= 4850; gy += 100) {
                double ang = atan2((double)gy - by, 12000.0 - bx);
                tryKick(ang, 800);
            }
            static const double speeds[] = {800, 650, 500, 380, 270, 180};
            for (int a = 0; a < 72; ++a) {
                double ang = a * (2 * 3.14159265358979323846 / 72);
                for (double sp : speeds) tryKick(ang, sp);
                if ((a & 7) == 7) {
                    double el = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - stepStart).count();
                    if (el > 4.5) break;
                }
            }
            // 不踢的选项：球已经在往对方门里走，且对手拦不到
            double vNo = -1e18;
            if (cur.res == 1) vNo = eval_ball(bx, by, bvx, bvy, 1, kminMate, tmp);
            if (bestV > vNo) {
                kx = bkx;
                ky = bky;
                kicked = true;
                make_traj(bx, by, kx, ky, best);
            }
        }

        if (kicked) {
            // 踢完跟上：冷却 5 步后去追自己的球
            int k = first_reach(me, best, 6, R_US);
            if (k < INF) chase(me, best, k, ax, ay);
            else chase(me, best, min(best.n, 12), ax, ay);
            chaser = true;
        } else if (chaser && oppT < INF && oppT + GSM < myT && cur.res != -1) {
            // 对手明显先拿到球：站到他拿球点与我方球门之间，封射门线
            double ox = cur.x[oppT], oy = cur.y[oppT];
            double dx = 0 - ox, dy = 4000 - oy;
            double d = sqrt(dx * dx + dy * dy);
            double off = min(GSOFF, d * 0.45);
            double tx = ox + (d > 1 ? dx / d * off : 0), ty = oy + (d > 1 ? dy / d * off : 0);
            if (tx < 305) tx = 305;
#ifndef GSKEEP
#define GSKEEP 12001
#endif
#if GSKEEP > 0
            // 队友守门不可靠：我也退回门前，站智能门将位
            if (ox < GSKEEP) {
                double want = max(800.0, min(KEEPMAX, d * KEEPF));
                tx = (d > 1 ? -dx / d * want : want);
                ty = 4000 + (d > 1 ? -dy / d * want : 0);
                if (tx < 305) tx = 305;
                smart_keeper(ox, oy, tx, ty);
            }
#endif
            move_to(me, tx, ty, ax, ay);
        } else if (chaser) {
            if (myT < INF && myT >= 1) chase(me, cur, myT, ax, ay);
            else if (myT == 0) move_to(me, cur.x[1], cur.y[1], ax, ay);
            else chase(me, cur, min(cur.n, 30), ax, ay);
        } else {
            // 防守：球要进自家门且我能截到 → 截
            if (cur.res == -1 && myT < INF) {
                chase(me, cur, myT, ax, ay);
            } else {
                int kk = min(cur.n, 4);
                double fx = cur.x[kk], fy = cur.y[kk];
                double gx = 0, gy = 4000;
                double dx = fx - gx, dy = fy - gy;
                double d = sqrt(dx * dx + dy * dy);
                double want = max(800.0, min(KEEPMAX, d * KEEPF));
                double tx = gx + (d > 1 ? dx / d * want : want), ty = gy + (d > 1 ? dy / d * want : 0);
                if (tx < 305) tx = 305;
#if SMARTKEEP
                {
                    // 威胁点：对手拿球处（拿不到就用 4 步后的球位）
                    double sx = fx, sy = fy;
                    if (oppT < INF) sx = cur.x[oppT], sy = cur.y[oppT];
                    if (sx < 7500) smart_keeper(sx, sy, tx, ty);
                }
#endif
                move_to(me, tx, ty, ax, ay);
            }
        }

        wasChaser = chaser;
        // 广播的 T：我是后卫（明显比队友靠后）且球有争抢、球不是正往自家门里去 → 加罚，让前面的人去抢，自己留守
        int bT = min(myT, 62);
        if (BACKPEN > 0 && me.x < P[1].x - 1000 && oppT <= myT + 2 && cur.res != -1 && myT > 0) bT = min(62, bT + BACKPEN);
#ifdef FORCE_ROLE
        if (FORCE_ROLE == 1) bT = 62;
        if (FORCE_ROLE == 2) bT = 0;
#endif
        prevMyT = bT;
        int msg = (bT + 1) | (chaser ? 64 : 0) | (kicked ? 128 : 0);
        cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << msg << '\n';
        cout.flush();
        (void)oppT;
    }
    return 0;
}
