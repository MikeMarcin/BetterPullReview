#!/usr/bin/env python3
"""Build and verify the exact macOS capsule for the attended live lane."""

from __future__ import annotations

import argparse
import hashlib
import os
import pathlib
import shutil
import stat
import subprocess
import sys
from dataclasses import dataclass


SCHEMA = "bpr.designated_live_macos_capsule"
VERSION = "1"
APP_RELATIVE = pathlib.PurePosixPath("BetterPullReview.app/Contents/MacOS/better_pull_review")
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
}


class CapsuleFailure(RuntimeError):
    """One fail-closed capsule contract violation."""


@dataclass(frozen=True)
class Entry:
    kind: str
    mode: int
    digest_or_target: str
    relative: str

    def render(self) -> str:
        return f"{self.kind}\t{self.mode:04o}\t{self.digest_or_target}\t{self.relative}"


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def relative_text(root: pathlib.Path, path: pathlib.Path) -> str:
    relative = path.relative_to(root).as_posix()
    if not relative or "\t" in relative or "\n" in relative or "\r" in relative:
        raise CapsuleFailure("capsule path is not inventory-safe")
    return relative


def inventory_entries(root: pathlib.Path) -> list[Entry]:
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


def mach_rpaths(path: pathlib.Path, otool: pathlib.Path) -> list[str]:
    result = command([str(otool), "-l", str(path)])
    if result.returncode != 0:
        raise CapsuleFailure("otool load-command inspection failed")
    lines = result.stdout.splitlines()
    rpaths: list[str] = []
    for index, line in enumerate(lines):
        if line.strip() == "cmd LC_RPATH" and index + 2 < len(lines):
            value = lines[index + 2].strip()
            if not value.startswith("path ") or " (offset " not in value:
                raise CapsuleFailure("LC_RPATH framing is invalid")
            rpaths.append(value[5 : value.index(" (offset ")])
    return rpaths


def mach_install_id(path: pathlib.Path, otool: pathlib.Path) -> str | None:
    result = command([str(otool), "-D", str(path)])
    if result.returncode != 0:
        raise CapsuleFailure("otool install-name inspection failed")
    lines = result.stdout.splitlines()
    if len(lines) == 1:
        return None
    if len(lines) != 2 or lines[0] != f"{path}:":
        raise CapsuleFailure("Mach-O install-name framing is invalid")
    return lines[1].strip()


def mach_dependencies(path: pathlib.Path, otool: pathlib.Path) -> list[str]:
    result = command([str(otool), "-L", str(path)])
    if result.returncode != 0:
        raise CapsuleFailure("otool dependency inspection failed")
    install_id = mach_install_id(path, otool)
    dependencies: list[str] = []
    for line in result.stdout.splitlines()[1:]:
        dependency = line.strip().split(" (compatibility version", 1)[0]
        if dependency and dependency != install_id:
            dependencies.append(dependency)
    return dependencies


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


def verify_dependencies(root: pathlib.Path, otool: pathlib.Path) -> None:
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
            resolved = [candidate.resolve(strict=True) for candidate in candidates if candidate.exists()]
            if not resolved:
                raise CapsuleFailure(f"capsule dependency cannot be resolved: {dependency}")
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
    inventory: pathlib.Path,
    *,
    expected_source_sha256: str | None = None,
    require_owner_private: bool = False,
) -> str:
    source_sha256, expected = parse_inventory(inventory.read_text(encoding="utf-8"))
    if expected_source_sha256 is not None and source_sha256 != expected_source_sha256:
        raise CapsuleFailure("capsule source identity differs from expected")
    actual = inventory_entries(root)
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


def verify(arguments: argparse.Namespace) -> None:
    root = arguments.capsule.resolve(strict=True)
    verify_inventory(
        root,
        arguments.inventory.resolve(strict=True),
        expected_source_sha256=arguments.source_sha256,
        require_owner_private=arguments.require_owner_private,
    )
    verify_signature(root / "BetterPullReview.app", arguments.codesign)
    verify_dependencies(root, arguments.otool)


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
    verify_parser.add_argument("--inventory", type=pathlib.Path, required=True)
    verify_parser.add_argument("--source-sha256")
    verify_parser.add_argument("--require-owner-private", action="store_true")
    verify_parser.add_argument("--codesign", type=pathlib.Path, default=pathlib.Path("/usr/bin/codesign"))
    verify_parser.add_argument("--otool", type=pathlib.Path, default=pathlib.Path("/usr/bin/otool"))
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        if arguments.action == "build":
            build(arguments)
        else:
            verify(arguments)
    except (CapsuleFailure, OSError, ValueError) as error:
        print(f"designated-live-macos-capsule:fail:{error}", file=sys.stderr)
        return 1
    print("designated-live-macos-capsule:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
