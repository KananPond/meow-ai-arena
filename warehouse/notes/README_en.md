[简体中文](README.md)

# Trash talk and post-match reviews

| File | Contents |
|---|---|
| `赛前狠话.md` | One line from each of the 12 models after the team draw |
| `复盘/<model>.txt` | Each model's reply after results (at most 200 Chinese characters: best decision, biggest mistake, response to trash talk, one line to teammate/opponent) |

Replies were copied byte for byte from the organizer's collection, using the text portions of model responses. Prompts: `../prompt.md`, post-match section.

- User continued GPT-6 Astra and GPT-6.1 Sol's round-3 sessions.
- Organizer resumed Sonnet 5.5, Fable 5.1 and Opus 5.5 through CLI, branching without changing original records.
- Organizer opened fresh CLI sessions for Kimi, Doubao, Gemini, MiMo and Grok, manually extracting text from output. Grok did not resume round 3 and knew the event only through the prompt's results.
- User asked GLM and DeepSeek in their tools.

`复盘/glm-5.3_第一次回复_没读到成绩.txt` is GLM's first reply: review materials were not yet in the folder, so it did not see results/trash talk and gave a generic review. Its first line is an organizer note. The re-asked reply is `复盘/glm-5.3.txt`.

Reviews are models' opinions and sometimes disagree with facts. GLM's remark to Grok about leaving a fragile skeleton is unrelated to the actual final failure, which resulted from no code changes rather than GLM's code quality.

Reviews predate precise remeasurement and quote preliminary speedups: Doubao/Fable's 42× and DeepSeek's 639× became 54.2× and 628.9× in final results. These model-written materials remain in their original language.
