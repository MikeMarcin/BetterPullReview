#!/usr/bin/env python3
"""Falsify how the RFC-BPR-0016 platform course launches the shipped artifact.

The course runs the shipped application twice outside CTest — the runtime
surface check and the device-selection probe — and on Windows a Qt-linked
executable finds its DLLs only through PATH. The registered tests get that
prefix from `cmake/BprTestIsolation.cmake`; the course takes the same
declaration from CTest's inventory rather than resolving Qt a second time.
These cases pin that the declaration is what gets applied, that a Windows tree
declaring none is a named refusal instead of an artifact that exits before
`main`, that other platforms launch in the ambient environment untouched, and
that both direct-launch steps actually receive the derived environment.

Owned by BPR-0511.
"""

from __future__ import annotations

import os
import pathlib
import subprocess
import sys
import unittest
from typing import Any


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from scripts import verify_rfc0016_platform_course as course  # noqa: E402


QT_DIRECTORY = "C:/Qt/6.8.2/msvc2022_64/bin"
FONT_DIRECTORY = "C:/Windows/Fonts"


def inventory(*modifications: list[str]) -> dict[str, Any]:
    """A json-v1 inventory with one test per modification list."""
    return {
        "kind": "ctestInfo",
        "tests": [
            {
                "name": f"test_{index}",
                "command": ["binary"],
                "properties": [
                    {"name": "LABELS", "value": ["bpr_network_isolated"]},
                    {"name": "ENVIRONMENT_MODIFICATION", "value": list(entry)},
                ],
            }
            for index, entry in enumerate(modifications)
        ],
    }


WINDOWS_DECLARATION = [
    f"PATH=path_list_prepend:{QT_DIRECTORY}",
    f"QT_QPA_FONTDIR=set:{FONT_DIRECTORY}",
]


class LaunchEnvironmentTests(unittest.TestCase):
    def test_windows_launch_takes_the_declared_qt_directory(self) -> None:
        base = {"PATH": "C:/Windows/system32", "USERPROFILE": "C:/Users/x"}
        environment = course.environment_from_inventory(
            inventory(WINDOWS_DECLARATION), base, "windows"
        )
        self.assertEqual(
            environment["PATH"],
            os.pathsep.join([QT_DIRECTORY, "C:/Windows/system32"]),
        )
        self.assertEqual(environment["QT_QPA_FONTDIR"], FONT_DIRECTORY)
        self.assertEqual(environment["USERPROFILE"], "C:/Users/x")
        self.assertEqual(base, {"PATH": "C:/Windows/system32", "USERPROFILE": "C:/Users/x"})

    def test_the_declaration_is_applied_once_across_many_tests(self) -> None:
        environment = course.environment_from_inventory(
            inventory(WINDOWS_DECLARATION, WINDOWS_DECLARATION, WINDOWS_DECLARATION),
            {"PATH": "base"},
            "windows",
        )
        self.assertEqual(environment["PATH"], os.pathsep.join([QT_DIRECTORY, "base"]))

    def test_an_empty_ambient_path_gets_only_the_declaration(self) -> None:
        environment = course.environment_from_inventory(
            inventory(WINDOWS_DECLARATION), {}, "windows"
        )
        self.assertEqual(environment["PATH"], QT_DIRECTORY)

    def test_a_windows_tree_declaring_no_qt_directory_is_refused_by_name(self) -> None:
        with self.assertRaises(course.CourseFailure) as refused:
            course.environment_from_inventory(
                inventory(["PYTHONPATH=unset:"]), {"PATH": "base"}, "windows"
            )
        self.assertEqual(str(refused.exception), "qt-runtime-directory-undeclared")

    def test_other_platforms_launch_in_the_ambient_environment(self) -> None:
        base = {"PATH": "/usr/bin", "HOME": "/home/x"}
        for platform_name in ("macos", "linux"):
            with self.subTest(platform=platform_name):
                environment = course.environment_from_inventory(
                    inventory(["PYTHONPATH=unset:", "LD_PRELOAD=unset:"]),
                    base,
                    platform_name,
                )
                self.assertEqual(environment, base)

    def test_a_malformed_inventory_entry_is_skipped_not_fatal(self) -> None:
        document = inventory(WINDOWS_DECLARATION)
        document["tests"].insert(0, "not-a-test")
        document["tests"].append({"name": "odd", "properties": ["odd"]})
        environment = course.environment_from_inventory(document, {}, "windows")
        self.assertEqual(environment["PATH"], QT_DIRECTORY)


class CourseRegistrationTests(unittest.TestCase):
    """The course names a missing selector instead of passing on a count."""

    def test_every_name_registered_passes_on_every_platform(self) -> None:
        for platform_name in ("macos", "linux"):
            course.check_course_registration(set(course.COURSE_SELECTOR_NAMES), platform_name)

    def test_a_missing_name_is_refused_by_name(self) -> None:
        registered = set(course.COURSE_SELECTOR_NAMES) - {"qt_device_authorization_scale_125"}
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_course_registration(registered, "linux")
        self.assertEqual(
            str(refused.exception),
            "course-selector-missing:linux:qt_device_authorization_scale_125",
        )

    def test_a_documented_exclusion_is_accepted_only_on_its_platform(self) -> None:
        excluded = set(course.PLATFORM_SELECTOR_EXCLUSIONS["windows"])
        registered = set(course.COURSE_SELECTOR_NAMES) - excluded
        course.check_course_registration(registered, "windows")
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_course_registration(registered, "linux")
        self.assertTrue(str(refused.exception).startswith("course-selector-missing:linux:"))

    def test_an_exclusion_whose_name_is_registered_is_stale(self) -> None:
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_course_registration(set(course.COURSE_SELECTOR_NAMES), "windows")
        self.assertTrue(str(refused.exception).startswith("course-exclusion-stale:windows:"))

    def test_the_enumeration_matches_the_pattern(self) -> None:
        import re as _re

        pattern = _re.compile(course.COURSE_SELECTOR_PATTERN)
        for name in course.COURSE_SELECTOR_NAMES:
            self.assertIsNotNone(pattern.match(name), name)
        self.assertEqual(len(set(course.COURSE_SELECTOR_NAMES)), len(course.COURSE_SELECTOR_NAMES))

    def test_every_exclusion_names_an_enumerated_selector_with_a_reason(self) -> None:
        for platform_name, table in course.PLATFORM_SELECTOR_EXCLUSIONS.items():
            self.assertIn(platform_name, course.PLATFORMS)
            for name, reason in table.items():
                self.assertIn(name, course.COURSE_SELECTOR_NAMES)
                self.assertTrue(reason.strip())


class DirectLaunchStepsTests(unittest.TestCase):
    """Both steps that run the artifact outside CTest receive the environment."""

    def setUp(self) -> None:
        self.calls: list[dict[str, Any]] = []
        self.original_run = course.run

    def tearDown(self) -> None:
        course.run = self.original_run

    def capture(self, stdout: str) -> None:
        def fake_run(command, *, cwd=None, timeout=3600, environment=None):
            self.calls.append({"command": list(command), "environment": environment})
            return subprocess.CompletedProcess(command, 0, stdout=stdout, stderr="")

        course.run = fake_run

    def test_the_device_probe_launches_in_the_derived_environment(self) -> None:
        self.capture(
            "DEVICE_FLOW_HOST_COMPOSED:true\n"
            "DEVICE_FLOW_PRESENTED:true\n"
            "DEVICE_FLOW_STAGE:unavailable\n"
            "DEVICE_FLOW_REQUESTS:0\n"
            "DEVICE_FLOW_UNAVAILABLE_OK\n"
        )
        launch = {"PATH": os.pathsep.join([QT_DIRECTORY, "base"]), "QT_QPA_FONTDIR": FONT_DIRECTORY}
        course.check_device_selection(pathlib.Path("shipped.exe"), "unavailable", launch)
        self.assertEqual(len(self.calls), 1)
        environment = self.calls[0]["environment"]
        self.assertEqual(environment["PATH"], launch["PATH"])
        self.assertEqual(environment["QT_QPA_FONTDIR"], FONT_DIRECTORY)
        self.assertEqual(environment["QT_QPA_PLATFORM"], "offscreen")
        self.assertNotIn("QT_QPA_PLATFORM", launch)

    def test_the_runtime_checks_launch_in_the_derived_environment(self) -> None:
        self.capture("RUNTIME_SURFACE_OK\n")
        launch = {"PATH": os.pathsep.join([QT_DIRECTORY, "base"])}
        course.check_runtime_and_dependencies(pathlib.Path("shipped.exe"), launch)
        self.assertEqual(len(self.calls), 2)
        for call in self.calls:
            self.assertEqual(call["environment"], launch)
        scripts = [pathlib.Path(call["command"][1]).name for call in self.calls]
        self.assertEqual(
            scripts, ["verify_binary_dependencies.py", "verify_runtime_surface.py"]
        )

    def test_without_a_derived_environment_the_ambient_one_is_used(self) -> None:
        self.capture(
            "DEVICE_FLOW_HOST_COMPOSED:true\nDEVICE_FLOW_PRESENTED:true\n"
            "DEVICE_FLOW_STAGE:unavailable\nDEVICE_FLOW_REQUESTS:0\n"
            "DEVICE_FLOW_UNAVAILABLE_OK\n"
        )
        course.check_device_selection(pathlib.Path("shipped"), "unavailable")
        environment = self.calls[0]["environment"]
        self.assertEqual(environment["QT_QPA_PLATFORM"], "offscreen")
        for name, value in os.environ.items():
            if name != "QT_QPA_PLATFORM":
                self.assertEqual(environment.get(name), value)


if __name__ == "__main__":
    unittest.main()
