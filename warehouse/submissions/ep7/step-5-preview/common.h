// common.h  ---  WMS 仓储调度  公共头文件（2026 重写版）
//
// 这一版保留了基线的全部外部行为（逐字节相同的输出），但内部表示全部换成
// 整数/数组，具体见 src/HANDOFF.md 的说明。字段含义与基线一一对应：
//   基线 RST 字符串              -> 这里的 RobState 枚举
//   基线 ORD: map<string, vector<string>>  -> 这里的 vector<Order> + ORDIDX
//   基线 PEND: vector<string>    -> 这里的 set<PendNode>（保持同样的排序规则）
//   基线 LOGBUF                  -> OUTBUF + append 助手
#ifndef COMMON_H_WMS
#define COMMON_H_WMS

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iosfwd>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

#define BIGNUM 1000000000

// 地图 ===========================================
extern int W, H, T;
extern vector<string> MAP;        // 原始地图行（'.' '#' 'C'），只留作存档
extern vector<uint8_t> WALL;      // W*H：1 = 货架墙
extern vector<uint8_t> BLKF;      // W*H：1 = 被 BLOCK 封锁
extern vector<uint8_t> PASS;      // W*H：1 = 可通行（不是墙且没被封锁）
extern long long PASSVER;         // 通行性版本号，BLOCK/UNBLOCK 真正改变状态时 +1

// 参数 ===========================================
extern int PRM[6];   // 0 满电 1 充电速度 2 低电量 3 安全余量 4 报表间隔 5 热点半径

// 机器人（老王说 struct 太慢，所以拆成数组；延续这个风格） ======
enum { ST_IDLE = 0, ST_TO_PICKUP, ST_DELIVERING, ST_TO_CHARGER, ST_CHARGING, ST_DEAD };
extern const char* const RSTNAME[6];
extern int NR;
extern vector<int> RX, RY, RB, RTX, RTY, RWS, RDS;   // 位置 电量 目标 连续等待 停机时刻
extern vector<long long> RWT, RTR;                    // 累计等待 移动步数
extern vector<int> RST;                               // RobState
extern vector<int> ROID;                              // 订单下标，-1 = 无
extern vector<string> RNM;                            // "R<i>"，输出用
extern vector<int> OCCG;                              // 格子 -> 机器人下标+1，0 = 空（DEAD 不占格）

// 订单 ===========================================
enum { O_PENDING, O_ASSIGNED, O_PICKED, O_DONE, O_CANCELLED, O_LOST };
struct Order {
    long long id;
    int px, py, dx, dy, prio, arrive, status, robot;
};
extern vector<Order> ORD;          // ORDIDX[id] = ORD 下标
extern unordered_map<long long, int> ORDIDX;

// 待派订单：按（优先级降、到达 tick 升、编号升）排序，与基线 dispatch 的 sort 完全一致
struct PendNode { int prio, arrive; long long id; int idx; };
struct PendCmp {
    bool operator()(const PendNode& a, const PendNode& b) const {
        if (a.prio != b.prio) return a.prio > b.prio;
        if (a.arrive != b.arrive) return a.arrive < b.arrive;
        return a.id < b.id;
    }
};
extern set<PendNode, PendCmp> PENDQ;

// 事件 ===========================================
enum { EV_ORDER = 0, EV_CANCEL, EV_BLOCK, EV_UNBLOCK, EV_UNKNOWN };
struct Ev { int tick, type; long long id; int px, py, dx, dy, prio; };
extern vector<vector<Ev> > EVT;    // EVT[tick]：本 tick 的事件，保持输入顺序

// 日志 & 统计 =====================================
extern string OUTBUF;
extern long long CNT[8];   // 0 送达 1 丢失 2 拒绝 3 取消 4 延迟和 5 延迟最大

// util.cpp
void outInt(long long v);
void outS(const char* s);
void outEnd();
void flushOut();
void initDcache(int cells, int capacity);   // 建距离表缓存（cells = W*H）
void dcacheSetVersion(long long v);         // 通行性变化后调用
const int* dcacheGet(int cell);             // 取起点的距离表；下次 get 前有效
int bfsDist(int sx, int sy, int tx, int ty);   // 未缓存的一次性 BFS（罕见路径用）
void bfsFillInto(int src, int* d);          // 往调用方缓冲区里填 BFS 距离表

// sim.cpp
void ReadAll(std::istream& in);
void RunSim();
void Finish();

#endif
