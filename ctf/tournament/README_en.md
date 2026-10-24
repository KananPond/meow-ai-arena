[中文原文](README.md)

# Reproduce the official tournament

The official tournament has two parts, rated together: 2–8 sides (`formal-2to8.json`, one copy per contestant) and 9–15 sides (`formal-9to15.json`, two copies per contestant available in the same game). Configs contain the original schedule seeds, so maps, seats and pairings can be reconstructed exactly.

Run from this directory (about 2 hours overall with 12 concurrent games; interrupt and resume with the same command):

```bash
python ../../arena/tournament.py plan formal-2to8.json  run-2to8
python ../../arena/tournament.py plan formal-9to15.json run-9to15
python ../../arena/tournament.py run  run-2to8  --workers 8
python ../../arena/tournament.py run  run-9to15 --workers 8
python ../../arena/tournament.py rate run-2to8 run-9to15
```

Flag-return spots use a separate random source, sampled afresh each game to prevent prediction from the map. Individual scores will therefore differ from the original tournament, though the overall ranking should agree. The 50 ms turn limit is sensitive to machine load: programs per game × concurrent games should preferably stay below about 3/4 of the CPU thread count.
