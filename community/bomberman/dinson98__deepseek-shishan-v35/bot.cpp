// ============================================================================
// deepseek 第三十二版 = 第三十一版 + 【全人数统一（去 P 分叉）】
// （2026-10-16 · 正式赛 2–10 人全打 → 一套配置；把 2 人局验证过的胜利要素带给 3–10 人）
//
// 原则：① 不再有任何按人数的条件分支（P<=2 / P>2 全清）；
//       ② 所有"绝对回合"窗口改为由配置推导：DEATHT = SHRINK + (LK-1)*EVERY（LK=最终存活层），
//          5p 下与 v31 数值逐一相等（340/345/330/360/400 ↔ DEATHT-85/80/95/65/25）；
//       ③ 终局"中心"改为通用"最终存活格集合"（地板中心=单格；墙中心=3×3 环 8 格）；
//       ④ rival = 最外围对手（离边界最近；并列取离我最近；2p 时=唯一对手）；
//       ⑤ 统一取值原则：2p 的胜利特征（追击/封锁/安全闸门）全人数启用；纯度量取更稳的一侧。
// ============================================================================
// deepseek 第二十九版 = 第二十七版 + 单开关默认值改动：【GUIDE_W 50000 -> 4000】
// （2026-10-16 · 决策层定点修复：撤离向导不得压过生存步权）
//
// 背景：dbg2 逐段分解（perf/patch_v29e.py + e6_dbg.exe）实锤 s1201@T372：
//   留在 (7,10)（lifeOpt=1）被 ESC_GUIDE 扣 -100000（distSafe=2），
//   扑进火格 (7,11)（lifeOpt=0 必死）只扣 -50000（distSafe=1）→ 选必死项。
//   对账：D 胜 37797.6 = GUIDE_W(50000) - (life 10000+pess 2200+deep 600) + 597.6。
//   同类死亡 100 局内 8 例，其中 7 例决策被本改动修正（3 例核验 life 0→1/2/3）。
//
// 改动：`#define GUIDE_W` 默认值 50000 -> 4000（< 生命步权 ≈12200）；2p 侧新增 `DUEL_GUIDE_W=50000` 保持原行为。
//   影响面：P>2 用 GUIDE_W=4000（经 e7 三区块 300 局验证）；P<=2 用 DUEL_GUIDE_W=50000（2p 行为=原版）。
//
// 竞测（2026-10-16，100 局区块 ×3）：见 perf/cyc_e7 / cyc_e7b / cyc_e7c。
// ============================================================================
// deepseek 第二十七版 = 第二十六版 + 单开关默认值改动：【RIVAL_REACH 默认关闭】
// （2026-10-16 · 泛化重校第一轮）
//
// 背景：v26（=v25 行为）在 150 局基线（seeds 1200-1349）上对 24c 净亏 -0.099；
//   归因消融（perf/ab1300）定位：1300-1349 段的 -0.450 完全来自危险度定价
//   RIVAL_REACH —— 关掉后该段反超 24c（+0.170）、1250-1299 +0.147、
//   1200-1249 平局（-0.020）；CELL_VALUE 在该段全惰性（逐局零差异）。
//
// 改动（唯一）：`#define RIVAL_REACH` 默认值 1 -> 0（代码与开关全保留；
//   `-DRIVAL_REACH=1` 可复现 v25/v26 行为做对照实验）。
// 影响面：RIVAL_REACH 仅对 P>2 生效 → 2 人局逐字节不变（照例跑 40 局回归）。
//
// 验收数据（2026-10-16，workers=1）：见 `deepseek第二十七版-说明.md`。
// ============================================================================
// deepseek 第二十五版 = 第二十四版-方案C（CHASE_PAIRS=0 / STAY_GUARD=0）为基础
//   + 两处"多人局补强"（2026-10-15，针对 5 人局 seed1203 / seed1207 两个负局）
//
// 背景：方案C 在 2 人局 1.475/第一率100%，但 5 人局 10 局配对里有两个 0 分/低分局：
//   seed1203（0.00, r5）t=360 在 row7 单格走廊 (8,7) 放雷，唯一垂直出口 (9,8) 被 Opus 占住，
//                        仍沿火线右跑 → t=364 自爆。
//   seed1207（1.00, r4）t=368~375 卡在 (11,9)，连续 8 回合请求 L→(10,9) 被 Sol 争格挡下，
//                        t=375 e7 变墙被缩圈压死（V244 当时在 (10,9) e8，活到 t=400 拿 2.25）。
//
// 改动（各有 #ifndef 开关）：
//   ① GUIDE_BLOCK_RIVAL=1  撤离向导 distSafe 的 BFS 把"活着的对手所在格"当不可通行（仅 P>2）。
//   ② ANTI_DEADLOCK_GEN=1  把"上一步被挡 → 惩罚重复方向"开放到多人局；多人局惩罚用
//                          BLOCK_PEN_MP*blockedCd^2（=60000/240000/540000），量级足够翻过
//                          "存活层级"高权重。P<=2 时表达式与 v24c 完全等价。
//
// 实测结论（2026-10-15）：
//   * 消融：两开关全关 → 逐局完全退回 v24c（1203=0.00 / 1207=1.00，死亡回合一致）。
//   * 单独开① 不修 1203（撤离向导的权重压不过"存活层级"项）；单独开② 能修好两局，
//     但也会打坏 1200/1202/1204/1205/1208；①②合开效果最好（互相抵消一部分扰动）。
//   * 2 人局 vs Astra：40 局配对逐局完全相同（1.475 / 第一率 100% / 零死亡）→ 无回归。
//   * 5 人局 vs Astra/Sol/Opus/第十版：40 局配对 1.969 → 1.913（-0.056，噪声级；
//     逐局 15 好 / 7 平 / 18 差）→ 两局目标修好，整体是"打地鼠"式的净持平。
//
// 红线：以上两处对 P<=2 逐字节等价 → 2 人局行为不变（40 局 vs Astra 配对回归验证）。
// 关闭开关可回到 v24c 原口径：-DGUIDE_BLOCK_RIVAL=0 -DANTI_DEADLOCK_GEN=0
// ============================================================================
// deepseek 第十五版 —— 2 人对 astra 专项
// 全部内容来自人类玩家实战回放（三个胜局结构一致）：站在**顶点**（四个斜对角都是
// 永久墙的格子，13x13 上就是 x,y ∈ {3,5,7,9} 那 16 格），比对手靠内一层，用 STAY
// 占住对手唯一能进来的那格；缩圈压死（1.0）或在门下放弹、直线撤离收人头（1.5）。
// 关键开关（默认值见下方）：
//   SWAP_FIX=1       绝不打进对手当前格（同格交换 = 被换到外层、被缩圈压死）
//   DEADEND_BAN=1    下了雷之后不进"三面都是墙"的格子
//   MINE_ESCAPE=1    下了雷之后落点必须走出自己那颗雷的火线
//   SHADOW_ALWAYS=2  对列镜像：跟对手所在的列/行，但只踩顶点
//   GATE_SEEK=2      逐格扫"能让对手封死"的门格，主动走过去
//   LEAF_PEN=20000   不踩叶子格（只有一个出口的格子）
//   BOMB_GATE=1      只允许三种放弹：线上有箱子 / 门口收人头 / 站在对手唯一出口上
//   DEPTH_GATE=1     DEPTH_W 只在"我比对手更靠外"时生效
// 详细证据与 A/B 数据见 deepseek第十五版-说明.md。
#include <bits/stdc++.h>
using namespace std;

static const int DX[5] = {0, 0, -1, 1, 0};
static const int DY[5] = {-1, 1, 0, 0, 0};
static const char DS[5] = {'U', 'D', 'L', 'R', 'S'};

#ifndef MR_WIN
#define MR_WIN 1000000
#endif
// ===== v24：用户规则 —— "日字追身"（CHASE_FOLLOW）=====
// 当对手与我呈"日"字形（|dx|,|dy| = 2,1）且它刚刚移动过：跟着它同方向走，保持贴身、才追得上。
// 依据（seed600 录像 t=48）：我 (3,7)、Astra (1,8) 正好日字形，它刚往下走一步；
// 当时 U/S 得 1120356、D 只有 867121（差 253235）→ bot 选了停。
// 实测发现"加分"不够：CROSS_HOLD（顶点站桩）会覆盖最终决策 →
// 做成**和锁门同级的覆盖式规则**：跟随方向有安全落点（life≥6，两个世界都活）就直接执行。
#ifndef CHASE_FOLLOW
#define CHASE_FOLLOW 1
#endif
// ===== v24：从 v22 移植 —— "放雷撤退"（MINE_FLEE）=====
// 用户指令："v22版有放雷撤退功能，加上去"。
// v22 原语义：本回合我站在**自己旧雷**的火线里时，落点必须离开火线（重罚）；原地不动再罚一层。
//   （只看这一拍之前就存在的雷——下雷那一拍落点还在火线里是正常的，还有 FUSE 回合可撤；
//    真正要罚的是"下完之后继续赖在火线里"。）
// v24 改动：作用范围从 v22 的"仅 P>2"放开到**全人数**（2 人局同样生效，这是 v24 的主战场）。
#ifndef MINE_FLEE
#define MINE_FLEE 1
#endif
#ifndef MINE_FLEE_PEN
#define MINE_FLEE_PEN 8000.0   // v30：5p 值（2p 用字面量 300k）
#endif
#ifndef MINE_FLEE_STAY
#define MINE_FLEE_STAY 4000.0   // v30：5p 值（2p 用字面量 200k）
#endif
// MINE_FLEE_LEAD：只在"自己的雷还差 MINE_FLEE_LEAD 回合就爆"时才强制撤离。
//   实测（100 局 vs Astra，seeds 600-699）：
//     99（全程撤）= -0.010（罚太早，会打断正常的守位/收人头节奏，丢 10 局击杀）
//     2 （爆前 2 回合才撤）= **+0.025**（保住击杀链、又能修掉"爆前还赖在火线里"的死亡）
//   → 定稿 2。注：FUSE=4 时站在自己雷火线里距爆最多 3 回合，故 LEAD≥3 与 99 同效。
#ifndef MINE_FLEE_LEAD
#define MINE_FLEE_LEAD 2
#endif
// ===== v24.1：从 v23t 移植 —— "三面墙 + 我" 死角收人头（DEADEND_BOMB）=====
// 用户指令（seed603 复盘："191 回合应该放雷"）：
//   某个对手被永久墙围住三面、第四面的唯一出口就是我脚下这一格 → 它已经无处可走。
//   我在脚下放雷（相邻 ⇒ 爆风必然覆盖它那格；它也无法走进被雷占住的我这格），然后撤出火线。
// 安全底线（"观察场上局势"）：落点要在【正常世界 + 悲观世界（含对手反手放雷）】都活过起爆拍。
//   （v23t 上实测：曾在 seed602 打出"条件成立 → 放雷 → 炸死"的完整击杀链。）
#ifndef DEADEND_BOMB
#define DEADEND_BOMB 1
#endif
// ===== v24.2：从"seed657 / seed644 复盘"来的两条修复 =====
// ESC_GUIDE：撤离向导 —— ①站在自己旧雷火线里时，落点每靠近"安全格"一步就扣分
//   （多源 BFS：安全格 = 不被任何现有雷覆盖的空地。seed657 t=25：U(3,6)【1 步脱险】应胜 R(4,7)【4 步】）；
//   ②撤离中禁止再放雷（seed657 t=28 的致命第二雷：在撤退路线上再放一颗，被链爆提前引爆，
//   行火线正好覆盖撤退目标 (7,7)，把自己炸死）。
#ifndef ESC_GUIDE
#define ESC_GUIDE 1
#endif
#ifndef GUIDE_W
#define GUIDE_W 4000.0    // v29：50000->4000（撤离向导"多步距离"权重必须 < 生命步权 ≈12200，
//   否则会选"必死但离安全格更近"的格子）。注：不能用 ESC_W 这个名字——它已被"对手生存压缩项"占用。
#endif
#ifndef DUEL_GUIDE_W
#define DUEL_GUIDE_W 50000.0   // v29：2 人局保留原值（= v24c~v27 行为；5p 用 GUIDE_W=4000）
#endif
#ifndef ENDGAME_CTR_W
#define ENDGAME_CTR_W 8000.0   // v30：终局中心引力（开）
#endif
#ifndef ENDGAME_CTR_T
#define ENDGAME_CTR_T 340   // v30：引力起始回合
#endif
#ifndef CHOKE_ATK
#define CHOKE_ATK 100.0    // 双覆盖雷轻税
#endif
#ifndef CHOKE_NEAR_MAX
#define CHOKE_NEAR_MAX 2   // 近处覆盖半径
#endif
#ifndef CHOKE_FAR_MIN
#define CHOKE_FAR_MIN 2    // 远处覆盖半径下限
#endif
#ifndef CHOKE_FORM
#define CHOKE_FORM 0       // 0=双覆盖；1=near+far-cover 变体
#endif
#ifndef CHOKE_PEN
#define CHOKE_PEN 500.0    // 否则重税
#endif
#ifndef FLEE_NO_BOMB_PEN
#define FLEE_NO_BOMB_PEN 300000.0
#endif
// SEEK_RIVAL：开局主动找对手 + 用雷清障（用户指令：seed644"应该开局找对手，清掉障碍物"）。
//   对"对手"做箱子可通行的 BFS（dRiv2）：缩圈前每靠近对手一步加 SEEK_PULL；
//   放雷能烧掉"比我更靠近对手"的箱子 → 每个 +SEEK_OPEN_W。
//   （seed644 里它卡在 (6,1) 90+ 回合、放雷与原地只差 2.5 分 → 永远不炸挡路的 (7,1) 箱。）
#ifndef SEEK_RIVAL
#define SEEK_RIVAL 1
#endif
#ifndef SEEK_PULL
#define SEEK_PULL 0.0   // 实测：方向拉力会扰动全局轨迹（SEEK_only=1.390 < 都关 1.445）；
                        // 归零后只保留"卡住清障"（SEEK_OPEN），100 局 = 1.470、第一率 100%
                        //（> 都关 1.445），657/644 两局仍为 1.50。保留宏以便将来单独启用。
#endif
#ifndef SEEK_OPEN_W
#define SEEK_OPEN_W 60000.0
#endif
// 清障放雷只在我"被卡住"（可走邻格 ≤ SEEK_OPEN_STUCK）时才鼓励 ——
// seed638/663/685 复盘：三局死前都有一次"清障放雷"（在开阔区随意炸箱子），旧版不放则活。
#ifndef SEEK_OPEN_STUCK
#define SEEK_OPEN_STUCK 1
#endif
// ===== v24.3：从"seed619 / 633 / 655 复盘"来的三处修复 =====
// CHASE_OVER_MARGIN：追身覆盖的门槛 —— 追逐落点与全局最佳分的差距超过此值时不覆盖。
//   （seed633 t=91 复盘：S(9,3)=1332262 本该原地卡人，被追身 U=876346 无脑覆盖，差 45.6 万。）
//   配合"并列全收集"：追身不再用"同向优先"裁剪并列方向
//   （seed619 t=117：L(6,5)=864649 本是全场最高，被同向 U 覆盖）。
#ifndef CHASE_OVER_MARGIN
#define CHASE_OVER_MARGIN 300000.0
#endif
// CHASE_PAIRS：1 = 距离并列的追逐方向**全收集**（交给评分选）；0 = 旧版"同向优先"。
// 【方案C 定稿 0】实测：全收集 + 改道门槛（CHASE_FLIP_MARGIN）虽能逐帧满足人类指令
//   （seed619 t=117 走 L），但 100 局 1.405、4 局死亡（638/656/684/685）；
//   关掉后（本文件默认）100 局 **1.475、第一率 100%、零死亡**，为当前最优。
//   代价：619 的 t=117 动作未精确复现（轨迹从 t≈113 分叉，那一下走 R 而非 L），分数仍 1.50。
//   601 t=59 与 619 t=117 几何完全同型（601 要 U、619 要 L）—— 靠分差区分需要上面那套机制，已弃用。
#ifndef CHASE_PAIRS
#define CHASE_PAIRS 0
#endif
#ifndef CHASE_FLIP_MARGIN
#define CHASE_FLIP_MARGIN 10000.0
#endif
// STAY_GUARD：安全哨位 —— 场上无雷 + 两世界 life 一致 + 对手贴身（≤ STAY_GUARD_RANGE）时，
//   原地不动（place=0 的纯停留）给加成（"绝对安全就别多走"，seed655 t=187 复盘）。
// 【方案C 定稿 0】拆分归因：seed685 的死亡由本机制造成；且关掉后 655 靠 CHASE_OVER_MARGIN
//   门槛间接修复（仍 1.50）。全收集/哨位两个机制合计让全局从 1.475 掉到 1.405。
#ifndef STAY_GUARD
#define STAY_GUARD 0
#endif
#ifndef STAY_GUARD_W
#define STAY_GUARD_W 300000.0
#endif
#ifndef STAY_GUARD_RANGE
#define STAY_GUARD_RANGE 2
#endif

int N, P, ME, T, LIMIT, FUSE, RANGE, CAP, FIRE, SHRINK, EVERY;
vector<string> board;

struct Pl { int x, y, alive; };
struct Bm { int owner, x, y, at, from; };
struct Fl { int x, y, end; };
vector<Pl> players;
vector<Bm> bombs;
vector<Fl> flames;

#ifndef GATE_SEEK
#define GATE_SEEK 2
#endif
#ifndef GATE_W
#define GATE_W 200000.0
#endif
#ifndef GATE_PULL
#define GATE_PULL 3000.0
#endif
#ifndef LOCK_BOMB_SINGLE
#define LOCK_BOMB_SINGLE 1
#endif
#ifndef ANTI_DEADLOCK
#define ANTI_DEADLOCK 1
#endif
#ifndef BLOCK_PEN
#define BLOCK_PEN 6000.0   // v33：统一平方惩罚基数（破争格死锁；v32 线性 1500×cd 翻不动、v31 多人=0）
#endif
// ===== v25：多人局两处补强（2 人局逐字节不变；见文件头 CHANGELOG）=====
// ① GUIDE_BLOCK_RIVAL：撤离向导的多源 BFS 里把"活着的对手所在格"当作不可通行。
//    由来 seed1203 t=360：我方在 row7 的单格走廊 (8,7) 放雷（RANGE=3 覆盖整条 row7），
//    唯一垂直出口 (9,8) 随后被 Opus 占住；但 distSafe 不知道对手会挡路，
//    把"朝右跑(9,7)"与"朝左跑(7,7)"判成等价 → 沿火线跑到 (11,7) 自爆。
//    只对 P>2 生效，2 人局保持 v24c 原口径。
#ifndef GUIDE_BLOCK_RIVAL
#define GUIDE_BLOCK_RIVAL 1
#endif
// ② ANTI_DEADLOCK_GEN / BLOCK_PEN_MP：把"上一步被挡 → 惩罚重复同一方向"开放到多人局，
//    且惩罚随连续被挡次数平方增长（多人局里该方向常被"存活层级"高权重锁死，小罚翻不动）。
//    由来 seed1207 t=368~375：我方卡在 (11,9)，连续 8 回合请求 L→(10,9) 全被 Sol 争格挡下，
//    直到 t=375 e7 变墙被缩圈压死（V244 当时在 (10,9) e8 活到 400）。
//    P<=2 时表达式与 v24c 完全等价 → 2 人局逐字节不变。
#ifndef ANTI_DEADLOCK_GEN
#define ANTI_DEADLOCK_GEN 1
#endif
#ifndef BLOCK_PEN_MP
#define BLOCK_PEN_MP 0.0   // v30：5p 关（原 60000 会压过生命项）
#endif
// 多人局升级惩罚的最低连续被挡次数：1=第一拍被挡就升级；2=要连续被挡两拍才升级。
// 调大可以把"偶发一次被挡"排除在外，只在真·死锁时触发（减少对正常局的扰动）。
#ifndef BLOCK_MP_MINCD
#define BLOCK_MP_MINCD 1
#endif
// 多人局升级惩罚的"残局闸门"：只有当前格距变墙 <= BLOCK_MP_LEAD 回合时才升级。
// 实测：LEAD=12（只在残局生效）会把 1203 的修复也一起关掉（1203 仍 0.00）；
// 默认 0 = 不设闸门（中盘也生效）。虽然中盘扰动更大，但 40 局 5 人局净影响只有 -0.056。
#ifndef BLOCK_MP_LEAD
#define BLOCK_MP_LEAD 0
#endif
// ===== v25b：末圈"目的地歧义保护"（AMBIG_ALT_PEN）=====
// predictedDest 在多人局（COLL_RT=1）里：目标格**现在**站着对手 → 预测"原地不动"。
// 但引擎里对手可能让开，实际会把 bot 推进那个 raw target —— 模型算的是 A 格的分，
// 引擎落到的却是 B 格。seed1203 T=354：模型以为"D 被 Opus 挡住→原地 (9,8) 放雷"，
// 引擎把 bot 推进 (9,9)（Opus 让开了），t=356 被 P1 的雷炸死（V25 因此只拿 2.00；
// 人类同局活到 t=377 拿 5.00）。
// 处理：若 raw target 与预测落点不同、且 raw target 在 FUSE 回合内会变火线 → 重罚该方向。
// 只对 P>2 生效，2 人局逐字节不变。
#ifndef AMBIG_ALT_PEN
#define AMBIG_ALT_PEN 1
#endif
#ifndef AMBIG_ALT_W
#define AMBIG_ALT_W 300000.0
#endif
// ===== v25c：末圈"安全收割"（HARVEST）—— 实测失败，默认关 =====
// 用户给的击杀法：配合缩圈把对手堵死再炸。做法 —— 缩圈开始后，若"在脚下放雷"能覆盖到
// 存活对手、且 escapeAfterBomb() 确认 FUSE 拍内有活路 → 按覆盖人数给大额加分。
// 结果（seed1203，单局）：不管权重 120000/250000、不管层限 5/6，全部从 3.50 掉到 0.00
// —— 静态活路检查太乐观（对手会移动/争格），bot 会"炸了撤不出去"自杀。
// 保留实现与开关供后续研究；要启用：-DHARVEST=1。仅 P>2 生效。
#ifndef HARVEST
#define HARVEST 0
#endif
#ifndef HARVEST_W
#define HARVEST_W 250000.0
#endif
// 只在"真末圈"生效：缩圈层 >= HARVEST_LAYER（N=19 时 6 = 已收到 5x5 以内）。
// 实测不加这条会在中盘乱炸（seed1203 从 3.50 掉到 1.50、t=332 就死）。
#ifndef HARVEST_LAYER
#define HARVEST_LAYER 6
#endif
// 活路检查的步数上限：1 = 放雷后"一步就能走出火线"才算有活路（最保守）。
// 实测：用 FUSE(4) 太乐观（bot 会"炸了撤不出"自杀）→ 收到 1 试试。
#ifndef HARVEST_ESC
#define HARVEST_ESC 1
#endif
// ===== v25d：封口袋击杀（GEN_PIN）—— 用户录像里的击杀法 =====
// 人类录像 seed1203 的前两杀：对手被缩圈困在"只有一个出口的纵向走廊"里
// （t=230 Astra 在 (5,2)、(5,1) 已被 e1 封死；t=330 V10 在 (11,6)、(11,5) 已被 e5 封死），
// 人类走到走廊另一端放雷把对手闷死。
// 源码里已有这套机制（rivalPocketExit → pinExit → PIN_W 拉过去），但对多人局硬编码关闭：
//   rivalPocketExit() 开头 `if (P > 2) return -1;`，且 pinExit 只在 P<=2 时计算。
// GEN_PIN=1 时开放到多人局（挑最近的"有口袋出口"的对手）。P<=2 行为不变。
#ifndef GEN_PIN
#define GEN_PIN 1
#endif
// ===== v25e：对手 3 拍可达集 → 危险度定价（RIVAL_REACH）=====
// 用户思路：把"每个对手在接下来 RIVAL_STEPS 拍内可能出现的所有格子"枚举出来（只按地形/缩圈/
// 炸弹推演，不猜对手意图），于是：
//   · rrCnt[dest] = 有多少个存活对手能在 RIVAL_STEPS 拍内到达该格 → 就是这格的"危险度"；
//   · rrCnt[dest] == 0 的格子 = **绝对安全格**（3 拍内没有任何对手能过来），给额外加成。
// 落点评分按危险度扣分，从而主动避开"会被争/被堵"的格子。只对 P>2 生效。
// v27（2026-10-16）：默认关闭（0）—— 1300 段泛化归因显示该定价在新区块净亏，详见文件头。
#ifndef RIVAL_REACH
#define RIVAL_REACH 0
#endif
#ifndef RIVAL_STEPS
#define RIVAL_STEPS 3
#endif
#ifndef RREACH_W
#define RREACH_W 8000.0
#endif
// ABS_SAFE_W：rrCnt==0（3 拍内无人能到）的额外加成。实测有害（+30000 → seed1203 从 4.00 掉到 0.00）
// —— "绝对安全"的格子往往在角落/外层，会把人拽出好位置。默认 0（只保留按危险度扣分）。
#ifndef ABS_SAFE_W
#define ABS_SAFE_W 0.0
#endif
// ===== v25f：细化"格子价值/危险度" =====
// 用户要求：危险度不只看周围放雷，还要看**地图固有属性**和**缩圈机制**；内圈天然比外圈值钱。
//   ① CELL_VALUE：落点结构分 = 出口数（openNbrs）+ 叶子格重罚 + 剩余存活拍数（缩圈）
//   ② SEAL_KILL ：**主动击杀** —— 对手站在我的火线里、且 FUSE 拍内走不出我的火线（被我堵死），
//                 同时我自己有活路 → 这是"保证安全"的稳杀，按堵死人数给大额加分。
// 两者都只对 P>2 生效。
#ifndef CELL_VALUE
#define CELL_VALUE 1
#endif
// 只在末圈生效（缩圈层 >= CV_MIN_LAYER）。实测：不设闸门时中盘就把 bot 往"出口多的开阔格"赶，
// 平均死亡从 348 掉到 291（10 局 5 人局 2.575 → 1.817）。
// 层限扫描（10 局 5 人局 1200-1209）：5 → 1.475；6 → 2.250；**7 → 2.775（最优）**；8 → 2.575。
// N=19 时 7 = e7 已变墙、可走区域只剩 3x3 —— 结构定价只在"3x3 抢位"这段才有正收益。
#ifndef CV_MIN_LAYER
#define CV_MIN_LAYER 7
#endif
#ifndef EXIT_W
#define EXIT_W 20000.0
#endif
#ifndef LEAF_MP_W
#define LEAF_MP_W 120000.0
#endif
#ifndef WALL_W
#define WALL_W 2000.0
#endif
#ifndef WALL_CAP
#define WALL_CAP 60
#endif
// 主动击杀（SEAL_KILL）：对手站在我的火线里、FUSE 拍内走不出（被我堵死）+ 我自己有活路 → 稳杀。
// **实测有害，默认关**：10 局 5 人局 2.775 → 2.275（击杀分也没涨）。原因：这条只在末圈偶发触发，
// 触发时把 bot 从更好的位置拽去放雷。保留实现供后续研究。要启用：-DSEAL_KILL=1。
#ifndef SEAL_KILL
#define SEAL_KILL 0
#endif
// SEAL_KILL 的末圈闸门：只在缩圈层 >= SEAL_MIN_LAYER 时生效（0=不设闸门）。
// 依据第 4 轮教训：任何"击杀激励"在中盘触发都会把人从好位置拽走（CELL_VALUE 同款问题）。
#ifndef SEAL_MIN_LAYER
#define SEAL_MIN_LAYER 7
#endif
// ===== v25g：预判式伏击（AMBUSH）—— 主动击杀的第二版 =====
// 用户思路："预测对手接下来会走到哪，在它的必经走廊上放雷"。
// 做法：对每个存活对手取他 RIVAL_STEPS 拍的可达集 R_i：
//   · |R_i| 小 = 他被地形/缩圈逼住（可预判）；
//   · 我的雷火线覆盖 R_i 的比例高 = 他大概率躲不开；
//   两者同时满足 + 我自己有活路 → 按"被伏击的对手数"加分。
// **实测有害，默认关**（80 局 1200-1279）：amb5(层≥5) 1.591 / amb5b(覆盖≥40%) 1.431 / amb7(层≥7) 2.022，
// 都低于不加它的 2.057；且单回合耗时飙到 354ms、出现 1 个故障局（性能风险）。
// 原因：候选循环里每个方向都要重算火线+覆盖率，代价高；而且"可达集覆盖率高"不等于真能收人头。
// 保留实现与开关供后续研究。要启用：-DAMBUSH=1。
#ifndef AMBUSH
#define AMBUSH 0
#endif
#ifndef AMBUSH_MIN_LAYER
#define AMBUSH_MIN_LAYER 5
#endif
#ifndef AMBUSH_MAXSET
#define AMBUSH_MAXSET 14
#endif
#ifndef AMBUSH_COVPCT
#define AMBUSH_COVPCT 60
#endif
#ifndef AMBUSH_ESC
#define AMBUSH_ESC 3
#endif
#ifndef AMBUSH_W
#define AMBUSH_W 250000.0
#endif
#ifndef SEAL_KILL_W
#define SEAL_KILL_W 400000.0
#endif
#ifndef SEAL_ESC
#define SEAL_ESC 2
#endif
#ifndef SHADOW_ALWAYS
#define SHADOW_ALWAYS 2
#endif
#ifndef SHADOW_MAX_STEPS
#define SHADOW_MAX_STEPS 3
#endif
#ifndef LEAF_PEN
#define LEAF_PEN 20000.0
#endif
#ifndef LEAF_LEAD
#define LEAF_LEAD 25
#endif
// 提前一层预测：算"下一次缩圈之后这个落点还有几个出口"，<=1 就不去。
// t=190 那一拍最毒：很多格子会在那一刻从 2 个出口掉到 1 个，我们站在里面就成了笼中鸟。
#ifndef LEAF_NEXT
#define LEAF_NEXT 0
#endif
#ifndef LEAF_NEXT_PEN
#define LEAF_NEXT_PEN 25000.0
#endif
// ===== v16：用户的完整策略规格 =====
// 1. 识别"上下左右都能走的点"（四个邻格都不是永久墙）
// 2. 走到**离对手最近、而且比对手靠内**的那个点
// 3. 那个点被可破坏的箱子占了 → 炸开它（放弹后火力覆盖该点才允许）
// 4. 站上去之后**不要动**；对手后退也不动；对手左右移动才跟着左右移动
#ifndef CROSS_MODE
#define CROSS_MODE 1
#endif
#ifndef CROSS_W
#define CROSS_W 250000.0
#endif
#ifndef CROSS_PULL
#define CROSS_PULL 4000.0
#endif
#ifndef CROSS_BOMB_W
#define CROSS_BOMB_W 120000.0
#endif
// CROSS_HOLD=1 无条件下发"站上十字点就不要动"——实测开局长站 = 送人头（负局 66/100）。
// 用户的"不要动"前提是**已经卡死对手**，所以默认 0：只靠 LOCK（对手真被封住）才站死。
// CROSS_HOLD 的语义（用户的澄清："这些策略的前提当然是你会主动躲炸弹"）：
//   0 = 不强制站死，只靠 LOCK
//   1 = 无条件站死（实测灾难：负局 66/100）
//   2 = **站死，但只要我的格子受到炸弹威胁就先躲开**，威胁过去再被 CROSS_PULL 拉回来
#ifndef CROSS_HOLD
#define CROSS_HOLD 2
#endif
// ===== v17：人类玩家给的三条 =====
// 引擎的动作语义（已从回放确认）：**先下雷再走** —— 雷下在"当前格"，然后才执行移动。
//
// ① SWAP_FIX：双方同时踩进对方格子 = **互换**。我们的模型把它算成"原地不动"，于是 bot
//    有一半概率发出"打进对手当前格"的方向 → 被引擎换到外层 → 缩圈压死。
//    证据：V16 在 t=165~199 的负局 23 个，23 个全部是同格交换；修掉后该段归零。
// ② DEADEND_BAN：下了雷之后绝不走进"三面都是墙"的格子（只剩一个出口，被雷封住必死）。
//    0=关；1=只在"我这一拍在下雷"时禁；2=只要有活雷就禁。
// ③ MINE_ESCAPE：雷下在**当前格**，所以落点必须走出这颗雷的火线 ——
//    否则就是"沿着自己的火线逃跑"。实测 s326/s343 就是这么死的。
#ifndef SWAP_FIX
#define SWAP_FIX 1
#endif
#ifndef DEADEND_BAN
#define DEADEND_BAN 1
#endif
#ifndef DEADEND_PEN
#define DEADEND_PEN 400000.0
#endif
#ifndef MINE_ESCAPE
#define MINE_ESCAPE 1
#endif
#ifndef MINE_ESCAPE_PEN
#define MINE_ESCAPE_PEN 200000.0
#endif
// MINE_EXIT：我正站在**自己某颗雷**的火线里时，落点也必须走出去 —— 不能沿着这条火线跑。
// 补的是 MINE_ESCAPE 漏掉的那半：本拍下雷、下一拍才移动（s342 就是这么死的）。
#ifndef MINE_EXIT
#define MINE_EXIT 0
#endif
#ifndef MINE_EXIT_PEN
#define MINE_EXIT_PEN 140000.0
#endif
// ===== v18：门闩收人头（LOCK_BOMB_SEAL）=====
// v17 的收人头条件里有一条 `!lckBlastHits(myCell, dest)` —— 要求撤退的第一步就
// **走出**自己的火线。但 RANGE=3 时以我为中心的四条线覆盖了全部四个邻格，
// 这个条件恒假 → lockBombOk 永远不会置位 → 一次收人头都没放过。
// 实测 seed301 t=151（我 (5,3) 挡住 astra 的 (5,2)，他 t=170 会被缩圈压死）：
// 在 (5,3) 放雷后 (4,3)→(3,3)→(3,4)，起爆是 t=155 —— 路线存在，
// 只是"穿过火线"被一刀切禁掉了。
// 改成数量正确的两条：
//   ① 我在**起爆那一拍**还活着  → opt.life[d] >= FUSE + 1
//   ② 对手**活不过那一拍**      → 他的 reach 最大 life <= FUSE
#ifndef LOCK_BOMB_SEAL
#define LOCK_BOMB_SEAL 1
#endif
// ===== v19：通用模型 —— 把 2 人局那套推广到所有人数 =====
// 下面这几条**不是 2 人局专属启发式，而是引擎规则级的正确性修正**，
// 在任何人数下都成立（旧版却把它们锁死在 P<=2）：
//   GEN_ALL=1  ① 绝不打进"对手当前所在格"（任何人数下都会被引擎判成互换/被顶）；
//              ② 下了雷不进"三面都是墙"的格子（被自己的雷封住必死）；
//              ③ 下了雷落点必须走出自己那条火线（否则就是沿着火线逃）。
//   GEN_LOCK=1 ④ 把"门格 + 门闩收人头"（LOCK / LOCK_BOMB_SEAL）开放到 5/10 人局，
//                 多人局里判据改成"至少一个对手被我封住且活不过起爆拍"。
// 每条规则独立开关（默认 0 = 只在 2 人局生效）。逐条 A/B 用，别一把全开。
#ifndef GEN_SWAP_T
#define GEN_SWAP_T 376   // v30：终局 3x3 争夺期启用
#endif
#ifndef GEN_SWAP
#define GEN_SWAP 1   // v32：全人数启用（原仅 2p）
#endif
#ifndef GEN_DEADEND
#define GEN_DEADEND 1
#endif
#ifndef GEN_ESCAPE
#define GEN_ESCAPE 1   // v32：全人数启用（原仅 2p）
#endif
#ifndef GEN_ALL
#define GEN_ALL 0
#endif
#ifndef GEN_LOCK
#define GEN_LOCK 0
#endif
// DEEP_MIN_ALIVE 原本人群局是 7 —— 也就是说 **5 人局里"对手会放雷"的威胁模型根本没开**。
// 5 人局的分数 91% 来自生存分，而 4 个对手的雷是主要死因，所以这个开关最值得试。
//
// v21 定稿：默认改成 **4**（=5 人局从第 1 回合起就开，10 人局继续）
//   证据（5 人局，用户阵容 ME+Astra+Sol+Opus+第十版，同种子逐局配对）：
//     seeds 1300-1379（80 局）：+0.058（36 好 / 13 同 / 31 差），平均死亡回合 359→369
//     seeds 1400-1499（100 局）：+0.077（47 好 / 18 同 / 35 差），平均死亡回合 355→374
//   合计 180 局 ≈ +0.069，两个独立种子块方向一致；代价是击杀分 0.142→0.105（用杀分换生存，5 人局划算）。
//   同族剂量 5 在 seeds 1200-1249 拿到 +0.230 但 seeds 1300-1379 只有 +0.021 → 取两腿都为正的 4。
//   另：`-DCROWD_DEEP_MIN_ALIVE=7` 可一键退回 v20 的人群局行为。
#ifndef CROWD_DEEP_MIN_ALIVE
#define CROWD_DEEP_MIN_ALIVE 4
#endif
#ifndef CROWD_PESS_W
#define CROWD_PESS_W PESS_W
#endif
// ===== 通用版"站桩"（GEN_HOLD）=====
// 2 人局的核心赢法是"站在比对手靠内的地方不动、等缩圈"；而 5 人局 **91% 的分来自生存分**，
// 所以这条更该生效。通用形式：只要"我这格活着"且"我比对手靠内"，
// 就在**生命值相同**的候选里优先原地不动（不越过 life 项，只做 tiebreak）。
//   GEN_HOLD=1 必须比**所有**活着的对手靠内；=2 比**一半以上**靠内就算。
#ifndef GEN_HOLD
#define GEN_HOLD 0
#endif
#ifndef HOLD_W
#define HOLD_W 6000.0
#endif
// ===== v20：按用户给的人群局打法 =====
// 用户规则："地图缩到 5x5 时全力争夺最中心的格子；在那之前全力搜索处在外层的玩家，
//            用 2 人对战的方式卡死他们，等缩圈让他们三面都是墙时炸死他们。"
// 现状的两个硬伤：
//   ① 人群局前瞻 H=20，而缩圈间隔 EVERY=25 → **bot 永远看不见下一次缩圈**（905 死于 t=376 的 e7 缩圈）。
//      这里用 O(1) 的"这格还能活到第几回合"（e=edgeD → 变墙回合 = SHRINK+(e-1)*EVERY）直接定价，
//      不依赖加大前瞻窗口（我们单回合已经 47~60ms，撞 50ms 限额）。
//   ② 人群局把"门格/卡死"整套关了 → 外层对手会自己跑进来活下来，白丢生存分。
#ifndef CROWD_CENTER
#define CROWD_CENTER 0        // 旧实现："越深越好"的单调奖励 —— 实测有害（-0.155），默认关
#endif
#ifndef CROWD_CENTER_D
#define CROWD_CENTER_D 5
#endif
#ifndef CROWD_CENTER_W
#define CROWD_CENTER_W 10000.0
#endif
// CROWD_KEEP：**阈值**模型 —— 目标是"活过下一次缩圈"，不是"越深越好"。
//   过下一次缩圈需要 edgeD >= shrinkLayer+2；我现在缺几层 = myLack；
//   落点缺几层 = dLack；补掉一层缺口 +CROWD_KEEP_PULL，缺得补不上时 -CROWD_KEEP_W*(来不及的层数)。
//   为什么是阈值：5 人局 LIMIT=400、最后一次有效缩圈 t=376 —— 只要在 3x3 内就能活到底，
//   中心 (9,9) 并不比 (8,10) 值钱；单调奖励会让 bot 从 t=351 就往中心狂奔 8 格送去被 4 个对手夹击。
//   而到了 t>=376（下一层缩圈要 edgeD>=9，只剩中心一格），阈值模型会自动变成"抢最中心格"。
#ifndef CROWD_KEEP
#define CROWD_KEEP 1
#endif
#ifndef CROWD_KEEP_PULL
#define CROWD_KEEP_PULL 30000.0
#endif
#ifndef CROWD_KEEP_W
#define CROWD_KEEP_W 400000.0
#endif
#ifndef GEN_GATE
#define GEN_GATE 0
#endif
#ifndef GEN_GATE_RIVALS
#define GEN_GATE_RIVALS 2
#endif
// 门格必须"比口袋死得晚"至少这么多回合 —— 封完还得有命撤回来。
// 实测：不加这条时击杀分翻 2.5 倍（0.100→0.250）但平均死亡回合从 371 掉到 333，
// 净亏 —— 因为封人的位置本身也是外层，封完来不及往里走。
#ifndef CROWD_GATE_MARGIN
#define CROWD_GATE_MARGIN EVERY
#endif
#ifndef LOCK_HOLD_PESS
#define LOCK_HOLD_PESS 1
#endif
#ifndef NO_SWAP
#define NO_SWAP 1
#endif
#ifndef NO_CHAIN
#define NO_CHAIN 1
#endif
#ifndef BOMB_EARLY
#define BOMB_EARLY 1
#endif

int lastActD = -1;
int blockedCd = 0;

inline int C(int x, int y) { return y * N + x; }
inline int edgeD(int x, int y) { return min(min(x, y), min(N - 1 - x, N - 1 - y)); }

// ===== v32：全人数统一的"最终存活格"几何与窗位（全部由配置推导，无人数分支） =====
int LKv = 0, DEATHTv = 0;            // LKv=最终存活层；DEATHTv=全图变墙拍（SHRINK+(LK-1)*EVERY）
vector<int> finalCells;              // 最终存活格集合（地板中心=单格；墙中心=3×3 环 8 格）
inline bool isFinal(int c) { return edgeD(c % N, c / N) >= LKv; }

struct Pred {
    // v26：扁平化为一维（H 段、每段 N*N；下标 k*N*N+c）。
    // 原实现每层一个 vector<unsigned char>，一个 Pred 要 3*H 次小分配 —— 全部省掉。
    vector<unsigned char> open, occ, safe;
};

Pred predict(const vector<Bm>& bombsIn, int H, const vector<Fl>& fl) {
    Pred o;
    const int NN = N * N;
    o.open.assign(H * NN, 0);
    o.occ.assign(H * NN, 0);
    o.safe.assign(H * NN, 0);
    // v26：函数内全部临时缓冲静态复用（原来每调用/每步都在分配），结果不变。
    static vector<string> grid; grid = board;
    static vector<int> exp; exp.assign(NN, -1);
    for (auto& f : fl) exp[C(f.x, f.y)] = f.end;
    static vector<int> at, atTouched;
    if ((int)at.size() != NN) { at.assign(NN, -1); atTouched.clear(); }
    static vector<char> hit;
    static vector<int> hitTouched;
    if ((int)hit.size() != NN) { hit.assign(NN, 0); hitTouched.clear(); }
    static vector<char> gone;
    static vector<int> q;
    static vector<Bm> bs, nbtmp;
    bs.assign(bombsIn.begin(), bombsIn.end());
    for (int k = 0; k < H; k++) {
        int now = T + k;
        if (now >= SHRINK && (now - SHRINK) % EVERY == 0) {
            int layer = 1 + (now - SHRINK) / EVERY;
            for (int y = 0; y < N; y++)
                for (int x = 0; x < N; x++)
                    if (edgeD(x, y) <= layer) { grid[y][x] = '#'; exp[C(x, y)] = -1; }
            nbtmp.clear();
            for (auto& b : bs) if (grid[b.y][b.x] != '#') nbtmp.push_back(b);
            bs.swap(nbtmp);
        }
        // at / hit 用触碰表复位（取代每步两个全盘填充）
        for (int c : atTouched) at[c] = -1;
        atTouched.clear();
        for (int c : hitTouched) hit[c] = 0;
        hitTouched.clear();
        for (int i = 0; i < (int)bs.size(); i++) {
            if (bs[i].from > now) continue;
            int c = C(bs[i].x, bs[i].y);
            if (at[c] < 0) atTouched.push_back(c);
            at[c] = i; o.occ[k * NN + c] = 1;
        }
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) o.open[k * NN + C(x, y)] = (grid[y][x] == '.');
        q.clear();
        size_t qh = 0;
        gone.assign(bs.size(), 0);
        for (int i = 0; i < (int)bs.size(); i++)
            if (bs[i].from <= now && (bs[i].at <= now || exp[C(bs[i].x, bs[i].y)] >= now)) q.push_back(i);
        while (qh < q.size()) {
            int i = q[qh++];
            if (gone[i]) continue; gone[i] = 1;
            Bm b = bs[i];
            int c0 = C(b.x, b.y);
            if (!hit[c0]) hitTouched.push_back(c0);
            hit[c0] = 1;
            for (int d = 0; d < 4; d++)
                for (int r = 1; r <= RANGE; r++) {
                    int x = b.x + DX[d] * r, y = b.y + DY[d] * r;
                    if (x < 0 || y < 0 || x >= N || y >= N || grid[y][x] == '#') break;
                    int c = C(x, y);
                    if (!hit[c]) hitTouched.push_back(c);
                    hit[c] = 1;
                    if (grid[y][x] == '+') break;
                    if (at[c] >= 0) { q.push_back(at[c]); break; }
                }
        }
        nbtmp.clear();
        for (int i = 0; i < (int)bs.size(); i++) if (!gone[i]) nbtmp.push_back(bs[i]);
        bs.swap(nbtmp);
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                int c = C(x, y);
                if (hit[c]) { exp[c] = max(exp[c], now + FIRE - 1); if (grid[y][x] == '+') grid[y][x] = '.'; }
                o.safe[k * NN + c] = (grid[y][x] == '.' && exp[c] < now);
            }
    }
    return o;
}

struct Reach {
    array<int, 5> life{}, count{};
    array<double, 5> util{};
    int unique = 0;
    int minReach = 1 << 20;
};

#ifdef REACH_CHECK
static vector<int> g_set_s, g_set_b, g_pp_s, g_pp_b;
static vector<vector<int>> g_tr_s, g_tr_b;
#endif
#ifdef REACH_CHECK
Reach reach_bits(const Pred& pr, int who, int forced = -1, const vector<double>* pot = nullptr,
            const vector<char>* blocked = nullptr) {
    // v26 加速版（结果与原实现逐位一致）：
    //   ① 每步只遍历"活跃格列表"（原版每步两次全盘扫 N*N，且每调用 2 次分配）；
    //   ② 全部缓冲静态复用，零分配。
    const int NN = N * N;
    int H = (int)pr.safe.size() / NN;   // v26：safe 已扁平化（H 段 × N*N），这里除回步数
    static vector<unsigned char> sbuf1, sbuf2;
    static vector<int> slist1, slist2;
    if ((int)sbuf1.size() != NN) {
        sbuf1.assign(NN, 0); sbuf2.assign(NN, 0);
        slist1.clear(); slist2.clear();
    }
    vector<unsigned char>* cur = &sbuf1;
    vector<unsigned char>* nxt = &sbuf2;
    vector<int>* cl = &slist1;
    vector<int>* nl = &slist2;
    Reach R; R.util.fill(-1e18);
    int src = C(players[who].x, players[who].y);
#ifdef REACH_CHECK
    g_tr_b.clear(); g_pp_b.clear();
#endif
    (*cur)[src] = 31;
    cl->push_back(src);
    for (int k = 0; k < H; k++) {
        for (int c : *cl) if ((*cur)[c] && !pr.open[k * NN + c]) (*cur)[c] = 0;
#ifdef REACH_CHECK
        if (T == 0 && who == 1 && k == 4) for (int c : *cl) if ((*cur)[c]) g_pp_b.push_back(c);
#endif
        for (int c : *cl) {
            unsigned char bits = (*cur)[c];
            if (!bits) continue;
            int x = c % N, y = c / N;
            for (int d = 0; d < 5; d++) {
                int nx = x + DX[d], ny = y + DY[d], nc = c;
                if (nx >= 0 && ny >= 0 && nx < N && ny < N) {
                    int z = C(nx, ny);
                    if (pr.open[k * NN + z] && (!pr.occ[k * NN + z] || z == c) && !(blocked && (*blocked)[z] && z != c))
                        nc = z;
                }
                if (k == 0 && forced >= 0) nc = forced;
                if (pr.safe[k * NN + nc]) {
                    if (!(*nxt)[nc]) nl->push_back(nc);
                    (*nxt)[nc] |= (k == 0 ? (1 << d) : bits);
                }
            }
        }
#ifdef REACH_CHECK
        g_tr_b.push_back(*nl);
#endif
        int live = 0;
        for (int c : *nl) {
            live++;
            unsigned char bits = (*nxt)[c];
            for (int d = 0; d < 5; d++) if (bits & (1 << d)) R.life[d] = k + 1;
        }
        if (k >= 1 && k <= MR_WIN && live < R.minReach) R.minReach = live;
        for (int c : *cl) (*cur)[c] = 0;
        cl->clear();
        swap(cur, nxt); swap(cl, nl);
    }
    for (int c : *cl) {
        R.unique++;
        unsigned char bits = (*cur)[c];
        for (int d = 0; d < 5; d++)
            if (bits & (1 << d)) { R.count[d]++; if (pot) R.util[d] = max(R.util[d], (*pot)[c]); }
    }
#ifdef REACH_CHECK
    g_set_b = *cl;
#endif
    for (int c : *cl) (*cur)[c] = 0;
    cl->clear();
    return R;
}
#endif

Reach reach_scalar(const Pred& pr, int who, int forced = -1, const vector<double>* pot = nullptr,
            const vector<char>* blocked = nullptr) {
    // v26 加速版：位无关的"集合推进"引擎（与逐位版等价，由 REACH_CHECK 对拍验证）。
    //   ① forced 调用：k=0 时五个方向位全部落在 forced 格，之后整体合并 —— 逐位 life/count 恒相等；
    //   ② 非 forced 调用：全量核对过读取口径，只有 .unique 与 max(life)（集合级量）。
    //   实现：单一集合 + 时间戳标记（无清盘）、kill(!open) 并入推进循环（消灭两次全盘扫）。
    const int NN = N * N;
    int H = (int)pr.safe.size() / NN;
    Reach R; R.util.fill(-1e18);
    if (H <= 0) {   // 与逐位版在 H<=0 时的取值一致（正常路径不会出现）
        R.unique = 1;
        for (int d = 0; d < 5; d++) R.count[d] = 1;
        return R;
    }
    // 两个成员数组（cur / nxt，逐拍交换）：本拍写入的数组不影响本拍的成员读取 ——
    // 与逐位版的 cur/nxt 完全同构（否则"父格在同一步被写中"会被误判出局）。
    static vector<int> sA, sB;
    static vector<int> list1, list2;
    if ((int)sA.size() != NN) { sA.assign(NN, 0); sB.assign(NN, 0); list1.clear(); list2.clear(); }
    static int gen = 0;
    if (gen > (1 << 30)) { fill(sA.begin(), sA.end(), 0); fill(sB.begin(), sB.end(), 0); gen = 0; }
    gen += H + 1;
    int g0 = gen - H;
    vector<int>* curS = &sA;
    vector<int>* nxtS = &sB;
    vector<int>* cl = &list1;
    vector<int>* nl = &list2;
    cl->clear(); nl->clear();
    int src = C(players[who].x, players[who].y);
    int lastNon = -1;
#ifdef REACH_CHECK
    g_tr_s.clear(); g_pp_s.clear();
#endif
    // ---- k = 0 ----
    if (pr.open[src]) {
        if (forced >= 0) {
            if (pr.safe[forced]) { (*nxtS)[forced] = g0; cl->push_back(forced); }
        } else {
            int x = src % N, y = src / N;
            for (int d = 0; d < 5; d++) {
                int nx = x + DX[d], ny = y + DY[d], nc = src;
                if (nx >= 0 && ny >= 0 && nx < N && ny < N) {
                    int z = C(nx, ny);
                    if (pr.open[z] && (!pr.occ[z] || z == src) && !(blocked && (*blocked)[z] && z != src))
                        nc = z;
                }
                if (pr.safe[nc] && (*nxtS)[nc] != g0) { (*nxtS)[nc] = g0; cl->push_back(nc); }
            }
        }
    }
    if (!cl->empty()) lastNon = 0;
#ifdef REACH_CHECK
    g_tr_s.push_back(*cl);
#endif
    // ---- k = 1 .. H-1 ----
    for (int k = 1; k < H; k++) {
        swap(curS, nxtS);   // curS = 上一拍写入的数组（成员以 gk-1 为戳）
        int gk = g0 + k;
        nl->clear();
        for (int c : *cl) {
            if ((*curS)[c] != gk - 1) continue;
            if (!pr.open[k * NN + c]) continue;
#ifdef REACH_CHECK
            if (T == 0 && who == 1 && k == 4) g_pp_s.push_back(c);
#endif
            int x = c % N, y = c / N;
            for (int d = 0; d < 5; d++) {
                int nx = x + DX[d], ny = y + DY[d], nc = c;
                if (nx >= 0 && ny >= 0 && nx < N && ny < N) {
                    int z = C(nx, ny);
                    if (pr.open[k * NN + z] && (!pr.occ[k * NN + z] || z == c) && !(blocked && (*blocked)[z] && z != c))
                        nc = z;
                }
                if (pr.safe[k * NN + nc] && (*nxtS)[nc] != gk) { (*nxtS)[nc] = gk; nl->push_back(nc); }
            }
        }
        if (!nl->empty()) lastNon = k;
        if ((int)nl->size() < R.minReach) R.minReach = (int)nl->size();
#ifdef REACH_CHECK
        g_tr_s.push_back(*nl);
#endif
        swap(cl, nl);
    }
    int finalN = (int)cl->size();
    R.unique = finalN;
    if (lastNon >= 0)
        for (int d = 0; d < 5; d++) { R.life[d] = lastNon + 1; R.count[d] = finalN; }
    if (pot && finalN > 0) {
        double mx = -1e18;
        for (int c : *cl) mx = max(mx, (*pot)[c]);
        for (int d = 0; d < 5; d++) R.util[d] = mx;
    }
#ifdef REACH_CHECK
    g_set_s = *cl;
#endif
    cl->clear(); nl->clear();
    return R;
}

// ===== v33p：批量传播内核 =====
// 依据：reach 的推进算子对集合可加 —— M(A∪B) = M(A) ∪ M(B)。
// 每个格子只存"哪些查询源能在此"的位标签，一次推进即得全部查询的 life/unique/minReach，
// 逐位等价于对每个源各调一次 reach_scalar（由 -DMULTI_CHECK 全量对拍）。
struct MultiRes {
    vector<int> life, unique, minReach;
};

// 与 reach_scalar 内联的移动判定逐字一致（open / occ 只允许原地 / blocked 只挡移动）
static inline int mvCell(const Pred& pr, int k, int c, int d, const vector<char>* blocked, int NN) {
    int x = c % N, y = c / N;
    int nx = x + DX[d], ny = y + DY[d], nc = c;
    if (nx >= 0 && ny >= 0 && nx < N && ny < N) {
        int z = ny * N + nx;
        if (pr.open[k * NN + z] && (!pr.occ[k * NN + z] || z == c) &&
            !(blocked && (*blocked)[z] && z != c)) nc = z;
    }
    return nc;
}

// srcCells/whoVec 长度 = Q；forced 非空时表示"Q 个落点的 forced 查询"（源固定 srcCells[0]）
static void reach_multi_core(const Pred& pr, const vector<int>& srcCells, const vector<int>& whoVec,
                             const vector<int>* forced, const vector<char>* blocked,
                             bool wantMinReach, MultiRes& out) {
    const int NN = N * N;
    const int Q = forced ? (int)forced->size() : (int)srcCells.size();
    out.life.assign(Q, 0);
    out.unique.assign(Q, 0);
    out.minReach.assign(Q, wantMinReach ? (1 << 20) : 0);
    if (Q <= 0) return;
    const int H = (int)pr.safe.size() / NN;
    if (H <= 0) {
        for (int q = 0; q < Q; q++) { out.unique[q] = 1; if (wantMinReach) out.minReach[q] = 1; }
        return;
    }
    static vector<unsigned int> la, lb;
    static vector<int> lc, ld;
    if ((int)la.size() != NN) { la.assign(NN, 0); lb.assign(NN, 0); lc.clear(); ld.clear(); }
    fill(la.begin(), la.end(), 0u);
    fill(lb.begin(), lb.end(), 0u);
    lc.clear(); ld.clear();

    // ---- k=0：复刻 reach_scalar 的首拍分支 ----
    //   非 forced：先 open[src]，再逐方向 open/occ/blocked，最后 safe[nc]
    //   forced  ：先 open[src]，再看 safe[forced]（不看 open/occ/blocked —— 与原实现一致）
    for (int q = 0; q < Q; q++) {
        int src = forced ? srcCells[0] : srcCells[q];
        unsigned int bit = 1u << q;
        if (!pr.open[src]) continue;
        if (forced) {
            int f = (*forced)[q];
            if (pr.safe[f]) { if (!la[f]) lc.push_back(f); la[f] |= bit; }
        } else {
            for (int d = 0; d < 5; d++) {
                int nc = mvCell(pr, 0, src, d, blocked, NN);
                if (pr.safe[nc]) { if (!la[nc]) lc.push_back(nc); la[nc] |= bit; }
            }
        }
    }
    vector<int> lastQ(Q, -1);
    {
        unsigned int orAll = 0;
        for (int c : lc) orAll |= la[c];
        for (int q = 0; q < Q; q++) if (orAll & (1u << q)) lastQ[q] = 0;
    }
    vector<int> cntQ(Q, 0);
    // ---- k = 1 .. H-1 ----
    for (int k = 1; k < H && !lc.empty(); k++) {
        ld.clear();
        for (int c : lc) {
            unsigned int m = la[c];
            if (!m) continue;
            if (!pr.open[k * NN + c]) { la[c] = 0; continue; }   // 本拍该格变墙 → 死
            for (int d = 0; d < 5; d++) {
                int nc = mvCell(pr, k, c, d, blocked, NN);
                if (pr.safe[k * NN + nc]) {
                    if (!lb[nc]) ld.push_back(nc);
                    lb[nc] |= m;
                }
            }
        }
        for (int c : lc) la[c] = 0;
        lc.clear();
        la.swap(lb);
        lc.swap(ld);
        unsigned int orAll = 0;
        for (int c : lc) orAll |= la[c];
        for (int q = 0; q < Q; q++) if (orAll & (1u << q)) lastQ[q] = k;
        if (wantMinReach) {
            fill(cntQ.begin(), cntQ.end(), 0);
            for (int c : lc) {
                unsigned int m = la[c];
                for (int q = 0; q < Q; q++) if (m & (1u << q)) cntQ[q]++;
            }
            for (int q = 0; q < Q; q++) if (cntQ[q] < out.minReach[q]) out.minReach[q] = cntQ[q];
        }
    }
    // 集合已在某一拍死光 → 后续各拍的 |S| 都是 0（与原实现 minReach 取到 0 一致）
    if (lc.empty() && wantMinReach)
        for (int q = 0; q < Q; q++) out.minReach[q] = 0;
    for (int c : lc) {
        unsigned int m = la[c];
        for (int q = 0; q < Q; q++) if (m & (1u << q)) out.unique[q]++;
    }
    for (int q = 0; q < Q; q++) if (lastQ[q] >= 0) out.life[q] = lastQ[q] + 1;
    for (int c : lc) la[c] = 0;
    lc.clear();
#ifdef MULTI_CHECK
    for (int q = 0; q < Q; q++) {
        Reach r = reach_scalar(pr, whoVec[q], forced ? (*forced)[q] : -1, nullptr, blocked);
        int mx = 0;
        for (int d = 0; d < 5; d++) mx = max(mx, r.life[d]);
        bool ok = (r.unique == out.unique[q]) && (mx == out.life[q]) &&
                  (!wantMinReach || r.minReach == out.minReach[q]);
        if (!ok) {
            fprintf(stderr,
                    "MULTI MISMATCH T=%d q=%d who=%d forced=%d | multi uniq=%d life=%d mr=%d | scalar uniq=%d life=%d mr=%d\n",
                    T, q, whoVec[q], forced ? (*forced)[q] : -1, out.unique[q], out.life[q],
                    wantMinReach ? out.minReach[q] : -1, r.unique, mx, r.minReach);
            abort();
        }
    }
#endif
}

Reach reach(const Pred& pr, int who, int forced = -1, const vector<double>* pot = nullptr,
            const vector<char>* blocked = nullptr) {
#ifdef REACH_CHECK
    Reach a = reach_scalar(pr, who, forced, pot, blocked);
    Reach b = reach_bits(pr, who, forced, pot, blocked);
    bool eq = (a.unique == b.unique) && (a.minReach == b.minReach);
    int mx = 0;
    for (int d = 0; d < 5; d++) mx = max(mx, b.life[d]);
    if (forced >= 0) {
        for (int d = 0; d < 5; d++) eq = eq && a.life[d] == b.life[d] && a.count[d] == b.count[d];
    } else {
        eq = eq && a.life[0] == mx;
    }
    if (!eq) {
        fprintf(stderr, "REACH MISMATCH T=%d who=%d forced=%d\n  scalar life=%d,%d,%d,%d,%d cnt=%d,%d,%d,%d,%d uniq=%d mr=%d\n  bits   life=%d,%d,%d,%d,%d cnt=%d,%d,%d,%d,%d uniq=%d mr=%d\n",
                T, who, forced,
                a.life[0], a.life[1], a.life[2], a.life[3], a.life[4],
                a.count[0], a.count[1], a.count[2], a.count[3], a.count[4], a.unique, a.minReach,
                b.life[0], b.life[1], b.life[2], b.life[3], b.life[4],
                b.count[0], b.count[1], b.count[2], b.count[3], b.count[4], b.unique, b.minReach);
        int steps = (int)min(g_tr_s.size(), g_tr_b.size());
        for (int q = 0; q < steps && q <= 6; q++) {
            vector<int> x = g_tr_s[q], y = g_tr_b[q];
            sort(x.begin(), x.end()); sort(y.begin(), y.end());
            fprintf(stderr, "  q=%d :", q);
            for (int c : x) fprintf(stderr, " (%d,%d)", c % N, c / N);
            fprintf(stderr, "\n      b:");
            for (int c : y) fprintf(stderr, " (%d,%d)", c % N, c / N);
            fprintf(stderr, "\n");
        }
        {
            vector<int> x = g_pp_s, y = g_pp_b;
            sort(x.begin(), x.end()); sort(y.begin(), y.end());
            fprintf(stderr, "  processed@k4 s(%d):", (int)x.size());
            for (int c : x) fprintf(stderr, " (%d,%d)", c % N, c / N);
            fprintf(stderr, "\n  processed@k4 b(%d):", (int)y.size());
            for (int c : y) fprintf(stderr, " (%d,%d)", c % N, c / N);
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "  nsteps s=%d b=%d set_s=%d set_b=%d\n", (int)g_tr_s.size(), (int)g_tr_b.size(),
                (int)g_set_s.size(), (int)g_set_b.size());
        abort();
    }
    return a;
#else
    return reach_scalar(pr, who, forced, pot, blocked);
#endif
}

vector<double> pot;

void makePot() {
    pot.assign(N * N, -1e18);
    priority_queue<pair<double, int>> pq;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            if (board[y][x] != '.') continue;
            int e = edgeD(x, y);
            double v = 2.2 * e;
            int op = 0;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (nx >= 0 && ny >= 0 && nx < N && ny < N && board[ny][nx] != '#') op++;
            }
            v += 0.9 * op;
            pot[C(x, y)] = v;
            pq.push({v, C(x, y)});
        }
    while (!pq.empty()) {
        auto [v, c] = pq.top(); pq.pop();
        if (v < pot[c] - 1e-9) continue;
        int x = c % N, y = c / N;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            if (board[ny][nx] != '.') continue;
            int z = C(nx, ny);
            if (v - 1.0 > pot[z]) { pot[z] = v - 1.0; pq.push({pot[z], z}); }
        }
    }
}

int blastStatic(int x, int y) {
    int sc = 0;
    for (int d = 0; d < 4; d++)
        for (int r = 1; r <= RANGE; r++) {
            int nx = x + DX[d] * r, ny = y + DY[d] * r;
            if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] == '#') break;
            if (board[ny][nx] == '+') { sc += 3; break; }
            for (int i = 0; i < P; i++)
                if (i != ME && players[i].alive && players[i].x == nx && players[i].y == ny) sc += 4;
        }
    return sc;
}

#ifndef PIN_W
#define PIN_W 250000.0
#endif
#ifndef PIN_NEAR_W
#define PIN_NEAR_W 40000.0
#endif
#ifndef PINBOMB_W
#define PINBOMB_W 250000.0
#endif
#ifndef DUEL_GATE_PEN
#define DUEL_GATE_PEN 0.0
#endif
#ifndef DUEL_GATE_LEAD
#define DUEL_GATE_LEAD 5
#endif

inline bool freeCell(int x, int y) {
    if (x < 0 || y < 0 || x >= N || y >= N) return false;
    if (board[y][x] != '.') return false;
    for (auto& b : bombs) if (b.x == x && b.y == y) return false;
    return true;
}
inline bool openCell(int x, int y) {
    return x >= 0 && y >= 0 && x < N && y < N && board[y][x] == '.';
}

// v25f：格子的"出口数"（地图固有属性）—— 4 邻格里可通行的个数。
// 出口越少越容易被堵死/被缩圈困住；<=1 就是叶子格。
inline int openNbrsOf(int c) {
    int x = c % N, y = c / N, k = 0;
    for (int d = 0; d < 4; d++) if (openCell(x + DX[d], y + DY[d])) k++;
    return k;
}

// v16：上下左右四个邻格都不是永久墙 = "上下左右都能走的点"。
// 13x13 上偶数 x/y 全是永久墙，所以这些点正好是 x,y ∈ {3,5,7,9} 那 16 格。
inline bool isCross(int x, int y) {
    if (x < 1 || y < 1 || x >= N - 1 || y >= N - 1) return false;
    if (board[y][x] == '#') return false;
    for (int d = 0; d < 4; d++) if (board[y + DY[d]][x + DX[d]] == '#') return false;
    return true;
}

// v17：4 邻格里"是永久墙"的个数。>=3 就是"三面都是墙"——只剩一个出口的格子。
inline int wallNbrs(int x, int y) {
    int k = 0;
    for (int d = 0; d < 4; d++) {
        int nx = x + DX[d], ny = y + DY[d];
        if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] == '#') k++;
    }
    return k;
}

inline bool isVert(int x, int y) {
    if (x < 0 || y < 0 || x >= N || y >= N) return false;
    if (board[y][x] != '.') return false;
    for (int dx = -1; dx <= 1; dx += 2)
        for (int dy = -1; dy <= 1; dy += 2) {
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && ny >= 0 && nx < N && ny < N && board[ny][nx] != '#') return false;
        }
    return true;
}

int pocketExit(int c) {
    int x = c % N, y = c / N, only = -1, cnt = 0;
    for (int d = 0; d < 4; d++)
        if (openCell(x + DX[d], y + DY[d])) { cnt++; only = C(x + DX[d], y + DY[d]); }
    return cnt == 1 ? only : -1;
}

int rivalPocketExit() {
    // v32：统一 —— 目标 = 最外围（离边界最近）的对手；并列取离我最近（2p 时=唯一对手）
    int oc = -1;
    {
        int bE = 1 << 20, bD = 1 << 20;
        for (int i = 0; i < P; i++) {
            if (i == ME || !players[i].alive) continue;
            int e2 = edgeD(players[i].x, players[i].y);
            int dd2 = abs(players[i].x - players[ME].x) + abs(players[i].y - players[ME].y);
            if (e2 < bE || (e2 == bE && dd2 < bD)) { bE = e2; bD = dd2; oc = C(players[i].x, players[i].y); }
        }
    }
    if (oc < 0) return -1;
    int pe = pocketExit(oc);
    if (pe >= 0) return pe;
    int nt = T + 1;
    if (nt < SHRINK || (nt - SHRINK) % EVERY != 0) return -1;
    int nl = 1 + (nt - SHRINK) / EVERY;
    int fx = oc % N, fy = oc / N;
    vector<int> opts;
    if (edgeD(fx, fy) > nl) opts.push_back(oc);
    for (int d = 0; d < 4; d++) {
        int nx = fx + DX[d], ny = fy + DY[d];
        if (freeCell(nx, ny) && edgeD(nx, ny) > nl) opts.push_back(C(nx, ny));
    }
    if (opts.size() != 1) return -1;
    int f = opts[0], x2 = f % N, y2 = f / N, cnt = 0, only = -1;
    for (int d = 0; d < 4; d++) {
        int nx = x2 + DX[d], ny = y2 + DY[d];
        if (openCell(nx, ny) && edgeD(nx, ny) > nl) { cnt++; only = C(nx, ny); }
    }
    return cnt == 1 ? only : -1;
}

int prevX[16], prevY[16];

#ifndef COLL_MODE
#define COLL_MODE 1
#endif

#ifndef DUEL_COLL
#define DUEL_COLL 1
#endif
int COLL_RT = COLL_MODE;

vector<int> bfs(const vector<string>& g, int src) {
    vector<int> dist(N * N, 1e9);
    if (src < 0 || g[src / N][src % N] != '.') return dist;
    dist[src] = 0;
    deque<int> q; q.push_back(src);
    while (!q.empty()) {
        int c = q.front(); q.pop_front();
        int x = c % N, y = c / N;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= N || ny >= N || g[ny][nx] != '.') continue;
            int z = C(nx, ny);
            if (dist[z] > dist[c] + 1) { dist[z] = dist[c] + 1; q.push_back(z); }
        }
    }
    return dist;
}

vector<int> recent;
double visits(int c) {
    double s = 0;
    for (int k = 0; k < (int)recent.size(); k++)
        if (recent[k] == c) s += 1.0 / (1 + 0.15 * (recent.size() - 1 - k));
    return s;
}

int terrainDest(const Pred& pr, int myDir) {
    int old = C(players[ME].x, players[ME].y);
    int tx = players[ME].x + DX[myDir], ty = players[ME].y + DY[myDir];
    if (tx >= 0 && ty >= 0 && tx < N && ty < N && pr.open[C(tx, ty)] &&
        (!pr.occ[C(tx, ty)] || old == C(tx, ty))) return C(tx, ty);
    return old;
}

int predictedDest(const Pred& pr, int myDir) {
    int old = C(players[ME].x, players[ME].y);
    int tx = players[ME].x + DX[myDir], ty = players[ME].y + DY[myDir], plain = old;
    if (tx >= 0 && ty >= 0 && tx < N && ty < N && pr.open[C(tx, ty)] &&
        (!pr.occ[C(tx, ty)] || old == C(tx, ty))) plain = C(tx, ty);
    if (COLL_RT == 0) return plain;
    if (COLL_RT == 1) {
        if (plain != old)
            for (int i = 0; i < P; i++)
                if (i != ME && players[i].alive && C(players[i].x, players[i].y) == plain) return old;
        return plain;
    }
    vector<int> ids, orig, target;
    for (int i = 0; i < P; i++) {
        if (!players[i].alive) continue;
        int o = C(players[i].x, players[i].y);
        if (!pr.open[o]) continue;
        int d = 4;
        if (i == ME) d = myDir;
        else if (prevX[i] >= 0 && abs(players[i].x - prevX[i]) + abs(players[i].y - prevY[i]) == 1) {
            if (players[i].y < prevY[i]) d = 0;
            else if (players[i].y > prevY[i]) d = 1;
            else if (players[i].x < prevX[i]) d = 2;
            else d = 3;
        }
        int x = players[i].x + DX[d], y = players[i].y + DY[d], dest = o;
        if (x >= 0 && y >= 0 && x < N && y < N && pr.open[C(x, y)] &&
            (!pr.occ[C(x, y)] || o == C(x, y))) dest = C(x, y);
        ids.push_back(i); orig.push_back(o); target.push_back(dest);
    }
    vector<int> res = target;
    for (size_t i = 0; i < target.size(); i++) {
        int cnt = 0;
        for (size_t j = 0; j < target.size(); j++) if (target[j] == target[i]) cnt++;
        if (cnt > 1) res[i] = orig[i];
    }
    while (true) {
        vector<char> stopped(N * N, 0);
        for (size_t i = 0; i < orig.size(); i++) if (res[i] == orig[i]) stopped[orig[i]] = 1;
        vector<int> upd = res;
        for (size_t i = 0; i < res.size(); i++) if (stopped[res[i]]) upd[i] = orig[i];
        if (upd == res) break;
        res = upd;
    }
    for (size_t i = 0; i < ids.size(); i++) if (ids[i] == ME) return res[i];
    return old;
}

bool DBG = false;

#ifndef POT_W
#define POT_W 25.0
#endif
#ifndef FL_W
#define FL_W 200.0
#endif
#ifndef MR_W
#define MR_W 200.0
#endif
#ifndef DUEL_MR_W
#define DUEL_MR_W 0.0
#endif
#ifndef DUEL_CONTEST
#define DUEL_CONTEST 0
#endif
#ifndef PESS_W
#define PESS_W 2200.0
#endif
#ifndef DUEL_PESS_W
#define DUEL_PESS_W 2200.0
#endif
#ifndef CNT_W
#define CNT_W 25.0
#endif
#ifndef DUEL_MULT
#define DUEL_MULT 1.0   // v32：统一（原 2p=0）
#endif
#ifndef DUEL_FL_MULT
#define DUEL_FL_MULT 1.0   // v32：统一（原 2p=0）
#endif
#ifndef CROWD_W
#define CROWD_W 2.0
#endif
#ifndef DUEL_CROWD_W
#define DUEL_CROWD_W 0.0
#endif
#ifndef DUEL_WIN_W
#define DUEL_WIN_W 0.0
#endif
#ifndef DUEL_NEAR_W
#define DUEL_NEAR_W 0.0
#endif
#ifndef DUEL_BLOCK_W
#define DUEL_BLOCK_W 300.0
#endif
#ifndef DUEL_VIS_W
#define DUEL_VIS_W 6.0
#endif
#ifndef KILL_BONUS
#define KILL_BONUS 6000.0
#endif
#ifndef DUEL_KILL_BONUS
#define DUEL_KILL_BONUS 6000.0
#endif
#ifndef DUEL_ESC_W
#define DUEL_ESC_W 30.0
#endif
#ifndef ESC_W
#define ESC_W 30.0
#endif
#ifndef CHAIN_W
#define CHAIN_W 120.0
#endif
#ifndef DUEL_CHAIN_W
#define DUEL_CHAIN_W 120.0
#endif
#ifndef HORIZON
#define HORIZON 20
#endif
#ifndef DUEL_HORIZON
#define DUEL_HORIZON 64
#endif
#ifndef UNI_H
#define UNI_H 20   // v32：全人数统一预测视野（50ms 预算：N=25 时 H=64→~93ms 超时，H=20→~11ms）
#endif
#ifndef CLOSE_THREAT
#define CLOSE_THREAT 1
#endif
#ifndef OFF_DUEL_MULT
#define OFF_DUEL_MULT 1.0
#endif
#ifndef DEEP_THREAT
#define DEEP_THREAT 1
#endif
#ifndef DEEP_N
#define DEEP_N 3
#endif
#ifndef MOB_FLOOR
#define MOB_FLOOR 3
#endif
#ifndef MOB_PEN
#define MOB_PEN 700.0
#endif
#ifndef DEEP_MIN_ALIVE
#define DEEP_MIN_ALIVE 7
#endif
#ifndef DUEL_DEEP_MIN_ALIVE
#define DUEL_DEEP_MIN_ALIVE 2
#endif
#ifndef DUEL_DEEP_N
#define DUEL_DEEP_N 6
#endif
#ifndef DUEL_DEEP_STEPS
#define DUEL_DEEP_STEPS 2
#endif
#ifndef DUEL_OPP_MODEL
#define DUEL_OPP_MODEL 0
#endif
#ifndef DUEL_OPP_STEPS
#define DUEL_OPP_STEPS 3
#endif
#ifndef DUEL_OPP_KEEP
#define DUEL_OPP_KEEP 6
#endif
#ifndef DUEL_OPP_NEAR
#define DUEL_OPP_NEAR 0
#endif
#ifndef DUEL_OPP_EXP_W
#define DUEL_OPP_EXP_W 0.0
#endif
#ifndef DUEL_OPP_EXP_STEPS
#define DUEL_OPP_EXP_STEPS 3
#endif
#ifndef DUEL_DEEP_W
#define DUEL_DEEP_W 1400.0
#endif
#ifndef DUEL_NO_OCC_SITE
#define DUEL_NO_OCC_SITE 0
#endif
#ifndef DUEL_RIVAL_W
#define DUEL_RIVAL_W 0.0
#endif
#ifndef DEEP_W
#define DEEP_W 600.0
#endif
#ifndef STAY_PEN
#define STAY_PEN 800.0
// v31dev：末段"持中"修复开关（复盘 idx1 型让中：STAY_PEN 薄差推走中心决策）
#ifndef HOLD_CTR_EXEMPT
#define HOLD_CTR_EXEMPT 1
#endif
#ifndef HOLD_CTR_T
#define HOLD_CTR_T 330
#endif
#ifndef HOLD_CTR_BONUS
#define HOLD_CTR_BONUS 0.0
#endif
// v31r2 开关
#ifndef ENDGAME_NOPLACE
#define ENDGAME_NOPLACE 1
#endif
#ifndef ENDGAME_NOPLACE_T
#define ENDGAME_NOPLACE_T 360
#endif
#ifndef ENDGAME_NOPLACE_R
#define ENDGAME_NOPLACE_R 3
#endif
#ifndef CLAIM_CTR
#define CLAIM_CTR 1
#endif
#ifndef CLAIM_CTR_T
#define CLAIM_CTR_T 345
#endif
#ifndef CLAIM_CTR_FL
#define CLAIM_CTR_FL 4
#endif
#ifndef CLAIM_CTR_MYFL
#define CLAIM_CTR_MYFL 2
#endif
#ifndef CLAIM_CTR_OCC
#define CLAIM_CTR_OCC 1
#endif
// ===== v31r5 开关：终局中心堡垒锁 + 中心自雷禁令 =====
// FORTRESS_LOCK：T>=FORTRESS_T 且我站在终局中心格(9,9)时，只要"留下"不损失生存
//   （留下生命 + FORTRESS_SLACK >= 任何离开候选的生命），就强制原地不动。
//   目的：防止 crowd/vis/pot 等软评分把我推离堡垒（离开=把中心送给对手=输）。
#ifndef FORTRESS_LOCK
#define FORTRESS_LOCK 1
#endif
#ifndef FORTRESS_T
#define FORTRESS_T 340
#endif
#ifndef FORTRESS_SLACK
#define FORTRESS_SLACK 0
#endif
// CTR_SELF_BOMB_BAN：T>=CSB_T 时，站在中心格绝不放雷
//   （自雷=自我驱逐 + 腾空中心 → 被对手接手锁死/互锁全灭）。
#ifndef CTR_SELF_BOMB_BAN
#define CTR_SELF_BOMB_BAN 1
#endif
#ifndef CSB_T
#define CSB_T 340
#endif
// ===== v31r6 开关：濒死白名单（死后抢分雷）=====
// SNIPE_DOOM：当"连邻居格都逃不出 FUSE+1 拍内致命区"且脚下雷能覆盖>=1对手时，
//   绕过 NOPLACE/CSB 自雷禁令 —— 此时雷是免费的（死在所难免），爆风留下抢 kill credit。
// SNIPE_DOOM_NEI=1 要求"邻居格也全部濒死"（严格）；=0 只看当前格（宽松）。
#ifndef SNIPE_DOOM
#define SNIPE_DOOM 1
#endif
#ifndef SNIPE_DOOM_T
#define SNIPE_DOOM_T 340
#endif
#ifndef SNIPE_DOOM_NEI
#define SNIPE_DOOM_NEI 1
#endif
#endif
#ifndef VIS_W
#define VIS_W 6.0
#endif
#ifndef STAY_MODE
#define STAY_MODE 1
#endif
#ifndef URGE_W
#define URGE_W 0.0
#endif
#ifndef URGE_K
#define URGE_K 4
#endif
#ifndef CORE_W
#define CORE_W 0.0
#endif
#ifndef EVICT_W
#define EVICT_W 0.0
#endif
#ifndef CORE_MIN_LAYER
#define CORE_MIN_LAYER 0
#endif
#ifndef CONTEST
#define CONTEST 0
#endif
#ifndef SHRINK_W
#define SHRINK_W 0.0
#endif
#ifndef SHRINK_H
#define SHRINK_H 60
#endif

#ifndef LOCK_MAX_REGION
#define LOCK_MAX_REGION 8
#endif
#ifndef LOCK_SAFE_RADIUS
#define LOCK_SAFE_RADIUS 3
#endif
#ifndef LOCK_WINDOW_DUEL
#define LOCK_WINDOW_DUEL 400
#endif
#ifndef LOCK_WINDOW_CROWD
#define LOCK_WINDOW_CROWD 150
#endif
#ifndef LOCK_CROWD_CLEARANCE
#define LOCK_CROWD_CLEARANCE 6
#endif
#ifndef SHADOW_W
#define SHADOW_W 120000.0
#endif
#ifndef SHADOW_PULL
#define SHADOW_PULL 2500.0
#endif
#ifndef CROWD_HORIZON
#define CROWD_HORIZON 0
#endif
#ifndef LOCK_CROWD
#define LOCK_CROWD 0
#endif
#ifndef DEPTH_W
#define DEPTH_W 300.0
#endif
#ifndef DEPTH_CAP
#define DEPTH_CAP 80.0
#endif
#ifndef DEPTH_ALL
#define DEPTH_ALL 0
#endif
#ifndef BOMB_GATE
#define BOMB_GATE 1
#endif
#ifndef SHADOW_LANE
#define SHADOW_LANE 1
#endif
#ifndef DEPTH_GATE
#define DEPTH_GATE 1
#endif

// v19：(x,y) 上是否站着任何一个存活的对手 —— 与人数无关的通用判定。
inline bool rivalAt(int x, int y) {
    for (int i = 0; i < P; i++)
        if (i != ME && players[i].alive && players[i].x == x && players[i].y == y) return true;
    return false;
}

inline bool lckOpen(int x, int y) { return x >= 0 && y >= 0 && x < N && y < N && board[y][x] == '.'; }
inline bool lckBombAt(int x, int y) {
    for (auto& b : bombs) if (b.x == x && b.y == y) return true;
    return false;
}

inline bool lckBlastHits(int bx, int by, int tx, int ty) {
    if (bx == tx && by == ty) return true;
    int dx = 0, dy = 0;
    if (bx == tx) dy = (ty > by) ? 1 : -1;
    else if (by == ty) dx = (tx > bx) ? 1 : -1;
    else return false;
    int x = bx, y = by;
    for (int r = 1; r <= RANGE; r++) {
        x += dx; y += dy;
        if (x < 0 || y < 0 || x >= N || y >= N) return false;
        if (board[y][x] == '#') return false;
        if (x == tx && y == ty) return true;
        if (board[y][x] == '+') return false;
        if (lckBombAt(x, y)) return false;
    }
    return false;
}

struct LockInfo {
    bool ok = false;
    int cell = -1, rival = -1, regionMaxE = -1, regionSize = 0, regionRadius = 0, deathTurn = 0;
    vector<char> region;
};

LockInfo lckSealInfo(int mine, int rival) {
    LockInfo s;
    if (mine < 0 || rival < 0) return s;
    // v32 性能：早停洪泛（原实现全图洪泛 + 每次调用分配 2×N² 向量；全人数门格扫描下
    // N=25 每步数百次调用过慢 → 50ms 超时）。语义等价：region>LOCK_MAX_REGION 与
    // "覆盖全图"两种情形原实现均返回 !ok。
    static vector<int> vis, dist, q;
    static int gen = 0;
    if ((int)vis.size() != N * N) { vis.assign(N * N, 0); dist.assign(N * N, 0); }
    q.clear();
    q.push_back(rival);
    vis[rival] = ++gen;
    int size = 0, maxE = -1, radius = 0;
    bool aborted = false;
    for (size_t qi = 0; qi < q.size(); qi++) {
        int c = q[qi];
        if (++size > LOCK_MAX_REGION) { aborted = true; break; }
        maxE = max(maxE, edgeD(c % N, c / N));
        radius = max(radius, dist[c]);
        int x = c % N, y = c / N;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!lckOpen(nx, ny)) continue;
            int z = C(nx, ny);
            if (z == mine || vis[z] == gen) continue;
            vis[z] = gen; dist[z] = dist[c] + 1; q.push_back(z);
        }
    }
    if (aborted) return s;
    if (radius > LOCK_SAFE_RADIUS) return s;
    if (edgeD(mine % N, mine / N) <= maxE) return s;
    s.ok = true; s.cell = mine; s.rival = rival;
    s.regionMaxE = maxE; s.regionSize = size; s.regionRadius = radius;
    s.deathTurn = SHRINK + (maxE - 1) * EVERY;
    s.region.assign(N * N, 0);
    for (size_t qi = 0; qi < q.size(); qi++) s.region[q[qi]] = 1;
    return s;
}

#if HARVEST || SEAL_KILL || AMBUSH
// v25f 通用工具：把"在 (bx,by) 放一颗雷"的火线标出来（含场上所有现有雷）。
void blastLaneOf(int bx, int by, vector<char>& lane) {
    lane.assign(N * N, 0);
    auto mark = [&](int ax, int ay) {
        lane[C(ax, ay)] = 1;
        for (int d = 0; d < 4; d++)
            for (int r = 1; r <= RANGE; r++) {
                int x = ax + DX[d] * r, y = ay + DY[d] * r;
                if (x < 0 || y < 0 || x >= N || y >= N || board[y][x] == '#') break;
                lane[C(x, y)] = 1;
                if (board[y][x] == '+') break;
            }
    };
    mark(bx, by);
    for (auto& b : bombs) mark(b.x, b.y);
}

// 从 start 出发，steps 拍内能否走到"不在 lane 里"的格子（blocked 为 1 的格子不可通行）。
bool canExitLane(int start, const vector<char>& lane, const vector<char>& blocked, int steps) {
    vector<int> dist(N * N, -1), q;
    dist[start] = 0; q.push_back(start);
    for (size_t h = 0; h < q.size(); h++) {
        int c = q[h];
        if (dist[c] >= 1 && !lane[c]) return true;
        if (dist[c] >= steps) continue;
        int x = c % N, y = c / N;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] != '.') continue;
            int z = C(nx, ny);
            if (blocked[z] || dist[z] >= 0) continue;
            dist[z] = dist[c] + 1; q.push_back(z);
        }
    }
    return false;
}

// 除 who 之外的存活玩家所在格。
void occMaskExcept(int who, vector<char>& occ) {
    occ.assign(N * N, 0);
    for (int i = 0; i < P; i++)
        if (i != who && players[i].alive) occ[C(players[i].x, players[i].y)] = 1;
}

// 我脚下放雷后，steps 拍内有没有活路（对手占格不可通行）。
bool escapeAfterBomb(int myCell, int steps) {
    vector<char> lane, occ;
    blastLaneOf(myCell % N, myCell / N, lane);
    occMaskExcept(ME, occ);
    return canExitLane(myCell, lane, occ, steps);
}

#if SEAL_KILL
// 对手 i 是否被我这颗雷"堵死"：他站在我的火线里，且 FUSE 拍内走不出火线。
bool rivalSealedByMyBomb(int i) {
    vector<char> lane, occ;
    blastLaneOf(players[ME].x, players[ME].y, lane);
    int c = C(players[i].x, players[i].y);
    if (!lane[c]) return false;
    occMaskExcept(i, occ);
    return !canExitLane(c, lane, occ, FUSE);
}
#endif
#endif

pair<int, int> decide() {
    if (LIMIT - T <= 0) return {4, 0};
#if ANTI_DEADLOCK
    if (ANTI_DEADLOCK_GEN && lastActD >= 0 && lastActD != 4 &&
        prevX[ME] == players[ME].x && prevY[ME] == players[ME].y)
        blockedCd = min(blockedCd + 1, 10);
    else blockedCd = 0;
#endif
    // 通用模型：三条引擎规则级修正各自是否对所有人数生效
    const bool gSwap = (GEN_SWAP || GEN_ALL);     // v32：默认 1 → 全人数
    const bool gDead = (GEN_DEADEND || GEN_ALL);  // v32：默认 1 → 全人数
    const bool gEsc  = (GEN_ESCAPE || GEN_ALL);   // v32：默认 1 → 全人数
    // 当前已变墙到第几层，以及剩余正方形区域的边长（N=19 时：layer6→5x5，layer7→3x3，layer8→中心 (9,9)）
    const int shrinkLayer = (T < SHRINK) ? 0 : 1 + (T - SHRINK) / EVERY;
    const int openD = max(1, N - 2 * (shrinkLayer + 1));
    // v32 统一：一套配置全人数（原 DUEL_*/CROWD_* 二选一 → 单一值）
    COLL_RT = COLL_MODE;
    int deepMinAlive = DUEL_DEEP_MIN_ALIVE;   // v32：2（原多人=4）
    int deepN = DUEL_DEEP_N;                  // v32：6（原多人=3）
    double deepW = DUEL_DEEP_W;               // v32：1400（原多人=600）
    bool noOccSite = DUEL_NO_OCC_SITE;        // 0（两套同值）
    double rivalW = DUEL_RIVAL_W;             // 0（两套同值）
    int deepSteps = DUEL_DEEP_STEPS;          // 2（两套同值）
    bool oppModel = DUEL_OPP_MODEL;           // 0（两套同值）
    int oppSteps = DUEL_OPP_STEPS;
    int oppKeep = DUEL_OPP_KEEP;
    int oppNear = DUEL_OPP_NEAR;
    double oppExpW = DUEL_OPP_EXP_W;
    int oppExpSteps = DUEL_OPP_EXP_STEPS;
    double crowdW = CROWD_W;                  // 2（取多人值，全人数）
    double visW = VIS_W;                      // 6（两套同值）
    double winW = DUEL_WIN_W;                 // 0（两套同值）
    double nearW = DUEL_NEAR_W;               // 0（两套同值）
    double blockW = DUEL_BLOCK_W;             // v32：300（原多人=0）
    int deepFrom = T + deepSteps;             // v32：统一（原多人=0）
    double chainW = CHAIN_W;                  // 120（两套同值）
    double killBonus = KILL_BONUS;            // 6000（两套同值）
    double escW = ESC_W;                      // 30（两套同值）
    double duelMrW = DUEL_MR_W;               // 0（两套同值）
    double pessW = PESS_W;                    // 2200（两套同值）
    makePot();
    int H = min((int)UNI_H, max(6, LIMIT - T));   // v32：全人数统一 H=20（CPU 预算；原 2p=64/多人=20）
    Pred normal = predict(bombs, H, flames);

    vector<Bm> pessB = bombs;
    vector<Bm> deepB;
    for (int i = 0; i < P; i++) {
        if (i == ME || !players[i].alive) continue;
        int md = abs(players[i].x - players[ME].x) + abs(players[i].y - players[ME].y);
        if (md > 9) continue;
        int cnt = 0; bool onb = false;
        for (auto& b : bombs) { if (b.owner == i) cnt++; if (b.x == players[i].x && b.y == players[i].y) onb = true; }
        if (cnt >= CAP || onb) continue;
        pessB.push_back({i, players[i].x, players[i].y, T + FUSE});
#if CLOSE_THREAT
        int bx = players[i].x, by = players[i].y;
        int bestc = -1, bestd = md;
        for (int d = 0; d < 4; d++) {
            int nx = bx + DX[d], ny = by + DY[d];
            if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] != '.') continue;
            bool occ = false;
            for (auto& b : bombs) if (b.x == nx && b.y == ny) occ = true;
            if (occ) continue;
            if (noOccSite) {
                bool taken = false;
                for (int q = 0; q < P; q++)
                    if (q != i && players[q].alive && players[q].x == nx && players[q].y == ny) taken = true;
                if (taken) continue;
            }
            int dd = abs(nx - players[ME].x) + abs(ny - players[ME].y);
            if (dd < bestd) { bestd = dd; bestc = C(nx, ny); }
        }
        if (bestc >= 0) pessB.push_back({i, bestc % N, bestc / N, T + 1 + FUSE});
#endif
#if DEEP_THREAT
        int aliveNow = 0;
        for (int q2 = 0; q2 < P; q2++) aliveNow += players[q2].alive;
        if (aliveNow >= deepMinAlive && !oppModel)
        {
            vector<int> dist(N * N, -1);
            deque<int> q;
            int src = C(players[i].x, players[i].y);
            dist[src] = 0; q.push_back(src);
            while (!q.empty()) {
                int c = q.front(); q.pop_front();
                if (dist[c] >= deepSteps) continue;
                int x = c % N, y = c / N;
                for (int d = 0; d < 4; d++) {
                    int nx = x + DX[d], ny = y + DY[d];
                    if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] != '.') continue;
                    int z = C(nx, ny);
                    bool occ = false;
                    for (auto& b : bombs) if (b.x == nx && b.y == ny) occ = true;
                    if (occ) continue;
                    if (dist[z] < 0) { dist[z] = dist[c] + 1; q.push_back(z); }
                }
            }
            vector<pair<int, int>> cand;
            for (int c = 0; c < N * N; c++)
                if (dist[c] >= 1) {
                    if (noOccSite) {
                        bool taken = false;
                        for (int q = 0; q < P; q++)
                            if (q != i && players[q].alive && players[q].x == c % N && players[q].y == c / N) taken = true;
                        if (taken) continue;
                    }
                    int dd = abs(c % N - players[ME].x) + abs(c / N - players[ME].y);
                    if (dd <= RANGE + 1) cand.push_back({dd, c});
                }
            sort(cand.begin(), cand.end());
            if ((int)cand.size() > deepN) cand.resize(deepN);
            for (auto& cd : cand) deepB.push_back({i, cd.second % N, cd.second / N, T + deepSteps + FUSE, deepFrom});
        }
#endif
    }
    if (oppModel) {
        int aliveNow = 0;
        for (int q2 = 0; q2 < P; q2++) aliveNow += players[q2].alive;
        if (aliveNow >= deepMinAlive) {
            Pred pessBase = predict(pessB, H, flames);
            Reach rbase = reach(pessBase, ME);
            int baseLife = 0;
            for (int q3 = 0; q3 < 5; q3++) baseLife = max(baseLife, rbase.life[q3]);
            for (int i = 0; i < P; i++) {
                if (i == ME || !players[i].alive) continue;
                int cnt = 0; bool onb = false;
                for (auto& b : bombs) { if (b.owner == i) cnt++; if (b.x == players[i].x && b.y == players[i].y) onb = true; }
                if (cnt >= CAP || onb) continue;
                if (abs(players[i].x - players[ME].x) + abs(players[i].y - players[ME].y) > 9) continue;
                vector<int> dist(N * N, -1);
                deque<int> q;
                int src = C(players[i].x, players[i].y);
                dist[src] = 0; q.push_back(src);
                while (!q.empty()) {
                    int c = q.front(); q.pop_front();
                    if (dist[c] >= oppSteps) continue;
                    int x = c % N, y = c / N;
                    for (int d = 0; d < 4; d++) {
                        int nx = x + DX[d], ny = y + DY[d];
                        if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] != '.') continue;
                        int z = C(nx, ny);
                        bool occ = false;
                        for (auto& b : bombs) if (b.x == nx && b.y == ny) occ = true;
                        if (occ) continue;
                        if (dist[z] < 0) { dist[z] = dist[c] + 1; q.push_back(z); }
                    }
                }
                vector<pair<double, int>> rank;
                for (int c = 0; c < N * N; c++) {
                    if (dist[c] < 1) continue;
                    if (oppNear > 0 &&
                        abs(c % N - players[ME].x) + abs(c / N - players[ME].y) > oppNear) continue;
                    bool taken = false;
                    for (int q4 = 0; q4 < P; q4++)
                        if (q4 != i && players[q4].alive && players[q4].x == c % N && players[q4].y == c / N) taken = true;
                    if (taken) continue;
                    vector<Bm> probe = pessB;
                    probe.push_back({i, c % N, c / N, T + dist[c] + FUSE, T + dist[c]});
                    Pred pp2 = predict(probe, H, flames);
                    Reach rr = reach(pp2, ME);
                    int lf = 0;
                    for (int q3 = 0; q3 < 5; q3++) lf = max(lf, rr.life[q3]);
                    double dmg = (baseLife - lf) * 1000.0 + (rbase.unique - rr.unique);
                    rank.push_back({dmg, c});
                }
                sort(rank.rbegin(), rank.rend());
                int keep = min((int)rank.size(), oppKeep);
                for (int k2 = 0; k2 < keep; k2++) {
                    if (rank[k2].first <= 0) break;
                    int c = rank[k2].second;
                    deepB.push_back({i, c % N, c / N, T + dist[c] + FUSE, T + dist[c]});
                }
            }
        }
    }
    vector<int> oppExpose(N * N, 0);
    if (oppExpW != 0.0) {
        for (int i = 0; i < P; i++) {
            if (i == ME || !players[i].alive) continue;
            vector<int> dist(N * N, -1);
            deque<int> q;
            int src = C(players[i].x, players[i].y);
            dist[src] = 0; q.push_back(src);
            while (!q.empty()) {
                int c = q.front(); q.pop_front();
                if (dist[c] >= oppExpSteps) continue;
                int x = c % N, y = c / N;
                for (int d = 0; d < 4; d++) {
                    int nx = x + DX[d], ny = y + DY[d];
                    if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] != '.') continue;
                    int z = C(nx, ny);
                    bool occ = false;
                    for (auto& b : bombs) if (b.x == nx && b.y == ny) occ = true;
                    if (occ) continue;
                    if (dist[z] < 0) { dist[z] = dist[c] + 1; q.push_back(z); }
                }
            }
            for (int c = 0; c < N * N; c++) {
                if (dist[c] < 0) continue;
                int x = c % N, y = c / N;
                for (int d = 0; d < 4; d++)
                    for (int r = 1; r <= RANGE; r++) {
                        int nx = x + DX[d] * r, ny = y + DY[d] * r;
                        if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] == '#') break;
                        oppExpose[C(nx, ny)]++;
                        if (board[ny][nx] == '+') break;
                    }
            }
        }
    }
    vector<char> rivalBlast(N * N, 0);
    if (nearW != 0.0)
        for (int i = 0; i < P; i++) {
            if (i == ME || !players[i].alive) continue;
            rivalBlast[C(players[i].x, players[i].y)] = 1;
            for (int d = 0; d < 4; d++)
                for (int r = 1; r <= RANGE; r++) {
                    int nx = players[i].x + DX[d] * r, ny = players[i].y + DY[d] * r;
                    if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] == '#') break;
                    rivalBlast[C(nx, ny)] = 1;
                    if (board[ny][nx] == '+') break;
                }
        }
    Pred pessNormal = predict(pessB, H, flames);
    vector<Bm> deepAll = pessB;
    deepAll.insert(deepAll.end(), deepB.begin(), deepB.end());
    // v26：deepNormal 仅按需计算。原式在 deepB 为空时会深拷贝 pessNormal（该对象无任何使用点）；
    // 非空时这里算一次，供下方 place=0 的 dp 直接引用（原来那次重复 predict 也一并省掉）。
    Pred deepNormal;
    if (!deepB.empty()) deepNormal = predict(deepAll, H, flames);

    vector<char> rivalMask(N * N, 0);
    for (int i = 0; i < P; i++)
        if (i != ME && players[i].alive) {
            rivalMask[C(players[i].x, players[i].y)] = 1;
            if (DUEL_CONTEST)
                for (int d = 0; d < 4; d++) {
                    int nx = players[i].x + DX[d], ny = players[i].y + DY[d];
                    if (nx >= 0 && ny >= 0 && nx < N && ny < N && board[ny][nx] == '.')
                        rivalMask[C(nx, ny)] = 1;
                }
        }

    // v33p：P-1 个对手的"自由可达集"一次传播解齐（原来每人一次 reach）
    vector<int> obase(P, 0), obaseLife(P, 0);
    {
        vector<int> who, src;
        for (int i = 0; i < P; i++)
            if (i != ME && players[i].alive) { who.push_back(i); src.push_back(C(players[i].x, players[i].y)); }
        MultiRes mr;
        reach_multi_core(normal, src, who, nullptr, nullptr, false, mr);
        for (int q = 0; q < (int)who.size(); q++) { obase[who[q]] = mr.unique[q]; obaseLife[who[q]] = mr.life[q]; }
    }

#if RIVAL_REACH || AMBUSH
    // 对手 RIVAL_STEPS 拍可达集 → 每格危险度 rrCnt（有多少对手能到）；同时保留每个对手的可达集 rrSeen[i]。
    vector<unsigned char> rrCnt(N * N, 0);
    vector<vector<char>> rrSeen(P, vector<char>(N * N, 0));
    if (P > 2 && H > RIVAL_STEPS) {
        for (int i = 0; i < P; i++) {
            if (i == ME || !players[i].alive) continue;
            vector<unsigned char> cur(N * N, 0), seen(N * N, 0);
            cur[C(players[i].x, players[i].y)] = 1;
            for (int k = 0; k <= RIVAL_STEPS; k++) {
                for (int c = 0; c < N * N; c++) if (cur[c]) seen[c] = 1;
                if (k == RIVAL_STEPS) break;
                vector<unsigned char> nxt(N * N, 0);
                for (int c = 0; c < N * N; c++) {
                    if (!cur[c]) continue;
                    int x = c % N, y = c / N;
                    for (int d = 0; d < 5; d++) {
                        int nx = x + DX[d], ny = y + DY[d], nc = c;
                        if (nx >= 0 && ny >= 0 && nx < N && ny < N) {
                            int z = C(nx, ny);
                            if (normal.open[k * N * N + z] && (!normal.occ[k * N * N + z] || z == c)) nc = z;
                        }
                        if (normal.safe[k * N * N + nc]) nxt[nc] = 1;
                    }
                }
                cur.swap(nxt);
            }
            for (int c = 0; c < N * N; c++) { if (seen[c]) rrCnt[c]++; rrSeen[i][c] = seen[c]; }
        }
    }
#endif

    int myCell = C(players[ME].x, players[ME].y);
#if CHASE_FOLLOW
    // v24（用户规则）：日字追身 —— 对手与我呈"日"字形且它刚刚移动过 →
    //   朝它"下一步最可能的位置"(r+v) 靠拢，最后**覆盖式**执行（要盖过"顶点站桩"）。
    // v24.1（seed602 t=144 复盘，用户指令"应该向上走"）：原版"复读对手方向"会追丢——
    //   t=144 我(7,7)、Astra(8,5)（日字形，它刚 L 走），评分本就选 U(7,6)（864617 全场最高），
    //   却被"跟 L"覆盖成 L(6,7)（851633）。改为**拦截法**：对每个方向算"移动后到 (r+v) 的距离"，
    //   取最小者；并列时优先它的前进方向。（600 t=48：D/L 并列→同向优先→D，与已验证行为一致。）
    int chaseFollowD[64], chaseIsSame[64], chaseN = 0;   // chaseIsSame：该方向是否与对手前进方向一致
    for (int i = 0; i < P; i++) {
        if (i == ME || !players[i].alive || prevX[i] < 0) continue;
        int mvx = players[i].x - prevX[i], mvy = players[i].y - prevY[i];
        if (abs(mvx) + abs(mvy) != 1) continue;   // 它本回合没动（或瞬移）→ 不算"刚动过"
        int ax = abs(players[i].x - players[ME].x), ay = abs(players[i].y - players[ME].y);
        if (!((ax == 2 && ay == 1) || (ax == 1 && ay == 2))) continue;   // 日字形
        // 拦截目标 = 它下一步最可能去的位置（沿它刚走的方向再走一格；出界则退化为它原位）
        int tx = players[i].x + mvx, ty = players[i].y + mvy;
        if (tx < 0 || ty < 0 || tx >= N || ty >= N) { tx = players[i].x; ty = players[i].y; }
        // v24.3：v24.1 的"同向优先"会在两方向距离并列时偷偷裁掉另一条（seed619 t=117：
        //   L(6,5) 评分本是最高的 864649，被同向的 U 覆盖）→ 改为**并列全收集**，交给评分选。
        int cfdDist = 1 << 20;
        for (int dd = 0; dd < 4; dd++) {
            int nx2 = players[ME].x + DX[dd], ny2 = players[ME].y + DY[dd];
            if (nx2 < 0 || ny2 < 0 || nx2 >= N || ny2 >= N) continue;
            int dist2 = abs(nx2 - tx) + abs(ny2 - ty);
            if (dist2 < cfdDist) cfdDist = dist2;
        }
        if (cfdDist == (1 << 20)) continue;
#if CHASE_PAIRS
        // 并列全收集（v24.3）——交给评分在"并列的追逐方向"里选最优。
        for (int dd = 0; dd < 4 && chaseN < 63; dd++) {
            int nx2 = players[ME].x + DX[dd], ny2 = players[ME].y + DY[dd];
            if (nx2 < 0 || ny2 < 0 || nx2 >= N || ny2 >= N) continue;
            if (abs(nx2 - tx) + abs(ny2 - ty) != cfdDist) continue;
            int fx = players[ME].x + DX[dd], fy = players[ME].y + DY[dd];
            // 保险（seed602 复盘：踩着自己刚放的雷往下追，钻进死走廊被对手第二颗雷封死）：
            // 脚下或跟随落点正处在任何现有雷的爆风里 → 先逃命，不追。
            bool bombDanger = false;
            for (auto& b : bombs)
                if (lckBlastHits(b.x, b.y, players[ME].x, players[ME].y) ||
                    lckBlastHits(b.x, b.y, fx, fy)) { bombDanger = true; break; }
            if (bombDanger) continue;
            chaseIsSame[chaseN] = (DX[dd] == mvx && DY[dd] == mvy) ? 1 : 0;
            chaseFollowD[chaseN++] = dd;
        }
#else
        // 旧逻辑（v24.1）：距离并列时"同向优先"。
        int cfd = 4, cfdSame = 1;
        for (int dd = 0; dd < 4; dd++) {
            int nx2 = players[ME].x + DX[dd], ny2 = players[ME].y + DY[dd];
            if (nx2 < 0 || ny2 < 0 || nx2 >= N || ny2 >= N) continue;
            if (abs(nx2 - tx) + abs(ny2 - ty) != cfdDist) continue;
            int same = (DX[dd] == mvx && DY[dd] == mvy) ? 0 : 1;
            if (same < cfdSame) { cfdSame = same; cfd = dd; }
        }
        if (cfd == 4) continue;
        {
            int fx = players[ME].x + DX[cfd], fy = players[ME].y + DY[cfd];
            bool bombDanger = false;
            for (auto& b : bombs)
                if (lckBlastHits(b.x, b.y, players[ME].x, players[ME].y) ||
                    lckBlastHits(b.x, b.y, fx, fy)) { bombDanger = true; break; }
            if (bombDanger) continue;
            chaseIsSame[chaseN] = 1;   // 旧逻辑选出的就是"同向优先"方向，标 1（避免未初始化读取）
            chaseFollowD[chaseN++] = cfd;
        }
#endif
    }
    bool chaseApplied = false;
    double chaseSc = -1e30;
    pair<int, int> chaseAns = {4, 0};
    bool chaseSameOk = false;          // v24.4：跟随候选里"同向方向"的最佳记录
    double chaseSameSc = -1e30;
    pair<int, int> chaseSameAns = {4, 0};
#endif
    vector<int> dMe = bfs(board, myCell);
    int goal = -1; double gv = -1e18;
    for (int c = 0; c < N * N; c++)
        if (dMe[c] < 100000000) {
            double v = 5.0 * edgeD(c % N, c / N) - 0.8 * dMe[c];
            if (v > gv) { gv = v; goal = c; }
        }
    vector<int> dGoal = goal >= 0 ? bfs(board, goal) : vector<int>(N * N, 100000000);

    vector<int> coreDist(N * N, 100000000);
    {
        int maxE = -1;
        for (int c = 0; c < N * N; c++)
            if (board[c / N][c % N] != '#') maxE = max(maxE, edgeD(c % N, c / N));
        vector<int> q;
        for (int c = 0; c < N * N; c++)
            if (board[c / N][c % N] != '#' && edgeD(c % N, c / N) == maxE) {
                coreDist[c] = 0;
                q.push_back(c);
            }
        for (size_t h = 0; h < q.size(); h++) {
            int c = q[h], x = c % N, y = c / N;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
                if (board[ny][nx] == '#') continue;
                int z = C(nx, ny);
                if (coreDist[z] > coreDist[c] + 1) { coreDist[z] = coreDist[c] + 1; q.push_back(z); }
            }
        }
    }

    vector<int> firstLethal(N * N, H);
    for (int c = 0; c < N * N; c++)
        for (int k = 0; k < H; k++)
            if (!normal.safe[k * N * N + c]) { firstLethal[c] = k; break; }

    int myCnt = 0; bool onBomb = false;
    for (auto& b : bombs) { if (b.owner == ME) myCnt++; if (b.x == players[ME].x && b.y == players[ME].y) onBomb = true; }

    int pinExit = rivalPocketExit();   // v32：全人数
    int pinDist = 1 << 20;
    if (pinExit >= 0) pinDist = abs(pinExit % N - players[ME].x) + abs(pinExit / N - players[ME].y);
    if (DBG && pinExit >= 0)
        fprintf(stderr, "PIN t=%d exit=(%d,%d) my=(%d,%d) dist=%d\n", T, pinExit % N, pinExit / N,
                players[ME].x, players[ME].y, pinDist);
    // v32：rival = 最外围对手（离边界最近；并列取离我最近；2p 时=唯一对手）
    int rivalCell = -1;
    {
        int bestE2 = 1 << 20, bestD2r = 1 << 20;
        for (int i = 0; i < P; i++) {
            if (i == ME || !players[i].alive) continue;
            int e2 = edgeD(players[i].x, players[i].y);
            int dd2 = abs(players[i].x - players[ME].x) + abs(players[i].y - players[ME].y);
            if (e2 < bestE2 || (e2 == bestE2 && dd2 < bestD2r)) {
                bestE2 = e2; bestD2r = dd2; rivalCell = C(players[i].x, players[i].y);
            }
        }
    }
    // v34k5：我与对手同行/列、中间隔墙，且我更靠内（= 我正挡在它往内圈那条线上）
    bool wallBetween = false;
    if (rivalCell >= 0) {
        int mx = players[ME].x, my = players[ME].y;
        int rx = rivalCell % N, ry = rivalCell / N;
        if (mx == rx) {
            int st = (ry > my) ? 1 : -1;
            for (int yy = my + st; yy != ry; yy += st) if (board[yy][mx] == '#') { wallBetween = true; break; }
        } else if (my == ry) {
            int st = (rx > mx) ? 1 : -1;
            for (int xx = mx + st; xx != rx; xx += st) if (board[my][xx] == '#') { wallBetween = true; break; }
        }
    }
    // v35b（用户规则）：我脚下是十字格、且有对手紧贴(曼哈顿1)、且那个对手比我更外圈
    //   → 站住不动额外 +500（抵掉相邻格的零碎项抖动）
    bool crossPress = false;
    if (isCross(players[ME].x, players[ME].y)) {
        int myE = edgeD(players[ME].x, players[ME].y);
        for (int i = 0; i < P; i++) {
            if (i == ME || !players[i].alive) continue;
            int dd = abs(players[i].x - players[ME].x) + abs(players[i].y - players[ME].y);
            if (dd == 1 && edgeD(players[i].x, players[i].y) < myE) { crossPress = true; break; }
        }
    }
    const bool holdWall = wallBetween && rivalCell >= 0 &&
        edgeD(players[ME].x, players[ME].y) > edgeD(rivalCell % N, rivalCell / N);
#if ESC_GUIDE || STAY_GUARD
    // 撤离向导：多源 BFS —— 从"安全格"（不被任何现有雷覆盖的空地）出发，
    // 反向算出每个空地到最近安全格的步数（distSafe）。全危险时保持大值（评分里截断）。
    // （STAY_GUARD 也用它判断"我格不被任何雷覆盖"。）
    vector<int> distSafe(N * N, 100000000);
    {
        // v25：多人局把"活着的对手所在格"视为不可通行（对手会挡路/换格），
        // 否则会朝一个被对手占住的安全格跑（seed1203 自爆）。P<=2 时 occRival 全 0 → 原口径。
        vector<char> occRival(N * N, 0);
#if GUIDE_BLOCK_RIVAL
        for (int i = 0; i < P; i++)
            if (i != ME && players[i].alive) occRival[C(players[i].x, players[i].y)] = 1;
#endif
        vector<int> q;
        for (int c = 0; c < N * N; c++) {
            int x = c % N, y = c / N;
            if (board[y][x] != '.') continue;
            if (occRival[c]) continue;
            bool danger = false;
            for (auto& b : bombs)
                if (lckBlastHits(b.x, b.y, x, y)) { danger = true; break; }
            if (!danger) { distSafe[c] = 0; q.push_back(c); }
        }
        for (size_t h = 0; h < q.size(); h++) {
            int c = q[h], x = c % N, y = c / N;
            for (int dd = 0; dd < 4; dd++) {
                int nx = x + DX[dd], ny = y + DY[dd];
                if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] != '.') continue;
                int z = C(nx, ny);
                if (occRival[z]) continue;
                if (distSafe[z] > distSafe[c] + 1) { distSafe[z] = distSafe[c] + 1; q.push_back(z); }
            }
        }
    }
#endif
#if SEEK_RIVAL
    // 开局找对手：对"对手"做箱子可通行的 BFS（箱子 '+' 当"能开"，只挡永久墙）。
    vector<int> dRiv2(N * N, 100000000);
    if (T < SHRINK && rivalCell >= 0) {
        vector<int> q;
        dRiv2[rivalCell] = 0;
        q.push_back(rivalCell);
        for (size_t h = 0; h < q.size(); h++) {
            int c = q[h], x = c % N, y = c / N;
            for (int dd = 0; dd < 4; dd++) {
                int nx = x + DX[dd], ny = y + DY[dd];
                if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
                if (board[ny][nx] == '#') continue;
                int z = C(nx, ny);
                if (dRiv2[z] > dRiv2[c] + 1) { dRiv2[z] = dRiv2[c] + 1; q.push_back(z); }
            }
        }
    }
#endif
    // v32：锁门全人数 —— 取"死得最早"的可封对手（2p 时=唯一对手）
    LockInfo lock;
    for (int i = 0; i < P; i++)
        if (i != ME && players[i].alive) {
            LockInfo cand = lckSealInfo(myCell, C(players[i].x, players[i].y));
            if (cand.ok && (!lock.ok || cand.deathTurn < lock.deathTurn)) lock = cand;
        }
    if (lock.ok) {
        int win = LOCK_WINDOW_DUEL;   // v32：统一 400
        if (lock.deathTurn - T > win) lock.ok = false;
    }
    if (lock.ok)
        for (int i = 0; i < P; i++)
            if (i != ME && i != lock.rival && players[i].alive &&
                abs(players[i].x - players[ME].x) + abs(players[i].y - players[ME].y) < LOCK_CROWD_CLEARANCE)
                lock.ok = false;
#if GATE_SEEK
    int gateT = -1, gateMaxE = -1, gateSize = 0, gateRival = -1;
    vector<char> gateReg;
#if GATE_SEEK == 2
    if (rivalCell >= 0) {
        int bestD = 1 << 20;
        for (int g = 0; g < N * N; g++) {
            int gx = g % N, gy = g / N;
            if (board[gy][gx] != '.') continue;
            if (lckBombAt(gx, gy) || g == rivalCell) continue;
            LockInfo li = lckSealInfo(g, rivalCell);
            if (!li.ok) continue;
            int rd = SHRINK + (li.regionMaxE - 1) * EVERY;
            if (rd - T <= 0) continue;
            int dist = abs(gx - players[ME].x) + abs(gy - players[ME].y);
            if (dist > rd - T) continue;
            if (dist < bestD) { bestD = dist; gateT = g; gateMaxE = li.regionMaxE; gateSize = li.regionSize; gateReg = li.region; }
        }
    }
#else
    if (rivalCell >= 0) {
        int bestD = 1 << 20;
        vector<char> reg(N * N, 0), reg0;
        vector<int> q;
        for (int g = 0; g < N * N; g++) {
            int gx = g % N, gy = g / N;
            if (board[gy][gx] != '.') continue;
            if (lckBombAt(gx, gy)) continue;
            if (g == rivalCell) continue;
            int ge = edgeD(gx, gy);
            if (ge <= 0) continue;
            int maxE = -1, size0 = 0;
            bool ok = true;
            for (int pass = 0; pass < 2; pass++) {
                fill(reg.begin(), reg.end(), 0);
                q.clear(); q.push_back(rivalCell); reg[rivalCell] = 1;
                maxE = edgeD(rivalCell % N, rivalCell / N);
                for (size_t qi = 0; qi < q.size(); qi++) {
                    int c = q[qi], x = c % N, y = c / N;
                    maxE = max(maxE, edgeD(x, y));
                    for (int dd = 0; dd < 4; dd++) {
                        int nx = x + DX[dd], ny = y + DY[dd];
                        if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
                        if (board[ny][nx] == '#') continue;
                        if (pass == 0 && board[ny][nx] != '.') continue;
                        int z = C(nx, ny);
                        if (z == g || reg[z]) continue;
                        reg[z] = 1; q.push_back(z);
                    }
                }
                if (pass == 0) { size0 = (int)q.size(); reg0 = reg; }
                if (maxE >= ge) { ok = false; break; }
            }
            if (!ok) continue;
            int rivalDeath = SHRINK + (maxE - 1) * EVERY;
            if (rivalDeath - T <= 0) continue;
            if (abs(gx - players[ME].x) + abs(gy - players[ME].y) > rivalDeath - T) continue;
            int dist = abs(gx - players[ME].x) + abs(gy - players[ME].y);
            if (dist < bestD) { bestD = dist; gateT = g; gateMaxE = maxE; gateSize = size0; gateReg = reg0; }
        }
    }
#endif
#if GEN_GATE
    // 人群局：只挑最靠外的 GEN_GATE_RIVALS 个对手，扫他们 4 个邻格做门格
    // （外层对手的"唯一出口"通常就紧挨着他，所以只扫邻格就够，开销可忽略）。
    if (P > 2 && openD > CROWD_CENTER_D) {
        vector<pair<int, int>> ord;
        for (int i = 0; i < P; i++)
            if (i != ME && players[i].alive) ord.push_back({edgeD(players[i].x, players[i].y), i});
        sort(ord.begin(), ord.end());
        int bestD2 = 1 << 20;
        for (int k = 0; k < (int)ord.size() && k < GEN_GATE_RIVALS; k++) {
            int i = ord[k].second, rx = players[i].x, ry = players[i].y, rc = C(rx, ry);
            for (int dd = 0; dd < 4; dd++) {
                int nx = rx + DX[dd], ny = ry + DY[dd];
                if (!lckOpen(nx, ny) || lckBombAt(nx, ny)) continue;
                if (rivalAt(nx, ny)) continue;
                int g = C(nx, ny);
                LockInfo li = lckSealInfo(g, rc);
                if (!li.ok) continue;
                if (li.regionMaxE >= edgeD(nx, ny)) continue;   // 门格必须比口袋更靠内
                int rd = SHRINK + (li.regionMaxE - 1) * EVERY;
                if (rd - T <= 0) continue;
                int gWall = SHRINK + (edgeD(nx, ny) - 1) * EVERY;   // 门格自己的变墙回合
                if (gWall < rd + CROWD_GATE_MARGIN) continue;       // 封完还得来得及撤回内圈
                int dist = abs(nx - players[ME].x) + abs(ny - players[ME].y);
                if (dist > rd - T) continue;
                if (dist < bestD2) {
                    bestD2 = dist; gateT = g; gateMaxE = li.regionMaxE;
                    gateSize = li.regionSize; gateReg = li.region; gateRival = rc;
                }
            }
        }
    }
#endif
    if (gateT >= 0 && gateT == myCell && !lock.ok) {
        lock.ok = true; lock.cell = myCell; lock.rival = (gateRival >= 0) ? gateRival : rivalCell;
        lock.regionMaxE = gateMaxE; lock.regionSize = gateSize; lock.regionRadius = 0;
        lock.deathTurn = SHRINK + (gateMaxE - 1) * EVERY;
        lock.region = gateReg;
    }
#endif

#if CROSS_MODE
    // v16：目标点 = 离对手最近、且比对手靠内的"十字点"。
    // 对手横向移动→目标点跟着同一列走（自动"他往左我往左"）；
    // 对手单纯后退→目标点不动（自动"他后退我别动"）。
    int crossT = -1;
    if (rivalCell >= 0) {
        int rx = rivalCell % N, ry = rivalCell / N, re = edgeD(rx, ry);
        int bestD = 1 << 20, bestE = -1;
        for (int y = 1; y < N - 1; y++)
            for (int x = 1; x < N - 1; x++) {
                if (!isCross(x, y)) continue;
                int e = edgeD(x, y);
                if (e <= re) continue;
                int dd = abs(x - rx) + abs(y - ry);
                if (dd < bestD || (dd == bestD && e > bestE)) { bestD = dd; bestE = e; crossT = C(x, y); }
            }
    }
    // v34k2：我与要追的对手在同一行/列、且中间隔着永久墙 → 十字点不适用。
    // 依据 3p seed992001 T=60：我(6,11)、Astra(6,13)，(6,12) 是墙，十字点却把我拉到侧面的 (5,11)，
    // 下一拍对手横移后十字点又翻到 (7,11)，形成左右来回。墙后看不到也压不住，直接关掉十字点。
    if (crossT >= 0 && wallBetween) {
        crossT = -1;
        if (DBG) fprintf(stderr, "CW t=%d cross off (wall between)\n", T);
    }
    const bool crossBomb = (crossT >= 0) && (board[crossT / N][crossT % N] == '+') &&
                           lckBlastHits(players[ME].x, players[ME].y, crossT % N, crossT / N);
#endif

    bool lockOn = lock.ok, lockCovers = false;
    if (lockOn) {
        lockCovers = true;
        for (int c = 0; c < N * N && lockCovers; c++)
            if (lock.region[c] && !lckBlastHits(myCell % N, myCell / N, c % N, c / N)) lockCovers = false;
#if LOCK_BOMB_SINGLE
        if (!lockCovers && lock.regionSize <= 1) {
            int ri = -1;
            for (int i = 0; i < P; i++) if (i != ME && players[i].alive) ri = i;
            if (ri >= 0 && lckBlastHits(myCell % N, myCell / N, players[ri].x, players[ri].y)) lockCovers = true;
        }
#endif
    }

    int shadowT = -1;
#if SHADOW_ALWAYS == 2
    if (rivalCell >= 0) {
        int rx = rivalCell % N, ry = rivalCell / N, be = edgeD(rx, ry);
        int bestE = be, bestD = 1 << 20;
        for (int d = 0; d < 4; d++) {
            int nx = rx + DX[d], ny = ry + DY[d], steps = 1;
            while (nx >= 0 && ny >= 0 && nx < N && ny < N && steps <= SHADOW_MAX_STEPS && !isVert(nx, ny)) {
                nx += DX[d]; ny += DY[d]; steps++;
            }
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            if (!isVert(nx, ny)) continue;
            int e = edgeD(nx, ny);
            if (e <= be) continue;
            int dist = abs(nx - players[ME].x) + abs(ny - players[ME].y);
            if (e > bestE || (e == bestE && dist < bestD)) { bestE = e; bestD = dist; shadowT = C(nx, ny); }
        }
    }
#else
    if (rivalCell >= 0
#if SHADOW_ALWAYS == 0
        && edgeD(players[ME].x, players[ME].y) <= edgeD(rivalCell % N, rivalCell / N)
#endif
    ) {
        int rx = rivalCell % N, ry = rivalCell / N, be = edgeD(rx, ry);
        int bestE = be, bestD = 1 << 20;
        for (int d = 0; d < 4; d++) {
            int nx = rx + DX[d], ny = ry + DY[d];
            while (nx >= 0 && ny >= 0 && nx < N && ny < N && !lckOpen(nx, ny)) {
                nx += DX[d]; ny += DY[d];
            }
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            int e = edgeD(nx, ny);
            if (e <= be) continue;
            int dist = abs(nx - players[ME].x) + abs(ny - players[ME].y);
            if (e > bestE || (e == bestE && dist < bestD)) { bestE = e; bestD = dist; shadowT = C(nx, ny); }
        }
    }
#endif
    if (DBG && (lockOn || shadowT >= 0))
        fprintf(stderr, "V11 t=%d lock=%d cell=(%d,%d) size=%d radius=%d maxE=%d deathT=%d covers=%d shadow=(%d,%d)\n",
                T, (int)lockOn, myCell % N, myCell / N, lock.regionSize, lock.regionRadius,
                lock.regionMaxE, lock.deathTurn, (int)lockCovers,
                shadowT >= 0 ? shadowT % N : -1, shadowT >= 0 ? shadowT / N : -1);

    // 起爆那一拍必须还活着；FUSE 从输入读，所以这里用运行时 max。
    const int lockBombLife = max(6, FUSE + 1);
    bool lockHoldOk = false, lockBombOk = false;
    double lockBombSc = -1e30;
    pair<int, int> lockBombAns = {4, 1};
#if DEADEND_BOMB
    bool deadEndBombOk = false;
    double deadEndBombSc = -1e30;
    pair<int, int> deadEndBombAns = {4, 1};
#endif

    int curLayer = (T < SHRINK) ? 0 : 1 + (T - SHRINK) / EVERY;
    auto marginOf = [&](int c) { return edgeD(c % N, c / N) - (curLayer + 1); };

    const int SH = min((int)SHRINK_H, max(6, LIMIT - T));
    Pred shrinkOnly;
    if (SHRINK_W != 0.0) shrinkOnly = predict({}, SH, {});

    double best = -1e30; pair<int, int> ans = {4, 0};
    int fLifeStay = -1, fLifeOther = -1;  // v31r5：堡垒锁用（place=0 世界里 留/离 的最大生命）
    bool staySafe = false;   // 原地不动在"正常 + 悲观"两个世界里都活得下去
    double riskMult = DUEL_MULT;   // v32：统一 1.0（原 2p=0）
    bool crateHit = false;
    for (int d = 0; d < 4 && !crateHit; d++)
        for (int r = 1; r <= RANGE; r++) {
            int nx = players[ME].x + DX[d] * r, ny = players[ME].y + DY[d] * r;
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) break;
            if (board[ny][nx] == '#') break;
            if (board[ny][nx] == '+') { crateHit = true; break; }
        }
    const bool crateOK = crateHit && (!BOMB_EARLY || T < SHRINK);
#if DEADEND_BOMB
    // 用户规则：某个对手被墙围三面、第四面的唯一出口就是我 → 它已经无路可走，我在脚下放雷必收。
    bool deadEndKill = false;
    for (int i = 0; i < P && !deadEndKill; i++) {
        if (i == ME || !players[i].alive) continue;
        int rx = players[i].x, ry = players[i].y;
        int wallCnt = 0, meCnt = 0;
        for (int dd = 0; dd < 4; dd++) {
            int nx = rx + DX[dd], ny = ry + DY[dd];
            // v34k：任一非空格（墙/箱/已放雷）都算"走不进去" —— 单出口判据不再漏掉箱子
            if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] != '.' || lckBombAt(nx, ny)) { wallCnt++; continue; }
            if (nx == players[ME].x && ny == players[ME].y) meCnt++;
        }
        if (wallCnt == 3 && meCnt == 1 && lckBlastHits(players[ME].x, players[ME].y, rx, ry))
            deadEndKill = true;
    }
#endif
    const bool placeOK = !BOMB_GATE || crateOK || (lockOn && lockCovers) || (pinExit >= 0 && myCell == pinExit)
#if DEADEND_BOMB
        || deadEndKill
#endif
#if CROSS_MODE
        || crossBomb
#endif
        ;
#if NO_CHAIN
    bool chainRisk = false;
    for (auto& b : bombs)
        if (b.owner != ME && abs(b.x - players[ME].x) + abs(b.y - players[ME].y) <= 1) chainRisk = true;
    const bool placeOK2 = placeOK && !chainRisk;
#else
    const bool placeOK2 = placeOK;
#endif
    bool snipeDoomed = false;  // v31r6：濒死白名单（见开关说明）
#if SNIPE_DOOM
    if (T >= DEATHTv - (17 * EVERY) / 5) {   // v32：全人数 + 配置化窗口（=5p 的 340）
        int worstFl = firstLethal[myCell];
#if SNIPE_DOOM_NEI
        for (int dd = 0; dd < 4; dd++) {
            int nx0 = players[ME].x + DX[dd], ny0 = players[ME].y + DY[dd];
            if (nx0 < 0 || ny0 < 0 || nx0 >= N || ny0 >= N) continue;
            if (board[ny0][nx0] != '.') continue;
            if (lckBombAt(nx0, ny0)) continue;   // 炸弹占格不算活路
            worstFl = max(worstFl, firstLethal[C(nx0, ny0)]);
        }
#endif
        if (worstFl <= FUSE + 1) {
            for (int i = 0; i < P; i++)
                if (i != ME && players[i].alive && lckBlastHits(players[ME].x, players[ME].y, players[i].x, players[i].y)) {
                    snipeDoomed = true;
                    break;
                }
        }
    }
#endif
    for (int place = 0; place <= 1; place++) {
        bool gateNoPlace = false, gateSelfBomb = false;   // v34k：改为标志位，by 候选做"收人头豁免"
#if ENDGAME_NOPLACE
        // v31r2：终局不在"自身爆风覆盖中心"的位置放雷（避免自己把中列炸热、被迫驱逐）
        if (place && T >= DEATHTv - (13 * EVERY) / 5 && !snipeDoomed) {   // v32：全人数 + 通用几何
            int mx2 = players[ME].x, my2 = players[ME].y;
            bool covers = false;
            for (size_t fc = 0; fc < finalCells.size() && !covers; fc++)
                if (lckBlastHits(mx2, my2, finalCells[fc] % N, finalCells[fc] / N)) covers = true;
            if (covers) gateNoPlace = true;
        }
#endif
#if CTR_SELF_BOMB_BAN
        // v31r5：中心自雷禁令 —— 站在终局中心格时不再放雷
        gateSelfBomb = (place && T >= DEATHTv - (17 * EVERY) / 5 && !snipeDoomed && isFinal(myCell));
#endif
        if (place && (myCnt >= CAP || onBomb)) continue;
        if (place && !placeOK2) continue;
        // v26：place=0 用只读引用绑定预算好的三个世界（原实现在此处各做一次整表深拷贝）；
        //      place=1 才构造"我方新雷"的三个世界；dp 的 place=0 分支复用 deepNormal。
        Pred prStore, ppStore, dpStore;
        if (place) {
            vector<Bm> bs = bombs;
            bs.push_back({ME, players[ME].x, players[ME].y, T + FUSE});
            prStore = predict(bs, H, flames);
            vector<Bm> pb = pessB;
            pb.push_back({ME, players[ME].x, players[ME].y, T + FUSE});
            ppStore = predict(pb, H, flames);
            if (!deepB.empty()) {
                vector<Bm> db = deepAll;
                db.push_back({ME, players[ME].x, players[ME].y, T + FUSE});
                dpStore = predict(db, H, flames);
            }
        }
        const Pred& pr = place ? prStore : normal;
        const Pred& pp = place ? ppStore : pessNormal;
        const Pred* dpPtr = deepB.empty() ? &pp : (place ? &dpStore : &deepNormal);
        const Pred& dp = *dpPtr;
        // v33p：本 place 下"对手自由可达集"一次传播解齐 —— denial / rFree / rivalDoomed 共用。
        // place=0 时 pr==normal，直接复用 obase（本来就不随候选变）。
        vector<int> freeU(P, 0), freeL(P, 0);
        if (place) {
            vector<int> who, src;
            for (int i = 0; i < P; i++)
                if (i != ME && players[i].alive) { who.push_back(i); src.push_back(C(players[i].x, players[i].y)); }
            MultiRes mr;
            reach_multi_core(pr, src, who, nullptr, nullptr, false, mr);
            for (int q = 0; q < (int)who.size(); q++) { freeU[who[q]] = mr.unique[q]; freeL[who[q]] = mr.life[q]; }
        } else {
            freeU = obase; freeL = obaseLife;
        }
        // v34k：place=1 时"我这颗雷是否必收人头" —— 对手在本雷世界里的生命 ≤ FUSE（活不过起爆拍）
        bool anyDoomed = false;
        if (place)
            for (int i = 0; i < P; i++)
                if (i != ME && players[i].alive && freeL[i] <= FUSE) { anyDoomed = true; break; }

        // v18：对手是不是"被我关在门里、活不过我这颗雷的起爆那一拍"。
        bool rivalDoomed = false;
#if LOCK_BOMB_SEAL
        if (place && lockOn && lockCovers) {
            // v32 统一：至少一个对手被封住且活不过起爆拍（2p 时=唯一对手）
            for (int i = 0; i < P; i++) {
                if (i == ME || !players[i].alive) continue;
                if (freeL[i] <= FUSE) { rivalDoomed = true; break; }
            }
        }
#endif

        double offMult = OFF_DUEL_MULT;   // v32：统一（=1.0）
        double denial = 0;
#if MINE_FLEE || ESC_GUIDE
        // v24：放雷撤退 —— 本回合我是否**已经站在自己旧雷的火线里**（只看这一拍之前就存在的雷）。
        int myDangerTurn = 1 << 20;
        for (auto& b : bombs)
            if (b.owner == ME && lckBlastHits(b.x, b.y, players[ME].x, players[ME].y))
                myDangerTurn = min(myDangerTurn, (int)b.at);
#endif
        if (place) {
            denial += 0.5 * blastStatic(players[ME].x, players[ME].y) - 4.0;
#if HARVEST
            // v25c：末圈安全收割 —— 放雷能覆盖对手 + 有活路 → 每个 +HARVEST_W。
            if (P > 2 && T >= SHRINK && shrinkLayer >= HARVEST_LAYER) {
                int cov = 0;
                for (int i = 0; i < P; i++)
                    if (i != ME && players[i].alive &&
                        lckBlastHits(players[ME].x, players[ME].y, players[i].x, players[i].y)) cov++;
                if (cov > 0 && escapeAfterBomb(myCell, HARVEST_ESC)) denial += HARVEST_W * cov;
            }
#endif
#if SEAL_KILL
            // v25f 主动击杀：把对手堵死（他 FUSE 拍内走不出我的火线）且我自己有活路 → 稳杀。
            if (P > 2 && shrinkLayer >= SEAL_MIN_LAYER) {
                int sealed = 0;
                for (int i = 0; i < P; i++)
                    if (i != ME && players[i].alive && rivalSealedByMyBomb(i)) sealed++;
                if (sealed > 0 && escapeAfterBomb(myCell, SEAL_ESC)) denial += SEAL_KILL_W * sealed;
            }
#endif
#if AMBUSH
            // v25g 预判式伏击：对手 3 拍可达集小（被逼住、可预判）且我的火线覆盖它大部分 → 大概率收人头。
            if (P > 2 && shrinkLayer >= AMBUSH_MIN_LAYER && H > RIVAL_STEPS) {
                vector<char> lane;
                blastLaneOf(players[ME].x, players[ME].y, lane);
                int amb = 0;
                for (int i = 0; i < P; i++) {
                    if (i == ME || !players[i].alive) continue;
                    int tot = 0, cov = 0;
                    for (int c = 0; c < N * N; c++)
                        if (rrSeen[i][c]) { tot++; if (lane[c]) cov++; }
                    if (tot > 0 && tot <= AMBUSH_MAXSET && cov * 100 >= AMBUSH_COVPCT * tot) amb++;
                }
                if (amb > 0 && escapeAfterBomb(myCell, AMBUSH_ESC)) denial += AMBUSH_W * amb;
            }
#endif
            for (int i = 0; i < P; i++)
                if (i != ME && players[i].alive && obase[i] > 0) {
                    int rem = freeU[i];
                    if (rem == 0) denial += offMult * killBonus;
                    else denial += offMult * escW * max(0.0, 1.0 - (double)rem / obase[i]);
                }
            for (auto& b : bombs)
                if (b.owner != ME && abs(b.x - players[ME].x) + abs(b.y - players[ME].y) <= RANGE + 1)
                    denial += offMult * chainW;
#if SEEK_RIVAL
            // 开局找对手：这一拍放雷若能烧掉"比我更靠近对手"的箱子 → 每个 +SEEK_OPEN_W（清障开路）。
            // 但只在我"被卡住"（可走邻格 ≤ SEEK_OPEN_STUCK）时才给 —— 避免在开阔区随意炸箱引发死亡。
            if (T < SHRINK && dRiv2[myCell] < 100000000) {
                int openNbrs = 0;
                for (int dd = 0; dd < 4; dd++) {
                    int nx = players[ME].x + DX[dd], ny = players[ME].y + DY[dd];
                    if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
                    if (board[ny][nx] == '.' && !lckBombAt(nx, ny)) openNbrs++;
                }
                if (openNbrs <= SEEK_OPEN_STUCK) {
                    int gain = 0;
                    for (int dd = 0; dd < 4; dd++)
                        for (int r = 1; r <= RANGE; r++) {
                            int nx = players[ME].x + DX[dd] * r, ny = players[ME].y + DY[dd] * r;
                            if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] == '#') break;
                            if (board[ny][nx] == '+') {
                                if (dRiv2[C(nx, ny)] < dRiv2[myCell]) gain++;
                                break;   // 火焰被这个箱子吃掉，后面不用看
                            }
                        }
                    if (gain) denial += SEEK_OPEN_W * gain;
                }
            }
#endif
            if (EVICT_W != 0.0) {
                vector<char> hit(N * N, 0);
                hit[C(players[ME].x, players[ME].y)] = 1;
                for (int d = 0; d < 4; d++)
                    for (int r = 1; r <= RANGE; r++) {
                        int nx = players[ME].x + DX[d] * r, ny = players[ME].y + DY[d] * r;
                        if (nx < 0 || ny < 0 || nx >= N || ny >= N || board[ny][nx] == '#') break;
                        hit[C(nx, ny)] = 1;
                        if (board[ny][nx] == '+') break;
                    }
                for (int i = 0; i < P; i++)
                    if (i != ME && players[i].alive && hit[C(players[i].x, players[i].y)])
                        denial += offMult * EVICT_W * max(0, marginOf(C(players[i].x, players[i].y)));
            }
        }
        // v33p：先按原判据把 5 个方向的实际落点定下来（顺序/跳过条件不变），
        //       再把 opt/ps/dp 的"3 个世界 × 5 个落点"合并成 3 次批量传播（原为 15 次 reach）。
        int destOf[5] = {0, 0, 0, 0, 0};
        bool actD[5] = {false, false, false, false, false};
        for (int d = 0; d < 5; d++) {
            int dest = predictedDest(pr, d);
#if SWAP_FIX
            if (gSwap && d != 4) {
                int rx = players[ME].x + DX[d], ry = players[ME].y + DY[d];
                if (rx >= 0 && ry >= 0 && rx < N && ry < N && rivalAt(rx, ry)) continue;
            }
#endif
#if NO_SWAP
            if (rivalCell >= 0 && dest == rivalCell) continue;
#endif
#if CONTEST
            if (marginOf(myCell) <= 0) {
                int td = terrainDest(pr, d);
                if (td != dest && marginOf(td) > marginOf(myCell)) dest = td;
            }
#endif
            destOf[d] = dest;
            actD[d] = true;
        }
        vector<int> destList, whoMe;
        int slotOf[5] = {-1, -1, -1, -1, -1};
        for (int d = 0; d < 5; d++)
            if (actD[d]) { slotOf[d] = (int)destList.size(); destList.push_back(destOf[d]); whoMe.push_back(ME); }
        MultiRes mOpt, mPs, mDp;
        {
            vector<int> srcMe(1, myCell);
            reach_multi_core(pr, srcMe, whoMe, &destList, nullptr, true, mOpt);
            reach_multi_core(pp, srcMe, whoMe, &destList, &rivalMask, false, mPs);
            if (!deepB.empty()) reach_multi_core(dp, srcMe, whoMe, &destList, &rivalMask, false, mDp);
        }
        for (int d = 0; d < 5; d++) {
            if (!actD[d]) continue;
            const int dest = destOf[d];
            const int q = slotOf[d];
            Reach opt;
            opt.life.fill(0); opt.count.fill(0); opt.util.fill(-1e18);
            for (int t = 0; t < 5; t++) { opt.life[t] = mOpt.life[q]; opt.count[t] = mOpt.unique[q]; }
            opt.unique = mOpt.unique[q];
            opt.minReach = mOpt.minReach[q];
#if FORTRESS_LOCK
            if (!place) {
                if (dest == myCell) fLifeStay = max(fLifeStay, opt.life[d]);
                else fLifeOther = max(fLifeOther, opt.life[d]);
            }
#endif
            Reach ps;
            ps.life.fill(0); ps.count.fill(0); ps.util.fill(-1e18);
            for (int t = 0; t < 5; t++) { ps.life[t] = mPs.life[q]; ps.count[t] = mPs.unique[q]; }
            ps.unique = mPs.unique[q];
            ps.minReach = mPs.minReach[q];
            if (place && (gateNoPlace || gateSelfBomb)) {
                // v34k 收人头豁免：对手必死 且 我在正常/悲观两个世界都能撤出爆风 → 允许放雷
                // （原实现只要"爆风碰到最终格"或"站在最终格"就一律禁雷，属误判威胁）
                if (!(anyDoomed && opt.life[d] >= FUSE + 1 && ps.life[d] >= FUSE + 1)) continue;
            }
            double sc = 10000.0 * opt.life[d] + pessW * ps.life[d];
            if (!deepB.empty()) sc += deepW * mDp.life[q];
            if (opt.count[d] > 0) sc += CNT_W * log(1.0 + opt.count[d]) + POT_W * pot[dest];
            else sc -= 1e6;
            if (!place && d == 4 && dest == myCell && opt.count[d] > 0 && ps.count[d] > 0) staySafe = true;
#if MINE_ESCAPE
            // v17：我这一拍在下雷（雷下在**当前格**）—— 落点必须走出这颗雷的火线。
            if (gEsc && place && dest != myCell && opt.count[d] > 0 &&
                lckBlastHits(players[ME].x, players[ME].y, dest % N, dest / N))
                sc -= MINE_ESCAPE_PEN;
#endif
#if MINE_EXIT
            if (P <= 2 && opt.count[d] > 0)
                for (auto& mb : bombs)
                    if (mb.owner == ME &&
                        lckBlastHits(mb.x, mb.y, players[ME].x, players[ME].y) &&
                        lckBlastHits(mb.x, mb.y, dest % N, dest / N)) { sc -= MINE_EXIT_PEN; break; }
#endif
#if MINE_FLEE
            // v24：放雷撤退 —— 脚下在自己旧雷的火线里 → 落点必须离开火线；原地不动再罚一层。
            // 天然要求"垂直于火力线撤离"（十字爆风下，横向一步就出火线；沿火线走还要再走 3 步）。
            if (myDangerTurn < (1 << 20) && myDangerTurn - T <= MINE_FLEE_LEAD) {
                bool stillIn = false;
                for (auto& b : bombs)
                    if (b.owner == ME && lckBlastHits(b.x, b.y, dest % N, dest / N)) { stillIn = true; break; }
                if (stillIn) {
                    sc -= MINE_FLEE_PEN;      // v32：统一 8k
                    if (dest == myCell) sc -= MINE_FLEE_STAY;   // v32：统一 4k
                }
            }
#endif
#if ESC_GUIDE
            // v24.2 撤离向导：站在"任何雷"的火线里时 ——
            //   ① 落点每远离"安全格"一步扣 GUIDE_W（引导朝最快脱险方向走）；
            //      触发口径用 distSafe[myCell]>0（含**对手**的雷——seed638/663/685 复盘：
            //      自己的雷爆完、只剩对手的雷时，旧口径（只看自己雷）不触发，导致在对手
            //      十字火线里沿轴线横跳/直行，被炸死）。
            //   ② 撤离中**禁止再放雷**（seed657 t=28 的致命第二雷：链爆提前引爆、把撤退目标烧掉）。
            if (distSafe[myCell] > 0)
                sc -= GUIDE_W * (double)min(6, distSafe[dest]);   // v32：统一 4000
            if (place && myDangerTurn < (1 << 20)) sc -= FLEE_NO_BOMB_PEN;
            if (place) {
                int oeC = openNbrsOf(myCell);
                if (oeC <= 2) {
                    bool covNear = false, covFar = false, nearAny = false;
                    for (int ci = 0; ci < P; ++ci) {
                        if (ci == ME || !players[ci].alive) continue;
                        int cd = abs(players[ci].x - players[ME].x) + abs(players[ci].y - players[ME].y);
#if CHOKE_FORM
                        if (cd <= 2) nearAny = true;
#endif
                        if (lckBlastHits(players[ME].x, players[ME].y, players[ci].x, players[ci].y)) {
                            if (cd <= CHOKE_NEAR_MAX) covNear = true;
                            if (cd >= CHOKE_FAR_MIN) covFar = true;
                        }
                    }
#if CHOKE_FORM
                    bool chokeAtk = nearAny && covFar;
#else
                    bool chokeAtk = covNear && covFar;
#endif
                    sc -= chokeAtk ? CHOKE_ATK : CHOKE_PEN;
                }
            }
            if (T >= DEATHTv - (17 * EVERY) / 5 && T < DEATHTv - EVERY && ENDGAME_CTR_W != 0.0) {   // v32
                int cx = (N - 1) / 2, cy = (N - 1) / 2;
                int dc = abs(dest % N - cx) + abs(dest / N - cy);
                int mc = abs(players[ME].x - cx) + abs(players[ME].y - cy);
                sc += ENDGAME_CTR_W * (double)(mc - dc);
            }
#endif
#if STAY_GUARD
            // v24.3 安全哨位：场上无雷 + 两世界 life 一致（无不确定性威胁）+ 对手贴身（≤4）
            //   → 原地不动（place=0 的纯停留）给加成 —— "绝对安全就别多走"（seed655 t=187 复盘）。
            // 只作用于缩圈开始之后（T >= SHRINK）：避开开局/中盘的各种必要机动。
            if (!place && dest == myCell && bombs.empty() && rivalCell >= 0 && T >= SHRINK
                && opt.life[d] == ps.life[d]) {
                int rmd = abs(players[ME].x - rivalCell % N) + abs(players[ME].y - rivalCell / N);
                if (rmd <= STAY_GUARD_RANGE) sc += STAY_GUARD_W;
            }
#endif
#if SEEK_RIVAL
            // v24.2 开局找对手：缩圈前，越靠近对手（箱子可通行口径）分数越高。
            if (T < SHRINK && dRiv2[dest] < 100000000)
                sc -= SEEK_PULL * min(24, dRiv2[dest]);
#endif
#if DEADEND_BAN
            // v17：下了雷之后绝不走进"三面都是墙"的格子（只有一个出口，被雷压住就必死）。
            if (gDead && dest != myCell) {
                bool bombLive = (place != 0);
                if (!bombLive && DEADEND_BAN >= 2)
                    for (auto& bb : bombs) { (void)bb; bombLive = true; break; }
                if (bombLive && wallNbrs(dest % N, dest / N) >= 3) sc -= DEADEND_PEN;
            }
#endif
            if (ps.count[d] > 0) sc += 0.5 * CNT_W * log(1.0 + ps.count[d]);
            sc += denial;
#if ANTI_DEADLOCK
            // v25：多人局也生效；多人局惩罚随连续被挡次数平方增长（该方向常被"存活层级"
            // 高权重锁死，BLOCK_PEN 量级翻不动）。只有连续被挡 >= BLOCK_MP_MINCD 拍、
            // 且当前格距变墙 <= BLOCK_MP_LEAD 回合（真·残局）时才升级，避免中盘扰动轨迹。
            // P<=2 时与 v24c 完全等价。
            if (blockedCd > 0 && !place && d == lastActD)
                sc -= BLOCK_PEN * blockedCd * blockedCd;   // v33a：平方增长（统一单值，避免死锁重复方向）
#endif
#if GATE_SEEK
            if (rivalCell >= 0 && dest != myCell) {
                if (T >= SHRINK - LEAF_LEAD) {
                    int oe = 0;
                    for (int dd = 0; dd < 4; dd++) if (openCell(dest % N + DX[dd], dest / N + DY[dd])) oe++;
                    if (oe <= 1) sc -= LEAF_PEN;
                }
#if LEAF_NEXT
                int nt = (T + 1 <= SHRINK) ? SHRINK
                       : SHRINK + ((T + 1 - SHRINK + EVERY - 1) / EVERY) * EVERY;
                if (nt - T <= LEAF_LEAD) {
                    int nl = 1 + (nt - SHRINK) / EVERY;
                    int surviv = 0;
                    for (int dd = 0; dd < 4; dd++) {
                        int nx = dest % N + DX[dd], ny = dest / N + DY[dd];
                        if (openCell(nx, ny) && edgeD(nx, ny) > nl) surviv++;
                    }
                    if (surviv <= 1) sc -= LEAF_NEXT_PEN;
                }
#endif
            }
#if CROSS_MODE
            if (crossT >= 0) {
                if (dest == crossT) sc += CROSS_W;
                else sc -= CROSS_PULL * min(12, abs(dest % N - crossT % N) + abs(dest / N - crossT / N));
                if (place && crossBomb) sc += CROSS_BOMB_W;
            }
#endif
            if (gateT >= 0) {
                if (dest == gateT) sc += GATE_W;
                else sc -= GATE_PULL * min(12, abs(dest % N - gateT % N) + abs(dest / N - gateT / N));
            }
#endif
            if (pinExit >= 0) {
                if (dest == pinExit) {
                    sc += PIN_W;
                } else if (pinDist <= 1) {
                    sc += PIN_NEAR_W;
                    if (myCell == pinExit && place && opt.count[d] > 0 && opt.life[d] >= 6)
                        sc += PINBOMB_W;
                }
            }
            if (DUEL_GATE_PEN > 0) {
                int e = edgeD(dest % N, dest / N);
                int wallT = (e <= 0) ? -1 : SHRINK + (e - 1) * EVERY;
                if (wallT > T && wallT - T <= DUEL_GATE_LEAD) sc -= DUEL_GATE_PEN;
            }
#if GEN_HOLD
            if ((GEN_HOLD != 0 || P > 2) && dest == myCell && opt.count[d] > 0) {
                int myE = edgeD(players[ME].x, players[ME].y);
                int alv = 0, deep = 0;
                for (int i = 0; i < P; i++) {
                    if (i == ME || !players[i].alive) continue;
                    alv++;
                    if (myE > edgeD(players[i].x, players[i].y)) deep++;
                }
#if GEN_HOLD == 2
                if (alv > 0 && deep * 2 >= alv) sc += HOLD_W;
#else
                if (alv > 0 && deep == alv) sc += HOLD_W;
#endif
            }
#endif
#if CROWD_KEEP
            // 人群局：阈值型"活过下一次缩圈"（见宏注释）。只对"还来得及"的部分用力。
            if (opt.count[d] > 0) {
                const int need = shrinkLayer + 2;
                const int nxt = SHRINK + shrinkLayer * EVERY;
                const int slack = max(0, nxt - T);
                const int myLack = max(0, need - edgeD(players[ME].x, players[ME].y));
                const int dLack = max(0, need - edgeD(dest % N, dest / N));
                sc += CROWD_KEEP_PULL * (myLack - dLack);
                int late = max(0, dLack - slack);
                if (late > 0) sc -= CROWD_KEEP_W * late;
            }
#endif
#if CROWD_CENTER
            // 缩圈到 <= CROWD_CENTER_D（默认 5x5）→ 按"这格还能活到第几回合"重奖更深的位置。
            // 1 层 = 多活 EVERY 回合，所以权重按 10000/回合 口径给（与 life 项同量级）。
            if (P > 2 && openD <= CROWD_CENTER_D && opt.count[d] > 0) {
                int de = edgeD(dest % N, dest / N);
                int wt = (de <= 0) ? T : SHRINK + (de - 1) * EVERY;
                sc += CROWD_CENTER_W * max(0, wt - T);
            }
#endif
            if (shadowT >= 0) {
                if (dest == shadowT) sc += SHADOW_W;
                else sc -= SHADOW_PULL * min(12, abs(dest % N - shadowT % N) + abs(dest / N - shadowT / N));
            }
            if (lockOn) {
                if (!place && d == 4 && dest == myCell && opt.count[d] > 0
                    && (!LOCK_HOLD_PESS || ps.count[d] > 0)) lockHoldOk = true;
                // v18：不再要求"第一步就走出火线"（RANGE 内四个邻格全在火线里，恒假），
                // 改成①我活过起爆拍 ②对手活不过起爆拍 ③悲观世界也活得下去。
                if (lockCovers && place && opt.count[d] > 0 && ps.count[d] > 0
                    && opt.life[d] >= lockBombLife && dest != myCell &&
#if LOCK_BOMB_SEAL
                    rivalDoomed
#else
                    !lckBlastHits(myCell % N, myCell / N, dest % N, dest / N)
#endif
                   ) {
                    if (sc > lockBombSc) { lockBombSc = sc; lockBombAns = {d, place}; lockBombOk = true; }
                }
            }
#if DEADEND_BOMB
            // 用户规则：死角收割 —— 在脚下放雷（相邻对手必被覆盖），
            // 撤到"两个世界都活过起爆拍"的落点（找不到安全落点就不记录 → 不动手）。
            if (deadEndKill && place && dest != myCell
                && opt.life[d] >= lockBombLife && ps.life[d] >= lockBombLife) {
                if (sc > deadEndBombSc) { deadEndBombSc = sc; deadEndBombAns = {d, place}; deadEndBombOk = true; }
            }
#endif
            if ((winW != 0.0 || blockW != 0.0) && !holdWall) {   // v34k5：挡在它内圈线路上时不看封锁项
                // v33p：封锁查询按对手批量（2(P-1) 次 → 1 次传播）；rFree 与落点无关，直接取 freeU
                static vector<char> myMask;
                myMask.assign(N * N, 0);
                myMask[dest] = 1;
                int rl = 0, rFree = 0, rBlocked = 0;
                vector<int> whoB, srcB;
                for (int i = 0; i < P; i++)
                    if (i != ME && players[i].alive) { whoB.push_back(i); srcB.push_back(C(players[i].x, players[i].y)); }
                MultiRes mBlk;
                reach_multi_core(pr, srcB, whoB, nullptr, &myMask, false, mBlk);
                for (int q = 0; q < (int)whoB.size(); q++) {
                    rl = max(rl, mBlk.life[q]);
                    rBlocked += mBlk.unique[q];
                }
                if (blockW != 0.0)
                    for (int i = 0; i < P; i++)
                        if (i != ME && players[i].alive) rFree += freeU[i];
                sc += winW * (opt.life[d] - rl);
                sc += blockW * (rFree - rBlocked);
            }
            if (rivalW != 0.0) {
                int rl = 0;
                for (int i = 0; i < P; i++)
                    if (i != ME && players[i].alive) {
                        Reach rr = reach(pr, i);
                        for (int q = 0; q < 5; q++) rl = max(rl, rr.life[q]);
                    }
                sc -= rivalW * rl;
            }
            if (nearW != 0.0 && rivalBlast[dest]) sc -= nearW;
            sc -= 7.0 * min(60, dGoal[dest]);
            double flMult = DUEL_FL_MULT;   // v32：统一 1.0（原 2p=0）
            sc += flMult * FL_W * min(8, firstLethal[dest]);
            sc += riskMult * MR_W * min(12, opt.minReach);
            if (riskMult > 0 && opt.minReach < MOB_FLOOR)
                sc -= MOB_PEN * (MOB_FLOOR - opt.minReach);
            if (duelMrW != 0.0) sc += duelMrW * min(12, opt.minReach);
            double crowd = 0;
            for (int i = 0; i < P; i++)
                if (i != ME && players[i].alive) {
                    int dd = abs(players[i].x - dest % N) + abs(players[i].y - dest / N);
                    if (dd == 0) crowd += 500;
                    else if (dd == 1) crowd += 30;
                    else if (dd == 2) crowd += 13;
                    else if (dd == 3) crowd += 5;
                }
            sc -= crowdW * crowd;
#if RIVAL_REACH
            // v25e：按"对手 3 拍可达集"给落点定危险度 —— 能被越多对手在 3 拍内踩到的格子越危险；
            // 危险度 0 = 绝对安全格（3 拍内没人能过来）→ 额外加成。
            if (P > 2 && H > RIVAL_STEPS) {
                sc -= RREACH_W * (double)rrCnt[dest];
                if (rrCnt[dest] == 0) sc += ABS_SAFE_W;
            }
#endif
#if CELL_VALUE
            // v25f：落点的"结构价值/危险度" —— 地图固有（出口数、叶子格）+ 缩圈机制（还能站多久）。
            if (shrinkLayer >= LKv - 2) {   // v32：全人数（原 P>2；层位=最终层-2，5p 下=7）
                int e = edgeD(dest % N, dest / N);
                int wt = (e <= 0) ? 0 : SHRINK + (e - 1) * EVERY;   // 该格变墙的回合
                int slack = wt - (T + 1);                            // 还能站几拍
                if (slack > 0) sc += WALL_W * (double)min(WALL_CAP, slack);
                int oe = openNbrsOf(dest);
                sc += EXIT_W * oe;                                   // 出口多 = 不易被堵死
                if (oe <= 1) sc -= LEAF_MP_W;                        // 叶子格重罚
            }
#endif
            if (CORE_W != 0.0 && curLayer >= CORE_MIN_LAYER) sc += CORE_W * max(0, marginOf(dest));
            if (SHRINK_W != 0.0) sc += SHRINK_W * reach(shrinkOnly, ME, dest).life[d];
            {
                int wt = SHRINK + (edgeD(dest % N, dest / N) - 1) * EVERY;
                int slack = wt - (T + 1) - coreDist[dest];
                if (slack < URGE_K) sc -= URGE_W * (URGE_K - slack);
            }
            sc -= visW * visits(dest);
            if (DEPTH_W != 0.0) {
#if DEPTH_GATE
                bool gate = true;
                if (rivalCell >= 0)
                    gate = edgeD(players[ME].x, players[ME].y) < edgeD(rivalCell % N, rivalCell / N);
                if (gate) {
#endif
                int de = edgeD(dest % N, dest / N);
                int dwt = (de <= 0) ? T : SHRINK + (de - 1) * EVERY;
                sc += DEPTH_W * min((double)DEPTH_CAP, (double)(dwt - T));
#if DEPTH_GATE
                }
#endif
            }
            if (oppExpW != 0.0) sc -= oppExpW * min(12, oppExpose[dest]);
#if STAY_MODE == 1
            if (dest == myCell) {
                // v31dev：末段中心格 (9,9) 是 t400 唯一存活格（引擎规则）——反怠工罚不应把持中推走
                bool holdCtr = (T >= DEATHTv - (19 * EVERY) / 5 && isFinal(dest));   // v32
                if (!(HOLD_CTR_EXEMPT && holdCtr)) sc -= STAY_PEN;
                if (HOLD_CTR_BONUS != 0.0 && holdCtr) sc += HOLD_CTR_BONUS;
            }
#else
            if (d == 4) sc -= STAY_PEN;
#endif
            if (holdWall && dest == myCell) sc += 1400.0;
            if (crossPress && dest == myCell) sc += 500.0;   // v35b：十字格压住外圈贴脸对手   // v34k5：隔墙对峙时站住"

#if CHASE_FOLLOW
            // 追身是"覆盖式"决策（和锁门同级）：记下最佳跟随落点，最后统一覆盖。
            // v24.4：另记"同向方向"的最佳（非同一方向要显著领先才允许改道——见覆盖段）。
            if (!place && dest != myCell)
                for (int k2 = 0; k2 < chaseN; k2++)
                    if (d == chaseFollowD[k2] && opt.life[d] >= 6 && ps.life[d] >= 6) {
                        if (sc > chaseSc) { chaseSc = sc; chaseAns = {d, place}; chaseApplied = true; }
                        if (chaseIsSame[k2] && sc > chaseSameSc) {
                            chaseSameSc = sc; chaseSameAns = {d, place}; chaseSameOk = true;
                        }
                        break;
                    }
#endif
            sc -= 0.001 * ((d + T) % 5);
#if AMBIG_ALT_PEN
            // v25b：目的地歧义保护 —— 模型预测"因对手占格而原地不动"，但引擎里对手可能让开，
            // 实际会把 bot 推进 raw target。若 raw target 在 FUSE 回合内会变成火线 → 重罚。
            {
                int alt = terrainDest(pr, d);
                if (alt != dest && firstLethal[alt] <= FUSE) sc -= AMBIG_ALT_W;
            }
#endif
            if (DBG)
                fprintf(stderr, "T=%d place=%d d=%c dest=(%d,%d) lifeOpt=%d lifePess=%d cntO=%d cntP=%d pot=%.1f fl=%d mr=%d dGoal=%d vis=%.2f crowd=%.0f denial=%.1f sc=%.1f\n",
                        T, place, DS[d], dest % N, dest / N, opt.life[d], ps.life[d], opt.count[d], ps.count[d],
                        pot[dest], firstLethal[dest], opt.minReach, min(60, dGoal[dest]), visits(dest), crowd, denial, sc);
            if (sc > best) { best = sc; ans = {d, place}; }
        }
    }
    if (lockOn) {
        if (lockCovers && lockBombOk) ans = lockBombAns;
        else if (lockHoldOk) ans = {4, 0};
    }
#if CHASE_FOLLOW
    // 用户规则：日字追身（拦截对手下一步）——盖过"顶点站桩"（CROSS_HOLD）。
    // v24.3 门槛：追逐落点与全局最佳分的差距 > CHASE_OVER_MARGIN 时不覆盖
    //   （seed633 t=91：S(9,3)=1332262 本该原地卡人，被追身 U=876346 无脑覆盖，差 45.6 万）。
    if (chaseApplied && chaseSc >= best - CHASE_OVER_MARGIN) {
        // v24.4 改道门槛：默认用跟随候选里评分最高者；但若它是"非同向"方向、且"同向方向"只落后
        //   不到 CHASE_FLIP_MARGIN → 回退到同向。依据：seed601 t=59 的 L 仅领先 U 5 分（应保持 U）；
        //   seed619 t=117 的 L 领先 U 1.3 万（应采纳 L）。两者几何完全同型，唯一的可区分量就是分差。
        if (chaseSameOk && chaseSameAns != chaseAns && chaseSameSc >= chaseSc - CHASE_FLIP_MARGIN)
            ans = chaseSameAns;
        else
            ans = chaseAns;
    }
#endif
#if CROSS_MODE && CROSS_HOLD
    // 站在十字点上保持不动 —— 前提是"主动躲炸弹"：
    //   CROSS_HOLD=2 时，只有在"正常预测和悲观预测里都活得下去"时才强制 STAY；
    //   一旦格子受威胁，就不覆盖，交给正常评分把我挪开（躲完 CROSS_PULL 会拉回来）。
    if (!lockOn && crossT >= 0 && myCell == crossT
#if CHASE_FOLLOW
        && !chaseApplied
#endif
#if CROSS_HOLD == 2
        && staySafe
#endif
       ) ans = {4, 0};
#endif
#if DEADEND_BOMB
    // 用户规则优先级最高：死角必收（脚下放雷 + 撤出火线；找不到安全落点则不动手）。
    if (deadEndBombOk) ans = deadEndBombAns;
#endif
#if ANTI_DEADLOCK
    lastActD = ans.first;
#endif
#if CLAIM_CTR
    // v31r2：终局"中心申报"——相邻且安全时无条件申报进中心（申报无副作用，可跟进腾空格/拦截他人）
    if (!deadEndBombOk && !lockOn && T >= DEATHTv - (16 * EVERY) / 5) {   // v32：全人数 + 通用几何
        int tgt = -1, tdd = 1 << 20;
        for (size_t fc = 0; fc < finalCells.size(); fc++) {
            int dd = abs(finalCells[fc] % N - players[ME].x) + abs(finalCells[fc] / N - players[ME].y);
            if (dd < tdd) { tdd = dd; tgt = finalCells[fc]; }
        }
        if (tgt >= 0 && tdd == 1) {
            bool ctrOcc = false;
            for (int qi = 0; qi < P; qi++)
                if (players[qi].alive && players[qi].x == tgt % N && players[qi].y == tgt / N) ctrOcc = true;
            if (firstLethal[tgt] >= CLAIM_CTR_FL && firstLethal[myCell] >= CLAIM_CTR_MYFL
                && (!CLAIM_CTR_OCC || ctrOcc)) {
                int cdx = tgt % N - players[ME].x, cdy = tgt / N - players[ME].y;
                int cdir = (cdx == 1) ? 3 : (cdx == -1) ? 2 : (cdy == 1) ? 1 : 0;
                ans = {cdir, 0};
            }
        }
    }
#endif
#if FORTRESS_LOCK
    // v31r5：终局中心堡垒锁（见文件头说明）。仅在"留下不损失生存"时锁定。
    if (T >= DEATHTv - (17 * EVERY) / 5 && T < DEATHTv - EVERY) {   // v32：全人数 + 通用几何
        if (finalCells.size() == 1 && isFinal(myCell) && fLifeStay >= 0)   // v32：仅"唯一终局格"才锁（环带无唯一格→不锁）
            if (fLifeStay + FORTRESS_SLACK >= fLifeOther) ans = {4, 0};
    }
#endif
    if (DBG) fprintf(stderr, "  -> chose %c place=%d\n", DS[ans.first], ans.second);
    recent.push_back(C(players[ME].x, players[ME].y));
    if (recent.size() > 40) recent.erase(recent.begin());
    // v34k7：最高优先级终局规则（用户定）——
    //   条件：剩余空格 **恰好 5 格** 且 我站在 **地图正中心格**（不是"度数>=3"、不是 <=5）。
    //   动作：立刻下雷；之后每拍原地不动（已有雷/满容量时只原地），直到对局结束。
    {
        // 用"本拍缩圈之后"的棋盘来数：引擎在本拍结束时把 edgeD <= nl 的环带变墙，
        // 这样"5 格"出现的那一拍就能立刻落雷（不必等下一拍，实战录像第 256 帧就是这一拍）。
        int nl = (T < SHRINK) ? 0 : 1 + (T - SHRINK) / EVERY;
        int openCnt = 0;
        for (int c = 0; c < N * N; c++) {
            if (board[c / N][c % N] != '.') continue;
            if (nl > 0 && edgeD(c % N, c / N) <= nl) continue;   // 本拍结束会变墙
            openCnt++;
        }
        const int ctr = C((N - 1) / 2, (N - 1) / 2);
        if (openCnt == 5 && myCell == ctr && players[ME].alive && (nl == 0 || edgeD(players[ME].x, players[ME].y) > nl)) {
            ans = {4, (onBomb || myCnt >= CAP) ? 0 : 1};
            if (DBG) fprintf(stderr, "EH t=%d open=5 center -> place=%d\n", T, ans.second);
        }
    }
    return ans;
}

bool readTurn() {
    string w;
    if (!(cin >> w >> T)) return false;
    cin >> w;
    for (int y = 0; y < N; y++) cin >> board[y];
    for (int i = 0; i < P; i++) {
        int id; cin >> w >> id >> players[id].x >> players[id].y >> players[id].alive;
    }
    int k; cin >> w >> k; bombs.resize(k);
    for (int i = 0; i < k; i++) cin >> w >> bombs[i].owner >> bombs[i].x >> bombs[i].y >> bombs[i].at;
    cin >> w >> k; flames.resize(k);
    for (int i = 0; i < k; i++) cin >> w >> flames[i].x >> flames[i].y >> flames[i].end;
    while (cin >> w) { if (w == "END") return true; string line; getline(cin, line); }
    return false;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false); cin.tie(nullptr);
    if (argc > 1 && string(argv[1]) == "--debug") DBG = true;
    string w;
    if (!(cin >> w >> N >> P >> ME >> LIMIT >> FUSE >> RANGE >> CAP >> FIRE >> SHRINK >> EVERY)) return 0;
    {   // v32：全人数统一 —— 由配置推导终局几何/窗位（非按人数设置）
        int c0 = (N - 1) / 2;
        LKv = ((c0 % 2) == 1) ? c0 : c0 - 1;
        DEATHTv = SHRINK + (LKv - 1) * EVERY;
        finalCells.clear();
        for (int c = 0; c < N * N; c++) {
            int x0 = c % N, y0 = c / N;
            if ((x0 % 2) == 0 && (y0 % 2) == 0) continue;   // 永久墙（棋盘柱）排除
            if (isFinal(c)) finalCells.push_back(c);
        }
    }
    board.resize(N); players.resize(P);
    for (int i = 0; i < 16; i++) prevX[i] = prevY[i] = -1;
    while (readTurn()) {
        auto [d, pl] = decide();
        cout << T << ' ' << DS[d] << ' ' << pl << '\n' << flush;
        for (int i = 0; i < P; i++) { prevX[i] = players[i].x; prevY[i] = players[i].y; }
    }
    return 0;
}
