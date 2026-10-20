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
    "core_anonymous_public_read": 1,
    "native_https_anonymous_read_contract": 1,
    "native_https_anonymous_read_fixture": 1,
    "github_anonymous_read_contract": 1,
    "application_anonymous_read_host": 1,
    "git_anonymous_public_read": 1,
    "sqlite_anonymous_epoch_authority": 1,
    "application_anonymous_diff_authority": 1,
    "qt_anonymous_public_preview_scale_100": 1,
    "qt_anonymous_public_preview_scale_125": 1,
    "qt_anonymous_public_preview_scale_200": 1,
    "qt_public_preview_conversation": 1,
    "application_anonymous_public_preview_unavailable": 1,
    "application_anonymous_public_preview_release_contract": 1,
    "application_anonymous_public_preview_ghes_refusal": 1,
    "application_anonymous_public_preview_public_path": 1,
    "application_anonymous_public_preview_enabled_probe": 1,
    "application_anonymous_public_preview_live_candidate_wiring": 1,
}

# Counting a name proves only that something claimed it. Each selector also
# declares an argument its command must carry, so re-pointing the registration
# at a different suite cannot leave this guard reporting health while the
# demonstration silently stops running.
REQUIRED_ARGUMENTS: dict[str, tuple[str, ...]] = {
    "rfc0017_credential_free_native_https": ("--suite", "rfc0017"),
    "qt_public_preview_conversation": ("--conversation-only",),
    "native_https_anonymous_read_contract": ("--contract",),
    "native_https_anonymous_read_fixture": ("--suite", "anonymous_read"),
    "application_anonymous_public_preview_unavailable": ("--public-preview-probe",),
    "application_anonymous_public_preview_release_contract": ("--public-preview-probe",),
    "application_anonymous_public_preview_ghes_refusal": ("--public-preview-ghes-course",),
    "application_anonymous_public_preview_public_path": ("--public-preview-enabled-course",),
    "application_anonymous_public_preview_enabled_probe": ("--public-preview-selection-enabled-probe",),
    "application_anonymous_public_preview_live_candidate_wiring": ("--public-preview-live-candidate-dry-run",),
}

# Not every configured graph builds every surface — the core-only preset has
# no Qt and therefore no native HTTPS loopback fixture to demonstrate against.
# Rather than let the count check pass vacuously there, each selector names
# the sibling registration that proves its graph builds the surface it tests:
# a graph that registers the anchor must register the selector exactly as
# stated above, and a graph that does not must register none of it. Both
# directions are checked, so neither a missing selector nor a stray one can
# hide. A selector anchored on `None` is required of every configured graph;
# the core policy suite links only the core library and has no graph in which
# its absence is legitimate (BPR-0300). The second field says whether the
# selector lives in the APPLE/Linux wire block: BPR-0446 registered the
# loopback fixture on Windows while deliberately leaving this RFC's wire
# suites out of it, so on Windows that anchor no longer implies them, while a
# contract suite registered beside `github_read_contract` is owed by every Qt
# graph on every platform (BPR-0301).
SELECTOR_ANCHORS: dict[str, tuple[str | None, bool]] = {
    "rfc0017_credential_free_native_https": ("native_https_transport_fixture", True),
    "core_anonymous_public_read": (None, False),
    "native_https_anonymous_read_contract": ("github_read_contract", False),
    "native_https_anonymous_read_fixture": ("native_https_transport_fixture", True),
    "github_anonymous_read_contract": ("github_read_contract", False),
    "application_anonymous_read_host": ("application_github_host", False),
    "git_anonymous_public_read": ("git_cache_contract", False),
    "sqlite_anonymous_epoch_authority": ("sqlite_epoch_catalog_storage_contract", False),
    "application_anonymous_diff_authority": ("application_github_host", False),
    "qt_anonymous_public_preview_scale_100": ("qt_composed_github_scale_100", False),
    "qt_anonymous_public_preview_scale_125": ("qt_composed_github_scale_125", False),
    "qt_anonymous_public_preview_scale_200": ("qt_composed_github_scale_200", False),
    "qt_public_preview_conversation": ("qt_composed_github_scale_100", False),
    "application_anonymous_public_preview_unavailable": ("application_device_authorization_unavailable", False),
    "application_anonymous_public_preview_release_contract": ("application_device_authorization_release_contract", False),
    "application_anonymous_public_preview_ghes_refusal": ("application_github_public_path_ghes_3_18", False),
    "application_anonymous_public_preview_public_path": ("application_github_public_path_github_com", False),
    "application_anonymous_public_preview_enabled_probe": ("application_device_authorization_unavailable", False),
    "application_anonymous_public_preview_live_candidate_wiring": ("application_device_authorization_unavailable", False),
}

WIRE_BLOCK_PLATFORMS = frozenset({"darwin", "linux"})

# Symbols that must not exist in a tests-disabled Release build. These are the
# `BPR_NATIVE_HTTPS_TESTING` factories: the only entry points that construct
# a transport under the test policy, and the only way the credential-free
# demonstration can be reached. If one appears, the test seam has escaped into
# the product. `credential_free_request` was listed here until BPR-0451; it is
# the transport's credential-free binding, which RFC-BPR-0016 ships on purpose
# (see KNOWN_PRODUCT_CREDENTIAL_FREE), so its presence no longer distinguishes
# the demonstration from the product.
FORBIDDEN_SYMBOLS: tuple[str, ...] = (
    "make_native_https_credential_free_transport_for_test",
    "make_native_https_bootstrap_transport_for_test",
    "make_native_https_anonymous_read_transport_for_test",
    "make_native_https_anonymous_smart_transport_for_test",
)

# Strings that must not appear in a shipped binary for the same reason. Only
# strings that a compiled artifact could actually carry belong here; a CTest
# selector name lives in CTestTestfile.cmake and never in a binary, so listing
# one would be a check that can never fire in either direction.
FORBIDDEN_STRINGS: tuple[str, ...] = (
    "make_native_https_credential_free_transport_for_test",
    "make_native_https_anonymous_read_transport_for_test",
    "make_native_https_anonymous_smart_transport_for_test",
    "credential-free-material-present",
    # BPR-0515: the tests-enabled flag that routes the device-authorization
    # probe through a closed loopback proxy so the dev-tree gate is denied
    # locally on every platform; a shipped build must not know it.
    "--network-denied",
)

# Credential-free product capabilities that ARE compiled into a shipped build
# and are not this RFC's evidence. Recording them here keeps a future reader
# from either flagging one as a regression or quoting it as a credential-free
# product capability this RFC established.
#
# - `make_designated_review_public_observer` performs a credential-free GET,
#   predates RFC-BPR-0017, and belongs to the attended designated-live lane.
# - `make_native_https_credential_free_transport` is RFC-BPR-0016's device
#   authorization transport: the device-code and token-poll requests run
#   before any credential exists, so the production factory binds
#   `credential_free_request` deliberately (BPR-0279, made truthful in
#   shipped builds by BPR-0318).
# - `make_native_https_anonymous_read_transport` is this RFC's own shipped
#   anonymous-read transport (BPR-0301): a distinct typed binding that admits
#   the eight credential-free read operations and nothing else. It is product
#   code by design, so its presence in a Release image is expected; the seam
#   this script guards is its `_for_test` twin above, which carries the
#   loopback proxy and fixture CA.
KNOWN_PRODUCT_CREDENTIAL_FREE: dict[str, str] = {
    "make_designated_review_public_observer": "designated-live lane (pre-RFC-BPR-0017)",
    "make_native_https_anonymous_read_transport": "RFC-BPR-0017 anonymous public-preview reads (BPR-0301)",
    "make_native_https_anonymous_smart_transport": "RFC-BPR-0017 public smart HTTP for the anonymous Git cache (BPR-0317)",
    "make_native_https_credential_free_transport": "RFC-BPR-0016 device authorization",
}


class InventoryFailure(RuntimeError):
    """A checked property did not hold."""


def registered_tests(
    build_root: pathlib.Path, build_config: str | None
) -> list[tuple[str, tuple[str, ...]]]:
    """Every configured CTest name and command, from CTest's own inventory."""
    ctest = shutil.which("ctest")
    if ctest is None:
        raise InventoryFailure("ctest-unavailable")
    # A multi-config generator lists nothing without a configuration, which
    # would turn the anchored expectation below into a vacuous pass.
    command = [ctest, "--show-only=json-v1"]
    if build_config:
        command.extend(("-C", build_config))
    listed = subprocess.run(
        command,
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
            # A focused build may leave unrelated multi-config executables
            # unbuilt, and CTest then omits their command. Keep their names
            # for anchor/count checks; every RFC selector still owes a real
            # readable command, including inside an AppContainer (BPR-0528).
            if name in SELECTORS:
                raise InventoryFailure(f"ctest-inventory-command-absent:{name}")
            command = []
        if isinstance(command, list) and all(
            isinstance(argument, str) for argument in command
        ):
            arguments = tuple(command)
        else:
            raise InventoryFailure("ctest-inventory-command")
        registered.append((name, arguments))
    if not registered:
        # Every configured graph in this repository registers tests, so an
        # empty inventory is a broken invocation, never a graph fact.
        raise InventoryFailure("ctest-inventory-empty")
    return registered


def selector_expected(
    selector: str, names: list[str]
) -> tuple[int, str]:
    """How many registrations this graph owes the selector, and why."""
    anchor, wire_block = SELECTOR_ANCHORS[selector]
    declared = SELECTORS[selector]
    if anchor is None:
        return declared, "required of every configured graph"
    if anchor not in names:
        return 0, f"graph does not build {anchor}"
    if wire_block and sys.platform not in WIRE_BLOCK_PLATFORMS:
        return 0, f"graph builds {anchor} outside the APPLE/Linux wire block"
    return declared, f"graph builds {anchor}"


def check_inventory(
    build_root: pathlib.Path,
    build_config: str | None,
    required: tuple[str, ...] = (),
    require_all: bool = False,
) -> list[str]:
    """Require every selector exactly its expected number of times.

    ``required`` names selectors the caller insists this graph registers —
    the ledger's ``--required`` form — so a graph that legitimately omits an
    anchored selector still fails when the caller needed it; ``require_all``
    is the same insistence over every selector, the ``--all`` form.
    """
    if set(SELECTORS) != set(SELECTOR_ANCHORS):
        raise InventoryFailure("selector-anchor-table-mismatch")
    unknown = sorted(set(required) - set(SELECTORS))
    if unknown:
        raise InventoryFailure(f"unknown-required-selector:{','.join(unknown)}")
    insisted = set(SELECTORS) if require_all else set(required)
    registered = registered_tests(build_root, build_config)
    names = [name for name, _ in registered]
    failures: list[str] = []
    for selector in sorted(SELECTORS):
        expected, reason = selector_expected(selector, names)
        if selector in insisted and expected == 0:
            failures.append(
                f"{selector}: required but not owed by this graph ({reason})"
            )
            continue
        matching = [
            arguments for name, arguments in registered if name == selector
        ]
        if len(matching) != expected:
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
        "--build-dir",
        dest="build_root",
        type=pathlib.Path,
        help="Configured tests-enabled build directory to inventory.",
    )
    parser.add_argument(
        "--required",
        action="append",
        default=[],
        metavar="SELECTOR",
        help=(
            "A selector this graph must register; repeatable. Fails when the "
            "graph does not owe it rather than accepting the anchored zero."
        ),
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Require every selector of this graph, whatever its anchor.",
    )
    parser.add_argument(
        "--build-config",
        help=(
            "CTest configuration to inventory; required for a multi-config "
            "generator, whose inventory is empty without one."
        ),
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
            failures.extend(
                check_inventory(
                    arguments.build_root,
                    arguments.build_config,
                    tuple(arguments.required),
                    arguments.all,
                )
            )
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
