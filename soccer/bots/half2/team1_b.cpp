// 2v2 足球 · 队内 1 名球员的通用控制程序（混元 Hy4，B 位）
//
// 与队友约定的 1 字节协议（每步都发）：
//   bit0..4 : 我到球的 ETA（步数，31 = 追不上）；ROLE=KICK 时改为射门/传球目标 y 档
//   bit5..6 : ROLE  0=ATTACK 主攻  1=DEFEND 回防  2=SUPPORT 接应  3=KICK 我正踢，让开球路
//   bit7    : URGENT 我方门前危险
// 分工：ETA 小者主攻，带 3 步迟滞，防止两人每步对调扑球。
//
// 核心：用 soccer::predict_ball 精确预测球；ETA 用与裁判同样的整数运动模型模拟自己；
//       射门前扫描球门若干目标点，剔除会被对手身体挡住的线路；无好射门则小力量控球推进 / 传球。

#include "soccer.hpp"

#include <cstdio>
#include <vector>

using namespace soccer;
typedef long long i64;

static Config CF;
static int MYIDX = 0;

static const i64 MAXSPD = 360;   // 球员实际稳态速度（加速度 40、摩擦 9/10）
static const i64 KLEN = 800;     // 踢球向量上限
static const i64 REACH = 500;    // 可踢距离
static const i64 DANGER = 470;   // 判定球路被身体挡住的半径（rp+rb=450，留余量）
static const int ETA_CAP = 30;
// false = 抢球型（本文件）；partner.cpp 里为 true = 门将型
static const bool KEEPER = false;

static inline i64 d2(i64 x, i64 y) { return x * x + y * y; }
static inline i64 ilen(i64 x, i64 y) { return isqrt(x * x + y * y); }

struct P {
    i64 x = 0, y = 0, vx = 0, vy = 0, cd = 0;
};

// 加速度：朝目标点走，接近时自动减速（停止距离约 10*v）
static void steer_raw(i64 x, i64 y, i64 vx, i64 vy, i64 tx, i64 ty, i64& ax, i64& ay) {
    if (KEEPER) {
        // 门将型要"站得住"：速度匹配 + 接近目标减速，别绕着目标打转
        i64 dx = tx - x, dy = ty - y;
        i64 dd = ilen(dx, dy);
        i64 des = dd;
        if (des > MAXSPD) des = MAXSPD;
        if (des < 80) des = 80;
        i64 wvx = 0, wvy = 0;
        if (dd > 0) {
            wvx = dx * des / dd;
            wvy = dy * des / dd;
        }
        ax = wvx - vx;
        ay = wvy - vy;
    } else {
        // 抢球型：直接朝目标全速加速
        ax = tx - x;
        ay = ty - y;
    }
    clamp_len(ax, ay, 40);
}

static void steer(const P& me, i64 tx, i64 ty, i64& ax, i64& ay) {
    steer_raw(me.x, me.y, me.vx, me.vy, tx, ty, ax, ay);
}

// 假设该球员保持当前速度方向继续加速，k 步后的位置
static void pos_after(const P& p, int k, i64& ox, i64& oy) {
    i64 x = p.x, y = p.y, vx = p.vx, vy = p.vy;
    i64 sp = ilen(vx, vy), ax = 0, ay = 0;
    if (sp > 0) {
        ax = vx * 40 / sp;
        ay = vy * 40 / sp;
    }
    for (int i = 0; i < k; ++i) {
        vx += ax;
        vy += ay;
        x += vx;
        y += vy;
        vx = tdiv(vx * 9, 10);
        vy = tdiv(vy * 9, 10);
    }
    ox = x;
    oy = y;
}

// 我最早能在第几步（>= cd）够到球。bxs[t] = 第 t 步开始时球的位置（bxs[0] = 当前）
static int eta_ball(const P& p, const std::vector<i64>& bxs, const std::vector<i64>& bys, int maxT, i64 reach) {
    for (int T = p.cd; T <= maxT; ++T) {
        i64 x = p.x, y = p.y, vx = p.vx, vy = p.vy;
        i64 tx = bxs[T], ty = bys[T];
        for (int s = 0; s < T; ++s) {
            i64 ax, ay;
            steer_raw(x, y, vx, vy, tx, ty, ax, ay);
            vx += ax;
            vy += ay;
            x += vx;
            y += vy;
            vx = tdiv(vx * 9, 10);
            vy = tdiv(vy * 9, 10);
        }
        if (d2(tx - x, ty - y) <= reach * reach) return T;
    }
    return ETA_CAP + 1;
}

// 到静止目标点需要几步
static int eta_point(const P& p, i64 tx, i64 ty, i64 tol, int maxT) {
    i64 x = p.x, y = p.y, vx = p.vx, vy = p.vy;
    for (int s = 1; s <= maxT; ++s) {
        i64 ax, ay;
        steer_raw(x, y, vx, vy, tx, ty, ax, ay);
        vx += ax;
        vy += ay;
        x += vx;
        y += vy;
        vx = tdiv(vx * 9, 10);
        vy = tdiv(vy * 9, 10);
        if (d2(tx - x, ty - y) <= tol * tol) return s;
    }
    return 99;
}

// 对手能否在 k 步之内赶到球路上的某点（拿到球）
static bool opp_can_catch(const P& o, i64 px, i64 py, int k) {
    if (k <= 0) return false;
    return eta_point(o, px, py, 480, k) <= k;
}

// 球路预测：返回最小值 = 路径上离任一阻挡者（按外推位置）的最近距离
static i64 path_clearance(const std::vector<BallPt>& path, const P* blk, int nb) {
    i64 best = (i64)1 << 60;
    const int n = (int)path.size();
    for (int k = 0; k < n; ++k) {
        i64 ox, oy;
        for (int j = 0; j < nb; ++j) {
            pos_after(blk[j], k + 1, ox, oy);
            i64 dd = ilen(path[k].x - ox, path[k].y - oy);
            if (dd < best) best = dd;
        }
    }
    return best;
}

static std::vector<BallPt> PATH;
static std::vector<BallPt> PATH2;

// 射门：在球门开口上取若干目标点，选能进且离对手最远的线路
static bool pick_shot(i64 bx, i64 by, const P* blk, int nb, i64& okx, i64& oky, i64& oclear) {
    bool found = false;
    i64 bestScore = -(i64)1 << 50;
    static const i64 targets[] = {3020, 3120, 3220, 3350, 3480, 3620, 3760, 3900, 4040, 4180, 4320, 4460, 4600, 4740, 4880, 4980};
    for (int i = 0; i < 16; ++i) {
        i64 kx = 12400 - bx, ky = targets[i] - by;
        clamp_len(kx, ky, KLEN);
        if (predict_ball(CF, bx, by, kx, ky, 70, PATH) != 1) continue;
        i64 cl = path_clearance(PATH, blk, nb);
        if (cl < 200) continue;
        // 对手能不能在球进门前截下来
        int G = (int)PATH.size();
        bool caught = false;
        for (int k = 2; k <= G && k <= 24; k += 3) {
            if (opp_can_catch(blk[0], PATH[k - 1].x, PATH[k - 1].y, k) ||
                opp_can_catch(blk[1], PATH[k - 1].x, PATH[k - 1].y, k)) { caught = true; break; }
        }
        if (caught) continue;
        i64 steps = (i64)PATH.size();
        i64 sc = (cl > 3000 ? 3000 : cl) - steps * 25;
        if (sc > bestScore) {
            bestScore = sc;
            okx = kx;
            oky = ky;
            oclear = cl;
            found = true;
        }
    }
    return found;
}

// 控球 / 解围：在一圈方向里挑一个"往前、且不送对手"的踢法
static void pick_push(i64 bx, i64 by, const P* blk, int nb, i64 basePower, i64 clearW, i64& okx, i64& oky) {
    i64 bestScore = -(i64)1 << 50;
    okx = KLEN;
    oky = 0;
    for (int a = -7; a <= 7; ++a) {
        // 以正右方为 0、每档 12 度的整数方向表（避免浮点）
        static const i64 dirs[15][2] = {{1000, 0},   {978, 208},  {914, 406},  {809, 588},  {669, 743},
                                       {500, 866},   {309, 951},  {105, 995},  {-105, 995}, {-309, 951},
                                       {-500, 866},  {-669, 743}, {-809, 588}, {-914, 406}, {-978, 208}};
        i64 ux = dirs[a + 7][0], uy = dirs[a + 7][1];
        i64 kx = ux * basePower / 1000, ky = uy * basePower / 1000;
        clamp_len(kx, ky, KLEN);
        predict_ball(CF, bx, by, kx, ky, 24, PATH2);
        i64 cl = path_clearance(PATH2, blk, nb);
        int n = (int)PATH2.size();
        i64 endx = PATH2[n - 1].x;
        i64 sc = endx;                        // 越靠对方球门越好
        if (bx < 6000) sc += (endx - bx);     // 解围时更看重推进
        sc += (cl > 1600 ? 1600 : cl) * clearW;   // 远离对手 = 保住球权
        if (cl < DANGER) sc -= (DANGER - cl) * 6;
        if (cl < 250) sc -= 4000;             // 几乎贴着对手 = 送球
        // 对手能不能提前截到这一脚（越早被截越糟）
        int catch_t = 99;
        for (int k = 2; k <= n && k <= 20; k += 4) {
            if (opp_can_catch(blk[0], PATH2[k - 1].x, PATH2[k - 1].y, k) ||
                opp_can_catch(blk[1], PATH2[k - 1].x, PATH2[k - 1].y, k)) { catch_t = k; break; }
        }
        if (catch_t < 90) sc -= (20 - catch_t) * 900;
        if (sc > bestScore) {
            bestScore = sc;
            okx = kx;
            oky = ky;
        }
    }
}

// 传球给队友
static void pick_pass(i64 bx, i64 by, const P& mate, const P* blk, int nb, i64& okx, i64& oky) {
    i64 tx = mate.x + mate.vx * 3 + 400;
    i64 ty = mate.y + mate.vy * 3;
    if (tx > 11600) tx = 11600;
    if (tx < 200) tx = 200;
    if (ty < 400) ty = 400;
    if (ty > 7600) ty = 7600;
    i64 dd = ilen(tx - bx, ty - by);
    i64 power = dd / 5;
    if (power < 220) power = 220;
    if (power > KLEN) power = KLEN;
    i64 kx = tx - bx, ky = ty - by;
    clamp_len(kx, ky, power);
    okx = kx;
    oky = ky;
    (void)nb;
    (void)blk;
}

int main() {
    char tag[64];
    i64 ver = 0;
    if (scanf("%63s %lld", tag, &ver) != 2) return 0;
    i64 w, h, goal, cham, rp, rb;
    if (scanf("%lld %lld %lld %lld %lld %lld", &w, &h, &goal, &cham, &rp, &rb) != 6) return 0;
    CF.w = w; CF.h = h; CF.goal = goal; CF.chamfer = cham; CF.rp = rp; CF.rb = rb;
    i64 am, pn, pd, bn, bd;
    if (scanf("%lld %lld %lld %lld %lld", &am, &pn, &pd, &bn, &bd) != 5) return 0;
    CF.acc_max = am; CF.pf_n = pn; CF.pf_d = pd; CF.bf_n = bn; CF.bf_d = bd;
    i64 km, kr, kc;
    if (scanf("%lld %lld %lld", &km, &kr, &kc) != 3) return 0;
    CF.kick_max = km; CF.kick_reach = kr; CF.kick_cooldown = kc;
    i64 e1, e2, e3, e4, e5, e6;
    if (scanf("%lld %lld %lld %lld %lld %lld", &e1, &e2, &e3, &e4, &e5, &e6) != 6) return 0;
    CF.e_wall_n = e1; CF.e_wall_d = e2; CF.e_ball_n = e3; CF.e_ball_d = e4; CF.e_pp_n = e5; CF.e_pp_d = e6;
    i64 steps, stall, mmx;
    if (scanf("%lld %lld %lld", &steps, &stall, &mmx) != 3) return 0;
    CF.steps = steps; CF.stall = stall; CF.msg_max = mmx;
    i64 stepms, firstms;
    if (scanf("%lld %lld %lld", &MYIDX, &stepms, &firstms) != 3) return 0;

    bool prevAttacker = false;
    bool havePrev = false;
    i64 prevX = 0, prevY = 0;
    int resetT = -1000;
    int myLastKick = -1000;

    while (true) {
        i64 T, LEFT, MS, OS, MM;
        if (scanf("%lld %lld %lld %lld %lld", &T, &LEFT, &MS, &OS, &MM) != 5) break;
        i64 bx, by, bvx, bvy;
        if (scanf("%lld %lld %lld %lld", &bx, &by, &bvx, &bvy) != 4) break;
        P me, mate, o0, o1;
        if (scanf("%lld %lld %lld %lld %lld", &me.x, &me.y, &me.vx, &me.vy, &me.cd) != 5) break;
        if (scanf("%lld %lld %lld %lld %lld", &mate.x, &mate.y, &mate.vx, &mate.vy, &mate.cd) != 5) break;
        if (scanf("%lld %lld %lld %lld %lld", &o0.x, &o0.y, &o0.vx, &o0.vy, &o0.cd) != 5) break;
        if (scanf("%lld %lld %lld %lld %lld", &o1.x, &o1.y, &o1.vx, &o1.vy, &o1.cd) != 5) break;

        // ---- 开球检测（位置瞬间跳变） ----
        if (havePrev && (d2(me.x - prevX, me.y - prevY) > 360000)) resetT = (int)T;
        prevX = me.x;
        prevY = me.y;
        havePrev = true;

        // ---- 球路预测 ----
        int ballFate = predict_ball(CF, bx, by, bvx, bvy, 45, PATH);   // -1 = 会进我方球门
        int NS = (int)PATH.size();
        std::vector<i64> bxs(NS + 1), bys(NS + 1);
        bxs[0] = bx; bys[0] = by;
        for (int i = 0; i < NS; ++i) { bxs[i + 1] = PATH[i].x; bys[i + 1] = PATH[i].y; }
        int maxT = NS < ETA_CAP ? NS : ETA_CAP;

        int myEta = eta_ball(me, bxs, bys, maxT, 470);
        int mateEtaC = eta_ball(mate, bxs, bys, maxT, 470);   // 我直接算队友的 ETA，双方算出来一致
        int o0Eta = eta_ball(o0, bxs, bys, maxT, 470);
        int o1Eta = eta_ball(o1, bxs, bys, maxT, 470);
        int oppEta = o0Eta < o1Eta ? o0Eta : o1Eta;

        int mateRole = (int)((MM >> 5) & 3);
        bool mateUrgent = ((MM >> 7) & 1) != 0;
        int mateEta = (int)(MM & 31);
        if (mateRole == 3) mateEta = 0;   // 队友正在踢，视为他已经控球

        // ---- 危险判定 ----
        i64 myDB = ilen(bx - me.x, by - me.y);
        i64 mateDB = ilen(bx - mate.x, by - mate.y);
        i64 o0DB = ilen(bx - o0.x, by - o0.y);
        i64 o1DB = ilen(bx - o1.x, by - o1.y);
        i64 oppDB = o0DB < o1DB ? o0DB : o1DB;

        // ---- 分工仲裁 ----
        // 用"代价"比较：ETA 为主、离球距离为辅。两个人看到的物理量相同，
        // 所以算出来的先后顺序一致，不会出现两边都以为对方会去抢的死锁。
        int myE = myEta, mtE = mateEtaC;
        bool attacker;
        if (mateRole == 3) {
            attacker = false;                       // 队友正在踢，我不抢
        } else if (prevAttacker) {
            attacker = (myE <= mtE + 2);
        } else {
            attacker = (myE + 2 < mtE) || (myE <= mtE && MYIDX == 0);
            if (myE <= ETA_CAP && mtE > ETA_CAP) attacker = true;
        }
        // 兜底：队友明说了他在防守/接应，就别两个人一起看着球（避免互相谦让）
        if (!attacker && (mateRole == 1 || mateRole == 2)) {
            if (myE <= mtE || (me.cd == 0 && myDB <= REACH)) attacker = true;
        }
        if (KEEPER && !(me.cd == 0 && myDB <= REACH)) {
            // 门将型：明显更快才上前抢，球已经在脚下时照常处理
            attacker = (myE + 4 < mtE);
        }
        bool urgent = (bx < 3500) && (bvx < -20 || oppDB < (myDB < mateDB ? myDB : mateDB));
        if (mateUrgent) urgent = true;

        i64 ax = 0, ay = 0, kx = 0, ky = 0;
        bool iKick = false;
        bool canKick = (me.cd == 0) && (myDB <= REACH);

        // 阻挡者：两名对手 + 我自己（踢出的球不能撞回自己身上）
        P blk[3];
        blk[0] = o0;
        blk[1] = o1;
        blk[2] = me;

        // ---- 开球脚本 ----
        bool kickoffNow = ((int)T - resetT) < 3;
        if (kickoffNow && canKick && myDB <= 505 && bx > 5500 && bx < 6500) {
            // 开球：往对手够不到的空当推，力度适中，然后自己追（不送球给对方前锋）
            static const i64 dirs[15][2] = {{1000, 0},   {978, 208},  {914, 406},  {809, 588},  {669, 743},
                                           {500, 866},   {309, 951},  {105, 995},  {-105, 995}, {-309, 951},
                                           {-500, 866},  {-669, 743}, {-809, 588}, {-914, 406}, {-978, 208}};
            // 斜向大力踢到对手够不到的空当，然后自己追
            P ob[2] = {o0, o1};
            i64 bestSc = -(i64)1 << 50;
            for (int a = 0; a < 15; ++a) {
                i64 ux = dirs[a][0], uy = dirs[a][1];
                i64 tx2 = ux * 800 / 1000, ty2 = uy * 800 / 1000;
                predict_ball(CF, bx, by, tx2, ty2, 26, PATH2);
                int n2 = (int)PATH2.size();
                i64 cl = path_clearance(PATH2, ob, 2);
                i64 endx = PATH2[n2 - 1].x;
                int ct = 99;
                for (int k = 2; k <= n2 && k <= 20; k += 4) {
                    if (opp_can_catch(o0, PATH2[k - 1].x, PATH2[k - 1].y, k) ||
                        opp_can_catch(o1, PATH2[k - 1].x, PATH2[k - 1].y, k)) { ct = k; break; }
                }
                i64 sc = (cl > 2500 ? 2500 : cl) + (endx > 7500 ? 3000 : 0);
                if (ct < 90) sc -= (20 - ct) * 1200;
                if (sc > bestSc) { bestSc = sc; kx = tx2; ky = ty2; }
            }
            iKick = true;
            myLastKick = (int)T;
        }

        // 球已经在往对方球门滚、对手也来不及救：别碰它
        bool letRoll = (ballFate == 1) && (o0Eta > 6) && (o1Eta > 6);

        if (!iKick && canKick && !letRoll) {
            i64 sx, sy, cl;
            bool shoot = false;
            if (attacker) shoot = pick_shot(bx, by, blk, 3, sx, sy, cl);
            if (shoot) {
                kx = sx; ky = sy;
            } else if (attacker) {
                // 先看能否传给位置更好的队友
                bool pass = false;
                if (mate.x > me.x + 500 && mateDB > 250) {
                    i64 px, py;
                    pick_pass(bx, by, mate, blk, 2, px, py);
                    predict_ball(CF, bx, by, px, py, 20, PATH2);
                    i64 clp = path_clearance(PATH2, blk, 2);
                    if (clp > 430) { kx = px; ky = py; pass = true; }
                }
                if (!pass) {
                    i64 px, py;
                    pick_push(bx, by, blk, 3, 200, 5, px, py);
                    kx = px; ky = py;
                }
            } else {
                // 在本方半场拿到球：解围式大脚，优先向前且不送给对手
                i64 px, py;
                pick_push(bx, by, blk, 3, 700, 2, px, py);
                kx = px; ky = py;
            }
            if (kx != 0 || ky != 0) { iKick = true; myLastKick = (int)T; }
        }

        // 防止 300 步无人踢球重开：长时间没踢过就找机会碰一下球
        if (!iKick && canKick && (int)T - myLastKick > 200) {
            i64 px, py;
            pick_push(bx, by, blk, 3, 300, 3, px, py);
            kx = px; ky = py;
            iKick = true;
            myLastKick = (int)T;
        }

        // ---- 跑位 ----
        i64 tx = bx, ty = by;
        int role = attacker ? 0 : 1;

        if (attacker) {
            // 找球路上最早能赶到的点（真正的拦截点），而不是傻追当前球位
            int lim = maxT < 30 ? maxT : 30;
            int best = 99;
            i64 btx = bx, bty = by;
            for (int k = 0; k <= lim; ++k) {
                int e = eta_point(me, bxs[k], bys[k], 420, 30);
                if (e <= k) { best = k; btx = bxs[k]; bty = bys[k]; break; }
            }
            if (best < 99) {
                tx = btx;
                ty = bty;
            } else {
                int Tk = myEta;
                if (Tk > maxT) Tk = maxT;
                if (Tk < 0) Tk = 0;
                tx = bxs[Tk];
                ty = bys[Tk];
            }
            // 站到球的靠本方一侧，方便顺势往前踢
            if (tx > 1000) tx -= 180;
        } else {
            role = 1;   // 防守：拦截球路，或站在球最接近我方球门的那一点上挡射门
            int lim = maxT < 28 ? maxT : 28;
            int best = 99;
            i64 btx = 0, bty = 0;
            // 球正向我方球门飞、而我在球后面（追不上）→ 直接退到门线挡射门
            bool mustBlock = (ballFate == -1) || (bvx < -150 && myEta > 5);
            if (mustBlock && me.x > bx - 200) {
                int ks = 0;
                for (int k = 1; k <= lim; ++k)
                    if (bxs[k] < bxs[ks]) ks = k;
                i64 qx = bxs[ks];
                if (qx < 450) qx = 450;
                if (qx > 1100) qx = 1100;
                i64 qy = bys[ks];
                if (qy < 3050) qy = 3050;
                if (qy > 4950) qy = 4950;
                tx = qx;
                ty = qy;
            } else {
            for (int k = 0; k <= lim; ++k) {
                if (bxs[k] > 6200) continue;
                int e = eta_point(me, bxs[k], bys[k], 320, 28);
                if (e <= k) { best = k; btx = bxs[k]; bty = bys[k]; break; }
            }
            if (best < 99) {
                tx = btx;
                ty = bty;
            } else {
                int ks = 0;
                for (int k = 1; k <= lim; ++k)
                    if (bxs[k] < bxs[ks]) ks = k;
                i64 qx = bxs[ks], qy = bys[ks];
                if (bxs[ks] > 4800 && !KEEPER) {
                    // 球不会逼近我方球门：拖后清道夫，站在球到本方门心的连线上，别太靠前
                    role = 2;
                    qx = bx * 22 / 100;
                    if (qx < 1200) qx = 1200;
                    if (qx > 2200) qx = 2200;
                    i64 bb = bx > 500 ? bx : 500;
                    qy = by + (4000 - by) * (bb - qx) / bb;
                    if (qy < 3500) qy = 3500;
                    if (qy > 4500) qy = 4500;
                } else {
                    // 门将：贴着本方门线站，横向对准球路的过门点
                    if (qx < 700) qx = 700;
                    if (qx > (KEEPER ? 1500 : 1300)) qx = (KEEPER ? 1500 : 1300);
                    if (qy < 3050) qy = 3050;
                    if (qy > 4950) qy = 4950;
                }
                tx = qx;
                ty = qy;
            }
            if (KEEPER && myEta > 3) {
                // 门将型：不出击，始终守在门前 x=1050，横向跟随球路的过门点
                int kb = 0;
                i64 bd = (i64)1 << 60;
                for (int k = 0; k <= lim; ++k) {
                    i64 d = bxs[k] - 1050;
                    if (d < 0) d = -d;
                    if (d < bd) { bd = d; kb = k; }
                }
                tx = 1050;
                ty = bys[kb];
                if (ty < 3120) ty = 3120;
                if (ty > 4880) ty = 4880;
            }
            }
        }

        // 队友在踢球：让开"球 → 目标 y"的连线
        if (mateRole == 3) {
            i64 code = MM & 31;
            i64 mty = 3000 + code * 2000 / 30;
            // 球到 (12000, mty) 的线段，若我离得太近就横向让开
            i64 sx0 = bx, sy0 = by, sx1 = 12000, sy1 = mty;
            i64 vx = sx1 - sx0, vy = sy1 - sy0;
            i64 vv = vx * vx + vy * vy;
            i64 t = 0;
            if (vv > 0) t = ((me.x - sx0) * vx + (me.y - sy0) * vy) * 1000 / vv;
            if (t < 0) t = 0;
            if (t > 1000) t = 1000;
            i64 cx = sx0 + vx * t / 1000, cy = sy0 + vy * t / 1000;
            if (d2(me.x - cx, me.y - cy) < 800 * 800) {
                i64 off = (me.y <= cy) ? -900 : 900;
                ty = me.y + off;
                if (ty < 500) ty = 500;
                if (ty > 7500) ty = 7500;
            }
        }

        steer(me, tx, ty, ax, ay);

        // ---- 消息 ----
        int msg;
        if (iKick) {
            // 目标 y 档：按踢球方向外推到 x=12000 时的 y
            i64 esty = by;
            if (kx > 0) esty = by + (12000 - bx) * ky / kx;
            if (esty < 3000) esty = 3000;
            if (esty > 5000) esty = 5000;
            int code = (int)((esty - 3000) * 30 / 2000);
            if (code < 0) code = 0;
            if (code > 30) code = 30;
            msg = code | (3 << 5) | (urgent ? 128 : 0);
        } else {
            int e = myEta;
            if (e > 31) e = 31;
            msg = e | (role << 5) | (urgent ? 128 : 0);
        }
        (void)oppEta;

        printf("%lld %lld %lld %lld %d\n", ax, ay, kx, ky, msg);
        fflush(stdout);
        prevAttacker = attacker;
    }
    return 0;
}
