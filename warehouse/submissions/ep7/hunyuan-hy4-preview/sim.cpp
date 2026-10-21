// sim.cpp   仿真主体
//
// 2026 重写版：流程、判定、输出顺序全部照抄 baseline/ 的**实际行为**
// （包括那些和 SPEC.md 不一致的地方 —— 清单见 HANDOFF.md）。
//
// 主要加速点：
//   1. 事件按 tick 预先分桶（基线是每 tick 扫全部事件并重新切词 → O(T*E)）。
//   2. BFS 距离场带缓存（见 util.cpp），派单/充电/移动都复用；订单的 b 值按订单缓存。
//   3. c = 到最近充电桩的距离，用一次多源 BFS 换成 O(1) 查询（见 util.cpp）。
//   4. 占据查询、充电桩清单全部 O(1) / 预计算，不再每次扫全场、扫全部机器人。
//   5. 订单/机器人状态改成结构体 + 整数枚举，不再用 string 做键和状态。
//   6. 输出先拼进一个大字符串，最后一次写出（基线每写一行都 flush）。
#include "common.h"

// 机器人状态（顺序与输出的名字一致）
enum { ST_IDLE = 0, ST_TO_PICKUP, ST_DELIVERING, ST_TO_CHARGER, ST_CHARGING, ST_DEAD };
static const char* STN[6] = {"IDLE", "TO_PICKUP", "DELIVERING", "TO_CHARGER", "CHARGING", "DEAD"};
// 订单状态
enum { O_PENDING = 0, O_ASSIGNED, O_PICKED, O_DONE, O_CANCELLED, O_LOST };
// 事件类型
enum { EV_ORDER = 0, EV_CANCEL, EV_BLOCK, EV_UNBLOCK, EV_OTHER };

struct Order {
    string id;          // 原样输出
    long long idnum;    // 排序用（数值）
    int px, py, dx, dy;
    int prio;
    int arrival;
    int status;
    int robot;          // 负责的机器人编号，-1 表示没有
    int pendCount;      // 在待派列表里的份数（合法输入下只会是 0/1）
    int pendPos;        // 在待派列表里的位置
    int bEpoch, bVal;   // b = dist(取货点, 送货点) 的缓存（按封锁版本号）
};

struct Ev {
    int tick;
    int type;
    string id;      // ORDER / CANCEL
    int a, b, c, d, e;
};

static vector<Order> ORD;
static unordered_map<string, int> ID2ORD;
static vector<int> PENDL;            // 待派订单（下标），顺序只影响派单前的快照
static bool g_hasDupPend = false;    // 是否出现过重复入列（非法输入才有）

static vector<int> RC;               // 机器人所在 cell
static vector<int> RB;               // 电量
static vector<int> RTC;              // 目标 cell，-1 表示没有
static vector<int> RWS;              // 连续等待次数
static vector<int> RDS;              // 停机时刻
static vector<int> RST;              // 状态
static vector<int> ROID;             // 手上订单下标，-1 表示没有
static vector<long long> RWT, RTR;   // 累计等待 / 移动步数

static vector<Ev> EVS;
static vector<int> EVORDER;          // 按 tick 分桶后的事件顺序（同 tick 保持输入顺序）
static vector<int> EVSTART;          // EVSTART[t]..EVSTART[t+1]

static string OUT;
static long long CNT[8];             // 0 送达 1 丢失 2 拒绝 3 取消 4 延迟和 5 延迟最大

// 充电桩“已被占”标记：只记被标记过的格子，下个 tick 增量清掉（避免每 tick 清全场）
static vector<unsigned char> claimedFlag;
static vector<int> claimedList;
static void claimCharger(int c) {
    if (c < 0 || claimedFlag[c]) return;
    claimedFlag[c] = 1;
    claimedList.push_back(c);
}

static void emit(int t, const string& msg) {
    APPEND_INT(OUT, (long long)t);
    OUT.push_back(' ');
    OUT += msg;
    OUT.push_back('\n');
}
static void emitRaw(const string& msg) {
    OUT += msg;
    OUT.push_back('\n');
}

// ---------------------------------------------------------------- 待派列表
static void pendPush(int oi) {
    Order& o = ORD[oi];
    if (o.pendCount > 0) g_hasDupPend = true;
    o.pendCount++;
    if (o.pendPos < 0) o.pendPos = (int)PENDL.size();
    PENDL.push_back(oi);
}

static void pendRemove(int oi) {
    Order& o = ORD[oi];
    if (o.pendCount <= 0) return;
    o.pendCount = 0;
    int p = o.pendPos;
    o.pendPos = -1;
    if (p >= 0 && p < (int)PENDL.size() && PENDL[p] == oi) {
        int last = PENDL.back();
        PENDL[p] = last;
        if (last != oi) ORD[last].pendPos = p;
        PENDL.pop_back();
    }
    if (g_hasDupPend) {   // 只有出现重复 id（非法输入）时才会走到这里
        for (int k = (int)PENDL.size() - 1; k >= 0; k--) {
            if (PENDL[k] != oi) continue;
            int last = PENDL.back();
            PENDL[k] = last;
            if (last != oi) ORD[last].pendPos = k;
            PENDL.pop_back();
        }
    }
}

// ---------------------------------------------------------------- 读输入
void ReadAll(istream& in) {
    in >> W >> H >> T;
    MAP.resize(H);
    for (int i = 0; i < H; i++) in >> MAP[i];
    string tmp;
    in >> tmp;                       // "PARAMS"
    for (int i = 0; i < 6; i++) in >> PRM[i];
    in >> tmp >> NR;                 // "ROBOTS"
    vector<int> sx(NR), sy(NR);
    for (int i = 0; i < NR; i++) in >> sx[i] >> sy[i];
    int ne = 0;
    in >> tmp >> ne;                 // "EVENTS"
    string line;
    getline(in, line);
    vector<string> lines;
    lines.reserve(ne);
    for (int i = 0; i < ne; i++) {
        getline(in, line);
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        lines.push_back(line);
    }
    gridInit();
    claimedFlag.assign(W * H, 0);
    claimedList.clear();

    // 机器人：满电出发
    RC.resize(NR); RB.assign(NR, PRM[0]); RTC.assign(NR, -1);
    RWS.assign(NR, 0); RDS.assign(NR, -1); RST.assign(NR, ST_IDLE); ROID.assign(NR, -1);
    RWT.assign(NR, 0); RTR.assign(NR, 0);
    for (int i = 0; i < NR; i++) {
        RC[i] = cellOf(sx[i], sy[i]);
        occAdd(RC[i], 1);
    }

    // 事件：只切一次词，之后按 tick 稳定分桶（同 tick 内保持输入顺序）
    EVS.reserve(lines.size());
    vector<int> cntPerTick(T + 1, 0);
    for (size_t i = 0; i < lines.size(); i++) {
        vector<string> w = SPLIT(lines[i]);
        Ev ev;
        ev.tick = 0; ev.type = EV_OTHER; ev.a = ev.b = ev.c = ev.d = ev.e = 0;
        if (w.size() < 2) { EVS.push_back(ev); continue; }   // 基线：少于 2 项直接跳过
        ev.tick = S2I(w[0]);
        const string& ty = w[1];
        if (ty == "ORDER" && w.size() >= 8) {
            ev.type = EV_ORDER;
            ev.id = w[2];
            ev.a = S2I(w[3]); ev.b = S2I(w[4]); ev.c = S2I(w[5]); ev.d = S2I(w[6]); ev.e = S2I(w[7]);
        } else if (ty == "CANCEL" && w.size() >= 3) {
            ev.type = EV_CANCEL;
            ev.id = w[2];
        } else if (ty == "BLOCK" && w.size() >= 4) {
            ev.type = EV_BLOCK;
            ev.a = S2I(w[2]); ev.b = S2I(w[3]);
        } else if (ty == "UNBLOCK" && w.size() >= 4) {
            ev.type = EV_UNBLOCK;
            ev.a = S2I(w[2]); ev.b = S2I(w[3]);
        }
        EVS.push_back(ev);
        if (ev.tick >= 0 && ev.tick < T) cntPerTick[ev.tick]++;
    }
    EVSTART.assign(T + 2, 0);
    for (int t = 0; t < T; t++) EVSTART[t + 1] = EVSTART[t] + cntPerTick[t];
    EVORDER.assign(EVSTART[T], -1);
    vector<int> fill(EVSTART.begin(), EVSTART.begin() + T + 1);
    for (size_t i = 0; i < EVS.size(); i++) {
        int t = EVS[i].tick;
        if (t < 0 || t >= T) continue;
        EVORDER[fill[t]++] = (int)i;
    }
}

// ---------------------------------------------------------------- 主循环
// 到达目标（TO_PICKUP / DELIVERING / TO_CHARGER 三种状态共用的收尾）
static void arrive(int t, int i) {
    RWS[i] = 0;
    if (RST[i] == ST_TO_PICKUP) {
        int oi = ROID[i];
        ORD[oi].status = O_PICKED;
        RST[i] = ST_DELIVERING;
        RTC[i] = cellOf(ORD[oi].dx, ORD[oi].dy);
        emit(t, "PICK " + ORD[oi].id + " " + RNAME(i));
    } else if (RST[i] == ST_DELIVERING) {
        int oi = ROID[i];
        ORD[oi].status = O_DONE;
        int lat = t - ORD[oi].arrival;
        emit(t, "DELIVER " + ORD[oi].id + " " + RNAME(i) + " " + I2S(lat));
        CNT[0]++;
        CNT[4] += lat;
        if (lat > CNT[5]) CNT[5] = lat;
        RST[i] = ST_IDLE;
        ROID[i] = -1;
        RTC[i] = -1;
    } else if (RST[i] == ST_TO_CHARGER) {
        RST[i] = ST_CHARGING;
        emit(t, "CHARGE " + RNAME(i));
    }
}

// 停机
static void killRobot(int t, int i) {
    emit(t, "DEAD " + RNAME(i));
    int oi = ROID[i];
    if (oi >= 0) {
        if (RST[i] == ST_TO_PICKUP) {
            ORD[oi].status = O_PENDING;
            ORD[oi].robot = -1;
            pendPush(oi);
            emit(t, "REQUEUE " + ORD[oi].id);
        } else if (RST[i] == ST_DELIVERING) {
            ORD[oi].status = O_LOST;
            emit(t, "LOST " + ORD[oi].id);
            CNT[1]++;
        }
    }
    occAdd(RC[i], -1);
    RST[i] = ST_DEAD;
    RDS[i] = t;
    ROID[i] = -1;
    RTC[i] = -1;
}

void RunSim() {
    vector<int> ids;              // 派单时的待派订单快照
    vector<pair<int, int> > cand; // (机器人, a)
    vector<int> imos;             // 热点统计用的二维差分
    int M = W + H - 1;

    for (int t = 0; t < T; t++) {
        // ====================== 1. 外部事件 ======================
        for (int ei = EVSTART[t]; ei < EVSTART[t + 1]; ei++) {
            const Ev& ev = EVS[EVORDER[ei]];
            if (ev.type == EV_ORDER) {
                int px = ev.a, py = ev.b, dx = ev.c, dy = ev.d;
                bool good = true;
                if (!INMAP(px, py)) good = false;
                if (good && !INMAP(dx, dy)) good = false;
                if (good && ISWALL(px, py)) good = false;
                if (good && ISWALL(dx, dy)) good = false;
                if (!good) {
                    emit(t, "REJECT " + ev.id);
                    CNT[2]++;
                    continue;
                }
                int oi = (int)ORD.size();
                Order o;
                o.id = ev.id;
                o.idnum = S2I(ev.id);
                o.px = px; o.py = py; o.dx = dx; o.dy = dy;
                o.prio = ev.e;
                o.arrival = t;
                o.status = O_PENDING;
                o.robot = -1;
                o.pendCount = 0; o.pendPos = -1;
                o.bEpoch = -1; o.bVal = INF;
                ORD.push_back(o);
                ID2ORD[o.id] = oi;      // 重复的 id 会覆盖（和基线 map 赋值一致）
                pendPush(oi);
            } else if (ev.type == EV_CANCEL) {
                unordered_map<string, int>::iterator it = ID2ORD.find(ev.id);
                if (it == ID2ORD.end()) {
                    emit(t, "CANCEL_FAIL " + ev.id);
                    continue;
                }
                CNT[3]++;               // 注意：取消失败也算进 cancelled（基线就这样）
                int oi = it->second;
                if (ORD[oi].status == O_PENDING) {
                    pendRemove(oi);
                    ORD[oi].status = O_CANCELLED;
                    ORD[oi].robot = -1;
                    emit(t, "CANCEL " + ev.id);
                } else if (ORD[oi].status == O_ASSIGNED) {
                    int ri = ORD[oi].robot;
                    RST[ri] = ST_IDLE;
                    ROID[ri] = -1;
                    RTC[ri] = -1;
                    RWS[ri] = 0;
                    ORD[oi].status = O_CANCELLED;
                    ORD[oi].robot = -1;
                    emit(t, "CANCEL " + ev.id);
                } else {
                    emit(t, "CANCEL_FAIL " + ev.id);
                }
            } else if (ev.type == EV_BLOCK) {
                setBlocked(cellOf(ev.a, ev.b), true);
            } else if (ev.type == EV_UNBLOCK) {
                setBlocked(cellOf(ev.a, ev.b), false);
            }
        }

        // ====================== 2. 救援 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != ST_DEAD) continue;
            if (t - RDS[i] < 100) continue;
            if (OCC(RC[i], -1)) continue;       // DEAD 的机器人自己不占格子
            RST[i] = ST_IDLE;
            RB[i] = PRM[0] / 2;                 // 注意：救回来只有半格电（SPEC 写的是充满）
            RWS[i] = 0;
            RDS[i] = -1;
            occAdd(RC[i], 1);
            emit(t, "RESCUE " + RNAME(i));
        }

        // ====================== 3. 充电 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != ST_CHARGING) continue;
            RB[i] = RB[i] + PRM[1];
            if (RB[i] > PRM[0]) RB[i] = PRM[0];
            if ((long long)RB[i] * 10 >= (long long)PRM[0] * 9) {   // 90% 就走（不是充满）
                RST[i] = ST_IDLE;
                RTC[i] = -1;
                emit(t, "CHARGED " + RNAME(i));
            }
        }

        // ====================== 4. 派单 ======================
        // 没有空闲（且电量够）的机器人时，整个派单阶段不可能有任何输出 —— 直接跳过。
        // 订单压了很多、机器人全在忙的时候，这一句省掉了「待派数 × 机器人数」的扫描。
        int availIdle = 0;
        for (int i = 0; i < NR; i++)
            if (RST[i] == ST_IDLE && RB[i] >= PRM[2]) availIdle++;

        ids.clear();
        if (availIdle > 0) ids = PENDL;
        sort(ids.begin(), ids.end(), [](int a, int b) {
            const Order& x = ORD[a];
            const Order& y = ORD[b];
            if (x.prio != y.prio) return x.prio > y.prio;
            if (x.arrival != y.arrival) return x.arrival < y.arrival;
            return x.idnum < y.idnum;
        });
        for (size_t oi = 0; oi < ids.size() && availIdle > 0; oi++) {
            Order& o = ORD[ids[oi]];
            int pc = cellOf(o.px, o.py);
            int dc = cellOf(o.dx, o.dy);
            // 候选机器人：IDLE 且电量 >= lowThreshold（本 tick 前面已被派掉的不算）
            cand.clear();
            for (int i = 0; i < NR; i++) {
                if (RST[i] != ST_IDLE) continue;
                if (RB[i] < PRM[2]) continue;
                int a;
                if (RC[i] == pc) a = 0;                        // 同格 → 0（即使那格被封了）
                else if (!PASSABLE(pc)) a = INF;
                else a = fieldDist(fieldFrom(RC[i]), pc);
                if (a >= INF) continue;
                cand.push_back(make_pair(i, a));
            }
            if (cand.empty()) continue;                        // 没有合格候选人，连 b/c 都不用算
            if (o.bEpoch != curEpoch()) {
                o.bEpoch = curEpoch();
                o.bVal = (pc == dc) ? 0
                       : (PASSABLE(dc) ? fieldDist(fieldFrom(pc), dc) : INF);
            }
            if (o.bVal >= INF) continue;
            int c = chargerDistFrom(o.dx, o.dy);
            if (c >= INF) continue;
            int best = INF, who = -1;
            for (size_t k = 0; k < cand.size(); k++) {
                int i = cand[k].first, a = cand[k].second;
                if (RB[i] < a + o.bVal + c + PRM[3]) continue;
                if (a <= best) { best = a; who = i; }          // 注意是 <=：平局取编号大的
            }
            if (who < 0) continue;
            availIdle--;                       // 机器人用完了，剩下的订单本 tick 不可能再派出去
            RST[who] = ST_TO_PICKUP;
            ROID[who] = ids[oi];
            RTC[who] = pc;
            RWS[who] = 0;
            o.status = O_ASSIGNED;
            o.robot = who;
            pendRemove(ids[oi]);
            string msg = "ASSIGN " + o.id + " " + RNAME(who) + " ";
            APPEND_INT(msg, best);
            emit(t, msg);
        }

        // ====================== 5. 低电量去充电 ======================
        for (size_t k = 0; k < claimedList.size(); k++) claimedFlag[claimedList[k]] = 0;
        claimedList.clear();
        for (int j = 0; j < NR; j++) {
            if (RST[j] == ST_CHARGING) claimCharger(RC[j]);
            else if (RST[j] == ST_TO_CHARGER) claimCharger(RTC[j]);
        }
        for (int i = 0; i < NR; i++) {
            if (RST[i] != ST_IDLE) continue;
            if (RB[i] >= PRM[2]) continue;
            const Field& f = fieldFrom(RC[i]);
            const vector<int>& cs = allChargerCells();
            int best = INF, cx = -1, cy = -1;
            for (size_t k = 0; k < cs.size(); k++) {
                int c = cs[k];
                if (!PASSABLE(c)) continue;
                if (claimedFlag[c]) continue;
                int d = fieldDist(f, c);
                if (d + PRM[3] > RB[i]) continue;
                if (d < best) { best = d; cx = c % W; cy = c / W; }
            }
            if (cx < 0) {
                // 都被占了 / 都太远：不管占不占，去最近的排队
                for (size_t k = 0; k < cs.size(); k++) {
                    int c = cs[k];
                    if (!PASSABLE(c)) continue;
                    int d = fieldDist(f, c);
                    if (d < best) { best = d; cx = c % W; cy = c / W; }
                }
            }
            if (cx < 0) continue;                              // 一个能到的充电桩都没有
            int tc = cellOf(cx, cy);
            RST[i] = ST_TO_CHARGER;
            RTC[i] = tc;
            RWS[i] = 0;
            claimCharger(tc);                                  // 后面的机器人要看到这个占用
            string msg = "GO_CHARGE " + RNAME(i) + " ";
            APPEND_INT(msg, cx);
            msg += " ";
            APPEND_INT(msg, cy);
            emit(t, msg);
        }

        // ====================== 6. 移动 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != ST_TO_PICKUP && RST[i] != ST_DELIVERING && RST[i] != ST_TO_CHARGER) continue;
            if (RC[i] == RTC[i]) { arrive(t, i); continue; }   // 已经在目标格上
            if (RB[i] == 0) { killRobot(t, i); continue; }     // 没电了，趴窝
            if (!PASSABLE(RTC[i])) { RWT[i]++; continue; }     // 目标被封，等着
            const Field& f = fieldFrom(RTC[i]);
            const int* nb = NB4(RC[i]);
            int best = INF, bc = -1;
            for (int k = 0; k < 4; k++) {
                int u = nb[k];
                if (u < 0 || !PASSABLE(u)) continue;
                int d = fieldDist(f, u);
                if (d < best) { best = d; bc = u; }
            }
            if (best >= INF) { RWT[i]++; continue; }           // 走不过去
            bool moved = false;
            if (OCC(bc, RC[i])) {
                if (RWS[i] >= 4 && bc != RTC[i]) {
                    const int* nb2 = NB4(RC[i]);
                    for (int k = 0; k < 4; k++) {
                        int u = nb2[k];
                        if (u < 0 || !PASSABLE(u)) continue;
                        if (OCC(u, RC[i])) continue;
                        occAdd(RC[i], -1);
                        RC[i] = u;
                        occAdd(u, 1);
                        RB[i]--;
                        RTR[i]++;
                        RWS[i] = 0;
                        emit(t, "SIDESTEP " + RNAME(i));
                        moved = true;
                        break;
                    }
                }
                if (!moved) {
                    RWT[i]++;
                    RWS[i]++;
                    continue;
                }
            } else {
                occAdd(RC[i], -1);
                RC[i] = bc;
                occAdd(bc, 1);
                RB[i]--;
                RTR[i]++;
                RWS[i] = 0;
            }
            if (RC[i] == RTC[i]) arrive(t, i);
        }

        // ====================== 7. 报表 ======================
        if (PRM[4] != 0 && (t + 1) % PRM[4] == 0) {
            int c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
            for (int i = 0; i < NR; i++) {
                if (RST[i] == ST_IDLE) c0++;
                else if (RST[i] == ST_TO_PICKUP) c1++;
                else if (RST[i] == ST_DELIVERING) c2++;
                else if (RST[i] == ST_TO_CHARGER) c3++;
                else if (RST[i] == ST_CHARGING) c4++;
                else if (RST[i] == ST_DEAD) c5++;
            }
            // 热点：每个非墙格子统计曼哈顿距离 <= hotRadius 的非停机机器人数，取最大，
            // 平局取行优先靠前的格子。用 (x+y, x-y) 旋转坐标把菱形变成矩形，二维差分。
            int hx = -1, hy = -1, hc = -1;
            int MW = M + 1;
            imos.assign((size_t)MW * MW, 0);
            int r = PRM[5];
            for (int i = 0; i < NR; i++) {
                if (RST[i] == ST_DEAD) continue;
                int x = RC[i] % W, y = RC[i] / W;
                int u0 = x + y, v0 = x - y + (H - 1);
                int u1 = u0 - r, u2 = u0 + r, v1 = v0 - r, v2 = v0 + r;
                if (u1 < 0) u1 = 0;
                if (v1 < 0) v1 = 0;
                if (u2 > M - 1) u2 = M - 1;
                if (v2 > M - 1) v2 = M - 1;
                if (u1 > u2 || v1 > v2) continue;
                imos[(size_t)u1 * MW + v1] += 1;
                imos[(size_t)(u2 + 1) * MW + v1] -= 1;
                imos[(size_t)u1 * MW + (v2 + 1)] -= 1;
                imos[(size_t)(u2 + 1) * MW + (v2 + 1)] += 1;
            }
            for (int u = 0; u <= M; u++) {
                int rowAcc = 0;
                for (int v = 0; v <= M; v++) {
                    rowAcc += imos[(size_t)u * MW + v];
                    int up = (u > 0) ? imos[(size_t)(u - 1) * MW + v] : 0;
                    imos[(size_t)u * MW + v] = rowAcc + up;
                }
            }
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    int c = CELL(x, y);
                    if (MAP[y][x] == '#') continue;
                    int n = imos[(size_t)(x + y) * MW + (x - y + (H - 1))];
                    if (n > hc) { hc = n; hx = x; hy = y; }
                }
            }
            string s = "REPORT pending=";
            APPEND_INT(s, PENDL.size());
            s += " idle=";  APPEND_INT(s, c0);
            s += " to_pickup="; APPEND_INT(s, c1);
            s += " delivering="; APPEND_INT(s, c2);
            s += " to_charger="; APPEND_INT(s, c3);
            s += " charging="; APPEND_INT(s, c4);
            s += " dead="; APPEND_INT(s, c5);
            s += " hot="; APPEND_INT(s, hx); s += ","; APPEND_INT(s, hy); s += ","; APPEND_INT(s, hc);
            emit(t, s);
        }
    }
}

// ---------------------------------------------------------------- 收尾统计
void Finish() {
    long long open = 0;
    for (size_t i = 0; i < ORD.size(); i++) {
        int st = ORD[i].status;
        if (st == O_PENDING || st == O_ASSIGNED || st == O_PICKED) open++;
    }
    string s = "SUMMARY delivered=";
    APPEND_INT(s, CNT[0]);
    s += " lost=";     APPEND_INT(s, CNT[1]);
    s += " rejected="; APPEND_INT(s, CNT[2]);
    s += " cancelled="; APPEND_INT(s, CNT[3]);
    s += " open=";     APPEND_INT(s, open);
    emitRaw(s);
    long long avg = 0;
    if (CNT[0] > 0) avg = (CNT[4] + CNT[0] / 2) / CNT[0];    // 注意：是四舍五入，SPEC 写的是整除
    s = "LATENCY sum=";
    APPEND_INT(s, CNT[4]);
    s += " max="; APPEND_INT(s, CNT[5]);
    s += " avg="; APPEND_INT(s, avg);
    emitRaw(s);
    for (int i = 0; i < NR; i++) {
        s = "ROBOT " + RNAME(i) + " ";
        APPEND_INT(s, RC[i] % W);
        s += " ";
        APPEND_INT(s, RC[i] / W);
        s += " ";
        APPEND_INT(s, RB[i]);
        s += " ";
        s += STN[RST[i]];
        s += " ";
        APPEND_INT(s, RTR[i]);
        s += " ";
        APPEND_INT(s, RWT[i]);
        emitRaw(s);
    }
}

void dumpLog() {
    fwrite(OUT.data(), 1, OUT.size(), stdout);
}
