// grid.h — 地图、封锁状态与 BFS 距离场缓存
//
// 坐标统一压成格子下标 cell = y * W + x。输入保证地图四周是墙，所以任何非墙格的
// 四个邻居都在图内，BFS 不需要做越界检查（所有 BFS 起点都是非墙格）。
#pragma once
#include <cstdint>
#include <string>
#include <vector>

constexpr int INF_DIST = 1000000000;      // 与基线的 BIGNUM 相同
constexpr uint16_t FIELD_INF = 0xFFFF;    // 距离场里的"不可达"（格子数 ≤ 40000，放得下）

struct Grid {
    int W = 0, H = 0;
    std::vector<uint8_t> wall, charger, blocked, ok;   // ok = 非墙且未封锁（"可通行"）
    std::vector<int> chargers;                         // 所有 'C' 格，行优先
    std::vector<int> chargerManh;                      // 到最近 'C' 的曼哈顿距离（无视墙和封锁，只作下界）
    int off[4] = {0, 0, 0, 0};                          // 上 右 下 左 —— 顺序决定所有平局，不能改

    // 每次封锁状态"真正改变"都记一条，距离场靠它惰性判断自己是否过期
    struct Toggle {
        int cell;
        bool nowBlocked;
    };
    std::vector<Toggle> log;

    void init(int w, int h, const std::vector<std::string>& rows);
    bool inMap(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
    int cell(int x, int y) const { return y * W + x; }
    void block(int x, int y);     // BLOCK 事件语义：图外/墙忽略，已封锁无变化
    void unblock(int x, int y);   // UNBLOCK 事件语义
};

// 距离场缓存。
//  * 单源场 get(src)：从 src 出发 BFS（src 本身无论是否可通行都是起点，之后只走可通行格），
//    与基线 BFS()/BFS2() 完全一致。
//  * 充电桩场 chargerField()：以所有"可通行的充电桩"为源的多源 BFS，用来求
//    "某点到最近可通行充电桩"的距离（基线 CHG_DIST）。
// 每个场记录自己建立时的 log 位置；取用时把之后的封锁变化逐条检查，只有真正会改变这个场
// 的变化才触发重算（见 grid.cpp 的 affects()）。
class FieldCache {
public:
    void init(const Grid* g);
    const uint16_t* get(int src);
    const uint16_t* chargerField();
    const uint16_t* peek(int src);     // 只返回已缓存且仍然正确的场，否则 nullptr（不重算）
    // 移动用：从 target BFS，求 from 的可通行邻居里 dist(target, ·) 最小者（平局按方向顺序），
    // 到达该层即停。不可达返回 -1。与用完整场 get(target) 选出的结果相同。
    int nextStep(int target, int from);
    int pointDist(int src, int dst);   // 单次 dist(src, dst)（dst 须可通行），BFS 到 dst 即停，不缓存

private:
    struct Entry {
        int src = -1;           // -2 表示充电桩多源场
        uint64_t logPos = 0;
        uint64_t lastUse = 0;
        std::vector<uint16_t> d;
    };
    const Grid* g = nullptr;
    std::vector<Entry> entries;
    std::vector<int> slotOf;    // cell -> entries 下标，-1 = 未缓存
    size_t capacity = 0;
    uint64_t useClock = 0;
    Entry chg;
    std::vector<int> queue, stamp, dist;
    int stampNow = 0;

    bool affects(const Entry& e, const Grid::Toggle& tg) const;
    void refresh(Entry& e);     // 校验/重算
    void build(Entry& e);
};
