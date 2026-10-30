[简体中文](README.md)

# Historical package and team-event instructions

| File | Contents |
|---|---|
| [format.md](format.md) | Original team-event template prepended to the base README after filling parameters; [translation](format_en.md) |
| [TEAMCHAT_README.md](TEAMCHAT_README.md) | Identical original from all 12 contestant folders, retaining UTF-8 BOM and line endings; [translation](TEAMCHAT_README_en.md) |
| [player_pack_manifest.json](player_pack_manifest.json) | All 23 initial files per slot, shared team files, runtime categories and six half-time packages; historical fixed-file hashes and public sources |
| [restore_en.md](restore_en.md) | File layout, README parameters, identity/clock interfaces, half-time reconstruction and limitations |
| [timing.json](timing.json) | Valid half start, seal, credit and net durations for 12 slots; selected timestamp fields only |

Six teams actually had a five-minute meeting and 40 minutes per half, with 32/44 seeds at half time/final. The initially distributed README still said 25 minutes for the second half and described team scoring only. The [half-time message](../prompt_en.md) announced 40 minutes, individual teams and 50%/50% weighting. Use 25 when reconstructing the initial README, rather than substituting the later configuration. The template's no-substantial-copying rule supersedes the older base rules.

The original chat instructions were checked for machine paths, internal routes and credentials before publication. They conditionally say to check messages **if** a reminder appears. Only `clock.py` actually integrated reminders; the other tools did not. See [conditions](../CONDITIONS_en.md); the original is not rewritten.

This supplies the complete inventory and reconstruction instructions, not a one-command generator of all historical services. Original clients such as `clock.py` and `tc.py` have static hashes listed, but live timing, identity configuration, chat storage and organizer services are omitted. Local development/match material can be reconstructed; a new event also needs independently configured timing/chat services. Do not use the full source tree as a contest-identical player package.
