// grid.cpp --- 地图与 BFS 距离表缓存
// 基线里每次查距离都整张图重跑 BFS；这里按“起点 + 封锁版本”缓存距离表。
// 基线 old_stuff.cpp 的缓存在 BLOCK/UNBLOCK 后不失效，那是它对不上输出的原因；
// 这里用 version 解决。
#include "common.h"

Grid G;

void Grid::init(int w, int h, const vector<string>& m) {
    W = w;
    H = h;
    PW = W + 2;
    PH = H + 2;
    map = m;
    walk.assign(PW * PH, 0);
    blocked.assign(PW * PH, 0);
    wall.assign(PW * PH, 1);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            bool isWall = map[y][x] == '#';
            wall[idx(x, y)] = isWall;
            walk[idx(x, y)] = !isWall;
        }
    off[0] = -PW;
    off[1] = 1;
    off[2] = PW;
    off[3] = -1;
    slotOf.assign(PW * PH, -1);
    // 缓存预算约 300MB，至少 16 个槽、至多 4096 个
    long long per = (long long)PW * PH * 4;
    cap = (int)max(16LL, min(4096LL, 300000000LL / per));
    queue_.resize(PW * PH);
}

void Grid::setBlock(int x, int y) {
    if (!inMap(x, y)) return;
    int i = idx(x, y);
    if (wall[i] || blocked[i]) return;
    blocked[i] = 1;
    walk[i] = 0;
    version++;
}

void Grid::clearBlock(int x, int y) {
    if (!inMap(x, y)) return;
    int i = idx(x, y);
    if (!blocked[i]) return;
    blocked[i] = 0;
    walk[i] = 1;
    version++;
}

const int* Grid::bfs(int src) {
    int s = slotOf[src];
    if (s >= 0 && slots[s].ver == version) return slots[s].d.data();
    if (s < 0) {
        if ((int)slots.size() < cap) {
            slots.emplace_back();
            s = (int)slots.size() - 1;
        } else {
            s = ring;
            ring = (ring + 1) % cap;
            slotOf[slots[s].src] = -1;
        }
        slots[s].src = src;
        slotOf[src] = s;
    }
    Slot& sl = slots[s];
    sl.ver = version;
    sl.d.assign(PW * PH, INF);
    int* d = sl.d.data();
    int* q = queue_.data();
    int head = 0, tail = 0;
    d[src] = 0;
    q[tail++] = src;
    const unsigned char* wk = walk.data();
    while (head < tail) {
        int c = q[head++];
        int nd = d[c] + 1;
        for (int k = 0; k < 4; k++) {
            int n = c + off[k];
            if (!wk[n] || d[n] != INF) continue;
            d[n] = nd;
            q[tail++] = n;
        }
    }
    return d;
}
