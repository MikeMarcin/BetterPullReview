#!/usr/bin/env python3
"""Verify RFC-BPR-0016's test inventory and its secret-free shipped artifact.

RFC-BPR-0016 replaces a pasted token with a device grant, which means the tree
now contains a client identifier, two service endpoints, a short-lived device
code, an access token, and a refresh token. Every one of those is a thing that
can be shipped by accident, and every claim the RFC makes about them is proved
by a named CTest. This script is the standing guard over both halves: the exact
evidence selectors must be registered exactly once each, and the tests-disabled
Release artifact must carry no secret and no testing seam.

It exists because a matrix that runs is not the same as a matrix that covers.
`ctest -R` over a regular expression passes just as happily when a selector was
never registered as when it passed, so the ledger's command runs this first and
CTest second: the inventory fails loudly on a missing name, and only then does
anything get to report green.

Later RFC-BPR-0016 cards extend the tables below in place rather than adding a
parallel checker, so the inventory stays one list with one count.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import platform
import re
import shutil
import subprocess
import sys


# Every CTest that carries RFC-BPR-0016 evidence, with the exact number of
# registrations expected. An extra registration is as much a defect as a
# missing one: two selectors sharing a name mean this inventory stops
# describing what actually ran.
#
# The names are the ones docs/verification/RFC-BPR-0016.md's final command
# ledger allocates. Changing one here without changing it there is the drift
# this file exists to prevent, so the ledger check below reads that document.
SELECTORS: dict[str, int] = {
    "core_device_authorization": 1,
    "rfc0016_device_capture_security": 1,
    "rfc0016_device_live_runner_security": 1,
    "rfc0016_production_identity_security": 1,
    "rfc0016_production_course_security": 1,
    "application_device_authorization_host": 1,
    "application_device_authorization_refresh": 1,
    "application_github_credential_vault": 1,
    "github_device_authorization_contract": 1,
    "github_device_authorization_refresh_contract": 1,
    "native_https_device_authorization_contract": 1,
    "native_https_device_authorization_fixture": 1,
    "qt_device_authorization_scale_100": 1,
    "qt_device_authorization_scale_125": 1,
    "qt_device_authorization_scale_200": 1,
    "application_device_authorization_public_path": 1,
    "application_device_authorization_unavailable": 1,
    "application_device_authorization_release_contract": 1,
    "application_device_authorization_registered_course": 1,
}

# Counting a name proves only that something claimed it. Each selector below
# also declares an argument its command must carry, so re-pointing a
# registration at a different suite cannot leave this guard reporting health
# while the evidence silently stops running.
REQUIRED_ARGUMENTS: dict[str, tuple[str, ...]] = {
    "github_device_authorization_refresh_contract": ("--suite", "refresh"),
    "application_device_authorization_public_path": (
        "--device-authorization-probe",
    ),
    "application_device_authorization_unavailable": (
        "--device-authorization-probe",
        "--ghes-host",
    ),
    "application_device_authorization_release_contract": (
        "--device-authorization-probe",
    ),
    "application_device_authorization_registered_course": (
        "--device-authorization-registered-course",
        "--github-fixture",
    ),
}

# Not every configured graph builds the Qt and wire halves — the `core-only`
# preset has no Qt and no provider, so it builds the core, application, and
# capture-tooling selectors and nothing else. Rather than let the count pass
# vacuously there, anchor it: a graph registering the sibling fixture must
# register every Qt and wire selector exactly as stated, and a graph that does
# not must register none of them, while the list below is required in both.
# Both directions are checked, so neither a missing selector nor a stray one
# can hide behind "this preset does not build that".
GRAPH_ANCHOR = "native_https_transport_fixture"
GRAPH_INDEPENDENT_SELECTORS = frozenset(
    {
        "core_device_authorization",
        "rfc0016_device_capture_security",
        "rfc0016_device_live_runner_security",
        "rfc0016_production_identity_security",
        "rfc0016_production_course_security",
        "application_device_authorization_host",
        "application_device_authorization_refresh",
        "application_github_credential_vault",
    }
)

# BPR-0446 registered the anchor fixture on Windows without the APPLE/Linux
# wire block that carries these two, so the anchor no longer implies them.
# Their condition is the platform holding that block, not the fixture.
WIRE_BLOCK_SELECTORS = frozenset(
    {
        "github_device_authorization_refresh_contract",
        "native_https_device_authorization_fixture",
    }
)

# Symbols that must not exist in a tests-disabled Release build. These are the
# testing seams; if one appears, a fixture has escaped into the product.
FORBIDDEN_SYMBOLS: tuple[str, ...] = (
    "github_public_path_fixture",
    "make_native_https_credential_free_transport_for_test",
    # Both exist only so the registered course can assert on them. The
    # recorded transport was gated correctly from the start; the enrollment
    # counter was not, and shipped into Release until this scan found it.
    "recorded_device_authorization_transport",
    "counting_device_authorization_enrollment",
)

# Strings that must not appear in a shipped binary. Two kinds: the testing
# seams above, and the device flow's own secret-shaped material.
#
# The service endpoints are deliberately absent from this list. The host owns
# the endpoints — RFC-BPR-0016 says so explicitly — and a shipped binary that
# can run the flow must contain them; forbidding them would be a check that
# fails on correct code.
FORBIDDEN_STRINGS: tuple[str, ...] = (
    "make_native_https_credential_free_transport_for_test",
    "BPR_GITHUB_DEVICE_AUTHORIZATION_FIXTURE_ROOT",
    "BPR_GITHUB_DEVICE_DOCUMENTED_FIXTURE_ROOT",
    # The attended capture's own recorded material. None of it is secret any
    # more — it was sanitized before it was committed — but a shipped binary
    # containing a recorded user code would mean a fixture reached the product.
    "WDJB-MJHT",
)

# Secret-shaped patterns. GitHub's token prefixes are documented and stable, so
# a shipped binary containing one is carrying a credential regardless of how it
# got there. `ghu_`/`ghr_` are the user-to-server and refresh prefixes the
# device grant issues; `ghp_`/`gho_`/`ghs_` cover the older shapes an operator
# might have pasted into a fixture.
# The recorded distributable production client, permitted by exact value and
# nothing wider. BPR-0318 recorded it (record commit
# c82e51c2c933887632fb0bb35db876375a151de9) and the enabled artifact is
# required to carry it; any other identifier-shaped material in a shipped
# binary is still someone's mistake. The private BPR-0276 verification client
# is separately forbidden by the platform course's no-private-identity check.
SHIPPED_PRODUCTION_CLIENT = "Iv23liitiYN1WSSbag50"

FORBIDDEN_PATTERNS: tuple[tuple[str, str, frozenset[str]], ...] = (
    (r"gh[uprso]_[A-Za-z0-9]{16,}", "github token", frozenset()),
    # A registered OAuth/App client identifier. Until BPR-0318 recorded the
    # distributable identity nothing of this shape belonged in a shipped
    # binary; now exactly the recorded client does, and only it.
    (
        r"\bIv[0-9]{2}[A-Za-z0-9]{16,}\b",
        "device client identifier",
        frozenset({SHIPPED_PRODUCTION_CLIENT}),
    ),
)

# Strings a shipped binary legitimately carries, recorded so a future reader
# neither flags them as a regression nor quotes them as something they are not.
# The routes rather than the whole URLs: the shipped transport builds its URL
# from the compiled profile hostname and a route constant, so the linker keeps
# the path and never a joined literal. Checking for the joined form would be a
# positive control that never fires on a correct artifact, which is worse than
# no positive control at all.
KNOWN_SHIPPED_DEVICE_FACTS = (
    "/login/device/code",
    "/login/oauth/access_token",
)

LEDGER = pathlib.Path("docs/verification/RFC-BPR-0016.md")


class InventoryFailure(RuntimeError):
    """A checked property did not hold."""


def registered_tests(
    build_dir: pathlib.Path, build_config: str | None
) -> list[tuple[str, tuple[str, ...]]]:
    """Every configured CTest name and command, from CTest's own inventory."""
    ctest = shutil.which("ctest")
    if ctest is None:
        raise InventoryFailure("ctest-unavailable")
    # A multi-config generator lists nothing without a configuration, which
    # would turn every anchored expectation below into a vacuous pass.
    command = [ctest, "--show-only=json-v1"]
    if build_config:
        command.extend(("-C", build_config))
    listed = subprocess.run(
        command,
        cwd=build_dir,
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
    if not registered:
        # Every configured graph in this repository registers tests, so an
        # empty inventory is a broken invocation, never a graph fact.
        raise InventoryFailure("ctest-inventory-empty")
    return registered


def check_inventory(
    build_dir: pathlib.Path, build_config: str | None
) -> list[str]:
    """Require every selector exactly its expected number of times."""
    registered = registered_tests(build_dir, build_config)
    names = [name for name, _ in registered]
    anchored = GRAPH_ANCHOR in names
    wire_block = sys.platform in {"darwin", "linux"}
    failures: list[str] = []
    for selector, declared in sorted(SELECTORS.items()):
        if selector in WIRE_BLOCK_SELECTORS:
            expected = declared if anchored and wire_block else 0
        elif anchored or selector in GRAPH_INDEPENDENT_SELECTORS:
            expected = declared
        else:
            expected = 0
        matching = [
            arguments for name, arguments in registered if name == selector
        ]
        if len(matching) != expected:
            if selector in WIRE_BLOCK_SELECTORS:
                reason = (
                    "the APPLE/Linux wire block is configured"
                    if expected
                    else "this platform has no APPLE/Linux wire block"
                )
            elif anchored:
                reason = "graph builds the Qt and loopback halves"
            else:
                reason = "graph builds neither Qt nor the loopback fixture"
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


LEDGER_SELECTOR_RE = re.compile(r"-R\s+'([^']+)'")


def check_ledger(repository_root: pathlib.Path) -> list[str]:
    """Require the ledger to still name every selector this file counts.

    The ledger is the RFC's authority over what will be replayed at closeout,
    and this table is what makes a missing registration fail early. They have
    to agree, and the cheapest way to keep them agreeing is to say so here.

    The ledger writes its selector sets as CTest alternations — the exact
    `-R` argument a replay will use — so this matches against those patterns
    rather than searching for literal names. Grepping for a literal would
    report a missing selector for every one the ledger collapsed, which would
    be a checker that fails on correct input.
    """
    ledger = repository_root / LEDGER
    if not ledger.is_file():
        raise InventoryFailure("ledger-missing")
    text = ledger.read_text(encoding="utf-8")
    patterns = []
    for raw in LEDGER_SELECTOR_RE.findall(text):
        try:
            patterns.append(re.compile(raw))
        except re.error:
            # A `-R` argument the ledger records that Python cannot compile is
            # a defect in the ledger, not something to skip past.
            return [f"ledger records an uncompilable selector pattern: {raw}"]
    if not patterns:
        return ["ledger records no CTest selector pattern at all"]

    failures = []
    for selector in sorted(SELECTORS):
        named = selector in text or any(
            pattern.search(selector) for pattern in patterns
        )
        if not named:
            failures.append(f"ledger does not name selector: {selector}")
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
            ("nm", ["-a", str(binary)]),
            ("llvm-nm", ["-a", str(binary)]),
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


def check_release_artifact(binary: pathlib.Path) -> list[str]:
    """Require the shipped artifact to carry no seam and no secret."""
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

    # Decoded permissively: a token that survives into a binary may sit beside
    # arbitrary bytes, and refusing to look at those bytes is how a scan
    # reports clean on an artifact it never read.
    decoded = payload.decode("latin-1")
    for pattern, description, permitted in FORBIDDEN_PATTERNS:
        for found in re.finditer(pattern, decoded):
            if found.group(0) in permitted:
                continue
            # The value itself is never printed. Reporting a secret to prove a
            # secret leaked would put it in the log, the CI output, and the
            # evidence file.
            failures.append(
                f"{description}-shaped material present in release binary "
                f"at offset {found.start()}"
            )
            break

    # A positive control. If the artifact contains none of what it is supposed
    # to contain, the scan above proved nothing: an empty or wrong file passes
    # every absence check trivially.
    if not any(
        fact.encode("ascii") in payload for fact in KNOWN_SHIPPED_DEVICE_FACTS
    ):
        failures.append(
            "release binary contains no device-flow endpoint, so the "
            "absence checks above ran against the wrong artifact"
        )

    return failures


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Verify the RFC-BPR-0016 evidence selectors are registered "
            "exactly once and that a tests-disabled Release build carries no "
            "testing seam and no secret."
        )
    )
    parser.add_argument(
        "--build-dir",
        type=pathlib.Path,
        help="Configured tests-enabled build directory to inventory.",
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
        help="Tests-disabled Release executable to scan.",
    )
    parser.add_argument(
        "--repository-root",
        type=pathlib.Path,
        default=pathlib.Path("."),
        help="Repository root, for the ledger agreement check.",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    if arguments.build_dir is None and arguments.release_binary is None:
        print("rfc0016-test-inventory:fail:no-subject", file=sys.stderr)
        return 1

    failures: list[str] = []
    try:
        if arguments.build_dir is not None:
            failures.extend(
                check_inventory(arguments.build_dir, arguments.build_config)
            )
            failures.extend(check_ledger(arguments.repository_root))
        if arguments.release_binary is not None:
            failures.extend(check_release_artifact(arguments.release_binary))
    except InventoryFailure as error:
        print(f"rfc0016-test-inventory:fail:{error}", file=sys.stderr)
        return 1
    except Exception:
        print("rfc0016-test-inventory:fail:internal", file=sys.stderr)
        return 1

    if failures:
        for failure in failures:
            print(f"rfc0016-test-inventory:fail:{failure}", file=sys.stderr)
        return 1

    print("RFC0016_TEST_INVENTORY_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
