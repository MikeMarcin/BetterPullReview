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

    def test_windows_preset_selects_visual_studio_2022_x64(self) -> None:
        configure_presets = {
            preset["name"]: preset
            for preset in self.presets["configurePresets"]
        }
        windows = configure_presets["windows-dev"]

        self.assertEqual(windows["generator"], "Visual Studio 17 2022")
        self.assertEqual(windows["architecture"], "x64")
        self.assertEqual(windows["cacheVariables"]["BPR_BUILD_QT"], "ON")
        self.assertEqual(windows["cacheVariables"]["BPR_BUILD_TESTS"], "ON")

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
        configure_presets = {
            preset["name"]: preset
            for preset in self.presets["configurePresets"]
        }
        developer = configure_presets["dev"]

        self.assertEqual(developer["generator"], "Ninja")
        self.assertEqual(developer["cacheVariables"]["BPR_BUILD_QT"], "ON")
        self.assertEqual(
            developer["cacheVariables"]["BPR_BUILD_TESTS"],
            "ON",
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
