/*
Tank round 1, 2026-10-18.

Derived from the repository's published tournament/sparring.cpp heuristics.
Coordinated opening informed by the supplied GPT-6 Astra source.
Round-1 changes: distinct opening supply assignments; per-tank local-state
transposition PUCT; inline 64-32-1 learned value model with 15% leaf blend;
cached action priors, occupancy and state hashes; bounded CPU-time search.

Search uses the official deterministic tank::step implementation. No files,
network, threads, external model, or opponent identity are used at runtime.
First decision: 24 CPU ms search allowance including init; later: 38 CPU ms.
The judge's budget is only queried, never reset or modified. Startup, output,
and periodic-check overhead are validated separately in formal-process tests.

Attribution and license of the substantial upstream portions:
MIT License

Copyright (c) 2026 meow-ai-arena contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <memory>
#include <array>
#define LOCAL_RANGE 3
#define SP_BUDGET 80000
#define SP_STARW 90
#define SP_LVV 70
#define SP_STAR_REACH 60
#include "tank.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace tank;

// ====================================================================
// 配置
// ====================================================================
#ifndef SP_BUDGET
#define SP_BUDGET 80000        // 每回合前推多少次 step
#endif
#ifndef SP_USE_TIME
#define SP_USE_TIME 1          // 1 = 按本程序 CPU 配额及裁判剩余预算停止
#endif
#ifndef SP_TIME_MARGIN
#define SP_TIME_MARGIN 4       // budget_left() 小于这个数就停（毫秒；引擎建议留 2–3 毫秒，每 16 次模拟查一次）
#endif
#ifndef SP_DEPTH
#define SP_DEPTH 6             // 每次模拟前推几回合
#endif
#ifndef SP_UCB
#define SP_UCB 80              // 探索系数 × 100（PUCT：Q + c·P·sqrt(N)/(1+n)）
#endif
#ifndef SP_PRIOR_T
#define SP_PRIOR_T 20          // 先验分换成概率时的温度：越小越相信规则给的先验
#endif
#ifndef SP_FPU
#define SP_FPU 0                // 没试过的命令的初始估值 = 节点均值 − 这个数 / 100
#endif
#ifndef SP_STARS
#define SP_STARS 1             // 0 = 评估里不看星星（"不抢星"的对照版本）
#endif
#ifndef SP_STARW
#define SP_STARW 26            // 星星的吸引力：离星星 0 回合时的加分，每远一回合少 2 分（"认真抢星"的版本用 90）
#endif
#ifndef SP_LVV
#define SP_LVV 20              // 评估里每一级值多少分（"认真抢星"的版本用 70）
#endif
#ifndef SP_STAR_REACH
#define SP_STAR_REACH 24       // 走位先验里，离星星不超过这么多（1/4 回合）才考虑绕过去
#endif
#ifndef SP_FFFIX
#define SP_FFFIX 1             // 1 = 出手前检查：选定的命令会不会当回合打中队友，会就把开炮的那辆换成次优命令
#endif
#ifndef SP_TV
#define SP_TV 22               // 快速型、装甲型耽误一回合值多少分（基地 1 血 = 100 分）
#endif
#ifndef SP_TV_FIRE
#define SP_TV_FIRE 34          // 火力型耽误一回合值多少分（它到位后每回合能打 1 血，是另外两种的两倍）
#endif
#ifndef SP_THREAT
#define SP_THREAT 40           // 敌车离"能打到我方基地"不到这么多（1/4 回合）就算威胁，要派车去拦
#endif
#ifndef SP_MAX_HUNTERS
#define SP_MAX_HUNTERS 1       // 最多几辆车去拦截（其余进攻）
#endif
#ifndef SP_EMERGENCY
#define SP_EMERGENCY 0         // 敌车离"能打到我方基地"不到这么多（1/4 回合）算紧急，紧急的有几辆就可以派几辆去拦；0 = 不启用
#endif
#ifndef SP_GUARDS
#define SP_GUARDS 1            // 专职守家的车数（先装甲型，再快速型）：对方有车过了半场就留在家门口等，没有才去进攻；9 = 按来犯数量自动定
#endif
#ifndef SP_GUARD_RANGE
#define SP_GUARD_RANGE 48      // 对方有车离"能打到我方基地"不到这么多（1/4 回合），守家的车就不出门
#endif
#ifndef SP_FAR_COST
#define SP_FAR_COST 3          // 射击位离基地每远一格（超过 2 格的部分）的代价（1/4 回合）：远了子弹飞得久、容易被挡被抵消
#endif
#ifndef SP_BLOCK_STILL
#define SP_BLOCK_STILL 0       // 炮线上的车连续这么多回合没动才算堵住炮线；0 = 只要在炮线上就算
#endif
#ifndef SP_HUNT_MAX
#define SP_HUNT_MAX 64         // 拦截的车最远从多远赶过去（1/4 回合）
#endif
#ifndef SP_SQUASH
#define SP_SQUASH 600          // 评估分压到 (-1, 1) 时的尺度
#endif
#ifndef SP_PATH_AVG
#define SP_PATH_AVG 1          // 1 = 一次模拟的分数取沿途每回合评估的平均；0 = 只看最后一回合（对照用）
#endif
#ifndef SP_METHOD
#define SP_METHOD 0            // 0 = 分散式 UCT（默认）；1 = 一回合前推 + 规则续推 + 轮流取最佳应对（对照用的另一种搜索）
#endif
#ifndef SP_IBR_ROUNDS
#define SP_IBR_ROUNDS 3        // SP_METHOD = 1 时轮流优化几轮
#endif
#ifndef SP_ROLLOUT_RANDOM
#define SP_ROLLOUT_RANDOM 4    // 树外策略每 N 次有一次乱走
#endif

static const int BIG = 30000;
static const int MAX_ACTS = 22;

// ====================================================================
// 每回合开头算一次的地图信息（墙没变就沿用）
// ====================================================================
struct Maps {
    bool valid = false;
    uint64_t wall_hash = 0;
    int16_t atk[2][CELLS];              // atk[seat][格]：seat 的慢车从这格出发，到"能有效射击对方基地"的代价，单位 1/4 回合
    int16_t sup[MAX_SUPPLY][CELLS];     // 到各补给点的代价，同单位
    int n_key[2];                       // key[seat]：seat 进攻炮线上的砖（打掉它对 seat 有利）
    int16_t key_cell[2][24];
    int16_t key_val[2][24];
};
static Maps M;

static void flood(const State& s, int16_t* dist) {
    static int queue[CELLS * 16];
    int head = 0, tail = 0;
    for (int c = 0; c < CELLS; ++c)
        if (dist[c] < BIG) queue[tail++] = c;
    while (head < tail) {
        const int c = queue[head++ % (CELLS * 16)];
        for (int d = 0; d < 4; ++d) {
            const int n = c + DIR_DELTA[d];
            const uint8_t g = s.grid[n];
            if (g != EMPTY && g != BRICK) continue;
            const int nd = dist[c] + (g == BRICK ? 12 : 4);     // 空地一步 1 回合；砖墙要先打掉，算 3 回合
            if (nd < dist[n]) { dist[n] = int16_t(nd); queue[tail++ % (CELLS * 16)] = n; }
        }
    }
}

static void build_maps(const State& s) {
    if (M.valid && M.wall_hash == s.wall_hash) return;
    M.valid = true;
    M.wall_hash = s.wall_hash;
    for (int seat = 0; seat < 2; ++seat) {
        int16_t* dist = M.atk[seat];
        for (int c = 0; c < CELLS; ++c) dist[c] = BIG;
        M.n_key[seat] = 0;
        // 基地只能沿它所在的行、列被打到。从对方基地朝四个方向走出去，沿途的空地就是射击位。
        for (int d = 0; d < 4; ++d) {
            int bricks = 0, k = 1;
            for (int c = s.base_pos[1 - seat] + DIR_DELTA[d];; c += DIR_DELTA[d], ++k) {
                const uint8_t g = s.grid[c];
                if (g == BRICK) {
                    if (M.n_key[seat] < 24) {
                        M.key_cell[seat][M.n_key[seat]] = int16_t(c);
                        M.key_val[seat][M.n_key[seat]++] = int16_t(bricks == 0 ? 16 : 8);
                    }
                    dist[c] = int16_t(8 * bricks + 12 + (k > 2 ? 2 * (k - 2) : 0));   // 打掉这块砖站进去也是射击位
                    ++bricks;
                    continue;
                }
                if (g == WATER) continue;
                if (g != EMPTY) break;
                dist[c] = int16_t(8 * bricks + (k > 2 ? 2 * (k - 2) : 0));   // 每块砖多打一炮；超过 2 格子弹当回合到不了
            }
        }
        flood(s, dist);
    }
    for (int k = 0; k < s.n_supply; ++k) {
        for (int c = 0; c < CELLS; ++c) M.sup[k][c] = BIG;
        M.sup[k][s.supply[k]] = 0;
        flood(s, M.sup[k]);
    }
}

// ====================================================================
// 评估函数：从 0 号方的角度给局面打分（基地 1 血 = 100 分）
// ====================================================================
static uint8_t root_grid[CELLS];        // 本回合开头的地形（用来看模拟里哪些关键的砖被打掉了）
static int soon_mask;                   // 马上要出星的补给点（提前去占位）

// 每辆车自己的进攻图：和 M.atk 一样，但把别的车此刻占着的格子算贵一些（多 2 回合），
// 这样通道被堵住（比如两辆车在窄道里对射僵住）时会绕路，而不是排在后面干等。每回合重算。
static int16_t atk_of[6][CELLS];

static void flood(const State& s, int16_t* dist);

static int still[6], still_pos[6];      // 每辆车已经连续几回合待在同一格

static void build_tank_maps(const State& s) {
    for (int j = 0; j < 6; ++j) {
        if (s.tank[j].alive && s.tank[j].pos == still_pos[j]) still[j] += 1;
        else still[j] = 0;
        still_pos[j] = s.tank[j].alive ? s.tank[j].pos : -1;
    }
    for (int i = 0; i < 6; ++i) {
        const int seat = i / 3;
        int16_t* dist = atk_of[i];
        for (int c = 0; c < CELLS; ++c) dist[c] = BIG;
        for (int d = 0; d < 4; ++d) {
            int bricks = 0, k = 1;
            for (int c = s.base_pos[1 - seat] + DIR_DELTA[d];; c += DIR_DELTA[d], ++k) {
                const uint8_t g = s.grid[c];
                if (g == BRICK) {
                    // 炮线上的砖：打掉之后站进去也是射击位（两侧被钢墙封住时，围墙那一格是唯一的侧面射击位）。
                    dist[c] = int16_t(8 * bricks + 12 + (k > 2 ? SP_FAR_COST * (k - 2) : 0));
                    ++bricks;
                    continue;
                }
                if (g == WATER) continue;
                if (g != EMPTY) break;
                dist[c] = int16_t(8 * bricks + (k > 2 ? SP_FAR_COST * (k - 2) : 0));
                // 炮线上有别的车（不是自己）：它后面的格子打不到基地，不算射击位。
                // （不这样的话，车会站在老远的地方隔着对方的车"打基地"，两边子弹互相抵消，谁也不动。）
                bool blocked = false;
                for (int j = 0; j < 6; ++j) blocked |= j != i && s.tank[j].alive && s.tank[j].pos == c && still[j] >= SP_BLOCK_STILL;
                if (blocked) break;
            }
        }
        // 带"别的车占着的格子更贵"的最短路。
        static int queue[CELLS * 16];
        static int16_t extra[CELLS];
        for (int c = 0; c < CELLS; ++c) extra[c] = 0;
        for (int j = 0; j < 6; ++j)
            if (j != i && s.tank[j].alive) extra[s.tank[j].pos] = int16_t(still[j] >= 3 ? 24 : 8);   // 站着不动好几回合的：当它是堵死的
        int head = 0, tail = 0;
        for (int c = 0; c < CELLS; ++c)
            if (dist[c] < BIG) queue[tail++] = c;
        while (head < tail) {
            const int c = queue[head++ % (CELLS * 16)];
            for (int d = 0; d < 4; ++d) {
                const int n = c + DIR_DELTA[d];
                const uint8_t g = s.grid[n];
                if (g != EMPTY && g != BRICK) continue;
                const int nd = dist[c] + (g == BRICK ? 12 : 4) + extra[c];      // 从 n 走进 c：c 上有别的车就多算
                if (nd < dist[n]) { dist[n] = int16_t(nd); queue[tail++ % (CELLS * 16)] = n; }
            }
        }
    }
}

// 一辆车离"能有效射击对方基地"还差多少，单位 1/4 回合。快速型按 0.6 倍算。
static inline int progress_cost(int i, int pos) {
    int a = atk_of[i][pos];
    if (a > 240) a = 240;
    return i % 3 == FAST ? a * 5 / 8 : a;
}

// ---- 分工（每回合开头按规则定，搜索期间不变）----
// 我方每辆车要么进攻（目标是对方基地的射击位），要么拦截某一辆逼近我方基地的敌车。
// 搜索负责"怎么走、什么时候开炮"，分工只决定评估函数里这辆车"离目标多远"按哪张图算。

static int opening_star[6];
static bool opening_active[6];
static void init_opening(const State&s){
 for(int seat=0;seat<2;seat++){
  int best=100000;
  for(int a=0;a<3;a++)for(int b=0;b<3;b++)for(int c=0;c<3;c++)if(a!=b&&a!=c&&b!=c){
   int z[3]={a,b,c},sum=0,mx=0;
   for(int t=0;t<3;t++){int d=M.sup[z[t]][s.spawn[seat*3+t]];if(t==FAST)d=d*5/8;sum+=d;if(d>mx)mx=d;}
   if(sum+mx/2<best){best=sum+mx/2;for(int t=0;t<3;t++)opening_star[seat*3+t]=z[t];}
  }
 }
}

static int root_me;
static int hunt_target[6];              // -1 = 进攻；否则是要拦截的敌车编号
static int16_t hunt_map[3][CELLS];      // hunt_map[k]：到"和第 k 辆敌车对上炮线"的代价
static int16_t post_map[CELLS];         // 到家门口的代价（守家的车没有目标时用）
static bool holding[6];                 // 这辆车本回合在守家待命
static int prev_target[3];

static int stale_turns;                 // 已经连续多少回合"什么都没发生"（基地血量、击毁数、墙都没变）
static int stale_hp[2], stale_kills[2];
static uint64_t stale_walls;

static void assign_roles(const State& s, int me) {
    // 僵持检测：局面长时间没有进展，就减少拦截的车、多派车去进攻，把僵局打破。
    if (s.base_hp[0] != stale_hp[0] || s.base_hp[1] != stale_hp[1] || s.kills[0] != stale_kills[0] ||
        s.kills[1] != stale_kills[1] || s.wall_hash != stale_walls) {
        stale_turns = 0;
        stale_hp[0] = s.base_hp[0]; stale_hp[1] = s.base_hp[1];
        stale_kills[0] = s.kills[0]; stale_kills[1] = s.kills[1];
        stale_walls = s.wall_hash;
    } else {
        stale_turns += 1;
    }
    int max_hunters = SP_MAX_HUNTERS;
    if (stale_turns >= 10 && max_hunters > 1) max_hunters = 1;
    if (stale_turns >= 30) max_hunters = 0;
    int n_hunters = 0;
#if SP_EMERGENCY
    // 已经有敌车到位、正在打我方基地（或一两回合内就能打）：有几辆就最多派几辆去拦，不受上面的名额限制。
    {
        int urgent = 0;
        for (int k = 0; k < 3; ++k) {
            const int foe = 3 * (1 - me) + k;
            if (!s.tank[foe].alive) continue;
            int c = M.atk[1 - me][s.tank[foe].pos];
            if (k == FAST) c = c * 5 / 8;
            urgent += c <= SP_EMERGENCY;
        }
        if (urgent > max_hunters && stale_turns < 10) max_hunters = urgent;
    }
#endif
    root_me = me;
    for (int i = 0; i < 6; ++i) { hunt_target[i] = -1; holding[i] = false; }
    const int foe0 = 3 * (1 - me);
    int threat[3], threat_cost[3], n = 0;
    for (int k = 0; k < 3; ++k) {
        if (!s.tank[foe0 + k].alive) continue;
        int c = M.atk[1 - me][s.tank[foe0 + k].pos];
        if (k == FAST) c = c * 5 / 8;
        if (c > SP_THREAT) continue;
        int at = n++;
        while (at > 0 && threat_cost[at - 1] > c) { threat[at] = threat[at - 1]; threat_cost[at] = threat_cost[at - 1]; --at; }
        threat[at] = k;
        threat_cost[at] = c;
    }
    bool used[3] = {};
    int now_target[3] = {-1, -1, -1};
    for (int q = 0; q < n && n_hunters < max_hunters; ++q) {
        const int k = threat[q], foe = foe0 + k;
        int16_t* dist = hunt_map[k];
        for (int c = 0; c < CELLS; ++c) dist[c] = BIG;
        for (int d = 0; d < 4; ++d) {
            int steps = 1;
            for (int c = s.tank[foe].pos + DIR_DELTA[d]; s.grid[c] == EMPTY || s.grid[c] == WATER; c += DIR_DELTA[d], ++steps)
                if (s.grid[c] == EMPTY) dist[c] = int16_t(steps <= 2 ? 0 : 2 * (steps - 2));
        }
        flood(s, dist);
        int best = -1, best_eta = SP_HUNT_MAX;
        for (int t = 0; t < 3; ++t) {
            if (used[t]) continue;
            const int i = 3 * me + t;
            const bool alive = s.tank[i].alive;
            const int pos = alive ? s.tank[i].pos : s.spawn[i];
            int eta = dist[pos] >= BIG ? BIG : (t == FAST ? dist[pos] * 5 / 8 : dist[pos]);
            if (!alive) eta += 4 * respawn_in(s, i);
            if (alive && progress_cost(i, pos) <= 4 && eta > 8) continue;        // 已经在打对方基地、离得又远：不回头
            if (prev_target[t] == foe) eta -= 8;                                 // 上回合就在追它：优先接着追
#if SP_GUARDS
            if (!(t == ARMOR || (SP_GUARDS >= 2 && t == FAST) || SP_GUARDS == 3)) eta += 12;   // 有专职守家的车时，优先派它们
#endif
            if (eta < best_eta) { best_eta = eta; best = t; }
        }
        if (best >= 0) {
            ++n_hunters;
            used[best] = true;
            hunt_target[3 * me + best] = foe;
            now_target[best] = foe;
        }
    }
    for (int t = 0; t < 3; ++t) prev_target[t] = now_target[t];
#if SP_GUARDS
    // 专职守家的车：没被派去拦截、而对方有车已经过来了，就在家门口待命。
    {
        bool coming = false;
        for (int k = 0; k < 3; ++k) {
            if (!s.tank[foe0 + k].alive) continue;
            int c = M.atk[1 - me][s.tank[foe0 + k].pos];
            if (k == FAST) c = c * 5 / 8;
            coming |= c <= SP_GUARD_RANGE;
        }
        static const int order[3] = {ARMOR, FAST, FIRE};
        int n_guards = SP_GUARDS;
        if (SP_GUARDS == 9) {                                    // 9 = 自动：对方来几辆，就留"来的数量减一"辆（最多两辆）
            int n_coming = 0;
            for (int k = 0; k < 3; ++k) {
                int c = M.atk[1 - me][s.tank[foe0 + k].alive ? s.tank[foe0 + k].pos : s.spawn[foe0 + k]];
                if (k == FAST) c = c * 5 / 8;
                n_coming += s.tank[foe0 + k].alive && c <= SP_GUARD_RANGE;
            }
            n_guards = n_coming - 1 - n_hunters;
            if (n_guards > 2) n_guards = 2;
        }
        for (int q = 0; q < n_guards && q < 3; ++q) {
            const int i = 3 * me + order[q];
            if (coming && hunt_target[i] < 0 && stale_turns < 30) holding[i] = true;
        }
        for (int c = 0; c < CELLS; ++c) post_map[c] = BIG;
        const int fwd = me == 0 ? UP : DOWN;
        const int post = s.base_pos[me] + 3 * DIR_DELTA[fwd];
        for (int d = 0; d < 4; ++d) {
            const int c = post + DIR_DELTA[d];
            if (s.grid[c] == EMPTY) post_map[c] = 0;
        }
        if (s.grid[post] == EMPTY) post_map[post] = 0;
        flood(s, post_map);
    }
#endif
}

// 评估和走位用的"离目标多远"：拦截的车看离目标敌车多远（它被打掉了就算到位），其余看进攻图。
static inline int tank_cost(const State& s, int i, int pos) {
    if(opening_active[i]){int d=M.sup[opening_star[i]][pos];return d>240?240:(i%3==FAST?d*5/8:d);}
    const int target = hunt_target[i];
    if (holding[i]) { const int a = post_map[pos]; return a > 240 ? 240 : (i % 3 == FAST ? a * 5 / 8 : a); }
    if (target < 0) return progress_cost(i, pos);
    if (!s.tank[target].alive) return 0;
    int a = hunt_map[target % 3][pos];
    if (a > 240) a = 240;
    return i % 3 == FAST ? a * 5 / 8 : a;
}

// Strategic value features. Uses only the current public state and root distance maps.
// Included inside the bot namespace, after map helpers.
constexpr int VALUE_INPUTS=64;
static void value_features(const State&s,int side,float*x){
 for(int k=0;k<VALUE_INPUTS;k++)x[k]=0;
 x[0]=s.base_hp[side]/24.f;x[1]=s.base_hp[1-side]/24.f;
 x[2]=(s.base_hp[side]-s.base_hp[1-side])/24.f;
 x[3]=std::min(30,int(s.kills[side]))/30.f;x[4]=std::min(30,int(s.kills[1-side]))/30.f;
 x[5]=std::min(12,int(s.stars_eaten[side]))/12.f;x[6]=std::min(12,int(s.stars_eaten[1-side]))/12.f;
 x[7]=s.turn/300.f;
 int next=s.P.supply_first;if(s.turn>=next)next+=((s.turn-next)/s.P.supply_period+1)*s.P.supply_period;
 x[8]=(next-s.turn)/20.f;
 int at=9;
 for(int type=0;type<3;type++){
  int i=side*3+type;const Tank&t=s.tank[i];int p=t.alive?t.pos:s.spawn[i];
  int enemynear=40;for(int j=(1-side)*3;j<(1-side)*3+3;j++)if(s.tank[j].alive){int d=std::abs(cell_x(p)-cell_x(s.tank[j].pos))+std::abs(cell_y(p)-cell_y(s.tank[j].pos));enemynear=std::min(enemynear,d);}
  x[at++]=t.alive?1.f:0.f;x[at++]=t.hp/6.f;x[at++]=t.level/2.f;
  x[at++]=progress_cost(i,p)/120.f;
  x[at++]=respawn_in(s,i)/4.f;
  x[at++]=can_fire(s,i)?1.f:0.f;
  x[at++]=std::min(20,enemynear)/20.f;
  x[at++]=(std::abs(cell_x(p)-cell_x(s.base_pos[side]))+std::abs(cell_y(p)-cell_y(s.base_pos[side])))/25.f;
  x[at++]=(std::abs(cell_x(p)-cell_x(s.base_pos[1-side]))+std::abs(cell_y(p)-cell_y(s.base_pos[1-side])))/25.f;
  int star=240;for(int k=0;k<s.n_supply;k++)if((s.star_mask&(1<<k))||next-s.turn<=8)star=std::min(star,int(M.sup[k][p]));
  x[at++]=std::min(120,star)/120.f;
 }
 for(int k=0;k<3;k++){
  int own=120,foe=120;
  for(int t=0;t<3;t++)for(int z=0;z<2;z++){
   int i=3*(z?1-side:side)+t;if(!s.tank[i].alive||s.tank[i].level>=2)continue;
   int d=M.sup[k][s.tank[i].pos];if(t==FAST)d=d*5/8;
   if(z)foe=std::min(foe,d);else own=std::min(own,d);
  }
  x[at++]=own/120.f;x[at++]=foe/120.f;x[at++]=(s.star_mask&(1<<k))?1.f:0.f;
 }
 int owneta=240,enemyeta=240;
 for(int type=0;type<3;type++){
  int j=(1-side)*3+type;const Tank&t=s.tank[j];int p=t.alive?t.pos:s.spawn[j];
  int d=progress_cost(j,p)+4*respawn_in(s,j);enemyeta=std::min(enemyeta,d);
  x[at++]=d/120.f;x[at++]=t.hp/6.f;
  int i=side*3+type;owneta=std::min(owneta,progress_cost(i,s.tank[i].alive?s.tank[i].pos:s.spawn[i])+4*respawn_in(s,i));
 }
 x[at++]=(enemyeta-owneta)/120.f;
 x[at++]=(s.base_hp[side]+enemyeta*.25f-s.base_hp[1-side]-owneta*.25f)/40.f;
 x[at++]=s.n_bullets/12.f;
}

static const float VALUE_W0[2048]={1.061573275e-03f,-5.065528676e-02f,7.297255099e-02f,-1.320130825e-01f,3.349069878e-02f,4.939976707e-02f,-5.375659093e-02f,5.103708506e-01f,3.715658784e-01f,6.174204499e-02f,1.876391768e-01f,6.537919398e-03f,-1.257970035e-01f,-3.561596945e-02f,-1.860994659e-02f,-2.650792897e-01f,1.310534924e-01f,8.474667370e-02f,-9.334973246e-02f,1.186018959e-01f,1.159865111e-01f,2.207188457e-01f,5.274844915e-02f,8.106552996e-03f,7.727159560e-02f,6.115568057e-02f,1.695797294e-01f,1.201652810e-01f,-3.013840318e-01f,-6.023061648e-02f,-1.516256481e-01f,8.884544671e-02f,1.474125534e-01f,-8.839653432e-02f,9.152638912e-02f,-1.217208952e-01f,-6.556221098e-02f,-2.070231363e-02f,-4.053864479e-01f,-7.574903220e-02f,6.031129882e-02f,6.057963148e-02f,-6.340055913e-02f,6.828802824e-02f,6.483402848e-02f,-1.967858076e-01f,2.805922031e-01f,1.269967556e-01f,-2.503445558e-02f,-1.785472780e-01f,1.530306041e-02f,6.810934842e-02f,-2.940147929e-02f,1.616390496e-01f,5.106842145e-03f,1.186497733e-01f,4.021387994e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-3.360366225e-01f,5.359455198e-02f,-4.023441672e-01f,1.231341437e-01f,-1.847573221e-01f,2.821450233e-01f,-3.169813454e-01f,-7.621961087e-02f,2.912545390e-02f,-2.863743156e-02f,2.701805532e-01f,3.184872270e-01f,2.229334712e-01f,2.475162596e-01f,2.583052265e-03f,2.461672574e-01f,5.504692346e-02f,1.105226055e-01f,-8.349115402e-02f,1.025866121e-01f,2.715341747e-01f,5.071095377e-02f,2.738018036e-01f,-1.593655646e-01f,3.074839152e-02f,6.822084785e-01f,-2.142641991e-01f,3.655878603e-01f,-5.323083326e-02f,-1.599395648e-02f,1.955893487e-01f,2.827376127e-02f,-4.475502670e-02f,-1.811040752e-02f,1.790138776e-03f,1.117563173e-01f,2.718318440e-02f,7.121517323e-03f,5.418196321e-02f,8.991064131e-02f,-3.232621774e-02f,9.617979825e-02f,2.368032932e-01f,1.676338911e-01f,-1.127215773e-01f,-2.771021128e-01f,-1.843304485e-01f,-2.453434281e-02f,-3.789242208e-01f,2.290477045e-02f,1.712365597e-01f,-1.599299610e-01f,1.941146255e-01f,-4.637269080e-01f,-1.970164627e-01f,-3.915727437e-01f,2.801795006e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,9.358152747e-01f,5.322663188e-01f,6.647762656e-01f,3.246637881e-01f,1.171479821e-01f,-5.735945329e-02f,6.346464902e-02f,-3.444450796e-01f,6.340083480e-02f,-3.189985640e-03f,-3.610138595e-02f,-1.055276860e-02f,2.262473106e-01f,3.387659192e-01f,7.535611093e-02f,-1.127406582e-01f,-6.794509292e-02f,-7.370006293e-02f,1.674038023e-01f,-1.269170200e-03f,9.791152179e-02f,1.166478172e-01f,6.331969053e-02f,-3.252738342e-02f,-4.129388928e-02f,-6.211784109e-02f,-3.468665481e-01f,6.731599569e-02f,1.026707739e-01f,-9.638927877e-03f,-2.145177573e-01f,-3.015638590e-01f,1.435492486e-01f,7.389146835e-02f,7.606911473e-03f,1.397852153e-01f,2.499246411e-02f,6.100057811e-02f,8.755013943e-01f,-6.863275170e-02f,-3.310194612e-01f,-3.191966712e-01f,1.425020248e-01f,-1.918613762e-01f,-4.534602538e-02f,-4.552845061e-01f,2.549011111e-01f,-2.000759095e-01f,1.242604181e-01f,5.557108670e-02f,-8.388328552e-02f,2.095991075e-01f,1.847661473e-02f,-1.957318187e-02f,8.329970390e-02f,4.262428582e-01f,1.324984282e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-2.458039485e-02f,-3.397115171e-01f,1.858198196e-01f,-3.595649302e-01f,4.440667331e-01f,-1.791874766e-01f,2.163310051e-01f,5.232091546e-01f,-6.643158942e-02f,-6.229168922e-02f,3.660885692e-01f,7.332514971e-02f,-6.084023416e-02f,2.020558715e-01f,9.334681928e-02f,6.545417309e-01f,1.786108017e-01f,-3.664354682e-01f,-1.863069683e-01f,1.758965850e-01f,3.407616541e-02f,-2.249037623e-01f,4.810068607e-01f,-1.364936233e-01f,1.943371259e-02f,-5.295693874e-01f,-9.342079759e-01f,3.938800097e-01f,1.374403238e-01f,-2.535501868e-02f,-3.027829155e-02f,-1.087259203e-01f,-5.667448416e-02f,-1.708413363e-01f,4.001323832e-04f,-2.932173014e-01f,-3.690356016e-02f,-2.988910377e-01f,8.235530555e-02f,3.456541300e-01f,-3.480214253e-02f,-4.606184736e-02f,2.102340311e-01f,2.315985411e-01f,-1.338164508e-01f,2.807856202e-01f,2.190184593e-01f,-7.721006870e-02f,1.783365756e-01f,3.771774173e-01f,-2.923361063e-01f,2.650468349e-01f,-2.442815527e-02f,-1.083935797e-01f,2.395276539e-02f,4.787417650e-01f,-3.038785756e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-1.205303743e-01f,-1.456440538e-01f,-3.259465992e-01f,-8.018675447e-01f,6.756045222e-01f,5.745077133e-02f,1.326404959e-01f,-3.552662730e-01f,8.188819885e-02f,7.634365559e-02f,1.444514394e-01f,-3.995703999e-03f,-1.329666674e-01f,-1.305736750e-01f,-1.598546980e-03f,3.770003021e-01f,-4.833643138e-02f,1.031262502e-01f,6.911273301e-02f,-2.228560485e-02f,-2.812977433e-01f,3.584618866e-01f,1.081207693e-01f,-5.658548325e-02f,-9.453441203e-02f,-5.979481339e-02f,9.648445249e-02f,-9.885062277e-02f,1.872418635e-02f,-7.654080540e-02f,-1.140643507e-01f,-1.101845503e-01f,-1.813914478e-01f,-2.600090578e-02f,-6.690546870e-02f,-2.349192500e-01f,-5.698258989e-03f,9.037260711e-02f,-6.429485232e-02f,3.613476753e-01f,-3.135101199e-01f,1.903675944e-01f,7.081335187e-01f,-2.231477946e-01f,3.470290303e-01f,1.648364514e-01f,-2.241526265e-03f,4.077954218e-02f,3.635039032e-01f,5.771508813e-02f,-5.564944744e-01f,3.902052343e-01f,1.237772182e-01f,-1.189992428e-01f,2.889267541e-02f,-1.204370633e-01f,3.667150438e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-1.136689633e-01f,-1.476919949e-01f,-7.482086867e-02f,4.519459233e-02f,-6.596013904e-02f,3.330708714e-03f,-1.485777497e-01f,-9.539306164e-02f,-1.360142529e-01f,-8.232802898e-02f,-6.988152117e-02f,-1.352184862e-01f,-5.838206783e-02f,-9.921773523e-02f,-1.675795391e-02f,-1.345665902e-01f,-1.050374359e-01f,-7.378613204e-02f,-1.432070434e-01f,-4.224026203e-02f,3.316599503e-02f,-4.387214407e-02f,-5.225278437e-02f,-1.102028117e-01f,7.915879041e-02f,4.003908485e-02f,-4.299547896e-02f,-1.303394884e-01f,7.629829645e-02f,4.008102790e-02f,2.094511874e-03f,-7.341607660e-02f,7.599273324e-02f,8.143652976e-02f,4.339369759e-02f,-3.749429714e-03f,-1.424143612e-01f,-7.831711322e-03f,-1.169386432e-01f,3.877966898e-03f,6.173529103e-02f,-7.291093469e-02f,-3.111607954e-02f,-8.602987975e-02f,-1.103003249e-01f,7.456719875e-02f,-3.941121325e-02f,6.910555810e-02f,-1.007088646e-02f,-6.462039426e-03f,-1.982788183e-02f,3.791632503e-02f,-7.879021019e-02f,5.987923220e-02f,5.351768062e-02f,-5.887885019e-02f,-1.209899932e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,1.367323697e-01f,-3.141636252e-01f,4.306223094e-01f,7.822677493e-02f,1.718847901e-01f,1.844022572e-01f,2.049529701e-01f,5.424932241e-01f,-7.914066315e-02f,-5.183859915e-02f,2.455589026e-01f,-2.017523944e-01f,4.386163354e-01f,2.617306821e-02f,7.892245427e-03f,9.541935474e-02f,2.789174318e-01f,7.474274933e-02f,-2.727044225e-01f,4.543980956e-02f,-7.187466323e-02f,-2.083210647e-01f,3.062892258e-01f,-1.573217660e-02f,-6.567880511e-02f,7.384064049e-02f,-5.174495578e-01f,2.250471264e-01f,3.267747164e-01f,-1.779028447e-03f,1.668971926e-01f,-1.306601167e-01f,-2.288103253e-01f,3.092125803e-02f,-4.076764360e-02f,2.372118384e-01f,2.191704810e-01f,-5.308634043e-01f,-9.923397005e-02f,3.705335855e-01f,3.454945236e-02f,2.506088912e-01f,-1.589176506e-01f,4.523317516e-01f,2.132096440e-01f,1.060123965e-01f,3.701872230e-01f,2.690912187e-01f,-3.858620822e-01f,-4.340174794e-01f,3.897369280e-02f,3.249422014e-01f,2.140154690e-01f,-1.848163269e-02f,-2.027134746e-01f,2.605193853e-01f,-1.691942811e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,6.397950649e-02f,-5.639526993e-02f,1.239043772e-01f,-1.937980354e-01f,3.224437311e-02f,1.954001039e-01f,5.727259442e-02f,-6.640395522e-02f,1.434213817e-01f,1.239096001e-01f,2.599033155e-02f,7.079618424e-02f,4.213935882e-02f,-6.037434563e-02f,4.786301032e-02f,-2.012716532e-01f,9.350750595e-02f,9.133810177e-03f,4.282341152e-02f,-3.608398372e-03f,-4.007299617e-02f,2.716405988e-01f,3.573470935e-02f,-6.699871272e-03f,1.159178615e-01f,-7.140661031e-02f,4.819674359e-04f,6.363833696e-02f,-3.692920506e-01f,9.035506099e-02f,3.202130087e-03f,7.727706432e-02f,2.216271497e-02f,-6.821903586e-02f,1.555139851e-02f,-8.265914023e-02f,1.256762259e-02f,8.099958301e-02f,-1.701555550e-01f,-1.078439280e-01f,8.147183061e-02f,-2.650637031e-01f,5.690984055e-02f,5.059928074e-02f,-1.719411463e-01f,-1.189188212e-01f,1.915034950e-01f,1.931283176e-01f,-3.831314668e-02f,-4.231654108e-03f,-1.655759960e-01f,6.708040088e-02f,-5.833536759e-02f,2.148611993e-01f,-1.380223781e-01f,-1.268053204e-01f,6.833685189e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,9.813147038e-02f,1.233858690e-01f,4.463714659e-01f,3.634516895e-01f,-7.929735184e-01f,-3.980227709e-01f,-2.680429518e-01f,-6.996130347e-01f,1.768113375e-01f,1.536917835e-01f,-2.083847821e-01f,-2.115858793e-01f,9.522858262e-02f,1.414447874e-01f,5.900477991e-04f,3.146020770e-01f,-2.345814556e-01f,-1.199346967e-02f,-1.850241423e-01f,-5.500634015e-02f,4.083677381e-02f,-3.395592570e-01f,-2.277438194e-01f,1.565920264e-01f,-2.346180938e-02f,1.417720038e-02f,-1.009324342e-01f,-1.016924232e-01f,1.721381396e-01f,5.076681077e-02f,1.393954307e-01f,-2.681528628e-01f,1.816877276e-01f,8.549657464e-02f,2.699366584e-02f,-1.523375660e-01f,-4.747281969e-02f,1.340913773e-02f,1.765425950e-01f,3.427267075e-01f,-3.898429275e-01f,4.495944828e-02f,8.842583001e-02f,-1.675174534e-01f,-1.777888536e-01f,1.555027515e-01f,-3.992539942e-01f,1.332931966e-02f,-8.471561968e-02f,-5.098954961e-02f,3.651988208e-01f,-2.507976890e-01f,2.313117534e-01f,-4.263176322e-01f,3.775810301e-01f,4.775540531e-01f,2.387286723e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,9.635899216e-02f,-1.956919730e-01f,2.461452335e-01f,4.620031118e-01f,-2.299018502e-01f,1.154545248e-01f,2.868477702e-01f,-1.645223796e-01f,8.594378084e-02f,1.251459271e-01f,-5.824520811e-02f,3.197954893e-01f,-4.171583951e-01f,2.138890624e-01f,-4.104480147e-02f,1.459518522e-01f,1.777138859e-01f,-3.278377056e-01f,-1.960735023e-01f,6.213266030e-02f,2.169977874e-01f,2.183273137e-01f,-1.181116253e-01f,3.861509264e-02f,-9.882115573e-02f,-3.712401958e-03f,1.297592223e-01f,-1.741547734e-01f,-2.426472120e-02f,1.056879293e-02f,-1.632211804e-01f,-8.285549469e-03f,1.179256570e-02f,1.134496406e-01f,-2.079458348e-02f,-3.076077402e-01f,-6.748480201e-01f,1.179363653e-01f,1.854355037e-01f,1.468336880e-01f,-5.463612080e-01f,-7.095229626e-02f,1.336170137e-01f,-4.290499389e-01f,1.403322220e-01f,3.008196652e-01f,-3.326604068e-01f,-4.159586504e-02f,4.619114399e-01f,1.070466340e-01f,8.781879209e-03f,3.572688997e-01f,-2.809219360e-01f,3.345535323e-02f,3.895809650e-01f,2.806745172e-01f,-1.137582287e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,1.399906427e-01f,7.791202515e-02f,-2.414396256e-01f,-1.883858740e-01f,-2.812032029e-02f,8.413498849e-02f,-1.425643861e-01f,-3.070130348e-01f,-3.449282423e-02f,-1.313002557e-01f,1.147885919e-01f,5.456656218e-02f,-5.346218869e-02f,1.011434104e-02f,3.234214708e-02f,-4.754591584e-01f,8.150547743e-02f,7.551097125e-02f,6.880046725e-01f,6.995929778e-02f,-8.069707453e-02f,2.303372175e-01f,1.800817251e-01f,-8.296961337e-02f,9.495658427e-02f,-1.843657047e-01f,-4.507861286e-02f,9.133050591e-02f,1.874039881e-02f,2.765643969e-02f,6.280815601e-02f,3.124811500e-02f,9.027448483e-03f,-3.542209044e-02f,5.713552609e-02f,3.652335331e-02f,-6.741855294e-02f,1.697181314e-01f,-1.423669755e-01f,6.362033635e-02f,2.973364294e-01f,-5.654546432e-03f,5.587726235e-01f,-5.209607482e-01f,-3.767205179e-01f,1.201457679e-01f,2.898867726e-01f,6.956852227e-02f,-9.449961782e-02f,-3.523340225e-01f,1.081907600e-01f,-1.544055641e-01f,6.864217669e-02f,-4.197689295e-01f,-1.021041945e-01f,-2.008009851e-01f,9.530792944e-03f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-2.362778485e-01f,2.973618507e-01f,-2.969589233e-01f,3.428413346e-02f,9.275793284e-02f,-2.362137884e-01f,8.079289645e-02f,-1.639147699e-01f,-1.919391453e-01f,1.032844186e-01f,-3.707563579e-01f,-3.738582879e-02f,-2.812003791e-01f,-1.381908264e-02f,-2.707938105e-02f,-1.203200594e-01f,9.008423239e-02f,-1.002868116e-01f,-1.056178287e-01f,5.494621396e-02f,-1.914279610e-01f,2.935761511e-01f,6.728793960e-03f,-6.782989949e-02f,1.845673844e-02f,-8.720634878e-02f,-3.674862981e-01f,9.226194024e-02f,2.923506871e-02f,5.405808613e-02f,-8.591970801e-02f,-1.218886733e+00f,1.210193112e-01f,5.781770498e-02f,6.501432508e-03f,3.394328952e-01f,-4.699026644e-01f,3.685465455e-01f,2.134812921e-01f,1.173401475e-01f,-1.398578137e-01f,-1.104035527e-01f,3.075810373e-01f,2.220366895e-01f,1.503708512e-01f,-5.041664094e-02f,6.731859874e-03f,7.768029720e-02f,-4.008486122e-02f,2.417711169e-01f,4.811064601e-01f,2.915462554e-01f,-2.427603453e-01f,-7.543507218e-02f,-1.390651166e-01f,-3.971926272e-01f,-3.262792528e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-4.167475551e-02f,5.104424059e-02f,-1.879139990e-02f,4.930341244e-01f,-6.230043769e-01f,3.826993331e-02f,4.120332003e-02f,-4.042505324e-01f,2.361737005e-02f,-6.672032923e-02f,-1.894582361e-01f,1.593992263e-01f,-3.990611434e-02f,1.036575437e-01f,6.493604928e-02f,-8.659567684e-02f,8.397385478e-02f,2.125972509e-01f,1.175439805e-01f,-1.343231201e-01f,1.173741519e-01f,6.808555126e-02f,-5.371001363e-01f,1.469502449e-01f,-1.022133455e-01f,1.042268425e-01f,6.759648025e-02f,-2.280439287e-01f,1.177369505e-01f,3.412221372e-02f,1.474920064e-01f,-6.776729971e-02f,6.131533906e-02f,1.035026684e-01f,-9.722051769e-02f,1.071415618e-01f,-6.507493258e-01f,2.050204277e-01f,3.305694163e-01f,1.591875255e-01f,-1.062278226e-01f,-4.286914319e-02f,5.780022740e-01f,-3.911365867e-01f,-1.873503625e-01f,5.688704848e-01f,-2.926259935e-01f,2.790834568e-02f,2.155775577e-01f,4.467084110e-01f,1.186885834e-01f,4.561086372e-02f,-9.690473974e-02f,2.658568025e-01f,1.117909513e-02f,6.012525409e-02f,-1.595738530e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-1.080524549e-01f,8.642517030e-03f,-8.531475067e-02f,-2.331651282e-03f,-4.159883037e-02f,2.833063155e-02f,9.904484451e-02f,1.022300273e-01f,9.538285434e-02f,9.110570885e-03f,-4.281599820e-02f,-1.384619623e-01f,4.902403429e-02f,-2.304888517e-02f,4.241812602e-02f,1.879481524e-01f,5.667214096e-02f,-1.558535099e-01f,1.627852023e-01f,-7.540017832e-03f,4.735946283e-02f,-2.358075418e-02f,2.885116637e-02f,1.587331593e-01f,-7.974395901e-02f,1.171508618e-02f,-7.912330329e-02f,4.536595568e-02f,3.360739052e-01f,1.226498410e-01f,5.933636799e-02f,1.942296326e-02f,9.518930316e-02f,1.136050671e-01f,-1.114723459e-02f,-6.325461715e-02f,9.415125102e-02f,7.664473355e-02f,3.443743587e-01f,1.027730629e-01f,-8.311793953e-02f,-9.753081016e-03f,2.454380989e-01f,1.488037757e-03f,1.574663259e-02f,1.113628447e-01f,-1.092455089e-01f,6.293907762e-02f,-6.776665151e-02f,1.276296843e-02f,3.261887282e-02f,1.598353088e-01f,1.185928658e-01f,3.927751631e-02f,1.529233903e-01f,-8.783538640e-02f,3.871929646e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,7.025746256e-02f,3.062480874e-02f,-6.961654127e-02f,2.174633741e-02f,7.626125962e-02f,1.250761151e-01f,-1.555391494e-02f,2.580301091e-02f,3.103346564e-02f,-1.138549969e-01f,8.431725204e-02f,4.631541669e-02f,1.758983359e-02f,-6.755363941e-02f,3.035093285e-02f,-9.228811413e-02f,7.869090140e-02f,5.199800432e-02f,-1.115860417e-01f,-4.447286576e-02f,-1.099462584e-01f,-6.446593441e-03f,-3.541511670e-02f,2.759432234e-02f,8.993312716e-03f,-5.167326704e-02f,-1.020601168e-01f,-1.190627962e-01f,-1.103267148e-01f,-1.982795633e-02f,2.467740700e-02f,8.671924472e-03f,-9.989164770e-02f,-5.921914801e-02f,-2.936068177e-02f,-6.428942829e-02f,-8.955465164e-04f,3.899100050e-02f,-1.549471915e-01f,-1.494591311e-02f,-6.595260650e-02f,-9.910301864e-02f,-4.508188739e-02f,1.389811095e-02f,-3.055693395e-02f,-7.339327037e-02f,2.214237303e-02f,-8.351089805e-02f,1.109642833e-01f,7.211309671e-02f,4.810970277e-02f,-1.385254879e-02f,-7.771728933e-02f,-6.953401864e-02f,-1.551964134e-02f,-4.296244308e-02f,7.759785652e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,5.768102128e-03f,1.616588384e-01f,2.477583736e-01f,5.335131884e-01f,-9.703481793e-01f,-6.084317341e-02f,-7.400828600e-02f,-1.342133731e-01f,2.162737697e-01f,3.736525774e-02f,-5.003962293e-02f,-9.226931632e-02f,-3.445950523e-02f,-1.576164365e-01f,-1.600288041e-02f,-6.764064729e-02f,1.035009474e-01f,9.776021540e-02f,-6.216384470e-02f,3.526964039e-02f,-5.641851574e-02f,-1.415024251e-01f,1.070041731e-01f,-2.590833902e-01f,-3.551034629e-02f,-4.179282114e-02f,-1.021526098e+00f,1.934019625e-01f,5.534431338e-02f,-4.935878888e-02f,1.885171831e-01f,-1.247726604e-01f,-3.484276235e-01f,-1.560778823e-02f,-9.840473533e-03f,8.421293646e-02f,9.717913717e-02f,-2.469317317e-01f,-4.108097777e-02f,1.797594130e-01f,-1.324398816e-01f,-7.440146059e-02f,1.096545607e-01f,-5.081677437e-01f,-7.621143758e-02f,2.020785511e-01f,-1.284432411e-01f,-3.098675609e-02f,-8.195744455e-02f,-5.877407268e-02f,-4.786537960e-02f,1.525805146e-02f,3.753463505e-03f,-2.588936500e-02f,-1.559766531e-01f,-1.254207548e-02f,4.688164219e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-1.932289079e-02f,-5.162125081e-02f,2.205484360e-01f,-1.361757219e-01f,2.192141414e-01f,1.067534834e-02f,-6.226515770e-02f,1.058211178e-01f,4.016926140e-02f,1.184494868e-01f,4.233934358e-02f,-4.125496373e-02f,4.742521420e-02f,5.336468667e-02f,2.958128415e-02f,-1.751534343e-01f,1.082428470e-01f,6.337507814e-02f,-4.142869264e-02f,-7.798700780e-02f,1.018734202e-01f,-1.913526654e-02f,6.241771206e-02f,8.164674789e-02f,-3.095562756e-02f,6.507308781e-02f,3.319796920e-01f,-1.421836466e-01f,-2.466679513e-01f,1.825471758e-03f,1.769668460e-01f,1.593447775e-01f,1.186866835e-01f,5.155628920e-02f,-1.435836107e-01f,2.445603162e-01f,1.580351591e-02f,3.704867512e-02f,8.447912335e-02f,-5.022868514e-03f,2.197964340e-01f,5.375138670e-02f,-1.084459275e-01f,2.897486389e-01f,-3.396114707e-02f,-1.671584994e-01f,3.263853490e-02f,-1.088424474e-01f,2.495718747e-01f,-2.616219409e-02f,-2.671718895e-01f,-8.561597764e-02f,1.197870728e-02f,-7.055419683e-02f,2.188130766e-01f,3.034108281e-01f,-7.509279251e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-1.029755771e-01f,6.488595903e-02f,-5.539089325e-04f,9.596446157e-02f,-1.734917909e-01f,-1.076782644e-01f,5.814741179e-02f,1.857472211e-01f,-6.847250462e-02f,4.877999425e-02f,-4.571777210e-02f,3.917445242e-02f,8.311940730e-02f,-7.354637235e-02f,1.440125853e-01f,3.220261037e-01f,8.012921363e-02f,-1.851954013e-01f,1.738488078e-01f,8.967085183e-02f,7.488715649e-02f,-1.998133808e-01f,-1.163431630e-01f,1.066178307e-01f,-7.138062268e-02f,4.752358794e-02f,-8.971248567e-02f,-1.308369935e-01f,4.226689041e-01f,2.159367353e-01f,-6.774453504e-05f,-8.482777327e-02f,1.648649722e-01f,-1.295901984e-01f,-8.683013916e-02f,3.851167858e-02f,2.017502785e-01f,5.312931724e-03f,4.215181470e-01f,-3.754196316e-02f,6.432810426e-02f,4.046424478e-02f,1.397154666e-02f,1.458549965e-02f,2.560603619e-02f,1.587071419e-01f,-1.956542581e-01f,-1.433900446e-01f,-1.067460924e-01f,1.087050587e-01f,1.678537130e-01f,8.189640194e-02f,1.506690085e-01f,-1.108041927e-01f,2.686005645e-02f,7.839319110e-02f,2.455116212e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-1.613417715e-01f,1.208564267e-02f,2.743065655e-01f,5.271937847e-01f,-1.433867812e-01f,-2.832030058e-01f,2.462702394e-01f,-1.779884994e-01f,2.069538087e-02f,-4.250017926e-02f,4.471516907e-01f,1.898309588e-01f,4.446243942e-01f,2.147234380e-01f,1.983127184e-02f,3.083775640e-01f,-1.812065765e-02f,5.562388301e-01f,-6.965415180e-02f,9.285613894e-02f,-3.117106259e-01f,-3.462837040e-01f,-2.895109355e-01f,1.875584871e-01f,-2.246732265e-02f,-5.537953973e-01f,7.652802765e-02f,-3.569224477e-01f,3.456802070e-01f,4.632790387e-02f,-3.419091925e-02f,-1.171360239e-01f,2.084734850e-02f,1.546780467e-01f,2.925236244e-03f,-3.213452101e-01f,1.919078827e-01f,1.930699497e-02f,-4.577715695e-02f,2.277464606e-02f,1.035002470e-01f,-1.368314959e-03f,3.941046000e-01f,-2.298745662e-01f,-7.285048719e-03f,2.476667315e-01f,-5.961917043e-01f,1.523413975e-02f,-6.172703207e-02f,-1.710653752e-01f,1.997107863e-01f,-2.917044461e-01f,1.661522686e-01f,-1.344339103e-01f,4.196174443e-02f,2.130395919e-02f,2.462782264e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-2.583709955e-01f,2.965210378e-02f,-3.812157810e-01f,-2.420055866e-02f,7.129433751e-02f,2.087958604e-01f,-2.328002602e-01f,1.736855805e-01f,-2.039662600e-01f,-5.990562215e-02f,2.035304308e-01f,6.837336421e-01f,1.701626778e-01f,6.259261817e-02f,-3.886730596e-02f,-2.959808707e-01f,1.967604458e-01f,1.570578814e-01f,1.692249626e-01f,-3.563053207e-03f,2.103441507e-01f,-4.594440013e-02f,2.949117124e-02f,1.215703562e-01f,1.259566867e-03f,4.320717156e-01f,-1.039352193e-01f,1.647616029e-01f,-1.600515991e-01f,-7.118899375e-02f,-2.091205493e-02f,-7.681329548e-02f,-8.488497697e-03f,-1.557790935e-01f,2.045699768e-02f,3.668499589e-01f,3.186672181e-02f,1.324457824e-01f,-5.135614425e-02f,-2.391148359e-01f,2.568528354e-01f,-1.035535783e-01f,-7.896616310e-02f,2.192524076e-01f,-2.003697306e-02f,-6.459531933e-02f,5.191868544e-02f,1.917589307e-01f,2.250400037e-01f,7.189460844e-02f,-1.539718509e-01f,4.116225988e-03f,-2.137964815e-01f,-2.045092732e-02f,-2.462020218e-01f,-3.154315650e-01f,2.333960682e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-2.616334520e-02f,-1.247654855e-01f,7.161437720e-02f,2.198034376e-01f,2.049264265e-03f,-3.537856787e-02f,1.071742177e-02f,6.938395649e-02f,5.823917687e-02f,2.024895251e-01f,1.216348484e-01f,3.780525625e-01f,-2.630383074e-01f,1.898641735e-01f,-1.293046027e-01f,-1.935573220e-01f,1.329890341e-01f,-1.053435802e-01f,2.622392178e-01f,-3.133844212e-02f,-2.545757592e-01f,-1.424863040e-01f,1.137581989e-01f,-9.189126641e-02f,1.464328021e-01f,-7.806529105e-02f,5.537596345e-02f,1.000717357e-01f,-2.087308168e-01f,1.640526950e-02f,2.411714196e-01f,-9.455189854e-02f,8.050718158e-02f,-1.711241603e-01f,-4.283078015e-02f,1.729888767e-01f,-2.891482115e-01f,2.210203111e-01f,1.380540878e-01f,3.577588499e-01f,3.378340304e-01f,-2.638932168e-01f,1.703403443e-01f,4.925702512e-02f,2.785593458e-02f,-2.132810652e-01f,2.522356436e-02f,-2.551003695e-01f,2.651958764e-01f,-1.514071375e-01f,1.629169658e-02f,-1.170305982e-01f,7.973073423e-02f,-7.216541469e-02f,1.595316641e-02f,5.090049282e-02f,1.155220792e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,5.094619840e-02f,-1.933351159e-02f,-2.390659787e-02f,-1.247896552e-01f,2.019028217e-01f,1.866502017e-01f,-7.862666249e-02f,3.283607662e-01f,4.131659865e-01f,6.214209273e-02f,-4.734725691e-03f,1.747526377e-01f,-4.492431879e-02f,-1.342674345e-01f,3.450194746e-02f,-7.366432250e-02f,4.551303014e-02f,5.538818985e-02f,-1.211343110e-01f,2.067044564e-02f,-7.265388221e-02f,2.821570039e-01f,1.748915911e-01f,-8.371815085e-02f,1.527909376e-02f,-6.522877514e-02f,1.427672356e-01f,5.603113770e-02f,-3.293438852e-01f,1.830347441e-02f,7.451429218e-02f,1.333389524e-02f,1.521503180e-01f,9.343593381e-03f,1.591121554e-01f,7.618352771e-02f,1.726208925e-01f,1.342523396e-01f,-2.968144715e-01f,1.274692733e-02f,6.310713291e-02f,7.808361948e-02f,6.928701419e-03f,1.915954053e-01f,7.643732429e-02f,-2.175286710e-01f,1.587037593e-01f,5.919837207e-02f,1.043745503e-01f,2.205228619e-02f,-1.340797246e-01f,-9.207846597e-03f,9.482423775e-03f,7.401674241e-02f,2.926131524e-02f,-2.233216353e-02f,2.638834417e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,1.304140240e-01f,-6.039028764e-01f,1.058724165e+00f,7.755943388e-02f,-5.193499327e-01f,1.625060588e-01f,1.798159331e-01f,-5.720062554e-02f,-1.329597682e-01f,-1.465862710e-02f,8.453479409e-02f,6.508696824e-02f,-5.234114826e-02f,1.521495432e-01f,7.096642256e-02f,-9.499897063e-02f,2.080549449e-01f,-1.846896112e-01f,5.536051095e-02f,-3.437045962e-02f,2.041958272e-01f,-6.661990285e-02f,-4.341003895e-01f,3.773617148e-01f,-7.233758923e-03f,4.281620085e-01f,4.253961444e-01f,-5.592212677e-01f,6.202873588e-02f,8.022700995e-02f,1.169143710e-02f,-1.248598322e-01f,-2.772907019e-01f,6.353527308e-02f,6.261844188e-03f,8.290097117e-02f,1.975234598e-01f,-3.165442050e-01f,1.350971218e-02f,2.273289710e-01f,9.929708391e-02f,4.681542143e-02f,-1.189420447e-01f,1.150438190e-01f,-3.447422758e-02f,2.855200469e-01f,2.348447442e-01f,-1.109220162e-01f,-5.579343438e-01f,2.286483794e-01f,-3.384520411e-01f,7.601988316e-02f,-1.738958657e-01f,1.318239328e-02f,6.381170452e-02f,1.120367289e+00f,2.957402496e-03f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-3.629672527e-01f,1.172754914e-01f,-1.313588321e-01f,2.639499903e-01f,-4.837764800e-02f,3.055280745e-01f,3.314876929e-02f,5.976858735e-01f,-2.547361553e-01f,1.741438508e-01f,-3.424405456e-01f,7.477119565e-02f,-1.223519295e-01f,-1.008820087e-01f,-2.579110675e-02f,6.637737155e-02f,-3.169351518e-01f,-1.786597073e-01f,-7.953671366e-02f,-1.131105125e-01f,2.513004541e-01f,2.428905964e-01f,4.540047646e-01f,-8.309137076e-02f,2.252375335e-02f,-3.903523088e-02f,1.517182589e-01f,2.610354424e-01f,3.215233982e-01f,6.657906622e-02f,-1.282706261e-01f,3.118314445e-01f,3.777165413e-01f,1.871716045e-02f,-5.674946308e-02f,-2.362906486e-01f,2.510035643e-03f,3.926183656e-02f,-2.310329825e-01f,1.862820685e-01f,3.483232483e-02f,2.536410987e-01f,1.920691133e-01f,3.934916854e-02f,-8.252046257e-02f,3.377476931e-01f,-2.221726440e-02f,3.272819519e-02f,1.907945424e-01f,1.742357761e-01f,3.976527750e-01f,1.117101088e-01f,-1.308975816e-01f,4.256553203e-02f,-1.098961309e-01f,-4.216471612e-01f,-2.833372653e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-4.632834345e-02f,8.365722746e-02f,-6.416295469e-02f,6.557126343e-02f,-2.833527140e-02f,5.783365294e-02f,7.740227133e-02f,2.738254890e-02f,-1.219316646e-01f,7.865705341e-02f,8.832120895e-02f,-2.721542725e-03f,2.458565170e-03f,-8.438314497e-02f,2.845281735e-02f,-3.516054153e-02f,2.347174846e-02f,6.967232912e-04f,-3.567145020e-02f,-5.489411415e-04f,-9.571787901e-03f,3.936303779e-02f,1.976889744e-02f,-3.657715023e-02f,-3.154325485e-02f,-1.364619732e-01f,-4.685848206e-02f,-1.215676889e-01f,4.785252362e-02f,-1.362213790e-01f,-1.464843303e-01f,-1.010887697e-01f,9.422444738e-03f,3.214085102e-02f,1.472921949e-02f,2.840150706e-02f,5.382168293e-02f,-1.390191019e-01f,-1.403203309e-01f,-1.427254677e-01f,-8.318524063e-02f,-1.268663704e-01f,-9.285201877e-02f,5.468383431e-02f,6.313779205e-02f,-4.108107463e-02f,-1.497345716e-01f,-4.810115322e-02f,4.061822593e-02f,-1.365450211e-02f,-6.800821424e-02f,4.353995249e-02f,2.171501890e-02f,-1.146437153e-01f,-9.041202068e-02f,-7.147748023e-02f,8.058026433e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-9.624101967e-02f,-2.195344493e-02f,3.637611121e-02f,-3.062900156e-02f,-8.576528728e-02f,8.429781348e-02f,-4.004297312e-03f,-5.362596363e-02f,3.907947242e-02f,-5.849513412e-02f,4.999189824e-02f,-8.938402683e-02f,9.699145705e-02f,9.509401768e-02f,-4.064476117e-02f,-1.747187786e-02f,-2.213749290e-02f,-9.828455746e-02f,-2.405960485e-02f,-7.715816796e-02f,-7.241298258e-02f,-1.075124964e-01f,4.284173623e-02f,-8.999837190e-02f,-8.604161441e-02f,-1.202610582e-01f,-1.057498381e-01f,3.174616024e-02f,8.548976481e-02f,8.846452087e-02f,-9.526242316e-02f,9.976341575e-02f,-4.386473820e-02f,-6.587965786e-02f,-1.132705286e-01f,-4.599749669e-02f,-1.166898571e-02f,2.138183452e-02f,-4.422956705e-02f,8.577612042e-02f,3.740485758e-02f,-8.470703661e-02f,-1.144940183e-01f,4.849093035e-02f,3.334202990e-02f,6.855776161e-02f,-9.680932760e-02f,-5.865125358e-02f,9.251524508e-02f,5.950596184e-02f,-1.156594083e-01f,8.554371446e-02f,-8.328492194e-02f,-1.123701259e-01f,7.368757855e-03f,4.225275293e-02f,-2.929038927e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,9.977595508e-02f,-1.559984237e-01f,6.558616459e-02f,-3.991482556e-01f,3.671829998e-01f,2.307915362e-03f,-3.845501244e-01f,-3.044344485e-02f,6.561646704e-03f,-1.098424345e-01f,1.454930007e-01f,-8.865644783e-02f,1.193846315e-01f,4.299691413e-03f,6.924844533e-02f,-3.601664603e-01f,1.335709076e-02f,1.300174627e-03f,1.996354163e-01f,-8.507762849e-02f,7.334073074e-03f,2.282279134e-01f,3.214856610e-02f,2.000256777e-01f,7.763189729e-03f,2.958288789e-02f,3.104617894e-01f,-3.298086226e-01f,-7.917778492e-01f,3.407724947e-02f,-4.029800743e-02f,-9.200504981e-03f,2.469130903e-01f,1.098638773e-01f,5.010495335e-02f,3.186704218e-02f,3.329532743e-01f,1.497980356e-01f,-5.293897390e-01f,1.668611020e-01f,2.067499012e-01f,-1.652031392e-01f,-2.405279130e-01f,9.621929377e-02f,-9.534014761e-02f,-2.542055547e-01f,1.744996011e-01f,1.828858703e-01f,3.725987300e-02f,-3.444549143e-01f,1.924391463e-02f,-1.756874323e-01f,1.311376989e-01f,-2.555424273e-01f,1.831097901e-01f,2.070242614e-01f,1.346019208e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,1.140168682e-01f,1.234601289e-01f,7.173667103e-02f,1.128998399e-01f,-8.310858160e-02f,4.285162315e-02f,-8.705532551e-02f,1.284415275e-01f,1.651295573e-01f,5.831780285e-02f,1.188710183e-01f,9.533839673e-02f,-1.735425182e-02f,-6.441412121e-02f,3.103131894e-03f,-6.030862778e-02f,-1.803086256e-03f,5.009533092e-02f,-9.442765266e-02f,1.660048068e-01f,1.120375916e-01f,1.386391521e-01f,-7.781092427e-04f,-1.325465553e-02f,-7.584092766e-02f,3.125969693e-02f,-4.316259548e-02f,-2.000293694e-02f,-1.925666183e-01f,-6.384652108e-03f,1.304629259e-03f,1.331076920e-01f,5.512029305e-02f,9.179672599e-02f,1.743319929e-01f,1.170666218e-01f,-3.289404884e-02f,8.067154884e-02f,-3.344781995e-01f,1.616008952e-02f,8.807368577e-02f,1.561275423e-01f,1.224023383e-02f,4.619800299e-02f,7.247123122e-02f,-6.412486732e-02f,2.623482645e-01f,4.843231663e-02f,1.917774826e-01f,-1.250725240e-01f,5.905983970e-02f,6.272996962e-02f,-2.805254050e-02f,-1.487455331e-02f,3.836233914e-02f,7.760006934e-02f,-1.088503450e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,4.290539324e-01f,1.431787461e-01f,4.401466250e-01f,5.184907913e-01f,-8.939561844e-01f,-1.958225220e-01f,1.655609114e-03f,-3.097071350e-01f,3.061018698e-02f,4.482840002e-02f,-1.454272121e-01f,-7.829979062e-02f,-1.914577186e-01f,-1.269051284e-01f,-8.062737063e-03f,-2.178438157e-01f,6.742568314e-02f,-5.104947090e-01f,-9.656017274e-02f,1.217346936e-01f,-3.231616318e-02f,-3.631646037e-01f,2.081695944e-01f,3.064867295e-02f,3.682280704e-02f,-1.286334246e-01f,-2.186783403e-01f,6.182381883e-02f,-4.197635949e-01f,-2.022179030e-02f,3.925942779e-01f,7.523015887e-02f,3.576473892e-02f,-2.297830209e-02f,-7.162271533e-03f,1.027979627e-01f,5.969341844e-02f,2.546405792e-02f,4.618712366e-01f,8.085035533e-02f,4.471392557e-02f,-1.350760162e-01f,4.332222641e-01f,-2.032236606e-01f,-5.012352020e-02f,-3.394937143e-02f,1.030305028e-01f,2.289877739e-03f,-3.506013751e-01f,-2.889136970e-01f,-5.024742484e-01f,1.548668146e-01f,2.402337641e-01f,-1.559635997e-01f,1.670658588e-01f,4.590401649e-01f,9.949459927e-04f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,3.263221681e-01f,1.767016947e-01f,6.623528600e-01f,4.141492248e-01f,-2.662910819e-01f,-1.469076127e-01f,2.512282729e-01f,-2.513854802e-01f,-1.365066227e-02f,-5.426396057e-02f,1.729351133e-01f,1.993362457e-01f,6.132181734e-02f,-6.766641885e-02f,2.334466577e-02f,-8.716297150e-02f,-4.055718780e-01f,-3.109443560e-02f,4.094027728e-02f,-4.366566986e-02f,1.000232156e-02f,9.294955432e-02f,1.167115718e-01f,8.172441274e-02f,1.109949127e-02f,-3.406035006e-01f,-2.629175186e-01f,2.861572206e-01f,-2.241715491e-01f,5.246331543e-02f,-8.089478314e-02f,-7.009000182e-01f,-1.745680720e-01f,2.221222222e-02f,1.791521162e-02f,2.377095819e-01f,-4.337139726e-01f,-3.678898513e-02f,1.725685894e-01f,1.884433776e-01f,-4.333792031e-01f,8.662848920e-02f,3.722683787e-01f,-1.698257625e-01f,-1.042537913e-01f,-2.830335684e-02f,-2.934485078e-01f,-6.385366619e-02f,2.034990937e-01f,-1.658947021e-01f,-2.942886949e-01f,2.149131298e-01f,2.654585838e-01f,5.045599118e-02f,1.167763025e-01f,5.233032703e-01f,2.080514580e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,-1.675707400e-01f,-4.309636056e-01f,2.748184651e-02f,4.028850794e-01f,-1.204885319e-01f,4.667407274e-01f,-3.463781253e-02f,1.199627444e-01f,1.639358997e-01f,1.076503694e-01f,1.557961404e-01f,4.438721538e-01f,-2.094328962e-02f,2.956653945e-02f,1.409495994e-02f,4.028161764e-01f,-2.388639450e-01f,6.038936973e-02f,-8.115796745e-02f,-4.307615105e-03f,6.221179664e-02f,2.538302839e-01f,-3.678226471e-01f,3.678747267e-02f,-2.053266205e-02f,3.193506598e-02f,7.971712202e-02f,-4.061449766e-01f,4.585395455e-01f,9.578266181e-03f,2.248611003e-01f,8.084316552e-02f,1.475762874e-01f,-6.619902700e-02f,4.784924537e-02f,-5.576533675e-01f,-3.185046315e-01f,1.908322126e-01f,1.234360099e+00f,3.999731243e-01f,-1.945603043e-01f,1.891293973e-01f,9.321121573e-01f,-2.908917665e-01f,-1.676383018e-01f,5.913348198e-01f,-9.904178232e-02f,2.923529409e-02f,-1.272382289e-01f,1.311485469e-01f,-1.179871187e-01f,1.811771542e-01f,-1.203847826e-01f,1.115709022e-01f,8.329527080e-02f,1.421257108e-01f,1.134104058e-01f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,5.277282000e-01f,6.024093032e-01f,1.800047117e-03f,8.463407755e-01f,-2.882829905e-01f,-1.961949766e-01f,3.123761117e-01f,-4.838271067e-02f,-1.182880625e-01f,1.797884256e-01f,-3.665723205e-01f,-2.621784210e-01f,2.365058986e-03f,-1.304962337e-01f,-6.888002902e-02f,-2.360854745e-01f,-9.131161869e-02f,-7.430564612e-02f,6.496386975e-02f,7.542335987e-02f,-3.738285303e-01f,-2.349043638e-01f,2.401748439e-03f,-8.827508986e-02f,-2.963617817e-02f,-4.591242373e-01f,-2.166539431e-01f,-1.498330534e-01f,-5.055325106e-02f,1.867279261e-01f,-5.631929040e-01f,-1.364709586e-01f,8.111701161e-02f,1.000948101e-01f,6.601274759e-02f,-1.719609499e-01f,4.293073714e-02f,1.475948691e-01f,1.958612055e-01f,-5.175077170e-02f,-3.634540364e-02f,5.127859488e-02f,-2.889947891e-01f,-1.532897800e-01f,1.116229966e-01f,2.750895172e-02f,-2.234874517e-01f,6.452405453e-02f,1.102065369e-01f,2.988517284e-01f,-6.308853626e-02f,-2.689052559e-02f,2.582082152e-01f,3.965389356e-02f,1.239604652e-01f,2.258510888e-01f,7.691099495e-02f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f,0.000000000e+00f};
static const float VALUE_B0[32]={3.824043870e-01f,-9.499850124e-02f,3.788867965e-02f,2.187544107e-02f,-8.976018429e-02f,6.334800273e-02f,-5.376125872e-02f,4.314887896e-02f,-6.088468060e-02f,1.658276916e-01f,-1.264688373e-01f,2.572761104e-02f,-6.211555377e-02f,1.217977181e-01f,-8.557949960e-02f,9.006886184e-02f,2.413618378e-02f,-7.286425214e-03f,-4.428967461e-02f,-1.085251421e-01f,6.127575878e-03f,1.810415834e-01f,5.706392601e-02f,-6.163980439e-02f,-9.555681050e-02f,-7.459151745e-02f,1.657563895e-01f,1.721495092e-01f,-7.771568745e-02f,-2.113855816e-02f,1.140571311e-01f,-4.950583726e-02f};
static const float VALUE_W1[32]={1.582452953e-01f,-7.958194613e-01f,-8.009541035e-01f,9.753332734e-01f,-9.216461778e-01f,2.973458730e-02f,6.490959525e-01f,1.919865310e-01f,-8.340839148e-01f,7.400537729e-01f,5.278265476e-01f,-8.654005527e-01f,5.762158632e-01f,-2.612779737e-01f,-1.035122899e-03f,1.440500259e+00f,4.595361054e-01f,-2.530033290e-01f,-9.084172249e-01f,-8.447163105e-01f,4.787698388e-01f,3.252868652e-01f,1.328001261e+00f,-8.364803791e-01f,1.288375258e-01f,7.514758501e-03f,8.868210316e-01f,1.744150370e-01f,-6.905173063e-01f,-1.031177402e+00f,9.339057207e-01f,-8.078611493e-01f};

// Same 64x32 weights and accumulation order; contiguous hidden-unit SIMD.
static float value_head(const float* x) {
    static const auto transposed=[] {
        std::array<float,2048> w{};
        for(int k=0;k<64;k++)for(int j=0;j<32;j++)w[k*32+j]=VALUE_W0[j*64+k];
        return w;
    }();
    float h[32];for(int j=0;j<32;j++)h[j]=VALUE_B0[j];
    for(int k=0;k<64;k++)for(int j=0;j<32;j++)h[j]+=transposed[k*32+j]*x[k];
    float value=0;for(int j=0;j<32;j++)if(h[j]>0)value+=VALUE_W1[j]*h[j];
    return value;
}

static float neural_value(const State&s){
 float x[64];value_features(s,0,x);float a=value_head(x);value_features(s,1,x);float b=value_head(x);
 return std::tanh(a-b);
}

static int evaluate(const State& s) {
    if (s.done) {
        if (s.winner < 0) return 0;
        return (s.winner == 0 ? 6000 : -6000) + 10 * (s.base_hp[0] - s.base_hp[1]);
    }
    static const int tv[3] = {SP_TV, SP_TV_FIRE, SP_TV};
    int e[2] = {0, 0};
    int8_t evocc[CELLS];std::memset(evocc,-1,sizeof(evocc));for(int i=0;i<6;i++)if(s.tank[i].alive)evocc[s.tank[i].pos]=i;
    for (int seat = 0; seat < 2; ++seat) {
        e[seat] += 100 * s.base_hp[seat] + 8 * s.kills[seat] + 2 * s.stars_eaten[seat];
        for (int k = 0; k < M.n_key[seat]; ++k)
            if (s.grid[M.key_cell[seat][k]] != BRICK && root_grid[M.key_cell[seat][k]] == BRICK) e[seat] += M.key_val[seat][k];
    }
    for (int i = 0; i < 6; ++i) {
        const Tank& t = s.tank[i];
        const int seat = i / 3, type = i % 3;
        if (t.alive) {
            e[seat] -= tv[type] * tank_cost(s, i, t.pos) / 4;
            e[seat] += 15 * (t.hp - 1) + SP_LVV * t.level;
        } else {
            e[seat] -= tv[type] * (tank_cost(s, i, s.spawn[i]) + 4 * respawn_in(s, i)) / 4 + 10;
        }
    }
#if SP_STARS
    // 星星：没满级的车离星星越近越好（吃到之后体现在等级和血量上）。
    const int mask = s.star_mask | soon_mask;
    if (mask) {
        for (int k = 0; k < s.n_supply; ++k) {
            if (!((mask >> k) & 1)) continue;
            for (int seat = 0; seat < 2; ++seat) {
                int best = BIG;
                for (int i = 3 * seat; i < 3 * seat + 3; ++i) {
                    if (!s.tank[i].alive || s.tank[i].level >= s.P.max_level) continue;
                    int a = M.sup[k][s.tank[i].pos];
                    if (i % 3 == FAST) a = a * 5 / 8;
                    if (a < best) best = a;
                }
                const int bonus = SP_STARW - best / 2;                 // 每远一回合少 2 分
                if (bonus > 0) e[seat] += bonus;
            }
        }
    }
#endif
    // 还在飞的子弹：沿弹道看它会先碰到什么。
    for (int b = 0; b < s.n_bullets; ++b) {
        const Bullet& bl = s.bullet[b];
        int c = bl.pos;
        for (int k = 1; k <= 8; ++k) {
            c += DIR_DELTA[bl.dir];
            const uint8_t g = s.grid[c];
            if (g == WATER) continue;
            if (g == BASE0 || g == BASE1) { e[g - BASE0] -= 70; break; }
            if (g != EMPTY) break;
            int v=evocc[c];
            if (v >= 0) {
                e[v / 3] -= k <= 2 ? 20 : 5;
                break;
            }
        }
    }
    return e[0] - e[1];
}

// ====================================================================
// 候选命令：给一辆车列出值得搜的命令，按粗略的好坏排好序（没访问过的按这个顺序先试）
// ====================================================================
static int8_t scan_occ[CELLS];
static uint8_t scan_bullet[CELLS];
static float prior_exp[2049];
static void prepare_scan(const State&s){
 std::memset(scan_occ,-1,sizeof(scan_occ));std::memset(scan_bullet,0,sizeof(scan_bullet));
 for(int i=0;i<6;i++)if(s.tank[i].alive)scan_occ[s.tank[i].pos]=i;
 for(int b=0;b<s.n_bullets;b++)scan_bullet[s.bullet[b].pos]|=1<<s.bullet[b].dir;
}
struct Scan {             // 朝一个方向看过去的结果
    bool useful;          // 这个方向开炮有没有意义
    int prior;            // 开炮的先验分
    bool danger;          // 这个方向有威胁（子弹正飞来，或 2 格内有能开炮的敌车）
};

static Scan scan_dir(const State& s, int i, int d) {
    Scan r{false, 0, false};
    const int seat = i / 3;
    int bricks = 0, c = s.tank[i].pos;
    for (int k = 1; k <= 14; ++k) {
        c += DIR_DELTA[d];
        const uint8_t g = s.grid[c];
        if (g == WATER) continue;
        if (g == BRICK) {
            if (bricks == 0) {
                bool key = false;
                for (int q = 0; q < M.n_key[seat]; ++q) key |= M.key_cell[seat][q] == c;
                if (key && k <= 6) { r.useful = true; r.prior = r.prior > 62 ? r.prior : 62; }
                else if (k <= 2) { r.useful = true; r.prior = r.prior > 30 ? r.prior : 30; }
            }
            ++bricks;
            continue;
        }
        if (g == BASE0 + (1 - seat)) {
            if (k <= 6) { r.useful = true; const int p = 96 - 10 * bricks; r.prior = r.prior > p ? r.prior : p; }
            break;
        }
        if (g == STEEL) {
            if (bricks == 0 && k <= 2 && s.tank[i].level >= 2 && s.P.level2_steel) { r.useful = true; r.prior = r.prior > 25 ? r.prior : 25; }
            break;
        }
        if (g != EMPTY) break;
        if (bricks) continue;
        if ((scan_bullet[c] & (1<<(d^1))) && k<=6) {
                r.useful = true;
                r.danger = true;
                r.prior = r.prior > 85 ? r.prior : 85;
            }
        int v = scan_occ[c];
        if (v >= 0) {
            if (v / 3 == seat) break;                         // 队友挡着
            r.useful = true;
            const bool armed = can_fire(s, v);
            if (k <= 2 && armed) r.danger = true;
            const int p = k <= 2 ? (armed ? 88 : 99) : (k <= 4 ? 64 : 44);
            r.prior = r.prior > p ? r.prior : p;
            break;
        }
        // 敌车就在炮线旁边一格，可能走进来：预判射击。
        if (k <= 5 && !r.useful) {
            const int side = (d < 2) ? 1 : STRIDE;
            for(int cell : {c+side,c-side})if(scan_occ[cell]>=0 && scan_occ[cell]/3!=seat){r.useful=true;r.prior=36;}
        }
    }
    return r;
}

struct Node {
    int32_t visits;
    float total;                // 经过这个节点的所有模拟的分数之和
    
    int32_t n[MAX_ACTS];
    float sum[MAX_ACTS];
    float p[MAX_ACTS];          // 先验概率
    Cmd act[MAX_ACTS];
    uint8_t n_acts;
};

static int goal_cost(const State& s, int i, int pos) {
    if(opening_active[i])return M.sup[opening_star[i]][pos];       // 走位的先验：离目标（或星星）多远
    if (hunt_target[i] >= 0) return hunt_map[hunt_target[i] % 3][pos];
    if (holding[i]) return post_map[pos];
    int a = atk_of[i][pos];
#if SP_STARS
    if (s.tank[i].level < s.P.max_level && (s.star_mask | soon_mask)) {
        for (int k = 0; k < s.n_supply; ++k)
            if (((s.star_mask | soon_mask) >> k) & 1) {
                const int b = M.sup[k][pos] + 8;
                if (b < a && M.sup[k][pos] <= SP_STAR_REACH) a = b;
            }
    }
#else
    (void)s;
#endif
    return a;
}

static void build_actions(Node& nd, const State& s, int i) {
    Cmd list[N_CMDS];
    const int n = legal_commands(s, i, list);
    const Tank& t = s.tank[i];
    Scan sc[4];
    bool danger = false;
    for (int d = 0; d < 4; ++d) { sc[d] = scan_dir(s, i, d); danger |= sc[d].danger; }
    const int here = goal_cost(s, i, t.pos);
    Cmd acts[N_CMDS];
    int prior[N_CMDS], m = 0;
    int ends[N_CMDS], n_ends = 0;     // 两步命令按终点去重
    for (int k = 0; k < n; ++k) {
        const Cmd c = list[k];
        int p;
        if (c == STAY) {
            p = 46;
        } else if (is_fire(c)) {
            if (!sc[c - 5].useful) continue;
            p = sc[c - 5].prior;
        } else if (is_move(c)) {
            const int to = t.pos + DIR_DELTA[c - 1];
            p = 50 + (here - goal_cost(s, i, to)) * 2;
            if (danger && !sc[c - 1].danger && !sc[(c - 1) ^ 1].danger) p += 30;      // 横向让开
        } else {
            const int d1 = (c - 9) >> 2, d2 = (c - 9) & 3;
            const int mid = t.pos + DIR_DELTA[d1];
            if (s.grid[mid] != EMPTY) continue;                     // "原地等一小步再走"：用处不大，不搜
            const int to = mid + DIR_DELTA[d2];
            if (to == t.pos) {                                      // 出去再回来：只在有威胁时才值得（晃过一发子弹）
                if (!danger || sc[d1].danger || sc[d2].danger) continue;
                p = 70;
            } else {
                if (s.grid[to] != EMPTY) continue;                  // 第二步多半走不成，等于一步
                bool dup = false;
                for (int q = 0; q < n_ends; ++q) dup |= ends[q] == to;
                if (dup) continue;                                  // 同一个终点的两条折线只留一条
                ends[n_ends++] = to;
                p = 52 + (here - goal_cost(s, i, to)) * 2;
                if (danger) p += 20;
            }
        }
        acts[m] = c;
        prior[m++] = p;
    }
    // 按先验分从高到低排，最多留 MAX_ACTS 条。
    for (int a = 1; a < m; ++a)
        for (int b = a; b > 0 && prior[b] > prior[b - 1]; --b) {
            const int tp = prior[b]; prior[b] = prior[b - 1]; prior[b - 1] = tp;
            const Cmd tc = acts[b]; acts[b] = acts[b - 1]; acts[b - 1] = tc;
        }
    if (m > MAX_ACTS) m = MAX_ACTS;
    nd.n_acts = uint8_t(m);
    float z = 0;
    for (int a = 0; a < m; ++a) {
        nd.act[a] = acts[a];
        
        nd.n[a] = 0;
        nd.sum[a] = 0;
        const int diff=prior[0]-prior[a];nd.p[a]=diff<=2048?prior_exp[diff]:std::exp(float(-diff)/float(SP_PRIOR_T));
        z += nd.p[a];
    }
    for (int a = 0; a < m; ++a) nd.p[a] /= z;
}

// ====================================================================
// 树外策略：便宜的规则走法（朝进攻位走，看得到目标就开炮）
// ====================================================================
static uint64_t rng_state;
static inline uint32_t rng() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return uint32_t(rng_state >> 16);
}

static Cmd rollout_cmd(const State& s, int i) {
    const Tank& t = s.tank[i];
    const int seat = i / 3;
    const uint32_t r = rng();
    const bool armed = can_fire(s, i);
    if (armed && (r & 7) != 0) {
        for (int d = 0; d < 4; ++d) {
            int c = t.pos, bricks = 0;
            for (int k = 1; k <= 4; ++k) {
                c += DIR_DELTA[d];
                const uint8_t g = s.grid[c];
                if (g == WATER) continue;
                if (g == BRICK) { if (++bricks > 1) break; continue; }
                if (g == BASE0 + (1 - seat)) return cmd_fire(d);
                if (g != EMPTY) break;
                if (bricks) continue;
                int v = scan_occ[c];
                if (v >= 0) {
                    if (v / 3 != seat && k <= 2) return cmd_fire(d);
                    break;
                }
            }
        }
    }
    const int16_t* dist = hunt_target[i] >= 0 ? hunt_map[hunt_target[i] % 3] : (holding[i] ? post_map : atk_of[i]);
    int pos = t.pos, dirs[2] = {-1, -1};
    const int steps = (i % 3 == FAST) ? 2 : 1;
    for (int k = 0; k < steps; ++k) {
        int best = -1, best_cost = goal_cost(s,i,pos);
        if (((r >> (8 + 4 * k)) & 15) < 16 / SP_ROLLOUT_RANDOM) {
            const int d = (r >> (16 + 2 * k)) & 3;
            if (s.grid[pos + DIR_DELTA[d]] == EMPTY) best = d;
        } else {
            for (int d = 0; d < 4; ++d) {
                const int to = pos + DIR_DELTA[d];
                if (goal_cost(s,i,to) < best_cost) { best_cost = goal_cost(s,i,to); best = d; }
            }
        }
        if (best < 0) break;
        if (s.grid[pos + DIR_DELTA[best]] == BRICK) {
            if (k == 0 && armed) return cmd_fire(best);
            break;
        }
        dirs[k] = best;
        pos += DIR_DELTA[best];
    }
    if (dirs[0] < 0) return STAY;
    if (dirs[1] < 0) return cmd_move(dirs[0]);
    return cmd_move2(dirs[0], dirs[1]);
}

// ====================================================================
// 搜索
// ====================================================================
static constexpr int POOL_CAPACITY=32768;
static std::unique_ptr<Node[]> pool;
static int pool_used;
static long long total_steps;           // 统计用：本局累计前推次数

static inline int new_node() {
    if (pool_used >= POOL_CAPACITY) return -1;
    Node& nd = pool[pool_used];
    nd.visits = 0;
    nd.total = 0;
    nd.n_acts = 0;
    return pool_used++;
}

#define ACTOR_BLEND 0.50f
// Cheap public-state features, evaluated only once per living root tank.
constexpr int ACTOR_F=160,ACTOR_H=32,ACTOR_A=25;
static int canonical_action(int a,int side){
 if(!side)return a;
 if(a>=1&&a<=4){int d=a-1;return 1+(d<2?(d^1):d);}
 if(a>=5&&a<=8){int d=a-5;return 5+(d<2?(d^1):d);}
 if(a>=9){int u=(a-9)/4,v=(a-9)%4;return 9+4*(u<2?(u^1):u)+(v<2?(v^1):v);}
 return a;
}
static void actor_features(const State&s,int id,const Node&nd,float*x){
 std::fill(x,x+ACTOR_F,0.f);
 int side=id/3;value_features(s,side,x);x[64+id%3]=1;
 for(int z=0;z<2;z++)for(int t=0;t<3;t++){
  int j=3*(z?1-side:side)+t,k=67+6*z+2*t;
  int p=s.tank[j].alive?s.tank[j].pos:s.spawn[j];
  x[k]=cell_x(p)/14.f;x[k+1]=(side?14-cell_y(p):cell_y(p))/14.f;
 }
 for(int a=0;a<25;a++)x[79+a]=-2.f;
 for(int k=0;k<nd.n_acts;k++){
  int a=int(nd.act[k]),ca=canonical_action(a,side);
  x[79+ca]=std::max(-16.f,std::log(std::max(1e-9f,nd.p[k])))/8.f;
  x[104+ca]=1;
  int p=s.tank[id].pos,got=0,wanted=0;
  auto move=[&](int d){wanted++;int q=p+DIR_DELTA[d];if(s.grid[q]==EMPTY&&tank_at(s,q)<0){p=q;got++;}};
  if(a>=1&&a<=4)move(a-1);
  if(a>=9){move((a-9)/4);move((a-9)%4);}
  x[129+ca]=wanted?float(got)/wanted:1.f;
 }
 for(int d=0;d<4;d++)x[154+(side&&d<2?(d^1):d)]=scan_dir(s,id,d).danger?1.f:0.f;
 x[158]=bullets_out(s,id)/2.f;x[159]=holding[id]?1.f:0.f;
}

static const float ACT_W0[5120]={
8.204516768e-02f,1.335584223e-01f,1.386637241e-01f,-3.303973973e-01f,7.650873065e-02f,6.599677354e-02f,-2.857459486e-01f,-1.363397539e-01f,3.994093239e-01f,3.344371170e-02f,3.202815652e-01f,3.001751006e-01f,-3.018088341e-01f,1.859109402e-01f,1.730972230e-01f,4.007648230e-01f,
-1.415720396e-02f,5.417750031e-02f,2.110634148e-01f,-4.728756472e-02f,2.388954014e-01f,1.505967462e-03f,-2.147894353e-02f,-1.903168857e-01f,-1.485299021e-01f,5.280293822e-01f,-1.001077741e-01f,1.483665556e-01f,-5.597770214e-03f,-1.085373387e-01f,-8.399371058e-02f,-7.667791098e-02f,
-2.025525272e-01f,-8.445508033e-02f,-9.083566815e-02f,7.878385484e-02f,-2.895607948e-01f,-6.599626970e-03f,-1.328488719e-02f,1.560782939e-01f,-4.608543590e-02f,-2.346466333e-01f,1.155701838e-02f,2.336836420e-02f,-1.754535735e-01f,2.466614991e-01f,-1.649580300e-01f,-1.582673788e-01f,
-1.561651975e-01f,3.126430139e-02f,-4.752645642e-02f,-3.908536211e-02f,-1.895357370e-01f,1.964388788e-02f,2.649422586e-01f,2.442445606e-01f,1.788315177e-01f,5.529450625e-02f,-1.290562470e-02f,-7.636117190e-02f,-2.684622537e-03f,2.864356339e-02f,3.204005212e-02f,-6.618712842e-02f,
-2.180215716e-01f,1.269696504e-01f,1.397798061e-01f,1.497038752e-01f,-8.533903956e-02f,7.768331468e-02f,1.810162514e-01f,-2.049523294e-01f,1.171745658e-01f,-2.624546885e-01f,2.345584482e-01f,8.011797443e-03f,-5.201921612e-02f,1.349140704e-01f,-2.735756338e-01f,-3.411061466e-01f,
1.167356595e-02f,-5.868370179e-03f,-5.599295720e-02f,2.473345771e-02f,9.504980594e-02f,9.616053849e-02f,-5.393614247e-02f,-2.013868187e-03f,-2.314443700e-02f,8.048629761e-02f,-6.858871877e-02f,-1.122494414e-02f,-6.690911949e-03f,-7.574427291e-04f,7.547891140e-02f,-5.956642330e-03f,
-3.713969514e-02f,3.656489030e-02f,1.129185036e-01f,-7.051355392e-02f,6.872695684e-02f,3.637550399e-02f,1.050929353e-02f,1.612862013e-02f,5.465954076e-03f,7.878945768e-02f,7.899011485e-03f,1.170466393e-01f,1.391005609e-02f,1.286042631e-01f,1.124199927e-01f,-1.214878410e-01f,
-9.968699515e-02f,-1.055081859e-01f,1.778147370e-02f,-1.623976231e-01f,-2.887208760e-01f,2.761512101e-01f,-2.711533755e-02f,6.142175943e-02f,-7.207620889e-02f,1.540414840e-01f,1.745789498e-01f,1.630309522e-01f,-3.783161342e-01f,7.913931459e-02f,-6.122631207e-02f,-3.781732619e-01f,
1.133477539e-01f,9.968223982e-03f,5.464985967e-01f,1.062993556e-01f,-1.177991405e-01f,1.134822331e-02f,-1.560485177e-02f,9.522725642e-02f,-1.411172152e-01f,-8.167076111e-02f,1.985406280e-01f,1.542476267e-01f,-4.610680789e-02f,2.787168026e-01f,2.855027318e-01f,-2.540523186e-02f,
1.278359890e-01f,2.173522301e-02f,2.439524531e-01f,9.507726133e-02f,-1.508346498e-01f,-1.986482143e-01f,6.226405501e-01f,-1.272220165e-01f,-3.669839501e-01f,2.516708896e-02f,7.782287002e-01f,6.542018652e-01f,-2.635217309e-01f,-5.846184492e-01f,-2.750616074e-01f,2.649311125e-01f,
7.638832927e-02f,-2.238343656e-02f,5.912732482e-01f,-4.532538652e-01f,4.540553316e-02f,-9.640317410e-03f,-6.771685183e-02f,-1.751223952e-01f,3.745438159e-01f,8.920007944e-02f,7.254775148e-03f,-6.181299686e-02f,-3.821433187e-01f,-3.225067854e-01f,-8.466858417e-03f,2.368024141e-01f,
-2.263432294e-01f,-3.890771270e-01f,8.699508756e-02f,1.056200918e-02f,-6.758081168e-02f,-8.039210737e-02f,-9.638574906e-03f,-2.128378488e-02f,1.388277411e-01f,3.839055002e-01f,-9.786997736e-02f,-1.291187853e-01f,-7.192420214e-02f,1.780050248e-02f,-4.103701934e-02f,-1.186760813e-01f,
-1.380458176e-01f,8.674230427e-02f,1.559937149e-01f,6.706132293e-01f,-1.889173314e-02f,5.186921731e-02f,-1.028164774e-01f,2.650557160e-01f,1.607096046e-01f,1.357043386e-01f,1.113942116e-01f,-2.707474232e-01f,-2.375607640e-01f,3.185791373e-01f,-8.020257950e-02f,6.763053685e-02f,
-9.243237972e-02f,-1.082205549e-01f,2.418182231e-02f,2.039545476e-01f,-1.276513040e-01f,3.086529970e-01f,4.596465230e-01f,7.054048777e-01f,5.409032106e-01f,8.830413222e-03f,-4.398178309e-02f,-4.006513953e-02f,-1.116938144e-02f,8.734926581e-03f,-4.269999638e-02f,-5.029891804e-02f,
-1.733900309e-01f,3.078316748e-01f,-9.371277690e-02f,-7.334012538e-02f,-1.843209937e-02f,1.471523941e-01f,9.485822171e-02f,-9.136022627e-02f,-3.715911298e-04f,3.791215271e-02f,-1.001985893e-01f,4.262496904e-02f,-1.678277105e-01f,-4.602041841e-02f,-6.492821127e-02f,-3.050013483e-01f,
-3.357520700e-01f,-6.701180339e-02f,4.912007600e-02f,4.682256281e-02f,-1.050530970e-01f,5.415068194e-02f,5.595689267e-02f,-1.403193572e-03f,-5.370705575e-02f,-6.366937608e-02f,-2.775597014e-02f,-1.871591359e-01f,9.482360631e-02f,2.295120247e-02f,-1.922087930e-02f,6.438039243e-02f,
7.358917594e-02f,4.724042863e-02f,-4.026671126e-02f,6.097606942e-02f,7.083687186e-02f,-6.304578483e-02f,-6.059281155e-02f,4.082725570e-02f,-4.555064440e-02f,-7.597215474e-02f,7.873933762e-03f,7.746364921e-02f,1.120826453e-01f,-9.510385245e-02f,-3.558047116e-02f,-8.723226190e-02f,
-1.689042598e-01f,-8.696507663e-02f,-2.065283805e-02f,-3.037899733e-02f,-3.569458425e-01f,3.787460923e-01f,1.161506847e-01f,3.797209635e-02f,2.058102489e-01f,3.451975062e-02f,-2.169781327e-01f,5.142458156e-02f,1.075802371e-01f,7.866413146e-02f,2.724909969e-02f,1.324800551e-01f,
9.966404736e-02f,4.491857439e-02f,3.299040496e-01f,6.851562858e-02f,-2.564575896e-02f,3.901204839e-02f,-6.710399687e-02f,6.637168676e-02f,-2.416843083e-03f,-1.296834946e-01f,1.679538786e-01f,-1.913325042e-01f,1.140645444e-01f,1.392514557e-01f,4.108868241e-01f,7.578056306e-02f,
-8.073730394e-03f,2.697836757e-01f,3.012625277e-01f,-1.554829776e-01f,-1.266116947e-01f,1.700729877e-01f,-1.706134379e-01f,-5.351945385e-02f,1.140942127e-01f,-1.651262343e-01f,6.590019166e-02f,-4.928753376e-01f,1.517388076e-01f,4.443470538e-01f,-2.046312839e-01f,3.725888580e-02f,
5.913684145e-02f,3.628785722e-03f,3.715038002e-01f,-5.568135977e-01f,1.568453312e-01f,-3.702953160e-01f,-9.333126247e-02f,-1.411883831e-01f,-9.569019312e-04f,4.823831550e-04f,-1.719777882e-01f,-1.585239917e-01f,-1.643465906e-01f,8.968240768e-02f,-6.069700792e-02f,2.551955879e-01f,
2.274328321e-01f,1.279741758e-03f,-9.926934540e-02f,-4.931352567e-03f,-1.290725823e-02f,-2.672417462e-01f,-5.399174057e-03f,-1.002092734e-01f,-7.187611610e-02f,9.661568911e-04f,1.555194557e-01f,1.272772849e-01f,-9.713423438e-03f,4.107600823e-02f,-1.634118706e-01f,-3.822551966e-01f,
-3.188126907e-02f,-4.255365729e-01f,1.199951917e-01f,-2.235644758e-01f,-3.121672571e-01f,-1.981535405e-01f,-1.364125609e-01f,1.154092699e-01f,-4.243049920e-01f,1.907706819e-02f,-1.041017696e-01f,6.256866455e-02f,-1.054769568e-02f,-2.827038243e-02f,-1.111315638e-01f,-3.292231634e-02f,
-3.199434280e-02f,-4.517914727e-02f,5.968674272e-02f,1.928026825e-01f,2.679876611e-02f,1.218757108e-01f,8.139306903e-01f,6.660514474e-01f,-2.521290779e-01f,2.711644582e-02f,-3.855518997e-02f,-3.429344669e-02f,1.452615671e-02f,-6.301652640e-02f,-5.369412899e-02f,3.496727347e-02f,
1.770320982e-01f,1.178987548e-01f,-1.826505065e-01f,1.343037486e-01f,-1.165647134e-01f,-5.324233696e-02f,5.196888372e-02f,3.153064847e-01f,1.036579162e-01f,-5.495177209e-02f,1.373885721e-01f,1.256074458e-01f,-3.964219242e-02f,-5.596081913e-02f,-3.214534000e-02f,-2.005611546e-02f,
-1.139520034e-01f,-1.782572456e-02f,-1.007969398e-02f,-2.257820815e-01f,-4.909364879e-02f,-5.391927436e-02f,1.176396236e-01f,-4.621644318e-02f,-1.047868747e-02f,6.922788173e-02f,-8.173183352e-02f,-1.050854102e-01f,8.305854350e-02f,-1.324180979e-02f,-1.722041331e-02f,-6.054976210e-02f,
-8.179564029e-02f,-3.748903051e-02f,-3.803163022e-02f,1.821108395e-03f,-5.819987133e-02f,3.739053756e-02f,2.214003913e-02f,-6.117244065e-02f,7.675138861e-02f,-1.616206318e-01f,8.228816092e-02f,-2.065099776e-02f,1.462414563e-01f,-1.696116328e-01f,-1.093564555e-01f,9.823305905e-02f,
-1.406274587e-01f,-3.482121974e-02f,2.256484777e-01f,-1.185097694e-01f,-1.418913305e-01f,1.422961056e-01f,-6.335946172e-02f,2.171101235e-02f,1.363604665e-01f,-3.463031724e-02f,1.231769398e-01f,1.168375239e-01f,-1.142332628e-01f,1.092587933e-01f,8.346370608e-02f,-4.378843904e-01f,
2.511676028e-02f,-1.888609491e-02f,1.272081882e-01f,-1.012930721e-01f,-1.674920171e-01f,-9.115651250e-02f,-1.997417063e-01f,-5.109541118e-02f,1.966730505e-01f,-6.276198477e-02f,2.561232746e-01f,2.372260541e-01f,1.100356691e-02f,4.683576152e-02f,1.254435480e-01f,4.022543505e-02f,
6.188919395e-02f,1.348043382e-01f,7.835923135e-02f,2.560840361e-02f,1.430241205e-02f,-1.849104464e-01f,-2.777810395e-01f,1.694478840e-01f,-4.188538790e-01f,-9.553198516e-02f,-4.318470955e-01f,4.548585415e-01f,-6.236870289e-01f,7.995787263e-02f,1.263722330e-01f,-8.556216024e-03f,
5.080595240e-02f,-4.053760786e-03f,4.316561520e-01f,-1.428066343e-01f,-4.654957950e-01f,-2.145585865e-01f,-4.041941762e-01f,-4.058154300e-02f,-2.838529050e-01f,-7.103717420e-03f,-7.671437413e-02f,-1.194480509e-01f,3.402750492e-01f,-9.973702580e-02f,-5.013559759e-02f,-1.570125967e-01f,
-1.174043342e-01f,2.262823880e-01f,-1.332353503e-01f,-4.931183904e-02f,-5.764496326e-02f,-2.810619771e-01f,3.217822313e-01f,5.443818867e-02f,1.013862938e-01f,-1.636286378e-01f,-1.091272831e-01f,5.739580467e-02f,-1.115227770e-02f,2.607773244e-02f,-3.534969687e-02f,5.110988766e-02f,
4.184409603e-02f,7.346645929e-03f,-6.825958937e-02f,9.677832574e-02f,2.887425423e-01f,1.511531770e-01f,-4.824092612e-02f,2.288105339e-01f,5.936292559e-02f,2.191787362e-01f,4.008586407e-01f,3.983713686e-02f,8.301925659e-02f,1.830492467e-01f,8.393995464e-02f,-1.617136151e-01f,
1.598685533e-01f,-2.811420858e-01f,2.006734759e-01f,9.354169667e-02f,1.268309504e-01f,-1.164454371e-01f,7.793115079e-02f,2.508277893e-01f,-4.323113859e-01f,-4.875744134e-02f,5.585751683e-02f,-4.755185451e-03f,-7.622768730e-02f,-1.113452017e-02f,4.790890217e-02f,5.526260287e-02f,
2.683088183e-01f,3.268575072e-01f,-7.946742773e-01f,2.258909345e-01f,3.691729009e-01f,4.335890710e-02f,1.741988361e-01f,8.010207675e-03f,-1.731145941e-02f,-7.327913493e-02f,-6.981804967e-02f,-9.188924916e-03f,-7.386735827e-02f,1.486438662e-01f,-1.340436786e-01f,6.731072813e-02f,
-1.624466032e-01f,-1.207370758e-01f,1.116343066e-01f,-5.919883028e-02f,4.501536489e-03f,-1.558836699e-01f,-1.868706942e-01f,-1.825510152e-02f,1.333767027e-01f,-7.849749923e-02f,7.029032707e-02f,7.779171690e-03f,-9.287551790e-02f,-4.016771168e-02f,-8.033000678e-02f,1.014045179e-01f,
-5.050669424e-04f,-4.407677427e-02f,-2.220856585e-02f,-2.440221794e-02f,-7.017581910e-02f,-9.380870499e-03f,8.486582339e-02f,9.399268776e-03f,3.764542565e-02f,3.282183036e-02f,-9.451159835e-02f,-5.049796030e-02f,4.082826525e-02f,-9.981105477e-02f,2.027445287e-02f,-6.042315811e-02f,
2.369584441e-01f,3.084670305e-01f,6.865999103e-02f,1.539284289e-01f,1.545933932e-01f,-2.459944636e-01f,7.917804271e-02f,-1.231895238e-01f,3.546791077e-01f,-1.487659365e-01f,-1.352931093e-02f,2.230555378e-02f,3.220941722e-01f,-7.428603619e-02f,1.514862105e-02f,3.902922571e-01f,
1.551192999e-01f,-2.861733176e-02f,-3.079852164e-01f,-1.826981753e-01f,9.048245847e-03f,-2.139079571e-01f,-5.296009593e-03f,-5.650068820e-02f,-1.036614273e-02f,1.854025424e-01f,1.893864721e-01f,1.309588850e-01f,6.327528507e-02f,-9.202771634e-02f,-3.284238577e-01f,-8.749196678e-02f,
-9.388941526e-02f,1.986191571e-01f,-1.832869500e-01f,-2.282191217e-01f,1.943478286e-01f,4.125410616e-01f,-1.101462618e-01f,9.520927817e-02f,3.111137748e-01f,1.256186068e-01f,4.162252247e-01f,-3.019697368e-01f,1.790911555e-01f,-8.216363192e-02f,4.158343077e-01f,-2.228316367e-01f,
2.305086255e-01f,1.531201135e-02f,6.300025582e-01f,9.343875200e-02f,-3.041925430e-01f,-4.217700660e-02f,-2.261170745e-01f,-2.240032852e-01f,2.947959006e-01f,1.194681600e-01f,1.991091520e-01f,1.187937707e-01f,-1.156686768e-01f,-7.347350717e-01f,1.178590022e-02f,3.407291770e-01f,
1.995013505e-01f,3.044703417e-02f,5.343060195e-02f,-5.642815679e-02f,-1.447016001e-01f,-2.580568194e-01f,1.035925671e-01f,9.587550908e-02f,-9.210651368e-02f,-1.194869205e-01f,-1.728527695e-01f,4.834232107e-02f,7.625031471e-02f,-5.528746918e-02f,-7.107619941e-02f,5.396452174e-02f,
3.953579441e-02f,-5.543360859e-02f,1.266815662e-01f,-7.027707994e-03f,-1.461907327e-01f,-1.085344404e-01f,5.089430138e-02f,-5.592652783e-02f,-3.274396360e-01f,1.058346331e-01f,-1.275342703e-01f,-1.402373761e-01f,1.610309184e-01f,-5.887512583e-03f,-3.025982082e-01f,-1.177706867e-01f,
-7.375358790e-02f,-1.664729267e-01f,3.035681546e-01f,-2.191912085e-01f,1.897042803e-02f,8.817613870e-02f,3.673091531e-01f,5.668065548e-01f,-1.732714325e-01f,5.223774165e-02f,-4.375972599e-02f,1.994278282e-03f,-7.445721328e-02f,-4.645294324e-02f,6.922457367e-02f,-3.416860104e-02f,
7.491234690e-02f,3.373053074e-01f,-5.347959995e-01f,2.821260393e-01f,-2.377670705e-01f,8.979444206e-02f,1.176391393e-01f,8.962680586e-04f,1.023419425e-01f,-2.145737261e-01f,9.537550062e-02f,5.021166056e-02f,-2.263936400e-01f,2.271836996e-01f,-1.210699677e-01f,7.122688740e-02f,
-6.979830563e-02f,-4.982181266e-02f,6.775181741e-03f,5.547111109e-02f,-4.953368008e-02f,-1.029985547e-01f,5.737231299e-02f,-2.297059260e-02f,1.435590684e-01f,-3.300332278e-02f,-4.641696066e-02f,5.022580922e-02f,-6.338725239e-02f,-5.414051190e-02f,1.989289932e-02f,-7.722357661e-02f,
-1.196362078e-02f,-4.101661593e-02f,-8.289560676e-02f,-6.512922049e-02f,-7.714380044e-04f,4.512081295e-02f,-8.145263046e-02f,9.564683586e-02f,-4.496817291e-02f,-7.282544672e-02f,2.662765980e-02f,-1.261693686e-01f,5.944021419e-02f,-6.716299802e-02f,-1.100526154e-01f,4.987386242e-02f,
3.048606403e-02f,1.316939443e-01f,2.295646071e-01f,-6.896437705e-02f,2.116895318e-01f,-4.498949647e-02f,1.589574069e-01f,5.781093240e-02f,4.768171906e-02f,-3.036152422e-01f,9.349504113e-02f,-1.299498677e-01f,-2.400669269e-02f,-4.039093554e-01f,-8.948793262e-02f,-5.872196704e-02f,
2.138855010e-01f,-7.641743124e-02f,-1.170359403e-01f,-2.210179567e-01f,1.709445268e-01f,-1.928444505e-01f,-5.472330004e-02f,-9.105620533e-02f,-9.861970320e-03f,-2.827913873e-02f,-1.539292186e-01f,2.971242964e-01f,-4.498749971e-02f,1.380742192e-01f,4.348573461e-02f,-2.519373409e-02f,
-6.392738968e-02f,-1.068371385e-01f,-1.746384203e-01f,-1.300389320e-01f,1.492833346e-01f,8.029429615e-02f,-1.633449458e-02f,-1.162691116e-01f,1.267396938e-02f,3.926509246e-02f,-8.556559682e-01f,6.132421494e-01f,1.546285897e-01f,-1.055938840e+00f,-2.973775193e-02f,-1.633341312e-01f,
1.702211797e-01f,1.486599743e-01f,3.454459310e-01f,-7.715241909e-01f,-2.432613969e-01f,-7.660599053e-02f,-1.896205544e-01f,-3.672490120e-01f,2.039987445e-01f,4.514621198e-02f,5.269489065e-02f,5.899209902e-02f,1.394875497e-01f,-8.682474494e-02f,1.295315772e-01f,1.149324328e-01f,
-1.415293515e-01f,-3.856117651e-02f,-1.730425283e-02f,-6.602026056e-03f,-3.802968189e-03f,-3.142785430e-01f,2.343535274e-01f,-9.830102324e-02f,-1.682224683e-02f,4.532938600e-01f,-4.027221724e-02f,3.940465674e-02f,-1.260074228e-01f,-6.604848057e-02f,-3.666093200e-02f,-1.415034682e-01f,
5.349637941e-02f,-1.144449040e-01f,-2.999045420e-03f,3.236005604e-01f,-2.232787013e-01f,-1.987915635e-01f,-1.510252953e-01f,1.543959975e-02f,-2.027153820e-01f,-2.237965912e-01f,-5.401909724e-02f,-2.189011127e-01f,2.022680342e-01f,5.287431479e-01f,-2.036875039e-01f,-1.452842951e-01f,
9.667652100e-02f,2.166013233e-02f,1.047950238e-01f,-7.133279741e-02f,1.593070328e-01f,1.131392196e-01f,7.040617615e-02f,2.558602393e-01f,5.309106782e-02f,-3.643786535e-02f,7.511403412e-02f,3.086132184e-02f,5.931899324e-02f,1.232107729e-02f,-3.466584533e-02f,7.566349953e-02f,
-9.049721062e-02f,4.736026376e-02f,1.512092445e-02f,-3.061106801e-01f,1.403274983e-01f,-7.041481137e-02f,1.640169024e-01f,-2.705470622e-01f,-5.376170203e-02f,-1.659682393e-02f,-1.602256149e-01f,1.782009155e-01f,-7.305268943e-02f,1.779577346e-03f,-1.598087139e-02f,-2.295545340e-01f,
-6.418256462e-02f,-1.478932053e-01f,-2.386199832e-01f,-1.896236907e-03f,7.466403395e-02f,9.255023301e-02f,-7.522653043e-02f,1.009745002e-01f,-2.075218968e-02f,7.990795374e-02f,4.210616648e-02f,-3.820097446e-02f,3.424666077e-02f,-2.277223021e-02f,-2.888885140e-02f,-6.345060468e-02f,
9.873207659e-03f,-4.122664034e-02f,-3.647610173e-02f,-2.540735342e-02f,1.878095977e-02f,5.529058352e-02f,3.692530841e-02f,-1.009144187e-01f,-3.954393044e-02f,-1.166690141e-01f,-3.805479035e-02f,8.759327233e-02f,-5.181179568e-02f,-8.432617784e-02f,4.558546096e-02f,-1.356896907e-01f,
1.874784976e-01f,-1.503630430e-01f,5.607989803e-02f,2.165416628e-01f,-1.786508858e-01f,-2.296995185e-02f,9.413772076e-02f,2.285843156e-02f,-1.416612566e-01f,2.218344510e-01f,1.462396383e-01f,8.724533021e-02f,1.429383904e-01f,-5.028565526e-01f,-1.598788798e-01f,6.947720647e-01f,
-5.118426085e-01f,-1.855117502e-03f,3.356782794e-01f,-7.366103679e-02f,-1.036066655e-02f,2.225110233e-01f,-2.029210329e-02f,-8.480505645e-02f,-1.249494255e-01f,2.627539933e-01f,-4.464250058e-03f,1.330507081e-02f,3.239259124e-01f,1.772635430e-01f,5.871977285e-02f,-6.483715773e-02f,
-1.105580665e-02f,1.829808652e-01f,4.204020798e-01f,-1.056229621e-01f,-1.982176304e-01f,9.381754696e-02f,-4.070247412e-01f,-3.174456358e-01f,7.380421758e-01f,-1.340802014e-01f,-5.382463336e-02f,-2.697521150e-01f,-4.301856458e-01f,2.587922215e-01f,-4.032117501e-02f,1.192063913e-01f,
1.372490525e-01f,9.151631594e-02f,2.108156830e-01f,-2.552413344e-01f,-4.716430902e-01f,-2.433022112e-01f,-2.050875425e-01f,-3.428861201e-01f,7.824143022e-02f,4.120242596e-02f,2.981229722e-01f,-6.831178069e-02f,2.265172601e-01f,8.020091802e-02f,1.097369567e-01f,2.298335880e-01f,
4.741040617e-02f,-3.106955066e-02f,-3.476136178e-02f,1.223242208e-01f,-2.252575159e-01f,-4.261747897e-01f,4.376727343e-01f,-3.380700648e-01f,1.516719162e-02f,-1.724554747e-01f,-5.334213376e-01f,1.185649261e-01f,6.615111232e-02f,1.953266561e-02f,-1.577859819e-01f,-3.562479913e-01f,
1.336074620e-01f,9.086331120e-04f,-4.106637836e-02f,3.277362585e-01f,2.047695369e-01f,2.079702318e-01f,6.289639324e-02f,3.861225843e-01f,-1.838715523e-01f,-9.120549262e-02f,2.437795401e-01f,-1.835935414e-01f,-1.565165371e-01f,8.950789273e-02f,-1.601141691e-02f,-2.059645653e-01f,
-1.085311268e-02f,2.645837069e-01f,1.488475800e-01f,1.966469735e-02f,4.055417329e-02f,-1.378252655e-01f,2.198263705e-01f,2.188540250e-01f,9.043765813e-02f,1.702930033e-02f,-6.641507894e-02f,-7.320847362e-02f,-6.205076724e-02f,-3.736658022e-02f,2.070719749e-02f,2.151636034e-02f,
1.760855466e-01f,3.836581111e-01f,-7.497359514e-01f,5.638483539e-02f,-2.556160092e-03f,1.427671686e-02f,2.636346817e-01f,-1.110045016e-01f,7.473424822e-02f,-5.957863759e-04f,1.921066828e-02f,4.483206198e-02f,-2.055280358e-01f,-2.530580163e-01f,2.288625203e-02f,-6.393440813e-02f,
-2.320385128e-01f,4.765922576e-02f,-1.157669574e-01f,-3.522210941e-02f,-3.916415572e-02f,6.664399058e-02f,-6.452575326e-02f,1.276521478e-02f,2.644921094e-02f,1.291966345e-02f,-2.531481348e-02f,-5.867939442e-02f,1.123215165e-02f,1.266684085e-01f,-3.822520375e-02f,-6.756895781e-02f,
6.199963018e-02f,3.159422777e-04f,5.074373633e-02f,4.414917901e-02f,-5.373270065e-02f,-5.363246799e-02f,-2.309673093e-02f,1.910804026e-02f,5.819409620e-03f,-3.842265159e-02f,-2.177633531e-02f,-1.965142637e-01f,-1.078052297e-01f,-6.170886382e-02f,-3.574505448e-02f,2.271806262e-02f,
5.895812437e-02f,-1.610146761e-01f,-2.126936466e-01f,-2.879636176e-02f,-1.079202220e-01f,-2.786167264e-01f,1.475580931e-01f,-3.570857644e-02f,-1.400506347e-01f,-1.505678892e-01f,-4.349193871e-01f,-2.236698382e-02f,1.017682850e-01f,-3.356905878e-01f,-2.913553715e-01f,2.798822820e-01f,
-1.780240536e-01f,-5.794921890e-02f,-4.918248653e-01f,6.224923581e-02f,2.976538241e-02f,1.010041237e-01f,-5.450814217e-02f,-1.823006384e-02f,-4.239423200e-02f,2.564507537e-02f,-6.616149843e-02f,-2.498572469e-01f,-2.831227519e-02f,4.267290607e-02f,-3.257741332e-01f,-4.076571390e-02f,
-2.211220860e-01f,1.770717464e-02f,-1.227071434e-01f,-3.652509749e-01f,-1.008152217e-01f,2.468938679e-01f,-4.965681136e-01f,-1.716418415e-01f,2.140741646e-01f,-2.431093901e-02f,5.796378851e-01f,-4.299975634e-01f,-5.626139045e-01f,-7.420457155e-02f,-9.336265177e-02f,5.025327578e-02f,
1.727932990e-01f,4.225365072e-02f,3.411952555e-01f,-3.370223343e-01f,-1.172676608e-01f,1.953554153e-01f,-1.482056081e-01f,-2.119351923e-01f,2.380138785e-01f,-5.745441467e-02f,1.819452494e-01f,2.142568529e-01f,-1.429750621e-01f,6.852941215e-02f,1.242989115e-02f,-8.768269420e-02f,
-1.228447855e-01f,-5.934904143e-02f,1.292206794e-01f,-7.624003291e-02f,2.476742417e-01f,4.692307413e-01f,-2.214135081e-01f,-5.070693046e-02f,1.613900661e-01f,1.163915843e-01f,3.606094122e-01f,-2.478791177e-01f,7.996008545e-02f,-9.380871803e-02f,-5.306431651e-02f,-1.012605131e-01f,
1.494961977e-01f,-1.256876718e-02f,-1.402828260e-03f,-3.647920862e-02f,1.764158346e-02f,2.246284485e-01f,1.736202538e-01f,5.435307324e-02f,-5.304230377e-02f,4.545993730e-02f,2.253645472e-02f,-3.190573752e-01f,-2.175850272e-01f,-1.224550605e-01f,8.546975255e-02f,-1.885064095e-01f,
-1.051313877e-01f,1.948712021e-01f,2.575916424e-02f,1.748956889e-01f,-1.220748276e-01f,-1.409634203e-01f,6.689139642e-03f,1.352673322e-01f,2.195440233e-01f,9.014554322e-03f,-3.670918941e-02f,4.305019230e-02f,1.796560735e-02f,-5.048812926e-02f,2.019351721e-02f,2.348141372e-02f,
-1.707118703e-03f,6.176410317e-01f,-9.985053539e-01f,1.168494672e-01f,6.755463779e-02f,1.542867422e-01f,-2.964798808e-01f,1.376263425e-02f,1.454094201e-01f,1.465182006e-02f,-6.216498464e-02f,5.356276780e-02f,-1.776198447e-01f,9.567534924e-02f,6.275376678e-02f,-2.772574425e-01f,
9.492568672e-02f,-8.046432585e-02f,-4.534479231e-03f,-1.518342346e-01f,-1.023152471e-01f,6.384459883e-02f,4.235949740e-02f,6.728688627e-02f,9.207033552e-03f,5.407867488e-03f,-1.076946110e-01f,-6.008074433e-02f,4.885646701e-02f,8.218056522e-03f,3.365159035e-02f,1.232458279e-01f,
1.097429544e-01f,-8.782770485e-03f,1.538762599e-01f,-3.165319189e-02f,-2.500593662e-02f,6.847348064e-02f,-6.828636676e-02f,1.101589724e-01f,-2.320683748e-02f,-5.874910578e-02f,-3.523673117e-02f,-1.713256352e-02f,4.133162275e-02f,-1.449625641e-01f,-8.524770848e-03f,7.181551307e-02f,
1.230061054e-01f,-1.183793694e-01f,-2.875140011e-01f,-3.596277237e-01f,-4.466897249e-01f,2.641617358e-01f,1.083232015e-01f,-1.435318589e-02f,2.260876298e-01f,2.805469930e-01f,-2.828974426e-01f,2.273882478e-01f,2.583557963e-01f,-1.631059796e-01f,-2.446359098e-01f,-1.242983937e-01f,
2.165986449e-01f,2.761010639e-02f,3.637143970e-01f,-5.880698562e-02f,-1.394507140e-01f,-9.320454299e-02f,-7.031048089e-02f,7.950075902e-03f,1.897920296e-02f,1.670265347e-01f,7.329478860e-02f,-2.041870356e-01f,-1.459984183e-01f,-2.153048525e-03f,1.034855843e-01f,-9.853362292e-02f,
7.053966820e-02f,2.495951355e-01f,-1.035289466e-01f,-1.920490265e-01f,5.116372555e-02f,3.323348761e-01f,-1.254053861e-01f,-1.769517511e-01f,-1.239581034e-01f,1.480181813e-01f,-4.086049795e-01f,-5.555393100e-01f,5.133606959e-03f,-6.764323451e-03f,-7.753115892e-02f,-4.056901634e-01f,
1.318370700e-01f,9.808566421e-02f,2.662620544e-01f,-6.886916161e-01f,-4.883643389e-01f,-3.252004385e-01f,-1.040528063e-02f,-3.081377745e-01f,2.134122699e-01f,1.657453179e-02f,-2.566316724e-01f,-1.608383209e-01f,1.623449326e-01f,-5.705031157e-01f,-1.522132158e-01f,1.683371365e-01f,
-1.402791739e-01f,2.812158689e-02f,-4.237810895e-02f,-1.075243726e-01f,-1.714164168e-01f,-2.237748355e-01f,2.773133516e-01f,-3.071231022e-02f,1.046610903e-02f,3.711407483e-01f,-3.137318194e-01f,1.636358202e-01f,2.251267899e-03f,-2.179901861e-02f,-5.938509852e-02f,-1.204897687e-01f,
1.878456622e-01f,7.732157409e-02f,7.106172293e-02f,3.545422256e-01f,-1.278834790e-01f,-6.296017766e-02f,-7.910641283e-02f,2.009494603e-01f,2.924130671e-02f,-1.469661668e-02f,1.245185360e-01f,2.350431494e-02f,-1.365439147e-01f,1.900200099e-01f,-1.011931300e-01f,-2.048999667e-01f,
8.488947898e-02f,-6.951159984e-02f,6.222934276e-02f,-7.361225784e-02f,2.466907799e-01f,1.337499321e-01f,1.629903764e-01f,2.191116214e-01f,-3.860406876e-01f,4.544076696e-02f,-9.106896818e-03f,5.583410710e-02f,3.689544648e-02f,-1.120216399e-02f,-3.323515505e-02f,5.959296972e-02f,
-6.503057480e-02f,1.463279128e-01f,-4.103438929e-02f,-1.467687637e-01f,1.975963563e-01f,5.866603181e-02f,1.969454885e-01f,-1.113129854e-01f,1.105307788e-01f,6.484455615e-02f,-1.901660711e-01f,-1.432605237e-01f,-4.906279594e-02f,3.443863988e-02f,-3.260031044e-01f,-2.597466707e-01f,
-4.100671709e-01f,5.932110921e-02f,-2.938514017e-02f,-1.339856535e-01f,1.243881956e-01f,5.589724705e-02f,1.562290546e-02f,-5.341307446e-02f,-7.723931968e-02f,-1.477779541e-02f,-9.012071043e-02f,-8.626215160e-02f,-2.815779671e-02f,1.645130366e-01f,9.693469852e-02f,-6.783346087e-02f,
-7.894830406e-02f,-3.789047943e-03f,-4.542247951e-02f,9.346430749e-02f,-9.002675302e-03f,6.501810998e-02f,9.382987767e-02f,2.832227014e-02f,1.991352253e-02f,-1.352966875e-01f,1.606333852e-01f,3.334843367e-02f,-8.449858427e-02f,-2.885032073e-02f,3.864384815e-02f,1.485098302e-01f,
-2.988532186e-02f,-8.123856969e-03f,2.078633197e-02f,-4.451630637e-02f,-1.460649967e-01f,-4.750930965e-01f,3.040679097e-01f,1.352504492e-01f,-2.645939030e-02f,-3.497044146e-01f,-3.473174199e-02f,-1.028127149e-01f,2.361933887e-01f,-1.857919693e-01f,-2.453687936e-01f,3.250674307e-01f,
-4.210917279e-02f,1.702623442e-02f,-1.418485790e-01f,-3.534913063e-02f,-6.933137029e-02f,5.120036751e-02f,1.755374367e-03f,5.884731933e-02f,5.977508053e-02f,-3.517788649e-02f,-1.718241721e-01f,-5.687939748e-02f,-1.501042843e-01f,-3.666830957e-01f,-5.193281770e-01f,-9.423612058e-02f,
-8.973704278e-02f,-2.133329809e-01f,-1.143251136e-01f,-2.755150497e-01f,-5.742510408e-02f,2.752667665e-01f,-4.346235096e-02f,-1.040343568e-01f,2.976326942e-01f,1.419577897e-01f,4.965308309e-01f,5.498942733e-01f,-1.692167819e-01f,-7.189998031e-01f,-5.046288371e-01f,1.085597724e-01f,
9.359467030e-02f,3.455277905e-02f,-3.572748974e-02f,2.418591082e-02f,-1.697860360e-01f,5.647325516e-02f,2.108366787e-01f,-1.384877134e-02f,1.478905827e-01f,5.317258462e-02f,-1.738711596e-01f,-1.843298599e-02f,1.266281307e-01f,-3.596922755e-02f,1.371522844e-01f,4.092159569e-01f,
9.112956934e-03f,2.614178509e-02f,1.538091600e-01f,8.394062519e-02f,1.027771980e-01f,1.436794102e-01f,-2.896912023e-02f,-3.243341744e-01f,1.828785986e-02f,1.796745062e-01f,-2.254514843e-01f,2.646599151e-02f,-7.888306677e-02f,5.656113476e-02f,-2.532005124e-02f,-1.621466428e-01f,
3.348558769e-02f,6.173714250e-02f,1.089199185e-01f,3.404721320e-01f,-2.445484549e-01f,-4.427582398e-02f,-3.819581121e-02f,9.602787322e-04f,-2.013766021e-01f,-1.399581134e-01f,-8.837893605e-02f,-5.627795681e-02f,-3.619033694e-01f,3.894245923e-01f,1.860699207e-01f,1.179424226e-01f,
8.530509472e-02f,-3.590522334e-02f,1.644853652e-01f,-3.399118781e-02f,1.176757291e-01f,-5.131908134e-02f,1.398003250e-01f,-3.842965141e-02f,1.534976959e-01f,-1.263664477e-02f,-4.145177826e-02f,-6.686924398e-02f,-1.524758339e-02f,-1.373953372e-02f,-7.133751363e-02f,-6.711804122e-02f,
-1.422337536e-02f,1.982941628e-01f,-2.826924324e-01f,-4.353488237e-02f,2.300938591e-03f,7.584228367e-02f,7.472861558e-02f,-9.885490872e-03f,7.284492254e-02f,2.623908408e-02f,6.848517805e-02f,1.018478908e-02f,-8.427028358e-02f,-1.705979705e-01f,-3.019186854e-01f,-1.683267951e-01f,
5.147860572e-02f,-1.227076426e-01f,-7.970168442e-02f,-5.077122152e-02f,5.803982541e-02f,-5.189260468e-02f,-7.808829192e-03f,-4.643726628e-03f,-8.475784212e-02f,8.180609345e-02f,4.714619741e-02f,-7.254925370e-02f,8.696134388e-02f,4.052617401e-02f,1.176973209e-01f,-6.594017148e-02f,
-3.037705831e-02f,-4.698697478e-02f,2.589151450e-02f,-3.426340222e-02f,2.048542909e-02f,-7.320881635e-02f,3.031003848e-02f,1.648738831e-01f,-8.818862960e-03f,-8.466772735e-02f,4.548912868e-02f,-6.309740245e-02f,7.726885378e-02f,4.401552305e-02f,-3.263044059e-01f,1.047190130e-01f,
-5.231826007e-02f,-2.180619240e-01f,1.656254381e-01f,-4.713388160e-02f,-1.987675279e-01f,2.964692749e-02f,1.720639467e-01f,5.186678842e-02f,-1.652087457e-02f,-7.928520441e-02f,-3.164985776e-01f,-4.428114276e-03f,1.090029851e-01f,-1.509347558e-01f,-3.066274226e-01f,2.462440915e-02f,
1.386737823e-01f,-5.580008402e-02f,2.732209563e-01f,-4.534539580e-02f,9.415903687e-02f,3.016302362e-02f,-1.804677956e-02f,-2.030648440e-01f,1.328137517e-01f,4.491863307e-03f,-1.620939076e-01f,2.266959548e-01f,7.154294848e-02f,9.290236980e-02f,2.563738823e-02f,1.074793469e-02f,
8.295606822e-02f,-1.180875897e-01f,-2.035128623e-01f,-2.966223061e-01f,6.111607887e-03f,1.154529601e-01f,2.906771898e-01f,-5.531078577e-01f,-5.536698550e-02f,4.136975855e-02f,-7.363646030e-01f,-9.498279542e-02f,-2.609596550e-01f,-6.042129397e-01f,8.110138029e-02f,5.181824043e-02f,
7.813218981e-02f,1.734912544e-01f,-3.912455589e-02f,-3.115504086e-01f,6.728064269e-02f,-5.981189013e-02f,4.305601493e-02f,-1.973296404e-01f,2.464784235e-01f,-1.450395677e-02f,-8.861025423e-02f,1.522493958e-01f,3.336422145e-02f,5.639489740e-02f,4.096570238e-02f,2.094243020e-01f,
5.811137706e-02f,2.686167881e-02f,1.613085568e-01f,-8.999453858e-03f,9.394050390e-02f,-2.543007731e-01f,-6.580788642e-02f,-2.864498496e-01f,2.464401871e-01f,2.586706877e-01f,3.345711390e-03f,5.876619741e-02f,1.418336332e-01f,3.512181342e-02f,-1.175385788e-01f,9.258682840e-03f,
-9.577238560e-02f,-3.877323121e-02f,-1.081677303e-01f,2.740842104e-01f,-4.030261338e-01f,-2.066286653e-01f,1.088299751e-01f,4.331952706e-02f,-2.915314436e-01f,-1.446666718e-01f,1.150681823e-01f,-1.127424184e-03f,-3.135888577e-01f,4.710032642e-01f,1.955970190e-02f,-2.020895034e-01f,
1.270603389e-02f,-1.467556041e-02f,-1.392221898e-01f,-2.932602167e-02f,5.033944175e-02f,1.182828397e-01f,2.531972826e-01f,1.792585999e-01f,-5.202605575e-02f,3.332031891e-03f,-4.658900201e-03f,-8.436426520e-03f,4.467541724e-02f,1.776240766e-02f,3.138387948e-02f,1.055033505e-02f,
3.899408132e-02f,3.532374799e-01f,-2.769850194e-01f,-1.410376728e-01f,-1.890265010e-02f,1.551087759e-02f,-2.993096225e-02f,-7.056611776e-02f,2.875780128e-02f,6.963037420e-03f,-3.242497891e-02f,1.095676571e-01f,-2.052839473e-02f,-8.730436116e-02f,-3.282741606e-01f,9.772792459e-03f,
-1.142859273e-02f,3.449666500e-01f,-1.133905277e-01f,1.373958495e-02f,-9.619095922e-02f,4.817615077e-02f,-1.047946978e-02f,3.298032284e-02f,-8.101338893e-02f,1.102867872e-01f,-1.153098568e-01f,-1.440040916e-01f,2.527887560e-02f,-2.410487086e-02f,7.143849880e-02f,8.481770009e-02f,
-3.956688195e-02f,7.559679449e-02f,-4.031981528e-02f,-3.328141570e-02f,-6.217222288e-02f,-7.587195374e-03f,-1.016299147e-02f,4.362378269e-02f,-9.278016537e-02f,-1.059055999e-01f,6.763525307e-02f,1.095872447e-01f,-5.275735259e-02f,-2.273380160e-01f,-1.247951612e-01f,-7.979233563e-02f,
5.680494756e-02f,-1.219254807e-01f,3.085558712e-01f,-1.284759790e-01f,-3.134139478e-01f,-5.340888724e-02f,1.198789924e-01f,1.573034078e-01f,-4.880496114e-02f,-2.947764397e-01f,8.835628629e-02f,7.148252428e-02f,-3.952999040e-02f,-1.113344654e-01f,-2.733680606e-01f,-1.720276289e-02f,
-2.337244004e-01f,5.014699697e-02f,2.791360021e-01f,7.203424722e-02f,-1.504161358e-01f,1.099951789e-01f,-1.706971377e-01f,-1.297914535e-01f,-5.053659901e-02f,9.301320463e-02f,8.265578188e-03f,2.645311356e-01f,7.292432711e-03f,-9.913642704e-02f,-6.547556072e-02f,-1.753098220e-01f,
7.489150763e-02f,9.803219885e-02f,7.548371702e-02f,5.012676865e-02f,-6.745304167e-02f,3.480831860e-03f,4.705537483e-02f,-1.733270437e-01f,2.896476910e-02f,-1.221979707e-01f,-2.817784846e-01f,-6.115258858e-02f,-9.109225869e-02f,9.863384999e-03f,3.894145191e-01f,-2.234844267e-01f,
1.954192221e-01f,3.174125776e-02f,3.168955743e-01f,-1.445649564e-01f,-4.764127731e-01f,1.163897812e-01f,4.055262730e-02f,-1.679175347e-01f,2.917531729e-01f,-7.803383470e-02f,-2.189453132e-02f,1.481583118e-01f,1.056204960e-01f,8.683424676e-05f,2.497234382e-02f,4.036506265e-02f,
-8.102647960e-02f,-7.708851993e-02f,1.712851673e-01f,9.179485962e-03f,-9.007610381e-02f,-2.433849275e-01f,-5.235392973e-02f,-3.307383657e-01f,9.970293194e-02f,1.751675904e-01f,-9.129917016e-04f,-2.298500985e-01f,2.263200432e-01f,1.568258554e-02f,1.324009448e-01f,1.227615997e-01f,
3.461703658e-02f,-1.686060727e-01f,1.575137377e-01f,1.328310668e-01f,-3.866966963e-01f,6.877160631e-03f,1.557599902e-01f,-9.945481271e-02f,-1.361394376e-01f,-1.454280466e-01f,-2.055608928e-01f,-3.231622577e-01f,-1.533863395e-01f,5.778469443e-01f,-2.679882050e-01f,2.723504044e-02f,
-6.817068160e-02f,3.153952584e-02f,3.897031024e-02f,-1.100088283e-01f,-1.549541652e-01f,2.138233632e-01f,-1.890112758e-01f,7.155473530e-02f,2.284067571e-01f,6.896983832e-02f,-1.914013550e-02f,5.092202872e-02f,-3.746907040e-02f,6.174349040e-02f,-6.401059031e-02f,-8.509106934e-03f,
1.164937466e-01f,1.666815877e-01f,-2.941711247e-01f,-3.217802644e-01f,-2.759255655e-02f,1.182573661e-01f,-1.581519395e-01f,-3.896408975e-01f,9.483710676e-02f,1.949901581e-01f,4.312558845e-02f,-8.850479499e-03f,-8.364263177e-02f,3.247373039e-03f,-7.820399478e-03f,-2.403457463e-02f,
-2.823469043e-01f,-1.426050346e-02f,-1.970892847e-01f,2.686911225e-01f,-8.696323261e-03f,6.893669814e-02f,-6.673067063e-02f,1.447774749e-02f,-7.370722294e-02f,-5.862746621e-04f,-7.134772837e-02f,2.768754028e-02f,-5.651749298e-02f,5.745420977e-02f,5.722360685e-02f,6.002025306e-02f,
-4.838659987e-02f,4.457617551e-02f,-4.600247741e-02f,-6.548950821e-02f,-4.723927379e-02f,-3.659944981e-02f,8.983712643e-02f,8.077523857e-02f,-1.994524524e-02f,-1.320517212e-01f,2.031619847e-02f,8.504991420e-03f,-9.207498282e-02f,-1.407792121e-01f,2.112605236e-02f,-1.443985850e-01f,
-1.893516979e-03f,-8.092017472e-02f,4.790267944e-01f,4.294839874e-02f,-2.207589895e-01f,-2.559921145e-01f,1.788940430e-01f,1.674446315e-01f,-5.796084926e-02f,-4.959430099e-01f,2.087350003e-02f,-1.325887740e-01f,-8.310654014e-02f,-3.235369623e-01f,-3.355108202e-02f,3.952599466e-01f,
7.217417657e-02f,3.524073586e-02f,1.545282453e-01f,-5.023862049e-02f,-2.663438395e-02f,1.510255188e-01f,-1.914989650e-01f,-5.226220936e-02f,-9.107366949e-02f,3.785099543e-04f,-1.514379680e-01f,4.656712413e-01f,1.133042797e-01f,-1.049008444e-01f,-3.193194866e-01f,1.746534407e-01f,
7.790520042e-02f,-2.186394297e-02f,-2.231166363e-01f,1.928414963e-02f,-6.618689746e-02f,3.007292282e-03f,2.095762938e-01f,-1.142461747e-01f,3.893634975e-01f,2.809230685e-01f,-2.245986462e-01f,-6.719800830e-01f,5.126785636e-01f,-9.264195561e-01f,4.304843545e-01f,3.694558144e-02f,
1.545046568e-01f,1.766582876e-01f,1.179817766e-01f,-1.180532813e+00f,-2.203696817e-01f,-2.553247213e-01f,-6.693547964e-01f,-7.650451660e-01f,3.100942075e-01f,-4.761136137e-03f,-2.440597303e-02f,-1.335826665e-01f,2.304316610e-01f,-1.671377919e-03f,-7.859352231e-02f,4.094262421e-01f,
-2.507408857e-01f,1.435118616e-01f,3.609843254e-01f,3.263579309e-02f,-7.693799585e-02f,-2.707068920e-01f,1.563431323e-01f,-1.157282963e-01f,5.511851981e-02f,3.170264065e-01f,6.079873443e-02f,1.368339509e-01f,2.811113298e-01f,-9.106428176e-02f,-3.429531353e-03f,-1.572639197e-01f,
2.287593633e-01f,-1.382347941e-01f,-8.500789106e-02f,4.431970716e-01f,-3.166144490e-01f,4.967818037e-03f,9.596945345e-02f,-7.163833082e-02f,-4.419927299e-01f,-4.917407632e-01f,3.197778463e-01f,-1.623069644e-01f,9.934251010e-02f,7.250329852e-02f,-3.435332775e-01f,-4.578893483e-01f,
9.928933531e-02f,-1.771991402e-01f,2.323245853e-01f,-2.819504738e-01f,-1.736592874e-02f,1.166206151e-01f,-2.686424926e-02f,8.114655316e-02f,-3.271919191e-01f,-3.659985214e-02f,-7.140595466e-02f,5.438303202e-02f,2.182643116e-02f,-6.233458221e-02f,6.342538446e-02f,-1.296731550e-02f,
-3.577404916e-01f,1.749456227e-01f,1.221658662e-02f,-6.389580667e-02f,3.226034045e-01f,-2.313192189e-02f,7.356494665e-02f,1.829300309e-03f,2.667928301e-02f,9.486818314e-02f,-2.236023992e-01f,-4.102966934e-02f,-9.590762854e-02f,1.081892848e-01f,1.080390215e-01f,-2.933865488e-01f,
9.145624936e-02f,-8.609257638e-02f,1.094783396e-01f,3.627305478e-02f,5.429320782e-02f,-2.688333951e-02f,5.957174674e-02f,4.065870866e-02f,-8.386231214e-02f,4.899001867e-02f,2.477488294e-02f,3.569955379e-02f,4.476143047e-02f,4.137170687e-02f,-4.751407728e-02f,-7.442384213e-02f,
3.078926541e-02f,9.979845583e-02f,2.043395489e-02f,3.330187127e-02f,-4.055840895e-02f,7.156157494e-02f,7.727718353e-02f,2.624155022e-02f,-2.894110791e-02f,-4.556074739e-02f,1.520501971e-01f,1.129659861e-01f,6.167731807e-02f,3.517867997e-02f,-4.894403741e-02f,2.760280296e-02f,
-1.058910340e-01f,-2.190899849e-01f,-1.203240678e-01f,-1.024131849e-02f,-1.927631795e-01f,-6.368555874e-02f,-1.680002958e-01f,-2.988518476e-01f,-3.094129562e-01f,-5.187567323e-02f,-4.828830808e-02f,-3.191636801e-01f,3.510002196e-01f,-1.135621816e-01f,-2.723422348e-01f,2.824361436e-02f,
-3.192828596e-01f,-4.435474798e-02f,3.419197798e-01f,1.666500270e-01f,1.749599911e-02f,2.796581388e-02f,-2.700984944e-03f,-1.280043870e-01f,5.034858733e-02f,1.627084427e-02f,-3.362425864e-01f,-8.263324201e-02f,1.331109852e-01f,-2.018354088e-01f,-3.764396533e-02f,-3.170370162e-01f,
-2.832145989e-01f,-2.801083326e-01f,4.811628535e-02f,-3.674630076e-02f,-1.806054413e-01f,3.003163040e-01f,-2.101369351e-01f,-1.213900670e-01f,-2.644036338e-02f,-2.804559469e-01f,2.494946271e-01f,-4.560618401e-01f,-1.100627184e-01f,2.194284648e-01f,-4.392557740e-01f,-5.093915015e-02f,
1.980310678e-01f,1.251275539e-01f,2.384319603e-01f,-6.400141716e-01f,1.820729226e-01f,-2.721657753e-01f,-1.375278980e-01f,-2.742421627e-01f,2.350817174e-01f,3.080007806e-02f,-2.095993608e-01f,-5.214402080e-02f,8.414428681e-02f,-2.559627593e-02f,-8.983593434e-02f,4.703008831e-01f,
-7.951321453e-02f,1.825667918e-02f,3.984399140e-02f,-3.084741719e-02f,9.625903331e-03f,-9.906455129e-02f,1.049996689e-01f,9.971426427e-02f,1.542333513e-01f,1.550152153e-01f,2.063808218e-02f,2.018563747e-01f,-1.505618449e-03f,6.474541966e-03f,-4.140304327e-01f,-4.789269865e-01f,
2.185788751e-01f,-2.564778924e-01f,-2.276193537e-02f,2.124144435e-01f,-3.797413111e-01f,1.178142503e-01f,-2.122133225e-02f,-4.024356231e-02f,-3.082561791e-01f,-2.081284523e-01f,2.490717545e-02f,-2.339984924e-01f,-6.871468574e-02f,4.986060411e-02f,-1.055543497e-01f,-7.915121317e-02f,
7.923266292e-02f,9.774243087e-02f,1.576967090e-01f,1.236616746e-01f,-8.110346645e-02f,1.484643668e-01f,-1.525893509e-01f,1.164449900e-01f,-1.316344086e-02f,-4.224131629e-02f,4.599899799e-02f,4.946142435e-03f,-7.469646633e-03f,7.054588944e-02f,5.672111735e-02f,-6.714398414e-02f,
9.319537878e-02f,2.983819544e-01f,-3.969663978e-01f,-1.696952432e-01f,9.842102975e-02f,4.695813358e-02f,7.125758380e-02f,-1.567608677e-02f,1.916402131e-01f,1.590508521e-01f,-1.288546473e-01f,-9.383075684e-02f,-2.445614785e-01f,4.740098864e-02f,1.220481377e-02f,-1.386564821e-01f,
5.163361430e-01f,-1.406517327e-01f,-1.437723488e-01f,5.620731413e-02f,5.880213901e-02f,6.708511710e-02f,-8.248192258e-03f,4.715857282e-02f,8.528202772e-02f,-3.794263583e-03f,-2.612329088e-02f,3.890058398e-02f,-4.659276083e-02f,-7.926897705e-02f,-1.343200803e-01f,4.520767182e-02f,
8.567869663e-03f,6.653319299e-02f,1.182937901e-02f,-7.667201012e-02f,4.212654755e-02f,5.817634612e-02f,6.213543937e-02f,-7.277275622e-02f,4.419693723e-02f,3.770470619e-02f,8.704660088e-02f,5.231448263e-02f,8.020660281e-02f,-8.069135994e-02f,6.528246403e-02f,-2.307357788e-01f,
-4.087034240e-02f,2.145991623e-01f,-8.685516566e-02f,2.436383627e-02f,-1.670101434e-01f,6.229897216e-02f,-1.874865592e-02f,-1.011704206e-01f,1.980069429e-01f,-1.044708788e-01f,9.914386272e-02f,3.899780661e-02f,-2.580077648e-01f,-2.782222927e-01f,-8.445747197e-02f,-7.280392200e-02f,
-8.586912602e-02f,-7.385110855e-02f,3.760392368e-01f,-9.403109550e-02f,-1.117439941e-01f,-8.260970935e-03f,-1.011810899e-01f,-9.339553071e-04f,-1.696607172e-01f,-7.674773782e-02f,1.692382395e-01f,3.353489190e-02f,2.423049659e-01f,-1.257019117e-02f,1.372660697e-01f,-8.439803123e-02f,
-2.675007880e-01f,9.441138804e-02f,-9.849952161e-02f,1.655895561e-01f,-1.784783155e-01f,-1.319919974e-01f,-2.699742019e-01f,-3.955151886e-02f,2.470571734e-02f,-1.132728308e-01f,-7.776196599e-01f,3.207226396e-01f,-4.170694053e-01f,-8.953249454e-02f,-1.428929269e-01f,8.844940364e-02f,
1.579064578e-01f,1.034322754e-01f,6.822274625e-02f,-3.973105550e-01f,-3.743545115e-01f,-1.914909333e-01f,-2.236176133e-01f,-2.302174270e-01f,1.199843138e-01f,1.413033064e-02f,-2.054469734e-01f,-1.335858107e-01f,8.506520838e-02f,-4.158847928e-01f,-3.839956596e-02f,1.812950224e-01f,
-1.937313229e-01f,6.752948463e-02f,-1.560615301e-01f,-5.805093050e-02f,-6.367585063e-02f,-1.101717353e-01f,-2.140362747e-03f,1.529275179e-01f,1.711699963e-01f,2.515118420e-01f,8.187390864e-02f,-1.065867320e-01f,-9.218369424e-02f,-1.925136521e-02f,-1.763716224e-03f,-9.047681838e-02f,
2.386290282e-01f,8.971446007e-02f,-1.926152036e-02f,4.658220112e-01f,-2.893431187e-01f,1.696788222e-01f,-1.078661531e-01f,4.951988757e-01f,-2.082674950e-01f,1.754411310e-01f,9.995395690e-02f,-8.562739193e-02f,-2.177437097e-01f,4.037808254e-02f,4.411324859e-03f,2.257454768e-02f,
-2.623191848e-02f,-8.972176909e-02f,1.270660013e-01f,4.113684408e-03f,5.677670240e-02f,-1.086802334e-01f,1.078471914e-01f,2.185101993e-02f,-5.012397468e-02f,-1.187473536e-02f,2.859435230e-02f,-5.910389125e-03f,5.023344606e-02f,7.095622271e-02f,-3.502587229e-02f,-3.428939357e-02f,
-2.148132473e-01f,2.646135390e-01f,-1.438483149e-01f,8.310675621e-02f,1.678180844e-01f,8.982373774e-02f,-8.038701117e-02f,7.585970685e-03f,9.433479607e-02f,-2.289443091e-02f,-1.519125104e-01f,4.977680277e-03f,-1.451766491e-01f,-4.539972288e-04f,1.785962377e-03f,-1.410911232e-01f,
-6.206303462e-02f,-1.113470048e-01f,1.605216116e-01f,-1.343002021e-01f,-3.879360110e-02f,1.043981966e-02f,-1.236519031e-02f,-1.100917161e-01f,-7.366599888e-02f,5.657236651e-02f,-1.399160475e-01f,4.642232135e-02f,2.812694572e-02f,-1.936682314e-02f,9.880097210e-02f,7.947170734e-02f,
-3.379601287e-03f,5.372584611e-02f,-1.235912368e-01f,9.756797552e-02f,-4.853617027e-02f,-5.557955056e-02f,4.953480884e-02f,6.214173045e-03f,-4.708004743e-02f,-1.336418390e-01f,8.389445394e-02f,8.130297065e-02f,1.400545239e-02f,-2.574799024e-02f,8.920176327e-02f,6.808856875e-02f,
-1.735197753e-01f,-3.046215326e-02f,3.503007069e-02f,-3.081308603e-01f,-1.035011932e-01f,1.229015887e-01f,1.194371656e-01f,9.805881977e-02f,3.083095141e-02f,-1.001926661e-01f,-3.545601666e-01f,-3.386791945e-01f,5.228629708e-01f,4.838595912e-02f,-2.325174361e-01f,-1.264308244e-01f,
9.078542888e-02f,1.186552923e-02f,7.507257164e-02f,-1.440253407e-01f,1.413484663e-01f,7.584754378e-02f,-1.232836209e-02f,8.783449419e-03f,3.444055095e-02f,-2.244050354e-01f,-2.515117824e-01f,-2.003339678e-02f,-4.658740461e-01f,5.020934716e-02f,7.184755523e-03f,-1.110289767e-01f,
1.168853324e-02f,3.732870100e-03f,3.660713881e-02f,-3.823062778e-01f,-1.747184396e-01f,5.228397846e-01f,-1.491582096e-01f,-3.432109058e-01f,-2.083744556e-01f,1.299279742e-03f,-9.491549432e-02f,-3.004853129e-01f,-7.618762851e-01f,-6.135304570e-01f,-2.219795883e-01f,1.520361006e-01f,
1.419651806e-01f,1.612280309e-01f,-2.830406725e-01f,-3.474084437e-01f,7.170139253e-02f,-1.482522786e-01f,-1.544152498e-01f,-2.661359310e-01f,2.240904272e-01f,1.451355312e-02f,-6.766103953e-02f,8.482400328e-02f,6.237907708e-02f,-1.698555946e-01f,-1.275333762e-02f,3.316605985e-01f,
-1.441446096e-01f,1.818570048e-01f,3.996649012e-02f,6.795794517e-02f,-8.044585586e-02f,-1.879868805e-01f,2.131320909e-02f,-3.612629175e-01f,4.205575585e-02f,3.201420009e-01f,6.725018471e-02f,1.081117168e-01f,-1.837669313e-02f,-4.023357108e-02f,-9.913908690e-02f,-4.788047969e-01f,
-4.963512346e-02f,3.212188184e-01f,1.062416658e-02f,3.113254607e-01f,-1.982707530e-01f,-1.185799539e-01f,-2.569530606e-01f,3.202866763e-02f,-9.411563724e-02f,-4.014225304e-02f,-4.328560457e-02f,9.395389259e-02f,1.602185518e-01f,2.274847776e-01f,-3.768976927e-01f,7.791380584e-02f,
1.476917267e-01f,-8.911583573e-02f,1.556348652e-01f,-2.622209787e-01f,-4.552318901e-02f,2.178216875e-01f,2.766280472e-01f,5.902926438e-03f,9.214184433e-02f,-6.051009148e-02f,-5.907973647e-02f,-6.570917368e-02f,1.444183942e-02f,7.245508581e-02f,-3.431252390e-02f,-7.558362186e-02f,
-1.843263954e-01f,1.370960772e-01f,-3.595135733e-02f,1.281065587e-02f,1.638133824e-01f,-1.545391381e-01f,5.242826045e-02f,-4.804708436e-02f,9.746748954e-02f,-1.739065051e-01f,-1.364841461e-01f,1.426818520e-01f,-2.287800908e-01f,1.213164181e-01f,-1.096094325e-01f,-3.221137822e-01f,
4.968692362e-02f,4.608608782e-02f,-3.087229729e-01f,1.123401672e-01f,-5.517781246e-03f,-6.642344594e-02f,-3.242530674e-02f,-3.990871459e-02f,3.077220358e-02f,7.115942240e-02f,-9.508534521e-02f,5.767688155e-02f,4.955102224e-03f,-2.454607002e-02f,8.661759645e-02f,-7.798174769e-02f,
-2.446024306e-02f,-1.347092912e-03f,-5.456428230e-02f,7.038042694e-02f,6.764826179e-02f,2.065667696e-02f,7.218497805e-03f,-7.057641633e-03f,4.578239098e-02f,-3.073580749e-02f,1.368050724e-01f,-6.077754498e-02f,4.652306437e-02f,-4.294202849e-02f,-1.980671138e-01f,5.594829470e-02f,
-1.744157523e-01f,-2.568668127e-02f,1.124289855e-01f,-2.019512057e-01f,1.362147778e-01f,3.685832918e-01f,5.723182112e-02f,3.012376428e-01f,-8.504102379e-02f,-1.957730949e-01f,1.412501931e-01f,-3.223645389e-01f,3.085995913e-01f,-2.009794116e-02f,-4.094409347e-01f,-2.409523129e-01f,
8.824947476e-02f,3.371874243e-02f,2.483627796e-01f,1.547657792e-02f,-1.225313917e-01f,1.598791778e-01f,-6.301663071e-02f,-9.616984427e-02f,-3.954562824e-03f,-1.128010303e-01f,-2.275701761e-01f,1.636003852e-01f,-1.412013173e-01f,3.314324617e-01f,3.497671485e-01f,-2.784640901e-02f,
2.322844714e-01f,-2.452925146e-01f,-3.698360026e-01f,-1.435042024e-01f,-5.761666894e-01f,3.113159835e-01f,2.067725062e-01f,-4.432526827e-01f,-1.506643295e-01f,2.477908507e-03f,2.594957948e-01f,-3.158032000e-01f,-2.512048781e-01f,-3.109132349e-01f,8.090446889e-02f,-1.032135263e-01f,
1.137993336e-01f,7.837205380e-02f,-1.379713267e-01f,3.215413690e-01f,2.771244347e-01f,-1.478837132e-01f,-2.564443350e-01f,-1.241128370e-01f,1.525129825e-01f,2.628971823e-03f,4.631311074e-02f,-2.313015014e-01f,1.639261097e-02f,-4.392419159e-01f,7.408069074e-02f,2.892818749e-01f,
4.671090841e-02f,4.203325883e-02f,2.721296549e-01f,-3.222789615e-02f,-1.723750979e-01f,-8.008909971e-02f,1.904285103e-01f,-1.754759401e-01f,-1.561425775e-01f,1.729043722e-01f,-5.711513758e-01f,2.827579975e-01f,3.431325406e-02f,-1.453737728e-02f,-1.195001137e-02f,-1.463781148e-01f,
-3.560245633e-01f,-1.328774989e-01f,1.979528069e-01f,1.387416571e-01f,1.699710935e-01f,-1.828932166e-01f,1.738419235e-01f,2.900829613e-01f,-2.263411433e-01f,-1.132639050e-01f,-2.167067528e-01f,-3.323960304e-01f,9.633302689e-02f,6.754634529e-02f,2.057649381e-02f,-4.074318334e-02f,
3.204990625e-01f,1.621042937e-01f,2.294252366e-01f,2.795807123e-01f,-1.578995958e-02f,-1.715496033e-01f,7.208791375e-01f,2.587613463e-01f,-1.316062957e-01f,-2.259872854e-02f,6.076788157e-02f,-2.019011788e-02f,-3.476253897e-02f,1.614496112e-02f,-4.221082851e-02f,-1.023180038e-02f,
-1.026105806e-01f,-3.523589075e-01f,4.452009201e-01f,3.377794921e-01f,-3.622717410e-02f,-5.277604237e-02f,4.670209885e-01f,6.497230381e-02f,-1.513727754e-01f,-1.356803179e-01f,-2.250785530e-01f,1.097850725e-01f,1.148590259e-02f,5.727837235e-02f,-2.365534902e-01f,-2.267748117e-01f,
7.349175215e-02f,1.243839115e-01f,-9.855171293e-02f,-4.164583683e-01f,9.761571139e-02f,1.867626756e-01f,-1.206130013e-01f,5.186276976e-03f,-7.216641307e-02f,5.230065063e-02f,4.307589307e-02f,-3.220294416e-02f,-6.217439473e-02f,3.481264040e-02f,-1.453045115e-04f,5.043604970e-02f,
1.924438775e-02f,-1.250959001e-02f,2.106799372e-02f,1.832385175e-02f,-5.330828205e-02f,-2.850351855e-02f,2.472508326e-02f,7.483597100e-02f,5.632934719e-02f,-7.964036427e-03f,1.323365420e-01f,-9.278524667e-02f,-4.414161295e-02f,1.037347168e-01f,1.528950036e-02f,-2.237411961e-02f,
-1.039222255e-01f,6.064711884e-02f,2.170934826e-01f,5.699500442e-02f,-1.162702888e-01f,3.155786078e-03f,1.488685459e-01f,-1.348801702e-01f,9.220514446e-02f,2.359008491e-01f,-4.914361238e-02f,-1.080156490e-02f,-5.263491347e-02f,-2.302849293e-01f,-1.285419706e-02f,-1.082553491e-01f,
8.983387798e-02f,2.585236914e-02f,1.006127223e-01f,-7.375789434e-02f,5.898242444e-02f,-2.876485288e-01f,4.780964926e-03f,3.606720269e-02f,-6.322144717e-02f,-5.931434408e-02f,-8.729107678e-02f,2.292978168e-01f,1.022965088e-01f,1.643486023e-01f,1.191981435e-01f,-3.462303663e-03f,
-1.170125306e-01f,-8.644450456e-02f,9.753116965e-02f,2.591060996e-01f,6.539842486e-02f,3.027622029e-02f,1.839094907e-01f,-1.599043012e-01f,-2.624274790e-01f,1.119204611e-01f,6.760944128e-01f,-6.549783349e-01f,-3.729658127e-01f,-1.960704625e-01f,1.820802838e-01f,-1.117071733e-01f,
1.430826187e-01f,1.474292427e-01f,1.044390723e-01f,3.231598139e-01f,-1.340885013e-01f,2.017241120e-01f,-1.155492514e-01f,-9.321665764e-02f,8.369958401e-02f,-7.616255432e-02f,2.913443148e-01f,1.224005297e-01f,3.641931573e-03f,1.903572381e-01f,2.139333338e-01f,2.203439027e-01f,
5.512639508e-02f,2.751716413e-03f,3.152298629e-01f,8.911304921e-02f,-3.162265942e-02f,1.466963440e-01f,2.107267641e-02f,-1.004235148e-01f,-2.745874971e-02f,-1.740408987e-01f,-5.834079534e-02f,-7.865000516e-03f,2.840605676e-01f,5.108752847e-02f,1.299616396e-01f,1.984545439e-01f,
3.233246207e-01f,-3.426998258e-01f,1.282189041e-01f,-2.611365914e-02f,-1.564984620e-01f,4.120154083e-01f,4.795911610e-01f,-3.400804847e-02f,9.935599566e-02f,-3.870361149e-01f,-1.396486163e-01f,-1.986667663e-01f,-1.903083026e-01f,-1.873840988e-01f,8.790387958e-02f,-1.508591622e-01f,
-8.526286483e-02f,2.479445562e-02f,-6.011550501e-02f,-2.591382265e-01f,9.975839406e-03f,-4.097877815e-02f,-3.636883497e-01f,-1.738854051e-01f,4.933638498e-03f,-3.526340425e-02f,-6.092043966e-02f,6.071472913e-02f,-5.806253478e-02f,-4.190552980e-03f,7.614132017e-03f,-4.303943366e-02f,
-4.263767973e-02f,-1.164565980e-01f,1.233241931e-01f,-1.223485619e-01f,-3.747517243e-02f,1.120657194e-02f,2.048414014e-02f,2.437091172e-01f,3.460131288e-01f,6.256733835e-02f,1.771590114e-01f,-3.148024902e-02f,-3.127298504e-02f,-1.026410982e-01f,6.499517709e-02f,4.007892907e-01f,
-1.478428394e-01f,-1.448640879e-02f,6.592494249e-02f,-1.825683378e-02f,-1.345289648e-01f,-5.120123969e-04f,6.436475366e-02f,3.480988368e-02f,-1.662848145e-01f,7.597986609e-02f,-2.776150778e-02f,2.272767201e-02f,-8.706732839e-02f,-9.613203257e-02f,-5.395752937e-02f,1.094194967e-02f,
4.283073545e-02f,3.459742665e-02f,-8.416955918e-02f,-7.177587599e-02f,-8.601238951e-03f,1.243546233e-02f,2.744736150e-02f,-3.020147048e-02f,9.015630186e-02f,-3.414275870e-02f,-1.693227291e-01f,-6.663847715e-02f,-4.700637236e-02f,6.025879085e-02f,1.447654814e-01f,1.025905684e-01f,
1.175615340e-01f,-1.731330007e-01f,2.700740993e-01f,6.702584215e-03f,-3.175513074e-02f,-3.120315373e-01f,-1.306816787e-01f,-2.266559601e-01f,-8.294968307e-02f,-7.973670214e-02f,-1.311466545e-01f,1.123318914e-02f,-1.482474804e-01f,3.391959965e-01f,4.122325182e-01f,-1.835364103e-01f,
3.676525876e-02f,-5.234779045e-02f,-8.508145064e-02f,2.249523252e-01f,-3.929226846e-02f,-1.244686246e-01f,1.723854803e-02f,1.659803092e-01f,1.136550754e-01f,3.317573294e-02f,1.245887950e-01f,2.465409935e-01f,1.994372308e-01f,1.920045465e-01f,-3.147622049e-01f,-1.445418000e-01f,
-1.292450577e-01f,7.869184762e-02f,1.299181283e-01f,-2.741016746e-01f,-1.220140234e-02f,-1.677196622e-01f,2.226360887e-01f,2.106111348e-01f,-1.587128192e-01f,-2.218561247e-02f,-8.277548552e-01f,6.475789100e-02f,1.561554223e-01f,5.271939188e-02f,-4.258749187e-01f,2.113899589e-01f,
-2.777025104e-02f,-5.110070109e-02f,-6.319442764e-03f,-4.391926900e-02f,-1.071069855e-02f,-5.319692567e-02f,-5.002233014e-02f,5.443402752e-02f,-7.083805650e-02f,-5.680274591e-02f,-3.010900319e-02f,-3.517362475e-02f,-3.742728382e-02f,-6.479033083e-02f,4.850916937e-02f,8.423313498e-03f,
1.986352913e-02f,6.653123349e-02f,-4.645314813e-02f,1.251669321e-02f,2.486440726e-02f,2.800480090e-02f,3.246593755e-03f,7.766291499e-02f,-6.890697032e-02f,6.985054910e-02f,-7.688756287e-02f,-8.452297747e-02f,-6.208163872e-02f,-8.557174355e-03f,-8.290642500e-02f,-6.355565786e-02f,
-4.984267056e-02f,4.719351977e-02f,1.688890345e-02f,-1.865136065e-02f,-6.764417887e-02f,-1.421735529e-02f,-5.266996101e-02f,5.877492577e-02f,-1.360743493e-02f,4.128582403e-02f,-9.263409302e-03f,2.910866402e-02f,7.047601044e-02f,-2.292309329e-02f,-7.855825871e-02f,6.802504510e-02f,
3.237700788e-03f,-1.491195988e-02f,-4.810700193e-02f,6.614094228e-02f,-5.249519646e-02f,-3.958713263e-03f,-2.244763821e-02f,-2.159402939e-03f,5.960332230e-02f,6.388816983e-02f,1.396436244e-02f,-5.687818676e-02f,-6.286455691e-02f,-7.714080065e-02f,-4.946984351e-03f,5.401427299e-02f,
2.265330404e-02f,-6.421892345e-02f,6.478273124e-02f,3.615596518e-02f,-3.529689088e-02f,5.775496364e-03f,-1.200159546e-02f,1.151684299e-02f,2.840958722e-02f,-5.184779316e-02f,-4.105618596e-02f,-4.467688128e-02f,-2.954797633e-02f,-5.334785208e-02f,4.650012031e-02f,7.323603332e-02f,
-3.355902433e-02f,-5.104190856e-02f,3.839650005e-02f,5.244026706e-02f,2.760221437e-02f,7.855442166e-02f,-1.490786235e-04f,-4.210179672e-02f,6.197094545e-02f,5.590546411e-03f,9.738343768e-03f,-4.053400084e-02f,4.520367086e-02f,8.444299549e-02f,4.256213084e-03f,8.059138805e-02f,
1.947696693e-02f,6.860254798e-03f,-4.498719797e-02f,-1.459389552e-02f,3.664378822e-02f,-3.754561394e-02f,1.146786846e-02f,-5.769402161e-02f,-3.143128008e-02f,-2.626371756e-02f,3.190582246e-02f,-5.115972459e-02f,1.789989136e-02f,-5.856118351e-02f,2.645364031e-02f,-4.205383360e-02f,
-7.468988001e-02f,-4.257997870e-02f,6.919134408e-02f,-7.991179824e-03f,2.491295338e-03f,4.011163861e-02f,-2.265634760e-02f,6.562105566e-02f,-3.710246459e-02f,-2.056726441e-02f,-7.423033565e-02f,-5.191912502e-02f,3.706413880e-02f,-1.732179336e-02f,-5.421031266e-03f,4.302210361e-02f,
3.325648978e-02f,-2.218821272e-02f,-5.706531554e-02f,-2.779610455e-02f,3.111946955e-02f,-6.029884145e-02f,4.644300044e-02f,-4.994320124e-02f,-3.111247346e-02f,1.665397733e-02f,-8.623778820e-03f,6.130976230e-02f,6.701219827e-03f,5.225340277e-02f,-4.516218230e-02f,-7.964047790e-02f,
-3.207563981e-02f,1.435293630e-02f,6.366336346e-02f,-4.390393943e-02f,3.428819403e-02f,-4.097247496e-02f,-6.322977692e-02f,4.119894654e-02f,5.778350751e-04f,6.251727790e-02f,3.653918952e-02f,1.537363743e-03f,6.133844703e-02f,-2.266142145e-02f,-8.343650959e-03f,2.374387532e-02f,
-4.575314000e-02f,-7.320980728e-02f,6.740472466e-02f,-3.004327789e-02f,-7.127074152e-02f,3.500748426e-02f,1.423475146e-02f,2.099720389e-02f,1.650495338e-03f,-4.813255370e-02f,-1.237586141e-03f,-7.108607888e-02f,2.845697105e-02f,6.254125386e-02f,4.560264200e-02f,-1.940091699e-02f,
-4.432458896e-03f,-1.909990795e-03f,5.652068183e-02f,-3.386550769e-02f,-2.571883088e-04f,3.555643186e-02f,-1.157004386e-02f,-5.773542076e-02f,2.580180019e-02f,-6.530582905e-02f,-5.863212049e-02f,-7.712395396e-03f,-3.030738793e-02f,-4.108703136e-02f,-6.238464266e-02f,7.839149982e-02f,
2.227731049e-02f,7.178796083e-02f,7.006651908e-02f,-5.963362753e-02f,-5.416769534e-02f,1.654234529e-02f,-7.841529697e-02f,-7.246496528e-02f,6.840669364e-02f,-4.697834328e-02f,3.899117559e-02f,5.385674536e-03f,1.609890163e-02f,-5.229871720e-02f,6.762576848e-02f,-9.528237395e-03f,
-6.268860400e-02f,6.518914551e-02f,5.605017394e-02f,7.653256506e-02f,4.509668797e-03f,4.716489464e-02f,-3.744842857e-02f,-6.483543664e-02f,1.277530938e-02f,1.846451312e-02f,2.419976890e-02f,-6.253692508e-02f,-9.325616062e-03f,-6.239138544e-02f,-4.746024683e-02f,-3.779359534e-02f,
1.768025011e-02f,-4.693042114e-02f,7.268924266e-02f,7.519299537e-02f,-6.439147890e-02f,4.387333989e-02f,-6.011657044e-02f,3.412842751e-03f,-5.005620047e-02f,5.119848996e-03f,5.984618329e-04f,-4.100174829e-02f,-7.313767076e-02f,-1.869857684e-02f,3.958556801e-03f,6.062240899e-02f,
-3.651221469e-02f,-5.365843326e-02f,7.815830410e-03f,-7.529539615e-02f,-7.331272960e-02f,4.246600717e-02f,4.226279259e-02f,7.226539403e-02f,4.553307220e-02f,6.935893744e-02f,7.162097842e-02f,1.027409732e-02f,4.631274194e-02f,2.779985592e-02f,2.019787952e-02f,3.050281107e-02f,
1.622772962e-02f,4.735425860e-02f,-9.133629501e-03f,3.236103803e-02f,7.100293785e-02f,5.999355763e-02f,-5.630179122e-02f,-1.557860523e-02f,3.609318286e-02f,-3.117910773e-02f,-6.360153854e-02f,4.086320847e-02f,1.607462019e-02f,1.895508543e-02f,1.544273086e-02f,2.655947581e-02f,
-7.345180959e-02f,1.914790273e-02f,-1.156045496e-02f,-3.172989562e-02f,-3.304578364e-03f,4.442357272e-02f,-1.610968262e-02f,4.449829459e-02f,-6.632380933e-02f,-5.471989512e-02f,-1.437350363e-02f,-4.259369895e-02f,3.122285008e-02f,7.569218427e-02f,7.463900000e-02f,6.701795012e-02f,
-5.845931917e-02f,6.088898331e-02f,-7.361198962e-02f,-5.861525238e-02f,-5.885979533e-02f,2.641141228e-02f,3.047240525e-02f,-6.418967247e-02f,-2.162019908e-02f,4.554797709e-02f,3.823918104e-02f,3.209807724e-02f,-4.439654201e-02f,-9.216050617e-03f,2.384907007e-02f,3.444421291e-02f,
-1.996112615e-02f,3.208713606e-03f,-4.105518386e-02f,3.275320679e-02f,7.602725178e-03f,-1.400296390e-02f,-2.786264196e-02f,4.375206679e-02f,1.999715716e-02f,6.174873188e-02f,-5.348458141e-02f,5.732315034e-02f,4.435365275e-02f,7.306223363e-02f,-1.228600461e-02f,-3.118380159e-02f,
9.144878387e-02f,1.081143916e-01f,2.964836657e-01f,1.602905393e-01f,-3.288834691e-01f,-2.089197785e-01f,-2.398732007e-01f,-3.167397529e-02f,-1.121833995e-01f,2.598672733e-02f,-1.021338105e-01f,-7.871438563e-02f,2.929855287e-01f,-2.361253053e-01f,-1.040262729e-01f,4.424998537e-02f,
1.539753824e-01f,1.625346094e-01f,-1.366006434e-01f,9.672609717e-02f,-3.184001893e-02f,-4.547823220e-02f,7.377779484e-02f,-1.255564839e-01f,4.072792456e-02f,-1.660885066e-01f,6.152369082e-02f,6.749112904e-02f,-1.397663057e-01f,7.791236788e-02f,-1.348878592e-01f,-3.784547448e-01f,
-6.581738591e-02f,-1.231812015e-01f,5.884881690e-02f,1.923672296e-02f,1.771508344e-02f,-7.494847756e-03f,-9.856075794e-02f,7.336728275e-02f,-2.414488494e-01f,1.791714430e-01f,-1.970179528e-01f,-2.316449024e-02f,5.232526362e-02f,1.684913225e-02f,-8.022411913e-02f,-4.030705243e-02f,
1.985339075e-01f,-1.473623812e-01f,1.821185946e-01f,6.383544952e-02f,2.226218879e-01f,-1.632248908e-01f,2.230098099e-01f,2.374154627e-01f,-3.379158080e-01f,7.262349874e-02f,2.606257331e-03f,7.491896302e-02f,5.378346890e-02f,7.739902288e-02f,-6.592030078e-02f,4.626453668e-02f,
2.085605115e-01f,-4.411540926e-03f,-1.769252568e-01f,1.737331748e-01f,8.325644583e-02f,1.609704196e-01f,5.307357758e-02f,-7.499609143e-03f,-9.411592036e-03f,5.266242102e-02f,-1.390065998e-01f,6.459248252e-03f,-9.482327849e-02f,1.892506480e-01f,-1.326642781e-01f,8.677113801e-02f,
-1.427438855e-01f,-1.882730871e-01f,-6.409256905e-02f,-7.564020157e-02f,6.770117581e-02f,2.523170970e-02f,1.441326831e-02f,-7.390554994e-02f,1.150229871e-01f,-7.419628650e-02f,-1.000841931e-01f,3.811150789e-02f,-9.126072377e-02f,5.036777351e-03f,-1.611071229e-01f,-2.269673347e-02f,
-6.367889792e-02f,-1.072922163e-02f,-3.187370952e-03f,-2.803017199e-02f,2.921234816e-02f,-6.485568732e-02f,4.860518500e-02f,4.645451531e-02f,-5.263411626e-02f,-1.048325002e-01f,7.038623840e-02f,-9.834118932e-02f,4.860172421e-02f,-6.103774160e-02f,-9.045628458e-02f,4.840333760e-02f,
-1.150861606e-01f,1.167891026e-01f,1.583329141e-01f,-3.634264320e-02f,1.136724427e-01f,-1.491987109e-01f,8.475850336e-03f,-2.270592004e-01f,7.035009563e-02f,-2.462646812e-01f,-1.198492199e-01f,-8.347338438e-02f,3.040347062e-02f,-1.484420449e-01f,-1.552092731e-01f,-7.097220980e-03f,
1.379545331e-01f,-3.592821583e-02f,-1.357809603e-01f,-4.341805726e-02f,1.619327962e-01f,4.971187189e-02f,-1.273314189e-02f,-1.418167651e-01f,1.619800180e-01f,-1.225452498e-01f,2.907230519e-02f,9.094152600e-02f,-1.757945865e-01f,-2.450136542e-01f,-7.689335942e-02f,-7.667044550e-02f,
-1.210390255e-01f,3.380450979e-02f,-3.222736120e-01f,-1.590514630e-01f,-3.853300214e-02f,1.219234616e-01f,-9.960099123e-03f,6.232887506e-02f,-2.601810917e-02f,1.118338108e-01f,-1.427811384e-01f,-6.602690220e-01f,-7.268275321e-02f,-1.221092224e+00f,5.061323047e-01f,-5.816359539e-03f,
1.194089130e-01f,-2.920670621e-02f,7.510205507e-01f,-3.123209476e-01f,-1.666833907e-01f,-2.513648272e-01f,-2.077929974e-01f,4.810266942e-02f,4.342069104e-02f,8.275415748e-02f,1.388007775e-02f,-6.804326922e-02f,2.100294530e-01f,-1.886351705e-01f,8.133845776e-02f,1.142550334e-01f,
1.793161929e-01f,5.483990908e-02f,-1.696195155e-01f,4.767365009e-02f,-4.426143318e-02f,-9.965954721e-02f,1.989385784e-01f,2.966651618e-01f,-2.868426591e-02f,6.756311655e-02f,6.681252271e-02f,1.116270944e-01f,-1.354593337e-01f,-1.383833494e-02f,-9.574645013e-02f,-3.629321456e-01f,
-7.745927572e-02f,-1.268444657e-01f,7.895190269e-02f,1.304942816e-01f,-1.998426020e-01f,-7.341133058e-02f,-2.511098683e-01f,4.854269326e-02f,-2.781260014e-01f,7.212288678e-02f,-1.490421221e-02f,-1.772661805e-01f,1.707324833e-01f,1.595492512e-01f,2.769500203e-02f,4.640416801e-02f,
1.807239801e-01f,8.571469784e-02f,2.785482109e-01f,-7.235694677e-02f,4.488706961e-02f,-1.762984097e-01f,4.605594873e-01f,7.116359472e-01f,-2.401729077e-01f,1.699152589e-02f,-3.786878288e-02f,2.541623265e-02f,-2.465365268e-02f,6.526989490e-02f,-2.001389489e-02f,-2.224117890e-02f,
5.417931080e-02f,-2.174253762e-02f,9.487614967e-03f,-1.843715161e-01f,-3.294590861e-02f,-4.502187669e-02f,-5.786649510e-02f,-1.146645993e-01f,1.384749170e-02f,1.253261566e-01f,-1.358987838e-01f,1.012525633e-01f,-1.855003685e-01f,-1.447055191e-01f,-3.542717174e-02f,-4.066979885e-02f,
-8.998985589e-02f,-1.878307760e-01f,9.421040863e-02f,-1.447544694e-01f,9.419537336e-02f,8.525368758e-03f,1.389236152e-01f,-1.801778004e-02f,-1.038649827e-01f,-7.493706048e-02f,4.320806637e-02f,-3.133486956e-02f,-8.731358498e-02f,5.228676274e-02f,3.761930764e-02f,-3.243167698e-02f,
5.947458744e-02f,-8.623897284e-02f,3.652502224e-02f,-8.158770949e-02f,-5.686896667e-02f,-3.959657624e-03f,2.636500821e-02f,-9.241367877e-02f,7.517904788e-02f,-1.554222405e-01f,5.602342635e-02f,7.142479718e-02f,-5.888088793e-02f,-5.077222362e-02f,-1.882890426e-02f,1.227547303e-01f,
-1.299192607e-01f,-3.747982532e-02f,5.121874064e-02f,8.201394230e-02f,-2.360483408e-01f,-1.125529855e-01f,3.214100376e-02f,2.373982407e-02f,-1.667006910e-01f,-3.890749067e-02f,-1.851877272e-01f,1.407618076e-01f,-6.203771383e-02f,-2.613304853e-01f,5.497780442e-02f,3.145430684e-01f,
-2.009601593e-01f,1.147931442e-02f,1.212513074e-01f,-5.634949729e-02f,-8.254141361e-02f,1.346561313e-01f,-6.011810619e-03f,-2.823335677e-02f,1.552350670e-01f,-1.842683554e-02f,1.546426713e-01f,6.375177205e-02f,1.864540577e-01f,-4.374139607e-01f,-1.699458212e-01f,-3.311626241e-02f,
3.510216624e-02f,8.857366443e-02f,5.279665068e-02f,-1.153265387e-01f,7.049188763e-02f,-7.772508264e-02f,-3.254433870e-01f,2.962235212e-01f,3.399594128e-01f,1.219450533e-01f,-2.529542446e-01f,-4.100302160e-01f,-5.615455508e-01f,-3.757899404e-01f,-1.046645418e-01f,7.882784307e-02f,
-1.141001284e-02f,-6.225428730e-02f,-2.794145048e-02f,4.819410294e-02f,-7.802617550e-02f,1.774983294e-02f,-4.065722227e-03f,-6.256578863e-02f,-2.790485695e-02f,2.728892490e-03f,-2.723891288e-02f,2.082773484e-02f,6.170092151e-02f,3.355980664e-02f,-1.324403938e-02f,1.082587987e-03f,
7.445096225e-02f,3.989508748e-02f,4.055076838e-02f,-2.420772240e-02f,2.098834515e-02f,4.397993535e-02f,-4.576886073e-02f,-6.451259367e-03f,5.501302332e-02f,6.691320986e-02f,5.455885082e-02f,-2.147424035e-02f,-7.336241007e-02f,-2.973559126e-02f,-4.228732362e-02f,-3.816133738e-02f,
5.142208189e-03f,-9.567826986e-03f,-6.453818083e-02f,2.711595036e-02f,-7.252079993e-02f,8.828878403e-03f,-6.827277690e-02f,3.368585557e-02f,4.727207869e-02f,-1.235345751e-02f,6.888958067e-02f,-3.709986061e-02f,3.153269738e-02f,1.331156492e-02f,-2.940044180e-02f,-5.011072755e-02f,
-5.041353405e-03f,2.296677977e-02f,6.727544218e-02f,6.033483893e-03f,3.480313718e-02f,2.362008393e-02f,2.483545989e-02f,-2.194080502e-02f,-7.684144378e-02f,3.995756060e-02f,3.407351673e-02f,4.894178361e-02f,-6.240747869e-02f,-4.381616041e-02f,9.493931197e-03f,-5.982135981e-02f,
-3.473658487e-02f,2.862471342e-02f,4.667402804e-02f,6.190954894e-02f,-3.467907198e-03f,4.407731444e-02f,-6.155925617e-02f,-3.175939247e-02f,2.964240871e-02f,7.570189890e-03f,-5.634987354e-02f,-4.128589109e-02f,5.863607675e-02f,3.166718781e-02f,-7.371375710e-02f,6.399614364e-02f,
3.759802133e-02f,3.896282613e-02f,-2.390516922e-02f,-5.260504410e-02f,1.082321256e-02f,7.355873287e-02f,-3.574896604e-02f,6.236191839e-02f,-2.915446833e-02f,-6.885053217e-02f,4.777946323e-02f,7.063754648e-02f,-3.697830485e-03f,2.051462978e-02f,7.087416947e-02f,7.097443193e-02f,
6.920509785e-02f,9.203101508e-03f,6.885730475e-02f,-6.968427449e-02f,-2.504772320e-02f,6.562305242e-02f,3.601790220e-02f,7.335636765e-03f,-3.421486542e-02f,-7.754977047e-02f,3.192250431e-02f,6.815110892e-02f,1.686976850e-02f,1.483756769e-02f,5.224951357e-02f,-5.311634019e-02f,
3.252717108e-02f,5.695053190e-02f,5.651524663e-02f,1.819545776e-02f,-3.952987492e-02f,-1.141563058e-02f,5.204709619e-02f,-1.005941629e-02f,6.392670423e-02f,-6.387364864e-02f,-5.869850516e-02f,6.063469499e-02f,-5.401343852e-02f,-6.018518470e-04f,-6.228132546e-02f,-2.207150869e-02f,
5.623566359e-02f,-5.843663216e-02f,-4.509933293e-02f,-2.171815559e-02f,-5.179486424e-02f,-6.194894761e-02f,-1.825891063e-02f,-5.020847917e-02f,2.669537067e-02f,3.174671903e-03f,-5.466353148e-03f,9.075962007e-03f,-3.683087602e-02f,-6.645593047e-02f,-2.363858372e-02f,-7.531955093e-02f,
8.280212060e-03f,4.090891033e-02f,-1.847891510e-02f,-4.841952026e-02f,3.275495023e-02f,3.862199187e-02f,-3.881188855e-02f,-6.990085542e-02f,-2.144763432e-02f,-4.519790038e-02f,3.487998620e-03f,-2.666468546e-02f,7.438705117e-02f,-5.204625428e-02f,2.205556445e-02f,5.134471506e-02f,
2.751159668e-01f,2.280524373e-01f,3.223163188e-01f,8.901186287e-02f,-7.981020026e-03f,-2.934880368e-02f,-3.076875769e-02f,-2.005817443e-01f,3.921458423e-01f,3.010669351e-02f,2.395812124e-01f,2.920717895e-01f,-2.793771625e-01f,-4.172255993e-01f,8.916820586e-02f,3.620091677e-01f,
-3.401479870e-02f,-2.833317518e-01f,2.457640916e-01f,-2.593842335e-02f,-2.125937045e-01f,-1.109657660e-01f,3.081636727e-01f,-2.936465740e-01f,1.796247438e-02f,-1.198179871e-01f,-4.036369026e-01f,5.445940420e-02f,1.508256048e-01f,-2.007784508e-02f,-1.238453016e-02f,-2.027766109e-01f,
4.542872310e-02f,3.371822089e-02f,2.279934585e-01f,4.360886812e-01f,-1.034402251e-01f,8.061724156e-02f,1.117270589e-01f,1.231302768e-01f,-7.306697965e-02f,-7.173320651e-02f,2.926028967e-01f,-1.365212053e-01f,8.432555012e-03f,-3.601450101e-02f,-7.217086852e-02f,-2.496564388e-01f,
-1.961581111e-01f,7.135101408e-02f,-3.417795058e-03f,-3.025487624e-02f,-5.435641855e-02f,-2.115949541e-01f,3.378165066e-01f,4.232347906e-01f,1.137204617e-01f,1.281213015e-02f,-2.695476636e-03f,5.787064880e-02f,7.358643413e-02f,-1.741656102e-03f,6.345120817e-02f,-7.395257801e-02f,
-5.439638346e-02f,6.760743260e-02f,3.720503533e-03f,3.704735637e-02f,-1.577319801e-01f,-8.075953275e-02f,2.299343646e-01f,-3.576554731e-02f,2.357646823e-02f,3.880467638e-02f,-1.134588867e-01f,-4.330454767e-02f,-3.197224112e-03f,2.501157485e-02f,-2.042035162e-01f,2.456932701e-02f,
-3.062489033e-01f,-1.695762128e-01f,3.213787973e-01f,1.424158216e-01f,-8.092267439e-03f,8.663026243e-02f,7.906313986e-02f,-3.953870013e-02f,5.647314712e-02f,-4.646955431e-02f,-7.799375057e-02f,-1.635629497e-02f,-7.843679190e-02f,-5.873416364e-02f,4.964382574e-02f,-3.935572505e-02f,
4.191782326e-02f,1.891745999e-02f,8.938465267e-02f,7.626915723e-02f,-7.454527169e-02f,2.207990328e-04f,5.750606209e-02f,-4.182235897e-02f,-3.970151767e-02f,-1.145195365e-01f,-9.348528087e-02f,1.060633138e-01f,9.043539315e-02f,1.384625882e-01f,-1.426498890e-01f,-1.087419242e-01f,
-1.088031977e-01f,6.905809045e-02f,-1.035150811e-01f,-3.213417530e-02f,-1.674474925e-01f,-6.266351938e-01f,4.752396047e-02f,1.146125942e-01f,-4.961014166e-02f,-4.108926952e-01f,-2.394396625e-02f,7.161144912e-02f,6.974237412e-02f,-2.840237319e-01f,-1.589576602e-01f,-1.285841912e-01f,
-7.893257588e-02f,-3.017525561e-02f,-1.643884182e-01f,4.407159984e-02f,1.518244296e-01f,3.488169014e-01f,7.778044045e-02f,-1.636518091e-01f,-3.073417023e-02f,-5.117694847e-03f,1.532405764e-01f,-8.117404580e-02f,-5.521185696e-02f,-4.916526377e-02f,-5.589674711e-01f,-8.053348213e-02f,
1.245500371e-01f,2.809859216e-01f,-1.294977367e-01f,-3.172017634e-02f,9.831915796e-02f,1.114053354e-01f,-9.392807633e-02f,2.131351829e-01f,-1.571864635e-01f,3.906045482e-02f,-8.884680867e-01f,-3.803892434e-01f,-4.887786508e-01f,-1.066882461e-01f,-1.498630792e-01f,-1.212618798e-01f,
2.175346948e-02f,-1.088411640e-02f,3.168722987e-01f,4.077516496e-01f,-2.607216239e-01f,1.474411786e-01f,-1.903086454e-01f,-2.352837622e-01f,4.609198868e-01f,-1.337330672e-03f,9.375441819e-02f,1.545374747e-02f,-6.844839454e-02f,-5.259613991e-01f,2.314805239e-01f,5.137962103e-01f,
-6.398313493e-02f,-6.634707749e-02f,1.984322518e-01f,-9.746295959e-02f,-1.224106997e-01f,1.681942493e-01f,1.015322953e-01f,3.653168306e-02f,-1.486897562e-02f,8.580271900e-02f,-2.511088848e-01f,-3.330221772e-02f,8.325808495e-02f,9.509125352e-02f,1.987961680e-01f,-5.474640802e-02f,
2.087551057e-01f,-1.734638512e-01f,-1.358777471e-02f,3.654281199e-01f,-2.370640337e-01f,1.627360135e-01f,1.289984137e-01f,9.392729402e-02f,-1.356881708e-01f,6.759744883e-02f,-2.055322565e-02f,-2.386437356e-01f,-9.983664006e-02f,-2.355366945e-01f,2.380294492e-03f,3.538478166e-02f,
-9.131477028e-02f,-1.833969653e-01f,5.168447644e-02f,7.415752858e-02f,1.146045700e-01f,-1.121708676e-01f,1.078906935e-02f,1.829707474e-01f,8.866942674e-02f,-8.500024676e-03f,-1.220948249e-02f,-2.622292936e-02f,6.721901149e-02f,-4.318851978e-02f,-2.930269390e-02f,-2.056890354e-02f,
-3.182719648e-02f,1.401973329e-02f,3.102809191e-02f,1.158356592e-01f,3.269198909e-02f,2.704243064e-01f,1.525647789e-01f,2.002793998e-01f,2.043404579e-01f,-1.028223559e-01f,3.291376680e-02f,-2.571061850e-01f,-1.240787730e-01f,-2.863956988e-02f,-4.414135814e-01f,1.228952035e-02f,
-4.504356161e-02f,9.325462580e-02f,1.890749857e-02f,-1.274641305e-01f,-5.925832316e-02f,-2.104979753e-02f,1.252828389e-01f,-7.202875614e-02f,-1.384009868e-01f,8.042643219e-02f,-7.671797276e-02f,6.316400319e-02f,-9.200084955e-03f,4.576149955e-02f,-1.077118739e-01f,7.441173494e-02f,
8.907030523e-02f,-6.905633956e-02f,-1.255461127e-01f,1.026461273e-02f,7.860765606e-02f,1.941908151e-02f,-1.183208893e-03f,3.810335696e-02f,-3.656527400e-02f,-3.622293472e-02f,1.059010550e-01f,-1.017646417e-01f,4.164820164e-02f,-7.710271329e-02f,-4.514314607e-02f,4.912260920e-02f,
-1.618426591e-01f,-2.479490638e-01f,8.767072856e-02f,-6.860689074e-02f,-6.631053984e-02f,-3.147665411e-02f,5.293482542e-02f,-1.586086452e-01f,1.676698327e-01f,4.481708631e-02f,-2.941061556e-01f,-2.577553093e-01f,9.114456177e-02f,2.748756707e-01f,1.490481496e-01f,6.261650473e-02f,
2.023942769e-01f,-9.851698764e-03f,7.080784440e-02f,2.496857382e-02f,9.851288050e-02f,-1.275965720e-01f,-4.466572404e-02f,-6.947915256e-02f,7.161301374e-02f,-2.560325861e-01f,-8.928622305e-02f,3.531869873e-02f,1.662749201e-01f,2.029138058e-01f,-2.155850641e-03f,-1.489039790e-02f,
-1.113204584e-01f,1.313202232e-01f,-1.924842410e-02f,-3.902964890e-01f,-6.373775005e-02f,4.059452936e-02f,4.051961601e-01f,-1.489462554e-01f,3.069786727e-02f,-5.862950347e-03f,-8.099123240e-01f,-2.931196690e-01f,-1.162797362e-01f,-9.511983991e-01f,-3.459753692e-01f,3.558366373e-02f,
-4.606935009e-02f,-1.110362113e-01f,3.846303225e-01f,-9.492639452e-02f,1.027440578e-01f,1.345045865e-01f,1.160663515e-01f,2.979587615e-01f,-5.775291100e-02f,8.281344548e-03f,1.258171201e-01f,2.169538289e-01f,2.571337521e-01f,1.719471216e-01f,-8.773999661e-02f,1.335916668e-01f,
7.337141782e-02f,-9.135718644e-02f,-1.640839502e-02f,-3.104518913e-02f,-1.112066209e-01f,-1.661778688e-01f,3.826830089e-01f,6.111952066e-01f,-2.039666921e-01f,2.782351896e-02f,-3.856709003e-01f,1.735051125e-01f,-5.088188685e-03f,5.430487171e-02f,-2.769953012e-01f,3.776215389e-02f,
3.326297551e-02f,3.113655746e-01f,-4.663207009e-02f,1.575069577e-01f,1.805957705e-01f,-5.060956627e-02f,1.857754588e-01f,1.757345498e-01f,1.520472318e-01f,1.553824730e-02f,-7.821840048e-02f,-5.952060595e-02f,-2.636037394e-02f,2.041495144e-01f,3.012435138e-01f,2.042253315e-02f,
2.006246001e-01f,-1.322849244e-01f,3.181595802e-01f,9.172531217e-02f,2.700187564e-01f,-6.394113880e-03f,4.025285542e-01f,3.971264064e-01f,-1.685953140e-01f,-3.533907980e-02f,6.193684787e-02f,5.314029753e-03f,-5.237691849e-02f,-3.403677791e-02f,9.931452572e-03f,-2.747784555e-02f,
3.355484903e-01f,-9.759952128e-02f,-2.151601464e-01f,5.415977538e-02f,-7.427594811e-02f,-3.240827844e-02f,3.398747742e-01f,-2.473596632e-01f,-1.355517954e-01f,-1.137805060e-01f,-6.645379961e-02f,-9.201767296e-02f,-4.419281613e-03f,1.398826987e-01f,9.076159447e-03f,3.658820987e-01f,
1.126527786e-01f,-2.123175263e-01f,-1.408150792e-01f,7.086888701e-02f,-1.441061683e-02f,-1.027550623e-01f,-6.328346580e-02f,1.722955704e-02f,-4.765192978e-03f,-6.585618109e-02f,-1.771131717e-02f,7.252623141e-02f,-1.841456071e-02f,-1.442399174e-01f,7.855562121e-02f,-3.697990254e-02f,
4.401361570e-02f,-5.181930587e-02f,-1.545569394e-02f,-1.373319626e-01f,5.238867551e-02f,-2.769219317e-02f,-4.927068204e-02f,2.119878121e-02f,1.998452842e-02f,2.708911151e-02f,-4.338759929e-02f,-7.400561869e-02f,1.256254315e-01f,5.148905143e-02f,5.369656906e-02f,-8.190663904e-02f,
1.584276743e-02f,-1.211083159e-01f,-2.306083292e-01f,1.238547079e-02f,8.669526875e-02f,-1.437657177e-01f,-1.906362921e-01f,1.566717029e-01f,6.187883019e-02f,4.805084467e-01f,-2.974187136e-01f,1.205559820e-01f,-4.317973256e-01f,4.522232711e-02f,-2.961878181e-01f,-9.287994355e-02f,
1.399900317e-01f,6.783973426e-02f,-9.918226302e-02f,-4.061783105e-02f,-3.221663833e-02f,-8.155760169e-02f,-1.857995987e-02f,8.323936909e-02f,2.846888080e-02f,8.585868403e-03f,2.974436879e-01f,-2.309984863e-01f,1.411374956e-01f,2.294788063e-01f,-2.719678581e-01f,2.355758846e-02f,
1.623683125e-01f,-7.814417779e-02f,2.539973855e-01f,-4.484751821e-01f,4.463367164e-02f,-4.201650620e-01f,1.355862021e-01f,-3.507646620e-01f,-8.095057309e-02f,-2.204169333e-01f,-3.255847394e-01f,-1.648344249e-01f,1.020580307e-01f,-2.334561199e-01f,2.375273108e-01f,-1.567746252e-01f,
8.536302298e-02f,3.599422798e-02f,1.027921513e-01f,1.547508985e-01f,1.890011728e-01f,-4.135298543e-03f,-7.921830565e-02f,2.153494209e-01f,-6.544852257e-02f,-4.776959494e-02f,1.307131499e-01f,8.072409779e-02f,4.105614126e-01f,-4.080977291e-02f,-1.486806013e-02f,-1.408789754e-01f,
5.187056214e-02f,1.220740676e-01f,-5.554717965e-03f,3.037022613e-02f,-1.147711743e-02f,-1.933803260e-01f,1.528299302e-01f,-1.781783551e-01f,-2.229461214e-03f,-2.009226084e-01f,-1.501985639e-01f,4.867167771e-02f,5.940677226e-02f,4.218560085e-02f,1.360974759e-01f,2.431582659e-01f,
-4.588863254e-02f,-8.844396472e-02f,-9.992598742e-02f,-1.327614635e-01f,-1.001922861e-01f,5.456495285e-02f,1.981623024e-01f,1.287890822e-01f,2.191648781e-01f,9.490922093e-02f,-5.346492678e-02f,1.877753884e-01f,3.332425654e-01f,2.322437018e-01f,2.285133600e-01f,1.475622356e-01f,
1.283294857e-01f,1.558695138e-01f,3.151357546e-02f,-8.104694635e-02f,4.838547483e-02f,-2.687970400e-01f,7.341495156e-02f,1.006927639e-01f,-2.708415687e-01f,6.615113467e-02f,4.975721985e-02f,-6.104599684e-03f,1.614669710e-02f,1.708761556e-04f,4.136861116e-02f,4.512484372e-02f,
3.382891715e-01f,-5.445923284e-02f,-1.557860225e-01f,-1.242315918e-01f,6.328869145e-03f,-1.361339837e-01f,6.069596857e-02f,-8.131312579e-02f,1.425577998e-01f,2.170505747e-02f,-5.848214775e-02f,1.819070876e-01f,-2.326975465e-01f,-1.767030656e-01f,-1.371980757e-01f,2.462615222e-01f,
-1.317407936e-01f,-5.263581499e-02f,-2.479629517e-01f,-1.276556402e-01f,-4.851991683e-02f,2.595854364e-02f,-1.690487117e-01f,-1.453545541e-01f,4.621444270e-02f,-2.518527675e-03f,-4.358162731e-02f,1.600021124e-02f,-2.211492136e-02f,9.736153483e-02f,-9.175518900e-02f,-2.532946877e-02f,
1.796579454e-03f,-9.635867923e-02f,5.219423398e-02f,-3.853563964e-02f,-8.462715894e-02f,-6.784833036e-03f,2.064344473e-03f,-3.086509928e-02f,1.202533022e-02f,-1.270364821e-01f,1.523611601e-02f,-6.408166140e-02f,-1.594140381e-01f,-1.039923579e-01f,6.176223513e-03f,-1.853877008e-01f,
-1.531573087e-01f,2.561152279e-01f,1.822267473e-01f,9.321864694e-02f,1.491916776e-01f,-1.443996727e-01f,6.037593633e-02f,-1.379040927e-01f,-8.249621838e-02f,-2.660562098e-01f,-2.103469223e-01f,2.300118357e-01f,-4.036390409e-02f,-1.642457098e-01f,1.749046743e-01f,1.149927303e-01f,
-3.071228862e-01f,4.537748173e-04f,-2.210805416e-01f,5.951872841e-02f,-8.546619117e-02f,-2.855538204e-02f,-1.579977870e-01f,-7.828585058e-02f,-1.033508033e-01f,-1.123882160e-01f,3.652181029e-01f,8.932314813e-02f,-4.365741462e-02f,-9.666188806e-02f,-2.237763107e-01f,-5.591099337e-02f,
2.088513598e-02f,6.352746487e-02f,-2.803783715e-01f,-4.368757606e-01f,2.412237674e-01f,-7.629228383e-02f,7.848702371e-02f,1.896191686e-01f,7.612800598e-02f,-2.076312713e-02f,-7.603692263e-02f,-5.695572495e-01f,-8.661089540e-01f,4.968331158e-01f,2.790066600e-01f,-8.402674645e-02f,
9.070066363e-02f,1.248919889e-01f,1.202327460e-01f,-1.055128217e+00f,-6.793383956e-01f,-6.553862095e-01f,-7.765267491e-01f,-7.900335193e-01f,1.835207790e-01f,2.878481336e-02f,-6.667327136e-02f,-2.136453390e-01f,2.559816241e-01f,-1.856649071e-01f,1.641111076e-01f,5.487041473e-01f,
-1.684028059e-01f,2.505121231e-01f,2.482003160e-02f,2.506425790e-02f,-1.630145758e-01f,-5.077161193e-01f,2.567044497e-01f,-3.281597495e-01f,9.571659565e-02f,2.373243570e-01f,-1.774131954e-01f,3.384631574e-01f,4.871972278e-02f,4.103295505e-03f,-1.000795066e-01f,-1.062083006e+00f,
1.803586930e-01f,-1.079543009e-01f,-1.010130718e-01f,3.587096632e-01f,-4.443259835e-01f,1.988244615e-02f,-1.449323595e-01f,8.682382852e-02f,-5.913271904e-01f,-2.797784507e-01f,7.616812736e-02f,-3.610779047e-01f,2.609952390e-01f,4.557237029e-02f,-1.911970377e-01f,-2.700370252e-01f,
1.608443111e-01f,-1.282665581e-01f,2.489053309e-01f,-3.758280873e-01f,7.067371905e-02f,-8.553952724e-02f,-5.685095116e-02f,-8.608809672e-03f,-4.831112325e-01f,4.710537940e-02f,-7.393770665e-02f,7.407592982e-02f,5.599556118e-02f,-6.338093430e-03f,-2.359002084e-02f,5.316819996e-02f,
-3.244776428e-01f,-2.962044179e-01f,3.767721951e-01f,8.532813936e-02f,2.369619608e-01f,6.261820905e-03f,2.235227227e-01f,2.105524577e-02f,1.990577728e-01f,-3.687089682e-02f,-2.701696455e-01f,-4.706009850e-02f,-2.987785041e-01f,3.354510665e-02f,-8.166942000e-02f,-1.819259524e-01f,
1.683122665e-01f,-1.029748544e-01f,-7.444918156e-02f,-1.144225616e-02f,2.349302471e-01f,-8.795080706e-03f,2.798965201e-03f,2.807618678e-02f,-1.251346059e-02f,3.673184291e-02f,2.806345373e-02f,-1.924271137e-02f,-2.745410241e-02f,-1.761151850e-02f,4.075859761e-05f,-1.021193061e-02f,
5.366870761e-02f,-3.268799558e-02f,-6.472247839e-02f,2.760957181e-02f,4.389689863e-02f,7.346606255e-02f,9.706433117e-02f,-2.990522049e-02f,-3.820285946e-02f,1.210245863e-02f,8.330867440e-02f,7.081895322e-02f,-7.061598450e-02f,-6.754417717e-02f,-7.009033114e-02f,-3.835530207e-02f,
5.790119618e-02f,-2.527141571e-01f,6.588562578e-02f,-1.558768302e-01f,-1.760085225e-01f,-7.390450686e-02f,-2.796286643e-01f,-1.874318719e-01f,-1.722856611e-01f,3.696794435e-02f,-1.097535491e-01f,-1.205313355e-01f,2.431156673e-02f,-9.428859502e-02f,-6.954676658e-02f,-1.387349218e-01f,
-5.779527128e-02f,-3.863387555e-02f,2.262792289e-01f,3.203087300e-02f,-8.762211539e-03f,-1.113257464e-02f,-2.187651396e-02f,-9.279894084e-02f,-9.078019112e-02f,-5.651819706e-02f,-1.662713438e-01f,6.064908579e-02f,-1.574746892e-02f,-1.987457424e-01f,-7.023897022e-02f,-2.271155417e-01f,
-2.270986885e-01f,-1.784216762e-01f,1.446820423e-02f,-1.527606547e-01f,-9.318029135e-02f,3.890763968e-02f,-3.133412078e-02f,-1.347165853e-01f,-3.406948969e-02f,-7.958182693e-02f,3.838224411e-01f,-3.442429006e-01f,-6.785939932e-01f,-2.478656620e-01f,-1.628583819e-01f,7.754462212e-02f,
1.539801806e-01f,1.494764388e-01f,-5.787680671e-02f,-2.969146008e-03f,3.390484452e-01f,1.887875795e-01f,1.211589277e-01f,1.203711182e-01f,4.056357443e-01f,3.373745829e-02f,3.556123748e-02f,2.585245371e-01f,-4.061505198e-02f,2.000488341e-01f,7.878692448e-02f,4.547782242e-01f,
-8.757290430e-03f,-8.002544194e-02f,9.893283993e-02f,1.138915941e-01f,9.586881846e-02f,-1.083493158e-01f,-1.279320717e-01f,-5.258854628e-01f,-9.607605636e-03f,2.970090806e-01f,-8.443201333e-02f,5.165519938e-02f,3.110835678e-06f,-1.098983549e-02f,-1.791283302e-02f,1.969440579e-01f,
-2.232656777e-01f,-1.321382821e-01f,-1.601914130e-02f,1.806382090e-01f,-3.497897685e-01f,-1.860624403e-01f,2.084152959e-02f,2.703641057e-01f,-1.010427028e-01f,-1.302042454e-01f,-3.255825862e-02f,2.940538526e-02f,-1.915211082e-01f,2.951003015e-01f,4.733461514e-02f,5.214355886e-02f,
-2.170002460e-01f,1.454090029e-01f,-2.711153924e-01f,1.703054756e-01f,1.009583287e-02f,2.531730197e-02f,3.053273261e-01f,1.678418517e-01f,-1.641863137e-01f,2.061989903e-02f,-3.419434652e-02f,1.335933059e-02f,-5.125674978e-02f,-5.744402111e-02f,1.252050698e-02f,-1.470182091e-02f,
-1.052572131e-01f,3.277136683e-01f,-2.943828404e-01f,-1.140058860e-01f,-2.575264685e-02f,1.300441623e-01f,1.626977324e-01f,9.451977909e-02f,4.928204417e-02f,4.448765516e-02f,1.582270861e-01f,-3.449456021e-02f,1.352783740e-01f,-1.244781017e-01f,-3.716901839e-01f,-1.937894225e-01f,
4.426079392e-01f,1.272428483e-01f,5.143356323e-02f,-1.420900077e-01f,-1.246492416e-01f,-2.270009741e-02f,6.933373958e-02f,6.124149263e-02f,5.643526837e-02f,-2.730056271e-02f,3.045840189e-02f,2.217573673e-02f,-8.448203094e-03f,4.587791488e-02f,-9.293470532e-02f,4.300200846e-03f,
2.308357507e-02f,-3.061157838e-02f,4.035507515e-02f,3.677416220e-02f,-3.082671203e-02f,6.596884131e-02f,2.596554905e-02f,8.148589730e-02f,-6.573354453e-02f,1.757533997e-01f,1.324963868e-01f,4.143387452e-02f,-8.716072887e-02f,-2.566521764e-01f,2.815169748e-03f,-9.502255917e-02f,
-7.270839065e-03f,-6.368110329e-02f,-3.212821321e-05f,-9.384560585e-02f,-2.338134646e-01f,4.519729614e-01f,-2.744055353e-02f,-8.062457293e-02f,-5.183991417e-02f,3.078633249e-01f,2.781635225e-01f,2.869710922e-01f,-2.053331435e-01f,3.424990475e-01f,8.950656280e-03f,6.796356756e-03f,
1.422025710e-01f,1.775631309e-02f,5.708175898e-01f,-6.953842472e-03f,-1.369485557e-01f,-1.878450960e-01f,-2.649867535e-01f,6.638397276e-02f,-6.138217449e-02f,3.711847588e-03f,8.098238707e-02f,7.029491663e-02f,2.000539601e-01f,1.581937373e-01f,3.619194627e-01f,-1.199561656e-01f,
-8.870617300e-02f,2.674774267e-02f,4.586347342e-01f,2.611585557e-01f,-5.865695700e-02f,-1.850199401e-01f,7.835413516e-02f,-7.047763467e-02f,-1.269477140e-02f,-1.929392666e-01f,-1.793890148e-01f,4.182213247e-01f,-4.353966415e-01f,-4.859838188e-01f,-1.717344671e-02f,-3.166094720e-01f,
9.807991236e-02f,1.753480285e-01f,8.578106761e-02f,-2.777754962e-01f,1.299899518e-01f,-6.977171451e-02f,-9.687827528e-02f,-2.382898778e-01f,1.651203781e-01f,-6.194107607e-02f,-1.971897110e-02f,1.226066351e-01f,1.414388120e-01f,-1.016300246e-01f,1.488879323e-01f,5.628723502e-01f,
-2.051352151e-02f,1.869270205e-01f,1.552296728e-01f,-9.132365882e-02f,-1.047134697e-01f,-1.801103950e-01f,2.096797824e-01f,-2.684884705e-02f,-5.919821933e-02f,1.834327430e-01f,-1.646819115e-01f,9.510169178e-02f,-1.443197206e-02f,-4.761826992e-02f,-4.935931787e-02f,-3.442100883e-01f,
2.098745406e-01f,-1.505269110e-01f,4.486549273e-02f,2.973383665e-01f,-4.193680584e-01f,6.317044608e-03f,-3.313018754e-02f,2.397390455e-01f,-2.007343322e-01f,-2.177654207e-01f,2.138385177e-01f,1.182141677e-01f,-2.204574794e-01f,7.775576087e-04f,1.478691399e-01f,-2.248949260e-01f,
3.698820621e-02f,1.366565526e-01f,1.922729611e-01f,7.997516543e-02f,1.270078570e-01f,1.100130030e-03f,4.113898054e-02f,2.597507089e-02f,-1.540253218e-02f,-6.859351695e-02f,6.067173928e-02f,-5.330395699e-02f,-6.265059114e-03f,-2.576123923e-02f,-3.630799055e-02f,-7.368166745e-02f,
-3.728993982e-02f,4.678317159e-02f,1.994271390e-02f,-1.126421243e-01f,1.954087317e-01f,-1.451213565e-02f,1.152562872e-01f,1.855647713e-01f,7.840697467e-02f,1.092545465e-01f,-9.780356288e-02f,-7.925120741e-02f,-1.565311551e-01f,-1.252325326e-01f,-2.498683035e-01f,-8.806290478e-02f,
1.614524573e-01f,-2.006672323e-01f,1.589270085e-01f,-2.142551839e-01f,8.612311631e-02f,-6.577181816e-02f,1.246718690e-02f,-1.820363663e-02f,5.266059190e-03f,2.108203992e-02f,4.684564099e-02f,-1.008376945e-02f,6.411952525e-02f,2.014023997e-02f,-2.717348561e-02f,-1.044142875e-03f,
-7.228269242e-03f,-6.921889633e-02f,7.276606560e-02f,-3.697187826e-02f,2.963097207e-02f,-5.707398057e-02f,-5.816834792e-02f,-7.711181045e-02f,2.686373703e-02f,-8.374083042e-02f,6.532758474e-03f,-8.355233073e-02f,-5.439643562e-02f,-2.244743519e-02f,-1.181201786e-01f,-1.360456645e-01f,
-1.299303323e-01f,-3.693528473e-02f,2.045845091e-01f,1.641806215e-03f,-1.523312777e-01f,-7.624694705e-02f,2.198296636e-01f,-2.215253562e-02f,-1.171077266e-01f,1.042723656e-01f,-3.374226391e-01f,1.931546628e-01f,-4.411227256e-02f,1.858495623e-01f,-2.630924582e-01f,-3.465552032e-01f,
-2.849270590e-02f,-5.709744245e-02f,1.358376443e-01f,-1.370299701e-02f,-2.482750639e-02f,1.472061276e-01f,1.299997605e-02f,-4.009962454e-02f,-1.341991723e-01f,-1.838860959e-01f,3.478271142e-02f,1.263051331e-01f,8.518578857e-02f,2.081306279e-01f,-3.503269702e-02f,7.581140846e-02f,
4.785145074e-02f,9.264412336e-03f,2.857997119e-01f,-2.935063243e-01f,2.489264868e-02f,5.412990227e-02f,2.393713146e-01f,-3.279599547e-01f,-3.258431852e-01f,-1.203497797e-01f,-5.124491453e-01f,-4.653946161e-01f,-4.250651896e-01f,-6.527296305e-01f,-2.663496733e-01f,-6.740625948e-02f,
2.042015046e-01f,1.357261539e-01f,-9.920568764e-02f,-7.041188329e-02f,2.562648654e-01f,1.103692278e-01f,-6.346280128e-02f,-1.065335497e-01f,1.942713708e-01f,-9.440065175e-02f,1.434147060e-01f,8.290873468e-02f,2.120327950e-01f,4.188393801e-02f,7.133569568e-02f,8.299144357e-02f,
-3.692818880e-01f,-2.876604162e-02f,1.270394772e-01f,-2.809829637e-02f,1.352648586e-01f,9.128732607e-03f,1.966593266e-01f,1.715378836e-02f,3.970594704e-02f,3.030052483e-01f,-5.287166834e-01f,-7.337792777e-04f,6.896287948e-02f,2.825472504e-02f,-7.373242080e-02f,1.162855253e-01f,
1.721141636e-01f,1.061391681e-01f,-1.017444506e-01f,5.857958198e-01f,8.197578788e-02f,3.341645300e-01f,3.566761613e-01f,2.363301516e-01f,4.060607553e-01f,3.243181854e-02f,1.539124697e-01f,-3.346397281e-01f,3.446661308e-02f,3.955738544e-01f,2.455673367e-01f,1.563762082e-03f,
1.100617498e-01f,1.676171124e-01f,6.428276189e-03f,1.659450084e-01f,1.122421622e-01f,-7.550595701e-02f,-4.444387183e-02f,-3.929574415e-02f,7.398136705e-02f,1.380015165e-02f,-5.579821020e-02f,2.236391883e-04f,5.701719970e-02f,-4.637390375e-03f,3.666155413e-02f,1.744209975e-02f,
4.448720813e-02f,-7.587571163e-03f,-7.380810380e-02f,7.384520024e-02f,2.153162509e-01f,-9.404321201e-03f,3.018004596e-01f,-2.099671066e-01f,1.514345556e-01f,-7.558476925e-02f,2.861898020e-02f,5.532771721e-02f,-1.575260013e-01f,-1.898429096e-01f,-2.043096274e-01f,1.360057667e-02f,
-9.529811889e-02f,1.504686475e-01f,-1.653556973e-01f,-7.840512693e-02f,-1.250162572e-01f,1.360847149e-02f,6.902946159e-03f,3.986208141e-02f,4.967266694e-02f,1.361928135e-02f,-2.275299281e-02f,6.467121840e-02f,-2.736148238e-02f,5.232507735e-02f,-1.183815822e-01f,-1.960407384e-02f,
1.259645633e-02f,-2.823172323e-02f,1.404477060e-01f,-9.018645622e-03f,4.700859264e-02f,5.689239874e-02f,7.606781274e-02f,9.444920719e-02f,-2.375054546e-02f,-1.173701286e-01f,-1.228658576e-02f,3.021428362e-02f,-1.973885857e-02f,4.483845457e-02f,8.903512359e-02f,1.348766834e-01f,
1.464412455e-02f,-1.713471487e-02f,-2.101123780e-01f,-1.200237125e-01f,-1.135466471e-01f,1.128852367e-01f,-3.414959088e-02f,-1.553921103e-01f,1.017685700e-02f,2.869795263e-01f,-1.780897826e-01f,2.995810211e-01f,-7.320095785e-03f,-1.071840003e-01f,6.652693450e-02f,1.041058153e-01f,
4.138052836e-02f,-1.719577983e-02f,1.362946182e-01f,2.417726628e-02f,-1.772621870e-01f,4.490778595e-02f,5.318452045e-02f,-3.055738285e-02f,1.207085401e-01f,8.128777146e-02f,2.159155756e-01f,-8.901920915e-02f,1.104055047e-01f,3.764271438e-01f,7.825881243e-02f,-2.686262727e-01f,
-2.116041034e-01f,4.425527528e-02f,1.082588658e-01f,-2.790440917e-01f,1.800540090e-02f,-1.571341902e-01f,2.966450453e-01f,-3.429895267e-02f,1.017643064e-01f,8.033650368e-02f,1.173177510e-01f,-6.490595937e-01f,-8.305572718e-02f,9.265991300e-02f,-2.135194242e-01f,4.666053504e-02f,
1.033517346e-01f,1.035233364e-01f,2.770329118e-01f,-6.073680148e-02f,-1.002498269e-01f,2.485567778e-01f,-1.273877919e-02f,-1.235628501e-01f,2.254616618e-01f,-5.525467545e-02f,-3.471017256e-02f,1.944556385e-01f,-1.394011825e-02f,1.748802960e-01f,8.732697368e-02f,3.228370845e-01f,
-2.122850716e-01f,-1.674905121e-01f,2.237630785e-01f,-1.778392307e-02f,1.528827194e-02f,2.535379827e-01f,-7.755766064e-02f,-1.504534483e-02f,-8.010213077e-02f,3.890347481e-01f,1.365828067e-01f,-2.626214623e-01f,8.828070015e-02f,4.482501373e-02f,-1.029429957e-02f,1.830211282e-01f,
1.690932214e-01f,-2.575714886e-01f,7.049418241e-02f,1.463197470e-01f,-2.666433752e-01f,2.067427188e-01f,2.385646105e-01f,8.345345408e-02f,1.463627964e-01f,-1.637980342e-01f,-4.359606653e-02f,-2.825897634e-01f,-8.824727684e-02f,7.867351174e-02f,6.955701858e-02f,-9.448017925e-02f,
-2.805854194e-02f,7.512449473e-02f,-5.607001111e-02f,-9.682684392e-02f,8.456412470e-04f,-2.020851336e-02f,-3.055195808e-01f,-5.950803682e-02f,4.087795913e-01f,-3.425335884e-02f,7.482349128e-02f,1.101338863e-02f,-4.756932333e-02f,-5.370469391e-02f,3.553277999e-02f,6.419844180e-02f,
1.309549212e-01f,7.163399458e-01f,-1.034462214e+00f,2.532822266e-02f,-3.963627666e-02f,4.155806825e-02f,-2.259209901e-01f,-4.869763926e-02f,2.029843330e-01f,-3.996831179e-02f,-6.415908039e-02f,5.842981115e-02f,-7.382267714e-02f,4.642635584e-02f,-2.769415677e-01f,-1.041922271e-01f,
-4.899946973e-02f,-2.622730434e-01f,2.635113150e-02f,-7.198742032e-02f,-3.756535053e-02f,1.035812274e-01f,4.300399125e-02f,5.994063616e-02f,2.685279585e-03f,3.252242133e-02f,-2.243128605e-02f,1.582135260e-02f,1.636030339e-02f,9.048201889e-02f,-8.906220645e-02f,-6.865938753e-02f,
-4.520225152e-02f,8.340737224e-02f,5.650859326e-02f,-2.285044082e-02f,5.290052295e-02f,5.012420565e-02f,-2.742434479e-02f,-8.330130018e-03f,-6.636111531e-03f,-4.592450336e-02f,-7.956033945e-02f,-8.682879852e-04f,1.425234508e-02f,-9.178933688e-03f,-6.197239738e-03f,-1.354049891e-02f,
-1.251632273e-01f,-1.122839004e-02f,-1.826500446e-01f,-5.645021424e-02f,-8.030740917e-02f,-8.881209046e-02f,7.651046664e-02f,-8.127780259e-02f,-9.111053497e-02f,-1.305180341e-01f,1.547742188e-01f,6.399657577e-02f,-6.263819337e-02f,-1.190945357e-01f,2.301676720e-01f,-2.452567071e-01f,
4.714851454e-02f,-4.070857167e-02f,2.213457227e-01f,6.514479220e-02f,-7.764236629e-02f,7.457079832e-03f,-4.525974020e-02f,-9.271277487e-02f,-5.245550722e-02f,-3.391085938e-02f,1.735841930e-01f,-1.157121286e-01f,2.582978904e-01f,4.339030683e-01f,-1.203001589e-01f,1.433212906e-01f,
-1.412716359e-01f,-7.523791865e-03f,-4.381884634e-01f,2.247186899e-01f,6.195834838e-03f,-4.808419943e-02f,-1.935764104e-01f,3.443877101e-01f,-3.462195396e-01f,-1.469762716e-02f,-3.105348051e-01f,6.438123435e-02f,-5.058988929e-01f,-3.986601532e-01f,-3.633770347e-02f,-1.383317113e-01f
};
static const float ACT_B0[32]={
5.689420272e-03f,-7.063231431e-03f,-6.320971251e-02f,-5.856528506e-02f,-2.654449269e-02f,-8.212426305e-02f,-7.640134543e-02f,-1.428439282e-02f,-5.664292723e-02f,-6.031795219e-02f,-7.895851135e-02f,1.010265015e-02f,2.228124999e-02f,-3.763570264e-02f,-1.988311857e-02f,1.005019899e-02f,
4.118179530e-02f,4.667007923e-02f,-7.918709889e-03f,7.199759036e-02f,-6.123682484e-02f,4.840341210e-02f,3.127107024e-02f,-5.200597271e-02f,4.678943381e-02f,5.808205903e-02f,7.404925674e-02f,-9.694294631e-02f,-6.101275980e-02f,-4.835948721e-02f,1.910108724e-03f,3.482324630e-02f
};
static const float ACT_W1[800]={
-1.834719558e-03f,-2.211086303e-01f,1.956865489e-01f,-1.855050400e-02f,2.141074240e-01f,-1.566105038e-01f,-7.159446925e-02f,-8.797351271e-02f,-2.788044512e-01f,-6.875482947e-02f,-5.090695620e-02f,-3.665134683e-02f,-1.202554554e-01f,3.123067133e-02f,-2.299859673e-01f,-5.589817464e-02f,
-4.409340397e-02f,4.405548796e-02f,-9.472469799e-03f,0.000000000e+00f,1.222486049e-01f,1.212785244e-01f,0.000000000e+00f,-4.474281892e-02f,-1.343016773e-01f,1.877494901e-01f,8.069596440e-02f,-2.227484733e-01f,-1.436571032e-01f,-1.101801693e-01f,-2.842378616e-01f,-1.521212012e-01f,
5.850318074e-02f,2.910067514e-02f,1.565370113e-01f,-7.715113461e-02f,-3.528463468e-02f,1.152850017e-01f,-6.748779416e-01f,4.206532538e-01f,6.170763448e-02f,1.136253849e-01f,1.992842108e-01f,7.577681541e-02f,2.481617779e-01f,2.724334002e-01f,6.228435040e-02f,1.289832294e-01f,
1.621164382e-01f,-1.964344680e-01f,0.000000000e+00f,0.000000000e+00f,1.046909466e-01f,1.219267547e-01f,0.000000000e+00f,-1.728486419e-01f,-4.905338958e-02f,-9.471892565e-02f,-8.973392099e-02f,4.273277223e-01f,1.445443034e-01f,4.816250131e-02f,-1.444818079e-01f,2.407577783e-01f,
2.877329886e-01f,5.326404050e-02f,2.048907429e-02f,2.962235212e-01f,1.161492523e-02f,-1.143535674e-01f,2.116909437e-02f,-3.966390491e-01f,-3.599210083e-01f,-1.355919540e-01f,-5.670035724e-03f,3.584366292e-02f,-1.028756574e-01f,-4.132421613e-01f,-2.980946898e-01f,-8.809492923e-03f,
-2.260115743e-02f,1.812531501e-01f,-9.616500698e-03f,0.000000000e+00f,-1.242373511e-01f,-1.151605546e-01f,0.000000000e+00f,-1.301680058e-01f,8.952429891e-02f,-3.515733182e-01f,3.532122821e-02f,-3.058941960e-01f,7.833619416e-02f,-2.807743549e-01f,9.838075191e-02f,-5.856832862e-01f,
-2.059783340e-01f,6.565459073e-02f,3.132616356e-02f,1.778070331e-01f,-5.695999414e-02f,-1.412624270e-01f,2.306164503e-01f,-1.156077683e-01f,9.543564171e-02f,-9.411814064e-02f,-1.567157209e-01f,-2.440207601e-01f,1.812582761e-01f,-2.209079564e-01f,1.685751379e-01f,-1.923856586e-01f,
-4.196423665e-02f,1.066384315e-01f,-6.949842907e-03f,0.000000000e+00f,-4.941677302e-02f,-6.591387838e-02f,0.000000000e+00f,1.327468455e-01f,1.318727136e-01f,-9.603045881e-02f,-9.100690484e-02f,-5.958342925e-03f,-9.076438844e-02f,1.551195830e-01f,-2.375198603e-01f,1.015112642e-02f,
-9.950844198e-02f,1.196741238e-01f,-1.002293378e-01f,-9.112734348e-02f,-1.036674436e-02f,2.220214307e-01f,2.895984054e-02f,-3.289251328e-01f,2.646990120e-01f,-1.144342348e-01f,-3.040655516e-02f,1.996774077e-01f,2.282640189e-01f,-9.848281741e-02f,-4.386459291e-02f,5.039602146e-02f,
-3.528804779e-01f,1.920548454e-02f,7.318775752e-04f,0.000000000e+00f,-5.635607243e-02f,2.344946563e-02f,0.000000000e+00f,9.010224044e-02f,-1.238747835e-01f,6.046204269e-02f,-2.190355770e-02f,-4.350484163e-02f,-1.990216970e-01f,-1.167627424e-01f,2.111273631e-02f,3.368715197e-02f,
-6.034316495e-02f,9.089595079e-02f,-1.163417175e-01f,4.656720906e-02f,-1.907597333e-01f,1.575459167e-02f,2.819105387e-01f,4.092595577e-01f,-1.653011143e-02f,7.871217281e-02f,-2.384993583e-01f,3.597970679e-02f,-3.148884475e-01f,7.220207155e-02f,1.512884349e-01f,-9.066389501e-02f,
1.836914867e-01f,-7.260923833e-02f,2.226765268e-03f,0.000000000e+00f,1.127708703e-01f,7.722464949e-02f,0.000000000e+00f,4.145236686e-02f,3.556992859e-02f,7.598363608e-02f,7.052356750e-02f,-1.473895609e-01f,-1.246177033e-01f,6.004629191e-03f,2.657599151e-01f,3.238077834e-02f,
-4.819393903e-02f,-8.396080136e-02f,-1.449923497e-02f,1.789071113e-01f,-7.127345353e-02f,-5.806613341e-02f,-2.430398017e-01f,-2.015669048e-01f,4.315872211e-03f,6.484050304e-02f,5.062965676e-03f,-2.064631581e-01f,-3.612723947e-01f,-1.002482250e-01f,-1.119072661e-01f,-2.066222876e-01f,
-5.216570497e-01f,1.525712311e-01f,-8.198251948e-03f,0.000000000e+00f,-1.743067205e-01f,6.383146625e-03f,0.000000000e+00f,-9.519945085e-02f,1.273490209e-02f,2.791381478e-01f,1.395488530e-02f,-1.758173406e-01f,1.737601459e-01f,-9.529106319e-03f,2.478217930e-01f,-1.089374274e-01f,
-3.968356848e-01f,-1.824668348e-01f,-9.056673199e-02f,2.061822265e-01f,1.663222313e-01f,-3.054862320e-01f,1.441635042e-01f,-4.146553203e-02f,-2.472672164e-01f,1.317551583e-01f,3.629387915e-02f,3.150300682e-01f,-1.401876509e-01f,-5.371062458e-02f,6.622212008e-03f,3.222588450e-02f,
2.076199651e-01f,5.252180994e-02f,2.062941901e-03f,0.000000000e+00f,3.090480864e-01f,-2.695568465e-02f,0.000000000e+00f,-1.535456721e-02f,1.091162786e-01f,9.700114280e-02f,4.485296085e-03f,-9.046123922e-02f,-4.167219400e-01f,-8.383216709e-02f,-3.194880486e-02f,-4.807564989e-02f,
-1.909113526e-01f,-1.445186883e-01f,1.499493569e-01f,2.628579037e-03f,-2.727603018e-01f,1.065520346e-01f,2.200996131e-01f,-9.113611281e-02f,-5.293824524e-02f,3.023093380e-02f,1.194506586e-01f,-6.613155454e-02f,-2.966820598e-01f,-7.374521345e-02f,-1.300649494e-01f,-1.224524900e-01f,
-1.226302162e-01f,7.274352759e-02f,0.000000000e+00f,0.000000000e+00f,-3.177653998e-02f,2.207307070e-01f,0.000000000e+00f,2.462792993e-01f,-1.263841987e-01f,-1.295319051e-01f,3.804273307e-01f,-8.867656440e-02f,-2.279052138e-01f,1.212333888e-01f,1.118819714e-01f,7.033854723e-02f,
2.549452484e-01f,2.014796436e-01f,1.312964410e-01f,-1.374987513e-01f,2.039521933e-01f,1.736284941e-01f,-5.304527283e-02f,1.501204222e-01f,-2.078885883e-01f,1.147417128e-01f,2.116378099e-01f,-4.042727873e-02f,6.359814852e-02f,2.723718584e-01f,1.362558305e-01f,2.183414698e-01f,
2.454314381e-02f,-1.509473380e-02f,0.000000000e+00f,0.000000000e+00f,3.833710216e-03f,-1.115604341e-01f,0.000000000e+00f,1.987679750e-01f,5.315556377e-02f,6.721673906e-02f,-8.365318179e-02f,5.821297318e-02f,2.710406184e-01f,1.212654561e-01f,-4.249895457e-03f,1.128434911e-01f,
-2.424823865e-02f,-3.119183145e-02f,1.054672524e-01f,1.223665923e-01f,1.085641608e-02f,2.154789269e-01f,-5.583854765e-02f,-1.671562493e-01f,-2.769279294e-03f,-1.682302058e-01f,6.467810273e-02f,-1.061624102e-02f,8.701637387e-03f,3.436460197e-01f,2.835062891e-02f,4.355861247e-01f,
-1.104012430e-01f,-3.125953674e-02f,0.000000000e+00f,0.000000000e+00f,2.826182246e-01f,-1.622911841e-01f,0.000000000e+00f,-1.299807280e-01f,-4.918728396e-02f,4.277534783e-02f,3.061800078e-02f,1.450219750e-02f,-5.387874693e-02f,-4.332093522e-02f,-1.585204750e-01f,-3.246714473e-01f,
1.095559597e-01f,9.389746934e-02f,-1.167158503e-02f,-1.456002891e-01f,1.904801130e-01f,8.565155417e-02f,8.284533396e-03f,-9.489266574e-02f,-3.085940331e-02f,2.250463963e-01f,-1.052782163e-01f,-1.108524725e-01f,2.814762592e-01f,-1.752346009e-01f,5.874315649e-02f,2.490933239e-01f,
1.335243583e-01f,2.003775723e-02f,0.000000000e+00f,0.000000000e+00f,-1.468156129e-01f,-3.668725025e-03f,0.000000000e+00f,5.579481274e-02f,2.879154682e-01f,5.091680586e-02f,-1.850081384e-01f,9.724283218e-02f,1.003220119e-02f,1.573335826e-01f,6.192130223e-02f,2.729179263e-01f,
2.750524692e-02f,2.059174776e-01f,-6.028645486e-02f,-1.717249155e-01f,-8.241706342e-02f,3.049474359e-01f,2.546039969e-02f,1.436930448e-01f,-1.005544141e-01f,-2.594111860e-02f,-6.238545850e-02f,7.023687661e-02f,-4.633024335e-02f,2.760744691e-01f,-1.289628148e-01f,-5.819948018e-02f,
2.522938885e-02f,7.116489857e-02f,0.000000000e+00f,0.000000000e+00f,-7.239385694e-02f,8.670806140e-02f,0.000000000e+00f,-6.534688920e-02f,2.139427215e-01f,-5.280923098e-02f,-2.756041847e-02f,1.850042492e-02f,7.250611484e-02f,2.222486883e-01f,-7.603767328e-03f,1.310158968e-01f,
-4.271024168e-01f,2.328730673e-01f,1.931253970e-01f,7.814439386e-02f,2.749634981e-01f,4.196532443e-02f,5.139133707e-02f,-2.368191071e-02f,6.480278075e-02f,-4.733480141e-02f,5.242778733e-02f,1.892562807e-01f,-3.878963366e-02f,-5.974461138e-02f,2.470414788e-01f,-4.476460218e-01f,
-2.082476914e-01f,-6.092038006e-03f,0.000000000e+00f,0.000000000e+00f,-5.643463507e-02f,3.723097444e-01f,0.000000000e+00f,1.443613917e-01f,1.114344448e-01f,6.813838333e-02f,1.263522506e-01f,-1.351363212e-02f,-2.753612101e-01f,-2.522028685e-01f,-2.950329781e-01f,1.992667913e-01f,
-2.954268083e-02f,9.800753742e-02f,-9.353403747e-02f,1.228379160e-01f,-1.230591610e-01f,-2.941529453e-01f,-8.564017713e-02f,-2.840460464e-02f,-2.279887944e-01f,-2.849460244e-01f,-9.929136187e-02f,1.161788404e-01f,-4.207296669e-02f,-3.659660816e-01f,4.016000405e-02f,-1.709455252e-01f,
-3.063704073e-02f,-5.706902966e-02f,-9.244963527e-03f,0.000000000e+00f,-8.178125322e-02f,-1.713445783e-01f,0.000000000e+00f,-3.505913615e-01f,-3.468808234e-01f,2.360349447e-01f,1.130270660e-01f,-6.455959380e-02f,5.107370019e-02f,-5.067526698e-01f,9.856859595e-02f,1.197901070e-01f,
6.008942425e-02f,2.016398311e-01f,4.165890068e-02f,1.521113887e-02f,-2.483685762e-01f,-3.202169240e-01f,9.182507545e-02f,-4.977494478e-02f,-1.159991995e-01f,3.290989250e-02f,-2.153536081e-01f,-1.731949747e-01f,-1.299813855e-02f,-3.437954187e-01f,-1.509660482e-02f,1.789536774e-01f,
1.843785346e-01f,-1.546164230e-02f,-9.131499566e-03f,0.000000000e+00f,-7.724925876e-02f,-1.204300895e-01f,0.000000000e+00f,-3.230645359e-01f,-1.981970295e-02f,2.596959472e-02f,-6.467279792e-02f,-2.754966728e-02f,-1.960332841e-01f,1.041948274e-01f,1.225608066e-01f,-8.239641041e-02f,
9.164483845e-02f,1.937185377e-01f,-7.848481089e-02f,-4.730282724e-02f,-3.073500991e-01f,7.242571563e-02f,9.138454497e-02f,5.739402771e-02f,-1.198763996e-01f,1.638622954e-02f,7.765685767e-02f,-1.484319866e-01f,-9.628501534e-02f,-2.366661280e-01f,4.234994575e-02f,3.093629777e-01f,
3.893993795e-02f,-1.136294603e-01f,-9.081326425e-03f,0.000000000e+00f,-1.430048347e-01f,-1.415797770e-01f,0.000000000e+00f,-1.023306251e-01f,-3.088021278e-01f,2.490083314e-02f,5.981159210e-02f,-4.468824714e-02f,6.304675341e-02f,-4.083241895e-02f,3.096404672e-02f,1.124716713e-03f,
1.621191651e-01f,1.581183374e-01f,-1.525680572e-01f,1.825896464e-02f,-1.568432152e-01f,2.508760393e-01f,1.025664955e-01f,6.750724465e-02f,-1.029263996e-02f,-2.758878842e-02f,7.340340316e-02f,-8.271824569e-02f,-5.926733464e-02f,-2.858830430e-02f,-1.106117591e-01f,-9.664992988e-02f,
1.490964144e-01f,-1.276044995e-01f,5.951750558e-03f,0.000000000e+00f,-1.571771502e-02f,-5.563945323e-02f,0.000000000e+00f,7.547427714e-02f,-9.338759631e-02f,-1.431519445e-02f,1.274657696e-01f,6.061942782e-03f,3.246590793e-01f,1.274398565e-01f,2.189252675e-01f,-8.229469508e-02f,
-1.764146686e-01f,-6.076245103e-03f,-9.599950281e-04f,-3.445752664e-04f,2.809411846e-02f,-2.979312837e-01f,-3.154025972e-02f,1.261981428e-01f,4.027213156e-02f,-5.415433273e-02f,1.198744774e-01f,8.766566962e-02f,-7.024065405e-02f,-9.553747624e-02f,3.776001185e-02f,-1.837213784e-01f,
-2.072564662e-01f,5.170615017e-02f,0.000000000e+00f,0.000000000e+00f,-2.281697243e-01f,-2.246141732e-01f,0.000000000e+00f,-1.765653864e-02f,-5.970989540e-02f,-1.522343904e-01f,9.818988293e-02f,-3.032648005e-02f,-1.148523241e-01f,-4.836224020e-02f,4.503060877e-02f,7.989117503e-02f,
1.017545015e-01f,3.264593659e-03f,-8.666357398e-02f,5.521924049e-02f,1.620071083e-01f,-3.796984553e-01f,8.959004283e-02f,8.696485311e-02f,3.301768303e-01f,-1.492592841e-01f,-8.733181655e-02f,-1.968821734e-01f,5.570390448e-02f,1.204339713e-01f,2.034326047e-01f,2.233511209e-01f,
1.188522801e-01f,-6.853075325e-02f,5.957760382e-03f,0.000000000e+00f,-1.906253956e-02f,-1.435809433e-01f,0.000000000e+00f,2.305682451e-01f,1.125876456e-01f,-8.500739187e-02f,-1.025448367e-02f,-7.249406073e-03f,-2.985766344e-02f,-1.229559630e-02f,8.925068378e-02f,5.721035972e-02f,
-3.775919601e-02f,-3.497025669e-01f,-2.329874188e-01f,1.352432966e-01f,1.963877529e-01f,-1.328186542e-01f,6.980910152e-02f,8.132144809e-02f,1.424921155e-01f,2.913000062e-04f,-1.456086338e-01f,-1.033496261e-01f,-1.959093660e-01f,1.570116431e-01f,2.788230181e-01f,1.695926189e-01f,
1.787300706e-01f,-9.122822434e-02f,2.554111416e-03f,0.000000000e+00f,1.570037305e-01f,9.171249717e-02f,0.000000000e+00f,-3.185325563e-01f,4.907478392e-02f,-1.497546285e-01f,2.645372041e-02f,7.454070263e-03f,1.680615991e-01f,-1.423635185e-01f,-2.724873126e-01f,-3.647989929e-01f,
2.804965377e-01f,-1.703370810e-01f,-3.293019235e-01f,6.954110414e-02f,5.362407863e-02f,-1.897966713e-01f,-8.152573556e-02f,-9.043838829e-02f,7.312837243e-02f,3.400189430e-02f,-6.982301362e-03f,1.935047954e-01f,-7.975517958e-02f,-3.863754869e-02f,5.883505102e-03f,-3.721760511e-01f,
4.850117024e-03f,2.243033424e-02f,-7.545025088e-03f,0.000000000e+00f,-1.064817011e-01f,-9.957193583e-02f,0.000000000e+00f,3.395720124e-01f,1.257035583e-01f,8.349115960e-03f,3.276163712e-02f,-2.043312415e-03f,-4.500172660e-02f,2.519699335e-01f,2.066282369e-02f,-5.369637511e-04f,
-1.239794120e-01f,-2.787458599e-01f,-8.142802864e-02f,1.058828533e-01f,-8.250866830e-02f,2.408721447e-01f,-8.844335377e-02f,1.354440153e-01f,-1.341382712e-01f,-1.003023162e-01f,-1.858406812e-01f,2.637828328e-02f,4.902459309e-02f,2.999689877e-01f,-4.040350392e-02f,-2.257109135e-01f,
-1.464850735e-02f,3.156038746e-02f,0.000000000e+00f,0.000000000e+00f,-6.764708087e-03f,1.063256785e-01f,0.000000000e+00f,-2.418059856e-01f,-5.639119074e-02f,-4.388810322e-02f,-9.925537556e-02f,-3.668326512e-02f,-9.178858995e-02f,2.917897142e-02f,4.303780571e-02f,-4.938180372e-02f,
-4.489675537e-02f,-5.083864927e-01f,-1.423356384e-01f,1.131982058e-01f,1.477143317e-01f,-1.943982840e-01f,1.974788755e-01f,1.444884539e-01f,1.990564466e-01f,-2.965508029e-02f,-2.239244990e-02f,-1.174751110e-02f,-7.525704801e-02f,-1.548907310e-01f,1.363112181e-01f,-5.466719717e-02f,
4.292995855e-02f,-2.552202158e-02f,5.457427353e-03f,0.000000000e+00f,3.108515441e-01f,1.793214828e-01f,0.000000000e+00f,-4.055352807e-01f,-8.358093351e-02f,-1.557540894e-01f,5.371885374e-02f,8.227666840e-03f,1.290601790e-01f,1.159994900e-01f,-2.209078148e-02f,-3.000478148e-01f,
2.959814295e-02f,-1.229843572e-01f,-2.476615906e-01f,-7.541169226e-02f,8.388774097e-02f,3.476392031e-01f,8.316875994e-02f,-2.000334114e-01f,2.299914658e-01f,1.933368295e-01f,1.466186047e-01f,2.721467614e-01f,7.815351337e-02f,-5.719135329e-02f,7.599277049e-02f,-1.706264764e-01f,
9.798529744e-02f,-5.445250310e-03f,4.214026034e-03f,0.000000000e+00f,-1.090439409e-02f,2.304868214e-02f,0.000000000e+00f,4.268047214e-02f,2.638862841e-02f,-1.165016461e-02f,3.137420630e-03f,9.300718457e-02f,2.770074606e-01f,8.274360746e-02f,-1.005903725e-02f,2.290787920e-02f
};
static const float ACT_B1[25]={
1.082007866e-02f,2.618396655e-02f,1.089293808e-01f,-2.808065852e-03f,1.224869676e-02f,-5.753319710e-03f,7.514671236e-02f,8.020929992e-03f,1.066635270e-02f,-8.892505616e-02f,1.137159765e-01f,-1.506064981e-01f,-1.127332523e-01f,4.885075614e-02f,9.749197215e-02f,5.642537773e-02f,
1.001396626e-01f,-4.330067709e-02f,-4.960871767e-03f,-5.553621054e-02f,1.218558028e-01f,-8.603667468e-02f,3.019297682e-02f,1.533770561e-01f,-4.546183720e-02f
};

static void actor_forward(const float*x,float*out){
 float h[32];
 for(int j=0;j<32;j++){float z=ACT_B0[j];for(int k=0;k<160;k++)z+=ACT_W0[j*160+k]*x[k];h[j]=std::max(0.f,z);}
 for(int a=0;a<25;a++){float z=ACT_B1[a];for(int j=0;j<32;j++)z+=ACT_W1[a*32+j]*h[j];out[a]=std::max(-6.f,std::min(6.f,z));}
}

static volatile float actor_cost_sink=0;
static void actor_root(const State&s,const int*root){
 prepare_scan(s);
 for(int i=0;i<6;i++)if(s.tank[i].alive){
  Node&nd=pool[root[i]];if(!nd.n_acts)build_actions(nd,s,i);
  float x[160],delta[25],p[25],sum=0;
  actor_features(s,i,nd,x);actor_forward(x,delta);
  float checksum=0;for(float z:delta)checksum+=z;actor_cost_sink=checksum;
  for(int a=0;a<nd.n_acts;a++){p[a]=nd.p[a]*std::exp(delta[canonical_action(nd.act[a],i/3)]);sum+=p[a];}
  for(int a=0;a<nd.n_acts;a++)nd.p[a]=(1-ACTOR_BLEND)*nd.p[a]+ACTOR_BLEND*p[a]/sum;
  // Root nodes have not been visited. Keep best prior at index zero once,
  // preserving the original O(1) first-visit choice everywhere in the tree.
  for(int a=1;a<nd.n_acts;a++)for(int b=a;b>0&&nd.p[b]>nd.p[b-1];b--){
   std::swap(nd.p[b],nd.p[b-1]);std::swap(nd.act[b],nd.act[b-1]);
  }
 }
}

static inline int select_action(const Node& nd) {
    // PUCT：估值 + 探索项。探索项按先验概率分配，所以先验高的命令先被试、试得多；
    // 没试过的命令估值取"节点均值减一点"，不会六辆车同时乱试。
    if (nd.visits == 0) return 0;                       // 第一次来：走先验最高的
    const float sq = std::sqrt(float(nd.visits)) * (SP_UCB / 100.0f);
    const float fpu = nd.total / float(nd.visits) - SP_FPU / 100.0f;
    int best = 0;
    float best_u = -1e9f;
    for (int a = 0; a < nd.n_acts; ++a) {
        const float q = nd.n[a] ? nd.sum[a] / float(nd.n[a]) : fpu;
        const float u = q + sq * nd.p[a] / float(1 + nd.n[a]);
        if (u > best_u) { best_u = u; best = a; }
    }
    return best;
}


static const int HTSIZE=32768;
struct StateSlot { uint64_t hash; int first; };
static StateSlot state_table[HTSIZE];
static uint64_t mix64(uint64_t x) {x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;return x^(x>>31);}


static uint64_t key_seeds[SP_DEPTH+1][1<<MAX_SUPPLY][6],key_current[6];
static int key_origin_turn;
static void make_key_seeds(int turn){
 key_origin_turn=turn;
 for(int d=0;d<=SP_DEPTH;d++)for(int mask=0;mask<(1<<MAX_SUPPLY);mask++)for(int i=0;i<6;i++)
  key_seeds[d][mask][i]=mix64(uint64_t(turn+d)|uint64_t(i+1)<<16|uint64_t(mask)<<24);
}
static void prepare_keys(const State&s){
 uint64_t tk[6];int x[6],y[6];
 for(int i=0;i<6;i++){
  const Tank&a=s.tank[i];int pos=a.alive?a.pos:s.spawn[i];x[i]=cell_x(pos);y[i]=cell_y(pos);
  uint64_t v=uint64_t(a.pos+1)|uint64_t(a.alive)<<10|uint64_t(a.hp+4)<<11|uint64_t(a.level)<<16|uint64_t(reload_left(s,i))<<20|uint64_t(respawn_in(s,i))<<28;
  tk[i]=mix64(v+uint64_t(i+1)*0x9e3779b97f4a7c15ULL);
  key_current[i]=key_seeds[s.turn-key_origin_turn][s.star_mask][i]^s.wall_hash^tk[i];
 }
 for(int i=0;i<6;i++)for(int j=i+1;j<6;j++)if(std::abs(x[i]-x[j])+std::abs(y[i]-y[j])<=LOCAL_RANGE){
  if(s.tank[j].alive)key_current[i]^=tk[j];
  if(s.tank[i].alive)key_current[j]^=tk[i];
 }
 for(int b=0;b<s.n_bullets;b++){
  const Bullet&a=s.bullet[b];int bx=cell_x(a.pos),by=cell_y(a.pos);
  uint64_t k=mix64(uint64_t(a.pos+1)|uint64_t(a.dir)<<10|uint64_t(a.owner+1)<<14|uint64_t(a.steel)<<18);
  for(int i=0;i<6;i++)if(a.owner==i||std::abs(x[i]-bx)+std::abs(y[i]-by)<=LOCAL_RANGE+2)key_current[i]^=k;
 }
}
static uint64_t state_key(const State&,int unit){return key_current[unit]?key_current[unit]:1;}
static int lookup_state(const State&s,int unit,bool create){
 uint64_t h=state_key(s,unit);unsigned at=unsigned(h)&(HTSIZE-1);
 for(int k=0;k<16;k++,at=(at+1)&(HTSIZE-1)){
  StateSlot&t=state_table[at];if(t.hash==h)return t.first;
  if(!t.hash){if(!create||pool_used>=POOL_CAPACITY)return -1;t.hash=h;t.first=new_node();return t.first;}
 }
 return -1;
}

static double init_cpu_ms=0,call_cpu_start=0,call_init_ms=0,call_cpu_limit=38;
static bool first_decision=true;
void init(const State& s, int me) {
    const double init_start=tank::budget_used();first_decision=true;
    if(!pool)pool.reset(new Node[POOL_CAPACITY]);
    for(int d=0;d<=2048;d++)prior_exp[d]=std::exp(float(-d)/float(SP_PRIOR_T));
    (void)me;
    M.valid = false;
    total_steps = 0;
    for (int t = 0; t < 3; ++t) prev_target[t] = -1;
    for (int j = 0; j < 6; ++j) { still[j] = 0; still_pos[j] = -1; }
    stale_turns = 0;
    stale_hp[0] = stale_hp[1] = stale_kills[0] = stale_kills[1] = -1;
    stale_walls = 0;

    build_maps(s); init_opening(s);
    init_cpu_ms=tank::budget_used()-init_start;
}

#if SP_METHOD == 1
// 对照用的另一种搜索：每辆车把自己的每条候选命令前推一回合（其余五辆用当前选定的命令），
// 之后所有车按树外的规则策略续推到 SP_DEPTH 回合，沿途评估取平均；六辆车轮流这样取最佳应对，做 SP_IBR_ROUNDS 轮。
// 这是"规则型 + 一回合前推 + 续推"的常规写法，预算用不满（每轮约 6 辆 × 十几条命令 × SP_DEPTH 次前推）。
static Orders decide_ibr(const State& s, int me) {
    static Node nd[6];
    Cmd choice[6];
    for (int i = 0; i < 6; ++i) {
        choice[i] = STAY;
        nd[i].n_acts = 0;
        if (!s.tank[i].alive) continue;
        build_actions(nd[i], s, i);
        choice[i] = nd[i].act[0];
    }
    const uint64_t seed = rng_state;
    for (int round = 0; round < SP_IBR_ROUNDS; ++round) {
        bool changed = false;
        for (int k = 0; k < 6; ++k) {
            const int i = (round & 1) ? 5 - k : k;
            if (nd[i].n_acts <= 1) continue;
            int best = -1;
            long long best_score = 0;
            for (int a = 0; a < nd[i].n_acts; ++a) {
                Orders o[2];
                for (int j = 0; j < 6; ++j) o[j / 3].cmd[j % 3] = j == i ? nd[i].act[a] : choice[j];
                State st = s;
                step(st, o[0], o[1]);
                long long sum = evaluate(st);
                rng_state = seed;                                  // 每条候选用同一串随机数，比较才公平
                for (int d = 1; d < SP_DEPTH; ++d) {
                    if (st.done) { sum += evaluate(st); continue; }
                    Orders q[2];
                    for (int j = 0; j < 6; ++j)
                        if (st.tank[j].alive) q[j / 3].cmd[j % 3] = rollout_cmd(st, j);
                    step(st, q[0], q[1]);
                    sum += evaluate(st);
                }
                if (i >= 3) sum = -sum;
                if (best < 0 || sum > best_score) { best_score = sum; best = a; }
            }
            if (nd[i].act[best] != choice[i]) { choice[i] = nd[i].act[best]; changed = true; }
        }
        if (!changed) break;
    }
    Orders out;
    for (int type = 0; type < 3; ++type) out.cmd[type] = choice[tank_id(me, type)];
    return out;
}
#endif

Orders decide(const State& s, int me) {
    call_cpu_start=tank::budget_used();call_init_ms=first_decision?init_cpu_ms:0;call_cpu_limit=first_decision?24:38;first_decision=false;
    build_maps(s);
    build_tank_maps(s);
    assign_roles(s, me);
    for(int i=0;i<6;i++)opening_active[i]=s.turn<11||(s.turn<17&&s.tank[i].level==0&&(s.star_mask&(1<<opening_star[i])));
    std::memcpy(root_grid, s.grid, sizeof(root_grid));
    // 下一批星星 8 回合内出现：把没有星星的补给点也当作目标，提前去占位。
    soon_mask = 0;
    {
        const int t = s.turn;
        int next = s.P.supply_first;
        if (t >= next) next += ((t - s.P.supply_first) / s.P.supply_period + 1) * s.P.supply_period;
        if (next - t <= 8) soon_mask = ((1 << s.n_supply) - 1) & ~s.star_mask;
    }
    rng_state = 0x9E3779B97F4A7C15ULL ^ (uint64_t(s.turn) * 0x100000001B3ULL) ^ s.wall_hash;
    for (int k = 0; k < 4; ++k) rng();

#if SP_METHOD == 1
    return decide_ibr(s, me);
#endif
    make_key_seeds(s.turn);prepare_keys(s);
    pool_used = 0; std::memset(state_table,0,sizeof(state_table));
    int root[6];for(int i=0;i<6;i++)root[i]=lookup_state(s,i,true);
    actor_root(s,root);

    int steps = 0, sims = 0;
    struct Visit { int32_t node; int8_t act; };
    Visit path[6][SP_DEPTH];
    int path_len[6];
    for (;;) {
        if (steps + SP_DEPTH > SP_BUDGET) break;
#if SP_USE_TIME
        if ((sims & 15) == 0 && (budget_left() < 8 || (budget_used()-call_cpu_start+call_init_ms) > call_cpu_limit)) break;
#endif
        State st = s;
        int cur[6];
        bool expanded[6] = {}; bool added=false;
        for (int i = 0; i < 6; ++i) { cur[i] = root[i]; path_len[i] = 0; }
        int e_sum = 0, depth_done = 0;
        for (int depth = 0; depth < SP_DEPTH && !st.done; ++depth) {
            prepare_keys(st);
            for(int i=0;i<6;i++){cur[i]=lookup_state(st,i,!expanded[i]);if(cur[i]>=0&&pool[cur[i]].visits==0&&depth>0)expanded[i]=true;}
            prepare_scan(st);
            Orders o[2];
            for (int i = 0; i < 6; ++i) {
                if (!st.tank[i].alive) continue;
                Cmd c;
                if (cur[i] >= 0) {
                    Node& nd = pool[cur[i]];
                    if (nd.n_acts == 0) build_actions(nd, st, i);
                    const int a = select_action(nd);
                    c = nd.act[a];
                    path[i][path_len[i]++] = Visit{cur[i], int8_t(a)};

                } else {
                    c = rollout_cmd(st, i);
                }
                o[i / 3].cmd[i % 3] = c;
            }
            step(st, o[0], o[1]);
            ++steps;
            ++depth_done;
            e_sum += evaluate(st);
            if (st.done) e_sum += (SP_DEPTH - depth_done) * evaluate(st);     // 提前结束：剩下的回合都按终局分算
        }
        // 一次模拟的分数取沿途每回合评估的平均，而不是只看最后一回合：
        // 这是个拼时间的游戏，"早三回合打到""中途死过一次又复活"都应该体现在分数里。
#if SP_PATH_AVG
        const int e = e_sum / SP_DEPTH;
#else
        const int e = evaluate(st);
#endif
        const float v = st.done ? (st.winner<0?0.f:st.winner==0?1.f:-1.f) : 0.650000f*float(e)/(float(SP_SQUASH)+std::abs(float(e)))+0.350000f*neural_value(st);
        for (int i = 0; i < 6; ++i) {
            const float vi = i < 3 ? v : -v;
            for (int k = 0; k < path_len[i]; ++k) {
                Node& nd = pool[path[i][k].node];
                nd.visits += 1;
                nd.total += vi;
                nd.n[path[i][k].act] += 1;
                nd.sum[path[i][k].act] += vi;
            }
        }
        ++sims;
    }
    total_steps += steps;
#ifdef SP_DEBUG
    if (me == SP_DEBUG) {
        std::fprintf(stderr, "T%d sims=%d nodes=%d\n", s.turn + 1, sims, pool_used);
        for (int i = 0; i < 6; ++i) {
            const Node& nd = pool[root[i]];
            std::fprintf(stderr, "  tank%d Q=%.3f:", i, nd.visits ? nd.total / nd.visits : 0.f);
            for (int a = 0; a < nd.n_acts; ++a)
                std::fprintf(stderr, " %s(p%.2f n%d q%.3f)", cmd_text(nd.act[a]).c_str(), nd.p[a], nd.n[a], nd.n[a] ? nd.sum[a] / nd.n[a] : 0.f);
            std::fputc(10, stderr);
        }
    }
#endif

    Orders out;
    for (int type = 0; type < 3; ++type) {
        const Node& nd = pool[root[tank_id(me, type)]];
        int best = -1;
        for (int a = 0; a < nd.n_acts; ++a)
            if (best < 0 || nd.n[a] > nd.n[best] || (nd.n[a] == nd.n[best] && nd.n[a] > 0 && nd.sum[a] > nd.sum[best])) best = a;
        if (best >= 0) out.cmd[type] = nd.act[best];
    }
#if SP_FFFIX
    // 三辆车的命令是各自在自己的树里选的，偶尔会凑成"一辆开炮、另一辆正好走进炮线"。
    // 出手前用对方最可能的命令前推一回合看一眼：有队友被自己人打中，就把开炮的那辆换成它访问次数次多的非开炮命令。
    for (int round = 0; round < 3; ++round) {
        Orders o[2];
        o[me] = out;
        for (int type = 0; type < 3; ++type) {
            const Node& nd = pool[root[tank_id(1 - me, type)]];
            int best = -1;
            for (int a = 0; a < nd.n_acts; ++a)
                if (best < 0 || nd.n[a] > nd.n[best]) best = a;
            if (best >= 0) o[1 - me].cmd[type] = nd.act[best];
        }
        State st = s;
        EventLog log;
        step(st, o[0], o[1], &log);
        int culprit = -1;
        for (const Event& e : log.list)
            if ((e.type == EV_HIT || e.type == EV_HIT_SWAP) && e.a / 3 == me && e.b / 3 == me && is_fire(out.cmd[e.b % 3])) culprit = e.b;
        if (culprit < 0) break;
        const Node& nd = pool[root[culprit]];
        int best = -1;
        for (int a = 0; a < nd.n_acts; ++a)
            if (!is_fire(nd.act[a]) && (best < 0 || nd.n[a] > nd.n[best])) best = a;
        out.cmd[culprit % 3] = best >= 0 ? nd.act[best] : STAY;
    }
#endif
    return out;
}
