// common.h  ---  WMS 仓储调度  公共头文件
//
// 2026 重写版：整个程序保留基线（baseline/）的**实际行为**，只换实现。
// 行为细节（包括那些看起来像 bug 的地方）都记在 HANDOFF.md 里，改逻辑前先看那份文档。
//
// 模块划分：
//   util.cpp  字符串/数字工具 + 地图网格 + BFS 距离场（带缓存）
//   sim.cpp   仿真主体：读输入、每个 tick 的流程、收尾统计
//   main.cpp  驱动
#ifndef COMMON_H_WMS
#define COMMON_H_WMS

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <climits>
#include <utility>

using namespace std;

// 原 BIGNUM：距离的“无穷大”
static const int INF = 1000000000;

// 地图 / 参数 ===========================================
extern int W, H, T;
extern vector<string> MAP;            // MAP[y][x]
extern int PRM[6];   // 0 满电 1 充电速度 2 低电量 3 安全余量 4 报表间隔 5 热点半径
extern int NR;

// util.cpp : 字符串 / 数字 ==============================
vector<string> SPLIT(const string& s);
int S2I(const string& s);            // 与 atoi 同语义
string I2S(long long v);
void APPEND_INT(string& out, long long v);   // 比 stringstream 快很多
string RNAME(int i);

// util.cpp : 地图网格 ===================================
void gridInit();                     // 读进地图后调用一次：建邻居表/充电桩表/BFS 缓存
inline int CELL(int x, int y) { return y * W + x; }
int cellOf(int x, int y);            // 不在图内返回 -1
bool INMAP(int x, int y);
bool ISWALL(int x, int y);
bool OK(int x, int y);
bool PASSABLE(int cell);             // 在图内 且 不是墙 且 没被封锁  == OK()
const int* NB4(int cell);            // 四个方向的邻居 cell，-1 表示出图（顺序：上右下左）

// 占据（非 DEAD 机器人才占格子）。selfCell = 自己当前所在 cell（自己不算占据），
// 已经 DEAD / 不存在时传 -1。
bool OCC(int cell, int selfCell);
void occAdd(int cell, int delta);

// 封锁：返回是否真的改变了封锁状态（改变了 → BFS 缓存失效）
bool setBlocked(int cell, bool v);
int curEpoch();                      // 封锁集合的版本号，用来判断缓存是否还有效

// util.cpp : BFS ========================================
// 一张“距离场”：从 srcCell 出发的四连通 BFS 距离（起点无论能不能走都算 0，
// 之后只走可通行格）。场被缓存，封锁集合一变就整体作废。
struct Field {
    vector<int> dist;
    vector<int> stamp;   // stamp[c]==gen 时 dist[c] 有效，否则是 INF（省掉每次 BFS 的清零）
    int gen = 0;
    int srcCell = -1;
};
const Field& fieldFrom(int cell);
inline int fieldDist(const Field& f, int cell) {
    return f.stamp[cell] == f.gen ? f.dist[cell] : INF;
}
// 原 CHG_DIST：从 (x,y) 出发，到最近的“可通行充电桩”的距离
int chargerDistFrom(int x, int y);
const vector<int>& allChargerCells();   // 全部充电桩 cell，行优先

// sim.cpp ===============================================
void ReadAll(istream& in);
void RunSim();
void Finish();
void dumpLog();

#endif
