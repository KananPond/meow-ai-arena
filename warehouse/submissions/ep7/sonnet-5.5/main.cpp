// WMS 仓储机器人调度仿真：从标准输入读场景，往标准输出写日志
#include "common.h"

int main() {
    string input;
    char buf[1 << 16];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, stdin)) > 0) input.append(buf, n);
    string out;
    out.reserve(1 << 20);
    runAll(input, out);
    fwrite(out.data(), 1, out.size(), stdout);
    return 0;
}
