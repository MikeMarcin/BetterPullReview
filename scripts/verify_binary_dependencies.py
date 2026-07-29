#!/usr/bin/env python3
from __future__ import annotations

import platform
import shutil
import subprocess
import sys
from pathlib import Path


FORBIDDEN = (
    "qtwebengine",
    "webkit",
    "chromium",
    "electron",
    "tauri",
    "node.dll",
    "libnode",
    "cef",
    "httpserver",
)


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


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_binary_dependencies.py <executable>", file=sys.stderr)
        return 2

    executable = Path(sys.argv[1]).resolve()
    if not executable.is_file():
        print(f"missing executable: {executable}", file=sys.stderr)
        return 2

    output = dependency_output(executable)
    lowered = output.lower()
    selected = [token for token in FORBIDDEN if token in lowered]
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
