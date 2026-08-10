#!/usr/bin/env python3
"""Assert that no live GitHub review mutation is reachable from a test binary.

Today the safety property "no automated test can mutate a non-designated live
repository" holds for a structural reason rather than a careful one:
`application::review_mutation_port` has exactly one implementation in the tree,
a fake inside a test, and no production code constructs a coordinator. There is
no code path that reaches GitHub with a mutation.

BPR-0142 introduces the first live implementation and destroys that argument. A
scan that keeps passing afterwards would be passing for a reason that no longer
exists, which is worse than no scan at all. This file replaces the structural
argument with one that survives the live provider: the live implementation must
be unreachable from every non-production binary, asserted rather than arranged.

Three clauses, because a live mutation can become reachable three ways.

    (A) Inventory     someone writes a second implementation of the port
    (B) Unreachability someone links the live provider into a test binary
    (C) Construction   someone calls the factory somewhere it must not be called

Each is checkable now, before the thing it guards exists, which is the only
time the guard can be shown to work on a tree where the answer is known.

Both directions matter. The allowlists say what may exist, and the positive
controls say what must exist: the fake must be found, and the coordinator must
be present in the binary that tests it. An inventory that finds nothing and a
symbol table that was never read look identical from a passing exit code, and
the positive controls are what tell them apart.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import subprocess
import sys

# The only files that may implement `application::review_mutation_port`. The
# live provider's two files are listed before they exist, on purpose: this is
# the naming contract BPR-0142 must implement against, and it is documented in
# docs/architecture/review-round-trip.md. A live implementation under any other
# name or path fails clause (A) rather than quietly becoming reachable.
PORT_IMPLEMENTATION_ALLOWLIST = frozenset(
    {
        "tests/application/review_mutation_tests.cpp",
        # Test-build-only provider owned by the recorded public-path fixture.
        # It has no transport, endpoint, credential, or production build path.
        "src/app/src/github_public_path_fixture.cpp",
        "src/adapters/github/src/live_review_mutation_provider.hpp",
        "src/adapters/github/src/live_review_mutation_provider.cpp",
    }
)

# Must be found. A tree where the inventory returns nothing is a tree where the
# instrument is broken, and it would satisfy a subset check perfectly.
REQUIRED_PORT_IMPLEMENTATION = "tests/application/review_mutation_tests.cpp"

# The symbol no non-production binary may contain.
FORBIDDEN_SYMBOL = "live_review_mutation_provider"

# Present in the binary that exercises the coordinator. Catches a stripped
# binary, a broken demangler, and a manifest pointing somewhere harmless.
POSITIVE_CONTROL_BINARY = "review_mutation_tests"
POSITIVE_CONTROL_SYMBOL = "review_mutation_coordinator"

# The factory. Callable only from the live implementation itself and from the
# composition root, which is the one place allowed to choose a concrete
# provider.
CONSTRUCTION_TOKEN = "make_live_review_mutation_provider"
CONSTRUCTION_SITE_ALLOWLIST = frozenset(
    {
        "src/adapters/github/src/live_review_mutation_provider.hpp",
        "src/adapters/github/src/live_review_mutation_provider.cpp",
        "src/app/src/github_application.cpp",
    }
)

SOURCE_SUFFIXES = (".cpp", ".hpp", ".h", ".cc", ".cxx")

_PORT_IMPLEMENTATION = re.compile(
    r":\s*public\s+(?:[A-Za-z_][A-Za-z_0-9]*::)*review_mutation_port\b"
)
_CONSTRUCTION_CALL = re.compile(rf"\b{CONSTRUCTION_TOKEN}\s*\(")


def _sources(root: pathlib.Path) -> list[pathlib.Path]:
    found: list[pathlib.Path] = []
    for directory in ("src", "tests"):
        base = root / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                found.append(path)
    return found


def port_implementations(root: pathlib.Path) -> set[str]:
    """Repository-relative paths declaring a `review_mutation_port` subclass."""
    found: set[str] = set()
    for path in _sources(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        if _PORT_IMPLEMENTATION.search(text):
            found.add(path.relative_to(root).as_posix())
    return found


def forbidden_symbols_in(table: str) -> list[str]:
    """Lines of a demangled symbol table naming the live provider."""
    return [line.strip() for line in table.splitlines() if FORBIDDEN_SYMBOL in line]


def construction_sites(root: pathlib.Path) -> set[str]:
    """Repository-relative paths that call the live provider's factory."""
    found: set[str] = set()
    for path in _sources(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        if _CONSTRUCTION_CALL.search(text):
            found.add(path.relative_to(root).as_posix())
    return found


def symbols(binary: str) -> str | None:
    """Demangled symbol table, or None when `nm` cannot read the binary."""
    nm = shutil.which("nm")
    if nm is None:
        return None
    listed = subprocess.run(
        [nm, binary], capture_output=True, text=True, check=False
    )
    if listed.returncode != 0:
        return None
    demangler = shutil.which("c++filt")
    if demangler is None:
        return listed.stdout
    demangled = subprocess.run(
        [demangler],
        input=listed.stdout,
        capture_output=True,
        text=True,
        check=False,
    )
    return demangled.stdout if demangled.returncode == 0 else listed.stdout


def _check_sources(root: pathlib.Path) -> list[str]:
    failures: list[str] = []

    implementations = port_implementations(root)
    for path in sorted(implementations - PORT_IMPLEMENTATION_ALLOWLIST):
        failures.append(
            f"{path} implements review_mutation_port and is not on the "
            f"allowlist. A live implementation must use the documented name "
            f"and path (docs/architecture/review-round-trip.md), or this "
            f"guard cannot tell it from a test double"
        )
    if REQUIRED_PORT_IMPLEMENTATION not in implementations:
        failures.append(
            f"{REQUIRED_PORT_IMPLEMENTATION} no longer implements "
            f"review_mutation_port. The inventory found "
            f"{len(implementations)} implementation(s); if the fake moved, "
            f"this guard was passing without reading anything"
        )

    sites = construction_sites(root)
    for path in sorted(sites - CONSTRUCTION_SITE_ALLOWLIST):
        where = "a test" if path.startswith("tests/") else "production code"
        failures.append(
            f"{path} calls {CONSTRUCTION_TOKEN}. Only the live "
            f"implementation and the composition root may construct it, and "
            f"{where} may not"
        )

    return failures


def _check_binaries(manifest: pathlib.Path) -> tuple[list[str], int, bool]:
    failures: list[str] = []
    binaries = [
        line.strip()
        for line in manifest.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    if not binaries:
        return (
            [f"{manifest} lists no executables; the build wrote an empty "
             f"manifest and every binary check below would vacuously pass"],
            0,
            False,
        )

    control_seen = False
    inspected = 0
    for binary in binaries:
        table = symbols(binary)
        if table is None:
            failures.append(f"nm could not read {binary}")
            continue
        inspected += 1
        for line in forbidden_symbols_in(table):
            failures.append(
                f"{pathlib.Path(binary).name} contains a "
                f"{FORBIDDEN_SYMBOL} symbol: {line}. A non-production binary "
                f"that links the live provider can reach GitHub with a "
                f"mutation"
            )
        if POSITIVE_CONTROL_BINARY in pathlib.Path(binary).name:
            control_seen = True
            if POSITIVE_CONTROL_SYMBOL not in table:
                failures.append(
                    f"{pathlib.Path(binary).name} does not contain "
                    f"{POSITIVE_CONTROL_SYMBOL}. The instrument is not "
                    f"reading symbols, so its silence about "
                    f"{FORBIDDEN_SYMBOL} means nothing"
                )

    if not control_seen:
        failures.append(
            f"no binary matching {POSITIVE_CONTROL_BINARY} is in the "
            f"manifest. Without the positive control this check cannot "
            f"distinguish a clean tree from an unread one"
        )

    return failures, inspected, True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--manifest", required=True)
    arguments = parser.parse_args()

    root = pathlib.Path(arguments.root)
    manifest = pathlib.Path(arguments.manifest)

    # The source half always runs. It needs no toolchain, so there is no
    # honest reason for it to be skipped.
    failures = _check_sources(root)

    inspected = 0
    if shutil.which("nm") is None:
        print(
            "LIVE_MUTATION_ISOLATION_SKIPPED: nm is unavailable, so the "
            "binary half did not run; the source half did",
            flush=True,
        )
    elif not manifest.is_file():
        failures.append(
            f"{manifest} does not exist. The build generates it, so a missing "
            f"manifest means the binary half never ran"
        )
    else:
        binary_failures, inspected, _ = _check_binaries(manifest)
        failures.extend(binary_failures)

    if failures:
        for failure in failures:
            print(f"LIVE_MUTATION_ISOLATION_FAILED: {failure}", flush=True)
        return 1

    print(
        f"LIVE_MUTATION_ISOLATION_OK: {inspected} non-production binaries "
        f"carry no {FORBIDDEN_SYMBOL} symbol, "
        f"{len(PORT_IMPLEMENTATION_ALLOWLIST)} paths may implement the port, "
        f"{len(CONSTRUCTION_SITE_ALLOWLIST)} may construct it",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
