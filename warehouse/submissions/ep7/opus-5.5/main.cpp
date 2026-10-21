// 仓储机器人调度仿真：从标准输入读场景，往标准输出写日志
#include <cstdio>
#include <iostream>
#include <iterator>
#include <string>

#include "sim.h"

int main() {
    std::ios::sync_with_stdio(false);
    std::string input((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    Sim sim;
    sim.read(input);
    sim.run();
    sim.finish();
    const std::string& out = sim.output();
    fwrite(out.data(), 1, out.size(), stdout);
    return 0;
}
