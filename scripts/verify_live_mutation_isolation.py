#!/usr/bin/env python3
"""Assert that no live GitHub review mutation is reachable from a test binary.

The production application links and composes the contracted live provider
behind an asynchronous host that has no production execution grant in this
slice. This guard establishes the separate non-production property: the
concrete provider remains unreachable from every test or helper executable.

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
import platform
import re
import shutil
import subprocess
import sys
from collections.abc import Sequence

# The only files that may implement `application::review_mutation_port`. The
# live provider's two files are listed before they exist, on purpose: this is
# the naming contract BPR-0142 must implement against, and it is documented in
# docs/architecture/review-round-trip.md. A live implementation under any other
# name or path fails clause (A) rather than quietly becoming reachable.
PORT_IMPLEMENTATION_ALLOWLIST = frozenset(
    {
        "tests/application/review_mutation_tests.cpp",
		"tests/application/review_mutation_application_host_tests.cpp",
        # [[BPR-0879]]. A double that forwards to review_mutation_session, so the
        # coordinator's statistics can be observed across a real contract session.
        # Deliberately not make_live_review_mutation_provider: that drags
        # FORBIDDEN_SYMBOL into a test binary, and this guard caught exactly that
        # on the first attempt. The symbol check below is unchanged and still
        # covers this binary.
        "tests/github_contract/live_review_mutation_provider_tests.cpp",
        # [[BPR-1746]]. The same forwarding double over the session,
        # in the CLI transport's composed reply course and in the two hand-run
        # live reply courses ([[BPR-1745]]'s and this card's), which are not
        # built by default. None calls make_live_review_mutation_provider, and
        # the symbol check still covers each binary that is built.
        "tests/github_contract/gh_cli_transport_tests.cpp",
        "tests/github_contract/outdated_reply_live_course.cpp",
        "tests/github_contract/cli_reply_live_course.cpp",
        # Test-build-only provider owned by the recorded public-path fixture.
        # Its protocol session uses a credential-free synthetic wire only;
        # it has no live transport or production build path.
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
CONSTRUCTION_SITE_EXPECTED_COUNTS = {
    "src/adapters/github/src/live_review_mutation_provider.hpp": 2,
    "src/adapters/github/src/live_review_mutation_provider.cpp": 1,
    "src/app/src/github_application.cpp": 1,
}

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
    """Lines of a symbol table naming the live provider.

    `dumpbin` prefaces every object's table with `Dump of file <path>`, and a
    test object named for the provider it tests carries the forbidden name in
    that path without defining a single symbol. Only record lines are
    searched; the header that names the file is not a symbol (BPR-0431).
    """
    return [
        line.strip()
        for line in table.splitlines()
        if FORBIDDEN_SYMBOL in line and not line.lstrip().startswith("Dump of file")
    ]


def construction_sites(root: pathlib.Path) -> set[str]:
    """Repository-relative paths that call the live provider's factory."""
    found: set[str] = set()
    for path in _sources(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        if _CONSTRUCTION_CALL.search(text):
            found.add(path.relative_to(root).as_posix())
    return found


def construction_site_counts(root: pathlib.Path) -> dict[str, int]:
    """Count factory-like declarations/definitions/calls at every source site."""
    found: dict[str, int] = {}
    for path in _sources(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        count = len(_CONSTRUCTION_CALL.findall(text))
        if count:
            found[path.relative_to(root).as_posix()] = count
    return found


def inspects_objects() -> bool:
    """True where the linked image carries no symbol table to read."""
    return platform.system() == "Windows"


def symbol_command(binary: str) -> list[str] | None:
    """Select one target-native symbol inspector without a skip path."""
    if inspects_objects():
        # `dumpbin /symbols` against a linked image prints a section summary and
        # no symbols at all, because the MSVC linker consumes the COFF table.
        # The objects are read instead; see object_symbols.
        candidates = (
            ("dumpbin", ["/symbols", binary]),
            ("llvm-readobj", ["--symbols", binary]),
            ("objdump", ["-t", "-C", binary]),
        )
    else:
        candidates = (
            ("nm", [binary]),
            ("llvm-nm", [binary]),
            ("objdump", ["-t", "-C", binary]),
        )
    for tool, arguments in candidates:
        resolved = shutil.which(tool)
        if resolved is not None:
            return [resolved, *arguments]
    return None


def object_manifest_entries(manifest: pathlib.Path) -> dict[str, list[str]]:
    """Binary path -> the object files that composed it, as the build recorded."""
    entries: dict[str, list[str]] = {}
    for line in manifest.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        binary, _, joined = line.partition("|")
        objects = [item for item in joined.split("|") if item]
        entries[binary] = objects
    return entries


# The null resource entry that begins every compiled resource script (`rc.exe`'s
# `.res`): no data, a 32-byte header, ordinal type 0 and ordinal name 0. A COFF
# object begins with its machine type, and a /bigobj one with 00 00 FF FF, so
# neither can be mistaken for it.
COMPILED_RESOURCE_HEADER = b"\x00\x00\x00\x00\x20\x00\x00\x00\xff\xff\x00\x00\xff\xff\x00\x00"


def is_compiled_resource(item: str) -> bool:
    """True for a `.res` that really is a compiled resource script.

    It holds resources and no code, so it has no symbol table for dumpbin to
    read and nothing a live provider could be linked through. The shipped
    helpers carry one since BPR-0974 (their version resource). It is recognised
    by its content, never by its name alone: anything else named `.res` is
    read like every other object.
    """
    path = pathlib.Path(item)
    if path.suffix.lower() != ".res":
        return False
    try:
        with path.open("rb") as stream:
            return stream.read(len(COMPILED_RESOURCE_HEADER)) == COMPILED_RESOURCE_HEADER
    except OSError:
        return False


def object_symbols(objects: Sequence[str]) -> str | None:
    """Concatenated symbol tables of a binary's objects, or None if unreadable.

    Every object must be readable. A partial table would let the forbidden-symbol
    scan stay silent about the one translation unit it could not open, which is
    exactly the failure the positive control exists to catch. Only a compiled
    resource script, which has no symbols, is passed over.
    """
    if not objects:
        return None
    inspector = shutil.which("dumpbin")
    if inspector is None:
        return None
    tables: list[str] = []
    for item in objects:
        if is_compiled_resource(item):
            continue
        listed = subprocess.run(
            [inspector, "/symbols", item],
            capture_output=True,
            text=True,
            check=False,
        )
        if listed.returncode != 0:
            return None
        tables.append(listed.stdout)
    return "".join(tables)


def symbols(binary: str) -> str | None:
    """Native symbol table, or None when no inspector can read it.

    Both checked identifiers appear literally in Itanium and MSVC decorated
    names. Demangling every unrelated template symbol adds no coverage and
    expands the largest application tables beyond 150 MiB per executable.
    """
    command = symbol_command(binary)
    if command is None:
        return None
    listed = subprocess.run(
        command, capture_output=True, text=True, check=False
    )
    if listed.returncode != 0:
        return None
    return listed.stdout


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

    counts = construction_site_counts(root)
    sites = set(counts)
    for path in sorted(sites - CONSTRUCTION_SITE_ALLOWLIST):
        where = "a test" if path.startswith("tests/") else "production code"
        failures.append(
            f"{path} calls {CONSTRUCTION_TOKEN}. Only the live "
            f"implementation and the composition root may construct it, and "
            f"{where} may not"
        )
    for path, expected in CONSTRUCTION_SITE_EXPECTED_COUNTS.items():
        actual = counts.get(path, 0)
        if actual != expected:
            failures.append(
                f"{path} contains {actual} {CONSTRUCTION_TOKEN} token(s); "
                f"expected exactly {expected}. A second factory call inside "
                f"an allowed file is still a second live mutation path"
            )

    return failures


def _check_binaries(
    manifest: pathlib.Path,
    object_manifest: pathlib.Path | None = None,
) -> tuple[list[str], int, bool]:
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

    objects_by_binary: dict[str, list[str]] = {}
    if inspects_objects():
        if object_manifest is None or not object_manifest.is_file():
            return (
                [f"this platform has no readable symbol table in a linked "
                 f"image, so the objects must be inspected instead, and "
                 f"{object_manifest} is missing. The build generates it, so a "
                 f"missing object manifest means nothing was inspected"],
                0,
                False,
            )
        objects_by_binary = object_manifest_entries(object_manifest)

    control_seen = False
    inspected = 0
    for binary in binaries:
        if inspects_objects():
            objects = objects_by_binary.get(binary)
            if not objects:
                failures.append(
                    f"{object_manifest} records no objects for {binary}, so its "
                    f"symbols were never read and its silence about "
                    f"{FORBIDDEN_SYMBOL} means nothing"
                )
                continue
            table = object_symbols(objects)
            if table is None:
                failures.append(
                    f"no inspector could read the {len(objects)} objects that "
                    f"composed {pathlib.Path(binary).name}. `dumpbin` is only "
                    f"on PATH inside a Visual Studio developer environment"
                )
                continue
        else:
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
    # Required only where the linked image carries no symbol table. The build
    # writes it on every platform so the two halves cannot drift apart.
    parser.add_argument("--object-manifest")
    arguments = parser.parse_args()

    root = pathlib.Path(arguments.root)
    manifest = pathlib.Path(arguments.manifest)

    # The source half always runs. It needs no toolchain, so there is no
    # honest reason for it to be skipped.
    failures = _check_sources(root)

    inspected = 0
    if not manifest.is_file():
        failures.append(
            f"{manifest} does not exist. The build generates it, so a missing "
            f"manifest means the binary half never ran"
        )
    else:
        object_manifest = (
            pathlib.Path(arguments.object_manifest)
            if arguments.object_manifest
            else None
        )
        binary_failures, inspected, _ = _check_binaries(manifest, object_manifest)
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
