#!/usr/bin/env python3
"""The contained temporary helper works at the floor, and says so when it fails.

Two defects in one small module, settled together.

[[BPR-0867]]. `shutil.rmtree` was called with `onexc=`, which arrived in Python
3.12, while `cmake/BprDependencies.cmake` declares a 3.10 floor. Nothing was red,
because every machine in the run happened to be newer; the gap would have
surfaced on whichever machine was oldest, as a `TypeError` from inside the
cleanup rather than as "your Python is too old". Two checks here exist so that a
two-minor-version drift cannot be silent again: one reads the floor out of the
CMake file and compares it against the version the module says it needs, and one
runs *both* callback branches on this interpreter. The older keyword is not a
rename — `onerror` is handed `sys.exc_info()`'s triple where `onexc` is handed
the exception — so the pre-3.12 branch is an adapter, and an adapter can be wrong
in a way a keyword cannot.

[[BPR-0868]]. The retry handler ended in `except OSError: pass`, so a root that
could not be removed produced no exception and no message. That is not what
`tempfile.TemporaryDirectory` did at the call sites this helper replaced, and on
Windows it is exactly the interesting case: a directory held open by a lingering
handle leaks into `%TEMP%` and the row goes green. [[BPR-0876]] recorded one such
leak, unreported. The checks below pin the replacement: already-gone is success,
anything else is named on standard error *and* raised.

## What this does not catch

`ast.parse(feature_version=...)` rejects grammar newer than the floor, not
library calls newer than the floor; a 3.12-only *function* would pass it. That is
why the `shutil.rmtree` keyword is checked by name as well, and why the load
-bearing assertion runs both branches rather than reading them. Neither check
generalises to the rest of the tree, deliberately: this is a gate on one module
with one known drift, not a version analyser.

The retry's *rescue* is a Windows behaviour. On POSIX a read-only file inside a
writable directory is removed without any retry at all, so the rescue is proved
here at the seam — a removal function that refuses an unwritable path — rather
than through a real `rmtree`, and the genuine `FILE_ATTRIBUTE_READONLY` case
still wants the Windows station. Nothing here calls `os.chmod` on Windows without
naming [[BPR-0876]] if it is denied, because that denial is measured.
"""

from __future__ import annotations

import ast
import contextlib
import inspect
import io
import os
import re
import shutil
import stat
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts import contained_temporary_support as support  # noqa: E402
from scripts.contained_temporary_support import (  # noqa: E402
    ContainedTemporaryDirectory,
    RemovalFailed,
    contained_temporary_directory,
)

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SUPPORT_MODULE = REPOSITORY_ROOT / "scripts" / "contained_temporary_support.py"
DEPENDENCIES_CMAKE = REPOSITORY_ROOT / "cmake" / "BprDependencies.cmake"

DECLARED_FLOOR = re.compile(
    r'set\(\s*BPR_PYTHON_MINIMUM_VERSION\s+"(?P<version>[0-9]+(?:\.[0-9]+)*)"'
)

# The two branches the module chooses between, named once.
FLOOR_BRANCH = (3, 10)
ONEXC_BRANCH = (3, 12)


def declared_python_floor() -> tuple[int, ...]:
    """The interpreter floor the tree declares, read from the CMake file.

    Read rather than restated. A floor restated in a test is a second place to
    forget, which is the shape of the defect this gate is for.
    """
    match = DECLARED_FLOOR.search(DEPENDENCIES_CMAKE.read_text(encoding="utf-8"))
    if match is None:
        raise AssertionError(
            f"{DEPENDENCIES_CMAKE} no longer states BPR_PYTHON_MINIMUM_VERSION in a"
            " form this gate can read; the floor has to stay machine-readable"
        )
    return tuple(int(part) for part in match.group("version").split("."))


def rmtree_calls(tree: ast.Module) -> list[tuple[int, list[str]]]:
    """Every `shutil.rmtree(...)` in a parsed module, with its keywords."""
    found: list[tuple[int, list[str]]] = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        function = node.func
        name = (
            function.attr
            if isinstance(function, ast.Attribute)
            else getattr(function, "id", None)
        )
        if name != "rmtree":
            continue
        found.append(
            (node.lineno, [keyword.arg for keyword in node.keywords if keyword.arg])
        )
    return found


def branch_is_runnable(version) -> bool:
    """Whether this interpreter still accepts the keyword that branch installs."""
    keyword = next(iter(support.rmtree_callback_arguments(Recorder(), version=version)))
    return keyword in inspect.signature(shutil.rmtree).parameters


class Recorder:
    """A removal handler that records what it was handed instead of retrying."""

    def __init__(self) -> None:
        self.calls: list[tuple[object, str, object]] = []

    def __call__(self, function, path, error) -> None:
        self.calls.append((function, path, error))


class the_module_stays_at_the_declared_floor(unittest.TestCase):
    def test_the_floor_is_readable_and_is_the_one_the_module_names(self) -> None:
        declared = declared_python_floor()
        self.assertGreaterEqual(len(declared), 2, declared)
        self.assertEqual(
            support.MINIMUM_PYTHON_VERSION,
            declared[:2],
            "scripts/contained_temporary_support.py says it needs Python "
            f"{support.MINIMUM_PYTHON_VERSION} while cmake/BprDependencies.cmake"
            f" declares {declared}. One moved without the other, which is the drift"
            " BPR-0867 is about: fix the module, or move the floor everywhere it is"
            " stated, but do not let the two disagree.",
        )

    def test_the_module_parses_under_the_declared_floors_grammar(self) -> None:
        floor = declared_python_floor()[:2]
        source = SUPPORT_MODULE.read_text(encoding="utf-8")
        try:
            ast.parse(source, filename=str(SUPPORT_MODULE), feature_version=floor)
        except SyntaxError as error:
            raise AssertionError(
                "scripts/contained_temporary_support.py uses grammar newer than"
                f" Python {'.'.join(str(part) for part in floor)}: {error}"
            ) from error

    def test_no_rmtree_call_pins_a_callback_keyword_directly(self) -> None:
        """The keyword has to come from the version seam, not from a literal.

        A literal `onexc=` is what made the module need 3.12. A literal
        `onerror=` would make it stop working on a future interpreter that has
        dropped it. Either way the choice belongs in one place.
        """
        calls = rmtree_calls(ast.parse(SUPPORT_MODULE.read_text(encoding="utf-8")))
        self.assertNotEqual(calls, [], "no shutil.rmtree call found to check")
        offences = [
            f"scripts/contained_temporary_support.py:{line}: rmtree({keyword}=...)"
            for line, keywords in calls
            for keyword in keywords
            if keyword in ("onexc", "onerror")
        ]
        self.assertEqual(
            offences,
            [],
            "\n".join(offences)
            + "\nuse rmtree_callback_arguments(handler) so that one interpreter"
            " version decision stays in one place",
        )


class both_callback_branches_work(unittest.TestCase):
    """The pre-3.12 branch is an adapter, so it is run rather than read."""

    def test_the_branch_is_selected_by_version(self) -> None:
        handler = Recorder()
        for version, expected in (
            ((3, 10), "onerror"),
            ((3, 11, 9), "onerror"),
            ((3, 12), "onexc"),
            ((3, 14, 7), "onexc"),
            ((4, 0), "onexc"),
        ):
            with self.subTest(version=version):
                self.assertEqual(
                    list(support.rmtree_callback_arguments(handler, version=version)),
                    [expected],
                )
        # At and above 3.12 the handler is installed unwrapped; below it, wrapped.
        self.assertIs(
            support.rmtree_callback_arguments(handler, version=ONEXC_BRANCH)["onexc"],
            handler,
        )
        self.assertIsNot(
            support.rmtree_callback_arguments(handler, version=FLOOR_BRANCH)["onerror"],
            handler,
        )

    def test_each_branch_hands_the_handler_an_exception_not_a_triple(self) -> None:
        """The whole of BPR-0867, stated as a claim that can fail.

        `onerror` reports `sys.exc_info()`. A handler written for `onexc` and
        installed on `onerror` without translation would be handed a tuple and
        would misread every argument, which no keyword check would notice.
        """
        ran = 0
        for version in (FLOOR_BRANCH, ONEXC_BRANCH):
            with self.subTest(version=version):
                if not branch_is_runnable(version):
                    self.skipTest(
                        f"Python {sys.version_info.major}.{sys.version_info.minor}"
                        f" no longer accepts the keyword the {version} branch"
                        " installs, so that branch cannot be run here"
                    )
                handler = Recorder()
                with contained_temporary_directory(prefix="bpr-ct-branch-") as root:
                    missing = Path(root) / "absent"
                    shutil.rmtree(
                        missing,
                        **support.rmtree_callback_arguments(handler, version=version),
                    )
                self.assertEqual(len(handler.calls), 1, handler.calls)
                _, path, error = handler.calls[0]
                self.assertIsInstance(
                    error,
                    BaseException,
                    f"the {version} branch handed the handler {error!r}",
                )
                self.assertIsInstance(error, FileNotFoundError)
                self.assertEqual(Path(path), missing)
                ran += 1
        self.assertGreater(ran, 0, "neither branch could be run on this interpreter")

    def test_each_branch_lets_a_raising_handler_out_of_rmtree(self) -> None:
        """Raising from the handler is how a removal error reaches the caller.

        It is `shutil.rmtree`'s behaviour rather than ours, both branches of the
        seam depend on it, and BPR-0868's answer rests on it, so both branches
        assert it.
        """

        class deliberate(Exception):
            pass

        def raising(function, path, error) -> None:
            raise deliberate(path)

        for version in (FLOOR_BRANCH, ONEXC_BRANCH):
            with self.subTest(version=version):
                if not branch_is_runnable(version):
                    self.skipTest("branch keyword unavailable on this interpreter")
                with contained_temporary_directory(prefix="bpr-ct-raise-") as root:
                    arguments = support.rmtree_callback_arguments(
                        raising, version=version
                    )
                    with self.assertRaises(deliberate):
                        shutil.rmtree(Path(root) / "absent", **arguments)


class a_failed_removal_is_named_and_raised(unittest.TestCase):
    def test_the_retry_reports_the_path_and_raises(self) -> None:
        def refuses(path) -> None:
            raise PermissionError(13, "the file is in use", str(path))

        with contained_temporary_directory(prefix="bpr-ct-report-") as root:
            target = Path(root) / "held"
            target.mkdir()
            captured = io.StringIO()
            with contextlib.redirect_stderr(captured):
                with self.assertRaises(RemovalFailed) as raised:
                    support._retry_removal(
                        refuses, str(target), PermissionError(13, "denied")
                    )
            reported = captured.getvalue()

        self.assertIn(str(target), reported)
        self.assertIn("the file is in use", reported)
        self.assertIn("could not remove", reported)
        self.assertIn(str(target), str(raised.exception))
        # An OSError, so a call site already treating a failed cleanup as an
        # ordinary filesystem failure keeps treating it as one.
        self.assertIsInstance(raised.exception, OSError)

    def test_the_retry_clears_a_read_only_bit_before_retrying(self) -> None:
        """The rescue the retry exists for, proved at the seam.

        Windows refuses `os.unlink` on a `FILE_ATTRIBUTE_READONLY` entry and
        clearing the bit rescues it. POSIX does not refuse, so the removal
        function is the one that insists here; what is under test either way is
        that the retry chmods *and then* calls the function again.
        """
        removed: list[str] = []

        def only_if_writable(path) -> None:
            if not stat.S_IMODE(os.stat(path).st_mode) & stat.S_IWUSR:
                raise PermissionError(13, "read-only", str(path))
            removed.append(str(path))

        with contained_temporary_directory(prefix="bpr-ct-readonly-") as root:
            target = Path(root) / "read-only"
            target.write_text("x", encoding="utf-8")
            try:
                target.chmod(stat.S_IRUSR)
            except PermissionError:
                self.skipTest("chmod is denied in this environment, BPR-0876")
            with self.assertRaises(PermissionError):
                only_if_writable(str(target))
            support._retry_removal(
                only_if_writable, str(target), PermissionError(13, "read-only")
            )
            self.assertEqual(removed, [str(target)])
            target.chmod(stat.S_IRUSR | stat.S_IWUSR)

    def test_a_root_that_is_already_gone_is_not_an_error(self) -> None:
        """Removal's postcondition is that the root is gone, and it is.

        `tempfile.TemporaryDirectory` ignores `FileNotFoundError` in exactly the
        same place, so this is parity rather than leniency.
        """
        with contained_temporary_directory(prefix="bpr-ct-gone-") as root:
            shutil.rmtree(root)
            self.assertFalse(Path(root).exists())
        # Leaving the block removes it a second time, and must stay quiet.

        held = ContainedTemporaryDirectory(prefix="bpr-ct-gone-object-")
        shutil.rmtree(held.name)
        held.cleanup()

    @unittest.skipIf(os.name == "nt", "the unwritable-parent case is POSIX; BPR-0876")
    @unittest.skipIf(
        hasattr(os, "geteuid") and os.geteuid() == 0,
        "root ignores directory write permission, so the removal would succeed",
    )
    def test_a_removal_the_retry_cannot_fix_escapes_the_context_manager(self) -> None:
        """End to end: the leak reaches the caller instead of vanishing."""
        captured = io.StringIO()
        held: Path | None = None
        try:
            with self.assertRaises(RemovalFailed):
                with contextlib.redirect_stderr(captured):
                    with contained_temporary_directory(prefix="bpr-ct-leak-") as root:
                        held = Path(root)
                        locked = held / "locked"
                        locked.mkdir()
                        (locked / "entry").write_text("x", encoding="utf-8")
                        locked.chmod(stat.S_IRUSR | stat.S_IXUSR)
        finally:
            if held is not None and held.exists():
                (held / "locked").chmod(stat.S_IRWXU)
                shutil.rmtree(held)

        reported = captured.getvalue()
        self.assertIn("could not remove", reported)
        self.assertIn("locked", reported)

    def test_the_ordinary_round_trip_still_works(self) -> None:
        """A gate whose happy path was broken would pass every case above."""
        with contained_temporary_directory(prefix="bpr-ct-round-") as root:
            path = Path(root)
            self.assertTrue(path.is_dir())
            (path / "nested").mkdir()
            (path / "nested" / "file").write_text("x", encoding="utf-8")
        self.assertFalse(path.exists())

        held = ContainedTemporaryDirectory(prefix="bpr-ct-round-object-")
        self.assertTrue(Path(held.name).is_dir())
        held.cleanup()
        self.assertFalse(Path(held.name).exists())


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False).result.wasSuccessful() else 1)
