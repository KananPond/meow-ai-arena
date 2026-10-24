// 你的 bot。现在是最简单的能合法行动的版本：能直走就直走，否则按 上、右、下、左 找第一个能走的方向。
// 规则见 RULES.md；局面 tron::State 的字段和可用的函数见 tron.hpp 开头。
#include "tron.hpp"

// 每回合调用一次，返回 tron::U / tron::R / tron::D / tron::L。
// s.me 是你的编号，s.N 是本局人数，s.turn 是回合号（从 0 开始）。
int decide(const tron::State& s) {
    const int me = s.me;
    if (tron::can_move(s, me, s.last_dir[me])) return s.last_dir[me];
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, me, d)) return d;
    return tron::U;    // 无路可走
}
