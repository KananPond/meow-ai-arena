[简体中文](README.md)

# Episode 7 — Legacy Code Optimization: results and verdicts

Except for this document and `counterexamples/README.md`, these are organizer evaluation-script outputs. Unlike Episode 5's `results/`, they **were sanitized** and are not byte-identical originals:

- Absolute paths became placeholders: `<主办方评测目录>/` (event evaluation folder), `<主办方目录>/` (other organizer projects), `<选手目录>/` (contestant workspace), and `~/` (home). Paths to files included here became repository-relative paths, such as `submissions/ep7/step-5-preview` and `results/ep7/counterexamples/…`.
- Internal event numbering differed from the published episode number. Prefixes were normalized to `ep7` (such as `ep7_Step`); new Astra counterexamples use `astra_`; “Episode N” references became “this event” or “Episode 5”.
- JSON was reindented; values and fields were unchanged.

| File | Contents |
|---|---|
| [汇总.md — English](汇总_en.md) | Unified table: correctness on 8 workloads, geometric and auxiliary speedups, counterexample counts/types, optimization time, and notes on baseline reuse, calibration, screening, background load, and special treatment. **Authoritative numbers**, subject to the notes below |
| `结果.json` | Full underlying data: per-run CPU/wall/memory/output hashes, counterexample verdicts, screening, manual decisions, supplemental runs, baseline timings and counterexample baseline timings |
| `提交清单.json` | Sealed manifests for 12 timed submissions: file SHA-256, time used, collection at deadline, and added seconds |
| `编译错误.json` | Original LongCat compiler diagnostics |
| `反例池清单.json` | Sources and SHA-256 of the 10 shared-pool cases |
| `独立复现_成立清单.json` | 18 Astra counterexamples independently upheld by the organizer: affected code, verdict, and discovery round |
| `独立复现_结果.json` | Full independent reproduction: validity checks for 53 candidates, baseline timings, and repeated runs against the relevant submissions, including rejected candidates |
| `counterexamples/` | Inputs and baseline outputs; see [the index](counterexamples/README_en.md) |

## Reading the summary

- **The table includes reference entries.** Of 26 rows, 12 marked “this event” are new submissions measured with exclusive machine use; 6 original Episode 5 lead submissions are `../../submissions/round1/` (Doubao's row supplies its event rank); 8 marked “10-17” come from a separate organizer evaluation on 10-17 (five GPT model/settings combinations and three DeepSeek trials). That evaluation's code and materials are absent here. These 14 rows reuse 10-17 measurements on the same machine/compiler/hidden inputs. Two calibration rows remeasure existing entries to check machine conditions; the ratios are 1.027 and 0.985.
- **The baseline was reused**, with CPU times from 10-17. Two baseline runs differed by −3.8% to +5.0% per workload and 0.65% geometrically over all 8. Individual speedups may therefore carry about ±5% systematic error, and aggregate speedups about ±1%. The user declared ties between 139.4× and 138.2×, and between 8.7× and 8.6×, within that margin.
- **Optimization times are raw collection timestamps.** Batch 2's four OpenCode entries (Step, Hunyuan, Mercury, Mistral) were not stopped automatically at 45 minutes and were collected manually over 10 minutes later, producing 56–62 minute entries. Their effective time was 45 minutes, and post-deadline `src/` changes were reverted. The three Claude sessions waited after finishing, producing 45–46 minute entries; their last responses give 26.0–33.8 minutes. Corrected times are in [the episode README](../../README_en.md) and [conditions](../../CONDITIONS_en.md).
- **Counterexamples count inputs, not root causes.** “12 pending” means 12 new Astra inputs were still untested. All 26 were completed only for timed ranked entries (Step, Hunyuan, Qwen, Mercury, ERNIE, and Episode 5 Doubao); Claude and reference rows received only partial coverage. Zero means no failure on those inputs, not proof of no defects. Targeted searches also differ: the 12 current entries had this event's two Astra rounds, 10-17 entries had that evaluation's Astra search, and old Episode 5 entries had only their original mutual testing. Counts cannot compare resistance to counterexample discovery.
- **Pangu's “pending independent reproduction” note describes the state when written.** Later reproduction confirmed both output errors and 5 timeout inputs; see `独立复现_成立清单.json`. Pangu did not run the shared counterexample evaluation because it timed out on 7 of 8 hidden workloads; that cell says “not run”.
- Mistral and dots are byte-identical to the baseline; correctness/counterexamples were recorded as baseline-identical without separate runs. Their 0.95× and 0.74× screening measurements are single-run variation.
