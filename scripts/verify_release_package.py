#!/usr/bin/env python3
"""Audit an expanded release candidate; never build, sign, upload or publish it.

This binds the producer's inventory to actual bytes and checks the selected build
graph plus native symbols/dependencies. It is not signature or license approval.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path, PurePosixPath
import plistlib
import re
import subprocess
import sys

import verify_binary_dependencies as dependencies
import verify_composed_hosts as composition
import verify_rfc0016_test_inventory as device_inventory

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bpr_package_runtime", ROOT / "packaging/package_runtime.py")
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)

FORBIDDEN_BYTES = (*device_inventory.FORBIDDEN_STRINGS, "--test-", "--epoch-fixture-",
                   "github_application_fixture_access", "BPR_GITHUB_NATIVE_HTTPS_TEST",
                   "BPR_CANARY_SECRET_", "BPR_CANARY_SOURCE_", "BPR_CANARY_COMMENT_")
FORBIDDEN_SYMBOLS = (*device_inventory.FORBIDDEN_SYMBOLS, "github_application_fixture_access",
                     "make_native_https_transport_for_test")
BINARY_MAGICS = (b"MZ", b"\x7fELF", b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf",
                 b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca")


def require(value: bool, reason: str) -> None:
    if not value:
        raise ValueError(reason)


def package_paths(root: Path, info: dict) -> tuple[Path, list[Path]]:
    directory = root / "Contents/MacOS" if info["platform"] == "Darwin" else root
    app = directory / ("BetterPullReview" if info["platform"] == "Darwin" else "better_pull_review.exe" if info["platform"] == "Windows" else "better_pull_review")
    suffix = ".exe" if info["platform"] == "Windows" else ""
    helpers = [directory / (name + suffix) for name in ("better_pull_review_agent_supervisor", "better_pull_review_mcp_context_helper")]
    if info["platform"] == "Darwin":
        helpers.append(directory / "bpr_update_helper")
        identity = plistlib.loads((root / "Contents/Info.plist").read_bytes())
        require(identity.get("CFBundleIdentifier") == "com.gamecoretech.betterpullreview"
                and identity.get("CFBundleExecutable") == app.name
                and identity.get("CFBundleShortVersionString") == info["version"], "invalid-package-identity")
    for path in (app, *helpers):
        require(path.is_file() and not path.is_symlink(), f"required-packaged-capability-missing:{path.name}")
    plugins = root / "Contents/PlugIns" if info["platform"] == "Darwin" else root / "plugins"
    actual = {path.relative_to(plugins).as_posix() for path in plugins.rglob("*") if path.is_file()}
    require(actual == set(runtime.PLUGINS[info["platform"]]), "package-plugin-set-mismatch")
    return app, helpers


def check_file_inventory(root: Path, sbom: Path, revision: str, info: dict) -> dict:
    record = json.loads(sbom.read_text(encoding="utf-8"))
    product = [item for item in record["packages"] if item["SPDXID"] == "SPDXRef-BetterPullReview"]
    require(len(product) == 1 and product[0]["versionInfo"] == info["version"], "invalid-product-inventory")
    require(product[0]["sourceInfo"] == f"git:{revision}; clean=True; tests-disabled Release; {info['platform']}", "package-source-identity-mismatch")
    expected = {}
    for item in record["files"]:
        name = item["fileName"]
        require(name.startswith("./"), "invalid-inventory-path")
        relative = PurePosixPath(name[2:])
        require(not relative.is_absolute() and ".." not in relative.parts and "\\" not in name
                and relative.as_posix() == name[2:] and name not in expected, "invalid-inventory-path")
        hashes = [value["checksumValue"] for value in item["checksums"] if value["algorithm"] == "SHA256"]
        require(len(hashes) == 1 and re.fullmatch(r"[a-f0-9]{64}", hashes[0]) is not None, "invalid-inventory-digest")
        expected[name] = hashes[0]
    actual = {}
    for path in sorted(root.rglob("*")):
        require(path.resolve().is_relative_to(root), "package-link-escape")
        if path.is_file() and path.resolve() != sbom.resolve():
            actual["./" + path.relative_to(root).as_posix()] = runtime.digest(path)
    require(bool(actual) and actual.keys() == expected.keys(), "package-file-set-mismatch")
    require(actual == expected, "package-file-digest-mismatch")
    legal = runtime.legal_root(root, info)
    require((legal / "corresponding-source.zip").is_file(), "corresponding-source-missing")
    return {"file_count": len(actual), "sbom_sha256": runtime.digest(sbom)}


def check_dependency_inventory(sbom: Path, info: dict) -> None:
    packages = json.loads(sbom.read_text(encoding="utf-8"))["packages"]
    identities = [item["SPDXID"] for item in packages]
    require(len(identities) == len(set(identities)), "duplicate-dependency-identity")
    for dependency in runtime.components(info):
        selected = [item for item in packages if item["SPDXID"] == "SPDXRef-" + dependency["name"]]
        algorithm = "SHA256" if "sha256" in dependency else "SHA3-256"
        checksum = dependency.get("sha256", dependency.get("sha3_256"))
        require(len(selected) == 1 and selected[0].get("versionInfo") == dependency["version"]
                and selected[0].get("downloadLocation") == dependency["url"]
                and {"algorithm": algorithm, "checksumValue": checksum} in selected[0].get("checksums", []), "declared-dependency-inventory-mismatch")


def check_graph(build: Path, info: dict, revision: str) -> dict:
    # This also permits reusing an older artifact after verifier/documentation-only
    # commits. Product or build changes require a new artifact, never a new label.
    unchanged = subprocess.run(["git", "-C", str(ROOT), "diff", "--quiet", revision, "--",
                                "src", "cmake", "CMakeLists.txt", "CMakePresets.json", "packaging", "third_party", "tools/release.sh"], check=False)
    require(unchanged.returncode == 0, "package-build-source-drift")
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    regions = set()
    first_party = 0
    for item in commands:
        source = Path(item["file"]).resolve()
        if not source.is_relative_to(ROOT):
            continue
        relative = source.relative_to(ROOT)
        require(relative.parts[0] != "tests", "tests-present-in-package-graph")
        if relative.parts[0] != "src":
            continue
        command = item.get("command", " ".join(item.get("arguments", [])))
        require(not re.search(r"(?:-D|/D)BPR_(?:BUILD_TESTING|.*_TESTING)(?:[=\s]|$)", command), "fixture-definition-in-package-graph")
        if relative.parts[1] == "core":
            require(not re.search(r"Qt(?:Core|Gui|Widgets|Network|6)|/src/adapters/|/src/application/", command), "effect-dependency-in-core-graph")
        if source.name == "installed_runtime.cpp":
            require("BPR_DISTRIBUTION_BUILD" in command, "installed-runtime-not-selected")
            regions.add("installed-runtime")
        regions.add(relative.parts[1])
        first_party += 1
    require({"core", "application", "adapters", "app", "installed-runtime"}.issubset(regions), "incomplete-package-graph")
    return {"first_party_compile_units": first_party, "build_platform": info["platform"]}


def check_payload(payload: bytes, symbols: tuple[str, ...]) -> None:
    require(bool(symbols), "empty-symbol-evidence")
    table = "\n".join(symbols)
    require(not any(value in table for value in FORBIDDEN_SYMBOLS), "fixture-symbol-in-package")
    require(not any(value.encode() in payload for value in FORBIDDEN_BYTES), "fixture-or-canary-bytes-in-package")
    for pattern, _description, permitted in device_inventory.FORBIDDEN_PATTERNS:
        require(all(match.group() in permitted for match in re.finditer(pattern, payload.decode("latin-1"))), "secret-shaped-bytes-in-package")


def check_dependencies(binary: Path, root: Path, system: str) -> None:
    output = dependencies.dependency_output(binary)
    require(not dependencies.forbidden_dependencies(output, system), "undeclared-runtime-dependency")
    if system != "Darwin":
        # Native Linux deployment already records its ldd closure; PE imports do
        # not encode loaded origins. M8/M9 supply those target-native observations.
        return
    frameworks = root / "Contents/Frameworks"
    for reference in dependencies.dependency_references(output, system):
        if reference.startswith(("/usr/lib/", "/System/Library/")):
            continue
        base = frameworks if reference.startswith("@rpath/") else binary.parent if reference.startswith("@loader_path/") else root / "Contents/MacOS"
        require(reference.startswith(("@rpath/", "@loader_path/", "@executable_path/")), "untrusted-library-origin")
        resolved = (base / reference.split("/", 1)[1]).resolve()
        require(resolved.is_relative_to(root) and resolved.is_file(), "unresolved-packaged-library")
    load_commands = subprocess.check_output(["otool", "-l", str(binary)], text=True)
    for entry in load_commands.split("cmd LC_RPATH")[1:]:
        match = re.search(r"\n\s*path (.+) \(offset", entry)
        require(match is not None, "unreadable-library-search-path")
        origin = match.group(1)
        base = binary.parent if origin.startswith("@loader_path/") else root / "Contents/MacOS"
        require(origin.startswith(("@loader_path/", "@executable_path/"))
                and (base / origin.split("/", 1)[1]).resolve().is_relative_to(root), "untrusted-library-search-path")


def audit(build: Path, root: Path, sbom: Path, revision: str) -> dict:
    require(re.fullmatch(r"[a-f0-9]{40}", revision) is not None, "exact-source-revision-required")
    info = runtime.inputs(build)
    app, helpers = package_paths(root, info)
    evidence = check_file_inventory(root, sbom, revision, info)
    check_dependency_inventory(sbom, info)
    evidence.update(check_graph(build, info, revision))
    binaries = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open("rb") as stream:
            magic = stream.read(4)
        if not magic.startswith(BINARY_MAGICS):
            continue
        table = composition.inspect_symbols(path)
        check_payload(path.read_bytes(), table.symbols)
        check_dependencies(path, root, info["platform"])
        if path == app:
            require(not composition.audit_symbols(table.symbols, composition.PRODUCTION_PROFILE), "production-composition-anchor-missing")
        binaries.append(path.relative_to(root).as_posix())
    require(all(path.relative_to(root).as_posix() in binaries for path in (app, *helpers)), "required-capability-not-a-binary")
    return {**evidence, "source_revision": revision, "executable_sha256": runtime.digest(app),
            "binaries": binaries, "trust": "not-assessed", "license_approval": "pending"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--sbom", required=True, type=Path)
    parser.add_argument("--source-revision", required=True)
    args = parser.parse_args()
    try:
        evidence = audit(args.build.resolve(strict=True), args.root.resolve(strict=True), args.sbom.resolve(strict=True), args.source_revision)
    except (ValueError, OSError, KeyError, subprocess.SubprocessError, composition.verification_error) as error:
        print(f"RELEASE_PACKAGE_FAILED:{error}", file=sys.stderr)
        return 1
    print(json.dumps({"result": "RELEASE_PACKAGE_OK", **evidence}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
