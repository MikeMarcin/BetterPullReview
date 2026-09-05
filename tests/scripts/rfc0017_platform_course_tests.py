#!/usr/bin/env python3
"""Falsify the RFC-BPR-0017 platform course's refusals without building anything.

The course itself takes an hour and needs a target-native host, so every claim
it makes about *refusing* would otherwise be untested: nobody reproduces a
missing registration or a nonzero request counter by hand on three operating
systems. These cases drive the same functions the course runs with fabricated
CTest listings and fabricated probe output, so each refusal is named here
rather than discovered on the one platform where it fires.

What is pinned: the course selector set is the inventory's plus exactly two
composition selectors, and cannot drift from it; a missing, duplicated, or
unaccounted RFC-BPR-0017 selector is refused by name before any test runs; the
probe's facts and its terminal verdict are read separately, so a wrong stage, a
nonzero request, header, or vault counter, and an absent verdict fail
independently; the enterprise probe accepts a refusal and rejects an
enablement; and a Windows tree that declares no Qt runtime directory is refused
rather than launched into a loader failure that prints nothing (BPR-0511).

Owned by BPR-0306.
"""

from __future__ import annotations

import os
import pathlib
import re
import subprocess
import sys
import unittest
from typing import Any


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from scripts import verify_rfc0017_platform_course as course  # noqa: E402

# The course imports the inventory as a top-level module from `scripts/`;
# importing `scripts.verify_rfc0017_test_inventory` here would produce a
# second module object, and patching that one would patch nothing the
# course reads. Take the course's own reference instead.
inventory = course.rfc0017_inventory


QT_DIRECTORY = "C:/Qt/6.8.2/msvc2022_64/bin"
FONT_DIRECTORY = "C:/Windows/Fonts"


def listing(names: list[str]) -> str:
    """CTest's `-N` output for a set of names."""
    lines = ["Test project /build"]
    lines += [f"  Test #{index + 1}: {name}" for index, name in enumerate(names)]
    lines.append(f"Total Tests: {len(names)}")
    return "\n".join(lines) + "\n"


def summary(total: int, failed: int = 0) -> str:
    percent = 100 if failed == 0 else 0
    clause = "" if failed == 0 else f", {failed} tests failed"
    return f"\n{percent}% tests passed{clause} out of {total}\n"


def probe_output(
    *,
    composed: str = "false",
    stage: str = "unavailable",
    requests: str = "0",
    header: str = "0",
    vault: str = "0",
    verdict: str | None = "PUBLIC_PREVIEW_UNAVAILABLE_OK",
) -> str:
    lines = [
        "STARTUP_OK",
        f"PUBLIC_PREVIEW_COMPOSED:{composed}",
        f"PUBLIC_PREVIEW_STAGE:{stage}",
        f"PUBLIC_PREVIEW_REQUESTS:{requests}",
        f"PUBLIC_PREVIEW_HEADER_EFFECTS:{header}",
        f"PUBLIC_PREVIEW_VAULT_EFFECTS:{vault}",
        "PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0",
    ]
    if verdict is not None:
        lines.append(verdict)
    return "\n".join(lines) + "\n"


def dev_listing(
    *,
    wire_selectors: bool = True,
    without: tuple[str, ...] = (),
    extra: tuple[str, ...] = (),
) -> list[str]:
    """A whole CTest listing for a graph that builds every RFC-0017 surface.

    Anchors first, because they are what `selector_expected` reads to decide
    what the graph owes; then the selectors themselves, the two composition
    selectors, the excused script contracts, and one unrelated test.
    """
    names = list(
        dict.fromkeys(
            anchor for anchor, _ in inventory.SELECTOR_ANCHORS.values() if anchor
        )
    )
    names += [
        selector for selector in inventory.SELECTORS
        if wire_selectors or not inventory.SELECTOR_ANCHORS[selector][1]
    ]
    names += list(course.COURSE_ONLY_SELECTORS)
    names += list(course.FAMILY_EXCEPTIONS)
    names.append("core_review_ledger")
    return [name for name in names if name not in without] + list(extra)


WIRE_SELECTORS = tuple(
    sorted(
        selector for selector, (_, wire) in inventory.SELECTOR_ANCHORS.items()
        if wire
    )
)


def setUpModule() -> None:
    """Judge every case from inside the wire block, whatever the host is.

    `selector_expected` reads the real `sys.platform`; the cases below
    describe macOS and Linux listings and use `as_windows` for Windows, so
    on a Windows host the un-narrowed cases inherited the host's answer and
    the wire selectors read as extras (found by BPR-0315's station). Pinning
    the platform here makes the module hermetic on every host.
    """
    global _module_platform
    _module_platform = sys.platform
    sys.platform = "darwin"


def tearDownModule() -> None:
    sys.platform = _module_platform


_module_platform = sys.platform


def as_windows(case: unittest.TestCase) -> None:
    """Answer `selector_expected` the way it answers on Windows.

    The wire block is a platform fact read from `sys.platform`, and the whole
    point of these cases is what the course does on a platform this host is
    not. Narrowing the block is the same question asked locally.
    """
    original = inventory.WIRE_BLOCK_PLATFORMS
    inventory.WIRE_BLOCK_PLATFORMS = frozenset()
    case.addCleanup(setattr, inventory, "WIRE_BLOCK_PLATFORMS", original)


class CapturingCourse(unittest.TestCase):
    """Runs the course's own functions with `run` replaced by a fake."""

    def setUp(self) -> None:
        self.calls: list[dict[str, Any]] = []
        self.original = course.run

    def tearDown(self) -> None:
        course.run = self.original

    def capture(self, stdout: str, returncode: int = 0, stderr: str = "") -> None:
        def fake_run(command, *, cwd=None, timeout=3600, environment=None):
            self.calls.append(
                {"command": list(command), "environment": environment}
            )
            return subprocess.CompletedProcess(
                command, returncode, stdout=stdout, stderr=stderr
            )

        course.run = fake_run


class SelectorSetTests(unittest.TestCase):
    def test_a_full_graph_owes_every_inventory_selector_plus_two(self) -> None:
        names = course.course_selector_names(dev_listing())
        self.assertEqual(
            set(names),
            set(inventory.SELECTORS) | set(course.COURSE_ONLY_SELECTORS),
        )
        self.assertEqual(
            sorted(course.COURSE_ONLY_SELECTORS),
            ["composed_application_hosts"],
        )
        self.assertNotIn("composed_application_hosts", inventory.SELECTORS)
        # The composed public path is owned by the inventory like its
        # siblings and reaches the course through the owed set, not twice.
        self.assertIn(
            "application_anonymous_public_preview_public_path",
            inventory.SELECTORS,
        )
        self.assertIn(
            "application_anonymous_public_preview_public_path", names
        )

    def test_a_windows_graph_owes_neither_wire_selector(self) -> None:
        as_windows(self)
        names = course.course_selector_names(
            dev_listing(wire_selectors=False)
        )
        for selector in WIRE_SELECTORS:
            self.assertNotIn(selector, names)
        # Everything else, including both composition selectors, is still owed.
        self.assertEqual(
            set(names),
            (set(inventory.SELECTORS) - set(WIRE_SELECTORS))
            | set(course.COURSE_ONLY_SELECTORS),
        )

    def test_a_graph_that_builds_no_surface_owes_the_unanchored_core_and_the_pair(self) -> None:
        # `core_anonymous_public_read` is anchored on nothing: the core policy
        # suite links only the core library, so no graph may decline it
        # (BPR-0300). The two composition selectors are owed everywhere too.
        names = course.course_selector_names(["core_review_ledger"])
        self.assertEqual(
            set(names),
            set(course.COURSE_ONLY_SELECTORS) | {"core_anonymous_public_read"},
        )

    def test_an_absent_anchor_drops_only_its_own_selectors(self) -> None:
        names = course.course_selector_names(
            dev_listing(without=("application_github_host",))
        )
        self.assertNotIn("application_anonymous_read_host", names)
        self.assertNotIn("application_anonymous_diff_authority", names)
        self.assertIn("core_anonymous_public_read", names)

    def test_the_pattern_is_anchored_to_exactly_those_names(self) -> None:
        names = course.course_selector_names(dev_listing())
        pattern = re.compile(course.selector_pattern(names))
        for name in names:
            self.assertIsNotNone(pattern.match(name), name)
        for name in ("rfc0017_test_inventory", "core_anonymous_public_reads",
                     "xcore_anonymous_public_read"):
            self.assertIsNone(pattern.match(name), name)

    def test_every_exception_is_reasoned_and_is_not_a_course_selector(self) -> None:
        names = course.course_selector_names(dev_listing())
        for name, reason in course.FAMILY_EXCEPTIONS.items():
            self.assertNotIn(name, names)
            self.assertTrue(reason.strip(), name)

    def test_the_family_pattern_catches_every_course_selector_but_two(self) -> None:
        family = re.compile(course.FAMILY_PATTERN)
        for name in course.course_selector_names(dev_listing()):
            if name == "composed_application_hosts":
                continue
            self.assertIsNotNone(family.search(name), name)
        for name in course.FAMILY_EXCEPTIONS:
            self.assertIsNotNone(family.search(name), name)


class RegistrationTests(unittest.TestCase):
    """A registration audit that passes on a count is not an audit."""

    def audit(self, listed: list[str], platform_name: str = "macos") -> None:
        course.check_course_registration(
            course.course_selector_names(listed), listed, platform_name
        )

    def test_a_complete_registration_passes(self) -> None:
        for platform_name in ("macos", "linux"):
            self.audit(dev_listing(), platform_name)

    def test_a_windows_graph_without_the_wire_selectors_passes(self) -> None:
        as_windows(self)
        self.audit(dev_listing(wire_selectors=False), "windows")

    def test_a_windows_graph_that_registered_one_anyway_is_an_extra(self) -> None:
        as_windows(self)
        listed = dev_listing(wire_selectors=False, extra=(WIRE_SELECTORS[0],))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_course_registration(
                course.course_selector_names(listed), listed, "windows"
            )
        self.assertEqual(
            str(refused.exception),
            f"course-selector-extra:windows:{WIRE_SELECTORS[0]}",
        )

    def test_the_same_absence_is_a_missing_selector_inside_the_wire_block(self) -> None:
        for platform_name in ("macos", "linux"):
            with self.subTest(platform=platform_name):
                listed = dev_listing(wire_selectors=False)
                with self.assertRaises(course.CourseFailure) as refused:
                    course.check_course_registration(
                        course.course_selector_names(listed),
                        listed,
                        platform_name,
                    )
                self.assertEqual(
                    str(refused.exception),
                    f"course-selector-missing:{platform_name}:"
                    f"{WIRE_SELECTORS[0]}",
                )

    def test_a_missing_composition_selector_is_refused_on_every_platform(self) -> None:
        as_windows(self)
        listed = dev_listing(
            wire_selectors=False,
            without=("application_anonymous_public_preview_public_path",),
        )
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_course_registration(
                course.course_selector_names(listed), listed, "windows"
            )
        self.assertEqual(
            str(refused.exception),
            "course-selector-missing:windows:"
            "application_anonymous_public_preview_public_path",
        )

    def test_a_duplicate_registration_is_refused_by_name(self) -> None:
        listed = dev_listing(extra=("core_anonymous_public_read",))
        with self.assertRaises(course.CourseFailure) as refused:
            self.audit(listed, "linux")
        self.assertEqual(
            str(refused.exception),
            "course-selector-duplicate:linux:core_anonymous_public_read:2",
        )

    def test_an_rfc_selector_the_inventory_does_not_know_is_refused(self) -> None:
        listed = dev_listing(
            extra=("application_anonymous_public_preview_mutation",)
        )
        with self.assertRaises(course.CourseFailure) as refused:
            self.audit(listed, "windows")
        self.assertEqual(
            str(refused.exception),
            "course-selector-extra:windows:"
            "application_anonymous_public_preview_mutation",
        )

    def test_an_unrelated_registration_is_not_an_extra(self) -> None:
        self.audit(dev_listing(extra=("qt_composed_github_scale_100",)))

    def test_an_exception_that_became_a_course_selector_is_stale(self) -> None:
        # Built before the table is mutated: the listing is what CTest saw.
        listed = dev_listing()
        original = course.FAMILY_EXCEPTIONS
        course.FAMILY_EXCEPTIONS = dict(original)
        course.FAMILY_EXCEPTIONS["core_anonymous_public_read"] = "synthetic"
        self.addCleanup(setattr, course, "FAMILY_EXCEPTIONS", original)
        with self.assertRaises(course.CourseFailure) as refused:
            self.audit(listed)
        self.assertEqual(
            str(refused.exception),
            "course-exception-stale:macos:core_anonymous_public_read",
        )


class ListingTests(CapturingCourse):
    def test_a_duplicate_name_survives_the_listing(self) -> None:
        self.capture(listing(["core_anonymous_public_read"] * 2))
        self.assertEqual(
            course.listed_selectors(pathlib.Path("build"), "pattern"),
            ["core_anonymous_public_read"] * 2,
        )

    def test_an_unreadable_listing_is_refused(self) -> None:
        self.capture("", returncode=1, stderr="no such build directory")
        with self.assertRaises(course.CourseFailure) as refused:
            course.listed_selectors(pathlib.Path("build"), "pattern")
        self.assertTrue(
            str(refused.exception).startswith("course-registration-unreadable:")
        )

    def test_the_whole_listing_is_read_without_a_pattern(self) -> None:
        self.capture(listing(["a", "b"]))
        self.assertEqual(course.listed_selectors(pathlib.Path("build")), ["a", "b"])
        self.assertNotIn("-R", self.calls[0]["command"])

    def test_a_multi_config_listing_names_its_configuration(self) -> None:
        self.capture(listing(["core_anonymous_public_read"]))
        course.listed_selectors(pathlib.Path("build"), "pattern", "Debug")
        command = self.calls[0]["command"]
        self.assertIn("-C", command)
        self.assertEqual(command[command.index("-C") + 1], "Debug")


class InventoryTests(CapturingCourse):
    def test_the_inventory_names_every_owed_selector_and_no_other(self) -> None:
        owed = course.owed_selectors(dev_listing())
        self.capture("RFC0017_TEST_INVENTORY_OK\n")
        course.check_inventory(pathlib.Path("build"), owed)
        command = self.calls[0]["command"]
        self.assertNotIn("--all", command)
        self.assertIn("--build-dir", command)
        self.assertNotIn("--build-config", command)
        self.assertTrue(command[1].endswith("verify_rfc0017_test_inventory.py"))
        required = [
            command[index + 1]
            for index, argument in enumerate(command)
            if argument == "--required"
        ]
        self.assertEqual(required, list(owed))
        self.assertEqual(set(required), set(inventory.SELECTORS))

    def test_a_windows_graph_does_not_require_the_wire_selectors(self) -> None:
        as_windows(self)
        owed = course.owed_selectors(dev_listing(wire_selectors=False))
        self.capture("RFC0017_TEST_INVENTORY_OK\n")
        course.check_inventory(pathlib.Path("build"), owed)
        command = self.calls[0]["command"]
        for selector in WIRE_SELECTORS:
            self.assertNotIn(selector, command)
        self.assertIn("core_anonymous_public_read", command)

    def test_a_multi_config_inventory_names_its_configuration(self) -> None:
        self.capture("RFC0017_TEST_INVENTORY_OK\n")
        course.check_inventory(pathlib.Path("build"), [], "Debug")
        command = self.calls[0]["command"]
        self.assertEqual(command[command.index("--build-config") + 1], "Debug")

    def test_a_failing_inventory_stops_the_course(self) -> None:
        self.capture("", returncode=1, stderr="rfc0017-test-inventory:fail:x")
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_inventory(pathlib.Path("build"), ["core_anonymous_public_read"])
        self.assertTrue(str(refused.exception).startswith("inventory-failed:"))


class CourseSelectorRunTests(CapturingCourse):
    def test_the_run_covers_exactly_the_audited_set(self) -> None:
        names = course.course_selector_names(dev_listing())
        self.capture(summary(len(names)))
        self.assertEqual(
            course.check_course_selectors(pathlib.Path("build"), names),
            len(names),
        )
        command = self.calls[0]["command"]
        self.assertIn("--no-tests=error", command)
        self.assertEqual(
            command[command.index("-R") + 1], course.selector_pattern(names)
        )

    def test_a_run_shorter_than_the_audit_is_refused(self) -> None:
        names = course.course_selector_names(dev_listing())
        self.capture(summary(len(names) - 1))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_course_selectors(pathlib.Path("build"), names)
        self.assertTrue(
            str(refused.exception).startswith("course-selector-count-mismatch:")
        )

    def test_a_failing_selector_is_refused(self) -> None:
        self.capture(summary(17, failed=1), returncode=8)
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_course_selectors(
                pathlib.Path("build"), course.course_selector_names(dev_listing())
            )
        self.assertTrue(
            str(refused.exception).startswith("course-selectors-failed:")
        )

    def test_an_unreadable_summary_is_refused(self) -> None:
        self.capture("nothing that looks like a summary\n")
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_course_selectors(
                pathlib.Path("build"), course.course_selector_names(dev_listing())
            )
        self.assertEqual(
            str(refused.exception), "course-selector-summary-unreadable"
        )


class PreviewSelectionTests(CapturingCourse):
    """Facts and verdict fail independently, and every failure is named."""

    def test_the_shipped_unavailable_selection_passes(self) -> None:
        self.capture(probe_output())
        facts = course.check_preview_selection(
            pathlib.Path("shipped"), "unavailable", {"PATH": "base"}
        )
        self.assertEqual(facts["PUBLIC_PREVIEW_STAGE"], "unavailable")
        command = self.calls[0]["command"]
        self.assertEqual(
            command[1:],
            ["--self-test", "--public-preview-probe",
             "--automated-run-budget-ms", "20000"],
        )
        environment = self.calls[0]["environment"]
        self.assertEqual(environment["QT_QPA_PLATFORM"], "offscreen")
        self.assertEqual(environment["PATH"], "base")

    def test_an_enabled_stage_under_the_shipped_selection_is_refused(self) -> None:
        self.capture(probe_output(composed="true", stage="enabled",
                                  verdict="PUBLIC_PREVIEW_ENABLED_OK"))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "unavailable")
        self.assertEqual(
            str(refused.exception), "preview-composed-not-false:true"
        )

    def test_a_wrong_stage_is_refused_by_name(self) -> None:
        self.capture(probe_output(stage="refused"))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "unavailable")
        self.assertEqual(
            str(refused.exception), "preview-stage-not-unavailable:refused"
        )

    def test_each_nonzero_effect_is_refused_by_its_own_name(self) -> None:
        for keyword, reason in (
            ("requests", "preview-requests-not-zero:1"),
            ("header", "preview-header-effects-not-zero:1"),
            ("vault", "preview-vault-effects-not-zero:1"),
        ):
            with self.subTest(effect=keyword):
                self.capture(probe_output(**{keyword: "1"}))
                with self.assertRaises(course.CourseFailure) as refused:
                    course.check_preview_selection(
                        pathlib.Path("shipped"), "unavailable"
                    )
                self.assertEqual(str(refused.exception), reason)

    def test_an_absent_verdict_is_refused_even_when_every_fact_holds(self) -> None:
        self.capture(probe_output(verdict=None))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "unavailable")
        self.assertEqual(
            str(refused.exception), "preview-unavailable-verdict-absent"
        )

    def test_a_failed_verdict_is_not_read_as_the_passing_one(self) -> None:
        self.capture(probe_output(verdict="PUBLIC_PREVIEW_UNAVAILABLE_FAILED"))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "unavailable")
        self.assertEqual(
            str(refused.exception), "preview-unavailable-verdict-absent"
        )

    def test_an_absent_counter_is_refused_rather_than_read_as_zero(self) -> None:
        self.capture(
            "PUBLIC_PREVIEW_COMPOSED:false\nPUBLIC_PREVIEW_STAGE:unavailable\n"
            "PUBLIC_PREVIEW_UNAVAILABLE_OK\n"
        )
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "unavailable")
        self.assertEqual(
            str(refused.exception),
            "preview-marker-absent:PUBLIC_PREVIEW_REQUESTS",
        )

    def test_a_probe_that_printed_nothing_is_refused(self) -> None:
        self.capture("STARTUP_OK\n")
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "unavailable")
        self.assertEqual(
            str(refused.exception), "preview-probe-produced-no-markers"
        )

    def test_a_nonzero_self_test_is_refused(self) -> None:
        self.capture(probe_output(), returncode=1)
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "unavailable")
        self.assertEqual(str(refused.exception), "self-test-nonzero")

    def test_the_enabled_selection_requires_its_own_verdict_and_zero_effects(self) -> None:
        self.capture(probe_output(composed="true", stage="enabled",
                                  verdict="PUBLIC_PREVIEW_ENABLED_OK"))
        course.check_preview_selection(pathlib.Path("shipped"), "enabled")

        self.capture(probe_output(composed="true", stage="enabled",
                                  verdict="PUBLIC_PREVIEW_UNAVAILABLE_OK"))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "enabled")
        self.assertEqual(
            str(refused.exception), "preview-enabled-verdict-absent"
        )

        self.capture(probe_output(composed="true", stage="enabled", requests="1",
                                  verdict="PUBLIC_PREVIEW_ENABLED_OK"))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "enabled")
        self.assertEqual(str(refused.exception), "preview-requests-not-zero:1")

    def test_an_unavailable_answer_from_an_enabled_platform_is_refused(self) -> None:
        self.capture(probe_output())
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_preview_selection(pathlib.Path("shipped"), "enabled")
        self.assertEqual(str(refused.exception), "preview-composed-not-true:false")


class EnterpriseRefusalTests(CapturingCourse):
    def test_the_enterprise_probe_names_the_invalid_host(self) -> None:
        self.capture(probe_output(composed="true", stage="refused",
                                  verdict="PUBLIC_PREVIEW_REFUSED_OK"))
        course.check_ghes_refusal(pathlib.Path("shipped"), {"PATH": "base"})
        command = self.calls[0]["command"]
        self.assertEqual(
            command[command.index("--ghes-host") + 1], "ghe.example.invalid"
        )

    def test_the_shipped_selection_answers_unavailable_and_still_passes(self) -> None:
        self.capture(probe_output())
        course.check_ghes_refusal(pathlib.Path("shipped"))

    def test_an_enterprise_host_that_enabled_the_mode_is_refused(self) -> None:
        self.capture(probe_output(composed="true", stage="enabled",
                                  verdict="PUBLIC_PREVIEW_ENABLED_OK"))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_ghes_refusal(pathlib.Path("shipped"))
        self.assertEqual(
            str(refused.exception), "preview-ghes-stage-unexpected:enabled"
        )

    def test_a_request_under_an_enterprise_host_is_refused(self) -> None:
        self.capture(probe_output(composed="true", stage="refused", requests="1",
                                  verdict="PUBLIC_PREVIEW_REFUSED_OK"))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_ghes_refusal(pathlib.Path("shipped"))
        self.assertEqual(
            str(refused.exception), "preview-ghes-requests-not-zero:1"
        )

    def test_a_failed_enterprise_verdict_is_refused(self) -> None:
        self.capture(probe_output(composed="true", stage="refused",
                                  verdict="PUBLIC_PREVIEW_REFUSED_FAILED"))
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_ghes_refusal(pathlib.Path("shipped"))
        self.assertEqual(
            str(refused.exception),
            "preview-ghes-verdict-failed:PUBLIC_PREVIEW_REFUSED_FAILED",
        )


class LaunchEnvironmentTests(unittest.TestCase):
    """BPR-0511, reused rather than re-solved: the tree declares the runtime."""

    def inventory_document(self, modification: list[str]) -> dict[str, Any]:
        return {
            "kind": "ctestInfo",
            "tests": [
                {
                    "name": "test_0",
                    "command": ["binary"],
                    "properties": [
                        {"name": "ENVIRONMENT_MODIFICATION", "value": modification}
                    ],
                }
            ],
        }

    def test_windows_takes_the_declared_qt_directory(self) -> None:
        environment = course.environment_from_inventory(
            self.inventory_document(
                [f"PATH=path_list_prepend:{QT_DIRECTORY}",
                 f"QT_QPA_FONTDIR=set:{FONT_DIRECTORY}"]
            ),
            {"PATH": "C:/Windows/system32"},
            "windows",
        )
        self.assertEqual(
            environment["PATH"],
            os.pathsep.join([QT_DIRECTORY, "C:/Windows/system32"]),
        )
        self.assertEqual(environment["QT_QPA_FONTDIR"], FONT_DIRECTORY)

    def test_a_windows_tree_declaring_no_qt_directory_is_refused_by_name(self) -> None:
        with self.assertRaises(course.CourseFailure) as refused:
            course.environment_from_inventory(
                self.inventory_document(["PYTHONPATH=unset:"]),
                {"PATH": "base"},
                "windows",
            )
        self.assertEqual(str(refused.exception), "qt-runtime-directory-undeclared")

    def test_other_platforms_launch_in_the_ambient_environment(self) -> None:
        base = {"PATH": "/usr/bin", "HOME": "/home/x"}
        for platform_name in ("macos", "linux"):
            with self.subTest(platform=platform_name):
                self.assertEqual(
                    course.environment_from_inventory(
                        self.inventory_document(["PYTHONPATH=unset:"]),
                        base,
                        platform_name,
                    ),
                    base,
                )


if __name__ == "__main__":
    unittest.main()
