// util.cpp  工具函数 (优化版)
#include "common.h"

int DX4[4] = {0, 1, 0, -1};
int DY4[4] = {-1, 0, 1, 0};   // 上 右 下 左

// === BFS 缓存 ===
static int bfs_version = 1;
static unordered_map<int, pair<int, shared_ptr<vector<vector<int>>>>> bfs_cache;

void invalidate_bfs_cache() {
    bfs_version++;
    if (bfs_version < 0) { bfs_cache.clear(); bfs_version = 1; }
}

const vector<vector<int>>& BFS(int sx, int sy) {
    int key = sx * 201 + sy;
    auto it = bfs_cache.find(key);
    if (it != bfs_cache.end() && it->second.first == bfs_version) {
        return *it->second.second;
    }
    auto d = make_shared<vector<vector<int>>>(H, vector<int>(W, BIGNUM));
    deque<pair<int,int>> q;
    (*d)[sy][sx] = 0;
    q.emplace_back(sx, sy);
    while (!q.empty()) {
        int cx = q.front().first, cy = q.front().second;
        q.pop_front();
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX4[k], ny = cy + DY4[k];
            if (!OK(nx, ny)) continue;
            if ((*d)[ny][nx] != BIGNUM) continue;
            (*d)[ny][nx] = (*d)[cy][cx] + 1;
            q.emplace_back(nx, ny);
        }
    }
    if ((int)bfs_cache.size() > 1000) bfs_cache.clear();
    bfs_cache[key] = {bfs_version, d};
    return *d;
}

// === 工具函数 ===

vector<string> SPLIT(const string& s) {
    vector<string> res;
    string cur = "";
    for (int i = 0; i < (int)s.size(); i++) {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            if (cur != "") { res.push_back(cur); cur = ""; }
        } else {
            cur += c;
        }
    }
    if (cur != "") res.push_back(cur);
    return res;
}

int S2I(const string& s) { return atoi(s.c_str()); }

bool OK(int x, int y) {
    if (!INMAP(x, y)) return false;
    if (MAP[y][x] == '#') return false;
    if (BLK_GRID[y * W + x]) return false;
    return true;
}

int DIST(int ax, int ay, int bx, int by) {
    if (ax == bx && ay == by) return 0;
    if (!OK(bx, by)) return BIGNUM;
    const vector<vector<int>>& d = BFS(ax, ay);
    return d[by][bx];
}

int CHG_DIST(int x, int y) {
    const vector<vector<int>>& d = BFS(x, y);
    int best = BIGNUM;
    for (int i = 0; i < (int)CHARGERS.size(); i++) {
        int cx = CHARGERS[i].first, cy = CHARGERS[i].second;
        if (!OK(cx, cy)) continue;
        if (d[cy][cx] < best) best = d[cy][cx];
    }
    return best;
}

pair<int, int> CHG_NEAR(int x, int y) {
    const vector<vector<int>>& d = BFS(x, y);
    int best = BIGNUM;
    pair<int, int> res = {-1, -1};
    for (int i = 0; i < (int)CHARGERS.size(); i++) {
        int cx = CHARGERS[i].first, cy = CHARGERS[i].second;
        if (!OK(cx, cy)) continue;
        if (d[cy][cx] < best) { best = d[cy][cx]; res = CHARGERS[i]; }
    }
    return res;
}

// 写日志
void writeLog(int t, const string& msg) {
    LOGBUF.push_back(I2S(t) + " " + msg);
    // 快速统计：只看首字符区分事件类型
    if (!msg.empty()) {
        char c = msg[0];
        if (c == 'D' && msg.compare(0, 7, "DELIVER") == 0) {
            size_t pos = msg.rfind(' ');
            if (pos != string::npos) {
                int lat = S2I(msg.substr(pos + 1));
                CNT[0]++; CNT[4] += lat;
                if (lat > CNT[5]) CNT[5] = lat;
            }
        } else if (c == 'L' && msg.compare(0, 4, "LOST") == 0) {
            CNT[1]++;
        } else if (c == 'R' && msg.compare(0, 6, "REJECT") == 0) {
            CNT[2]++;
        }
    }
}

void writeRaw(const string& msg) { LOGBUF.push_back(msg); }

void dumpLog() {
    for (int i = 0; i < (int)LOGBUF.size(); i++)
        cout << LOGBUF[i] << '\n';
}
