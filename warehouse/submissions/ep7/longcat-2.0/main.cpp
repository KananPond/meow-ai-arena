// WMS 仓储机器人调度仿真 (优化版)
#include "common.h"

int W = 0, H = 0, T = 0;
vector<string> MAP;
vector<char> BLK_GRID;
int PRM[6];
int NR = 0;
vector<int> RX, RY, RB, RTX, RTY, RWS, RDS;
vector<long long> RWT, RTR;
vector<string> RST;
vector<string> ROID;
map<string, vector<string> > ORD;
vector<string> PEND;
vector<string> LOGBUF;
long long CNT[8] = {0, 0, 0, 0, 0, 0, 0, 0};
vector<pair<int,int>> CHARGERS;
vector<int> OCC_GRID;
vector<vector<Event>> EVENTS_BY_TICK;

#ifndef PROFILE_BUILD
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    ReadAll(cin);
    RunSim();
    Finish();
    dumpLog();
    return 0;
}
#endif
