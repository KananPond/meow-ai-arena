// GPT's Appetite: closed-loop search, exact territory scoring and trail interception.
// Strategy references are recorded in the submission's meta.json and NOTES.md.
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace land {
constexpr int MAX_CELLS = 48 * 48;
constexpr int INF = 10000;
constexpr int DX[4] = {0, 0, -1, 1};
constexpr int DY[4] = {-1, 1, 0, 0};
constexpr char DIRS[] = "UDLR";
using Rows = std::array<std::uint64_t, 48>;
using Path = std::vector<int>;

struct Player {
    int pos = 0, dir = 0, length = 0, area = 0, deaths = 0;
};

struct Evaluation {
    Path path;
    double gain = 0, score = -1;
    int margin = -INF, length = 0;
    bool closed = false;
};

struct Leg { int dir, length; };
struct Candidate { double estimate; std::array<Leg, 5> legs; int count; };

class Bot {
public:
    int w, h, cells, turns, players, me, turn = 0;
    std::array<Player, 4> p;
    std::array<int, 4> spawn{}, idle{}, lastPos{}, lastDeaths{};
    int owner[MAX_CELLS], trail[MAX_CELLS], protect[MAX_CELLS];
    int next[MAX_CELLS][4], enemy[4][MAX_CELLS], danger[MAX_CELLS];
    int enemyHome[4], enemyParent[4][MAX_CELLS];
    int home[MAX_CELLS], distance[MAX_CELLS], parent[MAX_CELLS], parentDir[MAX_CELLS];
    int seen[MAX_CELLS]{}, stamp = 0;
    int expected = -1, previousDeaths = 0, previousLength = 0;
    int trailDanger = INF;
    Rows ownRows{}, trailRows{}, valueRows[5]{};
    double weights[5]{}, prefix[49][49]{};
    double denied[4]{};
    double rivalImportance[4]{};
    std::uint64_t full;
    Path plan;
    std::uint64_t rng = 0x91e10da5c79e7b1dULL;
    std::chrono::steady_clock::time_point started;

    Bot(int width, int height, int count, int id, int maxTurns,
        const std::array<int, 4>& spawns)
        : w(width), h(height), cells(w * h), turns(maxTurns), players(count),
          me(id), spawn(spawns), full((std::uint64_t{1} << w) - 1) {
        std::fill(owner, owner + cells, -1);
        std::fill(trail, trail + cells, -1);
        std::fill(protect, protect + cells, -1);
        lastPos.fill(-1);
        for (int k = 0; k < cells; ++k) {
            int x = k % w, y = k / w;
            for (int d = 0; d < 4; ++d) {
                int xx = x + DX[d], yy = y + DY[d];
                next[k][d] = xx < 0 || xx >= w || yy < 0 || yy >= h ? -1 : yy * w + xx;
            }
            for (int e = 0; e < players; ++e)
                if (std::abs(x - spawn[e] % w) <= 1 && std::abs(y - spawn[e] / w) <= 1)
                    protect[k] = e;
        }
    }

    int manhattan(int a, int b) const {
        return std::abs(a % w - b % w) + std::abs(a / w - b / w);
    }

    bool expired(double milliseconds = 7.0) const {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count() >= milliseconds;
    }

    unsigned random() {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        return static_cast<unsigned>(rng >> 16);
    }

    // Spread through each free horizontal interval, then exchange rows.
    Rows enclosed(const Rows& walls) const {
        Rows reached{};
        std::uint64_t edges = 1 | (std::uint64_t{1} << (w - 1));
        for (int y = 0; y < h; ++y)
            reached[y] = (y == 0 || y + 1 == h ? full : edges) & ~walls[y];
        bool changed = true;
        while (changed) {
            changed = false;
            for (int sweep = 0; sweep < 2; ++sweep) {
                for (int j = 0; j < h; ++j) {
                    int y = sweep ? h - 1 - j : j;
                    std::uint64_t free = full & ~walls[y];
                    std::uint64_t seeds = reached[y];
                    if (y) seeds |= reached[y - 1];
                    if (y + 1 < h) seeds |= reached[y + 1];
                    seeds &= free;
                    std::uint64_t left = seeds, right = seeds, a = free, b = free;
                    for (int shift = 1; shift < w; shift *= 2) {
                        left |= a & (left << shift); a &= a << shift;
                        right |= b & (right >> shift); b &= b >> shift;
                    }
                    std::uint64_t result = (left | right) & free;
                    changed |= result != reached[y];
                    reached[y] = result;
                }
            }
        }
        Rows captured{};
        for (int y = 0; y < h; ++y) captured[y] = full & ~reached[y];
        return captured;
    }

    bool collision(int d) const {
        int start = p[me].pos, target = next[start][d];
        if (target < 0) return true;
        for (int e = 0; e < players; ++e) if (e != me && idle[e] < 3) {
            for (int z = 0; z < 4; ++z) if (z != (p[e].dir ^ 1)) {
                int v = next[p[e].pos][z];
                if (v >= 0 && (v == target || (v == start && target == p[e].pos))) return true;
            }
        }
        return false;
    }

    void fields() {
        std::fill(danger, danger + cells, INF);
        if (players == 4) {
            double sum = 0, scale = std::max(70.0, (turns - turn) * 0.3);
            for (int e = 0; e < players; ++e) if (e != me) {
                double gap = std::abs(p[me].area - p[e].area) / scale;
                double x = std::exp(-gap);
                rivalImportance[e] = x / ((1 + x) * (1 + x));
                sum += rivalImportance[e];
            }
            for (int e = 0; e < players; ++e) if (e != me) rivalImportance[e] /= sum;
        }
        for (int e = 0; e < players; ++e) if (e != me) {
            idle[e] = p[e].pos == lastPos[e] && p[e].deaths == lastDeaths[e]
                      && p[e].pos == spawn[e] ? idle[e] + 1 : 0;
            lastPos[e] = p[e].pos; lastDeaths[e] = p[e].deaths;
            enemyHome[e] = p[e].length ? INF : 0;
            denied[e] = 0;
            std::fill(enemy[e], enemy[e] + cells, INF);
            if (idle[e] >= 3) continue;

            // Mode 1 permits crossing old trail after the opponent returns home.
            int dist[2 * MAX_CELLS], q[2 * MAX_CELLS], head = 0, tail = 0;
            std::fill(dist, dist + 2 * cells, INF);
            int start = p[e].pos, mode = p[e].length == 0;
            dist[start + mode * cells] = 0;
            for (int d = 0; d < 4; ++d) if (d != (p[e].dir ^ 1)) {
                int v = next[start][d];
                if (v < 0 || (!mode && trail[v] == e)) continue;
                int state = v + (mode || owner[v] == e) * cells;
                if (dist[state] != INF) continue;
                dist[state] = 1; q[tail++] = state; enemyParent[e][v] = start;
            }
            int goal = -1;
            while (head < tail) {
                int state = q[head++], u = state % cells, clear = state >= cells;
                if (goal < 0 && p[e].length && owner[u] == e) {
                    goal = u; enemyHome[e] = dist[state];
                }
                for (int d = 0; d < 4; ++d) {
                    int v = next[u][d];
                    if (v < 0 || (!clear && trail[v] == e)) continue;
                    int ns = v + (clear || owner[v] == e) * cells;
                    if (dist[ns] != INF) continue;
                    dist[ns] = dist[state] + 1; q[tail++] = ns;
                    if (!clear) enemyParent[e][v] = u;
                }
            }
            for (int k = 0; k < cells; ++k) {
                enemy[e][k] = std::min(dist[k], dist[k + cells]);
                danger[k] = std::min(danger[k], enemy[e][k]);
            }
            if (goal >= 0) {
                Rows walls{};
                for (int k = 0; k < cells; ++k)
                    if (owner[k] == e || trail[k] == e) walls[k / w] |= std::uint64_t{1} << (k % w);
                int steps = 0;
                for (int k = goal; k != start && steps++ < cells; k = enemyParent[e][k])
                    walls[k / w] |= std::uint64_t{1} << (k % w);
                if (steps >= cells) continue;
                Rows captured = enclosed(walls);
                for (int k = 0; k < cells; ++k) {
                    if (!(captured[k / w] & (std::uint64_t{1} << (k % w))) || owner[k] == e
                        || (protect[k] >= 0 && protect[k] != e)) continue;
                    if (players == 2)
                        denied[e] += owner[k] == me ? 2.2 : owner[k] < 0 ? 0.8 : 0.25;
                    else {
                        double value = rivalImportance[e];
                        if (owner[k] == me) value += 1;
                        else if (owner[k] >= 0) value -= rivalImportance[owner[k]];
                        denied[e] += value;
                    }
                }
            }
        }
        trailDanger = INF;
        ownRows.fill(0); trailRows.fill(0);
        for (auto& rows : valueRows) rows.fill(0);
        int leader = me;
        for (int e = 0; e < players; ++e) if (p[e].area > p[leader].area) leader = e;
        weights[0] = 1;
        for (int e = 0; e < players; ++e)
            weights[e + 1] = e == me ? 0 : players == 2 ? 2.4 : e == leader ? 2.3 : 1.7;
        for (int k = 0; k < cells; ++k) {
            std::uint64_t bit = std::uint64_t{1} << (k % w);
            if (owner[k] == me) ownRows[k / w] |= bit;
            if (trail[k] == me) {
                trailRows[k / w] |= bit; trailDanger = std::min(trailDanger, danger[k]);
            }
            if (owner[k] != me && (protect[k] < 0 || protect[k] == me))
                valueRows[owner[k] + 1][k / w] |= bit;
        }
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            int k = y * w + x;
            double value = protect[k] < 0 || protect[k] == me ? weights[owner[k] + 1] : 0;
            prefix[y + 1][x + 1] = value + prefix[y][x + 1] + prefix[y + 1][x] - prefix[y][x];
        }
        int q[MAX_CELLS], a = 0, b = 0;
        std::fill(home, home + cells, INF);
        for (int k = 0; k < cells; ++k) if (owner[k] == me && trail[k] != me) {
            home[k] = 0; q[b++] = k;
        }
        while (a < b) {
            int u = q[a++];
            for (int d = 0; d < 4; ++d) {
                int v = next[u][d];
                if (v < 0 || trail[v] == me || home[v] != INF) continue;
                home[v] = home[u] + 1; q[b++] = v;
            }
        }
    }

    double gain(const Rows& walls) const {
        Rows captured = enclosed(walls);
        double result = 0;
        for (int e = 0; e <= players; ++e) if (weights[e])
            for (int y = 0; y < h; ++y)
                result += weights[e] * std::popcount(captured[y] & valueRows[e][y]);
        return result;
    }

    double rate(double value, int length, int margin) const {
        double probability = margin >= 1 ? 1 : margin == 0 ? 0.65 : margin >= -2 ? 0.20 : 0.01;
        if (turns - turn <= 25) return value * probability / (1 + length * 0.008);
        return value * probability / (length + 18.0);
    }

    Evaluation evaluate(const Path& path, bool exact = true) {
        Evaluation e;
        if (path.empty()) return e;
        if (exact) e.path.reserve(path.size());
        ++stamp;
        Rows walls{};
        for (int y = 0; y < h; ++y) walls[y] = ownRows[y] | trailRows[y];
        int u = p[me].pos, heading = p[me].dir, risk = trailDanger;
        bool outside = p[me].length > 0;
        int minx = u % w, maxx = minx, miny = u / w, maxy = miny;
        for (int d : path) {
            if (d == (heading ^ 1) || next[u][d] < 0) return e;
            u = next[u][d]; heading = d;
            if (trail[u] == me || seen[u] == stamp) return e;
            if (exact) e.path.push_back(d);
            ++e.length;
            if (e.length > turns - turn || e.length > 180) return e;
            if (e.length == 1 && collision(d)) return e;
            minx = std::min(minx, u % w); maxx = std::max(maxx, u % w);
            miny = std::min(miny, u / w); maxy = std::max(maxy, u / w);
            if (owner[u] == me) {
                if (outside) { e.closed = true; break; }
            } else {
                outside = true; seen[u] = stamp;
                walls[u / w] |= std::uint64_t{1} << (u % w);
                risk = std::min(risk, danger[u]);
            }
        }
        if (!e.closed) return e;
        e.margin = risk - e.length;
        e.gain = exact ? gain(walls) : prefix[maxy + 1][maxx + 1] - prefix[miny][maxx + 1]
                     - prefix[maxy + 1][minx] + prefix[miny][minx];
        e.score = rate(e.gain, e.length, e.margin);
        return e;
    }

    Path pathFromLegs(const Candidate& c) const {
        Path path;
        path.reserve(160);
        int u = p[me].pos;
        bool outside = p[me].length > 0;
        for (int i = 0; i < c.count; ++i) {
            int d = c.legs[i].dir, count = c.legs[i].length;
            for (int j = 0; j < (count < 0 ? std::max(w, h) : count); ++j) {
                u = next[u][d];
                if (u < 0) return {};
                path.push_back(d);
                if (owner[u] == me && outside) return path;
                if (owner[u] != me) outside = true;
            }
        }
        return path;
    }

    Path returnPath(const Path& prefixPath = {}) {
        ++stamp;
        int u = p[me].pos, heading = p[me].dir;
        bool outside = p[me].length > 0;
        for (int d : prefixPath) {
            if (d == (heading ^ 1) || next[u][d] < 0) return {};
            u = next[u][d]; heading = d;
            if (trail[u] == me || seen[u] == stamp) return {};
            if (owner[u] == me && outside) return prefixPath;
            if (owner[u] != me) seen[u] = stamp, outside = true;
        }
        int dist[MAX_CELLS], par[MAX_CELLS], pd[MAX_CELLS], q[MAX_CELLS], a = 0, b = 0;
        std::fill(dist, dist + cells, INF);
        int start = u;
        dist[start] = 0;
        for (int d = 0; d < 4; ++d) if (d != (heading ^ 1)) {
            int v = next[start][d];
            if (v < 0 || trail[v] == me || seen[v] == stamp || (prefixPath.empty() && collision(d))) continue;
            dist[v] = 1; par[v] = start; pd[v] = d; q[b++] = v;
        }
        int goal = -1;
        while (a < b) {
            u = q[a++];
            if (owner[u] == me) { goal = u; break; }
            int dirs[4] = {0, 1, 2, 3};
            std::sort(dirs, dirs + 4, [&](int d, int e) {
                int v = next[u][d], z = next[u][e];
                return (v < 0 ? -INF : danger[v]) > (z < 0 ? -INF : danger[z]);
            });
            for (int d : dirs) {
                int v = next[u][d];
                if (v < 0 || dist[v] != INF || trail[v] == me || seen[v] == stamp) continue;
                dist[v] = dist[u] + 1; par[v] = u; pd[v] = d; q[b++] = v;
            }
        }
        if (goal < 0) return {};
        Path suffix;
        for (int k = goal; k != start; k = par[k]) suffix.push_back(pd[k]);
        std::reverse(suffix.begin(), suffix.end());
        Path result = prefixPath;
        result.insert(result.end(), suffix.begin(), suffix.end());
        return result;
    }

    Evaluation expansion() {
        Evaluation best = evaluate(plan);
        if (players == 4 && best.margin < 0) best = Evaluation{};
        if (best.closed) best.score *= 1.025;
        Evaluation fast = evaluate(returnPath());
        if ((players == 2 || fast.margin >= 0) && fast.score > best.score) best = std::move(fast);
        std::vector<Candidate> candidates;
        candidates.reserve(5000);
        std::unordered_set<std::uint64_t> unique;
        if (players == 4) unique.reserve(4000);
        auto consider = [&](Candidate c) {
            Path path = pathFromLegs(c);
            Evaluation e = evaluate(path, false);
            if (e.closed && e.margin >= 0 && e.gain > 0) {
                if (players == 4) {
                    std::uint64_t hash = 14695981039346656037ULL;
                    for (int d : path) hash = (hash ^ static_cast<unsigned>(d + 1)) * 1099511628211ULL;
                    if (!unique.insert(hash).second) return;
                }
                c.estimate = e.score;
                candidates.push_back(c);
            }
        };
        const int depths[] = {1, 3, 5, 8, 12, 16, 22, 28, 36, 44, 2, 4, 6, 10, 14, 18, 24, 32, 40};
        for (int a : depths) {
            for (int d = 0; d < 4; ++d) if (d != (p[me].dir ^ 1)) {
                int x = p[me].pos % w + DX[d] * a, y = p[me].pos / w + DY[d] * a;
                if (x < 0 || x >= w || y < 0 || y >= h) continue;
                for (int side = 0; side < 4; ++side) if ((d < 2) != (side < 2)) {
                    Candidate c{}; c.count = 2; c.legs[0] = {d, a}; c.legs[1] = {side, -1};
                    consider(c);
                    for (int b = 1; b < std::max(w, h); ++b) {
                        int xx = x + DX[side] * b, yy = y + DY[side] * b;
                        if (xx < 0 || xx >= w || yy < 0 || yy >= h) break;
                        c.count = 3; c.legs[1] = {side, b}; c.legs[2] = {d ^ 1, -1};
                        consider(c);
                        // A fourth side can close on a different part of an irregular frontier.
                        if (a <= 24 && b <= 28) {
                            c.count = 4; c.legs[2] = {d ^ 1, a}; c.legs[3] = {side ^ 1, -1};
                            consider(c);
                        }
                    }
                }
            }
            if (expired(3.0)) break;
        }
        int count = std::min(140, static_cast<int>(candidates.size()));
        std::partial_sort(candidates.begin(), candidates.begin() + count, candidates.end(),
                          [](const Candidate& a, const Candidate& b) { return a.estimate > b.estimate; });
        Candidate champion{};
        if (players == 2 && best.closed) {
            for (int d : best.path) {
                if (champion.count && champion.legs[champion.count - 1].dir == d)
                    ++champion.legs[champion.count - 1].length;
                else if (champion.count < 5)
                    champion.legs[champion.count++] = {d, 1};
                else { champion.count = 0; break; }
            }
            if (champion.count) champion.legs[champion.count - 1].length = -1;
        }
        for (int i = 0; i < count; ++i) {
            if (i > 0 && expired(5.0)) break;
            Evaluation e = evaluate(pathFromLegs(candidates[i]));
            if (e.score > best.score) { best = std::move(e); champion = candidates[i]; }
        }
        // A small bounded refinement handles staircases without searching game trees.
        for (int iteration = 0; iteration < 600 && !expired(); ++iteration) {
            Candidate c{};
            if (champion.count > 1 && random() % 3) {
                c = champion;
                int j = random() % (c.count - 1);
                c.legs[j].length += static_cast<int>(random() % 9) - 4;
                if (c.legs[j].length < 1) continue;
            } else {
                c.count = 3 + random() % 3;
                int d = random() % 4;
                if (d == (p[me].dir ^ 1)) d = p[me].dir;
                for (int j = 0; j < c.count; ++j) {
                    c.legs[j] = {d, j + 1 == c.count ? -1 : 1 + static_cast<int>(random() % 24)};
                    d = (d < 2 ? 2 : 0) + random() % 2;
                }
            }
            Path path = pathFromLegs(c);
            Evaluation quick = evaluate(path, false);
            if (!quick.closed || quick.margin < 0 || quick.score < best.score * 0.6) continue;
            Evaluation e = evaluate(path);
            if (e.score > best.score) { best = std::move(e); champion = c; }
        }
        return best;
    }

    std::pair<Path, double> attack() {
        int q[MAX_CELLS], a = 0, b = 0, start = p[me].pos;
        std::fill(distance, distance + cells, INF);
        distance[start] = 0;
        for (int d = 0; d < 4; ++d) if (d != (p[me].dir ^ 1) && !collision(d)) {
            int v = next[start][d];
            if (v < 0 || trail[v] == me) continue;
            distance[v] = 1; parent[v] = start; parentDir[v] = d; q[b++] = v;
        }
        while (a < b) {
            int u = q[a++];
            if (distance[u] >= 24) continue;
            for (int d = 0; d < 4; ++d) {
                int v = next[u][d];
                if (v < 0 || trail[v] == me || distance[v] != INF) continue;
                distance[v] = distance[u] + 1; parent[v] = u; parentDir[v] = d; q[b++] = v;
            }
        }
        Path best;
        double bestScore = -1;
        for (int e = 0; e < players; ++e) if (e != me && p[e].length) {
            int target = -1, length = INF;
            for (int k = 0; k < cells; ++k) if (trail[k] == e && distance[k] < length) {
                target = k; length = distance[k];
            }
            int pursuit = players == 4 ? 4 : 0;
            if (target < 0 || length > enemyHome[e] + pursuit || length > 24 || length > turns - turn) continue;
            if (players == 4 && enemyHome[e] > turns - turn) continue;
            Path path;
            for (int k = target; k != start; k = parent[k]) path.push_back(parentDir[k]);
            std::reverse(path.begin(), path.end());
            Path complete = returnPath(path);
            if (complete.empty()) continue;
            bool safe = true;
            auto check = [&](int k) {
                for (int enemyId = 0; enemyId < players; ++enemyId) if (enemyId != me) {
                    int limit = enemyId == e && length <= enemyHome[e] ? length : static_cast<int>(complete.size());
                    if (enemy[enemyId][k] <= limit) return false;
                    if (enemyId == e && length + manhattan(spawn[e], k) <= static_cast<int>(complete.size())) return false;
                }
                return true;
            };
            for (int k = 0; k < cells && safe; ++k) if (trail[k] == me) safe = check(k);
            int u = start;
            for (int d : complete) {
                u = next[u][d];
                if (u < 0) { safe = false; break; }
                if (owner[u] != me && u != target) safe &= check(u);
            }
            if (!safe) continue;
            double value = players == 2 ? 10 + 0.7 * p[e].length + denied[e]
                                       : rivalImportance[e] * (10 + 0.7 * p[e].length) + std::max(0.0, denied[e]);
            double score = value / (length + 9.0);
            if (players == 4) score *= std::min(1.0, (enemyHome[e] + 5.0 - length) / 5.0);
            if (length == 1) score += players == 2 ? 4 : 1;
            if (score > bestScore) bestScore = score, best = std::move(path);
        }
        return {best, bestScore};
    }

    int fallback() {
        int start = p[me].pos, result = p[me].dir;
        double best = -1e9;
        for (int d = 0; d < 4; ++d) if (d != (p[me].dir ^ 1)) {
            int v = next[start][d];
            if (v < 0 || trail[v] == me) continue;
            double value = (owner[v] == me ? 100 : 0) - (collision(d) ? 1000 : 0);
            value += std::min(danger[v], 60) * 0.3 - std::min(home[v], 100) * 3;
            for (int z = 0; z < 4; ++z) if (z != (d ^ 1)) {
                int k = next[v][z];
                if (k >= 0 && trail[k] != me) value += owner[k] == me ? 3 : 0.2;
            }
            if (d == p[me].dir) value += 0.05;
            if (value > best) best = value, result = d;
        }
        return result;
    }

    int decide() {
        started = std::chrono::steady_clock::now();
        if (expected != p[me].pos || previousDeaths != p[me].deaths
            || (previousLength && !p[me].length)) plan.clear();
        previousDeaths = p[me].deaths; previousLength = p[me].length;
        fields();
        auto [hunting, huntScore] = attack();
        Evaluation best = expansion();
        if (!hunting.empty() && (huntScore > best.score * 1.15 + 0.3 || (players == 2 && hunting.size() == 1)))
            plan = std::move(hunting);
        else if (best.closed && best.margin >= 0)
            plan = std::move(best.path);
        else
            plan.clear();
        int result = fallback();
        if (!plan.empty()) {
            result = plan.front(); plan.erase(plan.begin());
        }
        expected = next[p[me].pos][result];
        return result;
    }

    bool read() {
        std::string token;
        if (!(std::cin >> token >> turn)) return false;
        for (int i = 0; i < players; ++i) {
            int id, x, y; char d; Player q;
            std::cin >> token >> id >> x >> y >> d >> q.length >> q.area >> q.deaths;
            q.pos = y * w + x; q.dir = static_cast<int>(std::string(DIRS).find(d)); p[id] = q;
        }
        std::cin >> token;
        for (int y = 0; y < h; ++y) {
            std::cin >> token;
            for (int x = 0; x < w; ++x) owner[y * w + x] = token[x] == '.' ? -1 : token[x] - '0';
        }
        std::cin >> token;
        for (int y = 0; y < h; ++y) {
            std::cin >> token;
            for (int x = 0; x < w; ++x) trail[y * w + x] = token[x] == '.' ? -1 : token[x] - '0';
        }
        std::cin >> token;
        return static_cast<bool>(std::cin);
    }
};
} // namespace land

#ifndef LAND_TEST
int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string token;
    int w, h, turns, players, me, moveMs, initMs;
    if (!(std::cin >> token >> w >> h >> turns >> players >> me >> moveMs >> initMs)) return 0;
    std::array<int, 4> spawn{};
    for (int i = 0; i < players; ++i) {
        int id, x, y;
        std::cin >> token >> id >> x >> y; spawn[id] = y * w + x;
    }
    auto bot = std::make_unique<land::Bot>(w, h, players, me, turns, spawn);
    while (bot->read()) std::cout << bot->turn << ' ' << land::DIRS[bot->decide()] << std::endl;
}
#endif
