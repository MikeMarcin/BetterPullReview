#!/usr/bin/env python3
"""Audit an expanded release candidate; never build, sign, upload or publish it.

This binds the producer's inventory to actual bytes and checks the selected build
graph plus native symbols/dependencies. It is not signature or license approval.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

import verify_binary_dependencies as dependencies
import verify_composed_hosts as composition
import verify_rfc0016_test_inventory as device_inventory
import verify_pe_release as pe_release

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bpr_package_runtime", ROOT / "packaging/package_runtime.py")
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)

FORBIDDEN_BYTES = (*device_inventory.FORBIDDEN_STRINGS, "--test-", "--epoch-fixture-",
                   "github_application_fixture_access", "BPR_GITHUB_NATIVE_HTTPS_TEST", "BPR_MIMALLOC_PROBE",
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
    executable_names = {"Darwin": "BetterPullReview", "Windows": "better_pull_review.exe",
                        "Linux": "better_pull_review"}
    app = directory / executable_names[info["platform"]]
    suffix = ".exe" if info["platform"] == "Windows" else ""
    # [[BPR-1179]]. Every package carries the supervisor Git's guarded launches
    # run inside; only a package built with agent integrations carries the MCP
    # context helper, and one built without them must not.
    families = runtime.families(info)
    helpers = [directory / ("better_pull_review_agent_supervisor" + suffix)]
    mcp_helper = directory / ("better_pull_review_mcp_context_helper" + suffix)
    if families["agent_integrations"]:
        helpers.append(mcp_helper)
    else:
        require(not mcp_helper.exists() and not mcp_helper.is_symlink(), f"omitted-family-file-present:{mcp_helper.name}")
    if info["platform"] == "Darwin":
        helpers.append(directory / "bpr_update_helper")
        identity = plistlib.loads((root / "Contents/Info.plist").read_bytes())
        require(identity.get("CFBundleIdentifier") == "com.gamecoretech.betterpullreview"
                and identity.get("CFBundleExecutable") == app.name
                and identity.get("CFBundleShortVersionString") == info["version"], "invalid-package-identity")
    for path in (app, *helpers):
        require(path.is_file() and not path.is_symlink(), f"required-packaged-capability-missing:{path.name}")
    # [[BPR-0974]]. Declared runtime data the image refuses to start without.
    for name in runtime.RUNTIME_DATA[info["platform"]]:
        path = directory / name
        require(path.is_file() and not path.is_symlink(), f"required-packaged-capability-missing:{name}")
    for path in private_libraries(root, info):
        require(path.is_file() and not path.is_symlink(), f"required-packaged-capability-missing:{path.name}")
    plugins = root / "Contents/PlugIns" if info["platform"] == "Darwin" else root / "plugins"
    actual = {path.relative_to(plugins).as_posix() for path in plugins.rglob("*") if path.is_file()}
    require(actual == set(runtime.PLUGINS[info["platform"]]), "package-plugin-set-mismatch")
    return app, helpers


def private_libraries(root: Path, info: dict) -> list[Path]:
    """[[BPR-1485]]. The package's own shared libraries (Git's LGPL xdiff, as
    its own replaceable library), where the application loads them."""
    return [root / name for name in runtime.PRIVATE_LIBRARIES[info["platform"]]]


def check_file_inventory(root: Path, sbom: Path, revision: str, info: dict) -> dict:
    record = json.loads(sbom.read_text(encoding="utf-8"))
    installed_inventory = root / "sbom.spdx.json" if info["platform"] != "Darwin" else None
    if installed_inventory is not None:
        require(installed_inventory.is_file() and not installed_inventory.is_symlink()
                and runtime.digest(installed_inventory) == runtime.digest(sbom), "installed-inventory-mismatch")
    product = [item for item in record["packages"] if item["SPDXID"] == "SPDXRef-BetterPullReview"]
    require(len(product) == 1 and product[0]["versionInfo"] == info["version"], "invalid-product-inventory")
    require(product[0]["sourceInfo"] == runtime.product_source_info(info, revision, True), "package-source-identity-mismatch")
    expected = {}
    for item in record["files"]:
        name = item["fileName"]
        require(name.startswith("./"), "invalid-inventory-path")
        require(name != "./sbom.spdx.json", "recursive-inventory-entry")
        relative = PurePosixPath(name[2:])
        require(not relative.is_absolute() and ".." not in relative.parts and "\\" not in name
                and relative.as_posix() == name[2:] and name not in expected, "invalid-inventory-path")
        hashes = [value["checksumValue"] for value in item["checksums"] if value["algorithm"] == "SHA256"]
        require(len(hashes) == 1 and re.fullmatch(r"[a-f0-9]{64}", hashes[0]) is not None, "invalid-inventory-digest")
        expected[name] = hashes[0]
    actual = {}
    for path in sorted(root.rglob("*")):
        require(path.resolve().is_relative_to(root), "package-link-escape")
        if path.is_file() and path.resolve() != sbom.resolve() and path != installed_inventory:
            actual["./" + path.relative_to(root).as_posix()] = runtime.digest(path)
    require(bool(actual) and actual.keys() == expected.keys(), "package-file-set-mismatch")
    require(actual == expected, "package-file-digest-mismatch")
    runtime.installed_source_provenance(root, info, revision)
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
            regions.add("installed-cli-runtime" if "BPR_INSTALLED_TOUR_CLI" in command else "installed-runtime")
        regions.add(relative.parts[1])
        first_party += 1
    required = {"core", "application", "adapters", "app",
                "installed-cli-runtime" if info.get("layout") == "tour-cli" else "installed-runtime"}
    require(required.issubset(regions), "incomplete-package-graph")
    return {"first_party_compile_units": first_party, "build_platform": info["platform"]}


def check_payload(payload: bytes, symbols: tuple[str, ...]) -> None:
    require(bool(symbols), "empty-symbol-evidence")
    table = "\n".join(symbols)
    require(not any(value in table for value in FORBIDDEN_SYMBOLS), "fixture-symbol-in-package")
    check_data_payload(payload)


def check_data_payload(payload: bytes) -> None:
    # The byte controls a binary gets, for declared runtime data: the Windows
    # resource bundle holds what the image embedded before BPR-0974.
    require(not any(value.encode() in payload for value in FORBIDDEN_BYTES), "fixture-or-canary-bytes-in-package")
    for pattern, _description, permitted in device_inventory.FORBIDDEN_PATTERNS:
        require(all(match.group() in permitted for match in re.finditer(pattern, payload.decode("latin-1"))), "secret-shaped-bytes-in-package")


def check_dependencies(binary: Path, root: Path, system: str, *, tour_cli: bool = False) -> None:
    output = dependencies.dependency_output(binary)
    require(not dependencies.forbidden_dependencies(output, system), "undeclared-runtime-dependency")
    if system != "Darwin":
        # Native Linux deployment already records its ldd closure; PE imports do
        # not encode loaded origins. M8/M9 supply those target-native observations.
        return
    frameworks = root / ("lib" if tour_cli else "Contents/Frameworks")
    executable_directory = root / ("bin" if tour_cli else "Contents/MacOS")
    for reference in dependencies.dependency_references(output, system):
        if reference.startswith(("/usr/lib/", "/System/Library/")):
            continue
        if reference.startswith("@rpath/"):
            base = frameworks
        elif reference.startswith("@loader_path/"):
            base = binary.parent
        else:
            base = executable_directory
        require(reference.startswith(("@rpath/", "@loader_path/", "@executable_path/")), "untrusted-library-origin")
        resolved = (base / reference.split("/", 1)[1]).resolve()
        require(resolved.is_relative_to(root) and resolved.is_file(), "unresolved-packaged-library")
    load_commands = subprocess.check_output(["otool", "-l", str(binary)], text=True)
    for entry in load_commands.split("cmd LC_RPATH")[1:]:
        match = re.search(r"\n\s*path (.+) \(offset", entry)
        require(match is not None, "unreadable-library-search-path")
        origin = match.group(1)
        base = binary.parent if origin.startswith("@loader_path/") else executable_directory
        require(origin.startswith(("@loader_path/", "@executable_path/"))
                and (base / origin.split("/", 1)[1]).resolve().is_relative_to(root), "untrusted-library-search-path")


def linux_qt_symbol_evidence(binary: Path, root: Path, info: dict) -> tuple[composition.symbol_table, dict] | None:
    """Inspect dynamic definitions only for an exact relocated upstream Qt image.

    This is an explicit producer class, never a fallback after failed full-symbol
    inspection. Source-built images retain the ordinary full-symbol requirement.
    """
    if info["platform"] != "Linux":
        return None
    relative = binary.relative_to(root).as_posix()
    library_root = root
    if info.get("layout") == "tour-cli":
        library_root = root / "lib"
        if relative.startswith("lib/bpr/tls/"):
            relative = "plugins/tls/" + binary.name
        elif relative.startswith("lib/"):
            relative = relative[4:]
    library = re.fullmatch(r"libQt6([A-Za-z0-9]+)\.so\.6", relative)
    if library is not None and library[1] in runtime.QT_LIBRARIES:
        producer_relative = "lib/" + relative
    elif relative in {"plugins/" + name for name in runtime.PLUGINS["Linux"]}:
        producer_relative = relative
    else:
        return None
    qt = Path(info["qt_root"]).resolve(strict=True)
    producer = (qt / producer_relative).resolve(strict=True)
    require(producer.is_file() and producer.is_relative_to(qt), "qt-symbol-producer-origin-mismatch")
    upstream = runtime.qt_upstream_records(info, {binary.name})
    records = [item for item in upstream["files"]
               if item.get("fileName") == "./" + producer.relative_to(qt).as_posix()
               and "BINARY" in item.get("fileTypes", [])]
    require(len(records) == 1, "qt-symbol-producer-record-mismatch")
    checksums = [item for item in records[0].get("checksums", []) if item.get("algorithm") in {"SHA1", "SHA256"}]
    require(bool(checksums) and all(runtime.digest(producer, item["algorithm"].lower()) == item["checksumValue"]
                                   for item in checksums), "qt-symbol-producer-digest-mismatch")
    with tempfile.TemporaryDirectory(prefix="bpr-qt-symbol-relocation-") as directory:
        relocated = Path(directory) / binary.name
        shutil.copy2(producer, relocated)
        origin = os.path.relpath(library_root, binary.parent)
        runtime.run("patchelf", "--set-rpath", "$ORIGIN" if origin == "." else "$ORIGIN/" + origin, relocated)
        require(runtime.digest(relocated) == runtime.digest(binary), "qt-symbol-relocated-digest-mismatch")
    contents = subprocess.check_output(["nm", "-D", "--demangle", str(binary)], text=True)
    symbols = composition.parse_nm_defined_symbols(contents)
    return composition.symbol_table(symbols, "nm -D --demangle (exact upstream Qt relocation)"), {
        "producer": str(producer), "producer_sha256": runtime.digest(producer),
        "upstream_file": records[0]["SPDXID"], "upstream_checksums": checksums,
        "deployed_sha256": runtime.digest(binary), "defined_dynamic_symbols": len(symbols)}


def audit(build: Path, root: Path, sbom: Path, revision: str, *, windows_redist_root: Path | None = None,
          source_archive: Path | None = None, windows_import_library: Path | None = None,
          windows_user32_import_library: Path | None = None) -> dict:
    require(re.fullmatch(r"[a-f0-9]{40}", revision) is not None, "exact-source-revision-required")
    info = runtime.inputs(build)
    app, helpers = package_paths(root, info)
    evidence = check_file_inventory(root, sbom, revision, info)
    if windows_import_library is not None:
        require(info["platform"] == "Windows", "sdk-import-library-requires-windows")
        evidence["windows_import_metadata_specimen"] = pe_release.import_metadata_specimen(windows_import_library)
    if windows_user32_import_library is not None:
        require(info["platform"] == "Windows", "sdk-import-library-requires-windows")
        evidence["windows_user32_import_metadata_specimen"] = pe_release.user32_import_metadata_specimen(windows_user32_import_library)
    source = runtime.installed_source_provenance(root, info, revision)
    runtime.verify_source_asset(source_archive or sbom.parent / source["asset"]["name"], source)
    evidence["source_asset"] = source["asset"]
    check_dependency_inventory(sbom, info)
    evidence.update(check_graph(build, info, revision))
    if info["platform"] == "Linux":
        evidence["qt_companions"] = runtime.verify_linux_qt_companions(build, info, root)
        closure = json.loads((root / "runtime-closure.json").read_text(encoding="utf-8"))
        require(closure.get("qtCompanions") == evidence["qt_companions"], "qt-companion-provenance-drift")
        installed_icu = json.loads((runtime.legal_root(root, info) / "icu-runtime-provenance.json").read_text(encoding="utf-8"))
        require(installed_icu == runtime.linux_icu_producers(build, info)[1], "icu-source-build-provenance-drift")
    binaries = []
    symbol_evidence = {}
    qt_symbol_evidence = {}
    private_producers = tuple(runtime.private_library_producers(info)) if info["platform"] == "Windows" else ()
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open("rb") as stream:
            magic = stream.read(4)
        if not magic.startswith(BINARY_MAGICS):
            continue
        qt_symbols = linux_qt_symbol_evidence(path, root, info)
        if qt_symbols is not None:
            table, qt_symbol_evidence[path.relative_to(root).as_posix()] = qt_symbols
        else:
            table = (pe_release.inspect(path, info, windows_redist_root, private_producers=private_producers,
                                        windows_import_library=windows_import_library, windows_user32_import_library=windows_user32_import_library) if info["platform"] == "Windows"
                     else composition.inspect_symbols(path))
        check_payload(path.read_bytes(), table.symbols)
        check_dependencies(path, root, info["platform"])
        if path == app:
            require(not composition.audit_symbols(table.symbols, composition.PRODUCTION_PROFILE, **runtime.families(info)),
                    "production-composition-anchor-mismatch")
        binaries.append(path.relative_to(root).as_posix())
        symbol_evidence[path.relative_to(root).as_posix()] = table.inspector
    require(all(path.relative_to(root).as_posix() in binaries for path in (app, *helpers, *private_libraries(root, info))),
            "required-capability-not-a-binary")
    data = {}
    for name in runtime.RUNTIME_DATA[info["platform"]]:
        path = app.parent / name
        payload = path.read_bytes()
        require(not payload.startswith(BINARY_MAGICS), "declared-runtime-data-is-a-binary")
        check_data_payload(payload)
        data[path.relative_to(root).as_posix()] = runtime.digest(path)
    return {**evidence, "source_revision": revision, "families": runtime.families(info), "executable_sha256": runtime.digest(app),
            "binaries": binaries, "runtime_data": data, "symbol_evidence": symbol_evidence,
            "prebuilt_qt_symbol_evidence": qt_symbol_evidence,
            "trust": "not-assessed", "license_approval": "pending"}



def audit_tour_cli(build: Path, root: Path, sbom: Path, revision: str, *, source_archive: Path | None = None,
                   windows_redist_root: Path | None = None, windows_import_library: Path | None = None,
                   windows_user32_import_library: Path | None = None) -> dict:
    """Audit the selected standalone CLI, retaining the ordinary source/byte guards."""
    require(re.fullmatch(r"[a-f0-9]{40}", revision) is not None, "exact-source-revision-required")
    info = runtime.inputs(build, tour_cli=True)
    suffix = ".exe" if info["platform"] == "Windows" else ""
    required = [root / "bin" / (name + suffix) for name in ("bpr", "better_pull_review_agent_supervisor")]
    required += [root / name for name in info["private_libraries"]]
    for path in required:
        require(path.is_file() and not path.is_symlink(), f"required-packaged-capability-missing:{path.name}")
    evidence = check_file_inventory(root, sbom, revision, info)
    if windows_import_library is not None:
        require(info["platform"] == "Windows", "sdk-import-library-requires-windows")
        evidence["windows_import_metadata_specimen"] = pe_release.import_metadata_specimen(windows_import_library)
    if windows_user32_import_library is not None:
        require(info["platform"] == "Windows", "sdk-import-library-requires-windows")
        evidence["windows_user32_import_metadata_specimen"] = pe_release.user32_import_metadata_specimen(windows_user32_import_library)
    check_dependency_inventory(sbom, info)
    evidence.update(check_graph(build, info, revision))
    source = runtime.installed_source_provenance(root, info, revision)
    runtime.verify_source_asset(source_archive or sbom.parent / source["asset"]["name"], source)
    if info["platform"] == "Linux":
        evidence["qt_companions"] = runtime.verify_linux_qt_companions(build, info, root / "lib")
    binaries = []
    producers = tuple(runtime.private_library_producers(info)) if info["platform"] == "Windows" else ()
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open("rb") as stream:
            magic = stream.read(4)
        if not magic.startswith(BINARY_MAGICS):
            continue
        qt_symbols = linux_qt_symbol_evidence(path, root, info)
        if qt_symbols:
            table = qt_symbols[0]
        elif info["platform"] == "Windows":
            table = pe_release.inspect(path, info, windows_redist_root, private_producers=producers,
                                       windows_import_library=windows_import_library, windows_user32_import_library=windows_user32_import_library)
        else:
            table = composition.inspect_symbols(path)
        check_payload(path.read_bytes(), table.symbols)
        check_dependencies(path, root, info["platform"], tour_cli=True)
        binaries.append(path.relative_to(root).as_posix())
    require(all(path.relative_to(root).as_posix() in binaries for path in required), "required-capability-not-a-binary")
    return {**evidence, "source_revision": revision, "source_asset": source["asset"], "binaries": binaries,
            "trust": "not-assessed", "license_approval": "pending"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tour-cli", action="store_true", help="audit the separately prepared standalone CLI")
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--sbom", required=True, type=Path)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--windows-redist-root", type=Path)
    parser.add_argument("--windows-import-library", type=Path,
                        help="explicit reviewed SDK bcrypt.lib metadata specimen; not linker-selection proof")
    parser.add_argument("--windows-user32-import-library", type=Path, help="explicit reviewed SDK User32.lib metadata specimen")
    parser.add_argument("--source-archive", type=Path, help="defaults to the versioned asset beside the SPDX")
    args = parser.parse_args()
    try:
        if args.tour_cli:
            evidence = audit_tour_cli(args.build.resolve(strict=True), args.root.resolve(strict=True), args.sbom.resolve(strict=True), args.source_revision, source_archive=args.source_archive,
                                      windows_redist_root=args.windows_redist_root, windows_import_library=args.windows_import_library, windows_user32_import_library=args.windows_user32_import_library)
        else:
            evidence = audit(args.build.resolve(strict=True), args.root.resolve(strict=True), args.sbom.resolve(strict=True), args.source_revision,
                         windows_redist_root=args.windows_redist_root, source_archive=args.source_archive,
                         windows_import_library=args.windows_import_library, windows_user32_import_library=args.windows_user32_import_library)
    except (ValueError, OSError, KeyError, zipfile.BadZipFile, subprocess.SubprocessError, composition.verification_error) as error:
        print(f"RELEASE_PACKAGE_FAILED:{error}", file=sys.stderr)
        return 1
    print(json.dumps({"result": "RELEASE_PACKAGE_OK", **evidence}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
