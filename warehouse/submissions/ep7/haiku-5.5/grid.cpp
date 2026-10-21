// grid.cpp  地图、封锁状态与 BFS 距离
#include "grid.h"

namespace {
// 每张距离表约 W*H*2 字节（200x200 时约 80KB），2048 张约 160MB，远低于 1GB 上限
const size_t kMaxCachedTables = 2048;
}  // namespace

Grid::Grid(int w, int h, const std::vector<std::string>& rows)
    : w_(w), h_(h), rows_(rows), open_(w * h, 0), isCharger_(w * h, 0), slot_(w * h, -1) {
    for (int y = 0; y < h_; y++) {
        for (int x = 0; x < w_; x++) {
            if (rows_[y][x] != '#') open_[index(x, y)] = 1;
            if (rows_[y][x] == 'C') {
                chargers_.push_back(index(x, y));
                isCharger_[index(x, y)] = 1;
            }
        }
    }
}

bool Grid::setBlocked(int x, int y, bool on) {
    if (!inMap(x, y) || isWall(x, y)) return false;
    int i = index(x, y);
    bool blockedNow = !open_[i];
    if (blockedNow == on) return false;
    open_[i] = on ? 0 : 1;
    resetCache();
    version_++;
    return true;
}

const uint16_t* Grid::distFrom(int sx, int sy) {
    int i = index(sx, sy);
    if (slot_[i] < 0) {
        pool_.emplace_back();
        bfs(sx, sy, pool_.back());
        slot_[i] = static_cast<int>(pool_.size()) - 1;
        cached_.push_back(i);
    }
    return pool_[slot_[i]].data();
}

void Grid::bfs(int sx, int sy, std::vector<uint16_t>& out) const {
    out.assign(w_ * h_, INF16);
    std::vector<int> queue;
    queue.reserve(w_ * h_);
    out[index(sx, sy)] = 0;
    queue.push_back(index(sx, sy));
    for (size_t head = 0; head < queue.size(); head++) {
        int cur = queue[head];
        int cx = cur % w_, cy = cur / w_;
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX4[k], ny = cy + DY4[k];
            if (!isOK(nx, ny)) continue;
            int ni = index(nx, ny);
            if (out[ni] != INF16) continue;
            out[ni] = static_cast<uint16_t>(out[cur] + 1);
            queue.push_back(ni);
        }
    }
}

int Grid::distUntil(int sx, int sy, int target) const {
    // BFS 按距离非递减地发现格子，所以第一次发现目标时的距离就是最短距离
    auto isGoal = [&](int i) { return target >= 0 ? i == target : isCharger_[i] != 0; };
    int start = index(sx, sy);
    // 起点本身不一定可通行（机器人可能站在封锁格上），但充电桩和目标格都必须可通行
    if (isGoal(start) && isOK(sx, sy)) return 0;
    std::vector<int> dist(w_ * h_, -1);
    std::vector<int> queue;
    queue.reserve(w_ * h_);
    dist[start] = 0;
    queue.push_back(start);
    for (size_t head = 0; head < queue.size(); head++) {
        int cur = queue[head];
        int cx = cur % w_, cy = cur / w_;
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX4[k], ny = cy + DY4[k];
            if (!isOK(nx, ny)) continue;
            int ni = index(nx, ny);
            if (dist[ni] >= 0) continue;
            dist[ni] = dist[cur] + 1;
            if (isGoal(ni)) return dist[ni];
            queue.push_back(ni);
        }
    }
    return BIGNUM;
}

void Grid::trimCache() {
    if (pool_.size() >= kMaxCachedTables) resetCache();
}

void Grid::resetCache() {
    for (int i : cached_) slot_[i] = -1;
    cached_.clear();
    pool_.clear();
}
