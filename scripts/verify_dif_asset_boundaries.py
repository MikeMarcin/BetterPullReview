#!/usr/bin/env python3

"""Verify RFC-BPR-0019's offline Dif asset and selected-build boundary."""

from __future__ import annotations

import argparse
import ast
import hashlib
import importlib.util
import json
import sys
from pathlib import Path
from typing import Any


class BoundaryError(RuntimeError):
    """A direct RFC-BPR-0019 boundary check failed."""


PROFILE_EXPECTATIONS = {
    "dev": {"BUILD_TESTING": "ON", "BPR_BUILD_QT": "ON"},
    "core-only": {"BUILD_TESTING": "ON", "BPR_BUILD_QT": "OFF"},
    "app-release": {"BUILD_TESTING": "OFF", "BPR_BUILD_QT": "ON"},
}
FORBIDDEN_GRAPH_MARKERS = (
    "src/adapters/qt/assets/dif",
    "scripts/dif_assets.py",
    "dif-sprite-manifest.json",
    "dif-sprite-catalog.hpp",
    "dif-calm-idle.png",
    "dif-greet.png",
    "dif-rest.png",
    ":/bpr/dif/",
    "imagemagick",
    "imagegen",
    "openai",
    "pillow",
)
FORBIDDEN_RUNTIME_MARKERS = (
    b"src/adapters/qt/assets/dif",
    b"dif-sprite-manifest.json",
    b"dif-sprite-catalog.hpp",
    b"dif-calm-idle.png",
    b"dif-greet.png",
    b"dif-rest.png",
    b":/bpr/dif/",
    b"80ee8d9924bafe179d3b9b1172e8776254813ec1b62eab413bb31efa730a4f00",
)
FORBIDDEN_ASSET_TEXT = (
    b"/Users/",
    b"/home/",
    b"C:\\Users\\",
    b"BEGIN PRIVATE KEY",
    b"github_pat_",
    b"ghp_",
    b"http://",
    b"https://",
    b"file://",
    b"javascript:",
    b"<script",
)
ALLOWED_TOOL_IMPORTS = {
    "__future__",
    "argparse",
    "dataclasses",
    "hashlib",
    "json",
    "math",
    "pathlib",
    "struct",
    "sys",
    "tempfile",
    "typing",
    "zlib",
}
RUNTIME_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".cmake", ".qrc", ".rc"}


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def _load_json(path: Path, context: str) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise BoundaryError(f"{context} is unreadable or invalid JSON") from error


def _load_cache(build: Path) -> dict[str, str]:
    path = build / "CMakeCache.txt"
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeDecodeError) as error:
        raise BoundaryError("selected build has no readable CMakeCache.txt") from error
    values: dict[str, str] = {}
    for line in lines:
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        declaration, value = line.split("=", 1)
        key = declaration.split(":", 1)[0]
        values[key] = value
    return values


def _relative_first_party_sources(
    source: Path, commands: list[dict[str, Any]]
) -> list[str]:
    result: list[str] = []
    for index, entry in enumerate(commands):
        if not isinstance(entry, dict) or not isinstance(entry.get("file"), str):
            raise BoundaryError(f"compile_commands entry {index} has no file")
        candidate = Path(entry["file"])
        if not candidate.is_absolute():
            directory = Path(str(entry.get("directory", "")))
            candidate = directory / candidate
        try:
            relative = candidate.resolve().relative_to(source)
        except ValueError:
            continue
        result.append(relative.as_posix())
    return sorted(set(result))


def _read_selected_graph(build: Path) -> tuple[list[dict[str, Any]], str]:
    commands_path = build / "compile_commands.json"
    commands = _load_json(commands_path, "selected compile_commands.json")
    if not isinstance(commands, list) or not commands:
        raise BoundaryError("selected compile_commands.json must be a non-empty array")
    manifest_path = build / "build.ninja"
    try:
        manifest = manifest_path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise BoundaryError("selected build has no readable build.ninja") from error
    selected = manifest + "\n" + json.dumps(commands, sort_keys=True)
    return commands, selected


def _validate_ctest_registration(build: Path) -> None:
    path = build / "tests/CTestTestfile.cmake"
    try:
        content = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise BoundaryError(
            "tests-enabled graph has no tests/CTestTestfile.cmake"
        ) from error
    if content.count('add_test("dif_assets_contract"') != 1:
        raise BoundaryError("dif_assets_contract must be registered exactly once")
    for marker in (
        "run_network_isolated_test.py",
        "dif_asset_tests.py",
        '"--cxx"',
        'LABELS "bpr_network_isolated"',
    ):
        if marker not in content:
            raise BoundaryError(f"dif_assets_contract registration lacks {marker}")


def _validate_core_ctest_surface(build: Path) -> None:
    path = build / "tests/CTestTestfile.cmake"
    try:
        content = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise BoundaryError(
            "core-only graph has no readable tests/CTestTestfile.cmake"
        ) from error
    if "test_network_isolation_application" in content:
        raise BoundaryError(
            "core-only registered the unavailable Qt application probe"
        )


def _discover_application(build: Path) -> Path:
    candidates: list[Path] = []
    for name in ("better_pull_review", "better_pull_review.exe"):
        for path in build.rglob(name):
            if path.is_file() and "CMakeFiles" not in path.parts:
                candidates.append(path)
    unique = sorted(set(path.resolve() for path in candidates))
    if len(unique) != 1:
        raise BoundaryError(
            "selected application graph must contain one application binary, "
            f"found {len(unique)}"
        )
    return unique[0]


def _validate_runtime_markers(path: Path, context: str) -> str:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise BoundaryError(f"{context} is unreadable") from error
    lowered = data.lower()
    for marker in FORBIDDEN_RUNTIME_MARKERS:
        if marker.lower() in lowered:
            raise BoundaryError(
                f"{context} embeds RFC-BPR-0019 runtime marker {marker.decode('ascii')}"
            )
    return hashlib.sha256(data).hexdigest()


def _validate_core_artifacts(build: Path) -> dict[str, Any]:
    selected_roots = (
        build / "src/core",
        build / "src/application",
        build / "src/adapters",
    )
    artifacts: list[Path] = []
    for root in selected_roots:
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if not path.is_file() or "CMakeFiles" in path.parts:
                continue
            if path.suffix.lower() in {".a", ".lib", ".so", ".dylib", ".dll"}:
                artifacts.append(path)
    if not artifacts:
        raise BoundaryError("core-only graph has no built first-party library artifact")
    digests = [
        _validate_runtime_markers(path, "core-only library artifact")
        for path in sorted(artifacts)
    ]
    digest = hashlib.sha256("\n".join(digests).encode("ascii")).hexdigest()
    return {"artifact_count": len(artifacts), "combined_sha256": digest}


def validate_profile(source: Path, build: Path, profile: str) -> dict[str, Any]:
    source = source.resolve()
    build = build.resolve()
    expected = PROFILE_EXPECTATIONS.get(profile)
    if expected is None:
        raise BoundaryError(f"unknown selected profile: {profile}")
    cache = _load_cache(build)
    for key, value in expected.items():
        if cache.get(key) != value:
            raise BoundaryError(
                f"{profile} must select {key}={value}, got {cache.get(key, 'unset')}"
            )
    commands, selected_graph = _read_selected_graph(build)
    lowered_graph = selected_graph.lower().replace("\\", "/")
    for marker in FORBIDDEN_GRAPH_MARKERS:
        if marker.lower() in lowered_graph:
            raise BoundaryError(
                f"{profile} selected graph contains forbidden art/runtime "
                f"marker {marker}"
            )
    first_party = _relative_first_party_sources(source, commands)
    if not first_party:
        raise BoundaryError(f"{profile} selected no first-party source")

    if profile == "core-only":
        leaked = [
            path
            for path in first_party
            if path.startswith(("src/adapters/qt/", "src/app/"))
        ]
        if leaked:
            raise BoundaryError(f"core-only selected Qt/app source: {leaked[0]}")
        if "qt6::" in lowered_graph or "/qt6/" in lowered_graph:
            raise BoundaryError("core-only selected a Qt6 build dependency")
        _validate_core_ctest_surface(build)

    if profile == "app-release":
        leaked_tests = [path for path in first_party if path.startswith("tests/")]
        if leaked_tests:
            raise BoundaryError(
                f"tests-disabled application selected test source: {leaked_tests[0]}"
            )
        if "bpr_build_testing=1" in lowered_graph:
            raise BoundaryError("tests-disabled application selected BPR_BUILD_TESTING")
        if list(build.rglob("CTestTestfile.cmake")):
            raise BoundaryError(
                "tests-disabled application generated CTest registration"
            )
    else:
        _validate_ctest_registration(build)

    result: dict[str, Any] = {
        "build_testing": expected["BUILD_TESTING"],
        "bpr_build_qt": expected["BPR_BUILD_QT"],
        "first_party_translation_units": len(first_party),
    }
    if profile in {"dev", "app-release"}:
        result["application_sha256"] = _validate_runtime_markers(
            _discover_application(build), "selected application binary"
        )
    elif profile == "core-only":
        result["core_artifacts"] = _validate_core_artifacts(build)
    return result


def validate_offline_tool(path: Path) -> dict[str, Any]:
    try:
        source = path.read_text(encoding="utf-8")
        tree = ast.parse(source, filename=path.name)
    except (OSError, UnicodeDecodeError, SyntaxError) as error:
        raise BoundaryError("Dif asset tool is unreadable or invalid Python") from error
    imports: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            imports.update(alias.name.split(".", 1)[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module:
            imports.add(node.module.split(".", 1)[0])
    unexpected = sorted(imports - ALLOWED_TOOL_IMPORTS)
    if unexpected:
        raise BoundaryError(
            f"Dif asset tool imports unapproved capability: {unexpected[0]}"
        )
    lowered = source.lower()
    for marker in ("http://", "https://", "openai", "imagegen", "subprocess"):
        if marker in lowered:
            raise BoundaryError(
                f"Dif asset tool contains forbidden active capability: {marker}"
            )
    return {"imports": sorted(imports), "sha256": _sha256_file(path)}


def _runtime_files(source: Path) -> list[Path]:
    roots = [source / "CMakeLists.txt", source / "src", source / "packaging"]
    files: list[Path] = []
    for root in roots:
        if root.is_file():
            files.append(root)
            continue
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            if "assets" in path.parts and "dif" in path.parts:
                continue
            if path.name == "CMakeLists.txt" or path.suffix.lower() in RUNTIME_SUFFIXES:
                files.append(path)
    return sorted(set(files))


def validate_runtime_source_absence(source: Path) -> dict[str, Any]:
    source = source.resolve()
    checked = 0
    for path in _runtime_files(source):
        try:
            content = path.read_text(encoding="utf-8").lower().replace("\\", "/")
        except (OSError, UnicodeDecodeError) as error:
            raise BoundaryError(
                "runtime/package boundary source is unreadable"
            ) from error
        for marker in FORBIDDEN_GRAPH_MARKERS[:7]:
            if marker.lower() in content:
                relative = path.relative_to(source).as_posix()
                raise BoundaryError(
                    "runtime/package source selects RFC-BPR-0019 asset marker "
                    f"in {relative}"
                )
        checked += 1
    return {"checked_files": checked}


def _load_dif_module(source: Path) -> Any:
    script = source / "scripts/dif_assets.py"
    specification = importlib.util.spec_from_file_location("bpr_dif_assets", script)
    if specification is None or specification.loader is None:
        raise BoundaryError("could not load the Dif asset validator")
    module = importlib.util.module_from_spec(specification)
    sys.modules[specification.name] = module
    specification.loader.exec_module(module)
    return module


def validate_asset_file_hygiene(root: Path) -> dict[str, Any]:
    files = sorted(path for path in root.rglob("*") if path.is_file())
    pngs = [path for path in files if path.suffix.lower() == ".png"]
    for path in files:
        payload = path.read_bytes()
        for marker in FORBIDDEN_ASSET_TEXT:
            if marker.lower() in payload.lower():
                relative = path.relative_to(root).as_posix()
                raise BoundaryError(
                    f"asset file {relative} contains private/active marker: "
                    f"{marker.decode('ascii')}"
                )
    readme = (root / "README.md").read_text(encoding="utf-8")
    for marker in (
        "The checked-in PNGs, not a prompt replay, are the source of truth.",
        "The exact prompt was:",
    ):
        if marker not in readme:
            raise BoundaryError(f"Dif README lacks prompt disposition: {marker}")
    return {
        "checked_files": len(files),
        "png_count": len(pngs),
        "prompt_recorded_as_provenance_only": True,
    }


def validate_asset_tree(source: Path) -> dict[str, Any]:
    source = source.resolve()
    root = source / "src/adapters/qt/assets/dif"
    module = _load_dif_module(source)
    try:
        module.validate_assets(root)
    except Exception as error:
        if error.__class__.__name__ == "AssetError":
            raise BoundaryError(f"Dif asset validation failed: {error}") from error
        raise
    hygiene = validate_asset_file_hygiene(root)
    provenance = _load_json(root / "provenance.json", "Dif provenance")
    rights = provenance.get("rights") if isinstance(provenance, dict) else None
    if not isinstance(rights, dict):
        raise BoundaryError("Dif provenance has no rights record")
    if rights.get("notice_required") is not False:
        raise BoundaryError("Dif provenance does not record notice_required=false")
    notice = rights.get("notice_disposition")
    if not isinstance(notice, str) or not notice.strip():
        raise BoundaryError("Dif provenance has no notice disposition")
    return {
        "manifest_sha256": _sha256_file(root / "dif-sprite-manifest.json"),
        "notice_required": False,
        **hygiene,
        "provenance_sha256": _sha256_file(root / "provenance.json"),
    }


def verify_boundaries(
    source: Path, dev_build: Path, core_build: Path, app_build: Path
) -> dict[str, Any]:
    source = source.resolve()
    result = {
        "schema_name": "bpr.dif_asset_boundary_verification",
        "schema_version": 1,
        "asset_tree": validate_asset_tree(source),
        "offline_tool": validate_offline_tool(source / "scripts/dif_assets.py"),
        "runtime_source_absence": validate_runtime_source_absence(source),
        "selected_profiles": {
            "app-release": validate_profile(source, app_build, "app-release"),
            "core-only": validate_profile(source, core_build, "core-only"),
            "dev": validate_profile(source, dev_build, "dev"),
        },
        "limitations": [
            "Visual originality, GitHub-mark absence, and foreign-character "
            "exclusion remain attended decisions.",
            "This verifies development and tests-disabled application graphs, "
            "not a signed, notarized, installable, or certified package.",
        ],
    }
    return result


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--dev-build", type=Path, required=True)
    parser.add_argument("--core-build", type=Path, required=True)
    parser.add_argument("--app-build", type=Path, required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        result = verify_boundaries(
            arguments.source,
            arguments.dev_build,
            arguments.core_build,
            arguments.app_build,
        )
    except BoundaryError as error:
        print(f"DIF_ASSET_BOUNDARIES_ERROR: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    print("DIF_ASSET_BOUNDARIES_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
