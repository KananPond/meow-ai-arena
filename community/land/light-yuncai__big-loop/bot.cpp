// 圈地 bot —— 大方框
//
// 得分是"围出来的格子数 ÷ 花掉的回合数"。greedy 基准围 3~8 格的小方块，
// 那就反过来：每趟围一个尽可能大的矩形，让 面积/步数 最大。
//
// 从规则里读出来的三件事：
// 1. 踩到**别人**的轨迹不杀我，杀他自己（规则 2.4）；自己的轨迹才致命。
// 2. 阵亡不清领地，只是回出生点 —— 冒险的代价是回合，不是财产。
// 3. 圈地时被围住的区域连对手的领地一起改归属（保护区除外）。
#include <bits/stdc++.h>
using namespace std;

const int DX[4] = {0, 0, -1, 1};
const int DY[4] = {-1, 1, 0, 0};
const char DC[4] = {'U', 'D', 'L', 'R'};

static int dirIndex(char c) { return (int)string("UDLR").find(c); }

struct Player {
    int x = 0, y = 0, dir = 0, trailLen = 0, area = 0, deaths = 0;
};

struct State {
    int turn = 0;
    vector<Player> p;
    vector<string> owner;
    vector<string> trail;
};

static int W, H, MAX_TURNS, N, ME, MOVE_MS, INIT_MS;
static vector<pair<int, int>> spawn;
static deque<int> plan;
static bool wasOutside = false;

static bool readState(State& s) {
    string tok;
    if (!(cin >> tok)) return false;
    cin >> s.turn;
    s.p.assign(N, {});
    for (int i = 0; i < N; i++) {
        int id;
        char d;
        Player q;
        cin >> tok >> id >> q.x >> q.y >> d >> q.trailLen >> q.area >> q.deaths;
        q.dir = dirIndex(d);
        s.p[id] = q;
    }
    cin >> tok;
    s.owner.assign(H, "");
    for (auto& row : s.owner) cin >> row;
    cin >> tok;
    s.trail.assign(H, "");
    for (auto& row : s.trail) cin >> row;
    cin >> tok;
    return true;
}

static inline bool inb(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }

template <class F>
static vector<int> bfsPath(const State& s, F goal) {
    const Player& me = s.p[ME];
    char myTrail = (char)('0' + ME);
    vector<int> from(W * H, -1), fdir(W * H, -1);
    deque<int> q;
    int start = me.y * W + me.x;
    from[start] = start;
    for (int d = 0; d < 4; d++) {
        if (d == (me.dir ^ 1)) continue;
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (!inb(nx, ny) || s.trail[ny][nx] == myTrail) continue;
        int k = ny * W + nx;
        if (from[k] != -1) continue;
        from[k] = start;
        fdir[k] = d;
        q.push_back(k);
    }
    while (!q.empty()) {
        int k = q.front();
        q.pop_front();
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
            int nk = ny * W + nx;
            if (from[nk] != -1) continue;
            from[nk] = k;
            fdir[nk] = d;
            q.push_back(nk);
        }
    }
    return {};
}

static void makeBigLoop(const State& s) {
    const Player& me = s.p[ME];
    char mine = (char)('0' + ME);

    vector<vector<int>> un(W + 1, vector<int>(H + 1, 0));
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            un[x + 1][y + 1] = un[x][y + 1] + un[x + 1][y] - un[x][y] + (s.owner[y][x] != mine ? 1 : 0);
    auto countUn = [&](int x0, int y0, int x1, int y1) {
        if (x0 > x1) swap(x0, x1);
        if (y0 > y1) swap(y0, y1);
        x1 = min(x1, W - 1);
        y1 = min(y1, H - 1);
        x0 = max(x0, 0);
        y0 = max(y0, 0);
        return un[x1 + 1][y1 + 1] - un[x0][y1 + 1] - un[x1 + 1][y0] + un[x0][y0];
    };

    // 混战里三家的轨迹横着走，大圈太容易被人一刀切断；圈小一点、兑现快一点。
    int maxA = (N >= 3) ? 11 : W;
    int maxB = (N >= 3) ? 11 : H;
    double bestScore = 0.0;
    deque<int> bestPlan;
    int px = me.x, py = me.y;

    for (int d = 0; d < 4; d++) {
        if (d == (me.dir ^ 1)) continue;
        for (int sideSign = -1; sideSign <= 1; sideSign += 2) {
            int s;
            if (d < 2) s = (sideSign > 0 ? 3 : 2);
            else s = (sideSign > 0 ? 1 : 0);
            for (int A = 3; A <= maxA; A++) {
                int fxp = px + DX[d] * A, fyp = py + DY[d] * A;
                if (!inb(fxp, fyp)) break;
                for (int B = 3; B <= maxB; B++) {
                    int cx = fxp + DX[s] * B, cy = fyp + DY[s] * B;
                    if (!inb(cx, cy)) break;
                    int gain = countUn(px, py, cx, cy);
                    int steps = 2 * (A + B);
                    double score = (double)gain / (double)steps;
                    if (score > bestScore) {
                        bestScore = score;
                        bestPlan.clear();
                        for (int i = 0; i < A; i++) bestPlan.push_back(d);
                        for (int i = 0; i < B; i++) bestPlan.push_back(s);
                        for (int i = 0; i < A; i++) bestPlan.push_back(d ^ 1);
                        for (int i = 0; i < B; i++) bestPlan.push_back(s ^ 1);
                    }
                }
            }
        }
    }
    plan = bestPlan;
}

static int anySafe(const State& s) {
    const Player& me = s.p[ME];
    char myTrail = (char)('0' + ME);
    for (int d : {me.dir, 0, 1, 2, 3}) {
        if (d == (me.dir ^ 1)) continue;
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (inb(nx, ny) && s.trail[ny][nx] != myTrail) return d;
    }
    return me.dir;
}

static int decide(const State& s) {
    const Player& me = s.p[ME];
    char mine = (char)('0' + ME);
    bool inside = s.owner[me.y][me.x] == mine;

    if (inside && wasOutside) plan.clear();
    wasOutside = !inside;

    for (int d = 0; d < 4; d++) {
        if (d == (me.dir ^ 1)) continue;
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (!inb(nx, ny)) continue;
        char t = s.trail[ny][nx];
        if (t != '.' && t != mine) {
            plan.clear();
            return d;
        }
    }

    if (plan.empty() && inside) makeBigLoop(s);

    while (!plan.empty()) {
        int d = plan.front();
        int nx = me.x + DX[d], ny = me.y + DY[d];
        if (d != (me.dir ^ 1) && inb(nx, ny) && s.trail[ny][nx] != mine) {
            plan.pop_front();
            return d;
        }
        plan.clear();
        break;
    }

    if (!inside) {
        vector<int> home = bfsPath(s, [&](int x, int y) { return s.owner[y][x] == mine; });
        if (!home.empty()) return home[0];
    }
    return anySafe(s);
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string tok;
    cin >> tok;
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
        cout << s.turn << ' ' << DC[d] << endl;
    }
    return 0;
}
