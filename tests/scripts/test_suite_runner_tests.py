#!/usr/bin/env python3
"""Exercise complete-suite verdicts, timing interpretation and GUI scheduling."""
from __future__ import annotations

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
import run_test_suite as suite
from skip_report import SkipReportingTestRunner

class SuiteRunnerTests(unittest.TestCase):
    def test_historical_log_keeps_failure_skip_and_wall_time_distinct(self):
        with ContainedTemporaryDirectory() as temporary:
            path = Path(temporary) / "ctest.log"
            path.write_text("1/3 Test #1: slow .....***Failed Error regex found503.31 sec\n"
                            "2/3 Test #2: quick ..... Passed 0.20 sec\n"
                            "3/3 Test #3: unavailable .....***Skipped 0.01 sec\n"
                            "Total Test time (real) = 504.12 sec\n")
            report = suite.read_log(path)
            self.assertEqual(report["counts"], {"failed": 1, "passed": 1, "skipped": 1})
            self.assertAlmostEqual(report["summed_test_seconds"], 503.52)
            self.assertEqual(report["wall_seconds"], 504.12)
            self.assertEqual(report["tests"][0]["name"], "slow")

    def verdict(self, xml: str, code: int = 0):
        calls = []
        with ContainedTemporaryDirectory() as temporary:
            output = Path(temporary) / "results"
            def ctest(command, **kwargs):
                calls.append(command)
                if "--show-only=json-v1" in command:
                    return subprocess.CompletedProcess(command, 0, json.dumps({"tests": [{"name": "one"}, {"name": "two"}]}))
                (output / "junit.xml").write_text(xml)
                return subprocess.CompletedProcess(command, code)
            with patch.object(suite.subprocess, "run", side_effect=ctest), redirect_stdout(io.StringIO()):
                result = suite.run_suite("dev", 4, output, root=Path(temporary))
            report = json.loads((output / "timings.json").read_text())
            self.assertEqual(calls[1][0:5], ["ctest", "--preset", "dev", "--parallel", "4"])
            self.assertIn("--no-tests=error", calls[1])
            self.assertNotIn("--rerun-failed", calls[1])
            return result, report

    def test_failure_and_nonzero_exit_cannot_become_success(self):
        failing = '<testsuite><testcase name="one" time="8"><failure/></testcase><testcase name="two" time="3"/></testsuite>'
        self.assertNotEqual(self.verdict(failing)[0], 0)
        passing = failing.replace("<failure/>", "")
        self.assertEqual(self.verdict(passing, 8)[0], 8)

    def test_complete_coverage_and_overlapping_durations(self):
        xml = '<testsuite><testcase name="one" time="8"/><testcase name="two" time="3"><skipped/></testcase></testsuite>'
        result, report = self.verdict(xml)
        self.assertEqual(result, 0)
        self.assertEqual(report["counts"], {"passed": 1, "skipped": 1})
        self.assertEqual(report["summed_test_seconds"], 11)
        self.assertTrue(report["complete"])
        self.assertNotEqual(self.verdict('<testsuite><testcase name="one"/></testsuite>')[0], 0)
        self.assertNotEqual(self.verdict('<testsuite><testcase name="one"/><testcase name="one"/></testsuite>')[0], 0)

    def test_gui_and_performance_courses_cannot_run_concurrently(self):
        for prop in ({"name": "ENVIRONMENT", "value": ["QT_QPA_PLATFORM=offscreen"]},
                     {"name": "WORKING_DIRECTORY", "value": "C:\\build\\tests\\end_to_end"},
                     {"name": "LABELS", "value": ["bpr_serial_gui"]}):
            row = {"name": "interactive", "properties": [prop]}
            with self.assertRaisesRegex(ValueError, "not serial"):
                suite.validate_schedule({"tests": [row]})
            row["properties"].append({"name": "RUN_SERIAL", "value": True})
            suite.validate_schedule({"tests": [row]})
        suite.validate_schedule({"tests": [{"name": "core"}]})

    def test_full_suite_lock_refuses_overlap_and_releases_on_failure(self):
        with ContainedTemporaryDirectory() as temporary:
            path = Path(temporary) / "suite.lock"
            with self.assertRaisesRegex(ValueError, "test failure"):
                with suite.suite_lock(path):
                    with self.assertRaisesRegex(RuntimeError, "Another full-suite runner"):
                        with suite.suite_lock(path):
                            self.fail("overlapping gate acquired the lock")
                    raise ValueError("test failure")
            with suite.suite_lock(path):
                pass


if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
