"""Name every case a unittest run did not check, in the run's own output.

unittest's default output shows a skipped case as one ``s`` and a
``skipped=N`` count, and an expected failure as one ``x``. A reader of a green
row cannot tell which guarantee was not checked, or why, and a second case
skipping later for a worse reason looks exactly the same ([[BPR-0871]]).

A script that passes ``testRunner=SkipReportingTestRunner`` to
``unittest.main`` ends its run with one line per such case, just before
unittest's own summary::

    SKIPPED: <script>.<Class>.<case>: <reason>
    EXPECTED FAILURE: <script>.<Class>.<case>

Each line carries the whole identity, with the script's name in place of
``__main__``, and the reason. So one search over a sweep's log lists every case
the sweep did not check, and which row it was in::

    grep -h -e '^SKIPPED: ' -e '^EXPECTED FAILURE: ' Testing/Temporary/LastTest.log

CTest keeps only the first KiB of a passing row's output in its JUnit report,
and these lines come last. So when there is anything to name, the report also
prints ``CTEST_FULL_OUTPUT``, the string CTest documents as keeping a test's
whole output (``CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE``). A run with
nothing to name prints nothing extra.

``skip_report_tests.py`` pins this. It also fails when a unittest script under
``tests/`` can skip a case and does not run through this runner.
"""

from __future__ import annotations

from pathlib import Path
import sys
import unittest

FULL_OUTPUT_DIRECTIVE = "CTEST_FULL_OUTPUT"
SKIPPED_PREFIX = "SKIPPED: "
EXPECTED_FAILURE_PREFIX = "EXPECTED FAILURE: "


def case_identity(test: unittest.TestCase) -> str:
    """The case's dotted identity, naming the script rather than ``__main__``."""
    identity = test.id()
    main_file = getattr(sys.modules.get("__main__"), "__file__", None)
    if main_file and identity.startswith("__main__."):
        return Path(main_file).stem + identity[len("__main__"):]
    return identity


def _one_line(text: object) -> str:
    return " ".join(str(text).split())


class SkipReportingTestResult(unittest.TextTestResult):
    def printErrors(self) -> None:
        super().printErrors()
        lines = [
            f"{SKIPPED_PREFIX}{case_identity(test)}: {_one_line(reason)}"
            for test, reason in self.skipped
        ]
        lines.extend(
            f"{EXPECTED_FAILURE_PREFIX}{case_identity(test)}"
            for test, _ in self.expectedFailures
        )
        if not lines:
            return
        count = (
            "1 case was not checked"
            if len(lines) == 1
            else f"{len(lines)} cases were not checked"
        )
        self.stream.writeln(self.separator1)
        self.stream.writeln(
            f"{count} ({FULL_OUTPUT_DIRECTIVE} keeps this list in CTest's report):"
        )
        for line in lines:
            self.stream.writeln(line)
        self.stream.flush()


class SkipReportingTestRunner(unittest.TextTestRunner):
    resultclass = SkipReportingTestResult
