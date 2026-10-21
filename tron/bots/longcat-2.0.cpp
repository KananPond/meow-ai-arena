// Strong Tron bot: flood fill + 2-ply safety + Voronoi for crowd + 1v1 minimax
//
// Strategy:
// 1. For 2-4 player: flood fill + 2-ply safety check (avoid dead ends).
// 2. For 12 player: Voronoi territory control (navigate the crowd).
// 3. For 1v1: deep alpha-beta minimax with Voronoi + flood fill evaluation.
// 4. Tiebreaker: prefer going straight.

#include "tron.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>

using namespace tron;

// ---- Voronoi ----
static int vdist[MAXH][MAXW];
static int vowner[MAXH][MAXW];
static int vqx[MAXH * MAXW], vqy[MAXH * MAXW], vqo[MAXH * MAXW];

static void compute_voronoi(const State& s, int terr[MAXN]) {
    const int W = s.W, H = s.H, N = s.N;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            vdist[y][x] = -1, vowner[y][x] = -1;

    int head = 0, tail = 0;
    for (int k = 0; k < N; k++) if (s.alive[k]) {
        for (int d = 0; d < 4; d++) {
            int nx = s.x[k] + DX[d], ny = s.y[k] + DY[d];
            if (nx >= 0 && nx < W && ny >= 0 && ny < H &&
                s.cell[ny][nx] == EMPTY && vdist[ny][nx] == -1) {
                vdist[ny][nx] = 1;
                vowner[ny][nx] = k;
                vqx[tail] = nx; vqy[tail] = ny; vqo[tail] = k; tail++;
            }
        }
    }

    while (head < tail) {
        int x = vqx[head], y = vqy[head], o = vqo[head++];
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            if (s.cell[ny][nx] != EMPTY || vdist[ny][nx] != -1) continue;
            vdist[ny][nx] = vdist[y][x] + 1;
            vowner[ny][nx] = o;
            vqx[tail] = nx; vqy[tail] = ny; vqo[tail] = o; tail++;
        }
    }

    for (int k = 0; k < N; k++) terr[k] = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (vowner[y][x] >= 0)
                terr[vowner[y][x]]++;
}

// ---- Flood fill ----
static bool ff_seen[MAXH][MAXW];
static int fqx[MAXH * MAXW], fqy[MAXH * MAXW];

static int flood_fill(const State& s, int sx, int sy) {
    const int W = s.W, H = s.H;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            ff_seen[y][x] = false;

    int head = 0, tail = 0, count = 0;
    fqx[tail] = sx; fqy[tail] = sy; tail++;
    ff_seen[sy][sx] = true;

    while (head < tail) {
        int x = fqx[head], y = fqy[head]; head++;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx >= 0 && nx < W && ny >= 0 && ny < H &&
                s.cell[ny][nx] == EMPTY && !ff_seen[ny][nx]) {
                ff_seen[ny][nx] = true;
                count++;
                fqx[tail] = nx; fqy[tail] = ny; tail++;
            }
        }
    }
    return count;
}

// ---- Evaluate a move for multiplayer ----
// Uses flood fill with 2-ply safety check to avoid dead ends
static int eval_move_24(const State& s, int me, int dir) {
    int nx = s.x[me] + DX[dir], ny = s.y[me] + DY[dir];
    if (!is_free(s, nx, ny)) return -1000000;

    State sim = s;
    sim.cell[ny][nx] = (int8_t)me;
    sim.x[me] = nx; sim.y[me] = ny;

    int my_space = flood_fill(sim, nx, ny);

    // 2-ply safety: minimum flood fill among next moves (avoid dead ends)
    int min_next = 1000000;
    int next_count = 0;
    for (int d = 0; d < 4; d++) {
        if (d == opposite(dir)) continue;
        int nnx = nx + DX[d], nny = ny + DY[d];
        if (nnx < 0 || nnx >= s.W || nny < 0 || nny >= s.H) continue;
        if (sim.cell[nny][nnx] != EMPTY) continue;
        State sim2 = sim;
        sim2.cell[nny][nnx] = (int8_t)me;
        sim2.x[me] = nnx; sim2.y[me] = nny;
        int next_space = flood_fill(sim2, nnx, nny);
        if (next_space < min_next) min_next = next_space;
        next_count++;
    }

    if (next_count == 0) return -500000;

    return my_space * 70 + min_next * 30;
}

// ---- Evaluate a move for 12-player (Voronoi-based) ----
static int eval_move_12(const State& s, int me, int dir) {
    int nx = s.x[me] + DX[dir], ny = s.y[me] + DY[dir];
    if (!is_free(s, nx, ny)) return -1000000;

    State sim = s;
    sim.cell[ny][nx] = (int8_t)me;
    sim.x[me] = nx; sim.y[me] = ny;

    int terr[MAXN];
    compute_voronoi(sim, terr);
    return terr[me] * 100;
}

// ---- 1v1 minimax ----
static int eval_1v1(const State& s, int me, int opp) {
    if (!s.alive[me]) return -100000;
    if (!s.alive[opp]) return 100000;

    int terr[MAXN];
    compute_voronoi(s, terr);
    int voronoi_score = (terr[me] - terr[opp]) * 10;

    int my_space = flood_fill(s, s.x[me], s.y[me]);
    int opp_space = flood_fill(s, s.x[opp], s.y[opp]);
    int space_score = (my_space - opp_space) * 5;

    return voronoi_score + space_score;
}

static int search_1v1(const State& s, int depth, int alpha, int beta, int me, int opp) {
    if (budget_left() < 2) return eval_1v1(s, me, opp);
    if (s.over || depth == 0) return eval_1v1(s, me, opp);

    int my_moves[4], my_n = 0;
    int opp_moves[4], opp_n = 0;
    for (int d = 0; d < 4; d++) {
        if (can_move(s, me, d)) my_moves[my_n++] = d;
        if (can_move(s, opp, d)) opp_moves[opp_n++] = d;
    }
    if (my_n == 0) return -100000;
    if (opp_n == 0) return 100000;

    int best = -1000000;
    for (int i = 0; i < my_n; i++) {
        int worst = 1000000;
        for (int j = 0; j < opp_n; j++) {
            State sim = s;
            int mv[MAXN];
            for (int k = 0; k < MAXN; k++) mv[k] = -1;
            mv[me] = my_moves[i];
            mv[opp] = opp_moves[j];
            step(sim, mv);

            int val = search_1v1(sim, depth - 1, alpha, beta, me, opp);
            if (val < worst) worst = val;
            if (worst <= best) break;
        }
        if (worst > best) {
            best = worst;
            if (best > alpha) alpha = best;
        }
        if (best >= beta) break;
    }
    return best;
}

// ---- Main decide function ----
int decide(const State& s) {
    const int me = s.me;

    int legal[4], n_legal = 0;
    for (int d = 0; d < 4; d++)
        if (can_move(s, me, d)) legal[n_legal++] = d;

    if (n_legal == 0) return U;
    if (n_legal == 1) return legal[0];

    int alive = 0;
    for (int k = 0; k < s.N; k++) alive += s.alive[k];

    // ---- 1v1: deep minimax search ----
    if (alive == 2) {
        int opp = -1;
        for (int k = 0; k < s.N; k++)
            if (k != me && s.alive[k]) { opp = k; break; }

        if (opp >= 0) {
            int best_move = legal[0];

            int opp_moves[4], opp_n = 0;
            for (int d = 0; d < 4; d++)
                if (can_move(s, opp, d)) opp_moves[opp_n++] = d;
            if (opp_n == 0) return legal[0];

            for (int depth = 2; depth <= 10; depth += 2) {
                if (budget_left() < 10) break;

                int alpha = -1000000, beta = 1000000;
                int local_best = -1000000;
                int local_move = legal[0];
                bool completed = true;

                for (int i = 0; i < n_legal; i++) {
                    if (budget_left() < 3) { completed = false; break; }
                    int worst = 1000000;
                    for (int j = 0; j < opp_n; j++) {
                        State sim = s;
                        int mv[MAXN];
                        for (int k = 0; k < MAXN; k++) mv[k] = -1;
                        mv[me] = legal[i];
                        mv[opp] = opp_moves[j];
                        step(sim, mv);
                        int val = search_1v1(sim, depth - 1, alpha, beta, me, opp);
                        if (val < worst) worst = val;
                        if (worst <= local_best) break;
                    }
                    if (worst > local_best) {
                        local_best = worst;
                        local_move = legal[i];
                    }
                    if (local_best > alpha) alpha = local_best;
                }

                if (completed) best_move = local_move;
                else break;
            }
            return best_move;
        }
    }

    // ---- Multiplayer: pick best move ----
    int best_move = legal[0];
    int best_score = -1000000;

    for (int i = 0; i < n_legal; i++) {
        if (budget_left() < 5) break;

        int score;
        if (s.N >= 10) {
            score = eval_move_12(s, me, legal[i]);
        } else {
            score = eval_move_24(s, me, legal[i]);
        }

        // Tiebreaker: prefer going straight
        if (legal[i] == s.last_dir[me]) score += 1;

        if (score > best_score) {
            best_score = score;
            best_move = legal[i];
        }
    }

    return best_move;
}
