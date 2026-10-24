[中文版](README.md)

# Example submissions

These three folders demonstrate the submission format. They do not enter the community leaderboard and are not anyone's actual submissions.

| Folder | Description |
|---|---|
| [ctf/chen-006__stand-still/](ctf/chen-006__stand-still/) | Bot submission: `bot.cpp` plus `meta.json`. The code is the episode-3 package template; all three roles stand still |
| [tank/chen-006__starter/](tank/chen-006__starter/) | Tank submission: `bot.cpp` plus `meta.json`. An unchanged copy of the official episode-6 starter, defining only `init` and `decide`, using the official `tank.hpp` and `tank_main.cpp`, with no custom `main` |
| [warehouse/chen-006__kimi-k3-round1/](warehouse/chen-006__kimi-k3-round1/) | Warehouse submission: `src/` plus `meta.json`. Kimi K3's round-1 code, unchanged |

Try the local checker on them:

```bash
python community/check.py community/_example/ctf/chen-006__stand-still
python community/check.py community/_example/tank/chen-006__starter --skip-eval
python community/check.py community/_example/tank/chen-006__starter
python community/check.py community/_example/warehouse/chen-006__kimi-k3-round1
```

Tank's default formal evaluation runs 320 games; `--full` runs 1280. `--skip-eval` only checks structure and compilation. Official final-tournament opponent ratings stay fixed; only the entrant's rating is fitted.

For an actual submission, put the folder under `community/<episode>/`, rather than `community/_example/<episode>/`. See the [submission guide](../README_en.md) for the rules.
