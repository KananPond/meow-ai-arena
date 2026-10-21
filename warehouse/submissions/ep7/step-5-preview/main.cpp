// WMS 仓储机器人调度仿真  v4.0（性能重写版，行为与基线一致）
// 从标准输入读场景，往标准输出写日志
#include "common.h"
#include <iostream>

// 全局变量都在这里定义 ===============
int W = 0, H = 0, T = 0;
vector<string> MAP;
vector<uint8_t> WALL, BLKF, PASS;
long long PASSVER = 0;
int PRM[6];
int NR = 0;
vector<int> RX, RY, RB, RTX, RTY, RWS, RDS;
vector<long long> RWT, RTR;
vector<int> RST, ROID;
vector<string> RNM;
vector<int> OCCG;
vector<Order> ORD;
unordered_map<long long, int> ORDIDX;
set<PendNode, PendCmp> PENDQ;
vector<vector<Ev> > EVT;
string OUTBUF;
long long CNT[8] = {0, 0, 0, 0, 0, 0, 0, 0};

const char* const RSTNAME[6] = {"IDLE", "TO_PICKUP", "DELIVERING", "TO_CHARGER", "CHARGING", "DEAD"};

int main() {
    ReadAll(cin);
    RunSim();
    Finish();
    flushOut();
    return 0;
}
