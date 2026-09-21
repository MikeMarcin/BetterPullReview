#!/usr/bin/env python3
"""Native, tests-disabled runtime deployment and exact-artifact evidence.

No signing, credential discovery, upload or publication. macOS deployment stays
in tools/release.sh; this module supplies its shared source/notices/SBOM steps.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
QT_LIBRARIES = {"Core", "Gui", "Widgets", "Network", "DBus", "OpenGL", "XcbQpa"}
PLUGINS = {
    "Darwin": ["platforms/libqcocoa.dylib", "styles/libqmacstyle.dylib",
               "networkinformation/libqapplenetworkinformation.dylib", "tls/libqsecuretransportbackend.dylib"],
    "Windows": ["platforms/qwindows.dll", "styles/qmodernwindowsstyle.dll", "tls/qschannelbackend.dll"],
    "Linux": ["platforms/libqxcb.so", "tls/libqopensslbackend.so"],
}
CRT = re.compile(r"(?:msvcp140(?:_1|_2|_atomic_wait|_codecvt_ids)?|vcruntime140(?:_1|_threads)?|concrt140)\.dll", re.I)


def run(*arguments: str | Path, env: dict | None = None) -> str:
    return subprocess.check_output([str(arg) for arg in arguments], text=True, stderr=subprocess.STDOUT, env=env)


def digest(path: Path, algorithm: str = "sha256") -> str:
    result = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def write_json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def inputs(build: Path) -> dict:
    value = json.loads((build / "_bpr/package-inputs.json").read_text(encoding="utf-8"))
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    for setting in ("BUILD_TESTING:BOOL=OFF", "BPR_DISTRIBUTION_BUILD:BOOL=ON", "CMAKE_BUILD_TYPE:STRING=Release"):
        if setting not in cache.splitlines():
            raise ValueError(f"package requires {setting}")
    if value["schema"] != "bpr.package-inputs.v1" or Path(value["source"]).resolve() != ROOT:
        raise ValueError("package build belongs to a different source tree")
    if value["platform"] != platform.system() or value["qt_version"] != "6.10.3":
        raise ValueError("native platform and pinned Qt version must match package inputs")
    return value


def components(info: dict) -> list[dict]:
    entries = json.loads((ROOT / "packaging/dependencies.json").read_text(encoding="utf-8"))["components"]
    return [item for item in entries if info["platform"] in item.get("platforms", [info["platform"]])]


def source_archives(build: Path, info: dict) -> dict[str, Path]:
    directory = build / "release-sources"
    directory.mkdir(exist_ok=True)
    result = {}
    for item in components(info):
        algorithm = "sha256" if "sha256" in item else "sha3_256"
        extension = ".zip" if item["url"].endswith(".zip") else ".tar.xz" if item["url"].endswith(".xz") else ".tar.gz"
        path = directory / f"{item['name']}-{item['version']}{extension}"
        if not path.exists():
            descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".download", dir=directory)
            os.close(descriptor)
            temporary = Path(temporary_name)
            try:
                with urllib.request.urlopen(item["url"], timeout=60) as response, temporary.open("wb") as output:
                    if not response.url.startswith("https://"):
                        raise ValueError("source redirect left HTTPS")
                    shutil.copyfileobj(response, output)
                if digest(temporary, algorithm) != item[algorithm]:
                    raise ValueError(f"source digest mismatch: {item['name']}")
                temporary.rename(path)
            finally:
                temporary.unlink(missing_ok=True)
        if digest(path, algorithm) != item[algorithm]:
            raise ValueError(f"cached source digest mismatch: {item['name']}")
        result[item["name"]] = path
    return result


def legal_root(root: Path, info: dict) -> Path:
    return root / "Contents/Resources/ThirdParty" if info["platform"] == "Darwin" else root / "ThirdParty"


def qt_records(info: dict, root: Path) -> dict:
    # Match deployed runtime files to the producer's SBOM, then traverse only
    # runtime relationships. Build tools (rcc/moc/designer) are not product code.
    upstream = json.loads((Path(info["qt_root"]) / "sbom/qtbase-6.10.3.spdx.json").read_text(encoding="utf-8"))
    def identity(name: str) -> str:
        # ELF deployment preserves the SONAME; the producer records the full
        # versioned file behind that symlink. Both identify the same module.
        return re.sub(r"\.so(?:\.[0-9]+)+$", ".so", name) if name.startswith("libQt6") else name

    deployed = {identity(path.name) for path in root.rglob("*") if path.is_file() and "ThirdParty" not in path.parts}
    selected = {item["SPDXID"] for item in upstream["files"]
                if "BINARY" in item.get("fileTypes", []) and identity(Path(item["fileName"]).name) in deployed}
    for relation in upstream["relationships"]:
        if relation["relationshipType"] == "CONTAINS" and relation["relatedSpdxElement"] in selected:
            selected.add(relation["spdxElementId"])
    while True:
        previous = len(selected)
        for relation in upstream["relationships"]:
            if relation["relationshipType"] in {"DEPENDS_ON", "STATIC_LINK", "DYNAMIC_LINK"} and relation["spdxElementId"] in selected:
                selected.add(relation["relatedSpdxElement"])
        if len(selected) == previous:
            break
    packages = [item for item in upstream["packages"] if item["SPDXID"] in selected]
    if not {"Core", "Gui", "Widgets", "Network"}.issubset({item["name"] for item in packages}):
        raise ValueError("deployed Qt closure cannot be bound to the producer's SBOM")
    return {"upstreamNamespace": upstream["documentNamespace"], "packages": packages,
            "files": [item for item in upstream["files"] if item["SPDXID"] in selected and "BINARY" in item.get("fileTypes", [])],
            "relationships": [item for item in upstream["relationships"] if item["spdxElementId"] in selected and item["relatedSpdxElement"] in selected],
            "hasExtractedLicensingInfos": upstream.get("hasExtractedLicensingInfos", [])}


def notices(build: Path, info: dict, root: Path) -> None:
    archives = source_archives(build, info)
    destination = legal_root(root, info)
    destination.mkdir(parents=True, exist_ok=True)
    texts = [("Distribution notice", (ROOT / "packaging/DISTRIBUTION.txt").read_text())]
    for relative in ("sqlite/PUBLIC_DOMAIN.md", "quill/LICENSE", "quill/FMT-LICENSE", "glaze/LICENSE"):
        texts.append((relative, (ROOT / "third_party" / relative).read_text()))
    for name in ("QtBase", "KSyntaxHighlighting"):
        with tarfile.open(archives[name]) as source:
            for member in source.getmembers():
                if member.isfile() and "/LICENSES/" in member.name and member.size < 1024 * 1024:
                    stream = source.extractfile(member)
                    assert stream is not None
                    texts.append((member.name, stream.read().decode("utf-8")))
    # Preserve every embedded definition/theme's own attribution, including
    # legacy LGPL/GPL data licenses; never infer all KDE resources are MIT.
    with tarfile.open(archives["KSyntaxHighlighting"]) as source:
        for member in source.getmembers():
            if member.isfile() and any(part in member.name for part in ("/data/syntax/", "/data/themes/", "/src/lib/")) and member.size < 2 * 1024 * 1024:
                stream = source.extractfile(member)
                assert stream is not None
                body = stream.read().decode("utf-8", errors="replace")
                if member.name.endswith((".xml", ".theme")):
                    # Complete separately licensed data, not an abbreviated header.
                    texts.append((member.name, body))
                elif member.name.endswith((".cpp", ".h")):
                    texts.append((member.name, body.split("*/", 1)[0] + "*/"))
    if info["platform"] == "Darwin":
        texts.append(("LibArchive", (build / "_bpr/LibArchive-NOTICES.txt").read_text()))
    if info["platform"] == "Windows":
        # Microsoft redistributable terms are supplied by the licensed native
        # toolchain operator, never invented or silently copied from the web.
        notice = os.environ.get("BPR_MSVC_REDISTRIBUTION_NOTICE")
        if not notice or not Path(notice).is_file():
            raise ValueError("set BPR_MSVC_REDISTRIBUTION_NOTICE to the licensed toolchain's redistribution notice")
        texts.append(("Microsoft Visual C++ Runtime redistribution notice", Path(notice).read_text(encoding="utf-8")))
    records = qt_records(info, root)
    for item in records["packages"] + records["files"]:
        texts.append((item.get("name", item.get("fileName", "Qt")),
                      f"{item.get('copyrightText', 'NOASSERTION')}\nLicense: {item.get('licenseConcluded', 'NOASSERTION')}\nSource: {item.get('downloadLocation', records['upstreamNamespace'])}"))
    for item in records["hasExtractedLicensingInfos"]:
        texts.append((item["licenseId"], item["extractedText"]))
    (destination / "THIRD-PARTY-NOTICES.txt").write_text("\n\n".join(f"{title}\n{'=' * len(title)}\n{body}" for title, body in texts) + "\n", encoding="utf-8")
    write_json(destination / "qt-runtime-provenance.json", records)
    write_json(destination / "source-provenance.json", {"schema": "bpr.release-sources.v1", "components": components(info)})
    with zipfile.ZipFile(destination / "corresponding-source.zip", "w", compression=zipfile.ZIP_STORED) as output:
        for path in archives.values():
            output.write(path, path.name)
    (destination / "SOURCE-OFFERS.txt").write_text(
        "Corresponding third-party source is included in corresponding-source.zip, with exact upstream digests in source-provenance.json.\n"
        "Qt uses shared libraries. You may replace these libraries with ABI-compatible modified builds and debug those modifications.\n"
        "On macOS work on a copy of the bundle: replace the framework/plugin files and seal the copy locally with codesign --force --deep --sign - --timestamp=none. No Developer ID or notarization credentials are required. Modified bundles are not trusted updater destinations.\n"
        "On Windows/Linux replace the matching private Qt DLLs/shared objects and plugins with your compatible build. Keep the documented relative layout; no global search path is needed.\n"
        "KSyntaxHighlighting's MIT engine is statically linked. Individually licensed embedded definitions/themes, their complete source, license texts and attributions accompany it. System OS libraries are package dependencies, not redistributed runtime files.\n"
        "This evidence does not constitute owner/legal approval to distribute a release. That remains the release-readiness gate.\n", encoding="utf-8")


def linux_baseline() -> dict:
    release = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines() if "=" in line)
    identity = (release.get("ID", "").strip('"'), release.get("VERSION_ID", "").strip('"'))
    if identity not in {("ubuntu", "24.04"), ("rhel", "10.0"), ("rhel", "10")}:
        raise ValueError(f"not a supported package-build baseline: {identity}; use Ubuntu 24.04 or RHEL 10")
    if platform.machine() != "x86_64":
        raise ValueError("Linux packages require x86_64")
    return {"distribution": identity, "glibcFloor": "2.39", "cpuFloor": "x86-64-v3" if identity[0] == "rhel" else "x86-64"}


def qt_library(name: str) -> bool:
    match = re.fullmatch(r"(?:lib)?Qt6([A-Za-z0-9]+)(?:\.dll|\.so(?:\.[0-9.]+)?)", name, re.I)
    return bool(match and match[1] in QT_LIBRARIES)


def deploy(build: Path, info: dict, root: Path) -> None:
    target = info["platform"]
    if target == "Darwin":
        raise ValueError("macOS deployment is owned by tools/release.sh")
    baseline = linux_baseline() if target == "Linux" else {"osFloor": "Windows 10", "cpuFloor": "x64", "scope": "per-user"}
    qt = Path(info["qt_root"])
    for relative in PLUGINS[target]:
        source = qt / "plugins" / relative
        destination = root / "plugins" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    pending = [path for path in root.rglob("*") if path.is_file()]
    processed = set()
    system = set()
    while pending:
        binary = pending.pop()
        if binary in processed:
            continue
        processed.add(binary)
        if target == "Windows":
            headers = run("dumpbin", "/HEADERS", binary)
            if not re.search(r"8664 machine", headers, re.I):
                raise ValueError(f"not an x64 image: {binary}")
            dependencies = re.findall(r"^\s+([\w.-]+\.dll)\s*$", run("dumpbin", "/DEPENDENTS", binary), re.M | re.I)
            for name in dependencies:
                destination = root / name
                if destination.exists():
                    pending.append(destination)
                    continue
                if qt_library(name):
                    source = qt / "bin" / name
                elif CRT.fullmatch(name):
                    redist = os.environ.get("VCToolsRedistDir")
                    if not redist:
                        raise ValueError("run in the native VS x64 tools environment with VCToolsRedistDir")
                    found = list(Path(redist).glob(f"x64/Microsoft.VC*.CRT/{name}"))
                    if len(found) != 1:
                        raise ValueError(f"ambiguous or missing Microsoft CRT: {name}")
                    source = found[0]
                elif name.lower().startswith("qt6"):
                    raise ValueError(f"undeclared Qt module: {name}")
                elif name.lower().startswith(("api-ms-win-", "ext-ms-win-")) or (Path(os.environ["SystemRoot"]) / "System32" / name).is_file():
                    system.add(name)
                    continue
                else:
                    raise ValueError(f"undeclared PE runtime dependency: {name}")
                shutil.copy2(source, destination)
                pending.append(destination)
        else:
            if not binary.read_bytes()[:4] == b"\x7fELF":
                raise ValueError(f"unexpected runtime file: {binary}")
            headers = run("readelf", "--file-header", binary)
            if "ELF64" not in headers or not re.search(r"Machine:\s+Advanced Micro Devices X86-64", headers, re.I):
                raise ValueError(f"not an x86_64 ELF image: {binary}")
            # install(RPATH) already removed build-tree search paths; seed the
            # inspection from the declared provider until its closure is copied.
            environment = {key: value for key, value in os.environ.items() if not key.startswith(("LD_", "DYLD_"))}
            environment["LD_LIBRARY_PATH"] = os.pathsep.join((str(qt / "lib"), str(root)))
            output = run("ldd", binary, env=environment)
            if "not found" in output:
                raise ValueError(f"unresolved ELF dependency: {binary}: {output}")
            for name, source_text in re.findall(r"^\s*(\S+) => (/\S+) ", output, re.M):
                source = Path(source_text)
                if qt_library(name):
                    if name.startswith("libQt") and not source.resolve().is_relative_to(qt.resolve()):
                        raise ValueError(f"Qt runtime came from outside pinned provider: {source}")
                    destination = root / name
                    if not destination.exists():
                        shutil.copy2(source, destination)
                        pending.append(destination)
                elif name.startswith("libQt"):
                    raise ValueError(f"undeclared Qt module: {name}")
                elif str(source.resolve()).startswith(("/usr/lib/", "/lib/")):
                    system.add(name)
                else:
                    raise ValueError(f"untrusted ELF dependency: {source}")
    if target == "Linux":
        for binary in processed:
            relative = os.path.relpath(root, binary.parent)
            run("patchelf", "--set-rpath", "$ORIGIN" if relative == "." else "$ORIGIN/" + relative, binary)
            versions = run("readelf", "--version-info", binary)
            if any(tuple(map(int, value.split("."))) > (2, 39) for value in re.findall(r"\bGLIBC_([0-9.]+)", versions)):
                raise ValueError(f"ELF exceeds the glibc 2.39 floor: {binary}")
        baseline["requiredGLIBCXX"] = sorted({value for binary in processed for value in re.findall(r"\bGLIBCXX_([0-9.]+)", run("readelf", "--version-info", binary))}, key=lambda value: tuple(map(int, value.split("."))))
    (root / "qt.conf").write_text("[Paths]\nPlugins=plugins\n", encoding="utf-8")
    write_json(root / "runtime-closure.json", {"schema": "bpr.runtime-closure.v1", "baseline": baseline,
               "systemDependencies": sorted(system), "files": sorted(str(path.relative_to(root)) for path in processed), "plugins": PLUGINS[target]})
    notices(build, info, root)
    audit(build, info, root, root / "sbom.spdx.json")


def audit(build: Path, info: dict, root: Path, output: Path) -> None:
    legal = legal_root(root, info)
    if not (legal / "corresponding-source.zip").is_file():
        raise ValueError("corresponding source and notices must be prepared before artifact audit")
    revision = run("git", "-C", ROOT, "rev-parse", "HEAD").strip()
    source_clean = not run("git", "-C", ROOT, "status", "--porcelain").strip()
    records = qt_records(info, root)
    package = {"SPDXID": "SPDXRef-BetterPullReview", "name": "BetterPullReview", "versionInfo": info["version"],
               "downloadLocation": "NOASSERTION", "filesAnalyzed": False, "licenseConcluded": "NOASSERTION", "licenseDeclared": "NOASSERTION",
               "copyrightText": "NOASSERTION", "sourceInfo": f"git:{revision}; clean={source_clean}; tests-disabled Release; {info['platform']}"}
    packages = [package] + records["packages"]
    relations = records["relationships"] + [{"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES", "relatedSpdxElement": package["SPDXID"]}]
    for item in records["packages"]:
        if item["name"] in QT_LIBRARIES or "-qt-plugin-" in item["SPDXID"]:
            relations.append({"spdxElementId": package["SPDXID"], "relationshipType": "DEPENDS_ON", "relatedSpdxElement": item["SPDXID"]})
    for item in components(info):
        identity = "SPDXRef-" + item["name"]
        packages.append({"SPDXID": identity, "name": item["name"], "versionInfo": item["version"], "downloadLocation": item["url"],
                         "filesAnalyzed": False, "licenseConcluded": "NOASSERTION", "licenseDeclared": item["license"], "copyrightText": "NOASSERTION",
                         "sourceInfo": item["linkage"], "checksums": [{"algorithm": "SHA256" if "sha256" in item else "SHA3-256", "checksumValue": item.get("sha256", item.get("sha3_256"))}]})
        relations.append({"spdxElementId": identity, "relationshipType": "BUILD_DEPENDENCY_OF" if item["name"] == "extra-cmake-modules" else "DEPENDENCY_OF", "relatedSpdxElement": package["SPDXID"]})
    if info["platform"] == "Windows":
        crt_files = sorted(path.name for path in root.iterdir() if CRT.fullmatch(path.name))
        if not crt_files:
            raise ValueError("the native MSVC runtime closure was not recorded")
        packages.append({"SPDXID": "SPDXRef-Microsoft-Visual-C-Runtime", "name": "Microsoft Visual C++ Runtime",
                         "downloadLocation": "NOASSERTION", "filesAnalyzed": False, "licenseDeclared": "NOASSERTION", "licenseConcluded": "NOASSERTION",
                         "copyrightText": "NOASSERTION", "sourceInfo": "Native VCToolsRedistDir inputs; exact deployed files: " + ", ".join(crt_files)})
        relations.append({"spdxElementId": package["SPDXID"], "relationshipType": "DEPENDS_ON", "relatedSpdxElement": "SPDXRef-Microsoft-Visual-C-Runtime"})
    files = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink() and not path.resolve().is_relative_to(root.resolve()):
            raise ValueError(f"runtime symlink escapes package: {path}")
        if path.resolve() == output.resolve() or not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        identity = "SPDXRef-File-" + hashlib.sha256(relative.encode()).hexdigest()[:24]
        files.append({"SPDXID": identity, "fileName": "./" + relative, "checksums": [{"algorithm": "SHA256", "checksumValue": digest(path)}],
                      "licenseConcluded": "NOASSERTION", "licenseInfoInFiles": ["NOASSERTION"], "copyrightText": "NOASSERTION"})
        relations.append({"spdxElementId": package["SPDXID"], "relationshipType": "CONTAINS", "relatedSpdxElement": identity})
    # Qt file checksums describe the upstream input, not relocated/re-signed
    # bytes. Keep them in the provenance document; final bytes are hashed above.
    file_ids = {item["SPDXID"] for item in files}
    package_ids = {item["SPDXID"] for item in packages}
    relations = [item for item in relations if item["spdxElementId"] in package_ids | file_ids | {"SPDXRef-DOCUMENT"} and item["relatedSpdxElement"] in package_ids | file_ids]
    write_json(output, {"spdxVersion": "SPDX-2.3", "dataLicense": "CC0-1.0", "SPDXID": "SPDXRef-DOCUMENT", "name": f"BetterPullReview-{info['version']}-{info['platform']}",
               "documentNamespace": f"https://github.com/MikeMarcin/BetterPullReview/sbom/{revision}/{hashlib.sha256(json.dumps(files).encode()).hexdigest()}",
               "creationInfo": {"created": datetime.fromisoformat(run("git", "-C", ROOT, "show", "-s", "--format=%cI", "HEAD").strip()).astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "creators": ["Tool: BetterPullReview-package-runtime"]},
               "packages": packages, "files": files, "relationships": relations, "hasExtractedLicensingInfos": records["hasExtractedLicensingInfos"] +
               [{"licenseId": "LicenseRef-SQLite-Public-Domain", "extractedText": (ROOT / "third_party/sqlite/PUBLIC_DOMAIN.md").read_text()}]})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("deploy", "notices", "audit"))
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    build, root = args.build.resolve(strict=True), args.root.resolve(strict=True)
    info = inputs(build)
    if args.action == "deploy":
        deploy(build, info, root)
    elif args.action == "notices":
        notices(build, info, root)
    else:
        if args.output is None:
            parser.error("audit requires --output")
        audit(build, info, root, args.output.resolve())


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"package runtime refused: {error}") from error
