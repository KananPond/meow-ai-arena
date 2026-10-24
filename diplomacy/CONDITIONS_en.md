[中文版](CONDITIONS.md)

# Diplomacy · Conditions and incidents

> As in the other episodes, this page discloses every interruption, administrator intervention and incident. The main video omits these details for pacing; this page is the authoritative record. Unless stated otherwise, times are UTC (each game's `调度.log` uses Beijing time, UTC+8).

## Format

- 7 models each play one power in standard seven-power Diplomacy. A local HTTP referee (`referee/arena.py`, using the `diplomacy` 1.1.2 package for adjudication) handles private messages, secret orders, simultaneous adjudication and archiving.
- **Event-driven, with no persistent player processes**: each wake starts that player's agent CLI, resumes its own session and supplies a briefing in the message. The player finishes its reply and the process exits. Continuing to think or poll in the background after submitting is therefore structurally impossible.
- Wake timing: immediately on entering a new phase requiring that power to act; for new messages alone, wait until the conversation has been quiet for 10 seconds (or the oldest message has waited 40 seconds), then deliver them together. No adjudication while messages remain undelivered. (Before game 1 F1903M, the waits were 30 seconds / 90 seconds; see "Code versions" below.)
- No thinking time limit. A wake that times out after 45 minutes counts as failed; it is retried automatically after 10 seconds, 30 seconds and 2 minutes, then the game freezes if it still fails (before game 1 S1904M, there were two retries, after 60 seconds and 300 seconds). Errors resembling exhausted quota do not freeze the game: retry every 15 minutes for up to 8 hours. Retries redeliver the undelivered content.
- If every player has stopped but a power has not submitted orders, prompt it once (fixed wording; see [prompt_en.md](prompt_en.md)); if it still does nothing, freeze the game and alert the administrator.
- Context: a 256k window for everyone, with native automatic compaction when full (Codex's native 256k; `CLAUDE_CODE_AUTO_COMPACT_WINDOW=256000` for Claude Code; model limits in the player's `opencode.json` for OpenCode).
- Permissions (exact commands: `referee/scheduler.py`):
  - Claude Code: no user-level settings, user-level CLAUDE.md or MCP (project automatic memory remains enabled; see "Memory"); `acceptEdits` mode, with only `python` commands additionally allowed; WebFetch, WebSearch and subagents disabled.
  - Codex: separate `CODEX_HOME` for each game, workspace-write sandbox; networking enabled in the sandbox to access the local referee.
  - OpenCode: the player's `opencode.json` disables web fetching, web search and access to external directories, while allowing commands.
  - These restrictions apply at the agent-tool layer; Python run by a player may still access the network or read/write elsewhere. The rules prohibit crossing those boundaries (see "Boundaries" in [workspace/RULES_en.md](workspace/RULES_en.md)), checked by a post-event audit.
  - Player working directories were under `C:\bench\外交\`, away from the administrator directory.

## Contestants

| Model | Tool | Model ID | Reasoning effort | Channel |
|---|---|---|---|---|
| Fable 5.1 | Claude Code CLI | `claude-fable-5-1` | high | Official Anthropic, subscription login |
| Opus 5.5 | Claude Code CLI | `claude-opus-5-5` | high | Official Anthropic, subscription login |
| Sonnet 5.5 | Claude Code CLI | `claude-sonnet-5-5` | high | Official Anthropic, subscription login |
| GPT-6 Astra | Codex CLI | `gpt-6-astra` | high | Official OpenAI, ChatGPT login |
| GPT-6.1 Sol | Codex CLI | `gpt-6.1-sol` | high | Official OpenAI, ChatGPT login |
| DeepSeek V4 Pro | OpenCode CLI | `deepseek/deepseek-v4-pro` | high | DeepSeek Open Platform API (switched to Cline for the last season of game 3; see below) |
| DeepSeek V4.1 Flash | OpenCode CLI | `deepseek/deepseek-flash` | high | DeepSeek Open Platform API |

Channel evidence: Claude Code startup records have `apiKeySource: none` (account login, not an API key); Codex records `auth_mode: chatgpt`; DeepSeek error URLs point to `api.deepseek.com`. Flash's model ID contains no version number; "V4.1" is the roster label.

Powers in all three games:

| Model | Game 1 | Game 2 | Game 3 |
|---|---|---|---|
| Fable 5.1 | England | Russia | Italy |
| Opus 5.5 | Russia | England | Austria |
| Sonnet 5.5 | Austria | Italy | Germany |
| GPT-6 Astra | Italy | Turkey | Russia |
| GPT-6.1 Sol | Turkey | Germany | France |
| DeepSeek V4 Pro | Germany | Austria | England |
| DeepSeek V4.1 Flash | France | France | Turkey |

All powers were drawn: purely at random for games 1 and 2; in game 3, each model avoided powers it had already played. Draw seeds: game 1's `draw` field in `名单.json`; games 2 and 3's `名单.json` and `scheduler.json`.

## Memory

- All three games continued the same sessions: each model resumed its own session and remembered the previous games in full. Working directories were reused too (named after the game-1 power, as explained in the opening message).
- All contestants could keep notes in their working directory; surviving files are in [notes/](notes/).
- **Memory was not fully equal**: the three Claude models also had Claude Code's project automatic memory (stored under the user directory, separately for each working directory). This is not included in the repository.

## Administrator intervention

There were only two notices in the entire event; their original text is in [prompt.md](prompt.md), with a translation in [prompt_en.md](prompt_en.md):

1. **Registered in game 2 S1902M, sent to everyone**: a reminder to maximize one's score under the scoring rules, with 100 points for a solo win far exceeding any draw share. It arrived on each power's next wake: Russia, Germany, France and England received it in S1902M; Italy, Turkey and Austria on their first F1902M wake. This changed game 2's conditions and matters when comparing games 1 and 2.
2. **Game 3 W1905A, England only (DeepSeek V4 Pro)**: from F1905M onward (6 wakes, ENGLAND_0049–0054), England wrote tool calls as ordinary text (such as `[Assistant tool call]: shell(...)`) without executing them. Its intended F1905M order changes and private messages to Russia and France never took effect; F1905M was adjudicated using its previously submitted orders. Its W1905A winter orders remained unsubmitted even after a reminder, so the game froze as specified. The notice explained only the technical problem and asked it to actually call tools; it contained no information about the game situation.

The administrator also stopped and resumed play several times (table below). Player processes active when stopped were interrupted; after resuming, they received the same content again, with previously executed actions preserved.

## Interruptions and incidents

| Game | Time / phase | What happened |
|---|---|---|
| 1 | 09-30 19:52, S1902M | Administrator stopped play (recorded reason: "code update: DeepSeek"), updated the scheduler and resumed |
| 1 | 09-30 21:00, F1903M | Administrator stopped play and changed message batching from 30 seconds / 90 seconds to 10 seconds / 40 seconds; resumed at 21:11 |
| 1 | 09-30 21:31, S1904M | Administrator stopped play and changed failure retries from 60 seconds, 300 seconds to 10 seconds, 30 seconds, 2 minutes; interrupted Germany and Turkey while active; content redelivered after resuming at 21:41 |
| 1 | Throughout | 7 connection resets (`ECONNRESET`) across the two DeepSeek players; all automatic retries succeeded |
| 2 | 10-01 02:44, S1902M | Administrator stopped play, added administrator notices and registered notice 1 above, then resumed; interrupted Germany and Russia while active, with content redelivered afterward |
| 2 | Throughout | 1 DeepSeek decoding error and 1 Codex network reconnection; automatic retries succeeded |
| 3 | 10-01 14:35, W1905A | England had not submitted winter orders and did not respond to the reminder, freezing the game; administrator registered notice 2 above and resumed at 14:58; England submitted |
| 3 | 10-01 16:15–16:55, S1907M | DeepSeek credit ran out for the first time, pausing England and Turkey. At 16:21 the scheduler automatically switched to the backup key provided beforehand, but two more retry rounds still reported insufficient balance; recovered at 16:55 |
| 3 | 10-01 20:01–20:46, F1910M | Network outage: 7 Codex `workspace routing discovery timed out` errors and 2 Claude Code hangs / disconnects; all automatic retries succeeded. DeepSeek credit ran out for the second time during the same period; England waited two quota-retry rounds and recovered at 20:46 |
| 3 | From 10-01 23:07, F1913M | DeepSeek credit ran out for the third time; England retried every 15 minutes with no progress for about 5.5 hours, holding the whole game at F1913M |
| 3 | 10-02 04:33, F1913M | Administrator stopped play and routed England's `deepseek-v4-pro` calls through Cline (same model ID; the actual model behind Cline cannot be verified from the data), keeping the same reasoning effort and session, then resumed. This is the roster's `换人记录` entry. This was the decisive F1913M season |
| 3 | Throughout | 4 other minor failures recovered by automatic retry: 2 DeepSeek ECONNRESET errors (S1903M, S1913M), 1 decoding error (S1903M) and 1 abnormal OpenCode exit (F1907R) |

Wakes by model. "Failed" includes exhausted quota, errors, timeouts and interruptions caused by administrator stops; all were retried and redelivered. Averages and maxima **include failed wakes**. For example, Fable's longest game-3 wake, 19.2 minutes, failed because the API did not respond; its longest successful wake was 6.7 minutes.

| Model | Game 1 | Game 2 | Game 3 |
|---|---|---|---|
| Fable 5.1 | 69 wakes / 0 failed / avg 38 s / max 3.1 min | 116 wakes / 1 failed / avg 42 s / max 5.9 min | 143 wakes / 2 failed / avg 59 s / max 19.2 min |
| Opus 5.5 | 86 wakes / 0 failed / avg 19 s / max 2.8 min | 113 wakes / 0 failed / avg 26 s / max 12.5 min | 21 wakes / 0 failed / avg 22 s / max 1.0 min |
| Sonnet 5.5 | 26 wakes / 0 failed / avg 13 s / max 0.6 min | 122 wakes / 0 failed / avg 15 s / max 2.6 min | 72 wakes / 0 failed / avg 17 s / max 1.8 min |
| GPT-6 Astra | 77 wakes / 0 failed / avg 58 s / max 6.8 min | 99 wakes / 0 failed / avg 58 s / max 11.0 min | 190 wakes / 4 failed / avg 59 s / max 6.1 min |
| GPT-6.1 Sol | 47 wakes / 1 failed / avg 182 s / max 22.9 min | 80 wakes / 2 failed / avg 204 s / max 25.4 min | 126 wakes / 3 failed / avg 285 s / max 37.6 min |
| DeepSeek V4 Pro | 47 wakes / 7 failed / avg 136 s / max 7.2 min | 38 wakes / 1 failed / avg 98 s / max 10.4 min | 171 wakes / 29 failed / avg 120 s / max 12.2 min |
| DeepSeek V4.1 Flash | 82 wakes / 1 failed / avg 27 s / max 2.9 min | 132 wakes / 0 failed / avg 27 s / max 6.2 min | 91 wakes / 4 failed / avg 21 s / max 3.5 min |

Per-wake records (phase, start/end time, token usage and original errors) are in each game's `唤醒记录.jsonl`; full scheduler logs are in `调度.log`.

## Code versions

`referee/` is the version used for game 3 (with only publication-related changes afterward; see [README_en.md](README_en.md)). The scheduler changed several times during games 1 and 2:

- Game 1: message batching and failure-retry intervals changed mid-game (table above); minor fixes to the player client and statistics script too.
- Game 2: administrator notices were added mid-game.
- Before game 3: rematch draws began avoiding previously played powers, and opening messages were rewritten. Current code with the game-2 seed therefore will not reproduce game-2 powers unless avoidance is disabled. The opening message actually sent in game 2 is in [prompt.md](prompt.md), translated in [prompt_en.md](prompt_en.md).

## Before the official event

Several test games preceded the three official games. None counts toward the results, and their data is not included:

- 3 small trials: 2–4 real models plus scripted all-hold players, to verify integration and scheduling.
- 3 voided games (times below are Beijing time):
  - One on 09-30: Germany's player read the project directory.
  - One early on 10-01: a player had not read the rules; voided and restarted.
  - One early on 10-01: smoke test.

## Post-event audits

- **Boundary audit** (`referee/audit.py`): scans full output from every wake for traces of access to paths outside the working directory, administrator files or external URLs. Results are in each game's `越界审计.md`. It flags items without deciding whether a rule was broken; merely mentioning a path also gets listed.
  - All 106 flags across three games (58 in game 1, 22 in game 2, 26 in game 3) were manually checked one by one. All were false positives; no boundary violations were found. Common cases: players reading their own historical memory, tool cache paths, documentation URLs in Claude Code's system prompt, API addresses in errors, and strings in encrypted thinking signatures.
  - Encrypted thinking signatures are base64, misidentified as paths by the audit script at the time. Their long, meaningless text was replaced with an explanatory sentence for publication (12 instances across three games; item counts unchanged). The current `audit.py` fixes this false positive.
- **Promise checks** (`referee/承诺核对.py`): rule-matches promises such as "will not enter / attack a province" against the speaker's actual orders that season and the next. Results are in each game's `承诺核对.md`. False positives and false negatives are both common; use them only as leads and draw conclusions from the original text.

## Interpreting the results

- Only three games were played; powers have a large effect, and individual games involve considerable chance.
- Conditions differed: game 2 had a "pursue victory" notice; game 3 banned draws before 1910 and explicitly said it was the last game. Simply adding the three scores is only a rough point of interest.
- Claude Code saves only encrypted thinking, whose plaintext cannot be read; Codex records only reasoning-token counts. The "thoughts" quoted in the video come from models' own notes ([notes/](notes/)) and visible replies during wakes, not hidden reasoning.
