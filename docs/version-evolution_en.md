[中文版](version-evolution.md)

# Version evolution: do models get stronger or weaker as they edit? (Three games compared)

Method: replay each contestant's code edits from session records and take a snapshot whenever it compiles/runs the code, sampling at most 7 versions per contestant evenly over time. Each historical version challenges a fixed opponent pool (baseline + everyone's submitted version); historical versions never play one another. The submitted version is also retested as a challenger, so values in the same row are directly comparable.

Detailed tables: [ctf/appendix/version-evolution.md](../ctf/appendix/version-evolution.md), [bomberman/appendix/version-evolution.md](../bomberman/appendix/version-evolution.md), [land/appendix/version-evolution.md](../land/appendix/version-evolution.md). The session records themselves are not public.

## Submitted version minus its own historical best (negative = a stronger intermediate version was damaged or overwritten)

| Contestant | Capture the flag | Bomberman | Territory |
|---|---:|---:|---:|
| Astra | -19 | 0 | -6 |
| Sol | -16 | 0 | -129 * |
| Opus | +5 | Submitted version only | 0 |
| Sonnet | +12 | — | -44 |
| Fable | -23 | — | — |
| Kimi | Not tested † | -54 | -69 * |
| GLM | -51 | -16 | -12 |
| DeepSeek | -38 | 0 * | -51 * |
| Qwen | -49 | -42 | — |
| MiMo | -41 | -23 | -20 |
| 豆包 | -32 | -15 | — |

† This capture-the-flag experiment took place before Kimi K3 submitted, so K3 is absent.

The uncertainty for a single historical version is about ±40 (capture the flag) to ±70 (Bomberman/Territory, with fewer games); differences with an absolute value below 40 are generally indistinguishable.

\* marks cases where the replayed final state does not exactly match the submitted code (Sol Territory 95%, Kimi Territory 99%, DeepSeek Territory 97%, and only partial reconstruction for DeepSeek Bomberman). Intermediate versions may also be inaccurate.

## Observations

- Bomberman: the stronger models improve almost monotonically (Astra 223→371, Sol 119→274, with the submitted version the strongest). The rules are intuitive, feedback is clear, and local tests agree with actual playing strength.
- Capture the flag: almost nobody improves monotonically; most peak at an intermediate version. Only Opus and Sonnet submit their strongest version. Strategic interaction matters more, and self-tests (often against themselves or the baseline) can overfit.
- Territory: the largest swings. Sol reaches 271 at 00:51, later falls as low as -248, and submits at 142. Opus starts at 288, drops into the 190s, then recovers to 310.
- A common pattern among Chinese models: an intermediate peak followed by a decline, with the submitted version often weaker (Kimi Bomberman reaches 240 at 13:28 and submits at 186).
- Astra's first 4 Territory versions (02:51–03:11) crash in every game: the Bot object is on the stack and overflows Windows' default 1MB stack. Retesting with the original official compiler flags (`-O2 -std=c++20 -static`) confirms a real bug. The 03:19 version fixes it with `make_unique`, jumping directly from -227 to 252.
- Qwen's first Bomberman version fails in all 48 games; Doubao peaks with its second version, then makes no progress for over 20 minutes.

## Data gaps

- Bomberman: Opus ran in the cloud, so only the submitted version is available; 3 intermediate DeepSeek versions fail to compile after replay (possibly a reconstruction issue); MiMo's 18:27 version did not compile at the time.
- Territory: only 1 Spacebunny version could be reconstructed and it does not compile, leaving only the submitted version; MiMo's 05:21 version does not compile. Astra/Opus/Sonnet use version files saved by the contestants themselves; the others are reconstructed by replay.
