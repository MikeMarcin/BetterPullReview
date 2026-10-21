#!/usr/bin/env python3
"""Exercise complete-suite verdicts, timing interpretation and GUI scheduling."""
from __future__ import annotations

from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
import run_test_suite as suite
import release_phase_ledger as ledger
from skip_report import SkipReportingTestRunner

class SuiteRunnerTests(unittest.TestCase):
    def setUp(self):
        # Mocked CTest tests exercise admission independently of filesystem sealing.
        self.inputs = patch.object(suite, "bootstrap_inputs", return_value="fixture-identity")
        self.inputs.start()
        self.addCleanup(self.inputs.stop)

    def test_bootstrap_reuse_rejects_stale_missing_failed_and_interrupted_proof(self):
        names = sorted(suite.TOOLING_PREFLIGHT)
        valid = dict(invocation_id="live", input_identity="same", expected=names,
                     passed=True, complete=True, ctest_exit_code=0,
                     results={"counts": {"passed": len(names)}, "tests": [{"name": n, "status": "passed"} for n in names]})
        self.assertEqual(suite.reusable_bootstrap(valid, "live", "same"), names)
        for changed in ({"invocation_id": "previous"}, {"input_identity": "changed"},
                        {"passed": False}, {"complete": False}, {"ctest_exit_code": 8},
                        {"expected": names + ["unknown"]}, {"expected": names + names},
                        {"results": {"counts": {"skipped": 3}, "tests": []}}):
            with self.subTest(changed=changed):
                self.assertEqual(suite.reusable_bootstrap({**valid, **changed}, "live", "same"), [])
        self.assertEqual(suite.reusable_bootstrap(None, "live", "same"), [])
        self.assertEqual(suite.reusable_bootstrap(valid, "live", None), [])

    def test_bounded_selection_keeps_preflight_fresh_and_never_credits_other_rows(self):
        with ContainedTemporaryDirectory() as temporary:
            root = Path(temporary)
            calls = []
            names = sorted(suite.TOOLING_PREFLIGHT)
            def run(command, **kwargs):
                calls.append(command)
                if "--show-only=json-v1" in command:
                    return subprocess.CompletedProcess(command, 0, json.dumps({"tests": [{"name": n} for n in names + ["selected", "omitted"]]}))
                output = root / "results"
                preflight = "preflight-junit.xml" in str(command)
                actual = names if preflight else ["selected"]
                (output / ("preflight-junit.xml" if preflight else "junit.xml")).write_text(
                    '<testsuite>' + ''.join(f'<testcase name="{n}"/>' for n in actual) + '</testsuite>')
                return subprocess.CompletedProcess(command, 0)
            with patch.object(suite, "committed_tooling_pin", return_value={"fixture": "pin"}), patch.object(suite.subprocess, "run", side_effect=run), redirect_stdout(io.StringIO()):
                self.assertEqual(suite.run_suite("fixture", 1, root / "results", root=root, tests_regex="^selected$"), 0)
            report = json.loads((root / "results/timings.json").read_text())
            self.assertFalse(report["full_graph_selected"])
            self.assertEqual([row["name"] for row in report["tests"]], ["selected"])
            self.assertEqual(report["qualification_scope"]["fresh"], ["selected"])
            self.assertEqual(len(calls), 3)

    def test_actual_source_helper_tool_environment_and_manifest_identity_changes(self):
        # Exercise the real seal; no prior whole-source receipt is promoted.
        self.inputs.stop()
        with ContainedTemporaryDirectory() as temporary:
            root = Path(temporary)
            subprocess.run(["git", "init", "-q"], cwd=root, check=True)
            helper = root / ".agents/skills/implement-rfc/scripts"
            helper.mkdir(parents=True)
            (helper / "gate.py").write_text("gate-one")
            source = root / "source.py"
            source.write_text("source-one")
            tool = root / "tool"
            tool.write_text("tool-one")
            with patch.object(suite.shutil, "which", return_value=str(tool)):
                initial = suite.bootstrap_inputs(root, {"tests": []}, "ctest")
                for path in (source, helper / "gate.py", tool):
                    original = path.read_text()
                    path.write_text(original + "changed")
                    self.assertNotEqual(suite.bootstrap_inputs(root, {"tests": []}, "ctest"), initial)
                    path.write_text(original)
                self.assertNotEqual(suite.bootstrap_inputs(root, {"tests": [{"name": "changed"}]}, "ctest"), initial)
                with patch.dict(os.environ, {"BPR_QUALIFICATION_TEST_TRIGGER": "changed"}):
                    self.assertNotEqual(suite.bootstrap_inputs(root, {"tests": []}, "ctest"), initial)
                self.assertEqual(suite.bootstrap_inputs(root, {"tests": []}, "ctest"), initial)
        self.inputs.start()

    def test_python_optimization_is_refused_before_selection(self):
        # A full runner legitimately owns the real checkout lock while this
        # contract executes; this control owns a separate fixture checkout.
        with ContainedTemporaryDirectory() as temporary:
            root = Path(temporary)
            with patch.dict(os.environ, {"PYTHONOPTIMIZE": "1"}), self.assertRaisesRegex(ValueError, "Python assertions"):
                suite.run_suite("fixture", 1, root / "unused", root=root)

    def test_real_preflight_stops_failed_candidate_and_retains_accepted_full_inventory(self):
        for failed in (True, False):
            with self.subTest(failed=failed), ContainedTemporaryDirectory() as temporary:
                root = Path(temporary)
                names = sorted(suite.TOOLING_PREFLIGHT)
                worker = root / "worker.py"
                worker.write_text('''from pathlib import Path
import sys
root = Path(__file__).parent
name = sys.argv[1]
with (root / (name + ".runs")).open("a") as output:
    output.write("run\\n")
if name == "documentation_structure" and (root / "fail").exists():
    sys.exit(3)
''')
                cmake = ['cmake_minimum_required(VERSION 3.24)', 'project(Readiness NONE)', 'enable_testing()']
                for name in names + ["expensive_sentinel"]:
                    cmake.append(f'add_test(NAME {name} COMMAND "{Path(sys.executable).as_posix()}" "{worker.as_posix()}" {name})')
                (root / "CMakeLists.txt").write_text("\n".join(cmake) + "\n")
                (root / "CMakePresets.json").write_text(json.dumps({
                    "version": 3,
                    "configurePresets": [{"name": "fixture", "generator": "Ninja", "binaryDir": "${sourceDir}/build"}],
                    "testPresets": [{"name": "fixture", "configurePreset": "fixture"}],
                }))
                subprocess.run(["cmake", "--preset", "fixture"], cwd=root, check=True, capture_output=True)
                if failed:
                    (root / "fail").touch()
                output = root / "results"
                with patch.object(suite, "committed_tooling_pin", return_value={"fixture": "declared-pin"}), redirect_stdout(io.StringIO()):
                    result = suite.run_suite("fixture", 4, output, root=root)
                preflight = json.loads((output / "preflight.json").read_text())
                report = json.loads((output / "timings.json").read_text())
                self.assertEqual(preflight["passed"], not failed)
                self.assertEqual(preflight["results"]["counts"], {"failed": 1, "passed": 2} if failed else {"passed": 3})
                self.assertEqual(report["full_graph_started"], not failed)
                self.assertEqual(result == 0, not failed)
                self.assertEqual((root / "expensive_sentinel.runs").exists(), not failed)
                for name in names:
                    self.assertEqual((root / (name + ".runs")).read_text().splitlines(), ["run"])
                if failed:
                    self.assertFalse(report["complete"])
                    self.assertEqual(report["counts"], {})
                    self.assertEqual(set(report["not_run"]), set(names + ["expensive_sentinel"]))
                    self.assertFalse((output / "junit.xml").exists())
                else:
                    self.assertTrue(report["complete"])
                    self.assertEqual(report["counts"], {"passed": 4})
                    self.assertEqual(set(report["qualification_scope"]["reused"]), set(names))
                    self.assertEqual({row["name"] for row in report["tests"]}, set(names + ["expensive_sentinel"]))
                    self.assertGreaterEqual(report["total_runner_wall_seconds"], report["wall_seconds"] + preflight["wall_seconds"])

    def test_missing_or_invalid_committed_tooling_pin_stops_before_ctest(self):
        valid = {"source": "agentskills", "id": "implement-rfc", "commit": "a" * 40,
                 "payload_digest": "sha256:" + "b" * 64}
        for locked in ({"packages": []}, {"packages": [valid, valid]}, {"packages": [{}]},
                       {"packages": [{**valid, "payload_digest": "invalid"}]}, {"packages": "invalid"}):
            with self.subTest(locked=locked), ContainedTemporaryDirectory() as temporary:
                root = Path(temporary)
                output = root / "results"
                calls = []
                def run(command, **kwargs):
                    calls.append(command)
                    if "--show-only=json-v1" in command:
                        return subprocess.CompletedProcess(command, 0, json.dumps({"tests": [{"name": "documentation_structure"}, {"name": "expensive"}]}))
                    self.assertEqual(command, ["git", "show", "HEAD:.mask.lock.json"])
                    return subprocess.CompletedProcess(command, 0, json.dumps(locked).encode())
                with patch.object(suite.subprocess, "run", side_effect=run), redirect_stdout(io.StringIO()):
                    self.assertNotEqual(suite.run_suite("fixture", 4, output, root=root), 0)
                self.assertEqual(len(calls), 2)
                self.assertFalse(json.loads((output / "timings.json").read_text())["full_graph_started"])
                self.assertIn("error", json.loads((output / "preflight.json").read_text()))
        with patch.object(suite.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, json.dumps({"packages": [valid]}).encode())):
            declared = suite.committed_tooling_pin(ROOT)
        self.assertEqual(declared["commit"], valid["commit"])
        self.assertEqual(declared["payload_digest"], valid["payload_digest"])
        self.assertEqual(len(declared["lock_sha256"]), 64)

    def test_failed_skipped_or_incomplete_preflight_cannot_start_full_graph(self):
        names = sorted(suite.TOOLING_PREFLIGHT)
        passing = ''.join(f'<testcase name="{name}"/>' for name in names)
        cases = ((passing, 8), (passing.replace('/>', '><failure/></testcase>', 1), 0),
                 (passing.replace('/>', '><skipped/></testcase>', 1), 0),
                 (f'<testcase name="{names[0]}"/>', 0), (passing + '<testcase name="unexpected"/>', 0))
        for xml, code in cases:
            with self.subTest(xml=xml, code=code), ContainedTemporaryDirectory() as temporary:
                root = Path(temporary)
                output = root / "results"
                calls = []
                def ctest(command, **kwargs):
                    calls.append(command)
                    if "--show-only=json-v1" in command:
                        return subprocess.CompletedProcess(command, 0, json.dumps({"tests": [{"name": name} for name in names + ["expensive"]]}))
                    self.assertIn("--tests-regex", command)
                    (output / "preflight-junit.xml").write_text('<testsuite>' + xml + '</testsuite>')
                    return subprocess.CompletedProcess(command, code)
                with patch.object(suite, "committed_tooling_pin", return_value={"fixture": "pin"}), \
                        patch.object(suite.subprocess, "run", side_effect=ctest), redirect_stdout(io.StringIO()):
                    self.assertNotEqual(suite.run_suite("fixture", 4, output, root=root), 0)
                self.assertEqual(len(calls), 2)
                self.assertFalse(json.loads((output / "timings.json").read_text())["full_graph_started"])

    def test_composition_build_uses_bounded_producer_parallelism(self):
        script = ROOT / "tests/end_to_end/composition_contract_tests.py"
        with patch.object(sys, "argv", [str(script)]):
            composition = runpy.run_path(str(script), run_name="composition_build_control")
        for configured, expected in ((None, 2), ("", 2), ("1", 1), ("4", 4), ("8", 8), ("32", 8)):
            with self.subTest(configured=configured), patch.dict(os.environ):
                os.environ.pop("CMAKE_BUILD_PARALLEL_LEVEL", None)
                if configured is not None:
                    os.environ["CMAKE_BUILD_PARALLEL_LEVEL"] = configured
                self.assertEqual(composition["composition_build_jobs"](), expected)
        for configured in ("0", "-1", "invalid", " ", "1.5"):
            with self.subTest(configured=configured), patch.dict(os.environ, {"CMAKE_BUILD_PARALLEL_LEVEL": configured}):
                with self.assertRaisesRegex(ValueError, "positive integer"):
                    composition["composition_build_jobs"]()

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
            with patch.object(sys, "argv", ["run_test_suite.py", "--report-log", str(path)]), \
                    patch.object(suite.subprocess, "run", side_effect=AssertionError("report-only invoked prerequisites")), \
                    redirect_stdout(io.StringIO()):
                self.assertEqual(suite.main(), 0)

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
            "LABELS": ["bpr_isolated_correctness", f"bpr_correctness_{mode}", "bpr_correctness_config_Debug"],
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
            ("ENVIRONMENT", ["qt_qpa_platform=offscreen"]),
            ("ENVIRONMENT", ["QT_QPA_PLATFORM=offscreen", "QT_QPA_PLATFORM=windows"]),
            ("ENVIRONMENT", ["QT_QPA_PLATFORM=offscreen", "qt_qpa_platform=windows"]),
            ("ENVIRONMENT_MODIFICATION", ["QT_QPA_PLATFORM=set:windows"]),
            ("ENVIRONMENT_MODIFICATION", ["qt_qpa_platform=set:windows"]),
            ("RUN_SERIAL", True),
            ("LABELS", ["bpr_isolated_correctness", "bpr_correctness_offscreen", "bpr_correctness_config_Release"]),
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

    def configure_schedule(self, declaration, *, environment="QT_QPA_PLATFORM=offscreen", before_default="", configuration="Debug", generator="Ninja"):
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
            result = subprocess.run(["cmake", "-G", generator, f"-DCMAKE_BUILD_TYPE={configuration}", "-S", str(source), "-B", str(build)],
                                    capture_output=True, text=True, check=False)
            if result.returncode:
                return result, None
            manifest = subprocess.run(["ctest", "--test-dir", str(build), "-C", configuration, "--show-only=json-v1"],
                                      capture_output=True, text=True, check=True)
            return result, json.loads(manifest.stdout)

    def test_cmake_opt_in_preserves_locks_fixtures_timeout_and_unclassified_exclusivity(self):
        for mode, environment in (("offscreen", "QT_QPA_PLATFORM=offscreen"), ("headless", "")):
            result, manifest = self.configure_schedule(
                f'bpr_allow_isolated_correctness(DIRECTORY "${{CMAKE_CURRENT_SOURCE_DIR}}" MODE {mode} RESOURCE fixture CONFIGURATIONS Debug TESTS one)',
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

    def test_configuration_specific_timing_claims_cannot_inherit_debug_concurrency(self):
        declaration = 'bpr_allow_isolated_correctness(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" MODE offscreen RESOURCE fixture CONFIGURATIONS Debug TESTS one)'
        for generator in ("Ninja", "Ninja Multi-Config"):
            for configuration in ("Debug", "Release", "RelWithDebInfo", "Unknown"):
                with self.subTest(generator=generator, configuration=configuration):
                    result, manifest = self.configure_schedule(declaration, configuration=configuration, generator=generator)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    if not manifest["tests"]:  # Multi-config generators reject undeclared configurations.
                        self.assertNotEqual(configuration, "Debug")
                        continue
                    row = next(row for row in manifest["tests"] if row["name"] == "one")
                    properties = {prop["name"]: prop["value"] for prop in row["properties"]}
                    self.assertEqual(bool(properties.get("RUN_SERIAL")), configuration != "Debug")
                    self.assertEqual("bpr_isolated_correctness" in properties["LABELS"], configuration == "Debug")
                    suite.validate_schedule(manifest)
        row = self.isolated_row()
        row["config"] = "Release"
        with self.assertRaisesRegex(ValueError, "Invalid isolated correctness"):
            suite.validate_schedule({"tests": [row]})

    def test_cmake_refuses_unsafe_or_incomplete_opt_ins(self):
        declaration = 'bpr_allow_isolated_correctness(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" MODE offscreen RESOURCE fixture CONFIGURATIONS Debug TESTS one)'
        cases = [
            (declaration.replace("CONFIGURATIONS Debug", ""), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration.replace("RESOURCE fixture", ""), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration.replace("MODE offscreen", "MODE native"), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration.replace("TESTS one", "TESTS missing"), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration.replace("TESTS one", "TESTS explicit"), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration, "QT_QPA_PLATFORM=cocoa", ""),
            (declaration, "qt_qpa_platform=offscreen", ""),
            (declaration, "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM=windows", ""),
            (declaration, "QT_QPA_PLATFORM=offscreen;qt_qpa_platform=windows", ""),
            (declaration, "QT_QPA_PLATFORM=offscreen", 'set_tests_properties(one PROPERTIES ENVIRONMENT_MODIFICATION "QT_QPA_PLATFORM=set:windows")'),
            (declaration, "QT_QPA_PLATFORM=offscreen", 'set_tests_properties(one PROPERTIES ENVIRONMENT_MODIFICATION "qt_qpa_platform=set:windows")'),
            (declaration.replace("MODE offscreen", "MODE headless"), "QT_QPA_PLATFORM=offscreen", ""),
            (declaration.replace("MODE offscreen", "MODE headless"), "qt_qpa_platform=offscreen", ""),
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



class PhaseLedgerTests(unittest.TestCase):
    def test_first_request_failures_and_interrupted_phases_survive_resume(self):
        with ContainedTemporaryDirectory() as temporary:
            path = Path(temporary) / "ledger.jsonl"
            start = "2026-01-01T00:00:00Z"
            ledger.initialize(path, start, {"scope": "control"})
            with self.assertRaises(FileExistsError):
                ledger.initialize(path, start, {})
            for code in (0, 7):
                self.assertEqual(ledger.run(path, "control", {}, [sys.executable, "-c", f"import sys; print('stdout'); print('stderr',file=sys.stderr); sys.exit({code})"]), code)
            self.assertNotEqual(ledger.run(path, "launch", {}, [str(Path(temporary) / "missing")]), 0)
            interrupted = ledger.begin(path, "interrupted", {}, [])
            result = ledger.summary(path)
            self.assertEqual(result["request"]["request_start_utc"], start)
            self.assertEqual(result["pending"], [interrupted])
            self.assertEqual([p["terminal"]["outcome"] for p in result["phases"][:-1]], ["passed", "failed", "launch-failed"])
            done = result["phases"][0]["phase_id"]
            with self.assertRaisesRegex(ValueError, "cannot be rewritten"):
                ledger.finish(path, done, outcome="passed", seconds=0, exit_code=0)
            with path.open("a") as output:
                output.write('{"sequence":999,"request_id":"wrong"}\n')
            with self.assertRaisesRegex(ValueError, "sequence/request"):
                ledger.summary(path)

if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
