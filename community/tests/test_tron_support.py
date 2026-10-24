"""Tron submission contract and publisher regressions; no bot is executed.

Synthetic results use all-draw outcomes on the official deterministic schedule.
Native compilation tests are explicitly Windows-only and do not run binaries.
"""
import argparse
import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "community"))
import check
import ci_tools

spec = importlib.util.spec_from_file_location("tron_support_challenge", ROOT / "tron/challenge.py")
challenge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(challenge)
TABLE = json.loads((ROOT / "tron/results/results.json").read_text(encoding="utf-8"))["boards"]["强对手榜"]["score"]["table"]
META = {"author": "Test author", "github": "tester", "name": "Test bot",
        "description": "Tron contract fixture", "written_by": "human", "license": "MIT"}
BOT = '#include "tron.hpp"\nint decide(const tron::State&) { return tron::U; }\n'
BUDGET = {"budget_kind": "cpu", "first_ms": 2000, "turn_ms": 50}


def draw_document(source_sha="a" * 64, full=False):
    names = list(challenge.OPPONENTS)
    games = challenge.schedule(names, full)
    counts = {mode: 0 for mode in challenge.MODES}
    tallies = {name: {mode: [0, 0, 0] for mode in challenge.MODES} for name in names}
    for mode, _, _, seats in games:
        counts[mode] += 1
        for name in seats:
            if name != "me":
                tallies[name][mode][1] += 1
    opponents = [{"name": name, "ranked": name != "sparring_hard", "rating": TABLE[name]["total"],
                  "ratings": {mode: TABLE[name]["modes"][mode]["score"] or 0.0 for mode in challenge.MODES},
                  "tally": tallies[name]} for name in names]
    ratings = {mode: challenge.fit([(row["ratings"][mode], *row["tally"][mode])
                                   for row in opponents if sum(row["tally"][mode])])
               for mode in challenge.MODES}
    total = sum(challenge.MODES[mode] * ratings[mode] for mode in challenge.MODES)
    return {"schema": 1, "complete": True, "mode": "formal", "rating_method": "fixed-final-strong",
            "budget": dict(BUDGET), "reference": {"path": "tron/results/results.json", "board": "强对手榜", "anchor": "Doubao"},
            "source_sha256": source_sha, "seed": 5001, "full": full, "games": len(games),
            "games_by_mode": counts, "weights": dict(challenge.MODES), "ratings": ratings,
            "rating": total, "rank": 1 + sum(row["ranked"] and row["rating"] > total for row in opponents),
            "opponents": opponents}


class TronFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.here = self.root / "community"
        self.here.mkdir()
        # The contract loads only these trusted files, never submission Python.
        for relative in ("tron/challenge.py", "tron/results/results.json"):
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        original_all_entries = ci_tools.all_entries
        patcher = mock.patch.object(ci_tools, "all_entries", side_effect=lambda root=None:
                                    original_all_entries(self.root if root is None else root))
        patcher.start()
        self.addCleanup(patcher.stop)
        for module in (check, ci_tools):
            for attr, value in (("ROOT", self.root), ("HERE", self.here)):
                patcher = mock.patch.object(module, attr, value)
                patcher.start()
                self.addCleanup(patcher.stop)
        for attr, value in (("RESULTS", self.here / "results"), ("BOARD", self.here / "LEADERBOARD.md")):
            patcher = mock.patch.object(ci_tools, attr, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def entry(self, name="tester__tron", source=BOT, meta=None, episode="tron"):
        path = self.here / episode / name
        path.mkdir(parents=True, exist_ok=True)
        (path / "bot.cpp").write_text(source, encoding="utf-8")
        (path / "meta.json").write_text(json.dumps(META if meta is None else meta), encoding="utf-8")
        return path

    def parse(self, document):
        run = self.root / "run"
        run.mkdir(exist_ok=True)
        (run / "result.json").write_text(json.dumps(document), encoding="utf-8")
        return check.parse_tron(run)

    def record(self, entry, full=False):
        _, errors, _, files = check.validate(entry, "tron")
        self.assertEqual(errors, [])
        field, detail = self.parse(draw_document(files["bot.cpp"], full))
        detail.update(field=field, field_size=len(field))
        return {"schema": check.SCHEMA, "episode": "tron", "entry": entry.name, "status": "pass",
                "score": round(detail["rating"], 1), "files": files, "mode": "full" if full else "quick",
                "errors": [], "warnings": [], "time": "2026-10-24T00:00:00Z", "env": {}, "detail": detail}

    def artifact(self, entry, record):
        artifacts = self.root / "artifacts"
        directory = artifacts / ("result-tron-" + entry.name)
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "result.json").write_text(json.dumps(record), encoding="utf-8")
        return artifacts

    def collect(self, entry, record):
        return ci_tools.collect_one("tron", entry.name, self.artifact(entry, record), "trusted-commit")[0]

    def scope(self, paths, author="tester"):
        args = argparse.Namespace(repo=self.root, base="base", head="head", author=author, association="NONE")
        with mock.patch.object(ci_tools, "changed_paths", return_value=paths), \
                mock.patch.dict("os.environ", {}, clear=True), contextlib.redirect_stdout(io.StringIO()) as output:
            try:
                ci_tools.cmd_scope(args)
            except SystemExit as exc:
                return exc.code, output.getvalue()
        return 0, output.getvalue()


class TronValidationTests(TronFixture):
    def test_registered_and_single_source_metadata(self):
        self.assertIn("tron", check.BOT_EPISODES)
        self.assertIn("tron", check.EPISODES)
        self.assertIn("7", check.EPISODE_TITLES["tron"])
        self.assertIn("tron", check.BOT_SPEC)
        entry = self.entry()
        meta, errors, _, files = check.validate(entry, "tron")
        self.assertEqual(errors, [])
        self.assertEqual(meta["github"], "tester")
        self.assertEqual(files, {"bot.cpp": check.sha256_file(entry / "bot.cpp")})

    def test_rejects_supplied_helpers_and_sibling_sources(self):
        for filename in ("tron.hpp", "tron_main.cpp", "challenge.py", "arena.py", "helper.hpp", "extra.cpp"):
            with self.subTest(filename=filename):
                entry = self.entry(name="tester__" + filename.replace(".", "-"))
                (entry / filename).write_text("not trusted", encoding="utf-8")
                self.assertTrue(check.validate(entry, "tron")[1])
        entry = self.entry(name="tester__nested")
        (entry / "src").mkdir()
        self.assertTrue(check.validate(entry, "tron")[1])

    def test_thread_warning_does_not_ban_permitted_threads(self):
        entry = self.entry(source="#include <thread>\n" + BOT)
        _, errors, warnings, _ = check.validate(entry, "tron")
        self.assertEqual(errors, [])
        self.assertTrue(warnings)
        self.assertTrue(any("允许线程" in warning for warning in warnings))
        self.assertFalse(any("或多线程" in warning for warning in warnings))

    @unittest.skipIf(sys.platform == "win32", "non-Windows fail-closed check")
    def test_non_windows_does_not_compile_or_run(self):
        result = {"env": {}, "errors": [], "detail": {}, "mode": "skip-eval"}
        with mock.patch.object(check, "compile_in") as compiler, mock.patch.object(check, "run_tron_challenge") as runner:
            check.eval_bot(self.entry(), "tron", self.root / "out", False, 60, result)
        self.assertTrue(result["errors"])
        self.assertIn("Windows", result["errors"][0])
        compiler.assert_not_called()
        runner.assert_not_called()


class TronParserTests(TronFixture):
    def test_quick_and_full_fixed_reference_counts_and_rank(self):
        for full, games, modes in ((False, 82, {"1v1": 52, "3-4": 24, "12": 6}),
                                   (True, 328, {"1v1": 208, "3-4": 96, "12": 24})):
            with self.subTest(full=full):
                doc = draw_document(full=full)
                field, detail = self.parse(doc)
                self.assertEqual(detail["games"], games)
                self.assertEqual(detail["games_by_mode"], modes)
                self.assertEqual(len(detail["opponents"]), 13)
                self.assertEqual(len(field), 13)  # 12 ranked references plus entrant
                self.assertNotIn("sparring_hard", [row["name"] for row in field])
                hard = next(row for row in detail["opponents"] if row["name"] == "sparring_hard")
                self.assertFalse(hard["ranked"])
                self.assertGreater(sum(hard["tally"]["1v1"]), 0)
                self.assertEqual(detail["rank"], 1 + sum(TABLE[n]["total"] > doc["rating"]
                                 for n in challenge.OPPONENTS if n != "sparring_hard"))
                self.assertEqual(detail["weights"], {"1v1": .5, "3-4": .3, "12": .2})

    def test_contract_rejects_malformed_and_changed_scoring(self):
        valid = draw_document()
        mutations = {
            "schema bool": lambda d: d.update(schema=True),
            "fast": lambda d: d.update(mode="fast"),
            "incomplete": lambda d: d.update(complete=False),
            "wall": lambda d: d["budget"].update(budget_kind="wall"),
            "relaxed budget": lambda d: d["budget"].update(turn_ms=500),
            "full type": lambda d: d.update(full=1),
            "truncated schedule": lambda d: d.update(games=81),
            "float count": lambda d: d["games_by_mode"].update({"12": 6.0}),
            "seed": lambda d: d.update(seed=5002),
            "hash": lambda d: d.update(source_sha256="not-a-hash"),
            "refitted": lambda d: d.update(rating_method="refitted"),
            "anchor": lambda d: d["reference"].update(anchor="greedy"),
            "weights": lambda d: d["weights"].update({"1v1": .4}),
            "missing opponent": lambda d: d["opponents"].pop(),
            "duplicate opponent": lambda d: d["opponents"].__setitem__(0, copy.deepcopy(d["opponents"][1])),
            "reference score": lambda d: d["opponents"][0].update(rating=999),
            "reference part": lambda d: d["opponents"][0]["ratings"].update({"1v1": 999}),
            "sparring ranked": lambda d: d["opponents"][-1].update(ranked=True),
            "negative tally": lambda d: d["opponents"][0]["tally"]["1v1"].__setitem__(0, -1),
            "boolean tally": lambda d: d["opponents"][0]["tally"]["1v1"].__setitem__(0, False),
            "wrong count": lambda d: d["opponents"][0]["tally"]["1v1"].__setitem__(1, 3),
            "total": lambda d: d.update(rating=d["rating"] + 1),
            "part": lambda d: d["ratings"].update({"1v1": d["ratings"]["1v1"] + 1}),
            "nan": lambda d: d.update(rating=float("nan")),
            "rank": lambda d: d.update(rank=d["rank"] + 1),
            "zero bool reference": lambda d: next(r for r in d["opponents"] if r["name"] == "Doubao")["ratings"].update({"1v1": False}),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                doc = copy.deepcopy(valid)
                mutate(doc)
                with self.assertRaises((ValueError, TypeError, KeyError)):
                    self.parse(doc)


class TronScopeTests(TronFixture):
    def test_owned_submission_and_other_owner_rejection(self):
        self.entry()
        paths = ["community/tron/tester__tron/bot.cpp", "community/tron/tester__tron/meta.json"]
        code, text = self.scope(paths)
        self.assertEqual(code, 0)
        self.assertIn("kind=submission", text)
        self.assertIn("entry=community/tron/tester__tron", text)
        self.assertEqual(self.scope(paths, author="intruder")[0], 1)

    def test_mixed_infrastructure_and_multiple_entries_rejected(self):
        self.entry()
        for extra in ("community/check.py", "tron/workspace/tron.hpp", ".github/workflows/community-pr.yml",
                      "community/tron/tester__other/bot.cpp"):
            with self.subTest(extra=extra):
                self.assertEqual(self.scope(["community/tron/tester__tron/bot.cpp", extra])[0], 1)

    def test_discovery_and_plan_include_tron_not_examples(self):
        self.entry()
        self.entry(name="tester__example", episode="_example/tron")
        self.assertEqual(ci_tools.all_entries(self.root), [("tron", "tester__tron")])
        with mock.patch.dict("os.environ", {}, clear=True), contextlib.redirect_stdout(io.StringIO()) as output:
            ci_tools.cmd_plan(argparse.Namespace(entries="all", before="", after="HEAD"))
        self.assertIn('"episode": "tron"', output.getvalue())
        self.assertIn("count=1", output.getvalue())


class TronPublisherTests(TronFixture):
    def test_valid_quick_and_full_preserve_fixed_reference(self):
        entry = self.entry()
        for full in (False, True):
            with self.subTest(full=full):
                raw = self.record(entry, full)
                raw["meta"] = {"author": "Forged author"}
                record = self.collect(entry, raw)
                self.assertEqual(record["status"], "pass")
                self.assertAlmostEqual(record["score"], raw["score"])
                self.assertEqual(record["detail"]["games"], 328 if full else 82)
                self.assertEqual(record["detail"]["rating_method"], "fixed-final-strong")
                self.assertEqual(record["detail"]["budget"], BUDGET)
                self.assertNotIn("meta", record)

    def test_malformed_json_and_wrong_entry_identity_never_pass(self):
        entry = self.entry()
        raw = self.record(entry)
        artifacts = self.artifact(entry, raw)
        path = artifacts / ("result-tron-" + entry.name) / "result.json"
        path.write_text("{broken JSON", encoding="utf-8")
        record, _ = ci_tools.collect_one("tron", entry.name, artifacts, "commit")
        self.assertNotEqual(record["status"], "pass")
        self.assertIsNone(record["score"])
        for changes in ({"episode": "tank"}, {"entry": "intruder__tron"}):
            with self.subTest(changes=changes):
                changed = copy.deepcopy(raw)
                changed.update(changes)
                record = self.collect(entry, changed)
                self.assertNotEqual(record["status"], "pass")
                self.assertIsNone(record["score"])

    def test_stale_hash_is_not_published(self):
        entry = self.entry()
        raw = self.record(entry)
        raw["files"]["bot.cpp"] = "b" * 64
        self.assertIsNone(self.collect(entry, raw))

    def test_rejects_forged_or_incomplete_results(self):
        entry = self.entry()
        valid = self.record(entry)
        mutations = {
            "missing detail": lambda r: r.update(detail={}),
            "changed score": lambda r: r.update(score=r["score"] + 50),
            "nonfinite score": lambda r: r.update(score=float("inf")),
            "source mismatch": lambda r: r["detail"].update(source_sha256="b" * 64),
            "fast": lambda r: r["detail"].update(mode="fast"),
            "wall": lambda r: r["detail"]["budget"].update(budget_kind="wall"),
            "incomplete": lambda r: r["detail"].update(complete=False),
            "wrong count": lambda r: r["detail"].update(games=81),
            "wrong mode": lambda r: r.update(mode="full"),
            "no evaluation": lambda r: r.update(mode="skip-eval"),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                raw = copy.deepcopy(valid)
                mutate(raw)
                record = self.collect(entry, raw)
                self.assertIsNotNone(record)
                self.assertNotEqual(record["status"], "pass")
                self.assertIsNone(record["score"])

    def test_render_uses_repository_metadata_escapes_and_fixed_reference_language(self):
        entry = self.entry(meta={**META, "name": "<script>|[bad]", "author": "Actual author"})
        raw = self.record(entry)
        raw["meta"] = {"author": "Forged author"}
        artifacts = self.artifact(entry, raw)
        with contextlib.redirect_stdout(io.StringIO()):
            ci_tools.cmd_collect(argparse.Namespace(artifacts=artifacts, commit="commit",
                plan=json.dumps({"include": [{"episode": "tron", "entry": entry.name}]})))
            ci_tools.cmd_render(argparse.Namespace())
        board = ci_tools.BOARD.read_text(encoding="utf-8")
        self.assertIn("tron/tester__tron/", board)
        self.assertIn("Actual author", board)
        self.assertNotIn("Forged author", board)
        self.assertNotIn("<script>", board)
        self.assertIn("&lt;script&gt;", board)
        self.assertIn("强对手榜", board)
        self.assertIn("固定", board)
        self.assertIn("sparring_hard", board)
        self.assertNotIn("小比赛名次", board)
        self.assertNotIn("官方选手参照：每份投稿评测时", board)


@unittest.skipUnless(sys.platform == "win32" and check.find_compiler(), "native Tron compilation requires Windows and g++")
class TronNativeCompileTests(unittest.TestCase):
    def test_only_trusted_header_and_main_are_compiled(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entry = root / "entry"
            entry.mkdir()
            (entry / "bot.cpp").write_text(BOT, encoding="utf-8")
            for filename in ("tron.hpp", "tron_main.cpp"):
                (entry / filename).write_text('#error "untrusted sibling"\n', encoding="utf-8")
            result = {"env": {}, "errors": [], "detail": {}, "mode": "skip-eval"}
            with contextlib.redirect_stdout(io.StringIO()):
                check.eval_bot(entry, "tron", root / "work", False, 60, result)
            self.assertEqual(result["errors"], [])
            self.assertTrue(result["detail"]["compiled"])
            for filename in ("tron.hpp", "tron_main.cpp"):
                self.assertEqual((root / "work/build" / filename).read_bytes(),
                                 (ROOT / "tron/workspace" / filename).read_bytes())


if __name__ == "__main__":
    unittest.main()
