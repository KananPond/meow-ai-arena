// util.cpp  工具函数（优化版）
// 主要改动：
//   1) OK()/INMAP()/ISWALL() 全部内联，BLK 改为布尔数组 blockedIdx（O(1)）
//   2) OCC 改为增量维护占用计数 g_occ（O(1)），不再每次全表扫描
//   3) BFS 改为扁平 uint16 距离表 + 按 (起点, blkVersion) 缓存；命中 O(1)，miss 才重算
//   4) CHG_DIST/CHG_NEAR 复用同一个 BFS 缓存与静态充电桩列表
#include "common.h"

int DX4[4] = {0, 1, 0, -1};
int DY4[4] = {-1, 0, 1, 0};   // 上 右 下 左  !!!顺序不能改!!!

vector<string> SPLIT(const string& s) {
    vector<string> res;
    string cur;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            if (!cur.empty()) { res.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) res.push_back(cur);
    return res;
}

int S2I(const string& s) { return atoi(s.c_str()); }
string I2S(long long v) { return std::to_string(v); }

string RNAME(int i) { return "R" + std::to_string(i); }

bool INMAP(int x, int y) {
    return (unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H;
}
bool ISWALL(int x, int y) { return MAP[y][x] == '#'; }

bool OK(int x, int y) {
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return false;
    size_t idx = (size_t)y * W + x;
    return MAP[y][x] != '#' && !blockedIdx[idx];
}

// ---------- 占用计数（替代 OCC 全表扫描） ----------
static vector<unsigned short> g_occ;

void occInit() {
    g_occ.assign((size_t)W * H, 0);
    for (int i = 0; i < NR; i++) ++g_occ[(size_t)RY[i] * W + RX[i]];
}
void occAdd(int i) { ++g_occ[(size_t)RY[i] * W + RX[i]]; }
void occSub(int i) { --g_occ[(size_t)RY[i] * W + RX[i]]; }

bool OCC(int x, int y, int except) {
    size_t idx = (size_t)y * W + x;
    unsigned short c = g_occ[idx];
    if (c == 0) return false;
    if (except >= 0 && RST[except] != "DEAD" && RX[except] == x && RY[except] == y)
        return c > 1;
    return true;
}

// ---------- BFS（扁平 + 缓存） ----------
static unordered_map<int, pair<int, vector<unsigned short>>> g_bfs;
static const int BFS_CAP = 2000;   // 每个距离表 H*W*2 字节；2000 张 = 200*200 地图上限约 160MB

const vector<unsigned short>& bfsRef(int sx, int sy) {
    int key = sy * W + sx;
    auto it = g_bfs.find(key);
    if (it != g_bfs.end() && it->second.first == blkVersion) return it->second.second;

    int N = W * H;
    vector<unsigned short> d((size_t)N, INF16);
    vector<int> q;
    q.reserve(N);
    d[(size_t)sy * W + sx] = 0;
    q.push_back(key);
    size_t head = 0;
    while (head < q.size()) {
        int cur = q[head++];
        unsigned short cd = d[cur];   // d[cur] < 65535，+1 不会溢出
        int cx = cur % W, cy = cur / W;
        if (cy > 0) {
            int nb = cur - W;
            if (MAP[cy - 1][cx] != '#' && !blockedIdx[nb] && d[nb] == INF16) {
                d[nb] = cd + 1; q.push_back(nb);
            }
        }
        if (cx + 1 < W) {
            int nb = cur + 1;
            if (MAP[cy][cx + 1] != '#' && !blockedIdx[nb] && d[nb] == INF16) {
                d[nb] = cd + 1; q.push_back(nb);
            }
        }
        if (cy + 1 < H) {
            int nb = cur + W;
            if (MAP[cy + 1][cx] != '#' && !blockedIdx[nb] && d[nb] == INF16) {
                d[nb] = cd + 1; q.push_back(nb);
            }
        }
        if (cx > 0) {
            int nb = cur - 1;
            if (MAP[cy][cx - 1] != '#' && !blockedIdx[nb] && d[nb] == INF16) {
                d[nb] = cd + 1; q.push_back(nb);
            }
        }
    }
    if ((int)g_bfs.size() >= BFS_CAP) g_bfs.clear();
    auto pr = g_bfs.insert_or_assign(key, make_pair(blkVersion, std::move(d)));
    return pr.first->second.second;
}

// 等价原 DIST(ax,ay,bx,by)
int distTo(int sx, int sy, int tx, int ty) {
    if (sx == tx && sy == ty) return 0;
    if (!OK(tx, ty)) return BIGNUM;
    const vector<unsigned short>& d = bfsRef(sx, sy);
    unsigned short v = d[(size_t)ty * W + tx];
    return v == INF16 ? BIGNUM : (int)v;
}

// ---------- 充电桩静态列表 ----------
static vector<pair<int, int>> g_chg;
static bool g_chgReady = false;

const vector<pair<int, int>>& allChargers() {
    if (!g_chgReady) {
        g_chgReady = true;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (MAP[y][x] == 'C') g_chg.emplace_back(x, y);
    }
    return g_chg;
}

// 等价原 CHG_DIST(x,y)：到最近可用充电桩的距离（不可达返回 BIGNUM）
int chgDist(int x, int y) {
    const vector<unsigned short>& d = bfsRef(x, y);
    const auto& cs = allChargers();
    int best = BIGNUM;
    for (const auto& p : cs) {
        if (!OK(p.first, p.second)) continue;
        unsigned short v = d[(size_t)p.second * W + p.first];
        if (v == INF16) continue;
        if ((int)v < best) best = v;
    }
    return best;
}

// 等价原 CHG_NEAR(x,y)：最近可用充电桩（无可达返回 (-1,-1)）
pair<int, int> chgNear(int x, int y) {
    const vector<unsigned short>& d = bfsRef(x, y);
    const auto& cs = allChargers();
    int best = BIGNUM;
    pair<int, int> res(-1, -1);
    for (const auto& p : cs) {
        if (!OK(p.first, p.second)) continue;
        unsigned short v = d[(size_t)p.second * W + p.first];
        if (v == INF16) continue;
        if ((int)v < best) { best = v; res = p; }
    }
    return res;
}

int findRobot(const string& name) {
    if (name.empty() || name[0] != 'R') return -1;
    int v = 0;
    for (size_t i = 1; i < name.size(); i++) {
        char c = name[i];
        if (c < '0' || c > '9') return -1;
        v = v * 10 + (c - '0');
    }
    return (v >= 0 && v < NR) ? v : -1;
}

// 写日志（不再整行 SPLIT，用前缀判断统计）
void writeLog(int t, const string& msg) {
    LOGBUF.push_back(I2S(t) + " " + msg);
    if (msg.size() >= 7 && msg.compare(0, 7, "DELIVER") == 0) {
        size_t sp = msg.rfind(' ');
        long long lat = atoll(msg.c_str() + sp + 1);
        CNT[0]++;
        CNT[4] += lat;
        if (lat > CNT[5]) CNT[5] = lat;
    } else if (msg.size() >= 4 && msg.compare(0, 4, "LOST") == 0) {
        CNT[1]++;
    } else if (msg.size() >= 6 && msg.compare(0, 6, "REJECT") == 0) {
        CNT[2]++;
    }
}

void writeRaw(const string& msg) { LOGBUF.push_back(msg); }

void dumpLog() {
    for (size_t i = 0; i < LOGBUF.size(); i++) cout << LOGBUF[i] << "\n";
}
