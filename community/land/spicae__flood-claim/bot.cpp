// 圈地 bot（AI 编写）
// 策略：安全优先 —— 矩形圈地扩张领地；随时抓住机会切断对手轨迹；找不到圈地机会时
// 用洪泛（BFS）评估每个方向的可达空间来求生。全程避开墙壁、自己的轨迹和对手的头部（防对撞）。
#include <bits/stdc++.h>
using namespace std;

const int DX[4] = {0, 0, -1, 1};
const int DY[4] = {-1, 1, 0, 0};
const char DC[4] = {'U', 'D', 'L', 'R'};

int dirIndex(char c) { return string("UDLR").find(c); }

struct Player { int x, y, dir, trailLen, area, deaths; };
struct State {
    int turn;
    vector<Player> p;
    vector<string> owner;  // owner[y][x]: '.' 中立，'0'..'3' 该玩家领地
    vector<string> trail;  // trail[y][x]: '.' 无，'0'..'3' 该玩家轨迹
};

int W, H, MAX_TURNS, N, ME, MOVE_MS, INIT_MS;
vector<pair<int, int>> spawn;
mt19937 rng(20261019);
deque<int> plan;         // 当前矩形圈地计划的剩余方向
bool wasOutside = false; // 上一回合是否在领地外

bool readState(State& s) {
    string tok;
    if (!(cin >> tok)) return false;  // "TURN"
    cin >> s.turn;
    s.p.assign(N, {});
    for (int i = 0; i < N; i++) {
        int id; char d; Player q;
        cin >> tok >> id >> q.x >> q.y >> d >> q.trailLen >> q.area >> q.deaths;
        q.dir = dirIndex(d);
        s.p[id] = q;
    }
    cin >> tok;  // OWNER
    s.owner.assign(H, "");
    for (auto& row : s.owner) cin >> row;
    cin >> tok;  // TRAIL
    s.trail.assign(H, "");
    for (auto& row : s.trail) cin >> row;
    cin >> tok;  // END
    return true;
}

inline bool inb(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }

bool isEnemyHead(const State& s, int x, int y) {
    for (int o = 0; o < N; o++)
        if (o != ME && s.p[o].x == x && s.p[o].y == y) return true;
    return false;
}

// BFS 从我的头出发，找一条到目标格（goal 返回 true）的路径，返回方向序列。
// 只走安全格：不越界、不踩自己的轨迹、不走进对手头部。对手的轨迹可以走（踩上去等于切断对手）。
template <class F>
vector<int> bfsPath(const State& s, F goal) {
    const Player& me = s.p[ME];
    char myTrail = '0' + ME;
    vector<int> from(W * H, -1), fdir(W * H, -1);
    deque<int> q;
    int start = me.y * W + me.x;
    from[start] = start;
    for (int d = 0; d < 4; d++) {
        if (d == (me.dir ^ 1)) continue;  // 不能掉头
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (!inb(nx, ny) || s.trail[ny][nx] == myTrail) continue;
        if (isEnemyHead(s, nx, ny)) continue;
        int k = ny * W + nx;
        if (from[k] != -1) continue;
        from[k] = start; fdir[k] = d; q.push_back(k);
    }
    while (!q.empty()) {
        int k = q.front(); q.pop_front();
        int x = k % W, y = k / W;
        if (goal(x, y)) {
            vector<int> path;
            for (int c = k; c != start; c = from[c]) path.push_back(fdir[c]);
            reverse(path.begin(), path.end());
            return path;
        }
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!inb(nx, ny) || s.trail[ny][nx] == myTrail) continue;
            if (isEnemyHead(s, nx, ny)) continue;
            int nk = ny * W + nx;
            if (from[nk] != -1) continue;
            from[nk] = k; fdir[nk] = d; q.push_back(nk);
        }
    }
    return {};
}

// 从 (sx,sy) 出发能到达的开阔格数量（不越界、不踩自己的轨迹、避开对手头部）。
int reachableSpace(const State& s, int sx, int sy) {
    char myTrail = '0' + ME;
    vector<int> seen(W * H, 0);
    deque<int> q;
    int start = sy * W + sx;
    seen[start] = 1; q.push_back(start);
    int cnt = 1;
    while (!q.empty()) {
        int k = q.front(); q.pop_front();
        int x = k % W, y = k / W;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!inb(nx, ny) || seen[ny * W + nx]) continue;
            if (s.trail[ny][nx] == myTrail) continue;
            if (isEnemyHead(s, nx, ny)) continue;
            seen[ny * W + nx] = 1; q.push_back(ny * W + nx); cnt++;
        }
    }
    return cnt;
}

// 生成一个矩形圈地计划：从领地出发，向外画一个矩形再回来圈地。
void makePlan(const State& s) {
    char mine = '0' + ME;
    vector<int> path = bfsPath(s, [&](int x, int y) { return s.owner[y][x] != mine; });
    if (path.empty()) return;
    const Player& me = s.p[ME];
    int ex = me.x, ey = me.y;
    for (int d : path) { ex += DX[d]; ey += DY[d]; }
    int d = path.back();  // 出领地的方向
    int best = -1, ba = 0, bb = 0, bs = 0;
    // 矩形边长上限：混战对手多、轨迹更容易被切，用更小的圈地。
    int maxa = (N >= 4) ? 6 : 9;
    for (int it = 0; it < 60; it++) {
        int a = 3 + rng() % (maxa - 2);
        int b = 3 + rng() % (maxa - 2);
        int side = (d < 2) ? (2 + rng() % 2) : (rng() % 2);  // 与 d 垂直的方向
        int fx = ex + DX[d] * (a - 1), fy = ey + DY[d] * (a - 1);
        int cx = fx + DX[side] * b, cy = fy + DY[side] * b;
        if (!inb(fx, fy) || !inb(cx, cy)) continue;
        int gain = 0, danger = 0;
        int x0 = min(ex, cx), x1 = max(ex, cx);
        int y0 = min(ey, cy), y1 = max(ey, cy);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                if (s.owner[y][x] != mine) {
                    gain++;
                    if (s.owner[y][x] != '.') danger++;  // 抢对手领地，风险更高
                }
            }
        int score = gain * 2 - danger;
        if (score > best) { best = score; ba = a; bb = b; bs = side; }
    }
    plan.assign(path.begin(), path.end());
    if (best < 0) return;
    for (int i = 0; i < ba - 1; i++) plan.push_back(d);
    for (int i = 0; i < bb; i++) plan.push_back(bs);
    for (int i = 0; i < ba - 1; i++) plan.push_back(d ^ 1);
}

// 求生兜底：在安全方向里选可达空间最大的一个。
int anySafe(const State& s) {
    const Player& me = s.p[ME];
    char myTrail = '0' + ME;
    int best = me.dir, bestScore = -1;
    for (int d = 0; d < 4; d++) {
        if (d == (me.dir ^ 1)) continue;
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (!inb(nx, ny) || s.trail[ny][nx] == myTrail) continue;
        if (isEnemyHead(s, nx, ny)) continue;
        int sp = reachableSpace(s, nx, ny);
        if (sp > bestScore) { bestScore = sp; best = d; }
    }
    return best;
}

int decide(const State& s) {
    const Player& me = s.p[ME];
    char mine = '0' + ME;
    bool inside = s.owner[me.y][me.x] == mine;
    if (inside && wasOutside) plan.clear();  // 刚圈完地（或复活），清空计划
    wasOutside = !inside;

    // 1) 安全切断：相邻格是对手的轨迹（且不是对手头部），直接踩上去消灭对手。
    for (int d = 0; d < 4; d++) {
        if (d == (me.dir ^ 1)) continue;
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (!inb(nx, ny)) continue;
        char c = s.trail[ny][nx];
        if (c != '.' && c != mine && !isEnemyHead(s, nx, ny)) {
            plan.clear();
            return d;
        }
    }

    // 2) 执行计划。
    if (!plan.empty()) {
        int d = plan.front();
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (d != (me.dir ^ 1) && inb(nx, ny) && s.trail[ny][nx] != mine && !isEnemyHead(s, nx, ny)) {
            plan.pop_front();
            return d;
        }
        plan.clear();
        if (!inside) {  // 计划失效且在外面：先回家
            vector<int> home = bfsPath(s, [&](int x, int y) { return s.owner[y][x] == mine; });
            if (!home.empty()) return home[0];
        }
    }

    // 3) 没有计划：在领地内就规划圈地，在外面就回家。
    if (plan.empty()) {
        if (inside) {
            makePlan(s);
            if (!plan.empty()) {
                int d = plan.front();
                int nx = me.x + DX[d], ny = me.y + DY[d];
                if (d != (me.dir ^ 1) && inb(nx, ny) && s.trail[ny][nx] != mine && !isEnemyHead(s, nx, ny)) {
                    plan.pop_front();
                    return d;
                }
                plan.clear();
            }
        } else {
            vector<int> home = bfsPath(s, [&](int x, int y) { return s.owner[y][x] == mine; });
            if (!home.empty()) return home[0];
        }
    }

    // 4) 兜底求生。
    return anySafe(s);
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string tok;
    cin >> tok;  // INIT
    cin >> W >> H >> MAX_TURNS >> N >> ME >> MOVE_MS >> INIT_MS;
    spawn.resize(N);
    for (int i = 0; i < N; i++) {
        int id, x, y;
        cin >> tok >> id >> x >> y;
        spawn[id] = {x, y};
    }
    State s;
    while (readState(s)) {
        int d = decide(s);
        cout << s.turn << ' ' << DC[d] << endl;  // endl 即 flush，每回合必须
    }
}
