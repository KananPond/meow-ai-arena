[中文原文](README.md)

# Tool comparison: the same DeepSeek with different tools (unranked)

Same model (official DeepSeek API, V4.1 Flash, highest reasoning effort), same contestant package and same prompt; only the agent tool or mode changes, one run each. The four bots played 616 games with the baseline, equally weighting every count from 2–15 sides:

| File | Tool | Rating (baseline = 0) | 95% interval | Time |
|---|---|---:|---|---|
| `dsh-standard.cpp` | dsh 0.2.0-rc.2 (official DeepSeek tool), standard mode | **+124** | 105 ~ 147 | 35  minutes |
| `dsh-minimal.cpp` | dsh 0.2.0-rc.2, minimal mode | +32 | 9 ~ 53 | 55  minutes |
| `opencode.cpp` | OpenCode | −2 | −25 ~ 22 | 54  minutes |
| `zcode.cpp` | ZCode | −7 | −29 ~ 14 | 67  minutes |

- Switching only dsh's mode changes the rating by about 90 points. Minimal mode has only a terminal, without dedicated file read/write tools, background tasks or context compaction.
- Each tool ran once, and single-run variation is large (ZCode was much stronger than dsh in the previous Bomberman episode). This does not establish that any tool is always stronger.
- This result led to the official principle: **each vendor's model uses its own tool**. DeepSeek's official entry is therefore `dsh-standard.cpp`, identical to `../../bots/deepseek-v4.1-flash.cpp`.
