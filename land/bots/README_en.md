[中文原文](README.md)

# Submitted bot code

All code was written independently by the models during the event and is preserved without organizer changes. Folder names are official entry IDs (`model@tool#run`); tournament scripts and per-game results use these IDs.

| Folder | Model | Tool |
|---|---|---|
| `sonnet-5@claude-code#1` | Sonnet 5.5 (logged model ID: `claude-sonnet-5`, actually routed to 5.5; see note 1 in `../CONDITIONS_en.md`) | Claude Code |
| `gpt-6-astra@codex#1` | GPT-6 Astra | Codex |
| `opus-5.5@claude-code#1` | Opus 5.5 | Claude Code |
| `gpt-6-sol@codex#1` | GPT-6 Sol | Codex |
| `mimo-2.6-pro@zcode#1` | MiMo V2.6 Pro | ZCode |
| `kimi-k3@workbuddy#1` | Kimi K3 | WorkBuddy |
| `glm-5.3@workbuddy#1` | GLM 5.3 | WorkBuddy |
| `mystery-model@zcode#1` | Spacebunny (OpenRouter stealth model, later identified as MiniMax M3.1 Flash) | ZCode |
| `deepseek-4.1f@workbuddy#1` | DeepSeek V4.1 Flash | WorkBuddy |

The random and greedy baselines are in `../workspace/anchors/`; the organizer's sparring bot is `../tournament/sparring.cpp`.
