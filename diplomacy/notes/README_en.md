[中文版](README.md)

# Models' notes

Files left by each model in its working directory, copied byte-for-byte after all three games (excluding only the referee-provided rules, client, map materials, connection information and OpenCode configuration). The same working directory was used for all three games, so each folder mixes files from all three.

| Folder | Model | Working directory name (= game-1 power) | Main contents |
|---|---|---|---|
| [fable-5.1/](fable-5.1/) | Fable 5.1 | ENGLAND | Decision notes for all three games: `笔记.md` (game 1, England), `笔记_第二局_俄国.md`, `笔记_第三局_意大利.md`; hypothetical orders and scripts for simulation |
| [opus-5.5/](opus-5.5/) | Opus 5.5 | RUSSIA | `notes.md` (game 1, Russia), `notes_game2_england.md`, `notes_game3_austria.md`; simulation scripts |
| [gpt-6.1-sol/](gpt-6.1-sol/) | GPT-6.1 Sol | TURKEY | `战略笔记.md` (game 1, Turkey), `GERMANY_第二局.md`, `FRANCE_第三局.md`; over a hundred hypothetical-order files for simulation |
| [gpt-6-astra/](gpt-6-astra/) | GPT-6 Astra | ITALY | Hypothetical orders for simulation only; no written notes |
| [deepseek-v4-pro/](deepseek-v4-pro/) | DeepSeek V4 Pro | GERMANY | Hypothetical orders for simulation only |
| — | Sonnet 5.5 | AUSTRIA | Left no files in its working directory |
| — | DeepSeek V4.1 Flash | FRANCE | Left no files in its working directory |

The format of hypothetical-order files (`.json`) is described in [workspace/README_en.md](../workspace/README_en.md). Run `python game.py simulate <file>` to simulate without affecting the real game.

The three Claude models also used Claude Code's automatic memory, stored outside the working directory and not included here.
