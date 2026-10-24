"""Tron challenge schedule, fixed reference and fail-closed result contract.

All match results here are synthetic; these tests do not run submission code.
Native Windows judge coverage belongs to the Windows smoke job.
"""
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import types
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("tron_challenge", ROOT / "tron" / "challenge.py")
challenge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(challenge)
TABLE = json.loads((ROOT / "tron/results/results.json").read_text(encoding="utf-8"))["boards"]["强对手榜"]["score"]["table"]
BUDGET = {"budget_kind": "cpu", "first_ms": 2000, "turn_ms": 50}


def records(games):
    """Everyone dies on the same turn: a deterministic all-draw fixture."""
    index = {"me": 0, **{name: i + 1 for i, name in enumerate(challenge.OPPONENTS)}}
    return [{"id": i, "n": n, "seed": seed, "bots": [index[name] for name in seats], "turns": 6, "death_turn": [5] * n, "timeouts": [0] * n, "invalid": [0] * n,
             "crash_turn": [-1] * n, "first_ms": [1.0] * n, "max_ms": [2.0] * n}
            for i, (_, n, seed, seats) in enumerate(games)]


class ScheduleTests(unittest.TestCase):
    def test_quick_and_full_schedule_preserve_original_contract(self):
        names = list(challenge.OPPONENTS)
        self.assertEqual(len(names), 13)
        self.assertIn("sparring_hard", names)
        for full, multiplier in ((False, 1), (True, 4)):
            games = challenge.schedule(names, full)
            self.assertEqual(games, challenge.schedule(names, full))
            self.assertEqual(len(games), 82 * multiplier)
            self.assertEqual({m: sum(g[0] == m for g in games) for m in challenge.MODES},
                             {"1v1": 52 * multiplier, "3-4": 24 * multiplier, "12": 6 * multiplier})
            duels = [g for g in games if g[0] == "1v1"]
            for a, b in zip(duels[::2], duels[1::2]):
                self.assertEqual(a[:3], b[:3])
                self.assertEqual(a[3], list(reversed(b[3])))
            for mode, n, seed, seats in games:
                self.assertEqual(len(seats), n)
                self.assertEqual(len(set(seats)), n)
                self.assertEqual(seats.count("me"), 1)
                self.assertGreaterEqual(seed, 5001)
        self.assertEqual(challenge.MODES, {"1v1": .5, "3-4": .3, "12": .2})

    def test_bt_fit_preserves_fixed_reference_calibration(self):
        self.assertAlmostEqual(challenge.fit([(100, 0, 20, 0)]), 100, places=4)
        self.assertGreater(challenge.fit([(100, 20, 0, 0)]), 100)
        self.assertLess(challenge.fit([(100, 0, 0, 20)]), 100)

    def test_result_preserves_reference_scores_and_excludes_sparring_from_rank(self):
        games = challenge.schedule(list(challenge.OPPONENTS), False)
        recs = records(games)
        with mock.patch.object(challenge, "fit", return_value=320.0):
            result = challenge.make_result(games, recs, TABLE, "a" * 64, False, BUDGET)
        self.assertEqual(result["rank"], 1)  # +324 sparring_hard never takes a rank.
        self.assertEqual(result["rating"], 320)
        self.assertEqual(result["weights"], challenge.MODES)
        for row in result["opponents"]:
            name = row["name"]
            self.assertEqual(row["rating"], TABLE[name]["total"])
            self.assertEqual(row["ranked"], name != "sparring_hard")
            self.assertEqual(row["ratings"], {m: TABLE[name]["modes"][m]["score"] for m in challenge.MODES})
        self.assertEqual(sum(sum(r["tally"]["1v1"]) for r in result["opponents"]), 52)
        self.assertEqual(sum(sum(r["tally"]["3-4"]) for r in result["opponents"]), 24 * 3)
        self.assertEqual(sum(sum(r["tally"]["12"]) for r in result["opponents"]), 6 * 11)

    def test_win_draw_loss_uses_death_turn_not_rank_score(self):
        games = [("1v1", 2, 5001, ["me", "Mistral"]),
                 ("3-4", 4, 5002, ["me", "Mistral", "LongCat", "Doubao"]),
                 ("12", 12, 5003, ["me"] + list(challenge.OPPONENTS)[:11])]
        recs = records(games)
        recs[0]["death_turn"] = [-1, 10]
        recs[0]["turns"] = 11
        recs[1]["death_turn"] = [4, 2, 4, 5]
        result = challenge.make_result(games, recs, TABLE, "a" * 64, False, BUDGET)
        rows = {r["name"]: r for r in result["opponents"]}
        self.assertEqual(rows["Mistral"]["tally"]["1v1"], [1, 0, 0])
        self.assertEqual(rows["Mistral"]["tally"]["3-4"], [1, 0, 0])
        self.assertEqual(rows["LongCat"]["tally"]["3-4"], [0, 1, 0])
        self.assertEqual(rows["Doubao"]["tally"]["3-4"], [0, 0, 1])


class ChallengeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        self.source = self.directory / "bot.cpp"
        self.source.write_text("trusted synthetic fixture", encoding="utf-8")
        self.output = self.directory / "run"
        self.ws = self.directory / "workspace"
        (self.ws / "bots").mkdir(parents=True)
        (self.ws / "bots/sparring_hard.exe").write_bytes(b"not executed")
        shutil.copy(ROOT / "tron/workspace/tron.hpp", self.ws / "tron.hpp")
        self.native_calls = []

    def run_challenge(self, full=False, mutate_records=None, mutate_source=False, budget=None, output=True, here=None):
        def run_games(entries, games, formal, workers):
            self.native_calls.append((entries, games, formal, workers))
            result = records(challenge.schedule(list(challenge.OPPONENTS), full))
            if mutate_records:
                mutate_records(result)
            if mutate_source:
                self.source.write_text("changed source", encoding="utf-8")
            return result

        arena = types.SimpleNamespace(Entry=lambda value: value, run_games=run_games, Fail=RuntimeError)
        argv = ["challenge.py", str(self.source), "--require-cpu"]
        if output:
            argv += ["--out", str(self.output)]
        if full:
            argv.append("--full")
        with contextlib.ExitStack() as stack:
            stack.enter_context(mock.patch.object(sys, "argv", argv))
            stack.enter_context(mock.patch.object(challenge, "require_windows"))
            stack.enter_context(mock.patch.object(challenge, "ensure_compiler"))
            stack.enter_context(mock.patch.object(challenge, "load_arena", return_value=arena))
            stack.enter_context(mock.patch.object(challenge, "WS", self.ws))
            stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
            if budget is not None:
                stack.enter_context(mock.patch.object(challenge, "budget_info", return_value=budget))
            if here is not None:
                stack.enter_context(mock.patch.object(challenge, "HERE", here))
            challenge.main()

    def test_default_82_games_formal_fixed_reference_result(self):
        self.run_challenge()
        result = json.loads((self.output / "result.json").read_text(encoding="utf-8"))
        self.assertIs(result["complete"], True)
        self.assertEqual(result["schema"], 1)
        self.assertEqual(result["mode"], "formal")
        self.assertEqual(result["rating_method"], "fixed-final-strong")
        self.assertEqual(result["reference"], {"path": "tron/results/results.json", "board": "强对手榜", "anchor": "Doubao"})
        self.assertEqual(result["games"], 82)
        self.assertEqual(result["games_by_mode"], {"1v1": 52, "3-4": 24, "12": 6})
        self.assertEqual(result["budget"], BUDGET)
        self.assertEqual(result["source_sha256"], hashlib.sha256(self.source.read_bytes()).hexdigest())
        self.assertEqual(result["seed"], 5001)
        self.assertFalse(result["full"])
        self.assertEqual(len(self.native_calls), 1)
        self.assertIs(self.native_calls[0][2], True)
        self.assertEqual(len(self.native_calls[0][0]), 14)
        self.assertEqual(len(self.native_calls[0][1]), 82)
        self.assertAlmostEqual(result["rating"], sum(challenge.MODES[m] * result["ratings"][m] for m in challenge.MODES))
        report = (self.output / "report.md").read_text(encoding="utf-8")
        self.assertIn("固定", report)
        self.assertIn("不占名次", report)
        self.assertIn("不是本次重新测出", report)

    def test_full_328_games_keeps_formal_mode_and_budget(self):
        self.run_challenge(full=True)
        result = json.loads((self.output / "result.json").read_text(encoding="utf-8"))
        self.assertEqual(result["games"], 328)
        self.assertEqual(result["games_by_mode"], {"1v1": 208, "3-4": 96, "12": 24})
        self.assertTrue(result["full"])
        self.assertEqual(result["budget"], BUDGET)
        self.assertIs(self.native_calls[0][2], True)

    def test_incomplete_or_malformed_matches_do_not_produce_result(self):
        mutations = [lambda r: r.pop(), lambda r: r[0].update(id=99),
                     lambda r: r[0].update(n=4), lambda r: r[0].update(seed=1),
                     lambda r: r[0]["bots"].reverse(), lambda r: r[0].update(turns=True),
                     lambda r: r[0].update(death_turn=[-1, -1]),
                     lambda r: r[0]["death_turn"].__setitem__(0, 6),
                     lambda r: r[0]["death_turn"].pop(), lambda r: r[0]["timeouts"].__setitem__(0, True),
                     lambda r: r[0]["invalid"].__setitem__(0, -1),
                     lambda r: r[0]["crash_turn"].__setitem__(0, -2),
                     lambda r: r[0]["first_ms"].__setitem__(0, float("nan")),
                     lambda r: r[0]["max_ms"].__setitem__(0, float("inf"))]
        for mutation in mutations:
            with self.subTest(mutation=mutation), self.assertRaises(SystemExit):
                self.run_challenge(mutate_records=mutation)
            self.assertFalse((self.output / "result.json").exists())
            self.assertFalse((self.output / "report.md").exists())

    def test_source_change_does_not_produce_result(self):
        with self.assertRaises(SystemExit):
            self.run_challenge(mutate_source=True)
        self.assertFalse((self.output / "result.json").exists())

    def test_existing_result_or_report_is_not_silently_reused(self):
        self.output.mkdir()
        for name in ("result.json", "report.md"):
            path = self.output / name
            path.write_text("old result", encoding="utf-8")
            with self.subTest(name=name), self.assertRaises(SystemExit):
                self.run_challenge()
            self.assertEqual(path.read_text(encoding="utf-8"), "old result")
            path.unlink()
        self.assertEqual(self.native_calls, [])

    def test_cpu_gate_fails_before_matches(self):
        for budget in ({**BUDGET, "budget_kind": "wall"}, {**BUDGET, "turn_ms": 500}):
            with self.subTest(budget=budget), self.assertRaises(SystemExit):
                self.run_challenge(budget=budget)
        self.assertEqual(self.native_calls, [])
        self.assertFalse((self.output / "result.json").exists())

    def test_budget_info_reads_official_constants(self):
        self.assertEqual(challenge.budget_info(), BUDGET)
        (self.ws / "tron.hpp").write_text("missing constants", encoding="utf-8")
        with mock.patch.object(challenge, "WS", self.ws), self.assertRaises(SystemExit):
            challenge.budget_info()

    def test_default_runs_use_unique_directories(self):
        here = self.directory / "tron"
        (here / "results").mkdir(parents=True)
        shutil.copy(ROOT / "tron/results/results.json", here / "results/results.json")
        self.run_challenge(output=False, here=here)
        self.run_challenge(output=False, here=here)
        self.assertEqual(len(list((here / "challenge-runs").glob("*/result.json"))), 2)

    @unittest.skipIf(sys.platform == "win32", "non-Windows gate")
    def test_non_windows_explicitly_rejected(self):
        with self.assertRaisesRegex(SystemExit, "Windows"):
            challenge.require_windows()


if __name__ == "__main__":
    unittest.main()
