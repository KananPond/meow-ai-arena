// WMS 仓储机器人调度仿真  v3.2（2026 重写：行为不变，实现换新）
// 从标准输入读场景，往标准输出写日志
#include "common.h"

// 全局变量都在这里定义 ===============
int W = 0, H = 0, T = 0;
vector<string> MAP;
int PRM[6];
int NR = 0;

int main() {
    ios::sync_with_stdio(false);
    ReadAll(cin);
    RunSim();
    Finish();
    dumpLog();
    return 0;
}
