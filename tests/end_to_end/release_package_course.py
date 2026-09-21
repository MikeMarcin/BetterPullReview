#!/usr/bin/env python3
"""Exercise exact packaged capabilities and altered copies in a guarded scratch root.

The macOS course uses the production entry and shipped Cocoa plugin. It grants
no native install, authentication, signing, crash or clean-machine acceptance.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import verify_release_package as package
import verify_runtime_surface as surface


def expect_refusal(action, reason: str) -> None:
    try:
        action()
    except ValueError as error:
        package.require(reason in str(error), "wrong-negative-control-terminal")
        return
    raise ValueError("negative-control-falsely-passed:" + reason)


def helper_request(helper: Path, snapshot: Path, binding: str, path: str) -> dict:
    request = {"jsonrpc": "2.0", "id": 7, "method": "tools/call",
               "params": {"name": "read_snapshot_file", "arguments": {"path": path}}}
    result = subprocess.run([str(helper)], input=json.dumps(request) + "\n", text=True,
                            capture_output=True, timeout=5,
                            env={"PATH": "/usr/bin:/bin", "BPR_MCP_SNAPSHOT_ROOT": str(snapshot),
                                 "BPR_MCP_PINNED_BINDING": binding, "GITHUB_TOKEN": "BPR_CANARY_SECRET_HELPER"})
    package.require(result.returncode == 0 and "BPR_CANARY_SECRET_HELPER" not in result.stdout + result.stderr, "helper-process-or-secret-failure")
    value = json.loads(result.stdout)
    package.require(value.get("id") == 7, "helper-response-identity-mismatch")
    return value


def course(args, scratch: Path) -> dict:
    source = args.package.resolve(strict=True)
    # Copy, never alter the candidate. Spaces make accidental argv splitting
    # visible and relocation must leave the original build tree unused.
    relocated = scratch / "relocated package" / "BetterPullReview.app"
    shutil.copytree(source, relocated, symlinks=True)
    sbom = scratch / "candidate.spdx.json"
    shutil.copyfile(args.sbom, sbom)
    info = package.runtime.inputs(args.build)
    app, helpers = package.package_paths(relocated, info)
    source_archive = args.sbom.parent / package.runtime.source_asset_name(package.runtime.inputs(args.build))
    original = package.audit(args.build, relocated, sbom, args.source_revision, source_archive=source_archive)
    results = ["relocated-exact-audit"]

    home = scratch / "home"
    home.mkdir(mode=0o700)
    # Match the established external CLI course's child-only home isolation.
    # The live shell's HOME and the user's profile are never changed.
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("GH_", "GITHUB_", "QT_", "DYLD_"))}
    environment.update(HOME=str(home), CFFIXED_USER_HOME=str(home), PATH="/usr/bin:/bin:/usr/sbin:/sbin")
    def healthy(env):
        package.require(surface.inspect_application([str(app)], native_platform=True,
                        environment=env, package_root=relocated) == 0, "packaged-idle-runtime-failed")
    healthy(environment)
    package.require(any(home.rglob("profile-registry.sqlite3")), "isolated-production-profile-not-composed")
    results.append("native-cocoa-idle-private-home-loader-socket-process-canary")

    hostile = scratch / "hostile-plugins"
    shutil.copytree(relocated / "Contents/PlugIns", hostile)
    healthy({**environment, "QT_PLUGIN_PATH": str(hostile), "QT_QPA_PLATFORM_PLUGIN_PATH": str(hostile / "platforms"),
             "QT_QPA_GENERIC_PLUGINS": "untrusted", "QT_DEBUG_PLUGINS": "1"})
    results.append("hostile-plugin-environment-keeps-packaged-loader-origins")

    def refused(arguments):
        result = subprocess.run([str(app), *arguments], env=surface.launch_environment(True, environment),
                                capture_output=True, text=True, timeout=5)
        package.require(result.returncode != 0 and "INSTALLED_RUNTIME_REFUSED" in result.stderr, "broken-runtime-falsely-started")
    for arguments in (["-platformpluginpath", str(hostile)], ["--platformpluginpath=" + str(hostile)],
                      ["-plugin", "untrusted"], ["--plugin=untrusted"], ["-qmljsdebugger=port:1"]):
        refused(arguments)
    results.append("five-loader-argument-refusals-before-qt")

    plugins = relocated / "Contents/PlugIns"
    displaced = scratch / "displaced-plugins"
    plugins.rename(displaced)
    try:
        refused([])
        plugins.symlink_to(displaced, target_is_directory=True)
        try:
            refused([])
        finally:
            plugins.unlink()
    finally:
        displaced.rename(plugins)
    results.append("missing-and-outside-symlinked-plugin-root-refused")

    check = lambda: package.audit(args.build, relocated, sbom, args.source_revision, source_archive=source_archive)
    for helper in helpers:
        saved = scratch / helper.name
        helper.rename(saved)
        try:
            expect_refusal(check, "required-packaged-capability-missing")
            try:
                subprocess.run([str(helper)], check=False, capture_output=True, timeout=5)
            except FileNotFoundError:
                pass
            else:
                raise ValueError("missing-helper-found-a-fallback")
        finally:
            saved.rename(helper)
    results.append("three-removed-helpers-fail-audit-and-public-execution")

    identity_path = relocated / "Contents/Info.plist"
    identity_bytes = identity_path.read_bytes()
    identity = plistlib.loads(identity_bytes)
    identity["CFBundleIdentifier"] = "com.example.untrusted"
    identity_path.write_bytes(plistlib.dumps(identity))
    try:
        expect_refusal(check, "invalid-package-identity")
    finally:
        identity_path.write_bytes(identity_bytes)
    results.append("invalid-bundle-identity-refused")

    # Counterfeit producer hashes bypass the digest check on purpose so each
    # semantic/byte guard must catch the change itself, not claim another
    # guard's failure as its own evidence.
    def rebind_file(path):
        record = json.loads(args.sbom.read_text())
        for item in record["files"]:
            if item["fileName"] == "./" + path.relative_to(relocated).as_posix():
                item["checksums"] = [{"algorithm": "SHA256", "checksumValue": package.runtime.digest(path)}]
        sbom.write_text(json.dumps(record))
    helper = next(path for path in helpers if "mcp_context" in path.name)
    payload = helper.read_bytes()
    try:
        for marker in (b"--test-application-data-root", b"BPR_CANARY_SECRET_PACKAGE", b"BPR_CANARY_SOURCE_PACKAGE", b"BPR_CANARY_COMMENT_PACKAGE"):
            helper.write_bytes(payload + marker)
            rebind_file(helper)
            expect_refusal(check, "fixture-or-canary-bytes-in-package")
        helper.write_bytes(payload)
        subprocess.run(["install_name_tool", "-add_rpath", str(scratch / "untrusted-libraries"), str(helper)], check=True, capture_output=True)
        rebind_file(helper)
        expect_refusal(check, "untrusted-library-search-path")
    finally:
        helper.write_bytes(payload)
        shutil.copyfile(args.sbom, sbom)
    results.append("four-byte-canaries-and-real-untrusted-rpath-refused-after-rehash")

    snapshot = scratch / "snapshot"
    snapshot.mkdir(mode=0o700)
    (snapshot / "files").mkdir(mode=0o700)
    binding = "1/2/17/" + "1" * 40 + "/" + "2" * 40 + "/1/2"
    (snapshot / "binding").write_text(binding)
    (snapshot / "files/review.cpp").write_text("int exact_packaged_context = 17;\n")
    good = helper_request(helper, snapshot, binding, "review.cpp")
    package.require(good["result"]["content"][0]["text"] == "int exact_packaged_context = 17;\n", "packaged-helper-did-not-read-exact-snapshot")
    (snapshot / "private-canary").write_text("BPR_CANARY_SECRET_OUTSIDE_SNAPSHOT")
    bad = helper_request(helper, snapshot, binding, "../private-canary")
    package.require("path_outside_snapshot" in json.dumps(bad) and "BPR_CANARY" not in json.dumps(bad), "helper-path-escape-not-refused")
    (snapshot / "binding").write_text(binding[:-1] + "3")
    stale = helper_request(helper, snapshot, binding, "review.cpp")
    package.require("revision_binding_stale" in json.dumps(stale) and "exact_packaged_context" not in json.dumps(stale), "helper-stale-snapshot-not-refused")
    results.append("packaged-mcp-exact-stdio-read-path-escape-and-stale-binding")

    package.require(check() == original, "restored-copy-differs-from-original-candidate")
    package.require(package.audit(args.build, source, args.sbom, args.source_revision) == original, "source-package-drift")
    return {"result": "RELEASE_PACKAGE_COURSE_OK", "source_revision": args.source_revision,
            "executable_sha256": original["executable_sha256"], "checks": results,
            "native_install_auth_trust_crash_acceptance": "pending", "background_commands": []}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--sbom", required=True, type=Path)
    parser.add_argument("--source-revision", required=True)
    args = parser.parse_args()
    package.require(platform.system() == "Darwin", "native-macos-package-course-required")
    admitted = Path(os.environ["QUALIFICATION_TARGET_ROOT"]).resolve(strict=True)
    package.require(not admitted.is_relative_to(ROOT) and not ROOT.is_relative_to(admitted), "scratch-must-not-overlap-live-checkout")
    with tempfile.TemporaryDirectory(prefix="package-course-", dir=admitted) as temporary:
        evidence = course(args, Path(temporary).resolve(strict=True))
    print(json.dumps(evidence, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
