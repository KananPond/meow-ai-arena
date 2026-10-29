// 2v2 球员。视角永远是自己向右进攻。
// 消息：bit0-4=ETA，bit5-6=角色(0主攻 1回防 2接应)，bit7=紧急。
// 队友若发角色 3，当作他正在踢球。
#include "soccer.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

using soccer::i64;

static soccer::Config CF;
static soccer::Wall WALLS[8];
static int NW = 0;

struct Body {
    i64 x, y, vx, vy, cd;
};

static i64 clampi(i64 v, i64 lo, i64 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static i64 d2(i64 x1, i64 y1, i64 x2, i64 y2) {
    i64 dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

static void scale_to(i64& x, i64& y, i64 L) {
    i64 len = soccer::isqrt(x * x + y * y);
    if (len <= 0) {
        x = L;
        y = 0;
        return;
    }
    x = x * L / len;
    y = y * L / len;
}

static void clamp_field(i64& x, i64& y) {
    x = clampi(x, 400, CF.w - 400);
    y = clampi(y, 400, CF.h - 400);
    const i64 m = 1500;
    if (x + y < m) {
        i64 s = m - x - y;
        x += s / 2;
        y += s - s / 2;
    }
    if ((CF.w - x) + y < m) {
        i64 s = m - (CF.w - x) - y;
        x -= s / 2;
        y += s - s / 2;
    }
    if (x + (CF.h - y) < m) {
        i64 s = m - x - (CF.h - y);
        x += s / 2;
        y -= s - s / 2;
    }
    if ((CF.w - x) + (CF.h - y) < m) {
        i64 s = m - (CF.w - x) - (CF.h - y);
        x -= s / 2;
        y -= s - s / 2;
    }
}

static void step_body(Body& b, i64 tx, i64 ty) {
    i64 ax = tx - b.x, ay = ty - b.y;
    soccer::clamp_len(ax, ay, CF.acc_max);
    b.vx += ax;
    b.vy += ay;
    b.x += b.vx;
    b.y += b.vy;
    soccer::wall_collide(WALLS, NW, CF.gy0(), CF.gy1(), false, CF.rp, 0, 1, b.x, b.y, b.vx, b.vy);
    b.vx = b.vx * CF.pf_n / CF.pf_d;
    b.vy = b.vy * CF.pf_n / CF.pf_d;
}

static bool reach_in(Body b, i64 tx, i64 ty, int steps, i64 rad) {
    if (steps <= 0) return d2(b.x, b.y, tx, ty) <= rad * rad;
    i64 maxd = steps * 430LL + 250;
    if (d2(b.x, b.y, tx, ty) > maxd * maxd) return false;
    for (int s = 0; s < steps; ++s) step_body(b, tx, ty);
    return d2(b.x, b.y, tx, ty) <= rad * rad;
}

static soccer::BallPt BP[96];
static int BN = 0;
static int BG = 0;

static void load_ball(i64 bx, i64 by, i64 bvx, i64 bvy, std::vector<soccer::BallPt>& tmp) {
    BG = soccer::predict_ball(CF, bx, by, bvx, bvy, 80, tmp);
    BP[0] = {bx, by, bvx, bvy};
    int n = (int)tmp.size();
    if (n > 80) n = 80;
    for (int i = 0; i < n; ++i) BP[i + 1] = tmp[i];
    BN = n + 1;
    if (BG == 0) {
        while (BN < 48) {
            soccer::BallPt last = BP[BN - 1];
            last.vx = last.vy = 0;
            BP[BN++] = last;
        }
    }
}

static int eta_kick(const Body& src, int horizon) {
    const i64 R2 = CF.kick_reach * CF.kick_reach;
    if (src.cd <= 0 && d2(src.x, src.y, BP[0].x, BP[0].y) <= R2) return 0;
    int lim = std::min(BN - 1, horizon);
    for (int t = (int)std::max(src.cd, (i64)1); t <= lim; ++t) {
        if (reach_in(src, BP[t].x, BP[t].y, t, CF.kick_reach)) return t;
    }
    return 31;
}

// 对手沿当前速度滑行时，和球路的最近距离。
static i64 min_sep(const std::vector<soccer::BallPt>& path, const Body& o, int lim) {
    i64 best = 1LL << 62;
    i64 ox = o.x, oy = o.y, ovx = o.vx, ovy = o.vy;
    int m = std::min((int)path.size(), lim);
    for (int k = 0; k < m; ++k) {
        i64 dd = d2(ox, oy, path[k].x, path[k].y);
        if (dd < best) best = dd;
        ox += ovx;
        oy += ovy;
        ovx = ovx * 9 / 10;
        ovy = ovy * 9 / 10;
    }
    return soccer::isqrt(best);
}

static int g_idx = 0;
static int last_role = -1;

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string magic;
    i64 ver, W, H, GOAL, CHAMFER, RP, RB;
    i64 ACC_MAX, PF_N, PF_D, BF_N, BF_D;
    i64 KICK_MAX, KICK_REACH, KICK_COOLDOWN;
    i64 ew_n, ew_d, eb_n, eb_d, ep_n, ep_d;
    i64 STEPS, STALL, MSG_MAX, IDX, STEP_MS, FIRST_MS;
    if (!(std::cin >> magic >> ver)) return 0;
    std::cin >> W >> H >> GOAL >> CHAMFER >> RP >> RB;
    std::cin >> ACC_MAX >> PF_N >> PF_D >> BF_N >> BF_D;
    std::cin >> KICK_MAX >> KICK_REACH >> KICK_COOLDOWN;
    std::cin >> ew_n >> ew_d >> eb_n >> eb_d >> ep_n >> ep_d;
    std::cin >> STEPS >> STALL >> MSG_MAX;
    std::cin >> IDX >> STEP_MS >> FIRST_MS;
    CF.w = W;
    CF.h = H;
    CF.goal = GOAL;
    CF.chamfer = CHAMFER;
    CF.rp = RP;
    CF.rb = RB;
    CF.acc_max = ACC_MAX;
    CF.pf_n = PF_N;
    CF.pf_d = PF_D;
    CF.bf_n = BF_N;
    CF.bf_d = BF_D;
    CF.kick_max = KICK_MAX;
    CF.kick_reach = KICK_REACH;
    CF.kick_cooldown = KICK_COOLDOWN;
    CF.e_wall_n = ew_n;
    CF.e_wall_d = ew_d;
    CF.e_ball_n = eb_n;
    CF.e_ball_d = eb_d;
    CF.e_pp_n = ep_n;
    CF.e_pp_d = ep_d;
    CF.steps = STEPS;
    CF.stall = STALL;
    CF.msg_max = MSG_MAX;
    NW = soccer::make_walls(CF, WALLS);
    g_idx = (int)IDX;

    std::vector<soccer::BallPt> tmp, path;
    tmp.reserve(128);
    path.reserve(128);

    // 16 个方向，长度 1000 的整数近似。
    const i64 DX[16] = {1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383, 0, 383, 707, 924};
    const i64 DY[16] = {0, 383, 707, 924, 1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383};

    i64 T, LEFT, MY, OPP, MATE_MSG;
    i64 bx, by, bvx, bvy;
    while (std::cin >> T >> LEFT >> MY >> OPP >> MATE_MSG >> bx >> by >> bvx >> bvy) {
        Body pl[4];
        for (int i = 0; i < 4; ++i) std::cin >> pl[i].x >> pl[i].y >> pl[i].vx >> pl[i].vy >> pl[i].cd;
        const Body& me = pl[0];
        const Body& mate = pl[1];
        const Body& oA = pl[2];
        const Body& oB = pl[3];

        load_ball(bx, by, bvx, bvy, tmp);

        const bool kickoff = (bvx == 0 && bvy == 0 && me.vx == 0 && me.vy == 0 &&
                               std::llabs(bx - W / 2) < 800 && std::llabs(by - H / 2) < 800);
        if (kickoff) last_role = -1;

        int e_me = eta_kick(me, 32);
        int e_mate = eta_kick(mate, 32);
        int mate_eta_use = e_mate;
        int mate_role = 1;
        bool mate_claim = false;
        if (T > 0 && !(MATE_MSG == 0 && e_mate > 4)) {
            int cl = (int)(MATE_MSG & 31);
            int cr = (int)((MATE_MSG >> 5) & 3);
            if (cr == 3) {
                mate_claim = true;
                mate_eta_use = 0;
                mate_role = 0;
            } else {
                mate_role = cr;
                if (cr == 0) mate_claim = true;
                if (cl <= e_mate + 8) mate_eta_use = cl;
            }
        }

        bool primary = e_me < mate_eta_use || (e_me == mate_eta_use && g_idx == 0 && !mate_claim);
        if (last_role == 0 && e_me <= mate_eta_use + 2) primary = true;
        if (last_role != 0 && mate_claim && mate_eta_use <= e_me && e_me <= mate_eta_use + 2) primary = false;
        if (e_me + 4 < mate_eta_use) primary = true;
        if (mate_eta_use + 4 < e_me && last_role != 0) primary = false;
        // 现在就够得着的人必须去踢，不能干看着。
        if (e_me == 0 && e_mate > 0) primary = true;
        if (e_mate == 0 && e_me > 0) primary = false;
        // 队友明确在守（角色 1/2）时由我主攻，避免两个人都不碰球。
        if ((mate_role == 1 || mate_role == 2) && e_mate > 0 && e_me > 0) primary = true;
        // 球在对方半场、两边都够不着时，离自家球门更近的人必须回家，不能两个人一起追。
        if (bx > 5200 && e_me > 8 && e_mate > 8) {
            if (me.x + 200 < mate.x) primary = false;
            else if (mate.x + 200 < me.x) primary = true;
            else primary = (g_idx == 0 && !mate_claim);
        }

        const i64 reach2 = CF.kick_reach * CF.kick_reach;
        bool in_range = me.cd <= 0 && d2(me.x, me.y, bx, by) <= reach2;

        bool let_roll = false;
        if (BG == 1) {
            int G = std::max(BN - 1, 1);
            bool saved = reach_in(oA, BP[std::min(G, BN - 1)].x, BP[std::min(G, BN - 1)].y, G, 500) ||
                         reach_in(oB, BP[std::min(G, BN - 1)].x, BP[std::min(G, BN - 1)].y, G, 500);
            if (!saved && e_me > 2 && e_mate > 2) let_roll = true;
        }

        i64 kx = 0, ky = 0, aimx = 0, aimy = 0;
        bool shot = false;
        bool have_aim = false;

        // 队友刚把球向前踢开，不要马上用一脚软触把球停住。
        bool mate_launched = mate.cd >= 5 && bvx > 340 && BG != -1;
        if (in_range && !let_roll && !mate_launched && (primary || e_me == 0)) {
            i64 ox = 0, oy = 0;
            bool contest = false;
            const Body* opps[2] = {&oA, &oB};
            for (int i = 0; i < 2; ++i) {
                if (opps[i]->cd <= 0 && d2(opps[i]->x, opps[i]->y, bx, by) <= reach2) {
                    i64 dx = -bx, dy = H / 2 - by;
                    scale_to(dx, dy, CF.kick_max);
                    ox += dx;
                    oy += dy;
                    contest = true;
                }
            }

            long long best = -9000000000000000000LL;
            i64 bkx = 0, bky = 0, bax = bx + 400, bay = by;
            bool bshot = false;

            auto consider = [&](i64 vx, i64 vy) {
                i64 mine_x = vx, mine_y = vy;
                if (contest) {
                    vx += ox;
                    vy += oy;
                }
                soccer::clamp_len(vx, vy, CF.kick_max);
                if (vx == 0 && vy == 0) return;
                int res = soccer::predict_ball(CF, bx, by, vx, vy, 70, path);
                if (path.empty()) return;
                int G = (int)path.size();
                i64 sepA = min_sep(path, oA, std::min(G, 18));
                i64 sepB = min_sep(path, oB, std::min(G, 18));
                i64 sep = std::min(sepA, sepB);
                i64 sepMe = min_sep(path, me, std::min(G, 4));
                i64 endx = path[std::min(G, 14) - 1].x;
                int catch_t = 99;
                int limt = std::min(G, 21);
                for (int ct = 3; ct <= limt; ct += 3) {
                    if (reach_in(oA, path[ct - 1].x, path[ct - 1].y, ct, 500) ||
                        reach_in(oB, path[ct - 1].x, path[ct - 1].y, ct, 500)) {
                        catch_t = ct;
                        break;
                    }
                }
                int my_t = 99;
                for (int ct = 6; ct <= limt; ct += 3) {
                    if (reach_in(me, path[ct - 1].x, path[ct - 1].y, ct, 500)) {
                        my_t = ct;
                        break;
                    }
                }
                long long sc;
                bool is_shot = false;
                if (res == -1) {
                    sc = -8000000000000LL;
                } else if (res == 1 && catch_t >= G && sep >= 280 && sepMe >= 280) {
                    sc = 6000000000000LL - (long long)G * 2000000LL + sep * 1000000LL;
                    is_shot = true;
                } else {
                    sc = (long long)sep * 4000000LL + endx * 1800000LL + vx * 5000000LL;
                    if (sep < 220) sc -= 1500000000000LL;
                    if (sepMe < 280) sc -= 500000000000LL;
                    if (vx < 100) sc -= 3000000000000LL;
                    if (my_t < catch_t && vx > 140) sc += 2800000000000LL;
                    else if (catch_t < 90) sc -= 700000000000LL;
                }
                if (sc > best) {
                    best = sc;
                    // 输出的是自己这一脚，不含对手的对脚。
                    bkx = mine_x;
                    bky = mine_y;
                    bshot = is_shot;
                    int at = is_shot ? std::min(3, G) : std::min(8, G);
                    bax = path[at - 1].x;
                    bay = path[at - 1].y;
                }
            };

            for (int i = 0; i < 7; ++i) {
                i64 gy = CF.gy0() + 80 + (CF.gy1() - CF.gy0() - 160) * i / 6;
                i64 vx = W + 150 - bx, vy = gy - by;
                scale_to(vx, vy, CF.kick_max);
                consider(vx, vy);
            }
            const i64 pows[4] = {340, 480, 680, 800};
            int np = 4;
            int nd = 16;
            for (int p = 0; p < np; ++p) {
                for (int a = 0; a < nd; ++a) {
                    i64 vx = DX[a], vy = DY[a];
                    scale_to(vx, vy, pows[p]);
                    consider(vx, vy);
                }
            }
            if (best > -7000000000000LL) {
                kx = bkx;
                ky = bky;
                shot = bshot;
                aimx = bax;
                aimy = bay;
                have_aim = true;
            }
        }

        int cross_t = 99;
        i64 cross_y = H / 2;
        for (int t = 0; t < BN; ++t) {
            if (BP[t].x <= 1500 && BP[t].y >= 2700 && BP[t].y <= 5300) {
                cross_t = t;
                cross_y = BP[t].y;
                break;
            }
        }
        // 球已经会进门，而我赶不上这脚：先去封线，别从身后把球推进自家门。
        bool must_block = cross_t < 36 && e_me > cross_t;

        i64 tx, ty;
        if (let_roll) {
            i64 sign = (me.y < by) ? -1 : 1;
            tx = me.x;
            ty = by + sign * 800;
        } else if ((kx || ky) && shot) {
            i64 sign = (me.y <= by) ? -1 : 1;
            tx = me.x;
            ty = me.y + sign * 650;
        } else if ((kx || ky) && have_aim) {
            tx = aimx;
            ty = aimy;
        } else if (must_block || (!primary && (BG == -1 || bvx < -150 || cross_t < 28))) {
            tx = 1000;
            ty = cross_t < 90 ? cross_y : H / 2;
            if (cross_t >= 90) {
                i64 bestp = 1LL << 62;
                for (int t = 0; t < BN; ++t) {
                    i64 pen = std::llabs(BP[t].x - 1000);
                    if (pen < bestp) {
                        bestp = pen;
                        ty = BP[t].y;
                        if (BP[t].x < 200) break;
                    }
                }
            }
            if (e_mate > cross_t) ty += (g_idx == 0 ? -300 : 300);
            ty = clampi(ty, 3080, 4920);
        } else if (primary) {
            int t = e_me;
            if (t >= 31) t = std::min(14, BN - 1);
            t = (int)clampi(t, 0, BN - 1);
            tx = BP[t].x;
            ty = BP[t].y;
            if (BG != -1) tx -= 150;
        } else {
            // 球在前方时守在球门与球的连线上，深度靠后，保证射门时回得来。
            tx = clampi(bx * 28 / 100, 1200, 2600);
            if (bvx < -80) ty = by + bvy * (bx - tx) / (-bvx);
            else if (bx > tx + 40) ty = by + (H / 2 - by) * (bx - tx) / bx;
            else ty = H / 2;
            ty = clampi(ty, 3100, 4900);
        }
        clamp_field(tx, ty);

        // 我是 1 号且队友在主攻：钉在门前。球到脚下（ETA<=3）才离开。
        bool hold_post = g_idx == 1 && e_me > 3 && mate_role == 0 && kx == 0 && ky == 0;
        if (hold_post) {
            tx = 1050;
            i64 gy = H / 2;
            i64 bestp = 1LL << 62;
            for (int t = 0; t < BN; ++t) {
                if (BP[t].x > 4500) continue;
                i64 pen = std::llabs(BP[t].x - 1050);
                if (pen < bestp) {
                    bestp = pen;
                    gy = BP[t].y;
                }
                if (BP[t].x <= 150) break;
            }
            if (bvx < -60 && bx > 1050 && bestp > 500) gy = by + bvy * (bx - 1050) / (-bvx);
            if (bx < 2200 && by > 2600 && by < 5400) gy = by;
            i64 ylo = 3120, yhi = 4880;
            if (bx < 4200 && (by < 2900 || by > 5100)) {
                ylo = 2780;
                yhi = 5220;
            }
            ty = clampi(gy, ylo, yhi);
            clamp_field(tx, ty);
            primary = false;
        }

        i64 dx = tx - me.x, dy = ty - me.y;
        i64 ax, ay;
        if (hold_post) {
            i64 dist = soccer::isqrt(dx * dx + dy * dy);
            i64 cap = (dist < 140) ? 0 : soccer::isqrt(dist * 48);
            if (cap > 351) cap = 351;
            i64 wx = 0, wy = 0;
            if (cap > 0 && dist > 0) {
                wx = dx * cap / dist;
                wy = dy * cap / dist;
            }
            ax = wx - me.vx;
            ay = wy - me.vy;
        } else {
            ax = dx;
            ay = dy;
        }
        soccer::clamp_len(ax, ay, CF.acc_max);

        bool urgent = BG == -1 || (bx < 3600 && bvx < -20);
        int role = primary ? 0 : 1;
        int low = std::min(e_me, 31);
        // 只有开球位主攻的前向出球才报角色 3。门将出球仍报普通角色，避免队友把 ETA 当成 0 而让开。
        if (g_idx == 0 && primary && kx > 0) {
            i64 esty = by + (12000 - bx) * ky / kx;
            esty = clampi(esty, 3000, 5000);
            low = (int)((esty - 3000) * 30 / 2000);
            if (low < 0) low = 0;
            if (low > 30) low = 30;
            role = 3;
        }
        int msg = ((urgent ? 1 : 0) << 7) | (role << 5) | low;
        last_role = primary ? 0 : 1;
        (void)mate_role;

        std::cout << ax << ' ' << ay << ' ' << kx << ' ' << ky << ' ' << msg << std::endl;
    }
    return 0;
}
