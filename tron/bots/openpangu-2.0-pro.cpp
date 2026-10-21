#include "tron.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

// ============================================================
// BFS 可达空格数
// ============================================================
static int reachable(const tron::State& s, int sx, int sy) {
    static bool seen[41][41];
    std::memset(seen, 0, sizeof(seen));
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

// ============================================================
// 多源 BFS Voronoi
// ============================================================
static int voronoi(const tron::State& s, int me) {
    static int dist[41][41];
    static int8_t owner[41][41];
    const int INF = 10000;

    std::memset(dist, 0x3f, sizeof(dist));
    std::memset(owner, -1, sizeof(owner));

    std::vector<std::pair<int, int>> q;
    for (int k = 0; k < s.N; ++k) {
        if (!s.alive[k]) continue;
        dist[s.y[k]][s.x[k]] = 0;
        owner[s.y[k]][s.x[k]] = k;
        q.push_back({s.x[k], s.y[k]});
    }

    for (size_t h = 0; h < q.size(); ++h) {
        auto [x, y] = q[h];
        int d = dist[y][x];
        for (int dir = 0; dir < 4; ++dir) {
            int nx = x + tron::DX[dir], ny = y + tron::DY[dir];
            if (!tron::is_free(s, nx, ny)) continue;
            if (dist[ny][nx] > d + 1) {
                dist[ny][nx] = d + 1;
                owner[ny][nx] = owner[y][x];
                q.push_back({nx, ny});
            } else if (dist[ny][nx] == d + 1) {
                if (owner[ny][nx] >= 0 && owner[ny][nx] != owner[y][x])
                    owner[ny][nx] = -2;
            }
        }
    }

    int score = 0;
    for (int y = 0; y < s.H; ++y)
        for (int x = 0; x < s.W; ++x)
            if (owner[y][x] == me) ++score;
    return score;
}

// ============================================================
// 模拟一步
// ============================================================
static tron::State simulate(const tron::State& s, int player, int d) {
    tron::State ns = s;
    int nx = s.x[player] + tron::DX[d];
    int ny = s.y[player] + tron::DY[d];
    ns.cell[ny][nx] = int8_t(player);
    ns.x[player] = nx;
    ns.y[player] = ny;
    ns.last_dir[player] = d;
    ns.turn++;
    return ns;
}

// ============================================================
// 选项数
// ============================================================
static int options(const tron::State& s, int player) {
    int n = 0;
    for (int d = 0; d < 4; ++d)
        if (tron::can_move(s, player, d)) ++n;
    return n;
}

// ============================================================
// 1v1 两层搜索（minimax）
// ============================================================
static int decide_1v1(const tron::State& s) {
    const int me = s.me;
    int opp = -1;
    for (int k = 0; k < s.N; ++k)
        if (s.alive[k] && k != me) { opp = k; break; }

    const int straight = s.last_dir[me];
    int order[4] = {straight, (straight + 1) % 4, (straight + 2) % 4, (straight + 3) % 4};
    int best = straight, best_score = -1000000;

    for (int i = 0; i < 4; ++i) {
        int d = order[i];
        if (!tron::can_move(s, me, d)) continue;

        tron::State ns = simulate(s, me, d);

        int worst = 1000000;
        bool opp_can_move = false;
        for (int od = 0; od < 4; ++od) {
            if (!tron::can_move(ns, opp, od)) continue;
            opp_can_move = true;
            tron::State ns2 = simulate(ns, opp, od);
            int sc = voronoi(ns2, me) + options(ns2, me) * 5;
            if (sc < worst) worst = sc;
        }

        if (!opp_can_move) worst = 1000000;

        if (worst > best_score) {
            best_score = worst;
            best = d;
        }
    }

    return best;
}

// ============================================================
// 3~4 人 FFA：Voronoi + flood fill 混合 + chamber 惩罚
// ============================================================
static int decide_ffa_small(const tron::State& s) {
    const int me = s.me;
    const int straight = s.last_dir[me];
    int order[4] = {straight, (straight + 1) % 4, (straight + 2) % 4, (straight + 3) % 4};
    int best = straight, best_score = -1000000;

    int total_empty = 0;
    for (int y = 0; y < s.H; ++y)
        for (int x = 0; x < s.W; ++x)
            if (s.cell[y][x] == tron::EMPTY) ++total_empty;

    for (int i = 0; i < 4; ++i) {
        int d = order[i];
        if (!tron::can_move(s, me, d)) continue;

        int nx = s.x[me] + tron::DX[d];
        int ny = s.y[me] + tron::DY[d];

        tron::State ns = simulate(s, me, d);
        int r = reachable(ns, nx, ny);
        int v = voronoi(ns, me);
        int opt = options(ns, me);

        int sc = v * 60 / 100 + r * 40 / 100 + opt * 20;

        if (r < total_empty * 0.03) sc -= 800;
        else if (r < total_empty * 0.05) sc -= 400;
        else if (r < total_empty * 0.08) sc -= 150;

        if (sc > best_score) {
            best_score = sc;
            best = d;
        }
    }

    return best;
}

// ============================================================
// 12 人 FFA：纯 flood fill
// ============================================================
static int decide_ffa_12(const tron::State& s) {
    const int me = s.me;
    const int straight = s.last_dir[me];
    int order[4] = {straight, (straight + 1) % 4, (straight + 2) % 4, (straight + 3) % 4};
    int best = straight, best_score = -1000000;

    for (int i = 0; i < 4; ++i) {
        int d = order[i];
        if (!tron::can_move(s, me, d)) continue;

        int nx = s.x[me] + tron::DX[d];
        int ny = s.y[me] + tron::DY[d];

        tron::State ns = simulate(s, me, d);
        int r = reachable(ns, nx, ny);
        int sc = r;

        if (sc > best_score) {
            best_score = sc;
            best = d;
        }
    }

    return best;
}

// ============================================================
// 主入口
// ============================================================
int decide(const tron::State& s) {
    int opps = 0;
    for (int k = 0; k < s.N; ++k)
        if (s.alive[k] && k != s.me) ++opps;

    if (opps == 1) return decide_1v1(s);
    if (s.N >= 12) return decide_ffa_12(s);
    return decide_ffa_small(s);
}
