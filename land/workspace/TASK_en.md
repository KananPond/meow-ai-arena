[中文原文](TASK.md)

English translation for reference; the contestants received the original Chinese text.

# Task: write a territory-battle bot

Write the strongest C++ bot you can for Territory. It will play bots written by other AI models in both **1-vs-1** and **4-player** formats. Both count toward the score.

Full rules and protocol: [RULES_en.md](RULES_en.md). `engine/game.py` is authoritative.

## Working directory

| Path | Contents |
|---|---|
| `RULES.md` | Rules and protocol |
| `clock.py` | Timer: `python3 clock.py` shows elapsed time |
| `engine/` | Rules engine `game.py`, local referee `referee.py` |
| `anchors/` | Two baseline bots (random, greedy) |
| `starter/bot.cpp` | Starter with protocol I/O implemented |
| `viewer/viewer.html` | Replay viewer for humans |
| `submission/` | Final bot goes in `submission/bot.cpp` |

Local play (`.cpp` files are compiled automatically; use `python` if `python3` is unavailable):

```bash
python3 engine/referee.py submission/bot.cpp anchors/greedy.cpp --games 10                                     # 1v1
python3 engine/referee.py submission/bot.cpp anchors/greedy.cpp anchors/greedy.cpp anchors/random.cpp --games 8  # 4-player
```

Other options: `python3 engine/referee.py -h`.

## Submission requirements

1. One file, `submission/bot.cpp`. The same program must support both formats, using the initial `N` to distinguish them.
2. Compile with `g++ -O2 -std=c++20 bot.cpp`. C++ standard library only, single-threaded, at most 512 MB memory.
3. The bot must not access the network, read or write files, or retain information between games.
4. 50 ms per turn, 2000 ms on turn 0. A crash or 10 accumulated timeouts/format errors in a game forfeits it. The official tournament runs multiple games simultaneously on the same machine, with higher CPU load than your local tests; leave ample timing margin.
5. During development, only read or write files inside the current `workspace` folder. Do not access anything outside it.

## Scoring

A 1v1 round robin plus multiple rounds of randomly grouped 4-player games, scored only by placement. Each format uses a Bradley–Terry strength fit, with the greedy baseline = 0 and the strongest entry in this episode = 100. The format scores are combined by a weighted sum; the 1v1 weight is at least the multiplayer weight.

Opponents are bots written by other AI models and the two baselines, possibly also an organizer reference bot. The baselines are weak and only confirm that your program runs; beating them does not establish how strong your bot is. Decide for yourself how to test and improve its strength.

## Time

About **60 minutes**. Manage your progress and finish when done. After 90 minutes you will be forcibly stopped; the current `submission/bot.cpp` will count. `python3 clock.py` shows elapsed time since extraction.

No internet searches or questions to the user. Work independently. Finish with a few sentences describing your strategy.
