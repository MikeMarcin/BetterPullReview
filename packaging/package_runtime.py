#!/usr/bin/env python3
"""Native, tests-disabled runtime deployment and exact-artifact evidence.

No signing, credential discovery, upload or publication. macOS deployment stays
in tools/release.sh; this module supplies its shared source/notices/SBOM steps.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from typing import BinaryIO
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LINUX_TARGETS = {"DEB": "linux-ubuntu-26.04-x64", "RPM": "linux-rhel-10-x64"}
# RFC-BPR-0084 Q1: Svg rasterizes SVG body images (the library, no plugin).
QT_LIBRARIES = {"Core", "Gui", "Widgets", "Network", "DBus", "OpenGL", "XcbQpa", "Svg"}
# RFC-BPR-0084. GIF and JPEG decode body images; PNG is built into QtGui. Owner
# amendment 1 (Q2) adds WebP, qtimageformats' qwebp with its bundled libwebp.
PLUGINS = {
    "Darwin": ["platforms/libqcocoa.dylib", "styles/libqmacstyle.dylib",
               "networkinformation/libqapplenetworkinformation.dylib", "tls/libqsecuretransportbackend.dylib",
               "imageformats/libqgif.dylib", "imageformats/libqjpeg.dylib", "imageformats/libqwebp.dylib"],
    "Windows": ["platforms/qwindows.dll", "styles/qmodernwindowsstyle.dll", "tls/qschannelbackend.dll",
                "imageformats/qgif.dll", "imageformats/qjpeg.dll", "imageformats/qwebp.dll"],
    "Linux": ["platforms/libqxcb.so", "tls/libqopensslbackend.so",
              "imageformats/libqgif.so", "imageformats/libqjpeg.so", "imageformats/libqwebp.so"],
}
CRT = re.compile(r"(?:msvcp140(?:_1|_2|_atomic_wait|_codecvt_ids)?|vcruntime140(?:_1|_threads)?|concrt140)\.dll", re.I)
# Declared non-binary runtime files beside the application. [[BPR-0974]]: the
# Windows image keeps its Qt resources in one bundle and refuses to start
# without it; see cmake/BprQtResources.cmake.
RUNTIME_DATA = {"Darwin": [], "Windows": ["better_pull_review.rcc"], "Linux": []}
# [[BPR-1485]]. First-party-built shared libraries the application loads from
# its own package, relative to the package root. Git's LGPL xdiff ships only as
# its own replaceable shared library, as Qt does (RFC-BPR-0082 Q1).
PRIVATE_LIBRARIES = {
    "Darwin": ["Contents/Frameworks/libbpr_xdiff.dylib"],
    "Windows": ["bpr_xdiff.dll"],
    "Linux": ["libbpr_xdiff.so"],
}
# Retained third_party licence texts every installed THIRD-PARTY-NOTICES.txt
# carries whole. [[BPR-1270]]: the bundled code font's OFL and the authors
# list its copyright line names.
NOTICE_FILES = (
    "sqlite/PUBLIC_DOMAIN.md", "quill/LICENSE", "quill/FMT-LICENSE", "glaze/LICENSE",
    "jetbrains-mono/OFL.txt", "jetbrains-mono/AUTHORS.txt", "re2/LICENSE", "abseil/LICENSE", "zlib/LICENSE", "utf8proc/LICENSE.md", "gemoji/LICENSE", "fmt/LICENSE",
    "sha1collisiondetection/LICENSE", "hacl-star/LICENSE", "rapidfuzz-cpp/LICENSE", "dtl/COPYING",
    "git-xdiff/NOTICE", "git-xdiff/LGPL-2.1", "git-xdiff/xhistogram-EDL-1.0.txt",
)


def notice_files(info: dict) -> tuple[str, ...]:
    return NOTICE_FILES + (("icu/LICENSE",) if info["platform"] == "Linux" else ())


def run(*arguments: str | Path, env: dict | None = None) -> str:
    return subprocess.check_output([str(arg) for arg in arguments], text=True, stderr=subprocess.STDOUT, env=env)


def stream_digest(stream: BinaryIO, algorithm: str) -> str:
    result = hashlib.new(algorithm)
    for chunk in iter(lambda: stream.read(1024 * 1024), b""):
        result.update(chunk)
    return result.hexdigest()


def digest(path: Path, algorithm: str = "sha256") -> str:
    with path.open("rb") as stream:
        return stream_digest(stream, algorithm)


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
    qt_version = next(item["version"] for item in components(value) if item["name"] == "QtBase")
    if value["platform"] != platform.system() or value["qt_version"] != qt_version:
        raise ValueError("native platform and pinned Qt version must match package inputs")
    if value["platform"] == "Windows":
        for producer in private_library_producers(value):
            if not producer.resolve().is_relative_to(build.resolve()):
                raise ValueError("private-library producer is outside the selected build")
    return value


def private_library_producers(info: dict) -> list[Path]:
    """Source-built library identities come from the fixed runtime declaration.

    The selected CMake graph emits their producer paths; a filename in an
    inspected package cannot grant itself first-party producer authority.
    File bytes and full native link evidence are checked separately.
    """
    declared = info.get("private_libraries")
    if not isinstance(declared, dict) or set(declared) != set(PRIVATE_LIBRARIES[info["platform"]]):
        raise ValueError("private-library producer declaration mismatch; reconfigure the selected build")
    producers = []
    for relative, value in declared.items():
        if not isinstance(value, str):
            raise ValueError("invalid private-library producer path")
        producer = Path(value)
        if not producer.is_absolute() or producer.name != Path(relative).name or producer.is_symlink():
            raise ValueError("invalid private-library producer path")
        producers.append(producer)
    return producers


# [[BPR-1179]]. The two removable component families a package was built with.
# A package-inputs record from before BPR-1179 names none, and was built with both.
FAMILIES = ("agent_integrations", "local_automation")
FAMILY_WORDS = {"agent_integrations": "agent integrations", "local_automation": "local automation"}


def families(info: dict) -> dict[str, bool]:
    value = info.get("families", {name: True for name in FAMILIES})
    if set(value) != set(FAMILIES) or not all(isinstance(value[name], bool) for name in FAMILIES):
        raise ValueError("package inputs name an unknown component family")
    return {name: value[name] for name in FAMILIES}


def product_source_info(info: dict, revision: str, clean: bool) -> str:
    """The product package's SPDX sourceInfo: its source, its graph and, when
    it omits a family, which one. A full package's text is unchanged."""
    text = f"git:{revision}; clean={clean}; tests-disabled Release; {info['platform']}"
    absent = [FAMILY_WORDS[name] for name, present in families(info).items() if not present]
    return text + "".join(f"; {name} absent" for name in absent)


def components(info: dict) -> list[dict]:
    entries = json.loads((ROOT / "packaging/dependencies.json").read_text(encoding="utf-8"))["components"]
    return [item for item in entries if info["platform"] in item.get("platforms", [info["platform"]])]


def upstream_source_name(item: dict) -> str:
    if item["url"].endswith(".zip"):
        suffix = ".zip"
    elif item["url"].endswith(".xz"):
        suffix = ".tar.xz"
    else:
        suffix = ".tar.gz"
    return f"{item['name']}-{item['version']}{suffix}"


def build_input_name(item: dict, entry: dict) -> str:
    """[[BPR-1485]]. A first-party file that is part of a component's
    corresponding source (an LGPL library's build headers and script), stored
    in the source asset beside that component's upstream archive."""
    return f"{item['name']}-{item['version']}-build/{entry['path']}"


def build_input_path(entry: dict) -> Path:
    relative = Path(entry["path"])
    path = ROOT / relative
    if relative.is_absolute() or ".." in relative.parts or path.is_symlink() or not path.is_file():
        raise ValueError(f"corresponding source build input missing or unsafe: {entry['path']}")
    return path


def source_entries(items: list[dict]) -> dict[str, tuple[str, str]]:
    """Every entry the corresponding-source asset holds: name to (algorithm, digest)."""
    result = {}
    for item in items:
        algorithm = "sha256" if "sha256" in item else "sha3_256"
        result[upstream_source_name(item)] = (algorithm, item[algorithm])
        for entry in item.get("buildInputs", []):
            result[build_input_name(item, entry)] = ("sha256", entry["sha256"])
    return result


def source_archives(build: Path, info: dict) -> dict[str, Path]:
    directory = build / "release-sources"
    directory.mkdir(exist_ok=True)
    result = {}
    for item in components(info):
        algorithm = "sha256" if "sha256" in item else "sha3_256"
        path = directory / upstream_source_name(item)
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


def source_asset_name(info: dict) -> str:
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:[.-][0-9A-Za-z.-]+)?", info["version"]):
        raise ValueError("invalid source asset version")
    target = {"Darwin": "macos-universal", "Windows": "windows-x64", "Linux": "linux-x64"}[info["platform"]]
    if info["platform"] == "Linux" and info.get("target"):
        target = info["target"]
        if target not in LINUX_TARGETS.values():
            raise ValueError("invalid Linux release target")
    return f"Source-BetterPullReview-{info['version']}-{target}-corresponding-source.zip"


def source_asset_url(info: dict) -> str:
    return f"https://github.com/MikeMarcin/BetterPullReview/releases/download/v{info['version']}/{source_asset_name(info)}"


def source_asset_path(build: Path, info: dict, revision: str) -> Path:
    return build / "release-source-assets" / revision / source_asset_name(info)


def source_provenance(info: dict, revision: str, checksum: str) -> dict:
    return {"schema": "bpr.release-sources.v2", "components": components(info),
            "release": {"version": info["version"], "platform": info["platform"], "sourceRevision": revision},
            "asset": {"name": source_asset_name(info), "url": source_asset_url(info), "sha256": checksum}}


def check_source_provenance(provenance: dict, info: dict, revision: str | None = None) -> dict:
    recorded_revision = provenance.get("release", {}).get("sourceRevision", "")
    checksum = provenance.get("asset", {}).get("sha256", "")
    if not re.fullmatch(r"[a-f0-9]{40}", recorded_revision) or not re.fullmatch(r"[a-f0-9]{64}", checksum):
        raise ValueError("invalid corresponding source identity")
    if revision is not None and recorded_revision != revision:
        raise ValueError("corresponding source revision mismatch")
    if provenance != source_provenance(info, recorded_revision, checksum):
        raise ValueError("corresponding source provenance mismatch")
    return provenance


def check_source_references(provenance: dict, material: dict[str, str]) -> None:
    for name in ("SOURCE-OFFERS.txt", "THIRD-PARTY-NOTICES.txt"):
        if any(provenance["asset"][key] not in material[name] for key in ("name", "url", "sha256")):
            raise ValueError("corresponding source reference missing from " + name)


def installed_source_provenance(root: Path, info: dict, revision: str | None = None) -> dict:
    if any(root.rglob("*corresponding-source*.zip")):
        raise ValueError("corresponding source must be a separate release asset, not embedded")
    legal = legal_root(root, info)
    provenance = check_source_provenance(json.loads((legal / "source-provenance.json").read_text(encoding="utf-8")), info, revision)
    check_source_references(provenance, {name: (legal / name).read_text(encoding="utf-8")
                                        for name in ("SOURCE-OFFERS.txt", "THIRD-PARTY-NOTICES.txt")})
    return provenance


def verify_source_asset(archive: Path, provenance: dict) -> None:
    if archive.is_symlink() or not archive.is_file() or archive.name != provenance["asset"]["name"]:
        raise ValueError("corresponding source release asset missing or misnamed")
    if digest(archive) != provenance["asset"]["sha256"]:
        raise ValueError("corresponding source release asset digest mismatch")
    expected = source_entries(provenance["components"])
    with zipfile.ZipFile(archive) as stream:
        if sorted(stream.namelist()) != sorted(expected):
            raise ValueError("corresponding-source-inventory-mismatch")
        for name, (algorithm, value) in expected.items():
            with stream.open(name) as entry:
                checksum = stream_digest(entry, algorithm)
            if checksum != value:
                raise ValueError("corresponding-source-digest-mismatch")


def prepare_source_asset(build: Path, info: dict, root: Path, archives: dict[str, Path], output: Path | None) -> dict:
    revision = run("git", "-C", ROOT, "rev-parse", "HEAD").strip()
    output = output or source_asset_path(build, info, revision)
    if output.resolve().is_relative_to(root.resolve()) or output.name != source_asset_name(info) or output.is_symlink():
        raise ValueError("corresponding source requires its versioned name outside the installed runtime")
    output.parent.mkdir(parents=True, exist_ok=True)
    entries = [(path.name, path) for path in archives.values()]
    entries += [(build_input_name(item, entry), build_input_path(entry))
                for item in components(info) for entry in item.get("buildInputs", [])]
    # Fixed ZIP entry metadata makes the same upstream inputs byte-identical on
    # every preparation, independent of source-cache paths and download times.
    with tempfile.TemporaryDirectory(prefix=".source-asset-", dir=output.parent) as directory:
        temporary = Path(directory) / output.name
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_STORED) as stream:
            for name, path in sorted(entries):
                entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                entry.create_system = 3
                entry.external_attr = 0o100644 << 16
                with path.open("rb") as source, stream.open(entry, "w") as destination:
                    shutil.copyfileobj(source, destination)
        provenance = source_provenance(info, revision, digest(temporary))
        verify_source_asset(temporary, provenance)
        if output.exists():
            if digest(output) != provenance["asset"]["sha256"]:
                raise ValueError("existing versioned source asset differs; use a fresh output directory")
        else:
            temporary.rename(output)
    return provenance


def qt_upstream_records(info: dict, deployed_names: set[str]) -> dict:
    """Join only producer documents for the deployed Qt runtime modules.

    Basenames select a required document, never producer authority: callers
    still bind the file/relationship record and, for PE, exact upstream bytes.
    """
    qt = Path(info["qt_root"])
    upstream = json.loads((qt / f"sbom/qtbase-{info['qt_version']}.spdx.json").read_text(encoding="utf-8"))
    names = {name.lower() for name in deployed_names}
    svg = any(name in {"qtsvg", "qt6svg.dll", "libqt6svg.dylib"}
              or re.fullmatch(r"libqt6svg\.so(?:\.[0-9]+)*", name) for name in names)
    webp = bool(names & {"qwebp.dll", "libqwebp.dylib", "libqwebp.so"})
    for module, deployed in (("qtsvg", svg), ("qtimageformats", webp)):
        if not deployed:
            continue
        extra = json.loads((qt / f"sbom/{module}-{info['qt_version']}.spdx.json").read_text(encoding="utf-8"))
        for key in ("packages", "files", "relationships", "hasExtractedLicensingInfos"):
            upstream.setdefault(key, []).extend(item for item in extra.get(key, []) if item not in upstream.get(key, []))
    return upstream


def qt_records(info: dict, root: Path) -> dict:
    # Match deployed runtime files to the producer's SBOM, then traverse only
    # runtime relationships. Build tools (rcc/moc/designer) are not product code.
    deployed_names = {path.name for path in root.rglob("*") if path.is_file()}
    upstream = qt_upstream_records(info, deployed_names)
    def identity(name: str) -> str:
        if info.get("platform") == "Windows":
            return name.lower()
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


def notices(build: Path, info: dict, root: Path, source_archive: Path | None = None) -> None:
    archives = source_archives(build, info)
    provenance = prepare_source_asset(build, info, root, archives, source_archive)
    destination = legal_root(root, info)
    destination.mkdir(parents=True, exist_ok=True)
    texts = [("Distribution notice", (ROOT / "packaging/DISTRIBUTION.txt").read_text())]
    source_reference = (f"Corresponding third-party source for BetterPullReview {info['version']} ({info['platform']}), "
                        f"producer {provenance['release']['sourceRevision']}.\n"
                        f"Separate release asset: {provenance['asset']['name']}\n"
                        f"Download: {provenance['asset']['url']}\n"
                        f"SHA-256: {provenance['asset']['sha256']}\n")
    texts.append(("Corresponding source release asset", source_reference + "See SOURCE-OFFERS.txt for recipient access, retention and replacement instructions."))
    for relative in notice_files(info):
        texts.append((relative, (ROOT / "third_party" / relative).read_text(encoding="utf-8")))
    for name in ("QtBase", "QtSvg", "QtImageFormats", "KSyntaxHighlighting"):
        with tarfile.open(archives[name]) as source:
            for member in source.getmembers():
                if member.isfile() and "/LICENSES/" in member.name and member.size < 1024 * 1024:
                    stream = source.extractfile(member)
                    assert stream is not None
                    texts.append((member.name, stream.read().decode("utf-8")))
    # RFC-BPR-0084 Q2. libwebp is compiled into qwebp: its licence, patent
    # grant and authors travel whole.
    with tarfile.open(archives["QtImageFormats"]) as source:
        for member in source.getmembers():
            if member.isfile() and re.search(r"/src/3rdparty/libwebp/(COPYING|PATENTS|AUTHORS)$", member.name):
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
    write_json(destination / "source-provenance.json", provenance)
    (destination / "SOURCE-OFFERS.txt").write_text(
        source_reference +
        "The source archive is distributed beside the binary packages, not inside the application. Exact upstream digests are in source-provenance.json.\n"
        "Every binary recipient must have equivalent access to this exact source asset at no additional charge. Private repository access is required for the URL above; distributors must provide a copy through the original distribution channel if a recipient cannot access it, including after repository access changes. Corporate/offline redistribution must provide the same asset alongside the binaries.\n"
        "Retain the immutable source asset and recipient access while any matching binary is distributed and for every applicable source-offer/license retention period afterward. Retention is indefinite until the accountable distribution owner documents that all such obligations have ended; do not delete or replace the asset or rebind its version tag.\n"
        "Qt uses shared libraries. You may replace these libraries with ABI-compatible modified builds and debug those modifications.\n"
        "On macOS work on a copy of the bundle: replace the framework/plugin files and seal the copy locally with codesign --force --deep --sign - --timestamp=none. No Developer ID or notarization credentials are required. Modified bundles are not trusted updater destinations.\n"
        "On Windows/Linux replace the matching private Qt DLLs/shared objects and plugins with your compatible build. Keep the documented relative layout; no global search path is needed.\n"
        "Git's xdiff (LGPL-2.1-or-later; xdiff/xhistogram.c BSD-3-Clause, the Eclipse Distribution License 1.0) is its own separately replaceable shared library: "
        "Contents/Frameworks/libbpr_xdiff.dylib on macOS, bpr_xdiff.dll beside better_pull_review.exe on Windows, libbpr_xdiff.so beside the application on Linux. "
        "BetterPullReview links it dynamically and contains no copy of it. Its complete corresponding source is in the source asset above: the unmodified git-xdiff-2.56.0.tar.xz archive, "
        "whose xdiff/ directory is the library, and under git-xdiff-2.56.0-build/ the two compatibility headers, original provider, self-contained hardening helper and standalone recipient CMake project. "
        "Start with git-xdiff-2.56.0-build/third_party/git-xdiff/rebuild/README.md: verify and extract the supplied archive, then configure that directory against your extracted, optionally modified source. No application checkout, CPM or network acquisition is required. "
        "You may modify it and replace that one file with your own build exporting the same C interface (xdiff/xdiff.h of Git 2.56.0), as for the Qt libraries above, including the macOS resealing step.\n" +
        ("Linux ICU73.2 is rebuilt from the pinned source with PIC, stack protection, frame pointers, FORTIFY3 and full RELRO/NOW/nonexecutable stack. "
         "The source asset includes ICU-73.2.tar.gz and under ICU-73.2-build/ the exact packaging/linux_icu.py helper and third_party/icu/rebuild/README.md. "
         "Follow that README to build and replace the three matching data/uc/i18n SONAME73 files beside Qt; no application checkout or signing credentials are needed. "
         "The complete mixed upstream ICU notices are retained here, and ThirdParty/icu-runtime-provenance.json distinguishes the actual hardened source build from Qt's prebuilt ABI inputs.\n"
         if info["platform"] == "Linux" else "") +
        "KSyntaxHighlighting's MIT engine is statically linked. Individually licensed embedded definitions/themes, their complete source, license texts and attributions accompany it. System OS libraries are package dependencies, not redistributed runtime files.\n"
        "This evidence does not constitute owner/legal approval to distribute a release. That remains the release-readiness gate.\n", encoding="utf-8")


def linux_baseline() -> dict:
    release = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines() if "=" in line)
    identity = (release.get("ID", "").strip('"'), release.get("VERSION_ID", "").strip('"'))
    # Ubuntu is the selected next-release route. Keep the existing optional RPM
    # producer separate; its older ABI ceiling does not constrain Ubuntu.
    if identity not in {("ubuntu", "26.04"), ("rhel", "10.0"), ("rhel", "10")}:
        raise ValueError(f"not a supported package-build baseline: {identity}; use Ubuntu 26.04 (optional RPM: RHEL 10)")
    if platform.machine() != "x86_64":
        raise ValueError("Linux packages require x86_64")
    return {"distribution": identity, "glibcFloor": "2.43" if identity[0] == "ubuntu" else "2.39",
            "cpuFloor": "x86-64-v3" if identity[0] == "rhel" else "x86-64"}


def linux_target() -> str:
    return LINUX_TARGETS["DEB" if linux_baseline()["distribution"][0] == "ubuntu" else "RPM"]


def qt_library(name: str) -> bool:
    match = re.fullmatch(r"(?:lib)?Qt6([A-Za-z0-9]+)(?:\.dll|\.so(?:\.[0-9.]+)?)", name, re.I)
    return bool(match and match[1] in QT_LIBRARIES)


def linux_qt_companion(info: dict, name: str, observed: Path) -> tuple[Path, dict]:
    """Bind Qt's separately supplied ICU runtime to its exact upstream bytes."""
    if info["platform"] != "Linux":
        raise ValueError("Qt ICU companions require Linux")
    component = next(item for item in components(info) if item["name"] == "ICU")
    contract = component["qtRuntime"]
    if info.get("qt_version") != contract["qtVersion"] or name not in contract["files"]:
        raise ValueError("undeclared Qt ICU companion: " + name)
    expected = contract["files"][name]
    directory = (Path(info["qt_root"]) / "lib").resolve(strict=True)
    producer = (directory / expected["file"]).resolve(strict=True)
    if producer.parent != directory or observed.resolve(strict=True) != producer:
        raise ValueError("untrusted Qt ICU companion origin: " + name)
    if digest(producer) != expected["sha256"]:
        raise ValueError("Qt ICU companion producer digest mismatch: " + name)
    return producer, expected


def linux_icu_builder():
    # Non-Linux offline publication does not need this native build helper.
    spec = importlib.util.spec_from_file_location("bpr_linux_icu", ROOT / "packaging/linux_icu.py")
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    return builder


def linux_icu_compilers(build: Path) -> dict[str, Path]:
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    result = {}
    for name, key in (("CC", "CMAKE_C_COMPILER"), ("CXX", "CMAKE_CXX_COMPILER")):
        values = re.findall(r"(?m)^" + key + r":(?:FILEPATH|STRING)=(.+)$", cache)
        if len(values) != 1:
            raise ValueError("ICU needs the selected native CMake compiler: " + key)
        result[name] = Path(values[0])
    return result


def prepare_linux_icu(build: Path, info: dict) -> dict:
    component = next(item for item in components(info) if item["name"] == "ICU")
    archives = source_archives(build, info)
    return linux_icu_builder().prepare(build / "_bpr/icu/73.2", archives["ICU"], component, linux_icu_compilers(build))


def linux_icu_producers(build: Path, info: dict) -> tuple[dict[str, Path], dict]:
    component = next(item for item in components(info) if item["name"] == "ICU")
    directory = build / "_bpr/icu/73.2"
    builder = linux_icu_builder()
    receipt = builder.verify(directory, component, linux_icu_compilers(build))
    return {name: directory / "prefix/lib" / filename for name, filename in builder.FILES.items()}, receipt


def verify_linux_qt_companions(build: Path, info: dict, root: Path) -> list[dict]:
    if info["platform"] != "Linux":
        return []
    component = next(item for item in components(info) if item["name"] == "ICU")
    contract = component["qtRuntime"]
    names = set(contract["files"])
    actual = {path.relative_to(root).as_posix() for path in root.rglob("libicu*") if path.is_file()}
    if actual != names or any((root / name).is_symlink() for name in names):
        raise ValueError("Qt ICU companion runtime inventory mismatch")
    producers, receipt = linux_icu_producers(build, info)
    records = []
    with tempfile.TemporaryDirectory(prefix="bpr-icu-relocation-") as directory:
        for name in sorted(names):
            _, qt_input = linux_qt_companion(info, name, Path(info["qt_root"]) / "lib" / name)
            relocated = Path(directory) / name
            shutil.copy2(producers[name], relocated)
            run("patchelf", "--set-rpath", "$ORIGIN", relocated)
            checksum = digest(relocated)
            if digest(root / name) != checksum:
                raise ValueError("Qt ICU companion relocated digest mismatch: " + name)
            records.append({"name": name, "version": component["version"], "producerKind": "source-built",
                            "qtInputSHA256": qt_input["sha256"], "producerSHA256": receipt["outputs"][name],
                            "deployedSHA256": checksum, "qtVersion": contract["qtVersion"],
                            "qtArchiveSHA256": contract["sha256"], "sourceSHA256": component["sha256"],
                            "recipeSHA256": receipt["binding"]["recipeSHA256"]})
    return records


def runtime_data(root: Path, target: str) -> set[Path]:
    """The declared data files of an installed runtime; each must be a real file."""
    declared = {root / name for name in RUNTIME_DATA[target]}
    for path in sorted(declared):
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"declared runtime data missing: {path.name}")
    return declared


def private_libraries(root: Path, target: str) -> list[Path]:
    """[[BPR-1485]]. The package's own shared libraries; each must be a real file."""
    paths = [root / name for name in PRIVATE_LIBRARIES[target]]
    for path in paths:
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"private shared library missing: {path.name}")
    return paths


def deploy(build: Path, info: dict, root: Path, source_archive: Path | None = None) -> None:
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
    data = runtime_data(root, target)
    private_libraries(root, target)
    icu_receipt = None
    icu_producers = {}
    if target == "Linux":
        prepare_linux_icu(build, info)
        icu_producers, icu_receipt = linux_icu_producers(build, info)
    pending = [path for path in root.rglob("*") if path.is_file() and path not in data]
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
                elif name.startswith("libicu"):
                    linux_qt_companion(info, name, source)
                    producer = icu_producers[name]
                    destination = root / name
                    if destination.exists() and digest(destination) != icu_receipt["outputs"][name]:
                        raise ValueError("Qt ICU companion staging digest mismatch: " + name)
                    if not destination.exists():
                        shutil.copy2(producer, destination)
                        pending.append(destination)
                elif name.startswith("libQt"):
                    raise ValueError(f"undeclared Qt module: {name}")
                elif name in PRIVATE_LIBRARIES["Linux"] and source.resolve() == (root / name).resolve():
                    # [[BPR-1485]]. The package's own shared library, already
                    # installed beside the application and inspected in turn.
                    continue
                elif str(source.resolve()).startswith(("/usr/lib/", "/lib/")):
                    system.add(name)
                else:
                    raise ValueError(f"untrusted ELF dependency: {source}")
    if target == "Linux":
        for binary in processed:
            relative = os.path.relpath(root, binary.parent)
            run("patchelf", "--set-rpath", "$ORIGIN" if relative == "." else "$ORIGIN/" + relative, binary)
            versions = run("readelf", "--version-info", binary)
            ceiling = tuple(map(int, baseline["glibcFloor"].split(".")))
            if any(tuple(map(int, value.split("."))) > ceiling for value in re.findall(r"\bGLIBC_([0-9.]+)", versions)):
                raise ValueError(f"ELF exceeds the glibc {baseline['glibcFloor']} ceiling: {binary}")
        baseline["requiredGLIBCXX"] = sorted({value for binary in processed for value in re.findall(r"\bGLIBCXX_([0-9.]+)", run("readelf", "--version-info", binary))}, key=lambda value: tuple(map(int, value.split("."))))
    (root / "qt.conf").write_text("[Paths]\nPlugins=plugins\n", encoding="utf-8")
    closure = {"schema": "bpr.runtime-closure.v1", "baseline": baseline, "families": families(info),
               "systemDependencies": sorted(system), "files": sorted(str(path.relative_to(root)) for path in processed), "plugins": PLUGINS[target]}
    if RUNTIME_DATA[target]:
        closure["data"] = RUNTIME_DATA[target]
    if target == "Linux":
        closure["qtCompanions"] = verify_linux_qt_companions(build, info, root)
    write_json(root / "runtime-closure.json", closure)
    notices(build, info, root, source_archive)
    if icu_receipt is not None:
        write_json(legal_root(root, info) / "icu-runtime-provenance.json", icu_receipt)
    audit(build, info, root, root / "sbom.spdx.json", source_archive)


def audit(build: Path, info: dict, root: Path, output: Path, source_archive: Path | None = None) -> None:
    revision = run("git", "-C", ROOT, "rev-parse", "HEAD").strip()
    provenance = installed_source_provenance(root, info, revision)
    verify_source_asset(source_archive or source_asset_path(build, info, revision), provenance)
    source_clean = not run("git", "-C", ROOT, "status", "--porcelain").strip()
    records = qt_records(info, root)
    package = {"SPDXID": "SPDXRef-BetterPullReview", "name": "BetterPullReview", "versionInfo": info["version"],
               "downloadLocation": "NOASSERTION", "filesAnalyzed": False, "licenseConcluded": "NOASSERTION", "licenseDeclared": "NOASSERTION",
               "copyrightText": "NOASSERTION", "sourceInfo": product_source_info(info, revision, source_clean)}
    packages = [package]
    for item in records["packages"]:
        # Producer verification codes cover upstream files, not our relocated
        # and re-sealed bytes. Keep those assertions only in provenance.
        dependency = dict(item)
        dependency["filesAnalyzed"] = False
        dependency.pop("packageVerificationCode", None)
        dependency.pop("licenseInfoFromFiles", None)
        dependency["sourceInfo"] = f"Qt producer input: {records['upstreamNamespace']}; {item.get('sourceInfo', '')}"
        packages.append(dependency)
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
    if sys.argv[1:] == ["baseline"]:
        print(linux_target())
        return
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("deploy", "notices", "audit"))
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--source-archive", type=Path, help="separate versioned source release asset (outside the runtime)")
    args = parser.parse_args()
    build, root = args.build.resolve(strict=True), args.root.resolve(strict=True)
    info = inputs(build)
    if args.action == "deploy":
        deploy(build, info, root, args.source_archive)
    elif args.action == "notices":
        notices(build, info, root, args.source_archive)
    else:
        if args.output is None:
            parser.error("audit requires --output")
        audit(build, info, root, args.output.resolve(), args.source_archive)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        raise SystemExit(f"package runtime refused: {error}") from error
