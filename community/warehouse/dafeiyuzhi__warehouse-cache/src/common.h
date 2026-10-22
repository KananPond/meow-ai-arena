// common.h  ---  WMS 仓储调度  公共头文件
// 所有人都 include 这个，别乱改！！！ (2022 老王)
// 2023-03 张: 加了一些全局变量
// 2023-11 张: 加了 FAST_BFS 开关
// 2024-06 实习生: 加了注释（部分）
#ifndef COMMON_H_WMS
#define COMMON_H_WMS

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <utility>

using namespace std;

#define BIGNUM 1000000000
#define MAXR 512
#define FLAG_FAST_BFS 0      // 缓存版 BFS，见 old_stuff.cpp，线上先别开
#define FLAG_VERBOSE 0
#define USE_NEW_DISPATCH 1   // 0 = 用 old_stuff.cpp 里的旧派单

// 地图 ===========================================
extern int W, H, T;
extern vector<string> MAP;            // MAP[y][x]
extern set<pair<int, int> > BLK;      // 封锁的格子 (x,y)
extern long long BLK_VER;             // [P3] 每次 BLK 变化 +1（BFS2 缓存用）

// 参数 ===========================================
extern int PRM[6];   // 0 满电 1 充电速度 2 低电量 3 安全余量 4 报表间隔 5 热点半径

// 机器人（老王说 struct 太慢，所以拆成数组） ======
extern int NR;
extern vector<int> RX, RY, RB, RTX, RTY, RWS, RDS;   // 位置 电量 目标 连续等待 停机时刻
extern vector<long long> RWT, RTR;                    // 累计等待 移动步数
extern vector<string> RST;                            // 状态
extern vector<string> ROID;                           // 订单号

// 订单 ===========================================
// ORD[id] = {px, py, dx, dy, prio, arrival, status, robotName}
extern map<string, vector<string> > ORD;
extern vector<string> PEND;    // 待派订单

// 事件 & 日志 =====================================
extern vector<string> EVT;     // 原始事件行
extern vector<pair<int, vector<string> > > EVP;   // [P6] 事件一次解析并按 tick 排序
extern vector<string> LOGBUF;

// 统计 ===========================================
extern long long CNT[8];   // 0 送达 1 丢失 2 拒绝 3 取消 4 延迟和 5 延迟最大  6,7 没用

// util.cpp
vector<string> SPLIT(const string& s);
int S2I(const string& s);
string I2S(long long v);
string RNAME(int i);
bool OK(int x, int y);           // 能不能走
void OK_INIT();
void OK_SET_BLK(int x, int y, bool blocked);   // [P15]
bool INMAP(int x, int y);
bool ISWALL(int x, int y);
bool OCC(int x, int y, int except);
vector<vector<int> > BFS(int sx, int sy);
vector<vector<int> > BFS2(int sx, int sy);
const vector<int>& BFS2_FLAT(int sx, int sy);
const vector<int>& BFS_FLAT_SCRATCH(int sx, int sy);
void BFS2_PRUNE_AT_TICK();   // [P12] 只在 tick 边界清缓存   // [P10] 不缓存、零分配，复用一块 scratch   // [P5] 一维距离场，下标 y*W+x
int DIST(int ax, int ay, int bx, int by);
int CHG_DIST(int x, int y);
// [P20] 永久墙图的静态距离（势函数 h）：封路不让它失效，只按目标缓存
const vector<int>& STATIC_FIELD(int tx, int ty);
// [P20] 0/1 重标权局部搜索：返回第一步方向下标(0..3)，-1=无路，-2=交给全量 BFS
int DETOUR_STEP(int sx, int sy, int tx, int ty);
pair<int, int> CHG_NEAR(int x, int y);
vector<pair<int, int> > ALL_CHG();
void writeLog(int t, const string& msg);
void writeRaw(const string& msg);
void dumpLog();
int findRobot(const string& name);

// sim.cpp
void ReadAll(istream& in);
void RunSim();
void Finish();

// old_stuff.cpp
vector<vector<int> > bfs_fast_cached(int sx, int sy);
void oldDispatch(int t);

#endif
