// Traceflow / 循迹仓流. Released under the repository's MIT license.
#include "simulator.hpp"

#include <iostream>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    traceflow::simulate(std::cin, std::cout);
}
