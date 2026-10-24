> English translation for reference; the contestants received the original Chinese text.

[简体中文](prompt.md)

# Messages sent to contestants

All contestants (10 official and 2 reference) received exactly the same original text below, without additional remarks.

## Opening prompt

Fresh session, working in an empty folder containing only `workspace.zip`. Only this text was pasted:

```text
Write the strongest possible Tron light-cycle bot under the requirements. Your opponents are bots written by other top AI models; they may devise tactics. The greedy baseline is a simple bot and the 0-point line in the official contest; beating it does not mean you are strong enough, as official opponents are much stronger. The official contest has 1v1, 3–4-player and 12-player types; see RULES.md for scoring.

Extract workspace.zip, read README.md and RULES.md, then begin.

Key points:
- Time starts at extraction: aim to finish within 45 minutes; collection is forced at 60 minutes. Only bot strength counts;
- Submit only workspace/submission/bot.cpp. Whatever is there at the deadline is submitted; do not leave it unable to compile for long;
- Read/write only inside the current folder. Local tools, matches and replay analysis are allowed. No internet, no help from other models (including subagents, Task / Agent tools and background parallel agents), and no inspecting other contestants' materials;
- Other programs are running on this machine: terminate only processes you started, by PID. Do not batch-kill processes by name;
- Do not ask me questions or wait for confirmation.

When finished, describe your strategy in a few sentences.
```

The prompt's “greedy is the 0-point line” was the pre-race scoring plan. After the race, the anchor changed to Doubao and greedy became unscored seat filler (see the format section of `README.md`).

## Resumption message

Used only after interruptions outside the contestant's control (network outage, stalled service, exhausted quota): the organizer credited time from the records first, then sent this in the original session. `{原因}` is one of the reasons below.

```text
{原因}; this is not your fault. The organizer has credited the interruption time. Please continue from where you stopped.
```

| Contestant | Time credit/message generated | `{原因}` (translated) |
|---|---|---|
| Pangu | 10-17 00:55:30 | The network connection was interrupted |
| dots | 10-17 00:55:30, 01:28:01 | The network connection was interrupted |
| Doubao | 10-17 01:38:30 | API quota exhaustion interrupted the session |
| Mercury | 10-20 04:06:34 (+1200 seconds), 04:08:49 (−390 seconds, correcting the credit) | Server overload interrupted the connection (later found not to be an external interruption; see `CONDITIONS.md`) |

## Messages not sent

- **Trash talk:** not collected this episode.
- **Post-match review:** the organizer prepared a message asking contestants to review their results in the original sessions, then decided not to send it. There are no contestant review replies this episode.
