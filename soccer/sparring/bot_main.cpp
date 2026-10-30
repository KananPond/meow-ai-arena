// bot_main.cpp —— 把 bots.hpp 里的一个 bot 包成按协议读写的独立程序。编译时用 -DBOT_NAME="\"pro_b0\"" 指定。
#include "bots.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#ifndef BOT_NAME
#define BOT_NAME "plan16"
#endif

int main() {
    std::ios::sync_with_stdio(false);
    std::string magic;
    long long ver;
    if (!(std::cin >> magic >> ver)) return 0;
    soccer::Config c;
    long long msg_max, idx, step_ms, first_ms;
    std::cin >> c.w >> c.h >> c.goal >> c.chamfer >> c.rp >> c.rb >> c.acc_max >> c.pf_n >> c.pf_d >> c.bf_n >> c.bf_d >>
        c.kick_max >> c.kick_reach >> c.kick_cooldown >> c.e_wall_n >> c.e_wall_d >> c.e_ball_n >> c.e_ball_d >> c.e_pp_n >>
        c.e_pp_d >> c.steps >> c.stall >> msg_max >> idx >> step_ms >> first_ms;
    c.msg_max = msg_max;
    std::unique_ptr<sbot::Bot> bot(sbot::make_bot(BOT_NAME, c));
    if (!bot) return 1;
    soccer::Obs o;
    o.idx = int(idx);
    char buf[64];
    while (std::cin >> o.t >> o.left >> o.my_score >> o.opp_score >> o.mate_msg >> o.bx >> o.by >> o.bvx >> o.bvy) {
        for (auto& p : o.pl) std::cin >> p.x >> p.y >> p.vx >> p.vy >> p.cd;
        const sbot::Act a = bot->act(o);
        std::snprintf(buf, sizeof buf, "%lld %lld %lld %lld %lld\n", a.ax, a.ay, a.kx, a.ky, a.msg);
        std::fputs(buf, stdout);
        std::fflush(stdout);
    }
    return 0;
}
