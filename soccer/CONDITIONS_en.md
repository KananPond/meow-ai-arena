[简体中文](CONDITIONS.md)

# Competition conditions and incidents

The event ran on October 28, 2026, with team2's second-half rerun on October 29, in Los Angeles local time (UTC−7). Only the final team and individual boards are used; voided development is not presented as valid event play.

## Contestant conditions and time

Effort values below are recorded event/rerun settings, **not an independent verification of every request**. The 08:00 configuration note targets each model's highest available level: Doubao maps xhigh/max to high; Hunyuan, Step and Gemini use high; Grok uses xhigh; the others use max. Identical labels across tools are not equivalent. Codex's configured service tier was default.

| Slot / model | Tool | Configured effort | Recorded route | First / second-half net clock (min:sec) |
|---|---|---|---|---|
| team1_a / Grok | Grok Build | xhigh | Account route not recorded in event configuration | 40:02 / 40:01 |
| team1_b / Hunyuan | OpenCode | high | OpenRouter, pinned to Tencent | 40:02 / 40:01 |
| team2_a / Kimi | Kimi Code | max | First half: OpenRouter, Moonshot only; valid second-half rerun: Moonshot official → Volcano Ark | 40:01 / 40:03 |
| team2_b / Step | OpenCode | high | First half: OpenRouter, StepFun only; valid second-half rerun: StepFun official | 40:01 / 40:03 |
| team3_a / Gemini | OpenCode | high | Google Vertex AI | 40:04 / 40:02 |
| team3_b / Doubao | OpenCode | high | Volcano Engine | 40:04 / 40:02 |
| team4_a / Astra | Codex | max | Subscription/API route not recorded in event configuration | 40:02 / 40:01 |
| team4_b / Terra | Codex | max | Subscription/API route not recorded in event configuration | 40:02 / 40:01 |
| team5_a / Opus | Claude Code | max | Subscription/API route not recorded in event configuration | 40:04 / 40:02 |
| team5_b / GLM | OpenCode | max | First half: OpenRouter, Z.AI only; second half: Zhipu official | 40:04 / 40:02 |
| team6_a / DeepSeek | dsh | max | DeepSeek official (pre-event tool-configuration check) | 40:01 / 40:01 |
| team6_b / Haiku | Claude Code | max | Subscription/API route not recorded in event configuration | 40:01 / 40:01 |

Versions: the operations manual records Codex 0.165.0; pre-event research records OpenCode 2.2.0 for GLM. There is no complete version inventory for every slot and launch. Other versions are unverified; current installed versions are not substituted for historical evidence. Subscription, login type and routes absent from event records are not inferred.

Routes come from event configuration, the half-time change note and team2's rerun configuration. Kimi, GLM, Hunyuan and Step initially used OpenRouter pinned to their respective official suppliers. Kimi, GLM and Step switched to direct official APIs at half time; Hunyuan retained its route. In team2's valid rerun, Kimi then switched from Moonshot's official API to Volcano Ark. Only public service names are listed; account details, credential locations, project identifiers and proxy configuration are excluded.

### Clock definition

The first-half clock starts five minutes after both teammates arrive; the second-half clock starts when the half-time package is delivered. Net time = valid seal time − half start − credited time. Durations are computed from millisecond timestamps and rounded to seconds. Timer polling accounts for a few seconds beyond 40 minutes. Per-slot durations are above; sanitized timestamps are in [timing.json](packages/timing.json).

| Team | First-half clock start → seal (Oct 28) | Valid second-half clock start → seal | Second half elapsed / credited / net (min:sec) |
|---|---|---|---|
| team1 | 08:04:26 → 08:44:28 | 10-28 09:47:15 → 10:27:16 | 40:01 / 0:00 / 40:01 |
| team2 | 08:04:38 → 08:44:39 | 10-29 04:21:37 → 05:29:05 | 67:28 / 27:25 / 40:03 |
| team3 | 08:04:50 → 08:44:54 | 10-28 09:47:19 → 10:27:21 | 40:02 / 0:00 / 40:02 |
| team4 | 08:05:03 → 08:45:05 | 10-28 09:47:20 → 10:27:21 | 40:01 / 0:00 / 40:01 |
| team5 | 08:11:30 → 08:51:34 | 10-28 09:47:25 → 10:27:26 | 40:02 / 0:00 / 40:02 |
| team6 | 08:13:32 → 08:53:33 | 10-28 09:47:25 → 10:27:26 | 40:01 / 0:00 / 40:01 |

Teammates share a clock. Both team2 credits, 871 + 774 seconds, applied to the whole team, including Step. Only the valid October 29 second half is counted; the voided attempt is not added. Other teams received no credit. API inactivity, early stopping and manual message delays can consume the clock: these durations are **not active coding or continuous reasoning time**, and uncredited outages have not been deducted. Final state timestamps were cross-checked against start, package-delivery, credit and sealing events.

## Incidents and handling

| Item | Verified facts and handling |
|---|---|
| Voided 07:54 opening | The October 28 07:54:58 opening was voided because effort settings were wrong. Logs record stopping launched sessions at 07:57 and the valid event opening at 07:59:14. The official configuration records the highest-available-effort policy. Only valid seals and results count; voided programs, sessions and results are excluded. This does not establish that every contestant was previously unfamiliar with the task |
| Second-half duration change | All 12 initially distributed READMEs say 25 minutes for the second half. The half-time prompt explicitly changed this to 40 minutes, which the actual clock used. Initial-package reconstruction retains 25; event durations use valid clock records |
| GLM, first half | API timeout from 08:29:49 to 08:46:21, about 16.5 minutes; no compensation |
| DeepSeek, first half | About 5 minutes of stream inactivity from 08:18:55, plus transport retries; no compensation |
| Team2's original second half | Kimi was blocked by a daily token cap for about 33.5 minutes; a Step request timed out after about 15 minutes. The second half was voided and replayed from half time on October 29; first half and half-time results remain valid |
| Team2 rerun | Continued overload on Moonshot's official API; credits of 871 + 774 = 1,645 seconds (27 min 25 s). Kimi completed the remaining time on the same Kimi K3.1 Pro model served by Volcano Ark. The first resumption notice says quota exhaustion, but the actual failure was overload; the original wording is retained as historical evidence |
| Individual-event sandbox failure | All four bots failed to start in two Astra–DeepSeek games (system error 1450), incorrectly recorded as 0:0. The organizer voided the entire event and reran it with new seeds. Final individual results contain no start failures |
| Manual message delays | Second-half notices arrived about 1:43 late for Opus, 1:40 for Haiku and 1:54 for DeepSeek, without compensation |
| File audit | A Windows `nul` file disrupted the daemon from about 08:09 to 08:58 on October 28. A scan at 08:58:19 and later write-record/hash checks filled gaps. A backfill timestamp is not a write timestamp; missing records do not establish inactivity |
| Incomplete unread-message reminders | The [team-chat instructions](packages/TEAMCHAT_README_en.md) say **if** an unread-message reminder appears after compilation, match or replay tools, read the message; this is conditional wording. In the implementation, only `clock.py` had reminder integration; `build.py`, `match.py` and `replay.py` did not. All contestants used the same tools |

## Rulings

The base [rules](workspace/RULES_en.md) allowed copying, but the actual [opening/half-time prompts](prompt_en.md) and [team-event instructions](packages/format_en.md) prohibited entire-program or substantial-block copying, and prohibited editing teammate files. Reading code, discussing ideas and agreeing protocols were allowed. Both individual-team bots had to be self-written.

- A movement function in [DeepSeek's partner](bots/half2/team6_a.partner.cpp) constitutes substantial local copying of [Haiku's player code](bots/half2/team6_b.cpp). Results remain unchanged. This is not a whole-program finding and is not inferred from a line-attribution table alone.
- Grok wrote one system temporary file to send a team-chat message. This was a minor boundary violation; results remain unchanged.
- A line first observed in a directory is not proof of authorship: teammates kept read-only test snapshots, and auditing was interrupted. The contribution-attribution table and derived author percentages are therefore excluded.

## Limits

Early submission or voluntary waiting is not an API incident. In-game CPU overruns differ from development-time API timeouts. The final team event has no bot faults; one individual-game CPU overrun was handled by the normal rules. Conditions differed, and no hypothetical placing without incidents is inferred.

These notes summarize verified event facts and organizer rulings. Raw sessions, audit logs and analysis reports are excluded. Team4's new answers from the collection rerun after the October 29 fix are included.
