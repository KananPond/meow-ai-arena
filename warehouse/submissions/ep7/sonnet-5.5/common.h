// common.h --- 仓储机器人调度仿真：公共声明
// 行为以 baseline 为准（SPEC.md 有若干过时之处，见 HANDOFF.md 的“怪癖清单”）。
#ifndef COMMON_H_WMS
#define COMMON_H_WMS

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

constexpr int INF = 1000000000;   // “到不了”的距离（与基线 BIGNUM 相同）

enum State { IDLE, TO_PICKUP, DELIVERING, TO_CHARGER, CHARGING, DEAD };
enum OStatus { PENDING, ASSIGNED, PICKED, DONE, CANCELLED, LOST };

// ---------------- grid.cpp：地图 + 带缓存的 BFS ----------------
// 格子下标 = (y+1)*PW + (x+1)，四周多一圈不可走的哨兵，BFS 内部无需判界。
struct Grid {
    int W = 0, H = 0, PW = 0, PH = 0;
    vector<string> map;          // 原始地图 map[y][x]
    vector<unsigned char> walk;  // 1 = 可通行（图内、非墙、未封锁）
    vector<unsigned char> blocked;
    vector<unsigned char> wall;
    int version = 0;             // 封锁集合每次真正变化就 +1，用来让 BFS 缓存失效
    int off[4];                  // 上 右 下 左 的下标偏移（方向顺序不能改）

    void init(int w, int h, const vector<string>& m);
    int idx(int x, int y) const { return (y + 1) * PW + (x + 1); }
    bool inMap(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
    bool ok(int x, int y) const { return inMap(x, y) && walk[idx(x, y)]; }
    void setBlock(int x, int y);    // BLOCK 事件（图外/墙无效）
    void clearBlock(int x, int y);  // UNBLOCK 事件
    // 以 src 为起点的 BFS 距离表（起点无论是否可通行都是 0），不可达为 INF。
    // 返回的指针只在随后少量 bfs() 调用内有效（环形缓存，容量 >= 16）。
    const int* bfs(int src);

private:
    struct Slot {
        vector<int> d;
        int src = -1, ver = -1;
    };
    vector<Slot> slots;
    vector<int> slotOf;  // 格子 -> 槽位
    int ring = 0, cap = 0;
    vector<int> queue_;
};
extern Grid G;

// ---------------- sim.cpp ----------------
void runAll(const string& input, string& out);

#endif
