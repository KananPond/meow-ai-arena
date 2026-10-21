// Mo Hen Meow. Reference ideas: official GPT-6 Astra and the public baseline.
// Only the public stdin protocol is used. All persistent state is match-local.
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <queue>
#include <string>
#include <utility>
#include <vector>
using namespace std;

constexpr int M = 625, H = 12, INF = 10000;
using Clock = chrono::steady_clock;
const char *directions = "UDLRS";
int n, playerCount, me, turn, limit, fuse, radius, capacity, fireLength, shrink, interval;
int cells, extinction, neighbor[M][5], falls[M], previous[10];
array<char, M> board;
array<double, M> visits{}, centerCost{}, field{}, crateValue{}, tunnel{};
array<int, M> myDistance{}, enemyDistance{}, degree{};
int aliveCount, lastOrigin = -1, lastDirection = 4, blockedTurns = 0;
double reachableCenter;
uint64_t randomState = 0x8a5cd789635d2dffULL;
struct Player { int cell = 0, alive = 0; double kills = 0; };
struct Bomb { int owner, cell, at, born; };
struct Flame { int cell, end; };
struct Source { int cell, owner, end; };
vector<Player> players;
vector<Bomb> bombs;
vector<Flame> flames;
vector<Source> sources;

double randomUnit() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 7;
    randomState ^= randomState << 17;
    return (randomState >> 11) * 0x1.0p-53;
}

bool canPlace(int id) {
    int active = 0;
    for (auto b : bombs) if (falls[b.cell] > turn) {
        if (b.cell == players[id].cell) return false;
        active += b.owner == id;
    }
    return active < capacity && falls[players[id].cell] > turn;
}

struct Forecast {
    int horizon;
    uint8_t open[H][M]{}, occupied[H][M]{}, safe[H][M]{}, viable[H + 1][M]{};
    uint16_t owners[H][M]{};
    array<int, M> destroyed;

    Forecast(vector<Bomb> pending, bool provenance = false) {
        horizon = max(1, min({H, limit - turn, extinction - turn}));
        auto grid = board;
        array<int, M> expiry;
        expiry.fill(-1);
        destroyed.fill(INF);
        int sourceExpiry[10][M];
        if (provenance) {
            for (auto &row : sourceExpiry) fill(row, row + M, -1);
            for (auto s : sources) sourceExpiry[s.owner][s.cell] = s.end;
        }
        for (auto f : flames) expiry[f.cell] = max(expiry[f.cell], f.end);
        for (int k = 0; k < horizon; ++k) {
            int now = turn + k;
            for (int c = 0; c < cells; ++c) if (falls[c] <= now) {
                grid[c] = '#';
                expiry[c] = -1;
            }
            erase_if(pending, [&](Bomb b) { return grid[b.cell] == '#'; });
            array<int, M> at;
            at.fill(-1);
            for (int i = 0; i < int(pending.size()); ++i) if (pending[i].born <= now) {
                at[pending[i].cell] = i;
                occupied[k][pending[i].cell] = 1;
            }
            for (int c = 0; c < cells; ++c) open[k][c] = grid[c] == '.';
            vector<uint8_t> exploded(pending.size());
            vector<int> queue;
            auto enqueue = [&](int i) {
                if (!exploded[i]) { exploded[i] = 1; queue.push_back(i); }
            };
            for (int i = 0; i < int(pending.size()); ++i)
                if (pending[i].born <= now &&
                    (pending[i].at <= now || expiry[pending[i].cell] >= now)) enqueue(i);
            array<uint16_t, M> hit{};
            for (size_t head = 0; head < queue.size(); ++head) {
                auto b = pending[queue[head]];
                hit[b.cell] |= 1U << b.owner;
                for (int d = 0; d < 4; ++d) {
                    int z = b.cell;
                    for (int r = 0; r < radius; ++r) {
                        z = neighbor[z][d];
                        if (z < 0 || grid[z] == '#') break;
                        hit[z] |= 1U << b.owner;
                        if (grid[z] == '+') break;
                        if (at[z] >= 0) { enqueue(at[z]); break; }
                    }
                }
            }
            vector<Bomb> remaining;
            for (int i = 0; i < int(pending.size()); ++i)
                if (!exploded[i]) remaining.push_back(pending[i]);
            pending = move(remaining);
            for (int c = 0; c < cells; ++c) {
                if (hit[c]) {
                    expiry[c] = max(expiry[c], now + fireLength - 1);
                    if (grid[c] == '+') { grid[c] = '.'; destroyed[c] = k; }
                }
                safe[k][c] = grid[c] == '.' && expiry[c] < now;
                if (provenance && grid[c] != '#') for (int id = 0; id < playerCount; ++id) {
                    if (hit[c] & (1U << id)) sourceExpiry[id][c] = max(sourceExpiry[id][c], now + fireLength - 1);
                    if (sourceExpiry[id][c] >= now) owners[k][c] |= 1U << id;
                }
            }
        }
        rebuild();
    }
    int destination(int k, int c, int d) const {
        int z = neighbor[c][d];
        return z >= 0 && open[k][z] && (!occupied[k][z] || z == c) ? z : c;
    }
    void rebuild() {
        for (int c = 0; c < cells; ++c) viable[horizon][c] = 1;
        for (int k = horizon - 1; k >= 0; --k) for (int c = 0; c < cells; ++c) {
            viable[k][c] = 0;
            if (open[k][c]) for (int d = 0; d < 5; ++d) {
                int z = destination(k, c, d);
                if (safe[k][z] && viable[k + 1][z]) { viable[k][c] = 1; break; }
            }
        }
    }
};

struct Mobility {
    int life[5]{}, count[5]{}, width[5], space[H][5]{}, total = 0;
    double utility[5];
    Mobility() { fill(width, width + 5, INF); fill(utility, utility + 5, -1e6); }
};

Mobility mobility(const Forecast &f, int start, bool withUtility = false) {
    Mobility result;
    array<uint8_t, M> current{}, next{};
    current[start] = 31;
    for (int k = 0; k < f.horizon; ++k) {
        next.fill(0);
        for (int c = 0; c < cells; ++c) if (current[c] && f.open[k][c]) {
            for (int d = 0; d < 5; ++d) {
                int z = f.destination(k, c, d);
                if (f.safe[k][z]) next[z] |= k ? current[c] : uint8_t(1U << d);
            }
        }
        for (int c = 0; c < cells; ++c) if (next[c]) {
            for (int d = 0; d < 5; ++d) if (next[c] & (1U << d)) {
                result.life[d] = k + 1;
                if (f.viable[k + 1][c]) ++result.space[k][d];
                if (k == f.horizon - 1) {
                    ++result.count[d];
                    if (withUtility) result.utility[d] = max(result.utility[d],
                        field[c] > -1e5 ? field[c] : -.3 * centerCost[c]);
                }
            }
        }
        current = next;
    }
    for (int c = 0; c < cells; ++c) result.total += current[c] != 0;
    for (int d = 0; d < 5; ++d)
        for (int k = 1; k < min(7, f.horizon); ++k)
            result.width[d] = min(result.width[d], result.space[k][d]);
    return result;
}

array<int, M> distances(int start, const uint8_t *open = nullptr) {
    array<int, M> result;
    result.fill(INF);
    int queue[M], head = 0, tail = 0;
    result[start] = 0; queue[tail++] = start;
    while (head < tail) {
        int c = queue[head++];
        for (int d = 0; d < 4; ++d) {
            int z = neighbor[c][d];
            if (z >= 0 && (open ? bool(open[z]) : board[z] == '.') && result[z] == INF) {
                result[z] = result[c] + 1; queue[tail++] = z;
            }
        }
    }
    return result;
}

void objectives(const Forecast &normal) {
    int origin = players[me].cell;
    for (int c = 0; c < cells; ++c) visits[c] *= .89;
    visits[origin] += 1;
    myDistance = distances(origin);
    enemyDistance.fill(INF);
    aliveCount = 0;
    for (int id = 0; id < playerCount; ++id) if (players[id].alive) {
        ++aliveCount;
        if (id != me) {
            auto d = distances(players[id].cell);
            for (int c = 0; c < cells; ++c) enemyDistance[c] = min(enemyDistance[c], d[c]);
        }
    }
    priority_queue<pair<double, int>, vector<pair<double, int>>, greater<pair<double, int>>> queue;
    centerCost.fill(INF);
    int deepest = 0;
    for (int c = 0; c < cells; ++c) if (board[c] != '#') deepest = max(deepest, falls[c]);
    for (int c = 0; c < cells; ++c) if (board[c] != '#' && falls[c] >= deepest - interval) {
        centerCost[c] = double(deepest - falls[c]) / interval;
        queue.push({centerCost[c], c});
    }
    while (!queue.empty()) {
        auto [cost, c] = queue.top(); queue.pop();
        if (cost != centerCost[c]) continue;
        for (int d = 0; d < 4; ++d) {
            int z = neighbor[c][d];
            if (z < 0 || board[z] == '#') continue;
            double candidate = cost + (board[c] == '+' ? 7. : 1.);
            if (candidate < centerCost[z]) { centerCost[z] = candidate; queue.push({candidate, z}); }
        }
    }
    reachableCenter = INF;
    for (int c = 0; c < cells; ++c) if (myDistance[c] < INF) reachableCenter = min(reachableCenter, centerCost[c]);
    for (int c = 0; c < cells; ++c) {
        degree[c] = 0; crateValue[c] = 0; tunnel[c] = 0;
        for (int d = 0; d < 4; ++d) {
            int z = neighbor[c][d];
            if (z >= 0 && board[z] == '.') ++degree[c];
        }
        if (board[c] != '.') continue;
        for (int d = 0; d < 4; ++d) {
            int z = c;
            for (int r = 0; r < radius; ++r) {
                z = neighbor[z][d];
                if (z < 0 || board[z] == '#') break;
                if (board[z] == '+') {
                    if (normal.destroyed[z] == INF)
                        crateValue[c] += (reachableCenter <= 2 ? .35 : 1.) + .12 * max(0., centerCost[c] - centerCost[z]);
                    break;
                }
            }
        }
    }
    for (int c = 0; c < cells; ++c) if (board[c] == '.' && degree[c] <= 2) {
        int exits = 0, contested = 0, shortest = INF;
        for (int d = 0; d < 4; ++d) {
            int z = c, length = 0;
            while (true) {
                z = neighbor[z][d]; ++length;
                if (z < 0 || board[z] != '.') break;
                bool junction = false;
                for (int e = 0; e < 4; ++e) if (e / 2 != d / 2) {
                    int v = neighbor[z][e]; junction |= v >= 0 && board[v] == '.';
                }
                if (junction) {
                    ++exits; contested += enemyDistance[z] <= length + 1;
                    shortest = min(shortest, length); break;
                }
            }
        }
        if (exits && contested == exits) tunnel[c] = (exits == 1 ? 13. : 8.) + 1.4 * min(shortest, 5);
    }
    priority_queue<pair<double, int>> goals;
    field.fill(-1e6);
    for (int c = 0; c < cells; ++c) if (board[c] == '.') {
        double value = -(turn > shrink - 45 ? .42 : .26) * centerCost[c];
        value += .35 * min(degree[c], 3) + 1.65 * crateValue[c] - .42 * visits[c];
        int remaining = falls[c] - turn;
        value -= max(0, 30 - remaining);
        if (remaining < myDistance[c] + 10) value -= 20;
        if (enemyDistance[c] < INF) {
            if (aliveCount == 2) value += max(-3., 2.4 - .55 * abs(enemyDistance[c] - 4));
            else value -= 1.15 * max(0, 7 - enemyDistance[c]);
        }
        field[c] = value; goals.push({value, c});
    }
    while (!goals.empty()) {
        auto [value, c] = goals.top(); goals.pop();
        if (value + 1e-8 < field[c]) continue;
        for (int d = 0; d < 4; ++d) {
            int z = neighbor[c][d];
            if (z >= 0 && board[z] == '.' && field[z] + 1e-8 < value - .72) {
                field[z] = value - .72; goals.push({field[z], z});
            }
        }
    }
}

struct Threat { int owner, cell, delay; double weight; int secondCell = -1, secondDelay = 0; };
vector<Threat> threats(const Forecast &f) {
    auto distance = distances(players[me].cell, f.open[f.horizon - 1]);
    vector<Threat> result, paired;
    for (int id = 0; id < playerCount; ++id) if (id != me && players[id].alive && distance[players[id].cell] <= 9) {
        array<uint8_t, M> current{}, next{};
        current[players[id].cell] = 1;
        int depth = distance[players[id].cell] <= 6 ? 3 : 1;
        for (int k = 0; k <= depth && k < f.horizon; ++k) {
            int active = 0;
            for (auto b : bombs) active += b.owner == id && f.occupied[k][b.cell];
            next.fill(0);
            for (int c = 0; c < cells; ++c) if (current[c] && f.open[k][c]) {
                if (active < capacity && !f.occupied[k][c] && distance[c] <= 7)
                    result.push_back({id, c, k, pow(.84, k)});
                for (int d = 0; d < 5; ++d) {
                    int z = f.destination(k, c, d);
                    if (f.safe[k][z]) next[z] = 1;
                }
            }
            current = next;
        }
        // Follow legal routes to a second bomb that can extend the first blast.
        if (f.horizon > 1 && canPlace(id)) {
            int first = players[id].cell;
            array<uint8_t, M> reachable{}, following{}, seen{};
            reachable[first] = 1;
            for (int k = 0; k <= depth && k < f.horizon; ++k) {
                int active = 1;
                for (auto b : bombs) active += b.owner == id && f.occupied[k][b.cell];
                following.fill(0);
                for (int c = 0; c < cells; ++c) if (reachable[c] && f.open[k][c]) {
                    int distanceToFirst = abs(c % n - first % n) + abs(c / n - first / n);
                    if (k && c != first && !seen[c] && active < capacity && !f.occupied[k][c] &&
                        (c % n == first % n || c / n == first / n) && distanceToFirst <= radius) {
                        paired.push_back({id, first, 0, .60 * pow(.84, k - 1), c, k});
                        seen[c] = 1;
                    }
                    for (int d = 0; d < 5; ++d) {
                        int z = f.destination(k, c, d);
                        if ((z != first || c == first) && f.safe[k][z]) following[z] = 1;
                    }
                }
                reachable = following;
            }
        }
    }
    auto before = [&](Threat a, Threat b) {
        return 2. * max(a.delay, a.secondDelay) + .3 * distance[a.cell] <
               2. * max(b.delay, b.secondDelay) + .3 * distance[b.cell];
    };
    stable_sort(result.begin(), result.end(), before);
    stable_sort(paired.begin(), paired.end(), before);
    if (result.size() > 48) result.resize(48);
    if (paired.size() > 12) paired.resize(12);
    result.insert(result.end(), paired.begin(), paired.end());
    return result;
}

struct Choice { int direction = 4, place = 0; double score = -1e100; };
struct BudgetExceeded {};
Clock::time_point collisionDeadline;

// The movement game includes legal swaps and intentional blocking, with a
// fixed bomb schedule. Future bomb placements are covered separately below.
struct MovementGame {
    const Forecast &f;
    int enemy, horizon, count = 0, index[M], nodes = 0;
    vector<int16_t> memo;
    MovementGame(const Forecast &forecast, int rival) : f(forecast), enemy(rival), horizon(min(7, f.horizon)) {
        fill(index, index + M, -1);
        for (int c = 0; c < cells; ++c) if (board[c] != '#') {
            int a = players[me].cell, b = players[enemy].cell;
            int da = abs(c % n - a % n) + abs(c / n - a / n);
            int db = abs(c % n - b % n) + abs(c / n - b / n);
            if (min(da, db) <= horizon) index[c] = count++;
        }
        memo.assign(horizon * count * count, 32767);
    }
    bool canLive(int k, int a, int b, int na, int nb) const {
        if (!f.open[k][b]) return false;
        if (f.open[k][a] && na == nb) nb = b;
        return f.safe[k][nb] && f.viable[k + 1][nb];
    }
    int transition(int k, int a, int b, int na, int nb) {
        bool activeA = f.open[k][a], activeB = f.open[k][b];
        if (!activeA) na = a;
        if (!activeB) nb = b;
        if (activeA && activeB && na == nb) { na = a; nb = b; }
        bool liveA = activeA && f.safe[k][na], liveB = activeB && f.safe[k][nb];
        if (!liveA || !liveB) {
            if (aliveCount > 2) return !liveA ? (liveB ? -1000 : -750) : (f.viable[k + 1][na] ? 700 : -500);
            double difference = players[me].kills - players[enemy].kills;
            if (!liveA && activeA) {
                unsigned owners = f.owners[k][na] & ~(1U << me);
                if (owners & (1U << enemy)) difference -= 1. / popcount(owners);
            }
            if (!liveB && activeB) {
                unsigned owners = f.owners[k][nb] & ~(1U << enemy);
                if (owners & (1U << me)) difference += 1. / popcount(owners);
            }
            double score = int(liveA) - int(liveB) + .5 * difference;
            return score > 1e-8 ? 1000 : score < -1e-8 ? -1000 : 0;
        }
        return solve(k + 1, na, nb);
    }
    int solve(int k, int a, int b) {
        if (k >= horizon) return 0;
        if ((++nodes & 255) == 0 && Clock::now() >= collisionDeadline) throw BudgetExceeded{};
        int key = (k * count + index[a]) * count + index[b];
        if (memo[key] != 32767) return memo[key];
        int best = -1001, aa[5], bb[5], an = 0, bn = 0;
        for (int d = 0; d < 5; ++d) {
            int z = f.destination(k, a, d);
            if (find(aa, aa + an, z) == aa + an) aa[an++] = z;
            z = f.destination(k, b, d);
            if (find(bb, bb + bn, z) == bb + bn) bb[bn++] = z;
        }
        for (int i = 0; i < an; ++i) {
            int worst = 1001;
            bool rational = false;
            if (aliveCount > 2) for (int j = 0; j < bn; ++j) rational |= canLive(k, a, b, aa[i], bb[j]);
            for (int j = 0; j < bn; ++j) {
                if (rational && !canLive(k, a, b, aa[i], bb[j])) continue;
                worst = min(worst, transition(k, a, b, aa[i], bb[j]));
                if (worst <= best) break;
            }
            best = max(best, worst);
        }
        return memo[key] = best;
    }
};

void collisionValues(double value[2][5], Clock::time_point start) {
    collisionDeadline = start + chrono::milliseconds(5);
    vector<pair<int, int>> rivals;
    for (int id = 0; id < playerCount; ++id)
        if (id != me && players[id].alive && myDistance[players[id].cell] <= (aliveCount == 2 ? 9 : 6))
            rivals.push_back({myDistance[players[id].cell], id});
    sort(rivals.begin(), rivals.end());
    if (rivals.size() > 2) rivals.resize(2);
    for (auto [distance, id] : rivals) {
        double local[2][5]{};
        try {
            for (int place = 0; place < 2; ++place) {
                if (place && !canPlace(me)) continue;
                int worst[5]; fill(worst, worst + 5, 1001);
                double average[5]{}; int samples = 0;
                for (int other = 0; other < 2; ++other) {
                    if (other && !canPlace(id)) continue;
                    auto bs = bombs;
                    if (place) bs.push_back({me, players[me].cell, turn + fuse, turn});
                    if (other) bs.push_back({id, players[id].cell, turn + fuse, turn});
                    Forecast f(bs, true); MovementGame game(f, id);
                    int moves[5], count = 0;
                    for (int d = 0; d < 5; ++d) {
                        int z = f.destination(0, players[id].cell, d);
                        if (find(moves, moves + count, z) == moves + count) moves[count++] = z;
                    }
                    for (int d = 0; d < 5; ++d) {
                        int z = f.destination(0, players[me].cell, d);
                        bool rational = false;
                        if (aliveCount > 2) for (int j = 0; j < count; ++j)
                            rational |= game.canLive(0, players[me].cell, players[id].cell, z, moves[j]);
                        for (int j = 0; j < count; ++j) {
                            if (rational && !game.canLive(0, players[me].cell, players[id].cell, z, moves[j])) continue;
                            int score = game.transition(0, players[me].cell, players[id].cell, z, moves[j]);
                            worst[d] = min(worst[d], score); average[d] += score;
                        }
                    }
                    samples += count;
                }
                for (int d = 0; d < 5; ++d)
                    local[place][d] = (worst[d] < 0 ? 2. : .06) * worst[d] + .012 * average[d] / max(1, samples);
            }
        } catch (const BudgetExceeded &) { return; }
        for (int b = 0; b < 2; ++b) for (int d = 0; d < 5; ++d) value[b][d] += local[b][d];
    }
}

// Small endgames use the complete game, including both players' future bombs.
// C++20 bit operations keep the simulator portable and within the turn budget.
namespace Endgame {
using Bits = uint64_t;
struct State {
    Bits crates = 0, bomb[2][5]{}, fire[2]{};
    uint8_t pos[2]{}, time = 0;
    int16_t result = 2001;
};
struct Entry { Bits key = 0; int16_t value = 0; uint8_t depth = 0, flag = 0; uint32_t generation = 0; };
Entry table[1 << 18];
uint32_t generation = 0;

struct Solver {
    int enemy, size = 0, map[M], cell[64], next[64][5], rays[64][4][3], nodes = 0;
    Bits bit[64], available[32], adjacent[64], zCrate[64], zPos[2][64], zBomb[2][5][64], zFire[2][64], zTime[32];
    double killDifference;
    State initial;
    Clock::time_point deadline;
    bool aborted = false;
    Solver(int rival, Clock::time_point end) : enemy(rival), killDifference(players[me].kills - players[rival].kills), deadline(end) {
        fill(map, map + M, -1);
        for (int c = 0; c < cells; ++c) if (board[c] != '#') {
            map[c] = size; cell[size] = c; bit[size] = Bits(1) << size; ++size;
        }
        for (int i = 0; i < size; ++i) {
            int c = cell[i]; adjacent[i] = 0;
            if (board[c] == '+') initial.crates |= bit[i];
            for (int d = 0; d < 5; ++d) {
                int z = neighbor[c][d]; next[i][d] = z < 0 ? -1 : map[z];
                if (d < 4 && next[i][d] >= 0) adjacent[i] |= bit[next[i][d]];
            }
            for (int d = 0; d < 4; ++d) {
                int z = c;
                for (int r = 0; r < 3; ++r) { z = z < 0 ? -1 : neighbor[z][d]; rays[i][d][r] = z < 0 ? -1 : map[z]; }
            }
        }
        for (int k = 0; k < 32; ++k) {
            available[k] = 0;
            for (int i = 0; i < size; ++i) if (falls[cell[i]] > turn + k) available[k] |= bit[i];
        }
        initial.pos[0] = map[players[me].cell]; initial.pos[1] = map[players[enemy].cell];
        for (auto b : bombs) if (map[b.cell] >= 0)
            initial.bomb[b.owner == me ? 0 : 1][clamp(b.at - turn, 0, 4)] |= bit[map[b.cell]];
        for (auto s : sources) if (s.end >= turn && map[s.cell] >= 0)
            initial.fire[s.owner == me ? 0 : 1] |= bit[map[s.cell]];
        Bits seed = 0x1e390dc89f3795bdULL;
        auto hashNumber = [&]() {
            seed += 0x9e3779b97f4a7c15ULL; Bits z = seed;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL; return z ^ (z >> 31);
        };
        for (int i = 0; i < size; ++i) {
            zCrate[i] = hashNumber();
            for (int p = 0; p < 2; ++p) {
                zPos[p][i] = hashNumber(); zFire[p][i] = hashNumber();
                for (int k = 0; k < 5; ++k) zBomb[p][k][i] = hashNumber();
            }
        }
        for (auto &z : zTime) z = hashNumber();
        if (++generation == 0) { for (auto &entry : table) entry.generation = 0; ++generation; }
    }
    Bits occupied(const State &s) const {
        Bits result = 0; for (auto &owner : s.bomb) for (Bits b : owner) result |= b; return result;
    }
    Bits neighbors(Bits mask) const {
        Bits result = 0;
        while (mask) { int c = countr_zero(mask); mask &= mask - 1; result |= adjacent[c]; }
        return result;
    }
    State step(State s, int a, int b, Bits *dangerOut = nullptr, Bits *walkOut = nullptr) const {
        Bits valid = available[min(31, int(s.time))];
        s.crates &= valid;
        for (int p = 0; p < 2; ++p) { s.fire[p] &= valid; for (auto &bomb : s.bomb[p]) bomb &= valid; }
        bool alive[2] = {bool(valid & bit[s.pos[0]]), bool(valid & bit[s.pos[1]])};
        Bits occ = occupied(s);
        int actions[2] = {a, b};
        for (int p = 0; p < 2; ++p) if (alive[p] && actions[p] >= 5 && !(occ & bit[s.pos[p]])) {
            Bits own = 0; for (Bits bomb : s.bomb[p]) own |= bomb;
            if (popcount(own) < capacity) { s.bomb[p][4] |= bit[s.pos[p]]; occ |= bit[s.pos[p]]; }
        }
        Bits floor = valid & ~s.crates;
        int target[2];
        for (int p = 0; p < 2; ++p) {
            int c = s.pos[p], z = next[c][actions[p] % 5];
            target[p] = alive[p] && z >= 0 && (floor & bit[z]) && (!(occ & bit[z]) || z == c) ? z : c;
        }
        if (alive[0] && alive[1] && target[0] == target[1]) { target[0] = s.pos[0]; target[1] = s.pos[1]; }
        Bits old = s.fire[0] | s.fire[1], queued = s.bomb[0][0] | s.bomb[1][0] | (old & occ);
        Bits gone = 0, hit[2]{}, owner0 = 0;
        for (Bits bomb : s.bomb[0]) owner0 |= bomb;
        while (queued) {
            int c = countr_zero(queued); queued &= queued - 1;
            if (gone & bit[c]) continue;
            gone |= bit[c]; int owner = (owner0 & bit[c]) ? 0 : 1; hit[owner] |= bit[c];
            for (int d = 0; d < 4; ++d) for (int r = 0; r < radius; ++r) {
                int z = rays[c][d][r];
                if (z < 0 || !(valid & bit[z])) break;
                hit[owner] |= bit[z];
                if (s.crates & bit[z]) break;
                if (occ & bit[z]) { queued |= bit[z] & ~gone; break; }
            }
        }
        Bits danger = old | hit[0] | hit[1];
        if (dangerOut) *dangerOut = danger;
        if (walkOut) *walkOut = floor & ~occ;
        bool liveA = alive[0] && !(danger & bit[target[0]]), liveB = alive[1] && !(danger & bit[target[1]]);
        if (!liveA || !liveB) {
            double score = int(liveA) - int(liveB) + .5 * (killDifference
                + (!liveB && bool((s.fire[0] | hit[0]) & bit[target[1]]))
                - (!liveA && bool((s.fire[1] | hit[1]) & bit[target[0]])));
            s.result = score > 1e-8 ? 1000 : score < -1e-8 ? -1000 : 0;
        }
        s.crates &= ~(hit[0] | hit[1]);
        for (int p = 0; p < 2; ++p) {
            for (int k = 0; k < 4; ++k) s.bomb[p][k] = s.bomb[p][k + 1] & ~gone;
            s.bomb[p][4] = 0; s.fire[p] = hit[p]; s.pos[p] = target[p];
        }
        ++s.time;
        return s;
    }
    int actions(const State &s, int p, int out[10]) const {
        Bits occ = occupied(s), own = 0;
        for (Bits b : s.bomb[p]) own |= b;
        int c = s.pos[p], count = 0;
        bool drop = popcount(own) < capacity && !(occ & bit[c]);
        Bits floor = available[min(31, int(s.time))] & ~s.crates;
        for (int d = 0; d < 5; ++d) {
            int z = next[c][d];
            if (d != 4 && (z < 0 || !(floor & bit[z]) || (occ & bit[z]))) continue;
            out[count++] = d; if (drop) out[count++] = d + 5;
        }
        return count;
    }
    Bits hash(const State &s) const {
        Bits key = zTime[min(31, int(s.time))] ^ zPos[0][s.pos[0]] ^ zPos[1][s.pos[1]];
        auto add = [&](Bits mask, const Bits *keys) {
            while (mask) { int c = countr_zero(mask); mask &= mask - 1; key ^= keys[c]; }
        };
        add(s.crates, zCrate);
        for (int p = 0; p < 2; ++p) {
            add(s.fire[p], zFire[p]);
            for (int k = 0; k < 5; ++k) add(s.bomb[p][k], zBomb[p][k]);
        }
        return key;
    }
    int evaluate(State s) const {
        Bits reachable[2] = {bit[s.pos[0]], bit[s.pos[1]]};
        int life[2]{}; double room[2]{};
        for (int k = 0; k < 8; ++k) {
            int now = s.time; Bits danger, walk;
            s = step(s, 4, 4, &danger, &walk);
            for (int p = 0; p < 2; ++p) {
                reachable[p] &= available[min(now, 31)];
                reachable[p] = (reachable[p] | (neighbors(reachable[p]) & walk)) & ~danger;
                if (reachable[p]) life[p] = k + 1;
                room[p] += log1p(popcount(reachable[p]));
            }
            if (!available[min(31, int(s.time))]) break;
        }
        if (!reachable[0] || !reachable[1]) {
            if (reachable[0]) return 850;
            if (reachable[1]) return -850;
            if (life[0] != life[1]) return life[0] > life[1] ? 800 : -800;
            return killDifference > 1e-8 ? 700 : killDifference < -1e-8 ? -700 : 0;
        }
        return int(4 * (room[0] - room[1]) + 2 * (popcount(reachable[0]) - popcount(reachable[1])));
    }
    int search(const State &s, int depth, int alpha, int beta) {
        if (s.result != 2001) return s.result;
        if ((++nodes & 31) == 0 && Clock::now() >= deadline) { aborted = true; return 0; }
        if (!depth) return evaluate(s);
        Bits key = hash(s); Entry &entry = table[key & ((1 << 18) - 1)];
        int preferred = -1, alphaStart = alpha, betaStart = beta;
        if (entry.generation == generation && entry.key == key) {
            preferred = entry.flag >> 2;
            if (entry.depth >= depth) {
                int flag = entry.flag & 3;
                if (flag == 0) return entry.value;
                if (flag == 1) alpha = max(alpha, int(entry.value)); else beta = min(beta, int(entry.value));
                if (alpha >= beta) return entry.value;
            }
        }
        int aa[10], bb[10], an = actions(s, 0, aa), bn = actions(s, 1, bb);
        for (int i = 0; i < an; ++i) if (aa[i] == preferred) swap(aa[0], aa[i]);
        int best = -1100, action = aa[0];
        for (int i = 0; i < an; ++i) {
            int worst = 1100;
            for (int j = 0; j < bn; ++j) {
                worst = min(worst, search(step(s, aa[i], bb[j]), depth - 1, alpha, min(beta, worst)));
                if (aborted) return 0;
                if (worst <= alpha) break;
            }
            if (worst > best) { best = worst; action = aa[i]; }
            alpha = max(alpha, best); if (alpha >= beta) break;
        }
        entry = {key, int16_t(best), uint8_t(depth),
            uint8_t((best <= alphaStart ? 2 : best >= betaStart ? 1 : 0) | (action << 2)), generation};
        return best;
    }
    Choice choose(Choice fallback) {
        int aa[10], bb[10], an = actions(initial, 0, aa), bn = actions(initial, 1, bb);
        for (int i = 0; i < an; ++i) if (aa[i] == fallback.direction + 5 * fallback.place) swap(aa[0], aa[i]);
        Choice result = fallback;
        double policy[10]{}; int policyActions[10], policySize = 0;
        for (int depth = 1; depth <= 9; ++depth) {
            int matrix[10][10]{}, best = -1100, choice = -1; double bestAverage = -1e10;
            for (int i = 0; i < an && !aborted; ++i) {
                int worst = 1100; double average = 0;
                for (int j = 0; j < bn; ++j) {
                    int value = search(step(initial, aa[i], bb[j]), depth - 1, -1100, 1100);
                    if (aborted) break;
                    matrix[i][j] = value; worst = min(worst, value); average += value;
                }
                if (worst > best || (worst == best && average > bestAverage + .01)) {
                    best = worst; choice = aa[i]; bestAverage = average;
                }
            }
            if (aborted || choice < 0) break;
            result = {choice % 5, choice / 5, double(best)};
            double rowCount[10]{}, rowSum[10]{}, columnSum[10]{};
            for (int iteration = 0; iteration < 600; ++iteration) {
                int row = 0, column = 0;
                for (int i = 1; i < an; ++i) if (rowSum[i] > rowSum[row]) row = i;
                for (int j = 1; j < bn; ++j) if (columnSum[j] < columnSum[column]) column = j;
                ++rowCount[row];
                for (int i = 0; i < an; ++i) rowSum[i] += matrix[i][column];
                for (int j = 0; j < bn; ++j) columnSum[j] += matrix[row][j];
            }
            double lower = 1e9;
            for (int j = 0; j < bn; ++j) lower = min(lower, columnSum[j] / 600.);
            policySize = 0;
            if (lower > best + 8) {
                policySize = an;
                for (int i = 0; i < an; ++i) { policyActions[i] = aa[i]; policy[i] = rowCount[i] / 600.; }
            }
            for (int i = 0; i < an; ++i) if (aa[i] == choice) swap(aa[0], aa[i]);
            if (best == 1000 || Clock::now() >= deadline) break;
        }
        if (policySize) {
            double draw = randomUnit();
            for (int i = 0; i < policySize; ++i) if ((draw -= policy[i]) <= 0) {
                result.direction = policyActions[i] % 5; result.place = policyActions[i] / 5; break;
            }
        }
        return result;
    }
};
}

Choice decide() {
    auto start = Clock::now();
    int origin = players[me].cell;
    blockedTurns = lastDirection < 4 && origin == lastOrigin ? blockedTurns + 1 : 0;
    Forecast normal(bombs);
    objectives(normal);
    double collision[2][5]{};
    collisionValues(collision, start);
    Mobility enemyBase[10];
    for (int id = 0; id < playerCount; ++id)
        if (id != me && players[id].alive && myDistance[players[id].cell] < 18)
            enemyBase[id] = mobility(normal, players[id].cell);
    Choice best;
    for (int place = 0; place < 2; ++place) {
        if (place && !canPlace(me)) continue;
        auto bs = bombs;
        if (place) bs.push_back({me, origin, turn + fuse, turn});
        Forecast f(bs); auto own = mobility(f, origin, true);
        double attack = 0, excavation = 0;
        if (place) {
            for (int c = 0; c < cells; ++c) if (board[c] == '+' && f.destroyed[c] < normal.destroyed[c])
                excavation += (reachableCenter <= 2 ? .65 : 2.) + .25 * max(0., centerCost[origin] - centerCost[c]);
            for (int id = 0; id < playerCount; ++id) if (id != me && players[id].alive && enemyBase[id].total) {
                auto after = mobility(f, players[id].cell);
                if (!after.total) attack += 85;
                else {
                    attack += 3.2 * max(0., 1. - double(after.total) / enemyBase[id].total);
                    int before = 0, remaining = 0;
                    for (int d = 0; d < 5; ++d) {
                        before = max(before, enemyBase[id].space[min(4, f.horizon - 1)][d]);
                        remaining = max(remaining, after.space[min(4, f.horizon - 1)][d]);
                    }
                    if (before) attack += 2. * max(0., 1. - double(remaining) / before);
                }
            }
        }
        double worstRisk[5]{}, totalRisk[5]{}, room[5]{};
        int samples = 0, processed = 0;
        Forecast combined = f;
        auto scenarios = threats(f);
        auto volley = bs; int volleyCount = 0;
        for (auto s : scenarios) if (s.delay == 0 && s.secondCell < 0) {
            volley.push_back({s.owner, s.cell, turn + fuse, turn}); ++volleyCount;
        }
        // Leave time for both placement choices, a complete fallback, and I/O.
        auto threatDeadline = start + chrono::milliseconds(place ? 21 : 12);
        for (auto s : scenarios) {
            if (processed++ && Clock::now() >= threatDeadline) break;
            auto test = bs; test.push_back({s.owner, s.cell, turn + s.delay + fuse, turn + s.delay});
            if (s.secondCell >= 0) test.push_back({s.owner, s.secondCell, turn + s.secondDelay + fuse, turn + s.secondDelay});
            Forecast danger(test); auto escape = mobility(danger, origin);
            if (s.secondCell >= 0) {
                array<uint8_t, M> reachable{}, following{};
                reachable[s.cell] = 1;
                for (int k = 0; k < s.secondDelay; ++k) {
                    following.fill(0);
                    for (int c = 0; c < cells; ++c) if (reachable[c] && danger.open[k][c])
                        for (int d = 0; d < 5; ++d) {
                            int z = danger.destination(k, c, d);
                            if (danger.safe[k][z]) following[z] = 1;
                        }
                    reachable = following;
                }
                if (!reachable[s.secondCell]) continue;
            }
            double weight = s.weight;
            if (s.delay == 0 && !danger.viable[0][players[s.owner].cell]) weight *= .55;
            if (s.secondCell >= 0 && !danger.viable[s.secondDelay][s.secondCell]) weight *= .55;
            for (int d = 0; d < 5; ++d) {
                double risk = escape.life[d] < danger.horizon ? (115 + 8 * (danger.horizon - escape.life[d])) * weight : 0;
                worstRisk[d] = max(worstRisk[d], risk);
                if (s.secondCell < 0) { totalRisk[d] += risk; room[d] += log1p(escape.count[d]); }
            }
            // Joint predictions guard against traps without rewarding speculative room.
            if (s.secondCell >= 0) continue;
            if (aliveCount > 2) for (int k = 0; k < f.horizon; ++k) for (int c = 0; c < cells; ++c) {
                combined.safe[k][c] &= danger.safe[k][c];
                combined.open[k][c] &= danger.open[k][c];
                combined.occupied[k][c] |= danger.occupied[k][c];
            }
            ++samples;
        }
        if (volleyCount >= 2) {
            Forecast danger(volley); auto escape = mobility(danger, origin);
            for (int d = 0; d < 5; ++d) {
                double risk = escape.life[d] < danger.horizon ? 100 + 6 * (danger.horizon - escape.life[d]) : 0;
                worstRisk[d] = max(worstRisk[d], risk); totalRisk[d] += risk; room[d] += log1p(escape.count[d]);
            }
            ++samples;
        }
        Mobility robust = own;
        if (samples && aliveCount > 2) { combined.rebuild(); robust = mobility(combined, origin); }
        for (int d = 0; d < 5; ++d) {
            int dest = f.destination(0, origin, d);
            if (dest == origin && d != 4) continue;
            double value = own.life[d] * 2000.;
            if (own.life[d] == f.horizon) {
                value += 3.1 * field[dest] + .55 * own.utility[d];
                value += 1.05 * log1p(own.count[d]) + 1.1 * log1p(own.width[d]) - .75 * visits[dest];
                for (int k = 1; k < min(6, f.horizon); ++k) value += .30 * log1p(own.space[k][d]);
                value -= (aliveCount > 2 ? 2.8 : 1.5) * worstRisk[d];
                if (samples) value += (.50 * room[d] - .20 * totalRisk[d]) / samples;
                if (samples && aliveCount > 2 && robust.life[d] < f.horizon) value -= 12 + 2.5 * (f.horizon - robust.life[d]);
                if (dest == origin) value -= .25;
                value += attack + excavation - (place ? 1.1 : 0);
            } else value += attack * 5 + excavation;
            if (dest != origin) {
                double block = 0;
                for (int id = 0; id < playerCount; ++id) if (id != me && players[id].alive && f.open[0][players[id].cell]) {
                    int ec = players[id].cell;
                    for (int ed = 0; ed < 5; ++ed) if (f.destination(0, ec, ed) == dest) {
                        double weight = ec == dest ? .6 : .25;
                        if (previous[id] >= 0 && ec != previous[id])
                            for (int pd = 0; pd < 4; ++pd)
                                if (neighbor[previous[id]][pd] == ec && neighbor[ec][pd] == dest) weight = .65;
                        block = max(block, weight);
                    }
                }
                if (block) value -= own.life[4] < f.horizon ?
                    block * (6000 + 2000 * (f.horizon - own.life[4])) :
                    block * (1.5 + max(0., worstRisk[4] - worstRisk[d]));
            }
            if (enemyDistance[dest] <= 4 && degree[dest] < 2) value -= 4;
            value -= tunnel[dest] * (aliveCount > 2 ? 1. : .55);
            if (blockedTurns && dest == neighbor[origin][lastDirection])
                value -= 4 + 1.5 * min(5, blockedTurns) + 2.5 * max(0., field[dest] - field[origin]);
            value += collision[place][d] + .055 * randomUnit();
            if (value > best.score) best = {d, place, value};
        }
    }
    if (aliveCount == 2) {
        int spaces = 0, enemy = -1;
        for (int c = 0; c < cells; ++c) spaces += board[c] != '#';
        for (int id = 0; id < playerCount; ++id) if (id != me && players[id].alive) enemy = id;
        bool clean = true;
        for (auto b : bombs) if (b.owner != me && b.owner != enemy) clean = false;
        for (auto s : sources) if (s.end >= turn && s.owner != me && s.owner != enemy) clean = false;
        if (spaces <= 48 && clean && Clock::now() < start + chrono::milliseconds(15)) {
            Endgame::Solver solver(enemy, start + chrono::milliseconds(25)); best = solver.choose(best);
        }
    }
    for (int id = 0; id < playerCount; ++id) previous[id] = players[id].cell;
    lastOrigin = origin; lastDirection = best.direction;
    return best;
}

bool readState() {
    string word;
    if (!(cin >> word >> turn)) return false;
    cin >> word;
    for (int y = 0; y < n; ++y) {
        string row; cin >> row;
        for (int x = 0; x < n; ++x) board[y * n + x] = row[x];
    }
    for (int i = 0; i < playerCount; ++i) {
        int id, x, y, alive; cin >> word >> id >> x >> y >> alive;
        players[id].cell = y * n + x; players[id].alive = alive;
    }
    int count; cin >> word >> count; bombs.resize(count);
    for (auto &b : bombs) { int x, y; cin >> word >> b.owner >> x >> y >> b.at; b.cell = y * n + x; b.born = turn - 1; }
    cin >> word >> count; flames.resize(count);
    for (auto &f : flames) { int x, y; cin >> word >> x >> y >> f.end; f.cell = y * n + x; }
    while (cin >> word) {
        if (word == "END") return true;
        if (word == "STATUS") {
            cin >> count;
            for (int i = 0; i < count; ++i) {
                int id, death, numerator, denominator; cin >> word >> id >> death >> numerator >> denominator;
                players[id].kills = double(numerator) / denominator;
            }
        } else if (word == "SOURCES") {
            cin >> count; sources.resize(count);
            for (auto &s : sources) { int x, y; cin >> word >> x >> y >> s.owner >> s.end; s.cell = y * n + x; }
        } else { string rest; getline(cin, rest); }
    }
    return false;
}

int main() {
    ios::sync_with_stdio(false); cin.tie(nullptr);
    string word;
    if (!(cin >> word >> n >> playerCount >> me >> limit >> fuse >> radius >> capacity >> fireLength >> shrink >> interval)) return 0;
    if (n < 3 || n > 25 || playerCount < 2 || playerCount > 10 || me < 0 || me >= playerCount) return 0;
    cells = n * n; players.resize(playerCount); fill(previous, previous + 10, -1);
    for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
        int c = y * n + x, edge = min({x, y, n - 1 - x, n - 1 - y});
        falls[c] = edge ? shrink + (edge - 1) * interval : 0;
        neighbor[c][0] = y ? c - n : -1; neighbor[c][1] = y + 1 < n ? c + n : -1;
        neighbor[c][2] = x ? c - 1 : -1; neighbor[c][3] = x + 1 < n ? c + 1 : -1; neighbor[c][4] = c;
    }
    int deepest = n / 2; if (deepest % 2 == 0) --deepest;
    extinction = min(limit, shrink + (deepest - 1) * interval);
    randomState ^= uint64_t(me + 1) * 0x9e3779b97f4a7c15ULL;
    while (readState()) {
        auto choice = decide();
        cout << turn << ' ' << directions[choice.direction] << ' ' << choice.place << '\n' << flush;
    }
    return 0;
}
