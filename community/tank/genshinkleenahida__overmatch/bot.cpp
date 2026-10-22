/*
坦克超越火力（Overmatch）：完整指令兜底、首回合预测预取与终局证书。
首回合先固定最终己方指令，再让各模型在预测的下一公开状态上推进；每个模型采用独立的50毫秒预算视图。
下一根节点仅在完整状态及座位吻合时消费一次缓存，不重复推进历史或随机数；不可用的已推进模型归入未知类。
官方计量器不重置。先准备原生地图、动作及策略先验，辅助推断耗尽额度或无完整推演时仍保留合法完整候选。
一血队友面临直接炮击时，先进行有界完整结算协调，再经原友伤检查；终局胜利优先，其次生存，再以原评价破同分。
战斗搜索及原预训练权重继承自 community/tank/slzhta__value-based-puct/bot.cpp，并保留 sparring/Astra 启发式来源。
本稿未采用新训练网络。完整六步非终局推演采用65%压缩后的路径平均规则值与35%末状态神经值；终局结果覆盖估值。
对手根预测引用仓库MIT公开策略，来源如下：
tank/bots/half2/{gpt-6-astra,gpt-6.1-sol-ultrafast,gpt-6.1-sol,opus-5.5,
kimi-k3,gemini-4-argon,deepseek-v4.1-flash,mimo-v2.6-pro}.cpp；
tank/tournament/sparring.cpp；tank/workspace/baseline.cpp。
只使用公开状态及局内历史。完整上一步转移检验用于选择受限根响应；未知类采用均匀合法联合指令似然作为置信代理。
该代理不等同于校准的身份概率或纳什保证。计算不可用模型的概率份额仅转入未知类；最终友伤检查采用完整最大后验预测，同分优先未知。
官方预算仍为首2000、后50 CPU毫秒。后续内部总目标32毫秒，含辅助与过滤；首次原生搜索为辅助耗时加24毫秒。
未完成推演不回传。运行时不访问文件、网络，不启动进程或线程，不重置计量器。
默认参数的专用模拟与官方结算做过差分检验。MiMo复用精确占位、可开炮与射线结果；固定工作量下评价和指令经对照一致。
限时原策略可能停在不同推演前缀，故不声称每次限时预测完全相同。私有剩余视图预留8毫秒；低于14毫秒保护线即视作计算未完成。
主要上游部分的版权与许可如下，MIT许可原文亦见仓库LICENSE。
MIT许可证
版权所有（c）2026 meow-ai-arena 项目贡献者。
特此免费授予任何获得本软件及相关文档文件（以下简称“软件”）副本的人，不受限制地处理软件的权利，
包括但不限于使用、复制、修改、合并、出版、分发、再许可及／或出售软件副本，并允许获提供软件的人从事上述行为，但须遵守下列条件：
上述版权声明及本许可声明必须包含在软件的所有副本或实质部分中。
软件按“原样”提供，不附带任何明示或默示保证，包括但不限于适销性、特定用途适用性及不侵权保证。
无论因合同、侵权或其他原因产生，作者或版权持有人均不对因软件、软件使用或其他软件交易而起、由其引起或与其有关的任何索赔、损害或其他责任承担责任。





























*/
#include <algorithm>
#include <queue>
#include <utility>
#include <cstdio>
#include <unordered_map>
#include <limits>
#include <memory>
#include <array>
#define LOCAL_RANGE 3
#ifndef SP_BUDGET
#define SP_BUDGET 80000
#endif
#define SP_STARW 90
#define SP_LVV 70
#define SP_STAR_REACH 60
#include "tank.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
using namespace tank;

namespace model_budget_view{
static bool active=false;
static double started=0;
static double remaining(){
const double real=std::max(0.,tank::budget_left()-8.);
return active?std::min(real,std::max(0.,50.-(tank::budget_used()-started))):real;
}
struct Scope{
Scope(){started=tank::budget_used();active=true;}
~Scope(){active=false;}
};
}

static bool specialized_root_default=false;
static inline int specialized_bullet_cap(const State& s,int i){
constexpr Params P{};const TankType& t=P.types[type_of(i)];
if(s.tank[i].level>=1&&(!P.level1_no_reload||t.reload==0))return t.bullets+P.level1_bullets;
return t.bullets;
}
static inline int specialized_reload_turns(const State& s,int i){
constexpr Params P{};int rl=P.types[type_of(i)].reload;
if(rl&&s.tank[i].level>=1&&P.level1_no_reload)rl=0;
return rl;
}
static inline void specialized_destroy_tank(State& s,int i){
constexpr Params P{};Tank& t=s.tank[i];t.alive=false;t.pos=NO_CELL;t.hp=0;t.level=0;
t.last_fire=NEVER_FIRED;t.respawn_turn=int16_t(s.turn+P.respawn);
}
static inline void specialized_level_up(State& s,int i){
constexpr Params P{};Tank& t=s.tank[i];
if(t.level<P.max_level){t.level+=1;t.hp+=int16_t(P.level_hp);}
s.stars_eaten[seat_of(i)]+=1;
}
static inline void specialized_step_default(State& s,const Orders& seat0,const Orders& seat1){
EventLog* const log = nullptr;
if(log)log->list.clear();
if(s.done)return;
constexpr Params P{};
const int turn = ++s.turn;
using detail::add_event;
int8_t occupancy[CELLS];std::memset(occupancy,-1,sizeof(occupancy));
int owned_bullets[6]={};
for(int i=0;i<6;i++)if(s.tank[i].alive)occupancy[s.tank[i].pos]=int8_t(i);
for(int b=0;b<s.n_bullets;b++)++owned_bullets[s.bullet[b].owner];
int move1[6],move2[6];
int base_hits_now[2]={0,0};
for(int i = 0;i < 6;++i){
move1[i]= move2[i]= -1;
Tank& t = s.tank[i];
if(!t.alive)continue;
const int c =(i < 3 ? seat0 : seat1).cmd[i % 3];
if(c >= 1 && c <= 4){
move1[i]= c - 1;
}else if(c >= 5 && c <= 8){
if(owned_bullets[i]< specialized_bullet_cap(s,i)&& turn - t.last_fire > specialized_reload_turns(s,i)&&
s.n_bullets < MAX_BULLETS){
t.last_fire = int16_t(turn);
s.bullet[s.n_bullets++]= Bullet{t.pos,int8_t(c - 5),int8_t(i),
t.level >= 2 && P.level2_steel,int8_t(t.level)};
add_event(log,EV_FIRE,0,i,c - 5,t.pos);
}
}else if(c >= 9 && c <= 24){
move1[i]=(c - 9)>> 2;
if(P.types[i % 3].speed >= 2)move2[i]=(c - 9)& 3;
}
}
for(int sub = 1;sub <= 2;++sub){
const int* mv = sub == 1 ? move1 : move2;
int want_tank[6],want_cell[6],n_want = 0;
for(int i = 0;i < 6;++i){
if(mv[i]< 0)continue;
const int target = s.tank[i].pos + DIR_DELTA[mv[i]];
if(s.grid[target]== EMPTY && occupancy[target]< 0){
want_tank[n_want]= i;
want_cell[n_want++]= target;
}else{
add_event(log,EV_BLOCKED,sub,i,-1,s.tank[i].pos);
}
}
int moved_tank[6],moved_from[6],moved_to[6],n_moved = 0;
for(int a = 0;a < n_want;++a){
bool contested = false;
for(int b = 0;b < n_want;++b)contested |=(b != a && want_cell[b]== want_cell[a]);
if(contested){
add_event(log,EV_BLOCKED,sub,want_tank[a],-1,s.tank[want_tank[a]].pos);
}else{
moved_tank[n_moved]= want_tank[a];
moved_from[n_moved]= s.tank[want_tank[a]].pos;
moved_to[n_moved++]= want_cell[a];
}
}
for(int m = 0;m < n_moved;++m)occupancy[moved_from[m]]=-1;
for(int m = 0;m < n_moved;++m){
s.tank[moved_tank[m]].pos = int16_t(moved_to[m]);
occupancy[moved_to[m]]=int8_t(moved_tank[m]);
}
const int nb = s.n_bullets;
if(nb == 0)continue;
for(int k = 0;k < nb;++k)s.bullet[k].pos += int16_t(DIR_DELTA[s.bullet[k].dir]);
bool gone[MAX_BULLETS]={};
if(P.bullet_cancel && nb > 1){
for(int a = 0;a < nb;++a){
const Bullet& ba = s.bullet[a];
for(int b = a + 1;b < nb;++b){
const Bullet& bb = s.bullet[b];
if(bb.dir ==(ba.dir ^ 1)&& ba.pos - DIR_DELTA[ba.dir]== bb.pos){
gone[a]= gone[b]= true;
add_event(log,EV_CANCEL_CROSS,sub,ba.owner,bb.owner,ba.pos);
}
}
}
}
for(int m = 0;m < n_moved;++m){
Tank& t = s.tank[moved_tank[m]];
for(int k = 0;k < nb;++k){
const Bullet& b = s.bullet[k];
if(!gone[k]&& b.pos == moved_from[m]&& b.pos - DIR_DELTA[b.dir]== t.pos){
gone[k]= true;
t.hp -= 1;
add_event(log,EV_HIT_SWAP,sub,moved_tank[m],b.owner,t.pos);
}
}
}
int16_t brick_hit[MAX_BULLETS],steel_hit[MAX_BULLETS];
int n_brick = 0,n_steel = 0;
int own_took[2]={0,0};
for(int k = 0;k < nb;++k){
if(gone[k])continue;
const Bullet& b = s.bullet[k];
switch(s.grid[b.pos]){
case EMPTY:{
const int v = occupancy[b.pos];
if(v >= 0){
gone[k]= true;
s.tank[v].hp -= 1;
add_event(log,EV_HIT,sub,v,b.owner,b.pos);
}
break;
}
case WATER:       
break;
case BRICK:       
gone[k]= true;
brick_hit[n_brick++]= b.pos;
break;
case STEEL:       
gone[k]= true;
if(b.steel)steel_hit[n_steel++]= b.pos;
break;
case OUTSIDE:         
gone[k]= true;
break;
default:{
const int seat = s.grid[b.pos]- BASE0;
const bool enemy = seat_of(b.owner)!= seat;
gone[k]= true;
base_hits_now[seat]+= 1;
if(P.base_armor && b.level < P.base_armor){
base_hits_now[seat]-= 1;
}else if(s.base_hp[seat]> 0 && !(P.base_hit_cap && base_hits_now[seat]> P.base_hit_cap)){
s.base_hp[seat]-= 1;
if(enemy)s.base_dmg[1 - seat]+= 1;
else own_took[seat]+= 1;
}else if(enemy && own_took[seat]> 0 && !P.proto_base_dmg_by_bullet_order){
own_took[seat]-= 1;
s.base_dmg[1 - seat]+= 1;
}
add_event(log,EV_BASE_HIT,sub,seat,b.owner,b.pos);
break;
}
}
}
for(int k = 0;k < n_brick;++k)
if(s.grid[brick_hit[k]]== BRICK){
s.grid[brick_hit[k]]= EMPTY;
s.wall_hash ^= wall_key(brick_hit[k]);
add_event(log,EV_WALL_BRICK,sub,-1,-1,brick_hit[k]);
}
for(int k = 0;k < n_steel;++k)
if(s.grid[steel_hit[k]]== STEEL){
s.grid[steel_hit[k]]= EMPTY;
s.wall_hash ^= wall_key(steel_hit[k]);
add_event(log,EV_WALL_STEEL,sub,-1,-1,steel_hit[k]);
}
if(P.bullet_cancel){
bool left[MAX_BULLETS];
for(int k = 0;k < nb;++k)left[k]= !gone[k];
for(int k = 1;k < nb;++k){
if(!left[k])continue;
for(int j = 0;j < k;++j)
if(left[j]&& s.bullet[j].pos == s.bullet[k].pos){
gone[k]= gone[j]= true;
add_event(log,EV_CANCEL_SAME,sub,-1,-1,s.bullet[k].pos);
break;
}
}
}
int keep = 0;
for(int k = 0;k < nb;++k)
if(!gone[k])s.bullet[keep++]= s.bullet[k];
for(int k = keep;k < nb;++k)s.bullet[k]= Bullet{};
s.n_bullets = int16_t(keep);
}
for(int i = 0;i < 6;++i){
Tank& t = s.tank[i];
if(t.alive && t.hp <= 0){
add_event(log,EV_DEATH,0,i,-1,t.pos);
occupancy[s.tank[i].pos]=-1;
specialized_destroy_tank(s,i);
s.kills[1 - seat_of(i)]+= 1;
}
}
int plan_tank[6],plan_victim[6],n_plan = 0;
for(int i = 0;i < 6;++i){
if(s.tank[i].alive || s.tank[i].respawn_turn > turn)continue;
const int v = occupancy[s.spawn[i]];
if(v >= 0 &&(seat_of(v)== seat_of(i)|| !P.respawn_telefrag))continue;
plan_tank[n_plan]= i;
plan_victim[n_plan++]= v;
}
for(int k = 0;k < n_plan;++k){
const int v = plan_victim[k];
if(v < 0)continue;
add_event(log,EV_DEATH,0,v,plan_tank[k],s.tank[v].pos);
occupancy[s.tank[v].pos]=-1;
specialized_destroy_tank(s,v);
s.kills[seat_of(plan_tank[k])]+= 1;
}
for(int k = 0;k < n_plan;++k){
const int i = plan_tank[k];
Tank& t = s.tank[i];
t.alive = true;
t.pos = s.spawn[i];
occupancy[t.pos]=int8_t(i);
t.hp = int16_t(P.types[i % 3].hp);
t.level = 0;
t.last_fire = NEVER_FIRED;
t.respawn_turn = 0;
add_event(log,EV_RESPAWN,0,i,-1,t.pos);
if(P.respawn_clear_bullets && s.n_bullets > 0){
int keep = 0;
const int nb = s.n_bullets;
for(int b = 0;b < nb;++b)
if(s.bullet[b].pos != t.pos)s.bullet[keep++]= s.bullet[b];
if(keep != nb){
for(int b = keep;b < nb;++b)s.bullet[b]= Bullet{};
s.n_bullets = int16_t(keep);
add_event(log,EV_BULLET_CLEARED,0,i,-1,t.pos);
}
}
}
if(s.star_mask){
for(int i = 0;i < 6;++i){
if(!s.tank[i].alive)continue;
if(s.tank[i].level >= P.max_level && !P.proto_pickup_at_max_level)continue;
for(int k = 0;k < s.n_supply;++k)
if(((s.star_mask >> k)& 1)&& s.supply[k]== s.tank[i].pos){
s.star_mask &= uint8_t(~(1u << k));
specialized_level_up(s,i);
}
}
}
if(turn >= P.supply_first &&(turn - P.supply_first)% P.supply_period == 0){
for(int k = 0;k < s.n_supply;++k){
if((s.star_mask >> k)& 1)continue;
const int v = P.proto_supply_feeds_tank ? tank_at(s,s.supply[k]): -1;
if(v < 0){
s.star_mask |= uint8_t(1u << k);
add_event(log,EV_SUPPLY,0,-1,-1,s.supply[k]);
}else{
specialized_level_up(s,v);
}
}
}
if(P.base_regen){
for(int seat = 0;seat < 2;++seat){
if(base_hits_now[seat]|| s.base_hp[seat]<= 0){
s.base_quiet[seat]= 0;
}else if(s.base_hp[seat]< P.base_hp){
s.base_quiet[seat]+= 1;
if(s.base_quiet[seat]>= P.base_regen){
s.base_quiet[seat]= 0;
s.base_hp[seat]+= 1;
add_event(log,EV_BASE_REGEN,0,seat,-1,NO_CELL);
}
}
}
}
const int h0 = s.base_hp[0],h1 = s.base_hp[1];
if(h0 <= 0 || h1 <= 0 || turn >= P.max_turns){
s.done = true;
if(h0 <= 0 && h1 > 0){
s.winner = 1;
s.reason = R_BASE;
}else if(h1 <= 0 && h0 > 0){
s.winner = 0;
s.reason = R_BASE;
}else{
s.winner = -1;
s.reason = R_DRAW;
for(int k = 0;k < P.n_tiebreak;++k){
int a,b;
detail::tiebreak_values(s,P.tiebreak[k],a,b);
if(a != b){
s.winner = a > b ? 0 : 1;
s.reason = Reason(R_HP + P.tiebreak[k]);
break;
}
}
}
}
}
static inline void sim_step(State& s,const Orders& seat0,const Orders& seat1){
if(specialized_root_default)specialized_step_default(s,seat0,seat1);
else tank::step(s,seat0,seat1);
}
#ifndef SP_BUDGET
#define SP_BUDGET 80000         
#endif
#ifndef SP_USE_TIME
#define SP_USE_TIME 1           
#endif
#ifndef SP_TIME_MARGIN
#define SP_TIME_MARGIN 4        
#endif
#ifndef SP_DEPTH
#define SP_DEPTH 6              
#endif
#ifndef SP_UCB
#define SP_UCB 80               
#endif
#ifndef SP_PRIOR_T
#define SP_PRIOR_T 20           
#endif
#ifndef SP_FPU
#define SP_FPU 0                 
#endif
#ifndef SP_STARS
#define SP_STARS 1              
#endif
#ifndef SP_STARW
#define SP_STARW 26             
#endif
#ifndef SP_LVV
#define SP_LVV 20               
#endif
#ifndef SP_STAR_REACH
#define SP_STAR_REACH 24        
#endif
#ifndef SP_FFFIX
#define SP_FFFIX 1              
#endif
#ifndef SP_TV
#define SP_TV 22                
#endif
#ifndef SP_TV_FIRE
#define SP_TV_FIRE 34           
#endif
#ifndef SP_THREAT
#define SP_THREAT 40            
#endif
#ifndef SP_MAX_HUNTERS
#define SP_MAX_HUNTERS 1        
#endif
#ifndef SP_EMERGENCY
#define SP_EMERGENCY 0          
#endif
#ifndef SP_GUARDS
#define SP_GUARDS 1             
#endif
#ifndef SP_GUARD_RANGE
#define SP_GUARD_RANGE 48       
#endif
#ifndef SP_FAR_COST
#define SP_FAR_COST 3           
#endif
#ifndef SP_BLOCK_STILL
#define SP_BLOCK_STILL 0        
#endif
#ifndef SP_HUNT_MAX
#define SP_HUNT_MAX 64          
#endif
#ifndef SP_SQUASH
#define SP_SQUASH 600           
#endif
#ifndef SP_PATH_AVG
#define SP_PATH_AVG 1           
#endif
#ifndef SP_METHOD
#define SP_METHOD 0             
#endif
#ifndef SP_IBR_ROUNDS
#define SP_IBR_ROUNDS 3         
#endif
#ifndef SP_ROLLOUT_RANDOM
#define SP_ROLLOUT_RANDOM 4     
#endif
static const int BIG = 30000;
static const int MAX_ACTS = 22;
struct Maps{
bool valid = false;
uint64_t wall_hash = 0;
int16_t atk[2][CELLS];
int16_t sup[MAX_SUPPLY][CELLS];
int n_key[2];
int16_t key_cell[2][24];
int16_t key_val[2][24];
};
static Maps M;
static void flood(const State& s,int16_t* dist){
static int queue[CELLS * 16];
int head = 0,tail = 0;
for(int c = 0;c < CELLS;++c)
if(dist[c]< BIG)queue[tail++]= c;
while(head < tail){
const int c = queue[head++ %(CELLS * 16)];
for(int d = 0;d < 4;++d){
const int n = c + DIR_DELTA[d];
const uint8_t g = s.grid[n];
if(g != EMPTY && g != BRICK)continue;
const int nd = dist[c]+(g == BRICK ? 12 : 4);
if(nd < dist[n]){dist[n]= int16_t(nd);queue[tail++ %(CELLS * 16)]= n;}
}
}
}
static void build_maps(const State& s){
if(M.valid && M.wall_hash == s.wall_hash)return;
M.valid = true;
M.wall_hash = s.wall_hash;
for(int seat = 0;seat < 2;++seat){
int16_t* dist = M.atk[seat];
for(int c = 0;c < CELLS;++c)dist[c]= BIG;
M.n_key[seat]= 0;
for(int d = 0;d < 4;++d){
int bricks = 0,k = 1;
for(int c = s.base_pos[1 - seat]+ DIR_DELTA[d];;c += DIR_DELTA[d],++k){
const uint8_t g = s.grid[c];
if(g == BRICK){
if(M.n_key[seat]< 24){
M.key_cell[seat][M.n_key[seat]]= int16_t(c);
M.key_val[seat][M.n_key[seat]++]= int16_t(bricks == 0 ? 16 : 8);
}
dist[c]= int16_t(8 * bricks + 12 +(k > 2 ? 2 *(k - 2): 0));
++bricks;
continue;
}
if(g == WATER)continue;
if(g != EMPTY)break;
dist[c]= int16_t(8 * bricks +(k > 2 ? 2 *(k - 2): 0));
}
}
flood(s,dist);
}
for(int k = 0;k < s.n_supply;++k){
for(int c = 0;c < CELLS;++c)M.sup[k][c]= BIG;
M.sup[k][s.supply[k]]= 0;
flood(s,M.sup[k]);
}
}
static uint8_t root_grid[CELLS];
static int soon_mask;
static int16_t atk_of[6][CELLS];
static void flood(const State& s,int16_t* dist);
static int still[6],still_pos[6];
static void build_tank_maps(const State& s){
for(int j = 0;j < 6;++j){
if(s.tank[j].alive && s.tank[j].pos == still_pos[j])still[j]+= 1;
else still[j]= 0;
still_pos[j]= s.tank[j].alive ? s.tank[j].pos : -1;
}
for(int i = 0;i < 6;++i){
const int seat = i / 3;
int16_t* dist = atk_of[i];
for(int c = 0;c < CELLS;++c)dist[c]= BIG;
for(int d = 0;d < 4;++d){
int bricks = 0,k = 1;
for(int c = s.base_pos[1 - seat]+ DIR_DELTA[d];;c += DIR_DELTA[d],++k){
const uint8_t g = s.grid[c];
if(g == BRICK){
dist[c]= int16_t(8 * bricks + 12 +(k > 2 ? SP_FAR_COST *(k - 2): 0));
++bricks;
continue;
}
if(g == WATER)continue;
if(g != EMPTY)break;
dist[c]= int16_t(8 * bricks +(k > 2 ? SP_FAR_COST *(k - 2): 0));
bool blocked = false;
for(int j = 0;j < 6;++j)blocked |= j != i && s.tank[j].alive && s.tank[j].pos == c && still[j]>= SP_BLOCK_STILL;
if(blocked)break;
}
}
static int queue[CELLS * 16];
static int16_t extra[CELLS];
for(int c = 0;c < CELLS;++c)extra[c]= 0;
for(int j = 0;j < 6;++j)
if(j != i && s.tank[j].alive)extra[s.tank[j].pos]= int16_t(still[j]>= 3 ? 24 : 8);
int head = 0,tail = 0;
for(int c = 0;c < CELLS;++c)
if(dist[c]< BIG)queue[tail++]= c;
while(head < tail){
const int c = queue[head++ %(CELLS * 16)];
for(int d = 0;d < 4;++d){
const int n = c + DIR_DELTA[d];
const uint8_t g = s.grid[n];
if(g != EMPTY && g != BRICK)continue;
const int nd = dist[c]+(g == BRICK ? 12 : 4)+ extra[c];
if(nd < dist[n]){dist[n]= int16_t(nd);queue[tail++ %(CELLS * 16)]= n;}
}
}
}
}
static inline int progress_cost(int i,int pos){
int a = atk_of[i][pos];
if(a > 240)a = 240;
return i % 3 == FAST ? a * 5 / 8 : a;
}
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
static int hunt_target[6];
static int16_t hunt_map[3][CELLS];
static int16_t post_map[CELLS];
static bool holding[6];
static int prev_target[3];
static int stale_turns;
static int stale_hp[2],stale_kills[2];
static uint64_t stale_walls;
static void assign_roles(const State& s,int me){
if(s.base_hp[0]!= stale_hp[0]|| s.base_hp[1]!= stale_hp[1]|| s.kills[0]!= stale_kills[0]||
s.kills[1]!= stale_kills[1]|| s.wall_hash != stale_walls){
stale_turns = 0;
stale_hp[0]= s.base_hp[0];stale_hp[1]= s.base_hp[1];
stale_kills[0]= s.kills[0];stale_kills[1]= s.kills[1];
stale_walls = s.wall_hash;
}else{
stale_turns += 1;
}
int max_hunters = SP_MAX_HUNTERS;
if(stale_turns >= 10 && max_hunters > 1)max_hunters = 1;
if(stale_turns >= 30)max_hunters = 0;
int n_hunters = 0;
#if SP_EMERGENCY
{
int urgent = 0;
for(int k = 0;k < 3;++k){
const int foe = 3 *(1 - me)+ k;
if(!s.tank[foe].alive)continue;
int c = M.atk[1 - me][s.tank[foe].pos];
if(k == FAST)c = c * 5 / 8;
urgent += c <= SP_EMERGENCY;
}
if(urgent > max_hunters && stale_turns < 10)max_hunters = urgent;
}
#endif
root_me = me;
for(int i = 0;i < 6;++i){hunt_target[i]= -1;holding[i]= false;}
const int foe0 = 3 *(1 - me);
int threat[3],threat_cost[3],n = 0;
for(int k = 0;k < 3;++k){
if(!s.tank[foe0 + k].alive)continue;
int c = M.atk[1 - me][s.tank[foe0 + k].pos];
if(k == FAST)c = c * 5 / 8;
if(c > SP_THREAT)continue;
int at = n++;
while(at > 0 && threat_cost[at - 1]> c){threat[at]= threat[at - 1];threat_cost[at]= threat_cost[at - 1];--at;}
threat[at]= k;
threat_cost[at]= c;
}
bool used[3]={};
int now_target[3]={-1,-1,-1};
for(int q = 0;q < n && n_hunters < max_hunters;++q){
const int k = threat[q],foe = foe0 + k;
int16_t* dist = hunt_map[k];
for(int c = 0;c < CELLS;++c)dist[c]= BIG;
for(int d = 0;d < 4;++d){
int steps = 1;
for(int c = s.tank[foe].pos + DIR_DELTA[d];s.grid[c]== EMPTY || s.grid[c]== WATER;c += DIR_DELTA[d],++steps)
if(s.grid[c]== EMPTY)dist[c]= int16_t(steps <= 2 ? 0 : 2 *(steps - 2));
}
flood(s,dist);
int best = -1,best_eta = SP_HUNT_MAX;
for(int t = 0;t < 3;++t){
if(used[t])continue;
const int i = 3 * me + t;
const bool alive = s.tank[i].alive;
const int pos = alive ? s.tank[i].pos : s.spawn[i];
int eta = dist[pos]>= BIG ? BIG :(t == FAST ? dist[pos]* 5 / 8 : dist[pos]);
if(!alive)eta += 4 * respawn_in(s,i);
if(alive && progress_cost(i,pos)<= 4 && eta > 8)continue;
if(prev_target[t]== foe)eta -= 8;
#if SP_GUARDS
if(!(t == ARMOR ||(SP_GUARDS >= 2 && t == FAST)|| SP_GUARDS == 3))eta += 12;
#endif
if(eta < best_eta){best_eta = eta;best = t;}
}
if(best >= 0){
++n_hunters;
used[best]= true;
hunt_target[3 * me + best]= foe;
now_target[best]= foe;
}
}
for(int t = 0;t < 3;++t)prev_target[t]= now_target[t];
#if SP_GUARDS
{
bool coming = false;
for(int k = 0;k < 3;++k){
if(!s.tank[foe0 + k].alive)continue;
int c = M.atk[1 - me][s.tank[foe0 + k].pos];
if(k == FAST)c = c * 5 / 8;
coming |= c <= SP_GUARD_RANGE;
}
static const int order[3]={ARMOR,FAST,FIRE};
int n_guards = SP_GUARDS;
if(SP_GUARDS == 9){
int n_coming = 0;
for(int k = 0;k < 3;++k){
int c = M.atk[1 - me][s.tank[foe0 + k].alive ? s.tank[foe0 + k].pos : s.spawn[foe0 + k]];
if(k == FAST)c = c * 5 / 8;
n_coming += s.tank[foe0 + k].alive && c <= SP_GUARD_RANGE;
}
n_guards = n_coming - 1 - n_hunters;
if(n_guards > 2)n_guards = 2;
}
for(int q = 0;q < n_guards && q < 3;++q){
const int i = 3 * me + order[q];
if(coming && hunt_target[i]< 0 && stale_turns < 30)holding[i]= true;
}
for(int c = 0;c < CELLS;++c)post_map[c]= BIG;
const int fwd = me == 0 ? UP : DOWN;
const int post = s.base_pos[me]+ 3 * DIR_DELTA[fwd];
for(int d = 0;d < 4;++d){
const int c = post + DIR_DELTA[d];
if(s.grid[c]== EMPTY)post_map[c]= 0;
}
if(s.grid[post]== EMPTY)post_map[post]= 0;
flood(s,post_map);
}
#endif
}
static inline int tank_cost(const State& s,int i,int pos){
if(opening_active[i]){int d=M.sup[opening_star[i]][pos];return d>240?240:(i%3==FAST?d*5/8:d);}
const int target = hunt_target[i];
if(holding[i]){const int a = post_map[pos];return a > 240 ? 240 :(i % 3 == FAST ? a * 5 / 8 : a);}
if(target < 0)return progress_cost(i,pos);
if(!s.tank[target].alive)return 0;
int a = hunt_map[target % 3][pos];
if(a > 240)a = 240;
return i % 3 == FAST ? a * 5 / 8 : a;
}
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
#include <bit>
#include <limits>
namespace lossless_model_weights{
static_assert(sizeof(float)==4 && sizeof(std::uint32_t)==4);
static_assert(std::numeric_limits<float>::is_iec559);
static unsigned digit(char c){
if(c>='A'&&c<='Z')return unsigned(c-'A');
if(c>='a'&&c<='z')return unsigned(c-'a'+26);
if(c>='0'&&c<='9')return unsigned(c-'0'+52);
return c=='+'?62u:63u;
}
template<std::size_t N,std::size_t M>
static std::array<float,N> decode(const char(&text)[M]){
static_assert(M-1==4*((4*N+2)/3));
std::array<float,N> out{};
std::uint32_t accumulator=0,word=0;
unsigned bits=0,bytes=0;std::size_t at=0;
for(std::size_t k=0;k<M-1&&text[k]!='=';k++){
accumulator=(accumulator<<6)|digit(text[k]);bits+=6;
if(bits>=8){
bits-=8;const unsigned byte=(accumulator>>bits)&255u;
word|=std::uint32_t(byte)<<(8*bytes++);
if(bytes==4){out[at++]=std::bit_cast<float>(word);word=0;bytes=0;}
}
}
return out;
}
}
static const auto VALUE_W0=lossless_model_weights::decode<2048>(
"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"
"AAAAAAAAAAAAAAAAAAAAAAA=");
static const auto VALUE_B0=lossless_model_weights::decode<32>(
"gsrDPpOOwr0pMRs9IDSzPDDUt72ZvIE9xDRcveG8MD03Ynm9vM4pPgyBAb62wtI84Wx+vRVx+T1ORK+9Bna4PT+5xTz3wu67F2k1"
"vW5C3r3Vycg792I5Pt27aT0Feny9SrPDvXDDmL0LvCk+9kcwPmcpn73FKq28yZbpPaLGSr0=");
static const auto VALUE_W1=lossless_model_weights::decode<32>(
"DgsiPtO6S79UC02/ca95PwHxa7/zlfM8JysmPx6YRD6GhlW/KnQ9P6QfBz/kil2/4oITPzrGhb72rIe6UGK4P1FI6z6niYG+CI5o"
"v1Q/WL9SIfU+AIymPvL7qT+UI1a//O0DPl0+9ju0BmM/25kyPr7FML+f/YO/chRvP/3PTr8=");
static const std::array<float,2048>& value_weights_transposed(){
static const auto transposed=[]{
std::array<float,2048> w{};
for(int k=0;k<64;k++)for(int j=0;j<32;j++)w[k*32+j]=VALUE_W0[j*64+k];
return w;
}();
return transposed;
}
static float value_head(const float* x){
const auto& transposed=value_weights_transposed();
float h[32];for(int j=0;j<32;j++)h[j]=VALUE_B0[j];
for(int k=0;k<64;k++)for(int j=0;j<32;j++)h[j]+=transposed[k*32+j]*x[k];
float value=0;for(int j=0;j<32;j++)if(h[j]>0)value+=VALUE_W1[j]*h[j];
return value;
}
static float paired_value_heads(const float*x,const float*y){
const auto& w=value_weights_transposed();
float h[32],g[32];for(int j=0;j<32;j++)h[j]=g[j]=VALUE_B0[j];
for(int k=0;k<64;k++){
if(x[k]==0.f&&y[k]==0.f)continue;
for(int j=0;j<32;j++){const float v=w[k*32+j];h[j]+=v*x[k];g[j]+=v*y[k];}
}
float a=0,b=0;
for(int j=0;j<32;j++){if(h[j]>0)a+=VALUE_W1[j]*h[j];if(g[j]>0)b+=VALUE_W1[j]*g[j];}
return std::tanh(a-b);
}
static float neural_value(const State&s){
float x[64],y[64];value_features(s,0,x);value_features(s,1,y);
return paired_value_heads(x,y);
}
static int evaluate(const State& s){
if(s.done){
if(s.winner < 0)return 0;
return(s.winner == 0 ? 6000 : -6000)+ 10 *(s.base_hp[0]- s.base_hp[1]);
}
static const int tv[3]={SP_TV,SP_TV_FIRE,SP_TV};
int e[2]={0,0};
int8_t evocc[CELLS];std::memset(evocc,-1,sizeof(evocc));for(int i=0;i<6;i++)if(s.tank[i].alive)evocc[s.tank[i].pos]=i;
for(int seat = 0;seat < 2;++seat){
e[seat]+= 100 * s.base_hp[seat]+ 8 * s.kills[seat]+ 2 * s.stars_eaten[seat];
for(int k = 0;k < M.n_key[seat];++k)
if(s.grid[M.key_cell[seat][k]]!= BRICK && root_grid[M.key_cell[seat][k]]== BRICK)e[seat]+= M.key_val[seat][k];
}
for(int i = 0;i < 6;++i){
const Tank& t = s.tank[i];
const int seat = i / 3,type = i % 3;
if(t.alive){
e[seat]-= tv[type]* tank_cost(s,i,t.pos)/ 4;
e[seat]+= 15 *(t.hp - 1)+ SP_LVV * t.level;
}else{
e[seat]-= tv[type]*(tank_cost(s,i,s.spawn[i])+ 4 * respawn_in(s,i))/ 4 + 10;
}
}
#if SP_STARS
const int mask = s.star_mask | soon_mask;
if(mask){
for(int k = 0;k < s.n_supply;++k){
if(!((mask >> k)& 1))continue;
for(int seat = 0;seat < 2;++seat){
int best = BIG;
for(int i = 3 * seat;i < 3 * seat + 3;++i){
if(!s.tank[i].alive || s.tank[i].level >= s.P.max_level)continue;
int a = M.sup[k][s.tank[i].pos];
if(i % 3 == FAST)a = a * 5 / 8;
if(a < best)best = a;
}
const int bonus = SP_STARW - best / 2;
if(bonus > 0)e[seat]+= bonus;
}
}
}
#endif
for(int b = 0;b < s.n_bullets;++b){
const Bullet& bl = s.bullet[b];
int c = bl.pos;
for(int k = 1;k <= 8;++k){
c += DIR_DELTA[bl.dir];
const uint8_t g = s.grid[c];
if(g == WATER)continue;
if(g == BASE0 || g == BASE1){e[g - BASE0]-= 70;break;}
if(g != EMPTY)break;
int v=evocc[c];
if(v >= 0){
e[v / 3]-= k <= 2 ? 20 : 5;
break;
}
}
}
return e[0]- e[1];
}
static int8_t scan_occ[CELLS];
static uint8_t scan_bullet[CELLS];
static float prior_exp[2049];
static void prepare_scan(const State&s){
std::memset(scan_occ,-1,sizeof(scan_occ));std::memset(scan_bullet,0,sizeof(scan_bullet));
for(int i=0;i<6;i++)if(s.tank[i].alive)scan_occ[s.tank[i].pos]=i;
for(int b=0;b<s.n_bullets;b++)scan_bullet[s.bullet[b].pos]|=1<<s.bullet[b].dir;
}
struct Scan{
bool useful;
int prior;
bool danger;
};
static Scan scan_dir(const State& s,int i,int d){
Scan r{false,0,false};
const int seat = i / 3;
int bricks = 0,first_brick=0,c = s.tank[i].pos;bool screened=false;
for(int k = 1;k <= 14;++k){
c += DIR_DELTA[d];
const uint8_t g = s.grid[c];
if(g == WATER)continue;
if(g == BRICK){
if(bricks == 0){first_brick=k;
bool key = false;
for(int q = 0;q < M.n_key[seat];++q)key |= M.key_cell[seat][q]== c;
if(key && k <= 6){r.useful = true;r.prior = r.prior > 62 ? r.prior : 62;}
else if(k <= 2){r.useful = true;r.prior = r.prior > 30 ? r.prior : 30;}
}
++bricks;
continue;
}
if(g == BASE0 +(1 - seat)){
if(k <= 6){r.useful = true;const int p = 96 - 10 * bricks;r.prior = r.prior > p ? r.prior : p;}
break;
}
if(g == STEEL){
if(bricks == 0 && k <= 2 && s.tank[i].level >= 2 && s.P.level2_steel){r.useful = true;r.prior = r.prior > 25 ? r.prior : 25;}
break;
}
if(g != EMPTY)break;
if(bricks){
int v=scan_occ[c];
if(v>=0&&!screened){
screened=true;
// 可移除阻挡后方的敌车射击可能落在现有规划范围内。
// 此处仅保留动作；移动与炮弹抵消仍交由实际搜索判断。
const int arrival=std::max((first_brick+1)/2,reload_turns(s,i)+1)+(k+1)/2;
if(v/3!=seat&&bricks==1&&arrival<=SP_DEPTH){
const bool armed=can_fire(s,v);
const int p=k<=2?(armed?88:99):(k<=4?64:44);
r.useful=true;r.prior=r.prior>p?r.prior:p;
}
}
continue;
}
if((scan_bullet[c]&(1<<(d^1)))&& k<=6){
r.useful = true;
r.danger = true;
r.prior = r.prior > 85 ? r.prior : 85;
}
int v = scan_occ[c];
if(v >= 0){
if(v / 3 == seat)break;
r.useful = true;
const bool armed = can_fire(s,v);
if(k <= 2 && armed)r.danger = true;
const int p = k <= 2 ?(armed ? 88 : 99):(k <= 4 ? 64 : 44);
r.prior = r.prior > p ? r.prior : p;
break;
}
if(k <= 5 && !r.useful){
const int side =(d < 2)? 1 : STRIDE;
for(int cell :{c+side,c-side})if(scan_occ[cell]>=0 && scan_occ[cell]/3!=seat){r.useful=true;r.prior=36;}
}
}
return r;
}
struct Node{
int32_t visits;
float total;
int32_t n[MAX_ACTS];
float sum[MAX_ACTS];
float p[MAX_ACTS];
Cmd act[MAX_ACTS];
uint8_t n_acts;
};
static int goal_cost(const State& s,int i,int pos){
if(opening_active[i])return M.sup[opening_star[i]][pos];
if(hunt_target[i]>= 0)return hunt_map[hunt_target[i]% 3][pos];
if(holding[i])return post_map[pos];
int a = atk_of[i][pos];
#if SP_STARS
if(s.tank[i].level < s.P.max_level &&(s.star_mask | soon_mask)){
for(int k = 0;k < s.n_supply;++k)
if(((s.star_mask | soon_mask)>> k)& 1){
const int b = M.sup[k][pos]+ 8;
if(b < a && M.sup[k][pos]<= SP_STAR_REACH)a = b;
}
}
#else
(void)s;
#endif
return a;
}
static void build_actions(Node& nd,const State& s,int i){
Cmd list[N_CMDS];
const int n = legal_commands(s,i,list);
const Tank& t = s.tank[i];
Scan sc[4];
bool danger = false;
for(int d = 0;d < 4;++d){sc[d]= scan_dir(s,i,d);danger |= sc[d].danger;}
const int here = goal_cost(s,i,t.pos);
Cmd acts[N_CMDS];
int prior[N_CMDS],m = 0;
int ends[N_CMDS],n_ends = 0;
for(int k = 0;k < n;++k){
const Cmd c = list[k];
int p;
if(c == STAY){
p = 46;
}else if(is_fire(c)){
if(!sc[c - 5].useful)continue;
p = sc[c - 5].prior;
}else if(is_move(c)){
const int to = t.pos + DIR_DELTA[c - 1];
p = 50 +(here - goal_cost(s,i,to))* 2;
if(danger && !sc[c - 1].danger && !sc[(c - 1)^ 1].danger)p += 30;
}else{
const int d1 =(c - 9)>> 2,d2 =(c - 9)& 3;
const int mid = t.pos + DIR_DELTA[d1];
if(s.grid[mid]!= EMPTY)continue;
const int to = mid + DIR_DELTA[d2];
if(to == t.pos){
if(!danger || sc[d1].danger || sc[d2].danger)continue;
p = 70;
}else{
if(s.grid[to]!= EMPTY)continue;
bool dup = false;
for(int q = 0;q < n_ends;++q)dup |= ends[q]== to;
if(dup)continue;
ends[n_ends++]= to;
p = 52 +(here - goal_cost(s,i,to))* 2;
if(danger)p += 20;
}
}
acts[m]= c;
prior[m++]= p;
}
for(int a = 1;a < m;++a)
for(int b = a;b > 0 && prior[b]> prior[b - 1];--b){
const int tp = prior[b];prior[b]= prior[b - 1];prior[b - 1]= tp;
const Cmd tc = acts[b];acts[b]= acts[b - 1];acts[b - 1]= tc;
}
if(m > MAX_ACTS)m = MAX_ACTS;
nd.n_acts = uint8_t(m);
float z = 0;
for(int a = 0;a < m;++a){
nd.act[a]= acts[a];
nd.n[a]= 0;
nd.sum[a]= 0;
const int diff=prior[0]-prior[a];nd.p[a]=diff<=2048?prior_exp[diff]:std::exp(float(-diff)/float(SP_PRIOR_T));
z += nd.p[a];
}
for(int a = 0;a < m;++a)nd.p[a]/= z;
}
static uint64_t rng_state;
static inline uint32_t rng(){
rng_state ^= rng_state << 13;
rng_state ^= rng_state >> 7;
rng_state ^= rng_state << 17;
return uint32_t(rng_state >> 16);
}
static Cmd rollout_cmd(const State& s,int i){
const Tank& t = s.tank[i];
const int seat = i / 3;
const uint32_t r = rng();
const bool armed = can_fire(s,i);
if(armed &&(r & 7)!= 0){
for(int d = 0;d < 4;++d){
int c = t.pos,bricks = 0;
for(int k = 1;k <= 4;++k){
c += DIR_DELTA[d];
const uint8_t g = s.grid[c];
if(g == WATER)continue;
if(g == BRICK){if(++bricks > 1)break;continue;}
if(g == BASE0 +(1 - seat))return cmd_fire(d);
if(g != EMPTY)break;
if(bricks)continue;
int v = scan_occ[c];
if(v >= 0){
if(v / 3 != seat && k <= 2)return cmd_fire(d);
break;
}
}
}
}
const int16_t* dist = hunt_target[i]>= 0 ? hunt_map[hunt_target[i]% 3]:(holding[i]? post_map : atk_of[i]);
int pos = t.pos,dirs[2]={-1,-1};
const int steps =(i % 3 == FAST)? 2 : 1;
for(int k = 0;k < steps;++k){
int best = -1,best_cost = goal_cost(s,i,pos);
if(((r >>(8 + 4 * k))& 15)< 16 / SP_ROLLOUT_RANDOM){
const int d =(r >>(16 + 2 * k))& 3;
if(s.grid[pos + DIR_DELTA[d]]== EMPTY)best = d;
}else{
for(int d = 0;d < 4;++d){
const int to = pos + DIR_DELTA[d];
if(goal_cost(s,i,to)< best_cost){best_cost = goal_cost(s,i,to);best = d;}
}
}
if(best < 0)break;
if(s.grid[pos + DIR_DELTA[best]]== BRICK){
if(k == 0 && armed)return cmd_fire(best);
break;
}
dirs[k]= best;
pos += DIR_DELTA[best];
}
if(dirs[0]< 0)return STAY;
if(dirs[1]< 0)return cmd_move(dirs[0]);
return cmd_move2(dirs[0],dirs[1]);
}
static constexpr int POOL_CAPACITY=32768;
static std::unique_ptr<Node[]> pool;
static int pool_used;
static long long total_steps;
static inline int new_node(){
if(pool_used >= POOL_CAPACITY)return -1;
Node& nd = pool[pool_used];
nd.visits = 0;
nd.total = 0;
nd.n_acts = 0;
return pool_used++;
}
#define ACTOR_BLEND 0.50f
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
static const auto ACT_W0=lossless_model_weights::decode<5120>(
"TAeoPYrDCD7d/Q0+2SmpvpywnD1RKYc9S02SvqacC75hf8w+RvwIPfL7oz6NsJk+sIaavnBfPj5mQDE+DDHNPp7zZ7w66V09AiFY"
"PpuwQb3/oHQ+4mPFOpz0r7xu4kK+ORgYvu8sBz9OBc29Z+0XPoBtt7vTSN695QSsvU8Jnb3uaU++yfasvQ0Iur1uWaE9UEGUvq9B"
"2LvbqFm8/dIfPhXEPL01R3C+plk9PANvvzwaqjO+1ZR8PsLqKL7YECK+xekfvv8OAD0aq0K9+RcgvagVQr447KA8g6aHPj8bej6c"
"Hzc+fnxiPR1yU7w/Y5y9fvAvu+il6jxuPAM9Ho2HvQxBX75VBAI+cCIPPvlLGT48xq69bhifPVNcOT4G31G+OPnvPXZghr4XMHA+"
"6kMDPBoSVb3qJgo+GxKMvnelrr58Qj88dUvAu99YZb3Snco8eanCPdHvxD0l7Fy9GvsDu2eZvbwA1qQ9PniMvdToN7xwP9u7H49G"
"urCUmj3wL8O7yx8YvRHFFT3RQec9aWmQvbjAjD17/hQ9LC8sPCsghDy/G7M7XlyhPdtqATwmtu89AedjPNawAz50POY9ns74vbIo"
"zL2tFNi9dKqRPJBLJr4505O+sWONPvwg3rxilXs9sZyTvQ29HT7TxDI+lvEmPqeywb7LE6I9cch6ve2fwb7eIug9w1EjPFXnCz96"
"s9k9rUDxvebtOTx+q3+8ggbDPQiBEL4AQ6e9PE5LPhfzHT5+2jy9+LOOPmotkj6iHtC8cOcCPhEOsjysznk+3rfCPWZ0Gr5waku+"
"X2UfP31GAr5S5bu+NivOPP85Rz/GeSc/UuyGvo6pFb/g1Iy+DaWHPnxxnD14Xbe8r10XP+QQ6L4n+zk9bPIdvCKvir1JUzO+NcS/"
"Poiutj13ue07oC99vUqow76cH6W+lLgKvFV8cj6Fxme+HjXHvnsqsj1HDC08z2eKvZ6kpL0d6x28VVuuvNwoDj5Dj8Q+DnDIvbc3"
"BL7/TJO9XNKRPG8WKL1yDPO94lsNvvOlsT3RvB8+T60rP9bCmrzRdFQ9cpHSvWK1hz4PkSQ+FPYKPqYi5D1on4q+IUNzvs4coz5A"
"QaS94YGKPTBNvb29ot299RjGPHbZUD4GtwK+xAeePspW6z5qlTQ/ongKP3CtEDw+JjS9WBskvcj/NrzwHA88MeYuvT0GTr0ojTG+"
"HZydPnzsv71ZM5a96v6WvB6vFj4HRcI9Ehu7vR7SwrnGSRs96zTNvYWXLj0H2yu+6H88vRX5hL0jKZy+suervnw9ib0iMkk9BMk/"
"PRQm170bzV09DjNlPV3rt7ru+1u9F2WCvX1g47ylpj++4TLCPSkEvDwbdZ283tmDPey1lj0uf0E9te4kvQvCeT3sEpE9Jh6BvSsw"
"eL17Oic9UJM6vUqXm72sAQE8Q6WePZaL5T3PxcK91LwRvdSmsr099Sy+vxqyvSQwqbxg3fi8m8G2vgLrwT5p4O09oYgbPey/Uj6V"
"ZA09hC9evpWiUj0HU9w9qRqhPYE53zzaqAc+qhzMPYr8Nz0v6ag+7FGMPQ4X0rwZyx890m2JveHthz3mYx67wMsEvhr8Kz6r7EO+"
"rJrpPe+XDj7CX9I+1zKbPa5HBLwWIYo+FT+aPu42H75/pgG+nScuPkq1Lr43N1u9OqrpPdoWKb6u9oY9KFr8vmthGz51geM+3YpR"
"vsacGD15OXI97NBtO781vj5Wiw6/EJwgPlmXvb52JL+9sJMQvprYerpf6Pw58howvh1UIr55Sii+aau3PW2deL3/qII+J+RoPgK9"
"pzq6Tcu9L5ehu/d4U7zp04i+juuwu4U6zb3JM5O9sUV9On5AHz76VAI+DSUfvFE/KD1xVSe+9LbDvu+VAr3u39m+CsD1PRbuZL5j"
"1J++w+hKvryvC76yW+w9gT7ZvoNHnDxPM9W9ACSAPT7QLLxKl+e88pjjvY3ZBr2ADAO9xQ05vRZ6dD0RbkU+FonbPPmZ+T3DXVA/"
"WYIqPxAXgb5PI948DOwdvUl3DL0e/208zw6BvWDuW73YOQ8950c1Pud08T28CDu+7IYJPnu57r2jFFq9U91UPdpvoT6aStQ9HBVh"
"vZevDD49nwA+2l8ivSw3Zb3VqgO9uUykvKtf6b1BB5K8TSUlvGszZ75sFkm9ddpcvQvt8D10TT29za4rvFnHjT0FY6e9BTfXvZka"
"qj0t9Fi80xGNvAcDeL15hKe9GY4ZvQ7HG71Csu46/WJuvdImGT0HX7U89I96vdUvnT3hfyW+soaoPUgsqbxSwBU+rK4tvkf2371q"
"Lsk9pQAQvrKgDr1lEGc+QLXyvfZLEb4SthE+m8KBvUvbsTwUogs+hdgNvTFE/D2DSO89IfPpvRPD3z0F76o9YjLgvqrBzTwDt5q8"
"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"
"YgAaPXyXHz3G1MO8Y3hXvdhTMTz2pZY9gm0SvTZvfz1a1e68ggGNvWa0Qz1rqpA9TVdyu0wOqDx6JpE9C1uRPWe7jT2byBY8DwWN"
"PaG2jr3iMM28YWWGPYKHEz3IX/A74yQMvWrSnr0swQI9z5KLPXgyijxFGXM8lgNWPYWQWb02OwU99kRpPYh8Zz2kDpU8FOohvaAI"
"O7xWL1U9QNAkvAHsgj0w0IK92G1wvRZceD0yPV29mMUdurQaf71Pz7S8XldmPUBbb70Uuji9RuqxvNomVL0uvn29tpOVvGinTb1A"
"sNo8KA5QOxgfs7tYsxQ8+dsWvQwaiL20pcG8I0GavbqpBzwakCc9GGGXvIxTRr0OKgY9GDIePTf5Hr0uKI+987KvvG8hOb3olmQ7"
"5m/avD1YmD10LlW93620PNZOUj0A3Iw+lIZpPqUGpT7aS7Y908ICvOds8LzDDvy8TWVNvlfHyD5QovY8x1R1Pm+KlT6GCo++mJ7V"
"vtKdtj1EWbk+GlMLvdwQkb6VqXs+0XzUvCqyWb4FQuO9ocedPthYlr4KJpM8ImP1vX+pzr7TEF89B3IaPkt6pLx86Eq8rKRPvngT"
"Oj0eHAo9HndpPgRH3z542NO9pxqlPSjR5D26K/w9JKSVvdzokr0M0JU+N8wLvrMoCjzxgxO9Us6TvfClf76s3Ui+eyCSPRb9X7sT"
"2fe81qRevVmsWL5J9qw+O7LYPkbm6D346VE8mKYwu8YJbT18tJY9SEjkurXygT1zdJe9vs5evcR1ij2x03M7+L4XPX6EIb5BZaW9"
"6nNrPuV+Er1wI8E8p/EePSJd6L0cYDG9hYhRuxPlzDy6GlG+nUXJPKjMnL5jpS2+w4ukPnTVET5ulQS8NWuxPdvroT1V8yG9Y1Bn"
"PdxWPr0wu5+9o/2FvHijoL08k3C9U1dLPXgzIb0Gsis9yviaPE0Ptz0BM5w9MauYvUqGZzl6i2s97E0rvQ+eIr04ieq9NnW/vbk3"
"2T0xNrk9I8kNPtASEr4WtN69NtTevVRujT23/9O9IJ8DvVt3K74qayC/fKhCPQK66j0BNEu9h2DSviMmxLwGqZI9F9WOPY9rkb7M"
"xSK+k6sDvmenob0ZMve8cFUovmyEND3ddxs+IZiyPlpLnz1XlCe+Osb7vFayp7sZ6xw+lD6mvdQlYr2EYUm9fhgPv73upL0XFP89"
"Y92PPg6bBL4E7QG9jlvJPXso5D1dXcC9HEBaPn31IL7b/R89pXJjv2HCwr4yQfq+Xn/avbV1Gb4mWPi9VTSyPEtTMrwWPaI+08TQ"
"Pk59hb7S+hY+ReBCvjrucL6x/es+X0mvulECwD23MX08rC6MvWilBr87CW0+JogDP5cJg7364Ie90zFLPqmax712svq9HTssPirw"
"zz0/ohU9AZ1zvFa5rz1YkYC+6GcIvTeDqj00v8I9OZFLPsU9YL3mw1U+gqAxvkKfXrxlGbs+6sByvkWkJj4pGAQ+9FzAPdfxCr6I"
"cIo9PV+ovAZfdL4nd8y9iDBxvrf+Gzui7xA9PQO7vWrMO74as1M959+XPc216j3XueW9o8QwPK9cOz5RmLU9sEMLvEgKSLx40da8"
"H6qJPXLmML00DPC8HoCovDxdAr0Gs2U8oC7+PD877T0J6AU9DnWKPvE5HD4LFk0+oD5RPoeU0r2W0AY9bKODvgMd/r2Ineq89gDi"
"vvxZSTyZfzi9SPy+Pebjmjz1hQK+27hyvaBwrLwlSgA+0IOTvf24Db6dtqQ9UB6dvSFcgT30uxa8aXA7PQuY3L0uZZg9fmq2PWlt"
"jb0pjwC+6CwoPA39oD3EFJ885xWbukQSHD14xRW9gF4Uvafi2D37adC9TpcqPQfonb0F6Di9yjRJPRW6Jb5c5n2+toyzPcWBjL3S"
"zYe9qu0AvTDSWD1OaiK+pLErPh+SNz0VlZa+gfiDvgCquj2BvIw+FKAYPhU9gD1yQE8+BWkhvLQDkT3lisw8H8HJPa2oAr5o8za9"
"FkuOvdipkj20FoO+stu2vVeqED35Qyo+o8hPPixJDbvb9nO8+/vjvc94Bj7frp288dTHvvCIgr1zRiY933XPPl6FGL4Yevs8/h3A"
"u2pWT7/IE5a+EiTuvb2Bc7+vI7G+LcARPTezPL30ZuO9RO7EPsVowr16a9I9krsJPjK07T0Njpg+UY5svXquBzw01gA+JSlePgmn"
"gz7oEjA+B7GzvUHMCD7BQ5Y9ehm7veZqhrx7Uv68TMDjvYgqKr4H78M+SnccP6XcUL4m7uM8qHbFvlOrMT7Suqa7yW5ePVTSjb59"
"rBo9wj4IPU9rnz5FAT+9gUkhPhnuOD76S0+97Ds+PsLzMz5Fshs+IpR+PPgwoL3hy3O9tvHXvJIMUT6XPJo+KE2nPIlwTT6zdQe+"
"0OWiPnvauz3mP4o+t4XRuzkYzj4nVMs+QKQsvra/EL1+sX09UCGuOy6JVr0magu9iLciPDgZ4bwDzas+QuLHvfFSXL6k1l09/R2Y"
"vYu+BL0RBK4+2kt9vhfOCr7BBem97hiIvcNzvL2fz5C7aT0PPiy0FDzmVLs+gLbmPcRpWb7UMRC+tSORPYIabLw/cdK9w5qBvQAl"
"jTxWJZy7m9+GvVMXkbyiiJQ9ItqWvKGzE77F4aA9PXgXvZ9HND17QFS94Tl9vMCgDL6ClVY9vdrivA7QSb0Rqa08mLajPPzp3Twy"
"tzG9QpCXvfSjAD4v5lI97/BbPau+p72xyIE8owf4vZckbL5u7Eo8So2xPVI3E74pNkO+jG4gPqh0fT00BfY+REeYvg7m9j2KFN2+"
"DDs5Pe6ll77XN769jFkPPo/vij0SIMu94l4mvZj1A72sB6e9ADWYvGd5qj2SN+k8vqsMPIpKmD7eimy+WYYQPn78aj5fP4u+2PvA"
"POFDJj4OCqC98guCPoqe5b7M0TY94B/XvhzXCj5tl7O+asmlvfq0Yb4Ls6a+W8oovs0D0T0fD2++XDpzPoeJIL7P0q49r24TPbGE"
"0j0Fdx4+holBPmaBh7s1PaK9j4RcPuAJhr0NqkO9q9kFPq1SpT0bNdI+JignvSqZc7yUQhC+OnZUPfgB+j1aBLa7+8r4PIUKPLx+"
"BUa+c38cPmN0Nr4nHBK7qL5NvqfNGb70W0c9hFRzPc/KLD0jXQs+e/54Prj1O70cIrW9/6XMvZ/yB76dMc29gH9fPQ/rSj5J4QM+"
"wmxgPsRfwj0K/lq9MUhAPsWeqj5L0W0+aP9pPo4aFz7OaAM+QpwfPmEUAT3x+6W92S9GPcSfib6UWpY9AjjOPb+rir4/eoc9Os5L"
"PRgJyLsURoQ8OC0zOSJyKT3U1Dg9PTStPqUQX71fhh++Im3+vWdizzu1Zgu+Vpx4PX+Hpr2s+hE+zs6xPPqKb73aRTo+REhuvqbx"
"NL6nfQy++yt8Pg/nBr6nmFe9AOp9vim4Ar7SvEa9A6fUPBsbLb7T1xS+W0s9PeINJbumgjK94BKDPFoqtbx8Zcc9Jeq7vb9/z7w0"
"e+s6s1fFvZ/JVT2M1x29AVGtvU5T3rv0SQc7ztj8vOQFRTzaFQK+56B5PD89g71vPSO+8vnUvethyjtG1j2+RdUcvokhgz6mmTo+"
"a+m+PbTFGD6C3RO+wkx3Pbs2Db7H86i9hTiIvi9lV745iGs+n1QlvQcwKL42GjM+T4HrPTY/nb6g6O058GJivunJcz3mCK+9+uzp"
"vCzKIb5VVKC9lqnTvcsr5r3e/bo+Du+2PR7SMr2r9sW9niVlvu0CZb1OF6s8sBqCPcGNj74urt++XQN3PiE/nL3OvaA9hytCPgDp"
"mz1vF6q8P7mbvYHOEb9RuV2/6WD+PvbZjj43Fqy9RcG5PV/H/z2WPPY9cQ6Hvx/pLb9kxye/dcpGv6M/Sr/f7Ds+Ic7rPP+LiL3Y"
"xVq+BhCDPvEePr6+DCg+4HcMP8lxLL4gQ4A+YVPLPI9TzTxL7Sa+r/kBv8Rugz6OBKi+EAfEPSgFcz7OqzW+C0utPlWORz3wdIY7"
"fPbMvVbyh7/zrzg+JRfdvfHfzr3LqLc+sn7jvoPgojwmaRS+sdCxPThhF78fP4++Cf6bPTTfuL4roYU+GKo6PSjJQ75LQoq+X7Qk"
"Pk9YA74K4X4+imzAvmK9kD1ZL6+9i9xovfcLDbxbWve+kvFAPadsl70ftZc9mltlPcivz7vcP8G85sZZPe8hpr4bqJe+SejAPoXA"
"rj0opnI+9i/NOyTjZD4NfKw8zdVLPvAFF72tU4q+F8JAvX/5mL6YZgk9TEKnvcxKOr4NWiw+e+TSvdB4mL1NeDu8jpFwPj4ZELzY"
"bjc7CADmPEIFTbwhdBY9VOXlPOSinbxt5+C8CEaQvDb0KjjxTye8uNNbPdnjBb04jYS9eC3iPDzNMz1gdZY9qsnGPcv79Lyaehy9"
"ZElGPL2dqj2HCZE9HZ+QvZpUir2Fi4+9cxodvc4pbT3AY4G+C++GPS2eH76UOzS+P1uXvX0rj74k7j++p2swvrNrFz14xuC9Itn2"
"vQ0pxzxhGsG9iW6OvYcQDr68umy9jj4evb61Zz7OMgM9YY8PvGdlNrxgNrO8Xw2+vffqub2gf2e9CUMqvi1reD3WAIG8AYRLvnPZ"
"j736kGi+j4xoviy0Nr4MDG08Sm0cvk/Vvr2eXR89NVgAvSXzCb5zjAu9zPuivWCExD6bQLC+Vrgtv3/Qfb5ZxCa+t8+ePfusHT5a"
"EBk+OxBtvQGWQrvCl60+iFFBPiwi+D0ihfY9fa/PPkowCj2pqBE9VF2EPvhbJr2a2Uw+CluhPbHY6D69eg+8YeSjvU2dyj3/P+k9"
"31bEPT/m3b2gAAO+bqAGvzhpHbyTEZg+seqsvWeUUz3Aw1A2tg40vO+9kry0q0k+wp9kvkJPB76TOoO8Ofk4PqUXs74nhz6+27uq"
"PCptij567869Q1QFvs9bBb2Q4/A8HB5EvmMXlz7x4UE9fJRVPVA1Xr4Z5hQ+o8+Kvo9kLj7+aCU8O2bPPN1TnD683is+dSAovhDr"
"qDxfDwy9GOFaPJnyUb1sSmu90CJNPOjfcLwYkde9FsqnPlm5lr7re+m9N/fSvEwqBT48miY+lpPBPfjbST2wODY9SAYiPitKDb1q"
"hgo+YO7+vS1Ovr68cEa+gp3iPvNLAj4ArFI9C4ARvhpI/72O9bm82f6NPVzYej2rKGc9bqXfvOaD+TzkqbU8VWoKvHvqOz2NVL69"
"s+iMO8QZvTwixfq8XUslPYGgFj1NiPy8rBqHPbS11DwU4qY9T5+GvbP4Mz4irQc+kbYpPVOBsr3qZ4O+tn44OzCbwr04QO67PWuC"
"vWjBBrggMsC9zGxvvgBp5z4Dy+C8fx6lvRdWVL1DoJ0+c2uOPuDtkj7aQlK+CVyvPsalEjz5s947jZ0RPrB1kTwaIRI/D93juz48"
"DL53WkC+WKyHvlL0hz3ga3u9eEJzOxjapT3I9o898tpMPor9IT6CTbk+lKv1vZWrtb0VHts8LNLqPpO2hT5HQnC93nU9viJ4oD2U"
"VpC9u/1PvN+RRb7BsTe+GyHWPk/s3r7f0vi+VK+MvKMaor4f3sg9b44zPvytrz2XOI6+FhwFPnnkjr0eaMa9QwJ0vlEVKT7ttX29"
"romhvDAZ+z1W1RA+ZyPQvRR2GD5nGBA/+QuovMxpPz6H9B4+5ge7vQR01r3cbji+TLZWPh3y27zVeXK9y9U7PmCiKL6txMI9FHRs"
"vGALQ73/LEq9TjywvlrpVj66Ixq+4cQ3Pbw8mD5pt9a+Nv/OO4WzB70nfnU+TY1Nvub9Xr58+Fo+RRryPZu/Yb4C1Us6AmsXPttK"
"Zr7ygBc9su8LPjTjRD4FyqM9WQ4CPj0ykDpZgSg9rMnUPOdafLzCeoy97oJ4PUBVWr0gS827PAnTvLC3FL1q5pa9Vr0YvbafPz3n"
"XqM86rDmvToZSD5PxG28fQvsPbEEPj7Wk6A92cDfPTxNyL11TqK9tEkgvvU8AL563X++U1q0vctTJT62e02+w70iPrZlW75RYbA9"
"YLOGvSxDTDzLH5W86I6sOz60rDw34T89ZTYlvBlRgz0l/aQ87prevJ/biLoe2+y7o8KNvWAGlT3Tbxe9p7zyPGjGab3vQW69zOyd"
"vVcR3DxQgKu9wBDWO3wdq73Mzl69r+O3vP7o8b2OTwu+dQwFvnRJF72aflE+4DHXOrv8G75cJ5y9BxthPjx5tbwt1u+9wIzVPanC"
"rL5WykU+Eq80vVlPPj4OtIa+r2+xvolp6bwC32m9BhkLPomCYLwOY8u8NL0WPtr9VDyBPyS9gmsJvqNMPL5ReA49IlYBPuN1rj0y"
"IFU+cn4PvQNDmz3i/0M9w8kXPFdUkj52Rpa+q+vLPFG3XT3BHXU+XuqnvuvUpr7yefa93i8DvzRI7r4lotm+Shknv/xeiL5LDIq9"
"MxpRPsz7Cj5aLMu9GzSQvSY1gz5DCeI9yfiBvUMu2r0T70Y+IVXBvU7bEj4OzKk9IB9ZPn6OKz1zGJI9a/epPYQSvb7Dpuu8oxYC"
"PmYu5rzfggo+rpAVPBBhST4ahow8tKIiPYEjmz76WQe/GFtAumk8jT10duc8BgGXvRsn7j2yPjA+fl/ZPWVf0L239hU/7OKnPZ0X"
"qz5CnrY+iAByPjLnzz461wQ9O5sdPuZVq77dLA09qIjKPgF2ez4m98w6DmjhPdKjKz5KpNI7fe0pPjjf5T3eopq9xwo2vZP0IL2L"
"g5c9CBpiPKqMZL2wgGo53oppPUD1l7ttKhY9vOKOPDg4Nj0pofi7tCiXvSc8lz3de1w+lRQavJeFmj6eAVe+qREbPjDMmr1acuo8"
"UZ9iPX9OIb4uZkK+izZRvvTUXjypK8O9dBQaPgFTKb7ekqC9QwQAvhH2XjwcMuI7bEYjPZF1Sz1oI188fGS6vFhyhD0wJeC80lJW"
"PQty8r25mKC8XmFOPDdG57yG0Q8+8cITvBWMQD0BCGk9ccmbPZZuwT2BkMK8wF/wva1NSbzyg/c8Y7OhvIeoNz0MWLY9HR0KPunt"
"bzwaXoy8sydXvv7O9b0li+i9YDDnPXHgC70cHx++1bwmPPvukj4rXTa+r2KZPmrd77tJg9u9Rj+IPW411T2hfik9Kt6MvNGQCz5n"
"D8Y8OIQ1vjrxNz0D2Fk9elP6vAo29z00eqY9+RhdPrRPtr1IHOI9D7vAPihGoD1iiYm+v65YvgVFNT3Ttt093t6OvhCAkzzJ5yC+"
"3OGXPg99DL3OadA9d4ekPUpE8D3FKCa/HxmqvXvEvT3WpFq+Hh8/PROq0z0LBNQ9QteNPifHeL3IT829q4V+PlC2ULyFDv29bN9m"
"PrpSYr1BLA69YR9HPhhlZLzSEzM+fNiyPedKpT5CYVm+ooIrviYiZT6Xr5G8qXt6PLvPgT6N1p69wIB2vJYMpL2QL8c+XdwLPlJ2"
"hr6DzLQ9b5o3PWypKLzkaTs+xiYtPmngg75BX5A92NQVPnuFiL5dtFM+SEp0PqXpqj0h4BU+rLonvsqRMr2br5C+/bq0vZQfoT3p"
"c449037BvQfb5bzf2pk9q6llvSdNxr0Grl06U4ylvBBtnL6zvnO9j0vRPkBNDL0PPZk9gHE0PA3YQr10+Vu90ooRPXl6gz0MGQY+"
"DmI3P0JphL8ifc88plkivcs4Kj3VV2e+LXdHvSDbTz7QtSO91mWDvRlUbz1YMJe9kCk+PUnLjb68YtW9q7NIvadIhr5U3tc8JG6T"
"vSDeGb1lItQ99CQwPVCEdT2E+y87OzYFPdHBt7zIm4E8CwaGPKNOuT0/Zra9S52Mvf8lOb180ao9jnVnPdkwu7w4rlg9Ck9NPQWp"
"4LwZewi8vXPZu1UbPL2I8KK90p1juquCaTw9Yxa8NxLLu/jYXbzKKgC+SPc3vJ0IO75XOGe9NnikvR3jtb2FsZw9+nSmvSmYur2F"
"pgW+In0ePqMQgz10SIC91ufzvROxaz6TJHu+zR5BPQi+Jr10qGI+omqFPfYCn72FWvQ7R2I5vTLgvb2W21a9HeYKvQ7AMT57+uy9"
"nz+EPoso3j7uX/a90cISPoOpEL4kiva7PVrgvqgcZj5uBss78PNEveU4Rr6WU7A+sEOxvlHOcLxr/p6+T9qDPZeCAb8vHcy+2NYU"
"vdSmDb4=");
static const auto ACT_B0=lossless_model_weights::decode<32>(
"UW66O65y57sYdIG9J+JvvdZz2bzEMKi9T3icvRYJarxqAmi99Q93vQC1ob2WhSU8K4e2POUnGr3s4aK8l6kkPD6uKD0gKT89er0B"
"vHlzkz1303q9qEJGPRgWAD03BFW9R6Y/PXTnbT0jp5c9BorGvYToeb2ZFEa9nVz6OtKiDj0=");
static const auto ACT_W1=lossless_model_weights::decode<800>(
"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");
static const auto ACT_B1=lossless_model_weights::decode<25>(
"s0YxPMJ/1jxeFt89hwc4u8KuSDxYhry7heaZPThqAzztwS48Vx62vezj6D2XOBq+seDmvbsXSD3fqcc9TB5nPQYWzT0NXDG9z46i"
"u/B5Y72Jj/k9/zOwvUNX9zzgDh0+MTY6vQ==");
static void actor_forward(const float*x,float*out){
static const auto w0=[]{
std::array<float,5120> w{};
for(int k=0;k<160;k++)for(int j=0;j<32;j++)w[k*32+j]=ACT_W0[j*160+k];
return w;
}();
static const auto w1=[]{
std::array<float,800> w{};
for(int j=0;j<32;j++)for(int a=0;a<25;a++)w[j*25+a]=ACT_W1[a*32+j];
return w;
}();
float h[32];for(int j=0;j<32;j++)h[j]=ACT_B0[j];
for(int k=0;k<160;k++)if(x[k]!=0.f)
for(int j=0;j<32;j++)h[j]+=w0[k*32+j]*x[k];
for(int a=0;a<25;a++)out[a]=ACT_B1[a];
for(int j=0;j<32;j++)if(h[j]>0)
for(int a=0;a<25;a++)out[a]+=w1[j*25+a]*h[j];
for(int a=0;a<25;a++)out[a]=std::max(-6.f,std::min(6.f,out[a]));
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
for(int a=1;a<nd.n_acts;a++)for(int b=a;b>0&&nd.p[b]>nd.p[b-1];b--){
std::swap(nd.p[b],nd.p[b-1]);std::swap(nd.act[b],nd.act[b-1]);
}
}
}
static inline int select_action(const Node& nd){
if(nd.visits == 0)return 0;
const float sq = std::sqrt(float(nd.visits))*(SP_UCB / 100.0f);
const float fpu = nd.total / float(nd.visits)- SP_FPU / 100.0f;
int best = 0;
float best_u = -1e9f;
for(int a = 0;a < nd.n_acts;++a){
const float q = nd.n[a]? nd.sum[a]/ float(nd.n[a]): fpu;
const float u = q + sq * nd.p[a]/ float(1 + nd.n[a]);
if(u > best_u){best_u = u;best = a;}
}
return best;
}
static const int HTSIZE=32768;
struct StateSlot{uint64_t hash;int first;};
static StateSlot state_table[HTSIZE];
static uint64_t mix64(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;return x^(x>>31);}
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
namespace opponent_astra{
static bool model_guard_limited=false;
static double budget_left(){
const double left=model_budget_view::remaining();
if(left<6)model_guard_limited=true;
return left;
}
using namespace tank;
namespace ai{
constexpr float INF=10000;
struct Context{float field[6][CELLS];float grad[6];float attack[2][2][CELLS];float sd[2][MAX_SUPPLY][CELLS];float ed[6][CELLS];int occ[CELLS];float wallval[CELLS];int walls[CELLS],nw;uint64_t wh;int me;}ctx;
int hist[6][CELLS];int opening[6];
unsigned rng=7123671;
unsigned rnd(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int md(int a,int b){return std::abs(cell_x(a)-cell_x(b))+std::abs(cell_y(a)-cell_y(b));}
void dijkstra(const State&s,float* d,bool steel,float speed){
using P=std::pair<float,int>;std::priority_queue<P,std::vector<P>,std::greater<P>>q;
for(int y=0;y<s.P.size;y++)for(int x=0;x<s.P.size;x++){int c=cell_of(x,y);if(d[c]<INF)q.push({d[c],c});}
while(!q.empty()){auto[v,c]=q.top();q.pop();if(v>d[c]+.001)continue;
int gc=s.grid[c];float cost=1/speed+(gc==BRICK?1.8f:gc==STEEL?2.0f:0);
for(int k=0;k<4;k++){int n=c+DIR_DELTA[k],g=s.grid[n];if(g!=EMPTY&&g!=BRICK&&!(g==STEEL&&steel))continue;
float nv=v+cost;if(nv<d[n]){d[n]=nv;q.push({nv,n});}}
}
}
void setup(const State&s,int me){
ctx.me=me;ctx.nw=0;ctx.wh=s.wall_hash;std::fill(ctx.occ,ctx.occ+CELLS,-1);for(int i=0;i<6;i++)if(s.tank[i].alive)ctx.occ[s.tank[i].pos]=i;
for(int seat=0;seat<2;seat++)for(int st=0;st<2;st++){
float* d=ctx.attack[seat][st];std::fill(d,d+CELLS,INF);
int base=s.base_pos[1-seat];
for(int dir=0;dir<4;dir++){float cost=0;int len=0;for(int c=base+DIR_DELTA[dir];s.grid[c]!=OUTSIDE;c+=DIR_DELTA[dir]){
int g=s.grid[c];if(g==BASE0||g==BASE1||(g==STEEL&&!st))break;len++;cost+=(g==BRICK||g==STEEL)?1.9f:0;
if(g!=WATER)d[c]=cost+len*.75f;
int blocker=ctx.occ[c];if(blocker>=0)cost+=(blocker/3==seat?2.0f:6.0f);}}
dijkstra(s,d,st,1);
}
for(int st=0;st<2;st++)for(int k=0;k<s.n_supply;k++){float*d=ctx.sd[st][k];std::fill(d,d+CELLS,INF);d[s.supply[k]]=0;dijkstra(s,d,st,1);}
if(s.turn==0){for(int seat=0;seat<2;seat++){
float best=1e9;for(int a=0;a<3;a++)for(int b=0;b<3;b++)for(int c=0;c<3;c++)if(a!=b&&a!=c&&b!=c){
int z[3]={a,b,c};float sum=0,mx=0;
for(int t=0;t<3;t++){float d=ctx.sd[0][z[t]][s.spawn[seat*3+t]]/(t==0?1.65f:1.f);sum+=d;mx=std::max(mx,d);}
if(sum+.5f*mx<best){best=sum+.5f*mx;for(int t=0;t<3;t++)opening[seat*3+t]=z[t]+1;}
}
}}
for(int j=0;j<6;j++){
float*d=ctx.ed[j];std::fill(d,d+CELLS,INF);if(!s.tank[j].alive)continue;
int p=s.tank[j].pos;d[p]=1;
for(int k=0;k<4;k++)for(int n=1,c=p+DIR_DELTA[k];n<=5;n++,c+=DIR_DELTA[k]){
if(s.grid[c]!=EMPTY&&s.grid[c]!=WATER)break;
if(s.grid[c]==EMPTY)d[c]=n*.65f;
}
dijkstra(s,d,false,1);
}
for(int seat=0;seat<2;seat++){
float util[3][8];
for(int t=0;t<3;t++){int i=3*seat+t;const Tank&a=s.tank[i];int p=a.alive?a.pos:s.spawn[i];float speed=t==0?1.65f:1;
util[t][0]=34-1.6f*ctx.attack[seat][a.level>=2][p]/speed;
for(int k=0;k<s.n_supply;k++){
float dis=ctx.sd[a.level>=2][k][p]/speed;
int wait=(s.star_mask>>k&1)?0:(s.turn<10?11-s.turn:21-(s.turn-10)%20);
float val=33-2.25f*dis-3.6f*std::max(0.0f,wait-dis);
if(a.level>=2)val=-1000;
if(a.level==0)val+=7;else val-=12;
if(t==0)val+=3;
util[t][k+1]=val;
}
for(int e=0;e<3;e++){
int j=(1-seat)*3+e;float bd=ctx.attack[1-seat][s.tank[j].level>=2][s.tank[j].pos];
int home=md(s.tank[j].pos,s.base_pos[seat]);float threat=std::max(0,8-home)*4.5f;
float dis=ctx.ed[j][p]/speed;
util[t][s.n_supply+1+e]=s.tank[j].alive&&home<=7?40+threat-2.5f*dis:-1000;
}
}
int goals[3]={};float best=-1e9;
for(int a=0;a<=s.n_supply+3;a++)for(int b=0;b<=s.n_supply+3;b++)for(int c=0;c<=s.n_supply+3;c++){
if((a&&a==b)||(a&&a==c)||(b&&b==c))continue;
int nd=(a>s.n_supply)+(b>s.n_supply)+(c>s.n_supply);
float v=util[0][a]+util[1][b]+util[2][c]-std::max(0,nd-1)*24;if(v>best){best=v;goals[0]=a;goals[1]=b;goals[2]=c;}}
for(int t=0;t<3;t++){int i=seat*3+t;const Tank&a=s.tank[i];float speed=t==0?1.65f:1;int goal=goals[t];if(s.turn<11||(s.turn<17&&a.level==0&&(s.star_mask>>(opening[i]-1)&1)))goal=opening[i];ctx.grad[i]=(goal==0?1.6f:goal<=s.n_supply?2.25f:2.5f)/speed;
for(int c=0;c<CELLS;c++){
float atk=34-1.6f*std::min(40.f,ctx.attack[seat][a.level>=2][c]/speed);
float v=atk;
if(goal&&goal<=s.n_supply){int k=goal-1;float dis=ctx.sd[a.level>=2][k][c]/speed;v=47-2.25f*std::min(40.f,dis);}
if(goal>s.n_supply){int j=(1-seat)*3+goal-s.n_supply-1;v=50-2.5f*std::min(40.f,ctx.ed[j][c]/speed);}
ctx.field[i][c]=v;
}
}
}
for(int c=0;c<CELLS;c++)if(s.grid[c]==BRICK||s.grid[c]==STEEL){
ctx.walls[ctx.nw++]=c;float vv[2]={};
for(int i=0;i<6;i++)if(s.tank[i].alive){int p=s.tank[i].pos;int d=md(p,c);float gain=ctx.field[i][c]-ctx.field[i][p];
if(d<=6&&gain>0)vv[i/3]=std::max(vv[i/3],std::max(0.f,(gain-ctx.grad[i]*d)*1.2f+.3f));
}
if(md(c,s.base_pos[0])<=2)vv[1]+=7;
if(md(c,s.base_pos[1])<=2)vv[0]+=7;
ctx.wallval[c]=vv[0]-vv[1];
}
}
float bulletval(const State&s,const Bullet&b){
int seat=seat_of(b.owner),pos=b.pos;
float val=0;
for(int n=1;n<=s.P.size+1;n++){
pos+=DIR_DELTA[b.dir];int g=s.grid[pos];if(g==OUTSIDE)break;
if(g==BRICK||(g==STEEL&&b.steel)){
float gain=ctx.wallval[pos]*(seat==0?1:-1);
val=gain;break;
}
if(g==STEEL)break;
if(g==BASE0||g==BASE1){int side=g-BASE0;val=(side!=seat?1.f:-1.f)*135.f/(1+.035f*n);break;}
int hit=tank_at(s,pos);
if(hit>=0){
int free=0;for(int k=0;k<4;k++)if((k/2)!=(b.dir/2)&&s.grid[pos+DIR_DELTA[k]]==EMPTY&&tank_at(s,pos+DIR_DELTA[k])<0)free++;
float p=(n<=2?.22f:.025f);if(free==0)p=(n<=2?.7f:.12f);
if(can_fire(s,hit))p*=.55f;
float damage=16+(s.tank[hit].hp==1?45.f:0);
val=damage*p*(seat_of(hit)==seat?-1.f:1.f);break;
}
}
return seat==0?val:-val;
}
float eval(const State&s){
if(s.done)return s.winner==0?1e6f:s.winner==1?-1e6f:0;
float z=(s.base_hp[0]-s.base_hp[1])*180.f+(s.kills[0]-s.kills[1])*28.f+(s.stars_eaten[0]-s.stars_eaten[1])*2;
for(int i=0;i<6;i++){
const Tank&t=s.tank[i];float v;
if(!t.alive)v=-3*respawn_in(s,i);
else{
v=40+t.hp*15+t.level*8+ctx.field[i][t.pos];
if(can_fire(s,i))v+=1.4f;
int base=s.base_pos[1-i/3];int dir=-1;
if(cell_x(base)==cell_x(t.pos))dir=base>t.pos?DOWN:UP;
if(cell_y(base)==cell_y(t.pos))dir=base>t.pos?RIGHT:LEFT;
if(dir>=0){
bool clear=true;int dis=md(base,t.pos);
for(int c=t.pos+DIR_DELTA[dir];c!=base;c+=DIR_DELTA[dir])if(s.grid[c]==BRICK||s.grid[c]==STEEL||tank_at(s,c)>=0){clear=false;break;}
if(clear)v+=100.f/(1+dis*.35f)*(t.level==0&&i%3!=FIRE?.8f:1.f);
}
for(int j=0;j<6;j++)if(seat_of(j)!=seat_of(i)&&!s.tank[j].alive&&s.spawn[j]==t.pos)v-=respawn_in(s,j)<=1?100:15;
v-=std::min(8,hist[i][t.pos])*.11f;
}
z+=(i<3?v:-v);
}
if(s.wall_hash!=ctx.wh)for(int k=0;k<ctx.nw;k++){int c=ctx.walls[k];if(s.grid[c]==EMPTY)z+=ctx.wallval[c];}
for(int k=0;k<s.n_bullets;k++)z+=bulletval(s,s.bullet[k]);
return z;
}
float value(const State&s,int seat,const Orders&a,const Orders&b){State n=s;step_as(n,seat,a,b);float v=eval(n);return seat==0?v:-v;}
struct Choice{Cmd c;float v;};
struct Pool{std::vector<Orders> a;void add(Orders x){for(auto y:a)if(x==y)return;a.push_back(x);}};
Orders improve(const State&s,int seat,Orders own,const Orders&enemy,std::array<std::vector<Choice>,3>*rank=nullptr){
for(int t=0;t<3;t++){
Cmd cmds[N_CMDS];int n=legal_commands(s,seat*3+t,cmds);float best=-1e20;Cmd bc=0;std::vector<Choice>ls;
for(int k=0;k<n;k++){Orders a=own;a.cmd[t]=cmds[k];float v=value(s,seat,a,enemy);ls.push_back({cmds[k],v});if(v>best+1e-4){best=v;bc=cmds[k];}}
own.cmd[t]=bc;
if(rank){std::sort(ls.begin(),ls.end(),[](Choice a,Choice b){return a.v>b.v;});(*rank)[t]=ls;}
}
return own;
}
}
void init(const State&s,int me){using namespace ai;for(auto &row:hist)std::fill(row,row+CELLS,0);rng=7123671+me;}
Orders decide(const State&s,int me){
using namespace ai;setup(s,me);
for(int i=0;i<6;i++)if(s.tank[i].alive)hist[i][s.tank[i].pos]++;
Orders a,b;
a=improve(s,me,a,b);b=improve(s,1-me,b,a);
Pool pa,pb;pa.add(a);pb.add(b);pb.add(Orders{});
std::array<std::vector<Choice>,3> ra,rb;
for(int it=0;it<3;it++){
a=improve(s,me,a,b,&ra);pa.add(a);
b=improve(s,1-me,b,a,&rb);pb.add(b);
}
for(int t=0;t<3;t++){
for(int k=0;k<std::min(4,(int)ra[t].size());k++){Orders x=a;x.cmd[t]=ra[t][k].c;pa.add(x);}
for(int k=0;k<std::min(4,(int)rb[t].size());k++){Orders x=b;x.cmd[t]=rb[t][k].c;pb.add(x);}
}
for(int k=0;k<9;k++){
Orders x=a,y=b;for(int t=0;t<3;t++){int r=(k/(t==0?1:t==1?3:1)+t)%3;if(r<(int)ra[t].size())x.cmd[t]=ra[t][r].c;if(r<(int)rb[t].size())y.cmd[t]=rb[t][r].c;}pa.add(x);pb.add(y);
}
int na=pa.a.size(),nb=pb.a.size();float mat[32][32];
for(int i=0;i<na;i++)for(int j=0;j<nb;j++)mat[i][j]=value(s,me,pa.a[i],pb.a[j]);
float sa[32]={},sb[32]={};int ca[32]={},cb[32]={};int bi=0,bj=0;
for(int it=0;it<160;it++){
ca[bi]++;cb[bj]++;
for(int i=0;i<na;i++)sa[i]+=mat[i][bj];
for(int j=0;j<nb;j++)sb[j]+=mat[bi][j];
bi=0;bj=0;for(int i=1;i<na;i++)if(sa[i]>sa[bi])bi=i;for(int j=1;j<nb;j++)if(sb[j]<sb[bj])bj=j;
}
int sel=0;float bv=-1e20;
for(int i=0;i<na;i++){
float v=0,w=1e20;for(int j=0;j<nb;j++){v+=mat[i][j]*cb[j]/160.f;w=std::min(w,mat[i][j]);}
v=.85f*v+.15f*w;
if(v>bv){bv=v;sel=i;}
}
std::vector<int> ia,ib;
for(int i=0;i<na;i++)ia.push_back(i);
for(int j=0;j<nb;j++)ib.push_back(j);
float ar[64]={},br[64]={};
for(int i=0;i<na;i++){float w=1e20;for(int j=0;j<nb;j++){ar[i]+=mat[i][j]*cb[j]/160.f;w=std::min(w,mat[i][j]);}ar[i]=ar[i]*.85f+.15f*w;}
for(int j=0;j<nb;j++){float w=-1e20;for(int i=0;i<na;i++){br[j]+=mat[i][j]*ca[i]/160.f;w=std::max(w,mat[i][j]);}br[j]=br[j]*.85f+.15f*w;}
std::sort(ia.begin(),ia.end(),[&](int x,int y){return ar[x]>ar[y];});
std::sort(ib.begin(),ib.end(),[&](int x,int y){return br[x]<br[y];});
ia.erase(std::remove(ia.begin(),ia.end(),sel),ia.end());ia.insert(ia.begin(),sel);
ia.resize(std::min(12,(int)ia.size()));ib.resize(std::min(10,(int)ib.size()));
float deep[12][12];int nr=ia.size(),nc=ib.size();
for(int x=0;x<nr;x++)for(int y=0;y<nc;y++){
if(budget_left()<6)return pa.a[sel];
State n=s;step_as(n,me,pa.a[ia[x]],pb.a[ib[y]]);
float v=eval(n)*(me==0?1:-1)*.25f;
for(int depth=0;depth<2&&!n.done;depth++){
Orders aa,bb;
aa=improve(n,me,aa,bb);bb=improve(n,1-me,bb,aa);
Orders aa2=improve(n,me,aa,bb);
step_as(n,me,aa2,bb);
v+=eval(n)*(me==0?1:-1)*(depth==0?.30f:.45f);
}
deep[x][y]=n.done?eval(n)*(me==0?1:-1):v;
}
std::fill(sa,sa+32,0);std::fill(sb,sb+32,0);std::fill(ca,ca+32,0);std::fill(cb,cb+32,0);bi=bj=0;
for(int it=0;it<200;it++){
ca[bi]++;cb[bj]++;
for(int i=0;i<nr;i++)sa[i]+=deep[i][bj];
for(int j=0;j<nc;j++)sb[j]+=deep[bi][j];
bi=bj=0;for(int i=1;i<nr;i++)if(sa[i]>sa[bi])bi=i;for(int j=1;j<nc;j++)if(sb[j]<sb[bj])bj=j;
}
bv=-1e20;
for(int i=0;i<nr;i++){
float v=0,w=1e20;for(int j=0;j<nc;j++){v+=deep[i][j]*cb[j]/200.f;w=std::min(w,deep[i][j]);}
v=.85f*v+.15f*w;if(v>bv){bv=v;sel=ia[i];}
}
return pa.a[sel];
}
}
namespace opponent_soluf{
static bool model_guard_limited=false;
static double budget_left(){
const double left=model_budget_view::remaining();
if(left<18)model_guard_limited=true;
return left;
}
using namespace tank;
namespace bot{
constexpr float INF=10000;
float bd[6][CELLS],sd[6][MAX_SUPPLY][CELLS],hd[6][3][CELLS],field[6][CELLS];
int goal[6],previous[6];float anchor[6];
uint64_t rng=123456789;
uint64_t random64(){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
float rand01(){return(random64()>>40)*(1.0f/16777216);}
int md(int a,int b){return std::abs(cell_x(a)-cell_x(b))+std::abs(cell_y(a)-cell_y(b));}
int direction(int a,int b){if(a==b)return -1;if(cell_x(a)==cell_x(b))return a>b?0:1;if(cell_y(a)==cell_y(b))return a>b?2:3;return -1;}
bool clear(const State&s,int a,int b){int d=direction(a,b);if(d<0)return false;for(int p=a+DIR_DELTA[d];p!=b;p+=DIR_DELTA[d])if(s.grid[p]!=EMPTY&&s.grid[p]!=WATER)return false;return true;}
void dijkstra(const State&s,int i,float *dist,const std::vector<std::pair<int,float>>& seeds){
for(int p=0;p<CELLS;p++)dist[p]=INF;
using Node=std::pair<float,int>;
std::priority_queue<Node,std::vector<Node>,std::greater<Node>>q;
for(auto[p,v]:seeds)if(v<dist[p]){dist[p]=v;q.emplace(v,p);}
float walk=type_of(i)==FAST?.58f:1.f;
while(!q.empty()){
auto[v,p]=q.top();q.pop();if(v>dist[p]+.001f)continue;
int g=s.grid[p];float extra=g==BRICK?2.f:g==STEEL?3.f:0.f;
for(int d=0;d<4;d++){
int n=p+DIR_DELTA[d];if(n<0||n>=CELLS)continue;
int ng=s.grid[n];if(ng==OUTSIDE||ng==WATER||ng==BASE0||ng==BASE1||(ng==STEEL&&s.tank[i].level<2))continue;
float nv=v+walk+extra;
if(nv<dist[n]){dist[n]=nv;q.emplace(nv,n);}
}
}
}
void prepare(const State&s){
for(int i=0;i<6;i++){
int side=seat_of(i),base=s.base_pos[1-side];std::vector<std::pair<int,float>> seeds;
for(int d=0;d<4;d++){
int walls=0,range=0;
for(int p=base+DIR_DELTA[d];p>=0&&p<CELLS;p+=DIR_DELTA[d]){
int g=s.grid[p];if(g==OUTSIDE||g==BASE0||g==BASE1||(g==STEEL&&s.tank[i].level<2))break;
range++;if(g==BRICK||g==STEEL)walls++;
if(g!=WATER)seeds.emplace_back(p,range*(type_of(i)==FAST?1.45f:.85f)+walls*2.0f+((type_of(i)==FAST&&d<2)?5.f:0.f));
}
}
dijkstra(s,i,bd[i],seeds);
for(int k=0;k<s.n_supply;k++)dijkstra(s,i,sd[i][k],{{s.supply[k],0}});
}
for(int i=0;i<6;i++)for(int t=0;t<3;t++){
int j=(1-seat_of(i))*3+t,p=s.tank[j].pos;
std::vector<std::pair<int,float>> seeds;
if(s.tank[j].alive)for(int d=0;d<4;d++)for(int range=1;range<=5;range++){
int q=p+DIR_DELTA[d]*range;if(q<0||q>=CELLS||s.grid[q]==OUTSIDE)break;
int g=s.grid[q];if(g==BASE0||g==BASE1)break;if(g==STEEL&&s.tank[i].level<2)break;
if(g==EMPTY||g==BRICK||g==STEEL)seeds.emplace_back(q,1.0f*range+(g==EMPTY?0.f:2.f));
if(g==BRICK||g==STEEL)break;
}
dijkstra(s,i,hd[i][t],seeds);
}
for(int side=0;side<2;side++){
float options[3][7];int next=s.turn<10?10-s.turn:20-(s.turn-10)%20;
for(int t=0;t<3;t++){
int i=side*3+t;int p=s.tank[i].alive?s.tank[i].pos:s.spawn[i];
options[t][0]=-bd[i][p]*.65f;
for(int k=0;k<s.n_supply;k++){
float wait=(s.star_mask&(1<<k))?0.f:float(next+1);
float eta=sd[i][k][p],enemy=INF;
for(int j=(1-side)*3;j<(1-side)*3+3;j++)if(s.tank[j].alive&&s.tank[j].level<2)enemy=std::min(enemy,sd[j][k][s.tank[j].pos]);
float value=(s.tank[i].level==0?17.f:0.f)-1.5f*std::max(eta,wait);
if(enemy+.5f<eta)value-=std::min(18.f,(eta-enemy)*(t==0?4.f:t==2?1.f:2.f));
if(!s.tank[i].alive)value=-INF;
if(s.tank[i].level>=2)value=-INF;
options[t][k+1]=value;
}
for(int e=0;e<3;e++){
int j=(1-side)*3+e;
float threat=-INF;
if(s.tank[j].alive){
int dist=md(s.tank[j].pos,s.base_pos[side]);
if(dist<=7){threat=27.f-2.f*dist+3.f*s.tank[j].level;
if(clear(s,s.tank[j].pos,s.base_pos[side]))threat+=18.f;
}
}
options[t][4+e]=threat-1.5f*hd[i][e][p]-(t==1?8.f:0.f);
if(t==0||!s.tank[i].alive)options[t][4+e]=-INF;
}
}
float best=-INF*10;int choice[3]={};
for(int a=0;a<7;a++)for(int b=0;b<7;b++)for(int c=0;c<7;c++){
if((a&&a==b)||(a&&a==c)||(b&&b==c))continue;
float v=options[0][a]+options[1][b]+options[2][c];
if(v>best){best=v;choice[0]=a;choice[1]=b;choice[2]=c;}
}
for(int t=0;t<3;t++){
int i=side*3+t;goal[i]=choice[t]-1;
for(int p=0;p<CELLS;p++)field[i][p]=goal[i]<0?-bd[i][p]:goal[i]<3?-sd[i][goal[i]][p]:-hd[i][goal[i]-3][p];
}
}
}
float projectile(const State&s,const Bullet&b){
int side=seat_of(b.owner),p=b.pos,steps=0;
for(;;){
if(p<0||p>=CELLS)return 0;
int g=s.grid[p];
if(g==OUTSIDE||g==BRICK||g==STEEL)return 0;
if(g==BASE0||g==BASE1){
float value=60.f/(1.f+.08f*steps);
for(int j=0;j<6;j++)if(seat_of(j)!=side&&s.tank[j].alive){int dd=md(s.tank[j].pos,p);if(dd<steps/2+2)value*=.72f;}
return(g-BASE0==side?-1.f:1.f)*value;
}
for(int j=0;j<6;j++)if(s.tank[j].alive&&s.tank[j].pos==p){
int escapes=0;for(int d=0;d<4;d++){int n=p+DIR_DELTA[d];if(s.grid[n]==EMPTY&&tank_at(s,n)<0)escapes++;}
float value=(s.tank[j].hp==1?19.f:9.f)/(1.f+.35f*steps);
if(steps>0)value*=escapes? .3f:.9f;
if(steps>0&&can_fire(s,j)&&direction(p,b.pos)>=0)value*=.6f;
return(seat_of(j)==side?-1.f:1.f)*value;
}
p+=DIR_DELTA[b.dir];steps++;
}
}
float evaluate(const State&s,int side){
if(s.done)return s.winner<0?0.f:(s.winner==side?100000.f:-100000.f);
float v=140.f*(s.base_hp[side]-s.base_hp[1-side])+30.f*(s.kills[side]-s.kills[1-side])+4.f*(s.stars_eaten[side]-s.stars_eaten[1-side]);
for(int i=0;i<6;i++){
auto&t=s.tank[i];float val;
if(t.alive){
val=34.f+11.f*t.hp+14.f*t.level;
val+=5.f*(std::max(-45.f,field[i][t.pos])-anchor[i]);
if(clear(s,t.pos,s.base_pos[1-seat_of(i)]))val+=25.f/(1.f+.18f*md(t.pos,s.base_pos[1-seat_of(i)]));
}else val=-4.f*respawn_in(s,i)-20.f;
v+=(seat_of(i)==side?1.f:-1.f)*val;
}
for(int k=0;k<s.n_bullets;k++)v+=(seat_of(s.bullet[k].owner)==side?1.f:-1.f)*projectile(s,s.bullet[k]);
return v;
}
void path(const State&s,int i,Cmd c,int&p1,int&p2){
p1=p2=s.tank[i].pos;
int d1=-1,d2=-1;if(is_move(c))d1=c-1;else if(is_move2(c)){d1=(c-9)/4;d2=(c-9)%4;}
if(d1>=0){int n=p1+DIR_DELTA[d1];if(s.grid[n]==EMPTY&&tank_at(s,n)<0)p1=n;}
p2=p1;if(d2>=0){int n=p2+DIR_DELTA[d2];if(s.grid[n]==EMPTY&&tank_at(s,n)<0)p2=n;}
}
float shot_value(const State&s,int i,int d){
int p=s.tank[i].pos,side=seat_of(i),steps=0;
for(int n=p+DIR_DELTA[d];;n+=DIR_DELTA[d]){
steps++;int g=s.grid[n];if(g==OUTSIDE)return 0;
if(g==BASE0||g==BASE1)return g-BASE0==side?-200.f:70.f/(1.f+.08f*steps);
int j=tank_at(s,n);
if(j>=0){
if(seat_of(j)==side)return -25.f/(1.f+.1f*steps);
int exits=0;for(int k=0;k<4;k++)if(s.grid[n+DIR_DELTA[k]]==EMPTY&&tank_at(s,n+DIR_DELTA[k])<0)exits++;
float value=(s.tank[j].hp==1?20.f:13.f)/(1+.35f*steps);
if(exits>1)value*=.65f;
if(can_fire(s,j))value*=.65f;
return value;
}
if(g==BRICK||g==STEEL){
if(g==STEEL&&s.tank[i].level<2)return 0;
float gain=field[i][n]-field[i][p];
float value=std::max(0.f,2.0f*gain-1.f)/(1.f+.2f*steps);
int base=s.base_pos[1-side];
if(direction(p,base)==d){
bool line=true;for(int q=n+DIR_DELTA[d];q!=base;q+=DIR_DELTA[d]){if(s.grid[q]==OUTSIDE||(s.grid[q]==STEEL&&s.tank[i].level<2)){line=false;break;}}
if(line)value+=12.f/(1.f+.1f*md(p,base));
}
int own=s.base_pos[side];if(md(n,own)<=2&&md(p,own)<6)value-=8.f;
return value;
}
}
}
float primitive(const State&s,int i,Cmd c){
if(!s.tank[i].alive)return 0;
int p=s.tank[i].pos,p1,p2;path(s,i,c,p1,p2);
float val=3.2f*(field[i][p2]-field[i][p]);
bool fire=is_fire(c)&&can_fire(s,i);int fd=fire?c-5:-1;
if(fire)val+=shot_value(s,i,fd);
if(c==STAY)val-=.12f;
if(p2==p&&is_move(c))val-=.4f;
int damage=0;
for(int k=0;k<s.n_bullets;k++){
const auto&b=s.bullet[k];int n=b.pos+DIR_DELTA[b.dir],nn=n+DIR_DELTA[b.dir];
bool cancel=false;
if(fire){int f1=p+DIR_DELTA[fd],f2=f1+DIR_DELTA[fd];cancel=(f1==n)||(f1==b.pos&&n==p)||(f2==nn)||(f2==n&&nn==f1);}
if(cancel)continue;
bool hit=n==p1||(n==p&&b.pos==p1);
if(!hit&&s.grid[n]!=BRICK&&s.grid[n]!=STEEL&&s.grid[n]!=OUTSIDE&&tank_at(s,n)<0)hit=nn==p2||(nn==p1&&n==p2);
if(hit)damage++;
}
if(damage)val-=damage*17.f+(damage>=s.tank[i].hp?65.f+15*s.tank[i].level:0.f);
for(int j=0;j<6;j++)if(seat_of(i)!=seat_of(j)&&s.tank[j].alive&&can_fire(s,j)){
int e=s.tank[j].pos;
for(int d=0;d<4;d++){
int n=e+DIR_DELTA[d],nn=n+DIR_DELTA[d];
bool hit=(n==p1)||(nn==p2&&s.grid[n]!=BRICK&&s.grid[n]!=STEEL&&tank_at(s,n)<0)||(p1==e&&n==p);
if(hit){if(fire&&fd==(d^1)&&direction(p,e)==fd)continue;val-=(s.tank[i].hp==1?17.f:7.f);}
}
}
for(int j=0;j<6;j++)if(seat_of(j)!=seat_of(i)&&!s.tank[j].alive&&s.spawn[j]==p2&&respawn_in(s,j)<=1)val-=200;
if(goal[i]>=0&&goal[i]<3&&p2==s.supply[goal[i]]&&s.tank[i].level<2&&(s.star_mask&(1<<goal[i])))val+=28;
if(p2==previous[i]&&p2!=p)val-=.18f;
return val;
}
struct Rated{Cmd c;float v;};
struct Plan{Orders o;float v;};
std::vector<Rated> ranked(const State&s,int i){
Cmd a[N_CMDS];int n=legal_commands(s,i,a);std::vector<Rated>r;
for(int k=0;k<n;k++)r.push_back({a[k],primitive(s,i,a[k])});
std::stable_sort(r.begin(),r.end(),[](auto&a,auto&b){return a.v>b.v;});return r;
}
Orders policy_from_r(const State&s,int side,const std::vector<Rated>* rs){
Orders o;int p1[3],p2[3];
for(int t=0;t<3;t++){
int i=side*3+t;const auto&r=rs[t];float best=-INF;
for(auto x:r){int a,b;path(s,i,x.c,a,b);float v=x.v;
for(int j=0;j<t;j++)if(s.tank[side*3+j].alive&&s.tank[i].alive){if(a==p1[j])v-=20;if(b==p2[j])v-=20;}
if(v>best){best=v;o.cmd[t]=x.c;p1[t]=a;p2[t]=b;}
}
}
return o;
}
Orders policy(const State&s,int side){
std::vector<Rated> rs[3];for(int t=0;t<3;t++)rs[t]=ranked(s,side*3+t);
return policy_from_r(s,side,rs);
}
std::vector<Orders> plans(const State&s,int side){
std::vector<Rated> r[3];for(int t=0;t<3;t++)r[t]=ranked(s,side*3+t);
std::vector<Plan> all;
for(int a=0;a<std::min(4,(int)r[0].size());a++)for(int b=0;b<std::min(4,(int)r[1].size());b++)for(int c=0;c<std::min(4,(int)r[2].size());c++){
Orders o;o.cmd[0]=r[0][a].c;o.cmd[1]=r[1][b].c;o.cmd[2]=r[2][c].c;
int p1[3],p2[3];float v=r[0][a].v+r[1][b].v+r[2][c].v;
for(int t=0;t<3;t++)path(s,side*3+t,o.cmd[t],p1[t],p2[t]);
for(int t=0;t<3;t++)for(int j=t+1;j<3;j++)if(s.tank[side*3+t].alive&&s.tank[side*3+j].alive){if(p1[t]==p1[j])v-=20;if(p2[t]==p2[j])v-=20;}
all.push_back({o,v});
}
std::stable_sort(all.begin(),all.end(),[](auto&a,auto&b){return a.v>b.v;});
std::vector<Orders>out;
auto add=[&](Orders o){for(auto q:out)if(q==o)return;out.push_back(o);};
add(policy_from_r(s,side,r));for(int k=0;k<std::min(14,(int)all.size());k++)add(all[k].o);
Orders best=out[0];
for(int t=0;t<3;t++)for(int a=0;a<std::min(7,(int)r[t].size());a++){Orders o=best;o.cmd[t]=r[t][a].c;add(o);}
add(Orders{});return out;
}
std::vector<Orders> smallplans(const State&s,int side){
std::vector<Rated> rs[3];for(int t=0;t<3;t++)rs[t]=ranked(s,side*3+t);
Orders base=policy_from_r(s,side,rs);std::vector<Orders> a{base};
for(int t=0;t<3;t++){
const auto&r=rs[t];int got=0;
for(auto x:r)if(x.c!=base.cmd[t]){Orders q=base;q.cmd[t]=x.c;if(std::find(a.begin(),a.end(),q)==a.end())a.push_back(q);if(++got>=2)break;}
}
return a;
}
float subgame(const State&s,int side){
if(s.done)return evaluate(s,side);
auto a=smallplans(s,side),b=smallplans(s,1-side);int n=a.size(),m=b.size();
float M[8][8];for(int i=0;i<n;i++)for(int j=0;j<m;j++){State z=s;step_as(z,side,a[i],b[j]);M[i][j]=evaluate(z,side);}
double u[8]={},v[8]={},ca[8]={},cb[8]={};int x=0,y=0;
for(int it=0;it<160;it++){
if(it>=40){ca[x]++;cb[y]++;}
for(int i=0;i<n;i++)u[i]+=M[i][y];
for(int j=0;j<m;j++)v[j]+=M[x][j];
x=std::max_element(u,u+n)-u;y=std::min_element(v,v+m)-v;
}
double ans=0;for(int i=0;i<n;i++)for(int j=0;j<m;j++)ans+=ca[i]*cb[j]*M[i][j];return ans/14400;
}
struct GameMix{std::vector<double> a,b;};
GameMix solvegame(const std::vector<std::vector<float>>&M){
int n=M.size(),m=M[0].size();
std::vector<double>u(n,0),v(m,0),ca(n,0),cb(m,0);int x=0,y=0;
for(int it=0;it<1200;it++){
if(it>=300){ca[x]++;cb[y]++;}
for(int j=0;j<n;j++)u[j]+=M[j][y];
for(int j=0;j<m;j++)v[j]+=M[x][j];
x=std::max_element(u.begin(),u.end())-u.begin();y=std::min_element(v.begin(),v.end())-v.begin();
}
for(double &x:ca)x/=900;
for(double &x:cb)x/=900;
return{ca,cb};
}
uint64_t code(Orders o){return o.cmd[0]|(uint64_t(o.cmd[1])<<8)|(uint64_t(o.cmd[2])<<16);}
Orders choose_oracle(const State&s,int side){
prepare(s);for(int i=0;i<6;i++)anchor[i]=std::max(-45.f,field[i][s.tank[i].alive?s.tank[i].pos:s.spawn[i]]);auto a=plans(s,side),b=plans(s,1-side);
std::unordered_map<uint64_t,float>cache;cache.reserve(4000);
auto ev=[&](Orders a,Orders b){
uint64_t key=code(a)|(code(b)<<24);auto it=cache.find(key);if(it!=cache.end())return it->second;
State z=s;step_as(z,side,a,b);float first=evaluate(z,side);
for(int h=0;h<2&&!z.done;h++){Orders u=policy(z,side),v=policy(z,1-side);step_as(z,side,u,v);}
float ans=.3f*first+.7f*evaluate(z,side);cache.emplace(key,ans);return ans;
};
GameMix mix;
for(int round=0;round<3;round++){
std::vector<std::vector<float>>M(a.size(),std::vector<float>(b.size()));
for(int x=0;x<(int)a.size();x++)for(int y=0;y<(int)b.size();y++)M[x][y]=ev(a[x],b[y]);
mix=solvegame(M);if(round==2||budget_left()<18)break;
std::vector<std::pair<double,Orders>>sa,sb;
for(int x=0;x<(int)a.size();x++)if(mix.a[x]>.001)sa.emplace_back(mix.a[x],a[x]);
for(int y=0;y<(int)b.size();y++)if(mix.b[y]>.001)sb.emplace_back(mix.b[y],b[y]);
auto prep=[](auto&v){std::sort(v.begin(),v.end(),[](auto&a,auto&b){return a.first>b.first;});if(v.size()>6)v.resize(6);double sum=0;for(auto &x:v)sum+=x.first;for(auto &x:v)x.first/=sum;};prep(sa);prep(sb);
auto response=[&](int who,Orders best,const std::vector<std::pair<double,Orders>>&support){
auto val=[&](Orders o){double v=0;for(auto &[p,e]:support)v+=p*(who==side?ev(o,e):-ev(e,o));return v;};
double current=val(best);
for(int pass=0;pass<2;pass++)for(int t=0;t<3;t++){
if(budget_left()<7)return best;
Cmd list[N_CMDS];int n=legal_commands(s,who*3+t,list);Orders old=best;
for(int c=0;c<n;c++){Orders trial=old;trial.cmd[t]=list[c];double v=val(trial);if(v>current+.001){current=v;best=trial;}}
}
return best;
};
int ai=std::max_element(mix.a.begin(),mix.a.end())-mix.a.begin(),bi=std::max_element(mix.b.begin(),mix.b.end())-mix.b.begin();
Orders ar=response(side,a[ai],sb),br=response(1-side,b[bi],sa);
if(std::find(a.begin(),a.end(),ar)==a.end())a.push_back(ar);
if(std::find(b.begin(),b.end(),br)==b.end())b.push_back(br);
}
double target=rand01(),sum=0;for(int x=0;x<(int)a.size();x++){sum+=mix.a[x];if(sum>=target)return a[x];}
return a.back();
}
Orders choose_nested(const State&s,int side){
prepare(s);for(int i=0;i<6;i++)anchor[i]=std::max(-45.f,field[i][s.tank[i].alive?s.tank[i].pos:s.spawn[i]]);
auto a=plans(s,side),b=plans(s,1-side);
int n=a.size(),m=b.size();std::vector<float>matrix(n*m);
for(int x=0;x<n;x++)for(int y=0;y<m;y++){
if((y&3)==0&&budget_left()<5)return policy(s,side);
State z=s;step_as(z,side,a[x],b[y]);float first=evaluate(z,side);
matrix[x*m+y]=.15f*first+.85f*subgame(z,side);
}
std::vector<double>u(n,0),v(m,0),counts(n,0),opp(m,0);int x=0,y=0;
for(int it=0;it<220;it++){
counts[x]++;opp[y]++;
for(int j=0;j<n;j++)u[j]+=matrix[j*m+y];
for(int j=0;j<m;j++)v[j]+=matrix[x*m+j];
x=std::max_element(u.begin(),u.end())-u.begin();y=std::min_element(v.begin(),v.end())-v.begin();
}
double target=rand01()*220,acc=0;int chosen=n-1;
for(int j=0;j<n;j++){acc+=counts[j];if(acc>target){chosen=j;break;}}
return a[chosen];
}
Orders choose(const State&s,int side){
bool combat=false;
for(int i=side*3;i<side*3+3;i++)if(s.tank[i].alive){
for(int j=(1-side)*3;j<(1-side)*3+3;j++)if(s.tank[j].alive){
int d=md(s.tank[i].pos,s.tank[j].pos);
if(d<=3||(d<=6&&clear(s,s.tank[i].pos,s.tank[j].pos)))combat=true;
}
for(int k=0;k<s.n_bullets;k++)if(md(s.bullet[k].pos,s.tank[i].pos)<=4)combat=true;
}
return combat?choose_nested(s,side):choose_oracle(s,side);
}
}
void init(const State&s,int me){bot::rng=0x7d2543a71b9ULL^s.wall_hash^(me*987654321ULL);for(int i=0;i<6;i++)bot::previous[i]=-1;}
Orders decide(const State&s,int me){Orders o=bot::choose(s,me);for(int i=0;i<6;i++)bot::previous[i]=s.tank[i].alive?s.tank[i].pos:-1;return o;}
}
namespace ensemble_models{
static int active=-1;
static bool guard_limited[4]{};
static double private_budget_left(){
const double left=model_budget_view::remaining();
const double threshold[4]={12,25,8,4};
if(active>=0&&active<4&&left<threshold[active])guard_limited[active]=true;
return left;
}
}
namespace ensemble_models::sol{
using namespace tank;
namespace{
constexpr float INF=10000;
struct Context{const State* root;float attack[6][CELLS],supply[6][MAX_SUPPLY][CELLS],wall[2][CELLS],defend[6][CELLS];int goal[6],threat[6];};
static Context ctx;
static int priorGoal[6],visits[6][CELLS];
float walkCost(const State&s,int i,int c){int g=s.grid[c];float v=i%3==FAST?.56f:1.f;if(g==EMPTY)return v;if(g==BRICK||(g==STEEL&&s.tank[i].level>=2))return v+3.4f;return INF;}
void flood(const State&s,int i,float*dis){using Node=std::pair<float,int>;std::priority_queue<Node,std::vector<Node>,std::greater<Node>>q;for(int y=0;y<s.P.size;y++)for(int x=0;x<s.P.size;x++){int c=cell_of(x,y);if(dis[c]<INF)q.push({dis[c],c});}while(!q.empty()){auto[v,c]=q.top();q.pop();if(v!=dis[c])continue;float cost=walkCost(s,i,c);if(cost>=INF)continue;for(int d=0;d<4;d++){int n=c+DIR_DELTA[d];if(walkCost(s,i,n)>=INF)continue;float z=v+cost;if(z<dis[n]){dis[n]=z;q.push({z,n});}}}}
void prepare(const State&s){ctx.root=&s;for(int i=0;i<6;i++){float*a=ctx.attack[i];std::fill(a,a+CELLS,INF);int base=s.base_pos[1-i/3];for(int d=0;d<4;d++){float walls=0;int r=0;for(int c=base+DIR_DELTA[d];s.grid[c]!=OUTSIDE;c+=DIR_DELTA[d]){r++;int g=s.grid[c];if(g==BASE0||g==BASE1||(g==STEEL&&s.tank[i].level<2))break;int j=tank_at(s,c);
if((g==EMPTY||g==BRICK||g==STEEL)&&(j<0||j==i))a[c]=walls+.85f*r;
if(g==BRICK||g==STEEL)walls+=3.4f;
if(j>=0&&j!=i)walls+=j/3==i/3?2.f:5.f+1.5f*s.tank[j].hp;}}flood(s,i,a);for(int k=0;k<s.n_supply;k++){float*b=ctx.supply[i][k];std::fill(b,b+CELLS,INF);b[s.supply[k]]=0;flood(s,i,b);}}
int wait=s.turn<10?10-s.turn:20-(s.turn-10)%20;
for(int seat=0;seat<2;seat++){float best=-1e9;int bg[3]={-1,-1,-1};for(int a=-1;a<s.n_supply;a++)for(int b=-1;b<s.n_supply;b++)for(int c=-1;c<s.n_supply;c++){int gs[3]={a,b,c};if((a>=0&&a==b)||(a>=0&&a==c)||(b>=0&&b==c))continue;float v=0;bool valid=true;for(int t=0;t<3;t++){int i=3*seat+t,k=gs[t];const auto&tk=s.tank[i];int p=tk.alive?tk.pos:s.spawn[i];float av=18-1.35f*ctx.attack[i][p];if(k<0){v+=av*(t==0?2.5f:1.f);continue;}if(tk.level>=2||(t==0&&tk.level>=1)){valid=false;break;}float dis=ctx.supply[i][k][p];int w=(s.star_mask>>k&1)?0:wait;float sv=(tk.level==0?30.f:24.f)-2.4f*dis-(s.turn<10?.7f:2.4f)*std::max(0.f,float(w)-dis)-.6f*dis;if(priorGoal[i]==k)sv+=1.1f;if(!tk.alive)sv-=4;v+=sv*(t==0?2.5f:1.f);}if(valid&&v>best){best=v;for(int t=0;t<3;t++)bg[t]=gs[t];}}for(int t=0;t<3;t++)ctx.goal[3*seat+t]=bg[t];}
for(int seat=0;seat<2;seat++){
std::vector<std::pair<float,int>> threats;
for(int i=3*seat;i<3*seat+3;i++)ctx.threat[i]=-1;
for(int j=3*(1-seat);j<3*(1-seat)+3;j++)if(s.tank[j].alive){
int p=s.tank[j].pos,base=s.base_pos[seat],dy=std::abs(cell_y(p)-cell_y(base));float severity=8-dy;
if(cell_x(p)==cell_x(base)||cell_y(p)==cell_y(base)){
int de=cell_x(p)==cell_x(base)?(base>p?STRIDE:-STRIDE):(base>p?1:-1);int walls=0;
for(int c=p+de;c!=base;c+=de){if(s.grid[c]==BRICK)walls++;if(s.grid[c]==STEEL)walls+=5;}if(walls<=1)severity+=9;
}if(dy<8&&severity>0)threats.push_back({severity,j});
}std::sort(threats.rbegin(),threats.rend());
for(int k=0;k<std::min(2,int(threats.size()));k++){
int threat=threats[k].second;if(k>0&&threats[k].first<4)break;
int besti=-1;float bestcost=1e9;
for(int i=3*seat;i<3*seat+3;i++)if(ctx.goal[i]!=-2){
float dis[CELLS];std::fill(dis,dis+CELLS,INF);int target=s.tank[threat].pos;
for(int d=0;d<4;d++){float walls=0;for(int c=target+DIR_DELTA[d],r=1;r<=8;c+=DIR_DELTA[d],r++){
int g=s.grid[c];if(g==OUTSIDE||g==BASE0||g==BASE1||(g==STEEL&&s.tank[i].level<2))break;
if(g==BRICK||g==STEEL)walls+=3.4f;if(g==EMPTY)dis[c]=walls+.6f*r;
}}flood(s,i,dis);int p=s.tank[i].alive?s.tank[i].pos:s.spawn[i];float cost=dis[p];
if(!s.tank[i].alive)cost+=respawn_in(s,i);if(ctx.attack[i][p]<2.5f&&s.tank[i].level>=1)cost+=16;if(i%3==FAST)cost+=20;
if(cost<bestcost){bestcost=cost;besti=i;std::copy(dis,dis+CELLS,ctx.defend[i]);}
}if(besti>=0&&bestcost<20){ctx.goal[besti]=-2;ctx.threat[besti]=threat;}
}
}
for(int seat=0;seat<2;seat++){std::fill(ctx.wall[seat],ctx.wall[seat]+CELLS,0);for(int i=3*seat;i<3*seat+3;i++){int p=s.tank[i].alive?s.tank[i].pos:s.spawn[i];const float*dis=ctx.goal[i]==-2?ctx.defend[i]:(ctx.goal[i]<0?ctx.attack[i]:ctx.supply[i][ctx.goal[i]]);for(int d=0;d<4;d++){int n=p+DIR_DELTA[d];if(s.grid[n]==BRICK||(s.grid[n]==STEEL&&s.tank[i].level>=2))ctx.wall[seat][n]=std::max(ctx.wall[seat][n],std::max(0.f,dis[p]-dis[n])*.85f);}int base=ctx.goal[i]==-2?s.tank[ctx.threat[i]].pos:s.base_pos[1-seat];if(cell_x(p)==cell_x(base)||cell_y(p)==cell_y(base)){int delta=cell_x(p)==cell_x(base)?(base>p?STRIDE:-STRIDE):(base>p?1:-1);for(int c=p+delta;c!=base;c+=delta){if(s.grid[c]==OUTSIDE||(s.grid[c]==STEEL&&s.tank[i].level<2))break;if(s.grid[c]==BRICK||s.grid[c]==STEEL)ctx.wall[seat][c]=std::max(ctx.wall[seat][c],ctx.goal[i]==-2?12.f:4.5f);}}}}}
float position(const State&s,int i){const auto&t=s.tank[i];if(!t.alive)return -3.f*respawn_in(s,i);float v=18-1.35f*ctx.attack[i][t.pos];int k=ctx.goal[i];if(k==-2)v=60-4.2f*ctx.defend[i][t.pos];if(k>=0&&t.level<2&&t.level==ctx.root->tank[i].level)v=(t.level==0?30.f:24.f)-2.4f*ctx.supply[i][k][t.pos];v=std::max(-35.f,v);if(t.pos!=ctx.root->tank[i].pos)v-=std::min(2.f,visits[i][t.pos]*.12f);for(int j=3*(1-i/3);j<3*(1-i/3)+3;j++)if(!s.tank[j].alive&&s.spawn[j]==t.pos&&respawn_in(s,j)<=2)v-=280;return v;}
float evaluation(const State&s,int me){if(s.done)return s.winner<0?0:(s.winner==me?100000.f:-100000.f);float vals[2]={};for(int seat=0;seat<2;seat++){vals[seat]=70.f*s.base_hp[seat]+10.f*s.kills[seat]+1.5f*s.stars_eaten[seat];for(int i=3*seat;i<3*seat+3;i++){const Tank&t=s.tank[i];if(t.alive)vals[seat]+=35+48.f*t.level+18.f*t.hp+position(s,i);else vals[seat]+=position(s,i);}}
int occ[CELLS];std::fill(occ,occ+CELLS,-1);for(int i=0;i<6;i++)if(s.tank[i].alive)occ[s.tank[i].pos]=i;for(int b=0;b<s.n_bullets;b++){const auto&bl=s.bullet[b];int seat=bl.owner/3;for(int c=bl.pos+DIR_DELTA[bl.dir],r=1;r<=s.P.size+1;c+=DIR_DELTA[bl.dir],r++){int g=s.grid[c];if(g==OUTSIDE)break;if(g==BRICK||g==STEEL){if(g==BRICK||bl.steel)vals[seat]+=ctx.wall[seat][c]/(1+.12f*r);break;}if(g==BASE0||g==BASE1){int target=g-BASE0;vals[target]-=30.f/(1+.18f*r);break;}int j=occ[c];if(j>=0){float danger=(s.tank[j].hp<=1?25.f:12.f)*std::pow(.55f,float(r-1));int exits=0;for(int d=0;d<4;d++)if((d/2)!=(bl.dir/2)&&s.grid[c+DIR_DELTA[d]]==EMPTY&&occ[c+DIR_DELTA[d]]<0)exits++;if(exits==0)danger=std::max(danger,(s.tank[j].hp<=1?20.f:10.f)/(1+.25f*r));vals[j/3]-=danger;break;}}}
if(s.wall_hash!=ctx.root->wall_hash)for(int c=0;c<CELLS;c++)if((ctx.root->grid[c]==BRICK||ctx.root->grid[c]==STEEL)&&s.grid[c]==EMPTY)for(int seat=0;seat<2;seat++)vals[seat]+=ctx.wall[seat][c];return vals[me]-vals[1-me];}
struct Choice{Orders o;float value;};
std::vector<Choice>choices(const State&s,int seat,int limit,const Orders&other){std::vector<std::pair<float,Cmd>>ls[3];for(int t=0;t<3;t++){Cmd legal[N_CMDS];int n=legal_commands(s,3*seat+t,legal);for(int j=0;j<n;j++){Orders o;o.cmd[t]=legal[j];State z=s;step_as(z,seat,o,other);ls[t].push_back({evaluation(z,seat),legal[j]});}std::stable_sort(ls[t].begin(),ls[t].end(),[](auto a,auto b){return a.first>b.first;});if(ls[t].size()>7)ls[t].resize(7);}std::vector<Choice>out;for(auto a:ls[0])for(auto b:ls[1])for(auto c:ls[2]){Orders o;o.cmd[0]=a.second;o.cmd[1]=b.second;o.cmd[2]=c.second;State z=s;step_as(z,seat,o,other);out.push_back({o,evaluation(z,seat)});}std::stable_sort(out.begin(),out.end(),[](const Choice&a,const Choice&b){return a.value>b.value;});if(int(out.size())>limit)out.resize(limit);return out;}
Orders greedy(const State&s,int seat,const Orders&other){Orders o;for(int pass=0;pass<2;pass++)for(int t=0;t<3;t++){Cmd legal[N_CMDS];int n=legal_commands(s,3*seat+t,legal);float best=-1e9;Cmd bc=0;for(int k=0;k<n;k++){Orders a=o;a.cmd[t]=legal[k];State z=s;step_as(z,seat,a,other);float v=evaluation(z,seat);if(v>best){best=v;bc=legal[k];}}o.cmd[t]=bc;}return o;}
}
void init(const State&,int){std::fill(priorGoal,priorGoal+6,-1);for(auto&row:visits)std::fill(row,row+CELLS,0);}
Orders decide(const State&s,int me){prepare(s);float sophistication=std::clamp((s.stars_eaten[1-me]-1)*.5f,0.f,1.f);float rootRisk=.75f-.45f*sophistication,continuationRisk=.8f-.4f*sophistication;Orders idle;auto enemy=choices(s,1-me,24,idle);auto seeds=enemy;
for(int b=0;b<std::min(2,int(seeds.size()));b++)for(int t=0;t<3;t++){
Cmd legal[N_CMDS];int n=legal_commands(s,3*(1-me)+t,legal);
for(int k=0;k<n;k++){Orders o=seeds[b].o;o.cmd[t]=legal[k];
if(std::none_of(enemy.begin(),enemy.end(),[&](const Choice&z){return z.o==o;}))enemy.push_back({o,0});
}
}
auto mine=choices(s,me,60,enemy[0].o);auto quiet=choices(s,me,10,idle);for(auto a:quiet)if(std::none_of(mine.begin(),mine.end(),[&](const Choice&b){return a.o==b.o;}))mine.push_back(a);struct Ranked{int index;float value;int response;};std::vector<Ranked>rank;
int n=mine.size(),m=enemy.size();std::vector<float> matrix(n*m);
for(int a=0;a<n;a++)for(int b=0;b<m;b++){State z=s;step_as(z,me,mine[a].o,enemy[b].o);matrix[a*m+b]=evaluation(z,me);}
std::vector<float> r1(n),r2(m),p1(n),p2(m),avg2(m),u1(n),u2(m);
for(int iter=0;iter<80;iter++){
float sum1=0,sum2=0;for(int a=0;a<n;a++)sum1+=std::max(0.f,r1[a]);for(int b=0;b<m;b++)sum2+=std::max(0.f,r2[b]);
for(int a=0;a<n;a++)p1[a]=sum1>1e-5?std::max(0.f,r1[a])/sum1:1.f/n;
for(int b=0;b<m;b++){p2[b]=sum2>1e-5?std::max(0.f,r2[b])/sum2:1.f/m;avg2[b]+=p2[b];}
std::fill(u1.begin(),u1.end(),0);std::fill(u2.begin(),u2.end(),0);float v=0;
for(int a=0;a<n;a++)for(int b=0;b<m;b++){float x=matrix[a*m+b];u1[a]+=p2[b]*x;u2[b]-=p1[a]*x;}
for(int a=0;a<n;a++)v+=p1[a]*u1[a];for(int a=0;a<n;a++)r1[a]+=u1[a]-v;for(int b=0;b<m;b++)r2[b]+=u2[b]+v;
}
for(int a=0;a<n;a++){float worst=1e9,sum=0;int wi=0;for(int b=0;b<m;b++){float v=matrix[a*m+b];if(v<worst){worst=v;wi=b;}sum+=v*avg2[b]/80;}
rank.push_back({a,rootRisk*worst+(1-rootRisk)*sum,wi});}
std::stable_sort(rank.begin(),rank.end(),[](auto a,auto b){return a.value>b.value;});Orders result=mine[rank[0].index].o;float best=-1e9;for(int k=0;k<std::min(8,int(rank.size()));k++){if(::ensemble_models::private_budget_left()<12)break;auto r=rank[k];float worst=1e9,sum=0;int responses[3]={r.response,0,int(enemy.size())>1?1:0};for(int b:responses){if(::ensemble_models::private_budget_left()<6)return result;State z=s;step_as(z,me,mine[r.index].o,enemy[b].o);float v=evaluation(z,me);if(!z.done){Orders eo=greedy(z,1-me,idle),mo=greedy(z,me,eo);eo=greedy(z,1-me,mo);mo=greedy(z,me,eo);State q=z;step_as(q,me,mo,eo);v=.35f*v+.65f*evaluation(q,me);}worst=std::min(worst,v);sum+=v;}float value=.8f*(continuationRisk*worst+(1-continuationRisk)*sum/3)+.2f*r.value;if(value>best){best=value;result=mine[r.index].o;}}for(int i=0;i<6;i++){priorGoal[i]=ctx.goal[i];if(s.tank[i].alive)visits[i][s.tank[i].pos]++;}return result;}
}
namespace ensemble_models::opus{
using namespace tank;
#define ADDITIONAL_OPUS_FIXED_PASSES 0
static const int INF = 100000;
static int ME,OP;
static int atkField[2][CELLS];
static int huntField[3][CELLS];
static int starField[CELLS];
static int goalOf[3];
static const int* goalField[3];
static int FAR_ATK = 1000,FAR_K = 5,HUNT_KEEP = 4;
static double L2 = 0.5;
static int PREP_T1 = 10,PREP_T = 4,PREP_LIM1 = 9,PREP_LIM = 5,PREP_N1 = 3,PREP_N = 3;
static int BRICK_K = 2;
static int W_HUNT = 14,W_HUNT_HOT = 40;
static int W_STEP = 10,V_BASE = 100,V_MYBASE = 110,V_DEATH = 400,V_UNSAFE = 150,V_KILL = 20,V_KILLY = 12;
static int V_HP = 40,V_LEVEL = 120,B_BRICK = 6,B_FAR = 5,B_FARSTILL = 15,B_BASEFAR = 8;
static double M_STAY = 0.55,M_THREAT = 0.45,M_ANT = 0.0;
static int danger[CELLS];
static bool useDanger = false;
static int wStep[3]={10,10,10};
static int DANGER_NEAR = 0,DANGER_FAR = 0;
static uint8_t lane[CELLS];
static void dijkstra(const State& s,int* f,bool useLane = false){
typedef std::pair<int,int> PI;
std::priority_queue<PI,std::vector<PI>,std::greater<PI>> pq;
for(int c = 0;c < CELLS;++c)if(f[c]< INF)pq.push({f[c],c});
while(!pq.empty()){
PI p = pq.top();pq.pop();
if(p.first > f[p.second])continue;
for(int d = 0;d < 4;++d){
int n = p.second + DIR_DELTA[d];
uint8_t g = s.grid[n];
int w;
if(g == EMPTY)w = 1;else if(g == BRICK)w = 3;else continue;
if(useLane && lane[n])w += 8;
if(useDanger)w += danger[n];
if(f[n]> p.first + w){f[n]= p.first + w;pq.push({f[n],n});}
}
}
}
static bool buildAtk(const State& s,int seat,int* f,int only = -1){
for(int c = 0;c < CELLS;++c)f[c]= INF;
int B = s.base_pos[1 - seat];
bool any = false;
for(int d = 0;d < 4;++d){
if(only >= 0 && d != only)continue;
int bricks = 0;
for(int k = 1;k <= 9;++k){
int c = B + k * DIR_DELTA[d];
uint8_t g = s.grid[c];
int far = k > 2 ?(k - 2)* 2 : 0;
if(g == EMPTY){f[c]= std::min(f[c],2 * bricks + far);any = true;}
else if(g == BRICK){f[c]= std::min(f[c],2 * bricks + far + 2);++bricks;any = true;}
else if(g == WATER)continue;
else break;
}
}
useDanger = only >= 0;
dijkstra(s,f);
useDanger = false;
return any;
}
static int sideField[3][CELLS];
static bool sideOk[3];
static int sideOf[3]={-1,-1,-1};
static int lastBest[3]={INF,INF,INF},stuckCnt[3];
static int sidePen[3][3],sidePenUntil[3][3];
static void buildHunt(const State& s,int e,int* f){
for(int c = 0;c < CELLS;++c)f[c]= INF;
int P = s.tank[e].pos;
for(int d = 0;d < 4;++d){
for(int k = 1;k <= 7;++k){
int c = P + k * DIR_DELTA[d];
uint8_t g = s.grid[c];
if(g == EMPTY){if(!lane[c])f[c]=(k == 1 ? 2 :(k == 2 ? 0 :(k - 2)));}
else if(g == WATER)continue;
else break;
}
}
dijkstra(s,f,true);
}
static void buildLane(const State& s){
memset(lane,0,sizeof lane);
memset(danger,0,sizeof danger);
for(int t = 0;t < 3;++t){
int e = tank_id(OP,t);
if(!s.tank[e].alive)continue;
for(int d = 0;d < 4;++d)
for(int k = 1;k <= 5;++k){
int c = s.tank[e].pos + k * DIR_DELTA[d];
if(s.grid[c]!= EMPTY && s.grid[c]!= WATER)break;
danger[c]+= k <= 2 ? DANGER_NEAR : DANGER_FAR;
}
}
for(int t = 0;t < 3;++t){
int e = tank_id(OP,t);
if(!s.tank[e].alive)continue;
for(int d = 0;d < 4;++d){
int k = 1;bool hit = false;
for(;k <= 14;++k){
uint8_t g = s.grid[s.tank[e].pos + k * DIR_DELTA[d]];
if(g == BASE0 + ME){hit = true;break;}
if(g != EMPTY && g != WATER && g != BRICK)break;
}
if(hit)for(int j = 1;j < k;++j)lane[s.tank[e].pos + j * DIR_DELTA[d]]= 1;
}
}
for(int b = 0;b < s.n_bullets;++b){
if(seat_of(s.bullet[b].owner)== ME)continue;
for(int k = 1;k <= 6;++k){
int c = s.bullet[b].pos + k * DIR_DELTA[s.bullet[b].dir];
if(s.grid[c]!= EMPTY && s.grid[c]!= WATER)break;
lane[c]= 1;
}
}
}
static Cmd threatCmd(const State& s,int e){
const Tank& t = s.tank[e];
if(!t.alive || !can_fire(s,e))return STAY;
int side = seat_of(e);
int best = -1,bestP = 0;
for(int d = 0;d < 4;++d){
int bricks = 0;
for(int k = 1;k <= 14;++k){
int c = t.pos + k * DIR_DELTA[d];
uint8_t g = s.grid[c];
if(g == WATER)continue;
if(g == BRICK){++bricks;continue;}
if(g == BASE0 +(1 - side)){int p = 20 - bricks;if(p > bestP){bestP = p;best = d;}break;}
if(g != EMPTY)break;
int v = tank_at(s,c);
if(v >= 0){
if(seat_of(v)!= side && bricks == 0 && k <= 8){int p = 100 - k;if(p > bestP){bestP = p;best = d;}}
break;
}
}
}
return best < 0 ? STAY : cmd_fire(best);
}
static Orders threatOrders(const State& s,int side){
Orders o;
for(int t = 0;t < 3;++t)o.cmd[t]= threatCmd(s,tank_id(side,t));
return o;
}
static int destOf(const State& s,int i,Cmd c){
int p = s.tank[i].pos;
auto go =[&](int d){int n = p + DIR_DELTA[d];if(s.grid[n]== EMPTY && tank_at(s,n)< 0)p = n;};
if(is_move(c))go(c - 1);
else if(is_move2(c)){go((c - 9)/ 4);go((c - 9)% 4);}
return p;
}
static Orders anticipateOrders(const State& s,int side,int i,int dest){
Orders o = threatOrders(s,side);
for(int t = 0;t < 3;++t){
int e = tank_id(side,t);
if(!s.tank[e].alive || !can_fire(s,e))continue;
for(int d = 0;d < 4;++d){
for(int k = 1;k <= 2;++k){
int c = s.tank[e].pos + k * DIR_DELTA[d];
uint8_t g = s.grid[c];
if(g != EMPTY && g != WATER)break;
if(c == dest){o.cmd[t]= cmd_fire(d);break;}
int v = tank_at(s,c);
if(v >= 0 && v != i)break;
}
}
}
return o;
}
static double evalState(const State& s0,const State& s){
double v = 0;
if(s.done){
if(s.winner == ME)v += 100000;else if(s.winner == OP)v -= 100000;
}
v += V_BASE *(s0.base_hp[OP]- s.base_hp[OP]);
v -= V_MYBASE *(s0.base_hp[ME]- s.base_hp[ME]);
for(int t = 0;t < 3;++t){
int i = tank_id(ME,t),e = tank_id(OP,t);
const Tank& a0 = s0.tank[i];const Tank& a = s.tank[i];
if(a0.alive){
if(!a.alive)v -= V_DEATH + 60 * a0.level;
else{
int dh = a.hp - 2 *(a.level - a0.level)- a0.hp;
v += V_HP * dh;
v += V_LEVEL *(a.level - a0.level);
int f = goalField[t][a.pos];
if(f >= INF)f = 60;
v -= wStep[t]* f;
}
}
const Tank& b0 = s0.tank[e];const Tank& b = s.tank[e];
if(b0.alive){
if(!b.alive){
int dy = std::abs(cell_y(b0.pos)- cell_y(s.spawn[e]));
v += V_KILL + V_KILLY * dy + 60 * b0.level;
}else{
int dh = b.hp - 2 *(b.level - b0.level)- b0.hp;
v -= V_HP * dh;
v -= V_LEVEL *(b.level - b0.level);
}
}
}
return v;
}
static double cmdBonus(const State& s,int i,Cmd c,const int* field){
if(!is_fire(c))return 0;
int d = c - 5;
const Tank& t = s.tank[i];
for(int k = 1;k <= 14;++k){
int cc = t.pos + k * DIR_DELTA[d];
uint8_t g = s.grid[cc];
if(g == WATER)continue;
if(g == BRICK){
if(k <= BRICK_K && field[cc]<= field[t.pos]&& field[cc]< INF)return B_BRICK;
for(int kk = k + 1;kk <= 14;++kk){
uint8_t g2 = s.grid[t.pos + kk * DIR_DELTA[d]];
if(g2 == BASE0 + OP)return B_BRICK;
if(g2 != EMPTY && g2 != WATER && g2 != BRICK)break;
}
return -3;
}
if(g == BASE0 + OP)return k > 2 ? B_BASEFAR : 0;
if(g == BASE0 + ME)return -400;
if(g != EMPTY)return -3;
int v = tank_at(s,cc);
if(v >= 0){
if(seat_of(v)== ME)return -300;
if(k <= 2)return 0;
if(field[t.pos]<= FAR_ATK && k > FAR_K)return -3;
bool still = s.tank[v].last_fire >= s.turn - 1;
return(still ? B_FARSTILL : B_FAR)- k;
}
}
return -3;
}
static bool safeNext(const State& s1,int i){
const Tank& t = s1.tank[i];
if(!t.alive)return true;
Orders eo = threatOrders(s1,OP);
Cmd list[N_CMDS];
int n = legal_commands(s1,i,list);
for(int k = 0;k < n;++k){
State s2 = s1;
Orders mo;mo.cmd[type_of(i)]= list[k];
step_as(s2,ME,mo,eo);
if(s2.tank[i].alive && s2.tank[i].hp >= t.hp)return true;
if(s2.done && s2.winner == ME)return true;
}
return false;
}
void init(const State& s,int me){ME = me;OP = 1 - me;}
Orders decide(const State& s,int me){
ME = me;OP = 1 - me;
Orders orders;
buildAtk(s,ME,atkField[ME]);
buildAtk(s,OP,atkField[OP]);
buildLane(s);
bool anyStar = false;
for(int c = 0;c < CELLS;++c)starField[c]= INF;
for(int k = 0;k < s.n_supply;++k)if(s.star_mask >> k & 1){starField[s.supply[k]]= 0;anyStar = true;}
if(anyStar)dijkstra(s,starField);
{
const int sd[3]={ME == 0 ? DOWN : UP,LEFT,RIGHT};
for(int k = 0;k < 3;++k)sideOk[k]= buildAtk(s,ME,sideField[k],sd[k]);
for(int t = 0;t < 3;++t){
int i = tank_id(ME,t);
if(!s.tank[i].alive){stuckCnt[t]= 0;lastBest[t]= INF;continue;}
if(sideOf[t]>= 0){
int f = sideField[sideOf[t]][s.tank[i].pos];
if(f < lastBest[t]){lastBest[t]= f;stuckCnt[t]= 0;}
else if(f > 0 && ++stuckCnt[t]>= 8){
sidePen[t][sideOf[t]]= 25;sidePenUntil[t][sideOf[t]]= s.turn + 30;
stuckCnt[t]= 0;lastBest[t]= INF;
}
}
}
int bestTot = INF,bestA[3]={0,0,0};
for(int a = 0;a < 27;++a){
int as[3]={a % 3,a / 3 % 3,a / 9};
int tot = 0,cnt[3]={0,0,0};
for(int t = 0;t < 3;++t){
int i = tank_id(ME,t);
int k = as[t];
if(!sideOk[k]){tot += 1000;continue;}
cnt[k]++;
int pos = s.tank[i].alive ? s.tank[i].pos : s.spawn[i];
int f = sideField[k][pos];if(f >= INF)f = 200;
if(t == FAST)f =(f + 1)/ 2;
if(s.turn < sidePenUntil[t][k])f += sidePen[t][k];
if(k != sideOf[t])f += 3;
tot += f;
}
for(int k = 0;k < 3;++k)if(cnt[k]> 1)tot += 12 *(cnt[k]- 1);
if(tot < bestTot){bestTot = tot;for(int t = 0;t < 3;++t)bestA[t]= as[t];}
}
for(int t = 0;t < 3;++t){
if(bestA[t]!= sideOf[t]){sideOf[t]= bestA[t];stuckCnt[t]= 0;lastBest[t]= INF;}
goalOf[t]= 0;
goalField[t]= sideOk[sideOf[t]]? sideField[sideOf[t]]: atkField[ME];
}
}
for(int t = 0;t < 3;++t)wStep[t]= W_STEP;
bool huntBuilt[3]={false,false,false};
int order[3]={0,1,2};
std::sort(order,order + 3,[&](int a,int b){
int ea = tank_id(OP,a),eb = tank_id(OP,b);
int fa = s.tank[ea].alive ? atkField[OP][s.tank[ea].pos]: INF;
int fb = s.tank[eb].alive ? atkField[OP][s.tank[eb].pos]: INF;
return fa < fb;
});
bool used[3]={false,false,false};
int nHunt = 0;
for(int oi = 0;oi < 3;++oi){
int k = order[oi],e = tank_id(OP,k);
if(!s.tank[e].alive)continue;
int thr = atkField[OP][s.tank[e].pos];
if(thr > 7)continue;
buildHunt(s,e,huntField[k]);huntBuilt[k]= true;
int best = -1,bd = INF;
for(int t = 0;t < 3;++t){
int i = tank_id(ME,t);
if(used[t]|| !s.tank[i].alive)continue;
int dd = huntField[k][s.tank[i].pos];
if(t == FAST)dd =(dd + 1)/ 2;
if(goalField[t][s.tank[i].pos]<= HUNT_KEEP && dd > 2)continue;
if(dd < bd){bd = dd;best = t;}
}
if(best >= 0 && bd <= 14 && nHunt <(s.base_hp[ME]> s.base_hp[OP]+ 6 ? 0 : 1)){++nHunt;used[best]= true;goalOf[best]= 1 + k;goalField[best]= huntField[k];wStep[best]= thr <= 1 ? W_HUNT_HOT : W_HUNT;}
}
{
int W = s.turn < 10 ? 10 : 10 +((s.turn - 10 + 19)/ 20)* 20;
bool first = W == 10;
if(W - s.turn <=(first ? PREP_T1 : PREP_T)&& W <= 290){
static int spf[MAX_SUPPLY][CELLS];
for(int k = 0;k < s.n_supply;++k){
for(int c = 0;c < CELLS;++c)spf[k][c]= INF;
spf[k][s.supply[k]]= 0;
dijkstra(s,spf[k]);
}
int lim = first ? PREP_LIM1 : PREP_LIM;
int maxN = first ? PREP_N1 : PREP_N;
int bestTot = INF,bestA[3]={-1,-1,-1};
for(int a = 0;a < 64;++a){
int as[3]={a % 4 - 1,a / 4 % 4 - 1,a / 16 % 4 - 1};
int tot = 0,cnt = 0;bool ok = true;
for(int t = 0;t < 3 && ok;++t){
int k = as[t];
if(k < 0){tot += lim + 1;continue;}
int i = tank_id(ME,t);
if(k >= s.n_supply || !s.tank[i].alive || goalOf[t]!= 0 || s.tank[i].level >= 2){ok = false;break;}
for(int u = 0;u < t;++u)if(as[u]== k)ok = false;
int d = spf[k][s.tank[i].pos];
if(t == FAST)d =(d + 1)/ 2;
if(d > lim)ok = false;
tot += d;++cnt;
}
if(!ok || cnt > maxN)continue;
if(tot < bestTot){bestTot = tot;for(int t = 0;t < 3;++t)bestA[t]= as[t];}
}
static int prepF[3][CELLS];
for(int t = 0;t < 3;++t)if(bestA[t]>= 0){
memcpy(prepF[t],spf[bestA[t]],sizeof prepF[t]);
goalOf[t]= 4;goalField[t]= prepF[t];
}
}
}
if(anyStar){
for(int t = 0;t < 3;++t){
int i = tank_id(ME,t);
if(!s.tank[i].alive || goalOf[t]!= 0 || s.tank[i].level >= 2)continue;
int sd = starField[s.tank[i].pos];
int lim = s.tank[i].level == 0 ? 5 : 3;
if(sd <= lim && goalField[t][s.tank[i].pos]> 0){goalOf[t]= 4;goalField[t]= starField;}
}
}
const int seq[3]={FIRE,ARMOR,FAST};
int passes = 0;
for(int pass = 0;pass < 2;++pass){
if(pass > 0){
if(ADDITIONAL_OPUS_FIXED_PASSES ? pass >= ADDITIONAL_OPUS_FIXED_PASSES : ::ensemble_models::private_budget_left()< 25)break;
}
++passes;
bool changed = false;
for(int si = 0;si < 3;++si){
int t = seq[si],i = tank_id(ME,t);
if(!s.tank[i].alive)continue;
Cmd list[N_CMDS];
int n = legal_commands(s,i,list);
bool cf = can_fire(s,i);
Orders eStay,eThr = threatOrders(s,OP);
double bestV = -1e18;Cmd bestC = STAY;
for(int k = 0;k < n;++k){
Cmd c = list[k];
if(is_fire(c)&& !cf)continue;
Orders mo = orders;mo.cmd[t]= c;
double v = cmdBonus(s,i,c,goalField[t]);
Orders eAnt = anticipateOrders(s,OP,i,destOf(s,i,c));
const Orders* models[3]={&eStay,&eThr,&eAnt};
const double wts[3]={M_STAY,M_THREAT,M_ANT};
for(int m = 0;m < 3;++m){
State s1 = s;
step_as(s1,ME,mo,*models[m]);
double ev = evalState(s,s1);
if(wts[m]<= 0)continue;
if(!s1.done && s1.tank[i].alive && L2 > 0){
Cmd l2[N_CMDS];
int n2 = legal_commands(s1,i,l2);
bool cf2 = can_fire(s1,i);
Orders e2;if(m >= 1)e2 = threatOrders(s1,OP);
double b2 = -1e18;
for(int k2 = 0;k2 < n2;++k2){
if(is_fire(l2[k2])&& !cf2)continue;
State s2 = s1;
Orders m2;m2.cmd[t]= l2[k2];
step_as(s2,ME,m2,e2);
double e = evalState(s,s2)+ cmdBonus(s1,i,l2[k2],goalField[t]);
if(e > b2)b2 = e;
}
ev =(1 - L2)* ev + L2 * b2;
}
if(!s1.done && s1.tank[i].alive && !safeNext(s1,i))ev -= V_UNSAFE;
v += wts[m]* ev;
}
if(v > bestV){bestV = v;bestC = c;}
}
if(bestC != orders.cmd[t])changed = true;
orders.cmd[t]= bestC;
}
if(!changed)break;
}
#ifdef DBG
for(int t = 0;t < 3;++t){int i = tank_id(ME,t);fprintf(stderr,"t%d goal%d side%d f%d w%d | ",t,goalOf[t],sideOf[t],s.tank[i].alive ? goalField[t][s.tank[i].pos]: -1,wStep[t]);}
fputc(10,stderr);
#endif
return orders;
}
}
namespace ensemble_models::kimi{
using namespace tank;
static int g_me;
static int dist_base[2][CELLS];
static bool danger[CELLS];
static bool g_threat;
static int dist_supply[CELLS];
static void bfs_supply(const State& s){
static int queue[CELLS];
for(int c = 0;c < CELLS;++c)dist_supply[c]= 9999;
int head = 0,tail = 0;
for(int k = 0;k < s.n_supply;++k){
dist_supply[s.supply[k]]= 0;
queue[tail++]= s.supply[k];
}
while(head < tail){
const int c = queue[head++];
for(int d = 0;d < 4;++d){
const int nx = c + DIR_DELTA[d];
const uint8_t g = s.grid[nx];
if((g == EMPTY || g == BRICK)&& dist_supply[nx]> dist_supply[c]+ 1){
dist_supply[nx]= dist_supply[c]+ 1;
queue[tail++]= nx;
}
}
}
}
static Cmd opening_star_cmd(const State& s,int me){
if(s.turn >= 9 || s.n_bullets > 0)return STAY;
const int i = tank_id(me,FAST);
if(!s.tank[i].alive)return STAY;
const int p = s.tank[i].pos;
if(dist_supply[p]== 0)return STAY;
Cmd bestc = STAY;
int bestd = dist_supply[p];
for(int d1 = 0;d1 < 4;++d1){
const int p1 = p + DIR_DELTA[d1];
const uint8_t g1 = s.grid[p1];
if(g1 == BRICK){
if(dist_supply[p1]< bestd && can_fire(s,i)){bestd = dist_supply[p1];bestc = cmd_fire(d1);}
continue;
}
if(g1 != EMPTY || tank_at(s,p1)>= 0)continue;
if(dist_supply[p1]< bestd){bestd = dist_supply[p1];bestc = cmd_move(d1);}
for(int d2 = 0;d2 < 4;++d2){
const int p2 = p1 + DIR_DELTA[d2];
if(s.grid[p2]!= EMPTY || tank_at(s,p2)>= 0)continue;
if(dist_supply[p2]< bestd){bestd = dist_supply[p2];bestc = cmd_move2(d1,d2);}
}
}
return bestc;
}
static void bfs(const State& s,int src,int* dist){
static int queue[CELLS];
for(int c = 0;c < CELLS;++c)dist[c]= 9999;
int head = 0,tail = 0;
dist[src]= 0;
queue[tail++]= src;
while(head < tail){
const int c = queue[head++];
for(int d = 0;d < 4;++d){
const int nx = c + DIR_DELTA[d];
const uint8_t g = s.grid[nx];
if((g == EMPTY || g == BRICK)&& dist[nx]> dist[c]+ 1){
dist[nx]= dist[c]+ 1;
queue[tail++]= nx;
}
}
}
}
static double fire_line_score(const State& s,int seat,int pos,int d,int level,int* bricks = nullptr){
int nb = 0;
for(int c = pos + DIR_DELTA[d];;c += DIR_DELTA[d]){
const uint8_t g = s.grid[c];
if(g == EMPTY || g == WATER){
const int t = tank_at(s,c);
if(t >= 0){
if(bricks)*bricks = nb;
return seat_of(t)== seat ? -70.0 : 55.0;
}
continue;
}
if(bricks)*bricks = nb;
if(g == BRICK){++nb;continue;}
if(g == STEEL)return(level >= 2)? 6.0 : -2.0;
if(g == BASE0 +(1 - seat))return 45.0 - 6.0 * nb;
if(g == BASE0 + seat)return -120.0;
return -2.0;
}
}
static bool clear_line_to_base(const State& s,int pos,int base_cell){
const int bx = cell_x(base_cell),by = cell_y(base_cell);
const int px = cell_x(pos),py = cell_y(pos);
if(px != bx && py != by)return false;
int d;
if(px == bx)d = by > py ? DOWN : UP;
else d = bx > px ? RIGHT : LEFT;
for(int c = pos + DIR_DELTA[d];;c += DIR_DELTA[d]){
if(c == base_cell)return true;
const uint8_t g = s.grid[c];
if(g != EMPTY && g != WATER)return false;
}
}
static int min_bricks_to_base(const State& s,int seat,int pos){
const int base = s.base_pos[1 - seat];
if(cell_x(pos)!= cell_x(base)&& cell_y(pos)!= cell_y(base))return 99;
int d;
if(cell_x(pos)== cell_x(base))d = cell_y(base)> cell_y(pos)? DOWN : UP;
else d = cell_x(base)> cell_x(pos)? RIGHT : LEFT;
int nb = 0;
for(int c = pos + DIR_DELTA[d];;c += DIR_DELTA[d]){
if(c == base)return nb;
const uint8_t g = s.grid[c];
if(g == BRICK){++nb;continue;}
if(g == EMPTY || g == WATER)continue;
return 99;
}
}
static double evaluate(const State& s){
const int me = g_me,opp = 1 - me;
if(s.done){
if(s.winner == me)return 1e6 - s.turn;
if(s.winner == opp)return -1e6 + s.turn;
return 0.0;
}
double v = 0;
v += 140.0 *(s.base_hp[me]- s.base_hp[opp]);
v += 40.0 *(s.kills[me]- s.kills[opp]);
v += 10.0 *(s.stars_eaten[me]- s.stars_eaten[opp]);
for(int i = 0;i < 6;++i){
const Tank& t = s.tank[i];
const int seat = seat_of(i),foe = 1 - seat;
double tv;
if(t.alive){
tv = 20.0 + 5.0 * t.hp + 14.0 * t.level;
const int dfb = dist_base[foe][t.pos];
tv -= 4.0 * dfb;
tv += 4.0 *(12 - dfb < 0 ? 0 : 12 - dfb);
tv += 5.0 *(7 - dfb < 0 ? 0 : 7 - dfb);
double best_line = -1e9;
for(int d = 0;d < 4;++d){
const double ls = fire_line_score(s,seat,t.pos,d,t.level);
if(ls > best_line)best_line = ls;
}
tv += 0.25 * best_line;
const bool no_reload =(type_of(i)== FIRE)|| t.level >= 1;
const int nb2 = min_bricks_to_base(s,seat,t.pos);
if(nb2 == 0)tv += no_reload ? 320.0 : 150.0;
else if(nb2 < 99)tv +=(60.0 - 8.0 * nb2)*(no_reload ? 1.0 : 0.6);
if(nb2 == 0)
for(int j = 3 * foe;j < 3 * foe + 3;++j)
if(s.tank[j].alive){
const int md = std::abs(cell_x(s.tank[j].pos)- cell_x(t.pos))
+ std::abs(cell_y(s.tank[j].pos)- cell_y(t.pos));
tv -= 24.0 /(1.0 + md);
}
for(int j = 3 * foe;j < 3 * foe + 3;++j)
if(!s.tank[j].alive && t.pos == s.spawn[j]&& respawn_in(s,j)<= 1)tv -= 30.0;
}else{
tv = -2.0 * respawn_in(s,i);
}
v +=(seat == me ? tv : -tv);
}
{
const bool soon = s.turn + 1 >= s.P.supply_first &&
(s.turn + 1 - s.P.supply_first)% s.P.supply_period >= s.P.supply_period - 5;
if(s.star_mask || soon){
for(int q = 0;q < s.n_supply;++q){
const bool has =(s.star_mask >> q)& 1;
if(!has && !soon)continue;
const int sp = s.supply[q];
int dm = 99,dt = 99;
for(int i = 0;i < 6;++i){
if(!s.tank[i].alive)continue;
const int d = std::abs(cell_x(sp)- cell_x(s.tank[i].pos))+ std::abs(cell_y(sp)- cell_y(s.tank[i].pos));
if(seat_of(i)== me){if(d < dm)dm = d;}
else if(d < dt)dt = d;
}
v +=(has ? 5.0 : 2.0)*(dt - dm);
}
}
}
return v;
}
struct Ranked{Cmd c;double sc;};
static int rank_commands(const State& s,int i,int seat,Ranked* out,int k,bool guard){
Cmd list[N_CMDS];
const int n = legal_commands(s,i,list);
const Tank& t = s.tank[i];
const int foe = 1 - seat;
int m = 0;
auto add =[&](Cmd c){
for(int j = 0;j < m;++j)if(out[j].c == c)return;
if(m < k)out[m++]= Ranked{c,0};
};
add(STAY);
if(guard){
for(int pick = 0;pick < 2;++pick){
Cmd bc = 0;double bs = -1e9;
for(int j = 0;j < n;++j){
if(!is_move(list[j])&& !is_move2(list[j]))continue;
bool dup = false;
for(int q = 0;q < m;++q)if(out[q].c == list[j])dup = true;
if(dup)continue;
int d1,d2 = -1;
if(is_move(list[j]))d1 = list[j]- 1;
else{d1 =(list[j]- 9)>> 2;d2 =(list[j]- 9)& 3;}
int p1 = t.pos + DIR_DELTA[d1];
if(s.grid[p1]!= EMPTY)p1 = t.pos;
int p2 = p1;
if(d2 >= 0 && s.grid[p1 + DIR_DELTA[d2]]== EMPTY)p2 = p1 + DIR_DELTA[d2];
double sc = 3.0 *(dist_base[seat][t.pos]- dist_base[seat][p2]);
if(g_threat)
for(int e = 3 * foe;e < 3 * foe + 3;++e)
if(s.tank[e].alive && dist_base[seat][s.tank[e].pos]<= 6)
sc += 2.0 *(std::abs(cell_x(s.tank[e].pos)- cell_x(t.pos))+ std::abs(cell_y(s.tank[e].pos)- cell_y(t.pos))
- std::abs(cell_x(s.tank[e].pos)- cell_x(p2))- std::abs(cell_y(s.tank[e].pos)- cell_y(p2)));
if(sc > bs){bs = sc;bc = list[j];}
}
if(bc)add(bc);
}
}
auto endpoint =[&](Cmd c){
int d1,d2 = -1;
if(is_move(c))d1 = c - 1;
else{d1 =(c - 9)>> 2;d2 =(c - 9)& 3;}
int p1 = t.pos + DIR_DELTA[d1];
if(s.grid[p1]!= EMPTY)p1 = t.pos;
int p2 = p1;
if(d2 >= 0 && s.grid[p1 + DIR_DELTA[d2]]== EMPTY)p2 = p1 + DIR_DELTA[d2];
return p2;
};
for(int pick = 0;pick < 2;++pick){
Cmd bc = 0;double bs = -1e9;
for(int j = 0;j < n;++j){
if(!is_fire(list[j]))continue;
bool dup = false;
for(int q = 0;q < m;++q)if(out[q].c == list[j])dup = true;
if(dup)continue;
const double sc = fire_line_score(s,seat,t.pos,list[j]- 5,t.level);
if(sc > bs){bs = sc;bc = list[j];}
}
if(bc && bs > -5.0)add(bc);
}
for(int pick = 0;pick < 6;++pick){
Cmd bc = 0;double bs = -1e9;
for(int j = 0;j < n;++j){
if(!is_move(list[j])&& !is_move2(list[j]))continue;
bool dup = false;
for(int q = 0;q < m;++q)if(out[q].c == list[j])dup = true;
if(dup)continue;
const int p2 = endpoint(list[j]);
double sc = 3.0 *(dist_base[foe][t.pos]- dist_base[foe][p2]);
if(is_move2(list[j]))sc += 1.0;
if(danger[p2])sc -= 20.0;
if(sc > bs){bs = sc;bc = list[j];}
}
if(bc)add(bc);
}
for(int pick = 0;pick < 2;++pick){
Cmd bc = 0;double bs = -1e9;
for(int j = 0;j < n;++j){
if(!is_move(list[j])&& !is_move2(list[j]))continue;
bool dup = false;
for(int q = 0;q < m;++q)if(out[q].c == list[j])dup = true;
if(dup)continue;
const int p2 = endpoint(list[j]);
double sc = 0;
if(danger[t.pos]&& !danger[p2])sc += 30.0;
if(danger[p2])sc -= 30.0;
if(g_threat)sc += 3.0 *(dist_base[seat][t.pos]- dist_base[seat][p2]);
for(int q = 0;q < s.n_supply;++q)
if((s.star_mask >> q)& 1){
const int sp = s.supply[q];
sc += 6.0 *(std::abs(cell_x(sp)- cell_x(t.pos))+ std::abs(cell_y(sp)- cell_y(t.pos))
- std::abs(cell_x(sp)- cell_x(p2))- std::abs(cell_y(sp)- cell_y(p2)));
}
if(sc > bs){bs = sc;bc = list[j];}
}
if(bc)add(bc);
}
for(int j = 0;j < m;++j){
const Cmd c = out[j].c;
if(c == STAY)out[j].sc = 0;
else if(is_fire(c))out[j].sc = fire_line_score(s,seat,t.pos,c - 5,t.level);
else{
const int p2 = endpoint(c);
out[j].sc = 3.0 *(dist_base[foe][t.pos]- dist_base[foe][p2])-(danger[p2]? 20.0 : 0.0);
}
}
return m;
}
static void compute_danger(const State& s,int me){
for(int c = 0;c < CELLS;++c)danger[c]= false;
for(int k = 0;k < s.n_bullets;++k){
const Bullet& b = s.bullet[k];
if(seat_of(b.owner)== me)continue;
int c = b.pos;
for(int stepn = 0;stepn < 2;++stepn){
c += DIR_DELTA[b.dir];
danger[c]= true;
const uint8_t g = s.grid[c];
if(g != EMPTY && g != WATER)break;
}
}
}
static Orders joint_greedy(const State& s,int seat,const Orders& opp_prev,int k){
compute_danger(s,seat);
bool threat = false;
for(int i = 3 *(1 - seat);i < 3 *(1 - seat)+ 3;++i)
if(s.tank[i].alive &&
(dist_base[seat][s.tank[i].pos]<= 4 || clear_line_to_base(s,s.tank[i].pos,s.base_pos[seat])))
threat = true;
g_threat = threat;
Ranked rc[3][11];
int nn[3]={1,1,1};
for(int type = 0;type < 3;++type){
const int i = tank_id(seat,type);
if(s.tank[i].alive)nn[type]= rank_commands(s,i,seat,rc[type],k,false);
else rc[type][0]= Ranked{STAY,0};
}
Orders best;
double bv = -1e18;
for(int a = 0;a < nn[0];++a)
for(int b = 0;b < nn[1];++b)
for(int c = 0;c < nn[2];++c){
Orders o;
o.cmd[0]= rc[0][a].c;o.cmd[1]= rc[1][b].c;o.cmd[2]= rc[2][c].c;
State nx = s;
step_as(nx,seat,o,opp_prev);
const double v = seat == g_me ? evaluate(nx): -evaluate(nx);
if(v > bv){bv = v;best = o;}
}
return best;
}
static double rollout(State s,int depth){
Orders prev[2]={};
for(int t = 0;t < depth && !s.done;++t){
const Orders o0 = joint_greedy(s,0,prev[1],4);
const Orders o1 = joint_greedy(s,1,prev[0],4);
prev[0]= o0;prev[1]= o1;
step(s,o0,o1);
}
return evaluate(s);
}
void init(const State& s,int me){
g_me = me;
bfs(s,s.base_pos[0],dist_base[0]);
bfs(s,s.base_pos[1],dist_base[1]);
}
Orders decide(const State& s,int me){
g_me = me;
bfs(s,s.base_pos[0],dist_base[0]);
bfs(s,s.base_pos[1],dist_base[1]);
bfs_supply(s);
compute_danger(s,me);
g_threat = false;
for(int i = 3 *(1 - me);i < 3 *(1 - me)+ 3;++i)
if(s.tank[i].alive &&
(dist_base[me][s.tank[i].pos]<= 4 || clear_line_to_base(s,s.tank[i].pos,s.base_pos[me])))
g_threat = true;
const int K_MINE = 12,K_OPP = 6;
Ranked mine[3][14],opps[3][14];
int nm[3]={1,1,1},no[3]={1,1,1};
for(int type = 0;type < 3;++type){
const int im = tank_id(me,type),io = tank_id(1 - me,type);
if(s.tank[im].alive)nm[type]= rank_commands(s,im,me,mine[type],K_MINE,false);
else{mine[type][0]= Ranked{STAY,0};}
if(s.tank[io].alive)no[type]= rank_commands(s,io,1 - me,opps[type],K_OPP,false);
else{opps[type][0]= Ranked{STAY,0};}
}
struct Joint{Orders o;double sc;};
static Joint opp_joint[1000];
int njo = 0;
for(int a = 0;a < no[0];++a)
for(int b = 0;b < no[1];++b)
for(int c = 0;c < no[2];++c){
Orders o;
o.cmd[0]= opps[0][a].c;o.cmd[1]= opps[1][b].c;o.cmd[2]= opps[2][c].c;
opp_joint[njo++]= Joint{o,opps[0][a].sc + opps[1][b].sc + opps[2][c].sc};
}
std::sort(opp_joint,opp_joint + njo,[](const Joint& x,const Joint& y){return x.sc > y.sc;});
Orders best;
double best_val = -1e18;
int checked = 0;
static Orders top_ord[12];
static double top_val[12];
int n_top = 0;
auto consider_top =[&](const Orders& mo,double v){
if(n_top < 12){
top_ord[n_top]= mo;top_val[n_top]= v;++n_top;
}else{
int wi = 0;
for(int q = 1;q < 12;++q)if(top_val[q]< top_val[wi])wi = q;
if(v > top_val[wi]){top_ord[wi]= mo;top_val[wi]= v;}
}
};
for(int a = 0;a < nm[0];++a)
for(int b = 0;b < nm[1];++b)
for(int c = 0;c < nm[2];++c){
Orders mo;
mo.cmd[0]= mine[0][a].c;mo.cmd[1]= mine[1][b].c;mo.cmd[2]= mine[2][c].c;
double worst = 1e18,v_top = 0;
int j = 0;
for(;j < njo;++j){
State nx = s;
step_as(nx,me,mo,opp_joint[j].o);
const double v = evaluate(nx);
if(j == 0)v_top = v;
if(v < worst)worst = v;
if(worst < best_val && v_top < best_val)break;
if((j & 15)== 15 && ::ensemble_models::private_budget_left()< 8.0)break;
}
const double combined = 0.4 * worst + 0.6 * v_top;
if(combined > best_val){
best_val = combined;
best = mo;
}
if(j == njo)consider_top(mo,combined);
if(++checked >= 12){
checked = 0;
if(::ensemble_models::private_budget_left()< 8.0)goto done;
}
}
if(false && ::ensemble_models::private_budget_left()> 14.0){
double best2 = -1e18;
for(int q = 0;q < n_top;++q){
State leaf = s;
step_as(leaf,me,top_ord[q],opp_joint[0].o);
const double rv = rollout(leaf,8);
const double v2 = 0.6 * top_val[q]+ 0.4 * rv;
if(v2 > best2){best2 = v2;best = top_ord[q];}
if(::ensemble_models::private_budget_left()< 8.0)break;
}
}
done:;
return best;
}
}
namespace ensemble_models::sparring{
#define ADDITIONAL_SPAR_SP_BUDGET 14000
#define ADDITIONAL_SPAR_SP_STARW 90
#define ADDITIONAL_SPAR_SP_LVV 70
#define ADDITIONAL_SPAR_SP_STAR_REACH 60
#line 1 "sparring.cpp"
using namespace tank;
#ifndef ADDITIONAL_SPAR_SP_BUDGET
#define ADDITIONAL_SPAR_SP_BUDGET 20000         
#endif
#ifndef ADDITIONAL_SPAR_SP_USE_TIME
#define ADDITIONAL_SPAR_SP_USE_TIME 0           
#endif
#ifndef ADDITIONAL_SPAR_SP_TIME_MARGIN
#define ADDITIONAL_SPAR_SP_TIME_MARGIN 4        
#endif
#ifndef ADDITIONAL_SPAR_SP_DEPTH
#define ADDITIONAL_SPAR_SP_DEPTH 6              
#endif
#ifndef ADDITIONAL_SPAR_SP_UCB
#define ADDITIONAL_SPAR_SP_UCB 80               
#endif
#ifndef ADDITIONAL_SPAR_SP_PRIOR_T
#define ADDITIONAL_SPAR_SP_PRIOR_T 12           
#endif
#ifndef ADDITIONAL_SPAR_SP_FPU
#define ADDITIONAL_SPAR_SP_FPU 0                 
#endif
#ifndef ADDITIONAL_SPAR_SP_STARS
#define ADDITIONAL_SPAR_SP_STARS 1              
#endif
#ifndef ADDITIONAL_SPAR_SP_STARW
#define ADDITIONAL_SPAR_SP_STARW 26             
#endif
#ifndef ADDITIONAL_SPAR_SP_LVV
#define ADDITIONAL_SPAR_SP_LVV 20               
#endif
#ifndef ADDITIONAL_SPAR_SP_STAR_REACH
#define ADDITIONAL_SPAR_SP_STAR_REACH 24        
#endif
#ifndef ADDITIONAL_SPAR_SP_FFFIX
#define ADDITIONAL_SPAR_SP_FFFIX 1              
#endif
#ifndef ADDITIONAL_SPAR_SP_TV
#define ADDITIONAL_SPAR_SP_TV 22                
#endif
#ifndef ADDITIONAL_SPAR_SP_TV_FIRE
#define ADDITIONAL_SPAR_SP_TV_FIRE 34           
#endif
#ifndef ADDITIONAL_SPAR_SP_THREAT
#define ADDITIONAL_SPAR_SP_THREAT 40            
#endif
#ifndef ADDITIONAL_SPAR_SP_MAX_HUNTERS
#define ADDITIONAL_SPAR_SP_MAX_HUNTERS 1        
#endif
#ifndef ADDITIONAL_SPAR_SP_EMERGENCY
#define ADDITIONAL_SPAR_SP_EMERGENCY 0          
#endif
#ifndef ADDITIONAL_SPAR_SP_GUARDS
#define ADDITIONAL_SPAR_SP_GUARDS 1             
#endif
#ifndef ADDITIONAL_SPAR_SP_GUARD_RANGE
#define ADDITIONAL_SPAR_SP_GUARD_RANGE 48       
#endif
#ifndef ADDITIONAL_SPAR_SP_FAR_COST
#define ADDITIONAL_SPAR_SP_FAR_COST 3           
#endif
#ifndef ADDITIONAL_SPAR_SP_BLOCK_STILL
#define ADDITIONAL_SPAR_SP_BLOCK_STILL 0        
#endif
#ifndef ADDITIONAL_SPAR_SP_HUNT_MAX
#define ADDITIONAL_SPAR_SP_HUNT_MAX 64          
#endif
#ifndef ADDITIONAL_SPAR_SP_SQUASH
#define ADDITIONAL_SPAR_SP_SQUASH 600           
#endif
#ifndef ADDITIONAL_SPAR_SP_PATH_AVG
#define ADDITIONAL_SPAR_SP_PATH_AVG 1           
#endif
#ifndef ADDITIONAL_SPAR_SP_METHOD
#define ADDITIONAL_SPAR_SP_METHOD 0             
#endif
#ifndef ADDITIONAL_SPAR_SP_IBR_ROUNDS
#define ADDITIONAL_SPAR_SP_IBR_ROUNDS 3         
#endif
#ifndef ADDITIONAL_SPAR_SP_ROLLOUT_RANDOM
#define ADDITIONAL_SPAR_SP_ROLLOUT_RANDOM 4     
#endif
static const int BIG = 30000;
static const int MAX_ACTS = 22;
struct Maps{
bool valid = false;
uint64_t wall_hash = 0;
int16_t atk[2][CELLS];
int16_t sup[MAX_SUPPLY][CELLS];
int n_key[2];
int16_t key_cell[2][24];
int16_t key_val[2][24];
};
static Maps M;
static void flood(const State& s,int16_t* dist){
static int queue[CELLS * 16];
int head = 0,tail = 0;
for(int c = 0;c < CELLS;++c)
if(dist[c]< BIG)queue[tail++]= c;
while(head < tail){
const int c = queue[head++ %(CELLS * 16)];
for(int d = 0;d < 4;++d){
const int n = c + DIR_DELTA[d];
const uint8_t g = s.grid[n];
if(g != EMPTY && g != BRICK)continue;
const int nd = dist[c]+(g == BRICK ? 12 : 4);
if(nd < dist[n]){dist[n]= int16_t(nd);queue[tail++ %(CELLS * 16)]= n;}
}
}
}
static void build_maps(const State& s){
if(M.valid && M.wall_hash == s.wall_hash)return;
M.valid = true;
M.wall_hash = s.wall_hash;
for(int seat = 0;seat < 2;++seat){
int16_t* dist = M.atk[seat];
for(int c = 0;c < CELLS;++c)dist[c]= BIG;
M.n_key[seat]= 0;
for(int d = 0;d < 4;++d){
int bricks = 0,k = 1;
for(int c = s.base_pos[1 - seat]+ DIR_DELTA[d];;c += DIR_DELTA[d],++k){
const uint8_t g = s.grid[c];
if(g == BRICK){
if(M.n_key[seat]< 24){
M.key_cell[seat][M.n_key[seat]]= int16_t(c);
M.key_val[seat][M.n_key[seat]++]= int16_t(bricks == 0 ? 16 : 8);
}
dist[c]= int16_t(8 * bricks + 12 +(k > 2 ? 2 *(k - 2): 0));
++bricks;
continue;
}
if(g == WATER)continue;
if(g != EMPTY)break;
dist[c]= int16_t(8 * bricks +(k > 2 ? 2 *(k - 2): 0));
}
}
flood(s,dist);
}
for(int k = 0;k < s.n_supply;++k){
for(int c = 0;c < CELLS;++c)M.sup[k][c]= BIG;
M.sup[k][s.supply[k]]= 0;
flood(s,M.sup[k]);
}
}
static uint8_t root_grid[CELLS];
static int soon_mask;
static int16_t atk_of[6][CELLS];
static void flood(const State& s,int16_t* dist);
static int still[6],still_pos[6];
static void build_tank_maps(const State& s){
for(int j = 0;j < 6;++j){
if(s.tank[j].alive && s.tank[j].pos == still_pos[j])still[j]+= 1;
else still[j]= 0;
still_pos[j]= s.tank[j].alive ? s.tank[j].pos : -1;
}
for(int i = 0;i < 6;++i){
const int seat = i / 3;
int16_t* dist = atk_of[i];
for(int c = 0;c < CELLS;++c)dist[c]= BIG;
for(int d = 0;d < 4;++d){
int bricks = 0,k = 1;
for(int c = s.base_pos[1 - seat]+ DIR_DELTA[d];;c += DIR_DELTA[d],++k){
const uint8_t g = s.grid[c];
if(g == BRICK){
dist[c]= int16_t(8 * bricks + 12 +(k > 2 ? ADDITIONAL_SPAR_SP_FAR_COST *(k - 2): 0));
++bricks;
continue;
}
if(g == WATER)continue;
if(g != EMPTY)break;
dist[c]= int16_t(8 * bricks +(k > 2 ? ADDITIONAL_SPAR_SP_FAR_COST *(k - 2): 0));
bool blocked = false;
for(int j = 0;j < 6;++j)blocked |= j != i && s.tank[j].alive && s.tank[j].pos == c && still[j]>= ADDITIONAL_SPAR_SP_BLOCK_STILL;
if(blocked)break;
}
}
static int queue[CELLS * 16];
static int16_t extra[CELLS];
for(int c = 0;c < CELLS;++c)extra[c]= 0;
for(int j = 0;j < 6;++j)
if(j != i && s.tank[j].alive)extra[s.tank[j].pos]= int16_t(still[j]>= 3 ? 24 : 8);
int head = 0,tail = 0;
for(int c = 0;c < CELLS;++c)
if(dist[c]< BIG)queue[tail++]= c;
while(head < tail){
const int c = queue[head++ %(CELLS * 16)];
for(int d = 0;d < 4;++d){
const int n = c + DIR_DELTA[d];
const uint8_t g = s.grid[n];
if(g != EMPTY && g != BRICK)continue;
const int nd = dist[c]+(g == BRICK ? 12 : 4)+ extra[c];
if(nd < dist[n]){dist[n]= int16_t(nd);queue[tail++ %(CELLS * 16)]= n;}
}
}
}
}
static inline int progress_cost(int i,int pos){
int a = atk_of[i][pos];
if(a > 240)a = 240;
return i % 3 == FAST ? a * 5 / 8 : a;
}
static int root_me;
static int hunt_target[6];
static int16_t hunt_map[3][CELLS];
static int16_t post_map[CELLS];
static bool holding[6];
static int prev_target[3];
static int stale_turns;
static int stale_hp[2],stale_kills[2];
static uint64_t stale_walls;
static void assign_roles(const State& s,int me){
if(s.base_hp[0]!= stale_hp[0]|| s.base_hp[1]!= stale_hp[1]|| s.kills[0]!= stale_kills[0]||
s.kills[1]!= stale_kills[1]|| s.wall_hash != stale_walls){
stale_turns = 0;
stale_hp[0]= s.base_hp[0];stale_hp[1]= s.base_hp[1];
stale_kills[0]= s.kills[0];stale_kills[1]= s.kills[1];
stale_walls = s.wall_hash;
}else{
stale_turns += 1;
}
int max_hunters = ADDITIONAL_SPAR_SP_MAX_HUNTERS;
if(stale_turns >= 10 && max_hunters > 1)max_hunters = 1;
if(stale_turns >= 30)max_hunters = 0;
int n_hunters = 0;
#if ADDITIONAL_SPAR_SP_EMERGENCY
{
int urgent = 0;
for(int k = 0;k < 3;++k){
const int foe = 3 *(1 - me)+ k;
if(!s.tank[foe].alive)continue;
int c = M.atk[1 - me][s.tank[foe].pos];
if(k == FAST)c = c * 5 / 8;
urgent += c <= ADDITIONAL_SPAR_SP_EMERGENCY;
}
if(urgent > max_hunters && stale_turns < 10)max_hunters = urgent;
}
#endif
root_me = me;
for(int i = 0;i < 6;++i){hunt_target[i]= -1;holding[i]= false;}
const int foe0 = 3 *(1 - me);
int threat[3],threat_cost[3],n = 0;
for(int k = 0;k < 3;++k){
if(!s.tank[foe0 + k].alive)continue;
int c = M.atk[1 - me][s.tank[foe0 + k].pos];
if(k == FAST)c = c * 5 / 8;
if(c > ADDITIONAL_SPAR_SP_THREAT)continue;
int at = n++;
while(at > 0 && threat_cost[at - 1]> c){threat[at]= threat[at - 1];threat_cost[at]= threat_cost[at - 1];--at;}
threat[at]= k;
threat_cost[at]= c;
}
bool used[3]={};
int now_target[3]={-1,-1,-1};
for(int q = 0;q < n && n_hunters < max_hunters;++q){
const int k = threat[q],foe = foe0 + k;
int16_t* dist = hunt_map[k];
for(int c = 0;c < CELLS;++c)dist[c]= BIG;
for(int d = 0;d < 4;++d){
int steps = 1;
for(int c = s.tank[foe].pos + DIR_DELTA[d];s.grid[c]== EMPTY || s.grid[c]== WATER;c += DIR_DELTA[d],++steps)
if(s.grid[c]== EMPTY)dist[c]= int16_t(steps <= 2 ? 0 : 2 *(steps - 2));
}
flood(s,dist);
int best = -1,best_eta = ADDITIONAL_SPAR_SP_HUNT_MAX;
for(int t = 0;t < 3;++t){
if(used[t])continue;
const int i = 3 * me + t;
const bool alive = s.tank[i].alive;
const int pos = alive ? s.tank[i].pos : s.spawn[i];
int eta = dist[pos]>= BIG ? BIG :(t == FAST ? dist[pos]* 5 / 8 : dist[pos]);
if(!alive)eta += 4 * respawn_in(s,i);
if(alive && progress_cost(i,pos)<= 4 && eta > 8)continue;
if(prev_target[t]== foe)eta -= 8;
#if ADDITIONAL_SPAR_SP_GUARDS
if(!(t == ARMOR ||(ADDITIONAL_SPAR_SP_GUARDS >= 2 && t == FAST)|| ADDITIONAL_SPAR_SP_GUARDS == 3))eta += 12;
#endif
if(eta < best_eta){best_eta = eta;best = t;}
}
if(best >= 0){
++n_hunters;
used[best]= true;
hunt_target[3 * me + best]= foe;
now_target[best]= foe;
}
}
for(int t = 0;t < 3;++t)prev_target[t]= now_target[t];
#if ADDITIONAL_SPAR_SP_GUARDS
{
bool coming = false;
for(int k = 0;k < 3;++k){
if(!s.tank[foe0 + k].alive)continue;
int c = M.atk[1 - me][s.tank[foe0 + k].pos];
if(k == FAST)c = c * 5 / 8;
coming |= c <= ADDITIONAL_SPAR_SP_GUARD_RANGE;
}
static const int order[3]={ARMOR,FAST,FIRE};
int n_guards = ADDITIONAL_SPAR_SP_GUARDS;
if(ADDITIONAL_SPAR_SP_GUARDS == 9){
int n_coming = 0;
for(int k = 0;k < 3;++k){
int c = M.atk[1 - me][s.tank[foe0 + k].alive ? s.tank[foe0 + k].pos : s.spawn[foe0 + k]];
if(k == FAST)c = c * 5 / 8;
n_coming += s.tank[foe0 + k].alive && c <= ADDITIONAL_SPAR_SP_GUARD_RANGE;
}
n_guards = n_coming - 1 - n_hunters;
if(n_guards > 2)n_guards = 2;
}
for(int q = 0;q < n_guards && q < 3;++q){
const int i = 3 * me + order[q];
if(coming && hunt_target[i]< 0 && stale_turns < 30)holding[i]= true;
}
for(int c = 0;c < CELLS;++c)post_map[c]= BIG;
const int fwd = me == 0 ? UP : DOWN;
const int post = s.base_pos[me]+ 3 * DIR_DELTA[fwd];
for(int d = 0;d < 4;++d){
const int c = post + DIR_DELTA[d];
if(s.grid[c]== EMPTY)post_map[c]= 0;
}
if(s.grid[post]== EMPTY)post_map[post]= 0;
flood(s,post_map);
}
#endif
}
static inline int tank_cost(const State& s,int i,int pos){
const int target = hunt_target[i];
if(holding[i]){const int a = post_map[pos];return a > 240 ? 240 :(i % 3 == FAST ? a * 5 / 8 : a);}
if(target < 0)return progress_cost(i,pos);
if(!s.tank[target].alive)return 0;
int a = hunt_map[target % 3][pos];
if(a > 240)a = 240;
return i % 3 == FAST ? a * 5 / 8 : a;
}
static int evaluate(const State& s){
if(s.done){
if(s.winner < 0)return 0;
return(s.winner == 0 ? 6000 : -6000)+ 10 *(s.base_hp[0]- s.base_hp[1]);
}
static const int tv[3]={ADDITIONAL_SPAR_SP_TV,ADDITIONAL_SPAR_SP_TV_FIRE,ADDITIONAL_SPAR_SP_TV};
int e[2]={0,0};
for(int seat = 0;seat < 2;++seat){
e[seat]+= 100 * s.base_hp[seat]+ 8 * s.kills[seat]+ 2 * s.stars_eaten[seat];
for(int k = 0;k < M.n_key[seat];++k)
if(s.grid[M.key_cell[seat][k]]!= BRICK && root_grid[M.key_cell[seat][k]]== BRICK)e[seat]+= M.key_val[seat][k];
}
for(int i = 0;i < 6;++i){
const Tank& t = s.tank[i];
const int seat = i / 3,type = i % 3;
if(t.alive){
e[seat]-= tv[type]* tank_cost(s,i,t.pos)/ 4;
e[seat]+= 15 *(t.hp - 1)+ ADDITIONAL_SPAR_SP_LVV * t.level;
}else{
e[seat]-= tv[type]*(tank_cost(s,i,s.spawn[i])+ 4 * respawn_in(s,i))/ 4 + 10;
}
}
#if ADDITIONAL_SPAR_SP_STARS
const int mask = s.star_mask | soon_mask;
if(mask){
for(int k = 0;k < s.n_supply;++k){
if(!((mask >> k)& 1))continue;
for(int seat = 0;seat < 2;++seat){
int best = BIG;
for(int i = 3 * seat;i < 3 * seat + 3;++i){
if(!s.tank[i].alive || s.tank[i].level >= s.P.max_level)continue;
int a = M.sup[k][s.tank[i].pos];
if(i % 3 == FAST)a = a * 5 / 8;
if(a < best)best = a;
}
const int bonus = ADDITIONAL_SPAR_SP_STARW - best / 2;
if(bonus > 0)e[seat]+= bonus;
}
}
}
#endif
for(int b = 0;b < s.n_bullets;++b){
const Bullet& bl = s.bullet[b];
int c = bl.pos;
for(int k = 1;k <= 8;++k){
c += DIR_DELTA[bl.dir];
const uint8_t g = s.grid[c];
if(g == WATER)continue;
if(g == BASE0 || g == BASE1){e[g - BASE0]-= 70;break;}
if(g != EMPTY)break;
int v = -1;
for(int i = 0;i < 6;++i)
if(s.tank[i].alive && s.tank[i].pos == c)v = i;
if(v >= 0){
e[v / 3]-= k <= 2 ? 20 : 5;
break;
}
}
}
return e[0]- e[1];
}
struct Scan{
bool useful;
int prior;
bool danger;
};
static Scan scan_dir(const State& s,int i,int d){
Scan r{false,0,false};
const int seat = i / 3;
int bricks = 0,c = s.tank[i].pos;
for(int k = 1;k <= 14;++k){
c += DIR_DELTA[d];
const uint8_t g = s.grid[c];
if(g == WATER)continue;
if(g == BRICK){
if(bricks == 0){
bool key = false;
for(int q = 0;q < M.n_key[seat];++q)key |= M.key_cell[seat][q]== c;
if(key && k <= 6){r.useful = true;r.prior = r.prior > 62 ? r.prior : 62;}
else if(k <= 2){r.useful = true;r.prior = r.prior > 30 ? r.prior : 30;}
}
++bricks;
continue;
}
if(g == BASE0 +(1 - seat)){
if(k <= 6){r.useful = true;const int p = 96 - 10 * bricks;r.prior = r.prior > p ? r.prior : p;}
break;
}
if(g == STEEL){
if(bricks == 0 && k <= 2 && s.tank[i].level >= 2 && s.P.level2_steel){r.useful = true;r.prior = r.prior > 25 ? r.prior : 25;}
break;
}
if(g != EMPTY)break;
if(bricks)continue;
for(int b = 0;b < s.n_bullets;++b)
if(s.bullet[b].pos == c && s.bullet[b].dir ==(d ^ 1)&& k <= 6){
r.useful = true;
r.danger = true;
r.prior = r.prior > 85 ? r.prior : 85;
}
int v = -1;
for(int j = 0;j < 6;++j)
if(s.tank[j].alive && s.tank[j].pos == c)v = j;
if(v >= 0){
if(v / 3 == seat)break;
r.useful = true;
const bool armed = can_fire(s,v);
if(k <= 2 && armed)r.danger = true;
const int p = k <= 2 ?(armed ? 88 : 99):(k <= 4 ? 64 : 44);
r.prior = r.prior > p ? r.prior : p;
break;
}
if(k <= 5 && !r.useful){
const int side =(d < 2)? 1 : STRIDE;
for(int j = 3 *(1 - seat);j < 3 *(1 - seat)+ 3;++j)
if(s.tank[j].alive &&(s.tank[j].pos == c + side || s.tank[j].pos == c - side)){r.useful = true;r.prior = 36;}
}
}
return r;
}
struct Node{
int32_t visits;
float total;
int32_t child[MAX_ACTS];
int32_t n[MAX_ACTS];
float sum[MAX_ACTS];
float p[MAX_ACTS];
Cmd act[MAX_ACTS];
uint8_t n_acts;
};
static int goal_cost(const State& s,int i,int pos){
if(hunt_target[i]>= 0)return hunt_map[hunt_target[i]% 3][pos];
if(holding[i])return post_map[pos];
int a = atk_of[i][pos];
#if ADDITIONAL_SPAR_SP_STARS
if(s.tank[i].level < s.P.max_level &&(s.star_mask | soon_mask)){
for(int k = 0;k < s.n_supply;++k)
if(((s.star_mask | soon_mask)>> k)& 1){
const int b = M.sup[k][pos]+ 8;
if(b < a && M.sup[k][pos]<= ADDITIONAL_SPAR_SP_STAR_REACH)a = b;
}
}
#else
(void)s;
#endif
return a;
}
static void build_actions(Node& nd,const State& s,int i){
Cmd list[N_CMDS];
const int n = legal_commands(s,i,list);
const Tank& t = s.tank[i];
Scan sc[4];
bool danger = false;
for(int d = 0;d < 4;++d){sc[d]= scan_dir(s,i,d);danger |= sc[d].danger;}
const int here = goal_cost(s,i,t.pos);
Cmd acts[N_CMDS];
int prior[N_CMDS],m = 0;
int ends[N_CMDS],n_ends = 0;
for(int k = 0;k < n;++k){
const Cmd c = list[k];
int p;
if(c == STAY){
p = 46;
}else if(is_fire(c)){
if(!sc[c - 5].useful)continue;
p = sc[c - 5].prior;
}else if(is_move(c)){
const int to = t.pos + DIR_DELTA[c - 1];
p = 50 +(here - goal_cost(s,i,to))* 2;
if(danger && !sc[c - 1].danger && !sc[(c - 1)^ 1].danger)p += 30;
}else{
const int d1 =(c - 9)>> 2,d2 =(c - 9)& 3;
const int mid = t.pos + DIR_DELTA[d1];
if(s.grid[mid]!= EMPTY)continue;
const int to = mid + DIR_DELTA[d2];
if(to == t.pos){
if(!danger || sc[d1].danger || sc[d2].danger)continue;
p = 70;
}else{
if(s.grid[to]!= EMPTY)continue;
bool dup = false;
for(int q = 0;q < n_ends;++q)dup |= ends[q]== to;
if(dup)continue;
ends[n_ends++]= to;
p = 52 +(here - goal_cost(s,i,to))* 2;
if(danger)p += 20;
}
}
acts[m]= c;
prior[m++]= p;
}
for(int a = 1;a < m;++a)
for(int b = a;b > 0 && prior[b]> prior[b - 1];--b){
const int tp = prior[b];prior[b]= prior[b - 1];prior[b - 1]= tp;
const Cmd tc = acts[b];acts[b]= acts[b - 1];acts[b - 1]= tc;
}
if(m > MAX_ACTS)m = MAX_ACTS;
nd.n_acts = uint8_t(m);
float z = 0;
for(int a = 0;a < m;++a){
nd.act[a]= acts[a];
nd.child[a]= -1;
nd.n[a]= 0;
nd.sum[a]= 0;
nd.p[a]= std::exp(float(prior[a]- prior[0])/ float(ADDITIONAL_SPAR_SP_PRIOR_T));
z += nd.p[a];
}
for(int a = 0;a < m;++a)nd.p[a]/= z;
}
static uint64_t rng_state;
static inline uint32_t rng(){
rng_state ^= rng_state << 13;
rng_state ^= rng_state >> 7;
rng_state ^= rng_state << 17;
return uint32_t(rng_state >> 16);
}
static Cmd rollout_cmd(const State& s,int i){
const Tank& t = s.tank[i];
const int seat = i / 3;
const uint32_t r = rng();
const bool armed = can_fire(s,i);
if(armed &&(r & 7)!= 0){
for(int d = 0;d < 4;++d){
int c = t.pos,bricks = 0;
for(int k = 1;k <= 4;++k){
c += DIR_DELTA[d];
const uint8_t g = s.grid[c];
if(g == WATER)continue;
if(g == BRICK){if(++bricks > 1)break;continue;}
if(g == BASE0 +(1 - seat))return cmd_fire(d);
if(g != EMPTY)break;
if(bricks)continue;
int v = -1;
for(int j = 0;j < 6;++j)
if(s.tank[j].alive && s.tank[j].pos == c)v = j;
if(v >= 0){
if(v / 3 != seat && k <= 2)return cmd_fire(d);
break;
}
}
}
}
const int16_t* dist = hunt_target[i]>= 0 ? hunt_map[hunt_target[i]% 3]:(holding[i]? post_map : atk_of[i]);
int pos = t.pos,dirs[2]={-1,-1};
const int steps =(i % 3 == FAST)? 2 : 1;
for(int k = 0;k < steps;++k){
int best = -1,best_cost = dist[pos];
if(((r >>(8 + 4 * k))& 15)< 16 / ADDITIONAL_SPAR_SP_ROLLOUT_RANDOM){
const int d =(r >>(16 + 2 * k))& 3;
if(s.grid[pos + DIR_DELTA[d]]== EMPTY)best = d;
}else{
for(int d = 0;d < 4;++d){
const int to = pos + DIR_DELTA[d];
if(dist[to]< best_cost){best_cost = dist[to];best = d;}
}
}
if(best < 0)break;
if(s.grid[pos + DIR_DELTA[best]]== BRICK){
if(k == 0 && armed)return cmd_fire(best);
break;
}
dirs[k]= best;
pos += DIR_DELTA[best];
}
if(dirs[0]< 0)return STAY;
if(dirs[1]< 0)return cmd_move(dirs[0]);
return cmd_move2(dirs[0],dirs[1]);
}
static std::vector<Node> pool;
static int pool_used;
static long long total_steps;
static inline int new_node(){
if(pool_used >= int(pool.size()))return -1;
Node& nd = pool[pool_used];
nd.visits = 0;
nd.total = 0;
nd.n_acts = 0;
return pool_used++;
}
static inline int select_action(const Node& nd){
if(nd.visits == 0)return 0;
const float sq = std::sqrt(float(nd.visits))*(ADDITIONAL_SPAR_SP_UCB / 100.0f);
const float fpu = nd.total / float(nd.visits)- ADDITIONAL_SPAR_SP_FPU / 100.0f;
int best = 0;
float best_u = -1e9f;
for(int a = 0;a < nd.n_acts;++a){
const float q = nd.n[a]? nd.sum[a]/ float(nd.n[a]): fpu;
const float u = q + sq * nd.p[a]/ float(1 + nd.n[a]);
if(u > best_u){best_u = u;best = a;}
}
return best;
}
void init(const State& s,int me){
(void)me;
M.valid = false;
total_steps = 0;
for(int t = 0;t < 3;++t)prev_target[t]= -1;
for(int j = 0;j < 6;++j){still[j]= 0;still_pos[j]= -1;}
stale_turns = 0;
stale_hp[0]= stale_hp[1]= stale_kills[0]= stale_kills[1]= -1;
stale_walls = 0;
const size_t want = size_t(ADDITIONAL_SPAR_SP_BUDGET)/ ADDITIONAL_SPAR_SP_DEPTH * 6 + 64;
if(pool.size()!=(want < 600000 ? want : 600000))pool.assign(want < 600000 ? want : 600000,Node{});
build_maps(s);
}
#if ADDITIONAL_SPAR_SP_METHOD == 1
static Orders decide_ibr(const State& s,int me){
static Node nd[6];
Cmd choice[6];
for(int i = 0;i < 6;++i){
choice[i]= STAY;
nd[i].n_acts = 0;
if(!s.tank[i].alive)continue;
build_actions(nd[i],s,i);
choice[i]= nd[i].act[0];
}
const uint64_t seed = rng_state;
for(int round = 0;round < ADDITIONAL_SPAR_SP_IBR_ROUNDS;++round){
bool changed = false;
for(int k = 0;k < 6;++k){
const int i =(round & 1)? 5 - k : k;
if(nd[i].n_acts <= 1)continue;
int best = -1;
long long best_score = 0;
for(int a = 0;a < nd[i].n_acts;++a){
Orders o[2];
for(int j = 0;j < 6;++j)o[j / 3].cmd[j % 3]= j == i ? nd[i].act[a]: choice[j];
State st = s;
step(st,o[0],o[1]);
long long sum = evaluate(st);
rng_state = seed;
for(int d = 1;d < ADDITIONAL_SPAR_SP_DEPTH;++d){
if(st.done){sum += evaluate(st);continue;}
Orders q[2];
for(int j = 0;j < 6;++j)
if(st.tank[j].alive)q[j / 3].cmd[j % 3]= rollout_cmd(st,j);
step(st,q[0],q[1]);
sum += evaluate(st);
}
if(i >= 3)sum = -sum;
if(best < 0 || sum > best_score){best_score = sum;best = a;}
}
if(nd[i].act[best]!= choice[i]){choice[i]= nd[i].act[best];changed = true;}
}
if(!changed)break;
}
Orders out;
for(int type = 0;type < 3;++type)out.cmd[type]= choice[tank_id(me,type)];
return out;
}
#endif
Orders decide(const State& s,int me){
build_maps(s);
build_tank_maps(s);
assign_roles(s,me);
std::memcpy(root_grid,s.grid,sizeof(root_grid));
soon_mask = 0;
{
const int t = s.turn;
int next = s.P.supply_first;
if(t >= next)next +=((t - s.P.supply_first)/ s.P.supply_period + 1)* s.P.supply_period;
if(next - t <= 8)soon_mask =((1 << s.n_supply)- 1)& ~s.star_mask;
}
rng_state = 0x9E3779B97F4A7C15ULL ^(uint64_t(s.turn)* 0x100000001B3ULL)^ s.wall_hash;
for(int k = 0;k < 4;++k)rng();
#if ADDITIONAL_SPAR_SP_METHOD == 1
return decide_ibr(s,me);
#endif
pool_used = 0;
int root[6];
for(int i = 0;i < 6;++i)root[i]= new_node();
int steps = 0,sims = 0;
struct Visit{int32_t node;int8_t act;};
Visit path[6][ADDITIONAL_SPAR_SP_DEPTH];
int path_len[6];
for(;;){
if(steps + ADDITIONAL_SPAR_SP_DEPTH > ADDITIONAL_SPAR_SP_BUDGET)break;
#if ADDITIONAL_SPAR_SP_USE_TIME
if((sims & 15)== 0 && ::ensemble_models::private_budget_left()< ADDITIONAL_SPAR_SP_TIME_MARGIN)break;
#endif
State st = s;
int cur[6];
bool expanded[6]={};
for(int i = 0;i < 6;++i){cur[i]= root[i];path_len[i]= 0;}
int e_sum = 0,depth_done = 0;
for(int depth = 0;depth < ADDITIONAL_SPAR_SP_DEPTH && !st.done;++depth){
Orders o[2];
for(int i = 0;i < 6;++i){
if(!st.tank[i].alive)continue;
Cmd c;
if(cur[i]>= 0){
Node& nd = pool[cur[i]];
if(nd.n_acts == 0)build_actions(nd,st,i);
const int a = select_action(nd);
c = nd.act[a];
path[i][path_len[i]++]= Visit{cur[i],int8_t(a)};
if(nd.child[a]< 0 && !expanded[i]&& nd.n[a]> 0){
nd.child[a]= new_node();
expanded[i]= true;
}
cur[i]= nd.child[a];
}else{
c = rollout_cmd(st,i);
}
o[i / 3].cmd[i % 3]= c;
}
step(st,o[0],o[1]);
++steps;
++depth_done;
e_sum += evaluate(st);
if(st.done)e_sum +=(ADDITIONAL_SPAR_SP_DEPTH - depth_done)* evaluate(st);
}
#if ADDITIONAL_SPAR_SP_PATH_AVG
const int e = e_sum / ADDITIONAL_SPAR_SP_DEPTH;
#else
const int e = evaluate(st);
#endif
const float v = float(e)/(float(ADDITIONAL_SPAR_SP_SQUASH)+ float(e < 0 ? -e : e));
for(int i = 0;i < 6;++i){
const float vi = i < 3 ? v : -v;
for(int k = 0;k < path_len[i];++k){
Node& nd = pool[path[i][k].node];
nd.visits += 1;
nd.total += vi;
nd.n[path[i][k].act]+= 1;
nd.sum[path[i][k].act]+= vi;
}
}
++sims;
}
total_steps += steps;
#ifdef ADDITIONAL_SPAR_SP_DEBUG
if(me == ADDITIONAL_SPAR_SP_DEBUG){
std::fprintf(stderr,"T%d sims=%d nodes=%d\n",s.turn + 1,sims,pool_used);
for(int i = 0;i < 6;++i){
const Node& nd = pool[root[i]];
std::fprintf(stderr,"  tank%d Q=%.3f:",i,nd.visits ? nd.total / nd.visits : 0.f);
for(int a = 0;a < nd.n_acts;++a)
std::fprintf(stderr," %s(p%.2f n%d q%.3f)",cmd_text(nd.act[a]).c_str(),nd.p[a],nd.n[a],nd.n[a]? nd.sum[a]/ nd.n[a]: 0.f);
std::fputc(10,stderr);
}
}
#endif
Orders out;
for(int type = 0;type < 3;++type){
const Node& nd = pool[root[tank_id(me,type)]];
int best = -1;
for(int a = 0;a < nd.n_acts;++a)
if(best < 0 || nd.n[a]> nd.n[best]||(nd.n[a]== nd.n[best]&& nd.n[a]> 0 && nd.sum[a]> nd.sum[best]))best = a;
if(best >= 0)out.cmd[type]= nd.act[best];
}
#if ADDITIONAL_SPAR_SP_FFFIX
for(int round = 0;round < 3;++round){
Orders o[2];
o[me]= out;
for(int type = 0;type < 3;++type){
const Node& nd = pool[root[tank_id(1 - me,type)]];
int best = -1;
for(int a = 0;a < nd.n_acts;++a)
if(best < 0 || nd.n[a]> nd.n[best])best = a;
if(best >= 0)o[1 - me].cmd[type]= nd.act[best];
}
State st = s;
EventLog log;
step(st,o[0],o[1],&log);
int culprit = -1;
for(const Event& e : log.list)
if((e.type == EV_HIT || e.type == EV_HIT_SWAP)&& e.a / 3 == me && e.b / 3 == me && is_fire(out.cmd[e.b % 3]))culprit = e.b;
if(culprit < 0)break;
const Node& nd = pool[root[culprit]];
int best = -1;
for(int a = 0;a < nd.n_acts;++a)
if(!is_fire(nd.act[a])&&(best < 0 || nd.n[a]> nd.n[best]))best = a;
out.cmd[culprit % 3]= best >= 0 ? nd.act[best]: STAY;
}
#endif
return out;
}
}
namespace opponent_gemini{
static bool model_guard_limited=false;
static double budget_left(){
const double left=model_budget_view::remaining();
if(left<=10.)model_guard_limited=true;
return left;
}
using namespace tank;
namespace{
constexpr int INF = 1000000;
struct MapCache{
int dist_lane[6][3][CELLS];
int dist_atk[6][CELLS];
int dist_from_tank[6][CELLS];
int dist_supply[MAX_SUPPLY][CELLS];
bool is_base_shield[2][CELLS];
int base_ray_dist[2][CELLS];
int base_ray_dir[2][CELLS];
int base_ray_lane[2][CELLS];
int assigned_lane[6];
int assigned_supply[6];
int assigned_def_enemy[6];
int assigned_def_urgency[6];
uint8_t root_grid[CELLS];
bool is_atk_wall[2][CELLS];
};
static MapCache G;
static int last_pos_hist[6][6];
static int hist_turn = -1;
inline int manhattan(int c1,int c2){
return std::abs(cell_x(c1)- cell_x(c2))+ std::abs(cell_y(c1)- cell_y(c2));
}
inline bool is_enemy_spawn_soon(const State& s,int seat,int cell){
int op = 1 - seat;
for(int t = 0;t < 3;++t){
int i = tank_id(op,t);
if(s.spawn[i]== cell && !s.tank[i].alive && respawn_in(s,i)<= 1){
return true;
}
}
return false;
}
inline bool is_ally_spawn_soon(const State& s,int seat,int self_id,int cell){
for(int t = 0;t < 3;++t){
int i = tank_id(seat,t);
if(i != self_id && s.spawn[i]== cell && !s.tank[i].alive && respawn_in(s,i)<= 1){
return true;
}
}
return false;
}
inline void count_ray_obstacles(const State& s,int seat,int c,int& bricks,int& steels){
bricks = 0;
steels = 0;
int dist = G.base_ray_dist[seat][c];
if(dist <= 0)return;
int dir = G.base_ray_dir[seat][c];
for(int st = 1;st < dist;++st){
uint8_t g = s.grid[c + st * DIR_DELTA[dir]];
if(g == BRICK)bricks++;
else if(g == STEEL)steels++;
}
}
int enemy_base_threat_level(const State& s,int seat,int e){
if(!s.tank[e].alive)return 0;
int epos = s.tank[e].pos;
if(G.base_ray_dist[seat][epos]> 0){
int bricks = 0,steels = 0;
count_ray_obstacles(s,seat,epos,bricks,steels);
bool can_st =(s.tank[e].level >= 2 && s.P.level2_steel);
if(steels == 0 || can_st){
if(bricks == 0 && G.base_ray_dist[seat][epos]<= 7)return 3;
if(bricks <= 1 && G.base_ray_dist[seat][epos]<= 6)return 3;
if(bricks <= 2 && G.base_ray_dist[seat][epos]<= 6)return 2;
}
}
int eatk = G.dist_atk[e][epos];
int ey = cell_y(epos);
bool in_my_half =(seat == 0)?(ey >= 7):(ey <= 7);
bool deep_in_my_half =(seat == 0)?(ey >= 10):(ey <= 4);
if(eatk <= 7 || deep_in_my_half || manhattan(epos,s.base_pos[seat])<= 5)return 3;
if(eatk <= 14 || in_my_half)return 2;
return 0;
}
void spfa_run(int* dist,int* q,bool* in_q,int& head,int& tail,
const State& s,int seat,int self_id,int move_cost,int wall_cost,bool can_steel,bool add_congestion){
int op = 1 - seat;
while(head != tail){
int u = q[head++];
if(head == CELLS)head = 0;
in_q[u]= false;
int du = dist[u];
for(int dir = 0;dir < 4;++dir){
int v = u + DIR_DELTA[dir];
uint8_t g = s.grid[v];
if(g == OUTSIDE || g == WATER || g == BASE0 || g == BASE1)continue;
if(g == STEEL && !can_steel)continue;
if(G.is_base_shield[seat][v]&&(g == BRICK || g == STEEL))continue;
int step_c =(g == EMPTY)? move_cost : wall_cost;
if(add_congestion && v != s.tank[self_id].pos){
int occ = tank_at(s,v);
if(occ >= 0 && occ != self_id){
step_c +=(seat_of(occ)== seat)? 5 : 4;
}
for(int et = 0;et < 3;++et){
int eid = tank_id(op,et);
if(s.tank[eid].alive && can_fire(s,eid)&& manhattan(v,s.tank[eid].pos)== 1){
if(s.tank[self_id].hp <= s.tank[eid].hp)step_c += 3;
}
}
}
if(du + step_c < dist[v]){
dist[v]= du + step_c;
if(!in_q[v]){
in_q[v]= true;
q[tail++]= v;
if(tail == CELLS)tail = 0;
}
}
}
}
}
void precompute(const State& s){
std::memset(&G,0,sizeof(G));
std::memcpy(G.root_grid,s.grid,sizeof(s.grid));
for(int seat = 0;seat < 2;++seat){
for(int c = 0;c < CELLS;++c)G.base_ray_lane[seat][c]= -1;
}
const int n = s.P.size;
const int R = s.P.ring;
for(int seat = 0;seat < 2;++seat){
int bpos = s.base_pos[seat];
int bx = cell_x(bpos),by = cell_y(bpos);
for(int y = std::max(0,by - R);y <= std::min(n - 1,by + R);++y){
for(int x = std::max(0,bx - R);x <= std::min(n - 1,bx + R);++x){
int c = cell_of(x,y);
if(c != bpos)G.is_base_shield[seat][c]= true;
}
}
for(int d = 0;d < 4;++d){
int to_base_dir = d ^ 1;
int lane =(d == LEFT)? 0 :(d == RIGHT ? 2 : 1);
for(int k = 1;;++k){
int c = bpos + k * DIR_DELTA[d];
uint8_t g = s.grid[c];
if(g == OUTSIDE || g == BASE0 || g == BASE1)break;
if(k <= 3 &&(g == BRICK || g == STEEL)){
G.is_base_shield[seat][c]= true;
}
G.base_ray_dist[seat][c]= k;
G.base_ray_dir[seat][c]= to_base_dir;
G.base_ray_lane[seat][c]= lane;
}
}
}
int q[CELLS];
bool in_q[CELLS]={};
for(int k = 0;k < s.n_supply;++k){
for(int c = 0;c < CELLS;++c)G.dist_supply[k][c]= INF;
int src = s.supply[k];
int head = 0,tail = 0;
G.dist_supply[k][src]= 0;
q[tail++]= src;
in_q[src]= true;
while(head != tail){
int u = q[head++];
if(head == CELLS)head = 0;
in_q[u]= false;
int du = G.dist_supply[k][u];
for(int d = 0;d < 4;++d){
int v = u + DIR_DELTA[d];
uint8_t g = s.grid[v];
if(G.is_base_shield[0][v]|| G.is_base_shield[1][v]){
if(g == BRICK || g == STEEL)continue;
}
int w =(g == EMPTY)? 1 :(g == BRICK ? 3 : INF);
if(w < INF && du + w < G.dist_supply[k][v]){
G.dist_supply[k][v]= du + w;
if(!in_q[v]){
in_q[v]= true;
q[tail++]= v;
if(tail == CELLS)tail = 0;
}
}
}
}
}
for(int i = 0;i < 6;++i){
int seat = seat_of(i);
int op = 1 - seat;
int spd = s.P.types[type_of(i)].speed;
int rl = reload_turns(s,i);
bool can_steel =(s.tank[i].level >= 2 && s.P.level2_steel);
int move_cost =(spd >= 2)? 1 : 2;
int wall_cost = move_cost + 2 + 2 * rl;
for(int c = 0;c < CELLS;++c){
G.dist_atk[i][c]= INF;
G.dist_from_tank[i][c]= INF;
for(int l = 0;l < 3;++l)G.dist_lane[i][l][c]= INF;
}
int bpos = s.base_pos[op];
for(int d = 0;d < 4;++d){
int check_c = bpos + DIR_DELTA[d];
if(s.grid[check_c]== OUTSIDE)continue;
int lane =(d == LEFT)? 0 :(d == RIGHT ? 2 : 1);
int head = 0,tail = 0;
int bricks_b = 0,steels_b = 0;
for(int k = 1;;++k){
int c = bpos + k * DIR_DELTA[d];
uint8_t g = s.grid[c];
if(g == OUTSIDE || g == BASE0 || g == BASE1)break;
if(steels_b > 0 && !can_steel)break;
if(g != WATER &&(g != STEEL || can_steel)){
int walls_b = bricks_b + steels_b;
int shot_interval = 2 + 2 * rl +(k > 2 ?(k - 1): 0);
int post_cost = walls_b * shot_interval;
if(k == 1)post_cost += 0;
else if(k == 2)post_cost += 1;
else post_cost += 2 +(k - 2)* 3;
for(int t = 0;t < 3;++t){
int sp = s.spawn[tank_id(op,t)];
if(sp == c)post_cost += 5;
else if(c == bpos + DIR_DELTA[d]&& sp == bpos + 2 * DIR_DELTA[d]){
post_cost += 3;
}
}
if(g == BRICK || g == STEEL){
post_cost += 2 + 2 * rl;
}
if(post_cost < G.dist_lane[i][lane][c]){
G.dist_lane[i][lane][c]= post_cost;
if(!in_q[c]){
in_q[c]= true;
q[tail++]= c;
if(tail == CELLS)tail = 0;
}
}
}
if(g == BRICK)bricks_b++;
else if(g == STEEL)steels_b++;
}
spfa_run(G.dist_lane[i][lane],q,in_q,head,tail,s,seat,i,move_cost,wall_cost,can_steel,true);
}
for(int c = 0;c < CELLS;++c){
G.dist_atk[i][c]= std::min({G.dist_lane[i][0][c],G.dist_lane[i][1][c],G.dist_lane[i][2][c]});
}
int start_c = s.tank[i].alive ? s.tank[i].pos : s.spawn[i];
int head = 0,tail = 0;
G.dist_from_tank[i][start_c]= 0;
q[tail++]= start_c;
in_q[start_c]= true;
spfa_run(G.dist_from_tank[i],q,in_q,head,tail,s,seat,i,move_cost,wall_cost,can_steel,false);
}
int next_turn = s.turn + 1;
int turns_to_spawn = INF;
if(next_turn <= s.P.supply_first){
turns_to_spawn = s.P.supply_first - next_turn;
}else{
int rem =(next_turn - s.P.supply_first)% s.P.supply_period;
turns_to_spawn =(rem == 0)? 0 :(s.P.supply_period - rem);
}
for(int seat = 0;seat < 2;++seat){
int op = 1 - seat;
for(int tp = 0;tp < 3;++tp){
int i = tank_id(seat,tp);
G.assigned_lane[i]= 1;
G.assigned_supply[i]= -1;
G.assigned_def_enemy[i]= -1;
G.assigned_def_urgency[i]= 0;
}
for(int et = 0;et < 3;++et){
int e = tank_id(op,et);
int th = enemy_base_threat_level(s,seat,e);
if(th >= 2){
int best_t = -1,second_t = -1;
int best_d = INF,second_d = INF;
for(int tp = 0;tp < 3;++tp){
int i = tank_id(seat,tp);
int pos = s.tank[i].alive ? s.tank[i].pos : s.spawn[i];
if(s.tank[i].alive && G.dist_atk[i][pos]<= 1 && s.base_hp[op]<= 4 && s.base_hp[seat]>= s.base_hp[op]+ 3){
continue;
}
int d = G.dist_from_tank[e][pos]+(s.tank[i].alive ? 0 : respawn_in(s,i)* 3);
if(s.tank[i].alive && G.dist_atk[i][pos]<= 2)d += 10;
if(d < best_d){
second_d = best_d;second_t = best_t;
best_d = d;best_t = i;
}else if(d < second_d){
second_d = d;second_t = i;
}
}
if(best_t >= 0 &&(th == 3 || best_d <= 24)){
if(th > G.assigned_def_urgency[best_t]){
G.assigned_def_enemy[best_t]= e;
G.assigned_def_urgency[best_t]= th;
}
}
if(th == 3 &&(s.tank[e].level >= 1 || s.tank[e].hp >= 2)&& second_t >= 0 && second_d <= 14){
if(th > G.assigned_def_urgency[second_t]){
G.assigned_def_enemy[second_t]= e;
G.assigned_def_urgency[second_t]= th;
}
}
}
}
bool sup_taken[MAX_SUPPLY]={};
for(int step_match = 0;step_match < 3;++step_match){
int best_i = -1,best_k = -1,best_d = INF;
for(int tp = 0;tp < 3;++tp){
int i = tank_id(seat,tp);
if(!s.tank[i].alive || s.tank[i].level >= s.P.max_level)continue;
if(G.assigned_supply[i]>= 0)continue;
if(G.assigned_def_urgency[i]>= 3)continue;
if(G.dist_atk[i][s.tank[i].pos]<= 2)continue;
for(int k = 0;k < s.n_supply;++k){
if(sup_taken[k])continue;
bool has_star =((s.star_mask >> k)& 1)!= 0;
if(!has_star && turns_to_spawn > 9)continue;
int ds = G.dist_supply[k][s.tank[i].pos];
int max_reach = has_star ? 10 :(turns_to_spawn + 3);
if(ds <= max_reach && ds < best_d){
best_d = ds;
best_i = i;
best_k = k;
}
}
}
if(best_i >= 0 && best_k >= 0){
G.assigned_supply[best_i]= best_k;
sup_taken[best_k]= true;
}
}
int best_l[3]={0,1,2};
int best_total_cost = INF;
for(int l0 = 0;l0 < 3;++l0){
for(int l1 = 0;l1 < 3;++l1){
for(int l2 = 0;l2 < 3;++l2){
int ls[3]={l0,l1,l2};
int cost = 0;
for(int tp = 0;tp < 3;++tp){
int i = tank_id(seat,tp);
int pos = s.tank[i].alive ? s.tank[i].pos : s.spawn[i];
int d = G.dist_lane[i][ls[tp]][pos];
if(d >= INF / 2)d = 200;
if(ls[tp]!= 1)d -= 2;
cost += d;
}
if(l0 == l1)cost += 16;
if(l0 == l2)cost += 16;
if(l1 == l2)cost += 16;
if(cost < best_total_cost){
best_total_cost = cost;
best_l[0]= l0;best_l[1]= l1;best_l[2]= l2;
}
}
}
}
for(int tp = 0;tp < 3;++tp){
G.assigned_lane[tank_id(seat,tp)]= best_l[tp];
}
for(int c = 0;c < CELLS;++c){
if(s.grid[c]!= BRICK && s.grid[c]!= STEEL)continue;
if(G.is_base_shield[seat][c])continue;
if(G.is_base_shield[op][c]|| G.base_ray_dist[op][c]> 0){
G.is_atk_wall[seat][c]= true;
continue;
}
for(int tp = 0;tp < 3;++tp){
int i = tank_id(seat,tp);
if(!s.tank[i].alive)continue;
int pos = s.tank[i].pos;
if(manhattan(pos,c)<= 2){
int lane = G.assigned_lane[i];
if(G.dist_lane[i][lane][c]< G.dist_lane[i][lane][pos]||
G.dist_atk[i][c]< G.dist_atk[i][pos]){
G.is_atk_wall[seat][c]= true;
}
int sup = G.assigned_supply[i];
if(sup >= 0 && G.dist_supply[sup][c]< G.dist_supply[sup][pos]){
G.is_atk_wall[seat][c]= true;
}
int def_e = G.assigned_def_enemy[i];
if(def_e >= 0 && G.dist_from_tank[def_e][c]< G.dist_from_tank[def_e][pos]){
G.is_atk_wall[seat][c]= true;
}
}
}
}
}
}
struct RayHit{
int kind;
int dist;
int target_id;
int cell;
};
RayHit trace_shot(const State& s,int seat,int self_id,int pos,int dir){
int opp_dir = dir ^ 1;
for(int step = 1;;++step){
int c = pos + step * DIR_DELTA[dir];
for(int k = 0;k < s.n_bullets;++k){
if(s.bullet[k].pos == c && s.bullet[k].dir == opp_dir){
return RayHit{7,step,k,c};
}
}
uint8_t g = s.grid[c];
if(g == OUTSIDE)return RayHit{0,step,-1,c};
if(g == BASE0 + seat)return RayHit{3,step,seat,c};
if(g == BASE0 +(1 - seat))return RayHit{4,step,1 - seat,c};
if(g == BRICK)return RayHit{1,step,-1,c};
if(g == STEEL)return RayHit{2,step,-1,c};
if(g == EMPTY){
int tid = tank_at(s,c);
if(tid >= 0 && tid != self_id){
if(seat_of(tid)== seat)return RayHit{5,step,tid,c};
else return RayHit{6,step,tid,c};
}
}
}
}
struct MoveSafety{
int p1,p2;
int existing_bullet_hits;
int direct_enemy_threat;
int cross_enemy_threat;
int cancels_enemy_bullet;
int cancels_base_bullet;
int intercepts_enemy_fire;
};
MoveSafety check_cmd_safety(const State& s,int i,Cmd c){
MoveSafety ms{};
const Tank& t = s.tank[i];
int seat = seat_of(i);
int op = 1 - seat;
int pos = t.pos;
ms.p1 = pos;
ms.p2 = pos;
bool fired = false;
int fire_dir = -1;
int f1 = NO_CELL,f2 = NO_CELL;
if(is_move(c)){
int d1 = c - 1;
int n1 = pos + DIR_DELTA[d1];
if(s.grid[n1]== EMPTY && tank_at(s,n1)< 0)ms.p1 = n1;
ms.p2 = ms.p1;
}else if(is_move2(c)){
int d1 =(c - 9)>> 2,d2 =(c - 9)& 3;
int n1 = pos + DIR_DELTA[d1];
if(s.grid[n1]== EMPTY && tank_at(s,n1)< 0)ms.p1 = n1;
int n2 = ms.p1 + DIR_DELTA[d2];
if(s.grid[n2]== EMPTY &&(n2 == pos || tank_at(s,n2)< 0))ms.p2 = n2;
}else if(is_fire(c)&& can_fire(s,i)){
fired = true;
fire_dir = c - 5;
f1 = pos + DIR_DELTA[fire_dir];
if((s.grid[f1]== EMPTY || s.grid[f1]== WATER)&& tank_at(s,f1)< 0){
f2 = f1 + DIR_DELTA[fire_dir];
}
}
for(int k = 0;k < s.n_bullets;++k){
const Bullet& b = s.bullet[k];
int b1 = b.pos + DIR_DELTA[b.dir];
if(fired){
bool cancel1 =(b.dir ==(fire_dir ^ 1)&& b.pos == f1)||(b1 == f1 &&(s.grid[f1]== EMPTY || s.grid[f1]== WATER));
if(cancel1){
if(seat_of(b.owner)!= seat){
ms.cancels_enemy_bullet++;
if(G.base_ray_dist[seat][b.pos]> 0 && G.base_ray_dir[seat][b.pos]== b.dir){
ms.cancels_base_bullet++;
}
}
continue;
}
}
if(ms.p1 != pos && b1 == pos && ms.p1 == b.pos){
ms.existing_bullet_hits++;
continue;
}
if(b1 == ms.p1 && s.grid[b1]== EMPTY){
ms.existing_bullet_hits++;
continue;
}
uint8_t g1 = s.grid[b1];
if(g1 != EMPTY && g1 != WATER)continue;
int v1 = tank_at(s,b1);
if(v1 >= 0 && v1 != i)continue;
int b2 = b1 + DIR_DELTA[b.dir];
if(fired && f2 != NO_CELL){
bool cancel2 =(b.dir ==(fire_dir ^ 1)&& b1 == f2)||(b2 == f2 &&(s.grid[f2]== EMPTY || s.grid[f2]== WATER));
if(cancel2){
if(seat_of(b.owner)!= seat){
ms.cancels_enemy_bullet++;
if(G.base_ray_dist[seat][b.pos]> 0 && G.base_ray_dir[seat][b.pos]== b.dir){
ms.cancels_base_bullet++;
}
}
continue;
}
}
if(ms.p2 != ms.p1 && b2 == ms.p1 && ms.p2 == b1){
ms.existing_bullet_hits++;
continue;
}
if(b2 == ms.p2 && s.grid[b2]== EMPTY){
ms.existing_bullet_hits++;
continue;
}
}
for(int t_idx = 0;t_idx < 3;++t_idx){
int e = tank_id(op,t_idx);
if(!s.tank[e].alive || !can_fire(s,e))continue;
int epos = s.tank[e].pos;
for(int ed = 0;ed < 4;++ed){
int eb1 = epos + DIR_DELTA[ed];
if(fired){
bool cancel1 =(ed ==(fire_dir ^ 1)&& epos == f1)||(eb1 == f1 &&(s.grid[f1]== EMPTY || s.grid[f1]== WATER));
if(cancel1){
RayHit erh = trace_shot(s,op,e,epos,ed);
if(erh.kind == 6 || erh.kind == 4 ||(erh.kind == 1 && erh.dist <= 2)){
ms.intercepts_enemy_fire++;
}
continue;
}
}
bool hit = false;
if(eb1 == ms.p1 && s.grid[eb1]== EMPTY){
hit = true;
}else{
uint8_t g1 = s.grid[eb1];
int v1 = tank_at(s,eb1);
if((g1 == EMPTY || g1 == WATER)&&(v1 < 0 || v1 == i)){
int eb2 = eb1 + DIR_DELTA[ed];
bool cancelled2 = false;
if(fired && f2 != NO_CELL){
if((ed ==(fire_dir ^ 1)&& eb1 == f2)||(eb2 == f2 &&(s.grid[f2]== EMPTY || s.grid[f2]== WATER))){
cancelled2 = true;
RayHit erh = trace_shot(s,op,e,epos,ed);
if(erh.kind == 6 || erh.kind == 4 ||(erh.kind == 1 && erh.dist <= 2)){
ms.intercepts_enemy_fire++;
}
}
}
if(!cancelled2 && eb2 == ms.p2 && s.grid[eb2]== EMPTY){
hit = true;
}
}
}
if(hit){
RayHit erh = trace_shot(s,op,e,epos,ed);
if(erh.kind == 6 || erh.kind == 4 ||(erh.kind == 1 && erh.dist <= 2)){
ms.direct_enemy_threat++;
}else{
ms.cross_enemy_threat++;
}
}
}
}
return ms;
}
double score_single_cmd(const State& s,int i,Cmd c,bool use_global_maps = true){
const Tank& t = s.tank[i];
if(!t.alive)return 0.0;
int seat = seat_of(i);
int op = 1 - seat;
int pos = t.pos;
MoveSafety ms = check_cmd_safety(s,i,c);
if(is_move2(c)){
if(ms.p1 == pos && ms.p2 == pos)return -50000.0;
if(ms.p2 == pos && ms.p1 != pos)return -400.0;
}
double score = 0.0;
if(ms.existing_bullet_hits > 0){
if(ms.existing_bullet_hits >= t.hp){
score -= 6500.0 + 1800.0 * t.level;
}else{
score -= 2200.0 * ms.existing_bullet_hits;
}
}
if(ms.direct_enemy_threat > 0){
if(ms.direct_enemy_threat >= t.hp){
score -= 1350.0 + 500.0 * t.level;
}else{
score -= 380.0 * ms.direct_enemy_threat;
}
}
if(ms.cross_enemy_threat > 0){
if(t.hp == 1)score -= 180.0 * ms.cross_enemy_threat;
else score -= 45.0 * ms.cross_enemy_threat;
}
if(ms.cancels_enemy_bullet > 0){
score += 240.0 * ms.cancels_enemy_bullet;
}
if(ms.cancels_base_bullet > 0){
score += 1350.0 * ms.cancels_base_bullet;
}
if(ms.intercepts_enemy_fire > 0){
score += 320.0 * ms.intercepts_enemy_fire;
}
if(is_enemy_spawn_soon(s,seat,ms.p2)){
score -= 9000.0;
}
if(is_ally_spawn_soon(s,seat,i,ms.p2)){
score -= 1200.0;
}
bool is_stuck =(last_pos_hist[i][0]== pos && last_pos_hist[i][1]== pos && last_pos_hist[i][2]== pos);
if(is_fire(c)){
if(!can_fire(s,i))return -1e9;
int d = c - 5;
RayHit rh = trace_shot(s,seat,i,pos,d);
if(rh.kind == 3){
return -100000.0;
}
if(rh.kind == 5){
return -25000.0;
}
if(rh.kind == 1){
if(G.is_base_shield[seat][rh.cell]){
return -25000.0;
}else if(G.is_base_shield[op][rh.cell]||(G.base_ray_dist[op][pos]> 0 && G.base_ray_dir[op][pos]== d)){
if(rh.dist <= 2)score += 620.0;
else if(rh.dist <= 4)score += 350.0;
else score += 80.0;
}else if(rh.dist <= 2){
int lane = G.assigned_lane[i];
if(!use_global_maps || G.is_atk_wall[seat][rh.cell]||
G.dist_lane[i][lane][rh.cell]< G.dist_lane[i][lane][pos]||
G.dist_atk[i][rh.cell]< G.dist_atk[i][pos]){
score += 360.0;
}else{
score += 50.0;
}
}else{
score -=(ms.intercepts_enemy_fire > 0 ? 0.0 : 160.0);
}
}else if(rh.kind == 2){
if(t.level >= 2 && s.P.level2_steel){
if(G.is_base_shield[seat][rh.cell]){
return -25000.0;
}
int lane = G.assigned_lane[i];
if(rh.dist <= 3 &&(!use_global_maps || G.is_atk_wall[seat][rh.cell]||
G.dist_lane[i][lane][rh.cell]< G.dist_lane[i][lane][pos]||
G.dist_atk[i][rh.cell]< G.dist_atk[i][pos]|| G.base_ray_dist[op][pos]> 0)){
score += 420.0;
}else{
score -=(ms.intercepts_enemy_fire > 0 ? 0.0 : 100.0);
}
}else{
score -=(ms.intercepts_enemy_fire > 0 ? 0.0 : 250.0);
}
}else if(rh.kind == 4){
int next_c = pos + DIR_DELTA[d];
bool can_walk_closer =(s.grid[next_c]== EMPTY && tank_at(s,next_c)< 0);
if(rh.dist <= 2){
score += 1300.0;
}else if(rh.dist <= 4){
score += can_walk_closer ? 620.0 : 980.0;
}else{
score += can_walk_closer ? 60.0 : 620.0;
}
}else if(rh.kind == 6){
int enemy_id = rh.target_id;
const Tank& et = s.tank[enemy_id];
bool enemy_can_fire = can_fire(s,enemy_id);
int next_c = pos + DIR_DELTA[d];
bool can_advance =(s.grid[next_c]== EMPTY && tank_at(s,next_c)< 0);
int ethreat = enemy_base_threat_level(s,seat,enemy_id);
double priority_bonus = ethreat * 320.0 + et.level * 240.0 +(type_of(enemy_id)== FIRE ? 120.0 : 0.0);
if(G.base_ray_dist[op][pos]> 0 && G.base_ray_dir[op][pos]== d){
priority_bonus += 450.0;
}
if(rh.dist == 1){
score +=(enemy_can_fire ? 750.0 : 1200.0)+ priority_bonus;
if(et.hp == 1)score += 450.0;
}else if(rh.dist == 2){
score +=(enemy_can_fire ? 600.0 : 1000.0)+ priority_bonus;
if(et.hp == 1)score += 350.0;
}else if(rh.dist == 3){
if(is_stuck && can_advance)score -= 150.0;
else score +=(can_advance ? 30.0 : 180.0)+ priority_bonus * 0.25;
}else{
if(can_advance)score -= 180.0;
else if(bullet_cap(s,i)> 1)score += 60.0;
else score -= 100.0;
}
}else if(rh.kind == 7){
if(rh.dist <= 3)score += 260.0;
else score -= 60.0;
}else{
score -=(ms.intercepts_enemy_fire > 0 ? 0.0 : 240.0);
}
}
if(use_global_maps){
int def_e = G.assigned_def_enemy[i];
int def_urg = G.assigned_def_urgency[i];
int sup_k = G.assigned_supply[i];
int lane = G.assigned_lane[i];
if(def_e >= 0 && s.tank[def_e].alive){
int de_now = G.dist_from_tank[def_e][pos];
int de_next = G.dist_from_tank[def_e][ms.p2];
double w =(def_urg == 3)? 125.0 : 75.0;
if(de_next < INF){
score +=(de_now - de_next)* w - de_next * 4.0;
}
}
if(sup_k >= 0 && t.level < s.P.max_level){
bool has_star =((s.star_mask >> sup_k)& 1)!= 0;
int sup_cell = s.supply[sup_k];
int ds_now = G.dist_supply[sup_k][pos];
int ds_next = G.dist_supply[sup_k][ms.p2];
if(has_star && ms.p2 == sup_cell){
score += 1050.0;
}else{
double w = has_star ? 68.0 : 48.0;
score +=(ds_now - ds_next)* w - ds_next * 4.0;
if(ms.p2 == sup_cell)score += 180.0;
}
}
int dl_now = G.dist_lane[i][lane][pos];
int dl_next = G.dist_lane[i][lane][ms.p2];
int da_now = G.dist_atk[i][pos];
int da_next = G.dist_atk[i][ms.p2];
double lane_w =(def_e >= 0 && def_urg == 3)? 0.0 :(def_e >= 0 ? 12.0 :(sup_k >= 0 ? 22.0 : 42.0));
double atk_w  =(def_e >= 0 && def_urg == 3)? 0.0 :(def_e >= 0 ?  8.0 :(sup_k >= 0 ? 12.0 : 20.0));
if(dl_next < INF){
score +=(dl_now - dl_next)* lane_w - dl_next *(lane_w > 0 ? 1.5 : 0.0);
}
if(da_next < INF){
score +=(da_now - da_next)* atk_w - da_next *(atk_w > 0 ? 1.0 : 0.0);
}
if(is_stuck && ms.p2 == pos && !is_fire(c)){
score -= 120.0;
}
for(int h = 0;h < 4;++h){
if(ms.p2 == last_pos_hist[i][h]&& ms.p2 != pos){
score -= 18.0;
}
}
}
return score;
}
Orders fast_rollout_orders(const State& s,int seat){
Orders ord;
int chosen_p1[3]={-1,-1,-1};
int chosen_p2[3]={-1,-1,-1};
bool base_shot_taken = false;
int op = 1 - seat;
for(int type = 0;type < 3;++type){
int i = tank_id(seat,type);
if(!s.tank[i].alive){
ord.cmd[type]= STAY;
continue;
}
Cmd list[N_CMDS];
int n = legal_commands(s,i,list);
double best_sc = -1e18;
Cmd best_c = STAY;
for(int k = 0;k < n;++k){
Cmd c = list[k];
double sc = score_single_cmd(s,i,c,true);
MoveSafety ms = check_cmd_safety(s,i,c);
for(int prev = 0;prev < type;++prev){
if(ms.p1 == chosen_p1[prev]|| ms.p2 == chosen_p2[prev]){
sc -= 600.0;
}
}
if(is_fire(c)&& base_shot_taken){
int d = c - 5;
int pos = s.tank[i].pos;
if(G.base_ray_dist[op][pos]> 0 && G.base_ray_dir[op][pos]== d){
int b_cnt = 0,s_cnt = 0;
count_ray_obstacles(s,op,pos,b_cnt,s_cnt);
if(b_cnt == 0 && s_cnt == 0)sc -= 950.0;
}
}
if(sc > best_sc){
best_sc = sc;
best_c = c;
}
}
ord.cmd[type]= best_c;
MoveSafety ms = check_cmd_safety(s,i,best_c);
chosen_p1[type]= ms.p1;
chosen_p2[type]= ms.p2;
if(is_fire(best_c)){
int d = best_c - 5;
int pos = s.tank[i].pos;
if(G.base_ray_dist[op][pos]> 0 && G.base_ray_dir[op][pos]== d && G.base_ray_dist[op][pos]<= 2){
int b_cnt = 0,s_cnt = 0;
count_ray_obstacles(s,op,pos,b_cnt,s_cnt);
if(b_cnt == 0 && s_cnt == 0)base_shot_taken = true;
}
}
}
return ord;
}
double evaluate_state(const State& s,int me){
int op = 1 - me;
if(s.done){
if(s.winner == me){
return 1000000.0 +(300 - s.turn)* 500.0 +(s.base_hp[me]- s.base_hp[op])* 2000.0 +(s.kills[me]- s.kills[op])* 200.0;
}else if(s.winner == op){
return -1000000.0 -(300 - s.turn)* 500.0 +(s.base_hp[me]- s.base_hp[op])* 2000.0 +(s.kills[me]- s.kills[op])* 200.0;
}else{
return 0.0;
}
}
double val = 0.0;
int my_bhp = s.base_hp[me];
int op_bhp = s.base_hp[op];
val +=(my_bhp - op_bhp)* 3800.0;
if(my_bhp <= 6)val -=(7 - my_bhp)* 1400.0;
if(op_bhp <= 6)val +=(7 - op_bhp)* 1400.0;
val +=(s.kills[me]- s.kills[op])* 950.0;
val +=(s.stars_eaten[me]- s.stars_eaten[op])* 850.0;
for(int seat = 0;seat < 2;++seat){
double sgn =(seat == me)? 1.0 : -1.0;
int bpos = s.base_pos[seat];
for(int d = 0;d < 4;++d){
int c1 = bpos + DIR_DELTA[d];
if(s.grid[c1]== BRICK)val += sgn * 360.0;
}
}
if(s.wall_hash != 0){
for(int c = 0;c < CELLS;++c){
if(G.root_grid[c]!= EMPTY && s.grid[c]== EMPTY){
if(G.is_base_shield[me][c])val -= 500.0;
else if(G.is_base_shield[op][c])val += 420.0;
else{
if(G.is_atk_wall[me][c])val += 260.0;
if(G.is_atk_wall[op][c])val -= 220.0;
}
}
}
}
for(int k = 0;k < s.n_bullets;++k){
const Bullet& b = s.bullet[k];
for(int seat = 0;seat < 2;++seat){
if(G.base_ray_dist[seat][b.pos]> 0 && G.base_ray_dir[seat][b.pos]== b.dir){
int dist = G.base_ray_dist[seat][b.pos];
int bricks_in_way = 0,steels_in_way = 0;
count_ray_obstacles(s,seat,b.pos,bricks_in_way,steels_in_way);
if(steels_in_way == 0 || b.steel){
double sgn =(seat == me)? -1.0 : 1.0;
if(bricks_in_way == 0 && dist <= 6){
val += sgn *(1650.0 - dist * 180.0);
}else if(bricks_in_way == 1 && dist <= 4){
val += sgn * 340.0;
}
}
}
}
}
int best_supply_dist[2][MAX_SUPPLY];
for(int k = 0;k < s.n_supply;++k){
best_supply_dist[0][k]= INF;
best_supply_dist[1][k]= INF;
}
int best_atk_dist[2]={INF,INF};
int second_atk_dist[2]={INF,INF};
int lanes_occupied[2][3]={};
for(int i = 0;i < 6;++i){
int seat = seat_of(i);
int op_seat = 1 - seat;
double sgn =(seat == me)? 1.0 : -1.0;
const Tank& t = s.tank[i];
if(!t.alive){
int wait = respawn_in(s,i);
int sp_dist = G.dist_atk[i][s.spawn[i]];
if(sp_dist >= INF / 2)sp_dist = 30;
val -= sgn *(900.0 + wait * 260.0 + sp_dist * 16.0);
continue;
}
double tank_val = 1150.0 + t.hp * 580.0;
if(t.level == 1)tank_val += 1350.0;
else if(t.level >= 2)tank_val += 2700.0;
if(can_fire(s,i)){
tank_val += 110.0;
}else{
for(int k = 0;k < s.n_bullets;++k){
if(s.bullet[k].owner == i){
int bp = s.bullet[k].pos;
int bd = s.bullet[k].dir;
int steps_left = 0;
for(int st = 1;st <= 12;++st){
int nc = bp + st * DIR_DELTA[bd];
uint8_t g = s.grid[nc];
if(g != EMPTY && g != WATER)break;
if(tank_at(s,nc)>= 0)break;
steps_left++;
}
if(steps_left >= 3 && bullet_cap(s,i)== 1){
tank_val -= steps_left * 35.0;
}
}
}
}
int survival_options = 0;
Cmd list[N_CMDS];
int nc = legal_commands(s,i,list);
for(int k = 0;k < nc;++k){
MoveSafety ms = check_cmd_safety(s,i,list[k]);
if(ms.existing_bullet_hits == 0 && !is_enemy_spawn_soon(s,seat,ms.p2)){
survival_options++;
}
}
if(survival_options == 0){
if(t.hp <= 1)tank_val -=(1150.0 + t.hp * 580.0 + t.level * 1350.0 + 750.0);
else tank_val -= 650.0;
}else if(survival_options <= 2){
tank_val -= 120.0;
}
int th = enemy_base_threat_level(s,op_seat,i);
if(th == 3){
tank_val += 550.0 + t.hp * 200.0 +(can_fire(s,i)? 200.0 : 0.0);
}else if(th == 2){
tank_val += 240.0 + t.hp * 100.0;
}
val += sgn * tank_val;
int d_atk = G.dist_atk[i][t.pos];
if(G.base_ray_dist[op_seat][t.pos]> 0){
int rdist = G.base_ray_dist[op_seat][t.pos];
int rbricks = 0,rsteels = 0;
count_ray_obstacles(s,op_seat,t.pos,rbricks,rsteels);
bool can_st =(t.level >= 2 && s.P.level2_steel);
if(rsteels == 0 || can_st){
int cur_walls = rbricks + rsteels;
int dynamic_d = cur_walls * 4 +(rdist <= 2 ? 0 :(rdist - 2)* 2);
if(dynamic_d < d_atk)d_atk = dynamic_d;
int rlane = G.base_ray_lane[op_seat][t.pos];
if(rlane >= 0 && rdist <= 6 && cur_walls <= 1){
lanes_occupied[seat][rlane]++;
}
}
}
if(d_atk < best_atk_dist[seat]){
second_atk_dist[seat]= best_atk_dist[seat];
best_atk_dist[seat]= d_atk;
}else if(d_atk < second_atk_dist[seat]){
second_atk_dist[seat]= d_atk;
}
int def_e = G.assigned_def_enemy[i];
int def_urg = G.assigned_def_urgency[i];
if(def_e >= 0 && def_urg >= 2 && s.tank[def_e].alive){
int dd = G.dist_from_tank[def_e][t.pos];
if(dd < INF / 2){
val += sgn *(-dd *(def_urg == 3 ? 45.0 : 25.0));
}
}else{
int lane = G.assigned_lane[i];
int dl = G.dist_lane[i][lane][t.pos];
if(dl < INF / 2){
val += sgn *(-dl * 14.0);
}else if(d_atk < INF / 2){
val += sgn *(-d_atk * 14.0);
}
}
if(t.level < s.P.max_level){
for(int k = 0;k < s.n_supply;++k){
int ds = G.dist_supply[k][t.pos];
if(ds < best_supply_dist[seat][k]){
best_supply_dist[seat][k]= ds;
}
}
}
}
for(int seat = 0;seat < 2;++seat){
double sgn =(seat == me)? 1.0 : -1.0;
if(best_atk_dist[seat]< INF / 2){
val += sgn *(1200.0 - best_atk_dist[seat]* 40.0);
}
if(second_atk_dist[seat]< INF / 2){
val += sgn *(650.0 - second_atk_dist[seat]* 24.0);
}
int active_lanes =(lanes_occupied[seat][0]> 0)+(lanes_occupied[seat][1]> 0)+(lanes_occupied[seat][2]> 0);
if(active_lanes >= 2){
val += sgn * 380.0 *(active_lanes - 1);
}
}
int turns_to_spawn = INF;
if(s.turn < s.P.supply_first){
turns_to_spawn = s.P.supply_first - s.turn;
}else{
int rem =(s.turn - s.P.supply_first)% s.P.supply_period;
turns_to_spawn =(rem == 0)? s.P.supply_period :(s.P.supply_period - rem);
}
for(int k = 0;k < s.n_supply;++k){
bool has_star =((s.star_mask >> k)& 1)!= 0;
if(has_star || turns_to_spawn <= 8){
double weight = has_star ? 55.0 :(9 - turns_to_spawn)* 5.5;
if(best_supply_dist[me][k]< INF){
val += std::max(0.0,(13.0 - best_supply_dist[me][k])* weight);
}
if(best_supply_dist[op][k]< INF){
val -= std::max(0.0,(13.0 - best_supply_dist[op][k])* weight);
}
}
}
for(int seat = 0;seat < 2;++seat){
double sgn =(seat == me)? 1.0 : -1.0;
int enemy = 1 - seat;
for(int et = 0;et < 3;++et){
int eid = tank_id(enemy,et);
if(!s.tank[eid].alive)continue;
int th = enemy_base_threat_level(s,seat,eid);
if(th >= 2){
int min_def_dist = INF;
for(int mt = 0;mt < 3;++mt){
int mid = tank_id(seat,mt);
if(s.tank[mid].alive){
int d = G.dist_from_tank[eid][s.tank[mid].pos];
if(d < min_def_dist)min_def_dist = d;
}
}
if(min_def_dist < INF / 2){
double urgency =(th == 3)? 75.0 : 40.0;
val -= sgn * min_def_dist * urgency;
}
}
}
for(int a = 0;a < 3;++a){
int ia = tank_id(seat,a);
if(!s.tank[ia].alive)continue;
for(int b = a + 1;b < 3;++b){
int ib = tank_id(seat,b);
if(!s.tank[ib].alive)continue;
int md = manhattan(s.tank[ia].pos,s.tank[ib].pos);
if(md == 1)val -= sgn * 45.0;
else if(md == 2 &&(cell_x(s.tank[ia].pos)== cell_x(s.tank[ib].pos)||
cell_y(s.tank[ia].pos)== cell_y(s.tank[ib].pos))){
val -= sgn * 25.0;
}
}
}
}
return val;
}
struct ScoredCmd{
Cmd c;
double sc;
MoveSafety ms;
};
struct ScoredOrders{
Orders ord;
double prior;
};
int generate_candidate_orders(const State& s,int seat,ScoredOrders* out,int max_out){
ScoredCmd top_cmds[3][8];
int n_top[3]={0,0,0};
for(int type = 0;type < 3;++type){
int i = tank_id(seat,type);
if(!s.tank[i].alive){
top_cmds[type][0]= ScoredCmd{STAY,0.0,MoveSafety{}};
n_top[type]= 1;
continue;
}
Cmd list[N_CMDS];
int n = legal_commands(s,i,list);
ScoredCmd all[N_CMDS];
int valid_n = 0;
for(int k = 0;k < n;++k){
double sc = score_single_cmd(s,i,list[k],true);
if(sc <= -40000.0)continue;
all[valid_n].c = list[k];
all[valid_n].sc = sc;
all[valid_n].ms = check_cmd_safety(s,i,list[k]);
valid_n++;
}
if(valid_n == 0){
top_cmds[type][0]= ScoredCmd{STAY,0.0,check_cmd_safety(s,i,STAY)};
n_top[type]= 1;
continue;
}
std::sort(all,all + valid_n,[](const ScoredCmd& a,const ScoredCmd& b){
return a.sc > b.sc;
});
int keep = std::min(valid_n,(type == FAST ? 5 : 4));
for(int k = 0;k < keep;++k){
top_cmds[type][k]= all[k];
}
n_top[type]= keep;
}
ScoredOrders pool[120];
int n_pool = 0;
int op = 1 - seat;
for(int a = 0;a < n_top[0];++a){
for(int b = 0;b < n_top[1];++b){
for(int c = 0;c < n_top[2];++c){
const ScoredCmd* scs[3]={&top_cmds[0][a],&top_cmds[1][b],&top_cmds[2][c]};
double total = scs[0]->sc + scs[1]->sc + scs[2]->sc;
for(int u = 0;u < 3;++u){
int iu = tank_id(seat,u);
if(!s.tank[iu].alive)continue;
for(int v = u + 1;v < 3;++v){
int iv = tank_id(seat,v);
if(!s.tank[iv].alive)continue;
if(scs[u]->ms.p1 == scs[v]->ms.p1 || scs[u]->ms.p2 == scs[v]->ms.p2){
total -= 750.0;
}
if(scs[u]->ms.p1 == s.tank[iv].pos && scs[v]->ms.p1 == s.tank[iu].pos){
total -= 750.0;
}
if(is_fire(scs[u]->c)){
int fd = scs[u]->c - 5;
int p = s.tank[iu].pos;
if(p + DIR_DELTA[fd]== scs[v]->ms.p1 || p + 2 * DIR_DELTA[fd]== scs[v]->ms.p2){
total -= 5000.0;
}
}
if(is_fire(scs[v]->c)){
int fd = scs[v]->c - 5;
int p = s.tank[iv].pos;
if(p + DIR_DELTA[fd]== scs[u]->ms.p1 || p + 2 * DIR_DELTA[fd]== scs[u]->ms.p2){
total -= 5000.0;
}
}
}
}
int base_shots = 0;
for(int u = 0;u < 3;++u){
int iu = tank_id(seat,u);
if(!s.tank[iu].alive || !is_fire(scs[u]->c))continue;
int d = scs[u]->c - 5;
int pos = s.tank[iu].pos;
if(G.base_ray_dist[op][pos]> 0 && G.base_ray_dir[op][pos]== d && G.base_ray_dist[op][pos]<= 2){
int b_cnt = 0,s_cnt = 0;
count_ray_obstacles(s,op,pos,b_cnt,s_cnt);
if(b_cnt == 0 && s_cnt == 0)base_shots++;
}
}
if(base_shots >= 2)total -= 850.0 *(base_shots - 1);
Orders ord;
ord.cmd[0]= scs[0]->c;
ord.cmd[1]= scs[1]->c;
ord.cmd[2]= scs[2]->c;
pool[n_pool++]= ScoredOrders{ord,total};
}
}
}
std::sort(pool,pool + n_pool,[](const ScoredOrders& x,const ScoredOrders& y){
return x.prior > y.prior;
});
int res = std::min(n_pool,max_out);
for(int k = 0;k < res;++k)out[k]= pool[k];
return res;
}
}
void init(const State& s,int me){
(void)me;
std::memset(last_pos_hist,-1,sizeof(last_pos_hist));
hist_turn = -1;
precompute(s);
}
Orders decide(const State& s,int me){
int op = 1 - me;
if(s.turn != hist_turn){
for(int i = 0;i < 6;++i){
for(int h = 5;h > 0;--h)last_pos_hist[i][h]= last_pos_hist[i][h - 1];
last_pos_hist[i][0]= s.tank[i].alive ? s.tank[i].pos : -1;
}
hist_turn = s.turn;
}
precompute(s);
constexpr int MAX_MY = 32;
constexpr int MAX_OP = 22;
ScoredOrders my_cands[MAX_MY];
ScoredOrders op_cands[MAX_OP];
int n_my = generate_candidate_orders(s,me,my_cands,MAX_MY);
int n_op = generate_candidate_orders(s,op,op_cands,MAX_OP);
if(n_my <= 0)return Orders{};
if(n_my == 1)return my_cands[0].ord;
double op_weights[MAX_OP];
double max_op_p = op_cands[0].prior;
double sum_op_w = 0.0;
for(int j = 0;j < n_op;++j){
op_weights[j]= std::exp(std::max(-10.0,(op_cands[j].prior - max_op_p)/ 420.0));
sum_op_w += op_weights[j];
}
for(int j = 0;j < n_op;++j)op_weights[j]/= sum_op_w;
static double mat[MAX_MY][MAX_OP];
static State s1_cache[MAX_MY][MAX_OP];
for(int i = 0;i < n_my;++i){
for(int j = 0;j < n_op;++j){
State& s1 = s1_cache[i][j];
s1 = s;
step_as(s1,me,my_cands[i].ord,op_cands[j].ord);
mat[i][j]= evaluate_state(s1,me)+ my_cands[i].prior * 0.06;
}
}
for(int i = 0;i < n_my;++i){
if(budget_left()< 6.0)break;
for(int j = 0;j < std::min(n_op,14);++j){
if(budget_left()< 5.0)break;
const State& s1 = s1_cache[i][j];
if(s1.done)continue;
State s2 = s1;
Orders r0 = fast_rollout_orders(s2,0);
Orders r1 = fast_rollout_orders(s2,1);
step(s2,r0,r1);
double v2 = evaluate_state(s2,me);
if(!s2.done && j < 8 && budget_left()> 10.0){
State s3 = s2;
Orders r30 = fast_rollout_orders(s3,0);
Orders r31 = fast_rollout_orders(s3,1);
step(s3,r30,r31);
double v3 = evaluate_state(s3,me);
mat[i][j]= 0.40 * mat[i][j]+ 0.35 * v2 + 0.25 * v3;
}else{
mat[i][j]= 0.50 * mat[i][j]+ 0.50 * v2;
}
}
}
int best_idx = 0;
double best_total = -1e18;
int top_op_k = std::min(n_op,8);
for(int i = 0;i < n_my;++i){
double worst_top = 1e18;
double expected = 0.0;
for(int j = 0;j < n_op;++j){
double v = mat[i][j];
if(j < top_op_k && v < worst_top)worst_top = v;
expected += op_weights[j]* v;
}
double softmin_num = 0.0;
double softmin_den = 0.0;
for(int j = 0;j < n_op;++j){
double v = mat[i][j];
double w = op_weights[j]* std::exp(std::min(12.0,(worst_top - v)/ 600.0));
softmin_num += w * v;
softmin_den += w;
}
double softmin =(softmin_den > 0.0)?(softmin_num / softmin_den): worst_top;
double combined = 0.45 * expected + 0.35 * softmin + 0.20 * worst_top;
if(combined > best_total){
best_total = combined;
best_idx = i;
}
}
return my_cands[best_idx].ord;
}
}
namespace opponent_baseline{
using namespace tank;
static const int FAR = 9999;
#ifndef V2_BASELINE_BL_GUARD
#define V2_BASELINE_BL_GUARD 0               
#endif
#ifndef V2_BASELINE_BL_ALLSTAR
#define V2_BASELINE_BL_ALLSTAR 0             
#endif
#ifndef V2_BASELINE_BL_NOSTAR
#define V2_BASELINE_BL_NOSTAR 0              
#endif
#ifndef V2_BASELINE_GUARD_REACH
#define V2_BASELINE_GUARD_REACH 10           
#endif
static int my_seat;
static int last_pos[3],last_cmd[3],stuck[3];
static int lane_of[3];
static int far_shots[3],kills_then[3],bored_until[3];
static bool crowded[CELLS];
static void flood(const State& s,int* dist,int brick_cost){
static int queue[CELLS * 8];
int head = 0,tail = 0;
for(int c = 0;c < CELLS;++c)
if(dist[c]< FAR)queue[tail++]= c;
while(head < tail){
const int c = queue[head++ %(CELLS * 8)];
for(int d = 0;d < 4;++d){
const int n = c + DIR_DELTA[d];
const uint8_t g = s.grid[n];
if(g != EMPTY && g != BRICK)continue;
const int nd = dist[c]+(g == BRICK ? brick_cost : 1)+(crowded[n]? 4 : 0);
if(nd < dist[n]){dist[n]= nd;queue[tail++ %(CELLS * 8)]= n;}
}
}
}
static void attack_map(const State& s,int me,int lanes,int* dist){
for(int c = 0;c < CELLS;++c)dist[c]= FAR;
for(int d = 0;d < 4;++d){
if(!((lanes >> d)& 1))continue;
int bricks = 0,k = 1;
for(int c = s.base_pos[1 - me]+ DIR_DELTA[d];;c += DIR_DELTA[d],++k){
const uint8_t g = s.grid[c];
const int far = k <= 4 ? 0 : k - 4;
if(g == BRICK){
dist[c]= 2 * bricks + 3 + far;
++bricks;
continue;
}
if(g == WATER)continue;
if(g != EMPTY)break;
dist[c]= 2 * bricks + far;
}
}
flood(s,dist,3);
}
static void target_map(const State& s,const int* cells,int n,int* dist){
for(int c = 0;c < CELLS;++c)dist[c]= FAR;
for(int k = 0;k < n;++k)dist[cells[k]]= 0;
flood(s,dist,3);
}
static void hunt_map(const State& s,int e,int* dist){
for(int c = 0;c < CELLS;++c)dist[c]= FAR;
for(int d = 0;d < 4;++d){
int k = 1;
for(int c = s.tank[e].pos + DIR_DELTA[d];s.grid[c]== EMPTY || s.grid[c]== WATER;c += DIR_DELTA[d],++k)
if(s.grid[c]== EMPTY)dist[c]= k / 2;
}
flood(s,dist,3);
}
static int manhattan(int a,int b){
const int dx = cell_x(a)- cell_x(b),dy = cell_y(a)- cell_y(b);
return(dx < 0 ? -dx : dx)+(dy < 0 ? -dy : dy);
}
static int clear_line(const State& s,int a,int b){
for(int d = 0;d < 4;++d){
int k = 1;
for(int c = a + DIR_DELTA[d];s.grid[c]== EMPTY || s.grid[c]== WATER;c += DIR_DELTA[d],++k){
if(c == b)return k;
if(tank_at(s,c)>= 0)break;
}
}
return 0;
}
static int fire_value(const State& s,int me,int i,int d,bool brick_on_path,bool bored,bool* far_tank){
int bricks = 0,k = 1;
for(int c = s.tank[i].pos + DIR_DELTA[d];;c += DIR_DELTA[d],++k){
const uint8_t g = s.grid[c];
if(g == WATER)continue;
if(g == BRICK){++bricks;continue;}
if(g == BASE0 +(1 - me)){
if(k <= 4)return 700 - 40 * bricks;
break;
}
if(g == BASE0 + me)return bricks ? -500 : -1000;
if(g != EMPTY)break;
if(bricks)continue;
for(int b = 0;b < s.n_bullets;++b)
if(s.bullet[b].pos == c && s.bullet[b].dir ==(d ^ 1)&& seat_of(s.bullet[b].owner)!= me && k <= 6)return 150;
const int v = tank_at(s,c);
if(v < 0)continue;
if(seat_of(v)== me)return -1000;
*far_tank = true;
if(k <= 2 && !can_fire(s,v))return 600;
if(bored)return -5;
if(k <= 2)return 500;
return k <= 4 ? 200 : -5;
}
return(bricks && brick_on_path)? 120 : -5;
}
void init(const State& s,int me){
my_seat = me;
const int fire_side = cell_x(s.spawn[tank_id(me,FIRE)])<= cell_x(s.base_pos[me])? LEFT : RIGHT;
lane_of[FIRE]= 1 << fire_side;
lane_of[FAST]= 1 <<(fire_side ^ 1);
lane_of[ARMOR]= 1 <<(me == 0 ? DOWN : UP);
for(int k = 0;k < 3;++k){last_pos[k]= -1;last_cmd[k]= STAY;stuck[k]= 0;far_shots[k]= 0;kills_then[k]= 0;bored_until[k]= 0;}
}
Orders decide(const State& s,int me){
static int goal[CELLS];
Orders mine,theirs;
const int home = s.base_pos[me];
for(int type = 0;type < 3;++type){
const int i = tank_id(me,type);
const Tank& t = s.tank[i];
if(!t.alive){last_pos[type]= -1;stuck[type]= 0;continue;}
if(last_pos[type]== t.pos && last_cmd[type]!= STAY && !is_fire(Cmd(last_cmd[type])))stuck[type]+= 1;
else stuck[type]= 0;
for(int c = 0;c < CELLS;++c)crowded[c]= false;
for(int j = 3 * me;j < 3 * me + 3;++j)
if(j != i && s.tank[j].alive)crowded[s.tank[j].pos]= true;
int intruder = -1,intruder_dist = FAR;
for(int e = 3 *(1 - me);e < 3 *(1 - me)+ 3;++e){
if(!s.tank[e].alive || manhattan(s.tank[e].pos,home)> 7)continue;
const int gap = manhattan(s.tank[e].pos,t.pos);
if((type == ARMOR ?(V2_BASELINE_BL_GUARD || gap <= V2_BASELINE_GUARD_REACH): manhattan(t.pos,home)<= 5)&& gap < intruder_dist){intruder = e;intruder_dist = gap;}
}
if(intruder >= 0){
hunt_map(s,intruder,goal);
}else if(V2_BASELINE_BL_GUARD && type == ARMOR){
int post = home + 3 * DIR_DELTA[me == 0 ? UP : DOWN];
if(s.grid[post]!= EMPTY)post = s.spawn[i];
target_map(s,&post,1,goal);
}else{
attack_map(s,me,lane_of[type],goal);
if(goal[t.pos]>= FAR)attack_map(s,me,15,goal);
if(!V2_BASELINE_BL_NOSTAR &&(type == FAST || V2_BASELINE_BL_ALLSTAR)&& s.star_mask && t.level < s.P.max_level && goal[t.pos]> 3){
int stars[MAX_SUPPLY],n = 0;
for(int k = 0;k < s.n_supply;++k)
if((s.star_mask >> k)& 1)stars[n++]= s.supply[k];
target_map(s,stars,n,goal);
}
}
int best_step = -1;
for(int d = 0,best = goal[t.pos];d < 4;++d)
if(goal[t.pos + DIR_DELTA[d]]< best){best = goal[t.pos + DIR_DELTA[d]];best_step = d;}
Cmd list[N_CMDS];
const int n = legal_commands(s,i,list);
int best_score = -1000000;
Cmd best_cmd = STAY;
bool best_far = false;
for(int k = 0;k < n;++k){
const Cmd c = list[k];
Orders trial = mine;
trial.cmd[type]= c;
State next = s;
step_as(next,me,trial,theirs);
int score = 0;
for(int j = 3 * me;j < 3 * me + 3;++j)
if(s.tank[j].alive)score -= 1000 *(s.tank[j].hp -(next.tank[j].alive ? next.tank[j].hp : 0));
for(int e = 3 *(1 - me);e < 3 *(1 - me)+ 3;++e)
if(s.tank[e].alive)score += 400 *(s.tank[e].hp -(next.tank[e].alive ? next.tank[e].hp : 0));
score -= 1000 *(s.base_hp[me]- next.base_hp[me]);
score += 300 *(s.base_hp[1 - me]- next.base_hp[1 - me]);
const int pos = next.tank[i].alive ? next.tank[i].pos : t.pos;
for(int e = 3 *(1 - me);e < 3 *(1 - me)+ 3;++e){
if(!s.tank[e].alive || !can_fire(s,e))continue;
const int gap = clear_line(s,s.tank[e].pos,pos);
if(gap == 0 || gap > 2)continue;
bool answering = false;
if(is_fire(c)){
int probe = t.pos;
for(int step_k = 0;step_k < gap;++step_k)probe += DIR_DELTA[c - 5];
answering = probe == s.tank[e].pos;
}
if(!answering)score -= 250;
}
for(int b = 0;b < next.n_bullets;++b){
int probe = next.bullet[b].pos;
for(int step_k = 0;step_k < 4;++step_k){
probe += DIR_DELTA[next.bullet[b].dir];
if(next.grid[probe]!= EMPTY && next.grid[probe]!= WATER)break;
if(probe == pos){score -= 40;break;}
}
}
bool far_tank = false;
if(is_fire(c))
score += fire_value(s,me,i,c - 5,best_step == c - 5 && s.grid[t.pos + DIR_DELTA[c - 5]]== BRICK,
s.turn < bored_until[type],&far_tank);
score -= 10 *(goal[pos]< FAR ? goal[pos]: 200);
if(c == STAY)score -= 1;
if(c == last_cmd[type])score -= 30 * stuck[type];
if(score > best_score){best_score = score;best_cmd = c;best_far = far_tank;}
}
if(s.kills[me]!= kills_then[type]){kills_then[type]= s.kills[me];far_shots[type]= 0;}
if(best_far && ++far_shots[type]>= 3){far_shots[type]= 0;bored_until[type]= s.turn + 8;}
mine.cmd[type]= best_cmd;
last_pos[type]= t.pos;
last_cmd[type]= best_cmd;
}
return mine;
}
}
#undef V2_BASELINE_BL_GUARD
#undef V2_BASELINE_BL_ALLSTAR
#undef V2_BASELINE_BL_NOSTAR
#undef V2_BASELINE_GUARD_REACH
namespace opponent_deepseek{
static bool model_guard_limited=false;
static double private_budget_left(){
const double left=model_budget_view::remaining();
if(left<6.)model_guard_limited=true;
return left;
}
using namespace tank;
static const int INF = 1 << 20;
static double W_BASE = 250.0;
static double W_DMG = 120;
static double W_HP = 120.0;
static double W_LVL = 100.0;
static double W_DEAD = 200.0;
static double W_RESPAWN = 35.0;
static double W_KILL = 120.0;
static double W_STAR = 180.0;
static double W_POT = 14.0;
static double W_POT2 = 5.0;
static double W_CHASE = 60;
static double W_EXP = 10;
static double W_STARP = 6.0;
static int g_me = 0;
static int g_opp = 1;
static int g_potTo[2][CELLS];
static uint64_t g_potHash = 0;
static bool g_potValid = false;
static int g_starDist[CELLS];
static uint64_t g_starHash = 0;
static uint8_t g_starMask = 0;
static bool g_starValid = false;
static bool passable(uint8_t g){return g == EMPTY || g == BRICK;}
static int lineBricksTo(const State& s,int cell,int base){
const int x0 = cell_x(cell),y0 = cell_y(cell);
const int x1 = cell_x(base),y1 = cell_y(base);
if(x0 != x1 && y0 != y1)return INF;
if(x0 == x1 && y0 == y1)return INF;
int step;
if(y0 == y1)step =(x1 > x0)? 1 : -1;
else step =(y1 > y0)? STRIDE : -STRIDE;
int bricks = 0;
for(int c = cell + step;c != base;c += step){
const uint8_t g = s.grid[c];
if(g == BRICK)++bricks;
else if(g == STEEL)return INF;
else if(g != EMPTY && g != WATER)return INF;
}
return bricks;
}
static void computePot(const State& s,int base,int* out){
for(int c = 0;c < CELLS;++c)out[c]= INF;
static int head[1024];
static int nxt[CELLS];
for(int i = 0;i < 1024;++i)head[i]= -1;
int maxc = 0;
auto push =[&](int c,int cost){
if(cost < 0 || cost >= 1024)return;
nxt[c]= head[cost];
head[cost]= c;
if(cost > maxc)maxc = cost;
};
const int xb = cell_x(base),yb = cell_y(base);
for(int c = 0;c < CELLS;++c){
const int x = cell_x(c),y = cell_y(c);
if(x < 0 || y < 0 || x >= s.P.size || y >= s.P.size)continue;
if(x != xb && y != yb)continue;
if(c == base)continue;
const uint8_t g = s.grid[c];
if(!passable(g))continue;
const int lb = lineBricksTo(s,c,base);
if(lb >= INF)continue;
const int L = std::abs(x - xb)+ std::abs(y - yb);
int cost =(lb + 1)* L;
if(g == BRICK)cost += 4;
if(cost < out[c]){out[c]= cost;push(c,cost);}
}
for(int d = 0;d <= maxc;++d){
for(int c = head[d];c != -1;c = nxt[c]){
if(out[c]!= d)continue;
const int ec =(s.grid[c]== BRICK)? 4 : 2;
for(int k = 0;k < 4;++k){
const int n = c + DIR_DELTA[k];
if(!passable(s.grid[n]))continue;
if(out[c]+ ec < out[n]){out[n]= out[c]+ ec;push(n,out[n]);}
}
}
}
}
static void ensurePot(const State& s){
if(g_potValid && g_potHash == s.wall_hash)return;
computePot(s,s.base_pos[0],g_potTo[0]);
computePot(s,s.base_pos[1],g_potTo[1]);
g_potHash = s.wall_hash;
g_potValid = true;
}
static void ensureStar(const State& s){
if(g_starValid && g_starHash == s.wall_hash && g_starMask == s.star_mask)return;
g_starMask = s.star_mask;
for(int c = 0;c < CELLS;++c)g_starDist[c]= INF;
if(s.star_mask){
static int head[1024];
static int nxt[CELLS];
for(int i = 0;i < 1024;++i)head[i]= -1;
int maxc = 0;
auto push =[&](int c,int cost){
if(cost < 0 || cost >= 1024)return;
nxt[c]= head[cost];
head[cost]= c;
if(cost > maxc)maxc = cost;
};
for(int k = 0;k < s.n_supply;++k){
if(!((s.star_mask >> k)& 1))continue;
const int c = s.supply[k];
if(g_starDist[c]> 0){g_starDist[c]= 0;push(c,0);}
}
for(int d = 0;d <= maxc;++d){
for(int c = head[d];c != -1;c = nxt[c]){
if(g_starDist[c]!= d)continue;
const int ec =(s.grid[c]== BRICK)? 4 : 2;
for(int k = 0;k < 4;++k){
const int n = c + DIR_DELTA[k];
if(!passable(s.grid[n]))continue;
if(g_starDist[c]+ ec < g_starDist[n]){g_starDist[n]= g_starDist[c]+ ec;push(n,g_starDist[n]);}
}
}
}
}
g_starHash = s.wall_hash;
g_starValid = true;
}
static bool clearShot(const State& s,int from,int to){
if(from == to)return false;
const int x0 = cell_x(from),y0 = cell_y(from);
const int x1 = cell_x(to),y1 = cell_y(to);
if(x0 != x1 && y0 != y1)return false;
int step;
if(y0 == y1)step =(x1 > x0)? 1 : -1;
else step =(y1 > y0)? STRIDE : -STRIDE;
for(int c = from + step;c != to;c += step){
const uint8_t g = s.grid[c];
if(g == BRICK || g == STEEL || g == BASE0 || g == BASE1 || g == OUTSIDE)return false;
}
return true;
}
static double evalState(const State& s,int seat){
if(s.done)return s.winner == seat ? 1e9 :(s.winner < 0 ? 0.0 : -1e9);
const int opp = 1 - seat;
ensurePot(s);
double v = 0.0;
v += W_BASE * double(s.base_hp[seat]- s.base_hp[opp]);
for(int i = 0;i < 6;++i){
double m;
if(s.tank[i].alive)m = W_HP * s.tank[i].hp + W_LVL * s.tank[i].level;
else m = -(W_DEAD + W_RESPAWN * respawn_in(s,i));
v +=(seat_of(i)== seat)? m : -m;
}
v += W_KILL * double(s.kills[seat]- s.kills[opp]);
v += W_STAR * double(s.stars_eaten[seat]- s.stars_eaten[opp]);
const int* potToEnemy = g_potTo[opp];
const int* potToMine = g_potTo[seat];
const double T = double(std::max(0,s.P.max_turns - s.turn));
double offMe = 0.0,offOpp = 0.0;
{
double b1 = 0.0,b2 = 0.0;
for(int i = 0;i < 6;++i){
if(seat_of(i)!= seat || !s.tank[i].alive)continue;
const double setup = std::min(potToEnemy[s.tank[i].pos],160)/ 2.0;
const double rate = 1.0 /(1.0 + double(reload_turns(s,i)));
const double val = std::max(0.0,T - setup)* rate;
if(val > b1){b2 = b1;b1 = val;}
else if(val > b2)b2 = val;
}
offMe = b1 + 0.25 * b2;
}
{
double b1 = 0.0,b2 = 0.0;
for(int i = 0;i < 6;++i){
if(seat_of(i)== seat || !s.tank[i].alive)continue;
const double setup = std::min(potToMine[s.tank[i].pos],160)/ 2.0;
const double rate = 1.0 /(1.0 + double(reload_turns(s,i)));
const double val = std::max(0.0,T - setup)* rate;
if(val > b1){b2 = b1;b1 = val;}
else if(val > b2)b2 = val;
}
offOpp = b1 + 0.25 * b2;
}
v += W_DMG *(offMe - offOpp);
for(int j = 0;j < 6;++j){
if(seat_of(j)== seat || !s.tank[j].alive)continue;
const int pj = potToMine[s.tank[j].pos];
if(pj >= 14)continue;
int dmin = 99;
for(int i = 0;i < 6;++i){
if(seat_of(i)!= seat || !s.tank[i].alive)continue;
const int d = std::abs(cell_x(s.tank[i].pos)- cell_x(s.tank[j].pos))+
std::abs(cell_y(s.tank[i].pos)- cell_y(s.tank[j].pos));
dmin = std::min(dmin,d);
}
v -= W_CHASE * double(14 - pj)/ 14.0 * double(std::min(dmin,15));
}
for(int i = 0;i < 6;++i){
if(seat_of(i)!= seat || !s.tank[i].alive)continue;
int cnt = 0;
for(int j = 0;j < 6;++j){
if(seat_of(j)== seat || !s.tank[j].alive)continue;
if(clearShot(s,s.tank[j].pos,s.tank[i].pos))++cnt;
}
v -= W_EXP * double(cnt);
}
if(s.star_mask){
ensureStar(s);
for(int i = 0;i < 6;++i){
if(!s.tank[i].alive)continue;
const int d = std::min(g_starDist[s.tank[i].pos],40);
if(seat_of(i)== seat)v -= W_STARP * double(d);
else v += W_STARP * double(d);
}
}
return v;
}
static double planEval(const State& root,int seat,const Orders& mine,const Orders& theirs){
State t = root;
step_as(t,seat,mine,theirs);
return evalState(t,seat);
}
static Orders bestResponse(const State& root,int seat,const Orders& other,int rounds,Orders cur = Orders{}){
for(int r = 0;r < rounds;++r){
bool changed = false;
for(int ty = 0;ty < 3;++ty){
const int i = tank_id(seat,ty);
if(!root.tank[i].alive){cur.cmd[ty]= STAY;continue;}
Cmd list[N_CMDS];
const int n = legal_commands(root,i,list);
const Cmd keep = cur.cmd[ty];
double bestv = -1e18;
Cmd bestc = keep;
for(int k = 0;k < n;++k){
cur.cmd[ty]= list[k];
const double v = planEval(root,seat,cur,other);
if(v > bestv){bestv = v;bestc = list[k];}
}
cur.cmd[ty]= bestc;
if(bestc != keep)changed = true;
}
if(!changed)break;
}
return cur;
}
static void topCommands(const State& root,int seat,const Orders& other,Cmd out[3][8],int K,int cnt[3]){
for(int ty = 0;ty < 3;++ty){
const int i = tank_id(seat,ty);
cnt[ty]= 1;
if(!root.tank[i].alive){out[ty][0]= STAY;continue;}
Cmd list[N_CMDS];
const int n = legal_commands(root,i,list);
double sc[N_CMDS];
Orders probe;
for(int k = 0;k < n;++k){
probe.cmd[ty]= list[k];
sc[k]= planEval(root,seat,probe,other);
}
int idx[N_CMDS];
for(int k = 0;k < n;++k)idx[k]= k;
const int kk = std::min(n,K);
for(int a = 0;a < kk;++a){
int best = a;
for(int b = a + 1;b < n;++b)
if(sc[idx[b]]> sc[idx[best]])best = b;
std::swap(idx[a],idx[best]);
out[ty][a]= list[idx[a]];
}
cnt[ty]= kk;
}
}
static int dirToBase(const State& s,int pos,int tgt){
const int x0 = cell_x(pos),y0 = cell_y(pos),x1 = cell_x(tgt),y1 = cell_y(tgt);
if(x0 == x1 && y0 != y1)return(y1 > y0)? DOWN : UP;
if(y0 == y1 && x0 != x1)return(x1 > x0)? RIGHT : LEFT;
return -1;
}
static Cmd attackerCmd(const State& s,int me,int i){
if(!s.tank[i].alive)return STAY;
const int tgt = s.base_pos[1 - me];
const int pos = s.tank[i].pos;
const int d = dirToBase(s,pos,tgt);
if(d >= 0){
const int lb = lineBricksTo(s,pos,tgt);
if(lb < INF && can_fire(s,i))return cmd_fire(d);
}
int bestd = -1,bestv = g_potTo[1 - me][pos];
for(int k = 0;k < 4;++k){
const int n = pos + DIR_DELTA[k];
const uint8_t g = s.grid[n];
if(g != EMPTY && g != BRICK)continue;
if(tank_at(s,n)>= 0)continue;
if(g_potTo[1 - me][n]< bestv){bestv = g_potTo[1 - me][n];bestd = k;}
}
if(bestd < 0)return STAY;
if(s.grid[pos + DIR_DELTA[bestd]]== BRICK){
if(can_fire(s,i))return cmd_fire(bestd);
return STAY;
}
return cmd_move(bestd);
}
void init(const State& s,int me){
g_me = me;
g_opp = 1 - me;
g_potValid = false;
g_starValid = false;
(void)s;
}
Orders decide(const State& s,int me){
g_me = me;
g_opp = 1 - me;
g_potValid = false;
g_starValid = false;
ensurePot(s);
const Orders stay;
Orders opp = bestResponse(s,g_opp,stay,2);
Orders mine = bestResponse(s,me,opp,3);
for(int it = 0;it < 3;++it){
Orders no = bestResponse(s,g_opp,mine,2,opp);
if(no == opp)break;
opp = no;
mine = bestResponse(s,me,opp,3,mine);
}
const int K = 5;
Cmd cand[3][8];
int cnt[3];
topCommands(s,me,opp,cand,K,cnt);
const int n0 = cnt[0],n1 = cnt[1],n2 = cnt[2];
double bestv = -1e18;
Orders best = mine;
int evals = 0;
bool stop = false;
for(int a = 0;a < n0 && !stop;++a)
for(int b = 0;b < n1 && !stop;++b)
for(int c = 0;c < n2;++c){
Orders M;
M.cmd[0]= cand[0][a];
M.cmd[1]= cand[1][b];
M.cmd[2]= cand[2][c];
Orders R = bestResponse(s,g_opp,M,1,opp);
const double v = planEval(s,me,M,R);
++evals;
if(v > bestv){bestv = v;best = M;}
if((evals & 7)== 0 && ::opponent_deepseek::private_budget_left()< 6.0){stop = true;break;}
}
int atk = -1;
double atkScore = -1e18;
for(int ty = 0;ty < 3;++ty){
const int i = tank_id(me,ty);
if(!s.tank[i].alive)continue;
const double rate = 1.0 /(1.0 + double(reload_turns(s,i)));
const double score = rate * double(200 - std::min(g_potTo[1 - me][s.tank[i].pos],200));
if(score > atkScore){atkScore = score;atk = i;}
}
if(atk >= 0){
const int ty = type_of(atk);
if(best.cmd[ty]== STAY){
const Cmd pol = attackerCmd(s,me,atk);
if(pol != STAY){
Orders probe = best;
probe.cmd[ty]= pol;
const Orders R = bestResponse(s,g_opp,probe,1,opp);
if(planEval(s,me,probe,R)> planEval(s,me,best,R))best = probe;
}
}
}
return best;
}
}
namespace opponent_mimo{
static bool model_guard_limited=false;
static double private_budget_left(){
const double left=model_budget_view::remaining();
if(left<14.)model_guard_limited=true;
return left;
}
using namespace tank;
namespace{
constexpr int K_TOP = 8;
constexpr int M_ROLLOUT = 288;
double dist_base[2][CELLS];
double dist_supply[CELLS];
void bfs_dist(const State& s,const int16_t* seeds,int nseeds,double* out){
static int q[CELLS];
for(int c = 0;c < CELLS;++c)out[c]= 1e9;
int head = 0,tail = 0;
for(int k = 0;k < nseeds;++k){out[seeds[k]]= 0;q[tail++]= seeds[k];}
while(head < tail){
const int c = q[head++];
const double nd = out[c]+ 1.0;
for(int d = 0;d < 4;++d){
const int n = c + DIR_DELTA[d];
const uint8_t g = s.grid[n];
if((g == EMPTY || g == BRICK)&& out[n]> nd){out[n]= nd;q[tail++]= n;}
}
}
}
void prepare(const State& s){
int16_t seed0 = s.base_pos[0],seed1 = s.base_pos[1];
bfs_dist(s,&seed0,1,dist_base[0]);
bfs_dist(s,&seed1,1,dist_base[1]);
bfs_dist(s,s.supply,s.n_supply,dist_supply);
}
inline int manh(int a,int b){
int dx = cell_x(a)- cell_x(b),dy = cell_y(a)- cell_y(b);
return(dx < 0 ? -dx : dx)+(dy < 0 ? -dy : dy);
}
inline bool is_base_cell(const State& s,int c,int seat){
return s.grid[c]== BASE0 + seat;
}
inline double rate_factor(int d){
return d <= 2 ? 1.5 :(d <= 4 ? 1.0 :(d <= 6 ? 0.6 : 0.35));
}
struct EvalGeometry{
int8_t occupancy[CELLS];
bool fireable[6];
int tank_hit[6][4],tank_dist[6][4];
int bullet_hit[MAX_BULLETS],bullet_dist[MAX_BULLETS];
int ray(const State& s,int pos,int dir,int& dist)const{
int d = 0;
for(int c = pos + DIR_DELTA[dir];;c += DIR_DELTA[dir]){
++d;
const uint8_t g = s.grid[c];
if(g == OUTSIDE || occupancy[c]>= 0 ||
g == BRICK || g == STEEL || g == BASE0 || g == BASE1){
dist = d;
return c;
}
}
}
explicit EvalGeometry(const State& s){
std::fill_n(occupancy,CELLS,int8_t(-1));
for(int i = 5;i >= 0;--i)
if(s.tank[i].alive)occupancy[s.tank[i].pos]= int8_t(i);
int owned[6]={};
for(int k = 0;k < s.n_bullets;++k)++owned[s.bullet[k].owner];
for(int i = 0;i < 6;++i){
fireable[i]= s.tank[i].alive && reload_left(s,i)== 0 &&
owned[i]< bullet_cap(s,i)&& s.n_bullets < MAX_BULLETS;
if(fireable[i])
for(int dir = 0;dir < 4;++dir)
tank_hit[i][dir]= ray(s,s.tank[i].pos,dir,tank_dist[i][dir]);
}
for(int k = 0;k < s.n_bullets;++k)
bullet_hit[k]= ray(s,s.bullet[k].pos,s.bullet[k].dir,bullet_dist[k]);
}
};
double eval_state(const State& s,int me){
const int opp = 1 - me;
if(s.done){
if(s.winner == me)return 1e7;
if(s.winner < 0)return 0.0;
return -1e7;
}
const EvalGeometry geo(s);
double sc = 0.0;
sc += 35.0 *(s.base_hp[me]- s.base_hp[opp]);
sc += 45.0 *(s.kills[me]- s.kills[opp]);
sc += 28.0 *(s.stars_eaten[me]- s.stars_eaten[opp]);
for(int i = 0;i < 6;++i){
const Tank& t = s.tank[i];
if(!t.alive)continue;
const bool mine = seat_of(i)== me;
double v = 6.0 + 3.5 * t.hp + 10.0 * t.level;
double d = dist_base[1 - seat_of(i)][t.pos];
if(d > 26.0)d = 26.0;
v -= 3.0 * d;
sc += mine ? v : -v;
}
for(int k = 0;k < s.n_bullets;++k){
const Bullet& b = s.bullet[k];
int d = 0;
const int hitc = geo.bullet_hit[k];d = geo.bullet_dist[k];
const bool mine = seat_of(b.owner)== me;
const double w = d <= 2 ? 1.0 :(d <= 4 ? 0.6 : 0.3);
double v = 0.0;
const int tgt = geo.occupancy[hitc];
if(tgt >= 0){
const bool tgt_mine = seat_of(tgt)== me;
if(tgt_mine == mine)v = -10.0;
else if(s.tank[tgt].hp <= 1)v = 14.0;
else v = 8.0;
}else if(s.grid[hitc]== BASE0 || s.grid[hitc]== BASE1){
const bool base_mine =(s.grid[hitc]- BASE0)== me;
v = base_mine == mine ? -22.0 : 38.0;
}else if(s.grid[hitc]== BRICK){
const double dg = dist_base[1 - seat_of(b.owner)][hitc];
v = 2.0 +(dg <= 2.0 ? 14.0 :(dg <= 5.0 ? 5.0 : 0.0));
}else if(s.grid[hitc]== STEEL){
v = 0.2;
}
sc += mine ? w * v : -w * v;
}
for(int i = 0;i < 6;++i){
const Tank& t = s.tank[i];
if(!t.alive || !geo.fireable[i])continue;
const bool mine = seat_of(i)== me;
for(int dir = 0;dir < 4;++dir){
int d = 0;
const int hitc = geo.tank_hit[i][dir];d = geo.tank_dist[i][dir];
double v = 0.0;
const int tgt = geo.occupancy[hitc];
if(tgt >= 0){
if(seat_of(tgt)== seat_of(i))v = -4.0;
else v = s.tank[tgt].hp <= 1 ? 8.0 : 4.5;
}else if(s.grid[hitc]== BASE0 || s.grid[hitc]== BASE1){
v =((s.grid[hitc]- BASE0)== seat_of(i))? -24.0 : 36.0;
}
const double w = d <= 4 ? 1.0 : 0.6;
sc += mine ? w * v : -w * v;
}
}
for(int X = 0;X < 2;++X){
int tc[8];
int nt = 0;
for(int i = 0;i < 6 && nt < 8;++i){
const Tank& t = s.tank[i];
if(!t.alive || seat_of(i)== X || !geo.fireable[i])continue;
for(int dir = 0;dir < 4 && nt < 8;++dir){
int d;
const int hitc = geo.tank_hit[i][dir];d = geo.tank_dist[i][dir];
if(is_base_cell(s,hitc,X))tc[nt++]= t.pos;
}
}
for(int k = 0;k < s.n_bullets && nt < 8;++k){
const Bullet& b = s.bullet[k];
if(seat_of(b.owner)== X)continue;
int d;
const int hitc = geo.bullet_hit[k];d = geo.bullet_dist[k];
if(is_base_cell(s,hitc,X))tc[nt++]= b.pos;
}
if(!nt)continue;
const int home = s.base_pos[X];
int order[3]={0,1,2};
int md[3];
for(int k = 0;k < 3;++k){
const int i = tank_id(X,k);
md[k]= s.tank[i].alive ? manh(s.tank[i].pos,home): 999;
}
for(int a = 0;a < 3;++a)
for(int b2 = a + 1;b2 < 3;++b2)
if(md[order[b2]]< md[order[a]])std::swap(order[a],order[b2]);
for(int r = 0;r < 2;++r){
const int i = tank_id(X,order[r]);
if(!s.tank[i].alive)continue;
const int dd = manh(s.tank[i].pos,home);
const double v =(r == 0 ? 3.2 : 1.4)*(dd < 12 ? 12 - dd : 0);
sc += X == me ? v : -v;
}
}
{
const double wgt = s.star_mask ? 2.2 : 0.8;
for(int i = 0;i < 6;++i){
const Tank& t = s.tank[i];
if(!t.alive || t.level >= s.P.max_level)continue;
double d = dist_supply[t.pos];
if(d > 10.0)d = 10.0;
const double v = -wgt * d;
sc +=(seat_of(i)== me)? v : -v;
}
}
for(int i = 0;i < 6;++i){
const Tank& t = s.tank[i];
if(!t.alive)continue;
for(int e = 0;e < 3;++e){
const int j = tank_id(1 - seat_of(i),e);
if(s.tank[j].alive)continue;
if(s.spawn[j]== t.pos && respawn_in(s,j)<= 2){
sc +=(seat_of(i)== me)? -25.0 : 25.0;
}
}
}
return sc;
}
Orders greedy(const State& s,int seat,const Orders& opp_orders){
Orders res;
for(int type = 0;type < 3;++type){
const int i = tank_id(seat,type);
if(!s.tank[i].alive)continue;
Cmd list[N_CMDS];
const int n = legal_commands(s,i,list);
double best = -1e18;
Cmd bc = STAY;
for(int k = 0;k < n;++k){
State tmp = s;
Orders mine;
mine.cmd[type]= list[k];
step_as(tmp,seat,mine,opp_orders);
const double sc = eval_state(tmp,seat);
if(sc > best){best = sc;bc = list[k];}
}
res.cmd[type]= bc;
}
return res;
}
struct Cand{Cmd cmd;double sc;};
struct Combo{double sc;Cmd c[3];};
}
void init(const State& s,int me){(void)s;(void)me;}
Orders decide(const State& s,int me){
prepare(s);
const int opp = 1 - me;
const Orders stay_orders;
const Orders enemy_pred = greedy(s,opp,stay_orders);
Cand rk[3][N_CMDS];
int rn[3]={0,0,0};
for(int type = 0;type < 3;++type){
const int i = tank_id(me,type);
if(!s.tank[i].alive){rk[type][0]={STAY,0.0};rn[type]= 1;continue;}
Cmd list[N_CMDS];
const int n = legal_commands(s,i,list);
for(int k = 0;k < n;++k){
State tmp = s;
Orders mine;
mine.cmd[type]= list[k];
step_as(tmp,me,mine,enemy_pred);
rk[type][k]={list[k],eval_state(tmp,me)};
}
rn[type]= n;
std::sort(rk[type],rk[type]+ n,[](const Cand& a,const Cand& b){
return a.sc != b.sc ? a.sc > b.sc : a.cmd < b.cmd;
});
if(rn[type]> K_TOP)rn[type]= K_TOP;
}
static Combo combos[512];
int ncombo = 0;
for(int a = 0;a < rn[0];++a){
for(int b = 0;b < rn[1];++b){
for(int c = 0;c < rn[2];++c){
Orders mine;
mine.cmd[0]= rk[0][a].cmd;
mine.cmd[1]= rk[1][b].cmd;
mine.cmd[2]= rk[2][c].cmd;
State t1 = s;
step_as(t1,me,mine,enemy_pred);
double sc = eval_state(t1,me);
State t2 = s;
step_as(t2,me,mine,stay_orders);
sc = 0.5 * sc + 0.5 * eval_state(t2,me);
combos[ncombo].sc = sc;
combos[ncombo].c[0]= mine.cmd[0];
combos[ncombo].c[1]= mine.cmd[1];
combos[ncombo].c[2]= mine.cmd[2];
++ncombo;
}
}
}
std::sort(combos,combos + ncombo,[](const Combo& x,const Combo& y){return x.sc > y.sc;});
const int m = ncombo < M_ROLLOUT ? ncombo : M_ROLLOUT;
double best_final = -1e18;
int best_idx = 0;
for(int j = 0;j < m;++j){
if(j >= 8 && ::opponent_mimo::private_budget_left()< 14.0)break;
Orders mine;
mine.cmd[0]= combos[j].c[0];
mine.cmd[1]= combos[j].c[1];
mine.cmd[2]= combos[j].c[2];
State t1 = s;
step_as(t1,me,mine,enemy_pred);
double final;
if(t1.done){
final = eval_state(t1,me);
}else{
const Orders eg = greedy(t1,opp,stay_orders);
const Orders mg = greedy(t1,me,eg);
State t2 = t1;
step_as(t2,me,mg,eg);
double e2 = eval_state(t2,me);
if(!t2.done){
const Orders eg2 = greedy(t2,opp,stay_orders);
const Orders mg2 = greedy(t2,me,eg2);
State t3 = t2;
step_as(t3,me,mg2,eg2);
final = 0.3 * eval_state(t1,me)+ 0.4 * e2 + 0.3 * eval_state(t3,me);
}else{
final = 0.4 * eval_state(t1,me)+ 0.6 * e2;
}
}
if(final > best_final){best_final = final;best_idx = j;}
}
Orders out;
out.cmd[0]= combos[best_idx].c[0];
out.cmd[1]= combos[best_idx].c[1];
out.cmd[2]= combos[best_idx].c[2];
return out;
}
}
namespace ensemble_transition_belief{
template<std::size_t K> using LogWeights=std::array<double,K+1>;
enum class Status{updated,incomplete,invalid_turn,no_evidence,invalid_prior,invalid_input};
template<std::size_t K> struct Result{
static_assert(K>0,"The ensemble must have at least one known model.");
Status status=Status::incomplete;
LogWeights<K> log_weights{};
bool posterior_valid=false,unknown_fallback=false;
std::array<std::uint32_t,3> legal_counts{};
std::array<bool,K> known_match{};
std::uint32_t total_orders=0,tested=0,matching=0;
};
struct OfficialForward{
void operator()(tank::State&s,const tank::Orders&a,const tank::Orders&b)const{tank::step(s,a,b);}
};
template<std::size_t K> inline LogWeights<K> unknown_only(){
LogWeights<K> weights;
weights.fill(-std::numeric_limits<double>::infinity());
weights[K]=0.;
return weights;
}
template<std::size_t K> inline bool valid_prior(const LogWeights<K>&prior){
if(!std::isfinite(prior[K]))return false;
for(std::size_t k=0;k<K;k++)
if(std::isnan(prior[k])||prior[k]==std::numeric_limits<double>::infinity())return false;
return true;
}
template<std::size_t K> inline Result<K> fallback(Result<K> result,Status status){
result.status=status;result.log_weights=unknown_only<K>();
result.posterior_valid=false;result.unknown_fallback=true;
return result;
}
template<std::size_t K,class Forward,class Deadline>
Result<K> update(const tank::State&previous,const tank::Orders&our_last,
const tank::State&observed,int our_seat,
const std::array<tank::Orders,K>&predicted,
const LogWeights<K>&prior,Forward&&forward,Deadline&&deadline){
Result<K> result;result.log_weights=prior;
if(our_seat<0||our_seat>1)return fallback(result,Status::invalid_input);
if(previous.done||int(observed.turn)!=int(previous.turn)+1)return fallback(result,Status::invalid_turn);
if(!valid_prior<K>(prior))return fallback(result,Status::invalid_prior);
const int enemy_base=3*(1-our_seat);
tank::Cmd legal[3][tank::N_CMDS];
for(int type=0;type<3;type++){
const int n=tank::legal_commands(previous,enemy_base+type,legal[type]);
if(n<1||n>tank::N_CMDS)return fallback(result,Status::invalid_input);
result.legal_counts[type]=std::uint32_t(n);
}
result.total_orders=result.legal_counts[0]*result.legal_counts[1]*result.legal_counts[2];
auto matches=[&](const tank::Orders&enemy){
tank::State simulated=previous;
if(our_seat==0)forward(simulated,our_last,enemy);
else forward(simulated,enemy,our_last);
return simulated==observed;
};
for(std::size_t known=0;known<K;known++){
if(!deadline())return result;
result.known_match[known]=matches(predicted[known]);
}
for(std::uint32_t a=0;a<result.legal_counts[0];a++)
for(std::uint32_t b=0;b<result.legal_counts[1];b++)
for(std::uint32_t c=0;c<result.legal_counts[2];c++){
if(!deadline())return result;
tank::Orders enemy;enemy.cmd[0]=legal[0][a];enemy.cmd[1]=legal[1][b];enemy.cmd[2]=legal[2][c];
result.matching+=matches(enemy)?1u:0u;
++result.tested;
}
if(!deadline())return result;
const double zero=-std::numeric_limits<double>::infinity();
LogWeights<K> unnormalized;unnormalized.fill(zero);
for(std::size_t known=0;known<K;known++)if(result.known_match[known])unnormalized[known]=prior[known];
if(result.matching>0)
unnormalized[K]=prior[K]+std::log(double(result.matching))-std::log(double(result.total_orders));
double maximum=zero;
for(double v:unnormalized)if(v>maximum)maximum=v;
if(!std::isfinite(maximum))return fallback(result,Status::no_evidence);
if(result.matching==0)return fallback(result,Status::invalid_input);
double sum=0.;for(double v:unnormalized)sum+=std::exp(v-maximum);
const double log_normalizer=maximum+std::log(sum);
LogWeights<K> posterior;
for(std::size_t k=0;k<K+1;k++)posterior[k]=unnormalized[k]-log_normalizer;
if(!std::isfinite(posterior[K]))return fallback(result,Status::invalid_prior);
result.log_weights=posterior;result.status=Status::updated;
result.posterior_valid=true;
return result;
}
}
namespace opponent_belief{
constexpr std::size_t MODELS=10;
constexpr int UNKNOWN=int(MODELS);
static constexpr std::array<int,MODELS> MODEL_CALL_SCHEDULE={7,3,1,9,6,4,2,0,8,5};
using LogWeights=ensemble_transition_belief::LogWeights<MODELS>;
static LogWeights logs;
static State previous;
static Orders our_last;
static std::array<Orders,MODELS> predicted_last{},predicted_now{};
static bool have_previous=false,previous_predictions_valid=false,unknown_only=false,root_enabled=false;
static double root_probability[MODELS+1]{};
static int calls[MODELS]{},last_status=0;
static int refuted_turn[MODELS]{},refute_reason[MODELS]{},last_sims=0;
static int computational_reason[MODELS]{},computational_turn[MODELS]{};
static int computational_disable_reason=0;
static bool model_guard_limited[MODELS]{};
static double model_ms[MODELS]{},last_helper_ms=0,last_native_ms=0,last_total_ms=0;
static uint64_t forced_root_commands=0;

struct NextPrediction{State expected;Orders orders;bool attempted=false,complete=false,consumed=false;};
static std::array<NextPrediction,MODELS> next_prediction;
static bool prefetch_done=false,cache_pending=false;
static int prefetch_calls[MODELS]{},cache_hits[MODELS]{},cache_misses[MODELS]{};
static double prefetch_ms=0;
static int prefetch_seat=0;
static bool plausible(int model){return std::isfinite(logs[model]);}
static double logaddexp(double a,double b){
if(a==-std::numeric_limits<double>::infinity())return b;
if(b==-std::numeric_limits<double>::infinity())return a;
const double hi=std::max(a,b),lo=std::min(a,b);
return hi+std::log1p(std::exp(lo-hi));
}
static void refresh_unknown_only(){
unknown_only=true;
for(std::size_t k=0;k<MODELS;k++)if(plausible(int(k))){unknown_only=false;break;}
if(unknown_only)logs[UNKNOWN]=0;
}
static void collapse_model_mass(int k,int reason,int turn){
if(!plausible(k))return;
logs[UNKNOWN]=logaddexp(logs[UNKNOWN],logs[k]);
logs[k]=-std::numeric_limits<double>::infinity();
computational_reason[k]=reason;computational_turn[k]=turn;
computational_disable_reason=reason;
refresh_unknown_only();
}
static void collapse_uncalled_from(int first,int reason,int turn){
for(int k=first;k<int(MODELS);k++)collapse_model_mass(k,reason,turn);
}
static void collapse_scheduled_uncalled_from(std::size_t first,int reason,int turn){
for(std::size_t p=first;p<MODELS;p++)collapse_model_mass(MODEL_CALL_SCHEDULE[p],reason,turn);
}
static void model_init(int k,const State&s,int seat){
switch(k){
case 0:opponent_astra::init(s,seat);break;
case 1:opponent_soluf::init(s,seat);break;
case 2:ensemble_models::sol::init(s,seat);break;
case 3:
for(int t=0;t<3;t++){
ensemble_models::opus::sideOf[t]=-1;ensemble_models::opus::lastBest[t]=ensemble_models::opus::INF;
ensemble_models::opus::stuckCnt[t]=0;
for(int u=0;u<3;u++)ensemble_models::opus::sidePen[t][u]=ensemble_models::opus::sidePenUntil[t][u]=0;
}
ensemble_models::opus::init(s,seat);break;
case 4:ensemble_models::kimi::init(s,seat);break;
case 5:ensemble_models::sparring::init(s,seat);break;
case 6:opponent_gemini::model_guard_limited=false;opponent_gemini::init(s,seat);break;
case 7:opponent_baseline::init(s,seat);break;
case 8:opponent_deepseek::model_guard_limited=false;opponent_deepseek::init(s,seat);break;
case 9:opponent_mimo::model_guard_limited=false;opponent_mimo::init(s,seat);break;
}
}
static Orders model_predict(int k,const State&s,int seat){
if(k==0){opponent_astra::model_guard_limited=false;Orders o=opponent_astra::decide(s,seat);model_guard_limited[k]=opponent_astra::model_guard_limited;return o;}
if(k==1){opponent_soluf::model_guard_limited=false;Orders o=opponent_soluf::decide(s,seat);model_guard_limited[k]=opponent_soluf::model_guard_limited;return o;}
if(k==6){opponent_gemini::model_guard_limited=false;Orders o=opponent_gemini::decide(s,seat);model_guard_limited[k]=opponent_gemini::model_guard_limited;return o;}
if(k==7){model_guard_limited[k]=false;return opponent_baseline::decide(s,seat);}
if(k==8){opponent_deepseek::model_guard_limited=false;Orders o=opponent_deepseek::decide(s,seat);model_guard_limited[k]=opponent_deepseek::model_guard_limited;return o;}
if(k==9){opponent_mimo::model_guard_limited=false;Orders o=opponent_mimo::decide(s,seat);model_guard_limited[k]=opponent_mimo::model_guard_limited;return o;}
ensemble_models::active=k-2;ensemble_models::guard_limited[k-2]=false;
Orders o;
switch(k){
case 2:o=ensemble_models::sol::decide(s,seat);break;
case 3:o=ensemble_models::opus::decide(s,seat);break;
case 4:o=ensemble_models::kimi::decide(s,seat);break;
case 5:o=ensemble_models::sparring::decide(s,seat);break;
}
model_guard_limited[k]=ensemble_models::guard_limited[k-2];ensemble_models::active=-1;return o;
}
static void reset(const State&s,int me){
logs.fill(-std::log(double(MODELS+1)));have_previous=false;previous_predictions_valid=false;
prefetch_done=false;cache_pending=false;prefetch_ms=0;prefetch_seat=me;
for(std::size_t k=0;k<MODELS;k++){next_prediction[k]=NextPrediction{};prefetch_calls[k]=cache_hits[k]=cache_misses[k]=0;}

unknown_only=false;root_enabled=false;forced_root_commands=0;last_sims=last_status=0;
computational_disable_reason=0;last_helper_ms=last_native_ms=last_total_ms=0;
predicted_last.fill(Orders{});predicted_now.fill(Orders{});
for(std::size_t k=0;k<MODELS;k++){
calls[k]=0;refuted_turn[k]=-1;refute_reason[k]=0;model_guard_limited[k]=false;model_ms[k]=0;
computational_reason[k]=0;computational_turn[k]=-1;
}
for(double&p:root_probability)p=0;root_probability[UNKNOWN]=1;
if(!(s.P==Params{})||s.n_supply!=3){collapse_uncalled_from(0,2,s.turn);return;}
for(std::size_t k=0;k<MODELS;k++)model_init(int(k),s,1-me);
}
static bool model_budget_available(std::size_t schedule_position,int turn,const std::array<bool,MODELS>*called=nullptr){
if(tank::budget_left()>=16)return true;
for(std::size_t p=schedule_position;p<MODELS;p++){
const int k=MODEL_CALL_SCHEDULE[p];if(!called||!(*called)[k])collapse_model_mass(k,1,turn);
}
return false;
}
static bool accept_complete_prediction(int k,int turn){
if(!model_guard_limited[k])return true;
collapse_model_mass(k,3,turn);return false;
}

static void prefetch_next(const State&s,const Orders&ours,double origin){
if(prefetch_done)return;
prefetch_done=true;cache_pending=true;
const double begin=tank::budget_used();
const double deadline=std::min(origin+600.,begin+std::max(0.,tank::budget_left()-64.));
if(!unknown_only)for(int k:MODEL_CALL_SCHEDULE){
if(!plausible(k))continue;
if(tank::budget_used()>=deadline||tank::budget_left()<64)break;
NextPrediction&c=next_prediction[k];c.expected=s;
const int me=prefetch_seat;
if(me==0)sim_step(c.expected,ours,predicted_now[k]);else sim_step(c.expected,predicted_now[k],ours);
if(c.expected.done)continue;
c.attempted=true;
const bool prior_guard=model_guard_limited[k];
double model_start;
{
model_budget_view::Scope guard;model_start=model_budget_view::started;
c.orders=model_predict(k,c.expected,1-me);++prefetch_calls[k];
}
c.complete=!model_guard_limited[k]&&tank::budget_used()-model_start<=50.&&tank::budget_used()<=deadline&&tank::budget_left()>=8;
model_guard_limited[k]=prior_guard;
}
prefetch_ms=tank::budget_used()-begin;
last_helper_ms+=prefetch_ms;
}
static void consume_next(const State&s,int me,std::array<bool,MODELS>&called){
if(!cache_pending)return;
cache_pending=false;
for(std::size_t k=0;k<MODELS;k++){
NextPrediction&c=next_prediction[k];if(!plausible(int(k))||!c.attempted)continue;
if(c.complete&&!c.consumed&&me==prefetch_seat&&c.expected==s){
predicted_now[k]=c.orders;c.consumed=true;called[k]=true;++cache_hits[k];
}else{
++cache_misses[k];collapse_model_mass(int(k),c.complete?4:5,s.turn);
}
}
}

static bool prepare(const State&s,int me,double origin){
root_enabled=false;for(double&p:root_probability)p=0;root_probability[UNKNOWN]=1;
last_sims=0;bool observation_valid=false;
for(std::size_t k=0;k<MODELS;k++){model_ms[k]=0;model_guard_limited[k]=false;}
if(!unknown_only&&have_previous&&previous_predictions_valid){
const double deadline_ms=tank::budget_used()+5.;
const auto forward=[](State&state,const Orders&a,const Orders&b){sim_step(state,a,b);};
const auto deadline=[&](){return tank::budget_used()<deadline_ms&&tank::budget_left()>=16;};
const auto result=ensemble_transition_belief::update(previous,our_last,s,me,predicted_last,logs,forward,deadline);
last_status=int(result.status);observation_valid=result.posterior_valid;
if(result.posterior_valid){
for(std::size_t k=0;k<MODELS;k++)if(plausible(int(k))&&!std::isfinite(result.log_weights[k])){
refuted_turn[k]=s.turn;refute_reason[k]=result.known_match[k]?2:1;
}
logs=result.log_weights;refresh_unknown_only();
}
}
std::array<bool,MODELS> called{};
consume_next(s,me,called);
if(!unknown_only){
for(std::size_t p=0;p<MODELS;p++){
const int k=MODEL_CALL_SCHEDULE[p];if(!plausible(k)||called[k])continue;
if(!model_budget_available(p,s.turn,&called))break;
const double begin=tank::budget_used();predicted_now[k]=model_predict(k,s,1-me);
model_ms[k]=tank::budget_used()-begin;++calls[k];called[k]=true;
accept_complete_prediction(k,s.turn);
}
}
bool retained_complete=true;
for(std::size_t k=0;k<MODELS;k++)if(plausible(int(k))&&!called[k])retained_complete=false;
if(!unknown_only&&observation_valid&&retained_complete){
double sum=0;for(std::size_t k=0;k<=MODELS;k++){root_probability[k]=std::isfinite(logs[k])?std::exp(logs[k]):0;sum+=root_probability[k];}
if(sum>0){for(double&p:root_probability)p/=sum;root_enabled=true;}
}
predicted_last=predicted_now;previous_predictions_valid=retained_complete&&!unknown_only;
last_helper_ms=tank::budget_used()-origin;return !unknown_only;
}
static int draw_mode(const State&s,int simulation){
if(!root_enabled)return UNKNOWN;
const uint64_t bits=mix64(s.wall_hash^uint64_t(s.turn+1)*0xa0761d6478bd642fULL^uint64_t(simulation+1)*0xd6e8feb86659fd93ULL);
const double u=double(bits>>11)*(1./9007199254740992.);
double cumulative=0;for(std::size_t k=0;k<MODELS;k++){cumulative+=root_probability[k];if(u<cumulative)return int(k);}
return UNKNOWN;
}
static bool force_root_command(int depth,int unit,int me,int mode,Cmd&command){
if(!root_enabled||mode>=UNKNOWN||depth!=0||unit/3==me)return false;
command=predicted_now[mode].cmd[unit%3];++forced_root_commands;return true;
}
static int filter_map_mode(){
if(!root_enabled)return UNKNOWN;
int best=UNKNOWN;double probability=root_probability[UNKNOWN];
for(std::size_t k=0;k<MODELS;k++)if(root_probability[k]>probability){probability=root_probability[k];best=int(k);}
return best;
}
static bool validated_filter_orders(Orders& enemy){
const int mode=filter_map_mode();if(mode==UNKNOWN)return false;
enemy=predicted_now[mode];return true;
}
static Orders remember(const State&s,const Orders&out,double origin){
previous=s;our_last=out;have_previous=true;
prefetch_next(s,out,origin);
last_total_ms=tank::budget_used()-origin;
last_native_ms=std::max(0.,last_total_ms-last_helper_ms);return out;
}
}
static double init_cpu_ms=0,call_cpu_start=0,call_init_ms=0,call_cpu_limit=38;
static bool first_decision=true;
void init(const State& s,int me){
const double init_start=tank::budget_used();first_decision=true;
specialized_root_default=(s.P==Params{});
if(!pool)pool.reset(new Node[POOL_CAPACITY]);
for(int d=0;d<=2048;d++)prior_exp[d]=std::exp(float(-d)/float(SP_PRIOR_T));
(void)me;
M.valid = false;
total_steps = 0;
for(int t = 0;t < 3;++t)prev_target[t]= -1;
for(int j = 0;j < 6;++j){still[j]= 0;still_pos[j]= -1;}
stale_turns = 0;
stale_hp[0]= stale_hp[1]= stale_kills[0]= stale_kills[1]= -1;
stale_walls = 0;
build_maps(s);init_opening(s);
opponent_belief::reset(s,me);
init_cpu_ms=tank::budget_used()-init_start;
}
#if SP_METHOD == 1
static Orders decide_ibr(const State& s,int me){
static Node nd[6];
Cmd choice[6];
for(int i = 0;i < 6;++i){
choice[i]= STAY;
nd[i].n_acts = 0;
if(!s.tank[i].alive)continue;
build_actions(nd[i],s,i);
choice[i]= nd[i].act[0];
}
const uint64_t seed = rng_state;
for(int round = 0;round < SP_IBR_ROUNDS;++round){
bool changed = false;
for(int k = 0;k < 6;++k){
const int i =(round & 1)? 5 - k : k;
if(nd[i].n_acts <= 1)continue;
int best = -1;
long long best_score = 0;
for(int a = 0;a < nd[i].n_acts;++a){
Orders o[2];
for(int j = 0;j < 6;++j)o[j / 3].cmd[j % 3]= j == i ? nd[i].act[a]: choice[j];
State st = s;
sim_step(st,o[0],o[1]);
long long sum = evaluate(st);
rng_state = seed;
for(int d = 1;d < SP_DEPTH;++d){
if(st.done){sum += evaluate(st);continue;}
Orders q[2];
for(int j = 0;j < 6;++j)
if(st.tank[j].alive)q[j / 3].cmd[j % 3]= rollout_cmd(st,j);
sim_step(st,q[0],q[1]);
sum += evaluate(st);
}
if(i >= 3)sum = -sum;
if(best < 0 || sum > best_score){best_score = sum;best = a;}
}
if(nd[i].act[best]!= choice[i]){choice[i]= nd[i].act[best];changed = true;}
}
if(!changed)break;
}
Orders out;
for(int type = 0;type < 3;++type)out.cmd[type]= choice[tank_id(me,type)];
return out;
}
#endif
#ifdef ANYTIME_TEST
static int anytime_test_stop=0;
static bool anytime_test_returned_incumbent=false;
#endif
static bool anytime_injected_stop(int phase){
#ifdef ANYTIME_TEST
return anytime_test_stop==phase;
#else
(void)phase;return false;
#endif
}
static Orders anytime_ff(const State&s,int me,const int root[6],Orders out,bool current_prediction){
#if SP_FFFIX
for(int round = 0;round < 3;++round){
Orders o[2];
o[me]= out;
for(int type = 0;type < 3;++type){
const Node& nd = pool[root[tank_id(1 - me,type)]];
int best = -1;
for(int a = 0;a < nd.n_acts;++a)
if(best < 0 || nd.n[a]> nd.n[best])best = a;
if(best >= 0)o[1 - me].cmd[type]= nd.act[best];
}
if(current_prediction)opponent_belief::validated_filter_orders(o[1-me]);
State st = s;
EventLog log;
step(st,o[0],o[1],&log);
int culprit = -1;
for(const Event& e : log.list)
if((e.type == EV_HIT || e.type == EV_HIT_SWAP)&& e.a / 3 == me && e.b / 3 == me && is_fire(out.cmd[e.b % 3]))culprit = e.b;
if(culprit < 0)break;
const Node& nd = pool[root[culprit]];
int best = -1;
for(int a = 0;a < nd.n_acts;++a)
if(!is_fire(nd.act[a])&&(best < 0 || nd.n[a]> nd.n[best]))best = a;
out.cmd[culprit % 3]= best >= 0 ? nd.act[best]: STAY;
}
#endif
return out;
}
static Orders anytime_refine(const State&s,int me,const int root[6],Orders out){
Orders threat;bool urgent=false;
for(int type=0;type<3;type++){
const int foe=tank_id(1-me,type);const Node& nd=pool[root[foe]];
if(nd.n_acts)threat.cmd[type]=nd.act[0];
if(!s.tank[foe].alive||!can_fire(s,foe))continue;
bool aimed=false;
for(int d=0;d<4&&!aimed;d++){
int p=s.tank[foe].pos;
for(int distance=1;distance<=2;distance++){
p+=DIR_DELTA[d];const uint8_t g=s.grid[p];
if(g!=EMPTY&&g!=WATER)break;
const int victim=tank_at(s,p);
if(victim>=0){
if(victim/3==me&&s.tank[victim].hp==1){threat.cmd[type]=cmd_fire(d);urgent=aimed=true;}
break;
}
}
}
}
if(!urgent)return out;
auto grade=[&](const Orders& ours){
State next=s;step_as(next,me,ours,threat);
int alive=0;for(int i=3*me;i<3*me+3;i++)alive+=s.tank[i].alive&&next.tank[i].alive;
const int outcome=next.done&&next.winner==me?2:next.done&&next.winner>=0?0:1;
return std::array<int,3>{outcome,alive,(me?-1:1)*evaluate(next)};
};
auto best=grade(out);
for(int type=0;type<3;type++){
const Node& nd=pool[root[tank_id(me,type)]];Orders base=out;
for(int a=0;a<nd.n_acts;a++){
if(budget_left()<8)return out;
Orders trial=base;trial.cmd[type]=nd.act[a];const auto score=grade(trial);
if(score>best){out=trial;best=score;}
}
}
return out;
}
static Orders anytime_incumbent(const State&s,int me,const int root[6],bool current_prediction){
Orders out;
for(int type=0;type<3;type++){
const Node& nd=pool[root[tank_id(me,type)]];
if(nd.n_acts)out.cmd[type]=nd.act[0];
}
return anytime_ff(s,me,root,anytime_refine(s,me,root,out),current_prediction);
}
Orders incumbent_decide(const State& s,int me){
call_cpu_start=tank::budget_used();call_init_ms=first_decision?init_cpu_ms:0;
#ifdef ANYTIME_TEST
anytime_test_returned_incumbent=false;
#endif
const bool first=first_decision;first_decision=false;
specialized_root_default=(s.P==Params{});
build_maps(s);
build_tank_maps(s);
assign_roles(s,me);
for(int i=0;i<6;i++)opening_active[i]=s.turn<11||(s.turn<17&&s.tank[i].level==0&&(s.star_mask&(1<<opening_star[i])));
std::memcpy(root_grid,s.grid,sizeof(root_grid));
soon_mask = 0;
{
const int t = s.turn;
int next = s.P.supply_first;
if(t >= next)next +=((t - s.P.supply_first)/ s.P.supply_period + 1)* s.P.supply_period;
if(next - t <= 8)soon_mask =((1 << s.n_supply)- 1)& ~s.star_mask;
}
rng_state = 0x9E3779B97F4A7C15ULL ^(uint64_t(s.turn)* 0x100000001B3ULL)^ s.wall_hash;
for(int k = 0;k < 4;++k)rng();
make_key_seeds(s.turn);prepare_keys(s);
pool_used = 0;std::memset(state_table,0,sizeof(state_table));
int root[6];for(int i=0;i<6;i++)root[i]=lookup_state(s,i,true);
actor_root(s,root);
Orders incumbent=anytime_incumbent(s,me,root,false);
const double helper_begin=budget_used();
const bool helper_active=opponent_belief::prepare(s,me,helper_begin);
call_cpu_limit=first?opponent_belief::last_helper_ms+24:32;
if(budget_left()<8||budget_used()-call_cpu_start+call_init_ms>=call_cpu_limit||anytime_injected_stop(1)){
#ifdef ANYTIME_TEST
anytime_test_returned_incumbent=true;
#endif
return opponent_belief::remember(s,anytime_ff(s,me,root,incumbent,true),call_cpu_start);
}
if(opponent_belief::filter_map_mode()==7&&opening_active[tank_id(me,FIRE)]){
opening_active[tank_id(me,FIRE)]=false;
pool_used=0;std::memset(state_table,0,sizeof(state_table));prepare_keys(s);
for(int i=0;i<6;i++)root[i]=lookup_state(s,i,true);
actor_root(s,root);incumbent=anytime_incumbent(s,me,root,true);
}
#if SP_METHOD == 1
return opponent_belief::remember(s,decide_ibr(s,me),call_cpu_start);
#endif
int steps = 0,sims = 0;
struct Visit{int32_t node;int8_t act;};
Visit path[6][SP_DEPTH];
int path_len[6];
for(;;){
if(anytime_injected_stop(2))break;
if(steps + SP_DEPTH > SP_BUDGET)break;
#if SP_USE_TIME
if(budget_left()< 8 ||(budget_used()-call_cpu_start+call_init_ms)> call_cpu_limit)break;
#endif
const int belief_mode=opponent_belief::draw_mode(s,sims);
State st = s;
int cur[6];
bool expanded[6]={};bool added=false;
for(int i = 0;i < 6;++i){cur[i]= root[i];path_len[i]= 0;}
int e_sum = 0,depth_done = 0;
bool complete_rollout=true;
for(int depth = 0;depth < SP_DEPTH && !st.done;++depth){
#if SP_USE_TIME
if(budget_left()<8||budget_used()-call_cpu_start+call_init_ms>call_cpu_limit){
complete_rollout=false;break;
}
#endif
prepare_keys(st);
for(int i=0;i<6;i++){cur[i]=lookup_state(st,i,!expanded[i]);if(cur[i]>=0&&pool[cur[i]].visits==0&&depth>0)expanded[i]=true;}
prepare_scan(st);
Orders o[2];
for(int i = 0;i < 6;++i){
if(!st.tank[i].alive)continue;
Cmd c;
if(opponent_belief::force_root_command(depth,i,me,belief_mode,c)){
o[i/3].cmd[i%3]=c;continue;
}
if(cur[i]>= 0){
Node& nd = pool[cur[i]];
if(nd.n_acts == 0)build_actions(nd,st,i);
const int a = select_action(nd);
c = nd.act[a];
path[i][path_len[i]++]= Visit{cur[i],int8_t(a)};
}else{
c = rollout_cmd(st,i);
}
o[i / 3].cmd[i % 3]= c;
}
sim_step(st,o[0],o[1]);
++steps;
++depth_done;
e_sum += evaluate(st);
if(st.done)e_sum +=(SP_DEPTH - depth_done)* evaluate(st);
}
if(!complete_rollout)break;
#if SP_USE_TIME
if(budget_left()<8||budget_used()-call_cpu_start+call_init_ms>call_cpu_limit)break;
#endif
#if SP_PATH_AVG
const int e = e_sum / SP_DEPTH;
#else
const int e = evaluate(st);
#endif
const float v = st.done ?(st.winner<0?0.f:st.winner==0?1.f:-1.f): 0.650000f*float(e)/(float(SP_SQUASH)+std::abs(float(e)))+0.350000f*neural_value(st);
for(int i = 0;i < 6;++i){
const float vi = i < 3 ? v : -v;
for(int k = 0;k < path_len[i];++k){
Node& nd = pool[path[i][k].node];
nd.visits += 1;
nd.total += vi;
nd.n[path[i][k].act]+= 1;
nd.sum[path[i][k].act]+= vi;
}
}
++sims;
}
total_steps += steps;opponent_belief::last_sims=sims;
#ifdef SP_DEBUG
if(me == SP_DEBUG){
std::fprintf(stderr,"T%d sims=%d nodes=%d\n",s.turn + 1,sims,pool_used);
for(int i = 0;i < 6;++i){
const Node& nd = pool[root[i]];
std::fprintf(stderr,"  tank%d Q=%.3f:",i,nd.visits ? nd.total / nd.visits : 0.f);
for(int a = 0;a < nd.n_acts;++a)
std::fprintf(stderr," %s(p%.2f n%d q%.3f)",cmd_text(nd.act[a]).c_str(),nd.p[a],nd.n[a],nd.n[a]? nd.sum[a]/ nd.n[a]: 0.f);
std::fputc(10,stderr);
}
}
#endif
Orders out=incumbent;
#ifdef ANYTIME_TEST
if(!sims)anytime_test_returned_incumbent=true;
#endif
for(int type = 0;type < 3&&sims;++type){
const Node& nd = pool[root[tank_id(me,type)]];
int best = -1;
for(int a = 0;a < nd.n_acts;++a)
if(best < 0 || nd.n[a]> nd.n[best]||(nd.n[a]== nd.n[best]&& nd.n[a]> 0 && nd.sum[a]> nd.sum[best]))best = a;
if(best >= 0)out.cmd[type]= nd.act[best];
}
return opponent_belief::remember(s,anytime_ff(s,me,root,out,true),call_cpu_start);
}

// 第五轮终局证书；上方原生代码保持原逻辑。
// 可选的当前回合全称胜利证书；截止谓词为真才可继续。
// 规范合法指令的完整组合覆盖全部终局状态效果；必须穷尽敌方响应。
// 仅官方结算确认为己方胜利才认证；任一截止失败都保留原候选。
namespace terminal_win_guard{
struct Result{tank::Orders orders;bool certified;};
template<class Deadline> Result improve(const tank::State&s,int me,const tank::Orders&incumbent,Deadline deadline){
const Result fallback{incumbent,false};
if(s.done||me<0||me>1||s.base_hp[1-me]!=1||!deadline())return fallback;
tank::Cmd list[6][tank::N_CMDS];int n[6];
for(int i=0;i<6;i++){
if(!deadline())return fallback;
n[i]=tank::legal_commands(s,i,list[i]);if(n[i]<=0)return fallback;
}
const int own=3*me,enemy=3*(1-me);
std::size_t own_total=1,enemy_total=1;
for(int t=0;t<3;t++){own_total*=n[own+t];enemy_total*=n[enemy+t];}
auto verify=[&](const tank::Orders&mine){
std::size_t tested=0;
for(std::size_t k=0;k<enemy_total;k++){
if(!deadline())return -1;
std::size_t x=k;tank::Orders theirs;
for(int t=0;t<3;t++){theirs.cmd[t]=list[enemy+t][x%n[enemy+t]];x/=n[enemy+t];}
if(!deadline())return -1;
tank::State trial=s;
if(!deadline())return -1;
tank::step_as(trial,me,mine,theirs);++tested;
if(!deadline())return -1;
if(!trial.done||trial.winner!=me)return 0;
}
return tested==enemy_total&&enemy_total>0&&deadline()?1:-1;
};
int verdict=verify(incumbent);
if(verdict<0)return fallback;
if(verdict>0)return {incumbent,true};
for(std::size_t k=0;k<own_total;k++){
if(!deadline())return fallback;
std::size_t x=k;tank::Orders mine;bool same=true;
for(int t=0;t<3;t++){
mine.cmd[t]=list[own+t][x%n[own+t]];x/=n[own+t];
same= same&&mine.cmd[t]==incumbent.cmd[t];
}
if(same)continue;
verdict=verify(mine);
if(verdict<0)return fallback;
if(verdict>0)return {mine,true};
}
return fallback;
}
}

// 合作式目标4毫秒，预留官方余量8毫秒；不声称严格最坏执行时限。
// 原候选已完成一次友伤检查与记忆；证书通过后不再修改已认证动作。
Orders decide(const State&s,int me){
Orders out=incumbent_decide(s,me);
if(s.done||s.base_hp[1-me]!=1)return out;
const double begin=tank::budget_used(),allowance=std::min(4.,tank::budget_left()-8.);
terminal_win_guard::Result r{out,false};
if(allowance>0)r=terminal_win_guard::improve(s,me,out,[&]{return tank::budget_used()-begin<allowance&&tank::budget_left()>8.;});
if(r.certified){out=r.orders;opponent_belief::our_last=out;}
opponent_belief::last_total_ms=tank::budget_used()-call_cpu_start;
opponent_belief::last_native_ms=std::max(0.,opponent_belief::last_total_ms-opponent_belief::last_helper_ms);
return out;
}
