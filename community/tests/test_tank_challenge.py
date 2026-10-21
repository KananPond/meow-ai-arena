"""Tank challenge schema, fail-closed CPU gate, and native compile smoke tests."""
import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "community"))
import check

spec = importlib.util.spec_from_file_location("tank_challenge", ROOT / "tank" / "challenge.py")
challenge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(challenge)
FINAL = json.loads((ROOT / "tank/results/final/成绩.json").read_text(encoding="utf-8"))["players"]
BUDGET = {"budget_kind": "cpu", "first_ms": 2000, "turn_ms": 50}


class ChallengeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        self.source = self.directory / "bot.cpp"
        self.source.write_text("trusted test fixture", encoding="utf-8")
        self.output = self.directory / "run"

    def run_challenge(self, full=False, budget=None, counts=None, mutate=False):
        calls = []

        def batch(cmd, **kwargs):
            calls.append(cmd)
            games = int(cmd[cmd.index("--games") + 1])
            w, d, l = counts if counts is not None else (0, games, 0)
            if mutate:
                self.source.write_text("changed fixture", encoding="utf-8")
            return subprocess.CompletedProcess(cmd, 0, f"汇总：fixture {w}-{d}-{l}（胜-平-负） 故障：无\n", "")

        argv = ["challenge.py", str(self.source), "--out", str(self.output), "--require-cpu"]
        if full:
            argv.append("--full")
        with mock.patch.object(sys, "argv", argv), mock.patch.object(challenge, "ensure_compiler"), \
                mock.patch.object(challenge, "budget_info", return_value=BUDGET if budget is None else budget), \
                mock.patch.object(challenge.subprocess, "run", side_effect=batch), \
                contextlib.redirect_stdout(io.StringIO()):
            challenge.main()
        return calls

    def test_default_320_games_formal_and_fixed_reference_result(self):
        calls = self.run_challenge()
        self.assertEqual(len(calls), 16)
        self.assertTrue(all(cmd[4] == "batch" for cmd in calls))
        self.assertTrue(all(cmd[cmd.index("--games") + 1] == "20" for cmd in calls))
        self.assertTrue(all(cmd[cmd.index("--seed") + 1] == "1001" for cmd in calls))
        doc = json.loads((self.output / "result.json").read_text(encoding="utf-8"))
        self.assertEqual(doc["games"], 320)
        self.assertEqual(doc["source_sha256"], check.sha256_file(self.source))
        self.assertEqual(doc["rating_method"], "fixed-final")
        self.assertEqual(doc["budget"], BUDGET)
        field, extra = check.parse_tank(self.output)
        self.assertEqual(len(field), 15)  # fourteen AI references plus this submission
        self.assertEqual(sum(r["is_entry"] for r in field), 1)
        self.assertEqual(extra["rank"], doc["rank"])
        self.assertIn("固定", (self.output / "report.md").read_text(encoding="utf-8"))

    def test_full_1280_games_keeps_same_native_mode(self):
        calls = self.run_challenge(full=True)
        self.assertTrue(all(cmd[4] == "batch" and cmd[cmd.index("--games") + 1] == "80" for cmd in calls))
        self.assertEqual(check.parse_tank(self.output)[1]["games"], 1280)

    def test_rejects_wall_budget_and_changed_limits_before_matches(self):
        for budget in ({**BUDGET, "budget_kind": "wall"}, {**BUDGET, "turn_ms": 100}):
            with self.subTest(budget=budget), self.assertRaises(SystemExit):
                self.run_challenge(budget=budget)
        self.assertFalse((self.output / "result.json").exists())

    def test_incomplete_matches_do_not_produce_result(self):
        with self.assertRaises(SystemExit):
            self.run_challenge(counts=(0, 19, 0))
        self.assertFalse((self.output / "result.json").exists())

    def test_source_change_does_not_produce_result(self):
        with self.assertRaises(SystemExit):
            self.run_challenge(mutate=True)
        self.assertFalse((self.output / "result.json").exists())

    def test_existing_result_directory_is_not_silently_reused(self):
        self.run_challenge()
        with self.assertRaises(SystemExit):
            self.run_challenge()

    def test_parser_rejects_wrong_mode_budget_missing_games_and_reference(self):
        self.run_challenge()
        path = self.output / "result.json"
        valid = json.loads(path.read_text(encoding="utf-8"))
        mutations = [lambda x: x.update(mode="fast"), lambda x: x.update(complete=False),
                     lambda x: x.update(games=319), lambda x: x.update(rating=float("nan")),
                     lambda x: x["budget"].update(budget_kind="wall"),
                     lambda x: x["budget"].update(turn_ms=500),
                     lambda x: x["opponents"].pop(),
                     lambda x: x["opponents"][0].update(rating=999),
                     lambda x: x["opponents"][0].update(wins=True),
                     lambda x: x["opponents"][0].update(draws=19)]
        for mutate in mutations:
            doc = copy.deepcopy(valid)
            mutate(doc)
            path.write_text(json.dumps(doc), encoding="utf-8")
            with self.assertRaises((ValueError, TypeError, KeyError)):
                check.parse_tank(self.output)

    def test_bt_fit_preserves_reference_calibration(self):
        self.assertAlmostEqual(challenge.fit([(100, 0, 20, 0)]), 100, places=4)
        self.assertGreater(challenge.fit([(100, 20, 0, 0)]), 100)
        self.assertLess(challenge.fit([(100, 0, 0, 20)]), 100)


@unittest.skipUnless(check.find_compiler(), "native tests require g++ or clang++")
class NativeCompileTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        self.entry = self.directory / "tank" / "tester__native"
        self.entry.mkdir(parents=True)
        self.work = self.directory / "out"
        self.work.mkdir()

    def compile(self, source):
        (self.entry / "bot.cpp").write_text(source, encoding="utf-8")
        result = {"env": {}, "errors": [], "detail": {}, "mode": "skip-eval"}
        with contextlib.redirect_stdout(io.StringIO()):
            check.eval_bot(self.entry, "tank", self.work, False, 60, result)
        return result

    def test_native_hooks_compile_with_trusted_header_and_main(self):
        # Even a caller bypassing validate cannot substitute the trusted header/main.
        (self.entry / "tank.hpp").write_text('#error "untrusted header"\n', encoding="utf-8")
        (self.entry / "tank_main.cpp").write_text('#error "untrusted main"\n', encoding="utf-8")
        source = (ROOT / "tank/workspace/submission/bot.cpp").read_text(encoding="utf-8")
        result = self.compile(source)
        self.assertEqual(result["errors"], [])
        self.assertTrue(result["detail"]["compiled"])
        for name in ("tank.hpp", "tank_main.cpp"):
            self.assertEqual((self.work / "build" / name).read_bytes(), (ROOT / "tank/workspace" / name).read_bytes())

    def test_missing_hooks_fail_compilation(self):
        result = self.compile('#include "tank.hpp"\nvoid init(const tank::State&, int) {}\n')
        self.assertTrue(result["errors"])
        self.assertNotIn("compiled", result["detail"])

    def test_untrusted_sibling_include_is_not_available(self):
        (self.entry / "sibling.hpp").write_text('void init(const tank::State&, int) {}\n', encoding="utf-8")
        result = self.compile('#include "tank.hpp"\n#include "sibling.hpp"\ntank::Orders decide(const tank::State&, int) { return {}; }\n')
        self.assertTrue(result["errors"])
        self.assertNotIn("compiled", result["detail"])


if __name__ == "__main__":
    unittest.main()
