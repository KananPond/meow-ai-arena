// util.cpp  字符串/数字工具 + 地图网格 + BFS 距离场
//
// 性能要点（这些都是基线的主要瓶颈）：
//   * BFS 不再每次 new 一个 vector<vector<int>>：距离场放进固定大小的缓存池，
//     用 stamp 数组代替“每次清零”，lookup 是 O(1)。
//   * 距离场按 (起点, 封锁版本号) 缓存：同一个 tick 内、以及封锁没变的相邻
//     tick 之间可以复用（机器人朝同一个目标走时，目标距离场每 tick 都一样）。
//   * 占据查询 O(1)：网格计数，机器人移动/停机/被救时增量维护。
//   * 充电桩到全场的最近距离用一次“多源 BFS”算好，代替每次 CHG_DIST 都做一遍 BFS。
#include "common.h"

// ---------------------------------------------------------------- 字符串/数字
vector<string> SPLIT(const string& s) {
    vector<string> res;
    string cur;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            if (!cur.empty()) { res.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) res.push_back(cur);
    return res;
}

// 与 atoi 同语义：可选正负号 + 十进制数字，非法/空 → 0
int S2I(const string& s) {
    size_t i = 0;
    int sign = 1;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) {
        if (s[i] == '-') sign = -1;
        i++;
    }
    long long v = 0;
    for (; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') break;
        v = v * 10 + (s[i] - '0');
        if (v > 2147483647LL) v = 2147483647LL;
    }
    return (int)(sign * v);
}

void APPEND_INT(string& out, long long v) {
    char buf[24];
    int n = 0;
    if (v < 0) { out.push_back('-'); v = -v; }
    do { buf[n++] = char('0' + v % 10); v /= 10; } while (v);
    while (n) out.push_back(buf[--n]);
}

string I2S(long long v) {
    string s;
    APPEND_INT(s, v);
    return s;
}

string RNAME(int i) {
    string s = "R";
    APPEND_INT(s, i);
    return s;
}

// ---------------------------------------------------------------- 地图网格
static const int DX4[4] = {0, 1, 0, -1};
static const int DY4[4] = {-1, 0, 1, 0};   // 上 右 下 左  !!!顺序不能改!!!

static int g_N = 0;
static vector<int> g_nb;                 // 4*N：邻居 cell（-1 = 出图）
static vector<unsigned char> g_wall;     // 墙
static vector<unsigned char> g_blocked;  // 被封锁
static vector<unsigned char> g_pass;     // 可通行 = !墙 && !封锁
static vector<int> g_occ;                // 该格上非 DEAD 机器人数
static vector<int> g_chargers;           // 全部充电桩 cell（行优先）
static int g_epoch = 1;

int cellOf(int x, int y) {
    if (x < 0 || y < 0 || x >= W || y >= H) return -1;
    return y * W + x;
}
bool INMAP(int x, int y) { return cellOf(x, y) >= 0; }
bool ISWALL(int x, int y) { return MAP[y][x] == '#'; }
bool OK(int x, int y) {
    int c = cellOf(x, y);
    return c >= 0 && g_pass[c] != 0;
}
bool PASSABLE(int cell) { return cell >= 0 && g_pass[cell] != 0; }
const int* NB4(int cell) { return &g_nb[4 * cell]; }
const vector<int>& allChargerCells() { return g_chargers; }

bool OCC(int cell, int selfCell) {
    int n = g_occ[cell];
    if (selfCell == cell) n--;
    return n > 0;
}
void occAdd(int cell, int delta) { g_occ[cell] += delta; }

static void invalidateCache();   // 定义在下面的 BFS 缓存一节（要用到缓存池）

bool setBlocked(int cell, bool v) {
    if (cell < 0 || g_wall[cell]) return false;                 // 墙不能封
    if ((bool)g_blocked[cell] == v) return false;               // 没变化
    g_blocked[cell] = v ? 1 : 0;
    g_pass[cell] = g_wall[cell] ? 0 : (g_blocked[cell] ? 0 : 1);
    invalidateCache();
    return true;
}
int curEpoch() { return g_epoch; }

// ---------------------------------------------------------------- BFS 场缓存
static vector<Field> g_fields;
static vector<int> g_freeSlots;                 // 空闲槽位
static unordered_map<int, int> g_fieldOfCell;   // cell -> 槽位
static int g_rr = 0;
static vector<int> g_queue;

// 充电桩多源 BFS（一次算好“到最近可通行充电桩”的距离）
static vector<int> g_chgDist, g_chgStamp;
static int g_chgGen = 0, g_chgEpoch = -1;

void gridInit() {
    g_N = W * H;
    g_nb.assign(4 * g_N, -1);
    g_wall.assign(g_N, 0);
    g_blocked.assign(g_N, 0);
    g_pass.assign(g_N, 0);
    g_occ.assign(g_N, 0);
    g_chargers.clear();
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int c = CELL(x, y);
            char ch = MAP[y][x];
            g_wall[c] = (ch == '#') ? 1 : 0;
            g_pass[c] = g_wall[c] ? 0 : 1;
            if (ch == 'C') g_chargers.push_back(c);
            for (int k = 0; k < 4; k++) {
                int nx = x + DX4[k], ny = y + DY4[k];
                g_nb[4 * c + k] = cellOf(nx, ny);
            }
        }
    }
    // 距离场缓存池：总内存控制在 ~64MB 以内，最多 256 个
    // 距离场缓存池：内存预算 ~96MB，槽位数上限 512；机器人少的时候不需要那么多槽位
    long long per = 8LL * g_N;      // dist + stamp
    int cap = (int)((96LL << 20) / (per > 0 ? per : 1));
    if (cap > 512) cap = 512;
    int want = 3 * NR + 128;
    if (cap > want) cap = want;
    if (cap < 32) cap = 32;
    g_fields.assign(cap, Field());
    for (int i = 0; i < cap; i++) {
        g_fields[i].dist.resize(g_N);
        g_fields[i].stamp.assign(g_N, 0);
        g_fields[i].gen = 0;
        g_fields[i].srcCell = -1;
    }
    g_freeSlots.resize(cap);
    for (int i = 0; i < cap; i++) g_freeSlots[i] = i;
    g_fieldOfCell.clear();
    g_fieldOfCell.reserve((size_t)cap * 2);
    g_rr = 0;
    g_queue.resize(g_N);
    g_chgDist.resize(g_N);
    g_chgStamp.assign(g_N, 0);
    g_chgGen = 0;
    g_chgEpoch = -1;
    g_epoch = 1;
}

// 封锁集合变了：所有缓存作废（只是把槽位全部回收，内存不释放）
static void invalidateCache() {
    g_epoch++;
    g_fieldOfCell.clear();
    g_freeSlots.resize(g_fields.size());
    for (size_t i = 0; i < g_fields.size(); i++) {
        g_freeSlots[i] = (int)i;
        g_fields[i].srcCell = -1;
    }
    g_rr = 0;
}

static void bfsInto(Field& f, int src) {
    if (f.gen == INT_MAX) { fill(f.stamp.begin(), f.stamp.end(), 0); f.gen = 0; }
    int gen = ++f.gen;
    int* stamp = f.stamp.data();
    int* dist = f.dist.data();
    int* q = g_queue.data();
    const unsigned char* pass = g_pass.data();
    const int* nb = g_nb.data();
    int head = 0, tail = 0;
    stamp[src] = gen;
    dist[src] = 0;
    q[tail++] = src;
    while (head < tail) {
        int v = q[head++];
        int nd = dist[v] + 1;
        const int* p = nb + 4 * v;
        for (int k = 0; k < 4; k++) {
            int u = p[k];
            if (u < 0 || !pass[u] || stamp[u] == gen) continue;
            stamp[u] = gen;
            dist[u] = nd;
            q[tail++] = u;
        }
    }
    f.srcCell = src;
}

const Field& fieldFrom(int cell) {
    unordered_map<int, int>::iterator it = g_fieldOfCell.find(cell);
    if (it != g_fieldOfCell.end()) return g_fields[it->second];
    int slot;
    if (!g_freeSlots.empty()) {
        slot = g_freeSlots.back();
        g_freeSlots.pop_back();
    } else {
        slot = g_rr;                                  // 轮转淘汰
        g_rr = (g_rr + 1) % (int)g_fields.size();
        g_fieldOfCell.erase(g_fields[slot].srcCell);
    }
    Field& f = g_fields[slot];
    bfsInto(f, cell);
    g_fieldOfCell[cell] = slot;
    return f;
}

// 从所有“可通行充电桩”出发做一次多源 BFS
static void buildChargerField() {
    g_chgEpoch = g_epoch;
    if (g_chgGen == INT_MAX) { fill(g_chgStamp.begin(), g_chgStamp.end(), 0); g_chgGen = 0; }
    int gen = ++g_chgGen;
    int* stamp = g_chgStamp.data();
    int* dist = g_chgDist.data();
    int* q = g_queue.data();
    const unsigned char* pass = g_pass.data();
    const int* nb = g_nb.data();
    int head = 0, tail = 0;
    for (size_t i = 0; i < g_chargers.size(); i++) {
        int c = g_chargers[i];
        if (!pass[c]) continue;
        stamp[c] = gen;
        dist[c] = 0;
        q[tail++] = c;
    }
    while (head < tail) {
        int v = q[head++];
        int nd = dist[v] + 1;
        const int* p = nb + 4 * v;
        for (int k = 0; k < 4; k++) {
            int u = p[k];
            if (u < 0 || !pass[u] || stamp[u] == gen) continue;
            stamp[u] = gen;
            dist[u] = nd;
            q[tail++] = u;
        }
    }
}

// 原 CHG_DIST(x, y)：从 (x,y) 出发（起点无论能不能走都算 0），到最近可通行充电桩的距离。
// 可通行的格子直接用多源场；不可通行的格子 = 1 + min(可通行邻居的多源距离)。
int chargerDistFrom(int x, int y) {
    if (g_chgEpoch != g_epoch) buildChargerField();
    int c = cellOf(x, y);
    if (c < 0) return INF;
    if (g_pass[c]) return g_chgStamp[c] == g_chgGen ? g_chgDist[c] : INF;
    int best = INF;
    const int* p = NB4(c);
    for (int k = 0; k < 4; k++) {
        int u = p[k];
        if (u < 0 || !g_pass[u]) continue;
        int v = (g_chgStamp[u] == g_chgGen) ? g_chgDist[u] : INF;
        if (v < best) best = v;
    }
    return (best >= INF) ? INF : best + 1;
}
