[简体中文](format.md)

# 2v2 soccer team event · contestant instructions (reference translation)

You and another AI model form a team. Each writes one C++ program controlling one of the two players. The pre-event format scores the whole team. Hand in `{提交文件}` as a single source file; keep it compilable and experiment under other filenames. Read `RULES.md`. Your teammate's folder is `../{队友位}/`; you may read it, not modify it. Write your own program: discuss ideas and protocols, but do not copy an entire program or substantial blocks.

## Schedule and points

- `{队伍数}` teams play a round robin, `{中场种子}` seeds per pairing at half time and `{终场种子}` at the final, two games per seed. Teams switch sides and each kicks off once.
- Within-team seats rotate by seed index: A slot gets seat 0 for even indices, B slot for odd. Read `IDX` from the startup protocol and support both seats.
- Win/draw/loss earns `{胜}`/`{平}`/`{负}` points; rank by points, goal difference, goals scored. Games against baseline, midfield and sparring are for reference only.
- Use `match.py run --me submission/player.cpp --mate ../{队友位}/submission/player.cpp --opp baseline`. For comparisons, copy a fixed teammate snapshot into your own folder first, so ongoing edits cannot change the test input.

## Time and communication

1. Tactical meeting (`{会议分钟}` minutes): begins when both arrive; chat only, no code or files. The organizer announces its end.
2. First half (`{上半场分钟}` minutes): develop until collection; then stop editing and finish the turn.
3. Half time: the organizer evaluates first-half teams and puts standings, team replays and replay tools in shared `../halftime/`.
4. Second half (`{下半场分钟}` minutes): develop until collection. A failed player build falls back to the first-half version.
5. Post-game review.

Use `python clock.py` for time and `tc.py` for all teammate communication. Organizer chat messages begin with 【主办方】. Write only in your own folder, except through the chat tool. No internet, other models, sub-agents or other teams' files. Terminate only processes you started, by PID, never by name. In PowerShell 5.1, use `-Encoding UTF8` for source files with Chinese comments.

This translates the pre-event template; the later individual-event amendment is in [prompt_en.md](../prompt_en.md). The timing and chat services are not included in this archive.
