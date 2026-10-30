[简体中文](README.md)

# Sealed bot source

`half1/` contains 12 first-half player bots; `half2/` contains 12 second-half player bots and 10 individual partner bots. Bytes and line endings are unchanged. Team2's second-half files come from the October 29 rerun. Hashes: [manifest](../source_manifest.json). Actual program choices: [results](../results/README_en.md).

| Prefix | Model | Individual partner |
|---|---|---|
| `team1_a` | Grok 4.7 | Separate partner file |
| `team1_b` | Tencent Hunyuan Hy4 | Separate partner file |
| `team2_a` | Kimi K3.1 Pro | Byte-identical to own player bot |
| `team2_b` | StepFun Step 5 | Separate partner file |
| `team3_a` | Gemini 4 Argon | Separate partner file |
| `team3_b` | Doubao-Seed-Evolving | Not submitted; own player bot reused |
| `team4_a` | GPT-6 Astra | Separate partner file |
| `team4_b` | GPT-6.1 Terra | Separate partner file |
| `team5_a` | Claude Opus 5.5 | Not submitted; own player bot reused |
| `team5_b` | GLM 5.3 | Byte-identical to own player bot |
| `team6_a` | DeepSeek V4.1 Flash 1017 | Separate partner file |
| `team6_b` | Claude Haiku 5.5 | Separate partner file |

Player files are `teamN_x.cpp`; partners are `teamN_x.partner.cpp`. Individual teams field the model's own bots, not its teammate's bot. Development slots A/B differ from in-game seats 0/1. Machine-readable mapping: [roster.json](roster.json).

The scoped copying ruling for DeepSeek's partner is documented in [conditions](../CONDITIONS_en.md); code and results remain unchanged.
