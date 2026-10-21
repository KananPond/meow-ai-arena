// main.cpp  全局变量定义
#include "common.h"

int W = 0, H = 0, T = 0;
vector<string> MAP;
vector<unsigned char> blockedIdx;
int blkVersion = 0;

int PRM[6];
int NR = 0;
vector<int> RX, RY, RB, RTX, RTY, RWS, RDS;
vector<long long> RWT, RTR;
vector<string> RST;
vector<string> ROID;

unordered_map<string, vector<string>> ORD;
set<string> PEND;

vector<string> EVT;
vector<string> LOGBUF;

long long CNT[8] = {0, 0, 0, 0, 0, 0, 0, 0};

int main() {
    ReadAll(cin);
    RunSim();
    Finish();
    dumpLog();
    return 0;
}
