// sim.cpp   仿真主体
// 2022 老王 初版
// 2023 张   改派单、加救援
// 2024 李   加了报表、改充电（按运营要求）
// 注意：这个文件很重要，改之前跟我说一声 —— 张
#include "common.h"
#include <unordered_map>
struct PendKeyG { int pr, seq, id; };
static std::unordered_map<string, PendKeyG> g_pk;
struct PendKeyF { int pr, seq, id; string sid; };
static vector<PendKeyF> g_ids;   // 与 PEND 同步维护的有序表
static vector<string> g_pendErase;
static bool PK_LESS(const PendKeyF& a, const PendKeyF& b) {
    if (a.pr != b.pr) return a.pr > b.pr;
    if (a.seq != b.seq) return a.seq < b.seq;
    return a.id < b.id;
}
static void PK_INSERT(const string& sid, int pr, int seq, int id) {
    PendKeyF q; q.pr = pr; q.seq = seq; q.id = id; q.sid = sid;
    g_ids.insert(std::lower_bound(g_ids.begin(), g_ids.end(), q, PK_LESS), q);
}
static void PK_ERASE(const string& sid) {
    for (size_t k = 0; k < g_ids.size(); k++) if (g_ids[k].sid == sid) { g_ids.erase(g_ids.begin() + k); return; }
}   // 订单编号 -> 排序键（创建后不变）

extern int DX4[4];
extern int DY4[4];

// 读输入
void ReadAll(istream& in) {
    in >> W >> H >> T;
    MAP.resize(H);
    for (int i = 0; i < H; i++) in >> MAP[i];
    OK_INIT();   // [P15] 平铺可走图
    string tmp;
    in >> tmp;   // "PARAMS"
    for (int i = 0; i < 6; i++) in >> PRM[i];
    in >> tmp >> NR;   // "ROBOTS"
    for (int i = 0; i < NR; i++) {
        int a, b;
        in >> a >> b;
        RX.push_back(a);
        RY.push_back(b);
        RB.push_back(PRM[0]);   // 满电出发
        RTX.push_back(-1);
        RTY.push_back(-1);
        RWS.push_back(0);
        RDS.push_back(-1);
        RWT.push_back(0);
        RTR.push_back(0);
        RST.push_back("IDLE");
        ROID.push_back("");
    }
    int ne = 0;
    in >> tmp >> ne;   // "EVENTS"
    string line;
    getline(in, line);
    for (int i = 0; i < ne; i++) {
        getline(in, line);
        if (line.size() > 0 && line[line.size() - 1] == '\r') line = line.substr(0, line.size() - 1);
        EVT.push_back(line);
    }
    // [P6] 事件只解析一次，按 tick 稳定排序（保留同 tick 内的原始顺序）
    EVP.clear();
    for (int i = 0; i < (int)EVT.size(); i++) {
        vector<string> w = SPLIT(EVT[i]);
        if (w.size() < 2) continue;
        EVP.push_back(make_pair(S2I(w[0]), w));
    }
    stable_sort(EVP.begin(), EVP.end(), [](const pair<int, vector<string> >& a, const pair<int, vector<string> >& b) { return a.first < b.first; });
}

// 主循环 ----------------------------------------------------------------

// ====================== [P17] 单调路径证明 ======================
// 移动只需要"下一步走哪"，不需要整张距离场。
// 若从机器人到目标存在一条每步都让曼哈顿距离减一的路，则 d(机器人)==曼哈顿，
// 且此时"距离最小的邻格"必然也是曼哈顿减一的格子 —— 于是可以只验四个邻居。
// 证明建在 [min..max] x [min..max] 这个矩形里；矩形内既不能有新的封路，也不能有解封，
// 否则整张位图作废重建（解封为什么也算——见下面 PRF_INVALIDATE_AT 的说明）。
struct Proof {
    int tx, ty;
    int minx, maxx, miny, maxy;
    bool ok;      // 位图是否可用
    bool skip;    // 矩形太大，别建了，直接走 BFS
    std::vector<unsigned long long> bits;
    Proof() : tx(-1), ty(-1), minx(0), maxx(-1), miny(0), maxy(-1), ok(false), skip(false) {}
    bool contains(int x, int y) const {
        return x >= minx && x <= maxx && y >= miny && y <= maxy;
    }
    bool reachable(int x, int y) const {
        if (!ok || !contains(x, y)) return false;
        int w = maxx - minx + 1;
        size_t idx = (size_t)(y - miny) * w + (x - minx);
        return (bits[idx >> 6] >> (idx & 63)) & 1ULL;
    }
};
static std::vector<Proof> PRF;

static void PRF_BUILD(int i) {
    if ((int)PRF.size() <= i) PRF.resize((size_t)i + 1);
    Proof& pf = PRF[i];
    int tx = RTX[i], ty = RTY[i];
    pf.ok = false;
    pf.skip = false;
    pf.tx = tx; pf.ty = ty;
    pf.minx = 0; pf.maxx = -1; pf.miny = 0; pf.maxy = -1;
    pf.bits.clear();
    if (!INMAP(tx, ty) || !OK(tx, ty)) return;
    int minx = RX[i] < tx ? RX[i] : tx, maxx = RX[i] < tx ? tx : RX[i];
    int miny = RY[i] < ty ? RY[i] : ty, maxy = RY[i] < ty ? ty : RY[i];
    long long area = (long long)(maxx - minx + 1) * (maxy - miny + 1);
    if (area > 300000) { pf.skip = true; return; }
    pf.minx = minx; pf.maxx = maxx; pf.miny = miny; pf.maxy = maxy;
    int w = maxx - minx + 1;
    pf.bits.assign(((size_t)area + 63) / 64, 0ULL);
    static std::vector<int> q;
    q.clear();
    size_t ti = (size_t)(ty - miny) * w + (tx - minx);
    pf.bits[ti >> 6] |= 1ULL << (ti & 63);
    // 队列里直接存矩形内的相对下标，不要再拿 x*4096+y 打包：
    // 那样等于假设坐标 < 4096，地图一高就静默截断。
    q.push_back((int)ti);
    for (size_t head = 0; head < q.size(); ++head) {
        int cur = q[head];
        int cx = cur % w + minx, cy = cur / w + miny;
        int md = (cx > tx ? cx - tx : tx - cx) + (cy > ty ? cy - ty : ty - cy);
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX4[k], ny = cy + DY4[k];
            if (nx < minx || nx > maxx || ny < miny || ny > maxy) continue;
            if (!OK(nx, ny)) continue;
            int nd = (nx > tx ? nx - tx : tx - nx) + (ny > ty ? ny - ty : ty - ny);
            if (nd != md + 1) continue;
            size_t idx = (size_t)(ny - miny) * w + (nx - minx);
            if ((pf.bits[idx >> 6] >> (idx & 63)) & 1ULL) continue;
            pf.bits[idx >> 6] |= 1ULL << (idx & 63);
            q.push_back((int)idx);
        }
    }
    pf.ok = true;      // 构建成功——这行被清理脚本误删过一次，别再和计数器写一行
}

// 封路与解封都要作废：
//   封路——位图里的路可能已经断了；
//   解封——更多格子变得可达，旧位图会漏掉它们。而"漏掉"不是保守，是错：
//         两个邻格距离相同时，基线取先出现的那一个，漏标就会选中后面那个。
static void PRF_INVALIDATE_AT(int x, int y) {
    for (size_t i = 0; i < PRF.size(); i++) {
        Proof& p = PRF[i];
        if (p.maxx >= p.minx && p.contains(x, y)) { p.ok = false; }
    }
}

void RunSim() {
    int ep = 0;
    for (int t = 0; t < T; t++) {
        BFS2_PRUNE_AT_TICK();   // [P12]
        // ====================== 1. 事件 ======================
        // [P6] 指针推进：只处理本 tick 的事件
        for (; ep < (int)EVP.size() && EVP[ep].first == t; ep++) {
            const vector<string>& w = EVP[ep].second;
            string typ = w[1];
            if (typ == "ORDER") {
                string id = w[2];
                int px = S2I(w[3]), py = S2I(w[4]), dx = S2I(w[5]), dy = S2I(w[6]);
                int pr = S2I(w[7]);
                bool good = true;
                if (!INMAP(px, py)) good = false;
                if (good && !INMAP(dx, dy)) good = false;
                if (good && ISWALL(px, py)) good = false;
                if (good && ISWALL(dx, dy)) good = false;
                if (!good) {
                    writeLog(t, "REJECT " + id);
                    continue;
                }
                vector<string> f;
                f.push_back(I2S(px));
                f.push_back(I2S(py));
                f.push_back(I2S(dx));
                f.push_back(I2S(dy));
                f.push_back(I2S(pr));
                f.push_back(I2S(t));
                f.push_back("PENDING");
                f.push_back("");
                ORD[id] = f;
                PEND.push_back(id);
                { PendKeyG kk; kk.pr = pr; kk.seq = t; kk.id = S2I(id); g_pk[id] = kk; PK_INSERT(id, pr, t, kk.id); }
            } else if (typ == "CANCEL") {
                string id = w[2];
                if (ORD.find(id) == ORD.end()) {
                    writeLog(t, "CANCEL_FAIL " + id);
                    continue;
                }
                CNT[3]++;
                if (ORD[id][6] == "PENDING") {
                    // 从待派里删掉
                    vector<string> np;
                    for (int k = 0; k < (int)PEND.size(); k++)
                        if (PEND[k] != id) np.push_back(PEND[k]);
                    PEND = np;
                    PK_ERASE(id);
                    ORD[id][6] = "CANCELLED";
                    ORD[id][7] = "";
                    writeLog(t, "CANCEL " + id);
                } else if (ORD[id][6] == "ASSIGNED") {
                    int ri = findRobot(ORD[id][7]);
                    RST[ri] = "IDLE";
                    ROID[ri] = "";
                    RTX[ri] = -1;
                    RTY[ri] = -1;
                    RWS[ri] = 0;
                    ORD[id][6] = "CANCELLED";
                    ORD[id][7] = "";
                    writeLog(t, "CANCEL " + id);
                } else {
                    // 已经取货了 / 已经送完了，取消不了
                    writeLog(t, "CANCEL_FAIL " + id);
                }
            } else if (typ == "BLOCK") {
                int x = S2I(w[2]), y = S2I(w[3]);
                if (INMAP(x, y)) {
                    if (!ISWALL(x, y)) {
                        BLK.insert(make_pair(x, y));
                        OK_SET_BLK(x, y, true);   // [P15]
                        PRF_INVALIDATE_AT(x, y);  // [P17]
                        BLK_VER++;
                    }
                }
            } else if (typ == "UNBLOCK") {
                int x = S2I(w[2]), y = S2I(w[3]);
                if (INMAP(x, y)) {
                    if (BLK.count(make_pair(x, y))) { BLK.erase(make_pair(x, y)); OK_SET_BLK(x, y, false); BLK_VER++; PRF_INVALIDATE_AT(x, y); }
                }
            } else {
                // 不认识的事件，忽略
            }
        }

        // ====================== 2. 救援 ======================
        // 停机 100 tick 以后派人去救，只回一半电（PRM[0]/2）。
        // 原版这里写的是"充满电恢复"——注释在骗人，代码只给一半。
        for (int i = 0; i < NR; i++) {
            if (RST[i] == "DEAD") {
                if (t - RDS[i] >= 100) {
                    if (!OCC(RX[i], RY[i], i)) {
                        RST[i] = "IDLE";
                        RB[i] = PRM[0] / 2;
                        RWS[i] = 0;
                        RDS[i] = -1;
                        writeLog(t, "RESCUE " + RNAME(i));
                    }
                }
            }
        }

        // ====================== 3. 充电 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "CHARGING") continue;
            RB[i] = RB[i] + PRM[1];
            if (RB[i] > PRM[0]) RB[i] = PRM[0];
            if (RB[i] * 10 >= PRM[0] * 9) {   // 充满了就走
                RST[i] = "IDLE";
                RTX[i] = -1;
                RTY[i] = -1;
                writeLog(t, "CHARGED " + RNAME(i));
            }
        }

        // ====================== 4. 派单 ======================
#if USE_NEW_DISPATCH
        {
            // [P11] 一整个 tick 没有候选车：排序和逐单都不必做
            bool anyCandNow = false;
            for (int i = 0; i < NR; i++) {
                if (RST[i] == "IDLE" && RB[i] >= PRM[2]) { anyCandNow = true; break; }
            }
            g_pendErase.clear();
            int ready = 0;
            for (int i = 0; i < NR; i++) if (RST[i] == "IDLE" && RB[i] >= PRM[2]) ready++;
            for (int oi = 0; oi < (int)g_ids.size(); oi++) {
                if (ready <= 0) break;      // 没有能接单的车了，后面排得再前也没用
                string id = g_ids[oi].sid;
                int px = S2I(ORD[id][0]), py = S2I(ORD[id][1]);
                int dx = S2I(ORD[id][2]), dy = S2I(ORD[id][3]);
                int best = BIGNUM, who = -1;
                // [P7] 没有候选车就整单跳过
                bool anyCand = false;
                for (int i = 0; i < NR; i++) {
                    if (RST[i] == "IDLE" && RB[i] >= PRM[2]) { anyCand = true; break; }
                }
                if (!anyCand) continue;
                int c = CHG_DIST(dx, dy);   // [P7] 先算 c：它可能插入新缓存项（甚至触发清表），必须在取 fa 之前
                // [P7] 一张订单只算一次“从取货点出发”的距离场，a 和 b 都从它读
                const vector<int>* fa = OK(px, py) ? &BFS2_FLAT(px, py) : 0;
                int b;
                if (px == dx && py == dy) b = 0;
                else if (!OK(dx, dy)) b = BIGNUM;
                else if (fa) b = (*fa)[(size_t)dy * W + dx];
                else b = DIST(px, py, dx, dy);
                if (b < BIGNUM) {
                    if (c < BIGNUM) {
                        for (int i = 0; i < NR; i++) {
                            if (RST[i] != "IDLE") continue;
                            if (RB[i] < PRM[2]) continue;
                            int a;
                            if (RX[i] == px && RY[i] == py) a = 0;
                            else if (!fa) a = BIGNUM;
                            else if (OK(RX[i], RY[i])) a = (*fa)[(size_t)RY[i] * W + RX[i]];
                            else {
                                // 车站在不可通行格：baseline 起点不检查，仍可朝 OK 邻居出发
                                int mn = BIGNUM;
                                for (int k = 0; k < 4; k++) {
                                    int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                                    if (!OK(nx, ny)) continue;
                                    int v = (*fa)[(size_t)ny * W + nx];
                                    if (v < BIGNUM && v + 1 < mn) mn = v + 1;
                                }
                                a = mn;
                            }
                            if (a >= BIGNUM) continue;
                            if (RB[i] >= a + b + c + PRM[3]) {
                                // 选最近的；一样近选编号**大**的（基线是 <=，按编号升序遍历时后者胜出）
                                if (a <= best) {
                                    best = a;
                                    who = i;
                                }
                            }
                        }
                    }
                }
                if (who == -1) continue;
                ready--;
                RST[who] = "TO_PICKUP";
                ROID[who] = id;
                RTX[who] = px;
                RTY[who] = py;
                RWS[who] = 0;
                ORD[id][6] = "ASSIGNED";
                ORD[id][7] = RNAME(who);
                vector<string> np;
                for (int k = 0; k < (int)PEND.size(); k++)
                    if (PEND[k] != id) np.push_back(PEND[k]);
                PEND = np;
                g_pendErase.push_back(id);   // 遍历 g_ids 时不能删：先记下，循环结束再删
                writeLog(t, "ASSIGN " + id + " " + RNAME(who) + " " + I2S(best));
            }
            for (size_t z = 0; z < g_pendErase.size(); z++) PK_ERASE(g_pendErase[z]);   // 循环结束再删
        }
#else
        oldDispatch(t);
#endif

        // ====================== 5. 没电的去充电 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "IDLE") continue;
            if (RB[i] >= PRM[2]) continue;
            const vector<int>& d = BFS_FLAT_SCRATCH(RX[i], RY[i]);   // [P10] 不缓存，零分配
            vector<pair<int, int> > cs = ALL_CHG();
            int best = BIGNUM;
            int cx = -1, cy = -1;
            for (int k = 0; k < (int)cs.size(); k++) {
                int x = cs[k].first, y = cs[k].second;
                if (!OK(x, y)) continue;
                bool used = false;
                for (int j = 0; j < NR; j++) {
                    if (j == i) continue;
                    if (RST[j] == "CHARGING" && RX[j] == x && RY[j] == y) used = true;
                    if (RST[j] == "TO_CHARGER" && RTX[j] == x && RTY[j] == y) used = true;
                }
                if (used) continue;
                if (d[(size_t)y * W + x] + PRM[3] > RB[i]) continue;
                if (d[(size_t)y * W + x] < best) {
                    best = d[(size_t)y * W + x];
                    cx = x;
                    cy = y;
                }
            }
            if (cx < 0) {
                // 都被占了，那就去最近的排队
                pair<int, int> p = CHG_NEAR(RX[i], RY[i]);
                cx = p.first;
                cy = p.second;
            }
            if (cx < 0) continue;
            RST[i] = "TO_CHARGER";
            RTX[i] = cx;
            RTY[i] = cy;
            RWS[i] = 0;
            writeLog(t, "GO_CHARGE " + RNAME(i) + " " + I2S(cx) + " " + I2S(cy));
        }

        // ====================== 6. 走 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "TO_PICKUP" && RST[i] != "DELIVERING" && RST[i] != "TO_CHARGER") continue;
            if (RX[i] == RTX[i] && RY[i] == RTY[i]) {
                // 已经在目标上了
                RWS[i] = 0;
                if (RST[i] == "TO_PICKUP") {
                    string oid = ROID[i];
                    ORD[oid][6] = "PICKED";
                    RST[i] = "DELIVERING";
                    RTX[i] = S2I(ORD[oid][2]);
                    RTY[i] = S2I(ORD[oid][3]);
                    writeLog(t, "PICK " + oid + " " + RNAME(i));
                } else if (RST[i] == "DELIVERING") {
                    string oid = ROID[i];
                    ORD[oid][6] = "DONE";
                    int lat = t - S2I(ORD[oid][5]);
                    writeLog(t, "DELIVER " + oid + " " + RNAME(i) + " " + I2S(lat));
                    RST[i] = "IDLE";
                    ROID[i] = "";
                    RTX[i] = -1;
                    RTY[i] = -1;
                } else if (RST[i] == "TO_CHARGER") {
                    RST[i] = "CHARGING";
                    writeLog(t, "CHARGE " + RNAME(i));
                }
                continue;
            }
            if (RB[i] == 0) {
                // 没电了，趴窝
                writeLog(t, "DEAD " + RNAME(i));
                if (ROID[i] != "") {
                    string oid = ROID[i];
                    if (RST[i] == "TO_PICKUP") {
                        ORD[oid][6] = "PENDING";
                        ORD[oid][7] = "";
                        PEND.push_back(oid);
                        { std::unordered_map<string, PendKeyG>::const_iterator itk = g_pk.find(oid);
                          if (itk != g_pk.end()) PK_INSERT(oid, itk->second.pr, itk->second.seq, itk->second.id); }
                        writeLog(t, "REQUEUE " + oid);
                    } else if (RST[i] == "DELIVERING") {
                        ORD[oid][6] = "LOST";
                        writeLog(t, "LOST " + oid);
                    }
                }
                RST[i] = "DEAD";
                RDS[i] = t;
                ROID[i] = "";
                RTX[i] = -1;
                RTY[i] = -1;
                continue;
            }
            if (!OK(RTX[i], RTY[i])) {
                RWT[i]++;   // 目标被封了，等着
                continue;
            }
            // [P17] 先试单调路径证明；证不出来才回退全量 BFS
            if ((int)PRF.size() < NR) PRF.resize((size_t)NR);
            Proof& pf = PRF[i];
            bool have = pf.ok && pf.tx == RTX[i] && pf.ty == RTY[i] && pf.contains(RX[i], RY[i]);
            if (!have && !(pf.skip && pf.tx == RTX[i] && pf.ty == RTY[i])) PRF_BUILD(i);
            int best = BIGNUM, bx = -1, by = -1;
            if (pf.ok && pf.reachable(RX[i], RY[i])) {
                int mdr = (RX[i] > RTX[i] ? RX[i] - RTX[i] : RTX[i] - RX[i]) +
                          (RY[i] > RTY[i] ? RY[i] - RTY[i] : RTY[i] - RY[i]);
                for (int k = 0; k < 4; k++) {
                    int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                    if (!OK(nx, ny)) continue;
                    int nd = (nx > RTX[i] ? nx - RTX[i] : RTX[i] - nx) +
                             (ny > RTY[i] ? ny - RTY[i] : RTY[i] - ny);
                    if (nd != mdr - 1) continue;
                    if (!pf.reachable(nx, ny)) continue;
                    best = nd;
                    bx = nx;
                    by = ny;
                    break;
                }
            }
            if (bx < 0) {
                int nbb = DETOUR_STEP(RX[i], RY[i], RTX[i], RTY[i]);   // [P20]
                if (nbb >= 0) { bx = nbb % W; by = nbb / W; best = 1; }
            }
            if (bx < 0) {
                const vector<int>& d = BFS2_FLAT(RTX[i], RTY[i]);
                best = BIGNUM;
                for (int k = 0; k < 4; k++) {
                    int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                    if (!OK(nx, ny)) continue;
                    if (d[(size_t)ny * W + nx] < best) {
                        best = d[(size_t)ny * W + nx];
                        bx = nx;
                        by = ny;
                    }
                }
            }
            if (best >= BIGNUM) {
                RWT[i]++;   // 走不过去
                continue;
            }
            if (OCC(bx, by, i)) {
                bool moved = false;
                if (RWS[i] >= 4 && !(bx == RTX[i] && by == RTY[i])) {
                    // 等太久了，让一让
                    for (int k = 0; k < 4; k++) {
                        int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                        if (OK(nx, ny) && !OCC(nx, ny, i)) {
                            RX[i] = nx;
                            RY[i] = ny;
                            RB[i]--;
                            RTR[i]++;
                            RWS[i] = 0;
                            writeLog(t, "SIDESTEP " + RNAME(i));
                            moved = true;
                            break;
                        }
                    }
                }
                if (moved == false) {
                    RWT[i]++;
                    RWS[i]++;
                    continue;
                }
            } else {
                RX[i] = bx;
                RY[i] = by;
                RB[i]--;
                RTR[i]++;
                RWS[i] = 0;
            }
            // 走完看看到没到
            if (RX[i] == RTX[i] && RY[i] == RTY[i]) {
                RWS[i] = 0;
                if (RST[i] == "TO_PICKUP") {
                    string oid = ROID[i];
                    ORD[oid][6] = "PICKED";
                    RST[i] = "DELIVERING";
                    RTX[i] = S2I(ORD[oid][2]);
                    RTY[i] = S2I(ORD[oid][3]);
                    writeLog(t, "PICK " + oid + " " + RNAME(i));
                } else if (RST[i] == "DELIVERING") {
                    string oid = ROID[i];
                    ORD[oid][6] = "DONE";
                    int lat = t - S2I(ORD[oid][5]);
                    writeLog(t, "DELIVER " + oid + " " + RNAME(i) + " " + I2S(lat));
                    RST[i] = "IDLE";
                    ROID[i] = "";
                    RTX[i] = -1;
                    RTY[i] = -1;
                } else if (RST[i] == "TO_CHARGER") {
                    RST[i] = "CHARGING";
                    writeLog(t, "CHARGE " + RNAME(i));
                }
            }
        }

        // ====================== 7. 报表 ======================
        if ((t + 1) % PRM[4] == 0) {
            int c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
            for (int i = 0; i < NR; i++) {
                if (RST[i] == "IDLE") c0++;
                if (RST[i] == "TO_PICKUP") c1++;
                if (RST[i] == "DELIVERING") c2++;
                if (RST[i] == "TO_CHARGER") c3++;
                if (RST[i] == "CHARGING") c4++;
                if (RST[i] == "DEAD") c5++;
            }
            // 热点（李: 运营要看哪里堵）
            int hx = -1, hy = -1, hc = -1;
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    if (MAP[y][x] == '#') continue;
                    int n = 0;
                    for (int i = 0; i < NR; i++) {
                        if (RST[i] == "DEAD") continue;
                        if (abs(RX[i] - x) + abs(RY[i] - y) <= PRM[5]) n++;
                    }
                    if (n > hc) {
                        hc = n;
                        hx = x;
                        hy = y;
                    }
                }
            }
            string s = "REPORT pending=" + I2S(PEND.size()) + " idle=" + I2S(c0) + " to_pickup=" + I2S(c1) +
                       " delivering=" + I2S(c2) + " to_charger=" + I2S(c3) + " charging=" + I2S(c4) +
                       " dead=" + I2S(c5) + " hot=" + I2S(hx) + "," + I2S(hy) + "," + I2S(hc);
            writeLog(t, s);
        }
    }
}

// 收尾统计
void Finish() {


    long long open = 0;
    for (map<string, vector<string> >::iterator it = ORD.begin(); it != ORD.end(); ++it) {
        string st = it->second[6];
        if (st == "PENDING" || st == "ASSIGNED" || st == "PICKED") open++;
    }
    writeRaw("SUMMARY delivered=" + I2S(CNT[0]) + " lost=" + I2S(CNT[1]) + " rejected=" + I2S(CNT[2]) +
             " cancelled=" + I2S(CNT[3]) + " open=" + I2S(open));
    long long avg = 0;
    if (CNT[0] > 0) avg = (CNT[4] + CNT[0] / 2) / CNT[0];   // 平均延迟，整数除法
    writeRaw("LATENCY sum=" + I2S(CNT[4]) + " max=" + I2S(CNT[5]) + " avg=" + I2S(avg));
    for (int i = 0; i < NR; i++) {
        writeRaw("ROBOT " + RNAME(i) + " " + I2S(RX[i]) + " " + I2S(RY[i]) + " " + I2S(RB[i]) + " " + RST[i] + " " +
                 I2S(RTR[i]) + " " + I2S(RWT[i]));
    }
}
