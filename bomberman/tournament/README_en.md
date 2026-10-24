[中文原文](README.md)

# Reproduce the official tournament

`evaluate.py` and `bt.py` are the unchanged official evaluation and rating scripts. `run.py` was added for the open-source release: it first compiles C++ entries with the official flags, then calls `evaluate.py`.

From the episode directory:

```bash
python tournament/run.py --formal --output rerun/对战      # frozen schedule, 8930 games; several hours with 4 workers; adjust with --workers
```

`schedule-2to10.json.gz` is the frozen official schedule (map seeds and seat order); `--formal` extracts and uses it automatically. `evaluate.py` first verifies contestant-package hashes (`workspace/MANIFEST.json`) and refuses to run if package files have changed.

Some bots control search depth by elapsed time, so per-game results need not exactly match `results/games.json.gz`, though the overall ranking should agree.
