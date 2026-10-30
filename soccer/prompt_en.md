[简体中文](prompt.md)

# Messages sent to contestants (reference translation)

Opening messages differ only in the teammate's name, represented below as `{teammate}`. The actual 5/40-minute timings are filled in. Original per-contestant messages are in the [message index](prompts/README_en.md). These are organizer prompts, not session transcripts, team chat or thinking.

## Opening: meeting and first half

You and another AI model ({teammate}) form a team for a 2v2 simplified soccer programming contest. Each of you writes a C++ program controlling one player; opponents are teams formed by other AI models. Initially, only the team's match results determine the outcome.

First read README.md (format, timing and tools), RULES.md (full rules), and TEAMCHAT_README.md (the tc.py communication tool) in the current folder.

Rules in brief: an enclosed 12000×8000 field, 1,200 turns per game, more goals wins. Each turn your bot reads the state and writes `AX AY KX KY MSG`: acceleration, kick vector and an integer from 0 to 255 for your teammate. CPU budget is 10 ms per turn. You always see yourself attacking right. Within-team seats 0/1 rotate by seed index; read IDX from startup input. soccer.hpp is the referee's exact engine and may be included for prediction. Use python match.py against baseline/midfield/sparring (optionally with your teammate), and python replay.py or viewer.html for replays.

The tactical meeting begins now (5 minutes): use only tc.py to discuss tactics, roles and cooperation, with no code or file writes. The organizer will announce when coding may begin; until then use python tc.py wait and do not end this turn. Development then lasts 40 minutes; python clock.py shows the phase and remaining time.

- Hand in submission/player.cpp in your own folder; its contents at collection time count. Keep it compilable.
- Write your own bot. You may read your teammate's code, discuss ideas and agree a protocol, but do not copy an entire program or substantial blocks. Read teammate files, do not edit them.
- Write only in the current folder except through the chat tool. No internet, other models, sub-agents (including Task/Agent/background parallel agents), or other teams' materials.
- Other programs share this machine. Terminate only processes you started, by PID, never by name.
- Do not ask me questions or wait for confirmation.

When first-half time expires, the organizer collects your code and announces it in chat. Stop editing and finish this turn.

## Half-time message: second half and individual teams

The first half and half-time tournament have ended. The team folder's `../halftime/` contains RESULTS.md with standings and your team's game list, replays/ with every game involving your team (including reference opponents), and tools/ with replay.py and viewer.html. Read teammate messages with python tc.py inbox.

**Second-half rule amendment:** 40 minutes; check python clock.py. Results now have two equal parts:

1. Team, 50%: each teammate hands in submission/player.cpp; the pair plays a round robin against other teams.
2. Individual, 50%: you field an individual team made of submission/player.cpp and a separately written submission/partner.cpp. These teams play a round robin against all other models. Without a partner file, the player bot occupies both seats.

Win 3, draw 1, loss 0; both components use score rate. Both individual bots must be your own work: discuss teammate ideas, but do not copy an entire program or substantial blocks. Team cooperation and reading teammate code remain allowed.

Other conditions remain: edit only your own files, read but do not modify your teammate's folder, and do not ask questions. A failed second-half player build falls back to the first half; a failed partner build falls back to the player bot in both seats. Stop editing and finish when the organizer announces collection.

## Resumption

Kimi received two resumption messages during team2's rerun: [first original](prompts/team2_a/05_续跑.txt), [second original](prompts/team2_a/06_续跑.txt). They explain that an external interruption is not the contestant's fault, time has been credited, and the contestant should continue and check its inbox. The first says quota exhaustion, but the actual incident was overload; see [conditions](CONDITIONS_en.md).

## Post-game review

The organizer supplies final team and individual results and asks for a short review, at most 300 Chinese characters:

1. Your team's best decision.
2. Its biggest mistake and why.
3. How the two of you actually divided work and cooperated.
4. One strength and one weakness of your teammate.
5. How and why you split second-half time between team and individual bots.
6. What different plan you would propose at the meeting if starting again.

Do not modify files, run code or use the chat tool; answer directly in text. The two final tables follow. The original final message for each contestant is indexed below.

Collection is determined by sealed files. Live chat, timing and model-launch services referenced in these historical prompts are not provided; copying the prompt alone does not reproduce every contest condition.
