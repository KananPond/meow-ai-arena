"""Orchestration regressions; no compiler or submitted program is executed."""
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check


class ProcessTests(unittest.TestCase):
    def test_success_keeps_return_code_without_tree_kill(self):
        process = mock.Mock()
        process.wait.return_value = 7
        with mock.patch.object(check.subprocess, "Popen", return_value=process), \
                mock.patch.object(check.subprocess, "run") as cleanup:
            result = check.run_tron_challenge(["python", "challenge.py"], ".", {}, 60)
        self.assertEqual(result.returncode, 7)
        cleanup.assert_not_called()

    def test_timeout_kills_tree_before_reaping_parent(self):
        events = []
        process = mock.Mock(pid=1234)
        def wait(timeout):
            events.append("wait")
            if len(events) == 1:
                raise subprocess.TimeoutExpired("challenge", timeout)
            return 1
        process.wait.side_effect = wait
        process.poll.return_value = 1
        def taskkill(cmd, **kwargs):
            events.append("tree-kill")
            self.assertEqual(cmd, ["taskkill", "/PID", "1234", "/T", "/F"])
        with mock.patch.object(check.subprocess, "Popen", return_value=process), \
                mock.patch.object(check.subprocess, "run", side_effect=taskkill), \
                self.assertRaises(subprocess.TimeoutExpired):
            check.run_tron_challenge(["python", "challenge.py"], ".", {}, 60)
        self.assertEqual(events, ["wait", "tree-kill", "wait"])

    def test_compile_timeout_is_bounded_without_running_output(self):
        with mock.patch.object(check.subprocess, "run", side_effect=subprocess.TimeoutExpired("g++", 300)) as compile:
            exe, error = check.compile_in(".", ["bot.cpp"], "bot.exe", "g++", timeout=300)
        self.assertIsNone(exe)
        self.assertIn("5 分钟", error)
        self.assertEqual(compile.call_args.kwargs["timeout"], 300)


if __name__ == "__main__":
    unittest.main()
