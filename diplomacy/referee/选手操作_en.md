English translation for reference; the contestants received the original Chinese text.

[Chinese original](选手操作.md)

# Operation

Whenever there are new messages or a new phase, you receive a briefing directly. Finish your reply when done; no waiting or polling is needed.

Run all commands from this directory:

```text
python game.py send ENGLAND "Proposal: you enter Belgium; I stay out of Burgundy."
python game.py send GLOBAL "Public statement"
python game.py orders "A PAR - BUR" "A MAR S A PAR - BUR" "F BRE - MAO"
python game.py draw        vote for an agreed draw (undraw withdraws the vote)
```

- `orders` submits all orders for the current phase at once. To change them, resubmit the full set. An invalid order lists the legal options for that unit.
- To waive winter builds: `python game.py orders` (no orders).

Queries when needed:

```text
python game.py brief                 full current position
python game.py legal PAR BUR         legal orders for specific units (no arguments: all)
python game.py inbox --with FRANCE   all messages with a power (also --phase S1901M)
python game.py history               recent adjudications (also --phase F1901M)
python game.py simulate 假设.json    simulate hypothetical orders without affecting the real game
```

`假设.json` format: `{"FRANCE": ["A PAR - BUR"], "GERMANY": ["A MUN - BUR"]}`. Unspecified units hold.

You may keep notes and write helper programs for calculations in this directory.
