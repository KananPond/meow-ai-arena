> English translation for reference; the contestants received the original Chinese text.

[简体中文](2_攻手_攻防.md)

# Prompt 2: attacker (Round 2 — regression testing), rematch version (2026-10-09)

Recipients: the 6 higher-tier attacker models, after all lead submissions have been collected and the administrator has run `python 裁判/生成攻防包.py` to create `赛事/互测包/互测包_<队>.zip`.
Usage: `python 裁判/开局.py <队> 准备 --round 2` places the archive and populated `提示词.txt` in `C:\arena5\第2轮重赛 <队> 测试手 <模型>`. Start a **new conversation** there and paste the prompt; backfill the start time from its timestamp in the session record. Round 3 uses **another new conversation and folder**.

Changes from the first attempt:
- **All 6 use the same version** (GPT originally had its own). Terms are standardized to regression testing, equivalence verification, counterexamples, and disagreement, which neither Claude nor OpenAI safety mechanisms block. Avoid “攻防、攻击、击穿、hack、attack、target、对手、崩溃”.
- Scoring explicitly counts independent defects per implementation rather than inputs. Rarer discoveries earn more; the own-team component uses other testers' mean scores. Each `说明.txt` must explain the defect.
- Failure to finish in 60 seconds explicitly counts as disagreement, without discouraging or leading wording.
- Finishing early explicitly earns no bonus.
- The first version was retained in the organizer's `提示词/第一次_存档/`.

---

This is an AI team coding competition. You are the **tester** for {队伍}; your teammate (lead programmer) is {队友}.

Background: each team's lead programmer has just spent 30 minutes optimizing a slow, messy warehouse scheduling simulator, with output required to remain byte-identical to the original baseline. We now need equivalence testing: **write regression tests for the other 5 optimized implementations and find inputs on which they disagree with the reference implementation (baseline)**.

The current directory contains `互测包_*.zip` (use `review` directly if already extracted). Extract it yourself, enter `review`, read `REVIEW.md`, and independently complete the task exactly as specified.

Key points (see `REVIEW.md`):
- The 5 implementations are under `submissions/`. Put counterexamples in `cases/<队名>/`, at most 3 per implementation; save each as soon as you find it and explain the defect and its cause in one or two lines in `cases/<队名>/说明.txt`;
- Cases must pass `tools/check_input.py`, and the reference must finish within 10 seconds. Test locally with `tools/try_case.py`;
- Three outcomes count as disagreement: different output, abnormal exit, or failure to finish within 60 seconds;
- **Scores count defects, not inputs**: 3 independent defects in one implementation count as 3 points; 3 triggers for one defect count as 1 (classified by root cause after the competition). **Defects found by fewer testers earn more**: one found by everyone earns the base value, while one found only by you earns nearly twice that. Reserve the 3 slots for distinct defects where possible, and do not stop after finding one counterexample per implementation;
- **The hard limit is 45 minutes; you will be forcibly stopped. Finishing early earns no bonus.** Run `python clock.py` in `review` to check elapsed time;
- Another round follows; its task will be announced then;
- Read and write only within the current folder. You may use local tools to read/write files and run commands, but **no online searches and no help from other models**, including subagents, Task / Agent tools, or background parallel agents. Complete all work yourself in this conversation;
- Other programs are running on this machine. Terminate only processes you started yourself, by PID; **do not terminate processes in bulk by name**, such as `taskkill /IM python.exe` or `Stop-Process -Name python`;
- Do not ask me questions or wait for confirmation; continue until finished.

When finished, describe every defect you found: the implementation, the defect's location, and why it disagrees with the reference.

---
