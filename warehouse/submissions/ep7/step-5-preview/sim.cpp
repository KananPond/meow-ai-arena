// sim.cpp   仿真主体（性能重写版）
//
// 与基线的阶段顺序完全一致：事件 -> 救援 -> 充电 -> 派单 -> 低电量去充电 -> 移动 -> 报表。
// 每行的输出格式、每个判定条件、每个"看起来像 bug"的行为都按基线原样保留，
// 改动只在"怎么算"（距离表缓存、占用网格、充电桩占用计数、整数化订单表）。
#include "common.h"
#include <iostream>

namespace {

const char* PP = nullptr;   // 输入游标
const char* PE = nullptr;

inline bool isSep(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

// 取下一个空白分隔的 token（与基线的 `in >> x` 等价）
bool nextTok(const char*& s, int& len) {
    while (PP < PE && isSep(*PP)) PP++;
    if (PP >= PE) return false;
    s = PP;
    while (PP < PE && !isSep(*PP)) PP++;
    len = (int)(PP - s);
    return true;
}

// 与 atoi 对合法整数的解析结果一致
long long tokInt(const char* s, int len) {
    int i = 0;
    bool neg = false;
    if (i < len && (s[i] == '-' || s[i] == '+')) { neg = (s[i] == '-'); i++; }
    long long v = 0;
    for (; i < len; i++) {
        char c = s[i];
        if (c < '0' || c > '9') break;
        v = v * 10 + (c - '0');
    }
    return neg ? -v : v;
}

// 把一行按空白切 token（与基线的 SPLIT 等价，只取前 maxTok 个）
int splitLine(const char* b, const char* e, const char* tok[], int lens[], int maxTok) {
    int n = 0;
    const char* p = b;
    while (p < e && n < maxTok) {
        while (p < e && isSep(*p)) p++;
        if (p >= e) break;
        tok[n] = p;
        while (p < e && !isSep(*p)) p++;
        lens[n] = (int)(p - tok[n]);
        n++;
    }
    return n;
}

inline int IDX(int x, int y) { return y * W + x; }
inline bool INMAP(int x, int y) { return x >= 0 && y >= 0 && x < W && y < H; }

const int DX4[4] = {0, 1, 0, -1};
const int DY4[4] = {-1, 0, 1, 0};   // 上 右 下 左  !!!顺序不能改!!!

// 充电桩
vector<int> chgCells;      // 充电桩格子（行优先）
vector<int> chgIdx;        // 格子 -> 充电桩序号，-1 = 不是充电桩
vector<int> chgCnt;        // 每个充电桩"被占用/在途"的数量

// ---------------- 日志助手 ----------------
inline void logPrefix(int t) { outInt(t); outS(" "); }

// 到达目标（基线 3.6 的第 6 步）
void arrival(int i, int t) {
    int st = RST[i];
    RWS[i] = 0;
    if (st == ST_TO_PICKUP) {
        int oi = ROID[i];
        ORD[oi].status = O_PICKED;
        RST[i] = ST_DELIVERING;
        RTX[i] = ORD[oi].dx;
        RTY[i] = ORD[oi].dy;
        logPrefix(t); outS("PICK "); outInt(ORD[oi].id); outS(" R"); outInt(i); outEnd();
    } else if (st == ST_DELIVERING) {
        int oi = ROID[i];
        ORD[oi].status = O_DONE;
        long long lat = t - ORD[oi].arrive;
        CNT[0]++;
        CNT[4] += lat;
        if (lat > CNT[5]) CNT[5] = lat;
        logPrefix(t); outS("DELIVER "); outInt(ORD[oi].id); outS(" R"); outInt(i); outS(" "); outInt(lat); outEnd();
        RST[i] = ST_IDLE; ROID[i] = -1; RTX[i] = -1; RTY[i] = -1;
    } else if (st == ST_TO_CHARGER) {
        // 到桩：它此前已按 TO_CHARGER 计过占用，这里不重复计数
        RST[i] = ST_CHARGING;
        logPrefix(t); outS("CHARGE R"); outInt(i); outEnd();
    }
}

// 停机（基线 3.8）
void killRobot(int i, int t) {
    logPrefix(t); outS("DEAD R"); outInt(i); outEnd();
    int oi = ROID[i];
    if (oi >= 0) {
        if (RST[i] == ST_TO_PICKUP) {
            ORD[oi].status = O_PENDING;
            ORD[oi].robot = -1;
            PendNode pn;
            pn.prio = ORD[oi].prio; pn.arrive = ORD[oi].arrive; pn.id = ORD[oi].id; pn.idx = oi;
            PENDQ.insert(pn);
            logPrefix(t); outS("REQUEUE "); outInt(ORD[oi].id); outEnd();
        } else if (RST[i] == ST_DELIVERING) {
            ORD[oi].status = O_LOST;
            CNT[1]++;
            logPrefix(t); outS("LOST "); outInt(ORD[oi].id); outEnd();
        }
    }
    if (RST[i] == ST_TO_CHARGER) {
        int ci = chgIdx[IDX(RTX[i], RTY[i])];
        if (ci >= 0) chgCnt[ci]--;
    }
    RST[i] = ST_DEAD;
    RDS[i] = t;
    ROID[i] = -1;
    RTX[i] = -1;
    RTY[i] = -1;
    OCCG[IDX(RX[i], RY[i])] = 0;
}

inline void moveTo(int i, int nx, int ny) {
    OCCG[IDX(RX[i], RY[i])] = 0;
    RX[i] = nx;
    RY[i] = ny;
    RB[i]--;
    RTR[i]++;
    OCCG[IDX(nx, ny)] = i + 1;
}

}  // namespace

// ---------------- 读输入 ----------------
void ReadAll(istream& in) {
    string data((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    PP = data.data();
    PE = data.data() + data.size();

    const char* s;
    int len;
    nextTok(s, len); W = (int)tokInt(s, len);
    nextTok(s, len); H = (int)tokInt(s, len);
    nextTok(s, len); T = (int)tokInt(s, len);
    int ncell = W * H;
    MAP.resize(H);
    WALL.assign(ncell, 0);
    BLKF.assign(ncell, 0);
    PASS.assign(ncell, 0);
    for (int y = 0; y < H; y++) {
        nextTok(s, len);
        MAP[y].assign(s, len < W ? len : W);
        for (int x = 0; x < W; x++) {
            char c = (x < len) ? s[x] : '#';
            size_t i = (size_t)y * W + x;
            if (c == '#') WALL[i] = 1; else PASS[i] = 1;
        }
    }
    nextTok(s, len);                       // "PARAMS"
    for (int i = 0; i < 6; i++) { nextTok(s, len); PRM[i] = (int)tokInt(s, len); }
    nextTok(s, len);                       // "ROBOTS"
    nextTok(s, len); NR = (int)tokInt(s, len);
    RX.resize(NR); RY.resize(NR); RB.resize(NR); RTX.resize(NR); RTY.resize(NR);
    RWS.resize(NR); RDS.resize(NR); RWT.resize(NR); RTR.resize(NR);
    RST.assign(NR, ST_IDLE); ROID.assign(NR, -1); RNM.resize(NR);
    OCCG.assign(ncell, 0);
    for (int i = 0; i < NR; i++) {
        nextTok(s, len); int a = (int)tokInt(s, len);
        nextTok(s, len); int b = (int)tokInt(s, len);
        RX[i] = a; RY[i] = b;
        RB[i] = PRM[0];                     // 满电出发
        RTX[i] = -1; RTY[i] = -1;
        RWS[i] = 0; RDS[i] = -1; RWT[i] = 0; RTR[i] = 0;
        RNM[i] = "R" + to_string(i);
        OCCG[b * W + a] = i + 1;
    }
    nextTok(s, len);                       // "EVENTS"
    nextTok(s, len); int ne = (int)tokInt(s, len);
    while (PP < PE && *PP != '\n') PP++;  // 吃掉 "EVENTS ne" 行尾（对齐基线的 getline）
    if (PP < PE) PP++;
    EVT.assign(T, vector<Ev>());
    for (int e = 0; e < ne; e++) {
        const char* lb = PP;
        while (PP < PE && *PP != '\n') PP++;
        const char* le = PP;
        if (PP < PE) PP++;
        if (le > lb && *(le - 1) == '\r') le--;
        const char* tk[8];
        int tl[8];
        int n = splitLine(lb, le, tk, tl, 8);
        if (n < 2) continue;
        int tt = (int)tokInt(tk[0], tl[0]);
        if (tt < 0 || tt >= T) continue;
        Ev ev;
        ev.tick = tt; ev.type = EV_UNKNOWN;
        ev.id = 0; ev.px = ev.py = ev.dx = ev.dy = ev.prio = 0;
        if (tl[1] == 5 && memcmp(tk[1], "ORDER", 5) == 0 && n >= 8) {
            ev.type = EV_ORDER;
            ev.id = tokInt(tk[2], tl[2]);
            ev.px = (int)tokInt(tk[3], tl[3]); ev.py = (int)tokInt(tk[4], tl[4]);
            ev.dx = (int)tokInt(tk[5], tl[5]); ev.dy = (int)tokInt(tk[6], tl[6]);
            ev.prio = (int)tokInt(tk[7], tl[7]);
        } else if (tl[1] == 6 && memcmp(tk[1], "CANCEL", 6) == 0 && n >= 3) {
            ev.type = EV_CANCEL;
            ev.id = tokInt(tk[2], tl[2]);
        } else if (tl[1] == 5 && memcmp(tk[1], "BLOCK", 5) == 0 && n >= 4) {
            ev.type = EV_BLOCK;
            ev.px = (int)tokInt(tk[2], tl[2]); ev.py = (int)tokInt(tk[3], tl[3]);
        } else if (tl[1] == 7 && memcmp(tk[1], "UNBLOCK", 7) == 0 && n >= 4) {
            ev.type = EV_UNBLOCK;
            ev.px = (int)tokInt(tk[2], tl[2]); ev.py = (int)tokInt(tk[3], tl[3]);
        }
        EVT[tt].push_back(ev);
    }

    // 充电桩（行优先）
    chgIdx.assign(ncell, -1);
    chgCells.clear();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (MAP[y][x] == 'C') { chgIdx[IDX(x, y)] = (int)chgCells.size(); chgCells.push_back(IDX(x, y)); }
    chgCnt.assign(chgCells.size(), 0);

    PASSVER = 1;
    long long budget = 288LL << 20;            // 距离表缓存的内存预算（远低于 1GB 上限）
    int cap = (int)(budget / ((long long)ncell * 4));
    if (cap < 32) cap = 32;
    if (cap > 4096) cap = 4096;
    initDcache(ncell, cap);
    OUTBUF.reserve(1 << 20);
}

// ---------------- 主循环 ----------------
void RunSim() {
    const int ncell = W * H;
    vector<int> candI, candA, candIdx;      // 派单候选车 & 每个订单的候选距离
    vector<int> blockedRob;                 // 站在封锁格上的候选车
    vector<vector<int> > blockedTabs;       // 它们的距离表（本 tick 算一次）
    vector<int> blkOf(NR, -1);              // 车 -> blockedTabs 下标
    unordered_set<int> noChg;               // 已知没有可通行充电桩可达的格子
    long long verSeen = PASSVER;
    vector<int> stamp(ncell, 0), cnt(ncell, 0);
    int repStamp = 0;

    for (int t = 0; t < T; t++) {
        // ====================== 1. 事件 ======================
        for (size_t ei = 0; ei < EVT[t].size(); ei++) {
            const Ev& ev = EVT[t][ei];
            if (ev.type == EV_ORDER) {
                if (!INMAP(ev.px, ev.py) || !INMAP(ev.dx, ev.dy) ||
                    WALL[IDX(ev.px, ev.py)] || WALL[IDX(ev.dx, ev.dy)]) {
                    logPrefix(t); outS("REJECT "); outInt(ev.id); outEnd();
                    CNT[2]++;
                    continue;
                }
                Order o;
                o.id = ev.id; o.px = ev.px; o.py = ev.py; o.dx = ev.dx; o.dy = ev.dy;
                o.prio = ev.prio; o.arrive = t; o.status = O_PENDING; o.robot = -1;
                int idx = (int)ORD.size();
                ORD.push_back(o);
                ORDIDX[ev.id] = idx;
                PendNode pn;
                pn.prio = o.prio; pn.arrive = o.arrive; pn.id = o.id; pn.idx = idx;
                PENDQ.insert(pn);
            } else if (ev.type == EV_CANCEL) {
                unordered_map<long long, int>::iterator it2 = ORDIDX.find(ev.id);
                if (it2 == ORDIDX.end()) {
                    logPrefix(t); outS("CANCEL_FAIL "); outInt(ev.id); outEnd();
                    continue;
                }
                CNT[3]++;                      // 注意：编号存在就计数，哪怕取消失败
                int oi = it2->second;
                if (ORD[oi].status == O_PENDING) {
                    PendNode pn;
                    pn.prio = ORD[oi].prio; pn.arrive = ORD[oi].arrive; pn.id = ORD[oi].id; pn.idx = oi;
                    PENDQ.erase(pn);
                    ORD[oi].status = O_CANCELLED;
                    ORD[oi].robot = -1;
                    logPrefix(t); outS("CANCEL "); outInt(ev.id); outEnd();
                } else if (ORD[oi].status == O_ASSIGNED) {
                    int ri = ORD[oi].robot;
                    RST[ri] = ST_IDLE;
                    ROID[ri] = -1;
                    RTX[ri] = -1;
                    RTY[ri] = -1;
                    RWS[ri] = 0;
                    ORD[oi].status = O_CANCELLED;
                    ORD[oi].robot = -1;
                    logPrefix(t); outS("CANCEL "); outInt(ev.id); outEnd();
                } else {
                    logPrefix(t); outS("CANCEL_FAIL "); outInt(ev.id); outEnd();
                }
            } else if (ev.type == EV_BLOCK) {
                if (INMAP(ev.px, ev.py) && !WALL[IDX(ev.px, ev.py)]) {
                    int c = IDX(ev.px, ev.py);
                    if (!BLKF[c]) { BLKF[c] = 1; PASS[c] = 0; PASSVER++; }
                }
            } else if (ev.type == EV_UNBLOCK) {
                if (INMAP(ev.px, ev.py)) {
                    int c = IDX(ev.px, ev.py);
                    if (BLKF[c]) { BLKF[c] = 0; PASS[c] = WALL[c] ? 0 : 1; PASSVER++; }
                }
            }
        }
        dcacheSetVersion(PASSVER);
        if (PASSVER != verSeen) { noChg.clear(); verSeen = PASSVER; }

        // ====================== 2. 救援 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != ST_DEAD) continue;
            if (t - RDS[i] < 100) continue;
            int c = IDX(RX[i], RY[i]);
            if (OCCG[c]) continue;             // 格里还有别的车
            RST[i] = ST_IDLE;
            RB[i] = PRM[0] / 2;                // 注意：基线是半电，不是满电
            RWS[i] = 0;
            RDS[i] = -1;
            OCCG[c] = i + 1;
            logPrefix(t); outS("RESCUE R"); outInt(i); outEnd();
        }

        // ====================== 3. 充电 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != ST_CHARGING) continue;
            RB[i] += PRM[1];
            if (RB[i] > PRM[0]) RB[i] = PRM[0];
            if (RB[i] * 10 >= PRM[0] * 9) {     // 充满了就走（90%）
                RST[i] = ST_IDLE;
                RTX[i] = -1;
                RTY[i] = -1;
                int ci = chgIdx[IDX(RX[i], RY[i])];
                if (ci >= 0) chgCnt[ci]--;
                logPrefix(t); outS("CHARGED R"); outInt(i); outEnd();
            }
        }

        // ====================== 4. 派单 ======================
        {
            // 候选车：IDLE 且电量 >= 低电量阈值
            candI.clear();
            for (int i = 0; i < NR; i++)
                if (RST[i] == ST_IDLE && RB[i] >= PRM[2]) candI.push_back(i);
            // 站在被封锁格子上的候选车：基线会以它自己为源做 BFS，本 tick 预算好备用表
            for (int k = 0; k < NR; k++) blkOf[k] = -1;
            blockedRob.clear();
            for (size_t k = 0; k < candI.size(); k++) {
                int i = candI[k];
                if (!PASS[IDX(RX[i], RY[i])]) {
                    blkOf[i] = (int)blockedRob.size();
                    blockedRob.push_back(i);
                }
            }
            blockedTabs.resize(blockedRob.size());
            for (size_t k = 0; k < blockedRob.size(); k++) {
                blockedTabs[k].resize(ncell);
                bfsFillInto(IDX(RX[blockedRob[k]], RY[blockedRob[k]]), blockedTabs[k].data());
            }

            set<PendNode, PendCmp>::iterator it = PENDQ.begin();
            while (it != PENDQ.end()) {
                int oi = it->idx;
                Order& o = ORD[oi];
                int pc = IDX(o.px, o.py), dc = IDX(o.dx, o.dy);
                bool pPass = PASS[pc];
                const int* tp = nullptr;                 // 取货点距离表（按需取，读 b 之前有效）
                candIdx.clear();
                candA.clear();
                bool plausible = false;
                {
                    for (size_t k = 0; k < candI.size(); k++) {
                        int i = candI[k];
                        if (RST[i] != ST_IDLE) continue;      // 本 tick 已被前面的订单占用
                        int rc = IDX(RX[i], RY[i]);
                        int a;
                        if (rc == pc) {
                            a = 0;                            // 车就在取货点上（哪怕该格被封锁）
                        } else if (!pPass) {
                            continue;                         // 取货点不可通行 -> INF
                        } else if (PASS[rc]) {
                            if (!tp) tp = dcacheGet(pc);
                            a = tp[rc];
                            if (a >= BIGNUM) continue;
                        } else {
                            a = blockedTabs[blkOf[i]][pc];     // 车站在封锁格上：以它为源
                            if (a >= BIGNUM) continue;
                        }
                        if (RB[i] >= (long long)a + PRM[3]) plausible = true;
                        candIdx.push_back(i);
                        candA.push_back(a);
                    }
                    if (plausible) {
                        int b;
                        if (pc == dc) b = 0;
                        else if (!PASS[dc]) b = BIGNUM;
                        else { if (!tp) tp = dcacheGet(pc); b = tp[dc]; }
                        if (b < BIGNUM) {
                            const int* td = dcacheGet(dc);     // 送货点距离表
                            int c = BIGNUM;
                            for (size_t q = 0; q < chgCells.size(); q++) {
                                if (!PASS[chgCells[q]]) continue;
                                int v = td[chgCells[q]];
                                if (v < c) c = v;
                            }
                            if (c < BIGNUM) {
                                int best = BIGNUM, who = -1;
                                for (size_t k = 0; k < candIdx.size(); k++) {
                                    int i = candIdx[k], a = candA[k];
                                    if (RB[i] < (long long)a + b + c + PRM[3]) continue;
                                    if (a <= best) { best = a; who = i; }
                                }
                                if (who >= 0) {
                                    RST[who] = ST_TO_PICKUP;
                                    ROID[who] = oi;
                                    RTX[who] = o.px;
                                    RTY[who] = o.py;
                                    RWS[who] = 0;
                                    o.status = O_ASSIGNED;
                                    o.robot = who;
                                    logPrefix(t); outS("ASSIGN "); outInt(o.id); outS(" R");
                                    outInt(who); outS(" "); outInt(best); outEnd();
                                    it = PENDQ.erase(it);
                                    continue;
                                }
                            }
                        }
                    }
                }
                ++it;
            }
        }

        // ====================== 5. 没电的去充电 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != ST_IDLE) continue;
            if (RB[i] >= PRM[2]) continue;
            int rc = IDX(RX[i], RY[i]);
            if (noChg.find(rc) != noChg.end()) continue;   // 这个格子在当前版本下没有可达充电桩
            const int* d = dcacheGet(rc);
            int best = BIGNUM, ci = -1;
            for (size_t q = 0; q < chgCells.size(); q++) {
                int cell = chgCells[q];
                if (!PASS[cell]) continue;
                if (chgCnt[q]) continue;                  // 被别的车占着/正在过去
                int v = d[cell];
                if (v + PRM[3] > RB[i]) continue;
                if (v < best) { best = v; ci = (int)q; }
            }
            if (ci < 0) {
                // 都被占了/去不起：挑最近的排队，不管占用
                for (size_t q = 0; q < chgCells.size(); q++) {
                    int cell = chgCells[q];
                    if (!PASS[cell]) continue;
                    int v = d[cell];
                    if (v < best) { best = v; ci = (int)q; }
                }
            }
            if (ci < 0) { noChg.insert(rc); continue; }
            RST[i] = ST_TO_CHARGER;
            RTX[i] = chgCells[ci] % W;
            RTY[i] = chgCells[ci] / W;
            RWS[i] = 0;
            chgCnt[ci]++;
            logPrefix(t); outS("GO_CHARGE R"); outInt(i); outS(" ");
            outInt(RTX[i]); outS(" "); outInt(RTY[i]); outEnd();
        }

        // ====================== 6. 走 ======================
        for (int i = 0; i < NR; i++) {
            int st = RST[i];
            if (st != ST_TO_PICKUP && st != ST_DELIVERING && st != ST_TO_CHARGER) continue;
            if (RX[i] == RTX[i] && RY[i] == RTY[i]) {      // 已经在目标上
                RWS[i] = 0;
                arrival(i, t);
                continue;
            }
            if (RB[i] == 0) {                              // 没电了，趴窝
                killRobot(i, t);
                continue;
            }
            int tc = IDX(RTX[i], RTY[i]);
            if (!PASS[tc]) { RWT[i]++; continue; }         // 目标被封了，等着
            const int* d = dcacheGet(tc);                  // 从目标往回搜的距离表
            int best = BIGNUM, bx = -1, by = -1;
            for (int k = 0; k < 4; k++) {
                int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                int nc = IDX(nx, ny);
                if (!PASS[nc]) continue;
                int v = d[nc];
                if (v < best) { best = v; bx = nx; by = ny; }
            }
            if (best >= BIGNUM) { RWT[i]++; continue; }    // 走不过去
            if (OCCG[IDX(bx, by)]) {
                bool moved = false;
                if (RWS[i] >= 4 && !(bx == RTX[i] && by == RTY[i])) {
                    // 等太久了，让一让
                    for (int k = 0; k < 4; k++) {
                        int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                        if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                        int nc = IDX(nx, ny);
                        if (!PASS[nc] || OCCG[nc]) continue;
                        moveTo(i, nx, ny);
                        RWS[i] = 0;
                        logPrefix(t); outS("SIDESTEP R"); outInt(i); outEnd();
                        moved = true;
                        break;
                    }
                }
                if (!moved) { RWT[i]++; RWS[i]++; continue; }
            } else {
                moveTo(i, bx, by);
                RWS[i] = 0;
            }
            if (RX[i] == RTX[i] && RY[i] == RTY[i]) {
                RWS[i] = 0;
                arrival(i, t);
            }
        }

        // ====================== 7. 报表 ======================
        if ((t + 1) % PRM[4] == 0) {
            int c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0, nonDead = 0;
            for (int i = 0; i < NR; i++) {
                switch (RST[i]) {
                    case ST_IDLE: c0++; nonDead++; break;
                    case ST_TO_PICKUP: c1++; nonDead++; break;
                    case ST_DELIVERING: c2++; nonDead++; break;
                    case ST_TO_CHARGER: c3++; nonDead++; break;
                    case ST_CHARGING: c4++; nonDead++; break;
                    default: c5++; break;
                }
            }
            // 热点：曼哈顿距离 <= hotRadius 的非停机车数，最大，并列取行优先靠前
            int hx = -1, hy = -1, hc = -1;
            int r = PRM[5];
            if (r >= (W - 1) + (H - 1)) {
                // 半径足够大：所有非墙格子看到的结果一样
                hc = nonDead;
                for (int c = 0; c < ncell; c++)
                    if (!WALL[c]) { hx = c % W; hy = c / W; break; }
            } else {
                repStamp++;
                for (int i = 0; i < NR; i++) {
                    if (RST[i] == ST_DEAD) continue;
                    int y0 = RY[i] - r; if (y0 < 0) y0 = 0;
                    int y1 = RY[i] + r; if (y1 > H - 1) y1 = H - 1;
                    for (int y = y0; y <= y1; y++) {
                        int rem = r - (RY[i] > y ? RY[i] - y : y - RY[i]);
                        int x0 = RX[i] - rem; if (x0 < 0) x0 = 0;
                        int x1 = RX[i] + rem; if (x1 > W - 1) x1 = W - 1;
                        for (int x = x0; x <= x1; x++) {
                            int c = y * W + x;
                            if (stamp[c] != repStamp) { stamp[c] = repStamp; cnt[c] = 0; }
                            cnt[c]++;
                        }
                    }
                }
                for (int c = 0; c < ncell; c++) {
                    if (WALL[c]) continue;
                    int n = (stamp[c] == repStamp) ? cnt[c] : 0;
                    if (n > hc) { hc = n; hx = c % W; hy = c / W; }
                }
            }
            logPrefix(t);
            outS("REPORT pending="); outInt((long long)PENDQ.size());
            outS(" idle="); outInt(c0);
            outS(" to_pickup="); outInt(c1);
            outS(" delivering="); outInt(c2);
            outS(" to_charger="); outInt(c3);
            outS(" charging="); outInt(c4);
            outS(" dead="); outInt(c5);
            outS(" hot="); outInt(hx); outS(","); outInt(hy); outS(","); outInt(hc);
            outEnd();
        }
    }
}

// 收尾统计
void Finish() {
    long long open = 0;
    for (size_t i = 0; i < ORD.size(); i++) {
        int st = ORD[i].status;
        if (st == O_PENDING || st == O_ASSIGNED || st == O_PICKED) open++;
    }
    outS("SUMMARY delivered="); outInt(CNT[0]);
    outS(" lost="); outInt(CNT[1]);
    outS(" rejected="); outInt(CNT[2]);
    outS(" cancelled="); outInt(CNT[3]);
    outS(" open="); outInt(open);
    outEnd();
    long long avg = 0;
    if (CNT[0] > 0) avg = (CNT[4] + CNT[0] / 2) / CNT[0];   // 平均延迟，整数除法
    outS("LATENCY sum="); outInt(CNT[4]);
    outS(" max="); outInt(CNT[5]);
    outS(" avg="); outInt(avg);
    outEnd();
    for (int i = 0; i < NR; i++) {
        outS("ROBOT "); outS(RNM[i].c_str());
        outS(" "); outInt(RX[i]); outS(" "); outInt(RY[i]); outS(" "); outInt(RB[i]);
        outS(" "); outS(RSTNAME[RST[i]]);
        outS(" "); outInt(RTR[i]); outS(" "); outInt(RWT[i]);
        outEnd();
    }
}
