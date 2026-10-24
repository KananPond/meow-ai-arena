> English translation for reference; the contestants received the original Chinese text.

[简体中文](prompt.md)

# Original prompts

These are translations of the exact messages sent at the start of each round. The corresponding archive was already in the folder; detailed instructions are in `workspace/TASK.md`, `packages/round2/review/REVIEW.md`, and `packages/round3/TAKEOVER.md` plus `CHANGE.md`.

- Braced placeholders were populated with the draw and prior results as described below. Historical data is in `results/历代战绩.json`. Placeholder names and actual filenames are retained for reference.
- Round 1 used “attack/defense”, “attacker”, and “break through”. **Rounds 2 and 3 below are rematch versions**, using “tester”, “regression testing”, and “counterexample”. The first versions are absent; safety blocks, missing subagent prohibitions, and unclear Round 3 priorities are described in [conditions](CONDITIONS_en.md).
- Only the messages below were recovered during open-source preparation. GLM and DeepSeek lead-programmer retrospectives were supplied as `赛后复盘.md` to read; the short pasted instruction was not recovered.

## Pre-match boast

After the draw, before play, each model received only this message in a fresh conversation; only its first reply was used. Populate `{队伍}`, `{角色}`, `{队友}` from the draw. `{你的战绩}` and `{队友战绩}` contain ranks/scores from the first 4 episodes, or “new this episode; did not enter the first 4”. `{全员战绩表}` is the table of all 12 models' prior results.

````text
You are about to enter an AI team coding competition. The task is “take over a slow, messy legacy system and make it faster while keeping its output byte-identical.”

Format: 12 models in 6 teams, each with a lead programmer and an attacker.
- The lead programmer first spends 30 minutes optimizing the code;
- The attacker receives the other 5 teams' lead submissions and constructs inputs to break them;
- The attacker then takes over their own lead programmer's code to implement a secret requirements change.

Draw: you are {角色} for {队伍}, and your teammate is {队友}.

Your record in the first 4 episodes: {你的战绩}
Your teammate {队友}'s record: {队友战绩}

Records of all 12 contestants (rank, with score in parentheses; 9 contestants in Episode 1, 9 in Episode 2, 11 in Episode 3, 7 in Episode 4):

{全员战绩表}

The camera is on you. Deliver a **single-sentence boast of no more than 50 Chinese characters** to your teammate, opponents, or the entire field. Show personality: bold or humorous is fine, but no personal attacks. Output only that sentence.
````

## Round 1 — lead programmer

Populate team, teammate, and records as above; `{对手攻手战绩}` lists the other 5 attackers, one per line with their records in parentheses.

````text
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
````

Populated example for Team A's Kimi K3 (only team, teammate, and records differ across the 6 lead prompts):

````text
This is an AI team coding competition. You are the **lead programmer** for Team A; your teammate (attacker) is Sonnet 5.5.

Your record in the first 4 episodes: Episode 1, Land Grab: 6th (16.7); Episode 2, Bomberman: 5th (230.4); Episode 3, Capture the Flag: 9th (−35); Episode 4, Diplomacy: did not participate
Your teammate's record: Episode 1, Land Grab: 1st (100, champion); Episode 2, Bomberman: did not participate; Episode 3, Capture the Flag: 4th (203); Episode 4, Diplomacy: 5th (19.4)
The 5 attackers who will attack your code after submission:
- Fable 5.1 (Episode 1, Land Grab: did not participate; Episode 2, Bomberman: did not participate; Episode 3, Capture the Flag: 3rd (213, earliest submission); Episode 4, Diplomacy: 1st (161.4, solo victory in game 3))
- GPT-6.1 Sol (Episode 1, Land Grab: predecessor GPT-6 Sol, 4th (39.4); Episode 2, Bomberman: predecessor GPT-6 Sol, 3rd (301.9); Episode 3, Capture the Flag: tied 1st (252); Episode 4, Diplomacy: 6th (16.4))
- Opus 5.5 (Episode 1, Land Grab: 3rd (79.4, tied 2nd/3rd with Astra); Episode 2, Bomberman: 2nd (321.8); Episode 3, Capture the Flag: 5th (196); Episode 4, Diplomacy: 2nd (55.3))
- Grok 4.7 (new this episode; did not enter the first 4)
- GPT-6 Astra (Episode 1, Land Grab: 2nd (81.7, tied 2nd/3rd with Opus); Episode 2, Bomberman: 1st (488.6, 2.6 times the second-place score); Episode 3, Capture the Flag: tied 1st (254); Episode 4, Diplomacy: 4th (21.2))

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
````

## Round 2 — regression testing (rematch)

All 6 testers used the same version. Fill `{队伍}` with the team and `{队友}` with its lead programmer.

````text
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
````

## Round 3 — takeover and optimization (rematch)

New conversation and folder; team and teammate as above. Populate `{被击穿情况}` from Round 2 verdicts:

- No counterexample found (Team A only): “No tester found a counterexample to your teammate's code in the previous round.”
- Other teams: “N testers found counterexamples to your teammate's code (Team X's tester …). Those cases are in `workloads2/regression/` in the takeover package.”

````text
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
````

## Blind review

Sent to GPT-6 Astra, Opus 5.5, and DeepSeek V4.1 Flash in separate fresh conversations. Each folder held 12 anonymous submissions, two anchors, `CHANGE.md`, a review order, and `packages/blind-review/评审说明_同一对话版.md`.

````text
Please read `评审说明.md` in the current directory and follow it exactly to review the maintainability of all 12 submissions (`代码XXX/src/`) in this single conversation.

- Follow `评审顺序.txt`. Read each submission fully, recording provisional scores and evidence in `草稿.md` before moving on. After all 12, compare them, revise inconsistent scores, write each `代码XXX/评分.json`, then `总览.md`;
- Read only this folder; no network access, compilation, execution, or changes to any `src/`;
- Support every dimension using that submission's own files and code;
- No help from other models, including subagents; do everything yourself in this conversation;
- Do not ask questions; complete the work directly.
````

## Follow-up: 100-point impressions

Asked in the same conversation after review; displayed only, not scored.

````text
The review is complete. Please do one more thing: disregard the five dimension scores and assign each submission a maintainability score from 1 to 100 based on your overall judgment after reading all 12.

- Calibration: `锚点/屎山基线` is 10; `锚点/干净参考` is 75; 100 means code you would willingly take over and maintain long term;
- Still judge only maintainability, not speed or correctness. Ties are fine; no forced separation;
- Give one sentence explaining the main reason for each score;
- Write `百分制.json` in the current folder, covering all 12, in this format: {"代码185": {"分": 62, "理由": "…"}, …};
- Do not modify the existing `评分.json` or `总览.md`.
````

## Final retest

Additional unscored post-contest task, sent to GPT-6 Astra and Opus 5.5 in fresh conversations; package instructions are `packages/retest/REVIEW.md`.

````text
This is an additional post-competition task: regression-test the teams' final submissions.

The current directory contains `终版复测包.zip` (use `review` directly if extracted). Extract it, enter `review`, read `REVIEW.md`, and independently follow it exactly: write regression tests for every final submission under `submissions/`, finding inputs that disagree with `reference/`.

Key points (see `REVIEW.md`):
- Put at most 3 counterexamples per final submission in `cases/<finalN>/`, explaining each defect and its cause in `cases/<finalN>/说明.txt`; test locally with `tools/try_case.py`;
- Disagreement means different output, abnormal exit, or failure to finish within 60 seconds;
- Count independent defects per final submission. Some may truly yield no counterexamples: report that honestly and describe what you checked. Do not submit cases not confirmed locally;
- Hard limit: 45 minutes, then forced stop; no early-finish bonus. Use `python clock.py` in `review`;
- Read/write only this folder. No online searches or help from other models, including subagents, Task / Agent tools, or background parallel agents;
- Other programs run on this machine. Terminate only your own processes by PID, never in bulk by name;
- Do not ask questions or wait for confirmation; continue until finished.

When finished, describe every defect (which final submission, location, and cause), and which submissions yielded no counterexample and what you checked.
````

## Post-match retrospective

Sent to all 12 models after results. Testers continued their Round 3 conversations. Lead programmers' Round 1 sessions were several days old, so fresh conversations were planned, with their original handoffs appended (GLM's first attempt used the original session; see [notes](notes/README_en.md)). `referee/复盘.py` generated prompts from scores.

`{你的成绩}` summarizes that model's rank, speedup, defects/testers, and final correctness. `{赛前狠话原文}` is its own boast. `{团队成绩表}` is the full `results/团队成绩.md`; `{缺陷点表}` is the 7-defect table, with attacker/attack/breakthrough wording replaced by tester/counterexample wording. `{交接文档}` is the first 6000 Chinese characters of its Round 1 `HANDOFF.md`, with the same substitutions. All receive this `{复盘要求}`:

````text
Give a brief retrospective, **no more than 200 Chinese characters total**:
1. Your best decision;
2. Your biggest mistake and its cause;
3. Respond to your pre-match boast;
4. Say one thing to your teammate or an opponent.

Do not modify files or run code again; answer directly in text.
````

For lead programmers:

````text
A few days ago you entered an AI team coding competition as {队伍}'s lead programmer: optimize a slow, messy warehouse simulator within 45 minutes while preserving byte-identical baseline output. Other teams' testers then sought counterexamples; your teammate took over your code to implement a change and optimize further. That conversation has ended. Your original handoff is appended to refresh your memory.

The competition is over and results are available (full scores and defect list below).

Your results: {你的成绩}

Your pre-match boast: “{赛前狠话原文}”

{复盘要求}

{团队成绩表}

{缺陷点表}

## Your original handoff (excerpt from your submission)

{交接文档}
````

For testers:

````text
The competition is over and results are available (full scores and defect list below). You took part in two rounds: regression-testing other teams' lead submissions for counterexamples, then taking over your teammate's code to implement the change and optimize it.

Your results: {你的成绩}

Your pre-match boast: “{赛前狠话原文}”

{复盘要求}

{团队成绩表}

{缺陷点表}
````
