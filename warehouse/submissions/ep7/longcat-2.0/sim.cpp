// sim.cpp   仿真主体 (优化版)
#include "common.h"

// 读输入 ----------------------------------------------------------------
void ReadAll(istream& in) {
    in >> W >> H >> T;
    MAP.resize(H);
    for (int i = 0; i < H; i++) in >> MAP[i];
    string tmp;
    in >> tmp;   // "PARAMS"
    for (int i = 0; i < 6; i++) in >> PRM[i];
    in >> tmp >> NR;   // "ROBOTS"
    for (int i = 0; i < NR; i++) {
        int a, b; in >> a >> b;
        RX.push_back(a); RY.push_back(b);
        RB.push_back(PRM[0]);
        RTX.push_back(-1); RTY.push_back(-1);
        RWS.push_back(0); RDS.push_back(-1);
        RWT.push_back(0); RTR.push_back(0);
        RST.push_back("IDLE"); ROID.push_back("");
    }
    int ne = 0;
    in >> tmp >> ne;   // "EVENTS"
    string line; getline(in, line);

    // 预计算充电桩
    CHARGERS.clear();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (MAP[y][x] == 'C') CHARGERS.emplace_back(x, y);

    // 初始化占据网格
    OCC_GRID.assign(H * W, 0);
    for (int i = 0; i < NR; i++) OCC_GRID[RY[i] * W + RX[i]] = i + 1;

    // 预解析事件，按 tick 分组
    EVENTS_BY_TICK.assign(T, {});
    BLK_GRID.assign(H * W, 0);
    for (int i = 0; i < ne; i++) {
        getline(in, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        vector<string> w = SPLIT(line);
        if (w.size() < 2) continue;
        int tick = S2I(w[0]);
        if (tick < 0 || tick >= T) continue;
        Event e;
        string typ = w[1];
        if (typ == "ORDER" && w.size() == 8) {
            e.type = 0; e.id = w[2];
            e.a = S2I(w[3]); e.b = S2I(w[4]);
            e.c = S2I(w[5]); e.d = S2I(w[6]); e.e = S2I(w[7]);
            EVENTS_BY_TICK[tick].push_back(e);
        } else if (typ == "CANCEL" && w.size() == 3) {
            e.type = 1; e.id = w[2];
            EVENTS_BY_TICK[tick].push_back(e);
        } else if (typ == "BLOCK" && w.size() == 4) {
            e.type = 2; e.a = S2I(w[2]); e.b = S2I(w[3]);
            EVENTS_BY_TICK[tick].push_back(e);
        } else if (typ == "UNBLOCK" && w.size() == 4) {
            e.type = 3; e.a = S2I(w[2]); e.b = S2I(w[3]);
            EVENTS_BY_TICK[tick].push_back(e);
        }
    }
}

// 主循环 ----------------------------------------------------------------
void RunSim() {
    unsigned long long s_ev=0, s_re=0, s_ch=0, s_di=0, s_lo=0, s_mv=0, s_rp=0;
    for (int t = 0; t < T; t++) {
        __m128i _pad = _mm_setzero_si128(); // prevent optimization
        // ====================== 1. 事件 ======================
        for (const Event& e : EVENTS_BY_TICK[t]) {
            if (e.type == 0) { // ORDER
                int px = e.a, py = e.b, dx = e.c, dy = e.d, pr = e.e;
                bool good = true;
                if (!INMAP(px, py)) good = false;
                else if (!INMAP(dx, dy)) good = false;
                else if (ISWALL(px, py)) good = false;
                else if (ISWALL(dx, dy)) good = false;
                if (!good) {
                    writeLog(t, "REJECT " + e.id); continue;
                }
                vector<string> f;
                f.push_back(I2S(px)); f.push_back(I2S(py));
                f.push_back(I2S(dx)); f.push_back(I2S(dy));
                f.push_back(I2S(pr)); f.push_back(I2S(t));
                f.push_back("PENDING"); f.push_back("");
                ORD[e.id] = f; PEND.push_back(e.id);
            } else if (e.type == 1) { // CANCEL
                if (ORD.find(e.id) == ORD.end()) {
                    writeLog(t, "CANCEL_FAIL " + e.id); continue;
                }
                CNT[3]++;
                if (ORD[e.id][6] == "PENDING") {
                    vector<string> np;
                    for (int k = 0; k < (int)PEND.size(); k++)
                        if (PEND[k] != e.id) np.push_back(PEND[k]);
                    PEND = np;
                    ORD[e.id][6] = "CANCELLED"; ORD[e.id][7] = "";
                    writeLog(t, "CANCEL " + e.id);
                } else if (ORD[e.id][6] == "ASSIGNED") {
                    int ri = findRobot(ORD[e.id][7]);
                    RST[ri] = "IDLE"; ROID[ri] = "";
                    RTX[ri] = -1; RTY[ri] = -1; RWS[ri] = 0;
                    ORD[e.id][6] = "CANCELLED"; ORD[e.id][7] = "";
                    writeLog(t, "CANCEL " + e.id);
                } else {
                    writeLog(t, "CANCEL_FAIL " + e.id);
                }
            } else if (e.type == 2) { // BLOCK
                int x = e.a, y = e.b;
                if (INMAP(x, y) && !ISWALL(x, y) && !BLK_GRID[y * W + x]) {
                    BLK_GRID[y * W + x] = 1;
                    invalidate_bfs_cache();
                }
            } else if (e.type == 3) { // UNBLOCK
                int x = e.a, y = e.b;
                if (INMAP(x, y) && BLK_GRID[y * W + x]) {
                    BLK_GRID[y * W + x] = 0;
                    invalidate_bfs_cache();
                }
            }
        }

        // ====================== 2. 救援 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] == "DEAD" && t - RDS[i] >= 100 && !OCC_CHK(RX[i], RY[i], i)) {
                RST[i] = "IDLE"; RB[i] = PRM[0] / 2; RWS[i] = 0; RDS[i] = -1;
                OCC_GRID[RY[i] * W + RX[i]] = i + 1;
                writeLog(t, "RESCUE " + RNAME(i));
            }
        }

        // ====================== 3. 充电 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "CHARGING") continue;
            RB[i] += PRM[1]; if (RB[i] > PRM[0]) RB[i] = PRM[0];
            if (RB[i] * 10 >= PRM[0] * 9) {
                RST[i] = "IDLE"; RTX[i] = -1; RTY[i] = -1;
                writeLog(t, "CHARGED " + RNAME(i));
            }
        }

        // ====================== 4. 派单 ======================
        {
            vector<string> ids = PEND;
            sort(ids.begin(), ids.end(), [](const string& a, const string& b) {
                if (S2I(ORD[a][4]) != S2I(ORD[b][4])) return S2I(ORD[a][4]) > S2I(ORD[b][4]);
                if (S2I(ORD[a][5]) != S2I(ORD[b][5])) return S2I(ORD[a][5]) < S2I(ORD[b][5]);
                return S2I(a) < S2I(b);
            });
            for (int oi = 0; oi < (int)ids.size(); oi++) {
                string id = ids[oi];
                int px = S2I(ORD[id][0]), py = S2I(ORD[id][1]);
                int dx = S2I(ORD[id][2]), dy = S2I(ORD[id][3]);
                int best = BIGNUM, who = -1;
                for (int i = 0; i < NR; i++) {
                    if (RST[i] == "IDLE" && RB[i] >= PRM[2]) {
                        int a = DIST(RX[i], RY[i], px, py);
                        if (a < BIGNUM) {
                            int b = DIST(px, py, dx, dy);
                            if (b < BIGNUM) {
                                int c = CHG_DIST(dx, dy);
                                if (c < BIGNUM && RB[i] >= a + b + c + PRM[3]) {
                                    if (a <= best) { best = a; who = i; }
                                }
                            }
                        }
                    }
                }
                if (who == -1) continue;
                RST[who] = "TO_PICKUP"; ROID[who] = id;
                RTX[who] = px; RTY[who] = py; RWS[who] = 0;
                ORD[id][6] = "ASSIGNED"; ORD[id][7] = RNAME(who);
                vector<string> np;
                for (int k = 0; k < (int)PEND.size(); k++)
                    if (PEND[k] != id) np.push_back(PEND[k]);
                PEND = np;
                writeLog(t, "ASSIGN " + id + " " + RNAME(who) + " " + I2S(best));
            }
        }

        // ====================== 5. 低电量去充电 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "IDLE" || RB[i] >= PRM[2]) continue;
            const vector<vector<int>>& d = BFS(RX[i], RY[i]);
            int best = BIGNUM, cx = -1, cy = -1;
            for (int k = 0; k < (int)CHARGERS.size(); k++) {
                int x = CHARGERS[k].first, y = CHARGERS[k].second;
                if (!OK(x, y)) continue;
                bool used = false;
                for (int j = 0; j < NR; j++) {
                    if (j == i) continue;
                    if (RST[j] == "CHARGING" && RX[j] == x && RY[j] == y) used = true;
                    if (RST[j] == "TO_CHARGER" && RTX[j] == x && RTY[j] == y) used = true;
                }
                if (used) continue;
                if (d[y][x] + PRM[3] > RB[i]) continue;
                if (d[y][x] < best) { best = d[y][x]; cx = x; cy = y; }
            }
            if (cx < 0) {
                pair<int,int> p = CHG_NEAR(RX[i], RY[i]); cx = p.first; cy = p.second;
            }
            if (cx < 0) continue;
            RST[i] = "TO_CHARGER"; RTX[i] = cx; RTY[i] = cy; RWS[i] = 0;
            writeLog(t, "GO_CHARGE " + RNAME(i) + " " + I2S(cx) + " " + I2S(cy));
        }

        // ====================== 6. 走 ======================
        for (int i = 0; i < NR; i++) {
            if (RST[i] != "TO_PICKUP" && RST[i] != "DELIVERING" && RST[i] != "TO_CHARGER") continue;
            if (RX[i] == RTX[i] && RY[i] == RTY[i]) {
                RWS[i] = 0;
                if (RST[i] == "TO_PICKUP") {
                    string oid = ROID[i];
                    ORD[oid][6] = "PICKED"; RST[i] = "DELIVERING";
                    RTX[i] = S2I(ORD[oid][2]); RTY[i] = S2I(ORD[oid][3]);
                    writeLog(t, "PICK " + oid + " " + RNAME(i));
                } else if (RST[i] == "DELIVERING") {
                    string oid = ROID[i];
                    ORD[oid][6] = "DONE";
                    int lat = t - S2I(ORD[oid][5]);
                    writeLog(t, "DELIVER " + oid + " " + RNAME(i) + " " + I2S(lat));
                    RST[i] = "IDLE"; ROID[i] = ""; RTX[i] = -1; RTY[i] = -1;
                } else if (RST[i] == "TO_CHARGER") {
                    RST[i] = "CHARGING"; writeLog(t, "CHARGE " + RNAME(i));
                }
                continue;
            }
            if (RB[i] == 0) {
                writeLog(t, "DEAD " + RNAME(i));
                if (ROID[i] != "") {
                    string oid = ROID[i];
                    if (RST[i] == "TO_PICKUP") {
                        ORD[oid][6] = "PENDING"; ORD[oid][7] = "";
                        PEND.push_back(oid); writeLog(t, "REQUEUE " + oid);
                    } else if (RST[i] == "DELIVERING") {
                        ORD[oid][6] = "LOST"; writeLog(t, "LOST " + oid);
                    }
                }
                RST[i] = "DEAD"; RDS[i] = t; ROID[i] = ""; RTX[i] = -1; RTY[i] = -1;
                OCC_GRID[RY[i] * W + RX[i]] = 0;
                continue;
            }
            if (!OK(RTX[i], RTY[i])) { RWT[i]++; continue; }
            const vector<vector<int>>& d = BFS2(RTX[i], RTY[i]);
            int best = BIGNUM, bx = -1, by = -1;
            for (int k = 0; k < 4; k++) {
                int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                if (!OK(nx, ny)) continue;
                if (d[ny][nx] < best) { best = d[ny][nx]; bx = nx; by = ny; }
            }
            if (best >= BIGNUM) { RWT[i]++; continue; }
            if (OCC_CHK(bx, by, i)) {
                bool moved = false;
                if (RWS[i] >= 4 && !(bx == RTX[i] && by == RTY[i])) {
                    for (int k = 0; k < 4; k++) {
                        int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                        if (OK(nx, ny) && !OCC_CHK(nx, ny, i)) {
                            OCC_GRID[RY[i] * W + RX[i]] = 0;
                            RX[i] = nx; RY[i] = ny;
                            OCC_GRID[RY[i] * W + RX[i]] = i + 1;
                            RB[i]--; RTR[i]++; RWS[i] = 0;
                            writeLog(t, "SIDESTEP " + RNAME(i));
                            moved = true; break;
                        }
                    }
                }
                if (!moved) { RWT[i]++; RWS[i]++; continue; }
            } else {
                OCC_GRID[RY[i] * W + RX[i]] = 0;
                RX[i] = bx; RY[i] = by;
                OCC_GRID[RY[i] * W + RX[i]] = i + 1;
                RB[i]--; RTR[i]++; RWS[i] = 0;
            }
            if (RX[i] == RTX[i] && RY[i] == RTY[i]) {
                RWS[i] = 0;
                if (RST[i] == "TO_PICKUP") {
                    string oid = ROID[i];
                    ORD[oid][6] = "PICKED"; RST[i] = "DELIVERING";
                    RTX[i] = S2I(ORD[oid][2]); RTY[i] = S2I(ORD[oid][3]);
                    writeLog(t, "PICK " + oid + " " + RNAME(i));
                } else if (RST[i] == "DELIVERING") {
                    string oid = ROID[i];
                    ORD[oid][6] = "DONE";
                    int lat = t - S2I(ORD[oid][5]);
                    writeLog(t, "DELIVER " + oid + " " + RNAME(i) + " " + I2S(lat));
                    RST[i] = "IDLE"; ROID[i] = ""; RTX[i] = -1; RTY[i] = -1;
                } else if (RST[i] == "TO_CHARGER") {
                    RST[i] = "CHARGING"; writeLog(t, "CHARGE " + RNAME(i));
                }
            }
        }

        // ====================== 7. 报表 ======================
        if ((t + 1) % PRM[4] == 0) {
            int c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
            for (int i = 0; i < NR; i++) {
                if (RST[i] == "IDLE") c0++;
                else if (RST[i] == "TO_PICKUP") c1++;
                else if (RST[i] == "DELIVERING") c2++;
                else if (RST[i] == "TO_CHARGER") c3++;
                else if (RST[i] == "CHARGING") c4++;
                else if (RST[i] == "DEAD") c5++;
            }
            int hx = -1, hy = -1, hc = -1;
            int hr = PRM[5];
            // 热点计算：遍历每个非墙格子，统计附近非停机机器人
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    if (MAP[y][x] == '#') continue;
                    int n = 0;
                    for (int i = 0; i < NR; i++) {
                        if (RST[i] == "DEAD") continue;
                        if (abs(RX[i] - x) + abs(RY[i] - y) <= hr) n++;
                    }
                    if (n > hc) { hc = n; hx = x; hy = y; }
                }
            }
            writeLog(t, "REPORT pending=" + I2S(PEND.size()) + " idle=" + I2S(c0) +
                     " to_pickup=" + I2S(c1) + " delivering=" + I2S(c2) +
                     " to_charger=" + I2S(c3) + " charging=" + I2S(c4) +
                     " dead=" + I2S(c5) + " hot=" + I2S(hx) + "," + I2S(hy) + "," + I2S(hc));
        }
    }
}

// 收尾统计
void Finish() {
    long long open = 0;
    for (auto& it : ORD) {
        string st = it.second[6];
        if (st == "PENDING" || st == "ASSIGNED" || st == "PICKED") open++;
    }
    writeRaw("SUMMARY delivered=" + I2S(CNT[0]) + " lost=" + I2S(CNT[1]) + " rejected=" + I2S(CNT[2]) +
             " cancelled=" + I2S(CNT[3]) + " open=" + I2S(open));
    long long avg = 0;
    if (CNT[0] > 0) avg = (CNT[4] + CNT[0] / 2) / CNT[0];
    writeRaw("LATENCY sum=" + I2S(CNT[4]) + " max=" + I2S(CNT[5]) + " avg=" + I2S(avg));
    for (int i = 0; i < NR; i++) {
        writeRaw("ROBOT " + RNAME(i) + " " + I2S(RX[i]) + " " + I2S(RY[i]) + " " +
                 I2S(RB[i]) + " " + RST[i] + " " + I2S(RTR[i]) + " " + I2S(RWT[i]));
    }
}
