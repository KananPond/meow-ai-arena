[简体中文](README.md)

# Source for the three reference teams

`bots.hpp` and `bot_main.cpp` are the shared reference-bot source. Only the relative engine include in `bots.hpp` was adjusted. Contestants received executables only.

| Executable | Compile-time BOT_NAME |
|---|---|
| baseline | plan16 |
| midfield | plan32_b1.5 |
| sparring | pro_b0_n128 |

Run `python build_reference.py` from the parent folder. It uses the original `-std=c++17 -O2 -static -s` flags, outputs to `workspace/bots/` and builds the referee. Each reference team fields two copies. Reference games do not count toward points or places, but are used for reference records and BT fitting.
