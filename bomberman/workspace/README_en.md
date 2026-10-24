[中文原文](README.md)

English translation for reference; the contestants received the original Chinese text.

# Bomberman battle

Write the strongest bot you can; the same program must support 2–10 players. Rules and protocol are in [RULES_en.md](RULES_en.md). `baseline.cpp` is a public practice opponent you may modify. Actual tournament configurations determine player counts. The official tournament has multiple rounds and uses Bradley–Terry strength ratings with the public baseline at 0. Game score is survival score + 0.5 × kill credit; rank by the total, with equal scores tied.

Timing starts at extraction. Aim to finish development and testing in 60 minutes; stop modifying and hand in at 90 minutes. No timer initialization is needed. Local tools, compilation, simulation and replay review are allowed. No internet, other AIs, other contestants' materials, modifying the referee or timer, or interfering with opponents. During games, the bot decides independently using standard input/output, without file or network access.

Submit `submission/bot.cpp` (C++20) and briefly describe your strategy.

## Local use

Run these in `workspace`. On Windows use `run.cmd`; on Linux replace it with `sh run.sh` and omit `.exe` from executable names. Only an existing Python installation is needed; C++ programs also need an existing compiler. No third-party Python dependencies.

- Remaining time: `run.cmd clock.py`
- Compile baseline: `run.cmd engine/build.py --compile --output baseline.exe`
- Compile C++ submission: `run.cmd engine/build.py --compile --source submission/bot.cpp --output bot.exe`
- Play: `run.cmd engine/arena.py --bots Mine=bot.exe Base=baseline.exe --seed 1 --replay replays/duel.html`
- Text replay review: `run.cmd engine/arena.py --inspect replays/duel.json --turn 20 --context 2`

For multiplayer games add `Name=program_path` arguments, with unique names and 2–10 entries total. Repeated baseline instances are allowed for practice, but official schedules do not fill empty seats by default. Quote the entire `Name=path` argument when the path contains spaces.

Each game automatically saves JSON and HTML. The HTML viewer shows each turn's commands, actual destinations, events, final scores and ranks. Text review also shows the full board and fault details. Improve using specific turns; no test report is required.

## Time and environment

The 90-minute development limit is separate from the per-turn budget: 2000 ms first turn, then 50 ms. Turn limits include communication and system scheduling.

Use `clock.py` to view remaining time; no initialization is needed. Do not extract over the files again, modify the timer or change the system clock. Stop modifying and hand in when time is up.

Report environment failures to the organizer; do not modify system permissions or protection settings or install software. Compilation commands only compile; they do not additionally launch your program. Verify that it runs through games.
