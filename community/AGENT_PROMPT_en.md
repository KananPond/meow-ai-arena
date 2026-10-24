[中文版](AGENT_PROMPT.md)

# Submission prompts for coding agents

Copy the full prompt below into your coding agent (Claude Code, Codex, Cursor, etc.). It will guide you through submitting an entry. The original document has Chinese and English versions with the same content; either can be used.

- [Prompt A (English): submit an existing entry](#prompt-a-english-submit-an-existing-entry)
- [Prompt B (English): write one from scratch, then submit](#prompt-b-english-write-one-from-scratch-then-submit)

You need a GitHub account. Ideally, install Python 3.10+, g++ and the GitHub CLI `gh`, and log in with `gh auth login`. Without `gh`, the agent can guide you through the browser steps.

## Prompt A (English): submit an existing entry

```text
Please help me submit an entry to the GitHub repository chen-006/meow-ai-arena (the open-source repository of the Bilibili series "AI Coding Arena") by opening a pull request. Follow these steps in order.

1. Learn the rules
1.1 If the current directory is not a clone of that repository, fork and clone it: with gh, run `gh repo fork chen-006/meow-ai-arena --clone`; without gh, ask me to fork https://github.com/chen-006/meow-ai-arena in the browser, then `git clone` my fork.
1.2 Read `community/README.md` in full (it is in Chinese; `community/README_en.md` is a short English version). Its rules on folder names, meta.json fields and size limits are authoritative; if they differ from this prompt, follow the README.
1.3 Once the episode is chosen, read its rule files: territory: `land/workspace/TASK.md` and `RULES.md`; bomberman: `bomberman/workspace/README.md` and `RULES.md`; capture the flag: `ctf/workspace/README.md` and `RULES.md`; warehouse: `warehouse/workspace/TASK.md`; Tank: `tank/workspace/README.md`, `RULES.md` and `tank.hpp`. A Tank `bot.cpp` includes the official `tank.hpp` and defines only `init` and `decide`; the official `tank_main.cpp` provides `main`. Do not submit your own header or driver. Episode 4 (Diplomacy) does not accept submissions.

2. Confirm these with me (ask once, wait for my answers, do not guess)
- Which episode: land, bomberman, ctf, warehouse or tank.
- Where the entry is: a single .cpp file for the bot episodes; a folder of sources for the warehouse.
- The entry name: the part used in the folder name may only contain lowercase letters, digits and hyphens; I also need a display name for the leaderboard (any language).
- Credit: the author name shown on the leaderboard, and my GitHub username (you can look it up with `gh api user --jq .login` and ask me to confirm).
- Who wrote the code: a human (human), an AI (ai), or both (human+ai); which model and which tool were used.
- Whether it is based on someone else's code (an official bot, the original warehouse baseline, ...).
- A one-sentence description.

3. Put the files in place
3.1 Create a branch from the latest main, for example `submit/<episode>-<entry-name>`.
3.2 Create `community/<episode>/<github-username-in-lowercase>__<entry-name>/`. For bot episodes, put the code there as `bot.cpp`; for the warehouse, put the source files in its `src/` subfolder.
3.3 Write `meta.json` following the field table in `community/README.md`. Fill in written_by, model and tool truthfully: human if I wrote it myself; ai if you (or another AI) wrote it, with the real model name in model; human+ai if we both worked on it. Ask me if you do not know; do not make anything up. license must be "MIT"; before writing it, tell me this means agreeing to publish the entry under the repository's MIT license, and get my consent.
3.4 If I want to include notes on the approach, write them as `NOTES.md` in the same folder.
3.5 Do not add, modify or delete any other file in the repository. Do not touch the official `bots/`, `submissions/`, `workspace/`, `referee/`, and do not edit `community/check.py`, `community/LEADERBOARD.md` or `community/results/`.

4. Test locally
4.1 From the repository root, run `python community/check.py community/<episode>/<folder>` (use `python3` if there is no `python`). Tank uses native formal mode (2000 CPU ms for the first turn, 50 CPU ms thereafter), playing 20 games against each of 16 opponents, 320 total; `--full` uses 80 each, 1,280 total. Opponent final-tournament ratings stay fixed; only the entrant rating is fitted, rather than refitting a mini-tournament. Tank support must already be merged into the target branch because PR evaluation runs that branch's trusted scripts. Bot evaluation time depends on the programs; the warehouse needs to run the slow baseline first, about 5 to 10 minutes the first time.
4.2 If it does not pass, do not submit. Tell me why it failed. Fix structure and meta.json problems yourself; for problems in the code itself (compile errors, timeouts, warehouse output differing from the baseline), tell me first and let me decide whether to fix it or give up. Never edit the evaluation scripts or official files to make it pass.
4.3 If g++ or Python is missing, tell me what is missing and how to install it (see the root README). Only if I explicitly decline to install them, run with `--skip-eval` for the structure check and state "no local evaluation was run" in the pull request description.

5. Show me before opening the pull request
Opening a pull request is a public action. Before you commit, push or open it, send me the following and wait for my explicit approval:
- the list of files to be committed (`git status --short`) and the full text of `meta.json`;
- the pull request title, in the form `投稿：<episode> / <entry name>`;
- the pull request body: fill in the checklist from `.github/PULL_REQUEST_TEMPLATE.md` and paste the summary printed by check.py, starting at the line "## 社区投稿自测" (also saved in `community/.runs/<episode>/<folder>/summary.md`).
Nothing to be committed may contain keys, tokens, passwords, email addresses, phone numbers, local paths or other personal information; check the code, NOTES.md, meta.json and the commit message.

6. Submit
After I approve:
6.1 `git add community/<episode>/<folder>` (only this folder), then commit.
6.2 Push to my fork: `git push -u origin <branch>`.
6.3 With gh: `gh pr create --repo chen-006/meow-ai-arena --base main --title "<title>" --body-file <body file>`. Keep the body file in a temporary location outside the repository; do not commit it.
    Without gh: give me the link GitHub prints after the push (`https://github.com/<my-username>/meow-ai-arena/pull/new/<branch>`) together with the prepared title and body, and tell me to click "Compare & pull request", choose chen-006/meow-ai-arena and its main branch as the target, and paste them in.
6.4 Send me the pull request link and tell me what happens next: GitHub Actions evaluates the entry automatically (for an account's first pull request to this repository, a maintainer has to approve the run first); after a maintainer reviews and merges it, the entry appears in `community/LEADERBOARD.md`.

Also: do not take any other public action on my behalf (issues, comments, pull requests to other repositories). Community leaderboard numbers are not measured the same way as the official results from the show; do not compare them directly in the pull request.
```

## Prompt B (English): write one from scratch, then submit

Put this in front of the full text of Prompt A. Replace "capture the flag (ctf)" in the first line with the episode you want.

```text
I want you to write an entry from scratch and then submit it to the GitHub repository chen-006/meow-ai-arena. The episode is: capture the flag (ctf).

Fork and clone the repository first (see "1. Learn the rules" below), then work from the episode's task statement. See each episode's documentation for the original task and package. The open-source Tank package adds baseline source; to reproduce contest conditions exactly, first use `make_player_pack.py` as described in `tank/README.md`. Community development does not have to reproduce the contest time limit:
- Territory (land): the task statement is `land/prompt.md`, the package is `land/workspace/`; read `TASK.md` and `RULES.md` there first. The deliverable is `land/workspace/submission/bot.cpp`.
- Bomberman: the task statement is `bomberman/prompt.md`, the package is `bomberman/workspace/`; read `README.md` and `RULES.md` there first. The deliverable is `bomberman/workspace/submission/bot.cpp`.
- Capture the flag (ctf): the task statement is `ctf/prompt.md`, the package is `ctf/workspace/`; read `README.md` and `RULES.md` there first. The deliverable is `ctf/workspace/submission/bot.cpp`.
- Tank: the task statement is `tank/prompt.md`, the package is `tank/workspace/`; read `README.md`, `RULES.md` and `tank.hpp` first. The deliverable is `tank/workspace/submission/bot.cpp`, including the official `tank.hpp` and defining `init` and `decide`, with no custom `main`.
- Warehouse: the task statement is the round 1 section of `warehouse/prompt.md`, the package is `warehouse/workspace/`; read `TASK.md` there first. The deliverable is the rewritten `warehouse/workspace/src/`, whose output must be byte-identical to `baseline/` on every valid input.
These files are in Chinese.

While writing:
- Follow the rules in the task statement: C++20, only dependencies allowed by the episode (Tank uses the official `tank.hpp`; Windows-specific code cannot run in Linux community CI), a single file (several files under `src/` for the warehouse), no file or network access, and the per-turn time limits. Test repeatedly with the local referee in the package, or with `tools/bench.py`.
- The models on the show had a time limit (60 minutes in most episodes, 30 for the warehouse, 30 per half for Tank) and could not see other contestants' code. Neither restriction applies to you: you may study the official entries in the repository (`<episode>/bots/`, `warehouse/submissions/`). If you build on one of them, name it in based_on in meta.json.
- Keep your work-in-progress changes inside the package's `submission/` folder (`workspace/src/` for the warehouse); these are only working copies. When submitting, copy the finished code to `community/<episode>/<folder>/`, then restore the package with `git checkout -- <episode>/workspace`, so that the final pull request contains nothing but that one folder under community.
- When you are done, run `python <episode>/challenge.py <your bot.cpp>` to see where it ranks among the official bots (for the warehouse, `python tools/bench.py --baseline`), tell me the result, and ask whether to keep improving or submit as is.
- You wrote this entry, so in meta.json set written_by to ai, model to your own model name, and tool to the name of the tool I am using right now. If I suggested specific changes or edited the code myself along the way, use human+ai.

Then follow the submission procedure below. You already know where the entry is and who wrote it, so skip those two questions and confirm the rest with me as usual.
```
