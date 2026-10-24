// 模板：完整读入每回合的局面，然后让自己的 5 个英雄原地不动。
// 解析部分可以直接沿用，只需改写 decide()。协议细节见 RULES.md。
#include <bits/stdc++.h>
using namespace std;

struct ClassInfo { int hp, atk, rng, cd; string pos, cls, skill; };
struct Tower { int id, team, lane, tier, x, y, maxhp, hp, atk; };
struct Nexus { int id, team, x, y, maxhp, hp; };
struct Camp { int id, x, y, maxhp, atk, hp, resp; };
struct Minion { int id, team, lane, x, y, hp; };
struct Hero { int id, team, x, y, hp, maxhp, level, xp, cd, respawn; };   // 阵亡时 x=y=-1

struct Game {
    // 开局信息（INIT 块）
    int teams = 0, me = 0, S = 0, turns = 0, diff = 0, waveSize = 0, waveGap = 0, minionHp = 0, minionAtk = 0,
        towerRange = 0, respawn = 0, regen = 0, fountain = 0, minionCap = 0, aggro = 0, npcHeroes = 0;
    vector<string> mapRows;              // mapRows[y][x]：'#' 墙，'.' 空地
    vector<ClassInfo> cls;               // 5 个位置的职业
    vector<int> levelXp;                 // 升到 2、3、… 级各需要的经验
    vector<pair<int, int>> spawns;       // 10 个出生点：0 号队 5 个，1 号队 5 个
    vector<array<int, 4>> bases;         // 各队基地矩形 x0 y0 x1 y1
    vector<Tower> towers;
    vector<Nexus> nexuses;
    vector<Camp> camps;
    // 每回合更新（TURN 块）
    int turn = 0;
    vector<Minion> minions;
    vector<Hero> heroes;
    vector<string> events;               // 上一回合的事件原行，格式见 RULES.md
};

bool read_block(Game& g) {
    string w;
    int k;
    while (cin >> w) {
        if (w == "INIT") {
            cin >> g.teams >> g.me >> g.S >> g.turns >> g.diff >> g.waveSize >> g.waveGap >> g.minionHp
                >> g.minionAtk >> g.towerRange >> g.respawn >> g.regen >> g.fountain >> g.minionCap
                >> g.aggro >> g.npcHeroes;
            cin >> w;                        // MAP
            g.mapRows.assign(g.S, "");
            for (auto& row : g.mapRows) cin >> row;
            cin >> w >> k;                   // CLASSES 5
            g.cls.assign(k, {});
            for (auto& c : g.cls) { string tag; int idx; cin >> tag >> idx >> c.hp >> c.atk >> c.rng >> c.cd
                                         >> c.pos >> c.cls >> c.skill; }
            cin >> w >> k; g.levelXp.resize(k);
            for (auto& v : g.levelXp) cin >> v;
            cin >> w >> k; g.spawns.resize(k);
            for (auto& s : g.spawns) { string tag; cin >> tag >> s.first >> s.second; }
            cin >> w >> k; g.bases.resize(k);
            for (auto& b : g.bases) { string tag; cin >> tag >> b[0] >> b[1] >> b[2] >> b[3]; }
            cin >> w >> k; g.towers.resize(k);
            for (auto& t : g.towers) { string tag; cin >> tag >> t.id >> t.team >> t.lane >> t.tier >> t.x >> t.y >> t.maxhp >> t.atk; }
            cin >> w >> k; g.nexuses.resize(k);
            for (auto& n : g.nexuses) { string tag; cin >> tag >> n.id >> n.team >> n.x >> n.y >> n.maxhp; }
            cin >> w >> k; g.camps.resize(k);
            for (auto& c : g.camps) cin >> w >> c.id >> c.x >> c.y >> c.maxhp >> c.atk >> w;
            continue;                        // INIT 之后同一块里还有 TURN，继续读
        }
        // TURN 块
        if (w != "TURN") return false;       // 块首一定是 TURN
        cin >> g.turn;
        cin >> w;                            // CRYSTAL
        for (auto& n : g.nexuses) cin >> n.hp;
        cin >> w >> k;                       // TOWERS n
        for (int i = 0; i < k; i++) { int id, hp; string tag; cin >> tag >> id >> hp;
            for (auto& t : g.towers) if (t.id == id) t.hp = hp; }
        cin >> w >> k;                       // CAMPS n
        for (int i = 0; i < k; i++) { int id, hp, resp; string tag; cin >> tag >> id >> hp >> resp;
            for (auto& c : g.camps) if (c.id == id) { c.hp = hp; c.resp = resp; } }
        cin >> w >> k; g.minions.resize(k);
        for (auto& m : g.minions) { string tag; cin >> tag >> m.id >> m.team >> m.lane >> m.x >> m.y >> m.hp; }
        cin >> w >> k; g.heroes.resize(k);
        for (auto& h : g.heroes) { string tag; cin >> tag >> h.id >> h.team >> h.x >> h.y >> h.hp >> h.maxhp
                                         >> h.level >> h.xp >> h.cd >> h.respawn; }
        cin >> w >> k; getline(cin, w);      // EVENTS n，读完行尾
        g.events.resize(k);
        for (auto& e : g.events) getline(cin, e);
        cin >> w;                            // END
        return true;
    }
    return false;
}

// ---------- 在这里写你的策略：给 5 个英雄各决定一个移动和一个动作 ----------
// 移动：U 上 / D 下 / L 左 / R 右 / S 原地；动作：'-' 不做事 / 'Q' 技能 / 数字 = 普攻该 id
void decide(Game& g, vector<pair<string, string>>& out) {
    for (int i = 0; i < 5; i++) out.push_back({"S", "-"});
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    Game g;
    while (read_block(g)) {
        vector<pair<string, string>> out;
        decide(g, out);
        cout << g.turn;
        for (auto& [mv, act] : out) cout << ' ' << mv << ' ' << act;
        cout << endl;
    }
}
