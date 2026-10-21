// WMS 仓储机器人调度仿真
// 从标准输入读取场景，把仿真日志写到标准输出。
#include <iostream>

#include "sim.h"

int main() {
    std::ios::sync_with_stdio(false);
    Sim sim(std::cin);
    std::cout << sim.run();
    return 0;
}
