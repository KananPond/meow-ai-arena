> English translation for reference; the contestants received the original Chinese text.

[简体中文](1_主程.md)

# Prompt 1: lead programmer (Round 1 — build)

Recipients: the 6 lower-tier lead programmer models.
Usage: command-line seats receive the task automatically through `python 裁判/开跑.py <队>`. For manual seats, first run `python 裁判/开局.py <队> 准备`, open the model's agent tool in `C:\arena5\第1轮 <队> 主程 <模型>`, paste `提示词.txt` **verbatim**, and run `python 裁判/开局.py <队> 开始` at the same time. Generate populated prompts with `python 裁判/填写提示词.py 主程`.

---

This is an AI team coding competition. You are the **lead programmer** for {队伍}; your teammate (attacker) is {队友}.

Your record in the first 4 episodes: {你的战绩}
Your teammate's record: {队友战绩}
The 5 attackers who will attack your code after submission:
{对手攻手战绩}

The current directory contains `workspace.zip` (use the `workspace` folder directly if already extracted). Extract it yourself, enter `workspace`, read `TASK.md`, and independently complete the task exactly as specified: you are taking over a messy legacy system and must make the simulation in `src/` as fast as possible while keeping its output byte-identical to the baseline.

Key points (see `TASK.md`):
- Modify only `src/`; the fixed compilation command is `g++ -O2 -std=c++20 src/*.cpp`, using only the C++ standard library;
- Output must match the baseline byte for byte on every legal input, as defined by `tools/check_input.py --full`; `SPEC.md` is outdated, and actual baseline behavior is authoritative;
- After submission, the 5 attackers above will construct inputs specifically to break your code; your teammate will then take it over to implement a secret requirements change;
- Before submitting, write a handoff document for your teammate at `src/HANDOFF.md`;
- Budget about 30 minutes and manage your own progress. Run `python clock.py` in `workspace` to check elapsed time; you will be forcibly stopped after 45 minutes;
- Read and write only within the current folder. All local tools are allowed, but online searches are forbidden;
- Do not ask me questions or wait for confirmation; continue until finished.

When finished, give the full path of `src` and briefly summarize your changes.

---
