// tron.hpp —— 光轮对战的规则引擎。裁判用的就是这一份；RULES.md 是它的说明，两者不一致时以本文件为准。
//
// 你的 bot 只需要用到：
//   1. State            局面（棋盘、各人位置、存活、回合号、你的编号）
//   2. 方向与小工具      U R D L、DX/DY、inside、is_free、can_move、priority
//   3. step             精确结算一回合（想往前推演时，把局面拷一份再调用）
//   4. budget_left      本回合还剩多少思考预算（CPU 毫秒）
// 第 5、6 节（地图生成、文本）是主办方的工具，bot 一般用不到。
//
// 只依赖标准库。坐标 (x, y)：x 是列，向右增大；y 是行，向下增大；都从 0 开始。棋盘按 cell[y][x] 存。
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <ctime>
#endif

namespace tron {

inline constexpr const char* ENGINE_VERSION = "tron-1.0";
inline constexpr int MAXW = 41, MAXH = 41, MAXN = 12;

// ====================================================================
// 1. 局面
// ====================================================================

// 格子的取值：EMPTY 空地；WALL 障碍；0..N-1 是该玩家的轨迹（含他现在所在的格）。
inline constexpr int EMPTY = -1;
inline constexpr int WALL = -2;

// 死因
enum Cause : int { ALIVE = 0, OUT_OF_BOARD = 1, HIT_OBSTACLE = 2, HIT_SELF = 3, HIT_TRAIL = 4, HEAD_ON = 5 };
inline constexpr const char* CAUSE_NAME[] = {"alive", "out", "obstacle", "self", "trail", "headon"};

struct State {
    int W = 0, H = 0;           // 棋盘宽、高
    int N = 0;                  // 本局人数（2、3、4 或 12）
    int me = 0;                 // 你的编号（0..N-1）
    int turn = 0;               // 当前回合号，从 0 开始；decide 被调用时就是你要决定的这一回合
    int max_turns = 0;          // 回合上限（W×H，实际到不了）
    bool over = false;          // 对局是否已结束
    int8_t cell[MAXH][MAXW];    // cell[y][x]：EMPTY / WALL / 轨迹主人的编号
    int x[MAXN], y[MAXN];       // 各人头部（当前所在格）。死者停在死前所在格
    bool alive[MAXN];           // 是否存活
    int last_dir[MAXN];         // 各人上一回合实际走的方向（第 0 回合之前视为 U）
    int death_turn[MAXN];       // 死在第几回合（活着为 -1）
    int cause[MAXN];            // 死因（见 Cause）

    int at(int cx, int cy) const { return cell[cy][cx]; }
};

// ====================================================================
// 2. 方向与小工具
// ====================================================================

enum Dir : int { U = 0, R = 1, D = 2, L = 3 };     // 上、右、下、左（顺时针）
inline constexpr int DX[4] = {0, 1, 0, -1};
inline constexpr int DY[4] = {-1, 0, 1, 0};
inline constexpr char DIR_CHAR[] = "URDL";

inline int opposite(int d) { return (d + 2) & 3; }   // 反方向（掉头就是撞自己的轨迹，必死）

inline bool inside(const State& s, int cx, int cy) { return cx >= 0 && cx < s.W && cy >= 0 && cy < s.H; }

// 这一格在棋盘内并且是空地。
inline bool is_free(const State& s, int cx, int cy) { return inside(s, cx, cy) && s.cell[cy][cx] == EMPTY; }

// 玩家 k 朝 d 走一步，目标格现在是不是空地（不考虑别人这一回合也走进来）。
inline bool can_move(const State& s, int k, int d) { return is_free(s, s.x[k] + DX[d], s.y[k] + DY[d]); }

// 本回合的优先级：数值越小越优先。几个人同一回合走进同一格时，最优先的占下这一格，其余的死。
// 等于 (编号 - 回合号) mod 人数：2 人时偶数回合 0 号优先、奇数回合 1 号优先。
inline int priority(const State& s, int k) { return ((k - s.turn) % s.N + s.N) % s.N; }

inline int alive_count(const State& s) {
    int c = 0;
    for (int k = 0; k < s.N; ++k) c += s.alive[k];
    return c;
}

// ====================================================================
// 3. 结算一回合
// ====================================================================
// moves[k] 是玩家 k 这一回合的方向（死者的项不看）。不是 0..3 的值按"沿上一回合方向"处理。
// 结算顺序：
//   1. 目标格出界、是障碍、是任何人的轨迹（含任何人现在所在的格、自己来的格）→ 死。
//   2. 剩下的人里，几个人目标格相同 → priority 最小的那个占下，其余死（HEADON）。
//      两人互换位置：目标格是对方现在所在的格，按第 1 条两人都死。
//   3. 没死的人走到目标格，该格成为他的轨迹。死者停在原地，轨迹保留。
//   4. 回合号加一；存活不超过 1 人，或回合数到 max_turns，对局结束。
inline void step(State& s, const int moves[]) {
    if (s.over) return;
    int tx[MAXN], ty[MAXN], why[MAXN];
    for (int k = 0; k < s.N; ++k) {
        why[k] = ALIVE;
        if (!s.alive[k]) continue;
        int d = moves[k];
        if (d < 0 || d > 3) d = s.last_dir[k];
        s.last_dir[k] = d;
        tx[k] = s.x[k] + DX[d];
        ty[k] = s.y[k] + DY[d];
        if (!inside(s, tx[k], ty[k])) why[k] = OUT_OF_BOARD;
        else if (s.cell[ty[k]][tx[k]] == WALL) why[k] = HIT_OBSTACLE;
        else if (s.cell[ty[k]][tx[k]] == k) why[k] = HIT_SELF;
        else if (s.cell[ty[k]][tx[k]] >= 0) why[k] = HIT_TRAIL;
    }
    for (int k = 0; k < s.N; ++k) {
        if (!s.alive[k] || why[k] != ALIVE) continue;
        int best = k, rivals = 1;
        for (int j = 0; j < s.N; ++j) {
            if (j == k || !s.alive[j] || tx[j] != tx[k] || ty[j] != ty[k]) continue;
            ++rivals;
            if (priority(s, j) < priority(s, best)) best = j;
        }
        if (rivals >= 2 && best != k) why[k] = HEAD_ON;
    }
    for (int k = 0; k < s.N; ++k) {
        if (!s.alive[k]) continue;
        if (why[k] != ALIVE) {
            s.alive[k] = false;
            s.death_turn[k] = s.turn;
            s.cause[k] = why[k];
        } else {
            s.x[k] = tx[k];
            s.y[k] = ty[k];
            s.cell[ty[k]][tx[k]] = int8_t(k);
        }
    }
    ++s.turn;
    if (alive_count(s) <= 1 || s.turn >= s.max_turns) s.over = true;
}

// 名次（0 起）：活到最后的并列第 0；其余按死亡回合，越晚越靠前，同一回合死的并列。
// 返回值 = 比他严格靠前的人数。计分用的"同回合并列取平均"见 RULES.md。
inline int rank_of(const State& s, int k) {
    auto key = [&](int j) { return s.alive[j] ? s.max_turns + 1 : s.death_turn[j]; };
    int r = 0;
    for (int j = 0; j < s.N; ++j) r += key(j) > key(k);
    return r;
}

// ====================================================================
// 4. 思考预算
// ====================================================================
// 每回合的思考有预算：首回合（第 0 回合，含程序启动）BUDGET_FIRST_MS，之后每回合 BUDGET_TURN_MS，
// 单位是"CPU 毫秒"：你的程序自己消耗的 CPU 时间，等输入、被挂起的时间不算。
// bot 只需要用 budget_left()：
//     while (tron::budget_left() > 5) { /* 再多想一点 */ }
// 起点由主办方的主循环设好，bot 不要自己调 budget_setup / budget_begin。
// budget_left() 只看得到调用它的那个线程；裁判量的是整个进程，比它看到的多一点，留几毫秒余量。
inline constexpr int BUDGET_FIRST_MS = 2000;
inline constexpr int BUDGET_TURN_MS = 50;
inline constexpr int WALL_FIRST_MS = 4000;    // 墙上时钟的保险上限：首回合
inline constexpr int WALL_TURN_MS = 2000;     // 墙上时钟的保险上限：之后每回合

enum BudgetKind : int { BUDGET_NONE = 0, BUDGET_CPU = 1, BUDGET_WALL = 2 };

namespace detail {
#if defined(_WIN32)
namespace win {    // 只声明用到的两个系统函数，不引入 <windows.h>
extern "C" __declspec(dllimport) void* __stdcall GetCurrentThread();
extern "C" __declspec(dllimport) int __stdcall QueryThreadCycleTime(void* thread, unsigned long long* cycles);
}  // namespace win
#endif

struct BudgetState {
    int kind = BUDGET_NONE;
    double per_ms = 1;    // 读数的多少个单位算 1 毫秒
    double start = 0;
    double limit = 0;
};
inline BudgetState budget_state;

// 当前读数，只增不减。BUDGET_CPU：本线程消耗的 CPU（Windows 为周期数，其他系统为纳秒）；否则为墙上毫秒。
inline double budget_reading() {
    if (budget_state.kind == BUDGET_CPU) {
#if defined(_WIN32)
        unsigned long long c = 0;
        win::QueryThreadCycleTime(win::GetCurrentThread(), &c);
        return double(c);
#elif defined(CLOCK_THREAD_CPUTIME_ID)
        timespec ts{};
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
        return double(ts.tv_sec) * 1e9 + double(ts.tv_nsec);
#endif
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace detail

// 以下三个由主循环调用。
inline int budget_setup(int kind, double per_ms) {
    if (kind == BUDGET_CPU && per_ms > 0) {
        detail::budget_state.kind = BUDGET_CPU;
        detail::budget_state.per_ms = per_ms;
    } else {
        detail::budget_state.kind = BUDGET_WALL;
        detail::budget_state.per_ms = 1;
    }
    return detail::budget_state.kind;
}
inline void budget_begin(double limit_ms) {
    detail::budget_state.limit = limit_ms;
    detail::budget_state.start = detail::budget_reading();
}
inline double budget_used() {
    if (detail::budget_state.kind == BUDGET_NONE) return 0.0;
    return (detail::budget_reading() - detail::budget_state.start) / detail::budget_state.per_ms;
}

// 本回合还剩多少 CPU 毫秒（可能为负）。没有主循环时（比如自己写的测试程序里）返回一个很大的数。
inline double budget_left() {
    if (detail::budget_state.kind == BUDGET_NONE) return 1e12;
    return detail::budget_state.limit - budget_used();
}

// ====================================================================
// 5. 地图生成（公开，主办方用；由人数和种子完全决定）
// ====================================================================
//   2 人   21×21，点对称（绕中心转 180°），两个出生点互为对称，曼哈顿距离 ≥ 14
//   3 人   用 4 人的地图，空出 3 号出生点
//   4 人   25×25，90° 旋转对称，4 个出生点互为旋转
//   12 人  41×41，障碍 90° 旋转对称；出生点等角度放在半径 0.36×41 的圆上（近似对称）
// 障碍约占 8%，是 1～3 格的短条；出生点四邻清空（12 人局清 3×3）；与出生点不连通的空地封成障碍。

namespace gen {

struct Rng {    // splitmix64：只用整数运算，各平台结果相同
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    int below(int n) { return int(((next() >> 32) * uint64_t(n)) >> 32); }    // [0, n)
    double unit() { return double(next() >> 11) * (1.0 / 9007199254740992.0); }    // [0, 1)
};

inline uint64_t mix(uint64_t a, uint64_t b) {
    Rng r(a * 0x100000001B3ULL ^ (b + 0x632BE59BD9B4E019ULL));
    r.next();
    return r.next();
}

enum Sym { POINT, ROT4 };

struct Map {
    int W = 0, H = 0;
    std::vector<std::vector<bool>> wall;      // wall[y][x]
    std::vector<std::pair<int, int>> spawns;  // (x, y)
};

inline void sym_points(int x, int y, int W, int H, Sym sym, std::vector<std::pair<int, int>>& out) {
    out.clear();
    out.push_back({x, y});
    if (sym == POINT) {
        out.push_back({W - 1 - x, H - 1 - y});
    } else {
        for (int i = 0; i < 3; ++i) {    // 顺时针 90°：(x, y) -> (W-1-y, x)
            auto [px, py] = out.back();
            out.push_back({W - 1 - py, px});
        }
    }
}

// 4 邻连通块编号；返回每格的块号（障碍为 -1）
inline std::vector<std::vector<int>> components(const std::vector<std::vector<bool>>& wall, int W, int H) {
    std::vector<std::vector<int>> comp(H, std::vector<int>(W, -1));
    std::vector<std::pair<int, int>> q;
    int id = 0;
    for (int sy = 0; sy < H; ++sy)
        for (int sx = 0; sx < W; ++sx) {
            if (wall[sy][sx] || comp[sy][sx] >= 0) continue;
            q.assign(1, {sx, sy});
            comp[sy][sx] = id;
            for (size_t h = 0; h < q.size(); ++h) {
                auto [x, y] = q[h];
                for (int d = 0; d < 4; ++d) {
                    int nx = x + DX[d], ny = y + DY[d];
                    if (nx >= 0 && nx < W && ny >= 0 && ny < H && !wall[ny][nx] && comp[ny][nx] < 0) {
                        comp[ny][nx] = id;
                        q.push_back({nx, ny});
                    }
                }
            }
            ++id;
        }
    return comp;
}

// 所有出生点连通则封死其余空地并返回 true
inline bool seal(Map& m) {
    auto comp = components(m.wall, m.W, m.H);
    int main = comp[m.spawns[0].second][m.spawns[0].first];
    for (auto [x, y] : m.spawns)
        if (comp[y][x] != main || main < 0) return false;
    for (int y = 0; y < m.H; ++y)
        for (int x = 0; x < m.W; ++x)
            if (!m.wall[y][x] && comp[y][x] != main) m.wall[y][x] = true;
    return true;
}

// 对称地图（2 人点对称 / 4 人旋转对称）
inline Map symmetric(int W, Sym sym, uint64_t seed, double density = 0.08) {
    const int H = W, np = sym == POINT ? 2 : 4;
    std::vector<std::pair<int, int>> pts;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        Rng rng(mix(mix(seed, uint64_t(W) * 16 + sym), uint64_t(attempt)));
        Map m;
        m.W = W;
        m.H = H;
        m.wall.assign(H, std::vector<bool>(W, false));
        const int target = int(std::lround(density * W * H));
        int placed = 0;
        for (int guard = 0; placed < target && guard < 10000; ++guard) {
            const int x = rng.below(W), y = rng.below(H);
            static constexpr int LENS[4] = {1, 2, 2, 3};
            const int len = LENS[rng.below(4)];
            const bool horiz = rng.below(2) == 0;
            for (int i = 0; i < len; ++i) {
                const int cx = horiz ? x + i : x, cy = horiz ? y : y + i;
                if (cx >= W || cy >= H) break;
                sym_points(cx, cy, W, H, sym, pts);
                for (auto [px, py] : pts)
                    if (!m.wall[py][px]) {
                        m.wall[py][px] = true;
                        ++placed;
                    }
            }
        }
        // 出生点：在一个"基本区域"里选一点，其余由对称得到
        std::vector<std::pair<int, int>> region;
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x) {
                if (sym == POINT ? (std::make_pair(x, y) < std::make_pair(W - 1 - x, H - 1 - y))
                                 : (x < W / 2 && y < H / 2 - 1))
                    region.push_back({x, y});
            }
        const int md = (W + H) / 3;
        bool ok = false;
        for (int t = 0; t < 200 && !ok; ++t) {
            auto [sx, sy] = region[rng.below(int(region.size()))];
            sym_points(sx, sy, W, H, sym, pts);
            int dmin = 1 << 30;
            bool distinct = true;
            for (int i = 0; i < np; ++i)
                for (int j = i + 1; j < np; ++j) {
                    const int dd = std::abs(pts[i].first - pts[j].first) + std::abs(pts[i].second - pts[j].second);
                    if (dd == 0) distinct = false;
                    if (dd < dmin) dmin = dd;
                }
            if (distinct && dmin >= md) {
                m.spawns = pts;
                ok = true;
            }
        }
        if (!ok) continue;
        std::vector<std::pair<int, int>> q;
        for (auto [px, py] : std::vector<std::pair<int, int>>(m.spawns))
            for (int d = -1; d < 4; ++d) {    // 出生点本身和四邻，对称地清空
                const int cx = px + (d < 0 ? 0 : DX[d]), cy = py + (d < 0 ? 0 : DY[d]);
                if (cx < 0 || cx >= W || cy < 0 || cy >= H) continue;
                sym_points(cx, cy, W, H, sym, q);
                for (auto [qx, qy] : q) m.wall[qy][qx] = false;
            }
        if (seal(m)) return m;
    }
    return Map{};
}

// N 人圆周出生点（近似对称）
inline Map ring(int W, int n, uint64_t seed, double density = 0.08) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        Rng rng(mix(mix(seed, uint64_t(W) * 16 + 100 + uint64_t(n)), uint64_t(attempt)));
        Map m = symmetric(W, ROT4, mix(seed, 7777 + uint64_t(attempt)), density);
        if (m.W == 0) continue;
        const double c = (W - 1) / 2.0, R = 0.36 * W, a0 = rng.unit() * 2 * 3.14159265358979323846;
        m.spawns.clear();
        bool distinct = true;
        for (int k = 0; k < n; ++k) {
            const double a = a0 + 2 * 3.14159265358979323846 * k / n;
            const int x = int(std::lround(c + R * std::cos(a))), y = int(std::lround(c + R * std::sin(a)));
            for (auto p : m.spawns) distinct &= p != std::make_pair(x, y);
            m.spawns.push_back({x, y});
        }
        if (!distinct) continue;
        for (auto [px, py] : m.spawns)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (px + dx >= 0 && px + dx < W && py + dy >= 0 && py + dy < W) m.wall[py + dy][px + dx] = false;
        if (seal(m)) return m;
    }
    return Map{};
}

}  // namespace gen

// 由地图建立第 0 回合的局面。rows[y][x] 为 '#' 障碍、'.' 空地。
inline State make_state(int W, int H, const std::vector<std::string>& rows, const std::vector<std::pair<int, int>>& spawns,
                        int me = 0) {
    State s;
    s.W = W;
    s.H = H;
    s.N = int(spawns.size());
    s.me = me;
    s.turn = 0;
    s.max_turns = W * H;
    s.over = false;
    std::memset(s.cell, EMPTY, sizeof s.cell);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) s.cell[y][x] = int8_t(rows[y][x] == '#' ? WALL : EMPTY);
    for (int k = 0; k < MAXN; ++k) {
        s.x[k] = s.y[k] = -1;
        s.alive[k] = false;
        s.last_dir[k] = U;
        s.death_turn[k] = -1;
        s.cause[k] = ALIVE;
    }
    for (int k = 0; k < s.N; ++k) {
        s.x[k] = spawns[k].first;
        s.y[k] = spawns[k].second;
        s.alive[k] = true;
        s.cell[s.y[k]][s.x[k]] = int8_t(k);
    }
    return s;
}

inline int board_size(int n) { return n == 2 ? 21 : n <= 4 ? 25 : 41; }
inline bool valid_players(int n) { return n == 2 || n == 3 || n == 4 || n == 12; }

// 生成第 0 回合的局面。n 只能是 2、3、4、12；不合法时返回 N = 0 的局面。
inline State new_game(int n, uint64_t seed, int me = 0) {
    if (!valid_players(n)) return State{};
    gen::Map m = n == 2 ? gen::symmetric(21, gen::POINT, seed)
               : n <= 4 ? gen::symmetric(25, gen::ROT4, seed)
                        : gen::ring(41, n, seed);
    if (m.W == 0) return State{};
    if (n == 3) m.spawns.resize(3);
    std::vector<std::string> rows(m.H, std::string(m.W, '.'));
    for (int y = 0; y < m.H; ++y)
        for (int x = 0; x < m.W; ++x)
            if (m.wall[y][x]) rows[y][x] = '#';
    return make_state(m.W, m.H, rows, m.spawns, me);
}

// ====================================================================
// 6. 文本
// ====================================================================

// 地图行（'#' 障碍、'.' 空地，不含轨迹），用于协议与回放。
inline std::vector<std::string> map_rows(const State& s0) {
    std::vector<std::string> rows(s0.H, std::string(s0.W, '.'));
    for (int y = 0; y < s0.H; ++y)
        for (int x = 0; x < s0.W; ++x)
            if (s0.cell[y][x] == WALL) rows[y][x] = '#';
    return rows;
}

// 字符画：'#' 障碍，'.' 空地，轨迹用 a..l（第 k 人），活着的人的头部用 0..9、A、B，死者头部用 'x'。
inline std::string render(const State& s) {
    static const char TRAIL_CH[] = "abcdefghijkl", HEAD_CH[] = "0123456789AB";
    std::string out;
    for (int y = 0; y < s.H; ++y) {
        for (int x = 0; x < s.W; ++x) {
            const int c = s.cell[y][x];
            char ch = c == WALL ? '#' : c == EMPTY ? '.' : TRAIL_CH[c];
            if (c >= 0 && s.x[c] == x && s.y[c] == y) ch = s.alive[c] ? HEAD_CH[c] : 'x';
            out += ch;
        }
        out += '\n';
    }
    return out;
}

}  // namespace tron
