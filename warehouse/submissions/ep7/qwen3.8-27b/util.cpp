// util.cpp  工具函数（重写版）
#include "common.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>

// ---- 建地图相关的静态表 ----
void initChargers() {
    chargers.clear();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (MAP[y][x] == 'C') chargers.push_back(y * W + x);
}

void initPassable() {
    passable.assign(W * H, 0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (MAP[y][x] != '#') passable[y * W + x] = 1;
}

// ---- 扁平 BFS ----
// 与基线 BFS/BFS2 语义一致：起点 d=0（起点本身不要求可通行），
// 只向可通行邻居扩展。结果写入 dist（大小 W*H）。BFS 的最短距离与
// 邻居展开顺序无关，故这里按固定顺序展开即可。
static vector<int> qbuf;
void bfs(int sx, int sy, vector<int>& dist) {
    const int w = W, h = H, total = W * H;
    fill(dist.begin(), dist.end(), BIGNUM);
    qbuf.clear();
    qbuf.reserve(total);
    int s = sy * w + sx;
    dist[s] = 0;
    qbuf.push_back(s);
    size_t head = 0;
    const unsigned char* pas = passable.data();
    while (head < qbuf.size()) {
        int cur = qbuf[head++];
        int cx = cur % w, cy = cur / w;
        int nd = dist[cur] + 1;
        // 上
        if (cy > 0) {
            int ni = cur - w;
            if (pas[ni] && dist[ni] == BIGNUM) { dist[ni] = nd; qbuf.push_back(ni); }
        }
        // 下
        if (cy + 1 < h) {
            int ni = cur + w;
            if (pas[ni] && dist[ni] == BIGNUM) { dist[ni] = nd; qbuf.push_back(ni); }
        }
        // 左
        if (cx > 0) {
            int ni = cur - 1;
            if (pas[ni] && dist[ni] == BIGNUM) { dist[ni] = nd; qbuf.push_back(ni); }
        }
        // 右
        if (cx + 1 < w) {
            int ni = cur + 1;
            if (pas[ni] && dist[ni] == BIGNUM) { dist[ni] = nd; qbuf.push_back(ni); }
        }
    }
}

// ---- 日志 ----
// 行为与基线 writeLog 完全一致：写入 "t msg\n"，并顺带做统计
// （DELIVER 记延迟，LOST/REJECT 计数）。统计逻辑保留在这里，保证计数语义不变。
void writeLog(int t, const string& msg) {
    LOGBUF.append(to_string(t));
    LOGBUF.push_back(' ');
    LOGBUF.append(msg);
    LOGBUF.push_back('\n');
    // 统计（与基线相同：按首词识别事件类型）
    if (msg.rfind("DELIVER ", 0) == 0) {
        size_t q = msg.size();
        while (q > 0 && msg[q - 1] != ' ') q--;
        long long lat = atoll(msg.c_str() + q);
        CNT[0]++;
        CNT[4] += lat;
        if (lat > CNT[5]) CNT[5] = lat;
    } else if (msg.rfind("LOST ", 0) == 0) {
        CNT[1]++;
    } else if (msg.rfind("REJECT ", 0) == 0) {
        CNT[2]++;
    }
}

void writeRaw(const string& msg) {
    LOGBUF.append(msg);
    LOGBUF.push_back('\n');
}
