#!/usr/bin/env python3
"""A unittest row names every case it did not check, and why ([[BPR-0871]]).

`skip_report.py` adds one line per skipped case, with its reason, and one per
expected failure, before unittest's own summary. The first cases here pin that
output. The last case finds a row that could skip without naming the case,
before it does: every unittest script under `tests/` that can skip a case must
pass `testRunner=SkipReportingTestRunner` to `unittest.main`.

## What the scan does not catch

It reads each script's own text. A skip raised inside a helper module the
script imports, or a `unittest.main` reached through an alias such as
`from unittest import main`, is not seen. Neither form exists in `tests/`
today.
"""

from __future__ import annotations

import ast
import contextlib
import io
from pathlib import Path
import re
import sys
import types
import unittest

from skip_report import (
    EXPECTED_FAILURE_PREFIX,
    FULL_OUTPUT_DIRECTIVE,
    SKIPPED_PREFIX,
    SkipReportingTestRunner,
    case_identity,
)

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SCAN_ROOT = REPOSITORY_ROOT / "tests"

# The ways a case can end without checking its guarantee: skipTest, the skip
# decorators, a raised SkipTest, and an expected failure.
SKIP_CONSTRUCT = re.compile(
    r"\b(?:skipTest|skipIf|skipUnless|SkipTest|expectedFailure)\b|\bunittest\.skip\b"
)
RUNNER_NAME = "SkipReportingTestRunner"


def fixture_class(module: str = "fixture_module", **cases: object) -> type:
    """A TestCase class holding the given cases, outside this script's own suite."""
    return type("Fixture", (unittest.TestCase,), {"__module__": module, **cases})


def run(*tests: unittest.TestCase) -> tuple[unittest.TestResult, str]:
    stream = io.StringIO()
    result = SkipReportingTestRunner(stream=stream, verbosity=1).run(
        unittest.TestSuite(tests)
    )
    return result, stream.getvalue()


def passes(self: unittest.TestCase) -> None:
    pass


def skips_for_one_reason(self: unittest.TestCase) -> None:
    self.skipTest("the first reason")


def skips_for_another_reason(self: unittest.TestCase) -> None:
    self.skipTest("a second,\n different reason")


@unittest.expectedFailure
def fails_as_expected(self: unittest.TestCase) -> None:
    self.fail("a known defect")


def unittest_main_calls(tree: ast.AST) -> list[ast.Call]:
    return [
        node
        for node in ast.walk(tree)
        if isinstance(node, ast.Call)
        and isinstance(node.func, ast.Attribute)
        and node.func.attr == "main"
        and isinstance(node.func.value, ast.Name)
        and node.func.value.id == "unittest"
    ]


def uses_reporting_runner(call: ast.Call) -> bool:
    return any(
        keyword.arg == "testRunner"
        and isinstance(keyword.value, ast.Name)
        and keyword.value.id == RUNNER_NAME
        for keyword in call.keywords
    )


def skip_capable_unittest_scripts() -> dict[Path, list[ast.Call]]:
    """Each script under tests/ that runs unittest.main and can skip a case."""
    found: dict[Path, list[ast.Call]] = {}
    for path in sorted(SCAN_ROOT.rglob("*.py")):
        if "__pycache__" in path.parts:
            continue
        source = path.read_text(encoding="utf-8")
        if not SKIP_CONSTRUCT.search(source):
            continue
        calls = unittest_main_calls(ast.parse(source, filename=str(path)))
        if calls:
            found[path] = calls
    return found


class the_report_names_each_case_that_was_not_checked(unittest.TestCase):
    def test_a_skipped_case_is_named_with_its_reason_before_the_summary(self) -> None:
        cases = fixture_class(test_passes=passes, test_skips=skips_for_one_reason)
        result, output = run(cases("test_passes"), cases("test_skips"))

        self.assertTrue(result.wasSuccessful(), output)
        line = f"{SKIPPED_PREFIX}fixture_module.Fixture.test_skips: the first reason"
        self.assertIn(line + "\n", output)
        self.assertIn("1 case was not checked", output)
        self.assertLess(output.index(line), output.index("Ran 2 tests"))
        self.assertIn("OK (skipped=1)", output)
        self.assertNotIn("test_passes", output)

    def test_two_skips_for_different_reasons_are_told_apart(self) -> None:
        cases = fixture_class(
            test_first=skips_for_one_reason, test_second=skips_for_another_reason
        )
        _, output = run(cases("test_first"), cases("test_second"))

        lines = [line for line in output.splitlines() if line.startswith(SKIPPED_PREFIX)]
        self.assertEqual(
            lines,
            [
                f"{SKIPPED_PREFIX}fixture_module.Fixture.test_first: the first reason",
                f"{SKIPPED_PREFIX}fixture_module.Fixture.test_second:"
                " a second, different reason",
            ],
        )
        self.assertIn("2 cases were not checked", output)

    def test_an_expected_failure_is_named(self) -> None:
        cases = fixture_class(test_known_defect=fails_as_expected)
        result, output = run(cases("test_known_defect"))

        self.assertTrue(result.wasSuccessful(), output)
        self.assertIn(
            f"{EXPECTED_FAILURE_PREFIX}fixture_module.Fixture.test_known_defect\n",
            output,
        )

    def test_a_class_level_skip_names_every_case(self) -> None:
        cases = unittest.skip("a class-wide reason")(
            fixture_class(test_one=passes, test_two=passes)
        )
        _, output = run(cases("test_one"), cases("test_two"))

        for name in ("test_one", "test_two"):
            self.assertIn(
                f"{SKIPPED_PREFIX}fixture_module.Fixture.{name}: a class-wide reason\n",
                output,
            )

    def test_the_report_asks_ctest_to_keep_the_whole_output(self) -> None:
        # CTest keeps only the first KiB of a passing test's output in its
        # JUnit report unless the output contains this string.
        cases = fixture_class(test_skips=skips_for_one_reason)
        _, output = run(cases("test_skips"))

        self.assertEqual(FULL_OUTPUT_DIRECTIVE, "CTEST_FULL_OUTPUT")
        self.assertIn(FULL_OUTPUT_DIRECTIVE, output)

    def test_a_run_with_nothing_to_name_prints_nothing_extra(self) -> None:
        cases = fixture_class(test_passes=passes)
        result, output = run(cases("test_passes"))

        self.assertTrue(result.wasSuccessful(), output)
        self.assertNotIn(FULL_OUTPUT_DIRECTIVE, output)
        self.assertNotIn(SKIPPED_PREFIX, output)
        self.assertNotIn("not checked", output)

    def test_a_script_case_is_named_by_its_script_not_main(self) -> None:
        cases = fixture_class(module="__main__", test_passes=passes)
        main_file = getattr(sys.modules["__main__"], "__file__", None)
        if main_file is None:
            self.skipTest("this run's __main__ has no file to name a script by")

        self.assertEqual(
            case_identity(cases("test_passes")),
            f"{Path(main_file).stem}.Fixture.test_passes",
        )

    def test_unittest_main_reports_through_the_runner_class(self) -> None:
        # The rows pass the class, and unittest.main builds the runner itself.
        module = types.ModuleType("fixture_module")
        module.Fixture = fixture_class(
            test_passes=passes, test_skips=skips_for_one_reason
        )
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            program = unittest.main(
                module=module,
                argv=["fixture"],
                testRunner=SkipReportingTestRunner,
                exit=False,
            )

        self.assertTrue(program.result.wasSuccessful(), stderr.getvalue())
        self.assertIn(
            f"{SKIPPED_PREFIX}fixture_module.Fixture.test_skips: the first reason\n",
            stderr.getvalue(),
        )


class every_row_that_can_skip_names_its_skips(unittest.TestCase):
    def test_every_unittest_script_that_can_skip_runs_through_the_runner(self) -> None:
        scripts = skip_capable_unittest_scripts()
        # The row the defect was found in must be among those scanned, or the
        # scan has stopped seeing what it is for.
        self.assertIn(
            SCAN_ROOT / "scripts" / "dependency_modernization_tests.py", scripts
        )
        unreported = [
            f"{path.relative_to(REPOSITORY_ROOT).as_posix()}:{call.lineno}"
            for path, calls in scripts.items()
            for call in calls
            if not uses_reporting_runner(call)
        ]
        self.assertEqual(
            unreported,
            [],
            "these scripts can skip a case but call unittest.main without"
            f" testRunner={RUNNER_NAME}, so a skip would show only as an 's'",
        )


if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
