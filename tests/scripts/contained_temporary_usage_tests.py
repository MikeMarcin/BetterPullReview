#!/usr/bin/env python3
"""Every contained-temporary call under `tests/` can actually be made.

Two call sites could not, and both surfaced only when a full matrix ran the
branch containing them:

- `network_isolation_counterexamples.py` passed `dir=`, which is
  `tempfile.mkdtemp`'s keyword, to a function whose keyword is `parent`.
- `dependency_modernization_tests.py` called `tempfile.TemporaryDirectory()` in a
  file that never imported `tempfile`.

Both are `TypeError`/`NameError` at call time, so nothing short of executing that
exact branch would report them. This checks the shape instead, by parsing the
source, which is why it costs nothing and runs everywhere.

The `tempfile` half is not style. `scripts/contained_temporary_support.py` exists
because `tempfile`'s owner-only descriptor is unreadable inside a Windows
AppContainer, so a test that reaches for `tempfile` is not merely inconsistent
with its neighbours -- it is the form that cannot work where the suite runs.

See `docs/issues/*/BPR-0394-contained-temporary-migration-left-uncallable-sites.md`.
"""

from __future__ import annotations

import ast
import sys
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TESTS_ROOT = REPOSITORY_ROOT / "tests"
SUPPORT_MODULE = REPOSITORY_ROOT / "scripts" / "contained_temporary_support.py"

# The three public entry points and the keywords each accepts. Read from the
# support module at run time rather than restated, so adding a keyword there
# cannot leave this list quietly stale.
HELPER_NAMES = (
    "contained_temporary_directory",
    "ContainedTemporaryDirectory",
    "make_contained_temporary_directory",
)

# `tempfile` used as a namespace is fine -- `gettempdir()` reads a path and
# creates nothing. Only the directory-creating calls carry the AppContainer
# problem.
FORBIDDEN_TEMPFILE_ATTRIBUTES = ("TemporaryDirectory", "mkdtemp")


def python_sources_under(root: Path) -> list[Path]:
    return sorted(
        path
        for path in root.rglob("*.py")
        if "__pycache__" not in path.parts
    )


def accepted_keywords() -> dict[str, frozenset[str]]:
    """The keyword-only parameters each helper actually accepts."""
    tree = ast.parse(SUPPORT_MODULE.read_text(encoding="utf-8"))
    accepted: dict[str, frozenset[str]] = {}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            arguments = node.args
            names = {argument.arg for argument in arguments.kwonlyargs}
            names |= {argument.arg for argument in arguments.args}
            accepted[node.name] = frozenset(names)
        elif isinstance(node, ast.ClassDef):
            for member in node.body:
                if (
                    isinstance(member, ast.FunctionDef)
                    and member.name == "__init__"
                ):
                    names = {
                        argument.arg for argument in member.args.kwonlyargs
                    }
                    names |= {
                        argument.arg
                        for argument in member.args.args
                        if argument.arg != "self"
                    }
                    accepted[node.name] = frozenset(names)
    return accepted


def called_name(node: ast.Call) -> str | None:
    if isinstance(node.func, ast.Name):
        return node.func.id
    if isinstance(node.func, ast.Attribute):
        return node.func.attr
    return None


class the_support_module_is_readable(unittest.TestCase):
    def test_every_helper_is_present_with_keywords(self) -> None:
        accepted = accepted_keywords()
        for name in HELPER_NAMES:
            self.assertIn(name, accepted, f"{name} is missing from the support module")
            self.assertIn("parent", accepted[name], name)
            self.assertIn("prefix", accepted[name], name)
        # The mistake this exists to catch: `dir` is tempfile's keyword, and no
        # helper accepts it. If one ever does, this list has to be revisited
        # rather than the check quietly passing.
        for name in HELPER_NAMES:
            self.assertNotIn("dir", accepted[name], name)


class every_call_site_can_be_made(unittest.TestCase):
    def test_no_call_passes_a_keyword_the_helper_does_not_accept(self) -> None:
        accepted = accepted_keywords()
        offences: list[str] = []
        for path in python_sources_under(TESTS_ROOT):
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            for node in ast.walk(tree):
                if not isinstance(node, ast.Call):
                    continue
                name = called_name(node)
                if name not in HELPER_NAMES:
                    continue
                for keyword in node.keywords:
                    if keyword.arg is None:
                        continue
                    if keyword.arg not in accepted[name]:
                        offences.append(
                            f"{path.relative_to(REPOSITORY_ROOT).as_posix()}"
                            f":{node.lineno}: {name}({keyword.arg}=...) —"
                            f" accepted: {sorted(accepted[name])}"
                        )
        self.assertEqual(offences, [], "\n".join(offences))

    def test_no_test_creates_a_temporary_directory_through_tempfile(self) -> None:
        offences: list[str] = []
        for path in python_sources_under(TESTS_ROOT):
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            for node in ast.walk(tree):
                if not isinstance(node, ast.Attribute):
                    continue
                if node.attr not in FORBIDDEN_TEMPFILE_ATTRIBUTES:
                    continue
                if isinstance(node.value, ast.Name) and node.value.id == "tempfile":
                    offences.append(
                        f"{path.relative_to(REPOSITORY_ROOT).as_posix()}"
                        f":{node.lineno}: tempfile.{node.attr} —"
                        " use scripts/contained_temporary_support instead"
                    )
        self.assertEqual(offences, [], "\n".join(offences))

    def test_the_checks_have_something_to_check(self) -> None:
        """A scan that found no call sites would pass vacuously."""
        found = 0
        for path in python_sources_under(TESTS_ROOT):
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            for node in ast.walk(tree):
                if isinstance(node, ast.Call) and called_name(node) in HELPER_NAMES:
                    found += 1
        self.assertGreater(found, 20, "the helper scan found almost no call sites")


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False).result.wasSuccessful() else 1)
