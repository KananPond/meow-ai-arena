#include <bits/stdc++.h>
using namespace std;

struct Unit {
    int id, team, x, y, hp, flag, respawn_at;
    bool immune = false;
};
struct Flag {
    int id;
    string status;
    int x, y, holder, return_at;
};
struct Event {
    string type;
    vector<int> args;
};

struct Game {
    int teams = 0, me = 0, size = 0, turns = 0, hp = 0, damage = 0, respawn = 0, flag_return = 0, flag_cooldown = 0;
    vector<string> map;
    vector<pair<int, int>> bases;
    vector<pair<int, int>> spots;
    int turn = 0;
    vector<int> score;
    vector<Unit> units;
    vector<Flag> flags;
    vector<Event> events;
};

bool read_turn(Game& g) {
    string w;
    while (cin >> w) {
        if (w == "INIT") {
            cin >> g.teams >> g.me >> g.size >> g.turns >> g.hp >> g.damage >> g.respawn >> g.flag_return >> g.flag_cooldown;
        } else if (w == "MAP") {
            g.map.assign(g.size, "");
            for (auto& row : g.map) cin >> row;
        } else if (w == "BASES") {
            int k; cin >> k; g.bases.resize(k);
            for (auto& b : g.bases) cin >> b.first >> b.second;
        } else if (w == "SPOTS") {
            int k; cin >> k; g.spots.resize(k);
            for (auto& s : g.spots) cin >> s.first >> s.second;
        } else if (w == "TURN") {
            cin >> g.turn;
        } else if (w == "SCORE") {
            g.score.resize(g.teams);
            for (auto& s : g.score) cin >> s;
        } else if (w == "UNITS") {
            int k; cin >> k; g.units.resize(k);
            for (auto& u : g.units) {
                string tag;
                cin >> tag >> u.id >> u.team >> u.x >> u.y >> u.hp >> u.flag >> u.respawn_at;
                u.immune = false;
            }
        } else if (w == "FLAGS") {
            int k; cin >> k; g.flags.resize(k);
            for (auto& f : g.flags) {
                string tag;
                cin >> tag >> f.id >> f.status >> f.x >> f.y >> f.holder >> f.return_at;
            }
        } else if (w == "EVENTS") {
            int k; cin >> k; g.events.assign(k, {});
            string line; getline(cin, line);
            for (auto& e : g.events) {
                getline(cin, line);
                istringstream in(line);
                string tag; in >> tag >> e.type;
                for (int v; in >> v;) e.args.push_back(v);
            }
        } else if (w == "END") {
            return true;
        }
    }
    return false;
}

static const int DX[5] = {0, 0, 0, -1, 1};
static const int DY[5] = {0, -1, 1, 0, 0};
static const char* MOVE_STR[5] = {"S", "U", "D", "L", "R"};
static const int INF_DIST = 10000;

struct Solver {
    bool initialized = false;
    int S = 0, N = 0, teams = 0, me = 0;
    vector<int> is_wall;
    vector<vector<int16_t>> cell_dist;      // [N][N]
    vector<vector<int16_t>> base_dist;      // [teams][N]
    vector<vector<int>> spawn_order;        // [teams][9] cell indices
    vector<int> spot_cells;                 // [spots.size()]
    vector<int> is_spot_cell;               // [N] -> spot index or -1
    vector<int> in_team_base;               // [N] -> team index or -1
    vector<vector<int>> attackable_from;    // [N] -> list of cells within attack range 2 & LOS

    int cell_id(int x, int y) const { return y * S + x; }
    int cell_x(int c) const { return c % S; }
    int cell_y(int c) const { return c / S; }

    bool can_attack_cell(int a, int b) const {
        int ax = a % S, ay = a / S;
        int bx = b % S, by = b / S;
        int dx = bx - ax, dy = by - ay;
        int d = abs(dx) + abs(dy);
        if (d > 2) return false;
        if (abs(dx) == 2 && dy == 0) return !is_wall[ay * S + (ax + dx / 2)];
        if (abs(dy) == 2 && dx == 0) return !is_wall[(ay + dy / 2) * S + ax];
        if (abs(dx) == 1 && abs(dy) == 1) {
            return !is_wall[ay * S + (ax + dx)] || !is_wall[(ay + dy) * S + ax];
        }
        return true;
    }

    void init(const Game& g) {
        if (initialized) return;
        initialized = true;
        S = g.size;
        N = S * S;
        teams = g.teams;
        me = g.me;

        is_wall.assign(N, 0);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++)
                is_wall[cell_id(x, y)] = (g.map[y][x] == '#');

        in_team_base.assign(N, -1);
        spawn_order.assign(teams, {});
        int cx = S / 2, cy = S / 2;
        for (int t = 0; t < teams; t++) {
            int bcx = g.bases[t].first, bcy = g.bases[t].second;
            vector<pair<int, int>> pts;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    pts.push_back({bcx + dx, bcy + dy});
                    in_team_base[cell_id(bcx + dx, bcy + dy)] = t;
                }
            }
            stable_sort(pts.begin(), pts.end(), [&](pair<int, int> a, pair<int, int> b) {
                int da = abs(a.first - cx) + abs(a.second - cy);
                int db = abs(b.first - cx) + abs(b.second - cy);
                if (da != db) return da < db;
                int ta = abs(a.first - bcx) + 2 * abs(a.second - bcy);
                int tb = abs(b.first - bcx) + 2 * abs(b.second - bcy);
                return ta < tb;
            });
            for (auto& p : pts) spawn_order[t].push_back(cell_id(p.first, p.second));
        }

        is_spot_cell.assign(N, -1);
        spot_cells.clear();
        for (int i = 0; i < (int)g.spots.size(); i++) {
            int c = cell_id(g.spots[i].first, g.spots[i].second);
            spot_cells.push_back(c);
            is_spot_cell[c] = i;
        }

        cell_dist.assign(N, vector<int16_t>(N, INF_DIST));
        vector<int> q(N);
        for (int src = 0; src < N; src++) {
            if (is_wall[src]) continue;
            auto& d = cell_dist[src];
            int head = 0, tail = 0;
            d[src] = 0;
            q[tail++] = src;
            while (head < tail) {
                int u = q[head++];
                int ux = u % S, uy = u / S;
                int nd = d[u] + 1;
                for (int m = 1; m < 5; m++) {
                    int vx = ux + DX[m], vy = uy + DY[m];
                    if (vx < 0 || vx >= S || vy < 0 || vy >= S) continue;
                    int v = vy * S + vx;
                    if (!is_wall[v] && d[v] == INF_DIST) {
                        d[v] = nd;
                        q[tail++] = v;
                    }
                }
            }
        }

        base_dist.assign(teams, vector<int16_t>(N, INF_DIST));
        for (int t = 0; t < teams; t++) {
            auto& d = base_dist[t];
            int head = 0, tail = 0;
            for (int c : spawn_order[t]) {
                if (!is_wall[c]) {
                    d[c] = 0;
                    q[tail++] = c;
                }
            }
            while (head < tail) {
                int u = q[head++];
                int ux = u % S, uy = u / S;
                int nd = d[u] + 1;
                for (int m = 1; m < 5; m++) {
                    int vx = ux + DX[m], vy = uy + DY[m];
                    if (vx < 0 || vx >= S || vy < 0 || vy >= S) continue;
                    int v = vy * S + vx;
                    if (!is_wall[v] && d[v] == INF_DIST) {
                        d[v] = nd;
                        q[tail++] = v;
                    }
                }
            }
        }

        attackable_from.assign(N, {});
        for (int c = 0; c < N; c++) {
            if (is_wall[c]) continue;
            int cx0 = c % S, cy0 = c / S;
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    if (abs(dx) + abs(dy) > 2) continue;
                    int nx = cx0 + dx, ny = cy0 + dy;
                    if (nx < 0 || nx >= S || ny < 0 || ny >= S) continue;
                    int nc = ny * S + nx;
                    if (!is_wall[nc] && can_attack_cell(c, nc)) {
                        attackable_from[c].push_back(nc);
                    }
                }
            }
        }
    }

    int next_cell(int c, int m) const {
        int nx = (c % S) + DX[m], ny = (c / S) + DY[m];
        if (nx < 0 || nx >= S || ny < 0 || ny >= S) return c;
        int nc = ny * S + nx;
        return is_wall[nc] ? c : nc;
    }

    pair<int, int> resolve_pair(int su, int wu, int sv, int wv) const {
        if (wu == wv) return {su, sv};
        if (wu == sv && wv == su) return {su, sv};
        if (wu == sv && wv == sv) return {su, sv};
        if (wv == su && wu == su) return {su, sv};
        return {wu, wv};
    }

    struct EnemyPred {
        int id;
        int start_cell;
        double move_prob[5];
        int move_dest[5];
        int most_likely_dest;
    };

    string solve(Game g) {
        init(g);

        // 1. Simulate Step 1: Flag returns & Unit respawns at the start of g.turn
        int returning_flags_now = 0;
        int free_spots_count = 0;
        vector<int> spot_has_home_flag(spot_cells.size(), 0);
        vector<int> active_ground_flag_at(N, -1);

        for (auto& f : g.flags) {
            if ((f.status == "dropped" || f.status == "cooldown") && f.return_at != -1 && g.turn >= f.return_at) {
                returning_flags_now++;
                f.status = "returning_now";
                f.x = f.y = -1;
            } else if (f.status == "home" && f.x != -1) {
                int c = cell_id(f.x, f.y);
                int sidx = is_spot_cell[c];
                if (sidx != -1) spot_has_home_flag[sidx] = 1;
                if (active_ground_flag_at[c] == -1) active_ground_flag_at[c] = f.id;
            } else if (f.status == "dropped" && f.x != -1) {
                int c = cell_id(f.x, f.y);
                if (active_ground_flag_at[c] == -1) active_ground_flag_at[c] = f.id;
            }
        }
        for (int sidx = 0; sidx < (int)spot_cells.size(); sidx++) {
            if (!spot_has_home_flag[sidx]) free_spots_count++;
        }

        vector<int> occupied(N, 0);
        for (const auto& u : g.units) {
            if (u.x != -1) occupied[cell_id(u.x, u.y)] = 1;
        }
        for (auto& u : g.units) {
            u.immune = false;
            if (u.x == -1 && u.respawn_at != -1 && g.turn >= u.respawn_at) {
                for (int c : spawn_order[u.team]) {
                    if (!occupied[c]) {
                        u.x = cell_x(c);
                        u.y = cell_y(c);
                        u.hp = g.hp;
                        u.respawn_at = -1;
                        u.immune = true;
                        occupied[c] = 1;
                        break;
                    }
                }
            }
        }

        // 2. Build Enemy Movement & Threat Predictions
        vector<EnemyPred> enemy_preds;
        vector<double> cell_attack_risk(N, 0.0);
        vector<double> enemy_want_prob(N, 0.0);

        for (const auto& u : g.units) {
            if (u.team == me || u.x == -1) continue;
            EnemyPred ep;
            ep.id = u.id;
            ep.start_cell = cell_id(u.x, u.y);
            for (int m = 0; m < 5; m++) {
                ep.move_dest[m] = next_cell(ep.start_cell, m);
                ep.move_prob[m] = 0.0;
            }

            if (u.flag != -1) {
                int cur_d = base_dist[u.team][ep.start_cell];
                double sum_w = 0.0;
                for (int m = 0; m < 5; m++) {
                    int nc = ep.move_dest[m];
                    if (m > 0 && nc == ep.start_cell) continue;
                    int nd = base_dist[u.team][nc];
                    double w = (nd < cur_d) ? 10.0 : (nd == cur_d ? 1.5 : 0.3);
                    ep.move_prob[m] = w;
                    sum_w += w;
                }
                for (int m = 0; m < 5; m++) ep.move_prob[m] /= max(1e-9, sum_w);
            } else {
                int best_f_cell = -1;
                int best_f_dist = INF_DIST;
                for (const auto& f : g.flags) {
                    if (f.status == "home" || f.status == "dropped") {
                        int fc = cell_id(f.x, f.y);
                        int d = cell_dist[ep.start_cell][fc];
                        if (f.status == "dropped" && g.turn + max(0, d - 1) >= f.return_at) continue;
                        if (d < best_f_dist) {
                            best_f_dist = d;
                            best_f_cell = fc;
                        }
                    }
                }
                if (best_f_cell != -1 && best_f_dist == 0) {
                    ep.move_prob[0] = 0.85;
                    double rem = 0.15 / 4.0;
                    for (int m = 1; m < 5; m++) ep.move_prob[m] = rem;
                } else if (best_f_cell != -1 && best_f_dist == 1) {
                    for (int m = 0; m < 5; m++) {
                        if (ep.move_dest[m] == best_f_cell) ep.move_prob[m] = 0.80;
                        else ep.move_prob[m] = 0.05;
                    }
                } else {
                    int target_c = best_f_cell;
                    int target_d = best_f_dist;
                    for (const auto& cu : g.units) {
                        if (cu.team != u.team && cu.x != -1 && cu.flag != -1) {
                            int cc = cell_id(cu.x, cu.y);
                            int d = cell_dist[ep.start_cell][cc];
                            if (d < target_d) {
                                target_d = d;
                                target_c = cc;
                            }
                        }
                    }
                    // Also check if an enemy unit is very close (combat engagement)
                    for (const auto& cu : g.units) {
                        if (cu.team != u.team && cu.x != -1 && !cu.immune) {
                            int cc = cell_id(cu.x, cu.y);
                            int d = cell_dist[ep.start_cell][cc];
                            if (d <= 2 && d < target_d) {
                                target_d = d;
                                target_c = cc;
                            }
                        }
                    }
                    double sum_w = 0.0;
                    for (int m = 0; m < 5; m++) {
                        int nc = ep.move_dest[m];
                        if (m > 0 && nc == ep.start_cell) continue;
                        double w = 1.0;
                        if (target_c != -1) {
                            int cur_d = cell_dist[ep.start_cell][target_c];
                            int nd = cell_dist[nc][target_c];
                            if (nd < cur_d) w = 6.0;
                            else if (nd == cur_d) w = 1.5;
                            else w = 0.4;
                        }
                        ep.move_prob[m] = w;
                        sum_w += w;
                    }
                    for (int m = 0; m < 5; m++) ep.move_prob[m] /= max(1e-9, sum_w);
                }
            }

            int best_m = 0;
            for (int m = 1; m < 5; m++) {
                if (ep.move_prob[m] > ep.move_prob[best_m]) best_m = m;
            }
            ep.most_likely_dest = ep.move_dest[best_m];
            for (int m = 0; m < 5; m++) {
                enemy_want_prob[ep.move_dest[m]] += ep.move_prob[m];
            }

            if (u.flag == -1) {
                double immune_scale = u.immune ? 0.75 : 1.0;
                for (int m = 0; m < 5; m++) {
                    if (ep.move_prob[m] <= 0.0) continue;
                    int ec = ep.move_dest[m];
                    for (int ac : attackable_from[ec]) {
                        cell_attack_risk[ac] += immune_scale * ep.move_prob[m];
                    }
                }
            }

            enemy_preds.push_back(ep);
        }

        // Also add mild risk around enemy bases where enemies will respawn in 1-2 turns
        for (const auto& u : g.units) {
            if (u.team != me && u.x == -1 && u.respawn_at != -1 && u.respawn_at <= g.turn + 2) {
                int sc = spawn_order[u.team][0];
                for (int ac : attackable_from[sc]) {
                    cell_attack_risk[ac] += (u.respawn_at == g.turn + 1) ? 0.5 : 0.25;
                }
            }
        }

        // 3. Mild Danger-Aware Dijkstra to Our Base for Flag Carriers
        // Keep weights mild so carriers NEVER get herded to map edges away from our base!
        double risk_weights[3] = {1.4, 0.8, 0.35};
        vector<vector<double>> carrier_home_cost(3, vector<double>(N, 1e9));
        for (int tier = 0; tier < 3; tier++) {
            double rw = risk_weights[tier];
            auto& dist = carrier_home_cost[tier];
            priority_queue<pair<double, int>, vector<pair<double, int>>, greater<pair<double, int>>> pq;
            for (int c : spawn_order[me]) {
                if (!is_wall[c]) {
                    double init_cost = 0.2 * cell_attack_risk[c];
                    dist[c] = init_cost;
                    pq.push({init_cost, c});
                }
            }
            while (!pq.empty()) {
                auto [d, u] = pq.top();
                pq.pop();
                if (d > dist[u] + 1e-9) continue;
                int ux = u % S, uy = u / S;
                for (int m = 1; m < 5; m++) {
                    int vx = ux + DX[m], vy = uy + DY[m];
                    if (vx < 0 || vx >= S || vy < 0 || vy >= S) continue;
                    int v = vy * S + vx;
                    if (is_wall[v]) continue;
                    double step_cost = 1.0 + rw * cell_attack_risk[v] + 0.6 * enemy_want_prob[v];
                    if (in_team_base[v] != -1 && in_team_base[v] != me) step_cost += 1.5;
                    if (d + step_cost < dist[v]) {
                        dist[v] = d + step_cost;
                        pq.push({dist[v], v});
                    }
                }
            }
        }

        // 4. Identify Macro Targets for Our 3 Units (3*me + k)
        struct MacroTarget {
            int type; // 0: flag, 1: support_flag, 2: intercept_carrier, 3: escort_my_carrier, 4: camp_spot
            int target_cell;
            int ref_id;
            double base_val;
        };
        vector<MacroTarget> tasks;

        // 4a. Reachable ground flags (home or dropped)
        for (const auto& f : g.flags) {
            if (f.status != "home" && f.status != "dropped") continue;
            int fc = cell_id(f.x, f.y);
            int d_home = base_dist[me][fc];
            int min_enemy_d = INF_DIST;
            for (const auto& u : g.units) {
                if (u.team != me && u.x != -1 && u.flag == -1) {
                    int d = cell_dist[cell_id(u.x, u.y)][fc];
                    if (d < min_enemy_d) min_enemy_d = d;
                }
            }
            tasks.push_back({0, fc, f.id, 230.0 - 2.2 * d_home});
            if (teams == 2) {
                if (min_enemy_d <= 10 || d_home <= 12) {
                    double contest_bonus = (min_enemy_d <= 4) ? 24.0 : (min_enemy_d <= 7 ? 12.0 : 0.0);
                    tasks.push_back({1, fc, f.id, 142.0 - 1.8 * d_home + contest_bonus});
                }
            } else {
                if (min_enemy_d <= 8 || d_home <= 10) {
                    tasks.push_back({1, fc, f.id, 135.0 - 2.0 * d_home - 2.0 * min_enemy_d});
                }
            }
        }

        // 4b. Enemy flag carriers (accurate interception along path to enemy base!)
        int my_score = g.score[me];
        for (const auto& u : g.units) {
            if (u.team == me || u.x == -1 || u.flag == -1) continue;
            int vc = cell_id(u.x, u.y);
            int d_ebase = base_dist[u.team][vc];
            int hits_needed = (u.hp + g.damage - 1) / g.damage;

            int cur = vc;
            vector<int> path_to_ebase = {cur};
            for (int step = 0; step < d_ebase; step++) {
                int best_nc = cur;
                int best_d = base_dist[u.team][cur];
                for (int m = 1; m < 5; m++) {
                    int nc = next_cell(cur, m);
                    if (base_dist[u.team][nc] < best_d) {
                        best_d = base_dist[u.team][nc];
                        best_nc = nc;
                    }
                }
                cur = best_nc;
                path_to_ebase.push_back(cur);
            }

            // s = 1 .. d_ebase is the number of turns from now (s = 1 is THIS turn)
            int potential_hits = 0;
            int earliest_catch_step = INF_DIST;
            int best_intercept_cell = path_to_ebase.back();
            for (int k = 0; k < 3; k++) {
                const auto& mu = g.units[3 * me + k];
                if (mu.x == -1 || mu.flag != -1) continue;
                int mc = cell_id(mu.x, mu.y);
                for (int s = 1; s <= d_ebase; s++) {
                    if (s == 1 && mu.immune) continue;
                    int pc = path_to_ebase[s];
                    // Can mu reach within attack range 2 of pc in s moves?
                    bool can_hit_at_s = false;
                    for (int ac : attackable_from[pc]) {
                        if (cell_dist[mc][ac] <= s) {
                            can_hit_at_s = true;
                            break;
                        }
                    }
                    if (can_hit_at_s) {
                        int unit_hits = d_ebase - s + 1;
                        // If mu can reach pc itself in <= s moves, mu can body-block the carrier!
                        if (cell_dist[mc][pc] <= s) unit_hits += 1;
                        potential_hits += unit_hits;
                        if (s < earliest_catch_step) {
                            earliest_catch_step = s;
                            // Aim to step directly in front of the carrier on its path if reachable, else pc
                            best_intercept_cell = pc;
                            for (int bs = s; bs <= d_ebase; bs++) {
                                if (cell_dist[mc][path_to_ebase[bs]] <= bs) {
                                    best_intercept_cell = path_to_ebase[bs];
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }
            // Also count third-party enemies already adjacent to this carrier
            for (const auto& other : g.units) {
                if (other.team != me && other.team != u.team && other.x != -1 && other.flag == -1 && !other.immune) {
                    if (cell_dist[cell_id(other.x, other.y)][vc] <= 2) potential_hits += 1;
                }
            }

            if (potential_hits >= hits_needed) {
                double rival_bonus = (teams == 2) ? 75.0 : (abs(g.score[u.team] - my_score) <= 2 ? 35.0 : 10.0);
                double steal_bonus = max(0.0, 55.0 - 2.0 * base_dist[me][best_intercept_cell]);
                double val = 155.0 + rival_bonus + steal_bonus - 3.0 * earliest_catch_step + (3 - hits_needed) * 25.0;
                tasks.push_back({2, best_intercept_cell, u.id, val});
                if (hits_needed >= 2) {
                    tasks.push_back({2, best_intercept_cell, u.id, val - 25.0});
                }
            }
        }

        // 4c. Escort our own flag carrier(s)
        for (int k = 0; k < 3; k++) {
            const auto& mu = g.units[3 * me + k];
            if (mu.x == -1 || mu.flag == -1) continue;
            int mc = cell_id(mu.x, mu.y);
            int closest_threat_cell = mc;
            int min_ed = INF_DIST;
            for (const auto& eu : g.units) {
                if (eu.team == me || eu.x == -1 || eu.flag != -1) continue;
                int ec = cell_id(eu.x, eu.y);
                int d = cell_dist[ec][mc];
                if (d < min_ed) {
                    min_ed = d;
                    closest_threat_cell = ec;
                }
            }
            if (min_ed <= 7) {
                double urgency = (mu.hp <= 34 ? 65.0 : (mu.hp <= 68 ? 35.0 : 18.0));
                tasks.push_back({3, closest_threat_cell, mu.id, 150.0 + urgency - 3.5 * min_ed});
            }
        }

        // 4d. Camp unoccupied flag spots
        int min_return_rem = (returning_flags_now > 0) ? 0 : INF_DIST;
        for (const auto& f : g.flags) {
            if ((f.status == "cooldown" || f.status == "dropped") && f.return_at != -1) {
                int rem = max(0, f.return_at - g.turn);
                if (rem < min_return_rem) min_return_rem = rem;
            }
        }
        double base_spot_val = 95.0;
        if (teams == 2) {
            base_spot_val = 58.0;
            if (min_return_rem == 0) base_spot_val = 122.0;
            else if (min_return_rem <= 3) base_spot_val = 104.0 - 7.0 * min_return_rem;
            else if (min_return_rem <= 8) base_spot_val = 80.0 - 2.5 * min_return_rem;
        }

        for (int sidx = 0; sidx < (int)spot_cells.size(); sidx++) {
            if (spot_has_home_flag[sidx]) continue;
            int sc = spot_cells[sidx];
            int d_home = base_dist[me][sc];
            double spot_val = base_spot_val - 1.8 * d_home;
            if (sidx == me) spot_val += 22.0;
            if (sidx == teams) spot_val += 16.0;
            if (teams > 2 && returning_flags_now > 0) spot_val += 25.0;
            tasks.push_back({4, sc, sidx, spot_val});
        }
        // Patrol our own flag spot or center flag spot rather than hiking across the entire map to far enemy spots
        tasks.push_back({4, spot_cells[me], me, 60.0});
        tasks.push_back({4, spot_cells[teams], teams, 55.0});
        tasks.push_back({4, spawn_order[me][0], -1, -500.0});

        int num_tasks = (int)tasks.size();
        auto unit_task_score = [&](int k, int tidx) -> double {
            const auto& u = g.units[3 * me + k];
            if (u.x == -1 || u.flag != -1) return -1e9;
            int uc = cell_id(u.x, u.y);
            const auto& t = tasks[tidx];
            int d = cell_dist[uc][t.target_cell];

            if (t.type == 0 || t.type == 1) {
                const auto& f = g.flags[t.ref_id];
                int arrive_turn = g.turn + max(0, d - 1);
                if (f.status == "dropped" && f.return_at != -1 && arrive_turn >= f.return_at) {
                    return -1e8;
                }
                if (arrive_turn + base_dist[me][t.target_cell] > g.turns) {
                    return -1e7;
                }
                int min_ed = INF_DIST;
                int min_eteam = -1;
                for (const auto& eu : g.units) {
                    if (eu.team != me && eu.x != -1 && eu.flag == -1) {
                        int ed = cell_dist[cell_id(eu.x, eu.y)][t.target_cell];
                        if (ed < min_ed) {
                            min_ed = ed;
                            min_eteam = eu.team;
                        }
                    }
                }
                if (teams == 2 && t.type == 1) {
                    double s = t.base_val - 3.6 * d;
                    int enemy_pickup_turn = g.turn + max(0, min_ed - 1);
                    int enemy_score_turn = enemy_pickup_turn + (min_eteam != -1 ? base_dist[min_eteam][t.target_cell] : 10);
                    if (g.turn + max(0, d - 2) > enemy_score_turn) s -= 100.0;
                    return s;
                }
                double s = t.base_val - 4.0 * d;
                if (teams == 2 && t.type == 0 && min_ed <= d + 3) {
                    s += ((u.hp + 33) / 34) * 10.0;
                }
                if (d < min_ed) s += 35.0;
                else if (d == min_ed) s += 10.0 + (u.hp - 66) * 0.25;
                else {
                    int e_base_d = (min_eteam != -1) ? base_dist[min_eteam][t.target_cell] : 10;
                    int enemy_pickup_turn = g.turn + max(0, min_ed - 1);
                    int enemy_score_turn = enemy_pickup_turn + e_base_d;
                    if (teams == 2) {
                        if (g.turn + d > enemy_score_turn) s -= 120.0;
                        else s -= 8.0 * (d - min_ed);
                    } else {
                        int my_ebase_d = (min_eteam != -1) ? max(0, (int)base_dist[min_eteam][uc] - 2) : d;
                        if (g.turn + d > enemy_score_turn || g.turn + my_ebase_d > enemy_score_turn) {
                            s -= 220.0;
                        } else if (base_dist[me][t.target_cell] > 14 && d > min_ed + 1) {
                            s -= 140.0;
                        } else {
                            s -= 8.0 * (d - min_ed);
                        }
                    }
                }
                return s;
            }
            return t.base_val - 3.5 * d;
        };

        int assigned_task[3] = {-1, -1, -1};
        double best_assign_total = -1e18;
        for (int i = 0; i < num_tasks; i++) {
            for (int j = 0; j < num_tasks; j++) {
                for (int l = 0; l < num_tasks; l++) {
                    int idx[3] = {i, j, l};
                    bool dup = false;
                    for (int a = 0; a < 3; a++) {
                        if (g.units[3 * me + a].x == -1 || g.units[3 * me + a].flag != -1) continue;
                        for (int b = a + 1; b < 3; b++) {
                            if (g.units[3 * me + b].x == -1 || g.units[3 * me + b].flag != -1) continue;
                            if (idx[a] == idx[b]) dup = true;
                            if (tasks[idx[a]].type == 0 && tasks[idx[b]].type == 0 &&
                                tasks[idx[a]].ref_id == tasks[idx[b]].ref_id) dup = true;
                            if (tasks[idx[a]].type == 4 && tasks[idx[b]].type == 4 &&
                                tasks[idx[a]].target_cell == tasks[idx[b]].target_cell) dup = true;
                        }
                    }
                    if (dup) continue;
                    double total = 0.0;
                    for (int a = 0; a < 3; a++) {
                        if (g.units[3 * me + a].x != -1 && g.units[3 * me + a].flag == -1) {
                            total += unit_task_score(a, idx[a]);
                        }
                    }
                    if (total > best_assign_total) {
                        best_assign_total = total;
                        assigned_task[0] = i;
                        assigned_task[1] = j;
                        assigned_task[2] = l;
                    }
                }
            }
        }

        // 5. Joint 3-Unit Move + Action Search over all 5^3 = 125 move tuples
        int start_c[3];
        vector<int> cand_moves[3];
        for (int k = 0; k < 3; k++) {
            const auto& u = g.units[3 * me + k];
            if (u.x == -1) {
                start_c[k] = -1;
                cand_moves[k] = {0};
            } else {
                start_c[k] = cell_id(u.x, u.y);
                cand_moves[k] = {0};
                for (int m = 1; m < 5; m++) {
                    if (next_cell(start_c[k], m) != start_c[k]) {
                        cand_moves[k].push_back(m);
                    }
                }
            }
        }

        // Precompute exact pairwise incoming hit probability & outgoing hit probability for each unit k and move m
        // in_hit_prob[k][m][eidx]: probability enemy eidx can hit unit k if unit k makes move m
        double in_hit_prob[3][5][45];
        uint8_t in_hit_mask[3][5][45];
        double block_prob_move[3][5];
        for (int k = 0; k < 3; k++) {
            for (int m = 0; m < 5; m++) {
                block_prob_move[k][m] = 0.0;
                for (int eidx = 0; eidx < (int)enemy_preds.size(); eidx++) {
                    in_hit_prob[k][m][eidx] = 0.0;
                    in_hit_mask[k][m][eidx] = 0;
                }
                if (start_c[k] == -1) continue;
                int sc = start_c[k];
                int wc = next_cell(sc, m);
                double bp = 0.0;
                if (wc != sc) {
                    for (const auto& ep : enemy_preds) {
                        if (ep.start_cell == wc) {
                            for (int em = 0; em < 5; em++) {
                                if (ep.move_dest[em] == wc || ep.move_dest[em] == sc) bp += ep.move_prob[em];
                            }
                        } else {
                            for (int em = 1; em < 5; em++) {
                                if (ep.move_dest[em] == wc) bp += ep.move_prob[em];
                            }
                        }
                    }
                }
                block_prob_move[k][m] = min(1.0, bp);

                for (int eidx = 0; eidx < (int)enemy_preds.size(); eidx++) {
                    const auto& ep = enemy_preds[eidx];
                    const auto& eu = g.units[ep.id];
                    if (eu.immune) continue;
                    int sdx = abs(cell_x(sc) - eu.x), sdy = abs(cell_y(sc) - eu.y);
                    if (sdx + sdy > 4) continue;
                    double hp_p = 0.0;
                    uint8_t hmask = 0;
                    for (int em = 0; em < 5; em++) {
                        if (ep.move_prob[em] <= 0.0) continue;
                        auto [end_u, end_v] = resolve_pair(sc, wc, ep.start_cell, ep.move_dest[em]);
                        if (can_attack_cell(end_u, end_v)) {
                            hp_p += ep.move_prob[em];
                            hmask |= (uint8_t)(1 << em);
                        }
                    }
                    in_hit_prob[k][m][eidx] = hp_p;
                    in_hit_mask[k][m][eidx] = hmask;
                }
            }
        }

        double best_tuple_score = -1e18;
        int best_m[3] = {0, 0, 0};
        string best_act[3] = {"-", "-", "-"};

        for (int m0 : cand_moves[0]) {
            for (int m1 : cand_moves[1]) {
                for (int m2 : cand_moves[2]) {
                    int m[3] = {m0, m1, m2};
                    int want_c[3];
                    for (int k = 0; k < 3; k++) {
                        want_c[k] = (start_c[k] == -1) ? -1 : next_cell(start_c[k], m[k]);
                    }

                    // Check friendly self-collisions & chain-blocks
                    bool friendly_conflict = false;
                    for (int a = 0; a < 3; a++) {
                        if (start_c[a] == -1) continue;
                        for (int b = a + 1; b < 3; b++) {
                            if (start_c[b] == -1) continue;
                            if (want_c[a] == want_c[b]) friendly_conflict = true;
                        }
                        for (int b = 0; b < 3; b++) {
                            if (a == b || start_c[b] == -1) continue;
                            if (want_c[a] == start_c[b] && want_c[b] == start_c[b]) friendly_conflict = true;
                            if (want_c[a] == start_c[b] && want_c[b] != start_c[a]) {
                                for (int c = 0; c < 3; c++) {
                                    if (c != a && c != b && start_c[c] != -1 && want_c[b] == start_c[c] && want_c[c] == start_c[c]) {
                                        friendly_conflict = true;
                                    }
                                }
                            }
                        }
                    }
                    if (friendly_conflict) continue;

                    // Determine which units have a valid ground flag at want_c[k]
                    bool has_flag_at_wc[3] = {false, false, false};
                    int flag_id_at_wc[3] = {-1, -1, -1};
                    for (int k = 0; k < 3; k++) {
                        const auto& u = g.units[3 * me + k];
                        if (u.x == -1 || u.flag != -1) continue;
                        int wc = want_c[k];
                        int fid = active_ground_flag_at[wc];
                        if (fid != -1) {
                            const auto& fl = g.flags[fid];
                            if (fl.status == "home" || (fl.status == "dropped" && fl.return_at > g.turn)) {
                                has_flag_at_wc[k] = true;
                                flag_id_at_wc[k] = fid;
                            }
                        }
                    }

                    // For any unit k with has_flag_at_wc[k], we can evaluate BOTH pickup_mode=1 ('P') and pickup_mode=0 (fight/attack)!
                    int max_mask = 1;
                    int flag_unit_list[3];
                    int num_flag_units = 0;
                    for (int k = 0; k < 3; k++) {
                        if (has_flag_at_wc[k]) {
                            flag_unit_list[num_flag_units++] = k;
                            max_mask *= 2;
                        }
                    }

                    for (int mask = 0; mask < max_mask; mask++) {
                        bool try_pickup[3] = {false, false, false};
                        for (int idx = 0; idx < num_flag_units; idx++) {
                            if ((mask >> idx) & 1) {
                                try_pickup[flag_unit_list[idx]] = true;
                            }
                        }

                        double tuple_score = 0.0;
                        string act[3] = {"-", "-", "-"};
                        bool can_use_attack[3] = {false, false, false};
                        double exp_in_hits_turn[3] = {0.0, 0.0, 0.0};
                        int max_in_hits_turn[3] = {0, 0, 0};

                        for (int k = 0; k < 3; k++) {
                            const auto& u = g.units[3 * me + k];
                            if (u.x == -1 || u.immune) continue;
                            for (int eidx = 0; eidx < (int)enemy_preds.size(); eidx++) {
                                const auto& eu = g.units[enemy_preds[eidx].id];
                                if (eu.flag != -1 || eu.immune) continue;
                                double p = in_hit_prob[k][m[k]][eidx];
                                if (p > 0.05) {
                                    exp_in_hits_turn[k] += p;
                                    if (p > 0.20) max_in_hits_turn[k]++;
                                }
                            }
                        }

                        // Evaluate movement & carrier survival
                        for (int k = 0; k < 3; k++) {
                            const auto& u = g.units[3 * me + k];
                            if (u.x == -1) continue;
                            int sc = start_c[k];
                            int wc = want_c[k];
                            double bp = block_prob_move[k][m[k]];
                            int hits_to_die = (u.hp + g.damage - 1) / g.damage;

                            if (u.flag != -1) {
                                int tier = (u.hp <= 34) ? 0 : (u.hp <= 68 ? 1 : 2);
                                int d_sc = base_dist[me][sc];
                                int d_wc = base_dist[me][wc];
                                double eff_d = (1.0 - bp) * d_wc + bp * (d_sc + 0.9);
                                double cost_wc = carrier_home_cost[tier][wc];
                                double cost_sc = carrier_home_cost[tier][sc];
                                double eff_cost = (1.0 - bp) * cost_wc + bp * (cost_sc + 1.0);

                                // Strongly reward true base_dist progress so carrier never runs away to map border!
                                tuple_score += 500.0 - 26.0 * eff_d - 6.0 * eff_cost;
                                if (d_wc > d_sc) {
                                    // Heavy penalty for moving strictly away from base
                                    tuple_score -= 32.0;
                                }

                                // Check next-turn pursuit danger (including newly respawned immune enemies!)
                                if (in_team_base[wc] != me) {
                                    int min_ed_wc = INF_DIST;
                                    double next_turn_pursuit = 0.0;
                                    for (int eidx = 0; eidx < (int)enemy_preds.size(); eidx++) {
                                        const auto& ep = enemy_preds[eidx];
                                        const auto& eu = g.units[ep.id];
                                        if (eu.flag != -1) continue;
                                        int d0 = cell_dist[ep.start_cell][wc];
                                        if (d0 < min_ed_wc) min_ed_wc = d0;
                                        if (d0 <= 4) {
                                            for (int em = 0; em < 5; em++) {
                                                if (ep.move_prob[em] <= 0.0) continue;
                                                if (cell_dist[ep.move_dest[em]][wc] <= 2) {
                                                    next_turn_pursuit += ep.move_prob[em];
                                                }
                                            }
                                        }
                                    }
                                    if (min_ed_wc < INF_DIST) {
                                        tuple_score += 0.35 * min(8, min_ed_wc);
                                    }
                                    double pursuit_w = (hits_to_die == 1) ? 5.5 : 3.0;
                                    tuple_score -= pursuit_w * next_turn_pursuit;
                                }

                                // Immediate capture in Step 6 if we enter our base AND survive Step 3!
                                double death_prob = 0.0;
                                if (max_in_hits_turn[k] >= hits_to_die) {
                                    death_prob = min(1.0, exp_in_hits_turn[k] / hits_to_die);
                                }
                                if (in_team_base[wc] == me) {
                                    tuple_score += 240.0 * (1.0 - bp) * (1.0 - 0.9 * death_prob);
                                }

                                if (max_in_hits_turn[k] >= hits_to_die) {
                                    tuple_score -= 140.0 * exp_in_hits_turn[k];
                                    if (exp_in_hits_turn[k] >= hits_to_die - 0.35) tuple_score -= 160.0;
                                } else {
                                    tuple_score -= 8.0 * exp_in_hits_turn[k];
                                }
                            } else {
                                // Non-carrier unit
                                can_use_attack[k] = !u.immune && !try_pickup[k];
                                int tidx = assigned_task[k];
                                int target_c = (tidx != -1) ? tasks[tidx].target_cell : spot_cells[me];
                                int ttype = (tidx != -1) ? tasks[tidx].type : 4;

                                int d_sc = cell_dist[sc][target_c];
                                int d_wc = cell_dist[wc][target_c];
                                double eff_d = (1.0 - bp) * d_wc + bp * (d_sc + 0.8);
                                double weight_d = (ttype == 0) ? 10.0 : (ttype == 2 ? 9.5 : 7.0);
                                tuple_score += 100.0 - weight_d * eff_d;

                                if (has_flag_at_wc[k] && !try_pickup[k]) {
                                    // Standing on or stepping onto the flag while fighting!
                                    // Bonus is awarded inside combat loop when unit k actually attacks an enemy
                                    tuple_score += 0.0;
                                }

                                if (ttype == 2 && tidx != -1) {
                                    int carrier_id = tasks[tidx].ref_id;
                                    const auto& cu = g.units[carrier_id];
                                    if (cu.x != -1) {
                                        int cc = cell_id(cu.x, cu.y);
                                        if (wc == cc) tuple_score += 25.0;
                                        else if (cell_dist[wc][cc] == 1 && base_dist[cu.team][wc] < base_dist[cu.team][cc]) {
                                            tuple_score += 32.0;
                                        }
                                    }
                                }

                                if (ttype == 3 && tidx != -1) {
                                    int my_cid = tasks[tidx].ref_id;
                                    const auto& mcu = g.units[my_cid];
                                    if (mcu.x != -1 && wc == cell_id(mcu.x, mcu.y)) {
                                        tuple_score += 16.0;
                                    }
                                }

                                // Escort body-blocking & swap screening for any nearby friendly flag carrier
                                for (int ck = 0; ck < 3; ck++) {
                                    if (ck == k || start_c[ck] == -1 || g.units[3 * me + ck].flag == -1) continue;
                                    int wc_c = want_c[ck];
                                    if (cell_dist[wc][wc_c] <= 2) {
                                        for (const auto& ep : enemy_preds) {
                                            const auto& eu = g.units[ep.id];
                                            if (eu.flag != -1) continue;
                                            int d_ec = cell_dist[ep.start_cell][wc_c];
                                            if (d_ec <= 3) {
                                                // Unit k interposes between enemy ep and our carrier ck!
                                                if (cell_dist[ep.start_cell][wc] < d_ec && cell_dist[wc][wc_c] == 1) {
                                                    tuple_score += 14.0;
                                                }
                                                if (wc == ep.start_cell || wc == ep.most_likely_dest) {
                                                    tuple_score += 10.0;
                                                }
                                            }
                                        }
                                    }
                                }

                                // Non-carrier survival: don't double-count on flag tiles, and never make 32-HP units flee from fights!
                                if (!has_flag_at_wc[k]) {
                                    if (max_in_hits_turn[k] >= hits_to_die) {
                                        double penalty = (hits_to_die == 1) ? 18.0 : 12.0;
                                        tuple_score -= penalty * exp_in_hits_turn[k];
                                    } else {
                                        tuple_score -= 3.0 * exp_in_hits_turn[k];
                                    }
                                }
                            }
                        }

                        // 6. Coordinated Attack Target Selection for units with can_use_attack[k] == true
                        struct HitOption {
                            int enemy_idx;
                            double hit_prob;
                        };
                        vector<HitOption> options[3];
                        for (int k = 0; k < 3; k++) {
                            if (!can_use_attack[k]) {
                                options[k].push_back({-1, 0.0});
                                continue;
                            }
                            for (int eidx = 0; eidx < (int)enemy_preds.size(); eidx++) {
                                double hp_p = in_hit_prob[k][m[k]][eidx];
                                if (hp_p > 1e-6) {
                                    options[k].push_back({eidx, hp_p});
                                }
                            }
                            options[k].push_back({-1, 0.0});
                        }

                        double best_combat_plus_pickup = -1e18;
                        int best_target_eidx[3] = {-1, -1, -1};

                        for (const auto& o0 : options[0]) {
                            for (const auto& o1 : options[1]) {
                                for (const auto& o2 : options[2]) {
                                    int sel_e[3] = {o0.enemy_idx, o1.enemy_idx, o2.enemy_idx};
                                    double sel_p[3] = {o0.hit_prob, o1.hit_prob, o2.hit_prob};

                                    double c_score = 0.0;
                                    double enemy_kill_prob[45] = {0.0};

                                    for (int a = 0; a < 3; a++) {
                                        int eidx = sel_e[a];
                                        if (eidx == -1) continue;
                                        bool first = true;
                                        for (int b = 0; b < a; b++) {
                                            if (sel_e[b] == eidx) first = false;
                                        }
                                        if (!first) continue;

                                        int h_em[5] = {0, 0, 0, 0, 0};
                                        double sum_p = 0.0;
                                        int attackers_cnt = 0;
                                        for (int b = a; b < 3; b++) {
                                            if (sel_e[b] == eidx) {
                                                sum_p += sel_p[b];
                                                attackers_cnt++;
                                                uint8_t hmask = in_hit_mask[b][m[b]][eidx];
                                                for (int em = 0; em < 5; em++) {
                                                    if ((hmask >> em) & 1) h_em[em]++;
                                                }
                                            }
                                        }

                                        const auto& ep = enemy_preds[eidx];
                                        const auto& eu = g.units[ep.id];
                                        int needed = (eu.hp + g.damage - 1) / g.damage;

                                        double effective_hits = 0.0;
                                        double kp = 0.0;
                                        bool minimax_guar_kill = true;
                                        for (int em = 0; em < 5; em++) {
                                            if (ep.move_prob[em] <= 0.0) continue;
                                            effective_hits += ep.move_prob[em] * min(needed, h_em[em]);
                                            if (h_em[em] >= needed) {
                                                kp += ep.move_prob[em];
                                            } else {
                                                minimax_guar_kill = false;
                                            }
                                        }

                                        double hit_val = 30.0;
                                        if (eu.flag != -1) {
                                            int d_eb = base_dist[eu.team][ep.start_cell];
                                            hit_val = 80.0 + max(0, 10 - d_eb) * 5.0;
                                            if (teams == 2 || abs(g.score[eu.team] - my_score) <= 2) hit_val += 25.0;
                                        } else {
                                            int ec = ep.start_cell;
                                            for (const auto& fl : g.flags) {
                                                if ((fl.status == "home" || fl.status == "dropped") && fl.x != -1) {
                                                    if (cell_dist[ec][cell_id(fl.x, fl.y)] <= 2) hit_val += 25.0;
                                                }
                                            }
                                            for (int mk = 0; mk < 3; mk++) {
                                                const auto& mu = g.units[3 * me + mk];
                                                if (mu.x != -1 && mu.flag != -1 && cell_dist[ec][cell_id(mu.x, mu.y)] <= 3) {
                                                    hit_val += 32.0;
                                                }
                                            }
                                        }

                                        c_score += effective_hits * hit_val;
                                        if (attackers_cnt >= needed) {
                                            enemy_kill_prob[eidx] = kp;
                                            double kill_bonus = (eu.flag != -1) ? 165.0 : 65.0;
                                            c_score += kp * kill_bonus;
                                            if (minimax_guar_kill) c_score += 28.0;
                                        }
                                        c_score += 0.5 * sum_p;
                                    }

                                    // Now evaluate Pickup Survival for any unit k with has_flag_at_wc[k]
                                    for (int k = 0; k < 3; k++) {
                                        if (!has_flag_at_wc[k]) continue;
                                        if (!try_pickup[k]) {
                                            if (sel_e[k] != -1) {
                                                // Occupying/blocking the flag tile while actively shooting an enemy in range!
                                                c_score += 110.0 * (1.0 - block_prob_move[k][m[k]]);
                                            } else {
                                                // Standing on a flag without picking it up AND without attacking anyone is only for 2p ambush when lethal chase awaits
                                                c_score += (teams == 2) ? 85.0 : -150.0;
                                            }
                                            continue;
                                        }
                                        const auto& u = g.units[3 * me + k];
                                        int sc = start_c[k];
                                        int wc = want_c[k];
                                        double bp = block_prob_move[k][m[k]];

                                        // If stepping onto wc from sc != wc and an enemy is on wc or adjacent to wc (contesting),
                                        // stepping onto wc with 'P' will get blocked by collision!
                                        bool contested_step = false;
                                        if (sc != wc) {
                                            for (int eidx = 0; eidx < (int)enemy_preds.size(); eidx++) {
                                                const auto& eu = g.units[enemy_preds[eidx].id];
                                                int ec = enemy_preds[eidx].start_cell;
                                                if (ec == wc && !eu.immune) contested_step = true;
                                                if (eu.flag == -1 && !eu.immune && cell_dist[ec][wc] == 1 && active_ground_flag_at[ec] == -1) {
                                                    contested_step = true;
                                                }
                                            }
                                        }
                                        int fid = flag_id_at_wc[k];
                                        bool urgent_expire = (fid != -1 && g.flags[fid].status == "dropped" && g.flags[fid].return_at == g.turn + 1);
                                        if (contested_step && !urgent_expire && !u.immune) {
                                            // Collision blocks pickup anyway; fighting is strictly better!
                                            c_score -= 100.0;
                                            continue;
                                        }

                                        int hits_to_die = (u.hp + g.damage - 1) / g.damage;
                                        int d_home = base_dist[me][wc];

                                        // Check Step 3 survival on THIS turn: if we die in Step 3, Step 4 pickup doesn't happen!
                                        if (max_in_hits_turn[k] >= hits_to_die && exp_in_hits_turn[k] >= hits_to_die - 0.45) {
                                            c_score -= 120.0;
                                            continue;
                                        }

                                        // If wc is inside our base (d_home == 0), surviving Step 3 immediately captures in Step 6!
                                        if (d_home == 0) {
                                            c_score += 440.0 * (1.0 - bp);
                                            continue;
                                        }

                                        // Otherwise (d_home > 0), check if surviving armed enemies will chase & kill unit k before reaching base!
                                        int armed_teammates_near = 0;
                                        for (int mk = 0; mk < 3; mk++) {
                                            if (mk == k) continue;
                                            const auto& mu = g.units[3 * me + mk];
                                            if (mu.x != -1 && mu.flag == -1 && !try_pickup[mk] && mu.hp > 34) {
                                                if (cell_dist[want_c[mk]][wc] <= 3) armed_teammates_near++;
                                            }
                                        }

                                        double total_chase_hits = exp_in_hits_turn[k];
                                        int max_chaser_hits_before = 0;
                                        for (int eidx = 0; eidx < (int)enemy_preds.size(); eidx++) {
                                            const auto& eu = g.units[enemy_preds[eidx].id];
                                            if (eu.flag != -1) continue;
                                            if (teams > 2 && eu.immune) continue;
                                            double survive_e = 1.0 - enemy_kill_prob[eidx];
                                            if (survive_e <= 0.1) continue;
                                            int ec = enemy_preds[eidx].start_cell;
                                            int d_ev = cell_dist[ec][wc];
                                            int e_hits_before = (eu.hp + g.damage - 1) / g.damage;
                                            if (d_ev <= 3) {
                                                max_chaser_hits_before = max(max_chaser_hits_before, e_hits_before);
                                            }
                                            int e_hits_left = e_hits_before;
                                            for (int b = 0; b < 3; b++) {
                                                if (sel_e[b] == eidx && sel_p[b] >= 0.5) e_hits_left = max(1, e_hits_left - 1);
                                            }
                                            int max_chase = (teams == 2 && armed_teammates_near > 0) ? min(d_home, e_hits_left) : d_home;
                                            if (eu.immune) max_chase = min(max_chase, max(0, d_home - 1));

                                            if (d_ev <= 2) {
                                                total_chase_hits += survive_e * max_chase;
                                            } else if (d_ev == 3) {
                                                if (teams == 2) {
                                                    total_chase_hits += survive_e * 0.85 * max_chase;
                                                } else if (base_dist[me][ec] <= d_home) {
                                                    total_chase_hits += survive_e * max(0, d_home - 1);
                                                }
                                            } else if (teams == 2 && d_ev == 4 && base_dist[me][ec] <= d_home) {
                                                total_chase_hits += survive_e * max(0, max_chase - 1);
                                            }
                                        }

                                        // In 2p (zero-sum), always whittle down the chaser & block the flag rather than disarming at lethal chase;
                                        // In 3+p FFA, only stand and fight if we have enough HP/support to win the duel, else relay toward our base!
                                        bool can_win_standoff = (teams == 2) || (hits_to_die >= max_chaser_hits_before) || (armed_teammates_near > 0);

                                        if (total_chase_hits >= hits_to_die - 0.15 && can_win_standoff && !urgent_expire && !u.immune) {
                                            // Disarming ourselves by picking up the flag now means we die on the run home,
                                            // whereas we have enough HP/support to win the fight on the flag tile first!
                                            c_score -= 65.0;
                                        } else {
                                            // Either safe to run all the way home, OR Flag Relay toward our base when out-HP'd!
                                            c_score += 315.0 * (1.0 - 0.7 * bp) - 12.0 * total_chase_hits;
                                        }
                                    }

                                    if (c_score > best_combat_plus_pickup) {
                                        best_combat_plus_pickup = c_score;
                                        best_target_eidx[0] = sel_e[0];
                                        best_target_eidx[1] = sel_e[1];
                                        best_target_eidx[2] = sel_e[2];
                                    }
                                }
                            }
                        }

                        tuple_score += best_combat_plus_pickup;

                        for (int k = 0; k < 3; k++) {
                            if (try_pickup[k]) {
                                act[k] = "P";
                            } else if (can_use_attack[k] && best_target_eidx[k] != -1) {
                                act[k] = to_string(enemy_preds[best_target_eidx[k]].id);
                            } else if (!has_flag_at_wc[k] && g.units[3 * me + k].x != -1 && g.units[3 * me + k].flag == -1) {
                                act[k] = "P";
                            }
                        }

                        if (returning_flags_now > 0 && free_spots_count > 0) {
                            for (int k = 0; k < 3; k++) {
                                if (start_c[k] == -1 || g.units[3 * me + k].flag != -1) continue;
                                int wc = want_c[k];
                                int sidx = is_spot_cell[wc];
                                if (sidx != -1 && !spot_has_home_flag[sidx]) {
                                    // Never overwrite a valid attack on an in-range enemy with a speculative pickup!
                                    if (can_use_attack[k] && best_target_eidx[k] != -1) continue;
                                    act[k] = "P";
                                    tuple_score += 18.0 * min(1.0, (double)returning_flags_now / free_spots_count);
                                }
                            }
                        }

                        if (tuple_score > best_tuple_score) {
                            best_tuple_score = tuple_score;
                            for (int k = 0; k < 3; k++) {
                                best_m[k] = m[k];
                                best_act[k] = act[k];
                            }
                        }
                    }
                }
            }
        }

        string out = to_string(g.turn);
        for (int k = 0; k < 3; k++) {
            out += " ";
            out += MOVE_STR[best_m[k]];
            out += " ";
            out += best_act[k];
        }
        return out;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    Game g;
    Solver solver;
    while (read_turn(g)) {
        cout << solver.solve(g) << endl;
    }
    return 0;
}
