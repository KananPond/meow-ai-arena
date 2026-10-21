// grid.h  地图、封锁状态与 BFS 距离
//
// 距离语义与基线 BFS 完全一致：起点不论能否通行都作为起点，之后只经过可通行格；
// 到不了记为不可达。距离表按起点缓存，封锁状态改变时整体失效。
#ifndef WMS_GRID_H
#define WMS_GRID_H

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

// 方向顺序：上、右、下、左。所有"按方向"的平局都依赖这个顺序，不能改。
inline constexpr int DX4[4] = {0, 1, 0, -1};
inline constexpr int DY4[4] = {-1, 0, 1, 0};

// 不可达距离（与基线 BIGNUM 取值相同）
inline constexpr int BIGNUM = 1000000000;
// 距离表用 uint16 存储，INF16 表示不可达。正式地图最大 200x200，真实距离远小于 65535。
inline constexpr uint16_t INF16 = 65535;

inline int toInt(uint16_t d) { return d == INF16 ? BIGNUM : d; }

class Grid {
public:
    Grid() = default;
    Grid(int w, int h, const std::vector<std::string>& rows);

    int W() const { return w_; }
    int H() const { return h_; }
    int index(int x, int y) const { return y * w_ + x; }
    bool inMap(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    bool isWall(int x, int y) const { return rows_[y][x] == '#'; }  // 调用前须保证在图内

    // 能否通行：在图内、不是墙、没有被封锁
    bool isOK(int x, int y) const { return inMap(x, y) && open_[index(x, y)]; }

    // BLOCK / UNBLOCK。返回封锁状态是否真的发生了变化。墙和图外的格子不受影响。
    bool setBlocked(int x, int y, bool on);

    // 每次封锁状态变化都会递增，用来判断派单代价缓存是否过期
    int version() const { return version_; }

    // 所有充电桩的格子下标，行优先
    const std::vector<int>& chargers() const { return chargers_; }

    // 从 (sx, sy) 出发的距离表（带缓存）。返回的指针在下一次 trimCache() 或封锁变化前有效。
    const uint16_t* distFrom(int sx, int sy);

    // 不经缓存，把从 (sx, sy) 出发的距离表写入 out
    void bfs(int sx, int sy, std::vector<uint16_t>& out) const;

    // 不经缓存，从 (sx, sy) 出发搜索到目标格的距离，找到就停止。
    // target 为格子下标；target 为 -1 时目标是任意可通行的充电桩。不可达返回 BIGNUM。
    int distUntil(int sx, int sy, int target) const;

    // 缓存表过多时清空。只能在没有持有距离表指针的时候调用（每 tick 开头）。
    void trimCache();

private:
    void resetCache();

    int w_ = 0, h_ = 0;
    std::vector<std::string> rows_;
    std::vector<char> open_;         // 可通行（非墙且未封锁）
    std::vector<char> isCharger_;    // 是否为充电桩
    std::vector<int> chargers_;
    int version_ = 0;

    std::deque<std::vector<uint16_t>> pool_;  // 距离表；deque 保证 push_back 不会使已有引用失效
    std::vector<int> slot_;                   // 格子下标 -> pool_ 下标，-1 表示未缓存
    std::vector<int> cached_;                 // 已缓存的格子，用于清空
};

#endif
