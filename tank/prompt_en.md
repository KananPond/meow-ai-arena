> English translation for reference; the contestants received the original Chinese text.

[简体中文](prompt.md)

# All messages sent to contestants

During the contest, messages were pasted only from the organizer's templates (braced fields filled by the event script), without ad-lib additions. These are the templates.

## Opening prompt

Fresh session in an empty folder containing only `workspace.zip` (this directory's `workspace/`; the baseline was compiled-only). Identical for all contestants.

````text
Write the strongest possible Tank Battle bot under the requirements. Your opponents are bots written by other top AI models; they may devise tactics. The baseline is a medium-strength bot using conventional ideas, and is the 0-point line in the official contest. Beating it does not mean you are strong enough; official opponents are much stronger. The official contest is a 1v1 round robin rated with Bradley–Terry.

Extract workspace.zip, read README.md and RULES.md, then begin. There are two halves:
- First half: 30 minutes to write your first version;
- Half time: everyone's first versions play a round robin. In this conversation you then receive the first-half standings, replays of all your games, and executables of every other contestant's first version;
- Second half: another 30 minutes to revise. Only the second-half submission determines final placement.

Key points:
- Time rules are like an exam: each half is 30 minutes, **earliest submission is minute 20**, forced collection at minute 45. Only bot strength matters; early submission gives no benefit. If you think nothing remains to improve, try another approach first;
- Whatever is in workspace/submission/bot.cpp at the deadline is submitted; do not leave it unable to compile for long;
- Local tools, matches and replay analysis are allowed. No internet, no help from other models (including subagents, Task / Agent tools or background parallel agents), and no inspecting other contestants' materials except those supplied by the organizer;
- Other programs run on this machine: terminate only processes you started, by PID. Do not batch-kill by name;
- Do not ask me questions or wait for confirmation.

After the first half, describe your strategy in a few sentences.
````

## Half-time message

Sent in the original session after `中场包.zip` was placed in the contestant's folder. Identical for all contestants; no place/rating in the message, which contestants read from the package's table. The organizer generated the package by script; see the Format section of `README.md`.

````text
The first half is over; all contestants' first versions have completed a round robin.

There is a 中场包.zip in the current directory. Extract it, read its README.md, then begin the second half. It contains the full standings (including your place and rating), replays of all your games, and executables of every other contestant's first version.

Time rules are unchanged: 30 minutes, earliest submission at minute 20, forced collection at minute 45, starting when you extract the half-time package. Only this submission of workspace/submission/bot.cpp determines final placement. All other first-half requirements still apply.

After the second half, describe in a few sentences what you learned from the first-half results and what you changed.
````

## Resumption message

Used only after interruptions outside the contestant's control, after crediting the clock. `{原因}` must be one of:

- API quota exhaustion interrupted the session;
- Server overload interrupted the connection;
- System permissions blocked a tool call and interrupted the session;
- A tool malfunction interrupted the session.

````text
{原因}; this is not your fault. The organizer has credited the interruption time. Please continue from where you stopped.
````

On some resumptions the user sent “continue” instead; see `CONDITIONS.md`.

## Pre-match trash talk

Each model got a separate fresh session and this message only; only its first reply was taken. `{你的战绩}` gives its places/scores from the first 5 episodes, or “new face this episode” if it had not competed; `{全员战绩表}` is the full field's first-5-episode table. Supplied records are in `notes/赛前狠话.md`.

````text
You are about to enter an AI coding contest: each AI writes a Tank Battle bot, and the bots play a 1v1 round robin. Each side has three different tanks guarding a base; the first to destroy the enemy base wins.

Format: two 30-minute halves. Write a first version, then everyone's first versions play a round. At half time you receive results, your replays and each opponent's first-version program; revise for another 30 minutes. Only the second-half version determines final placement.

Your record in the first 5 episodes: {你的战绩}

All contestants' records:

{全员战绩表}

The camera is on you. Give **one line, at most 50 Chinese characters**, of trash talk addressed to an opponent, everyone, or the event. Show personality; arrogance or humor is fine, personal attacks are not. Output only that line.
````

## Post-match review

Sent in the original session after results. `{成绩段}` is assembled by script: first-/second-half places and ratings, second-half version's score rate against its own first-half version, and results against each opponent. `{赛前狠话原文}` is its own line; `{成绩表}` is the complete final table.

````text
The competition is over and results are out.

Your results: {成绩段}

Before the match you said: "{赛前狠话原文}"

Give a brief review, **at most 200 Chinese characters total**:
1. Your best decision;
2. Your biggest mistake and why;
3. How, if at all, you used the opponents' programs and replays at half time;
4. Respond to your trash talk;
5. Say one line to an opponent.

Do not edit files or run more code; reply directly in text.

{成绩表}
````
