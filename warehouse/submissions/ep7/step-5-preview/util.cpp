// util.cpp  工具：输出缓冲、BFS 距离表缓存
//
// 距离表缓存是本程序性能的关键：BFS 的结果只取决于（起点, 通行性）。
// 通行性只在 BLOCK/UNBLOCK 真正改变格子状态时变化，用 PASSVER 记录版本号，
// 于是同一个版本内同样的起点只需要算一次 BFS（基线是每次都要重算）。
#include "common.h"
#include <charconv>

// ---------------- 输出 ----------------
void outInt(long long v) {
    char b[24];
    auto r = std::to_chars(b, b + sizeof(b), v);
    OUTBUF.append(b, (size_t)(r.ptr - b));
}

void outS(const char* s) { OUTBUF.append(s); }

void outEnd() { OUTBUF.push_back('\n'); }

void flushOut() {
    if (!OUTBUF.empty()) fwrite(OUTBUF.data(), 1, OUTBUF.size(), stdout);
    OUTBUF.clear();
    OUTBUF.shrink_to_fit();
}

// ---------------- BFS ----------------
// 规则与基线一致：起点本身不需要可通行（d[src] = 0），之后只走可通行格。
static void bfsFill(int ncell, int src, int* d, vector<int>& q) {
    for (int i = 0; i < ncell; i++) d[i] = BIGNUM;
    d[src] = 0;
    q.clear();
    q.push_back(src);
    size_t h = 0;
    while (h < q.size()) {
        int cur = q[h++];
        int x = cur % W, y = cur / W;
        int nd = d[cur] + 1;
        int i;
        if (y > 0) { i = cur - W; if (PASS[i] && d[i] == BIGNUM) { d[i] = nd; q.push_back(i); } }
        if (y < H - 1) { i = cur + W; if (PASS[i] && d[i] == BIGNUM) { d[i] = nd; q.push_back(i); } }
        if (x > 0) { i = cur - 1; if (PASS[i] && d[i] == BIGNUM) { d[i] = nd; q.push_back(i); } }
        if (x < W - 1) { i = cur + 1; if (PASS[i] && d[i] == BIGNUM) { d[i] = nd; q.push_back(i); } }
    }
}

static vector<int> SCRATCH, SQUEUE;

void bfsFillInto(int src, int* d) {
    bfsFill(W * H, src, d, SQUEUE);
}

int bfsDist(int sx, int sy, int tx, int ty) {
    if (sx == tx && sy == ty) return 0;
    if (sx < 0 || sy < 0 || sx >= W || sy >= H) return BIGNUM;
    if (tx < 0 || ty < 0 || tx >= W || ty >= H) return BIGNUM;
    if (!PASS[(size_t)ty * W + tx]) return BIGNUM;
    SCRATCH.resize((size_t)W * H);
    bfsFill(W * H, sy * W + sx, SCRATCH.data(), SQUEUE);
    return SCRATCH[(size_t)ty * W + tx];
}

// ---------------- 带 LRU 的距离表缓存 ----------------
// 槽位里的表在用到时才分配；槽位可被不同起点复用，控制内存上限。
struct DistCache {
    int cap, ncell, used;
    int head, tail;
    long long cur;
    vector<vector<int> > buf;   // cap 个表，每个 ncell 个 int
    vector<long long> ver;      // 槽位对应的版本号
    vector<int> key;            // 槽位对应的起点格子，-1 = 空闲槽
    vector<int> lprev, lnext;   // LRU 双向链表，head = 最近用过
    unordered_map<int, int> where;
    vector<int> q;

    void init(int cells, int capacity) {
        ncell = cells;
        cap = capacity;
        used = 0;
        head = tail = -1;
        cur = PASSVER;
        buf.assign(cap, vector<int>());
        ver.assign(cap, 0);
        key.assign(cap, -1);
        lprev.assign(cap, -1);
        lnext.assign(cap, -1);
        where.clear();
        where.reserve(cap * 2);
    }

    void unlink(int s) {
        int p = lprev[s], n = lnext[s];
        if (p >= 0) lnext[p] = n; else head = n;
        if (n >= 0) lprev[n] = p; else tail = p;
        lprev[s] = lnext[s] = -1;
    }

    void pushFront(int s) {
        lprev[s] = -1;
        lnext[s] = head;
        if (head >= 0) lprev[head] = s;
        head = s;
        if (tail < 0) tail = s;
    }

    // 返回起点 cell 的距离表指针；在下一次 get() 之前有效
    const int* get(int cell) {
        auto it = where.find(cell);
        int s;
        if (it != where.end()) {
            s = it->second;
            if (ver[s] == cur) {           // 命中：移到最前
                unlink(s);
                pushFront(s);
                return buf[s].data();
            }
            unlink(s);                     // 同一起点、旧版本：重用槽位
        } else {
            if (used < cap) {
                s = used++;
            } else {
                s = tail;                   // 淘汰最久未用
                unlink(s);
                where.erase(key[s]);
            }
        }
        key[s] = cell;
        ver[s] = cur;
        where[cell] = s;
        if ((int)buf[s].size() != ncell) buf[s].resize(ncell);
        bfsFill(ncell, cell, buf[s].data(), q);
        pushFront(s);
        return buf[s].data();
    }
};

static DistCache DC;

void initDcache(int cells, int capacity) { DC.init(cells, capacity); }

void dcacheSetVersion(long long v) { DC.cur = v; }

const int* dcacheGet(int cell) { return DC.get(cell); }
