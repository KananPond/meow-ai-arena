// judge.cpp —— 裁判程序（Windows）。规则全部来自 ../tron.hpp；arena.py 只负责编译、排赛程、统计。
//
//   judge map <人数> <种子>
//       打印地图：第一行 "W H N"，接着 H 行地图，再 N 行出生点 "x y"。
//   judge render              （标准输入：第一行 "<人数> <种子> <回合>"，之后每行一回合的实际方向）
//       打印该回合结算后的字符画（回合 = 已结算的回合数）。
//   judge run fast|formal [--workers K] [--stderr-dir 目录] [--tmp 目录]
//       标准输入：若干行 "BOT <编号> <路径>"（快速模式为 .dll，正式模式为 .exe），
//                 若干行 "GAME <局号> <人数> <种子> <0 号座位的 bot 编号> <1 号座位的> ..."
//       标准输出：每局一行 JSON（见 write_result）；出现无法继续的故障时打一行 "X <说明>"，退出码 3。
//
// 两种模式共用同一段对局代码（play_game），区别只在怎么向 bot 要方向：
//   fast    bot 编成动态库，装进本进程直接调用；每个工作线程、每个座位用一份单独的副本，每局重新加载，
//           所以同一个 bot 坐几个座位、开几个线程都互不干扰，全局变量每局回到初始值。
//           CPU 用量只统计不判罚；bot 崩溃会让整批停下。
//   formal  每个 bot 一个进程，按文本协议通信（见 tron_main.cpp）。按进程 CPU 用量判超时，墙上时钟只作保险。
// 不看时间的 bot 在两种模式下打出的对局逐回合相同（只要正式模式下没有超时）。

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>

#include "../tron.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#include <x86intrin.h>
#endif

namespace {

// -------------------------------------------------------------------- 杂项

std::FILE* g_out = stdout;    // 结果输出（原来的标准输出）
std::mutex g_out_mu;
double g_per_ms = 1;          // 每 CPU 毫秒多少个周期

void emit(const std::string& line) {
    std::lock_guard<std::mutex> lk(g_out_mu);
    std::fputs(line.c_str(), g_out);
    std::fputc('\n', g_out);
    std::fflush(g_out);
}

[[noreturn]] void fatal(const std::string& what) {
    emit("X " + what);
    std::fflush(nullptr);
    ExitProcess(3);
}

std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

std::string json_str(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') o += '\\', o += char(c);
        else if (c < 0x20) {
            char b[8];
            std::snprintf(b, sizeof b, "\\u%04x", c);
            o += b;
        } else o += char(c);
    }
    return o + "\"";
}

// 每 CPU 毫秒多少周期：环境变量 TRON_CYCLES_PER_MS > CPUID 0x15 > 对着高精度时钟数 250 毫秒
double measure_per_ms() {
    if (const char* t = std::getenv("TRON_CYCLES_PER_MS"))
        if (std::atof(t) > 0) return std::atof(t);
#if defined(__x86_64__) || defined(__i386__)
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (__get_cpuid_max(0, nullptr) >= 0x15 && __get_cpuid(0x15, &a, &b, &c, &d) && a && b && c) {
        const unsigned long long hz = (unsigned long long)c * b / a;
        if (hz > 100000000ULL) return double(hz / 1000);
    }
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    const unsigned long long c0 = __rdtsc();
    do QueryPerformanceCounter(&t1);
    while (double(t1.QuadPart - t0.QuadPart) / double(f.QuadPart) < 0.25);
    const unsigned long long c1 = __rdtsc();
    const double ms = 1000.0 * double(t1.QuadPart - t0.QuadPart) / double(f.QuadPart);
    return double((long long)(double(c1 - c0) / ms / 100.0 + 0.5) * 100);
#else
    return 1e6;
#endif
}

double now_ms() {
    static LARGE_INTEGER f = [] {
        LARGE_INTEGER x;
        QueryPerformanceFrequency(&x);
        return x;
    }();
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return 1000.0 * double(t.QuadPart) / double(f.QuadPart);
}

double thread_cycles() {
    ULONG64 c = 0;
    QueryThreadCycleTime(GetCurrentThread(), &c);
    return double(c);
}

// -------------------------------------------------------------------- 每个座位的统计

struct SeatStats {
    double first_ms = 0, sum_ms = 0, max_ms = 0;
    int turns = 0;          // 第 0 回合之后被问过的回合数
    int timeouts = 0;       // 超预算（正式模式判罚：沿上一回合方向；快速模式只统计）
    int invalid = 0;        // 返回值不合法
    int crash_turn = -1;    // 进程崩溃 / 退出 / 读不懂的回合（只有正式模式）
    void add(int turn, double ms) {
        if (turn == 0) first_ms = ms;
        else {
            sum_ms += ms;
            max_ms = std::max(max_ms, ms);
            ++turns;
        }
    }
};

// 向 bot 要方向的接口。begin 每局一次；ask 把本回合的局面发出去（正式模式下各进程并行思考）；
// collect 取回方向（0..3，-1 = 没有合法的回复，按"沿上一回合方向"结算）。
struct Agent {
    SeatStats st;
    virtual ~Agent() = default;
    virtual void begin(const tron::State& s0) = 0;
    virtual void ask(const tron::State& s, const std::string& prev_moves) = 0;
    virtual int collect(const tron::State& s) = 0;
    virtual void finish() {}
};

// -------------------------------------------------------------------- 快速模式

using AbiFn = int (*)(int*, const char**);
using SetupFn = int (*)(int, double);
using DecideFn = int (*)(const tron::State*, int*, char*, int);

std::wstring g_tmp;    // 动态库副本放在这里
std::vector<std::wstring> g_bot_path;
thread_local int t_game = -1, t_seat = -1;
std::atomic<int> g_in_bot[64];         // 每个工作线程：正在调用的座位（-1 不在 bot 里）
std::atomic<double> g_in_since[64];

LONG WINAPI crash_filter(EXCEPTION_POINTERS* ep) {
    char line[256];
    std::snprintf(line, sizeof line, "X 快速模式下 bot 崩溃：第 %d 局 %d 号座位，异常代码 0x%08lX（用正式模式重打这一局可以定位）",
                  t_game, t_seat, ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0UL);
    std::fputs(line, g_out);
    std::fputc('\n', g_out);
    std::fflush(g_out);
    ExitProcess(3);
}

struct FastAgent : Agent {
    int bot, worker, seat;
    HMODULE lib = nullptr;
    DecideFn decide = nullptr;
    FastAgent(int b, int w, int s) : bot(b), worker(w), seat(s) {}
    ~FastAgent() override {
        if (lib) FreeLibrary(lib);
    }
    void begin(const tron::State&) override {
        // 副本按原文件名区分（arena.py 给的文件名里带内容哈希），内容变了就是新副本
        std::wstring stem = g_bot_path[size_t(bot)];
        stem = stem.substr(stem.find_last_of(L"\\/") + 1);
        if (stem.size() > 4) stem.resize(stem.size() - 4);
        const std::wstring copy =
            g_tmp + L"\\" + stem + L"_w" + std::to_wstring(worker) + L"_s" + std::to_wstring(seat) + L".dll";
        if (GetFileAttributesW(copy.c_str()) == INVALID_FILE_ATTRIBUTES &&
            !CopyFileW(g_bot_path[size_t(bot)].c_str(), copy.c_str(), FALSE))
            fatal("复制动态库失败（bot " + std::to_string(bot) + "）");
        lib = LoadLibraryW(copy.c_str());
        if (!lib) fatal("加载动态库失败（bot " + std::to_string(bot) + "，错误 " + std::to_string(GetLastError()) + "）");
        auto abi = (AbiFn)(void*)GetProcAddress(lib, "tronbot_abi");
        auto setup = (SetupFn)(void*)GetProcAddress(lib, "tronbot_setup");
        decide = (DecideFn)(void*)GetProcAddress(lib, "tronbot_decide");
        int size = 0;
        const char* ver = "";
        if (!abi || !setup || !decide || !abi(&size, &ver) || size != int(sizeof(tron::State)) ||
            std::strcmp(ver, tron::ENGINE_VERSION) != 0)
            fatal("动态库与本程序不是同一份 tron.hpp 编出来的（bot " + std::to_string(bot) + "）");
        setup(tron::BUDGET_CPU, g_per_ms);
    }
    void ask(const tron::State&, const std::string&) override {}
    int collect(const tron::State& s) override {
        tron::State view = s;
        view.me = seat;
        int dir = -1;
        char err[512] = "";
        t_seat = seat;
        g_in_since[worker] = now_ms();
        g_in_bot[worker] = seat;
        const double c0 = thread_cycles();
        const int rc = decide(&view, &dir, err, sizeof err);
        const double ms = (thread_cycles() - c0) / g_per_ms;
        g_in_bot[worker] = -1;
        if (rc != 0)
            fatal("快速模式下 bot 抛出了没接住的异常：第 " + std::to_string(t_game) + " 局 " + std::to_string(seat) +
                  " 号座位：" + err);
        st.add(s.turn, ms);
        if (ms > (s.turn == 0 ? tron::BUDGET_FIRST_MS : tron::BUDGET_TURN_MS)) ++st.timeouts;
        if (dir < 0) ++st.invalid;
        return dir;
    }
    void finish() override {
        if (lib) FreeLibrary(lib);
        lib = nullptr;
    }
};

// -------------------------------------------------------------------- 正式模式

HANDLE g_job = nullptr;
std::wstring g_stderr_dir;

double process_cycles(HANDLE p) {
    ULONG64 c = 0;
    QueryProcessCycleTime(p, &c);
    return double(c);
}

struct Reply {
    int turn;
    char dir;
    double cycles;
};

struct FormalAgent : Agent {
    int bot, seat, game;
    HANDLE proc = nullptr, in_w = nullptr, out_r = nullptr;
    std::thread reader;
    std::mutex mu;
    std::condition_variable* cv;
    std::mutex* cv_mu;
    std::deque<Reply> replies;
    bool eof = false;
    double mark = 0;        // 上一次结算用量时的读数（周期）
    double asked_at = 0;    // 本回合发出局面的墙上时刻
    bool waiting = false;

    FormalAgent(int b, int s, int g, std::condition_variable* c, std::mutex* m) : bot(b), seat(s), game(g), cv(c), cv_mu(m) {}

    ~FormalAgent() override { finish(); }

    bool alive_proc() { return proc && !eof; }

    void send(const std::string& text) {
        if (!in_w) return;
        DWORD w = 0;
        if (!WriteFile(in_w, text.data(), DWORD(text.size()), &w, nullptr)) {
            std::lock_guard<std::mutex> lk(mu);
            eof = true;
        }
    }

    void read_loop() {
        std::string buf;
        char chunk[512];
        for (;;) {
            DWORD n = 0;
            if (!ReadFile(out_r, chunk, sizeof chunk, &n, nullptr) || n == 0) break;
            buf.append(chunk, n);
            size_t pos;
            while ((pos = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, pos);
                buf.erase(0, pos + 1);
                const double cyc = process_cycles(proc);
                int t = -1;
                char d = 'X';
                if (std::sscanf(line.c_str(), "%d %c", &t, &d) != 2) t = -2, d = 'X';
                {
                    std::lock_guard<std::mutex> lk(mu);
                    replies.push_back({t, d, cyc});
                }
                std::lock_guard<std::mutex> lk(*cv_mu);
                cv->notify_all();
            }
            if (buf.size() > 100000) break;
        }
        {
            std::lock_guard<std::mutex> lk(mu);
            eof = true;
        }
        std::lock_guard<std::mutex> lk(*cv_mu);
        cv->notify_all();
    }

    void begin(const tron::State& s0) override {
        SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
        HANDLE in_r = nullptr, out_w = nullptr, err_h = nullptr;
        if (!CreatePipe(&in_r, &in_w, &sa, 0) || !CreatePipe(&out_r, &out_w, &sa, 0)) fatal("建管道失败");
        SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
        if (!g_stderr_dir.empty()) {
            const std::wstring p = g_stderr_dir + L"\\g" + std::to_wstring(game) + L"_s" + std::to_wstring(seat) + L".txt";
            err_h = CreateFileW(p.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        } else {
            err_h = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
        }
        // 只让子进程继承这三个句柄：别的局同时在建进程，不能把它们的管道也继承过去
        HANDLE inherit[3] = {in_r, out_w, err_h};
        SIZE_T sz = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &sz);
        std::vector<char> attr_buf(sz);
        auto attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.data());
        InitializeProcThreadAttributeList(attrs, 1, 0, &sz);
        UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof inherit, nullptr, nullptr);
        STARTUPINFOEXW si{};
        si.StartupInfo.cb = sizeof si;
        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        si.StartupInfo.hStdInput = in_r;
        si.StartupInfo.hStdOutput = out_w;
        si.StartupInfo.hStdError = err_h;
        si.lpAttributeList = attrs;
        PROCESS_INFORMATION pi{};
        std::wstring cmd = L"\"" + g_bot_path[size_t(bot)] + L"\"";
        const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                       EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
                                       &si.StartupInfo, &pi);
        DeleteProcThreadAttributeList(attrs);
        CloseHandle(in_r);
        CloseHandle(out_w);
        if (err_h && err_h != INVALID_HANDLE_VALUE) CloseHandle(err_h);
        if (!ok) fatal("启动 bot 程序失败（bot " + std::to_string(bot) + "，错误 " + std::to_string(GetLastError()) + "）");
        if (g_job) AssignProcessToJobObject(g_job, pi.hProcess);
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        proc = pi.hProcess;
        reader = std::thread([this] { read_loop(); });
        mark = 0;    // 首回合从进程启动算起
        std::string head = "TRON " + std::to_string(s0.N) + " " + std::to_string(s0.W) + " " + std::to_string(s0.H) + " " +
                           std::to_string(seat) + " cpu " + std::to_string((long long)g_per_ms) + "\n";
        for (auto& r : tron::map_rows(s0)) head += r + "\n";
        for (int k = 0; k < s0.N; ++k) head += std::to_string(s0.x[k]) + " " + std::to_string(s0.y[k]) + "\n";
        send(head);
    }

    void ask(const tron::State& s, const std::string& prev) override {
        waiting = alive_proc();
        asked_at = now_ms();
        if (waiting) send("T " + std::to_string(s.turn) + " " + (s.turn == 0 ? std::string("-") : prev) + "\n");
    }

    // 本回合的结果：0 = 还在等，1 = 已有结论
    int poll(const tron::State& s, int& dir) {
        if (!waiting) {
            dir = -1;
            return 1;
        }
        const double limit = s.turn == 0 ? tron::BUDGET_FIRST_MS : tron::BUDGET_TURN_MS;
        const double wall = s.turn == 0 ? tron::WALL_FIRST_MS : tron::WALL_TURN_MS;
        std::lock_guard<std::mutex> lk(mu);
        while (!replies.empty()) {
            Reply r = replies.front();
            replies.pop_front();
            if (r.turn != s.turn) {
                if (r.turn == -2) {    // 读不懂的回复：主循环出了问题
                    st.crash_turn = s.turn;
                    eof = true;
                    dir = -1;
                    waiting = false;
                    return 1;
                }
                continue;    // 上一回合迟到的回复
            }
            const double ms = (r.cycles - mark) / g_per_ms;
            mark = r.cycles;
            st.add(s.turn, ms);
            waiting = false;
            if (ms > limit) {
                ++st.timeouts;
                dir = -1;
            } else {
                dir = -1;
                for (int d = 0; d < 4; ++d)
                    if (tron::DIR_CHAR[d] == r.dir) dir = d;
                if (dir < 0) ++st.invalid;
            }
            return 1;
        }
        if (eof) {
            if (st.crash_turn < 0) st.crash_turn = s.turn;
            waiting = false;
            dir = -1;
            return 1;
        }
        const double cyc = process_cycles(proc);
        if ((cyc - mark) / g_per_ms > limit || now_ms() - asked_at > wall) {
            st.add(s.turn, (cyc - mark) / g_per_ms);
            mark = cyc;
            ++st.timeouts;
            waiting = false;
            dir = -1;
            return 1;
        }
        return 0;
    }

    int collect(const tron::State&) override { return -1; }    // 正式模式由 play_game 统一轮询

    void finish() override {
        if (in_w) {
            send("END\n");
            CloseHandle(in_w);
            in_w = nullptr;
        }
        if (proc) {
            if (WaitForSingleObject(proc, 500) != WAIT_OBJECT_0) TerminateProcess(proc, 1);
        }
        if (reader.joinable()) reader.join();
        if (out_r) CloseHandle(out_r), out_r = nullptr;
        if (proc) CloseHandle(proc), proc = nullptr;
    }
};

// -------------------------------------------------------------------- 一局

struct Job {
    int id, n;
    unsigned long long seed;
    std::vector<int> bots;
};

std::string play_game(const Job& job, bool formal, int worker) {
    t_game = job.id;
    tron::State s = tron::new_game(job.n, job.seed);
    if (s.N == 0) fatal("地图生成失败：人数 " + std::to_string(job.n) + " 种子 " + std::to_string(job.seed));
    std::condition_variable cv;
    std::mutex cv_mu;
    std::vector<std::unique_ptr<Agent>> ag;
    for (int k = 0; k < s.N; ++k) {
        if (formal) ag.emplace_back(new FormalAgent(job.bots[size_t(k)], k, job.id, &cv, &cv_mu));
        else ag.emplace_back(new FastAgent(job.bots[size_t(k)], worker, k));
        ag.back()->begin(s);
    }
    std::vector<std::string> moves;
    std::string prev;
    while (!s.over) {
        int mv[tron::MAXN];
        for (int k = 0; k < s.N; ++k) mv[k] = -1;
        if (formal) {
            for (int k = 0; k < s.N; ++k)
                if (s.alive[k]) ag[size_t(k)]->ask(s, prev);
            std::vector<bool> done(size_t(s.N), false);
            for (int k = 0; k < s.N; ++k) done[size_t(k)] = !s.alive[k];
            for (;;) {
                bool all = true;
                for (int k = 0; k < s.N; ++k)
                    if (!done[size_t(k)]) {
                        if (static_cast<FormalAgent*>(ag[size_t(k)].get())->poll(s, mv[k])) done[size_t(k)] = true;
                        else all = false;
                    }
                if (all) break;
                std::unique_lock<std::mutex> lk(cv_mu);
                cv.wait_for(lk, std::chrono::milliseconds(2));
            }
        } else {
            for (int k = 0; k < s.N; ++k)
                if (s.alive[k]) mv[k] = ag[size_t(k)]->collect(s);
        }
        bool was[tron::MAXN];
        for (int k = 0; k < s.N; ++k) was[k] = s.alive[k];
        tron::step(s, mv);
        prev.assign(size_t(s.N), '-');
        for (int k = 0; k < s.N; ++k)
            if (was[k]) prev[size_t(k)] = tron::DIR_CHAR[s.last_dir[k]];
        moves.push_back(prev);
        // 死了的 bot 不再需要：正式模式下马上结束它的进程
        if (formal)
            for (int k = 0; k < s.N; ++k)
                if (was[k] && !s.alive[k]) ag[size_t(k)]->finish();
    }
    for (auto& a : ag) a->finish();

    std::string o = "{\"id\":" + std::to_string(job.id) + ",\"n\":" + std::to_string(s.N) + ",\"seed\":" +
                    std::to_string(job.seed) + ",\"turns\":" + std::to_string(s.turn);
    auto arr = [&](const char* name, auto f) {
        o += ",\"" + std::string(name) + "\":[";
        for (int k = 0; k < s.N; ++k) o += (k ? "," : "") + f(k);
        o += "]";
    };
    auto num = [](double v) {
        char b[32];
        std::snprintf(b, sizeof b, "%.2f", v);
        return std::string(b);
    };
    arr("bots", [&](int k) { return std::to_string(job.bots[size_t(k)]); });
    arr("rank", [&](int k) { return std::to_string(tron::rank_of(s, k)); });
    arr("death_turn", [&](int k) { return std::to_string(s.death_turn[k]); });
    arr("cause", [&](int k) { return json_str(tron::CAUSE_NAME[s.cause[k]]); });
    arr("final", [&](int k) { return "[" + std::to_string(s.x[k]) + "," + std::to_string(s.y[k]) + "]"; });
    arr("first_ms", [&](int k) { return num(ag[size_t(k)]->st.first_ms); });
    arr("avg_ms", [&](int k) { auto& t = ag[size_t(k)]->st; return num(t.turns ? t.sum_ms / t.turns : 0); });
    arr("max_ms", [&](int k) { return num(ag[size_t(k)]->st.max_ms); });
    arr("timeouts", [&](int k) { return std::to_string(ag[size_t(k)]->st.timeouts); });
    arr("invalid", [&](int k) { return std::to_string(ag[size_t(k)]->st.invalid); });
    arr("crash_turn", [&](int k) { return std::to_string(ag[size_t(k)]->st.crash_turn); });
    o += ",\"moves\":[";
    for (size_t i = 0; i < moves.size(); ++i) o += (i ? ",\"" : "\"") + moves[i] + "\"";
    o += "]}";
    return o;
}

// -------------------------------------------------------------------- 命令

int cmd_map(int n, unsigned long long seed) {
    tron::State s = tron::new_game(n, seed);
    if (s.N == 0) {
        std::fprintf(stderr, "人数只能是 2、3、4、12\n");
        return 2;
    }
    std::printf("%d %d %d\n", s.W, s.H, s.N);
    for (auto& r : tron::map_rows(s)) std::printf("%s\n", r.c_str());
    for (int k = 0; k < s.N; ++k) std::printf("%d %d\n", s.x[k], s.y[k]);
    return 0;
}

int cmd_render() {
    int n = 0, upto = 0;
    unsigned long long seed = 0;
    if (std::scanf("%d %llu %d", &n, &seed, &upto) != 3) return 2;
    tron::State s = tron::new_game(n, seed);
    if (s.N == 0) return 2;
    char buf[64];
    for (int t = 0; t < upto && !s.over && std::scanf("%63s", buf) == 1; ++t) {
        int mv[tron::MAXN];
        for (int k = 0; k < s.N; ++k) {
            mv[k] = -1;
            for (int d = 0; d < 4; ++d)
                if (buf[k] == tron::DIR_CHAR[d]) mv[k] = d;
        }
        tron::step(s, mv);
    }
    std::printf("%s", tron::render(s).c_str());
    return 0;
}

struct WorkerArg {
    std::vector<Job>* jobs;
    std::atomic<size_t>* next;
    bool formal;
    int worker;
};

unsigned __stdcall worker_main(void* p) {
    auto* a = static_cast<WorkerArg*>(p);
    for (;;) {
        const size_t i = a->next->fetch_add(1);
        if (i >= a->jobs->size()) break;
        emit(play_game((*a->jobs)[i], a->formal, a->worker));
    }
    return 0;
}

int cmd_run(int argc, char** argv) {
    const bool formal = argc > 2 && std::strcmp(argv[2], "formal") == 0;
    int workers = 1;
    // 路径参数按宽字符取（char 版的 argv 是系统代码页，路径里有中文会坏）
    int wargc = 0;
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    g_tmp = L".";
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--workers")) workers = std::max(1, std::min(64, std::atoi(argv[i + 1])));
        else if (!std::strcmp(argv[i], "--stderr-dir") && wargv && i + 1 < wargc) g_stderr_dir = wargv[i + 1];
        else if (!std::strcmp(argv[i], "--tmp") && wargv && i + 1 < wargc) g_tmp = wargv[i + 1];
    }
    CreateDirectoryW(g_tmp.c_str(), nullptr);
    if (!g_stderr_dir.empty()) CreateDirectoryW(g_stderr_dir.c_str(), nullptr);
    std::vector<Job> jobs;
    char line[8192];
    while (std::fgets(line, sizeof line, stdin)) {
        std::string l(line);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (l.rfind("BOT ", 0) == 0) {
            int idx = 0, off = 0;
            if (std::sscanf(l.c_str(), "BOT %d %n", &idx, &off) < 1 || idx < 0 || idx > 1000) fatal("BOT 行读不懂：" + l);
            if (int(g_bot_path.size()) <= idx) g_bot_path.resize(size_t(idx) + 1);
            g_bot_path[size_t(idx)] = widen(l.substr(size_t(off)));
        } else if (l.rfind("GAME ", 0) == 0) {
            Job j;
            int off = 0;
            if (std::sscanf(l.c_str(), "GAME %d %d %llu %n", &j.id, &j.n, &j.seed, &off) < 3 || !tron::valid_players(j.n))
                fatal("GAME 行读不懂：" + l);
            const char* p = l.c_str() + off;
            for (int k = 0; k < j.n; ++k) {
                int b = 0, used = 0;
                if (std::sscanf(p, "%d%n", &b, &used) != 1 || b < 0 || b >= int(g_bot_path.size()) ||
                    g_bot_path[size_t(b)].empty())
                    fatal("GAME 行的 bot 编号不对：" + l);
                j.bots.push_back(b);
                p += used;
            }
            jobs.push_back(j);
        }
    }
    if (formal) {
        g_job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &info, sizeof info);
    } else {
        SetUnhandledExceptionFilter(crash_filter);
        // 快速模式：bot 的调试输出（标准输出、标准错误）都丢掉
        const int nul = _open("NUL", _O_WRONLY);
        if (nul >= 0) {
            _dup2(nul, 1);
            _dup2(nul, 2);
        }
    }
    for (auto& a : g_in_bot) a = -1;
    std::atomic<size_t> next{0};
    std::vector<WorkerArg> args(static_cast<size_t>(workers), WorkerArg{});
    std::vector<HANDLE> th;
    for (int w = 0; w < workers; ++w) {
        args[size_t(w)] = {&jobs, &next, formal, w};
        th.push_back((HANDLE)_beginthreadex(nullptr, 64u << 20, worker_main, &args[size_t(w)],
                                            STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr));
    }
    // 快速模式的看门狗：一次 decide 超过 60 秒（墙上）就认为卡死
    for (;;) {
        if (WaitForMultipleObjects(DWORD(th.size()), th.data(), TRUE, 500) != WAIT_TIMEOUT) break;
        if (!formal)
            for (int w = 0; w < workers; ++w)
                if (g_in_bot[w] >= 0 && now_ms() - g_in_since[w] > 60000)
                    fatal("快速模式下 bot 卡死：一次 decide 超过 60 秒（工作线程 " + std::to_string(w) + "，座位 " +
                          std::to_string(g_in_bot[w].load()) + "）");
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // 结果输出单独留一个句柄（快速模式会把 1、2 号描述符改到 NUL）
    const int fd = _dup(1);
    if (fd >= 0)
        if (std::FILE* f = _fdopen(fd, "w")) g_out = f;
    _setmode(_fileno(g_out), _O_BINARY);
    if (argc >= 4 && !std::strcmp(argv[1], "map")) return cmd_map(std::atoi(argv[2]), std::strtoull(argv[3], nullptr, 10));
    if (argc >= 2 && !std::strcmp(argv[1], "render")) return cmd_render();
    if (argc >= 3 && !std::strcmp(argv[1], "run")) {
        g_per_ms = measure_per_ms();
        return cmd_run(argc, argv);
    }
    if (argc >= 2 && !std::strcmp(argv[1], "calib")) {
        std::printf("%.0f\n", measure_per_ms());
        return 0;
    }
    std::fprintf(stderr, "用法：judge map <人数> <种子> | judge render | judge run fast|formal [--workers K]\n");
    return 2;
}
