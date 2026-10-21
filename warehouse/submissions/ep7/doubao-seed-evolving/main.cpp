// WMS 仓储机器人调度仿真 —— 性能重写版（行为与 baseline/ 逐字节一致）
//
// 文件结构：
//   1. 输入解析（整体令牌化；事件按 tick 分桶，不再每 tick 扫全部事件）
//   2. 寻路：
//        - multiTargetDists：单源 -> 一组目标格的精确距离
//          单目标用曼哈顿启发的 A*（四邻接网格上 h 是下界，距离值与 BFS 相同），
//          多目标用“找齐目标即停”的 BFS；扁平数组 + 版本戳，零反复分配。
//        - nearestGoal：单源 BFS，按层提前结束，返回最近目标格
//          （同层多个目标时取行优先最靠前的），用于低电量选桩。
//        - ChargerField：所有“当前可通行充电桩”的多源 BFS（每 tick 惰性建一次），
//          给出每格到最近充电桩的距离，以及行优先平局选中的充电桩。
//   3. 每 tick：事件 → 救援 → 充电 → 派单 → 低电量去充电 → 移动 → 报表
//   4. 收尾统计
//
// 所有“看起来像 bug 但会影响输出”的基线怪行为都用 [BASELINE] 标注，
// 详见同目录 HANDOFF.md。
#include <bits/stdc++.h>
using namespace std;

namespace {

const int INF = 1000000000;

// ---------- 全局状态 ----------
int W, H, T, N, NC;                 // 地图宽高、tick 数、机器人数、格数
vector<string> MAP;                 // MAP[y][x]
vector<char> passable;              // 扁平化：当前是否可通行（非墙且未封锁）
vector<int> chargers;               // 所有充电桩格（flatten），行优先顺序
vector<char> isCharger;
vector<int> chgIndex;               // 格 -> chargers 下标（-1）
long long PRM[6];                   // 满电/充电速度/低电量阈值/安全余量/报表间隔/热点半径

enum State : unsigned char { IDLE, TO_PICKUP, DELIVERING, TO_CHARGER, CHARGING, DEAD };
const char* const STATE_NAME[6] =
    {"IDLE", "TO_PICKUP", "DELIVERING", "TO_CHARGER", "CHARGING", "DEAD"};

struct Robot {
    int x, y;            // 位置
    int cell;
    long long bat;       // 电量
    int tx, ty;          // 目标格（无目标时 -1）
    int waitRun;         // RWS：连续等待（让路阈值用）
    int deadTick;        // RDS：停机时刻
    long long waitTotal; // RWT：累计等待
    long long steps;     // RTR：移动步数
    State st;
    int order;           // 负责的订单下标，-1 无
};
vector<Robot> robots;

struct Order {
    long long id;        // 原始订单号（按十进制整数比较/排序）
    int px, py, dx, dy;
    int prio;
    int arrival;
    // 0 PENDING 1 ASSIGNED 2 PICKED 3 DONE 4 CANCELLED 5 LOST
    unsigned char status;
    int robot;
};
vector<Order> orders;
unordered_map<long long, int> orderById;
vector<int> pend;                  // PENDING 订单下标（保持到达顺序）

struct Event {
    // 0 ORDER 1 CANCEL 2 BLOCK 3 UNBLOCK
    unsigned char kind;
    long long id;
    int a, b, c, d, prio;
};
vector<vector<Event>> eventsByTick;

// ---------- 输出与统计 ----------
struct Counts {
    long long delivered = 0, lost = 0, rejected = 0, cancelled = 0;
    long long latSum = 0, latMax = 0;
} cnt;

string outBuf;
void writeLog(int t, string_view msg) {
    outBuf += to_string(t);
    outBuf.push_back(' ');
    outBuf.append(msg.data(), msg.size());
    outBuf.push_back('\n');
}
void writeRaw(string_view msg) {
    outBuf.append(msg.data(), msg.size());
    outBuf.push_back('\n');
}

// [BASELINE] 基线在 writeLog 里靠 SPLIT 消息首词统计 DELIVER/LOST/REJECT；
// CANCEL 的计数则在事件处理处“只要订单存在就 +1”（取消失败也算）。
void logDeliver(int t, long long id, int r, int lat) {
    writeLog(t, "DELIVER " + to_string(id) + " R" + to_string(r) + " " + to_string(lat));
    cnt.delivered++;
    cnt.latSum += lat;
    if (lat > cnt.latMax) cnt.latMax = lat;
}
void logLost(int t, long long id)       { writeLog(t, "LOST " + to_string(id)); cnt.lost++; }
void logReject(int t, long long id)     { writeLog(t, "REJECT " + to_string(id)); cnt.rejected++; }
void logCancel(int t, long long id)     { writeLog(t, "CANCEL " + to_string(id)); }
void logCancelFail(int t, long long id) { writeLog(t, "CANCEL_FAIL " + to_string(id)); }
void logRescue(int t, int r)            { writeLog(t, "RESCUE R" + to_string(r)); }
void logCharged(int t, int r)           { writeLog(t, "CHARGED R" + to_string(r)); }
void logAssign(int t, long long id, int r, int a) {
    writeLog(t, "ASSIGN " + to_string(id) + " R" + to_string(r) + " " + to_string(a));
}
void logGoCharge(int t, int r, int x, int y) {
    writeLog(t, "GO_CHARGE R" + to_string(r) + " " + to_string(x) + " " + to_string(y));
}
void logPick(int t, long long id, int r)   { writeLog(t, "PICK " + to_string(id) + " R" + to_string(r)); }
void logCharge(int t, int r)               { writeLog(t, "CHARGE R" + to_string(r)); }
void logDead(int t, int r)                 { writeLog(t, "DEAD R" + to_string(r)); }
void logRequeue(int t, long long id)       { writeLog(t, "REQUEUE " + to_string(id)); }
void logSidestep(int t, int r)             { writeLog(t, "SIDESTEP R" + to_string(r)); }

// ---------- 网格工具 ----------
inline bool inMap(int x, int y) { return x >= 0 && y >= 0 && x < W && y < H; }
inline int cellOf(int x, int y) { return y * W + x; }

// ---------- 输入解析（空白分隔的 ASCII 文本） ----------
struct Token { const char* p; int len;
    long long asInt() const {
        long long v = 0, sign = 1; int i = 0;
        if (p[0] == '-') { sign = -1; i = 1; }
        for (; i < len; i++) v = v * 10 + (p[i] - '0');
        return v * sign;
    }
    string_view sv() const { return string_view(p, len); }
};
vector<Token> tokens;

void readAll(const string& input) {
    const char* p = input.data();
    size_t i = 0, n = input.size();
    while (i < n) {
        while (i < n && (unsigned char)p[i] <= ' ') i++;
        if (i >= n) break;
        size_t j = i;
        while (j < n && (unsigned char)p[j] > ' ') j++;
        tokens.push_back({p + i, (int)(j - i)});
        i = j;
    }
    size_t k = 0;
    W = (int)tokens[k++].asInt();
    H = (int)tokens[k++].asInt();
    T = (int)tokens[k++].asInt();
    MAP.resize(H);
    for (int y = 0; y < H; y++) MAP[y] = string(tokens[k++].sv());
    k++; // "PARAMS"
    for (int i = 0; i < 6; i++) PRM[i] = tokens[k++].asInt();
    k++; // "ROBOTS"
    N = (int)tokens[k++].asInt();
    robots.resize(N);
    for (int i = 0; i < N; i++) {
        int x = (int)tokens[k++].asInt();
        int y = (int)tokens[k++].asInt();
        robots[i] = Robot{x, y, cellOf(x, y), PRM[0], -1, -1, 0, -1, 0, 0, IDLE, -1};
    }
    k++; // "EVENTS"
    int E = (int)tokens[k++].asInt();
    eventsByTick.assign(T, {});
    for (int e = 0; e < E; e++) {
        int tick = (int)tokens[k++].asInt();
        string_view typ = tokens[k++].sv();
        Event ev{};
        if (typ == "ORDER") {
            ev.kind = 0;
            ev.id = tokens[k++].asInt();
            ev.a = (int)tokens[k++].asInt();   // px
            ev.b = (int)tokens[k++].asInt();   // py
            ev.c = (int)tokens[k++].asInt();   // dx
            ev.d = (int)tokens[k++].asInt();   // dy
            ev.prio = (int)tokens[k++].asInt();
        } else if (typ == "CANCEL") {
            ev.kind = 1;
            ev.id = tokens[k++].asInt();
        } else if (typ == "BLOCK") {
            ev.kind = 2;
            ev.a = (int)tokens[k++].asInt();
            ev.b = (int)tokens[k++].asInt();
        } else { // UNBLOCK（合法输入不会有别的类型）
            ev.kind = 3;
            ev.a = (int)tokens[k++].asInt();
            ev.b = (int)tokens[k++].asInt();
        }
        eventsByTick[tick].push_back(ev);
    }
}

// =====================================================================
// 寻路。方向顺序与基线一致：上 右 下 左（所有平局都靠它）
// =====================================================================
const int DX[4] = {0, 1, 0, -1};
const int DY[4] = {-1, 0, 1, 0};

struct Search {
    vector<int> dist, stamp, q;
    int curStamp = 0;
    // 目标集合（版本戳）：gpos[cell]==gstamp 时 cell 是目标，gidx 是其序号
    vector<int> gpos, gidx, gstampArr;
    int gStamp = 0;
    explicit Search(int n)
        : dist(n), stamp(n, 0), q(n), gpos(n, 0), gidx(n, -1), gstampArr(n, 0) {}

    void beginGoals() { ++gStamp; }
    // 目标格必须互不相同（调用处满足：候选机器人位置互异、充电桩格互异）
    void addGoal(int cell, int idx) {
        gpos[cell] = gStamp;
        gidx[cell] = idx;
    }
    bool isGoal(int cell) const { return gpos[cell] == gStamp; }

    void beginDist(int src) {
        ++curStamp;
        dist[src] = 0;
        stamp[src] = curStamp;
    }
    bool seen(int c) const { return stamp[c] == curStamp; }
};

// src 到 targets 中每个格的距离，写入 out（同序，不可达 INF）。
//   * target == src -> 0（基线：起点无论是否可通行，自距离都是 0）；
//   * target 不可通行且 != src -> 不可达（BFS 永远不会走到它）；
//   * src 允许不可通行：只从 src 起步，之后只走可通行格。
// 单目标走 A*（h=曼哈顿，四邻接网格上的可采纳启发式）；多目标走找齐即停 BFS。
void multiTargetDists(Search& srch, int src, const vector<int>& targets,
                      vector<int>& out) {
    fill(out.begin(), out.end(), INF);
    if (targets.empty()) return;

    srch.beginGoals();
    int nGoal = 0;
    int selfGoal = -1;
    for (size_t i = 0; i < targets.size(); i++) {
        int g = targets[i];
        if (g == src) { out[i] = 0; selfGoal = (int)i; continue; }
        if (!passable[g]) continue;
        srch.addGoal(g, nGoal++);
    }
    if (nGoal == 0) return;
    (void)selfGoal;

    srch.beginDist(src);
    const int sx = src % W, sy = src / W;

    if (nGoal == 1) {
        // ---------- A* ----------
        int gcell = -1;
        for (size_t i = 0; i < targets.size(); i++)
            if (targets[i] != src && passable[targets[i]]) { gcell = targets[i]; break; }
        const int gx = gcell % W, gy = gcell / W;
        struct Node { int f, g, c; };
        // 堆序：f 小先；f 同则 g 大先；再按格行优先。结果只要求距离值正确，
        // 平局次序只影响性能不影响答案。
        auto cmp = [](const Node& a, const Node& b) {
            if (a.f != b.f) return a.f > b.f;
            if (a.g != b.g) return a.g < b.g;
            return a.c > b.c;
        };
        vector<Node> heap;
        heap.reserve(256);
        heap.push_back({abs(sx - gx) + abs(sy - gy), 0, src});
        while (!heap.empty()) {
            pop_heap(heap.begin(), heap.end(), cmp);
            Node nd = heap.back();
            heap.pop_back();
            if (srch.seen(nd.c) && srch.dist[nd.c] < nd.g) continue;
            if (nd.c == gcell) {
                for (size_t i = 0; i < targets.size(); i++)
                    if (targets[i] == gcell) out[i] = nd.g;
                return;
            }
            int x = nd.c % W, y = nd.c / W;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inMap(nx, ny)) continue;
                int nc = ny * W + nx;
                if (!passable[nc]) continue;
                int ng = nd.g + 1;
                if (srch.seen(nc) && srch.dist[nc] <= ng) continue;
                srch.dist[nc] = ng;
                srch.stamp[nc] = srch.curStamp;
                heap.push_back({ng + abs(nx - gx) + abs(ny - gy), ng, nc});
                push_heap(heap.begin(), heap.end(), cmp);
            }
        }
        return;
    }

    // ---------- 多目标 BFS：所有目标都找到即停 ----------
    int found = 0;
    int qh = 0, qt = 0;
    srch.q[qt++] = src;
    while (qh < qt) {
        int c = srch.q[qh++];
        if (c != src && srch.isGoal(c)) {
            int gi = srch.gidx[c];
            // 目标格互异；把该格对应的 targets 项填上（通常只有一项）
            for (size_t i = 0; i < targets.size(); i++)
                if (targets[i] == c) out[i] = srch.dist[c];
            (void)gi;
            if (++found >= nGoal) return;
        }
        int x = c % W, y = c / W;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!inMap(nx, ny)) continue;
            int nc = ny * W + nx;
            if (!passable[nc] || srch.seen(nc)) continue;
            srch.stamp[nc] = srch.curStamp;
            srch.dist[nc] = srch.dist[c] + 1;
            srch.q[qt++] = nc;
        }
    }
}

// 单源 BFS，返回最近目标格；同一距离有多个目标时取 flatten 最小（行优先）。
// src 本身是目标时返回 src。无目标可达返回 -1。
// 按层推进：一整层都展开完再在该层目标里取最小，保证平局严格行优先。
int nearestGoal(Search& srch, int src) {
    if (srch.isGoal(src)) return src;
    srch.beginDist(src);
    int qh = 0, qt = 0, layerEnd = 0;
    srch.q[qt++] = src;
    layerEnd = qt;
    while (qh < qt) {
        int best = -1;
        // 处理当前层（[qh, layerEnd)），新节点追加到队尾（下一层）
        while (qh < layerEnd) {
            int c = srch.q[qh++];
            int x = c % W, y = c / W;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inMap(nx, ny)) continue;
                int nc = ny * W + nx;
                if (!passable[nc] || srch.seen(nc)) continue;
                srch.stamp[nc] = srch.curStamp;
                srch.dist[nc] = srch.dist[c] + 1;
                srch.q[qt++] = nc;
                if (srch.isGoal(nc) && (best < 0 || nc < best)) best = nc;
            }
        }
        if (best >= 0) return best;
        layerEnd = qt;
    }
    return -1;
}

// 充电桩多源 BFS：每 tick 惰性建一次。
//   dist[c]  = c 到最近“可通行充电桩”的步数（不可达用 stamp 判断）
//   which[c] = 平局时选中的充电桩在 chargers 中的下标（行优先）
// 关键：不能用“谁先入队谁赢”——同距离层里小编号源的波可能更晚到。
// 做法是逐层推进：上一层的标签已最终确定，本层新格的标签取所有同层父节点
// 标签的最小值。每格只入队一次，总复杂度 O(W*H)。
struct ChargerField {
    vector<int> dist, which, stamp, q;
    int curStamp = 0, builtTick = -1;
    explicit ChargerField(int n) : dist(n), which(n), stamp(n, 0), q(n) {}
};
ChargerField* cf = nullptr;

void buildChargerField(int tick) {
    if (cf->builtTick == tick) return;
    cf->builtTick = tick;
    ++cf->curStamp;
    int qh = 0, qt = 0;
    for (int i = 0; i < (int)chargers.size(); i++) {
        int c = chargers[i];
        if (!passable[c]) continue;
        if (cf->stamp[c] != cf->curStamp) {
            cf->stamp[c] = cf->curStamp;
            cf->dist[c] = 0;
            cf->which[c] = i;
            cf->q[qt++] = c;
        }
    }
    int layerEnd = qt;
    while (qh < qt) {
        while (qh < layerEnd) {
            int c = cf->q[qh++];
            int x = c % W, y = c / W;
            int gv = cf->dist[c], wi = cf->which[c];
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inMap(nx, ny)) continue;
                int nc = ny * W + nx;
                if (!passable[nc]) continue;
                if (cf->stamp[nc] != cf->curStamp) {
                    cf->stamp[nc] = cf->curStamp;
                    cf->dist[nc] = gv + 1;
                    cf->which[nc] = wi;
                    cf->q[qt++] = nc;
                } else if (cf->dist[nc] == gv + 1 && wi < cf->which[nc]) {
                    cf->which[nc] = wi;   // 同层父节点标签取最小
                }
            }
        }
        layerEnd = qt;
    }
}

inline bool fieldReachable(int c) { return cf->stamp[c] == cf->curStamp; }

// ---------- 占据（救援/让路用，O(N)） ----------
bool occupiedByOther(int x, int y, int except) {
    for (int i = 0; i < N; i++) {
        if (i == except || robots[i].st == DEAD) continue;
        if (robots[i].x == x && robots[i].y == y) return true;
    }
    return false;
}

Search* srch = nullptr;

// ---------- 到达处理（开始时已在目标 / 移动后到达，共用） ----------
void arrive(int t, int r) {
    Robot& rb = robots[r];
    rb.waitRun = 0;
    int oidx = rb.order;
    if (rb.st == TO_PICKUP) {
        Order& o = orders[oidx];
        o.status = 2; // PICKED
        rb.st = DELIVERING;
        rb.tx = o.dx;
        rb.ty = o.dy;
        logPick(t, o.id, r);
    } else if (rb.st == DELIVERING) {
        Order& o = orders[oidx];
        o.status = 3; // DONE
        int lat = t - o.arrival;
        logDeliver(t, o.id, r, lat);
        rb.st = IDLE;
        rb.order = -1;
        rb.tx = rb.ty = -1;
    } else { // TO_CHARGER
        rb.st = CHARGING;
        logCharge(t, r);
    }
}

// ---------- 停机（3.8） ----------
void die(int t, int r) {
    Robot& rb = robots[r];
    logDead(t, r);
    if (rb.order >= 0) {
        Order& o = orders[rb.order];
        if (rb.st == TO_PICKUP) {
            o.status = 0; // PENDING（保留到达 tick）
            o.robot = -1;
            pend.push_back(rb.order);
            logRequeue(t, o.id);
        } else if (rb.st == DELIVERING) {
            o.status = 5; // LOST
            logLost(t, o.id);
        }
    }
    rb.st = DEAD;
    rb.deadTick = t;
    rb.order = -1;
    rb.tx = rb.ty = -1;
}

// =====================================================================
// 主循环
// =====================================================================
void runSim() {
    NC = W * H;
    passable.assign(NC, 0);
    isCharger.assign(NC, 0);
    chgIndex.assign(NC, -1);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int c = cellOf(x, y);
            passable[c] = (MAP[y][x] != '#');
            if (MAP[y][x] == 'C') {
                chgIndex[c] = (int)chargers.size();
                chargers.push_back(c);
                isCharger[c] = 1;
            }
        }
    srch = new Search(NC);
    cf = new ChargerField(NC);

    vector<int> pendSorted;
    vector<int> cand, targets, dists;
    vector<int> oneT(1), oneD(1);
    vector<char> chUsed(chargers.size(), 0);

    // 报表用的旋转坐标差分表（u=y+x, v=y-x+W）
    int U = W + H, V = W + H;
    vector<int> hotDiff((U + 1) * (V + 1), 0);

    for (int t = 0; t < T; t++) {
        // ====================== 1. 事件 ======================
        for (const Event& ev : eventsByTick[t]) {
            if (ev.kind == 0) { // ORDER
                int px = ev.a, py = ev.b, dx = ev.c, dy = ev.d;
                bool good = inMap(px, py) && inMap(dx, dy) &&
                            MAP[py][px] != '#' && MAP[dy][dx] != '#';
                if (!good) { logReject(t, ev.id); continue; }
                int idx = (int)orders.size();
                orders.push_back({ev.id, px, py, dx, dy, ev.prio, t, 0, -1});
                orderById.emplace(ev.id, idx);
                pend.push_back(idx);
            } else if (ev.kind == 1) { // CANCEL
                auto it = orderById.find(ev.id);
                if (it == orderById.end()) { logCancelFail(t, ev.id); continue; }
                cnt.cancelled++;   // [BASELINE] 订单存在就计数，取消失败也算
                Order& o = orders[it->second];
                if (o.status == 0) { // PENDING
                    pend.erase(find(pend.begin(), pend.end(), it->second));
                    o.status = 4;
                    o.robot = -1;
                    logCancel(t, ev.id);
                } else if (o.status == 1) { // ASSIGNED
                    int ri = o.robot;
                    robots[ri].st = IDLE;
                    robots[ri].order = -1;
                    robots[ri].tx = robots[ri].ty = -1;
                    robots[ri].waitRun = 0;
                    o.status = 4;
                    o.robot = -1;
                    logCancel(t, ev.id);
                } else {
                    logCancelFail(t, ev.id);
                }
            } else if (ev.kind == 2) { // BLOCK
                int x = ev.a, y = ev.b;
                if (inMap(x, y) && MAP[y][x] != '#') passable[cellOf(x, y)] = 0;
            } else { // UNBLOCK
                int x = ev.a, y = ev.b;
                if (inMap(x, y) && MAP[y][x] != '#') passable[cellOf(x, y)] = 1;
            }
        }

        // ====================== 2. 救援 ======================
        for (int i = 0; i < N; i++) {
            Robot& rb = robots[i];
            if (rb.st != DEAD) continue;
            if (t - rb.deadTick >= 100 && !occupiedByOther(rb.x, rb.y, i)) {
                rb.st = IDLE;
                rb.bat = PRM[0] / 2;       // [BASELINE] SPEC 说充满，实际只充一半
                rb.waitRun = 0;
                rb.deadTick = -1;
                logRescue(t, i);
            }
        }

        // ====================== 3. 充电 ======================
        for (int i = 0; i < N; i++) {
            Robot& rb = robots[i];
            if (rb.st != CHARGING) continue;
            rb.bat = min(PRM[0], rb.bat + PRM[1]);
            // [BASELINE] SPEC 说满电才走，实际到 90% 就走
            if (rb.bat * 10 >= PRM[0] * 9) {
                rb.st = IDLE;
                rb.tx = rb.ty = -1;
                logCharged(t, i);
            }
        }

        // ====================== 4. 派单 ======================
        // 基线在“排序后的副本”上遍历，每派掉一单就从“原 PEND”里删；
        // 所以未派出订单保留 PEND 原始（到达）顺序。这里同样只对副本排序。
        if (!pend.empty()) {
            pendSorted = pend;
            sort(pendSorted.begin(), pendSorted.end(), [&](int a, int b) {
                const Order& oa = orders[a];
                const Order& ob = orders[b];
                if (oa.prio != ob.prio) return oa.prio > ob.prio;
                if (oa.arrival != ob.arrival) return oa.arrival < ob.arrival;
                return oa.id < ob.id;
            });
            for (int oidx : pendSorted) {
                if (orders[oidx].status != 0) continue;  // 防御
                Order& o = orders[oidx];
                int pcell = cellOf(o.px, o.py), dcell = cellOf(o.dx, o.dy);

                cand.clear();
                for (int i = 0; i < N; i++)
                    if (robots[i].st == IDLE && robots[i].bat >= PRM[2]) cand.push_back(i);
                if (cand.empty()) break;   // 后续订单也不会有候选

                // b = dist(pickup -> delivery)
                int b = INF;
                if (pcell == dcell) b = 0;
                else if (passable[dcell]) {
                    oneT[0] = dcell;
                    multiTargetDists(*srch, pcell, oneT, oneD);
                    b = oneD[0];
                }
                // c = dist(delivery -> 最近可通行充电桩)
                // 基线 CHG_DIST 是“从送货点出发”的单源 BFS；无向图上这与
                // 多源充电桩场在该格取值等价（送货点不可通行时亦然：
                // 两边都是把该点当距离 0 的起点、之后只走可通行格）。
                int cdist = INF;
                if (b < INF) {
                    buildChargerField(t);
                    if (fieldReachable(dcell)) cdist = cf->dist[dcell];
                }
                if (b >= INF || cdist >= INF) continue;

                // a = dist(机器人 -> pickup)：无向图上等价于从 pickup 反向搜。
                // 机器人可能站在当 tick 被封的格上——反向 BFS 只走可通行格，
                // 正好等价于基线“从被封源点起步”的 BFS。
                targets.clear();
                for (int r : cand) targets.push_back(robots[r].cell);
                dists.assign(targets.size(), INF);
                multiTargetDists(*srch, pcell, targets, dists);

                // [BASELINE] 基线写的是 `if (a <= best) who = i`，按编号升序遍历，
                // 同距时留下编号“最大”的合格者（SPEC 写最小，以代码为准）。
                int bestA = INF, who = -1;
                for (size_t ci = 0; ci < cand.size(); ci++) {
                    int r = cand[ci], a = dists[ci];
                    if (a >= INF) continue;
                    if (robots[r].bat < (long long)a + b + cdist + PRM[3]) continue;
                    if (a <= bestA) { bestA = a; who = r; }
                }
                if (who < 0) continue;

                Robot& rr = robots[who];
                rr.st = TO_PICKUP;
                rr.order = oidx;
                rr.tx = o.px; rr.ty = o.py;
                rr.waitRun = 0;
                o.status = 1;
                o.robot = who;
                logAssign(t, o.id, who, bestA);
                pend.erase(find(pend.begin(), pend.end(), oidx));
            }
        }

        // ====================== 5. 低电量去充电 ======================
        for (int i = 0; i < N; i++) {
            Robot& rb = robots[i];
            if (rb.st != IDLE || rb.bat >= PRM[2]) continue;

            // 被“其他”机器人占用的充电桩：CHARGING 在其上 / TO_CHARGER 目标是它
            for (char& v : chUsed) v = 0;
            for (int j = 0; j < N; j++) {
                if (j == i) continue;
                const Robot& rj = robots[j];
                if (rj.st == CHARGING && chgIndex[rj.cell] >= 0)
                    chUsed[chgIndex[rj.cell]] = 1;
                if (rj.st == TO_CHARGER && rj.tx >= 0) {
                    int ci = chgIndex[cellOf(rj.tx, rj.ty)];
                    if (ci >= 0) chUsed[ci] = 1;
                }
            }

            // 第 1 步：在未被占用的可通行充电桩中找最近的（平局行优先）。
            // 电量条件 d + safetyMargin <= bat 随 d 单调：最近的都去不了，
            // 更远的也去不了。所以只需找最近一个再判一次。
            srch->beginGoals();
            int goalCnt = 0;
            for (int k = 0; k < (int)chargers.size(); k++)
                if (passable[chargers[k]] && !chUsed[k]) {
                    srch->addGoal(chargers[k], k);
                    goalCnt++;
                }
            int pickCell = -1;
            if (goalCnt > 0) {
                int gc = nearestGoal(*srch, rb.cell);
                if (gc >= 0) {
                    int d = (gc == rb.cell) ? 0 : srch->dist[gc];
                    if ((long long)d + PRM[3] <= rb.bat) pickCell = gc;
                }
            }
            if (pickCell >= 0) {
                rb.st = TO_CHARGER;
                rb.tx = pickCell % W; rb.ty = pickCell / W;
                rb.waitRun = 0;
                logGoCharge(t, i, rb.tx, rb.ty);
                continue;
            }

            // 第 2 步：在“所有可通行充电桩”里找最近（不管占用/电量）。
            // 机器人在可通行格上时直接读本 tick 的多源充电桩场（标签平局已正确）；
            // 机器人可能站在被封格上（场不覆盖该格），退化为显式分层 BFS。
            int cc = -1;
            if (passable[rb.cell]) {
                buildChargerField(t);
                if (fieldReachable(rb.cell)) cc = chargers[cf->which[rb.cell]];
            } else {
                srch->beginGoals();
                int gc2 = 0;
                for (int k = 0; k < (int)chargers.size(); k++)
                    if (passable[chargers[k]]) { srch->addGoal(chargers[k], k); gc2++; }
                if (gc2 > 0) cc = nearestGoal(*srch, rb.cell);
            }
            if (cc < 0) continue;   // 没有可通行/可达充电桩，不动作
            rb.st = TO_CHARGER;
            rb.tx = cc % W; rb.ty = cc / W;
            rb.waitRun = 0;
            logGoCharge(t, i, rb.tx, rb.ty);
        }

        // ====================== 6. 移动 ======================
        for (int i = 0; i < N; i++) {
            Robot& rb = robots[i];
            if (rb.st != TO_PICKUP && rb.st != DELIVERING && rb.st != TO_CHARGER) continue;

            if (rb.x == rb.tx && rb.y == rb.ty) { arrive(t, i); continue; }
            if (rb.bat == 0) { die(t, i); continue; }
            int tcell = cellOf(rb.tx, rb.ty);
            if (!passable[tcell]) { rb.waitTotal++; continue; }   // 目标被封，等待

            // 下一步：在四个可通行邻居中选 dist(目标, 邻居) 最小，
            // 平局按方向顺序（上右下左）——逐方向严格 < 即可保留方向序。
            int neighbor[4], nDir[4], nv = 0;
            for (int d = 0; d < 4; d++) {
                int nx = rb.x + DX[d], ny = rb.y + DY[d];
                if (inMap(nx, ny) && passable[cellOf(nx, ny)]) {
                    neighbor[nv] = cellOf(nx, ny);
                    nDir[nv] = d;
                    nv++;
                }
            }
            int bdir = -1, bdist = INF;
            if (nv > 0) {
                // 无向图：dist(目标, 邻居) 等价于从目标出发搜邻居们
                targets.assign(neighbor, neighbor + nv);
                dists.assign(nv, INF);
                multiTargetDists(*srch, tcell, targets, dists);
                for (int k = 0; k < nv; k++)
                    if (dists[k] < bdist) { bdist = dists[k]; bdir = nDir[k]; }
            }
            if (bdist >= INF) { rb.waitTotal++; continue; }   // 走不过去，等待

            int bx = rb.x + DX[bdir], by = rb.y + DY[bdir];
            if (occupiedByOther(bx, by, i)) {
                bool moved = false;
                // 连续等待 >=4 且下一步不是目标格：让路（方向序找第一个可走空位）
                if (rb.waitRun >= 4 && !(bx == rb.tx && by == rb.ty)) {
                    for (int d = 0; d < 4; d++) {
                        int nx = rb.x + DX[d], ny = rb.y + DY[d];
                        if (!inMap(nx, ny)) continue;
                        int nc = cellOf(nx, ny);
                        if (!passable[nc] || occupiedByOther(nx, ny, i)) continue;
                        rb.x = nx; rb.y = ny; rb.cell = nc;
                        rb.bat--; rb.steps++; rb.waitRun = 0;
                        logSidestep(t, i);
                        moved = true;
                        break;
                    }
                }
                if (!moved) { rb.waitTotal++; rb.waitRun++; continue; }
            } else {
                rb.x = bx; rb.y = by; rb.cell = cellOf(bx, by);
                rb.bat--; rb.steps++; rb.waitRun = 0;
            }

            if (rb.x == rb.tx && rb.y == rb.ty) arrive(t, i);
        }

        // ====================== 7. 报表 ======================
        if ((t + 1) % (int)PRM[4] == 0) {
            int c[6] = {0, 0, 0, 0, 0, 0};
            for (int i = 0; i < N; i++) c[robots[i].st]++;

            // 热点：每个非墙格统计曼哈顿距离 <= hotRadius 的非 DEAD 机器人数，
            // 取最大，平局行优先（基线逐格扫描 + 严格 >，这里顺序一致）。
            // 旋转坐标 u=y+x, v=y-x：|dx|+|dy| = max(|du|,|dv|)，曼哈顿球
            // 变成 (u,v) 空间的轴对齐正方形；每个机器人在其 (u,v) 加点，
            // 做一次二维前缀和即可 O(1) 查询。球内奇偶性不匹配的点不对应
            // 任何真实格，自然没有机器人点权，不影响计数。
            fill(hotDiff.begin(), hotDiff.end(), 0);
            auto hat = [&](int u, int vv) -> int& { return hotDiff[u * (V + 1) + vv]; };
            for (int i = 0; i < N; i++) {
                if (robots[i].st == DEAD) continue;
                int u = robots[i].y + robots[i].x;
                int v = robots[i].y - robots[i].x + W;
                hat(u, v)++;
            }
            for (int u = 1; u < U; u++)
                for (int vv = 1; vv < V; vv++)
                    hat(u, vv) += hat(u - 1, vv) + hat(u, vv - 1) - hat(u - 1, vv - 1);
            auto rect = [&](int u0, int v0, int u1, int v1) -> int {
                u0 = max(u0, 0); v0 = max(v0, 0);
                u1 = min(u1, U - 1); v1 = min(v1, V - 1);
                if (u0 > u1 || v0 > v1) return 0;
                return hat(u1, v1)
                     - (u0 ? hat(u0 - 1, v1) : 0)
                     - (v0 ? hat(u1, v0 - 1) : 0)
                     + ((u0 && v0) ? hat(u0 - 1, v0 - 1) : 0);
            };
            int hx = -1, hy = -1, hc = -1;
            int R = (int)PRM[5];
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    if (MAP[y][x] == '#') continue;
                    int u = y + x, v = y - x + W;
                    int n = rect(u - R, v - R, u + R, v + R);
                    if (n > hc) { hc = n; hx = x; hy = y; }
                }
            string s = "REPORT pending=" + to_string(pend.size()) +
                       " idle=" + to_string(c[IDLE]) +
                       " to_pickup=" + to_string(c[TO_PICKUP]) +
                       " delivering=" + to_string(c[DELIVERING]) +
                       " to_charger=" + to_string(c[TO_CHARGER]) +
                       " charging=" + to_string(c[CHARGING]) +
                       " dead=" + to_string(c[DEAD]) +
                       " hot=" + to_string(hx) + "," + to_string(hy) + "," + to_string(hc);
            writeLog(t, s);
        }
    }
}

// ---------- 收尾 ----------
void finish() {
    long long open = 0;
    // 基线遍历 map（按 id 字符串序）但只计数，顺序不影响结果。
    for (const Order& o : orders)
        if (o.status <= 2) open++;   // PENDING / ASSIGNED / PICKED
    writeRaw("SUMMARY delivered=" + to_string(cnt.delivered) +
             " lost=" + to_string(cnt.lost) +
             " rejected=" + to_string(cnt.rejected) +
             " cancelled=" + to_string(cnt.cancelled) +
             " open=" + to_string(open));
    // [BASELINE] 平均延迟是四舍五入（+delivered/2），不是向下取整
    long long avg = 0;
    if (cnt.delivered > 0) avg = (cnt.latSum + cnt.delivered / 2) / cnt.delivered;
    writeRaw("LATENCY sum=" + to_string(cnt.latSum) +
             " max=" + to_string(cnt.latMax) +
             " avg=" + to_string(avg));
    for (int i = 0; i < N; i++) {
        const Robot& rb = robots[i];
        writeRaw("ROBOT R" + to_string(i) + " " + to_string(rb.x) + " " + to_string(rb.y) +
                 " " + to_string(rb.bat) + " " + STATE_NAME[rb.st] + " " +
                 to_string(rb.steps) + " " + to_string(rb.waitTotal));
    }
}

} // namespace

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    stringstream ss;
    ss << cin.rdbuf();
    string input = ss.str();
    readAll(input);
    runSim();
    finish();
    fwrite(outBuf.data(), 1, outBuf.size(), stdout);
    return 0;
}
