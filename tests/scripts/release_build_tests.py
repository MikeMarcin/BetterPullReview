#!/usr/bin/env python3
"""Public release-cohort allocation, durable recovery and source/native misuse proofs."""
from __future__ import annotations

import copy
import json
import multiprocessing
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "packaging"))
import release_build as cohort
from release_build_fixture import windows_floor


def lock_holder(common, ready, release):
    with cohort.reservation_lock(Path(common)):
        ready.set()
        release.wait(15)


class ReleaseBuildTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="bpr-release-cohort-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "source"
        self.root.mkdir()
        self.git("init", "-b", "main")
        self.git("config", "user.name", "BPR fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        (self.root / "CMakeLists.txt").write_text("project(\n BetterPullReview\n VERSION 0.4.1\n LANGUAGES CXX\n)\n")
        for name in cohort.CONFIG_FILES:
            path = self.root / name
            path.parent.mkdir(exist_ok=True)
            path.write_text("{}\n")
        cohort.write(self.root / cohort.WINDOWS_FLOOR, windows_floor())
        cohort.write(self.root / cohort.COUNTER, {"schema": "bpr.release-counter.v1", "product": cohort.PRODUCT, "last": 0})
        self.commit("base")
        self.inputs = {"schema": "bpr.release-inputs.v1", "previousWindows": {"marketingVersion": "0.4.0", "msiProductVersion": "0.4.0"},
                       "producers": {"mac": {"platform": "Darwin", "architectures": ["arm64"], "preset": "app-release", "buildType": "Release",
                           "options": {key: "ON" if key in {"BPR_BUILD_QT", "BPR_DISTRIBUTION_BUILD"} else "OFF" for key in cohort.OPTIONS},
                           "settings": {"CMAKE_CXX_FLAGS": ""}, "toolchain": {"cmake": "4.4.2", "compilerId": "AppleClang", "compilerVersion": "17.0.0", "qtVersion": "6.11.2"}}}}

    def git(self, *args):
        return cohort.git(self.root, *args)

    def commit(self, message):
        self.git("add", ".")
        self.git("commit", "-qm", message)
        return self.git("rev-parse", "HEAD")

    def allocate(self):
        return cohort.allocation(self.root, self.inputs)

    def frozen(self):
        receipt = self.allocate()
        self.commit("allocation only")
        return cohort.freeze(self.root, receipt["number"])

    def test_public_prepare_freeze_retry_and_changed_input_cohorts(self):
        base = self.git("rev-parse", "HEAD")
        receipt = self.allocate()
        self.assertEqual(receipt["number"], 1)
        self.assertEqual(self.allocate(), receipt)  # Interrupted before commit.
        self.commit("allocation only")
        frozen = cohort.freeze(self.root, 1)
        self.assertEqual(frozen["allocation"]["sourceBase"], base)
        self.assertEqual(cohort.check(frozen, self.root)["number"], 1)
        self.assertEqual(self.allocate(), receipt)  # Unchanged retry after freeze.
        self.inputs["producers"]["mac"]["toolchain"]["compilerVersion"] = "17.0.1"
        second = self.allocate()
        self.assertEqual(second["number"], 2)
        self.commit("new input allocation")
        self.assertNotEqual(cohort.freeze(self.root, 2)["cohortDigest"], frozen["cohortDigest"])
        (self.root / "product.cpp").write_text("// changed\n")
        self.commit("source change")
        self.assertEqual(self.allocate()["number"], 3)

    def test_common_git_reservation_survives_abandonment_other_worktree(self):
        receipt = self.allocate()
        self.git("reset", "--hard", "HEAD")
        (self.root / cohort.receipt_path(1)).unlink()
        other = Path(self.temporary.name) / "other"
        self.git("worktree", "add", "-b", "other", str(other))
        # Same exact input recovers the reservation, including after process loss.
        self.assertEqual(self.allocate(), receipt)
        self.git("reset", "--hard", "HEAD")
        (self.root / cohort.receipt_path(1)).unlink()
        (self.root / "source.cpp").write_text("changed\n")
        self.commit("changed candidate")
        self.assertEqual(self.allocate()["number"], 2)
        self.assertEqual(len(list((Path(self.git("rev-parse", "--path-format=absolute", "--git-common-dir")) / "bpr-release-builds/reservations").glob("*.json"))), 2)

    def test_os_lock_excludes_concurrent_process_and_recovers_after_termination(self):
        common = self.root / ".git/bpr-release-builds"
        context = multiprocessing.get_context("spawn")
        ready, release = context.Event(), context.Event()
        process = context.Process(target=lock_holder, args=(str(common), ready, release))
        process.start()
        try:
            self.assertTrue(ready.wait(10))
            with self.assertRaisesRegex(ValueError, "another release allocator"):
                self.allocate()
        finally:
            process.terminate()
            process.join(10)
        self.assertEqual(self.allocate()["number"], 1)

    def test_history_floor_prevents_reuse_after_counter_or_branch_rollback(self):
        base = self.git("rev-parse", "HEAD")
        frozen = self.frozen()
        self.git("branch", "retained", frozen["sourceCommit"])
        self.git("reset", "--hard", base)
        # A fresh clone has history but no process journal.
        clone = Path(self.temporary.name) / "clone"
        subprocess.run(["git", "clone", "--quiet", str(self.root), str(clone)], check=True)
        self.assertEqual(cohort.allocation(clone, self.inputs)["number"], 2)

    def test_freeze_rejects_product_changes_merges_and_uncommitted_inputs(self):
        self.allocate()
        (self.root / "product.cpp").write_text("changed\n")
        with self.assertRaisesRegex(ValueError, "commit source/configuration"):
            self.allocate()
        self.commit("wrong allocation slice")
        with self.assertRaisesRegex(ValueError, "contains product"):
            cohort.freeze(self.root, 1)

    @unittest.skipIf(sys.platform == "win32", "publisher preparation shell needs Bash")
    def test_publish_release_preparation_and_freeze_public_commands(self):
        for name in ("tools/publish-release.sh", "packaging/release_build.py"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / name).read_bytes())
        self.commit("public tooling")
        remote = Path(self.temporary.name) / "origin.git"
        subprocess.run(["git", "init", "--bare", "--quiet", str(remote)], check=True)
        self.git("remote", "add", "origin", str(remote))
        self.git("push", "--quiet", "-u", "origin", "main")
        inputs = Path(self.temporary.name) / "inputs.json"
        cohort.write(inputs, self.inputs)
        output = Path(self.temporary.name) / "allocation.json"
        command = ["bash", str(self.root / "tools/publish-release.sh")]
        result = subprocess.run(command + ["--prepare-candidate", "--candidate-inputs", str(inputs), "--cohort-output", str(output)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(cohort.read(output)["number"], 1)
        self.commit("allocation only")
        frozen = Path(self.temporary.name) / "cohort.json"
        result = subprocess.run(command + ["--freeze-candidate", "1", "--cohort-output", str(frozen)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        cohort.check(cohort.read(frozen), self.root)
        self.assertEqual(self.git("status", "--porcelain"), "")

    def test_allocation_exhaustion_changes_nothing(self):
        cohort.write(self.root / cohort.COUNTER, {"schema": "bpr.release-counter.v1", "product": cohort.PRODUCT, "last": 9999})
        self.commit("exhausted history")
        before = self.git("rev-parse", "HEAD")
        with self.assertRaisesRegex(ValueError, "1..9999"):
            self.allocate()
        self.assertEqual(self.git("rev-parse", "HEAD"), before)
        self.assertEqual(self.git("status", "--porcelain"), "")

    def test_actual_producer_rejects_changed_flags_toolchain_or_source(self):
        frozen = self.frozen()
        declared = self.inputs["producers"]["mac"]
        observed = {key: declared[key] for key in ("platform", "architectures", "buildType", "options", "toolchain", "settings")}
        observed["version"] = "0.4.1"
        self.assertEqual(cohort.configure(frozen, self.root, observed, "mac")["number"], 1)
        for key, value in [("buildType", "Debug"), ("architectures", ["x86_64"])]:
            altered = copy.deepcopy(observed)
            altered[key] = value
            with self.assertRaisesRegex(ValueError, "actual CMake"):
                cohort.configure(frozen, self.root, altered, "mac")
        altered = copy.deepcopy(observed)
        altered["options"]["BPR_BUILD_AGENT_INTEGRATIONS"] = "ON"
        with self.assertRaisesRegex(ValueError, "actual CMake"):
            cohort.configure(frozen, self.root, altered, "mac")

    @unittest.skipUnless(shutil.which("cmake"), "native CMake is unavailable")
    def test_real_cmake_module_consumes_frozen_identity_and_local_build_consumes_nothing(self):
        for name in ("BprReleaseCohort.cmake", "BprBuildIdentity.cmake"):
            destination = self.root / "cmake" / name
            destination.parent.mkdir(exist_ok=True)
            shutil.copyfile(ROOT / "cmake" / name, destination)
        (self.root / "packaging/release_build.py").write_bytes((ROOT / "packaging/release_build.py").read_bytes())
        (self.root / ".gitignore").write_text("/build/\n")
        options = "\n".join(f'option({name} "fixture option" OFF)' for name in cohort.OPTIONS)
        (self.root / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.25)
project(
 BetterPullReview
 VERSION 0.4.1
 LANGUAGES CXX
)
''' + options + '''
set(BPR_PRODUCT_QT_VERSION "6.12.0")
include(cmake/BprReleaseCohort.cmake)
include(cmake/BprBuildIdentity.cmake)
add_executable(probe probe.cpp)
bpr_add_build_identity(probe)
''')
        (self.root / "probe.cpp").write_text('#include "bpr_build_identity.hpp"\n#include <iostream>\nint main(){std::cout<<BPR_RELEASE_BUILD_NUMBER;}\n')
        self.commit("native CMake identity composition fixture")
        build = self.root / "build"
        def run(*args):
            result = subprocess.run(args, cwd=self.root, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return result.stdout.strip()
        run("cmake", "-S", str(self.root), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release")
        run("cmake", "--build", str(build), "--config", "Release")
        candidates = [build / "probe.exe", build / "Release/probe.exe"] if os.name == "nt" else [build / "probe"]
        executable = next(path for path in candidates if path.is_file())
        self.assertEqual(run(str(executable)), "0")
        self.assertEqual(cohort.counter_value(self.root), 0)
        observed = cohort.read(build / "_bpr/release-observed.json")
        self.inputs["producers"] = {"native": {key: value for key, value in observed.items() if key != "version"} | {"preset": "fixture"}}
        frozen = self.frozen()
        external = Path(self.temporary.name) / "native-cohort.json"
        cohort.write(external, frozen)
        run("cmake", "-S", str(self.root), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release",
            "-DBPR_RELEASE_COHORT=" + str(external), "-DBPR_RELEASE_PRODUCER=native")
        run("cmake", "--build", str(build), "--config", "Release")
        self.assertEqual(run(str(executable)), "1")
        self.assertEqual(self.allocate(), frozen["allocation"])
        (self.root / "probe.cpp").write_text("#error changed source must not compile\n")
        refused = subprocess.run(["cmake", "--build", str(build), "--config", "Release"], capture_output=True, text=True)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("release cohort source/configuration binding mismatch", refused.stdout + refused.stderr)
        self.assertNotIn("#error changed source", refused.stdout + refused.stderr)

    def test_native_mapping_bounds_and_actual_legacy_msi_order(self):
        self.assertEqual(cohort.native("0.4.1", 1, self.inputs["previousWindows"]), {
            "displayVersion": "0.4.1 (build 1)", "assetVersion": "0.4.1-build.1", "macBundleVersion": "1",
            "releaseTag": "v0.4.1-build.1", "msiProductCode": cohort.native("0.4.1", 1)["msiProductCode"],
            "windowsImageVersion": "0.4.1.1", "msiProductVersion": "0.4.1", "debianVersion": "0.4.1+build.1"})
        for version, number in [("0.4.1", 0), ("0.4.1", 10000), ("256.4.1", 1), ("0.256.1", 1), ("0.4.65536", 1)]:
            with self.assertRaises(ValueError):
                cohort.native(version, number)
        with self.assertRaisesRegex(ValueError, "downgrade"):
            cohort.native("0.3.9", 9999, self.inputs["previousWindows"])
        with self.assertRaisesRegex(ValueError, "MSI upgrade"):
            cohort.native("0.4.2", 1, {"marketingVersion": "0.4.1", "msiProductVersion": "0.4.1"})

    def test_global_number_crosses_marketing_patch_without_msi_collision(self):
        project = self.root / "CMakeLists.txt"
        project.write_text(project.read_text().replace("0.4.1", "0.4.0"))
        self.commit("first marketing version")
        first = self.frozen()["allocation"]
        self.assertEqual(first["native"]["msiProductVersion"], "0.4.1")
        self.assertNotEqual(cohort.native("0.4.0", 1)["msiProductCode"], cohort.native("0.4.0", 2)["msiProductCode"])
        self.assertEqual(cohort.native("0.4.0", 1)["msiProductCode"], cohort.native("0.4.0", 1)["msiProductCode"])
        observed = windows_floor() | {"msiProductVersion": "0.4.1", "releaseTag": "v0.4.0-build.1",
                                     "installerName": "BetterPullReview-0.4.0-build.1-windows-x64.msi",
                                     "sourceCommit": self.git("rev-parse", "HEAD"), "installerSHA256": "a" * 64, "releaseId": 43}
        cohort.write(self.root / cohort.WINDOWS_FLOOR, observed)
        project.write_text(project.read_text().replace("0.4.0", "0.4.1"))
        self.commit("observed prior numbered MSI and next patch")
        self.inputs["previousWindows"] = cohort.floor_identity(observed)
        second = self.allocate()
        self.assertEqual((second["number"], second["native"]["msiProductVersion"]), (2, "0.4.2"))
        self.commit("second allocation")
        self.assertEqual(self.allocate(), second)

    def test_legacy_floor_refuses_insufficient_number_without_reservation(self):
        observed = windows_floor() | {"marketingVersion": "0.4.7", "msiProductVersion": "0.4.7",
                                     "releaseTag": "v0.4.7", "installerName": "BetterPullReview-0.4.7-windows-x64.msi"}
        cohort.write(self.root / cohort.WINDOWS_FLOOR, observed)
        project = self.root / "CMakeLists.txt"
        project.write_text(project.read_text().replace("0.4.1", "0.4.7"))
        self.commit("observed legacy MSI above fresh number")
        self.inputs["previousWindows"] = cohort.floor_identity(observed)
        with self.assertRaisesRegex(ValueError, "MSI upgrade"):
            self.allocate()
        self.assertEqual(cohort.counter_value(self.root), 0)
        self.assertEqual(self.git("status", "--porcelain"), "")
        self.assertEqual(list((self.root / ".git/bpr-release-builds/reservations").glob("*.json")), [])
        # An operator cannot select an older input or edit the tracked floor backwards.
        self.inputs["previousWindows"] = cohort.floor_identity(windows_floor())
        with self.assertRaisesRegex(ValueError, "authoritative"):
            self.allocate()
        cohort.write(self.root / cohort.WINDOWS_FLOOR, windows_floor())
        self.commit("attempted floor rollback")
        with self.assertRaisesRegex(ValueError, "roll back"):
            self.allocate()
        self.assertEqual(cohort.counter_value(self.root), 0)

    def test_unknown_windows_floor_is_fail_closed(self):
        cohort.write(self.root / cohort.WINDOWS_FLOOR, {})
        self.commit("unknown prior observation")
        with self.assertRaisesRegex(ValueError, "floor"):
            self.allocate()
        self.assertEqual(cohort.counter_value(self.root), 0)

    def test_configuration_mixed_cohort_and_tampered_source_are_refused(self):
        frozen = self.frozen()
        for field in ("number", "configurationDigest", "sourceBase"):
            changed = copy.deepcopy(frozen)
            changed["allocation"][field] = 2 if field == "number" else "b" * 64
            with self.assertRaises(ValueError):
                cohort.check(changed)
        (self.root / "product.cpp").write_text("changed\n")
        with self.assertRaises(ValueError):
            cohort.check(frozen, self.root)
        missing = copy.deepcopy(self.inputs)
        missing["producers"]["mac"]["options"].pop("BPR_BUILD_QT")
        with self.assertRaisesRegex(ValueError, "all release feature"):
            cohort.configuration(missing)


if __name__ == "__main__":
    unittest.main()
