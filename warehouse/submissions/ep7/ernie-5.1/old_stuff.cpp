// old_stuff.cpp
// 旧代码，先留着，万一要回滚  —— 张 2023-03
//
// 2023-11 张：加了 bfs_fast_cached，按起点缓存 BFS 结果，本地测快了十倍！
//            线上还没开（FLAG_FAST_BFS），等有空再全面测一下
// 2024-02 李：FIXME 开了之后有个负载结果对不上，没查出来，先关着
// 2026-10 主程: 修复缓存版 BFS，启用 FLAG_FAST_BFS=1
#include "common.h"

extern int DX4[4];
extern int DY4[4];

// 缓存: 使用 flat 1D 数组存储缓存结果
static map<int, vector<vector<int> > > g_bfsCache;
static int g_cacheHit = 0, g_cacheMiss = 0;

// 临时 BFS 结果缓冲区
static vector<int> g_bfsTmp;
static int g_bfsTmpSz = 0;

vector<vector<int> > bfs_fast_cached(int sx, int sy) {
    int key = (sy << 16) | sx;
    map<int, vector<vector<int> > >::iterator it = g_bfsCache.find(key);
    if (it != g_bfsCache.end()) {
        g_cacheHit++;
        return it->second;
    }
    g_cacheMiss++;
    
    int sz = W * H;
    if (g_bfsTmpSz < sz) {
        g_bfsTmp.resize(sz);
        g_bfsTmpSz = sz;
    }
    fill(g_bfsTmp.begin(), g_bfsTmp.begin() + sz, BIGNUM);
    
    static int qx[50000], qy[50000];
    int qh = 0, qt = 0;
    
    g_bfsTmp[sy * W + sx] = 0;
    qx[qt] = sx; qy[qt] = sy; qt++;
    
    while (qh < qt) {
        int cx = qx[qh], cy = qy[qh]; qh++;
        int cd = g_bfsTmp[cy * W + cx];
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX4[k], ny = cy + DY4[k];
            if (!OK(nx, ny)) continue;
            int idx = ny * W + nx;
            if (g_bfsTmp[idx] != BIGNUM) continue;
            g_bfsTmp[idx] = cd + 1;
            qx[qt] = nx; qy[qt] = ny; qt++;
        }
    }
    
    // 转换为 vector<vector<int>> 并缓存
    vector<vector<int> > d(H, vector<int>(W));
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            d[y][x] = g_bfsTmp[y * W + x];
    
    if (g_bfsCache.size() > 5000) g_bfsCache.clear();   // 防止内存爆
    g_bfsCache[key] = d;
    return d;
}

// 旧版派单（2022 老王）。新版在 sim.cpp，这个不用了
// 旧版是按订单到达顺序派的，没有优先级
void oldDispatch(int t) {
    vector<string> done;
    for (int oi = 0; oi < (int)PEND.size(); oi++) {
        string id = PEND[oi];
        int px = S2I(ORD[id][0]), py = S2I(ORD[id][1]);
        int best = BIGNUM, who = -1;
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "IDLE") continue;
            int a = DIST(RX[i], RY[i], px, py);
            if (a < best) {
                best = a;
                who = i;
            }
        }
        if (who < 0) continue;
        RST[who] = "TO_PICKUP";
        ROID[who] = id;
        RTX[who] = px;
        RTY[who] = py;
        ORD[id][6] = "ASSIGNED";
        ORD[id][7] = RNAME(who);
        done.push_back(id);
        writeLog(t, "ASSIGN " + id + " " + RNAME(who) + " " + I2S(best));
    }
    // 使用 swap-and-pop 优化移除
    for (int k = 0; k < (int)done.size(); k++) {
        for (int j = 0; j < (int)PEND.size(); j++) {
            if (PEND[j] == done[k]) {
                if (j < (int)PEND.size() - 1) PEND[j] = PEND.back();
                PEND.pop_back();
                break;
            }
        }
    }
}

#if 0
// 想改成按区域派单，没写完
void zoneDispatch(int t) {
    int zones = 4;
    for (int z = 0; z < zones; z++) {
        // TODO
    }
}
#endif

// 调试用
void dumpCacheStat() {
    cerr << "cache hit " << g_cacheHit << " miss " << g_cacheMiss << endl;
}
