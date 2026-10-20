#!/usr/bin/env python3
"""BPR-1656: offline recipient rebuild from an existing source asset.

Explicit course, not a full application build. Requires --source-zip,
--delivered-library and a new --evidence directory. Generates an assessment
asset through the public writer, restricted to the actual xdiff component.
Only its extracted files are available to the recipient build.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "packaging"))
sys.path.insert(0, str(ROOT / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
import package_runtime as runtime

SOURCES = {"xdiffi.c", "xemit.c", "xhistogram.c", "xmerge.c", "xpatience.c", "xprepare.c", "xutils.c"}
CONSUMER = r'''
#include "git-compat-util.h"
#include "xdiff.h"
static char output[1024];
static size_t used;
static int collect(void *unused, mmbuffer_t *buffers, int count)
{
    (void)unused;
    for (int i = 0; i < count; ++i) {
        if (buffers[i].size < 0 || (size_t)buffers[i].size >= sizeof(output) - used) return -1;
        memcpy(output + used, buffers[i].ptr, (size_t)buffers[i].size);
        used += (size_t)buffers[i].size;
    }
    return 0;
}
#ifdef MODIFIED
extern unsigned long bpr_xdiff_recipient_modification(void);
#endif
int main(int argc, char **argv)
{
    (void)argv;
    char before[] = "one\ntwo\nthree\n";
    char after[] = "one\nchanged\nthree\n";
    mmfile_t left = {before, sizeof(before) - 1}, right = {after, sizeof(after) - 1};
    xpparam_t parameters = {0};
    xdemitconf_t config = {0};
    xdemitcb_t callback = {0};
    parameters.flags = XDF_INDENT_HEURISTIC;
    config.ctxlen = 3;
    callback.out_line = collect;
    if (xdl_diff(&left, &right, &parameters, &config, &callback) < 0) return 1;
    if (strcmp(output, argc > 1 ? "wrong result" : "@@ -1,3 +1,3 @@\n one\n-two\n+changed\n three\n")) return 2;
#ifdef MODIFIED
    if (bpr_xdiff_recipient_modification() != 1656UL) return 3;
#endif
    puts("XDIFF_RECIPIENT_API_OK");
    return 0;
}
'''


class RecipientRebuildTests(unittest.TestCase):
    def command(self, label, arguments, cwd=None, success=True):
        result = subprocess.run(list(map(str, arguments)), cwd=cwd, capture_output=True, text=True, timeout=120)
        (self.evidence / (label + ".stdout.txt")).write_text(result.stdout)
        (self.evidence / (label + ".stderr.txt")).write_text(result.stderr)
        self.receipt["commands"].append({"label": label, "argv": list(map(str, arguments)),
                                         "cwd": str(cwd) if cwd else None, "exitCode": result.returncode})
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, label + " must refuse")
        return result.stdout

    def exports(self, path, label):
        if sys.platform == "darwin":
            output = self.command(label, ["nm", "-arch", platform.machine(), "-gjU", path])
            return set(output.splitlines())
        if sys.platform == "win32":
            output = self.command(label, ["dumpbin", "/exports", path])
            return set(re.findall(r"^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)", output, re.M))
        output = self.command(label, ["nm", "-D", "--defined-only", path])
        return {line.split()[-1] for line in output.splitlines() if line.split()}

    def consumer(self, scratch, source, shims, library, label, modified=False):
        driver = scratch / (label + "-driver")
        driver.mkdir()
        (driver / "main.c").write_text(CONSUMER)
        (driver / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.21)
project(RecipientConsumer LANGUAGES C)
add_library(recipient_xdiff SHARED IMPORTED)
set_target_properties(recipient_xdiff PROPERTIES IMPORTED_LOCATION "${LIBRARY}")
if(WIN32)
  set_target_properties(recipient_xdiff PROPERTIES IMPORTED_IMPLIB "${IMPORT_LIBRARY}")
endif()
add_executable(recipient_consumer main.c)
target_include_directories(recipient_consumer PRIVATE "${SOURCE}/xdiff" "${SHIMS}")
target_link_libraries(recipient_consumer PRIVATE recipient_xdiff)
if(MODIFIED)
  target_compile_definitions(recipient_consumer PRIVATE MODIFIED)
endif()
''')
        build = scratch / (label + "-consumer")
        arguments = ["cmake", "-S", driver, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                     f"-DSOURCE={source}", f"-DSHIMS={shims}", f"-DLIBRARY={library}", f"-DMODIFIED={'ON' if modified else 'OFF'}"]
        if sys.platform == "win32":
            arguments.append(f"-DIMPORT_LIBRARY={library.with_suffix('.lib')}")
        self.command(label + "-configure", arguments)
        self.command(label + "-link", ["cmake", "--build", build])
        executable = build / ("recipient_consumer.exe" if sys.platform == "win32" else "recipient_consumer")
        if sys.platform == "win32":
            shutil.copy2(library, build / library.name)
        self.assertIn("XDIFF_RECIPIENT_API_OK", self.command(label + "-run", [executable]))
        self.command(label + "-wrong-result", [executable, "wrong"], success=False)

    def test_original_modified_and_refusal_controls(self):
        self.evidence = OPTIONS.evidence.resolve()
        self.evidence.mkdir(mode=0o700, parents=True, exist_ok=False)
        self.receipt = {"scope": "archive-only xdiff component assessment; not final package/native desktop qualification",
                        "sourceRevision": runtime.run("git", "-C", ROOT, "rev-parse", "HEAD").strip(),
                        "sourceDirty": bool(runtime.run("git", "-C", ROOT, "status", "--porcelain").strip()),
                        "assessmentInputHashes": {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                                                  for path in ("packaging/dependencies.json", "packaging/package_runtime.py",
                                                               "tests/scripts/xdiff_recipient_rebuild_tests.py")},
                        "platform": platform.platform(), "commands": [], "complete": False}
        delivered = OPTIONS.delivered_library.resolve()
        original_hash = runtime.digest(delivered)
        self.receipt["deliveredLibrary"] = {"path": str(delivered), "sha256": original_hash}
        try:
            with ContainedTemporaryDirectory(prefix="xdiff-recipient-") as directory:
                scratch = Path(directory)
                item = next(item for item in runtime.components({"platform": platform.system()}) if item["name"] == "git-xdiff")
                archive = scratch / runtime.upstream_source_name(item)
                with zipfile.ZipFile(OPTIONS.source_zip) as old_asset:
                    archive.write_bytes(old_asset.read(archive.name))
                self.assertEqual(runtime.digest(archive), item["sha256"])
                self.receipt["originalArchiveSha256"] = runtime.digest(archive)
                build, installed = scratch / "asset-build", scratch / "unused-runtime"
                build.mkdir()
                installed.mkdir()
                info = {"platform": platform.system(), "version": "0.3.0"}
                with patch.object(runtime, "components", return_value=[item]):
                    provenance = runtime.prepare_source_asset(build, info, installed, {item["name"]: archive}, None)
                    asset = runtime.source_asset_path(build, info, self.receipt["sourceRevision"])
                    runtime.verify_source_asset(asset, provenance)
                    self.receipt["assessmentProvenance"] = provenance
                    recipient = scratch / "recipient-only"
                    with zipfile.ZipFile(asset) as stream:
                        stream.extractall(recipient)
                    # Real corrupt pinned archive must fail the public asset writer.
                    archive.write_bytes(archive.read_bytes() + b"changed")
                    with self.assertRaisesRegex(ValueError, "corresponding-source-digest-mismatch"):
                        runtime.prepare_source_asset(build, info, installed, {item["name"]: archive}, build / "corrupt" / asset.name)
                archive.unlink()
                with tarfile.open(recipient / archive.name, "r:xz") as stream:
                    members = [m for m in stream.getmembers() if m.name.startswith("git-2.56.0/xdiff/") or m.name in ("git-2.56.0/LGPL-2.1", "git-2.56.0/COPYING")]
                    stream.extractall(recipient, members=members, filter="data")
                source = recipient / "git-2.56.0"
                material = recipient / "git-xdiff-2.56.0-build"
                shims = material / "third_party/git-xdiff"
                recipe = shims / "rebuild"
                arguments = ["cmake", "-S", recipe, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                             f"-DBPR_XDIFF_SOURCE_DIR={source}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
                if sys.platform == "darwin":
                    arguments += ["-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0"]
                for label, missing in (("missing-helper", material / "cmake/Hardening.cmake"),
                                       ("missing-header", shims / "git-compat-util.h"),
                                       ("missing-source", source / "xdiff/xemit.c")):
                    saved = missing.read_bytes()
                    missing.unlink()
                    try:
                        self.command(label, arguments + ["-B", scratch / label], success=False)
                    finally:
                        missing.write_bytes(saved)
                output = scratch / "rebuilt"
                self.command("recipient-configure", arguments + ["-B", output])
                self.command("recipient-build", ["cmake", "--build", output])
                commands = json.loads((output / "compile_commands.json").read_text())
                self.assertEqual({Path(c["file"]).name for c in commands}, SOURCES)
                self.assertEqual(len(commands), 7)
                for entry in commands:
                    self.assertTrue(Path(entry["file"]).is_relative_to(source))
                    if sys.platform == "win32":
                        for option in ("/sdl", "/guard:cf"):
                            self.assertIn(option, entry["command"])
                    else:
                        for option in ("-std=c11", "-fPIC", "-fvisibility=default", "-fstack-protector-strong",
                                       "-fno-omit-frame-pointer", "-D_FORTIFY_SOURCE=3"):
                            self.assertIn(option, entry["command"])
                (self.evidence / "compile_commands.json").write_text(json.dumps(commands, indent=2) + "\n")
                library = output / {"Darwin": "libbpr_xdiff.dylib", "Windows": "bpr_xdiff.dll", "Linux": "libbpr_xdiff.so"}[platform.system()]
                if sys.platform == "darwin":
                    self.assertEqual(set(self.command("rebuilt-architectures", ["lipo", "-archs", library]).split()), {"arm64", "x86_64"})
                exports = self.exports(library, "rebuilt-exports")
                self.assertTrue(exports)
                self.assertEqual(exports, self.exports(delivered, "delivered-exports"))
                self.receipt["originalExports"] = sorted(exports)
                self.receipt["originalRebuiltLibrarySha256"] = runtime.digest(library)
                self.consumer(scratch, source, shims, library, "original")
                # Edit recipient source without updating the original archive pin.
                modified = source / "xdiff/xutils.c"
                with modified.open("a") as stream:
                    stream.write("\nunsigned long bpr_xdiff_recipient_modification(void) { return 1656UL; }\n")
                self.command("modified-build", ["cmake", "--build", output])
                added = "_bpr_xdiff_recipient_modification" if sys.platform == "darwin" else "bpr_xdiff_recipient_modification"
                self.assertEqual(self.exports(library, "modified-exports"), exports | {added})
                self.consumer(scratch, source, shims, library, "modified", modified=True)
                self.receipt["modifiedLibrarySha256"] = runtime.digest(library)
                self.receipt["controls"] = ["corrupt-original-archive", "missing-helper", "missing-header", "missing-source", "wrong-original-result", "wrong-modified-result", "modified-source-marker-and-retained-ABI"]
                self.assertEqual(runtime.digest(delivered), original_hash)
            self.receipt["complete"] = True
        finally:
            (self.evidence / "receipt.json").write_text(json.dumps(self.receipt, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-zip", type=Path, required=True)
    parser.add_argument("--delivered-library", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
