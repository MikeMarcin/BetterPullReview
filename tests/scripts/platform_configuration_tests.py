#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from skip_report import SkipReportingTestRunner  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BUILD_DIR: Path | None = None


def vcpkg_is_disabled(project: str) -> bool:
    """The generated project opts out unconditionally, before any imports."""
    root = ET.fromstring(project)
    namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    for group in root:
        if group.tag.endswith("}Import"):
            break
        if group.get("Label") == "Globals" and not group.get("Condition"):
            values = group.findall("m:VcpkgEnabled", namespace)
            return (
                len(values) == 1
                and not values[0].get("Condition")
                and values[0].text == "false"
            )
    return False


def generated_visual_studio_projects(build: Path) -> list[Path]:
    """Use CMake's target inventory, not upstream examples or cached probes."""
    projects = set()
    for inventory in build.rglob("CMakeFiles/TargetDirectories.txt"):
        if "CMakeFiles" in inventory.relative_to(build).parts[:-2]:
            continue
        for line in inventory.read_text(encoding="utf-8").splitlines():
            target = Path(line)
            project = target.parent.parent / (target.stem + ".vcxproj")
            if project.is_file():
                projects.add(project)
    return sorted(projects)


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
        cache: dict[str, object] = {}
        inherited = preset.get("inherits", [])
        if isinstance(inherited, str):
            inherited = [inherited]
        for parent in inherited:
            parent_values = self.resolved_configure_preset(parent)
            resolved.update(parent_values)
            parent_cache = parent_values.get("cacheVariables")
            if isinstance(parent_cache, dict):
                cache.update(parent_cache)
        resolved.update(preset)
        preset_cache = preset.get("cacheVariables")
        if isinstance(preset_cache, dict):
            cache.update(preset_cache)
        resolved["cacheVariables"] = cache
        return resolved

    def test_windows_preset_selects_visual_studio_2026_x64(self) -> None:
        windows = self.resolved_configure_preset("windows-dev")

        # BPR-0336 retargeted this preset from the uninstalled Visual Studio 17
        # 2022 generator when a real Windows host first became available. The
        # pin stays exact so that a further retarget has to be deliberate.
        self.assertEqual(windows["generator"], "Visual Studio 18 2026")
        self.assertEqual(windows["architecture"], "x64")
        self.assertEqual(windows["cacheVariables"]["BPR_BUILD_QT"], "ON")
        self.assertEqual(windows["cacheVariables"]["BUILD_TESTING"], "ON")
        self.assertNotIn("CMAKE_EXPORT_COMPILE_COMMANDS", windows["cacheVariables"])
        # BPR-0495: the syntax provider builds its dependency once per entry
        # of this list, and Visual Studio's four-entry default made a fresh
        # configure build MinSizeRel, which does not compile and which nothing
        # selects. The pin is exact so that adding a configuration is a
        # deliberate change to what the provider builds.
        self.assertEqual(
            windows["cacheVariables"]["CMAKE_CONFIGURATION_TYPES"],
            "Debug;Release",
        )

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

    def test_generated_visual_studio_projects_disable_vcpkg(self) -> None:
        if BUILD_DIR is None:
            self.skipTest("no --build-dir given")
        cache = (BUILD_DIR / "CMakeCache.txt").read_text(encoding="utf-8")
        if "CMAKE_GENERATOR:INTERNAL=Visual Studio " not in cache:
            self.skipTest("this tree does not use Visual Studio")
        projects = generated_visual_studio_projects(BUILD_DIR)
        self.assertTrue(projects, "no generated Visual Studio projects")
        findings = [
            str(project.relative_to(BUILD_DIR)) for project in projects
            if not vcpkg_is_disabled(project.read_text(encoding="utf-8-sig"))
        ]
        self.assertEqual(findings, [], "vcpkg is not disabled: " + ", ".join(findings))

    def test_generated_project_check_refuses_missing_or_enabled_integration(self) -> None:
        def project(properties: str) -> str:
            return (
                '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">'
                f'<PropertyGroup Label="Globals">{properties}</PropertyGroup></Project>'
            )

        self.assertTrue(vcpkg_is_disabled(project("<VcpkgEnabled>false</VcpkgEnabled>")))
        for properties in (
            "", "<VcpkgEnabled>true</VcpkgEnabled>",
            '<VcpkgEnabled Condition="sometimes">false</VcpkgEnabled>',
            '<VcpkgEnabled>false</VcpkgEnabled><VcpkgEnabled>true</VcpkgEnabled>',
        ):
            self.assertFalse(vcpkg_is_disabled(project(properties)), properties)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--build-dir", type=Path)
    options, remaining = parser.parse_known_args()
    BUILD_DIR = options.build_dir
    unittest.main(argv=[sys.argv[0], *remaining], testRunner=SkipReportingTestRunner)
