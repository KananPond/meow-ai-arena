// random：在不会立刻撞上的方向里随机挑一个（没有就往上走）。
// 随机数由回合号、编号和各人位置决定，所以同一局重打结果相同。
#include "tron.hpp"

#include <cstdint>

int decide(const tron::State& s) {
    uint64_t h = 0x9E3779B97F4A7C15ULL ^ uint64_t(s.turn) * 1000003ULL ^ uint64_t(s.me) * 7919ULL;
    for (int k = 0; k < s.N; ++k) h = (h ^ uint64_t(s.x[k] * 64 + s.y[k])) * 0x100000001B3ULL;
    h ^= h >> 29;
    int ok[4], n = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, s.me, d)) ok[n++] = d;
    return n ? ok[h % uint64_t(n)] : tron::U;
}
