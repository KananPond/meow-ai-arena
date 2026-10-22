// util.cpp  工具函数
#include "common.h"
#include <algorithm>
#include <deque>
#include <queue>
#include <functional>
#include <unordered_map>
#include <array>

int DX4[4] = {0, 1, 0, -1};
int DY4[4] = {-1, 0, 1, 0};   // 上 右 下 左  !!!顺序不能改!!!

vector<string> SPLIT(const string& s) {
    vector<string> res;
    string cur = "";
    for (int i = 0; i < (int)s.size(); i++) {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            if (cur != "") {
                res.push_back(cur);
                cur = "";
            }
        } else {
            cur = cur + c;
        }
    }
    if (cur != "") res.push_back(cur);
    return res;
}

int S2I(const string& s) {
    return atoi(s.c_str());
}

string I2S(long long v) {
    stringstream ss;
    ss << v;
    return ss.str();
}

string RNAME(int i) {
    return "R" + I2S(i);
}

bool INMAP(int x, int y) {
    if (x < 0) return false;
    if (y < 0) return false;
    if (x >= W) return false;
    if (y >= H) return false;
    return true;
}

bool ISWALL(int x, int y) {
    return MAP[y][x] == '#';
}

// 能不能走（在图内 不是墙 没封）
// [P15] OK() 是所有 BFS 的内循环，原来每个格子要查一次 std::set（红黑树）。
// 改成一张平铺字节图：可走=1，墙/封锁=0。封锁/解锁时同步维护。
static vector<char> g_ok;
// [P16] 预计算四邻居表：BFS 里每格原来要算两次整数除法（%W、/W）再做四次边界检查。
// 表建好了就只剩一次数组访存。
static std::vector<std::array<int, 4> > g_nb;
static std::vector<char> g_wall;   // 只有墙，不含封路：势函数要它不随封路变
void OK_INIT() {
    g_ok.assign((size_t)W * H, 0);
    g_wall.assign((size_t)W * H, 0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            g_ok[(size_t)y * W + x] = (MAP[y][x] != '#') ? 1 : 0;
            g_wall[(size_t)y * W + x] = (MAP[y][x] == '#') ? 1 : 0;
        }
    g_nb.assign((size_t)W * H, std::array<int, 4>());
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int t = y - 1, b = y + 1, l = x - 1, r = x + 1;
            std::array<int, 4>& a = g_nb[(size_t)y * W + x];
            a[0] = (t >= 0) ? t * W + x : -1;
            a[1] = (r < W) ? y * W + r : -1;
            a[2] = (b < H) ? b * W + x : -1;
            a[3] = (l >= 0) ? y * W + l : -1;
        }
    }
}
void OK_SET_BLK(int x, int y, bool blocked) {
    if (!INMAP(x, y)) return;
    if (g_ok.size() != (size_t)W * H) OK_INIT();
    g_ok[(size_t)y * W + x] = blocked ? 0 : ((MAP[y][x] != '#') ? 1 : 0);
}
bool OK(int x, int y) {
    if (!INMAP(x, y)) return false;
    if (g_ok.size() != (size_t)W * H) OK_INIT();   // 保底
    return g_ok[(size_t)y * W + x] != 0;
}

// 格子上有没有别的车（死了的不算）
bool OCC(int x, int y, int except) {
    for (int i = 0; i < NR; i++) {
        if (i == except) continue;
        if (RST[i] == "DEAD") continue;
        if (RX[i] == x && RY[i] == y) return true;
    }
    return false;
}

// BFS 距离表  d[y][x]   起点不管能不能走都是 0
vector<vector<int> > BFS(int sx, int sy) {
#if FLAG_FAST_BFS
    return bfs_fast_cached(sx, sy);
#endif
    vector<vector<int> > d;
    for (int y = 0; y < H; y++) {
        vector<int> row;
        for (int x = 0; x < W; x++) row.push_back(BIGNUM);
        d.push_back(row);
    }
    queue<pair<int, int> > q;
    d[sy][sx] = 0;
    q.push(make_pair(sx, sy));
    while (q.size() > 0) {
        pair<int, int> p = q.front();
        q.pop();
        for (int k = 0; k < 4; k++) {
            int nx = p.first + DX4[k];
            int ny = p.second + DY4[k];
            if (OK(nx, ny) == false) continue;
            if (d[ny][nx] != BIGNUM) continue;
            d[ny][nx] = d[p.second][p.first] + 1;
            q.push(make_pair(nx, ny));
        }
    }
    return d;
}

// [P10] 不缓存的一维距离场：起点每 tick 都变（机器人位置），缓存只会膨胀；这里只把每次 BFS 的分配省掉
const vector<int>& BFS_FLAT_SCRATCH(int sx, int sy) {
    static vector<int> d, q;
    d.assign((size_t)W * H, BIGNUM);
    q.clear();
    d[(size_t)sy * W + sx] = 0;
    q.push_back(sy * W + sx);
    for (size_t head = 0; head < q.size(); ++head) {
        int cur = q[head];
        int cx = cur % W, cy = cur / W;
        int nd = d[cur] + 1;
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX4[k], ny = cy + DY4[k];
            if (!OK(nx, ny)) continue;
            size_t idx = (size_t)ny * W + nx;
            if (d[idx] < BIGNUM) continue;
            d[idx] = nd;
            q.push_back((int)idx);
        }
    }
    return d;
}


static std::unordered_map<long long, vector<int> > g_b3;   // [P16]
static long long g_b3ver = -1, g_b3cells = 0;

// [P5] 一维距离场 + 数组队列：免掉每次 BFS 的 37 个 vector 分配和 list 的逐节点 malloc
const vector<int>& BFS2_FLAT(int sx, int sy) {
    if (g_b3ver != BLK_VER) { g_b3.clear(); g_b3ver = BLK_VER; g_b3cells = 0; }
    std::unordered_map<long long, vector<int> >::iterator hit = g_b3.find((long long)sx * 100000 + sy);   // [P16]
    if (hit != g_b3.end()) return hit->second;
    static vector<int> q;
    vector<int> d((size_t)W * H, BIGNUM);
    q.clear();
    size_t start = (size_t)sy * W + sx;
    if (start >= d.size()) {   // [P18] 起点不可走也要从它走：基线写的是「起点不管能不能走都是 0」
        g_b3cells += (long long)W * H;
        return g_b3[(long long)sx * 100000 + sy] = std::move(d);
    }
    d[start] = 0;
    q.push_back((int)start);
    for (size_t head = 0; head < q.size(); ++head) {
        int cur = q[head];
        int nd = d[cur] + 1;
        const std::array<int, 4>& nb = g_nb[cur];
        for (int k = 0; k < 4; k++) {
            int idx = nb[k];
            if (idx < 0 || !g_ok[idx] || d[idx] < BIGNUM) continue;
            d[idx] = nd;
            q.push_back(idx);
        }
    }
    g_b3cells += (long long)W * H;
    return g_b3[(long long)sx * 100000 + sy] = std::move(d);
}

// [P12] 清表只能放在 tick 边界：在函数度里 clear 会把调用方手里拿着的
// vector<int>& （比如派单里同时持有 fa 和 fb）变成野指针。
// 超上限就不插了不行（调用方要求返回引用），所以改成“下个 tick 开头再清”。
void BFS2_PRUNE_AT_TICK() {
    if (g_b3cells > 60000000LL) { g_b3.clear(); g_b3cells = 0; }
}

// ============================ [P20] 0/1 重标权 ============================
// 势函数 h(v) = 永久墙图里 v 到目标的精确距离；动态封路只删点，h 不变。
// 四邻格图是二部图 ⇒ 相邻格 h 差是奇数；又 |差| <= 1 ⇒ 差恰为 ±1。于是
//     w'(u,v) = (1 + h(v) - h(u)) / 2  ∈ {0,1}
// 沿任意路径把 h 差望远镜求和：路径长度 = h(s) + 2 * Σw'。
// 所以"最短绕路"就是 0/1 最短路，且 dist(v) = h(v) + 2*k(v) 直接读回真实距离。
static std::map<long long, vector<int> > g_h;

const vector<int>& STATIC_FIELD(int tx, int ty) {
    long long key = (long long)ty * W + tx;
    std::map<long long, vector<int> >::iterator it = g_h.find(key);
    if (it != g_h.end()) return it->second;
    if (g_h.size() > 64) g_h.clear();
    static vector<int> qs;
    vector<int> d((size_t)W * H, BIGNUM);
    qs.clear();
    size_t s0 = (size_t)ty * W + tx;
    if (s0 < d.size()) {
        d[s0] = 0;
        qs.push_back((int)s0);
        for (size_t head = 0; head < qs.size(); ++head) {
            int cur = qs[head];
            int nd = d[cur] + 1;
            const std::array<int, 4>& nb = g_nb[cur];
            for (int k = 0; k < 4; k++) {
                int nx = nb[k];
                if (nx < 0 || g_wall[nx] || d[nx] < BIGNUM) continue;
                d[nx] = nd;
                qs.push_back(nx);
            }
        }
    }
    return g_h[key] = std::move(d);
}

static std::vector<int> g_k;        // 重标权代价 k
static std::vector<int> g_kmark;    // 本查询的标记（避免每次清全表）
static std::vector<int> g_kdone;    // 是否已出队（值已定稿）；必须用 int，char 会串号
static int g_kq = 0;
static std::deque<int> g_dq[4];
static std::vector<int> g_k4;
static std::vector<int> g_kdone4;
static std::vector<int> g_kmark4;   // 按 (格,方向) 记的临时值标记

// 返回第一步的邻格扁平下标；-1 表示走不到；-2 表示交给全量 BFS。
//
// 关键：w' 是有方向的（朝目标 0、背离 1），所以不能从起点算出"到目标的代价"再拆开。
// 用 K = 4*k + 首步方向：每条边只加 0 或 4，首步占低两位。
// 这样 K 最小的那条路 = 最短、且首步方向最靠前 —— 正好是基线的规则。
int DETOUR_STEP(int sx, int sy, int tx, int ty) {
    const vector<int>& HF = STATIC_FIELD(tx, ty);
    size_t src = (size_t)sy * W + sx;
    size_t tgt = (size_t)ty * W + tx;
    if (src >= HF.size() || tgt >= HF.size()) return -2;
    if (HF[src] >= BIGNUM || HF[tgt] >= BIGNUM) return -2;   // 静态都不可达：交给全量 BFS
    if (src == tgt) return -2;
    size_t N = (size_t)W * H;
    if (g_k4.size() != N * 4) {
        g_k4.assign(N * 4, 0);
        g_kmark.assign(N, 0);
        g_kdone4.assign(N * 4, 0);
        g_kmark4.assign(N * 4, 0);
    }
    if (++g_kq == 0) {
        std::fill(g_kmark.begin(), g_kmark.end(), 0);
        std::fill(g_kdone4.begin(), g_kdone4.end(), 0);
        std::fill(g_kmark4.begin(), g_kmark4.end(), 0);
        g_kq = 1;
    }
    const std::array<int, 4> nb0 = g_nb[src];
    int hsrc = HF[src];
    for (int r = 0; r < 4; r++) g_dq[r].clear();
    int openNb = 0;
    for (int r = 0; r < 4; r++) {
        int nx = nb0[r];
        if (nx < 0 || !g_ok[nx] || HF[nx] >= BIGNUM) continue;
        int cost = (1 + HF[nx] - hsrc) / 2;         // 0 或 1
        size_t slot = (size_t)nx * 4 + r;
        g_k4[slot] = cost; g_kmark4[slot] = g_kq;
        if (cost == 0) g_dq[r].push_front(nx); else g_dq[r].push_back(nx);
        openNb++;
    }

    int pops = 0;
    const int LIMIT = 20000;
    for (;;) {
        // 四个队列里挑"代价最小、方向最靠前"的那一个弹出
        int pick = -1, bestK = 0;
        for (int r = 0; r < 4; r++) {
            if (g_dq[r].empty()) continue;
            int c = g_dq[r].front();
            int kk = g_k4[(size_t)c * 4 + r];
            if (pick < 0 || kk < bestK) { pick = r; bestK = kk; }
        }
        if (pick < 0) break;                       // 全空：走不到
        int cur = g_dq[pick].front(); g_dq[pick].pop_front();
        size_t slot = (size_t)cur * 4 + pick;
        if (g_k4[slot] < bestK) continue;          // 陈旧项
        if (g_kdone4[slot] == g_kq) continue;
        g_kdone4[slot] = g_kq;
        if ((size_t)cur == tgt) return nb0[pick];
        if (++pops > LIMIT) return -2;
        const std::array<int, 4>& nb = g_nb[cur];
        int hc = HF[cur];
        for (int k = 0; k < 4; k++) {
            int nx = nb[k];
            if (nx < 0 || !g_ok[nx]) continue;
            if (nx == (int)src) continue;
            int hn = HF[nx];
            if (hn >= BIGNUM) continue;
            int cost = (1 + hn - hc) / 2;
            int K2 = bestK + cost;
            size_t s2 = (size_t)nx * 4 + pick;
            if (g_kdone4[s2] == g_kq) continue;
            if (g_kmark4[s2] == g_kq && g_k4[s2] <= K2) continue;
            g_kmark4[s2] = g_kq;
            g_k4[s2] = K2;
            if (cost == 0) g_dq[pick].push_front(nx); else g_dq[pick].push_back(nx);
        }
    }
    return -1;
}

// BFS2: 给移动用的（从目标往回搜）。张: 和 BFS 一样，但是别合并，上次合并出过事
static map<pair<int, int>, vector<vector<int> > > g_b2;
static long long g_b2ver = -1, g_b2cells = 0;

vector<vector<int> > BFS2(int sx, int sy) {
    // [P3] 按起点缓存；失效条件绑在 BLK_VER 上（封路/解封才作废，不是每 tick）
    if (g_b2ver != BLK_VER) { g_b2.clear(); g_b2ver = BLK_VER; g_b2cells = 0; }
    map<pair<int, int>, vector<vector<int> > >::iterator hit = g_b2.find(make_pair(sx, sy));
    if (hit != g_b2.end()) return hit->second;
    vector<vector<int> > d(H, vector<int>(W, BIGNUM));
    list<pair<int, int> > q;
    d[sy][sx] = 0;
    q.push_back(make_pair(sx, sy));
    while (!q.empty()) {
        int cx = q.front().first, cy = q.front().second;
        q.pop_front();
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX4[k], ny = cy + DY4[k];
            if (!OK(nx, ny)) continue;
            if (d[ny][nx] < BIGNUM) continue;
            d[ny][nx] = d[cy][cx] + 1;
            q.push_back(make_pair(nx, ny));
        }
    }
    // [P3] 存入缓存（带总量上限，约 240MB）
    if (g_b2cells + (long long)H * W <= 60000000LL) {
        g_b2[make_pair(sx, sy)] = d;
        g_b2cells += (long long)H * W;
    }
    return d;
}

// a 到 b 的距离
int DIST(int ax, int ay, int bx, int by) {
    if (ax == bx && ay == by) return 0;
    if (!OK(bx, by)) return BIGNUM;
    vector<vector<int> > d = BFS(ax, ay);
    return d[by][bx];
}

vector<pair<int, int> > ALL_CHG() {
    // [P7] 地图读完后不变，查一次就够
    static vector<pair<int, int> > cached;
    static bool built = false;
    if (!built) {
        built = true;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (MAP[y][x] == 'C') cached.push_back(make_pair(x, y));
            }
        }
    }
    return cached;
}

// 到最近充电桩的距离（2023-03 张: 从 sim.cpp 挪过来的，BFS 先内联着，回头再改）
int CHG_DIST(int x, int y) {
    // [P7] 和移动/派单共用同一份缓存距离场；起点不查可通行（与 baseline 一致）
    const vector<int>& d = BFS2_FLAT(x, y);
    int best = BIGNUM;
    vector<pair<int, int> > cs = ALL_CHG();
    for (int i = 0; i < (int)cs.size(); i++) {
        if (!OK(cs[i].first, cs[i].second)) continue;
        int v = d[(size_t)cs[i].second * W + cs[i].first];
        if (v < best) best = v;
    }
    return best;
}

// 最近的充电桩，没有就 (-1,-1)
pair<int, int> CHG_NEAR(int x, int y) {
    const vector<int>& d = BFS2_FLAT(x, y);   // [P9]
    int best = BIGNUM;
    pair<int, int> res = make_pair(-1, -1);
    vector<pair<int, int> > cs = ALL_CHG();
    for (int i = 0; i < (int)cs.size(); i++) {
        if (!OK(cs[i].first, cs[i].second)) continue;
        if (d[(size_t)cs[i].second * W + cs[i].first] < best) {
            best = d[(size_t)cs[i].second * W + cs[i].first];
            res = cs[i];
        }
    }
    return res;
}

int findRobot(const string& name) {
    for (int i = 0; i < NR; i++) {
        if (RNAME(i) == name) return i;
    }
    return -1;
}

// 写日志
void writeLog(int t, const string& msg) {
    string line = I2S(t) + " " + msg;
    LOGBUF.push_back(line);
    // 顺便统计一下，省得到处加
    vector<string> w = SPLIT(msg);
    if (w.size() > 0) {
        if (w[0] == "DELIVER") {
            long long lat = S2I(w[w.size() - 1]);
            CNT[0]++;
            CNT[4] += lat;
            if (lat > CNT[5]) CNT[5] = lat;
        } else if (w[0] == "LOST") {
            CNT[1]++;
        } else if (w[0] == "REJECT") {
            CNT[2]++;
        }
    }
#if FLAG_VERBOSE
    cerr << line << endl;
#endif
}

void writeRaw(const string& msg) {
    LOGBUF.push_back(msg);
}

void dumpLog() {
    for (int i = 0; i < (int)LOGBUF.size(); i++) {
        cout << LOGBUF[i] << endl;
    }
}
