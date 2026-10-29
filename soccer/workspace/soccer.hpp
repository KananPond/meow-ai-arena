// soccer.hpp —— 2v2 简化足球 规则引擎（规则 v1 的唯一权威实现）
//
// C++17，只依赖标准库，单个头文件。裁判程序 arena.exe 和选手程序都可以直接 #include 它：
//   选手可以用它在自己的程序里做本地模拟（例如 soccer::predict_ball 预测球的轨迹，
//   或者复制一个 soccer::Game 往前推几步），结果与裁判逐位一致。
//
// 与 Python 原型（原型/engine.py）逐步逐位一致，对拍见 引擎/对拍/。
//
// 坐标：x 向右、y 向下，单位"毫"，原点在左上角。场地坐标里 A 队（0、1 号）向右进攻，B 队（2、3 号）向左进攻。
// 选手看到的永远是"自己向右进攻"的视角（Game::observe），输出也按这个视角给（Game::step 会换算）。
//
// 整数约定：
//   tdiv(a, b)  b > 0，向零取整（就是 C/C++ 的 a / b）。
//   isqrt(n)    向下取整的平方根（精确）。
//   clamp_len(x, y, m)：若 x²+y² > m²，d = isqrt(x²+y²)，(x, y) ← (tdiv(x·m, d), tdiv(y·m, d))。
//   全部中间量用 64 位有符号整数，不会溢出（动作里每个数的绝对值不超过 ACTION_ABS_MAX = 10^9）。
//
// 每步结算顺序（Game::step）：
//   1. 动作换算回场地坐标；msg 不在 0..msg_max 内按 0。
//   2. 踢球（用本步开始时的位置判断距离）：踢球向量非零、冷却为 0、圆心距 <= kick_reach 时有效。
//      每个有效踢球向量先限长到 kick_max，全部相加，再限长到 kick_max，直接成为球的新速度。
//      踢的人冷却 = kick_cooldown，且本步末尾不减（所以踢球后接下来 kick_cooldown 步都不能踢）。
//   3. 加速：(ax, ay) 限长到 acc_max，加到速度上。
//   4. 移动：球员和球的位置加上速度。
//   5. 碰撞：球员与球员（同时结算）→ 球员与墙 → 球与球员（同时结算）→ 进球判定 → 球与墙（没进球时）。
//      两名球员圆心完全重合时的分离方向（与编号无关，左右镜像对称）：
//        a) 按两人本步移动前的相对位置分开（= 相对速度的反方向）；
//        b) 若移动前也重合（相对速度为 0）：不同队 → 沿 x 方向各自退向本方球门；同队 → 队内 0 号向上、1 号向下。
//   6. 摩擦：球员速度 ×pf_n/pf_d，球速度 ×bf_n/bf_d（向零取整）。冷却减 1（本步刚踢球的人不减）。
//   7. 步数 +1。进球 → 比分 +1，失球方开球；否则连续 stall 步无有效踢球 → 原开球方重新开球。到 steps 步结束。
//
// 注意：本文件刻意不用 min / max / near / far / small / OUT / IN 等会和 <windows.h> 宏冲突的名字，
// 先包含 <windows.h> 再包含本文件也能编译。

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace soccer {

using i64 = long long;
using u64 = unsigned long long;

constexpr const char* ENGINE_VERSION = "soccer2v2-v1";
constexpr i64 ACTION_ABS_MAX = 1000000000LL;  // 动作里每个数的绝对值上限；超出则整步动作不合法（按 0 0 0 0 0）

// ------------------------------------------------------------------ 整数工具

inline i64 tdiv(i64 a, i64 b) { return a / b; }  // b > 0；C++11 起整数除法向零取整

inline i64 isqrt(i64 n) {
    if (n <= 0) return 0;
    i64 r = 0;
    // 位法求平方根，精确向下取整
    u64 x = u64(n), bit = u64(1) << 62;
    u64 res = 0;
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    r = i64(res);
    return r;
}

inline void clamp_len(i64& x, i64& y, i64 m) {
    const i64 s = x * x + y * y;
    if (s > m * m) {
        const i64 d = isqrt(s);
        x = tdiv(x * m, d);
        y = tdiv(y * m, d);
    }
}

// 开球扰动用的随机数（规则写死）
struct SplitMix64 {
    u64 s = 0;
    explicit SplitMix64(u64 seed = 0) : s(seed) {}
    u64 next() {
        s += 0x9E3779B97F4A7C15ULL;
        u64 z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    i64 jitter(i64 j) { return i64(next() % u64(2 * j + 1)) - j; }  // [-j, j]
};

// ------------------------------------------------------------------ 参数

enum KickoffMode { KICKOFF_CONCEDER = 0, KICKOFF_SYM = 1 };

struct Config {
    i64 w = 12000, h = 8000;
    i64 goal = 2000;          // 球门宽（左右两侧墙正中开口）
    i64 chamfer = 1000;       // 四角切角（45° 斜墙，直角边长）；0 = 直角
    i64 rp = 300, rb = 150;   // 球员半径、球半径
    i64 acc_max = 40;         // 每步加速度上限
    i64 pf_n = 9, pf_d = 10;  // 球员摩擦
    i64 bf_n = 97, bf_d = 100;  // 球摩擦
    i64 kick_max = 800;       // 踢球速度上限
    i64 kick_reach = 500;     // 可踢距离（圆心距）
    i64 stall = 300;          // 连续这么多步无有效踢球 → 重新开球；0 = 关闭
    i64 kick_cooldown = 5;    // 踢球后冷却
    i64 e_wall_n = 4, e_wall_d = 5;  // 球撞墙恢复系数
    i64 e_ball_n = 1, e_ball_d = 2;  // 球撞球员恢复系数（球员视为质量无穷大）
    i64 e_pp_n = 1, e_pp_d = 2;      // 球员互撞恢复系数（等质量）
    i64 steps = 1200;         // 一场的步数
    int kickoff = KICKOFF_CONCEDER;  // 失球方开球（第一次由 first_kick 队开）；KICKOFF_SYM 为对称站位（只用于实验）
    i64 jitter = 600;         // 开球站位扰动幅度
    i64 ko_front = 1500;      // 非开球方前锋离中线的距离
    i64 ko_back = 2000;       // 后卫离本方底线的距离
    i64 msg_max = 255;        // 队友消息取值 0..msg_max；-1 = 关闭通信

    i64 gy0() const { return (h - goal) / 2; }
    i64 gy1() const { return (h - goal) / 2 + goal; }

    // 按原型的参数名设置（"W"、"KICK_MAX"、"KICKOFF" 等）；不认识的名字或值返回 false
    bool set(const std::string& key, const std::string& val) {
        if (key == "KICKOFF") {
            if (val == "conceder") kickoff = KICKOFF_CONCEDER;
            else if (val == "sym") kickoff = KICKOFF_SYM;
            else return false;
            return true;
        }
        if (key == "MSG_MAX" && val == "None") {
            msg_max = -1;
            return true;
        }
        char* end = nullptr;
        const long long v = std::strtoll(val.c_str(), &end, 10);
        if (val.empty() || !end || *end) return false;
        struct Item { const char* name; i64 Config::*field; };
        static const Item items[] = {
            {"W", &Config::w}, {"H", &Config::h}, {"GOAL", &Config::goal}, {"CHAMFER", &Config::chamfer},
            {"RP", &Config::rp}, {"RB", &Config::rb}, {"ACC_MAX", &Config::acc_max}, {"PF_N", &Config::pf_n},
            {"PF_D", &Config::pf_d}, {"BF_N", &Config::bf_n}, {"BF_D", &Config::bf_d}, {"KICK_MAX", &Config::kick_max},
            {"KICK_REACH", &Config::kick_reach}, {"STALL", &Config::stall}, {"KICK_COOLDOWN", &Config::kick_cooldown},
            {"E_WALL_N", &Config::e_wall_n}, {"E_WALL_D", &Config::e_wall_d}, {"E_BALL_N", &Config::e_ball_n},
            {"E_BALL_D", &Config::e_ball_d}, {"E_PP_N", &Config::e_pp_n}, {"E_PP_D", &Config::e_pp_d},
            {"STEPS", &Config::steps}, {"JITTER", &Config::jitter}, {"KO_FRONT", &Config::ko_front},
            {"KO_BACK", &Config::ko_back}, {"MSG_MAX", &Config::msg_max},
        };
        for (const Item& it : items)
            if (key == it.name) {
                this->*(it.field) = v;
                return true;
            }
        return false;
    }

    // 与原型回放文件的 CFG 行同样的写法（参数名与顺序都相同）
    std::string cfg_line() const {
        std::string o;
        auto add = [&](const char* k, const std::string& v) {
            if (!o.empty()) o += ' ';
            o += k;
            o += '=';
            o += v;
        };
        auto n = [](i64 v) { return std::to_string(v); };
        add("W", n(w)); add("H", n(h)); add("GOAL", n(goal)); add("CHAMFER", n(chamfer)); add("RP", n(rp)); add("RB", n(rb));
        add("ACC_MAX", n(acc_max)); add("PF_N", n(pf_n)); add("PF_D", n(pf_d)); add("BF_N", n(bf_n)); add("BF_D", n(bf_d));
        add("KICK_MAX", n(kick_max)); add("KICK_REACH", n(kick_reach)); add("STALL", n(stall));
        add("KICK_COOLDOWN", n(kick_cooldown)); add("E_WALL_N", n(e_wall_n)); add("E_WALL_D", n(e_wall_d));
        add("E_BALL_N", n(e_ball_n)); add("E_BALL_D", n(e_ball_d)); add("E_PP_N", n(e_pp_n)); add("E_PP_D", n(e_pp_d));
        add("STEPS", n(steps)); add("KICKOFF", kickoff == KICKOFF_SYM ? "sym" : "conceder"); add("JITTER", n(jitter));
        add("KO_FRONT", n(ko_front)); add("KO_BACK", n(ko_back)); add("MSG_MAX", msg_max < 0 ? std::string("None") : n(msg_max));
        return o;
    }
};

// ------------------------------------------------------------------ 墙

// 墙：a·x + b·y >= c 为场内。前两面（左、右）在球门开口处对球不存在。
struct Wall { i64 a, b, c; };

inline int make_walls(const Config& cf, Wall out[8]) {
    const i64 W = cf.w, H = cf.h, C = cf.chamfer;
    int n = 0;
    out[n++] = {1, 0, 0};
    out[n++] = {-1, 0, -W};
    out[n++] = {0, 1, 0};
    out[n++] = {0, -1, -H};
    if (C > 0) {
        out[n++] = {1, 1, C};
        out[n++] = {-1, 1, C - W};
        out[n++] = {1, -1, C - H};
        out[n++] = {-1, -1, C - W - H};
    }
    return n;
}

constexpr const char* WALL_NAME[8] = {"左墙", "右墙", "上墙", "下墙", "左上切角", "右上切角", "左下切角", "右下切角"};

// 圆（半径 r）与墙的碰撞。is_ball = true 时球门开口处的左右墙不算，恢复系数 en/ed；
// 返回值：按位记录本次真正反弹（法向速度朝墙）的墙编号。
inline unsigned wall_collide(const Wall* walls, int nw, i64 gy0, i64 gy1, bool is_ball, i64 r, i64 en, i64 ed, i64& x,
                             i64& y, i64& vx, i64& vy) {
    const i64 rd = isqrt(2 * r * r);
    unsigned hit = 0;
    for (int i = 0; i < nw; ++i) {
        const Wall& wl = walls[i];
        if (is_ball && i < 2 && gy0 <= y && y <= gy1) continue;  // 球门开口
        const i64 n2 = wl.a * wl.a + wl.b * wl.b;
        const i64 need = n2 == 1 ? r : rd;
        const i64 pen = need - (wl.a * x + wl.b * y - wl.c);
        if (pen <= 0) continue;
        if (n2 == 1) {
            x += wl.a * pen;
            y += wl.b * pen;
        } else {
            const i64 s = (pen + 1) / 2;  // pen > 0，与向下取整相同
            x += wl.a * s;
            y += wl.b * s;
        }
        const i64 dot = wl.a * vx + wl.b * vy;
        if (dot < 0) {
            vx -= tdiv((ed + en) * dot * wl.a, ed * n2);
            vy -= tdiv((ed + en) * dot * wl.b, ed * n2);
            hit |= 1u << i;
        }
    }
    return hit;
}

// ------------------------------------------------------------------ 球轨迹预测

struct BallPt { i64 x, y, vx, vy; };

// 不考虑球员时，球在后续 n 步的轨迹（与引擎逐位一致，只要球没碰到人、没人踢）。
// out[k] 是 k+1 步后（移动、撞墙、摩擦之后）的位置和速度。
// 返回 +1 = 途中进右门，-1 = 进左门，0 = 没进；进球时 out 的最后一项是越线那一刻（速度记 0）。
// 球停住（速度为 0）后提前结束。这是场地坐标；在选手视角下同样适用（视角翻转不改变物理）。
inline int predict_ball(const Config& cf, i64 bx, i64 by, i64 bvx, i64 bvy, int n, std::vector<BallPt>& out) {
    out.clear();
    Wall walls[8];
    const int nw = make_walls(cf, walls);
    const i64 gy0 = cf.gy0(), gy1 = cf.gy1();
    i64 x = bx, y = by, vx = bvx, vy = bvy;
    for (int k = 0; k < n; ++k) {
        x += vx;
        y += vy;
        if (gy0 <= y && y <= gy1 && (x < 0 || x > cf.w)) {
            out.push_back({x, y, 0, 0});
            return x > cf.w ? 1 : -1;
        }
        wall_collide(walls, nw, gy0, gy1, true, cf.rb, cf.e_wall_n, cf.e_wall_d, x, y, vx, vy);
        vx = tdiv(vx * cf.bf_n, cf.bf_d);
        vy = tdiv(vy * cf.bf_n, cf.bf_d);
        out.push_back({x, y, vx, vy});
        if (vx == 0 && vy == 0) break;
    }
    return 0;
}

// ------------------------------------------------------------------ 动作、观测、事件

struct Action {
    i64 ax = 0, ay = 0, kx = 0, ky = 0, msg = 0;
    bool ok = true;  // false = 不合法 / 超时 / 崩溃，按 0 0 0 0 0 处理
};

struct PlayerView { i64 x, y, vx, vy, cd; };

// 选手看到的局面（"自己向右进攻"视角）
struct Obs {
    i64 t = 0, left = 0, my_score = 0, opp_score = 0, mate_msg = 0;
    i64 bx = 0, by = 0, bvx = 0, bvy = 0;
    PlayerView pl[4] = {};  // 0 自己、1 队友、2 对手（编号小者）、3 对手（编号大者）
    int idx = 0;            // 我在队里是 0 号还是 1 号
};

enum EventType {
    EV_KICKOFF,  // a = 开球队（-1 = 对称开球），b, c = 球的位置
    EV_KICK,     // a = 球员，b, c = 限长后的踢球向量（场地坐标），d, e = 踢后球速，f = 本步有效踢球人数
    EV_NOKICK,   // a = 球员，b = 原因（0 冷却中，1 够不着），c = 剩余冷却 / 圆心距
    EV_TOUCH,    // a = 球员（球与他重叠，球被推出），b = 1 表示同时有反弹冲量（两者正在相互靠近），0 表示只修正位置
    EV_BUMP,     // a, b = 相撞的两名球员
    EV_WALL,     // a = 墙编号（WALL_NAME）
    EV_GOAL,     // a = 得分队，b = 最后触球者（最后一次触球只有一人时；否则 -1），c, d = 进球后比分，
                 // e = 最后一次触球的全部球员（位掩码，1 << 球员号；同一步同一阶段同时触球的人都算；0 = 开球后没人碰过）
    EV_STALL,    // a = 久无踢球的步数阈值
};
constexpr const char* EVENT_NAME[8] = {"kickoff", "kick", "nokick", "touch", "bump", "wall", "goal", "stall"};

struct Event {
    int type;
    i64 t;  // 发生在第几步（该步开始时的步号）；开球、久无踢球重开记为新的步号
    i64 a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
};

// ------------------------------------------------------------------ 对局

class Game {
public:
    Config cfg;
    bool log = false;              // true 时记录 events（不影响结算）
    std::vector<Event> events;
    Wall walls[8];
    int nwalls = 0;
    i64 gy0 = 0, gy1 = 0;
    SplitMix64 rng;
    u64 seed = 0;
    i64 score[2] = {0, 0};
    i64 t = 0;
    bool over = false;
    i64 msg[4] = {0, 0, 0, 0};     // 上一步各人发出的消息
    i64 cd[4] = {0, 0, 0, 0};
    int kick_team = 0;             // 下一次开球的队
    i64 bx = 0, by = 0, bvx = 0, bvy = 0;
    i64 px[4] = {}, py[4] = {}, pvx[4] = {}, pvy[4] = {};
    // 统计（不影响结算）
    std::vector<std::pair<i64, int>> goals;  // (进球后的步数, 得分队)
    i64 kicks[4] = {0, 0, 0, 0};
    i64 invalid[4] = {0, 0, 0, 0};
    unsigned touch_mask = 0;  // 最后一次触球（同一步同一阶段：踢球或球与球员碰撞）的全部球员，位掩码；开球时清零
    i64 last_kick_t = 0;
    i64 stalls = 0;

    Game() : Game(Config(), 0, 0, false) {}
    Game(const Config& c, u64 seed_, int first_kick, bool log_ = false)
        : cfg(c), log(log_), rng(seed_), seed(seed_), kick_team(first_kick) {
        nwalls = make_walls(cfg, walls);
        gy0 = cfg.gy0();
        gy1 = cfg.gy1();
        kickoff();
    }

    void kickoff() {
        const Config& c = cfg;
        const i64 W = c.w, H = c.h;
        const i64 db = rng.jitter(c.jitter);
        const i64 d1 = rng.jitter(c.jitter);
        const i64 d2 = rng.jitter(c.jitter);
        bx = W / 2;
        by = H / 2 + db;
        bvx = bvy = 0;
        for (int team = 0; team < 2; ++team) {
            i64 fx = W / 2 - c.ko_front, fy = H / 2 + d1;
            i64 kx = c.ko_back, ky = H / 2 + d2;
            if (c.kickoff == KICKOFF_CONCEDER && team == kick_team) {
                fx = W / 2 - (c.rp + c.rb + 50);
                fy = H / 2 + db;
            }
            if (team == 1) {
                fx = W - fx;
                kx = W - kx;
            }
            px[2 * team] = fx;
            py[2 * team] = fy;
            px[2 * team + 1] = kx;
            py[2 * team + 1] = ky;
        }
        for (int p = 0; p < 4; ++p) pvx[p] = pvy[p] = cd[p] = 0;
        touch_mask = 0;
        last_kick_t = t;
        if (log) {
            Event e{EV_KICKOFF, t};
            e.a = c.kickoff == KICKOFF_CONCEDER ? kick_team : -1;
            e.b = bx;
            e.c = by;
            events.push_back(e);
        }
    }

    // 球员 p 看到的局面（自己向右进攻）
    Obs observe(int p) const {
        Obs o;
        const int team = p / 2;
        const bool flip = team == 1;
        const i64 W = cfg.w;
        auto fx = [&](i64 x) { return flip ? W - x : x; };
        auto fv = [&](i64 v) { return flip ? -v : v; };
        o.t = t;
        o.left = cfg.steps - t;
        o.my_score = score[team];
        o.opp_score = score[1 - team];
        o.bx = fx(bx);
        o.by = by;
        o.bvx = fv(bvx);
        o.bvy = bvy;
        int order[4] = {p, p ^ 1, 0, 0};
        int k = 2;
        for (int q = 0; q < 4; ++q)
            if (q / 2 != team) order[k++] = q;
        for (int i = 0; i < 4; ++i) {
            const int q = order[i];
            o.pl[i] = {fx(px[q]), py[q], fv(pvx[q]), pvy[q], cd[q]};
        }
        o.mate_msg = msg[p ^ 1];
        o.idx = p % 2;
        return o;
    }

    static bool action_in_range(const Action& a) {
        const i64 v[5] = {a.ax, a.ay, a.kx, a.ky, a.msg};
        for (i64 x : v)
            if (x > ACTION_ABS_MAX || x < -ACTION_ABS_MAX) return false;
        return true;
    }

    // 走一步。acts[p] 是球员 p 自己视角（向右进攻）的输出。返回进球队（0 = A，1 = B），没进球返回 -1。
    int step(const Action acts_in[4]) {
        if (over) return -1;
        const Config& c = cfg;
        i64 A[4][5];
        for (int p = 0; p < 4; ++p) {
            const Action& a = acts_in[p];
            i64 ax = a.ax, ay = a.ay, kx = a.kx, ky = a.ky, m = a.msg;
            if (!a.ok || !action_in_range(a)) {
                ++invalid[p];
                ax = ay = kx = ky = m = 0;
            }
            if (p / 2 == 1) {
                ax = -ax;
                kx = -kx;
            }
            if (c.msg_max < 0 || m < 0 || m > c.msg_max) m = 0;
            A[p][0] = ax;
            A[p][1] = ay;
            A[p][2] = kx;
            A[p][3] = ky;
            A[p][4] = m;
        }
        // 2. 踢球
        const i64 reach2 = c.kick_reach * c.kick_reach;
        i64 sx = 0, sy = 0;
        bool kicked = false;
        int nk = 0;
        int kp[4];
        i64 kvx[4], kvy[4];
        bool kicked_now[4] = {false, false, false, false};
        for (int p = 0; p < 4; ++p) {
            i64 kx = A[p][2], ky = A[p][3];
            if (kx || ky) {
                const i64 dx = bx - px[p], dy = by - py[p];
                if (cd[p] > 0) {
                    if (log) {
                        Event e{EV_NOKICK, t};
                        e.a = p;
                        e.b = 0;
                        e.c = cd[p];
                        events.push_back(e);
                    }
                    continue;
                }
                if (dx * dx + dy * dy <= reach2) {
                    clamp_len(kx, ky, c.kick_max);
                    sx += kx;
                    sy += ky;
                    kicked = true;
                    kp[nk] = p;
                    kvx[nk] = kx;
                    kvy[nk] = ky;
                    ++nk;
                    cd[p] = c.kick_cooldown;
                    kicked_now[p] = true;
                    ++kicks[p];
                } else if (log) {
                    Event e{EV_NOKICK, t};
                    e.a = p;
                    e.b = 1;
                    e.c = isqrt(dx * dx + dy * dy);
                    events.push_back(e);
                }
            }
        }
        if (kicked) {
            last_kick_t = t + 1;
            touch_mask = 0;
            for (int i = 0; i < nk; ++i) touch_mask |= 1u << kp[i];
            clamp_len(sx, sy, c.kick_max);
            bvx = sx;
            bvy = sy;
            if (log)
                for (int i = 0; i < nk; ++i) {
                    Event e{EV_KICK, t};
                    e.a = kp[i];
                    e.b = kvx[i];
                    e.c = kvy[i];
                    e.d = bvx;
                    e.e = bvy;
                    e.f = nk;
                    events.push_back(e);
                }
        }
        // 3. 加速
        for (int p = 0; p < 4; ++p) {
            i64 ax = A[p][0], ay = A[p][1];
            clamp_len(ax, ay, c.acc_max);
            pvx[p] += ax;
            pvy[p] += ay;
        }
        // 4. 移动
        for (int p = 0; p < 4; ++p) {
            px[p] += pvx[p];
            py[p] += pvy[p];
        }
        bx += bvx;
        by += bvy;
        // 5. 碰撞
        players_collide();
        for (int p = 0; p < 4; ++p) wall_collide(walls, nwalls, gy0, gy1, false, c.rp, 0, 1, px[p], py[p], pvx[p], pvy[p]);
        ball_players();
        int scored = -1;
        if (gy0 <= by && by <= gy1) {
            if (bx < 0) scored = 1;
            else if (bx > c.w) scored = 0;
        }
        if (scored < 0) {
            const unsigned hit = wall_collide(walls, nwalls, gy0, gy1, true, c.rb, c.e_wall_n, c.e_wall_d, bx, by, bvx, bvy);
            if (log)
                for (int i = 0; i < nwalls; ++i)
                    if (hit & (1u << i)) {
                        Event e{EV_WALL, t};
                        e.a = i;
                        events.push_back(e);
                    }
        }
        // 6. 摩擦、冷却、消息
        for (int p = 0; p < 4; ++p) {
            pvx[p] = tdiv(pvx[p] * c.pf_n, c.pf_d);
            pvy[p] = tdiv(pvy[p] * c.pf_n, c.pf_d);
            if (cd[p] > 0 && !kicked_now[p]) --cd[p];
        }
        bvx = tdiv(bvx * c.bf_n, c.bf_d);
        bvy = tdiv(bvy * c.bf_n, c.bf_d);
        for (int p = 0; p < 4; ++p) msg[p] = A[p][4];
        // 7. 进球与结束
        ++t;
        if (scored >= 0) {
            ++score[scored];
            goals.push_back({t, scored});
            if (log) {
                Event e{EV_GOAL, t - 1};
                e.a = scored;
                e.b = -1;
                for (int p = 0; p < 4; ++p)
                    if (touch_mask == (1u << p)) e.b = p;
                e.c = score[0];
                e.d = score[1];
                e.e = i64(touch_mask);
                events.push_back(e);
            }
            kick_team = 1 - scored;
            kickoff();
        } else if (c.stall && t - last_kick_t >= c.stall && t < c.steps) {
            ++stalls;
            if (log) {
                Event e{EV_STALL, t};
                e.a = c.stall;
                events.push_back(e);
            }
            kickoff();
        }
        if (t >= c.steps) over = true;
        return scored;
    }

    // 状态摘要：与原型 engine.Game.digest() 相同（repr(state()) 的 SHA-1 前 16 位十六进制）
    std::string state_repr() const {
        std::string o = "(" + std::to_string(t) + ", (" + std::to_string(score[0]) + ", " + std::to_string(score[1]) +
                        "), " + std::to_string(bx) + ", " + std::to_string(by) + ", " + std::to_string(bvx) + ", " +
                        std::to_string(bvy);
        auto tup = [&](const i64* v) {
            o += ", (";
            for (int i = 0; i < 4; ++i) {
                if (i) o += ", ";
                o += std::to_string(v[i]);
            }
            o += ")";
        };
        tup(px);
        tup(py);
        tup(pvx);
        tup(pvy);
        tup(cd);
        tup(msg);
        o += ")";
        return o;
    }
    std::string digest() const;

private:
    void players_collide() {
        const Config& c = cfg;
        const i64 R = 2 * c.rp;
        const i64 en = c.e_pp_n, ed = c.e_pp_d;
        i64 X[4], Y[4], VX[4], VY[4];
        for (int p = 0; p < 4; ++p) X[p] = px[p], Y[p] = py[p], VX[p] = pvx[p], VY[p] = pvy[p];
        for (int a = 0; a < 4; ++a)
            for (int b = a + 1; b < 4; ++b) {
                i64 dx = X[b] - X[a], dy = Y[b] - Y[a];
                i64 d2 = dx * dx + dy * dy;
                if (d2 >= R * R) continue;
                i64 pen;
                if (d2 == 0) {
                    // 圆心完全重合：分离方向只由镜像不变的量决定（见文件开头第 5 条）
                    pen = R + 1;
                    dx = VX[a] - VX[b];  // 移动前 b 相对 a 的位置 = −(相对速度)
                    dy = VY[a] - VY[b];
                    if (dx == 0 && dy == 0) {
                        if (a / 2 != b / 2) dx = 1;  // a 属 A 队（编号小）：a 退向左（本方门），b 退向右
                        else dy = 1;                 // 同队：a 是队内 0 号，向上；b 向下
                    }
                    d2 = dx * dx + dy * dy;
                } else {
                    pen = R - isqrt(d2) + 1;
                }
                const i64 d = isqrt(d2);
                if (log) {
                    Event e{EV_BUMP, t};
                    e.a = a;
                    e.b = b;
                    events.push_back(e);
                }
                const i64 sx = tdiv(pen * dx, 2 * d), sy = tdiv(pen * dy, 2 * d);
                px[a] -= sx;
                py[a] -= sy;
                px[b] += sx;
                py[b] += sy;
                const i64 rvx = VX[b] - VX[a], rvy = VY[b] - VY[a];
                const i64 dot = rvx * dx + rvy * dy;
                if (dot < 0) {
                    const i64 jx = tdiv((ed + en) * dot * dx, 2 * ed * d2);
                    const i64 jy = tdiv((ed + en) * dot * dy, 2 * ed * d2);
                    pvx[a] += jx;
                    pvy[a] += jy;
                    pvx[b] -= jx;
                    pvy[b] -= jy;
                }
            }
    }

    void ball_players() {
        const Config& c = cfg;
        const i64 R = c.rp + c.rb;
        const i64 en = c.e_ball_n, ed = c.e_ball_d;
        const i64 x0 = bx, y0 = by, vx0 = bvx, vy0 = bvy;
        unsigned touched = 0;
        for (int p = 0; p < 4; ++p) {
            i64 dx = x0 - px[p], dy = y0 - py[p];
            i64 d2 = dx * dx + dy * dy;
            if (d2 >= R * R) continue;
            if (d2 == 0) dx = 0, dy = 1, d2 = 1;
            const i64 d = isqrt(d2);
            const i64 pen = R - d + 1;
            const i64 rvx = vx0 - pvx[p], rvy = vy0 - pvy[p];
            const i64 dot = rvx * dx + rvy * dy;
            if (log) {
                Event e{EV_TOUCH, t};
                e.a = p;
                e.b = dot < 0 ? 1 : 0;
                events.push_back(e);
            }
            touched |= 1u << p;
            bx += tdiv(pen * dx, d);
            by += tdiv(pen * dy, d);
            if (dot < 0) {
                bvx -= tdiv((ed + en) * dot * dx, ed * d2);
                bvy -= tdiv((ed + en) * dot * dy, ed * d2);
            }
        }
        if (touched) touch_mask = touched;
    }
};

// ------------------------------------------------------------------ SHA-1（只用于状态摘要）

namespace detail {
inline std::string sha1_hex(const std::string& msg) {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    std::string m = msg;
    const u64 bits = u64(msg.size()) * 8;
    m += char(0x80);
    while (m.size() % 64 != 56) m += char(0);
    for (int i = 7; i >= 0; --i) m += char((bits >> (8 * i)) & 0xFF);
    auto rol = [](uint32_t x, int k) { return (x << k) | (x >> (32 - k)); };
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(uint8_t(m[off + 4 * i])) << 24) | (uint32_t(uint8_t(m[off + 4 * i + 1])) << 16) |
                   (uint32_t(uint8_t(m[off + 4 * i + 2])) << 8) | uint32_t(uint8_t(m[off + 4 * i + 3]));
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999u;
            else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1u;
            else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDCu;
            else f = b ^ c ^ d, k = 0xCA62C1D6u;
            const uint32_t tmp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = tmp;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }
    char buf[41];
    for (int i = 0; i < 5; ++i) std::snprintf(buf + 8 * i, 9, "%08x", unsigned(h[i]));
    return std::string(buf, 40);
}
}  // namespace detail

inline std::string Game::digest() const { return detail::sha1_hex(state_repr()).substr(0, 16); }

// ------------------------------------------------------------------ 选手协议（文本）

// 开局时发给选手的头部（7 行），最后一行是 "IDX STEP_MS FIRST_MS"。idx = 选手在队里的编号（0/1）
inline std::string protocol_header(const Config& c, int idx, i64 step_ms, i64 first_ms) {
    char b[512];
    std::snprintf(b, sizeof b,
                  "SOCCER2V2 1\n%lld %lld %lld %lld %lld %lld\n%lld %lld %lld %lld %lld\n%lld %lld %lld\n"
                  "%lld %lld %lld %lld %lld %lld\n%lld %lld %lld\n%d %lld %lld\n",
                  c.w, c.h, c.goal, c.chamfer, c.rp, c.rb, c.acc_max, c.pf_n, c.pf_d, c.bf_n, c.bf_d, c.kick_max,
                  c.kick_reach, c.kick_cooldown, c.e_wall_n, c.e_wall_d, c.e_ball_n, c.e_ball_d, c.e_pp_n, c.e_pp_d,
                  c.steps, c.stall, c.msg_max < 0 ? 0LL : c.msg_max, idx, step_ms, first_ms);
    return b;
}

// 每步发给选手的局面（6 行）
inline std::string protocol_step(const Obs& o) {
    char b[512];
    int n = std::snprintf(b, sizeof b, "%lld %lld %lld %lld %lld\n%lld %lld %lld %lld\n", o.t, o.left, o.my_score,
                          o.opp_score, o.mate_msg, o.bx, o.by, o.bvx, o.bvy);
    for (int i = 0; i < 4; ++i)
        n += std::snprintf(b + n, sizeof b - size_t(n), "%lld %lld %lld %lld %lld\n", o.pl[i].x, o.pl[i].y, o.pl[i].vx,
                           o.pl[i].vy, o.pl[i].cd);
    return std::string(b, size_t(n));
}

// 解析选手的一行回复 "AX AY KX KY MSG"。恰好 5 个十进制整数（可带正负号），空白分隔，
// 每个绝对值不超过 ACTION_ABS_MAX；否则返回 ok = false。
inline Action parse_action(const std::string& line) {
    Action a;
    i64 v[5];
    int k = 0;
    size_t i = 0;
    const size_t n = line.size();
    auto space = [](char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; };
    while (true) {
        while (i < n && space(line[i])) ++i;
        if (i >= n) break;
        if (k == 5) {
            a.ok = false;
            return a;
        }
        bool neg = false;
        if (line[i] == '+' || line[i] == '-') neg = line[i] == '-', ++i;
        if (i >= n || line[i] < '0' || line[i] > '9') {
            a.ok = false;
            return a;
        }
        i64 x = 0;
        while (i < n && line[i] >= '0' && line[i] <= '9') {
            x = x * 10 + (line[i] - '0');
            if (x > ACTION_ABS_MAX) {
                a.ok = false;
                return a;
            }
            ++i;
        }
        if (i < n && !space(line[i])) {
            a.ok = false;
            return a;
        }
        v[k++] = neg ? -x : x;
    }
    if (k != 5) {
        a.ok = false;
        return a;
    }
    a.ax = v[0];
    a.ay = v[1];
    a.kx = v[2];
    a.ky = v[3];
    a.msg = v[4];
    return a;
}

}  // namespace soccer
