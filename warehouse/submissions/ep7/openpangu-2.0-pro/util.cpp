// util.cpp  工具函数
#include "common.h"

int DX4[4] = {0, 1, 0, -1};
int DY4[4] = {-1, 0, 1, 0};   // 上 右 下 左  !!!顺序不能改!!!

// ========== 优化: 预计算充电桩列表 ==========
static vector<pair<int, int> > s_chargers;
static bool s_chargers_valid = false;

static void refresh_chargers() {
    s_chargers.clear();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (MAP[y][x] == 'C')
                s_chargers.push_back(make_pair(x, y));
    s_chargers_valid = true;
}

vector<pair<int, int> > ALL_CHG() {
    if (!s_chargers_valid) refresh_chargers();
    return s_chargers;
}

// ========== 优化: BFS 缓存（带失效机制） ==========
static vector<vector<int> > s_bfs_cache;
static pair<int, int> s_bfs_cache_key = make_pair(-1, -1);
static int s_bfs_cache_blkver = 0;
static int s_blk_version = 0;

void invalidateBFS() {
    s_blk_version++;
    s_bfs_cache_key = make_pair(-1, -1);  // 强制失效
}

vector<vector<int> > BFS(int sx, int sy) {
    // 缓存命中检查（坐标相同且封锁未变化）
    if (s_bfs_cache_key.first == sx && s_bfs_cache_key.second == sy &&
        s_bfs_cache_blkver == s_blk_version && !s_bfs_cache.empty()) {
        return s_bfs_cache;
    }

    vector<vector<int> > d(H, vector<int>(W, BIGNUM));
    queue<pair<int, int> > q;
    d[sy][sx] = 0;
    q.push(make_pair(sx, sy));
    while (!q.empty()) {
        pair<int, int> p = q.front();
        q.pop();
        for (int k = 0; k < 4; k++) {
            int nx = p.first + DX4[k], ny = p.second + DY4[k];
            if (!OK(nx, ny)) continue;
            if (d[ny][nx] != BIGNUM) continue;
            d[ny][nx] = d[p.second][p.first] + 1;
            q.push(make_pair(nx, ny));
        }
    }
    s_bfs_cache = d;
    s_bfs_cache_key = make_pair(sx, sy);
    s_bfs_cache_blkver = s_blk_version;
    return d;
}

// BFS2: 和 BFS 逻辑相同，共用缓存
vector<vector<int> > BFS2(int sx, int sy) {
    return BFS(sx, sy);
}

// ========== 优化: SPLIT 避免逐字符拼接 ==========
vector<string> SPLIT(const string& s) {
    vector<string> res;
    size_t i = 0, n = s.size();
    while (i < n) {
        // 跳过空白
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' ||
                         s[i] == '\n' || s[i] == '\v' || s[i] == '\f'))
            i++;
        if (i >= n) break;
        size_t start = i;
        while (i < n && !(s[i] == ' ' || s[i] == '\t' || s[i] == '\r' ||
                          s[i] == '\n' || s[i] == '\v' || s[i] == '\f'))
            i++;
        res.push_back(s.substr(start, i - start));
    }
    return res;
}

int S2I(const string& s) {
    return atoi(s.c_str());
}

// ========== 优化: I2S 用 to_string 替代 stringstream ==========
string I2S(long long v) {
    return to_string(v);
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
bool OK(int x, int y) {
    if (!INMAP(x, y)) return false;
    if (ISWALL(x, y)) return false;
    if (BLK.find(make_pair(x, y)) != BLK.end()) return false;
    return true;
}

// ========== 优化: OCC 用 occupancy grid ==========
// 在移动阶段之前构建占据网格，O(1)查询代替O(NR)遍历
vector<vector<bool> > s_occ;
bool s_occ_valid = false;

void build_occ() {
    s_occ.assign(H, vector<bool>(W, false));
    for (int i = 0; i < NR; i++) {
        if (RST[i] != "DEAD") {
            s_occ[RY[i]][RX[i]] = true;
        }
    }
    s_occ_valid = true;
}

bool OCC(int x, int y, int except) {
    if (!s_occ_valid) build_occ();
    if (!INMAP(x, y)) return false;
    return s_occ[y][x];
}

void mark_occ(int x, int y, bool val) {
    if (INMAP(x, y)) s_occ[y][x] = val;
}

void clear_occ_valid() {
    s_occ_valid = false;
}

// a 到 b 的距离
int DIST(int ax, int ay, int bx, int by) {
    if (ax == bx && ay == by) return 0;
    if (!OK(bx, by)) return BIGNUM;
    vector<vector<int> > d = BFS(ax, ay);
    return d[by][bx];
}

// 到最近充电桩的距离
int CHG_DIST(int x, int y) {
    if (!s_chargers_valid) refresh_chargers();
    vector<vector<int> > d(H, vector<int>(W, BIGNUM));
    queue<pair<int, int> > q;
    d[y][x] = 0;
    q.push(make_pair(x, y));
    while (!q.empty()) {
        pair<int, int> p = q.front();
        q.pop();
        for (int k = 0; k < 4; k++) {
            int nx = p.first + DX4[k], ny = p.second + DY4[k];
            if (!INMAP(nx, ny)) continue;
            if (MAP[ny][nx] == '#') continue;
            if (BLK.count(make_pair(nx, ny))) continue;
            if (d[ny][nx] != BIGNUM) continue;
            d[ny][nx] = d[p.second][p.first] + 1;
            q.push(p);
            q.back() = make_pair(nx, ny);
        }
    }
    int best = BIGNUM;
    for (size_t i = 0; i < s_chargers.size(); i++) {
        int cx = s_chargers[i].first, cy = s_chargers[i].second;
        if (!OK(cx, cy)) continue;
        if (d[cy][cx] < best) best = d[cy][cx];
    }
    return best;
}

// 最近的充电桩，没有就 (-1,-1)
pair<int, int> CHG_NEAR(int x, int y) {
    if (!s_chargers_valid) refresh_chargers();
    vector<vector<int> > d = BFS(x, y);
    int best = BIGNUM;
    pair<int, int> res = make_pair(-1, -1);
    for (size_t i = 0; i < s_chargers.size(); i++) {
        int cx = s_chargers[i].first, cy = s_chargers[i].second;
        if (!OK(cx, cy)) continue;
        if (d[cy][cx] < best) {
            best = d[cy][cx];
            res = s_chargers[i];
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

// ========== 优化: writeLog 避免每次 SPLIT 解析 ==========
// 用简单的字符串扫描替代 SPLIT 解析
void writeLog(int t, const string& msg) {
    string line = I2S(t) + " " + msg;
    LOGBUF.push_back(line);
    // 顺便统计一下，省得到处加
    const char* m = msg.c_str();
    size_t len = msg.size();
    if (len >= 7 && m[0] == 'D' && m[1] == 'E' && m[2] == 'L' &&
        m[3] == 'I' && m[4] == 'V' && m[5] == 'E' && m[6] == 'R') {
        // DELIVER ... N
        size_t last_space = msg.rfind(' ');
        if (last_space != string::npos) {
            long long lat = S2I(msg.c_str() + last_space + 1);
            CNT[0]++;
            CNT[4] += lat;
            if (lat > CNT[5]) CNT[5] = lat;
        }
    } else if (len >= 4 && m[0] == 'L' && m[1] == 'O' && m[2] == 'S' && m[3] == 'T') {
        CNT[1]++;
    } else if (len >= 6 && m[0] == 'R' && m[1] == 'E' && m[2] == 'J' &&
               m[3] == 'E' && m[4] == 'C' && m[5] == 'T') {
        CNT[2]++;
    }
#if FLAG_VERBOSE
    cerr << line << endl;
#endif
}

void writeRaw(const string& msg) {
    LOGBUF.push_back(msg);
}

void dumpLog() {
    for (size_t i = 0; i < LOGBUF.size(); i++) {
        cout << LOGBUF[i] << endl;
    }
}
