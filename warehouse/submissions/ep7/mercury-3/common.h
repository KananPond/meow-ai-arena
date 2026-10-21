// common.h  ---  WMS 仓储调度 公共头文件
// 说明：本文件保留原全局变量布局，但做了类型/内部结构调整以提速。
//   - ORD 由 map 改为 unordered_map（无代码依赖迭代顺序，只有 Finish 计数）
//   - PEND 由 vector 改为 set（派单本来就全量排序，顺序无意义，set 便于删除）
//   - 保留 EVT 原始事件行；事件解析在 sim.cpp 预处理成 Event 列表
#ifndef COMMON_H_WMS
#define COMMON_H_WMS

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <utility>
#include <algorithm>

using namespace std;

#define BIGNUM 1000000000
#define MAXR 512
#define INF16 65535     // uint16 距离表里的不可达哨兵（真实最短路 < 65535）

// 地图 ===========================================
extern int W, H, T;
extern vector<string> MAP;                 // MAP[y][x]
extern vector<unsigned char> blockedIdx;   // blockedIdx[y*W+x] = 1 表示当前被 BLOCK
extern int blkVersion;                     // BLK 变化版本号，用于 BFS 缓存失效

// 参数 ===========================================
extern int PRM[6];   // 0 满电 1 充电速度 2 低电量 3 安全余量 4 报表间隔 5 热点半径

// 机器人（老王说 struct 太慢，所以拆成数组） ======
extern int NR;
extern vector<int> RX, RY, RB, RTX, RTY, RWS, RDS;   // 位置 电量 目标 连续等待 停机时刻
extern vector<long long> RWT, RTR;                    // 累计等待 移动步数
extern vector<string> RST;                            // 状态
extern vector<string> ROID;                           // 订单号

// 订单 ===========================================
extern unordered_map<string, vector<string>> ORD;
extern set<string> PEND;    // 待派订单

// 事件 & 日志 =====================================
extern vector<string> EVT;
extern vector<string> LOGBUF;

// 统计 ===========================================
extern long long CNT[8];

// util.cpp
vector<string> SPLIT(const string& s);
int S2I(const string& s);
string I2S(long long v);
string RNAME(int i);
bool OK(int x, int y);
bool INMAP(int x, int y);
bool ISWALL(int x, int y);
bool OCC(int x, int y, int except);
void occInit();
void occAdd(int i);
void occSub(int i);
const vector<unsigned short>& bfsRef(int sx, int sy);   // 扁平 BFS 距离表（缓存，版本失效）
int distTo(int sx, int sy, int tx, int ty);             // 等价原 DIST
int chgDist(int x, int y);                              // 等价原 CHG_DIST
pair<int, int> chgNear(int x, int y);                   // 等价原 CHG_NEAR
const vector<pair<int, int>>& allChargers();
int findRobot(const string& name);
void writeLog(int t, const string& msg);
void writeRaw(const string& msg);
void dumpLog();

// sim.cpp
void ReadAll(istream& in);
void RunSim();
void Finish();

#endif
