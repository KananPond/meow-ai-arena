// grid.cpp — 地图与距离场缓存实现
#include "grid.h"

void Grid::init(int w, int h, const std::vector<std::string>& rows) {
    W = w;
    H = h;
    int n = W * H;
    wall.assign(n, 0);
    charger.assign(n, 0);
    blocked.assign(n, 0);
    ok.assign(n, 0);
    chargers.clear();
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int c = cell(x, y);
            char ch = rows[y][x];
            wall[c] = (ch == '#');
            charger[c] = (ch == 'C');
            ok[c] = !wall[c];
            if (charger[c]) chargers.push_back(c);
        }
    }
    // 曼哈顿到最近充电桩：在整个矩形上做多源 BFS
    chargerManh.assign(n, INF_DIST);
    std::vector<int> q(chargers);
    for (int c : chargers) chargerManh[c] = 0;
    for (size_t h = 0; h < q.size(); h++) {
        int c = q[h], x = c % W, y = c / W;
        const int nx[4] = {x, x + 1, x, x - 1}, ny[4] = {y - 1, y, y + 1, y};
        for (int k = 0; k < 4; k++) {
            if (!inMap(nx[k], ny[k])) continue;
            int m = cell(nx[k], ny[k]);
            if (chargerManh[m] != INF_DIST) continue;
            chargerManh[m] = chargerManh[c] + 1;
            q.push_back(m);
        }
    }
    off[0] = -W;
    off[1] = 1;
    off[2] = W;
    off[3] = -1;
}

void Grid::block(int x, int y) {
    if (!inMap(x, y)) return;
    int c = cell(x, y);
    if (wall[c] || blocked[c]) return;
    blocked[c] = 1;
    ok[c] = 0;
    log.push_back({c, true});
}

void Grid::unblock(int x, int y) {
    if (!inMap(x, y)) return;
    int c = cell(x, y);
    if (!blocked[c]) return;
    blocked[c] = 0;
    ok[c] = 1;
    log.push_back({c, false});
}

// ---------------------------------------------------------------------------

void FieldCache::init(const Grid* grid) {
    g = grid;
    size_t cells = (size_t)g->W * g->H;
    // 最多用约 300MB 存距离场（200x200 时约 3750 个）
    capacity = (300u << 20) / (cells * sizeof(uint16_t));
    if (capacity < 16) capacity = 16;
    if (capacity > 8192) capacity = 8192;
    slotOf.assign(cells, -1);
    queue.resize(cells);
    stamp.assign(cells, 0);
    dist.assign(cells, 0);
    chg.src = -2;
}

// 某次封锁变化会不会改变场 e（在 e 当前内容仍然正确的前提下判断）：
//  * 封锁 X：只有 X 原本可达（距离有限）时才变；
//  * 解封 X：X 是充电桩（对多源场而言 X 变成新源），或 X 有可达的邻居时才变；
//  * 单源场的源本身被封/解封不影响场（源无论如何都是起点）。
bool FieldCache::affects(const Entry& e, const Grid::Toggle& tg) const {
    if (tg.cell == e.src) return false;
    const uint16_t* d = e.d.data();
    if (tg.nowBlocked) return d[tg.cell] != FIELD_INF;
    if (e.src == -2 && g->charger[tg.cell]) return true;
    for (int k = 0; k < 4; k++)
        if (d[tg.cell + g->off[k]] != FIELD_INF) return true;
    return false;
}

void FieldCache::refresh(Entry& e) {
    uint64_t end = g->log.size();
    if (e.logPos == end) return;
    // 变化太多时逐条检查不如直接重算
    bool stale = end - e.logPos > 4096;
    for (uint64_t i = e.logPos; !stale && i < end; i++) stale = affects(e, g->log[i]);
    if (stale) build(e);
    e.logPos = end;
}

void FieldCache::build(Entry& e) {
    const size_t cells = (size_t)g->W * g->H;
    e.d.assign(cells, FIELD_INF);
    uint16_t* d = e.d.data();
    const uint8_t* ok = g->ok.data();
    int head = 0, tail = 0;
    if (e.src >= 0) {
        d[e.src] = 0;
        queue[tail++] = e.src;
    } else {
        for (int c : g->chargers)
            if (ok[c]) {
                d[c] = 0;
                queue[tail++] = c;
            }
    }
    const int o0 = g->off[0], o1 = g->off[1], o2 = g->off[2], o3 = g->off[3];
    while (head < tail) {
        int c = queue[head++];
        uint16_t nd = d[c] + 1;
        int n;
        n = c + o0; if (ok[n] && d[n] == FIELD_INF) { d[n] = nd; queue[tail++] = n; }
        n = c + o1; if (ok[n] && d[n] == FIELD_INF) { d[n] = nd; queue[tail++] = n; }
        n = c + o2; if (ok[n] && d[n] == FIELD_INF) { d[n] = nd; queue[tail++] = n; }
        n = c + o3; if (ok[n] && d[n] == FIELD_INF) { d[n] = nd; queue[tail++] = n; }
    }
    e.logPos = g->log.size();
}

const uint16_t* FieldCache::get(int src) {
    int s = slotOf[src];
    if (s < 0) {
        if (entries.size() < capacity) {
            s = (int)entries.size();
            entries.emplace_back();
        } else {   // 淘汰最久未用的
            s = 0;
            for (size_t i = 1; i < entries.size(); i++)
                if (entries[i].lastUse < entries[s].lastUse) s = (int)i;
            slotOf[entries[s].src] = -1;
        }
        slotOf[src] = s;
        entries[s].src = src;
        build(entries[s]);
    } else {
        refresh(entries[s]);
    }
    entries[s].lastUse = ++useClock;
    return entries[s].d.data();
}

const uint16_t* FieldCache::chargerField() {
    if (chg.d.empty()) build(chg);
    else refresh(chg);
    return chg.d.data();
}

int FieldCache::pointDist(int src, int dst) {
    if (src == dst) return 0;
    ++stampNow;
    const uint8_t* ok = g->ok.data();
    int head = 0, tail = 0;
    queue[tail++] = src;
    stamp[src] = stampNow;
    dist[src] = 0;
    while (head < tail) {
        int c = queue[head++];
        for (int k = 0; k < 4; k++) {
            int n = c + g->off[k];
            if (!ok[n] || stamp[n] == stampNow) continue;
            stamp[n] = stampNow;
            dist[n] = dist[c] + 1;
            if (n == dst) return dist[n];
            queue[tail++] = n;
        }
    }
    return INF_DIST;
}

const uint16_t* FieldCache::peek(int src) {
    int s = slotOf[src];
    if (s < 0) return nullptr;
    Entry& e = entries[s];
    uint64_t end = g->log.size();
    for (uint64_t i = e.logPos; i < end; i++)
        if (end - e.logPos > 4096 || affects(e, g->log[i])) return nullptr;
    e.logPos = end;
    e.lastUse = ++useClock;
    return e.d.data();
}

int FieldCache::nextStep(int target, int from) {
    const uint8_t* ok = g->ok.data();
    ++stampNow;
    int head = 0, tail = 0;
    queue[tail++] = target;
    stamp[target] = stampNow;
    dist[target] = 0;
    int nb[4], nbCount = 0;
    for (int k = 0; k < 4; k++) {
        int n = from + g->off[k];
        if (ok[n]) nb[nbCount++] = n;
    }
    if (nbCount == 0) return -1;
    int found = INF_DIST;   // 已发现的邻居里最小的距离
    for (int k = 0; k < nbCount; k++)
        if (nb[k] == target) found = 0;
    // 队首距离 ≥ found 时，距离为 found 的格子都已被发现，可以停
    while (head < tail && dist[queue[head]] < found) {
        int c = queue[head++];
        for (int k = 0; k < 4; k++) {
            int n = c + g->off[k];
            if (!ok[n] || stamp[n] == stampNow) continue;
            stamp[n] = stampNow;
            dist[n] = dist[c] + 1;
            queue[tail++] = n;
            for (int j = 0; j < nbCount; j++)
                if (nb[j] == n && dist[n] < found) found = dist[n];
        }
    }
    if (found == INF_DIST) return -1;
    for (int k = 0; k < nbCount; k++)
        if (stamp[nb[k]] == stampNow && dist[nb[k]] == found) return nb[k];
    return -1;
}
