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

See `docs/issues/*/BPR-0350-owner-private-temp-roots-inside-appcontainer.md`.
"""

from __future__ import annotations

import contextlib
import os
import secrets
import shutil
import stat
import tempfile
from collections.abc import Iterator
from pathlib import Path

__all__ = [
    "ContainedTemporaryDirectory",
    "inherits_parent_permissions",
    "contained_temporary_directory",
    "make_contained_temporary_directory",
]

# Enough attempts that a name collision is a real defect rather than luck.
_NAME_ATTEMPTS = 64

# 8 bytes of hex is the same 16-character name width `tempfile` uses.
_NAME_BYTES = 8


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


def _force_writable(function, path, excinfo) -> None:
    """Clear a read-only bit a test set deliberately, then retry once."""
    del excinfo
    try:
        os.chmod(path, stat.S_IWRITE | stat.S_IREAD)
        function(path)
    except OSError:
        # Removal of a scratch directory is not the claim under test.
        pass


@contextlib.contextmanager
def contained_temporary_directory(
    *,
    prefix: str = "bpr",
    suffix: str = "",
    parent: str | os.PathLike[str] | None = None,
) -> Iterator[str]:
    """`TemporaryDirectory` for contained tests, yielding the path as `str`.

    Yields `str` rather than `Path` so that it substitutes directly for
    `tempfile.TemporaryDirectory()` at an existing call site.
    """
    root = make_contained_temporary_directory(prefix=prefix, suffix=suffix, parent=parent)
    try:
        yield str(root)
    finally:
        shutil.rmtree(root, onexc=_force_writable)


class ContainedTemporaryDirectory:
    """The same root as an object, for a call site that holds one.

    Exposes `name` and `cleanup()` so that it substitutes directly for a
    `tempfile.TemporaryDirectory` held across `setUp`/`tearDown`.
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
        shutil.rmtree(self._path, onexc=_force_writable)

    def __enter__(self) -> str:
        return self.name

    def __exit__(self, *unused: object) -> None:
        self.cleanup()
