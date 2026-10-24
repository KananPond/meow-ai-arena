> English translation for reference; the contestants received the original Chinese text.

[简体中文](3_攻手_接手.md)

# Prompt 3: attacker (Round 3 — takeover), rematch version (2026-10-09)

Recipient: the same attacker model, in **a new conversation and a new folder**, after the administrator has run `击穿判定.py` and `生成接手包.py`.
Reason: the Round 2 conversation contains the other 5 teams' lead code, which could otherwise be copied directly from the fastest submission. The new folder contains only the model's teammate's code.
Usage: `python 裁判/开局.py <队> 准备 --round 3` places the archive and populated `提示词.txt` in `C:\arena5\第3轮重赛 <队> 测试手 <模型>`. Paste it in a new conversation and backfill the start timestamp from the session record. **Start all 6 in the same batch.**

Changes from the first attempt (archived under the organizer's `提示词/第一次_存档/`):
- The first wording said “optimize if time permits” and “you may finish when done”, without weights; some models stopped after 1–10 minutes. This round was intended to test optimization, so the new version states that priority, the speed formula, and the lack of an early-finish bonus.
- Added 3 workloads near official scale and `bench2.py --big` with relative speedups. The first package had only small workloads totaling less than 1 second, insufficient to distinguish speed.
- Added value now directly measures final speedup over the original lead submission on the same data.

---

This is an AI team coding competition. You are the **tester** for {队伍}; your teammate (lead programmer) is {队友}.

Background: each lead programmer spent 30 minutes optimizing a slow, messy warehouse scheduling simulator while preserving byte-identical baseline output. Testers then performed regression testing on the other teams' lead submissions. That round has ended. {被击穿情况}

This is Round 3: **take over and optimize**. The current directory contains `接手包_*.zip` (use `workspace` directly if already extracted). Extract it yourself to obtain `workspace`, containing teammate {队友}'s submitted code (`src/`, including `src/HANDOFF.md`), the requirements change `CHANGE.md`, and your team's regression cases from the previous round. Enter `workspace`, read `TAKEOVER.md`, and independently complete the task exactly as specified.

**This round primarily tests optimization ability**: fixing the regression defects and implementing the change are prerequisites; the main scoring factor is the changed program's speed on large workloads.

Key points (see `TAKEOVER.md`):
- You may freely modify `src/`, make major changes, rewrite it, or restart from `baseline/`;
- `python tools/bench2.py` checks correctness on small, edge, and regression cases; `python tools/bench2.py --big` checks speed on 3 workloads near official scale and reports speedup over the reference;
- **Speed scores compare against the fastest submission using a logarithmic scale**. A workload with wrong output earns 0 speed points. Implementing the change correctly without optimization earns nearly 0 speed-related points;
- **The hard limit is 45 minutes; you will be forcibly stopped. Finishing early earns no bonus.** Get fixes and changes correct promptly, optimize for the remaining time, and reserve the final 5 minutes for verification. Run `python clock.py` in `workspace` to check elapsed time;
- Whatever is in `src/` at the deadline is submitted. Avoid leaving it uncompilable or producing incorrect output for long;
- Read and write only within the current folder. You may use local tools to read/write files and run commands, but **no online searches and no help from other models**, including subagents, Task / Agent tools, or background parallel agents. Complete all work yourself in this conversation;
- Other programs are running on this machine. Terminate only processes you started yourself, by PID; **do not terminate processes in bulk by name**, such as `taskkill /IM python.exe` or `Stop-Process -Name python`;
- Do not ask me questions or wait for confirmation; continue until finished.

When finished, give the full path of `workspace/src`, the three `--big` speedup ratios, and a brief explanation of your changes and optimizations.

---
