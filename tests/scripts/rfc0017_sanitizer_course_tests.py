#!/usr/bin/env python3
"""Falsify the RFC-BPR-0017 sanitizer course without configuring a build.

The course's value is that it cannot quietly run less than it should and
cannot report health over a sanitizer report. Neither property is observable
from a green run, and reproducing either by hand means a fresh instrumented
build. So these cases drive the derivation and the verdict directly: a
fabricated CTest listing stands in for the sanitized graph, and fabricated
CTest output stands in for the run.

What is pinned: the owed selector set comes from the inventory's anchors, so a
Qt-free graph owes the selectors it can build and nothing else; a selector the
graph owes but did not register, one registered twice, and one registered by a
graph whose anchor says it cannot build it are three separate named refusals
before anything runs; an empty owed set is refused rather than reported as a
pass; a sanitizer diagnostic fails the course even when CTest exited zero and
reported every test passing; and the configured tree carries the RFC-BPR-0016
ledger's exact toolchain.

Owned by BPR-0306.
"""

from __future__ import annotations

import pathlib
import subprocess
import sys
import unittest
from typing import Any


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from scripts import verify_rfc0017_sanitizer_course as course  # noqa: E402

# The course imports the inventory as a top-level module from `scripts/`;
# importing `scripts.verify_rfc0017_test_inventory` here would produce a
# second module object, and patching that one would patch nothing the
# course reads. Take the course's own reference instead.
inventory = course.rfc0017_inventory


# A Qt-free instrumented graph: the core policy suite, the application host
# and its anchor, the SQLite epoch authority and its anchor, and one test that
# has nothing to do with this RFC. No native-HTTPS fixture, no Qt scale
# courses, no GHES public-path anchor — exactly what `BPR_BUILD_QT=OFF` leaves.
SANITIZED_GRAPH = [
    "core_anonymous_public_read",
    "application_github_host",
    "application_anonymous_read_host",
    "application_anonymous_diff_authority",
    "sqlite_epoch_catalog_storage_contract",
    "sqlite_anonymous_epoch_authority",
    "core_review_ledger",
]
OWED = [
    "application_anonymous_diff_authority",
    "application_anonymous_read_host",
    "core_anonymous_public_read",
    "sqlite_anonymous_epoch_authority",
]


def listing(names: list[str]) -> str:
    lines = ["Test project /build"]
    lines += [f"  Test #{index + 1}: {name}" for index, name in enumerate(names)]
    lines.append(f"Total Tests: {len(names)}")
    return "\n".join(lines) + "\n"


def summary(total: int, failed: int = 0) -> str:
    percent = 100 if failed == 0 else 0
    clause = "" if failed == 0 else f", {failed} tests failed"
    return f"\n{percent}% tests passed{clause} out of {total}\n"


class DerivationTests(unittest.TestCase):
    def test_a_qt_free_graph_owes_what_it_can_build_and_nothing_else(self) -> None:
        self.assertEqual(course.derive_selectors(SANITIZED_GRAPH), OWED)

    def test_a_selector_the_graph_owes_but_did_not_register_is_refused(self) -> None:
        names = [
            name for name in SANITIZED_GRAPH
            if name != "application_anonymous_read_host"
        ]
        with self.assertRaises(course.CourseFailure) as refused:
            course.derive_selectors(names)
        self.assertTrue(
            str(refused.exception).startswith(
                "sanitizer-selector-missing:application_anonymous_read_host:"
            ),
            str(refused.exception),
        )

    def test_a_selector_registered_twice_is_refused(self) -> None:
        with self.assertRaises(course.CourseFailure) as refused:
            course.derive_selectors(SANITIZED_GRAPH + ["core_anonymous_public_read"])
        self.assertTrue(
            str(refused.exception).startswith(
                "sanitizer-selector-duplicate:core_anonymous_public_read:2:1"
            ),
            str(refused.exception),
        )

    def test_a_selector_whose_graph_cannot_build_it_is_refused(self) -> None:
        names = [
            name for name in SANITIZED_GRAPH
            if name != "sqlite_epoch_catalog_storage_contract"
        ]
        with self.assertRaises(course.CourseFailure) as refused:
            course.derive_selectors(names)
        self.assertTrue(
            str(refused.exception).startswith(
                "sanitizer-selector-extra:sqlite_anonymous_epoch_authority:1:"
            ),
            str(refused.exception),
        )

    def test_a_wire_block_selector_outside_the_block_is_an_extra(self) -> None:
        original = inventory.WIRE_BLOCK_PLATFORMS
        inventory.WIRE_BLOCK_PLATFORMS = frozenset()
        self.addCleanup(
            setattr, inventory, "WIRE_BLOCK_PLATFORMS", original
        )
        names = SANITIZED_GRAPH + [
            "native_https_transport_fixture",
            "rfc0017_credential_free_native_https",
            "native_https_anonymous_read_fixture",
        ]
        with self.assertRaises(course.CourseFailure) as refused:
            course.derive_selectors(names)
        self.assertIn("sanitizer-selector-extra:", str(refused.exception))

    def test_a_graph_owing_nothing_is_refused_rather_than_passed(self) -> None:
        with self.assertRaises(course.CourseFailure) as refused:
            course.derive_selectors(["core_review_ledger"])
        # The always-owed core selector is the one this graph is missing.
        self.assertTrue(
            str(refused.exception).startswith(
                "sanitizer-selector-missing:core_anonymous_public_read:"
            ),
            str(refused.exception),
        )

    def test_an_empty_owed_set_is_refused(self) -> None:
        original = dict(inventory.SELECTOR_ANCHORS)
        anchors = dict(original)
        anchors["core_anonymous_public_read"] = ("absent_anchor", False)
        inventory.SELECTOR_ANCHORS = anchors
        self.addCleanup(setattr, inventory, "SELECTOR_ANCHORS", original)
        with self.assertRaises(course.CourseFailure) as refused:
            course.derive_selectors(["core_review_ledger"])
        self.assertEqual(str(refused.exception), "sanitizer-selectors-empty")


class DiagnosticTests(unittest.TestCase):
    def test_clean_output_passes(self) -> None:
        course.check_sanitizer_diagnostics("1/4 Test #1: core ... Passed\n")

    def test_an_address_sanitizer_report_is_refused(self) -> None:
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_sanitizer_diagnostics(
                "==11==ERROR: AddressSanitizer: heap-use-after-free on 0x1\n"
            )
        self.assertIn("sanitizer-diagnostic:", str(refused.exception))
        self.assertIn("heap-use-after-free", str(refused.exception))

    def test_an_undefined_behavior_report_is_refused(self) -> None:
        with self.assertRaises(course.CourseFailure) as refused:
            course.check_sanitizer_diagnostics(
                "diff.cpp:42:9: runtime error: signed integer overflow\n"
            )
        self.assertIn("signed integer overflow", str(refused.exception))


class CapturingCourse(unittest.TestCase):
    def setUp(self) -> None:
        self.calls: list[dict[str, Any]] = []
        self.original = course.run

    def tearDown(self) -> None:
        course.run = self.original

    def capture(self, stdout: str, returncode: int = 0, stderr: str = "") -> None:
        def fake_run(command, *, cwd=None, timeout=3600, environment=None):
            self.calls.append({"command": list(command)})
            return subprocess.CompletedProcess(
                command, returncode, stdout=stdout, stderr=stderr
            )

        course.run = fake_run


class ToolchainTests(CapturingCourse):
    def test_the_tree_carries_the_ledger_toolchain(self) -> None:
        self.capture("")
        course.configure_and_build(pathlib.Path("build/sanitize"), False)
        configure = self.calls[0]["command"]
        self.assertEqual(configure[:2], ["cmake", "-S"])
        self.assertIn("-DBPR_BUILD_QT=OFF", configure)
        self.assertIn("-DCMAKE_BUILD_TYPE=Debug", configure)
        self.assertIn(
            "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer",
            configure,
        )
        self.assertIn(
            "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined", configure
        )
        self.assertIn("Ninja", configure)
        self.assertNotIn("--fresh", configure)
        self.assertNotIn("--clean-first", self.calls[1]["command"])

    def test_fresh_reconfigures_and_rebuilds_from_clean_state(self) -> None:
        self.capture("")
        course.configure_and_build(pathlib.Path("build/sanitize"), True)
        self.assertIn("--fresh", self.calls[0]["command"])
        self.assertIn("--clean-first", self.calls[1]["command"])

    def test_a_failing_configure_stops_before_the_build(self) -> None:
        self.capture("", returncode=1, stderr="no Ninja generator")
        with self.assertRaises(course.CourseFailure) as refused:
            course.configure_and_build(pathlib.Path("build/sanitize"), False)
        self.assertTrue(str(refused.exception).startswith("configure-failed:"))
        self.assertEqual(len(self.calls), 1)


class RunTests(CapturingCourse):
    def test_the_run_names_exactly_the_derived_selectors(self) -> None:
        self.capture(summary(len(OWED)))
        course.run_selectors(pathlib.Path("build/sanitize"), OWED)
        command = self.calls[0]["command"]
        self.assertIn("--no-tests=error", command)
        pattern = command[command.index("-R") + 1]
        self.assertTrue(pattern.startswith("^(") and pattern.endswith(")$"))
        for name in OWED:
            self.assertIn(name, pattern)

    def test_a_diagnostic_fails_a_run_ctest_called_green(self) -> None:
        self.capture(
            summary(len(OWED)) + "diff.cpp:42:9: runtime error: null pointer\n"
        )
        with self.assertRaises(course.CourseFailure) as refused:
            course.run_selectors(pathlib.Path("build/sanitize"), OWED)
        self.assertIn("sanitizer-diagnostic:", str(refused.exception))

    def test_a_diagnostic_on_standard_error_is_read_too(self) -> None:
        self.capture(
            summary(len(OWED)),
            stderr="==1==ERROR: AddressSanitizer: stack-buffer-overflow\n",
        )
        with self.assertRaises(course.CourseFailure) as refused:
            course.run_selectors(pathlib.Path("build/sanitize"), OWED)
        self.assertIn("stack-buffer-overflow", str(refused.exception))

    def test_a_failing_selector_is_refused(self) -> None:
        self.capture(summary(len(OWED), failed=1), returncode=8)
        with self.assertRaises(course.CourseFailure) as refused:
            course.run_selectors(pathlib.Path("build/sanitize"), OWED)
        self.assertTrue(
            str(refused.exception).startswith("sanitizer-selectors-failed:")
        )

    def test_a_run_shorter_than_the_derivation_is_refused(self) -> None:
        self.capture(summary(len(OWED) - 1))
        with self.assertRaises(course.CourseFailure) as refused:
            course.run_selectors(pathlib.Path("build/sanitize"), OWED)
        self.assertTrue(
            str(refused.exception).startswith("sanitizer-selector-count-mismatch:")
        )


class ListingTests(CapturingCourse):
    def test_the_listing_keeps_duplicates(self) -> None:
        self.capture(listing(["a", "a", "b"]))
        self.assertEqual(
            course.registered_names(pathlib.Path("build/sanitize")),
            ["a", "a", "b"],
        )

    def test_an_empty_listing_is_refused(self) -> None:
        self.capture(listing([]))
        with self.assertRaises(course.CourseFailure) as refused:
            course.registered_names(pathlib.Path("build/sanitize"))
        self.assertEqual(str(refused.exception), "sanitizer-registration-empty")

    def test_an_unreadable_listing_is_refused(self) -> None:
        self.capture("", returncode=1, stderr="no such build directory")
        with self.assertRaises(course.CourseFailure) as refused:
            course.registered_names(pathlib.Path("build/sanitize"))
        self.assertTrue(
            str(refused.exception).startswith("sanitizer-registration-unreadable:")
        )


if __name__ == "__main__":
    unittest.main()
