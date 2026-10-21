// greedy：计分的零点（greedy = 0 分）。
// 做法：在能走的方向里，选"走过去之后还能到达的空格最多"的那个（flood fill 数格子）；
// 一样多时先直行，再按 上、右、下、左 的顺序。不看对手，不做任何预判。
#include "tron.hpp"

#include <vector>

// 从 (sx, sy) 出发（这一格当作已经是墙）能到达的空格数
static int reachable(const tron::State& s, int sx, int sy) {
    static bool seen[tron::MAXH][tron::MAXW];
    for (int y = 0; y < s.H; ++y)
        for (int x = 0; x < s.W; ++x) seen[y][x] = false;
    seen[sy][sx] = true;
    std::vector<std::pair<int, int>> q{{sx, sy}};
    int count = 0;
    for (size_t h = 0; h < q.size(); ++h) {
        auto [x, y] = q[h];
        for (int d = 0; d < 4; ++d) {
            int nx = x + tron::DX[d], ny = y + tron::DY[d];
            if (tron::is_free(s, nx, ny) && !seen[ny][nx]) {
                seen[ny][nx] = true;
                ++count;
                q.push_back({nx, ny});
            }
        }
    }
    return count;
}

int decide(const tron::State& s) {
    const int me = s.me, straight = s.last_dir[me];
    int order[4] = {straight, 0, 0, 0}, n = 1;
    for (int d = 0; d < 4; ++d)
        if (d != straight) order[n++] = d;
    int best = straight, best_score = -1;
    for (int d : order) {
        if (!tron::can_move(s, me, d)) continue;
        const int score = reachable(s, s.x[me] + tron::DX[d], s.y[me] + tron::DY[d]);
        if (score > best_score) best = d, best_score = score;
    }
    return best;
}
