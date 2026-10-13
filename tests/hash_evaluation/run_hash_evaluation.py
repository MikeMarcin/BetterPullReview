#!/usr/bin/env python3
"""[[BPR-1446]] Drive RFC-BPR-0080's hash evaluation and write its raw results.

Runs the evaluation harness (`bench_hash_evaluation`, output name
`hash_evaluation`) built with -DBPR_HASH_EVALUATION=ON, together with the
installed Git, and writes one JSON document: the machine, the build, every
line the harness printed, and the comparisons this script made against Git.

    run_hash_evaluation.py --build BUILD_DIR --repository REPO --output FILE
        [--phases correctness,git,sweep,throughput,window]

Nothing here touches the network or a remote. Scratch files live in a fresh
owner-private temporary directory that this script removes.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import random
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

if sys.version_info < (3, 10):
    raise SystemExit(f"Python 3.10 or newer is required; found {platform.python_version()} at {sys.executable}")

ALL_PHASES = ("correctness", "git", "sweep", "sweep256", "throughput", "window")
SEED = 20261013
MIB = 1 << 20


def run(arguments: list[str], **options) -> subprocess.CompletedProcess:
    return subprocess.run(arguments, check=False, capture_output=True, **options)


def harness_lines(output: bytes) -> list[dict]:
    return [json.loads(line) for line in output.decode().splitlines() if line.startswith("{")]


def text(arguments: list[str]) -> str:
    result = run(arguments, text=True)
    return result.stdout.strip() if result.returncode == 0 else ""


def machine() -> dict:
    facts = {
        "platform": platform.platform(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "git": text(["git", "--version"]),
        "load_average": text(["sysctl", "-n", "vm.loadavg"]) if sys.platform == "darwin" else Path("/proc/loadavg").read_text().strip() if Path("/proc/loadavg").exists() else "",
    }
    if sys.platform == "darwin":
        facts["cpu"] = text(["sysctl", "-n", "machdep.cpu.brand_string"])
        facts["os"] = " ".join(text(["sw_vers", key]) for key in ("-productName", "-productVersion", "-buildVersion"))
        facts["sha_extensions"] = {
            name: text(["sysctl", "-n", f"hw.optional.arm.{name}"]) for name in ("FEAT_SHA1", "FEAT_SHA256", "FEAT_SHA512", "FEAT_SHA3")
        }
        facts["logical_cpus"] = text(["sysctl", "-n", "hw.logicalcpu"])
    elif Path("/proc/cpuinfo").exists():
        cpuinfo = Path("/proc/cpuinfo").read_text()
        model = next((line.split(":", 1)[1].strip() for line in cpuinfo.splitlines() if line.startswith("model name")), "")
        flags = next((line.split(":", 1)[1].split() for line in cpuinfo.splitlines() if line.startswith(("flags", "Features"))), [])
        facts["cpu"] = model
        facts["sha_extensions"] = {flag: flag in flags for flag in ("sha_ni", "sha1", "sha2")}
    return facts


def build_facts(build: Path) -> dict:
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if "=" in line and ":" in line.split("=", 1)[0]:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    compiler = cache.get("CMAKE_CXX_COMPILER", "")
    return {
        "build_type": cache.get("CMAKE_BUILD_TYPE", ""),
        "cxx_compiler": compiler,
        "cxx_compiler_version": text([compiler, "--version"]).splitlines()[0] if compiler else "",
        "cxx_flags": cache.get("CMAKE_CXX_FLAGS", ""),
        "cxx_flags_release": cache.get("CMAKE_CXX_FLAGS_RELEASE", ""),
        "c_flags_release": cache.get("CMAKE_C_FLAGS_RELEASE", ""),
        "osx_architectures": cache.get("CMAKE_OSX_ARCHITECTURES", ""),
    }


def seeded_files(directory: Path, large: bool) -> list[Path]:
    generator = random.Random(SEED)
    sizes = [0, 1, 55, 56, 63, 64, 65, 119, 120, 1000, 4096, 65536, MIB, 16 * MIB]
    sizes += [generator.randrange(0, MIB) for _ in range(40)]
    if large:
        sizes.append(256 * MIB)
    files = []
    for index, size in enumerate(sizes):
        path = directory / f"d{index % 5}" / f"e{index % 3}" / f"blob-{index:03}-{size}.bin"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(generator.randbytes(size))
        files.append(path)
    return files


GIT_IDENTITY = {
    "GIT_AUTHOR_NAME": "Hash Evaluation",
    "GIT_AUTHOR_EMAIL": "hash-evaluation@example.invalid",
    "GIT_AUTHOR_DATE": "2026-10-13T12:00:00Z",
    "GIT_COMMITTER_NAME": "Hash Evaluation",
    "GIT_COMMITTER_EMAIL": "hash-evaluation@example.invalid",
    "GIT_COMMITTER_DATE": "2026-10-13T12:00:00Z",
    "GIT_CONFIG_NOSYSTEM": "1",
}


def git_phase(harness: Path, scratch: Path, large: bool) -> dict:
    """Blob identities against `git hash-object`, and trees and commits built
    by `write-tree`, `mktree` and `commit-tree`, in both object formats."""
    content = scratch / "content"
    files = seeded_files(content, large)
    environment = {**os.environ, **GIT_IDENTITY, "HOME": str(scratch)}
    ids = {line["file"]: line for line in harness_lines(run([str(harness), "object-ids", *map(str, files)]).stdout) if "file" in line}
    result: dict = {"files": len(files), "bytes": sum(path.stat().st_size for path in files), "formats": {}}
    for object_format in ("sha1", "sha256"):
        repository = scratch / f"repository-{object_format}"
        run(["git", "init", "-q", f"--object-format={object_format}", str(repository)], env=environment)
        git = ["git", "--git-dir", str(repository / ".git"), "--work-tree", str(content)]
        environment["PWD"] = str(content)
        hashed = run([*git, "hash-object", "-w", "--no-filters", *map(str, files)], env=environment, text=True, cwd=content)
        git_ids = hashed.stdout.split()
        mismatches: dict[str, int] = {}
        arms: set[str] = set()
        for path, git_id in zip(files, git_ids):
            for arm, value in ids[str(path)][object_format].items():
                if arm.endswith("-detected"):
                    mismatches[arm] = mismatches.get(arm, 0) + 1
                    continue
                arms.add(arm)
                if value != git_id:
                    mismatches[arm] = mismatches.get(arm, 0) + 1
        run([*git, "add", "-A"], env=environment, cwd=content)
        tree = run([*git, "write-tree"], env=environment, text=True).stdout.strip()
        listing = run([*git, "ls-tree", tree], env=environment).stdout
        made = run([*git, "mktree"], env=environment, input=listing).stdout.decode().strip()
        commit = run([*git, "commit-tree", "-m", "hash evaluation", tree], env=environment, text=True).stdout.strip()
        second = run([*git, "commit-tree", "-p", commit, "-m", "second", tree], env=environment, text=True).stdout.strip()
        batch = run(["git", "-C", str(repository), "cat-file", "--batch-all-objects", "--batch"], env=environment)
        sweep = run([str(harness), "sweep", object_format], input=batch.stdout)
        result["formats"][object_format] = {
            "hash_object_count": len(git_ids),
            "arms": sorted(arms),
            "blob_mismatches": mismatches,
            "write_tree": tree,
            "mktree_equals_write_tree": made == tree,
            "commits": [commit, second],
            "sweep_exit": sweep.returncode,
            "sweep": harness_lines(sweep.stdout),
        }
    return result


def sweep_phase(harness: Path, repository: Path) -> dict:
    """Every object in this repository's object store, in its SHA-1 format."""
    started = time.monotonic()
    batch = subprocess.Popen(["git", "-C", str(repository), "cat-file", "--batch-all-objects", "--batch"], stdout=subprocess.PIPE)
    sweep = run([str(harness), "sweep", "sha1"], stdin=batch.stdout)
    batch.wait()
    return {
        "repository_head": text(["git", "-C", str(repository), "rev-parse", "HEAD"]),
        "object_format": text(["git", "-C", str(repository), "rev-parse", "--show-object-format"]),
        "cat_file_exit": batch.returncode,
        "sweep_exit": sweep.returncode,
        "seconds": round(time.monotonic() - started, 1),
        "lines": harness_lines(sweep.stdout),
    }


def sweep256_phase(harness: Path, repository: Path, scratch: Path) -> dict:
    """`main`'s history converted into a SHA-256 repository by fast-import, so
    the SHA-256 arms recompute every object of a real history too."""
    started = time.monotonic()
    converted = scratch / "converted.git"
    run(["git", "init", "-q", "--bare", "--object-format=sha256", str(converted)])
    export = subprocess.Popen(
        ["git", "-C", str(repository), "fast-export", "--signed-tags=strip", "--tag-of-filtered-object=drop", "--reencode=no", "main"], stdout=subprocess.PIPE
    )
    imported = run(["git", "-C", str(converted), "fast-import", "--quiet"], stdin=export.stdout)
    export.wait()
    batch = subprocess.Popen(["git", "-C", str(converted), "cat-file", "--batch-all-objects", "--batch"], stdout=subprocess.PIPE)
    sweep = run([str(harness), "sweep", "sha256"], stdin=batch.stdout)
    batch.wait()
    return {
        "source_head": text(["git", "-C", str(repository), "rev-parse", "main"]),
        "converted_head": text(["git", "-C", str(converted), "rev-parse", "main"]),
        "fast_export_exit": export.returncode,
        "fast_import_exit": imported.returncode,
        "sweep_exit": sweep.returncode,
        "seconds": round(time.monotonic() - started, 1),
        "lines": harness_lines(sweep.stdout),
    }


def window_phase(build: Path) -> dict:
    """The window-thread baseline a later adoption is compared with."""
    rows = {}
    for name in ("application_feedback_counter_costs",):
        result = run(["ctest", "--test-dir", str(build), "-R", f"^{name}$", "-j1", "--output-on-failure", "-V"], text=True)
        rows[name] = {"exit": result.returncode, "output": [line for line in result.stdout.splitlines() if line.split(": ", 1)[-1].startswith("feedback-cost")]}
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--repository", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--phases", default=",".join(ALL_PHASES))
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--no-large", action="store_true", help="skip the 256 MiB inputs")
    options = parser.parse_args()
    phases = options.phases.split(",")
    unknown = sorted(set(phases) - set(ALL_PHASES))
    if unknown:
        parser.error(f"unknown phases: {', '.join(unknown)}")
    build = options.build.resolve()
    harness = build / "tests" / "hash_evaluation" / ("hash_evaluation.exe" if os.name == "nt" else "hash_evaluation")
    vectors = build / "_bpr" / "hash-evaluation"
    collisions = next(vectors.glob("sha1dc/sha1collisiondetection-*/test"))
    large = not options.no_large

    document: dict = {
        "schema": "bpr.hash-evaluation.v1",
        "rfc": "RFC-BPR-0080",
        "card": "BPR-1446",
        "revision": text(["git", "-C", str(Path(__file__).resolve().parent), "rev-parse", "HEAD"]),
        "dirty": bool(text(["git", "-C", str(Path(__file__).resolve().parent), "status", "--porcelain", "--untracked-files=no"])),
        "seed": SEED,
        "machine": machine(),
        "build": build_facts(build),
        "phases": {},
    }
    failures = []
    if "correctness" in phases:
        correctness = {}
        for name, arguments in (
            ("kat", ["kat", str(vectors / "cavp" / "shabytetestvectors")]),
            ("chunking", ["chunking"]),
            ("collision", ["collision", str(collisions)]),
            ("differential", ["differential", str(SEED), *(["--large"] if large else [])]),
        ):
            started = time.monotonic()
            result = run([str(harness), *arguments])
            correctness[name] = {"exit": result.returncode, "seconds": round(time.monotonic() - started, 1), "lines": harness_lines(result.stdout)}
            if result.returncode != 0:
                failures.append(name)
        document["phases"]["correctness"] = correctness
    if "git" in phases:
        scratch = Path(tempfile.mkdtemp(prefix="bpr-hash-evaluation-"))
        try:
            document["phases"]["git"] = git_phase(harness, scratch, large)
        finally:
            shutil.rmtree(scratch)
        for object_format, facts in document["phases"]["git"]["formats"].items():
            if facts["blob_mismatches"] or not facts["mktree_equals_write_tree"] or facts["sweep_exit"] != 0 or facts["hash_object_count"] != document["phases"]["git"]["files"]:
                failures.append(f"git-{object_format}")
    if "sweep" in phases:
        document["phases"]["sweep"] = sweep_phase(harness, options.repository.resolve())
        if document["phases"]["sweep"]["sweep_exit"] != 0:
            failures.append("sweep")
    if "sweep256" in phases:
        scratch = Path(tempfile.mkdtemp(prefix="bpr-hash-evaluation-"))
        try:
            document["phases"]["sweep256"] = sweep256_phase(harness, options.repository.resolve(), scratch)
        finally:
            shutil.rmtree(scratch)
        if document["phases"]["sweep256"]["sweep_exit"] != 0:
            failures.append("sweep256")
    if "throughput" in phases:
        document["machine"]["load_average_before_throughput"] = machine()["load_average"]
        result = run([str(harness), "throughput", str(options.runs), str(SEED)])
        document["phases"]["throughput"] = {"exit": result.returncode, "runs": options.runs, "lines": harness_lines(result.stdout)}
        document["machine"]["load_average_after_throughput"] = machine()["load_average"]
    if "window" in phases:
        document["phases"]["window"] = window_phase(build)
    document["failures"] = failures
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(json.dumps(document, indent=1) + "\n")
    print(json.dumps({"output": str(options.output), "failures": failures}))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
