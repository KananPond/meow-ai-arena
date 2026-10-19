"""Community integration/security regression tests; no submitted program is run.

Run from the repository root: python -m unittest discover -s community/tests -v
"""
import argparse
import contextlib
import copy
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

COMMUNITY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(COMMUNITY))
import check
import ci_tools


META = {
    "author": "Test author", "github": "tester", "name": "Test bot",
    "description": "Community integration fixture", "written_by": "human",
    "license": "MIT",
}
BOT = '#include "tank.hpp"\nvoid init(const tank::State&, int) {}\ntank::Orders decide(const tank::State&, int) { return {}; }\n'


class CommunityFixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.here = self.root / "community"
        self.here.mkdir()
        # all_entries binds ROOT in its default argument at module import time.
        original_all_entries = ci_tools.all_entries
        patcher = mock.patch.object(ci_tools, "all_entries",
            side_effect=lambda root=None: original_all_entries(self.root if root is None else root))
        patcher.start()
        self.addCleanup(patcher.stop)
        for module in (check, ci_tools):
            for attr, value in (("HERE", self.here), ("ROOT", self.root)):
                patcher = mock.patch.object(module, attr, value)
                patcher.start()
                self.addCleanup(patcher.stop)
        for attr, value in (("RESULTS", self.here / "results"), ("BOARD", self.here / "LEADERBOARD.md")):
            patcher = mock.patch.object(ci_tools, attr, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def entry(self, episode="tank", name="tester__tank", meta=None, source=BOT):
        path = self.here / episode / name
        path.mkdir(parents=True, exist_ok=True)
        (path / "meta.json").write_text(json.dumps(META if meta is None else meta), encoding="utf-8")
        (path / "bot.cpp").write_text(source, encoding="utf-8")
        return path

    def record(self, entry):
        _, errors, _, files = check.validate(entry, entry.parent.name)
        self.assertEqual(errors, [])
        return {"schema": check.SCHEMA, "episode": entry.parent.name, "entry": entry.name,
                "status": "pass", "score": 123.5, "files": files, "mode": "quick",
                "errors": [], "time": "2026-10-19T00:00:00Z", "env": {"compiler": "g++"},
                "detail": {"rank": 3, "field_size": 3, "games_note": "320 games",
                           "rating_method": "fixed-final", "games": 320,
                           "budget": {"budget_kind": "cpu", "first_ms": 2000, "turn_ms": 50},
                           "field": [{"name": "Astra", "score": 506.9, "is_entry": False},
                                     {"name": "Test bot", "score": 123.5, "is_entry": True},
                                     {"name": "baseline", "score": 0, "is_entry": False}]}}

    def artifact(self, entry, record=None):
        artifacts = self.root / "artifacts"
        directory = artifacts / ("result-%s-%s" % (entry.parent.name, entry.name))
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "result.json").write_text(json.dumps(self.record(entry) if record is None else record), encoding="utf-8")
        return artifacts

    def scope(self, paths, author="tester", association="NONE"):
        args = argparse.Namespace(repo=self.root, base="base", head="head", author=author, association=association)
        buf = io.StringIO()
        code = 0
        with mock.patch.object(ci_tools, "changed_paths", return_value=paths), mock.patch.dict("os.environ", {}, clear=True), contextlib.redirect_stdout(buf):
            try:
                ci_tools.cmd_scope(args)
            except SystemExit as exc:
                code = exc.code
        return code, buf.getvalue()


class TankValidationTests(CommunityFixture):
    def test_registered_as_bot_episode(self):
        self.assertIn("tank", check.BOT_EPISODES)
        self.assertIn("tank", check.EPISODES)
        self.assertIn("6", check.EPISODE_TITLES["tank"])

    def test_valid_single_source_metadata_and_hash(self):
        entry = self.entry()
        meta, errors, _, files = check.validate(entry, "tank")
        self.assertEqual(errors, [])
        self.assertEqual(meta["github"], "tester")
        self.assertEqual(files, {"bot.cpp": check.sha256_file(entry / "bot.cpp")})

    def test_trusted_engine_cannot_be_supplied_in_entry(self):
        for name in ("tank.hpp", "tank_main.cpp", "core.py", "challenge.py", "extra.cpp"):
            with self.subTest(name=name):
                entry = self.entry(name="tester__" + name.replace(".", "-"))
                (entry / name).write_text("untrusted fixture", encoding="utf-8")
                self.assertTrue(check.validate(entry, "tank")[1])

    def test_rejects_nested_source_folder_and_symlink(self):
        entry = self.entry()
        (entry / "src").mkdir()
        self.assertTrue(check.validate(entry, "tank")[1])
        (entry / "src").rmdir()
        (entry / "bot.cpp").unlink()
        target = self.root / "outside.cpp"
        target.write_text(BOT, encoding="utf-8")
        (entry / "bot.cpp").symlink_to(target)
        self.assertTrue(check.validate(entry, "tank")[1])

    def test_metadata_and_entry_directory_symlinks_are_rejected(self):
        entry = self.entry()
        meta = entry / "meta.json"
        target = self.root / "outside-meta.json"
        meta.replace(target)
        meta.symlink_to(target)
        self.assertTrue(check.validate(entry, "tank")[1])
        meta.unlink()
        target.replace(meta)
        linked = self.here / "tank" / "tester__linked"
        linked.symlink_to(entry, target_is_directory=True)
        self.assertTrue(check.validate(linked, "tank")[1])

    def test_rejects_oversize_nontext_missing_source(self):
        entry = self.entry(source="x" * (check.MAX_BOT + 1))
        self.assertTrue(check.validate(entry, "tank")[1])
        (entry / "bot.cpp").write_bytes(b"a\x00b")
        self.assertTrue(check.validate(entry, "tank")[1])
        (entry / "bot.cpp").write_bytes(b"\xff")
        self.assertTrue(check.validate(entry, "tank")[1])
        (entry / "bot.cpp").unlink()
        self.assertTrue(check.validate(entry, "tank")[1])

    def test_metadata_author_model_license_and_field_rules(self):
        cases = ({"github": "someone-else"}, {"written_by": "ai", "model": ""},
                 {"license": "proprietary"}, {"unexpected": "field"}, {"author": "bad\nauthor"})
        for fields in cases:
            with self.subTest(fields=fields):
                self.assertTrue(check.validate(self.entry(meta={**META, **fields}), "tank")[1])

    def test_suspicious_code_remains_advisory(self):
        entry = self.entry(source=BOT + '\nvoid suspicious() { system("fixture"); }\n')
        _, errors, warnings, _ = check.validate(entry, "tank")
        self.assertEqual(errors, [])
        self.assertTrue(warnings)

    def test_tank_thread_warning_preserves_native_allowance(self):
        entry = self.entry(source='#include <thread>\n' + BOT)
        _, errors, warnings, _ = check.validate(entry, "tank")
        self.assertEqual(errors, [])
        self.assertFalse(any("或多线程" in warning for warning in warnings),
                         "Tank permits threads and charges their combined CPU time")

    def test_invalid_names_and_wrong_metadata_identity(self):
        for name in ("tester__Bad", "tester__-bad", "tester__two--hyphens", "tester__newline\n", "tester__" + "x" * 41,
                     "x" * 40 + "__name"):
            with self.subTest(name=name):
                self.assertTrue(check.validate(self.entry(name=name), "tank")[1])


class TankScopeTests(CommunityFixture):
    def test_single_owned_entry_is_submission(self):
        entry = self.entry()
        code, output = self.scope(["community/tank/%s/bot.cpp" % entry.name,
                                   "community/tank/%s/meta.json" % entry.name])
        self.assertEqual(code, 0)
        self.assertIn("kind=submission", output)
        self.assertIn("entry=community/tank/tester__tank", output)

    def test_other_authors_entry_is_rejected(self):
        self.entry()
        code, output = self.scope(["community/tank/tester__tank/bot.cpp"], author="intruder")
        self.assertEqual(code, 1)
        self.assertIn("kind=invalid", output)

    def test_two_entries_and_mixed_infrastructure_are_rejected(self):
        self.entry()
        for extra in ("community/tank/tester__second/bot.cpp", "community/ctf/tester__other/bot.cpp",
                      "community/check.py", ".github/workflows/community-pr.yml", "tank/workspace/tank.hpp"):
            with self.subTest(extra=extra):
                code, output = self.scope(["community/tank/tester__tank/bot.cpp", extra])
                self.assertEqual(code, 1)
                self.assertIn("kind=invalid", output)

    def test_generated_result_and_board_edits_are_rejected(self):
        for path in ("community/results/tank/tester__tank.json", "community/LEADERBOARD.md"):
            code, output = self.scope([path])
            self.assertEqual(code, 1)
            self.assertIn("kind=invalid", output)

    def test_maintainer_mixed_change_is_not_evaluated(self):
        self.entry()
        code, output = self.scope(["community/tank/tester__tank/bot.cpp", "community/check.py"], association="OWNER")
        self.assertEqual(code, 0)
        self.assertIn("kind=other", output)
        self.assertNotIn("kind=submission", output)

    def test_removal_does_not_run_code(self):
        code, output = self.scope(["community/tank/tester__tank/bot.cpp"])
        self.assertEqual(code, 0)
        self.assertIn("kind=removal", output)

    def test_discovery_includes_tank_and_excludes_examples(self):
        self.entry()
        self.entry(episode="ctf", name="tester__flag")
        self.entry(episode="_example/tank", name="tester__sample")
        self.assertEqual(ci_tools.all_entries(self.root), [("ctf", "tester__flag"), ("tank", "tester__tank")])

    def test_plan_all_includes_tank(self):
        self.entry()
        with mock.patch.dict("os.environ", {}, clear=True), contextlib.redirect_stdout(io.StringIO()) as output:
            ci_tools.cmd_plan(argparse.Namespace(entries="all", before="", after="HEAD"))
        self.assertIn('"episode": "tank"', output.getvalue())
        self.assertIn("count=1", output.getvalue())


class TankArtifactTests(CommunityFixture):
    def test_collection_uses_repository_identity_and_hash(self):
        entry = self.entry()
        raw = self.record(entry)
        raw["meta"] = {"author": "Spoofed author", "github": "intruder"}
        artifacts = self.artifact(entry, raw)
        rec, _ = ci_tools.collect_one("tank", entry.name, artifacts, "trusted-commit")
        self.assertEqual(rec["status"], "pass")
        self.assertEqual(rec["score"], 123.5)
        self.assertEqual(rec["commit"], "trusted-commit")
        self.assertNotIn("meta", rec)
        self.assertEqual([r["name"] for r in rec["detail"]["field"]], ["Astra", "baseline"])

    def test_stale_hash_and_wrong_identity_never_pass(self):
        entry = self.entry()
        raw = self.record(entry)
        raw["files"]["bot.cpp"] = "0" * 64
        artifacts = self.artifact(entry, raw)
        rec, _ = ci_tools.collect_one("tank", entry.name, artifacts, "commit")
        self.assertIsNone(rec)
        raw = self.record(entry)
        raw["entry"] = "tester__somebody-else"
        rec, _ = ci_tools.collect_one("tank", entry.name, self.artifact(entry, raw), "commit")
        self.assertNotEqual(rec["status"], "pass")

    def test_nonfinite_bool_or_text_score_never_passes(self):
        entry = self.entry()
        for value in (float("nan"), float("inf"), True, "123", None):
            with self.subTest(value=value):
                raw = self.record(entry)
                raw["score"] = value
                rec, _ = ci_tools.collect_one("tank", entry.name, self.artifact(entry, raw), "commit")
                self.assertNotEqual(rec["status"], "pass")
                self.assertIsNone(rec["score"])

    def test_tank_collection_rejects_non_native_or_incomplete_results(self):
        entry = self.entry()
        valid = self.record(entry)
        mutations = [lambda x: x["detail"]["budget"].update(budget_kind="wall"),
                     lambda x: x["detail"]["budget"].update(turn_ms=500),
                     lambda x: x["detail"]["budget"].update(first_ms=4000),
                     lambda x: x["detail"].update(rating_method="refitted"),
                     lambda x: x["detail"].pop("rating_method"),
                     lambda x: x["detail"].update(games=319),
                     lambda x: x.update(mode="full"),
                     lambda x: x.update(mode="skip-eval")]
        for index, mutate in enumerate(mutations):
            with self.subTest(index=index):
                raw = copy.deepcopy(valid)
                mutate(raw)
                rec, _ = ci_tools.collect_one("tank", entry.name, self.artifact(entry, raw), "commit")
                self.assertNotEqual(rec["status"], "pass")
                self.assertIsNone(rec["score"])

    def test_tank_collection_preserves_native_evaluation_metadata(self):
        entry = self.entry()
        rec, _ = ci_tools.collect_one("tank", entry.name, self.artifact(entry), "commit")
        self.assertEqual(rec["detail"]["rating_method"], "fixed-final")
        self.assertEqual(rec["detail"]["games"], 320)
        self.assertEqual(rec["detail"]["budget"],
                         {"budget_kind": "cpu", "first_ms": 2000, "turn_ms": 50})

    def test_only_planned_results_are_collected(self):
        first = self.entry()
        second = self.entry(name="tester__second")
        artifacts = self.artifact(first)
        self.artifact(second)
        with contextlib.redirect_stdout(io.StringIO()):
            ci_tools.cmd_collect(argparse.Namespace(artifacts=artifacts, commit="commit",
                plan=json.dumps({"include": [{"episode": "tank", "entry": first.name}]})))
        self.assertTrue((ci_tools.RESULTS / "tank" / (first.name + ".json")).exists())
        self.assertFalse((ci_tools.RESULTS / "tank" / (second.name + ".json")).exists())

    def test_oversize_and_missing_result_fail_safely(self):
        entry = self.entry()
        rec, _ = ci_tools.collect_one("tank", entry.name, self.root / "missing", "commit")
        self.assertEqual(rec["status"], "missing")
        artifacts = self.artifact(entry)
        result = artifacts / ("result-tank-" + entry.name) / "result.json"
        result.write_text(" " * (512 * 1024 + 1), encoding="utf-8")
        rec, _ = ci_tools.collect_one("tank", entry.name, artifacts, "commit")
        self.assertNotEqual(rec["status"], "pass")

    def test_artifact_symlink_is_not_followed(self):
        entry = self.entry()
        artifacts = self.artifact(entry)
        result = artifacts / ("result-tank-" + entry.name) / "result.json"
        target = self.root / "outside-result.json"
        result.replace(target)
        result.symlink_to(target)
        rec, _ = ci_tools.collect_one("tank", entry.name, artifacts, "commit")
        self.assertNotEqual(rec["status"], "pass")
        self.assertIsNone(rec["score"])

    def test_board_uses_repo_metadata_escapes_and_contains_tank(self):
        entry = self.entry(meta={**META, "name": "<script>|[bad]", "author": "Actual author"})
        raw = self.record(entry)
        raw["meta"] = {"author": "Spoofed author"}
        artifacts = self.artifact(entry, raw)
        with contextlib.redirect_stdout(io.StringIO()):
            ci_tools.cmd_collect(argparse.Namespace(artifacts=artifacts, commit="commit",
                plan=json.dumps({"include": [{"episode": "tank", "entry": entry.name}]})))
            ci_tools.cmd_render(argparse.Namespace())
        board = ci_tools.BOARD.read_text(encoding="utf-8")
        self.assertIn(check.EPISODE_TITLES["tank"], board)
        self.assertIn("tank/tester__tank/", board)
        self.assertIn("Actual author", board)
        self.assertNotIn("Spoofed author", board)
        self.assertNotIn("<script>", board)
        self.assertIn("&lt;script&gt;", board)


class ExistingEpisodeParserTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.run = Path(self.tmp.name)

    def write(self, relative, value):
        target = self.run / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(value), encoding="utf-8")

    def test_land_parser_retains_two_mode_rating(self):
        self.write("ratings.json", {"models": {
            "your-bot@you": {"score": 95, "modes": {"duel": {"theta": 1.0}, "melee": {"theta": 2.0}}},
            "greedy": {"score": 0, "modes": {"duel": {"theta": 0}, "melee": {"theta": 0}}}},
            "modes": {"duel": {"games": 20}, "melee": {"games": 30}}})
        field, extra = check.parse_land(self.run)
        self.assertTrue(field[0]["is_entry"])
        self.assertEqual(field[0]["score"], 260.6)
        self.assertEqual(field[0]["total"], 95)
        self.assertEqual(extra["pairs"], 50)

    def test_bomberman_parser_retains_entry_and_game_count(self):
        self.write("bt.json", {"ratings": {"你的 bot": {"rating": 123.45}, "baseline": 0}})
        self.write("results.json", [{}, {}, {}])
        field, extra = check.parse_bomberman(self.run)
        self.assertEqual(field[0], {"name": "你的 bot", "is_entry": True, "score": 123.5})
        self.assertEqual(extra["games_note"], "3 局")

    def test_ctf_parser_retains_confidence_intervals(self):
        self.write("games/成绩.json", {"ratings": {"你的 bot": [123.45, 100.1, 150.9], "基准": [0, -3, 3]}})
        (self.run / "games/结果.jsonl").write_text('{}\n\n{}\n', encoding="utf-8")
        field, extra = check.parse_ctf(self.run)
        self.assertEqual(field[0]["ci95"], [100.1, 150.9])
        self.assertTrue(field[0]["is_entry"])
        self.assertEqual(extra["games_note"], "2 局")

    def test_existing_episode_specs_remain_registered(self):
        for episode in ("land", "bomberman", "ctf"):
            self.assertIn(episode, check.BOT_EPISODES)
            self.assertIn(episode, check.BOT_SPEC)
        self.assertIn("warehouse", check.EPISODES)
        self.assertNotIn("warehouse", check.BOT_EPISODES)

    def test_existing_real_entries_still_validate(self):
        for episode in ("land", "ctf"):
            for entry in (COMMUNITY / episode).iterdir():
                if entry.is_dir() and (entry / "meta.json").is_file():
                    with self.subTest(entry=entry.name):
                        self.assertEqual(check.validate(entry, episode)[1], [])


if __name__ == "__main__":
    unittest.main()
