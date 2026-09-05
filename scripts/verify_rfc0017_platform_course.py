#!/usr/bin/env python3
"""Run RFC-BPR-0017's anonymous-preview course on one platform, from clean state.

RFC-BPR-0017 claims that a shipped BetterPullReview either refuses the
signed-out public preview outright or, once a platform is enabled, enters it
without an Authorization header, without touching the credential vault, and
without sending anything the operator did not ask for. Those are claims about
an artifact on an operating system, not about a source tree, so this course
builds its own Debug and Release trees under an isolated `--build-root`, runs
the RFC's selector inventory before any test, runs exactly the selectors that
inventory knows, and then interrogates the shipped Release artifact.

It takes `--expected-preview-selection` rather than discovering it. A course
that asked the binary what it selects and then asserted it had selected that
would pass on every build; the caller states what this platform is supposed to
answer and the run fails if the artifact disagrees. That is what makes one
script usable before and after the one-time enablement flip.

The platform-neutral machinery is RFC-BPR-0016's and is imported rather than
copied: the configure/build pair, the multi-config resolution, the artifact
locator, the CTest-declared launch environment (BPR-0511), the runtime and
dependency gates, and the symbol reader. What is RFC-BPR-0017's own lives
here: the selector set, the registration refusals, the forbidden-engine scan
of this RFC's artifact, and the public-preview probe.

Owned by BPR-0306 (the macOS unavailable course), BPR-0314 and BPR-0315 (the
Linux and Windows unavailable-path proofs), BPR-0316 (the macOS enablement),
and BPR-0309 (the immutable final-head replay).
"""

from __future__ import annotations

import argparse
import os
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


# The RFC-BPR-0016 course already answers every platform-neutral question this
# one asks, and answering them a second time is how two courses drift. These
# names are the seam; a reader looking for their definitions should look there.
CourseFailure = rfc0016.CourseFailure
require = rfc0016.require
run = rfc0016.run
host_platform = rfc0016.host_platform
configure_and_build = rfc0016.configure_and_build
tree_configuration = rfc0016.tree_configuration
application_binary = rfc0016.application_binary
ctest_inventory = rfc0016.ctest_inventory
environment_from_inventory = rfc0016.environment_from_inventory
launch_environment = rfc0016.launch_environment
check_runtime_and_dependencies = rfc0016.check_runtime_and_dependencies
check_composed_hosts_of_the_artifact = rfc0016.check_composed_hosts_of_the_artifact
symbol_table = rfc0016.symbol_table

PLATFORMS = rfc0016.PLATFORMS
SELECTIONS = rfc0016.SELECTIONS

# Course selectors this RFC's inventory does not carry, each with the reason
# it is owed by the course anyway. Everything else the course runs comes from
# `verify_rfc0017_test_inventory.SELECTORS`, so the two cannot disagree about
# what this RFC's evidence is.
COURSE_ONLY_SELECTORS: dict[str, str] = {
    # BPR-0305's composed public path was a course-only name until its
    # registration landed; the inventory owns it now like its siblings, so it
    # arrives through the owed set rather than being listed twice.
    # BPR-0523: the composed-host audit of the tests-enabled binary, so a
    # platform whose dev graph lost a required constructor fails here rather
    # than in the artifact audit further down.
    "composed_application_hosts": "the composed-host audit of the dev graph",
}

# Every registered name that looks like this RFC's and is deliberately not a
# course selector, with its reason. The course refuses an unlisted one
# (`course-selector-extra`), which is what makes a new RFC-BPR-0017 evidence
# selector that never reached the inventory a failure here instead of a
# silent omission; and it refuses an entry that turns out to be a course
# selector after all, so the table cannot outlive what it excuses.
FAMILY_EXCEPTIONS: dict[str, str] = {
    "rfc0017_test_inventory": (
        "the inventory guard's own registration; the course runs "
        "verify_rfc0017_test_inventory.py directly, naming every selector "
        "this graph owes, before any test"
    ),
    "rfc0017_release_absence_negative_control": (
        "proves the release-absence scan fails against a tests-enabled "
        "binary; the course runs that scan against the shipped artifact"
    ),
    "rfc0017_public_preview_decision_contract": (
        "hermetic contract for BPR-0312's decision validator; it audits a "
        "review packet, never the artifact"
    ),
    "rfc0017_test_inventory_contract": (
        "hermetic contract for the inventory script itself"
    ),
    "rfc0017_platform_course_launch_contract": (
        "this course's own hermetic contract"
    ),
    "rfc0017_sanitizer_course_contract": (
        "the sanitizer course's hermetic contract"
    ),
}

# What "looks like this RFC's" means to CTest. Deliberately wider than the
# course set: it is the net that catches a selector the inventory has not been
# told about. CTest matches `-R` as a search, so no anchors here.
FAMILY_PATTERN = "rfc0017_|anonymous|public_preview"

# The public-preview probe's markers. `<name>:<value>` facts first, then one
# terminal verdict with no value.
PREVIEW_STAGES = ("unavailable", "enabled", "refused")
ZERO_EFFECT_MARKERS = (
    "PUBLIC_PREVIEW_REQUESTS",
    "PUBLIC_PREVIEW_HEADER_EFFECTS",
    "PUBLIC_PREVIEW_VAULT_EFFECTS",
)
GHES_PROBE_HOST = "ghe.example.invalid"


def owed_selectors(names: Sequence[str]) -> list[str]:
    """The inventory selectors this platform's graph owes, and only those.

    Not every platform owes every selector, and `--all` said otherwise. The
    Windows tree registers the native-HTTPS loopback fixture but deliberately
    not this RFC's wire suites (BPR-0446), so insisting on them there would
    fail a graph that is behaving exactly as the inventory describes. The
    inventory already knows the difference — each selector names the sibling
    registration that proves its graph builds the surface it tests — so this
    asks `selector_expected` against the tree's own listing rather than
    carrying a second, hand-maintained platform table that could disagree.

    An anchor that is present makes its selector owed, so a genuinely missing
    registration is still refused; only a surface the graph does not build at
    all drops out.
    """
    listed = list(names)
    return [
        selector
        for selector in sorted(rfc0017_inventory.SELECTORS)
        if rfc0017_inventory.selector_expected(selector, listed)[0] > 0
    ]


def course_selector_names(names: Sequence[str]) -> tuple[str, ...]:
    """What this course runs here: the owed selectors plus its own two.

    The two composition selectors are owed on every platform. Neither is
    anchored on a surface a supported platform can decline to build: one is
    the composed public path this RFC exists to prove, the other is the
    composed-host audit of the graph that proves it.
    """
    return tuple(sorted(set(owed_selectors(names)) | set(COURSE_ONLY_SELECTORS)))


def selector_pattern(names: Sequence[str]) -> str:
    """One anchored alternation, so a looser pattern cannot skip a name."""
    return "^(" + "|".join(re.escape(name) for name in sorted(names)) + ")$"


def listed_selectors(
    build_dir: pathlib.Path,
    pattern: str | None = None,
    configuration: str | None = None,
) -> list[str]:
    """The names CTest lists for a pattern, duplicates kept.

    A set would hide the one failure a registration audit exists to catch
    twice over: two registrations claiming one name means the run that
    follows no longer describes what the inventory counted.
    """
    command = ["ctest", "--test-dir", str(build_dir)]
    if configuration is not None:
        command += ["-C", configuration]
    command.append("-N")
    if pattern is not None:
        command += ["-R", pattern]
    result = run(command, cwd=REPOSITORY_ROOT, timeout=600)
    require(
        result.returncode == 0,
        f"course-registration-unreadable:{result.stderr.strip()[:400]}",
    )
    return re.findall(r"Test\s+#\d+:\s+(\S+)", result.stdout)


def check_course_registration(
    course_names: Sequence[str],
    registered: Sequence[str],
    platform_name: str,
) -> None:
    """Every owed selector once, and no RFC-0017 name unaccounted for.

    Three refusals, because three different mistakes hide behind a passing
    count. A selector this graph owes and did not register means the audit the
    RFC promised never ran; a name registered twice means the count stopped
    describing the run; and a name that looks like this RFC's but is neither
    owed nor written down means either evidence nothing is guarding or a
    registration on a platform the inventory says cannot build it.
    """
    known = set(course_names)
    counts = Counter(registered)
    for name in sorted(known):
        found = counts.get(name, 0)
        require(found != 0, f"course-selector-missing:{platform_name}:{name}")
        require(
            found == 1,
            f"course-selector-duplicate:{platform_name}:{name}:{found}",
        )
    for name, reason in FAMILY_EXCEPTIONS.items():
        require(
            name not in known,
            f"course-exception-stale:{platform_name}:{name}",
        )
        require(bool(reason.strip()), f"course-exception-unreasoned:{name}")
    family = re.compile(FAMILY_PATTERN)
    for name in sorted(set(registered)):
        if not family.search(name):
            continue
        require(
            name in known or name in FAMILY_EXCEPTIONS,
            f"course-selector-extra:{platform_name}:{name}",
        )


def check_inventory(
    build_dir: pathlib.Path,
    owed: Sequence[str],
    configuration: str | None = None,
) -> None:
    """The inventory runs before any test, never after.

    Every selector this graph owes is named with `--required`, so an anchored
    zero cannot excuse a registration the course is about to run. `--all` was
    the wrong instrument for that: it insists on selectors a supported
    platform legitimately does not build, which would have failed the Windows
    course on the two wire suites BPR-0446 deliberately left out.
    """
    command = [
        sys.executable,
        str(REPOSITORY_ROOT / "scripts" / "verify_rfc0017_test_inventory.py"),
        "--build-dir",
        str(build_dir),
    ]
    for selector in owed:
        command += ["--required", selector]
    # BPR-0462: a multi-config inventory reads an empty manifest without this.
    if configuration is not None:
        command += ["--build-config", configuration]
    result = run(command, cwd=REPOSITORY_ROOT, timeout=900)
    require(
        "RFC0017_TEST_INVENTORY_OK" in result.stdout,
        f"inventory-failed:{(result.stdout + result.stderr).strip()[-400:]}",
    )


def check_course_selectors(
    build_dir: pathlib.Path,
    course_names: Sequence[str],
    configuration: str | None = None,
) -> int:
    """Run exactly the course selectors and require all of them to pass."""
    command = ["ctest", "--test-dir", str(build_dir)]
    if configuration is not None:
        command += ["-C", configuration]
    command += [
        "--no-tests=error",
        "--output-on-failure",
        "-R",
        selector_pattern(course_names),
    ]
    result = run(command, cwd=REPOSITORY_ROOT, timeout=3600)
    require(
        result.returncode == 0,
        f"course-selectors-failed:{result.stdout.strip()[-600:]}",
    )
    summary = re.search(
        r"(\d+)% tests passed(?:, (\d+) tests failed)? out of (\d+)",
        result.stdout,
    )
    require(summary is not None, "course-selector-summary-unreadable")
    require(summary.group(1) == "100", "course-selectors-not-all-passing")
    require(
        summary.group(2) in (None, "0"),
        "course-selectors-reported-failures",
    )
    total = int(summary.group(3))
    # The registration check above already named every selector, so a total
    # that disagrees with it means CTest ran a different set than was audited.
    require(
        total == len(course_names),
        f"course-selector-count-mismatch:{total}:{len(course_names)}",
    )
    return total


def check_release_absence(binary: pathlib.Path) -> None:
    """The demonstration seam did not survive into the shipped artifact."""
    result = run(
        [
            sys.executable,
            str(REPOSITORY_ROOT / "scripts" / "verify_rfc0017_test_inventory.py"),
            "--release-binary",
            str(binary),
        ],
        cwd=REPOSITORY_ROOT,
        timeout=600,
    )
    require(
        "RFC0017_TEST_INVENTORY_OK" in result.stdout,
        f"release-absence-failed:{(result.stdout + result.stderr).strip()[-400:]}",
    )


def check_platform_surfaces(binary: pathlib.Path) -> None:
    """No embedded browser engine, and no client secret, in the artifact.

    RFC-BPR-0016 wrote these prohibitions down for the device flow; they hold
    for the anonymous preview for the same reason and are read from there
    rather than copied, so a design this repository forbids is forbidden once.
    The vault-symbol half of that course is deliberately not reused: this RFC's
    shipped selection touches no vault at all, and the probe below measures
    that directly.
    """
    table = symbol_table(binary)
    payload = binary.read_bytes()
    for symbol in rfc0016.PROHIBITED_SYMBOLS:
        require(symbol not in table, f"prohibited-symbol:{symbol}")
    for text in rfc0016.PROHIBITED_STRINGS:
        require(
            text.encode("ascii") not in payload,
            f"prohibited-string:{text}",
        )


def preview_markers(output: str) -> tuple[dict[str, str], set[str]]:
    """Split the probe's output into its facts and its terminal verdicts.

    The probe prints `NAME:value` facts — composed, stage, and the three
    counts that must be zero — and then exactly one valueless verdict. They
    are separate on purpose: "the authority was composed", "which answer it
    produced", and "nothing was sent" fail independently, and one verdict
    would hide which. Parsing them separately keeps that property here.
    """
    facts: dict[str, str] = {}
    verdicts: set[str] = set()
    for line in output.splitlines():
        line = line.strip()
        if not line.startswith("PUBLIC_PREVIEW_"):
            continue
        if ":" in line:
            name, _, value = line.partition(":")
            facts[name] = value
        else:
            verdicts.add(line)
    return facts, verdicts


def probe(
    binary: pathlib.Path,
    launch: dict[str, str] | None = None,
    ghes_host: str | None = None,
) -> tuple[dict[str, str], set[str]]:
    """Run the shipped artifact's unconditional public-preview probe."""
    environment = dict(os.environ if launch is None else launch)
    environment["QT_QPA_PLATFORM"] = "offscreen"
    command = [
        str(binary),
        "--self-test",
        "--public-preview-probe",
        "--automated-run-budget-ms",
        "20000",
    ]
    if ghes_host is not None:
        command += ["--ghes-host", ghes_host]
    result = run(command, cwd=REPOSITORY_ROOT, timeout=180, environment=environment)
    facts, verdicts = preview_markers(result.stdout)
    require(
        result.returncode == 0,
        "self-test-nonzero" if ghes_host is None else "ghes-self-test-nonzero",
    )
    require(bool(facts), "preview-probe-produced-no-markers")
    return facts, verdicts


def require_zero_effects(facts: dict[str, str], prefix: str) -> None:
    """No request, no Authorization header, no vault lookup. Each named."""
    for marker in ZERO_EFFECT_MARKERS:
        value = facts.get(marker)
        require(value is not None, f"{prefix}marker-absent:{marker}")
        suffix = marker[len("PUBLIC_PREVIEW_"):].lower().replace("_", "-")
        require(value == "0", f"{prefix}{suffix}-not-zero:{value}")


def check_preview_selection(
    binary: pathlib.Path,
    expected: str,
    launch: dict[str, str] | None = None,
) -> dict[str, str]:
    """Run the shipped probe and require the exact stated selection.

    Both branches require the three counts to be zero. That is not an
    oversight in the enabled branch: activating the signed-out mode composes
    an authority and publishes a state, and by itself opens no pull request,
    so an enabled build that sent something during activation alone would be
    reaching the network before a reviewer asked it to.
    """
    facts, verdicts = probe(binary, launch)
    stage = facts.get("PUBLIC_PREVIEW_STAGE")
    composed = facts.get("PUBLIC_PREVIEW_COMPOSED")

    if expected == "unavailable":
        require(
            composed == "false",
            f"preview-composed-not-false:{composed}",
        )
        require(
            stage == "unavailable",
            f"preview-stage-not-unavailable:{stage}",
        )
        require_zero_effects(facts, "preview-")
        require(
            "PUBLIC_PREVIEW_UNAVAILABLE_OK" in verdicts,
            "preview-unavailable-verdict-absent",
        )
    else:
        require(composed == "true", f"preview-composed-not-true:{composed}")
        require(stage == "enabled", f"preview-stage-not-enabled:{stage}")
        require_zero_effects(facts, "preview-")
        require(
            "PUBLIC_PREVIEW_ENABLED_OK" in verdicts,
            "preview-enabled-verdict-absent",
        )
    return facts


def check_ghes_refusal(
    binary: pathlib.Path,
    launch: dict[str, str] | None = None,
) -> dict[str, str]:
    """An enterprise host is refused before anything is sent.

    The refusal is a core policy decision, so it is reached whether or not
    this platform's selection is enabled: an enabled build answers `refused`,
    and a build under the shipped selection never gets that far and answers
    `unavailable`. Either is a refusal; a request is not. The one answer this
    rejects outright is `enabled`, which would mean the enterprise hostname
    changed nothing.
    """
    facts, verdicts = probe(binary, launch, ghes_host=GHES_PROBE_HOST)
    stage = facts.get("PUBLIC_PREVIEW_STAGE")
    require(
        stage in ("refused", "unavailable"),
        f"preview-ghes-stage-unexpected:{stage}",
    )
    require_zero_effects(facts, "preview-ghes-")
    failed = sorted(name for name in verdicts if name.endswith("_FAILED"))
    require(not failed, f"preview-ghes-verdict-failed:{','.join(failed)}")
    return facts


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run the RFC-BPR-0017 anonymous public-preview course for one "
            "platform against isolated Debug and Release trees."
        )
    )
    parser.add_argument("--platform", choices=PLATFORMS, required=True)
    parser.add_argument("--dev-preset", required=True)
    parser.add_argument("--release-preset", required=True)
    parser.add_argument("--build-root", type=pathlib.Path, required=True)
    parser.add_argument(
        "--expected-preview-selection",
        choices=SELECTIONS,
        required=True,
    )
    parser.add_argument(
        "--fresh",
        action="store_true",
        help="Reconfigure and rebuild both trees from clean state.",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    started = time.monotonic()
    try:
        require(
            arguments.platform == host_platform(),
            f"platform-mismatch:course={arguments.platform}:"
            f"host={host_platform()}",
        )

        debug_dir = arguments.build_root / "debug"
        release_dir = arguments.build_root / "release"

        debug_configuration = configure_and_build(
            arguments.dev_preset, debug_dir, arguments.fresh, "Debug"
        )
        # Registration before execution, in that order and never the other:
        # a selector that is missing, doubled, or unknown to the inventory
        # makes every result below it unreadable.
        listed = listed_selectors(debug_dir, None, debug_configuration)
        course_names = course_selector_names(listed)
        check_inventory(debug_dir, owed_selectors(listed), debug_configuration)
        check_course_registration(course_names, listed, arguments.platform)
        selectors = check_course_selectors(
            debug_dir, course_names, debug_configuration
        )
        launch = launch_environment(
            debug_dir, debug_configuration, arguments.platform
        )

        release_configuration = configure_and_build(
            arguments.release_preset, release_dir, arguments.fresh, "Release"
        )
        shipped = application_binary(
            release_dir, release=True, configuration=release_configuration
        )

        check_platform_surfaces(shipped)
        check_release_absence(shipped)
        check_composed_hosts_of_the_artifact(shipped)
        check_runtime_and_dependencies(shipped, launch)
        facts = check_preview_selection(
            shipped, arguments.expected_preview_selection, launch
        )
        check_ghes_refusal(shipped, launch)
    except CourseFailure as error:
        print(f"RFC0017_PLATFORM_COURSE_FAILED:{error}", file=sys.stderr)
        return 1
    except subprocess.TimeoutExpired as error:
        print(
            f"RFC0017_PLATFORM_COURSE_FAILED:timeout:{error.cmd[0]}",
            file=sys.stderr,
        )
        return 1
    except Exception as error:  # noqa: BLE001 - reported, never swallowed
        print(
            f"RFC0017_PLATFORM_COURSE_FAILED:internal:{type(error).__name__}",
            file=sys.stderr,
        )
        return 1

    elapsed = int(time.monotonic() - started)
    print(f"RFC0017_PLATFORM:{arguments.platform}")
    print(f"RFC0017_SELECTION:{arguments.expected_preview_selection}")
    print(f"RFC0017_COURSE_SELECTORS:{selectors}")
    print(f"RFC0017_PREVIEW_STAGE:{facts.get('PUBLIC_PREVIEW_STAGE', '')}")
    print(f"RFC0017_PREVIEW_REQUESTS:{facts.get('PUBLIC_PREVIEW_REQUESTS', '')}")
    print(f"RFC0017_RELEASE_ARTIFACT:{shipped}")
    print(f"RFC0017_ELAPSED_SECONDS:{elapsed}")
    print("RFC0017_PLATFORM_COURSE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
