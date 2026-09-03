#!/usr/bin/env python3
"""Run RFC-BPR-0017's selectors under AddressSanitizer and UndefinedBehaviorSanitizer.

The anonymous public preview parses attacker-shaped input: JSON and headers
from an unauthenticated GitHub read, packfile bytes from a public smart-HTTP
fetch, and diffs rendered from both. A memory error or an undefined operation
in that path is a security defect, not a stability one, and the ordinary
matrices cannot see it because they run an unsanitized build.

RFC-BPR-0016's ledger ran this by hand: an isolated Qt-free Debug tree with
`-fsanitize=address,undefined`, then an exact selector set. Written by hand it
was reproducible only by transcription, and a selector quietly dropped from
the list would have made the run shorter rather than red. This script is that
course with the two failures closed: the isolated tree is configured with the
same flags every time, and the selectors are derived from
`verify_rfc0017_test_inventory` rather than retyped, so what runs here is what
the RFC's inventory says this graph owes.

A sanitized graph owes less than the full one. `BPR_BUILD_QT=OFF` builds no Qt
and no native-HTTPS loopback fixture, so the inventory's anchored selectors —
the wire suites and the Qt scale courses — are owed zero here and their
absence is a graph fact rather than a missing registration. `selector_expected`
already knows the difference; this course asks it instead of guessing.

Owned by BPR-0306.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys
import time
from collections import Counter
from typing import Sequence


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent

if str(REPOSITORY_ROOT / "scripts") not in sys.path:
    sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))

import verify_rfc0016_platform_course as rfc0016  # noqa: E402
import verify_rfc0017_test_inventory as rfc0017_inventory  # noqa: E402


CourseFailure = rfc0016.CourseFailure
require = rfc0016.require
run = rfc0016.run

# The RFC-BPR-0016 ledger's exact toolchain, kept verbatim so a run here and
# the hand run recorded there measure the same build. `-fno-omit-frame-pointer`
# is what makes an ASan report name the frame that did it.
SANITIZE_FLAGS = "-fsanitize=address,undefined"
CONFIGURE_DEFINITIONS = (
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DBPR_BUILD_QT=OFF",
    f"-DCMAKE_CXX_FLAGS={SANITIZE_FLAGS} -fno-omit-frame-pointer",
    f"-DCMAKE_EXE_LINKER_FLAGS={SANITIZE_FLAGS}",
)

# What a sanitizer says when it has found something. Both runtimes report on
# stderr and keep going by default, so a UBSan `runtime error:` can leave a
# test — and therefore CTest — green. The scan is the verdict here; the exit
# status is only its second opinion.
SANITIZER_DIAGNOSTICS = (
    "ERROR: AddressSanitizer",
    "runtime error:",
)


def configure_and_build(build_root: pathlib.Path, fresh: bool) -> None:
    configure = [
        "cmake",
        "-S",
        str(REPOSITORY_ROOT),
        "-B",
        str(build_root),
        "-G",
        "Ninja",
        *CONFIGURE_DEFINITIONS,
    ]
    if fresh:
        configure.append("--fresh")
    result = run(configure, cwd=REPOSITORY_ROOT)
    require(
        result.returncode == 0,
        f"configure-failed:{(result.stdout + result.stderr).strip()[-600:]}",
    )
    build = ["cmake", "--build", str(build_root)]
    if fresh:
        build.append("--clean-first")
    result = run(build, cwd=REPOSITORY_ROOT)
    require(
        result.returncode == 0,
        f"build-failed:{(result.stdout + result.stderr).strip()[-600:]}",
    )


def registered_names(build_root: pathlib.Path) -> list[str]:
    """Every name CTest lists in the sanitized tree, duplicates kept."""
    result = run(
        ["ctest", "--test-dir", str(build_root), "-N"],
        cwd=REPOSITORY_ROOT,
        timeout=600,
    )
    require(
        result.returncode == 0,
        f"sanitizer-registration-unreadable:{result.stderr.strip()[:400]}",
    )
    names = re.findall(r"Test\s+#\d+:\s+(\S+)", result.stdout)
    require(bool(names), "sanitizer-registration-empty")
    return names


def derive_selectors(names: Sequence[str]) -> list[str]:
    """Exactly the RFC-BPR-0017 selectors this graph owes, refusing surprises.

    Three refusals, before anything runs. A selector the graph owes and did
    not register would make the course pass by running less; one registered
    twice would make the count stop describing the run; and one registered by
    a graph that does not owe it means the inventory's anchor is wrong about
    what this build contains. None of the three can be told apart afterwards
    from a green matrix, so all three are named here.
    """
    counts = Counter(names)
    listed = list(names)
    owed: list[str] = []
    for selector in sorted(rfc0017_inventory.SELECTORS):
        expected, reason = rfc0017_inventory.selector_expected(selector, listed)
        found = counts.get(selector, 0)
        if expected == 0:
            require(
                found == 0,
                f"sanitizer-selector-extra:{selector}:{found}:{reason}",
            )
            continue
        require(
            found != 0,
            f"sanitizer-selector-missing:{selector}:{reason}",
        )
        require(
            found == expected,
            f"sanitizer-selector-duplicate:{selector}:{found}:{expected}",
        )
        owed.append(selector)
    # A graph that owes nothing would let this course pass having measured
    # nothing at all, which is the one outcome it must never report as health.
    require(bool(owed), "sanitizer-selectors-empty")
    return owed


def check_sanitizer_diagnostics(output: str) -> None:
    """Any sanitizer report is a failure, whatever CTest concluded."""
    for line in output.splitlines():
        for diagnostic in SANITIZER_DIAGNOSTICS:
            if diagnostic in line:
                raise CourseFailure(
                    f"sanitizer-diagnostic:{line.strip()[:300]}"
                )


def run_selectors(build_root: pathlib.Path, selectors: Sequence[str]) -> None:
    """Run exactly the derived selectors and read the sanitizers first."""
    pattern = "^(" + "|".join(re.escape(name) for name in selectors) + ")$"
    result = run(
        [
            "ctest",
            "--test-dir",
            str(build_root),
            "--no-tests=error",
            "--output-on-failure",
            "-R",
            pattern,
        ],
        cwd=REPOSITORY_ROOT,
        timeout=7200,
    )
    output = result.stdout + result.stderr
    # Order matters. A test that died inside an ASan report fails for the
    # sanitizer's reason, and naming the CTest exit status first would bury it.
    check_sanitizer_diagnostics(output)
    require(
        result.returncode == 0,
        f"sanitizer-selectors-failed:{result.stdout.strip()[-600:]}",
    )
    summary = re.search(
        r"(\d+)% tests passed(?:, (\d+) tests failed)? out of (\d+)",
        result.stdout,
    )
    require(summary is not None, "sanitizer-summary-unreadable")
    require(summary.group(1) == "100", "sanitizer-selectors-not-all-passing")
    require(summary.group(2) in (None, "0"), "sanitizer-reported-failures")
    require(
        int(summary.group(3)) == len(selectors),
        f"sanitizer-selector-count-mismatch:{summary.group(3)}:{len(selectors)}",
    )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run the RFC-BPR-0017 selectors this graph owes under "
            "AddressSanitizer and UndefinedBehaviorSanitizer, in an "
            "isolated Qt-free Debug tree."
        )
    )
    parser.add_argument("--build-root", type=pathlib.Path, required=True)
    parser.add_argument(
        "--fresh",
        action="store_true",
        help="Reconfigure and rebuild the sanitized tree from clean state.",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    started = time.monotonic()
    try:
        configure_and_build(arguments.build_root, arguments.fresh)
        selectors = derive_selectors(registered_names(arguments.build_root))
        run_selectors(arguments.build_root, selectors)
    except CourseFailure as error:
        print(f"RFC0017_SANITIZER_COURSE_FAILED:{error}", file=sys.stderr)
        return 1
    except subprocess.TimeoutExpired as error:
        print(
            f"RFC0017_SANITIZER_COURSE_FAILED:timeout:{error.cmd[0]}",
            file=sys.stderr,
        )
        return 1
    except Exception as error:  # noqa: BLE001 - reported, never swallowed
        print(
            f"RFC0017_SANITIZER_COURSE_FAILED:internal:{type(error).__name__}",
            file=sys.stderr,
        )
        return 1

    elapsed = int(time.monotonic() - started)
    print(f"RFC0017_SANITIZER_BUILD_ROOT:{arguments.build_root}")
    for selector in selectors:
        print(f"RFC0017_SANITIZER_SELECTOR:{selector}")
    print(f"RFC0017_SANITIZER_SELECTORS:{len(selectors)}")
    print(f"RFC0017_SANITIZER_ELAPSED_SECONDS:{elapsed}")
    print("RFC0017_SANITIZER_COURSE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
