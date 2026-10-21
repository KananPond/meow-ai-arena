// common.h  ---  WMS 仓储调度  公共头文件（重写版）
// 说明：这是对 2022 年老代码的一次完整重构。行为与 baseline/ 逐字节一致，
// 但数据结构改为面向数组 / 扁平化，去掉了 old_stuff.cpp 里的死代码。
// 交接说明见 src/HANDOFF.md。
#ifndef COMMON_H_WMS
#define COMMON_H_WMS

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <utility>

using namespace std;

#define BIGNUM 1000000000

// ===================== 地图 =====================
extern int W, H, T;
extern vector<string> MAP;            // MAP[y][x]
extern vector<unsigned char> passable;// passable[y*W+x]：在图内、非墙、未被封锁
extern vector<int> chargers;          // 所有 'C' 格子的扁平下标 y*W+x，行优先
extern unordered_set<int> BLK;        // 当前被封锁的格子（扁平下标）

// ===================== 参数 =====================
extern int PRM[6];   // 0 满电 1 充电速度 2 低电量阈值 3 安全余量 4 报表间隔 5 热点半径

// ===================== 机器人（SoA 数组） =====================
extern int NR;
extern vector<int> RX, RY, RB, RTX, RTY, RWS, RDS;   // 位置 电量 目标 连续等待 停机tick
extern vector<long long> RWT, RTR;                   // 累计等待 移动步数
extern vector<int> RST;                               // 状态（enum）
extern vector<int> ROID;                              // 持有订单 id（0=无）
extern vector<int> occ;                                // occ[y*W+x]=机器人下标+1，0=空
extern vector<string> RNAMES;                          // 预生成的 "R<i>"

// 机器人状态
enum { S_IDLE=0, S_TOPICKUP=1, S_DELIVERING=2, S_TOCHARGER=3, S_CHARGING=4, S_DEAD=5 };
extern const char* STNAME[6];

// ===================== 订单 =====================
enum { O_PENDING=0, O_ASSIGNED=1, O_PICKED=2, O_DONE=3, O_CANCELLED=4, O_LOST=5 };
struct Order {
    int id;
    int px, py, dx, dy, prio, arrival;
    int status;
    int robot;    // 1-based 机器人下标，0=无
};
extern unordered_map<int, Order> ORD;   // order id -> Order
extern vector<int> PEND;                // PENDING 订单 id（保持插入顺序）

// ===================== 事件 =====================
enum { EV_ORDER=0, EV_CANCEL=1, EV_BLOCK=2, EV_UNBLOCK=3 };
struct Event { int tick; int type; int a,b,c,d,e,f; };
extern vector<Event> EVT;                 // 按输入顺序
extern vector<vector<int>> EVTT;          // EVTT[tick] = 该 tick 的事件下标（输入顺序）

// ===================== 日志 =====================
extern string LOGBUF;      // 预分配的输出缓冲，最后一次性写出
extern long long CNT[8];   // 0 送达 1 丢失 2 拒绝 3 取消 4 延迟和 5 延迟最大

// util.cpp
void initChargers();                       // 建 chargers 表
void initPassable();                       // 建 passable 表（仅墙）
void bfs(int sx, int sy, vector<int>& dist);   // 扁平 BFS，起点不限可通行，展开只走可通行格
void writeLog(int t, const string& msg);
void writeRaw(const string& msg);

// sim.cpp
void ReadAll(istream& in);
void RunSim();
void Finish();

#endif