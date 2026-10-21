// main.cpp  全局变量定义 + 入口
#include "common.h"

int W = 0, H = 0, T = 0;
vector<string> MAP;
vector<unsigned char> passable;
vector<int> chargers;
unordered_set<int> BLK;
int PRM[6];
int NR = 0;
vector<int> RX, RY, RB, RTX, RTY, RWS, RDS;
vector<long long> RWT, RTR;
vector<int> RST, ROID, occ;
vector<string> RNAMES;
const char* STNAME[6] = { "IDLE", "TO_PICKUP", "DELIVERING", "TO_CHARGER", "CHARGING", "DEAD" };
unordered_map<int, Order> ORD;
vector<int> PEND;
vector<Event> EVT;
vector<vector<int>> EVTT;
string LOGBUF;
long long CNT[8] = {0, 0, 0, 0, 0, 0, 0, 0};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    ReadAll(cin);
    RunSim();
    Finish();
    cout << LOGBUF;
    return 0;
}
