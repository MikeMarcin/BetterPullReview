#!/usr/bin/env python3
"""Every contained-temporary call in the tree can actually be made.

Two call sites could not, and both surfaced only when a full matrix ran the branch
containing them:

- `network_isolation_counterexamples.py` passed `dir=`, which is
  `tempfile.mkdtemp`'s keyword, to a function whose keyword is `parent`.
- `dependency_modernization_tests.py` called `tempfile.TemporaryDirectory()` in a
  file that never imported `tempfile`.

One is a `TypeError` and one a `NameError`, both at call time, so nothing short of
executing that exact branch reports them — `compileall` would not. This checks the
shape instead, by parsing the source, which is why it costs a fraction of a second
and runs everywhere.

The `tempfile` half is not style. `scripts/contained_temporary_support.py` exists
because `tempfile`'s owner-only descriptor is unreadable inside a Windows
AppContainer, so a test that reaches for `tempfile` is not merely inconsistent with
its neighbours — it is the form that cannot work where the suite runs.

## What this does not catch

Stated rather than implied, because an earlier version of this file claimed to
close the class and did not. The scan resolves names lexically, so it misses an
aliased import (`import … as ctd`), a `**kwargs` splat, a `functools.partial`
binding, and `tempfile` reached through an alias other than its own name. Those are
recorded as limits, not as coverage.

See `docs/issues/*/BPR-0394-contained-temporary-migration-left-uncallable-sites.md`.
"""

from __future__ import annotations

import ast
import sys
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SUPPORT_MODULE = REPOSITORY_ROOT / "scripts" / "contained_temporary_support.py"

# Wherever the helpers are called, a call that cannot be made is a defect. That is
# every first-party Python root, not just `tests/`: `scripts/` calls them too.
CALL_SCAN_ROOTS = ("tests", "scripts")

# The `tempfile` prohibition is narrower on purpose. `scripts/run_network_isolated_test.py`
# is the launcher that *creates* the AppContainer and runs outside it, and
# `scripts/designated_live_macos_capsule.py` is a tests-disabled macOS packaging
# path, so neither is subject to the constraint. The support module implements it.
TEMPFILE_SCAN_ROOTS = ("tests",)

HELPER_NAMES = (
    "contained_temporary_directory",
    "ContainedTemporaryDirectory",
    "make_contained_temporary_directory",
)

# `tempfile` as a namespace is fine — `gettempdir()` reads a path and creates
# nothing. Only the calls that create a filesystem object carry the descriptor
# problem, and that includes the file-creating ones, whose descriptors are stamped
# the same way.
FORBIDDEN_TEMPFILE_ATTRIBUTES = (
    "TemporaryDirectory",
    "mkdtemp",
    "NamedTemporaryFile",
    "TemporaryFile",
    "mkstemp",
)


def python_sources() -> list[Path]:
    out: list[Path] = []
    for root in CALL_SCAN_ROOTS:
        out.extend(
            path
            for path in (REPOSITORY_ROOT / root).rglob("*.py")
            if "__pycache__" not in path.parts
        )
    return sorted(out)


def tempfile_sources() -> list[Path]:
    roots = {REPOSITORY_ROOT / root for root in TEMPFILE_SCAN_ROOTS}
    return [
        path
        for path in python_sources()
        if any(root in path.parents for root in roots)
    ]


def accepted_parameters() -> dict[str, frozenset[str]]:
    """The parameters each helper accepts, read from the support module itself.

    Read rather than restated so that adding a parameter there cannot leave this
    check quietly stale.
    """
    tree = ast.parse(SUPPORT_MODULE.read_text(encoding="utf-8"))
    accepted: dict[str, frozenset[str]] = {}

    def record(name: str, arguments: ast.arguments, *, drop_self: bool) -> None:
        names = {argument.arg for argument in arguments.kwonlyargs}
        names |= {
            argument.arg
            for argument in arguments.args
            if not (drop_self and argument.arg == "self")
        }
        accepted[name] = frozenset(names)

    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            record(node.name, node.args, drop_self=False)
        elif isinstance(node, ast.ClassDef):
            for member in node.body:
                if isinstance(member, ast.FunctionDef) and member.name == "__init__":
                    record(node.name, member.args, drop_self=True)
    return accepted


def positional_capacity() -> dict[str, int]:
    """How many positional arguments each helper accepts. All are keyword-only."""
    tree = ast.parse(SUPPORT_MODULE.read_text(encoding="utf-8"))
    capacity: dict[str, int] = {}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            capacity[node.name] = len(node.args.posonlyargs) + len(node.args.args)
        elif isinstance(node, ast.ClassDef):
            for member in node.body:
                if isinstance(member, ast.FunctionDef) and member.name == "__init__":
                    capacity[node.name] = (
                        len(member.args.posonlyargs)
                        + len([a for a in member.args.args if a.arg != "self"])
                    )
    return capacity


def called_name(node: ast.Call) -> str | None:
    if isinstance(node.func, ast.Name):
        return node.func.id
    if isinstance(node.func, ast.Attribute):
        return node.func.attr
    return None


def bound_names(tree: ast.Module) -> set[str]:
    """Every name the module binds by import, assignment, def, or class."""
    names: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                names.add(alias.asname or alias.name.split(".")[0])
        elif isinstance(node, ast.ImportFrom):
            for alias in node.names:
                names.add(alias.asname or alias.name)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            names.add(node.name)
        elif isinstance(node, ast.Name) and isinstance(node.ctx, ast.Store):
            names.add(node.id)
    return names


class the_support_module_is_readable(unittest.TestCase):
    def test_every_helper_is_present_with_keyword_only_parameters(self) -> None:
        accepted = accepted_parameters()
        capacity = positional_capacity()
        for name in HELPER_NAMES:
            self.assertIn(name, accepted, f"{name} is missing from the support module")
            self.assertIn("parent", accepted[name], name)
            self.assertIn("prefix", accepted[name], name)
            # Keyword-only is what makes a positional call a TypeError, which is
            # the shape the call scan below relies on.
            self.assertEqual(capacity.get(name), 0, f"{name} accepts a positional")
            # `dir` is tempfile's keyword and no helper accepts it. If one ever
            # does, this list has to be revisited rather than passing quietly.
            self.assertNotIn("dir", accepted[name], name)


class every_call_site_can_be_made(unittest.TestCase):
    def offences(self, check) -> list[str]:
        found: list[str] = []
        for path in python_sources():
            if path == SUPPORT_MODULE:
                continue
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            relative = path.relative_to(REPOSITORY_ROOT).as_posix()
            found.extend(check(tree, relative))
        return found

    def test_no_call_passes_a_keyword_the_helper_does_not_accept(self) -> None:
        accepted = accepted_parameters()

        def check(tree: ast.Module, relative: str):
            for node in ast.walk(tree):
                if not isinstance(node, ast.Call):
                    continue
                name = called_name(node)
                if name not in HELPER_NAMES:
                    continue
                for keyword in node.keywords:
                    if keyword.arg is None or keyword.arg in accepted[name]:
                        continue
                    yield (
                        f"{relative}:{node.lineno}: {name}({keyword.arg}=...) —"
                        f" accepted: {sorted(accepted[name])}"
                    )

        offences = self.offences(check)
        self.assertEqual(offences, [], "\n".join(offences))

    def test_no_call_passes_a_positional_to_a_keyword_only_helper(self) -> None:
        def check(tree: ast.Module, relative: str):
            for node in ast.walk(tree):
                if not isinstance(node, ast.Call):
                    continue
                name = called_name(node)
                if name not in HELPER_NAMES or not node.args:
                    continue
                yield (
                    f"{relative}:{node.lineno}: {name}(...) passes"
                    f" {len(node.args)} positional argument(s); every parameter"
                    " is keyword-only"
                )

        offences = self.offences(check)
        self.assertEqual(offences, [], "\n".join(offences))

    def test_no_file_calls_a_helper_it_never_bound(self) -> None:
        """The NameError half of the same class.

        `tempfile.TemporaryDirectory()` without importing `tempfile` was one of
        the two original defects. `ContainedTemporaryDirectory()` without
        importing it is the same failure, and shape is the only cheap way to see
        either.
        """
        offences: list[str] = []
        for path in python_sources():
            if path == SUPPORT_MODULE:
                continue
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            bound = bound_names(tree)
            relative = path.relative_to(REPOSITORY_ROOT).as_posix()
            for node in ast.walk(tree):
                if not isinstance(node, ast.Call):
                    continue
                if not isinstance(node.func, ast.Name):
                    continue
                if node.func.id in HELPER_NAMES and node.func.id not in bound:
                    offences.append(
                        f"{relative}:{node.lineno}: {node.func.id} is called but"
                        " never imported"
                    )
        self.assertEqual(offences, [], "\n".join(offences))

    def test_no_test_creates_a_temporary_through_tempfile(self) -> None:
        offences: list[str] = []
        for path in tempfile_sources():
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            bound = bound_names(tree)
            relative = path.relative_to(REPOSITORY_ROOT).as_posix()
            for node in ast.walk(tree):
                if (
                    isinstance(node, ast.Attribute)
                    and node.attr in FORBIDDEN_TEMPFILE_ATTRIBUTES
                    and isinstance(node.value, ast.Name)
                    and node.value.id == "tempfile"
                ):
                    offences.append(
                        f"{relative}:{node.lineno}: tempfile.{node.attr} —"
                        " use scripts/contained_temporary_support instead"
                    )
                # `from tempfile import TemporaryDirectory` reaches the same
                # constructor through a bare name.
                elif (
                    isinstance(node, ast.Call)
                    and isinstance(node.func, ast.Name)
                    and node.func.id in FORBIDDEN_TEMPFILE_ATTRIBUTES
                    and node.func.id in bound
                ):
                    offences.append(
                        f"{relative}:{node.lineno}: {node.func.id} imported from"
                        " tempfile — use scripts/contained_temporary_support"
                        " instead"
                    )
        self.assertEqual(offences, [], "\n".join(offences))

    def test_the_checks_have_something_to_check(self) -> None:
        """A scan that matched nothing would pass every case above vacuously."""
        helper_calls = 0
        for path in python_sources():
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            for node in ast.walk(tree):
                if isinstance(node, ast.Call) and called_name(node) in HELPER_NAMES:
                    helper_calls += 1
        self.assertGreater(helper_calls, 20, "the helper scan found almost no calls")
        self.assertGreater(len(tempfile_sources()), 20, "the tempfile scan is empty")


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False).result.wasSuccessful() else 1)
