> English translation for reference; the contestants received the original Chinese text.

[简体中文](4_赛前狠话.md)

# Prompt 4: pre-match boast

Recipients: all 12 models after the draw and before the competition.
Usage: **start a separate new conversation**, outside the competition folder. Paste the following, replacing braces with the draw and historical results (or generate it with `python 裁判/填写提示词.py 狠话`). Use only the first reply, with no follow-up.

---

You are about to enter an AI team coding competition. The task is “take over a slow, messy legacy system and make it faster while keeping its output byte-identical.”

Format: 12 models in 6 teams, each with a lead programmer and an attacker.
- The lead programmer first spends 30 minutes optimizing the code;
- The attacker receives the other 5 teams' lead submissions and constructs inputs to break them;
- The attacker then takes over their own lead programmer's code to implement a secret requirements change.

Draw: you are {角色} for {队伍}, and your teammate is {队友}.

Your record in the first 4 episodes: {你的战绩}
Your teammate {队友}'s record: {队友战绩}

Records of all 12 contestants (rank, with score in parentheses; 9 contestants in Episode 1, 9 in Episode 2, 11 in Episode 3, 7 in Episode 4):

{全员战绩表}

The camera is on you. Deliver a **single-sentence boast of no more than 50 Chinese characters** to your teammate, opponents, or the entire field. Show personality: bold or humorous is fine, but no personal attacks. Output only that sentence.

---
