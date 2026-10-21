// sim.cpp --- 仿真主体（读入、逐 tick 流程、收尾）
// 流程与基线完全一致：事件 → 救援 → 充电 → 派单 → 低电量去充电 → 移动 → 报表。
// 与 SPEC.md 不一致之处按基线实现，见 HANDOFF.md。
#include "common.h"

namespace {

struct Order {
    int id, px, py, dx, dy, prio, arrival;
    OStatus st = PENDING;
    int robot = -1;
    int ver = -1, b = INF, c = INF;   // b = dist(取货,送货)，c = 送货点到最近可用充电桩；随封锁版本缓存
};

struct Event {
    int type;   // 0 ORDER 1 CANCEL 2 BLOCK 3 UNBLOCK
    int a[7];   // ORDER: id px py dx dy prio；CANCEL: id；BLOCK/UNBLOCK: x y
};

// ---- 输入 ----
bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

struct Scanner {
    const string& s;
    size_t p = 0;
    explicit Scanner(const string& str) : s(str) {}
    string token() {
        while (p < s.size() && isSpace(s[p])) p++;
        size_t b = p;
        while (p < s.size() && !isSpace(s[p])) p++;
        return s.substr(b, p - b);
    }
    int integer() { return atoi(token().c_str()); }
    // 与 getline 相同：读到换行为止，返回不含换行的内容；去掉末尾 '\r'
    void line(string& out) {
        out.clear();
        if (p >= s.size()) return;
        size_t e = s.find('\n', p);
        if (e == string::npos) e = s.size();
        out.assign(s, p, e - p);
        p = e < s.size() ? e + 1 : e;
        if (!out.empty() && out.back() == '\r') out.pop_back();
    }
};

vector<string> split(const string& s) {
    vector<string> r;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isSpace(s[i])) i++;
        size_t b = i;
        while (i < s.size() && !isSpace(s[i])) i++;
        if (i > b) r.push_back(s.substr(b, i - b));
    }
    return r;
}

// ---- 仿真状态 ----
int T, NR;
int PRM[6];   // 0 满电 1 充电速度 2 低电量 3 安全余量 4 报表间隔 5 热点半径
vector<int> RX, RY, RB, RTX, RTY, RWS, RDS, ROID;
vector<long long> RWT, RTR;
vector<State> RST;
vector<int> occ;   // 格子 -> 占据它的（非停机）机器人，没有为 -1
vector<Order> ords;
unordered_map<int, int> ordIdx;   // 订单号 -> ords 下标
set<array<int, 4> > pend;         // {-优先级, 到达tick, 订单号, 下标}：遍历顺序即派单顺序
vector<array<int, 3> > chargers;  // 所有 'C' 格（行优先）：{格子下标, x, y}
long long cDelivered = 0, cLost = 0, cRejected = 0, cCancel = 0, latSum = 0, latMax = 0;
string* OUT;
int posVersion = 0;   // 机器人位置/存活变化计数，用来缓存热点计算

void appendInt(long long v) { *OUT += to_string(v); }
void logHead(int t) {
    appendInt(t);
    *OUT += ' ';
}
void rname(int i) {
    *OUT += 'R';
    appendInt(i);
}
void endl_() { *OUT += '\n'; }

bool occupied(int idx, int except) {
    int o = occ[idx];
    return o != -1 && o != except;
}

void pendInsert(int oi) {
    const Order& o = ords[oi];
    pend.insert({-o.prio, o.arrival, o.id, oi});
}
void pendErase(int oi) {
    const Order& o = ords[oi];
    pend.erase({-o.prio, o.arrival, o.id, oi});
}

// ---- 1. 事件 ----
void doEvent(int t, const Event& e) {
    if (e.type == 0) {
        int id = e.a[0], px = e.a[1], py = e.a[2], dx = e.a[3], dy = e.a[4], pr = e.a[5];
        if (!G.inMap(px, py) || !G.inMap(dx, dy) || G.wall[G.idx(px, py)] || G.wall[G.idx(dx, dy)]) {
            logHead(t);
            *OUT += "REJECT ";
            appendInt(id);
            endl_();
            cRejected++;
            return;
        }
        Order o;
        o.id = id; o.px = px; o.py = py; o.dx = dx; o.dy = dy; o.prio = pr; o.arrival = t;
        ords.push_back(o);
        ordIdx[id] = (int)ords.size() - 1;   // （合法输入订单号不重复）
        pendInsert((int)ords.size() - 1);
    } else if (e.type == 1) {
        int id = e.a[0];
        auto it = ordIdx.find(id);
        logHead(t);
        if (it == ordIdx.end()) {
            *OUT += "CANCEL_FAIL ";
            appendInt(id);
            endl_();
            return;
        }
        cCancel++;   // 基线怪癖：只要订单存在，无论取消是否成功都计入 cancelled
        Order& o = ords[it->second];
        if (o.st == PENDING) {
            pendErase(it->second);
            o.st = CANCELLED;
            *OUT += "CANCEL ";
        } else if (o.st == ASSIGNED) {
            int ri = o.robot;
            RST[ri] = IDLE;
            ROID[ri] = -1;
            RTX[ri] = RTY[ri] = -1;
            RWS[ri] = 0;
            o.st = CANCELLED;
            o.robot = -1;
            *OUT += "CANCEL ";
        } else {
            *OUT += "CANCEL_FAIL ";
        }
        appendInt(id);
        endl_();
    } else if (e.type == 2) {
        G.setBlock(e.a[0], e.a[1]);
    } else {
        G.clearBlock(e.a[0], e.a[1]);
    }
}

// ---- 2/3. 救援、充电 ----
void rescue(int t) {
    for (int i = 0; i < NR; i++) {
        if (RST[i] != DEAD || t - RDS[i] < 100) continue;
        int ix = G.idx(RX[i], RY[i]);
        if (occupied(ix, i)) continue;
        RST[i] = IDLE;
        RB[i] = PRM[0] / 2;   // 基线：救援只恢复一半电量（SPEC 写的是充满）
        RWS[i] = 0;
        RDS[i] = -1;
        occ[ix] = i;
        posVersion++;
        logHead(t);
        *OUT += "RESCUE ";
        rname(i);
        endl_();
    }
}

void charge(int t) {
    for (int i = 0; i < NR; i++) {
        if (RST[i] != CHARGING) continue;
        RB[i] += PRM[1];
        if (RB[i] > PRM[0]) RB[i] = PRM[0];
        if (RB[i] * 10 >= PRM[0] * 9) {   // 基线：达到 90% 就走（SPEC 写的是充满）
            RST[i] = IDLE;
            RTX[i] = RTY[i] = -1;
            logHead(t);
            *OUT += "CHARGED ";
            rname(i);
            endl_();
        }
    }
}

// ---- 4. 派单 ----
// 订单的 b、c 与机器人无关，只随封锁变化，按版本缓存；不可行的订单直接跳过。
void refreshOrder(Order& o) {
    o.ver = G.version;
    int p = G.idx(o.px, o.py), d = G.idx(o.dx, o.dy);
    o.b = INF;
    o.c = INF;
    if (p == d) o.b = 0;
    else if (G.walk[d]) o.b = G.bfs(p)[d];
    if (o.b >= INF) return;
    const int* dd = G.bfs(d);
    int best = INF;
    for (auto& ch : chargers)
        if (G.walk[ch[0]] && dd[ch[0]] < best) best = dd[ch[0]];
    o.c = best;
}

// 机器人 rIdx 到取货点 p 的距离（基线 DIST(r, p)，方向敏感）：
// 起点即使被封锁也能出发；利用 p 出发的 BFS 表 dp 推算。
int distToPickup(int rIdx, int p, const int* dp) {
    if (rIdx == p) return 0;
    if (!G.walk[p]) return INF;
    if (G.walk[rIdx]) return dp[rIdx];
    int m = INF;
    for (int k = 0; k < 4; k++) {
        int n = rIdx + G.off[k];
        if (G.walk[n] && dp[n] < m) m = dp[n];
    }
    return m >= INF ? INF : m + 1;
}

void dispatch(int t) {
    vector<int> cands;
    for (int i = 0; i < NR; i++)
        if (RST[i] == IDLE && RB[i] >= PRM[2]) cands.push_back(i);
    for (auto it = pend.begin(); it != pend.end() && !cands.empty();) {
        int oi = (*it)[3];
        Order& o = ords[oi];
        if (o.ver != G.version) refreshOrder(o);
        if (o.b >= INF || o.c >= INF) { ++it; continue; }
        int need = o.b + o.c + PRM[3];
        int p = G.idx(o.px, o.py);
        const int* dp = nullptr;
        int best = INF, who = -1, wpos = -1;
        for (int k = 0; k < (int)cands.size(); k++) {
            int i = cands[k];
            if (RB[i] < need) continue;
            int ri = G.idx(RX[i], RY[i]);
            if (!dp && ri != p && G.walk[p]) dp = G.bfs(p);
            int a = distToPickup(ri, p, dp);
            if (a >= INF || RB[i] < a + need) continue;
            if (a <= best) {   // 基线：距离相同取编号大的（SPEC 写的是编号小的）
                best = a;
                who = i;
                wpos = k;
            }
        }
        if (who < 0) { ++it; continue; }
        RST[who] = TO_PICKUP;
        ROID[who] = oi;
        RTX[who] = o.px;
        RTY[who] = o.py;
        RWS[who] = 0;
        o.st = ASSIGNED;
        o.robot = who;
        it = pend.erase(it);
        cands.erase(cands.begin() + wpos);
        logHead(t);
        *OUT += "ASSIGN ";
        appendInt(o.id);
        *OUT += ' ';
        rname(who);
        *OUT += ' ';
        appendInt(best);
        endl_();
    }
}

// ---- 5. 低电量去充电 ----
void lowBattery(int t) {
    vector<unsigned char> used;   // 被占用的充电桩（别人在充，或别人正前往）
    bool built = false;
    for (int i = 0; i < NR; i++) {
        if (RST[i] != IDLE || RB[i] >= PRM[2]) continue;
        if (!built) {
            built = true;
            used.assign(G.PW * G.PH, 0);
            for (int j = 0; j < NR; j++) {
                if (RST[j] == CHARGING) used[G.idx(RX[j], RY[j])] = 1;
                else if (RST[j] == TO_CHARGER) used[G.idx(RTX[j], RTY[j])] = 1;
            }
        }
        const int* d = G.bfs(G.idx(RX[i], RY[i]));
        int best = INF, cx = -1, cy = -1;
        for (auto& ch : chargers) {
            if (!G.walk[ch[0]] || used[ch[0]]) continue;
            if (d[ch[0]] + PRM[3] > RB[i]) continue;
            if (d[ch[0]] < best) {
                best = d[ch[0]];
                cx = ch[1];
                cy = ch[2];
            }
        }
        if (cx < 0) {   // 都被占了：去最近的可达充电桩排队
            for (auto& ch : chargers) {
                if (!G.walk[ch[0]]) continue;
                if (d[ch[0]] < best) {
                    best = d[ch[0]];
                    cx = ch[1];
                    cy = ch[2];
                }
            }
        }
        if (cx < 0) continue;
        RST[i] = TO_CHARGER;
        RTX[i] = cx;
        RTY[i] = cy;
        RWS[i] = 0;
        used[G.idx(cx, cy)] = 1;
        logHead(t);
        *OUT += "GO_CHARGE ";
        rname(i);
        *OUT += ' ';
        appendInt(cx);
        *OUT += ' ';
        appendInt(cy);
        endl_();
    }
}

// ---- 6. 移动 ----
void arrive(int t, int i) {
    RWS[i] = 0;
    if (RST[i] == TO_PICKUP) {
        Order& o = ords[ROID[i]];
        o.st = PICKED;
        RST[i] = DELIVERING;
        RTX[i] = o.dx;
        RTY[i] = o.dy;
        logHead(t);
        *OUT += "PICK ";
        appendInt(o.id);
        *OUT += ' ';
        rname(i);
        endl_();
    } else if (RST[i] == DELIVERING) {
        Order& o = ords[ROID[i]];
        o.st = DONE;
        int lat = t - o.arrival;
        cDelivered++;
        latSum += lat;
        if (lat > latMax) latMax = lat;
        logHead(t);
        *OUT += "DELIVER ";
        appendInt(o.id);
        *OUT += ' ';
        rname(i);
        *OUT += ' ';
        appendInt(lat);
        endl_();
        RST[i] = IDLE;
        ROID[i] = -1;
        RTX[i] = RTY[i] = -1;
    } else if (RST[i] == TO_CHARGER) {
        RST[i] = CHARGING;
        logHead(t);
        *OUT += "CHARGE ";
        rname(i);
        endl_();
    }
}

void die(int t, int i) {
    logHead(t);
    *OUT += "DEAD ";
    rname(i);
    endl_();
    if (ROID[i] != -1) {
        Order& o = ords[ROID[i]];
        if (RST[i] == TO_PICKUP) {
            o.st = PENDING;
            o.robot = -1;
            pendInsert(ROID[i]);
            logHead(t);
            *OUT += "REQUEUE ";
            appendInt(o.id);
            endl_();
        } else if (RST[i] == DELIVERING) {
            o.st = LOST;
            cLost++;
            logHead(t);
            *OUT += "LOST ";
            appendInt(o.id);
            endl_();
        }
    }
    occ[G.idx(RX[i], RY[i])] = -1;
    posVersion++;
    RST[i] = DEAD;
    RDS[i] = t;
    ROID[i] = -1;
    RTX[i] = RTY[i] = -1;
}

void moveStep(int i, int nx, int ny) {
    occ[G.idx(RX[i], RY[i])] = -1;
    RX[i] = nx;
    RY[i] = ny;
    occ[G.idx(nx, ny)] = i;
    RB[i]--;
    RTR[i]++;
    RWS[i] = 0;
    posVersion++;
}

const int DXs[4] = {0, 1, 0, -1};
const int DYs[4] = {-1, 0, 1, 0};   // 上 右 下 左，顺序不能改

void moveAll(int t) {
    for (int i = 0; i < NR; i++) {
        State s = RST[i];
        if (s != TO_PICKUP && s != DELIVERING && s != TO_CHARGER) continue;
        if (RX[i] == RTX[i] && RY[i] == RTY[i]) {
            arrive(t, i);
            continue;
        }
        if (RB[i] == 0) {
            die(t, i);
            continue;
        }
        int tIdx = G.idx(RTX[i], RTY[i]);
        if (!G.walk[tIdx]) {   // 目标被封了，等着
            RWT[i]++;
            continue;
        }
        const int* d = G.bfs(tIdx);
        int cur = G.idx(RX[i], RY[i]);
        int best = INF, bk = -1;
        for (int k = 0; k < 4; k++) {
            int n = cur + G.off[k];
            if (!G.walk[n]) continue;
            if (d[n] < best) {
                best = d[n];
                bk = k;
            }
        }
        if (best >= INF) {   // 走不过去
            RWT[i]++;
            continue;
        }
        int bIdx = cur + G.off[bk];
        if (occupied(bIdx, i)) {
            bool moved = false;
            if (RWS[i] >= 4 && bIdx != tIdx) {   // 等太久了，让一让（让路到第一个空闲相邻格）
                for (int k = 0; k < 4; k++) {
                    int n = cur + G.off[k];
                    if (G.walk[n] && !occupied(n, i)) {
                        moveStep(i, RX[i] + DXs[k], RY[i] + DYs[k]);
                        logHead(t);
                        *OUT += "SIDESTEP ";
                        rname(i);
                        endl_();
                        moved = true;
                        break;
                    }
                }
            }
            if (!moved) {
                RWT[i]++;
                RWS[i]++;
                continue;
            }
        } else {
            moveStep(i, RX[i] + DXs[bk], RY[i] + DYs[bk]);
        }
        if (RX[i] == RTX[i] && RY[i] == RTY[i]) arrive(t, i);
    }
}

// ---- 7. 报表 ----
struct Hot {
    int x = -1, y = -1, k = -1;
    int ver = -1;
} hotCache;

void computeHot(Hot& h) {
    int R = PRM[5];
    vector<int> ax, ay;
    for (int i = 0; i < NR; i++)
        if (RST[i] != DEAD) {
            ax.push_back(RX[i]);
            ay.push_back(RY[i]);
        }
    int n = (int)ax.size();
    int W = G.W, H = G.H;
    int S = W + H - 1;
    long long cells = (long long)W * H;
    h.k = -1;
    auto consider = [&](int x, int y, int k) {
        if (k > h.k) {
            h.k = k;
            h.x = x;
            h.y = y;
        }
    };
    if (cells * n <= (long long)S * S + 8 * cells) {   // 机器人少：直接统计
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                if (G.map[y][x] == '#') continue;
                int c = 0;
                for (int j = 0; j < n; j++)
                    if (abs(ax[j] - x) + abs(ay[j] - y) <= R) c++;
                consider(x, y, c);
            }
        return;
    }
    // 曼哈顿球在 (u=x+y, v=x-y+H-1) 坐标下是正方形：二维前缀和
    int P = S + 1;
    vector<int> pre((size_t)P * P, 0);
    for (int j = 0; j < n; j++) {
        int u = ax[j] + ay[j], v = ax[j] - ay[j] + H - 1;
        pre[(size_t)(u + 1) * P + (v + 1)]++;
    }
    for (int u = 1; u <= S; u++)
        for (int v = 1; v <= S; v++)
            pre[(size_t)u * P + v] += pre[(size_t)(u - 1) * P + v] + pre[(size_t)u * P + v - 1] - pre[(size_t)(u - 1) * P + v - 1];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (G.map[y][x] == '#') continue;
            int u = x + y, v = x - y + H - 1;
            int u0 = max(0, u - R), u1 = min(S - 1, u + R);
            int v0 = max(0, v - R), v1 = min(S - 1, v + R);
            int c = pre[(size_t)(u1 + 1) * P + v1 + 1] - pre[(size_t)u0 * P + v1 + 1] - pre[(size_t)(u1 + 1) * P + v0] +
                    pre[(size_t)u0 * P + v0];
            consider(x, y, c);
        }
}

void report(int t) {
    int c[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < NR; i++) c[RST[i]]++;
    if (hotCache.ver != posVersion) {
        computeHot(hotCache);
        hotCache.ver = posVersion;
    }
    logHead(t);
    *OUT += "REPORT pending=";
    appendInt((long long)pend.size());
    *OUT += " idle=";
    appendInt(c[IDLE]);
    *OUT += " to_pickup=";
    appendInt(c[TO_PICKUP]);
    *OUT += " delivering=";
    appendInt(c[DELIVERING]);
    *OUT += " to_charger=";
    appendInt(c[TO_CHARGER]);
    *OUT += " charging=";
    appendInt(c[CHARGING]);
    *OUT += " dead=";
    appendInt(c[DEAD]);
    *OUT += " hot=";
    appendInt(hotCache.x);
    *OUT += ',';
    appendInt(hotCache.y);
    *OUT += ',';
    appendInt(hotCache.k);
    endl_();
}

const char* STATE_NAME[6] = {"IDLE", "TO_PICKUP", "DELIVERING", "TO_CHARGER", "CHARGING", "DEAD"};

}  // namespace

void runAll(const string& input, string& out) {
    OUT = &out;
    Scanner sc(input);
    int W = sc.integer(), H = sc.integer();
    T = sc.integer();
    vector<string> mp(H);
    for (int i = 0; i < H; i++) mp[i] = sc.token();
    sc.token();   // PARAMS
    for (int i = 0; i < 6; i++) PRM[i] = sc.integer();
    sc.token();   // ROBOTS
    NR = sc.integer();
    G.init(W, H, mp);
    RX.resize(NR); RY.resize(NR); RB.assign(NR, PRM[0]);
    RTX.assign(NR, -1); RTY.assign(NR, -1); RWS.assign(NR, 0); RDS.assign(NR, -1);
    ROID.assign(NR, -1); RWT.assign(NR, 0); RTR.assign(NR, 0); RST.assign(NR, IDLE);
    occ.assign(G.PW * G.PH, -1);
    for (int i = 0; i < NR; i++) {
        RX[i] = sc.integer();
        RY[i] = sc.integer();
        occ[G.idx(RX[i], RY[i])] = i;
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (mp[y][x] == 'C') chargers.push_back({G.idx(x, y), x, y});

    sc.token();   // EVENTS
    int ne = sc.integer();
    // 基线按行读事件（getline）。按 tick 分桶，桶内保持输入顺序；tick 不在 [0,T) 的事件永远不会被处理。
    vector<vector<Event> > byTick(T);
    string line;
    sc.line(line);   // 吃掉 "EVENTS E" 这一行剩下的部分
    for (int e = 0; e < ne; e++) {
        sc.line(line);
        vector<string> w = split(line);
        if (w.size() < 2) continue;
        int tk = atoi(w[0].c_str());
        if (tk < 0 || tk >= T) continue;
        Event ev;
        for (int& v : ev.a) v = 0;
        const string& ty = w[1];
        if (ty == "ORDER") {
            ev.type = 0;
            for (int k = 0; k < 6; k++) ev.a[k] = atoi(w[2 + k].c_str());
        } else if (ty == "CANCEL") {
            ev.type = 1;
            ev.a[0] = atoi(w[2].c_str());
        } else if (ty == "BLOCK" || ty == "UNBLOCK") {
            ev.type = ty == "BLOCK" ? 2 : 3;
            ev.a[0] = atoi(w[2].c_str());
            ev.a[1] = atoi(w[3].c_str());
        } else {
            continue;
        }
        byTick[tk].push_back(ev);
    }

    for (int t = 0; t < T; t++) {
        for (const Event& ev : byTick[t]) doEvent(t, ev);
        rescue(t);
        charge(t);
        dispatch(t);
        lowBattery(t);
        moveAll(t);
        if ((t + 1) % PRM[4] == 0) report(t);
    }

    long long open = 0;
    for (const Order& o : ords)
        if (o.st == PENDING || o.st == ASSIGNED || o.st == PICKED) open++;
    out += "SUMMARY delivered=" + to_string(cDelivered) + " lost=" + to_string(cLost) +
           " rejected=" + to_string(cRejected) + " cancelled=" + to_string(cCancel) + " open=" + to_string(open) + "\n";
    long long avg = cDelivered > 0 ? (latSum + cDelivered / 2) / cDelivered : 0;   // 基线：四舍五入
    out += "LATENCY sum=" + to_string(latSum) + " max=" + to_string(latMax) + " avg=" + to_string(avg) + "\n";
    for (int i = 0; i < NR; i++)
        out += "ROBOT R" + to_string(i) + " " + to_string(RX[i]) + " " + to_string(RY[i]) + " " + to_string(RB[i]) + " " +
               STATE_NAME[RST[i]] + " " + to_string(RTR[i]) + " " + to_string(RWT[i]) + "\n";
}
