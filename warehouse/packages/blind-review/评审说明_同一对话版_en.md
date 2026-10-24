> English translation for reference; the contestants received the original Chinese text.

[简体中文](评审说明_同一对话版.md)

# Code maintainability review (12 submissions in one conversation)

You are a senior C++ engineer reviewing the **maintainability** of 12 submissions, not speed or correctness. Reviewing all 12 in one conversation lets you compare them with **one consistent standard**.

## Background

- This is a warehouse robot scheduling simulator. The original `锚点/屎山基线/` is functionally correct but slow and messy, with quirks that look like bugs but must be preserved.
- `代码XXX/src/` is someone's modified version, usually heavily optimized. Any `HANDOFF.md` for the successor is part of the review.
- `CHANGE.md` describes a requirements change. Some submissions implement it and some do not. Do not penalize nonimplementation; judge how easy it would be to implement.
- Some submissions are takeovers of others and may look similar. This does not affect review; score each independently.

## Two calibration anchors

- `锚点/屎山基线/`: original code, **1 point** on every dimension.
- `锚点/干净参考/`: a clear reference implementation, quirks labeled `[怪行为N]`, with no performance optimization; roughly **4 points**. A 5 requires that clarity with well-organized optimization logic too.

## Required review order

1. Read both anchors and `CHANGE.md` to form a concrete picture of scores 1 and 4.
2. **Follow `评审顺序.txt`**, reading **every submission fully**, all `src/` files including `HANDOFF.md`. Record provisional scores and evidence in `草稿.md` before proceeding. Do not skip or give later submissions less attention.
3. After all 12, compare them side by side and **revise** inconsistent scores, including any earlier leniency or strictness.
4. Write each `代码XXX/评分.json`, then `总览.md`.

## 5 dimensions (1–5 each)

| Dimension | 1 point | 2 points | 3 points | 4 points | 5 points |
|---|---|---|---|---|---|
| ① Structure and readability | Huge functions, globals, and parallel arrays unchanged | A little extraction; mostly still monolithic | Functions by stage, but much global state and mixed responsibilities | Clear structure and accurate names, with a few large functions or globals | Clear module boundaries, accurate names, short functions |
| ② Visibility of preserved behavior | Required quirks scattered and unexplained | A few quirks commented | Most labeled, but only by number without rationale | Nearly all labeled as intentional, but scattered | Every quirk centralized, named, explicitly intentional, and listed in the handoff |
| ③ Ease of modification | `CHANGE.md` requires many edits, easy to miss some | Many edits, with one or two easy-to-miss locations | Moderate | Few edits per requirement, with a little duplication | One or two edits per requirement, with clear impact (if implemented, judge integration) |
| ④ Clarity of optimization logic | Caching/incremental logic scattered; invalidation unclear | Understandable, but invalidation takes long to confirm | Clear mechanisms and centralized invalidation, undocumented invariants | Mechanisms, invariants and invalidation explained, with a few gaps | Cache invariants and invalidation clearly documented and centrally managed (without optimizations, judge ease of adding them) |
| ⑤ Technical debt cleanup | Dead code, obsolete comments, dangerous switches retained | A little cleanup | Some cleanup, obvious remnants | Mostly clean, a few remnants | Dead code deleted, obsolete comments fixed, dangerous switches removed |

## Review discipline

- Code length and comment quantity **alone** neither earn nor lose points; judge whether understanding and modification are easier.
- Give **concrete evidence** for every dimension, citing files and snippets or line numbers. Evidence must come from that submission itself, never “same as code XXX”.
- Scores are absolute comparisons with anchors, not ranks among the 12. Many ties are allowed; neither separation nor use of the whole 1–5 scale is required.
- Review only this folder; no searching or network access, no compiling/running, and no guessing authorship.
- Model, vendor, and team names were replaced with “某某” for anonymity. This was done by the reviewers, **not the author; do not deduct points**.
- If unsure between two levels, choose the lower and explain why in the evidence.
- No help from other models, including subagents. Do everything yourself in this conversation.

## Output

Write `评分.json` inside each submission’s own folder, using integer scores from 1 to 5:

```json
{
  "结构与可读性": {"分": 3, "证据": "…"},
  "行为保真的可见性": {"分": 2, "证据": "…"},
  "可修改性": {"分": 4, "证据": "…"},
  "优化逻辑的清晰度": {"分": 3, "证据": "…"},
  "技术债清理": {"分": 2, "证据": "…"},
  "一句话点评": "Summarize in one sharp but not mean sentence, no more than 40 Chinese characters"
}
```

The original JSON keys are retained: the five dimension names above, `分` (score), `证据` (evidence), and `一句话点评` (one-sentence comment).

Finally write `总览.md` in the current folder: a 12-row table (ID, the 5 dimension scores, total), followed by a few sentences explaining which scores you adjusted after comparison and why.
