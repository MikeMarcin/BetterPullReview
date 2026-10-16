#!/usr/bin/env python3
"""Build and verify the exact macOS capsule for the attended live lane."""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import os
import pathlib
import plistlib
import re
import shutil
import signal
import socket
import stat
import subprocess
import tempfile
import sys
import time
from dataclasses import dataclass
from typing import Iterator


SCHEMA = "bpr.designated_live_macos_capsule"
VERSION = "1"
APP_RELATIVE = pathlib.PurePosixPath("BetterPullReview.app/Contents/MacOS/BetterPullReview")
HELPER_RELATIVE = pathlib.PurePosixPath(
    "BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor"
)
QT_CONF_RELATIVE = pathlib.PurePosixPath(
    "BetterPullReview.app/Contents/Resources/qt.conf"
)
VERIFIER_RELATIVE = pathlib.PurePosixPath(
    "BetterPullReview.app/Contents/Resources/designated_live_macos_capsule.py"
)
QT_CONF = b"[Paths]\nPlugins = PlugIns\n"

# BPR-0262 fact_1. The image carries the capsule and the inventory that
# describes it, side by side. The inventory cannot sit inside the capsule --
# it would have to cover itself -- so it is a sibling, and both are inside the
# image the digest binds.
IMAGE_CAPSULE_RELATIVE = pathlib.PurePosixPath("capsule")
IMAGE_INVENTORY_RELATIVE = pathlib.PurePosixPath("capsule.inventory")
IMAGE_VOLUME_NAME = "BetterPullReviewCapsule"
MACH_MAGICS = {
    b"\xfe\xed\xfa\xce",
    b"\xce\xfa\xed\xfe",
    b"\xfe\xed\xfa\xcf",
    b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe",
    b"\xbe\xba\xfe\xca",
    b"\xca\xfe\xba\xbf",
    b"\xbf\xba\xfe\xca",
}
ALLOWED_PLUGIN_FILES = {
    pathlib.PurePosixPath("platforms/libqcocoa.dylib"),
    pathlib.PurePosixPath("styles/libqmacstyle.dylib"),
    pathlib.PurePosixPath("tls/libqsecuretransportbackend.dylib"),
    pathlib.PurePosixPath("networkinformation/libqapplenetworkinformation.dylib"),
    # RFC-BPR-0084. Body images decode GIF and JPEG.
    pathlib.PurePosixPath("imageformats/libqgif.dylib"),
    pathlib.PurePosixPath("imageformats/libqjpeg.dylib"),
}
MAXIMUM_INVENTORY_BYTES = 16 * 1024 * 1024
MAXIMUM_IMAGE_BYTES = 16 * 1024 * 1024 * 1024
IMAGE_DETACH_TIMEOUT_SECONDS = 60.0
STAGE_ONE_TIMEOUT_SECONDS = 1500.0
STAGE_ONE_TERM_GRACE_SECONDS = 10.0
STAGE_ONE_KILL_GRACE_SECONDS = 10.0
STAGE_ZERO_SIGNALS = (signal.SIGTERM, signal.SIGINT, signal.SIGHUP)


class CapsuleFailure(RuntimeError):
    """One fail-closed capsule contract violation."""


class StageZeroInterrupted(CapsuleFailure):
    """A handled termination request that must unwind through cleanup."""

    def __init__(self, signum: int) -> None:
        super().__init__(f"stage zero interrupted by signal {signum}")
        self.signum = signum


class StageZeroSignalScope:
    """Raises one termination request and ignores signals during cleanup."""

    def __init__(self) -> None:
        self.previous: dict[int, signal.Handlers] = {}
        self.deferred = 0
        self.pending: int | None = None
        self.cleaning = False
        self.interrupted = False

    def install(self) -> None:
        for signum in STAGE_ZERO_SIGNALS:
            self.previous[signum] = signal.getsignal(signum)
            signal.signal(signum, self.handle)

    def restore(self) -> None:
        for signum, handler in self.previous.items():
            signal.signal(signum, handler)

    def ignore(self) -> None:
        for signum in STAGE_ZERO_SIGNALS:
            signal.signal(signum, signal.SIG_IGN)

    def handle(self, signum: int, _frame: object) -> None:
        if self.cleaning or self.interrupted or self.pending is not None:
            return
        if self.deferred:
            self.pending = signum
            return
        self.interrupted = True
        self.ignore()
        raise StageZeroInterrupted(signum)

    @contextlib.contextmanager
    def defer_interrupts(self) -> Iterator[None]:
        self.deferred += 1
        try:
            yield
        finally:
            self.deferred -= 1
            if not self.deferred and self.pending is not None:
                signum = self.pending
                self.pending = None
                self.interrupted = True
                self.ignore()
                raise StageZeroInterrupted(signum)

    def begin_cleanup(self) -> None:
        self.cleaning = True
        self.ignore()


@contextlib.contextmanager
def scoped_stage_zero_signals() -> Iterator[StageZeroSignalScope]:
    scope = StageZeroSignalScope()
    scope.install()
    try:
        yield scope
    finally:
        scope.restore()


@dataclass(frozen=True)
class Entry:
    kind: str
    mode: int
    digest_or_target: str
    relative: str

    def render(self) -> str:
        return f"{self.kind}\t{self.mode:04o}\t{self.digest_or_target}\t{self.relative}"


@dataclass(frozen=True)
class FilesystemIdentity:
    device: int
    inode: int
    mode: int
    owner: int
    group: int
    size: int
    modified_ns: int
    changed_ns: int


@dataclass(frozen=True)
class RetainedImage:
    path: pathlib.Path
    descriptor: int
    identity: FilesystemIdentity
    digest: str


@dataclass(frozen=True)
class RuntimeIdentity:
    mount: FilesystemIdentity
    capsule: FilesystemIdentity
    files: tuple[tuple[pathlib.PurePosixPath, FilesystemIdentity, str], ...]
    mount_descriptor: int = -1
    capsule_descriptor: int = -1


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def sha256_descriptor(descriptor: int) -> str:
    digest = hashlib.sha256()
    os.lseek(descriptor, 0, os.SEEK_SET)
    while block := os.read(descriptor, 1024 * 1024):
        digest.update(block)
    os.lseek(descriptor, 0, os.SEEK_SET)
    return digest.hexdigest()


def filesystem_identity(status: os.stat_result) -> FilesystemIdentity:
    return FilesystemIdentity(
        device=status.st_dev,
        inode=status.st_ino,
        mode=status.st_mode,
        owner=status.st_uid,
        group=status.st_gid,
        size=status.st_size,
        modified_ns=status.st_mtime_ns,
        changed_ns=status.st_ctime_ns,
    )


def require_resolved_absolute(path: pathlib.Path, label: str) -> pathlib.Path:
    if not path.is_absolute() or pathlib.Path(os.path.normpath(path)) != path:
        raise CapsuleFailure(f"{label} path is not absolute and normalized")
    resolved = path.resolve(strict=True)
    if path != resolved:
        raise CapsuleFailure(f"{label} path is not resolved")
    return path


def retain_image(path: pathlib.Path) -> RetainedImage:
    path = require_resolved_absolute(path, "image")
    descriptor = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
    try:
        status = os.fstat(descriptor)
        if not stat.S_ISREG(status.st_mode) or status.st_uid != os.geteuid():
            raise CapsuleFailure("image is not an owner-controlled regular file")
        if status.st_mode & 0o077:
            raise CapsuleFailure("image is reachable by group or world")
        if status.st_size <= 0 or status.st_size > MAXIMUM_IMAGE_BYTES:
            raise CapsuleFailure("image size is outside the bounded range")
        identity = filesystem_identity(status)
        digest = sha256_descriptor(descriptor)
        if filesystem_identity(os.fstat(descriptor)) != identity:
            raise CapsuleFailure("image changed while it was hashed")
        return RetainedImage(path, descriptor, identity, digest)
    except BaseException:
        os.close(descriptor)
        raise


def image_identity_except_ctime(identity: FilesystemIdentity) -> tuple[int, ...]:
    # DiskImages changes the image inode's ctime while attaching even though
    # the data fork, inode, size, mode, owner, and mtime remain unchanged.
    # This exception is used exactly once to establish the post-attach
    # baseline. Later checks include ctime, so it is not an ongoing blind spot.
    return (
        identity.device,
        identity.inode,
        identity.mode,
        identity.owner,
        identity.group,
        identity.size,
        identity.modified_ns,
    )


def rebaseline_attached_image(image: RetainedImage) -> RetainedImage:
    """Accepts only DiskImages' attach ctime transition, then closes it."""
    attached_identity = filesystem_identity(os.fstat(image.descriptor))
    if image_identity_except_ctime(attached_identity) != image_identity_except_ctime(
        image.identity
    ):
        raise CapsuleFailure("retained image identity drifted during attach")
    if sha256_descriptor(image.descriptor) != image.digest:
        raise CapsuleFailure("retained image bytes drifted during attach")
    if filesystem_identity(os.fstat(image.descriptor)) != attached_identity:
        raise CapsuleFailure("retained image changed while rebaselined")
    try:
        path_status = os.lstat(image.path)
    except OSError as error:
        raise CapsuleFailure("image path identity drifted") from error
    if filesystem_identity(path_status) != attached_identity:
        raise CapsuleFailure("image path identity drifted")
    path_descriptor = os.open(
        image.path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW
    )
    try:
        if filesystem_identity(os.fstat(path_descriptor)) != attached_identity:
            raise CapsuleFailure("image path identity drifted")
        if sha256_descriptor(path_descriptor) != image.digest:
            raise CapsuleFailure("image path bytes drifted")
        if filesystem_identity(os.fstat(path_descriptor)) != attached_identity:
            raise CapsuleFailure("image path changed while rebaselined")
    finally:
        os.close(path_descriptor)
    return RetainedImage(
        image.path,
        image.descriptor,
        attached_identity,
        image.digest,
    )


def revalidate_image(image: RetainedImage) -> None:
    if filesystem_identity(os.fstat(image.descriptor)) != image.identity:
        raise CapsuleFailure("retained image identity drifted")
    if sha256_descriptor(image.descriptor) != image.digest:
        raise CapsuleFailure("retained image bytes drifted")
    if filesystem_identity(os.fstat(image.descriptor)) != image.identity:
        raise CapsuleFailure("retained image changed while revalidated")
    try:
        path_status = os.lstat(image.path)
    except OSError as error:
        raise CapsuleFailure("image path identity drifted") from error
    if filesystem_identity(path_status) != image.identity:
        raise CapsuleFailure("image path identity drifted")
    path_descriptor = os.open(
        image.path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW
    )
    try:
        if filesystem_identity(os.fstat(path_descriptor)) != image.identity:
            raise CapsuleFailure("image path identity drifted")
        if sha256_descriptor(path_descriptor) != image.digest:
            raise CapsuleFailure("image path bytes drifted")
        if filesystem_identity(os.fstat(path_descriptor)) != image.identity:
            raise CapsuleFailure("image path changed while revalidated")
    finally:
        os.close(path_descriptor)


def relative_text(root: pathlib.Path, path: pathlib.Path) -> str:
    relative = path.relative_to(root).as_posix()
    if not relative or "\t" in relative or "\n" in relative or "\r" in relative:
        raise CapsuleFailure("capsule path is not inventory-safe")
    return relative


def retained_relative_path(path: pathlib.Path) -> pathlib.Path:
    """Keeps one descriptor-mode lookup relative to the retained cwd."""
    if path.is_absolute():
        raise CapsuleFailure("retained capsule lookup is not relative")
    normalized = pathlib.PurePosixPath(os.path.normpath(path.as_posix()))
    if normalized.is_absolute() or ".." in normalized.parts:
        raise CapsuleFailure("retained capsule lookup escapes the root")
    return pathlib.Path(normalized)


def inventory_entries(
    root: pathlib.Path,
    *,
    retained_root: bool = False,
) -> list[Entry]:
    if not root.is_dir() or root.is_symlink():
        raise CapsuleFailure("capsule root is not a real directory")
    entries: list[Entry] = []
    for path in sorted(root.rglob("*"), key=lambda value: value.as_posix()):
        status = path.lstat()
        mode = stat.S_IMODE(status.st_mode)
        relative = relative_text(root, path)
        if stat.S_ISLNK(status.st_mode):
            target = os.readlink(path)
            if not target or "\t" in target or "\n" in target or "\r" in target:
                raise CapsuleFailure("capsule symlink target is not inventory-safe")
            pure = pathlib.PurePosixPath(target)
            if pure.is_absolute():
                raise CapsuleFailure("capsule contains an absolute symlink")
            try:
                if retained_root:
                    resolved = retained_relative_path(path.parent / target)
                    if not resolved.exists():
                        raise OSError("retained symlink target does not exist")
                else:
                    resolved = (path.parent / target).resolve(strict=True)
                    resolved.relative_to(root.resolve(strict=True))
            except (OSError, ValueError) as error:
                raise CapsuleFailure("capsule symlink escapes or does not resolve") from error
            entries.append(Entry("symlink", mode, target, relative))
        elif stat.S_ISDIR(status.st_mode):
            entries.append(Entry("directory", mode, "-", relative))
        elif stat.S_ISREG(status.st_mode):
            entries.append(Entry("file", mode, sha256(path), relative))
        else:
            raise CapsuleFailure(f"capsule contains unsupported filesystem entry: {relative}")
    return entries


def render_inventory(root: pathlib.Path, source_sha256: str) -> str:
    if len(source_sha256) != 64 or any(character not in "0123456789abcdef" for character in source_sha256):
        raise CapsuleFailure("source SHA-256 is not canonical")
    lines = [
        f"schema_name={SCHEMA}",
        f"schema_version={VERSION}",
        f"source_sha256={source_sha256}",
        f"application={APP_RELATIVE}",
        f"supervisor={HELPER_RELATIVE}",
        f"verifier={VERIFIER_RELATIVE}",
    ]
    lines.extend(entry.render() for entry in inventory_entries(root))
    return "\n".join(lines) + "\n"


def parse_inventory(text: str) -> tuple[str, list[Entry]]:
    if "\r" in text or "\0" in text or not text.endswith("\n"):
        raise CapsuleFailure("inventory framing is invalid")
    lines = text.splitlines()
    expected = [
        f"schema_name={SCHEMA}",
        f"schema_version={VERSION}",
    ]
    if lines[:2] != expected or len(lines) < 7:
        raise CapsuleFailure("inventory header is invalid")
    if not lines[2].startswith("source_sha256="):
        raise CapsuleFailure("inventory source identity is absent")
    source_sha256 = lines[2].removeprefix("source_sha256=")
    if len(source_sha256) != 64 or any(character not in "0123456789abcdef" for character in source_sha256):
        raise CapsuleFailure("inventory source identity is invalid")
    if (
        lines[3] != f"application={APP_RELATIVE}"
        or lines[4] != f"supervisor={HELPER_RELATIVE}"
        or lines[5] != f"verifier={VERIFIER_RELATIVE}"
    ):
        raise CapsuleFailure("inventory executable identities are invalid")
    entries: list[Entry] = []
    previous = ""
    for line in lines[6:]:
        fields = line.split("\t")
        if len(fields) != 4 or fields[0] not in {"directory", "file", "symlink"}:
            raise CapsuleFailure("inventory entry is malformed")
        kind, mode_text, value, relative = fields
        if len(mode_text) != 4 or any(character not in "01234567" for character in mode_text):
            raise CapsuleFailure("inventory mode is malformed")
        if relative <= previous or pathlib.PurePosixPath(relative).is_absolute() or ".." in pathlib.PurePosixPath(relative).parts:
            raise CapsuleFailure("inventory paths are not canonical and sorted")
        if kind == "file" and (len(value) != 64 or any(character not in "0123456789abcdef" for character in value)):
            raise CapsuleFailure("inventory file digest is malformed")
        if kind == "directory" and value != "-":
            raise CapsuleFailure("inventory directory value is malformed")
        entries.append(Entry(kind, int(mode_text, 8), value, relative))
        previous = relative
    return source_sha256, entries


def read_inventory_descriptor(descriptor: int) -> str:
    """Reads exactly one anonymous inherited inventory stream through EOF."""
    if descriptor < 0:
        raise CapsuleFailure("inventory descriptor is invalid")
    status = os.fstat(descriptor)
    if not stat.S_ISSOCK(status.st_mode):
        raise CapsuleFailure("inventory descriptor is not a socket")
    endpoint = socket.socket(fileno=descriptor)
    try:
        anonymous = endpoint.getsockname() in {"", b""} and endpoint.getpeername() in {
            "",
            b"",
        }
        stream = endpoint.getsockopt(socket.SOL_SOCKET, socket.SO_TYPE) == socket.SOCK_STREAM
    except OSError as error:
        raise CapsuleFailure("inventory descriptor socket cannot be inspected") from error
    finally:
        endpoint.detach()
    if not anonymous:
        raise CapsuleFailure("inventory descriptor socket is named")
    if not stream:
        raise CapsuleFailure("inventory descriptor socket is not a stream")
    content = bytearray()
    while len(content) <= MAXIMUM_INVENTORY_BYTES:
        block = os.read(
            descriptor,
            min(1024 * 1024, MAXIMUM_INVENTORY_BYTES + 1 - len(content)),
        )
        if not block:
            break
        content.extend(block)
    if not content or len(content) > MAXIMUM_INVENTORY_BYTES:
        raise CapsuleFailure("inventory descriptor framing is invalid")
    try:
        return content.decode("utf-8")
    except UnicodeDecodeError as error:
        raise CapsuleFailure("inventory descriptor is not UTF-8") from error


def is_mach_o(path: pathlib.Path) -> bool:
    if not path.is_file() or path.is_symlink():
        return False
    with path.open("rb") as source:
        return source.read(4) in MACH_MAGICS


def command(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        arguments,
        check=False,
        capture_output=True,
        text=True,
        env={"PATH": "/usr/bin:/bin:/usr/sbin:/sbin"},
    )


def mach_architecture_sections(
    path: pathlib.Path, stdout: str, label: str
) -> list[list[str]]:
    """Split otool output into one body per architecture, validating framing.

    A thin file frames its output as ``<path>:``; a universal file repeats
    ``<path> (architecture <name>):`` once per slice. Anything else is
    unrecognized framing and fails closed.
    """
    lines = stdout.splitlines()
    headers = [
        index
        for index, line in enumerate(lines)
        if line == f"{path}:"
        or (line.startswith(f"{path} (architecture ") and line.endswith("):"))
    ]
    if not headers or headers[0] != 0:
        raise CapsuleFailure(f"{label} framing is invalid")
    if lines[0] == f"{path}:" and len(headers) != 1:
        raise CapsuleFailure(f"{label} framing is invalid")
    sections: list[list[str]] = []
    for position, start in enumerate(headers):
        end = headers[position + 1] if position + 1 < len(headers) else len(lines)
        sections.append(lines[start + 1 : end])
    return sections


def agreed_across_architectures(values: list, label: str):
    """Every architecture slice must report the same fact."""
    for value in values[1:]:
        if value != values[0]:
            raise CapsuleFailure(f"{label} disagrees between architectures")
    return values[0]


def parse_rpaths(path: pathlib.Path, stdout: str) -> list[str]:
    sections = mach_architecture_sections(path, stdout, "LC_RPATH")
    per_architecture: list[list[str]] = []
    for body in sections:
        rpaths: list[str] = []
        for index, line in enumerate(body):
            if line.strip() == "cmd LC_RPATH" and index + 2 < len(body):
                value = body[index + 2].strip()
                if not value.startswith("path ") or " (offset " not in value:
                    raise CapsuleFailure("LC_RPATH framing is invalid")
                rpaths.append(value[5 : value.index(" (offset ")])
        per_architecture.append(rpaths)
    return agreed_across_architectures(per_architecture, "LC_RPATH")


def parse_install_id(path: pathlib.Path, stdout: str) -> str | None:
    sections = mach_architecture_sections(path, stdout, "Mach-O install-name")
    identities: list[str | None] = []
    for body in sections:
        if not body:
            identities.append(None)
        elif len(body) == 1:
            identities.append(body[0].strip())
        else:
            raise CapsuleFailure("Mach-O install-name framing is invalid")
    return agreed_across_architectures(identities, "Mach-O install-name")


def parse_dependencies(
    path: pathlib.Path, stdout: str, install_id: str | None
) -> list[str]:
    sections = mach_architecture_sections(path, stdout, "Mach-O dependency")
    per_architecture: list[list[str]] = []
    for body in sections:
        dependencies: list[str] = []
        for line in body:
            dependency = line.strip().split(" (compatibility version", 1)[0]
            if dependency and dependency != install_id:
                dependencies.append(dependency)
        per_architecture.append(dependencies)
    return agreed_across_architectures(per_architecture, "Mach-O dependency")


def mach_rpaths(path: pathlib.Path, otool: pathlib.Path) -> list[str]:
    result = command([str(otool), "-l", str(path)])
    if result.returncode != 0:
        raise CapsuleFailure("otool load-command inspection failed")
    return parse_rpaths(path, result.stdout)


def mach_install_id(path: pathlib.Path, otool: pathlib.Path) -> str | None:
    result = command([str(otool), "-D", str(path)])
    if result.returncode != 0:
        raise CapsuleFailure("otool install-name inspection failed")
    return parse_install_id(path, result.stdout)


def mach_dependencies(path: pathlib.Path, otool: pathlib.Path) -> list[str]:
    result = command([str(otool), "-L", str(path)])
    if result.returncode != 0:
        raise CapsuleFailure("otool dependency inspection failed")
    return parse_dependencies(path, result.stdout, mach_install_id(path, otool))


def resolve_token_path(
    token: str,
    binary: pathlib.Path,
    app_executable: pathlib.Path,
) -> pathlib.Path:
    if token == "@loader_path":
        return binary.parent
    if token.startswith("@loader_path/"):
        return binary.parent / token.removeprefix("@loader_path/")
    if token == "@executable_path":
        return app_executable.parent
    if token.startswith("@executable_path/"):
        return app_executable.parent / token.removeprefix("@executable_path/")
    return pathlib.Path(token)


def verify_dependencies(
    root: pathlib.Path,
    otool: pathlib.Path,
    *,
    retained_root: bool = False,
) -> None:
    app_executable = root / APP_RELATIVE
    for binary in (path for path in root.rglob("*") if is_mach_o(path)):
        rpaths = mach_rpaths(binary, otool)
        for dependency in mach_dependencies(binary, otool):
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue
            candidates: list[pathlib.Path]
            if dependency.startswith("@rpath/"):
                suffix = dependency.removeprefix("@rpath/")
                candidates = [
                    resolve_token_path(rpath, binary, app_executable) / suffix
                    for rpath in rpaths
                ]
            elif dependency.startswith(("@loader_path/", "@executable_path/")):
                candidates = [resolve_token_path(dependency, binary, app_executable)]
            else:
                raise CapsuleFailure(f"external runtime dependency is forbidden: {dependency}")
            if retained_root:
                bounded = [retained_relative_path(candidate) for candidate in candidates]
                resolved = [candidate for candidate in bounded if candidate.exists()]
            else:
                resolved = [candidate.resolve(strict=True) for candidate in candidates if candidate.exists()]
            if not resolved:
                raise CapsuleFailure(f"capsule dependency cannot be resolved: {dependency}")
            if not retained_root:
                for candidate in resolved:
                    try:
                        candidate.relative_to(root.resolve(strict=True))
                    except ValueError as error:
                        raise CapsuleFailure("runtime dependency escapes capsule") from error


def prune_plugins(root: pathlib.Path) -> None:
    plugins = root / "BetterPullReview.app/Contents/PlugIns"
    if not plugins.is_dir() or plugins.is_symlink():
        raise CapsuleFailure("macdeployqt did not create the plugin directory")
    for path in sorted(plugins.rglob("*"), key=lambda value: len(value.parts), reverse=True):
        relative = pathlib.PurePosixPath(path.relative_to(plugins).as_posix())
        if path.is_symlink() or path.is_file():
            if relative not in ALLOWED_PLUGIN_FILES:
                path.unlink()
        elif path.is_dir() and not any(path.iterdir()):
            path.rmdir()
    absent = [relative for relative in ALLOWED_PLUGIN_FILES if not (plugins / relative).is_file()]
    if absent:
        raise CapsuleFailure("required Qt plugin is absent")


def write_exact_qt_conf(root: pathlib.Path) -> None:
    target = root / QT_CONF_RELATIVE
    target.parent.mkdir(mode=0o755, parents=True, exist_ok=True)
    temporary = target.parent / ".qt.conf.bpr-new"
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    try:
        written = 0
        while written != len(QT_CONF):
            count = os.write(descriptor, QT_CONF[written:])
            if count <= 0:
                raise CapsuleFailure("exact Qt policy write failed")
            written += count
        os.fsync(descriptor)
    finally:
        os.close(descriptor)
    os.replace(temporary, target)
    directory = os.open(target.parent, os.O_RDONLY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)


def mach_files(root: pathlib.Path) -> list[pathlib.Path]:
    return sorted(
        (path for path in root.rglob("*") if is_mach_o(path)),
        key=lambda value: value.as_posix(),
    )


def capsule_candidates(
    root: pathlib.Path,
    dependency: str,
) -> list[pathlib.Path]:
    frameworks = root / "BetterPullReview.app/Contents/Frameworks"
    suffix = dependency.removeprefix("@rpath/")
    result: list[pathlib.Path] = []
    for candidate in mach_files(frameworks):
        relative = candidate.relative_to(frameworks).as_posix()
        if relative == suffix or ("/" not in suffix and candidate.name == suffix):
            result.append(candidate)
    return result


def external_rpath_candidates(
    dependency: str,
    binary: pathlib.Path,
    app_executable: pathlib.Path,
    rpaths: list[str],
) -> list[pathlib.Path]:
    suffix = dependency.removeprefix("@rpath/")
    result: list[pathlib.Path] = []
    for rpath in rpaths:
        candidate = resolve_token_path(rpath, binary, app_executable) / suffix
        if candidate.exists():
            resolved = candidate.resolve(strict=True)
            if resolved not in result:
                result.append(resolved)
    return result


def copy_external_dependency(
    source: pathlib.Path,
    frameworks: pathlib.Path,
    copied: dict[pathlib.Path, pathlib.Path],
) -> pathlib.Path:
    source = source.resolve(strict=True)
    if source in copied:
        return copied[source]
    if ".framework" in source.parts:
        raise CapsuleFailure("macdeployqt omitted a required framework bundle")
    digest = sha256(source)
    destination = frameworks / f"{digest[:16]}-{source.name}"
    if destination.exists() or destination.is_symlink():
        if not destination.is_file() or sha256(destination) != digest:
            raise CapsuleFailure("dependency closure destination collides")
    else:
        shutil.copy2(source, destination)
        destination.chmod(stat.S_IMODE(destination.stat().st_mode) & ~0o022)
    copied[source] = destination
    return destination


def resolve_closure_dependency(
    root: pathlib.Path,
    binary: pathlib.Path,
    dependency: str,
    app_executable: pathlib.Path,
    app_rpaths: list[str],
    otool: pathlib.Path,
    copied: dict[pathlib.Path, pathlib.Path],
) -> pathlib.Path:
    root_real = root.resolve(strict=True)
    frameworks = root / "BetterPullReview.app/Contents/Frameworks"
    candidates: list[pathlib.Path] = []
    if dependency.startswith("@rpath/"):
        candidates.extend(capsule_candidates(root, dependency))
        if not candidates:
            candidates.extend(
                external_rpath_candidates(
                    dependency,
                    binary,
                    app_executable,
                    [*mach_rpaths(binary, otool), *app_rpaths],
                )
            )
    elif dependency.startswith(("@loader_path/", "@executable_path/")):
        candidate = resolve_token_path(dependency, binary, app_executable)
        if candidate.exists():
            candidates.append(candidate.resolve(strict=True))
    else:
        candidate = pathlib.Path(dependency)
        if candidate.is_absolute() and candidate.exists():
            candidates.append(candidate.resolve(strict=True))
    unique: list[pathlib.Path] = []
    for candidate in candidates:
        if candidate not in unique:
            unique.append(candidate)
    if len(unique) != 1:
        raise CapsuleFailure(f"dependency closure is missing or ambiguous: {dependency}")
    candidate = unique[0]
    try:
        candidate.relative_to(root_real)
        return candidate
    except ValueError:
        return copy_external_dependency(candidate, frameworks, copied)


def normalize_dependency_closure(
    root: pathlib.Path,
    otool: pathlib.Path,
    install_name_tool: pathlib.Path,
) -> None:
    app_executable = root / APP_RELATIVE
    frameworks = root / "BetterPullReview.app/Contents/Frameworks"
    app_rpaths = mach_rpaths(app_executable, otool)
    copied: dict[pathlib.Path, pathlib.Path] = {}
    edges: dict[tuple[pathlib.Path, str], pathlib.Path] = {}
    inspected: set[pathlib.Path] = set()
    while True:
        pending = [path for path in mach_files(root) if path not in inspected]
        if not pending:
            break
        for binary in pending:
            inspected.add(binary)
            for dependency in mach_dependencies(binary, otool):
                if dependency.startswith(("/System/Library/", "/usr/lib/")):
                    continue
                edges[(binary, dependency)] = resolve_closure_dependency(
                    root,
                    binary,
                    dependency,
                    app_executable,
                    app_rpaths,
                    otool,
                    copied,
                )

    for binary in mach_files(root):
        for rpath in mach_rpaths(binary, otool):
            removed = command([str(install_name_tool), "-delete_rpath", rpath, str(binary)])
            if removed.returncode != 0:
                raise CapsuleFailure("existing Mach-O RPATH could not be removed")
        relative_frameworks = os.path.relpath(frameworks, binary.parent)
        safe_rpath = "@loader_path" if relative_frameworks == "." else f"@loader_path/{relative_frameworks}"
        added = command([str(install_name_tool), "-add_rpath", safe_rpath, str(binary)])
        if added.returncode != 0:
            raise CapsuleFailure("capsule-relative Mach-O RPATH could not be installed")
        install_id = mach_install_id(binary, otool)
        if install_id is not None:
            new_id = f"@rpath/{binary.relative_to(frameworks).as_posix()}"
            changed = command([str(install_name_tool), "-id", new_id, str(binary)])
            if changed.returncode != 0:
                raise CapsuleFailure("capsule Mach-O install name could not be rewritten")
    for (binary, dependency), target in edges.items():
        rewritten = f"@rpath/{target.relative_to(frameworks).as_posix()}"
        changed = command(
            [str(install_name_tool), "-change", dependency, rewritten, str(binary)]
        )
        if changed.returncode != 0:
            raise CapsuleFailure("capsule dependency edge could not be rewritten")


def verify_signature(bundle: pathlib.Path, codesign: pathlib.Path) -> None:
    result = command([str(codesign), "--verify", "--deep", "--strict", "--verbose=2", str(bundle)])
    if result.returncode != 0:
        raise CapsuleFailure("capsule code signature is invalid")


def verify_inventory(
    root: pathlib.Path,
    inventory: pathlib.Path | None,
    *,
    inventory_text: str | None = None,
    expected_source_sha256: str | None = None,
    require_owner_private: bool = False,
    retained_root: bool = False,
) -> str:
    if (inventory is None) == (inventory_text is None):
        raise CapsuleFailure("exactly one inventory source is required")
    if inventory is not None:
        text = inventory.read_text(encoding="utf-8")
    else:
        assert inventory_text is not None
        text = inventory_text
    source_sha256, expected = parse_inventory(text)
    if expected_source_sha256 is not None and source_sha256 != expected_source_sha256:
        raise CapsuleFailure("capsule source identity differs from expected")
    actual = inventory_entries(root, retained_root=retained_root)
    if actual != expected:
        raise CapsuleFailure("capsule inventory differs from filesystem")
    if (root / QT_CONF_RELATIVE).read_bytes() != QT_CONF:
        raise CapsuleFailure("capsule Qt lookup policy is not exact")
    if not (root / APP_RELATIVE).is_file() or not os.access(root / APP_RELATIVE, os.X_OK):
        raise CapsuleFailure("capsule application is unavailable")
    if not (root / HELPER_RELATIVE).is_file() or not os.access(root / HELPER_RELATIVE, os.X_OK):
        raise CapsuleFailure("capsule supervisor is unavailable")
    if require_owner_private:
        status = root.stat()
        if status.st_uid != os.geteuid() or stat.S_IMODE(status.st_mode) != 0o700:
            raise CapsuleFailure("execution capsule root is not owner-private")
        for entry in actual:
            path = root / entry.relative
            if path.lstat().st_uid != os.geteuid():
                raise CapsuleFailure("execution capsule contains an alien-owned entry")
            if entry.kind != "symlink" and entry.mode & 0o022:
                raise CapsuleFailure("execution capsule contains a group/world-writable entry")
    return source_sha256


def sign_capsule(root: pathlib.Path, codesign: pathlib.Path) -> None:
    bundle = root / "BetterPullReview.app"
    nested = sorted(
        (path for path in bundle.rglob("*") if is_mach_o(path)),
        key=lambda path: len(path.parts),
        reverse=True,
    )
    for binary in nested:
        result = command(
            [str(codesign), "--force", "--sign", "-", "--timestamp=none", str(binary)]
        )
        if result.returncode != 0:
            raise CapsuleFailure("nested ad-hoc signing failed")
    result = command(
        [str(codesign), "--force", "--deep", "--sign", "-", "--timestamp=none", str(bundle)]
    )
    if result.returncode != 0:
        raise CapsuleFailure("outer capsule ad-hoc signing failed")


def build(arguments: argparse.Namespace) -> None:
    source_app = arguments.application.resolve(strict=True)
    supervisor = arguments.supervisor.resolve(strict=True)
    output = arguments.output.absolute()
    if output.exists() or output.is_symlink():
        raise CapsuleFailure("capsule output must not already exist")
    output.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(output.parent, 0o700)
    output.mkdir(mode=0o700)
    bundle = output / "BetterPullReview.app"
    shutil.copytree(source_app, bundle, symlinks=True)
    helper = output / HELPER_RELATIVE
    helper.parent.mkdir(mode=0o755, parents=True, exist_ok=True)
    shutil.copy2(supervisor, helper)
    verifier = output / VERIFIER_RELATIVE
    shutil.copy2(arguments.verifier.resolve(strict=True), verifier)
    verifier.chmod(0o644)
    deployed = command(
        [
            str(arguments.macdeployqt.resolve(strict=True)),
            str(bundle),
            "-always-overwrite",
            "-codesign=-",
            f"-executable={helper}",
        ]
    )
    if deployed.returncode != 0:
        raise CapsuleFailure("macdeployqt failed")
    write_exact_qt_conf(output)
    prune_plugins(output)
    normalize_dependency_closure(output, arguments.otool, arguments.install_name_tool)
    sign_capsule(output, arguments.codesign)
    verify_signature(bundle, arguments.codesign)
    verify_dependencies(output, arguments.otool)
    arguments.inventory.write_text(
        render_inventory(output, arguments.source_sha256), encoding="utf-8", newline="\n"
    )
    arguments.inventory.chmod(0o600)
    verify_inventory(output, arguments.inventory, expected_source_sha256=arguments.source_sha256)


def image_sha256(path: pathlib.Path) -> str:
    """The digest the image is bound by.

    fact_5 permits a raw image pathname hash as release evidence but forbids
    claiming it as an FD-atomic DiskImages attach identity. The operator packet
    records why: the tested /dev/fd/N path is not accepted by DiskImages, so
    there is no way to tie the bytes hashed here to the bytes attached later
    without a window. This is evidence about a file, and it is not more.
    """
    return sha256(path)


def attached_image_disks(image: pathlib.Path, hdiutil: pathlib.Path) -> list[str]:
    """The whole-disk devices of every attachment of exactly `image`.

    Matching this build's own resolved image path is the whole safety argument
    for detaching what comes back: an attachment of any other image, however
    alike, is never returned. A listing that cannot be read, or an attachment
    of this image with no whole disk to detach, fails closed, because neither
    shows that the image is not attached.
    """
    listing = command([str(hdiutil), "info", "-plist"])
    if listing.returncode != 0:
        raise CapsuleFailure(f"image attachment listing failed: {listing.stderr.strip()}")
    try:
        parsed = plistlib.loads(listing.stdout.encode("utf-8"))
    except Exception as error:
        raise CapsuleFailure("image attachment listing is unreadable") from error
    images = parsed.get("images", []) if isinstance(parsed, dict) else None
    if not isinstance(images, list):
        raise CapsuleFailure("image attachment listing is unreadable")
    target = os.path.realpath(image)
    disks: list[str] = []
    for attached in images:
        if not isinstance(attached, dict):
            raise CapsuleFailure("image attachment listing is unreadable")
        path = attached.get("image-path")
        if not isinstance(path, str) or os.path.realpath(path) != target:
            continue
        entities = attached.get("system-entities", [])
        whole = [
            entity["dev-entry"]
            for entity in (entities if isinstance(entities, list) else [])
            if isinstance(entity, dict)
            and isinstance(entity.get("dev-entry"), str)
            and re.fullmatch(r"/dev/disk[0-9]+", entity["dev-entry"])
        ]
        if not whole:
            raise CapsuleFailure("image is attached without a whole disk to detach")
        disks.extend(whole)
    return disks


def detach_created_image(image: pathlib.Path, hdiutil: pathlib.Path) -> None:
    """Leaves no attachment of the image this build just created.

    BPR-0836, after BPR-0814. While other image creates run on the machine,
    `hdiutil create` can exit 0 with its write-once device still attached, and
    the device stays attached; stage zero's later attach of the image then
    fails. The image bytes are already final. Each attempt re-lists and
    ordinarily detaches the next whole disk of exactly this image, because
    detaching the physical store removes the container synthesized on it but
    the listing does not say which disk is the store. Force is never used
    (fact_6): an attachment that outlasts the bound is a refusal, and no digest
    is reported for it.
    """
    deadline = time.monotonic() + IMAGE_DETACH_TIMEOUT_SECONDS
    attempt = 0
    reason = "no ordinary detach was attempted"
    while disks := attached_image_disks(image, hdiutil):
        if time.monotonic() >= deadline:
            raise CapsuleFailure(
                f"image is still attached after creation as {', '.join(disks)}: {reason}"
            )
        detached = command([str(hdiutil), "detach", disks[attempt % len(disks)], "-quiet"])
        reason = detached.stderr.strip() or "ordinary detach did not remove it"
        attempt += 1
        time.sleep(0.1)


def build_image(arguments: argparse.Namespace) -> None:
    """Stages the capsule and its inventory, then writes a read-only image."""
    capsule = arguments.capsule.resolve(strict=True)
    inventory = arguments.inventory.resolve(strict=True)
    if arguments.output.exists():
        raise CapsuleFailure(f"image already exists: {arguments.output}")
    verify_inventory(
        capsule,
        inventory,
        expected_source_sha256=arguments.source_sha256,
    )
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=arguments.output.parent) as staging:
        stage = pathlib.Path(staging) / "image"
        stage.mkdir(mode=0o700)
        shutil.copytree(capsule, stage / IMAGE_CAPSULE_RELATIVE, symlinks=True)
        shutil.copy2(inventory, stage / IMAGE_INVENTORY_RELATIVE)
        result = command(
            [
                str(arguments.hdiutil),
                "create",
                "-srcfolder",
                str(stage),
                "-format",
                "UDZO",
                "-volname",
                IMAGE_VOLUME_NAME,
                "-quiet",
                str(arguments.output),
            ]
        )
    try:
        detach_created_image(arguments.output, arguments.hdiutil)
    except (CapsuleFailure, OSError, ValueError) as refusal:
        # A failed create is the first cause; its detach refusal follows it.
        if result.returncode == 0:
            raise
        raise CapsuleFailure(
            f"image creation failed: {result.stderr.strip()}; {refusal}"
        ) from refusal
    if result.returncode != 0:
        raise CapsuleFailure(f"image creation failed: {result.stderr.strip()}")
    arguments.output.chmod(0o600)
    digest = image_sha256(arguments.output)
    if arguments.image_digest is not None:
        arguments.image_digest.write_text(f"{digest}\n", encoding="utf-8", newline="\n")
        arguments.image_digest.chmod(0o600)
    print(f"designated-live-macos-capsule:image:{digest}")


def owner_private_mount_root(parent: pathlib.Path) -> pathlib.Path:
    """A fresh mount root nobody else can reach into.

    Anchored on a directory descriptor rather than re-resolved by name, so the
    parent cannot be swapped between the check and the create.
    """
    if not parent.is_absolute() or pathlib.Path(os.path.normpath(parent)) != parent:
        raise CapsuleFailure("mount parent is not absolute and normalized")
    parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    if parent.resolve(strict=True) != parent:
        raise CapsuleFailure("mount parent is not resolved")
    parent_status = os.lstat(parent)
    if (
        not stat.S_ISDIR(parent_status.st_mode)
        or stat.S_ISLNK(parent_status.st_mode)
        or parent_status.st_uid != os.geteuid()
        or parent_status.st_mode & 0o077
    ):
        raise CapsuleFailure("mount parent is not owner-private")
    descriptor = os.open(parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        name = f"mount-{os.urandom(8).hex()}"
        os.mkdir(name, mode=0o700, dir_fd=descriptor)
        root = parent / name
        status = os.stat(root, follow_symlinks=False)
        if not stat.S_ISDIR(status.st_mode) or status.st_uid != os.geteuid():
            raise CapsuleFailure("mount root is not an owner-private directory")
        if status.st_mode & 0o077:
            raise CapsuleFailure("mount root is reachable by others")
        return root
    finally:
        os.close(descriptor)


def attach_read_only(
    image: pathlib.Path, root: pathlib.Path, hdiutil: pathlib.Path
) -> None:
    """Attaches beneath an existing owner-private root, read-only.

    fact_6: a writable mount fails closed. The flag asks for read-only and the
    check below establishes it, because asking is not the same as getting.
    """
    result = command(
        [
            str(hdiutil),
            "attach",
            "-readonly",
            "-nobrowse",
            "-noautoopen",
            "-mountpoint",
            str(root),
            "-quiet",
            str(image),
        ]
    )
    if result.returncode != 0:
        raise CapsuleFailure(f"attach failed: {result.stderr.strip()}")


def require_read_only_mount(root: pathlib.Path) -> None:
    if not os.statvfs(root).f_flag & os.ST_RDONLY:
        raise CapsuleFailure("attached capsule image is not a read-only mount")
    probe = root / f".bpr-write-probe-{os.urandom(6).hex()}"
    try:
        with probe.open("wb") as sink:
            sink.write(b"\0")
    except OSError:
        return
    # The write succeeded, so the mount is writable. Remove the evidence and
    # refuse: a writable capsule is the substitution this whole topology exists
    # to prevent.
    probe.unlink(missing_ok=True)
    raise CapsuleFailure("attached capsule image is writable")


def detach_without_force(root: pathlib.Path, hdiutil: pathlib.Path) -> None:
    """fact_6: a detach that requires force fails closed.

    Force is never attempted. If the plain detach does not succeed, something
    still holds the mount, and that is the condition worth failing on rather
    than papering over -- a forced detach would leave exactly the uncertainty
    about what was still reading that this lane cannot carry.
    """
    result = command([str(hdiutil), "detach", str(root), "-quiet"])
    if result.returncode != 0:
        raise CapsuleFailure(f"ordinary detach failed: {result.stderr.strip()}")


def capture_directory_identity(path: pathlib.Path, label: str) -> FilesystemIdentity:
    status = os.lstat(path)
    if stat.S_ISLNK(status.st_mode) or not stat.S_ISDIR(status.st_mode):
        raise CapsuleFailure(f"{label} is not a real directory")
    return filesystem_identity(status)


def capture_runtime_identity(root: pathlib.Path) -> RuntimeIdentity:
    capsule = root / IMAGE_CAPSULE_RELATIVE
    mount_descriptor = os.open(
        root, os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
    )
    capsule_descriptor = -1
    try:
        capsule_descriptor = os.open(
            capsule, os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
        )
        files: list[tuple[pathlib.PurePosixPath, FilesystemIdentity, str]] = []
        for relative in (
            IMAGE_INVENTORY_RELATIVE,
            IMAGE_CAPSULE_RELATIVE / APP_RELATIVE,
            IMAGE_CAPSULE_RELATIVE / HELPER_RELATIVE,
            IMAGE_CAPSULE_RELATIVE / VERIFIER_RELATIVE,
        ):
            path = root / relative
            status = os.lstat(path)
            if stat.S_ISLNK(status.st_mode) or not stat.S_ISREG(status.st_mode):
                raise CapsuleFailure(f"mounted runtime file is not regular: {relative}")
            files.append((relative, filesystem_identity(status), sha256(path)))
        return RuntimeIdentity(
            mount=filesystem_identity(os.fstat(mount_descriptor)),
            capsule=filesystem_identity(os.fstat(capsule_descriptor)),
            files=tuple(files),
            mount_descriptor=mount_descriptor,
            capsule_descriptor=capsule_descriptor,
        )
    except BaseException:
        if capsule_descriptor >= 0:
            os.close(capsule_descriptor)
        os.close(mount_descriptor)
        raise


def close_runtime_identity(identity: RuntimeIdentity) -> None:
    if identity.capsule_descriptor >= 0:
        os.close(identity.capsule_descriptor)
    if identity.mount_descriptor >= 0:
        os.close(identity.mount_descriptor)


def revalidate_runtime_identity(
    root: pathlib.Path,
    expected: RuntimeIdentity,
    source_sha256: str,
) -> None:
    if (
        expected.mount_descriptor < 0
        or expected.capsule_descriptor < 0
        or filesystem_identity(os.fstat(expected.mount_descriptor)) != expected.mount
        or filesystem_identity(os.fstat(expected.capsule_descriptor)) != expected.capsule
    ):
        raise CapsuleFailure("retained mounted runtime identity drifted")
    if capture_directory_identity(root, "mounted image root") != expected.mount:
        raise CapsuleFailure("mounted image identity drifted")
    if (
        capture_directory_identity(root / IMAGE_CAPSULE_RELATIVE, "mounted capsule")
        != expected.capsule
    ):
        raise CapsuleFailure("mounted capsule identity drifted")
    require_read_only_mount(root)
    for relative, identity, digest in expected.files:
        path = root / relative
        try:
            status = os.lstat(path)
        except OSError as error:
            raise CapsuleFailure(f"mounted runtime identity drifted: {relative}") from error
        if filesystem_identity(status) != identity or sha256(path) != digest:
            raise CapsuleFailure(f"mounted runtime identity drifted: {relative}")
    verify_inventory(
        root / IMAGE_CAPSULE_RELATIVE,
        root / IMAGE_INVENTORY_RELATIVE,
        expected_source_sha256=source_sha256,
    )


def require_underlying_mount_root(
    root: pathlib.Path, expected: FilesystemIdentity
) -> None:
    if capture_directory_identity(root, "underlying mount root") != expected:
        raise CapsuleFailure("underlying mount root did not return after detach")


def profile_id(value: str) -> str:
    if not value.isascii() or not value.isdecimal() or value.startswith("0") or int(value) > (1 << 64) - 1:
        raise argparse.ArgumentTypeError("profile must be a canonical positive uint64")
    return value


def approval_path(
    arguments: argparse.Namespace,
    mount_root: pathlib.Path,
) -> pathlib.Path | None:
    if getattr(arguments, "bpr_profile", None) is not None and not arguments.attended_launch:
        raise CapsuleFailure("profile selection requires attended launch")
    if arguments.attended_launch:
        if arguments.approval is None:
            raise CapsuleFailure("attended launch requires an external approval")
        approval = require_resolved_absolute(arguments.approval, "approval")
        status = os.lstat(approval)
        parent_status = os.lstat(approval.parent)
        if (
            stat.S_ISLNK(status.st_mode)
            or not stat.S_ISREG(status.st_mode)
            or status.st_uid != os.geteuid()
            or status.st_mode & 0o077
            or stat.S_ISLNK(parent_status.st_mode)
            or not stat.S_ISDIR(parent_status.st_mode)
            or parent_status.st_uid != os.geteuid()
            or parent_status.st_mode & 0o077
        ):
            raise CapsuleFailure("approval is not an external owner-private file")
        if approval.is_relative_to(mount_root):
            raise CapsuleFailure("approval must remain outside the mounted image")
        return approval
    if arguments.approval is not None:
        raise CapsuleFailure("probe launch cannot receive an approval")
    return None


def expected_image_digest(arguments: argparse.Namespace) -> str:
    if arguments.expected_image_sha256_file is None:
        expected = arguments.expected_image_sha256
    else:
        if not arguments.probe_launch:
            raise CapsuleFailure("only the build probe may read an image digest file")
        digest_path = require_resolved_absolute(
            arguments.expected_image_sha256_file, "image digest"
        )
        if digest_path.is_symlink() or not digest_path.is_file():
            raise CapsuleFailure("image digest is not a regular file")
        text = digest_path.read_text(encoding="utf-8")
        if not text.endswith("\n") or text.count("\n") != 1:
            raise CapsuleFailure("image digest file framing is invalid")
        expected = text.removesuffix("\n")
    if expected is None or len(expected) != 64 or any(
        character not in "0123456789abcdef" for character in expected
    ):
        raise CapsuleFailure("expected image SHA-256 is not canonical")
    return expected


def process_group_exists(process_group: int) -> bool:
    try:
        os.killpg(process_group, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def wait_for_process_group_exit(process_group: int, deadline: float) -> bool:
    while process_group_exists(process_group):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        time.sleep(min(0.05, remaining))
    return True


def signal_process_group(process_group: int, requested: signal.Signals) -> None:
    try:
        os.killpg(process_group, requested)
    except ProcessLookupError:
        return


def terminate_process_group(process: subprocess.Popen[bytes]) -> None:
    """Terminates and reaps the complete stage-one process group."""
    process_group = process.pid
    term_deadline = time.monotonic() + STAGE_ONE_TERM_GRACE_SECONDS
    signal_process_group(process_group, signal.SIGTERM)
    try:
        process.wait(timeout=max(0.0, term_deadline - time.monotonic()))
    except subprocess.TimeoutExpired:
        pass
    if wait_for_process_group_exit(process_group, term_deadline):
        return

    kill_deadline = time.monotonic() + STAGE_ONE_KILL_GRACE_SECONDS
    signal_process_group(process_group, signal.SIGKILL)
    try:
        process.wait(timeout=max(0.0, kill_deadline - time.monotonic()))
    except subprocess.TimeoutExpired as error:
        raise CapsuleFailure("stage-one process leader could not be reaped") from error
    if not wait_for_process_group_exit(process_group, kill_deadline):
        raise CapsuleFailure("stage-one process group survived SIGKILL")


def run_stage_one(
    helper: pathlib.Path,
    inventory: pathlib.Path,
    approval: pathlib.Path | None,
    signals: StageZeroSignalScope | None = None,
    profile: str | None = None,
) -> subprocess.CompletedProcess[bytes]:
    arguments = [str(helper)]
    if approval is None:
        arguments.extend(["--probe-launch", "--inventory", str(inventory)])
    else:
        arguments.extend(
            [
                "--attended-launch",
                "--inventory",
                str(inventory),
                "--approval",
                str(approval),
            ]
        )
    if profile is not None:
        if approval is None:
            raise CapsuleFailure("profile selection requires attended approval")
        arguments.extend(["--bpr-profile", profile_id(profile)])
    process: subprocess.Popen[bytes] | None = None
    spawn_scope = (
        signals.defer_interrupts()
        if signals is not None
        else contextlib.nullcontext()
    )
    try:
        with spawn_scope:
            process = subprocess.Popen(arguments, start_new_session=True)
        try:
            returncode = process.wait(timeout=STAGE_ONE_TIMEOUT_SECONDS)
        except subprocess.TimeoutExpired as error:
            if signals is not None:
                signals.begin_cleanup()
            terminate_process_group(process)
            raise CapsuleFailure("mounted supervisor exceeded its 1500-second deadline") from error
        if process_group_exists(process.pid):
            if signals is not None:
                signals.begin_cleanup()
            terminate_process_group(process)
            raise CapsuleFailure("mounted supervisor left a live process group")
        return subprocess.CompletedProcess(arguments, returncode)
    except BaseException:
        if process is not None and process_group_exists(process.pid):
            if signals is not None:
                signals.begin_cleanup()
            terminate_process_group(process)
        raise


def run_stage_zero(
    arguments: argparse.Namespace,
    signals: StageZeroSignalScope,
) -> None:
    """Runs the complete supervisor/application lane from one read-only image."""
    retained_image = retain_image(arguments.image)
    try:
        expected = expected_image_digest(arguments)
        if retained_image.digest != expected:
            raise CapsuleFailure(
                f"image digest drifted: expected {expected}, "
                f"found {retained_image.digest}"
            )
        root = owner_private_mount_root(arguments.mount_parent)
        underlying = capture_directory_identity(root, "underlying mount root")
        attached = False
        mounted: RuntimeIdentity | None = None
        try:
            with signals.defer_interrupts():
                attach_read_only(retained_image.path, root, arguments.hdiutil)
                attached = True
            retained_image = rebaseline_attached_image(retained_image)
            require_read_only_mount(root)
            mounted = capture_runtime_identity(root)
            if mounted.mount == underlying:
                raise CapsuleFailure("image was not mounted over the private root")
            capsule = root / IMAGE_CAPSULE_RELATIVE
            inventory = root / IMAGE_INVENTORY_RELATIVE
            verify_inventory(
                capsule,
                inventory,
                expected_source_sha256=arguments.source_sha256,
            )
            approval = approval_path(arguments, root)
            revalidate_image(retained_image)
            revalidate_runtime_identity(root, mounted, arguments.source_sha256)
            helper = root / IMAGE_CAPSULE_RELATIVE / HELPER_RELATIVE
            print(f"designated-live-macos-capsule:mounted:{root}", flush=True)
            interrupted_during_postcheck: StageZeroInterrupted | None = None
            try:
                profile = getattr(arguments, "bpr_profile", None)
                profile_arguments = {"profile": profile} if profile is not None else {}
                launched = run_stage_one(helper, inventory, approval, signals, **profile_arguments)
            finally:
                # Recheck every exit path, retrying if the handled termination
                # request arrived during this final identity observation.
                while True:
                    try:
                        revalidate_image(retained_image)
                        revalidate_runtime_identity(
                            root,
                            mounted,
                            arguments.source_sha256,
                        )
                        break
                    except StageZeroInterrupted as error:
                        interrupted_during_postcheck = error
            if interrupted_during_postcheck is not None:
                raise interrupted_during_postcheck
            if launched.returncode != 0:
                raise CapsuleFailure(
                    f"mounted supervisor exited with status {launched.returncode}"
                )
        finally:
            signals.begin_cleanup()
            if mounted is not None:
                close_runtime_identity(mounted)
            if attached:
                detach_without_force(root, arguments.hdiutil)
                require_underlying_mount_root(root, underlying)
            with contextlib.suppress(OSError):
                root.rmdir()
    finally:
        os.close(retained_image.descriptor)


def stage_zero(arguments: argparse.Namespace) -> None:
    """Runs the complete supervisor/application lane from one read-only image."""
    with scoped_stage_zero_signals() as signals:
        run_stage_zero(arguments, signals)


def verify(arguments: argparse.Namespace) -> None:
    retained_root = arguments.inventory_fd is not None
    if retained_root and arguments.capsule != pathlib.Path("."):
        raise CapsuleFailure("descriptor verification requires the retained capsule cwd")
    root = (
        arguments.capsule
        if retained_root
        else arguments.capsule.resolve(strict=True)
    )
    inventory = (
        arguments.inventory.resolve(strict=True)
        if arguments.inventory is not None
        else None
    )
    inventory_text = (
        read_inventory_descriptor(arguments.inventory_fd)
        if arguments.inventory_fd is not None
        else None
    )
    verify_inventory(
        root,
        inventory,
        inventory_text=inventory_text,
        expected_source_sha256=arguments.source_sha256,
        require_owner_private=arguments.require_owner_private,
        retained_root=retained_root,
    )
    verify_signature(root / "BetterPullReview.app", arguments.codesign)
    verify_dependencies(root, arguments.otool, retained_root=retained_root)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    subcommands = result.add_subparsers(dest="action", required=True)
    build_parser = subcommands.add_parser("build")
    build_parser.add_argument("--application", type=pathlib.Path, required=True)
    build_parser.add_argument("--supervisor", type=pathlib.Path, required=True)
    build_parser.add_argument("--verifier", type=pathlib.Path, required=True)
    build_parser.add_argument("--output", type=pathlib.Path, required=True)
    build_parser.add_argument("--inventory", type=pathlib.Path, required=True)
    build_parser.add_argument("--source-sha256", required=True)
    build_parser.add_argument("--macdeployqt", type=pathlib.Path, required=True)
    build_parser.add_argument("--codesign", type=pathlib.Path, default=pathlib.Path("/usr/bin/codesign"))
    build_parser.add_argument("--otool", type=pathlib.Path, default=pathlib.Path("/usr/bin/otool"))
    build_parser.add_argument(
        "--install-name-tool",
        type=pathlib.Path,
        default=pathlib.Path("/usr/bin/install_name_tool"),
    )
    verify_parser = subcommands.add_parser("verify")
    verify_parser.add_argument("--capsule", type=pathlib.Path, required=True)
    verify_inventory_source = verify_parser.add_mutually_exclusive_group(required=True)
    verify_inventory_source.add_argument("--inventory", type=pathlib.Path)
    verify_inventory_source.add_argument("--inventory-fd", type=int)
    verify_parser.add_argument("--source-sha256")
    verify_parser.add_argument("--require-owner-private", action="store_true")
    verify_parser.add_argument("--codesign", type=pathlib.Path, default=pathlib.Path("/usr/bin/codesign"))
    verify_parser.add_argument("--otool", type=pathlib.Path, default=pathlib.Path("/usr/bin/otool"))
    image_parser = subcommands.add_parser("image")
    image_parser.add_argument("--capsule", type=pathlib.Path, required=True)
    image_parser.add_argument("--inventory", type=pathlib.Path, required=True)
    image_parser.add_argument("--output", type=pathlib.Path, required=True)
    image_parser.add_argument("--source-sha256")
    image_parser.add_argument("--image-digest", type=pathlib.Path)
    image_parser.add_argument("--hdiutil", type=pathlib.Path, default=pathlib.Path("/usr/bin/hdiutil"))
    stage_zero_parser = subcommands.add_parser("stage-zero")
    stage_zero_parser.add_argument("--image", type=pathlib.Path, required=True)
    expected_image = stage_zero_parser.add_mutually_exclusive_group(required=True)
    expected_image.add_argument("--expected-image-sha256")
    expected_image.add_argument("--expected-image-sha256-file", type=pathlib.Path)
    stage_zero_parser.add_argument("--mount-parent", type=pathlib.Path, required=True)
    stage_zero_parser.add_argument("--source-sha256", required=True)
    stage_zero_mode = stage_zero_parser.add_mutually_exclusive_group(required=True)
    stage_zero_mode.add_argument("--probe-launch", action="store_true")
    stage_zero_mode.add_argument("--attended-launch", action="store_true")
    stage_zero_parser.add_argument("--approval", type=pathlib.Path)
    stage_zero_parser.add_argument("--bpr-profile", type=profile_id)
    stage_zero_parser.add_argument("--hdiutil", type=pathlib.Path, default=pathlib.Path("/usr/bin/hdiutil"))
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        if arguments.action == "build":
            build(arguments)
        elif arguments.action == "image":
            build_image(arguments)
        elif arguments.action == "stage-zero":
            stage_zero(arguments)
        else:
            verify(arguments)
    except (CapsuleFailure, OSError, ValueError) as error:
        print(f"designated-live-macos-capsule:fail:{error}", file=sys.stderr)
        return 1
    print("designated-live-macos-capsule:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
