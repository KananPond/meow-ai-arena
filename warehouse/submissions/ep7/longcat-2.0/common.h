// common.h  ---  WMS 仓储调度  公共头文件
// 优化版：只改了声明，实现全在 util.cpp / sim.cpp
#ifndef COMMON_H_WMS
#define COMMON_H_WMS

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <queue>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <utility>

extern int DX4[4];
extern int DY4[4];

using namespace std;

#define BIGNUM 1000000000
#define MAXR 512

// 地图 ===========================================
extern int W, H, T;
extern vector<string> MAP;
extern vector<char> BLK_GRID;  // 1 = 封锁, 替代 set<pair<int,int>>

// 参数 ===========================================
extern int PRM[6];   // 0 满电 1 充电速度 2 低电量 3 安全余量 4 报表间隔 5 热点半径

// 机器人 ===========================================
extern int NR;
extern vector<int> RX, RY, RB, RTX, RTY, RWS, RDS;
extern vector<long long> RWT, RTR;
extern vector<string> RST;
extern vector<string> ROID;

// 订单 ===========================================
extern map<string, vector<string> > ORD;
extern vector<string> PEND;

// 日志 ===========================================
extern vector<string> LOGBUF;

// 统计 ===========================================
extern long long CNT[8];

// 预计算 ===========================================
extern vector<pair<int,int>> CHARGERS;  // 充电桩列表（只算一次）
extern vector<int> OCC_GRID;            // 占据网格，OCC_GRID[y*W+x] = 机器人编号+1 或 0

// 事件结构（预解析）--------------------------------
struct Event {
    int type;     // 0=ORDER 1=CANCEL 2=BLOCK 3=UNBLOCK
    string id;    // ORDER/CANCEL 用
    int a, b, c, d, e;  // 通用参数
};
extern vector<vector<Event>> EVENTS_BY_TICK;  // 按 tick 分组的事件

// util.cpp
vector<string> SPLIT(const string& s);
int S2I(const string& s);
inline string I2S(long long v) { return to_string(v); }
inline string RNAME(int i) { return "R" + to_string(i); }
inline bool INMAP(int x, int y) { return x >= 0 && y >= 0 && x < W && y < H; }
inline bool ISWALL(int x, int y) { return MAP[y][x] == '#'; }
bool OK(int x, int y);
inline bool OCC_CHK(int x, int y, int except) {
    if (!INMAP(x, y)) return false;
    int id = OCC_GRID[y * W + x];
    return id != 0 && (id - 1) != except;
}
const vector<vector<int>>& BFS(int sx, int sy);
inline const vector<vector<int>>& BFS2(int sx, int sy) { return BFS(sx, sy); }
int DIST(int ax, int ay, int bx, int by);
int CHG_DIST(int x, int y);
pair<int, int> CHG_NEAR(int x, int y);
void writeLog(int t, const string& msg);
void writeRaw(const string& msg);
void dumpLog();
inline int findRobot(const string& name) { return atoi(name.c_str() + 1); }
void invalidate_bfs_cache();

// sim.cpp
void ReadAll(istream& in);
void RunSim();
void Finish();

#endif
