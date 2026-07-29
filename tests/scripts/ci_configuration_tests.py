#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class CiConfigurationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.presets = json.loads(
            (ROOT / "CMakePresets.json").read_text(encoding="utf-8")
        )
        cls.workflow = (
            ROOT / ".github/workflows/ci.yml"
        ).read_text(encoding="utf-8")

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

    def test_workflow_selects_matching_windows_qt_archive(self) -> None:
        self.assertRegex(
            self.workflow,
            re.compile(
                r"- os: windows-2025\s+"
                r"preset: windows-dev\s+"
                r"qt_arch: win64_msvc2022_64"
            ),
        )
        self.assertIn("arch: ${{ matrix.qt_arch }}", self.workflow)

    def test_every_selected_action_is_commit_pinned(self) -> None:
        action_lines = [
            line.strip()
            for line in self.workflow.splitlines()
            if line.strip().startswith("uses:")
        ]
        self.assertGreater(len(action_lines), 0)
        for line in action_lines:
            reference = line.split("#", 1)[0].strip()
            self.assertRegex(reference, r"@[0-9a-f]{40}$")


if __name__ == "__main__":
    unittest.main()
