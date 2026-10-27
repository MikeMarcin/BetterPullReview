#!/usr/bin/env python3
"""Falsify source/command/configuration/occurrence and output-ownership declarations."""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
import run_test_suite as suite
import workbench_case_inventory as inventory


class RegistryTests(unittest.TestCase):
    def test_named_occurrences_retain_original_default_and_selector_order(self):
        data = json.loads((ROOT / "tests/workbench_case_inventory.json").read_text(encoding="utf-8"))
        text = (ROOT / "tests/adapters_qt/workbench_window_tests.cpp").read_text(encoding="utf-8")
        main = text[text.index("auto main(int argc,"):]
        selected = None
        actual = {}
        for line in main.splitlines():
            match = re.search(r'arguments\(\)\.contains\(QStringLiteral\("(--[^\"]+)"', line)
            if match:
                selected = match[1]
            case = re.search(r'runCase\("([^\"]+)"', line)
            if case:
                actual.setdefault(selected or "<default>", []).append(case[1])
            if line == '\t}':
                selected = None
        for name, called in re.findall(r'runCase\("([^\"]+)", &([A-Za-z0-9_]+)\)', main):
            self.assertEqual(name, called, "an inventory label must name the function actually called")
        self.assertEqual(actual, data["selector_cases"])
        self.assertEqual(len(actual["<default>"]), 119)
        for platform, count in (("Windows", 46), ("Darwin", 45), ("Linux", 45)):
            self.assertEqual(len(inventory.expected_rows(platform)), count)

    def test_missing_duplicate_extra_reordered_and_zero_cases_are_not_credit(self):
        expected = ["first", "second"]
        inventory.validate_cases(expected, 'WORKBENCH_CASE:first\nWORKBENCH_CASE:second\n')
        for actual in ([], ["first"], ["first", "first"], ["first", "second", "extra"], ["second", "first"]):
            with self.subTest(actual=actual), self.assertRaises(ValueError):
                inventory.validate_cases(expected, ''.join(f'WORKBENCH_CASE:{name}\n' for name in actual))
        with self.assertRaises(ValueError):
            inventory.validate_cases(expected, 'WORKBENCH_CASE:first\nWORKBENCH_CASE:second\nworkbench:pass\n')

    def test_real_registry_invocations_require_exact_manifest_coverage(self):
        registry = json.loads((ROOT / "tests/command_correctness_registry.json").read_text(encoding="utf-8"))
        rows = []
        directory = Path("/build/tests/adapters_qt").as_posix()
        for declaration in registry["families"][0]["rows"]:
            rows.append({"name": declaration["name"],
                         "command": [directory + "/" + declaration["executable"] + ".exe", *declaration["arguments"]],
                         "properties": [{"name": "WORKING_DIRECTORY", "value": directory},
                                        {"name": "ENVIRONMENT", "value": [item.replace("{binary_dir}", directory) for item in declaration["environment"]]}]})
        self.assertEqual(len(inventory.validate_manifest({"tests": rows}, "Windows")), 46)
        for changed in (rows[:-1], rows + [rows[0]], rows + [{**rows[0], "name": "unknown"}]):
            with self.subTest(count=len(changed)), self.assertRaisesRegex(ValueError, "Missing, duplicate or unknown"):
                inventory.validate_manifest({"tests": changed}, "Windows")

    def test_registry_command_scale_output_source_and_unset_mutations_fail_closed(self):
        registry = json.loads((ROOT / "tests/command_correctness_registry.json").read_text(encoding="utf-8"))
        for current in registry["families"]:
            for row in current["rows"]:
                self.assertRegex(row["resource"], r"^[a-z0-9_]+$")
        family = registry["families"][0]
        candidate = next(row for row in family["rows"] if row["name"] == "qt_workbench_window_scale_100")
        directory = Path("/build/tests/adapters_qt")
        props = {"WORKING_DIRECTORY": directory.as_posix(),
                 "ENVIRONMENT": [item.replace("{binary_dir}", directory.as_posix()) for item in candidate["environment"]],
                 "ENVIRONMENT_MODIFICATION": [f'{key}=unset:' for key in family["unset_environment"]]}
        row = {"name": candidate["name"], "command": ["/build/app/" + candidate["executable"]]}
        suite.validate_command_owned_row(row, props)
        for kind in ("unknown", "selector", "output", "scale", "unset", "override"):
            bad_row, bad_props = copy.deepcopy(row), copy.deepcopy(props)
            if kind == "unknown": bad_row["name"] = "unknown"
            if kind == "selector": bad_row["command"].append("--conversation-reveal")
            if kind == "output": bad_props["ENVIRONMENT"] = [entry.replace("workbench-window-100", "workbench-window-200") for entry in bad_props["ENVIRONMENT"]]
            if kind == "scale": bad_props["ENVIRONMENT"] = [entry.replace("QT_SCALE_FACTOR=1.0", "QT_SCALE_FACTOR=2.0") for entry in bad_props["ENVIRONMENT"]]
            if kind == "unset": bad_props["ENVIRONMENT_MODIFICATION"].pop()
            if kind == "override": bad_props["ENVIRONMENT_MODIFICATION"].append(f'{family["unset_environment"][0].lower()}=set:/shared')
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                suite.validate_command_owned_row(bad_row, bad_props)

    def configure_registry(self, mutation=None, features=None, configuration="Debug"):
        with ContainedTemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / "tests/adapters_qt"
            directory.mkdir(parents=True)
            source = directory / "case.cpp"
            source.write_text("original\n")
            family = {"source": "tests/adapters_qt/case.cpp", "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                      "directory": "tests/adapters_qt", "configurations": ["Debug"], "inputs": [], "unset_environment": ["BPR_OPTIONAL_RENDER_ROOT"], "rows": [
                          {"name": "one", "command": "worker;--correctness", "scheduling": "correctness", "resource": "one",
                           "platforms": ["Darwin", "Linux", "Windows"], "environment": ["QT_SCALE_FACTOR=1.0"]}]}
            if mutation:
                mutation(family, source)
            registry = root / "registry.json"
            registry.write_text(json.dumps({"schema": "bpr.command-correctness.v1", "features": features or {}, "families": [family]}))
            module = (ROOT / "cmake/BprTestScheduling.cmake").as_posix()
            (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.24)
project(CommandRegistry NONE)
enable_testing()
include("{module}")
add_subdirectory(tests/adapters_qt)
bpr_schedule_test_directory("${{CMAKE_CURRENT_SOURCE_DIR}}")
bpr_apply_command_correctness_registry("{registry.as_posix()}")
''')
            (directory / "CMakeLists.txt").write_text('''add_test(NAME one COMMAND worker --correctness)
set_tests_properties(one PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
set_property(TEST one PROPERTY BPR_LOGICAL_COMMAND "worker;--correctness")
''')
            build = root / "build"
            configured = subprocess.run(["cmake", "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={configuration}", "-S", str(root), "-B", str(build)], capture_output=True, text=True)
            if configured.returncode:
                return configured, None
            manifest = subprocess.run(["ctest", "--test-dir", str(build), "--show-only=json-v1"], capture_output=True, text=True, check=True)
            return configured, json.loads(manifest.stdout)["tests"][0]

    def test_python_driver_binds_source_binary_configuration_and_private_output(self):
        registry = json.loads((ROOT / "tests/command_correctness_registry.json").read_text(encoding="utf-8"))
        family = next(item for item in registry["families"] if item["source"].endswith("/profile_window_review_restoration_public_path_tests.cpp"))
        # Exercise declaration semantics on a current, private fixture. A stale
        # production input must remain serial; this test does not renew its seal.
        with ContainedTemporaryDirectory() as temporary:
            root = Path(temporary)
            for item in [{"path": family["source"]}, *family["inputs"]]:
                target = root / item["path"]
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((ROOT / item["path"]).read_bytes())
                digest = hashlib.sha256(target.read_bytes()).hexdigest()
                if "sha256" in item:
                    item["sha256"] = digest
                else:
                    family["sha256"] = digest
            (root / "tests/command_correctness_registry.json").write_text(json.dumps(registry))
            self.check_python_driver_declaration(root, family)

    def check_python_driver_declaration(self, root, family):
        candidate = family["rows"][0]
        directory = Path("/build/tests/end_to_end")
        props = {"WORKING_DIRECTORY": directory.as_posix(),
                 "ENVIRONMENT": ["QT_SCALE_FACTOR=1.0", "BPR_REVIEW_RESTORATION_ARTIFACT_ROOT=/build/bpr0710-restoration-artifacts"],
                 "ENVIRONMENT_MODIFICATION": [f"{key}=unset:" for key in family["unset_environment"]]}
        row = {"name": candidate["name"], "config": "Release", "command": ["/tools/python.exe",
               (root / "tests/end_to_end/profile_window_review_restoration_contract.py").as_posix(),
               "--application", "/build/tests/end_to_end/Release/profile_window_review_restoration_public_path.exe"]}
        suite.validate_command_owned_row(row, props, root)
        for kind in ("driver", "binary", "configuration", "output", "capture"):
            bad_row, bad_props = copy.deepcopy(row), copy.deepcopy(props)
            if kind == "driver": bad_row["command"][1] = "/other/driver.py"
            if kind == "binary": bad_row["command"][-1] = "/other/profile_window_review_restoration_public_path.exe"
            if kind == "configuration": bad_row["config"] = "Debug"
            if kind == "output": bad_props["ENVIRONMENT"][-1] += "-shared"
            if kind == "capture": bad_props["ENVIRONMENT_MODIFICATION"] = []
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                suite.validate_command_owned_row(bad_row, bad_props, root)

    def test_source_or_argv_drift_remains_serial_and_optional_output_is_unset(self):
        result, row = self.configure_registry()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        props = {prop["name"]: prop["value"] for prop in row["properties"]}
        self.assertFalse(props.get("RUN_SERIAL", False))
        self.assertIn("BPR_OPTIONAL_RENDER_ROOT=unset:", props["ENVIRONMENT_MODIFICATION"])
        for mutation in (lambda family, source: source.write_text("new timing assertion\n"),
                         lambda family, source: family["rows"][0].update(command="worker;--timed"),
                         lambda family, source: family["rows"][0].update(environment=["QT_SCALE_FACTOR=2.0"]),
                         lambda family, source: family.update(inputs=[{"path": "tests/adapters_qt/case.cpp", "sha256": "0" * 64}])):
            result, row = self.configure_registry(mutation)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            props = {prop["name"]: prop["value"] for prop in row["properties"]}
            self.assertTrue(props["RUN_SERIAL"])
            self.assertNotIn("bpr_command_owned", props["LABELS"])

    def test_release_requires_explicit_audited_configuration(self):
        for configuration, audited, serial in (("Release", ["Debug"], True), ("Release", ["Debug", "Release"], False), ("RelWithDebInfo", ["Debug", "Release"], True)):
            result, row = self.configure_registry(lambda family, source: family.update(configurations=audited), configuration=configuration)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            props = {prop["name"]: prop["value"] for prop in row["properties"]}
            self.assertEqual(props.get("RUN_SERIAL", False), serial)

    def test_unknown_feature_inputs_keep_the_command_exclusive(self):
        result, row = self.configure_registry(features={"UNKNOWN_FEATURE": "ON"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        properties = {prop["name"]: prop["value"] for prop in row["properties"]}
        self.assertTrue(properties["RUN_SERIAL"])
        self.assertNotIn("bpr_command_owned", properties["LABELS"])

    def test_duplicate_unknown_registry_rows_and_unknown_policy_refuse_configuration(self):
        for mutation in (lambda family, source: family["rows"].append(copy.deepcopy(family["rows"][0])),
                         lambda family, source: family["rows"][0].update(name="missing"),
                         lambda family, source: family["rows"][0].update(scheduling="guess")):
            result, _ = self.configure_registry(mutation)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
