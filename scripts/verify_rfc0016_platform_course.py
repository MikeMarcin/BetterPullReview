#!/usr/bin/env python3
"""Run RFC-BPR-0016's target-native course on one platform, from clean state.

The RFC's claims are about a shipped application on a real operating system:
that the device grant reaches the platform credential store and nothing else,
that the browser handoff is the system browser and never an embedded one, that
an unregistered build refuses before it builds a request, and that the shipped
artifact carries no secret. None of those can be proved by a build tree that
has been incrementally patched for a week, and none of them can be proved on a
different operating system.

So this script builds its own trees. `--fresh` reconfigures Debug and Release
into an isolated `--build-root` that belongs to nobody else, runs the selector
inventory before any test so a missing registration fails loudly instead of
passing vacuously, runs the exact platform checks for the host it is on, and
scans the resulting Release artifact.

It takes `--expected-device-selection` rather than discovering it. A course
that asked the binary what it selects and then asserted that it selected that
would pass on every build; the caller states what this platform is supposed to
answer, and the run fails if the artifact disagrees. That is what makes the
same script usable before and after a one-time enablement flip.

Owned by BPR-0282 (the macOS unavailable course), BPR-0310 and BPR-0311 (the
Linux and Windows one-time enablement runs), BPR-0318 (the macOS enablement),
and BPR-0285 (the idempotent final-head replay).
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import time


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent

PLATFORMS = ("macos", "linux", "windows")
SELECTIONS = ("unavailable", "enabled")

# The focused selector set the RFC-BPR-0016 ledger replays. Written as one
# anchored alternation so a name that is not registered cannot be quietly
# skipped by a looser pattern.
COURSE_SELECTOR_PATTERN = (
    "^("
    "core_device_authorization"
    "|application_device_authorization_(host|refresh|public_path|unavailable"
    "|release_contract|registered_course)"
    "|application_github_credential_vault"
    "|github_device_authorization_(contract|refresh_contract)"
    "|native_https_device_authorization_(contract|fixture)"
    "|qt_device_authorization_scale_(100|125|200)"
    "|rfc0016_test_inventory"
    ")$"
)

# The platform credential store each host must select, and the symbol that
# proves the shipped artifact actually contains it. A build that linked the
# wrong one would still pass a selection check that only read a boolean.
PLATFORM_VAULT_SYMBOL = {
    "macos": "macos_keychain",
    "linux": "linux_secret_service",
    "windows": "windows_credential_manager",
}

# Designs the RFC prohibits outright. Each is a symbol or string whose presence
# in a shipped artifact would mean the prohibited thing exists, whatever the
# code around it claims.
PROHIBITED_SYMBOLS = (
    "QWebEngine",
    "QWebView",
    "QtWebEngine",
)
PROHIBITED_STRINGS = (
    "client_secret",
    "QtWebEngineCore",
)


class CourseFailure(RuntimeError):
    """A checked property did not hold."""


def run(
    command: list[str],
    *,
    cwd: pathlib.Path | None = None,
    timeout: int = 3600,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=str(cwd) if cwd is not None else None,
        capture_output=True,
        text=True,
        errors="replace",
        timeout=timeout,
        env=environment,
        check=False,
    )


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise CourseFailure(reason)


def host_platform() -> str:
    system = platform.system()
    if system == "Darwin":
        return "macos"
    if system == "Linux":
        return "linux"
    if system == "Windows":
        return "windows"
    raise CourseFailure(f"unsupported-host:{system}")


def tree_configuration(build_dir: pathlib.Path, intended: str) -> str | None:
    """The configuration to name for this tree, or None when it is single-config.

    A multi-config generator — Visual Studio, Xcode — defers the choice to build
    time and writes artifacts under a per-configuration directory, so the build,
    the test run, the inventory, and the artifact probe must all name it or they
    will not agree. A single-config tree fixed the choice at configure time, and
    naming it again would be wrong rather than merely redundant.

    The single-config branch checks rather than assumes. A Release course that
    silently scanned a Debug tree would pass every assertion below it while
    proving nothing about the shipped artifact, which is the quiet failure this
    course exists to prevent.
    """
    cache = build_dir / "CMakeCache.txt"
    require(cache.is_file(), f"cmake-cache-missing:{build_dir}")

    types = ""
    build_type = ""
    for line in cache.read_text(errors="replace").splitlines():
        if line.startswith("CMAKE_CONFIGURATION_TYPES:"):
            types = line.partition("=")[2].strip()
        elif line.startswith("CMAKE_BUILD_TYPE:"):
            build_type = line.partition("=")[2].strip()

    if types:
        available = [entry for entry in types.split(";") if entry]
        require(
            intended in available,
            f"configuration-unavailable:{intended}:{types}",
        )
        return intended

    require(
        build_type == intended,
        f"configuration-mismatch:tree={build_type or 'none'}:course={intended}",
    )
    return None


def application_binary(
    build_dir: pathlib.Path,
    release: bool,
    configuration: str | None = None,
) -> pathlib.Path:
    """The shipped executable inside a configured build tree."""
    leaves = (
        pathlib.Path("BetterPullReview.app") / "Contents" / "MacOS"
        / "BetterPullReview",
        pathlib.Path("better_pull_review.app") / "Contents" / "MacOS"
        / "better_pull_review",
        pathlib.Path("BetterPullReview.exe"),
        pathlib.Path("better_pull_review.exe"),
        pathlib.Path("BetterPullReview"),
        pathlib.Path("better_pull_review"),
    )
    # A multi-config tree keeps the configuration directory first: the flat
    # path can also exist there as a stale leftover, and picking it up would
    # scan the wrong bytes.
    roots = [build_dir / "src" / "app"]
    if configuration is not None:
        roots.insert(0, build_dir / "src" / "app" / configuration)
    for root in roots:
        for leaf in leaves:
            candidate = root / leaf
            if candidate.is_file():
                return candidate
    raise CourseFailure(
        f"application-binary-missing:{'release' if release else 'debug'}"
    )


def configure_and_build(
    preset: str,
    build_dir: pathlib.Path,
    fresh: bool,
    configuration: str,
) -> str | None:
    configure = ["cmake", "--preset", preset, "-B", str(build_dir)]
    if fresh:
        configure.append("--fresh")
    result = run(configure, cwd=REPOSITORY_ROOT)
    # The tail matters as much here as on the ctest and inventory paths. A bare
    # `configure-failed:<preset>` cost a hand reproduction on Windows to learn
    # that a dependency's generated tool had been blocked by Device Guard, which
    # the captured output said outright.
    require(
        result.returncode == 0,
        f"configure-failed:{preset}:"
        f"{(result.stdout + result.stderr).strip()[-600:]}",
    )

    resolved = tree_configuration(build_dir, configuration)

    build = ["cmake", "--build", str(build_dir)]
    if resolved is not None:
        build += ["--config", resolved]
    if fresh:
        build.append("--clean-first")
    result = run(build, cwd=REPOSITORY_ROOT)
    require(
        result.returncode == 0,
        f"build-failed:{preset}:"
        f"{(result.stdout + result.stderr).strip()[-600:]}",
    )
    return resolved


def check_inventory(
    build_dir: pathlib.Path,
    configuration: str | None = None,
) -> None:
    """The inventory runs before any test, never after."""
    command = [
        sys.executable,
        str(REPOSITORY_ROOT / "scripts" / "verify_rfc0016_test_inventory.py"),
        "--build-dir",
        str(build_dir),
        "--repository-root",
        str(REPOSITORY_ROOT),
    ]
    # BPR-0462: a multi-config inventory reads an empty manifest without this,
    # so omitting it would let the selector check pass having inventoried
    # nothing at all.
    if configuration is not None:
        command += ["--build-config", configuration]
    result = run(command, cwd=REPOSITORY_ROOT)
    require(
        "RFC0016_TEST_INVENTORY_OK" in result.stdout,
        f"inventory-failed:{result.stderr.strip()[:400]}",
    )


def check_course_selectors(
    build_dir: pathlib.Path,
    configuration: str | None = None,
) -> int:
    """Run the focused matrix, requiring at least one test to match."""
    command = ["ctest", "--test-dir", str(build_dir)]
    if configuration is not None:
        command += ["-C", configuration]
    command += [
        "--no-tests=error",
        "--output-on-failure",
        "-R",
        COURSE_SELECTOR_PATTERN,
    ]
    result = run(command, cwd=REPOSITORY_ROOT, timeout=3600)
    require(
        result.returncode == 0,
        f"course-selectors-failed:{result.stdout.strip()[-600:]}",
    )
    # CTest drops the failure clause when nothing failed: a clean run reads
    # "100% tests passed out of 15" and a dirty one reads "93% tests passed,
    # 1 tests failed out of 15". Both halves matter — the percentage is the
    # verdict and the total is what makes "the matrix ran" a number rather
    # than an impression — so the clause is optional and the count is not.
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
    require(total > 0, "course-selectors-matched-nothing")
    return total


def symbol_table(binary: pathlib.Path) -> str:
    if platform.system() == "Windows":
        tools = (("dumpbin", ["/symbols"]), ("llvm-readobj", ["--symbols"]))
    else:
        tools = (("nm", ["-a"]), ("llvm-nm", ["-a"]), ("objdump", ["-t"]))
    for tool, arguments in tools:
        resolved = shutil.which(tool)
        if resolved is None:
            continue
        result = run([resolved, *arguments, str(binary)])
        if result.returncode == 0:
            return result.stdout
    raise CourseFailure("symbol-inspector-unavailable")


def check_platform_surfaces(
    binary: pathlib.Path,
    course_platform: str,
) -> None:
    """The shipped artifact links this platform's vault and no browser engine."""
    table = symbol_table(binary)
    payload = binary.read_bytes()

    vault = PLATFORM_VAULT_SYMBOL[course_platform]
    require(vault in table, f"platform-vault-absent:{vault}")

    for other, symbol in PLATFORM_VAULT_SYMBOL.items():
        if other == course_platform:
            continue
        require(
            symbol not in table,
            f"foreign-platform-vault-present:{symbol}",
        )

    for symbol in PROHIBITED_SYMBOLS:
        require(symbol not in table, f"prohibited-symbol:{symbol}")
    for text in PROHIBITED_STRINGS:
        require(
            text.encode("ascii") not in payload,
            f"prohibited-string:{text}",
        )


def check_release_artifact(binary: pathlib.Path) -> None:
    result = run(
        [
            sys.executable,
            str(REPOSITORY_ROOT / "scripts" / "verify_rfc0016_test_inventory.py"),
            "--release-binary",
            str(binary),
        ],
        cwd=REPOSITORY_ROOT,
    )
    require(
        "RFC0016_TEST_INVENTORY_OK" in result.stdout,
        f"release-artifact-failed:{result.stderr.strip()[:400]}",
    )


def check_runtime_and_dependencies(binary: pathlib.Path) -> None:
    for script in (
        "verify_binary_dependencies.py",
        "verify_runtime_surface.py",
    ):
        result = run(
            [sys.executable, str(REPOSITORY_ROOT / "scripts" / script), str(binary)],
            cwd=REPOSITORY_ROOT,
            timeout=300,
        )
        require(
            result.returncode == 0,
            f"{script}:{(result.stdout + result.stderr).strip()[-400:]}",
        )


def check_device_selection(
    binary: pathlib.Path,
    expected: str,
) -> dict[str, str]:
    """Run the shipped self-test and require the exact stated selection."""
    environment = dict(os.environ)
    environment["QT_QPA_PLATFORM"] = "offscreen"
    result = run(
        [
            str(binary),
            "--self-test",
            "--device-authorization-probe",
            "--automated-run-budget-ms",
            "20000",
        ],
        cwd=REPOSITORY_ROOT,
        timeout=180,
        environment=environment,
    )
    markers: dict[str, str] = {}
    for line in result.stdout.splitlines():
        if line.startswith("DEVICE_FLOW_"):
            name, _, value = line.partition(":")
            markers[name] = value

    require(result.returncode == 0, "self-test-nonzero")
    require(
        markers.get("DEVICE_FLOW_HOST_COMPOSED") == "true",
        "device-host-not-composed",
    )
    require(
        markers.get("DEVICE_FLOW_PRESENTED") == "true",
        "device-choice-not-presented",
    )

    if expected == "unavailable":
        require(
            markers.get("DEVICE_FLOW_STAGE") == "unavailable",
            f"device-selection-not-unavailable:{markers.get('DEVICE_FLOW_STAGE')}",
        )
        require(
            markers.get("DEVICE_FLOW_REQUESTS") == "0",
            f"device-requests-not-zero:{markers.get('DEVICE_FLOW_REQUESTS')}",
        )
        require(
            "DEVICE_FLOW_UNAVAILABLE_OK" in result.stdout,
            "device-unavailable-verdict-absent",
        )
    else:
        # An enabled build must NOT answer `unavailable`. What it does answer
        # depends on whether a reviewer authorizes during the run, so the
        # course requires only that the refusal is gone and the flow started.
        require(
            markers.get("DEVICE_FLOW_STAGE") != "unavailable",
            "device-selection-still-unavailable",
        )
    return markers


def check_no_private_identity(binary: pathlib.Path) -> None:
    """The private owner-only verification client is not distribution authority.

    BPR-0276's registration record names a private, owner-only GitHub App used
    to record contracts. It is not a distributable identity and must never be
    the thing a shipped artifact authenticates as. The record lives in the
    repository, so this reads the identifier from there rather than carrying a
    copy, and requires the artifact not to contain it.
    """
    record = (
        REPOSITORY_ROOT
        / "tests"
        / "fixtures"
        / "github"
        / "device-authorization"
        / "v1"
        / "registration.json"
    )
    if not record.is_file():
        raise CourseFailure("registration-record-missing")
    try:
        document = json.loads(record.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise CourseFailure("registration-record-malformed") from error

    identifiers: list[str] = []

    def collect(value: object) -> None:
        if isinstance(value, dict):
            for key, item in value.items():
                # The record carries both the identifier and its digest. Only
                # the identifier is a thing an artifact could authenticate as.
                if (
                    isinstance(item, str)
                    and "client_id" in key
                    and not key.endswith("_sha256")
                ):
                    identifiers.append(item)
                else:
                    collect(item)
        elif isinstance(value, list):
            for item in value:
                collect(item)

    collect(document)
    require(
        bool(identifiers),
        "registration-record-names-no-client-identifier",
    )
    payload = binary.read_bytes()
    for identifier in identifiers:
        require(
            identifier.encode("ascii") not in payload,
            "private-verification-identity-present-in-artifact",
        )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run the RFC-BPR-0016 target-native course for one platform "
            "against isolated fresh Debug and Release trees."
        )
    )
    parser.add_argument("--platform", choices=PLATFORMS, required=True)
    parser.add_argument("--dev-preset", required=True)
    parser.add_argument("--release-preset", required=True)
    parser.add_argument("--build-root", type=pathlib.Path, required=True)
    parser.add_argument(
        "--expected-device-selection",
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
        check_inventory(debug_dir, debug_configuration)
        selectors = check_course_selectors(debug_dir, debug_configuration)

        release_configuration = configure_and_build(
            arguments.release_preset, release_dir, arguments.fresh, "Release"
        )
        shipped = application_binary(
            release_dir, release=True, configuration=release_configuration
        )

        check_platform_surfaces(shipped, arguments.platform)
        check_release_artifact(shipped)
        check_no_private_identity(shipped)
        check_runtime_and_dependencies(shipped)
        markers = check_device_selection(
            shipped, arguments.expected_device_selection
        )
    except CourseFailure as error:
        print(f"rfc0016-platform-course:fail:{error}", file=sys.stderr)
        return 1
    except subprocess.TimeoutExpired as error:
        print(
            f"rfc0016-platform-course:fail:timeout:{error.cmd[0]}",
            file=sys.stderr,
        )
        return 1
    except Exception as error:  # noqa: BLE001 - reported, never swallowed
        print(
            f"rfc0016-platform-course:fail:internal:{type(error).__name__}",
            file=sys.stderr,
        )
        return 1

    elapsed = int(time.monotonic() - started)
    print(f"RFC0016_PLATFORM:{arguments.platform}")
    print(f"RFC0016_SELECTION:{arguments.expected_device_selection}")
    print(f"RFC0016_COURSE_SELECTORS:{selectors}")
    print(f"RFC0016_DEVICE_STAGE:{markers.get('DEVICE_FLOW_STAGE', '')}")
    print(f"RFC0016_DEVICE_REQUESTS:{markers.get('DEVICE_FLOW_REQUESTS', '')}")
    print(f"RFC0016_RELEASE_ARTIFACT:{shipped}")
    print(f"RFC0016_ELAPSED_SECONDS:{elapsed}")
    print("RFC0016_PLATFORM_COURSE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
