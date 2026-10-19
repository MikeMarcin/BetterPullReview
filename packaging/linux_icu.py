#!/usr/bin/env python3
"""Build the exact ICU 73.2 shared runtime with explicit native hardening.

This helper writes only a fresh, caller-selected build root. It uses the pinned
source archive, never modifies Qt's provider, and runs no application or tests.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

VERSION = "73.2"
FILES = {f"libicu{part}.so.73": f"libicu{part}.so.73.2" for part in ("data", "uc", "i18n")}
FLAGS = "-O2 -DNDEBUG -fPIC -fstack-protector-strong -fno-omit-frame-pointer -D_FORTIFY_SOURCE=3"
LINK_FLAGS = "-Wl,-z,relro,-z,now,-z,noexecstack"
BUILD_ENVIRONMENT = {"PATH": "/usr/bin:/bin", "LC_ALL": "C", "CONFIG_SITE": "/dev/null", "CONFIG_SHELL": "/bin/sh"}
OPTIONS = ["--enable-shared", "--disable-static", "--disable-tests", "--disable-samples",
           "--disable-extras", "--disable-icuio", "--with-data-packaging=library"]


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def tree_digest(root: Path) -> str:
    result = hashlib.sha256()
    for path in sorted(root.rglob("*"), key=lambda value: value.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            value = "link:" + os.readlink(path)
        elif path.is_file():
            value = digest(path)
        else:
            continue
        result.update((relative + "\0" + value + "\n").encode())
    return result.hexdigest()



def archive_tree_digest(archive: Path) -> str:
    entries = {}
    with tarfile.open(archive) as stream:
        for member in stream.getmembers():
            if member.isdir():
                continue
            if not member.name.startswith("icu/"):
                raise ValueError("ICU source archive root mismatch")
            relative = member.name.removeprefix("icu/")
            if member.issym():
                value = "link:" + member.linkname
            else:
                content = stream.extractfile(member)
                if content is None:
                    raise ValueError("unexpected ICU source member")
                value = hashlib.file_digest(content, "sha256").hexdigest()
            if relative in entries:
                raise ValueError("duplicate ICU source member")
            entries[relative] = value
    result = hashlib.sha256()
    for relative, value in sorted(entries.items()):
        result.update((relative + "\0" + value + "\n").encode())
    return result.hexdigest()


def binding(component: dict, compilers: dict[str, Path]) -> dict:
    if component["version"] != VERSION:
        raise ValueError("unsupported ICU source version")
    tools = {}
    for name, path in compilers.items():
        if name not in {"CC", "CXX"} or not path.is_absolute():
            raise ValueError("ICU needs the configured native C/C++ compilers")
        resolved = path.resolve(strict=True)
        tools[name] = {"path": str(resolved), "sha256": digest(resolved),
                       "version": subprocess.check_output([str(resolved), "--version"], text=True).splitlines()[0]}
    if set(tools) != {"CC", "CXX"}:
        raise ValueError("ICU needs both configured native compilers")
    make = Path(shutil.which("make") or "/missing-make").resolve(strict=True)
    return {"make": {"path": str(make), "sha256": digest(make),
                      "version": subprocess.check_output([str(make), "--version"], text=True).splitlines()[0]},
            "version": VERSION, "sourceSHA256": component["sha256"], "recipeSHA256": digest(Path(__file__)),
            "compilers": tools, "CFLAGS": FLAGS, "CXXFLAGS": FLAGS, "LDFLAGS": LINK_FLAGS,
            "configureOptions": OPTIONS, "environment": BUILD_ENVIRONMENT}


def verify(root: Path, component: dict, compilers: dict[str, Path]) -> dict:
    receipt = json.loads((root / "receipt.json").read_text(encoding="utf-8"))
    if receipt.get("schema") != "bpr.linux-icu-source-build.v1" or receipt.get("binding") != binding(component, compilers):
        raise ValueError("ICU producer binding mismatch")
    archive = root / "source.tar.gz"
    if digest(archive) != component["sha256"]:
        raise ValueError("ICU cached source archive digest mismatch")
    expected_source = archive_tree_digest(archive)
    if tree_digest(root / "icu") != expected_source or receipt.get("sourceTreeSHA256") != expected_source:
        raise ValueError("ICU extracted source drift")
    if set(receipt.get("outputs", {})) != set(FILES):
        raise ValueError("ICU producer inventory mismatch")
    for name, filename in FILES.items():
        producer = root / "prefix/lib" / filename
        if (producer.is_symlink() or not producer.resolve(strict=True).is_relative_to(root.resolve(strict=True))
                or digest(producer) != receipt["outputs"][name]):
            raise ValueError("ICU source-built producer digest mismatch: " + name)
    return receipt


def prepare(root: Path, archive: Path, component: dict, compilers: dict[str, Path]) -> dict:
    expected = binding(component, compilers)
    if digest(archive) != component["sha256"]:
        raise ValueError("ICU source archive digest mismatch")
    if root.exists():
        return verify(root, component, compilers)
    root.mkdir(parents=True)
    build = root / "build"
    build.mkdir()
    shutil.copy2(archive, root / "source.tar.gz")
    with tarfile.open(archive) as stream:
        stream.extractall(root, filter="data")
    source_checksum = archive_tree_digest(archive)
    if tree_digest(root / "icu") != source_checksum:
        raise ValueError("ICU extracted source differs from pinned archive")
    environment = dict(BUILD_ENVIRONMENT)
    environment.update({name: record["path"] for name, record in expected["compilers"].items()})
    environment.update(CFLAGS=FLAGS, CXXFLAGS=FLAGS, LDFLAGS=LINK_FLAGS)
    commands = [[str(root / "icu/source/configure"), "--prefix=" + str(root / "prefix"), *OPTIONS],
                [expected["make"]["path"], "-j8"], [expected["make"]["path"], "install"]]
    for index, command in enumerate(commands):
        with (root / f"step-{index + 1}.log").open("w") as output:
            subprocess.run(command, cwd=build, env=environment, stdout=output, stderr=subprocess.STDOUT, check=True)
    if tree_digest(root / "icu") != source_checksum:
        raise ValueError("ICU build changed extracted source")
    receipt = {"schema": "bpr.linux-icu-source-build.v1", "binding": expected,
               "sourceTreeSHA256": source_checksum, "commands": commands,
               "outputs": {name: digest(root / "prefix/lib" / filename) for name, filename in FILES.items()}}
    (root / "receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return verify(root, component, compilers)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--cc", type=Path, required=True)
    parser.add_argument("--cxx", type=Path, required=True)
    args = parser.parse_args()
    component = {"version": VERSION, "sha256": args.sha256}
    print(json.dumps(prepare(args.build.resolve(), args.archive.resolve(strict=True), component,
                             {"CC": args.cc, "CXX": args.cxx}), sort_keys=True))


if __name__ == "__main__":
    main()
