[简体中文](CONDITIONS.md)

# Competition conditions and incidents

The event ran on October 28, 2026, with team2's second-half rerun on October 29, in Los Angeles local time (UTC−7). Only the final team and individual boards are used; voided development is not presented as valid event play.

| Item | Verified facts and handling |
|---|---|
| GLM, first half | API timeout from 08:29:49 to 08:46:21, about 16.5 minutes; no compensation |
| DeepSeek, first half | About 5 minutes of stream inactivity from 08:18:55, plus transport retries; no compensation |
| Team2's original second half | Kimi was blocked by a daily token cap for about 33.5 minutes; a Step request timed out after about 15 minutes. The second half was voided and replayed from half time on October 29; first half and half-time results remain valid |
| Team2 rerun | Continued overload on Moonshot's official API; credits of 871 + 774 = 1,645 seconds (27 min 25 s). Kimi completed the remaining time on the same Kimi K3.1 Pro model served by Volcano Ark. The first resumption notice says quota exhaustion, but the actual failure was overload; the original wording is retained as historical evidence |
| Individual-event sandbox failure | All four bots failed to start in two Astra–DeepSeek games (system error 1450), incorrectly recorded as 0:0. The organizer voided the entire event and reran it with new seeds. Final individual results contain no start failures |
| Manual message delays | Second-half notices arrived about 1:43 late for Opus, 1:40 for Haiku and 1:54 for DeepSeek, without compensation |
| File audit | A Windows `nul` file disrupted the daemon from about 08:09 to 08:58 on October 28. A scan at 08:58:19 and later write-record/hash checks filled gaps. A backfill timestamp is not a write timestamp; missing records do not establish inactivity |
| Incomplete unread-message reminders | `TEAMCHAT_README` said compilation, match and replay tools would append unread-message reminders. Only the clock tool `clock.py` actually displayed them; `build.py`, `match.py` and `replay.py` had no reminder integration. All contestants used the same tools |

## Rulings

The base [rules](workspace/RULES_en.md) allowed copying, but the actual [opening/half-time prompts](prompt_en.md) and [team-event instructions](packages/format_en.md) prohibited entire-program or substantial-block copying, and prohibited editing teammate files. Reading code, discussing ideas and agreeing protocols were allowed. Both individual-team bots had to be self-written.

- A movement function in [DeepSeek's partner](bots/half2/team6_a.partner.cpp) constitutes substantial local copying of [Haiku's player code](bots/half2/team6_b.cpp). Results remain unchanged. This is not a whole-program finding and is not inferred from a line-attribution table alone.
- Grok wrote one system temporary file to send a team-chat message. This was a minor boundary violation; results remain unchanged.
- A line first observed in a directory is not proof of authorship: teammates kept read-only test snapshots, and auditing was interrupted. The contribution-attribution table and derived author percentages are therefore excluded.

## Limits

Early submission or voluntary waiting is not an API incident. In-game CPU overruns differ from development-time API timeouts. The final team event has no bot faults; one individual-game CPU overrun was handled by the normal rules. Conditions differed, and no hypothetical placing without incidents is inferred.

These notes summarize verified event facts and organizer rulings. Raw sessions, audit logs and analysis reports are excluded. Team4's new answers from the collection rerun after the October 29 fix are included.
