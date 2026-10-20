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

    def isolated_row(self, mode="offscreen"):
        properties = {
            "WORKING_DIRECTORY": "/build/tests/end_to_end",
            "LABELS": ["bpr_isolated_correctness", f"bpr_correctness_{mode}"],
            "RESOURCE_LOCK": ["bpr_isolated_correctness_fixture"],
        }
        if mode == "offscreen":
            properties["ENVIRONMENT"] = ["QT_QPA_PLATFORM=offscreen"]
        return {"name": "isolated", "properties": [
            {"name": name, "value": value} for name, value in properties.items()]}

    def test_only_complete_isolated_correctness_declarations_admit_concurrency(self):
        for mode in ("headless", "offscreen"):
            suite.validate_schedule({"tests": [self.isolated_row(mode)]})
        corruptions = [
            ("RESOURCE_LOCK", []),
            ("RESOURCE_LOCK", ["some_unrelated_lock"]),
            ("ENVIRONMENT", ["QT_QPA_PLATFORM=cocoa"]),
            ("ENVIRONMENT", ["QT_QPA_PLATFORM=offscreen", "QT_QPA_PLATFORM=windows"]),
            ("ENVIRONMENT_MODIFICATION", ["QT_QPA_PLATFORM=set:windows"]),
            ("RUN_SERIAL", True),
            ("LABELS", ["bpr_correctness_offscreen"]),
            ("LABELS", ["bpr_isolated_correctness", "bpr_correctness_native"]),
            ("LABELS", ["bpr_isolated_correctness", "bpr_correctness_offscreen", "bpr_correctness_headless"]),
            ("LABELS", ["bpr_isolated_correctness", "bpr_correctness_offscreen", "bpr_explicit_serial"]),
            ("LABELS", ["bpr_isolated_correctness", "bpr_correctness_offscreen", "bpr_serial_gui"]),
        ]
        for name, value in corruptions:
            with self.subTest(property=name, value=value):
                row = self.isolated_row()
                row["properties"] = [prop for prop in row["properties"] if prop["name"] != name]
                row["properties"].append({"name": name, "value": value})
                with self.assertRaisesRegex(ValueError, "Invalid isolated correctness"):
                    suite.validate_schedule({"tests": [row]})
        for argument in ("-platform", "-platform=windows", "--platform", "--platform=cocoa"):
            row = self.isolated_row()
            row["command"] = ["application", argument]
            with self.assertRaisesRegex(ValueError, "Invalid isolated correctness"):
                suite.validate_schedule({"tests": [row]})
        row = self.isolated_row("headless")
        row["properties"].append({"name": "ENVIRONMENT", "value": ["QT_QPA_PLATFORM=offscreen"]})
        with self.assertRaisesRegex(ValueError, "Invalid isolated correctness"):
            suite.validate_schedule({"tests": [row]})

    def configure_schedule(self, declaration, *, environment="QT_QPA_PLATFORM=offscreen", before_default=""):
        with ContainedTemporaryDirectory() as temporary:
            source = Path(temporary) / "tests/end_to_end"
            source.mkdir(parents=True)
            build = Path(temporary) / "build"
            module = (ROOT / "cmake/BprTestScheduling.cmake").as_posix()
            (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.24)
project(Scheduling NONE)
enable_testing()
include("{module}")
add_test(NAME one COMMAND "${{CMAKE_COMMAND}}" -E true)
add_test(NAME untouched COMMAND "${{CMAKE_COMMAND}}" -E true)
add_test(NAME explicit COMMAND "${{CMAKE_COMMAND}}" -E true)
add_test(NAME seed COMMAND "${{CMAKE_COMMAND}}" -E true)
set_tests_properties(seed PROPERTIES FIXTURES_SETUP seed_fixture)
set_tests_properties(one PROPERTIES ENVIRONMENT "{environment}"
    LABELS maintained_label RESOURCE_LOCK retained_lock FIXTURES_REQUIRED seed_fixture TIMEOUT 17)
set_tests_properties(explicit PROPERTIES RUN_SERIAL TRUE)
{before_default}
bpr_schedule_test_directory("${{CMAKE_CURRENT_SOURCE_DIR}}")
{declaration}
''')
            result = subprocess.run(["cmake", "-G", "Ninja", "-S", str(source), "-B", str(build)],
                                    capture_output=True, text=True, check=False)
            if result.returncode:
                return result, None
            manifest = subprocess.run(["ctest", "--test-dir", str(build), "--show-only=json-v1"],
                                      capture_output=True, text=True, check=True)
            return result, json.loads(manifest.stdout)

    def test_cmake_opt_in_preserves_locks_fixtures_timeout_and_unclassified_exclusivity(self):
        for mode, environment in (("offscreen", "QT_QPA_PLATFORM=offscreen"), ("headless", "")):
            result, manifest = self.configure_schedule(
                f'bpr_allow_isolated_correctness(DIRECTORY "${{CMAKE_CURRENT_SOURCE_DIR}}" MODE {mode} RESOURCE fixture TESTS one)',
                environment=environment)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            rows = {row["name"]: {prop["name"]: prop["value"] for prop in row["properties"]}
                    for row in manifest["tests"]}
            self.assertFalse(rows["one"].get("RUN_SERIAL", False))
            self.assertEqual(set(rows["one"]["RESOURCE_LOCK"]), {"retained_lock", "bpr_isolated_correctness_fixture"})
            self.assertEqual(rows["one"]["FIXTURES_REQUIRED"], ["seed_fixture"])
            self.assertEqual(rows["one"]["TIMEOUT"], 17)
            self.assertIn("maintained_label", rows["one"]["LABELS"])
            self.assertTrue(rows["untouched"]["RUN_SERIAL"])
            self.assertTrue(rows["explicit"]["RUN_SERIAL"])
            self.assertIn("bpr_explicit_serial", rows["explicit"]["LABELS"])
            suite.validate_schedule(manifest)

    def test_cmake_refuses_unsafe_or_incomplete_opt_ins(self):
        declaration = 'bpr_allow_isolated_correctness(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" MODE offscreen RESOURCE fixture TESTS one)'
        cases = [
            (declaration.replace("RESOURCE fixture", ""), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration.replace("MODE offscreen", "MODE native"), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration.replace("TESTS one", "TESTS missing"), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration.replace("TESTS one", "TESTS explicit"), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration, "QT_QPA_PLATFORM=cocoa", ""),
            (declaration, "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM=windows", ""),
            (declaration, "QT_QPA_PLATFORM=offscreen", 'set_tests_properties(one PROPERTIES ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:windows")'),
            (declaration.replace("MODE offscreen", "MODE headless"), "QT_QPA_PLATFORM=offscreen", ""),
        ]
        for expression, environment, before in cases:
            with self.subTest(declaration=expression, environment=environment, before=before):
                result, _ = self.configure_schedule(expression, environment=environment, before_default=before)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

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
