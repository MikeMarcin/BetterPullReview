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
RAW_REGISTRATION_OWNER = "cmake/BprTestIsolation.cmake"
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


def cmake_sources(root: pathlib.Path) -> list[pathlib.Path]:
    result: list[pathlib.Path] = []
    for path in root.rglob("*"):
        if any(part in {".git", "build", "_build"} for part in path.parts):
            continue
        if path.is_file() and (path.name == "CMakeLists.txt" or path.suffix == ".cmake"):
            result.append(path)
    return sorted(result)


def registration_inventory(root: pathlib.Path) -> tuple[list[str], int]:
    raw: list[str] = []
    isolated = 0
    for path in cmake_sources(root):
        text = path.read_text(encoding="utf-8", errors="strict")
        if RAW_REGISTRATION.search(text):
            raw.append(path.relative_to(root).as_posix())
        isolated += len(ISOLATED_REGISTRATION.findall(text))
    return raw, isolated


def property_value(test: dict[str, Any], name: str) -> Any | None:
    for candidate in test.get("properties", []):
        if candidate.get("name") == name:
            return candidate.get("value")
    return None


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
) -> list[str]:
    failures: list[str] = []
    tests = manifest.get("tests")
    if not isinstance(tests, list) or not tests:
        return ["CTest JSON contains no tests"]
    for test in tests:
        name = test.get("name", "<unnamed>")
        failures.extend(environment_injection_failures(test, name))
        command = test.get("command")
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
        labels = property_value(test, "LABELS")
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
    arguments = parser.parse_args()

    raw, isolated = registration_inventory(arguments.root)
    failures: list[str] = []
    if raw != [RAW_REGISTRATION_OWNER]:
        failures.append(
            f"raw add_test ownership is {raw!r}, expected only {RAW_REGISTRATION_OWNER}"
        )
    if isolated < 1:
        failures.append("no isolated test registrations were found")

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
        failures.extend(
            configured_failures(
                manifest,
                prefix,
                macos_runner=arguments.backend == "macos-sandbox-exec",
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
