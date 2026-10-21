// 强力 Tron Bot v13 - 最终版
// 策略：空间控制 + 人数自适应激进 + 快速安全检查
#include "tron.hpp"
#include <vector>
#include <algorithm>
#include <cstring>

static int seen_stamp = 0;
static int seen_data[tron::MAXH][tron::MAXW];

// 快速 flood fill
static int flood(const tron::State& s, int sx, int sy) {
    ++seen_stamp;
    seen_data[sy][sx] = seen_stamp;
    std::vector<std::pair<int, int>> q;
    q.push_back({sx, sy});
    int cnt = 0;
    for (size_t i = 0; i < q.size(); ++i) {
        auto [x, y] = q[i];
        for (int d = 0; d < 4; ++d) {
            int nx = x + tron::DX[d], ny = y + tron::DY[d];
            if (nx >= 0 && nx < s.W && ny >= 0 && ny < s.H && 
                s.cell[ny][nx] == tron::EMPTY && seen_data[ny][nx] != seen_stamp) {
                seen_data[ny][nx] = seen_stamp;
                ++cnt;
                q.push_back({nx, ny});
            }
        }
    }
    return cnt;
}

// 位置安全度
static int pos_safe(const tron::State& s, int x, int y) {
    int cnt = 0;
    for (int d = 0; d < 4; ++d) {
        if (tron::is_free(s, x + tron::DX[d], y + tron::DY[d])) ++cnt;
    }
    return cnt;
}

// 评估移动
static int score(const tron::State& s, int me, int d) {
    if (!tron::can_move(s, me, d)) return -100000;
    
    int nx = s.x[me] + tron::DX[d], ny = s.y[me] + tron::DY[d];
    
    // 抢格检查
    for (int k = 0; k < s.N; ++k) {
        if (k == me || !s.alive[k]) continue;
        int kx = s.x[k] + tron::DX[s.last_dir[k]], ky = s.y[k] + tron::DY[s.last_dir[k]];
        if (kx == nx && ky == ny && tron::priority(s, k) <= tron::priority(s, me)) {
            return -80000;
        }
    }
    
    int reach = flood(s, nx, ny);
    int safe = pos_safe(s, nx, ny);
    
    if (reach < 4) return -40000;
    if (safe < 1) return -50000;
    
    int safe_val = safe * 20;
    
    // 距离对手
    int min_dist = s.W + s.H;
    int max_dist = 0;
    for (int k = 0; k < s.N; ++k) {
        if (k == me || !s.alive[k]) continue;
        int dd = std::abs(nx - s.x[k]) + std::abs(ny - s.y[k]);
        if (dd < min_dist) min_dist = dd;
        if (dd > max_dist) max_dist = dd;
    }
    
    // 根据人数调整激进程度
    int aggression = (s.N == 2) ? 0 : ((s.N <= 4) ? 25 : 40);
    
    return reach * 8 + safe_val - (s.W + s.H - min_dist) / 2 + aggression - max_dist / 10;
}

// 选择最佳
static int choose(const tron::State& s, int me) {
    int best_d = tron::U, best_sc = -1000000;
    
    int straight = s.last_dir[me];
    if (tron::can_move(s, me, straight)) {
        int sc = score(s, me, straight);
        if (sc > best_sc) { best_sc = sc; best_d = straight; }
    }
    
    for (int d = 0; d < 4; ++d) {
        if (d == straight) continue;
        if (!tron::can_move(s, me, d)) continue;
        int sc = score(s, me, d);
        if (sc > best_sc) { best_sc = sc; best_d = d; }
    }
    
    return best_d;
}

// 紧急
static int urgent(const tron::State& s, int me) {
    for (int d = 0; d < 4; ++d) {
        if (tron::can_move(s, me, d)) return d;
    }
    return tron::U;
}

int decide(const tron::State& s) {
    const int me = s.me;
    const double budget = tron::budget_left();
    
    if (s.turn == 0 && budget > 200) {
        std::vector<std::pair<int, int>> opts;
        for (int d = 0; d < 4; ++d) {
            if (tron::can_move(s, me, d)) {
                opts.push_back({score(s, me, d), d});
            }
        }
        if (!opts.empty()) {
            std::sort(opts.begin(), opts.end(), std::greater<>());
            return opts[0].second;
        }
    }
    
    if (budget > 5) {
        return choose(s, me);
    }
    
    return urgent(s, me);
}
