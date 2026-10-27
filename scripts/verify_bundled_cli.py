#!/usr/bin/env python3
"""Exercise a macOS app's headless CLI at its installed and relocated paths.

Uses only disposable profiles and noninteractive authentication refusals. Native
sign-in, Developer ID, notarization and human acceptance remain separate.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import verify_binary_dependencies as dependencies


GUI_IMAGES = ("QtGui.framework/", "QtWidgets.framework/", "QtWebEngine", "QtQuick", "/platforms/", "libqcocoa", "libqoffscreen")


def require(value: bool, message: str) -> None:
    if not value:
        raise ValueError(message)


def headless_closure(app: Path, binary: Path) -> list[str]:
    pending, checked = [binary], set()
    while pending:
        image = pending.pop().resolve(strict=True)
        if image in checked:
            continue
        require(image.is_relative_to(app), "CLI dependency escapes the app")
        checked.add(image)
        output = dependencies.dependency_output(image)
        require(not any(name in output for name in GUI_IMAGES), "headless CLI links a GUI/QPA image")
        for reference in dependencies.dependency_references(output, "Darwin"):
            if reference.startswith(("/usr/lib/", "/System/Library/")):
                continue
            bases = {"@rpath": app / "Contents/Frameworks", "@loader_path": image.parent,
                     "@executable_path": binary.parent}
            prefix, _, relative = reference.partition("/")
            require(prefix in bases, "CLI dependency has an external origin")
            pending.append(bases[prefix] / relative)
    return sorted(str(path.relative_to(app)) for path in checked)


def probe(app: Path, root: Path) -> dict:
    binary = app / "Contents/MacOS/bpr"
    require(binary.is_file() and not binary.is_symlink() and os.access(binary, os.X_OK), "bundled bpr missing or not executable")
    closure = headless_closure(app, binary)
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("DYLD_", "QT_", "BPR_")) and key != "DISPLAY"}
    environment.update({"PATH": "/usr/bin:/bin", "QT_QPA_PLATFORM": "bpr-no-display", "DYLD_PRINT_LIBRARIES": "1"})
    profile = root / "data"
    observations = []
    def run(arguments: list[str], status: int, code: str | None = None) -> dict:
        result = subprocess.run([str(binary), "tour", *arguments, "--json", "--data-root", str(profile)],
                                stdin=subprocess.DEVNULL, capture_output=True, text=True, cwd=root,
                                env=environment, timeout=30)
        require(result.returncode == status, f"{arguments[0]} exit {result.returncode}: {result.stderr[-1000:]}")
        require(not any(image in result.stderr for image in GUI_IMAGES), "headless CLI loaded a GUI/QPA image")
        value = json.loads(result.stdout)
        require(value.get("schema") == "bpr.tour.cli-result.v1", "unexpected CLI envelope")
        if code is not None:
            require(value.get("error", {}).get("code") == code, f"{arguments[0]} did not refuse {code}")
        observations.append({"command": arguments[0], "exit": result.returncode, "error": code})
        return value
    capabilities = run(["capabilities"], 0)["data"]
    require(capabilities.get("cli_contract") == "1" and "assist" in capabilities, "bundled CLI contract unavailable")
    run(["open", "https://github.com/MikeMarcin/BPR-Test/pull/1", "--offline"], 2, "target_ambiguous")
    registry = profile / "state/v1/profile-registry.sqlite3"
    require(registry.is_file(), "first-run profile registry absent")
    require(profile.stat().st_mode & 0o077 == 0 and registry.stat().st_mode & 0o077 == 0, "first-run profile permissions are not owner-private")
    run(["connect", "--host", "github.com", "--transport", "native"], 3, "interaction_required")
    run(["open", "https://github.com/MikeMarcin/BPR-Test/pull/1"], 5, "authentication_required")
    return {"binary": str(binary), "platform": capabilities.get("platform"), "gui_images_loaded": False, "headless_dependency_closure": closure,
            "first_run_profile": "owner-private", "observations": observations}


def verify(app: Path) -> dict:
    with tempfile.TemporaryDirectory(prefix="bpr-bundled-cli-") as temporary:
        root = Path(temporary)
        original = root / "original-profile"
        original.mkdir(mode=0o700)
        installed = probe(app, original)
        relocated = root / "Relocated app ü" / app.name
        shutil.copytree(app, relocated, symlinks=True)
        second = root / "relocated-profile"
        second.mkdir(mode=0o700)
        moved = probe(relocated, second)
        return {"result": "BUNDLED_CLI_OK", "installed": installed, "relocated": moved,
                "native_authentication": "human sign-in pending for these bytes", "trust": "not assessed"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        evidence = verify(arguments.app.resolve(strict=True))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"BUNDLED_CLI_FAILED:{error}")
        return 1
    print(json.dumps(evidence, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
