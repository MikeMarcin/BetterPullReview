#!/usr/bin/env python3
from __future__ import annotations

import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path


FORBIDDEN = (
    "qtwebengine",
    "qt5webengine",
    "qt6webengine",
    "webkit",
    "chromium",
    "electron",
    "tauri",
    "node.dll",
    "libnode",
    "cef",
    "httpserver",
)

WINDOWS_DLL_PATTERN = re.compile(
    r"(?i)(?:[a-z]:)?[a-z0-9_./\\:+-]*[a-z0-9_.+-]+\.dll"
)
LINUX_ADDRESS_SUFFIX = re.compile(r"\s+\(0x[0-9a-fA-F]+\)\s*$")


def windows_dependency_command(executable: Path) -> list[str]:
    candidates = (
        ("dumpbin", ["/dependents", str(executable)]),
        ("llvm-readobj", ["--coff-imports", str(executable)]),
        ("objdump", ["-p", str(executable)]),
    )
    for tool, arguments in candidates:
        resolved = shutil.which(tool)
        if resolved is not None:
            return [resolved, *arguments]

    raise RuntimeError(
        "dumpbin, llvm-readobj, or objdump is required on Windows"
    )


def dependency_output(executable: Path) -> str:
    system = platform.system()
    if system == "Darwin":
        command = ["otool", "-L", str(executable)]
    elif system == "Linux":
        command = ["ldd", str(executable)]
    elif system == "Windows":
        command = windows_dependency_command(executable)
    else:
        raise RuntimeError(f"unsupported dependency-inspection platform: {system}")

    completed = subprocess.run(
        command,
        check=True,
        capture_output=True,
        text=True,
        timeout=10,
    )
    return completed.stdout + completed.stderr


def dependency_references(output: str, system: str) -> tuple[str, ...]:
    references: list[str] = []
    if system == "Linux":
        for raw_line in output.splitlines():
            line = raw_line.strip()
            if not line:
                continue
            if "=>" in line:
                name, resolved = (part.strip() for part in line.split("=>", 1))
                references.append(name)
                resolved = LINUX_ADDRESS_SUFFIX.sub("", resolved)
                if resolved != "not found":
                    references.append(resolved)
                continue

            reference = LINUX_ADDRESS_SUFFIX.sub("", line)
            if reference and reference not in {
                "not a dynamic executable",
                "statically linked",
            }:
                references.append(reference)
    elif system == "Darwin":
        for raw_line in output.splitlines():
            line = raw_line.strip()
            if not line or line.endswith(":"):
                continue
            reference = line.split(" (", 1)[0]
            if reference:
                references.append(reference)
    elif system == "Windows":
        references.extend(WINDOWS_DLL_PATTERN.findall(output))
    else:
        raise RuntimeError(
            f"unsupported dependency-inspection platform: {system}"
        )

    return tuple(references)


def forbidden_dependencies(output: str, system: str) -> tuple[str, ...]:
    dependencies = "\n".join(dependency_references(output, system)).lower()
    return tuple(token for token in FORBIDDEN if token in dependencies)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_binary_dependencies.py <executable>", file=sys.stderr)
        return 2

    executable = Path(sys.argv[1]).resolve()
    if not executable.is_file():
        print(f"missing executable: {executable}", file=sys.stderr)
        return 2

    system = platform.system()
    output = dependency_output(executable)
    selected = forbidden_dependencies(output, system)
    if selected:
        print(
            "forbidden linked runtime dependencies: " + ", ".join(selected),
            file=sys.stderr,
        )
        return 1

    print("BINARY_DEPENDENCIES_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
