// tron_main.cpp —— 主循环：读局面、调用你的 decide、写方向、设每回合的思考预算起点。不需要改。
//
// 你的 bot.cpp 只需要定义一个函数：
//
//     int decide(const tron::State& s);    // 每回合调用一次，返回 tron::U / R / D / L
//
// 正式编译命令（在选手包目录里；arena.py 会自动编译，一般用不到）：
//
//     g++ -std=c++20 -O2 -static -Wl,--stack,67108864 -I. submission/bot.cpp tron_main.cpp -o bot.exe
//
// 同一份文件两种编法：
//   默认            独立程序，从标准输入读、往标准输出写（正式模式）。
//   -DTRON_DLL      动态库，给快速模式的对局程序直接调用（arena.py 自动处理）。
// 两种编法里，你的 decide 看到的局面完全相同。

#include "tron.hpp"

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

int decide(const tron::State& s);

namespace {

// 调用 decide；返回方向（0..3），其余值都当作不合法
int call_decide(const tron::State& s) {
    const int d = decide(s);
    return (d >= 0 && d <= 3) ? d : -1;
}

}  // namespace

#ifdef TRON_DLL
// ====================================================================
// 快速模式：动态库。对局程序每局重新加载一次，全局变量每局都回到初始值。
// ====================================================================
#define TRON_EXPORT extern "C" __declspec(dllexport)

TRON_EXPORT int tronbot_abi(int* state_size, const char** engine_version) {
    *state_size = int(sizeof(tron::State));
    *engine_version = tron::ENGINE_VERSION;
    return 1;
}

TRON_EXPORT int tronbot_setup(int kind, double per_ms) { return tron::budget_setup(kind, per_ms); }

// 返回 0 正常；1 bot 抛出了没接住的异常（说明写进 err）。*dir 为 0..3 或 -1（不合法）。
TRON_EXPORT int tronbot_decide(const tron::State* s, int* dir, char* err, int cap) {
    tron::budget_begin(s->turn == 0 ? tron::BUDGET_FIRST_MS : tron::BUDGET_TURN_MS);
    try {
        *dir = call_decide(*s);
        return 0;
    } catch (const std::exception& e) {
        std::snprintf(err, size_t(cap), "%s", e.what());
    } catch (...) {
        std::snprintf(err, size_t(cap), "未知类型的异常");
    }
    return 1;
}

#else
// ====================================================================
// 正式模式：独立程序。协议（选手不用关心）：
//   开局  TRON <N> <W> <H> <我的编号> <cpu|wall> <每 CPU 毫秒多少个读数单位>
//         接着 H 行地图（'#' 障碍、'.' 空地），再 N 行出生点 "x y"
//   每回合 T <回合号> <上一回合各人实际走的方向，N 个字符，死者为 '-'；第 0 回合为 '-'>
//   回复  <回合号> <U|R|D|L|X>          X 表示 decide 的返回值不合法
//   读到 END 或输入结束就退出。
// ====================================================================
#include <io.h>

namespace {

bool read_line(std::string& line) {
    line.clear();
    char buf[4096];
    while (std::fgets(buf, sizeof buf, stdin)) {
        line += buf;
        if (!line.empty() && line.back() == '\n') break;
    }
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    return !line.empty() || !std::feof(stdin);
}

int dir_of(char c) {
    for (int d = 0; d < 4; ++d)
        if (tron::DIR_CHAR[d] == c) return d;
    return -1;
}

}  // namespace

int main() {
    // 协议输出单独留一个句柄，标准输出并到标准错误：bot 里 printf / cout 的调试信息不会被当成回复。
    std::FILE* out = stdout;
    std::fflush(stdout);
    const int fd = _dup(1);
    if (fd >= 0 && _dup2(2, 1) >= 0)
        if (std::FILE* f = _fdopen(fd, "w")) {
            out = f;
            std::setvbuf(stdout, nullptr, _IONBF, 0);
        }

    std::string line;
    if (!read_line(line)) return 0;
    int N = 0, W = 0, H = 0, me = -1;
    char kind[16] = "wall";
    double per_ms = 1;
    if (std::sscanf(line.c_str(), "TRON %d %d %d %d %15s %lf", &N, &W, &H, &me, kind, &per_ms) < 4 || N < 2 ||
        N > tron::MAXN || W < 1 || W > tron::MAXW || H < 1 || H > tron::MAXH || me < 0 || me >= N) {
        std::fprintf(stderr, "tron_main: 开局第一行读不懂：%s\n", line.c_str());
        return 2;
    }
    const bool cpu = tron::budget_setup(std::strcmp(kind, "cpu") == 0 ? tron::BUDGET_CPU : tron::BUDGET_WALL, per_ms) ==
                     tron::BUDGET_CPU;
    // 首回合的预算从进程启动算起（裁判也这样记）
    tron::detail::budget_state.limit = tron::BUDGET_FIRST_MS;
    tron::detail::budget_state.start = cpu ? 0.0 : tron::detail::budget_reading();

    std::vector<std::string> rows(H);
    for (int y = 0; y < H; ++y)
        if (!read_line(rows[y]) || int(rows[y].size()) < W) {
            std::fprintf(stderr, "tron_main: 地图第 %d 行读不懂\n", y);
            return 2;
        }
    std::vector<std::pair<int, int>> spawns(N);
    for (int k = 0; k < N; ++k)
        if (!read_line(line) || std::sscanf(line.c_str(), "%d %d", &spawns[k].first, &spawns[k].second) != 2) {
            std::fprintf(stderr, "tron_main: 出生点读不懂\n");
            return 2;
        }
    tron::State s = tron::make_state(W, H, rows, spawns, me);

    while (read_line(line)) {
        if (line == "END") break;
        int t = -1;
        char moves[64] = "";
        if (std::sscanf(line.c_str(), "T %d %63s", &t, moves) != 2) {
            std::fprintf(stderr, "tron_main: 回合行读不懂：%s\n", line.c_str());
            return 2;
        }
        if (t > 0) {
            if (int(std::strlen(moves)) != N) {
                std::fprintf(stderr, "tron_main: 回合行读不懂：%s\n", line.c_str());
                return 2;
            }
            int mv[tron::MAXN];
            for (int k = 0; k < N; ++k) mv[k] = dir_of(moves[k]);
            tron::step(s, mv);
            if (!cpu) tron::budget_begin(tron::BUDGET_TURN_MS);
        }
        if (s.turn != t) {
            std::fprintf(stderr, "tron_main: 回合号对不上（本地 %d，裁判 %d）\n", s.turn, t);
            return 2;
        }
        if (s.over) break;
        const int d = call_decide(s);
        std::fprintf(out, "%d %c\n", t, d < 0 ? 'X' : tron::DIR_CHAR[d]);
        std::fflush(out);
        if (cpu) tron::budget_begin(tron::BUDGET_TURN_MS);    // 等输入不耗 CPU，下一回合从这里算起
    }
    return 0;
}
#endif
