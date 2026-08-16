#!/usr/bin/env python3
"""Verify RFC-BPR-0017's test inventory and its absence from a shipped build.

RFC-BPR-0017 asks whether BetterPullReview can read a public pull request
without the operator authenticating. Answering that needs a credential-free
request on the wire, and the moment such a thing exists in the tree it becomes
possible to ship one by accident. This script is the standing guard against
that: every RFC-BPR-0017 evidence selector must be registered exactly once and
must not survive into a tests-disabled Release build.

Later RFC-BPR-0017 cards extend ``SELECTORS`` and ``FORBIDDEN`` in the same
place rather than adding a parallel checker, so the inventory stays one list
with one count.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import platform
import shutil
import subprocess
import sys


# Every CTest that carries RFC-BPR-0017 evidence, with the exact number of
# registrations expected. An extra registration is as much a defect as a
# missing one: two selectors with the same name mean the inventory below stops
# describing what actually ran.
SELECTORS: dict[str, int] = {
    "rfc0017_credential_free_native_https": 1,
}

# Counting a name proves only that something claimed it. Each selector also
# declares an argument its command must carry, so re-pointing the registration
# at a different suite cannot leave this guard reporting health while the
# demonstration silently stops running.
REQUIRED_ARGUMENTS: dict[str, tuple[str, ...]] = {
    "rfc0017_credential_free_native_https": ("--suite", "rfc0017"),
}

# Not every configured graph builds the native HTTPS loopback fixture — the
# core-only preset has no Qt and therefore no transport to demonstrate. Rather
# than let the count check pass vacuously there, anchor it: a graph that
# registers the sibling fixture must register every selector exactly as stated
# above, and a graph that does not must register none of them. Both directions
# are checked, so neither a missing selector nor a stray one can hide.
GRAPH_ANCHOR = "native_https_transport_fixture"

# Symbols that must not exist in a tests-disabled Release build. These are the
# credential-free entry points; if one appears, the demonstration has escaped
# into the product.
FORBIDDEN_SYMBOLS: tuple[str, ...] = (
    "make_native_https_credential_free_transport_for_test",
    "credential_free_request",
)

# Strings that must not appear in a shipped binary for the same reason. Only
# strings that a compiled artifact could actually carry belong here; a CTest
# selector name lives in CTestTestfile.cmake and never in a binary, so listing
# one would be a check that can never fire in either direction.
FORBIDDEN_STRINGS: tuple[str, ...] = (
    "make_native_https_credential_free_transport_for_test",
    "credential-free-material-present",
)

# `make_designated_review_public_observer` also performs a credential-free GET
# and IS compiled into a shipped build. It predates RFC-BPR-0017, belongs to
# the attended designated-live lane, and is not this RFC's evidence. Recording
# it here keeps a future reader from either flagging it as a regression or
# quoting it as a credential-free product capability this RFC established.
KNOWN_PRODUCT_CREDENTIAL_FREE = ("make_designated_review_public_observer",)


class InventoryFailure(RuntimeError):
    """A checked property did not hold."""


def registered_tests(build_root: pathlib.Path) -> list[tuple[str, tuple[str, ...]]]:
    """Every configured CTest name and command, from CTest's own inventory."""
    ctest = shutil.which("ctest")
    if ctest is None:
        raise InventoryFailure("ctest-unavailable")
    listed = subprocess.run(
        [ctest, "--show-only=json-v1"],
        cwd=build_root,
        capture_output=True,
        text=True,
        check=False,
    )
    if listed.returncode != 0:
        raise InventoryFailure("ctest-inventory-unavailable")
    try:
        document = json.loads(listed.stdout)
    except json.JSONDecodeError as error:
        raise InventoryFailure("ctest-inventory-malformed") from error
    tests = document.get("tests")
    if not isinstance(tests, list):
        raise InventoryFailure("ctest-inventory-shape")
    registered: list[tuple[str, tuple[str, ...]]] = []
    for entry in tests:
        if not isinstance(entry, dict):
            raise InventoryFailure("ctest-inventory-entry")
        name = entry.get("name")
        if not isinstance(name, str):
            raise InventoryFailure("ctest-inventory-name")
        command = entry.get("command")
        if command is None:
            arguments: tuple[str, ...] = ()
        elif isinstance(command, list) and all(
            isinstance(argument, str) for argument in command
        ):
            arguments = tuple(command)
        else:
            raise InventoryFailure("ctest-inventory-command")
        registered.append((name, arguments))
    return registered


def check_inventory(build_root: pathlib.Path) -> list[str]:
    """Require every selector exactly its expected number of times."""
    registered = registered_tests(build_root)
    names = [name for name, _ in registered]
    anchored = GRAPH_ANCHOR in names
    failures: list[str] = []
    for selector, declared in sorted(SELECTORS.items()):
        expected = declared if anchored else 0
        matching = [
            arguments for name, arguments in registered if name == selector
        ]
        if len(matching) != expected:
            reason = (
                "graph builds the native HTTPS fixture"
                if anchored
                else "graph does not build the native HTTPS fixture"
            )
            failures.append(
                f"{selector}: expected {expected} registration(s), "
                f"found {len(matching)} ({reason})"
            )
            continue
        required = REQUIRED_ARGUMENTS.get(selector, ())
        for arguments in matching:
            missing = [
                argument for argument in required if argument not in arguments
            ]
            if missing:
                failures.append(
                    f"{selector}: command omits {' '.join(missing)}"
                )
    return failures


def symbol_command(binary: pathlib.Path) -> list[str] | None:
    """Select one target-native symbol inspector, with no skip path."""
    if platform.system() == "Windows":
        candidates = (
            ("dumpbin", ["/symbols", str(binary)]),
            ("llvm-readobj", ["--symbols", str(binary)]),
            ("objdump", ["-t", "-C", str(binary)]),
        )
    else:
        candidates = (
            ("nm", [str(binary)]),
            ("llvm-nm", [str(binary)]),
            ("objdump", ["-t", "-C", str(binary)]),
        )
    for tool, arguments in candidates:
        resolved = shutil.which(tool)
        if resolved is not None:
            return [resolved, *arguments]
    return None


def symbol_table(binary: pathlib.Path) -> str:
    """Demangled symbol table. No inspector is a failure, never a skip."""
    command = symbol_command(binary)
    if command is None:
        raise InventoryFailure("symbol-inspector-unavailable")
    listed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    if listed.returncode != 0:
        raise InventoryFailure("symbol-table-unreadable")
    demangler = shutil.which("c++filt")
    if demangler is None:
        return listed.stdout
    demangled = subprocess.run(
        [demangler],
        input=listed.stdout,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    return demangled.stdout if demangled.returncode == 0 else listed.stdout


def check_release_absence(binary: pathlib.Path) -> list[str]:
    """Require the demonstration to be absent from a shipped binary."""
    if not binary.is_file():
        raise InventoryFailure("release-binary-missing")
    failures: list[str] = []

    table = symbol_table(binary)
    for symbol in FORBIDDEN_SYMBOLS:
        if symbol in table:
            failures.append(f"symbol present in release binary: {symbol}")

    payload = binary.read_bytes()
    for text in FORBIDDEN_STRINGS:
        if text.encode("ascii") in payload:
            failures.append(f"string present in release binary: {text}")

    return failures


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Verify the RFC-BPR-0017 evidence selectors are registered "
            "exactly once and are absent from a tests-disabled Release build."
        )
    )
    parser.add_argument(
        "--build-root",
        type=pathlib.Path,
        help="Configured tests-enabled build directory to inventory.",
    )
    parser.add_argument(
        "--release-binary",
        type=pathlib.Path,
        help="Tests-disabled Release executable to scan for absence.",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    if arguments.build_root is None and arguments.release_binary is None:
        print(
            "rfc0017-test-inventory:fail:no-subject",
            file=sys.stderr,
        )
        return 1

    failures: list[str] = []
    try:
        if arguments.build_root is not None:
            failures.extend(check_inventory(arguments.build_root))
        if arguments.release_binary is not None:
            failures.extend(check_release_absence(arguments.release_binary))
    except InventoryFailure as error:
        print(f"rfc0017-test-inventory:fail:{error}", file=sys.stderr)
        return 1
    except Exception:
        print("rfc0017-test-inventory:fail:internal", file=sys.stderr)
        return 1

    if failures:
        for failure in failures:
            print(f"rfc0017-test-inventory:fail:{failure}", file=sys.stderr)
        return 1

    print("RFC0017_TEST_INVENTORY_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
