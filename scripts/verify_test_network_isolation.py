#!/usr/bin/env python3
"""Verify that every configured automated test crosses the network boundary."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
from typing import Any


RAW_REGISTRATION = re.compile(r"\badd_test\s*\(", re.IGNORECASE)
ISOLATED_REGISTRATION = re.compile(
    r"\bbpr_add_network_isolated_test\s*\(", re.IGNORECASE
)
LIVE_REGISTRATION = re.compile(
    r"\bbpr_add_designated_live_mutation_test\s*\(", re.IGNORECASE
)
EXACT_LIVE_REGISTRATION = re.compile(
    r"\bbpr_add_designated_live_mutation_test\s*\(\s*\)", re.IGNORECASE
)
RAW_REGISTRATION_OWNER = "cmake/BprTestIsolation.cmake"
LIVE_TEST_NAME = "live_review_mutation_designated_target"
LIVE_TEST_LABEL = "bpr_live_mutation"
EXIT_VERDICT_TESTS = {
    LIVE_TEST_NAME,
    "architecture_boundaries",
    "architecture_boundary_contract",
    "designated_live_mutation_gate",
    "designated_live_mutation_runner_digest_mismatch",
    "test_network_isolation_structure_contract",
    "test_network_isolation_manifest",
    "test_network_isolation_external",
    "test_network_isolation_loopback",
    "test_network_isolation_unrelated",
    "test_network_isolation_child",
    "test_network_isolation_application",
    "live_mutation_isolation",
    "live_mutation_isolation_contract",
}
FORBIDDEN_EXIT_VERDICT_PROPERTIES = {
    "PASS_REGULAR_EXPRESSION",
    "FAIL_REGULAR_EXPRESSION",
    "SKIP_REGULAR_EXPRESSION",
    "WILL_FAIL",
    "SKIP_RETURN_CODE",
    "DISABLED",
}
REQUIRED_COMMAND_BOUND_TESTS = {
    "architecture_boundaries",
    "architecture_boundary_contract",
}
MACOS_LOOPBACK_AUTHORITIES = {
    "test_network_isolation_loopback": 1,
    "test_network_isolation_unrelated": 1,
    "native_https_transport_fixture": 2,
    "native_git_smart_transport_fixture": 1,
}
MACOS_UNRELATED_ENDPOINT_PROBE = "test_network_isolation_unrelated"
PRE_BOUNDARY_INJECTION_VARIABLES = (
    "PYTHONPATH",
    "PYTHONHOME",
    "PYTHONUSERBASE",
    "PYTHONINSPECT",
    "PYTHONSTARTUP",
    "DYLD_INSERT_LIBRARIES",
    "DYLD_LIBRARY_PATH",
    "DYLD_FRAMEWORK_PATH",
    "DYLD_VERSIONED_LIBRARY_PATH",
    "DYLD_VERSIONED_FRAMEWORK_PATH",
    "DYLD_FALLBACK_LIBRARY_PATH",
    "DYLD_FALLBACK_FRAMEWORK_PATH",
    "DYLD_IMAGE_SUFFIX",
    "DYLD_ROOT_PATH",
    "DYLD_SHARED_REGION",
    "LD_PRELOAD",
    "LD_LIBRARY_PATH",
    "LD_AUDIT",
    "LD_DEBUG",
    "LD_PROFILE",
)
LIVE_CREDENTIAL_VARIABLES = (
    "GH_TOKEN",
    "GITHUB_TOKEN",
    "GH_ENTERPRISE_TOKEN",
    "GITHUB_ENTERPRISE_TOKEN",
)


def cmake_sources(root: pathlib.Path) -> list[pathlib.Path]:
    result: list[pathlib.Path] = []
    for path in root.rglob("*"):
        if any(part in {".git", "build", "_build"} for part in path.parts):
            continue
        if path.is_file() and (path.name == "CMakeLists.txt" or path.suffix == ".cmake"):
            result.append(path)
    return sorted(result)


def registration_inventory(root: pathlib.Path) -> tuple[list[str], int, int, int]:
    raw: list[str] = []
    isolated = 0
    live = 0
    exact_live = 0
    for path in cmake_sources(root):
        text = path.read_text(encoding="utf-8", errors="strict")
        if RAW_REGISTRATION.search(text):
            raw.append(path.relative_to(root).as_posix())
        isolated += len(ISOLATED_REGISTRATION.findall(text))
        live += len(LIVE_REGISTRATION.findall(text))
        exact_live += len(EXACT_LIVE_REGISTRATION.findall(text))
    return raw, isolated, live, exact_live


def property_value(test: dict[str, Any], name: str) -> Any | None:
    for candidate in test.get("properties", []):
        if candidate.get("name") == name:
            return candidate.get("value")
    return None


def exit_verdict_failures(test: dict[str, Any], name: str) -> list[str]:
    present = sorted(
        candidate.get("name")
        for candidate in test.get("properties", [])
        if candidate.get("name") in FORBIDDEN_EXIT_VERDICT_PROPERTIES
    )
    return (
        [f"{name} must use process exit as its only verdict; found {present!r}"]
        if present
        else []
    )


def environment_injection_failures(test: dict[str, Any], name: str) -> list[str]:
    failures: list[str] = []
    dangerous = set(PRE_BOUNDARY_INJECTION_VARIABLES)
    environment = property_value(test, "ENVIRONMENT")
    if isinstance(environment, list):
        for entry in environment:
            variable = entry.split("=", 1)[0] if isinstance(entry, str) else ""
            if variable in dangerous:
                failures.append(f"{name} sets pre-boundary variable {variable}")

    modifications = property_value(test, "ENVIRONMENT_MODIFICATION")
    expected = {f"{variable}=unset:" for variable in PRE_BOUNDARY_INJECTION_VARIABLES}
    actual = set(modifications) if isinstance(modifications, list) else set()
    missing = sorted(expected - actual)
    if missing:
        failures.append(f"{name} lacks pre-boundary environment scrubs: {missing!r}")
    for entry in actual:
        variable = entry.split("=", 1)[0]
        if variable in dangerous and entry not in expected:
            failures.append(f"{name} modifies pre-boundary variable: {entry}")
    return failures


def live_credential_failures(test: dict[str, Any], name: str) -> list[str]:
    failures: list[str] = []
    credentials = set(LIVE_CREDENTIAL_VARIABLES)
    environment = property_value(test, "ENVIRONMENT")
    if isinstance(environment, list):
        for entry in environment:
            variable = entry.split("=", 1)[0] if isinstance(entry, str) else ""
            if variable in credentials:
                failures.append(f"{name} sets ambient credential {variable}")

    modifications = property_value(test, "ENVIRONMENT_MODIFICATION")
    expected = {f"{variable}=unset:" for variable in LIVE_CREDENTIAL_VARIABLES}
    actual = set(modifications) if isinstance(modifications, list) else set()
    missing = sorted(expected - actual)
    if missing:
        failures.append(f"{name} lacks ambient credential scrubs: {missing!r}")
    for entry in actual:
        variable = entry.split("=", 1)[0]
        if variable in credentials and entry not in expected:
            failures.append(f"{name} modifies ambient credential: {entry}")
    return failures


def expected_prefix(
    backend: str,
    profile: pathlib.Path | None,
    linux_launcher: pathlib.Path | None,
    cmake: pathlib.Path,
    root: pathlib.Path,
    python: pathlib.Path | None,
    macos_runner: pathlib.Path | None,
) -> list[str]:
    if backend == "macos-sandbox-exec":
        if profile is None or python is None or macos_runner is None:
            raise ValueError("macOS backend requires profile, Python, and runner")
        return [
            str(python.absolute()),
            "-I",
            "-S",
            str(macos_runner.resolve()),
            "--sandbox-exec",
            "/usr/bin/sandbox-exec",
            "--profile-template",
            str(profile.resolve()),
        ]
    if backend == "linux-network-namespace":
        if linux_launcher is None:
            raise ValueError("Linux backend requires a launcher")
        return [str(linux_launcher.resolve())]
    if backend == "windows-unavailable":
        return [
            str(cmake.resolve()),
            "-P",
            str((root / "cmake/FailClosedAutomatedTestIsolation.cmake").resolve()),
        ]
    raise ValueError(f"unknown backend: {backend}")


def configured_failures(
    manifest: dict[str, Any],
    prefix: list[str],
    macos_runner: bool = False,
    live_command: list[str] | None = None,
    required_commands: dict[str, list[str]] | None = None,
) -> list[str]:
    failures: list[str] = []
    tests = manifest.get("tests")
    if not isinstance(tests, list) or not tests:
        return ["CTest JSON contains no tests"]
    live_named = [test for test in tests if test.get("name") == LIVE_TEST_NAME]
    if live_command is None:
        if live_named:
            failures.append(f"{LIVE_TEST_NAME} is configured while the live lane is disabled")
    elif len(live_named) != 1:
        failures.append(
            f"configured live exception count is {len(live_named)}, expected exactly one"
        )

    if required_commands is not None:
        if set(required_commands) != REQUIRED_COMMAND_BOUND_TESTS:
            failures.append(
                "required command bindings must contain the exact protected pair: "
                f"{sorted(required_commands)!r}"
            )
        for required_name in sorted(REQUIRED_COMMAND_BOUND_TESTS):
            expected_command = required_commands.get(required_name)
            named = [test for test in tests if test.get("name") == required_name]
            if len(named) != 1:
                failures.append(
                    f"protected test {required_name} count is {len(named)}, expected one"
                )
            elif named[0].get("command") != expected_command:
                failures.append(
                    f"protected test {required_name} does not use its exact command: "
                    f"{named[0].get('command')!r}"
                )
            if expected_command is not None:
                renamed = [
                    test.get("name", "<unnamed>")
                    for test in tests
                    if test.get("name") != required_name
                    and test.get("command") == expected_command
                ]
                if renamed:
                    failures.append(
                        f"protected command for {required_name} is registered under "
                        f"other names: {renamed!r}"
                    )

    for test in tests:
        name = test.get("name", "<unnamed>")
        failures.extend(environment_injection_failures(test, name))
        if name in EXIT_VERDICT_TESTS:
            failures.extend(exit_verdict_failures(test, name))
        command = test.get("command")
        labels = property_value(test, "LABELS")
        if name == LIVE_TEST_NAME:
            if live_command is not None and command != live_command:
                failures.append(
                    f"{name} does not use the exact runner/approval command: {command!r}"
                )
            if not isinstance(labels, list) or set(labels) != {LIVE_TEST_LABEL}:
                failures.append(
                    f"{name} labels are {labels!r}, expected only {LIVE_TEST_LABEL}"
                )
            if property_value(test, "TIMEOUT") != 30.0:
                failures.append(f"{name} must retain its exact 30-second timeout")
            failures.extend(live_credential_failures(test, name))
            continue
        if isinstance(labels, list) and LIVE_TEST_LABEL in labels:
            failures.append(
                f"{name} uses {LIVE_TEST_LABEL}; only {LIVE_TEST_NAME} may be live"
            )
        if not isinstance(command, list) or command[: len(prefix)] != prefix:
            failures.append(f"{name} does not start with {prefix!r}: {command!r}")
        elif macos_runner:
            tail = command[len(prefix) :]
            if (
                len(tail) < 4
                or tail[0] != "--loopback-port-count"
                or not tail[1].isdigit()
                or int(tail[1]) > 8
            ):
                failures.append(f"{name} has invalid runner authority: {tail!r}")
            else:
                delimiter = 3 if tail[2:3] == ["--unrelated-endpoint-probe"] else 2
                if tail[delimiter : delimiter + 1] != ["--"] or len(tail) <= delimiter + 1:
                    failures.append(f"{name} has no runner command delimiter: {tail!r}")
        if not isinstance(labels, list) or "bpr_network_isolated" not in labels:
            failures.append(f"{name} lacks the bpr_network_isolated label")
        if macos_runner and isinstance(command, list) and command[: len(prefix)] == prefix:
            tail = command[len(prefix) :]
            if len(tail) >= 2 and tail[0] == "--loopback-port-count" and tail[1].isdigit():
                count = int(tail[1])
                has_fixture_label = isinstance(labels, list) and "bpr_loopback_fixture" in labels
                if (count > 0) != has_fixture_label:
                    failures.append(f"{name} loopback count and label disagree")
                expected_count = MACOS_LOOPBACK_AUTHORITIES.get(name, 0)
                if count != expected_count:
                    failures.append(
                        f"{name} has loopback count {count}, expected {expected_count}"
                    )
                has_unrelated_flag = tail[2:3] == ["--unrelated-endpoint-probe"]
                has_unrelated_label = (
                    isinstance(labels, list)
                    and "bpr_unrelated_endpoint_probe" in labels
                )
                expected_unrelated = name == MACOS_UNRELATED_ENDPOINT_PROBE
                if has_unrelated_flag != expected_unrelated:
                    failures.append(f"{name} unrelated-probe flag is incorrect")
                if has_unrelated_label != expected_unrelated:
                    failures.append(f"{name} unrelated-probe label is incorrect")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=pathlib.Path)
    parser.add_argument("--build-dir", required=True, type=pathlib.Path)
    parser.add_argument("--ctest", required=True, type=pathlib.Path)
    parser.add_argument("--cmake", required=True, type=pathlib.Path)
    parser.add_argument("--backend", required=True)
    parser.add_argument("--profile", type=pathlib.Path)
    parser.add_argument("--python", type=pathlib.Path)
    parser.add_argument("--macos-runner", type=pathlib.Path)
    parser.add_argument("--linux-launcher", type=pathlib.Path)
    parser.add_argument("--live-runner", type=pathlib.Path)
    parser.add_argument("--live-approval", type=pathlib.Path)
    arguments = parser.parse_args()

    raw, isolated, live, exact_live = registration_inventory(arguments.root)
    failures: list[str] = []
    if raw != [RAW_REGISTRATION_OWNER]:
        failures.append(
            f"raw add_test ownership is {raw!r}, expected only {RAW_REGISTRATION_OWNER}"
        )
    if isolated < 1:
        failures.append("no isolated test registrations were found")
    if live != 1 or exact_live != 1:
        failures.append(
            "designated live source registration must be exactly one empty fixed call: "
            f"calls={live}, exact={exact_live}"
        )

    shown = subprocess.run(
        [
            str(arguments.ctest),
            "--test-dir",
            str(arguments.build_dir),
            "--show-only=json-v1",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if shown.returncode != 0:
        failures.append(f"ctest manifest failed: {shown.stderr.strip()}")
        manifest: dict[str, Any] = {"tests": []}
    else:
        try:
            manifest = json.loads(shown.stdout)
        except json.JSONDecodeError as error:
            failures.append(f"ctest manifest is not JSON: {error}")
            manifest = {"tests": []}

    try:
        prefix = expected_prefix(
            arguments.backend,
            arguments.profile,
            arguments.linux_launcher,
            arguments.cmake,
            arguments.root,
            arguments.python,
            arguments.macos_runner,
        )
    except ValueError as error:
        failures.append(str(error))
        prefix = []
    if prefix:
        live_arguments = (
            arguments.live_runner,
            arguments.live_approval,
        )
        if any(value is not None for value in live_arguments) and not all(
            value is not None for value in live_arguments
        ):
            failures.append(
                "live runner and approval arguments must be supplied together"
            )
            live_command = None
        elif all(value is not None for value in live_arguments):
            live_command = [
                str(arguments.live_runner.absolute()),
                "--approval",
                str(arguments.live_approval.absolute()),
            ]
        else:
            live_command = None
        if arguments.python is None:
            failures.append("exact protected command binding requires Python")
            required_commands = None
        else:
            protected_prefix = (
                [*prefix, "--loopback-port-count", "0", "--"]
                if arguments.backend == "macos-sandbox-exec"
                else prefix
            )
            required_commands = {
                "architecture_boundaries": [
                    *protected_prefix,
                    str(arguments.cmake.resolve()),
                    f"-DBPR_ROOT={arguments.root.resolve()}",
                    "-P",
                    str(
                        (
                            arguments.root
                            / "cmake"
                            / "VerifyArchitectureBoundaries.cmake"
                        ).resolve()
                    ),
                ],
                "architecture_boundary_contract": [
                    *protected_prefix,
                    str(arguments.python.absolute()),
                    str(
                        (
                            arguments.root
                            / "tests"
                            / "scripts"
                            / "architecture_boundary_tests.py"
                        ).resolve()
                    ),
                ],
            }
        failures.extend(
            configured_failures(
                manifest,
                prefix,
                macos_runner=arguments.backend == "macos-sandbox-exec",
                live_command=live_command,
                required_commands=required_commands,
            )
        )

    if failures:
        for failure in failures:
            print(f"TEST_NETWORK_ISOLATION_FAILED:{failure}", flush=True)
        return 1
    print(
        "TEST_NETWORK_ISOLATION_OK:"
        f"configured={len(manifest['tests'])}:registrations={isolated}:"
        f"backend={arguments.backend}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
