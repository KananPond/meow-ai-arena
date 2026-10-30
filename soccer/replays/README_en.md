[简体中文](README.md)

# Replays: a fixed-seed random sample

18 games: one per team in the final team event and one per model in the individual event, drawn only from contestant-vs-contestant games, without replacement within each phase. No filtering by score, outcome or length. Open the [viewer](../workspace/viewer.html) offline and drag in a `.txt`, or run `python workspace/replay.py info path/to/replay.txt`.

Selection: Python `random.Random(20261029)`, final then individual; sort team IDs lexicographically, sort each team's eligible games by `id`, exclude already selected games in that phase, then `rng.choice`. See [selection.json](selection.json). Files are byte-identical; hashes are in the [source manifest](../source_manifest.json).

| Phase | Sampled for | Seed | Score | File |
|---|---|---:|---|---|
| final | team1 | 510658765 | 17:13 | [team1-team5-s12-2](final/team1-team5-s12-2.txt) |
| final | team2 | 894186889 | 2:20 | [team2-team4-s31-1](final/team2-team4-s31-1.txt) |
| final | team3 | 1309329913 | 10:2 | [team2-team3-s20-2](final/team2-team3-s20-2.txt) |
| final | team4 | 754346795 | 8:1 | [team4-team5-s38-1](final/team4-team5-s38-1.txt) |
| final | team5 | 763542250 | 0:5 | [team3-team5-s1-2](final/team3-team5-s1-2.txt) |
| final | team6 | 262145976 | 1:7 | [team3-team6-s35-2](final/team3-team6-s35-2.txt) |
| individual | Astra | 676540338 | 3:3 | [Astra-Terra-s2-2](individual/Astra-Terra-s2-2.txt) |
| individual | DeepSeek | 1726133518 | 4:7 | [Doubao-DeepSeek-s8-1](individual/Doubao-DeepSeek-s8-1.txt) |
| individual | Doubao | 555467454 | 7:2 | [Step-Doubao-s9-2](individual/Step-Doubao-s9-2.txt) |
| individual | GLM | 2007588829 | 6:2 | [Kimi-GLM-s13-1](individual/Kimi-GLM-s13-1.txt) |
| individual | Gemini | 1144914481 | 4:4 | [Gemini-Terra-s6-2](individual/Gemini-Terra-s6-2.txt) |
| individual | Grok | 683524627 | 2:8 | [Grok-DeepSeek-s14-1](individual/Grok-DeepSeek-s14-1.txt) |
| individual | Haiku | 2007588829 | 23:0 | [Gemini-Haiku-s13-1](individual/Gemini-Haiku-s13-1.txt) |
| individual | Hunyuan | 1855639151 | 10:10 | [Hunyuan-GLM-s10-2](individual/Hunyuan-GLM-s10-2.txt) |
| individual | Kimi | 2007588829 | 15:0 | [Kimi-Astra-s13-2](individual/Kimi-Astra-s13-2.txt) |
| individual | Opus | 1144914481 | 9:1 | [Opus-DeepSeek-s6-1](individual/Opus-DeepSeek-s6-1.txt) |
| individual | Step | 1023710713 | 0:24 | [Step-Terra-s3-1](individual/Step-Terra-s3-1.txt) |
| individual | Terra | 1144914481 | 0:20 | [Grok-Terra-s6-1](individual/Grok-Terra-s6-1.txt) |

The full three-event replay collection is about 1.75 GB and is omitted. All per-game results, seeds and seats are retained; see [reproduction](../tournament/README_en.md). One game is not a substitute for the full leaderboard.
