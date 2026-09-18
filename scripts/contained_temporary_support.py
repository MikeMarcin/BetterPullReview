#!/usr/bin/env python3
"""Temporary roots that a test can still use after the sandbox contains it.

`tempfile.mkdtemp` — and therefore `tempfile.TemporaryDirectory` — calls
`os.mkdir(path, 0o700)`. On POSIX that mode is exactly right: it keeps other
local users out of the directory. On Windows modern CPython translates the same
mode into a *protected* security descriptor granting `SYSTEM`,
`Administrators`, and `OWNER RIGHTS`, with inheritance switched off.

That descriptor is unusable from inside an AppContainer. A lowbox token's access
check additionally requires the object to grant the container's own package SID
or `ALL APPLICATION PACKAGES`, and a protected owner-only descriptor names
neither. The directory's owner is the invoking user rather than the package SID,
so the `OWNER RIGHTS` entry cannot satisfy the restricted pass either. The
contained process is therefore denied access to the directory it just created,
with `WinError 5`, on the very next call.

The fix cannot live where the problem happens, and it must not be injected
ahead of the boundary: the isolation manifest deliberately scrubs
`PYTHONSTARTUP` and its siblings precisely so nothing can execute before the
sandbox. So contained tests call into here instead.

On Windows this creates the directory with no explicit mode, which lets it
inherit the parent temporary directory's entries — including the container's
own `(I)(OI)(CI)(M)` grant. That introduces no new trustee: `%TEMP%` is already
per-user, and inside the container the container is the stronger privacy
boundary. On POSIX nothing changes at all; `tempfile` is used directly and the
`0o700` protection is preserved.

## A removal that fails is not silent

A root that cannot be removed is reported by path to standard error and then
raised as `RemovalFailed`. `tempfile.TemporaryDirectory`, which these call sites
used before, raises as well, so this is the contract they already had rather
than a new one. On Windows a root that will not go away is usually a handle the
course leaked, which is precisely the case this module exists for, and the
report is what makes the leak visible even where the raise arrives too late to
change a verdict. A caller that genuinely wants tolerance says so at its own
call site by catching `RemovalFailed`, where it is visible in that caller's
source; a swallowed `OSError` in here was visible nowhere. A root that is
already gone is not a failure and is not reported.

## The interpreter floor is the tree's, not this module's

`shutil.rmtree`'s error callback changed in Python 3.12: `onexc` replaced
`onerror`, and the two do not take the same arguments. This module selects
between the two shapes so that it keeps working at the declared 3.10 floor
rather than quietly requiring a newer interpreter than the tree asks for.
`tests/scripts/contained_temporary_support_tests.py` fails if that drifts.

See `docs/issues/*/BPR-0350-owner-private-temp-roots-inside-appcontainer.md`,
`docs/issues/*/BPR-0867-the-contained-temporary-helper-needs-python-3-12-*.md`
and `docs/issues/*/BPR-0868-the-contained-temporary-helper-swallows-*.md`.
"""

from __future__ import annotations

import contextlib
import os
import secrets
import shutil
import stat
import sys
import tempfile
from collections.abc import Callable, Iterator, Sequence
from pathlib import Path

__all__ = [
    "ContainedTemporaryDirectory",
    "RemovalFailed",
    "inherits_parent_permissions",
    "contained_temporary_directory",
    "make_contained_temporary_directory",
]

# Enough attempts that a name collision is a real defect rather than luck.
_NAME_ATTEMPTS = 64

# 8 bytes of hex is the same 16-character name width `tempfile` uses.
_NAME_BYTES = 8

# The interpreter this module is written to run on, stated as the tree states
# it in `cmake/BprDependencies.cmake`. The gate reads both and fails if they
# stop agreeing, which is the drift BPR-0867 is about.
MINIMUM_PYTHON_VERSION = (3, 10)

# `shutil.rmtree` grew `onexc` here. Below this the keyword is `onerror`, and
# its callback is handed `sys.exc_info()`'s triple rather than the exception, so
# the older branch needs an adapter rather than a renamed keyword.
ONEXC_PYTHON_VERSION = (3, 12)

# `(function, path, exception)` — `onexc`'s shape, which is the one this module
# writes its handlers in.
RemovalHandler = Callable[[Callable[[str], object], str, BaseException], None]


class RemovalFailed(OSError):
    """A temporary root that could not be removed, named with its path.

    Derived from `OSError` so that a call site already treating a failed
    cleanup as an ordinary filesystem failure keeps treating it as one.
    """


def inherits_parent_permissions() -> bool:
    """True when a new temporary root must inherit rather than set its mode."""
    return os.name == "nt"


def make_contained_temporary_directory(
    *,
    prefix: str = "bpr",
    suffix: str = "",
    parent: str | os.PathLike[str] | None = None,
) -> Path:
    """Create and return a new temporary directory the caller can use.

    Mirrors `tempfile.mkdtemp`'s contract: the directory is new, the name is
    unpredictable, and the caller owns removing it. Unlike `mkdtemp`, the
    Windows path does not stamp a protected owner-only descriptor that an
    AppContainer cannot read back.
    """
    base = Path(tempfile.gettempdir()) if parent is None else Path(parent)

    if not inherits_parent_permissions():
        return Path(
            tempfile.mkdtemp(prefix=prefix, suffix=suffix, dir=base)
        )

    for _ in range(_NAME_ATTEMPTS):
        candidate = base / f"{prefix}{secrets.token_hex(_NAME_BYTES)}{suffix}"
        try:
            # No mode argument: inherit the parent's entries deliberately.
            os.mkdir(candidate)
        except FileExistsError:
            continue
        return candidate

    raise FileExistsError(
        f"could not find an unused temporary name under {base} in "
        f"{_NAME_ATTEMPTS} attempts"
    )


def _as_onerror(handler: RemovalHandler):
    """Present an `onexc` handler through the pre-3.12 `onerror` shape.

    The difference between the two is not the keyword that installs them:
    `onerror` is handed `sys.exc_info()`'s triple where `onexc` is handed the
    exception itself, so the older branch needs this translation to reach a
    handler written once.
    """

    def onerror(function, path, exc_info) -> None:
        handler(function, path, exc_info[1])

    return onerror


def rmtree_callback_arguments(
    handler: RemovalHandler,
    *,
    version: Sequence[int] = sys.version_info,
) -> dict[str, object]:
    """The keyword arguments that install `handler` on `shutil.rmtree`.

    `version` is a parameter rather than a read of `sys.version_info` so that
    the gate can run both branches on one interpreter. Nothing in this module
    passes it.
    """
    if tuple(version[:2]) >= ONEXC_PYTHON_VERSION:
        return {"onexc": handler}
    return {"onerror": _as_onerror(handler)}


def _retry_removal(function, path, error) -> None:
    """Clear a read-only bit a test set deliberately and retry once.

    What the retry cannot fix is named and raised rather than dropped: a
    scratch root that will not go away is a leak, and on Windows it is usually
    a handle the course is still holding.
    """
    if isinstance(error, FileNotFoundError):
        # The postcondition is that the root is gone, and it is.
        return
    try:
        os.chmod(path, stat.S_IWRITE | stat.S_IREAD)
        function(path)
    except FileNotFoundError:
        return
    except OSError as retry_error:
        raise _removal_failed(path, retry_error) from retry_error


def _removal_failed(path, error: BaseException) -> RemovalFailed:
    """Report the leak, then hand back the error to raise.

    Reported as well as raised because the two answer different questions. The
    raise fails the row wherever the removal still precedes the row's verdict.
    The report is what survives the cases where it does not — a caller that
    catches broadly, or a row whose marker is already on standard output
    ([[BPR-0890]]) — and a leak nobody can see is the defect this replaced.
    """
    message = f"contained temporary: could not remove {path}: {error}"
    print(message, file=sys.stderr, flush=True)
    return RemovalFailed(message)


def _remove_tree(path) -> None:
    shutil.rmtree(path, **rmtree_callback_arguments(_retry_removal))


@contextlib.contextmanager
def contained_temporary_directory(
    *,
    prefix: str = "bpr",
    suffix: str = "",
    parent: str | os.PathLike[str] | None = None,
) -> Iterator[str]:
    """`TemporaryDirectory` for contained tests, yielding the path as `str`.

    Yields `str` rather than `Path` so that it substitutes directly for
    `tempfile.TemporaryDirectory()` at an existing call site, and raises
    `RemovalFailed` on the way out if the root cannot be removed, which is what
    `tempfile.TemporaryDirectory` did at those call sites before.
    """
    root = make_contained_temporary_directory(prefix=prefix, suffix=suffix, parent=parent)
    try:
        yield str(root)
    finally:
        _remove_tree(root)


class ContainedTemporaryDirectory:
    """The same root as an object, for a call site that holds one.

    Exposes `name` and `cleanup()` so that it substitutes directly for a
    `tempfile.TemporaryDirectory` held across `setUp`/`tearDown`. `cleanup()`
    raises `RemovalFailed` if the root will not go away, as that one did.
    """

    def __init__(
        self,
        *,
        prefix: str = "bpr",
        suffix: str = "",
        parent: str | os.PathLike[str] | None = None,
    ) -> None:
        self._path = make_contained_temporary_directory(
            prefix=prefix, suffix=suffix, parent=parent
        )
        self.name = str(self._path)

    @property
    def path(self) -> Path:
        return self._path

    def cleanup(self) -> None:
        _remove_tree(self._path)

    def __enter__(self) -> str:
        return self.name

    def __exit__(self, *unused: object) -> None:
        self.cleanup()
