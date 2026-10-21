// sim.cpp   仿真主体（优化版）
// 相比基线的关键优化：
//   1) 事件处理：基线每 tick 扫描全部事件（O(T*E)），改为按时间指针一次性推进（O(T+E)）。
//      事件在输入中时间非递减（合法性检查保证），在 tick t 只会执行时间==t 的事件，顺序与原循环一致。
//   2) 派单：排序键一次解析；每单只算一次 b=DIST(pickup,delivery)、c=CHG_DIST(delivery)
//      （原代码在“每机器人×每订单”里重复计算，这两个量其实与机器人无关）。
//   3) 移动：BFS2 来自目标点，走同一个目标的机器人共享缓存。
//   4) 低电量去充电：占用/目标计数 O(1) 判断充电桩是否被占，不再每桩全表扫描。
//   5) 报表热点：把曼哈顿菱形计数用 (u=x+y, v=x-y) 旋转 + 2D 差分/前缀和，O(NR+W*H) 替代 O(W*H*NR)。
// 行为保持：所有决策、日志、统计与基线逐字节一致。
#include "common.h"

extern int DX4[4];
extern int DY4[4];

struct Event {
    int t;                // 首 token 解析出的时间（atoi 语义，与原 S2I(w[0]) 一致）
    vector<string> tok;   // 拆好的 token
};
static vector<Event> g_evt;

// 充电桩占用/目标计数（与基线 OCC 式 used 检查等价）
static vector<unsigned short> g_chgOcc;    // CHARGING 且正停在该格的机器人数量
static vector<unsigned short> g_chgTgt;    // TO_CHARGER 且目标为该格的机器人数量

void ReadAll(istream& in) {
    in >> W >> H >> T;
    MAP.resize(H);
    for (int i = 0; i < H; i++) in >> MAP[i];

    blockedIdx.assign((size_t)W * H, 0);
    blkVersion = 0;

    string tmp;
    in >> tmp;   // "PARAMS"
    for (int i = 0; i < 6; i++) in >> PRM[i];

    in >> tmp >> NR;   // "ROBOTS"
    for (int i = 0; i < NR; i++) {
        int a, b;
        in >> a >> b;
        RX.push_back(a);
        RY.push_back(b);
        RB.push_back(PRM[0]);
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
    g_evt.clear();
    g_evt.reserve(ne);
    for (int i = 0; i < ne; i++) {
        getline(in, line);
        if (!line.empty() && line[line.size() - 1] == '\r') line.pop_back();
        EVT.push_back(line);
        Event ev;
        ev.tok = SPLIT(line);
        ev.t = ev.tok.empty() ? 0 : S2I(ev.tok[0]);
        g_evt.push_back(std::move(ev));
    }
}

// ---------- 派单（保持原排序与挑选逻辑） ----------
static void doDispatch(int t) {
    if (PEND.empty()) return;
    struct K { const string* id; int pr, arr, num; };
    vector<K> ks;
    ks.reserve(PEND.size());
    for (auto it = PEND.begin(); it != PEND.end(); ++it) {
        const string& id = *it;
        const vector<string>& o = ORD[id];
        ks.push_back({&id, S2I(o[4]), S2I(o[5]), S2I(id)});
    }
    sort(ks.begin(), ks.end(), [](const K& a, const K& b) {
        if (a.pr != b.pr) return a.pr > b.pr;
        if (a.arr != b.arr) return a.arr < b.arr;
        return a.num < b.num;
    });

    for (const K& k : ks) {
        const string& id = *k.id;
        vector<string>& o = ORD[id];
        int px = S2I(o[0]), py = S2I(o[1]), dx = S2I(o[2]), dy = S2I(o[3]);

        // b、c 只与订单有关，提到循环外
        int b = distTo(px, py, dx, dy);
        if (b >= BIGNUM) continue;
        int c = chgDist(dx, dy);
        if (c >= BIGNUM) continue;

        int best = BIGNUM, who = -1;
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "IDLE") continue;
            if (RB[i] < PRM[2]) continue;
            int a = distTo(RX[i], RY[i], px, py);
            if (a >= BIGNUM) continue;
            if (RB[i] < a + b + c + PRM[3]) continue;
            if (a <= best) { best = a; who = i; }   // 保持基线“<=”：同距取编号更大者
        }
        if (who < 0) continue;

        RST[who] = "TO_PICKUP";
        ROID[who] = id;
        RTX[who] = px;
        RTY[who] = py;
        RWS[who] = 0;
        o[6] = "ASSIGNED";
        o[7] = RNAME(who);
        PEND.erase(id);
        writeLog(t, "ASSIGN " + id + " " + RNAME(who) + " " + I2S(best));
    }
}

void RunSim() {
    occInit();
    g_chgOcc.assign((size_t)W * H, 0);
    g_chgTgt.assign((size_t)W * H, 0);

    // 报表热点差分板（一次性分配复用）
    int GU = W + H - 1, G1 = GU + 1;
    vector<int> diffD((size_t)G1 * G1, 0);

    size_t ei = 0;
    for (int t = 0; t < T; t++) {
        // ============ 1. 事件（指针推进，等效全扫描） ============
        while (ei < g_evt.size() && g_evt[ei].t == t) {
            Event& ev = g_evt[ei++];
            vector<string>& w = ev.tok;
            if (w.size() < 2) continue;
            const string& typ = w[1];
            if (typ == "ORDER") {
                const string& id = w[2];
                int px = S2I(w[3]), py = S2I(w[4]);
                int dx = S2I(w[5]), dy = S2I(w[6]);
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
                ORD[id] = std::move(f);
                PEND.insert(id);
            } else if (typ == "CANCEL") {
                const string& id = w[2];
                auto it = ORD.find(id);
                if (it == ORD.end()) {
                    writeLog(t, "CANCEL_FAIL " + id);
                    continue;
                }
                CNT[3]++;
                if (it->second[6] == "PENDING") {
                    PEND.erase(id);
                    it->second[6] = "CANCELLED";
                    it->second[7] = "";
                    writeLog(t, "CANCEL " + id);
                } else if (it->second[6] == "ASSIGNED") {
                    int ri = findRobot(it->second[7]);
                    RST[ri] = "IDLE";
                    ROID[ri] = "";
                    RTX[ri] = -1;
                    RTY[ri] = -1;
                    RWS[ri] = 0;
                    it->second[6] = "CANCELLED";
                    it->second[7] = "";
                    writeLog(t, "CANCEL " + id);
                } else {
                    writeLog(t, "CANCEL_FAIL " + id);
                }
            } else if (typ == "BLOCK") {
                int x = S2I(w[2]), y = S2I(w[3]);
                if (INMAP(x, y)) {
                    if (!ISWALL(x, y)) {
                        int idx = y * W + x;
                        if (!blockedIdx[idx]) { blockedIdx[idx] = 1; blkVersion++; }
                    }
                }
            } else if (typ == "UNBLOCK") {
                int x = S2I(w[2]), y = S2I(w[3]);
                if (INMAP(x, y)) {
                    int idx = y * W + x;
                    if (blockedIdx[idx]) { blockedIdx[idx] = 0; blkVersion++; }
                }
            }
            // 其它未知类型：忽略
        }

        // ============ 2. 救援 ============
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "DEAD") continue;
            if (t - RDS[i] < 100) continue;
            if (OCC(RX[i], RY[i], i)) continue;
            RST[i] = "IDLE";
            RB[i] = PRM[0] / 2;
            RWS[i] = 0;
            RDS[i] = -1;
            occAdd(i);
            writeLog(t, "RESCUE " + RNAME(i));
        }

        // ============ 3. 充电 ============
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "CHARGING") continue;
            RB[i] += PRM[1];
            if (RB[i] > PRM[0]) RB[i] = PRM[0];
            if (RB[i] * 10 >= PRM[0] * 9) {
                int ci = RY[i] * W + RX[i];
                g_chgOcc[ci]--;
                RST[i] = "IDLE";
                RTX[i] = -1;
                RTY[i] = -1;
                writeLog(t, "CHARGED " + RNAME(i));
            }
        }

        // ============ 4. 派单 ============
        doDispatch(t);

        // ============ 5. 低电量去充电 ============
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "IDLE") continue;
            if (RB[i] >= PRM[2]) continue;
            const vector<unsigned short>& d = bfsRef(RX[i], RY[i]);
            const auto& cs = allChargers();
            int best = BIGNUM, cx = -1, cy = -1;
            for (const auto& p : cs) {
                int x = p.first, y = p.second;
                if (!OK(x, y)) continue;
                int ci = y * W + x;
                if (g_chgOcc[ci] + g_chgTgt[ci] > 0) continue;
                unsigned short v = d[ci];
                if (v == INF16) continue;
                if ((int)v + PRM[3] > RB[i]) continue;
                if ((int)v < best) { best = v; cx = x; cy = y; }
            }
            if (cx < 0) {
                pair<int, int> p = chgNear(RX[i], RY[i]);
                cx = p.first; cy = p.second;
            }
            if (cx < 0) continue;
            RST[i] = "TO_CHARGER";
            RTX[i] = cx;
            RTY[i] = cy;
            RWS[i] = 0;
            g_chgTgt[cy * W + cx]++;
            writeLog(t, "GO_CHARGE " + RNAME(i) + " " + I2S(cx) + " " + I2S(cy));
        }

        // ============ 6. 移动 ============
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "TO_PICKUP" && RST[i] != "DELIVERING" && RST[i] != "TO_CHARGER") continue;

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
                    int ci = RY[i] * W + RX[i];
                    g_chgTgt[ci]--;
                    g_chgOcc[ci]++;
                    RST[i] = "CHARGING";
                    writeLog(t, "CHARGE " + RNAME(i));
                }
                continue;
            }

            if (RB[i] == 0) {
                writeLog(t, "DEAD " + RNAME(i));
                if (ROID[i] != "") {
                    string oid = ROID[i];
                    if (RST[i] == "TO_PICKUP") {
                        ORD[oid][6] = "PENDING";
                        ORD[oid][7] = "";
                        PEND.insert(oid);
                        writeLog(t, "REQUEUE " + oid);
                    } else if (RST[i] == "DELIVERING") {
                        ORD[oid][6] = "LOST";
                        writeLog(t, "LOST " + oid);
                    }
                }
                if (RST[i] == "TO_CHARGER") g_chgTgt[RTY[i] * W + RTX[i]]--;
                occSub(i);
                RST[i] = "DEAD";
                RDS[i] = t;
                ROID[i] = "";
                RTX[i] = -1;
                RTY[i] = -1;
                continue;
            }

            if (!OK(RTX[i], RTY[i])) {
                RWT[i]++;
                continue;
            }

            const vector<unsigned short>& d = bfsRef(RTX[i], RTY[i]);
            int best = BIGNUM, bx = -1, by = -1;
            for (int k = 0; k < 4; k++) {
                int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                if (!OK(nx, ny)) continue;
                unsigned short v = d[(size_t)ny * W + nx];
                if (v == INF16) continue;
                if ((int)v < best) { best = v; bx = nx; by = ny; }
            }
            if (best >= BIGNUM) {
                RWT[i]++;
                continue;
            }

            if (OCC(bx, by, i)) {
                bool moved = false;
                if (RWS[i] >= 4 && !(bx == RTX[i] && by == RTY[i])) {
                    for (int k = 0; k < 4; k++) {
                        int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                        if (OK(nx, ny) && !OCC(nx, ny, i)) {
                            occSub(i);
                            RX[i] = nx;
                            RY[i] = ny;
                            occAdd(i);
                            RB[i]--;
                            RTR[i]++;
                            RWS[i] = 0;
                            writeLog(t, "SIDESTEP " + RNAME(i));
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
                occSub(i);
                RX[i] = bx;
                RY[i] = by;
                occAdd(i);
                RB[i]--;
                RTR[i]++;
                RWS[i] = 0;
            }

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
                    int ci = RY[i] * W + RX[i];
                    g_chgTgt[ci]--;
                    g_chgOcc[ci]++;
                    RST[i] = "CHARGING";
                    writeLog(t, "CHARGE " + RNAME(i));
                }
            }
        }

        // ============ 7. 报表 ============
        if ((t + 1) % PRM[4] == 0) {
            int c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
            for (int i = 0; i < NR; i++) {
                const string& st = RST[i];
                if (st == "IDLE") c0++;
                else if (st == "TO_PICKUP") c1++;
                else if (st == "DELIVERING") c2++;
                else if (st == "TO_CHARGER") c3++;
                else if (st == "CHARGING") c4++;
                else if (st == "DEAD") c5++;
            }

            // 热点：旋转坐标 (u=x+y, v=x-y) + 2D 差分前缀和
            int r = PRM[5];
            fill(diffD.begin(), diffD.end(), 0);
            for (int i = 0; i < NR; i++) {
                if (RST[i] == "DEAD") continue;
                int u0 = RX[i] + RY[i];
                int v0 = RX[i] - RY[i] + H - 1;
                int u1 = u0 - r; if (u1 < 0) u1 = 0;
                int u2 = u0 + r; if (u2 > GU - 1) u2 = GU - 1;
                int v1 = v0 - r; if (v1 < 0) v1 = 0;
                int v2 = v0 + r; if (v2 > GU - 1) v2 = GU - 1;
                diffD[u1 * G1 + v1] += 1;
                diffD[(u2 + 1) * G1 + v1] -= 1;
                diffD[u1 * G1 + v2 + 1] -= 1;
                diffD[(u2 + 1) * G1 + v2 + 1] += 1;
            }
            for (int u = 0; u < GU; u++) {
                int ru = u * G1;
                for (int v = 0; v < GU; v++) {
                    int val = diffD[ru + v];
                    if (u > 0) val += diffD[ru - G1 + v];
                    if (v > 0) val += diffD[ru + v - 1];
                    if (u > 0 && v > 0) val -= diffD[ru - G1 + v - 1];
                    diffD[ru + v] = val;
                }
            }
            int hx = -1, hy = -1, hc = -1;
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    if (MAP[y][x] == '#') continue;
                    int n = diffD[(x + y) * G1 + (x - y + H - 1)];
                    if (n > hc) { hc = n; hx = x; hy = y; }
                }
            }

            string s = "REPORT pending=" + I2S(PEND.size()) + " idle=" + I2S(c0) +
                       " to_pickup=" + I2S(c1) + " delivering=" + I2S(c2) +
                       " to_charger=" + I2S(c3) + " charging=" + I2S(c4) +
                       " dead=" + I2S(c5) + " hot=" + I2S(hx) + "," + I2S(hy) +
                       "," + I2S(hc);
            writeLog(t, s);
        }
    }
}

void Finish() {
    long long open = 0;
    for (auto it = ORD.begin(); it != ORD.end(); ++it) {
        string st = it->second[6];
        if (st == "PENDING" || st == "ASSIGNED" || st == "PICKED") open++;
    }
    writeRaw("SUMMARY delivered=" + I2S(CNT[0]) + " lost=" + I2S(CNT[1]) +
             " rejected=" + I2S(CNT[2]) + " cancelled=" + I2S(CNT[3]) +
             " open=" + I2S(open));
    long long avg = 0;
    if (CNT[0] > 0) avg = (CNT[4] + CNT[0] / 2) / CNT[0];
    writeRaw("LATENCY sum=" + I2S(CNT[4]) + " max=" + I2S(CNT[5]) +
             " avg=" + I2S(avg));
    for (int i = 0; i < NR; i++) {
        writeRaw("ROBOT " + RNAME(i) + " " + I2S(RX[i]) + " " + I2S(RY[i]) +
                 " " + I2S(RB[i]) + " " + RST[i] + " " + I2S(RTR[i]) +
                 " " + I2S(RWT[i]));
    }
}
