#!/usr/bin/env python3
from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class PlatformConfigurationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.presets = json.loads(
            (ROOT / "CMakePresets.json").read_text(encoding="utf-8")
        )
        cls.configure_presets = {
            preset["name"]: preset
            for preset in cls.presets["configurePresets"]
        }

    def resolved_configure_preset(self, name: str) -> dict[str, object]:
        preset = self.configure_presets[name]
        resolved: dict[str, object] = {}
        inherited = preset.get("inherits", [])
        if isinstance(inherited, str):
            inherited = [inherited]
        for parent in inherited:
            parent_values = self.resolved_configure_preset(parent)
            resolved.update(parent_values)
            if "cacheVariables" in parent_values:
                resolved["cacheVariables"] = dict(
                    parent_values["cacheVariables"]  # type: ignore[arg-type]
                )
        resolved.update(preset)
        cache = dict(resolved.get("cacheVariables", {}))
        for parent in inherited:
            cache.update(
                self.resolved_configure_preset(parent).get("cacheVariables", {})
            )
        cache.update(preset.get("cacheVariables", {}))
        resolved["cacheVariables"] = cache
        return resolved

    def test_windows_preset_selects_visual_studio_2022_x64(self) -> None:
        windows = self.resolved_configure_preset("windows-dev")

        self.assertEqual(windows["generator"], "Visual Studio 17 2022")
        self.assertEqual(windows["architecture"], "x64")
        self.assertEqual(windows["cacheVariables"]["BPR_BUILD_QT"], "ON")
        self.assertEqual(windows["cacheVariables"]["BUILD_TESTING"], "ON")
        self.assertNotIn("CMAKE_EXPORT_COMPILE_COMMANDS", windows["cacheVariables"])

    def test_windows_build_and_test_select_debug_configuration(self) -> None:
        for key in ("buildPresets", "testPresets"):
            presets = {
                preset["name"]: preset
                for preset in self.presets[key]
            }
            self.assertEqual(
                presets["windows-dev"]["configurePreset"],
                "windows-dev",
            )
            self.assertEqual(
                presets["windows-dev"]["configuration"],
                "Debug",
            )

    def test_portable_developer_preset_uses_ninja(self) -> None:
        developer = self.resolved_configure_preset("dev")

        self.assertEqual(developer["generator"], "Ninja")
        self.assertEqual(developer["cacheVariables"]["BPR_BUILD_QT"], "ON")
        self.assertEqual(
            developer["cacheVariables"]["BUILD_TESTING"],
            "ON",
        )
        self.assertEqual(
            developer["cacheVariables"]["CMAKE_EXPORT_COMPILE_COMMANDS"],
            "ON",
        )

    def test_schema_12_workflows_and_strict_test_presets(self) -> None:
        self.assertEqual(self.presets["version"], 12)
        self.assertEqual(
            self.presets["cmakeMinimumRequired"],
            {"major": 4, "minor": 4, "patch": 2},
        )
        workflows = {
            preset["name"]: preset for preset in self.presets["workflowPresets"]
        }
        tests = {
            preset["name"]: preset for preset in self.presets["testPresets"]
        }
        common_test = tests["_test-common"]
        self.assertEqual(common_test["execution"]["noTestsAction"], "error")
        for name in ("dev", "core-only", "windows-dev"):
            self.assertEqual(
                [step["type"] for step in workflows[name]["steps"]],
                ["configure", "build", "test"],
            )

    def test_repository_selects_no_github_actions_workflow(self) -> None:
        workflow_root = ROOT / ".github/workflows"
        if not workflow_root.exists():
            return

        workflows: list[Path] = []
        for pattern in ("*.yml", "*.yaml"):
            workflows.extend(workflow_root.glob(pattern))
        self.assertEqual(
            workflows,
            [],
            "GitHub Actions is not selected for BetterPullReview",
        )


if __name__ == "__main__":
    unittest.main()
