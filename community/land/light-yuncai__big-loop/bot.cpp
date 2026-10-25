// 大方框 v2 —— 会算真账的圈地 bot
// 思路：枚举「折线闭合圈」，先按包围盒粗估挑前 K 个，再用泛洪精确算围住多少；
// 每一步都拿对手的 BFS 到达时间做安全检查（他能不能先踩到我的轨迹/头）。
// 参照了社区榜第 2 名「皇家圈地」的公开思路（MIT），代码是自己重写的简化版。
#include <bits/stdc++.h>
using namespace std;

static const int DX[4] = {0, 0, -1, 1};
static const int DY[4] = {-1, 1, 0, 0};
static const char DC[4] = {'U', 'D', 'L', 'R'};
static const int INF = 1e9;

int W, H, MAXT, N, ME, MOVE_MS, INIT_MS;
vector<int> own, trl, prot;      // -1 = 中立/无
struct P { int x, y, dir, tl, area, deaths; };
vector<P> pl;
int turnNo = 0;
int deMin[48 * 48];
int qbuf[48 * 48];

inline int ID(int x, int y) { return y * W + x; }
inline bool inb(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }
int dirIdx(char c) { return (int)string("UDLR").find(c); }

bool readState() {
    string tok;
    if (!(cin >> tok)) return false;
    cin >> turnNo;
    pl.assign(N, {});
    for (int i = 0; i < N; i++) {
        int id; char d;
        cin >> tok >> id >> pl[id].x >> pl[id].y >> d >> pl[id].tl >> pl[id].area >> pl[id].deaths;
        pl[id].dir = dirIdx(d);
    }
    cin >> tok;
    own.assign(W * H, -1);
    for (int y = 0; y < H; y++) {
        string row; cin >> row;
        for (int x = 0; x < W; x++) own[ID(x, y)] = row[x] == '.' ? -1 : row[x] - '0';
    }
    cin >> tok;
    trl.assign(W * H, -1);
    for (int y = 0; y < H; y++) {
        string row; cin >> row;
        for (int x = 0; x < W; x++) trl[ID(x, y)] = row[x] == '.' ? -1 : row[x] - '0';
    }
    cin >> tok;
    return true;
}

void bfsFrom(int sx, int sy, int* dist, int blockOwnTrail) {
    for (int i = 0; i < W * H; i++) dist[i] = INF;
    int qh = 0, qt = 0;
    int s = ID(sx, sy);
    dist[s] = 0;
    qbuf[qt++] = s;
    while (qh < qt) {
        int k = qbuf[qh++];
        int x = k % W, y = k / W;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!inb(nx, ny)) continue;
            int nk = ID(nx, ny);
            if (dist[nk] != INF) continue;
            if (blockOwnTrail) { /* 对手也不能踩着自己的轨迹（会死），保守起见不穿 */ }
            dist[nk] = dist[k] + 1;
            qbuf[qt++] = nk;
        }
    }
}

// ---- 模拟一段折线路径 ----
struct Sim {
    int L = 0;
    bool cap = false;
    int lastDir = 0;
    vector<int> cells;
};

static int visitStamp[48 * 48];
static int stampCtr = 0;
bool simulate(const int* dirs, const int* lens, int nl, int lim, Sim& s) {
    stampCtr++;
    int x = pl[ME].x, y = pl[ME].y, dir = pl[ME].dir;
    bool hasTrail = pl[ME].tl > 0;
    s = Sim();
    s.cells.reserve(80);
    bool first = true;
    for (int li = 0; li < nl; li++) {
        int d = dirs[li], len = lens[li];
        int cnt = 0;
        while (true) {
            if (len > 0 && cnt >= len) break;
            if (first && d == (dir ^ 1)) return false;
            first = false;
            int nx = x + DX[d], ny = y + DY[d];
            if (!inb(nx, ny)) return false;
            int k = ID(nx, ny);
            if (trl[k] == ME) return false;
            if (visitStamp[k] == stampCtr) return false;
            visitStamp[k] = stampCtr;
            x = nx; y = ny; dir = d;
            s.cells.push_back(k);
            s.L++;
            if (s.L > lim) return false;
            if (own[k] == ME) {
                if (hasTrail) { s.cap = true; s.lastDir = d; return true; }
            } else {
                hasTrail = true;
            }
            cnt++;
            if (len < 0 && s.L > 200) return false;
        }
    }
    return false;
}

bool safeToClose(const Sim& s) {
    // 我自己这条圈（含旧轨迹）任何一格，如果对手能在我闭合之前踩到，就不安全
    for (int k : s.cells) if (deMin[k] <= s.L) return false;
    for (int k = 0; k < W * H; k++)
        if (trl[k] == ME && deMin[k] <= s.L) return false;
    return true;
}

// ---- 包围盒粗估（用前缀和）----
vector<int> ps;
void buildPrefix() {
    int W1 = W + 1;
    ps.assign((W + 1) * (H + 1), 0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            ps[(y + 1) * W1 + (x + 1)] = ps[y * W1 + (x + 1)] + ps[(y + 1) * W1 + x] - ps[y * W1 + x] + (own[ID(x, y)] == ME);
}
double estGain(const Sim& s) {
    int x0 = W, x1 = -1, y0 = H, y1 = -1;
    for (int k : s.cells) { int x = k % W, y = k / W; x0 = min(x0, x); x1 = max(x1, x); y0 = min(y0, y); y1 = max(y1, y); }
    if (x1 < 0) return 0;
    int W1 = W + 1;
    int area = (x1 - x0 + 1) * (y1 - y0 + 1);
    int mine = ps[(y1 + 1) * W1 + (x1 + 1)] - ps[y0 * W1 + (x1 + 1)] - ps[(y1 + 1) * W1 + x0] + ps[y0 * W1 + x0];
    return (double)(area - mine);
}

// ---- 精确围住多少（泛洪）----
static vector<char> wall, vis;
double exactGain(const Sim& s, vector<int>* encOut = nullptr) {
    int WH = W * H;
    wall.assign(WH, 0); vis.assign(WH, 0);
    for (int k = 0; k < WH; k++) if (own[k] == ME || trl[k] == ME) wall[k] = 1;
    for (int k : s.cells) wall[k] = 1;
    int qh = 0, qt = 0;
    for (int x = 0; x < W; x++) {
        for (int y : {0, H - 1}) {
            int k = ID(x, y);
            if (!wall[k] && !vis[k]) { vis[k] = 1; qbuf[qt++] = k; }
        }
    }
    for (int y = 0; y < H; y++) {
        for (int x : {0, W - 1}) {
            int k = ID(x, y);
            if (!wall[k] && !vis[k]) { vis[k] = 1; qbuf[qt++] = k; }
        }
    }
    while (qh < qt) {
        int k = qbuf[qh++];
        int x = k % W, y = k / W;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!inb(nx, ny)) continue;
            int nk = ID(nx, ny);
            if (wall[nk] || vis[nk]) continue;
            vis[nk] = 1; qbuf[qt++] = nk;
        }
    }
    double g = 0;
    for (int k = 0; k < WH; k++) {
        if (wall[k] || vis[k]) continue;
        if (prot[k] != -1 && prot[k] != ME) continue;   // 别人的保护区永远不算
        g += (own[k] == -1) ? 1.0 : 1.6;                 // 中立格 1，对手的格更值钱
        if (encOut) encOut->push_back(k);
    }
    return g;
}

int chooseMove(int& outLen) {
    int remaining = MAXT - turnNo;
    int WH = W * H;
    // 对手到达时间
    for (int k = 0; k < WH; k++) deMin[k] = INF;
    for (int e = 0; e < N; e++) {
        if (e == ME) continue;
        static int de[48 * 48];
        bfsFrom(pl[e].x, pl[e].y, de, 0);
        for (int k = 0; k < WH; k++) deMin[k] = min(deMin[k], de[k]);
    }
    buildPrefix();

    struct Cand { int dirs[3], lens[3], nl; double est; Sim s; };
    vector<Cand> cands;
    cands.reserve(6000);
    int A = min(26, remaining);
    int B = min(18, remaining);
    for (int d1 = 0; d1 < 4; d1++) {
        if (d1 == (pl[ME].dir ^ 1)) continue;
        for (int a = 1; a <= A; a++) {
            for (int side = 0; side < 2; side++) {
                int sd = (d1 < 2 ? 2 : 0) + side;
                {
                    Cand c; c.nl = 2; c.dirs[0] = d1; c.lens[0] = a; c.dirs[1] = sd; c.lens[1] = -1;
                    if (simulate(c.dirs, c.lens, c.nl, remaining, c.s)) {
                        if (c.s.cap && safeToClose(c.s)) { c.est = estGain(c.s) / (c.s.L + 20.0); cands.push_back(c); }
                    }
                }
                for (int b = 1; b <= B; b++) {
                    Cand c; c.nl = 3; c.dirs[0] = d1; c.lens[0] = a;
                    c.dirs[1] = sd; c.lens[1] = b; c.dirs[2] = d1 ^ 1; c.lens[2] = -1;
                    if (simulate(c.dirs, c.lens, c.nl, remaining, c.s)) {
                        if (c.s.cap && safeToClose(c.s)) { c.est = estGain(c.s) / (c.s.L + 20.0); cands.push_back(c); }
                    }
                }
            }
        }
    }
    double bestRate = 0; int bestDir = -1; int bestL = 0;
    if (!cands.empty()) {
        int K = min<int>(300, cands.size());
        partial_sort(cands.begin(), cands.begin() + K, cands.end(),
                     [](const Cand& x, const Cand& y) { return x.est > y.est; });
        for (int i = 0; i < K; i++) {
            double g = exactGain(cands[i].s);
            double rate = g / (cands[i].s.L + 20.0);
            if (rate > bestRate) { bestRate = rate; bestDir = cands[i].dirs[0]; bestL = cands[i].s.L; }
        }
    }
    if (bestDir >= 0) { outLen = bestL; return bestDir; }

    // 回退 1：留在自己领地里绕
    {
        int best = -1, bestScore = -1;
        for (int d = 0; d < 4; d++) {
            if (d == (pl[ME].dir ^ 1)) continue;
            int nx = pl[ME].x + DX[d], ny = pl[ME].y + DY[d];
            if (!inb(nx, ny)) continue;
            int k = ID(nx, ny);
            if (trl[k] == ME || deMin[k] <= 1) continue;
            int sc = (own[k] == ME) ? 2 : 0;
            if (sc > bestScore) { bestScore = sc; best = d; }
        }
        if (best >= 0) { outLen = 1; return best; }
    }
    // 回退 2：回家（朝最近的自有领地走）
    {
        static int hd[48 * 48];
        for (int k = 0; k < WH; k++) hd[k] = INF;
        int qh = 0, qt = 0;
        for (int k = 0; k < WH; k++) if (own[k] == ME) { hd[k] = 0; qbuf[qt++] = k; }
        while (qh < qt) {
            int k = qbuf[qh++]; int x = k % W, y = k / W;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inb(nx, ny)) continue;
                int nk = ID(nx, ny);
                if (trl[nk] == ME || hd[nk] != INF) continue;
                hd[nk] = hd[k] + 1; qbuf[qt++] = nk;
            }
        }
        int bd = -1, bv = INF;
        for (int d = 0; d < 4; d++) {
            if (d == (pl[ME].dir ^ 1)) continue;
            int nx = pl[ME].x + DX[d], ny = pl[ME].y + DY[d];
            if (!inb(nx, ny)) continue;
            int k = ID(nx, ny);
            if (trl[k] == ME) continue;
            if (hd[k] < bv) { bv = hd[k]; bd = d; }
        }
        if (bd >= 0) { outLen = 1; return bd; }
    }
    for (int d = 0; d < 4; d++) {
        if (d == (pl[ME].dir ^ 1)) continue;
        int nx = pl[ME].x + DX[d], ny = pl[ME].y + DY[d];
        if (inb(nx, ny)) { outLen = 1; return d; }
    }
    outLen = 1;
    return pl[ME].dir;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string tok;
    cin >> tok;
    cin >> W >> H >> MAXT >> N >> ME >> MOVE_MS >> INIT_MS;
    vector<pair<int, int>> spawn(N);
    for (int i = 0; i < N; i++) { int id, x, y; cin >> tok >> id >> x >> y; spawn[id] = {x, y}; }
    prot.assign(W * H, -1);
    for (int i = 0; i < N; i++)
        for (int y = spawn[i].second - 1; y <= spawn[i].second + 1; y++)
            for (int x = spawn[i].first - 1; x <= spawn[i].first + 1; x++)
                if (inb(x, y)) prot[ID(x, y)] = i;
    while (readState()) {
        int len = 1;
        int d = chooseMove(len);
        cout << turnNo << ' ' << DC[d] << endl;
    }
    return 0;
}

