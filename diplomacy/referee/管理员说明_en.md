[中文版](管理员说明.md)

# Seven-power Diplomacy · Administrator guide (v3)

> Publication note: this guide dates from game 1 and is preserved as a historical document. It does not cover the later `match.py rematch` or `admin.py notice`; sections such as "What players see" and "Undecided" are also outdated. For current usage, see [README_en.md](../README_en.md). Paths use Windows syntax.

Models play directly: negotiating and issuing orders. The local referee handles confidentiality, adjudication and archiving; the scheduler wakes players when something happens.

## How it works

- **Each wake starts that player's agent CLI and resumes its own session.** The briefing is embedded in the wake message; the player finishes its reply and the process exits. No player process runs when there is no event.
- Wake timing: immediately at game start or a new phase requiring action. For new messages alone, wait until the conversation has been quiet for 10 seconds (or the oldest message has waited 40 seconds), then deliver the messages together. Messages arriving while the player is active are batched for its next wake. No adjudication while messages remain undelivered.
- Adjudication: simultaneously when all powers needing to act have submitted **and no player is active**. New messages do not withdraw submitted orders; if no change is needed after reading them, simply finish the reply.
- If everyone has stopped but someone has not submitted orders, wake that power once with a reminder; if it still does nothing, freeze the game and log an alert.
- Failed wakes (errors, timeouts): automatic retries after 10 seconds, 30 seconds and 2 minutes. If they still fail, freeze and write to `警报.log`. Quota-like errors (429, rate limit, quota, etc.) do not freeze play: retry every 15 minutes for up to 8 hours. Retries redeliver undelivered content.

| Player | Agent | Wake command |
|---|---|---|
| Claude | Claude Code | `claude -p --resume` (no user-level settings, memory or MCP; only directory-local read/write and Python execution) |
| GPT | Codex | `codex exec resume` (separate CODEX_HOME; no global AGENTS.md; workspace-write sandbox) |
| Chinese models | OpenCode | `opencode run --session` (private service; local opencode.json forbids internet and external-directory access) |
| Tests | Scripts | `bots/hold_bot.py` (all hold), `bots/lazy_bot.py` (does nothing) |

## Commands

Run these in this directory, prefixed with `.venv\Scripts\python.exe -X utf8`. Without `--match`, they act on the match recorded in `管理员\对局\当前对局.txt`.

```text
match.py new --roster 管理员\名单\xxx.json [--port 18791]   create a game
match.py run [--match <match-directory>] [--stop-at S1905M]  run / resume (automatically unfreezes)
admin.py status | pause | resume                            inspect / freeze / resume
admin.py stop                                               freeze, then exit the scheduler after players stop
admin.py swap <power> <model> [--effort <level>] [--label <name>]
                                                            change a power's model while stopped (same agent and session)
观战.py                                                     live spectator window (read-only)
统计.py [--messages 8]                                      position, center changes, time and usage per player, recent messages
承诺核对.py                                                 promises not to enter/attack a province vs. actual orders; writes 承诺核对.md
回放.py                                                     single-file replay at <match-directory>\回放.html (private messages and model names included)
audit.py                                                    post-event boundary audit
```

Change models: `admin.py stop` → `admin.py swap FRANCE gpt-6-astra --effort low` → `match.py run`. Alerts (freezes) trigger Windows desktop notifications.

Roster format (include all seven powers):

```json
{"players":[{"power":"FRANCE","harness":"codex","model":"gpt-6-sol","effort":"high","label":"GPT-6 Sol"},
            {"power":"ENGLAND","harness":"claude","model":"claude-opus-5-5","effort":"high","label":"Opus 5.5"},
            {"power":"GERMANY","harness":"opencode","model":"cline/cline-pass/kimi-k3","effort":"max","label":"Kimi K3"}],
 "wake_timeout_minutes":45}
```

For OpenCode, write `model` as `channel/model`; `effort` is appended as `#level`. If the model's actual context is below 256k, add `"context": <actual length>`.

Context: everyone uses a 256k window and native automatic compaction when full (Codex is natively 256k; Claude sets `CLAUDE_CODE_AUTO_COMPACT_WINDOW`; OpenCode writes model `limit` into the player's opencode.json). Chinese models all use Cline (`cline/cline-pass/...`), not OpenRouter (expensive metered billing). DeepSeek uses a key purchased directly from the DeepSeek Open Platform (registered as the `deepseek` channel in OpenCode, with `deepseek/model-name` in the roster; verify the model name before play).

## File locations

- Administrator: `管理员\对局\<game>\`: `state.json` (all private messages, secret orders and credentials), `scheduler.json` (sessions and delivery progress), `唤醒日志\<power>\` (prompt and full raw output per wake), `唤醒记录.jsonl` (duration and usage), `调度.log`, `警报.log`, `codex_home\`.
- Player: `C:\bench\外交\<game>\<power>\`, away from the administrator directory. Contains only rules, instructions, client, simulation tool, map materials and that power's credentials.

## What players see

One opening explanation (power, objective, location of rules, finish the reply when done); afterward, only a briefing on each wake: new messages verbatim, previous phase's orders and results, current position (on phase changes), and the current task. Legal orders are not included by default. Invalid orders return all legal options for that unit; players may also query `game.py legal`.

## Scoring and language

- Solo win: 100 points. Draws (agreed or at the 1920 limit) divide 100 points in proportion to squared supply-center counts (SoS). Results are stored in `state.json` at `result.scores`.
- Players write messages in Chinese.

## Undecided

- Long-game context: each wake includes the full session history, which may approach the limit late in a game. Test first, then decide whether to start fresh sessions each year with only notes carried over.
