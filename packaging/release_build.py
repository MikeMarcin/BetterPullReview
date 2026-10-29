#!/usr/bin/env python3
"""BPR release-cohort allocation, freeze and producer validation; local builds never allocate."""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

PRODUCT = "BetterPullReview"
DIRECTORY = Path("packaging/release-builds")
COUNTER = DIRECTORY / "counter.json"
MAXIMUM = 9999
OPTIONS = ("BUILD_TESTING", "BPR_DISTRIBUTION_BUILD", "BPR_BUILD_QT", "BPR_BUILD_TOUR_CLI",
           "BPR_BUILD_AGENT_INTEGRATIONS", "BPR_BUILD_LOCAL_AUTOMATION", "BPR_EXTERNAL_QT_RESOURCES")
CONFIG_FILES = ("CMakePresets.json", "packaging/dependencies.json")


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True, allow_nan=False).encode("ascii")


def digest(value: object) -> str:
    return hashlib.sha256(canonical(value)).hexdigest()


def read(path: Path) -> dict:
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON member: " + key)
            result[key] = value
        return result
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)


def write(path: Path, value: dict) -> None:
    """Replace durably on the same volume, including the directory entry on POSIX."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".bpr-", delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(json.dumps(value, indent=2, sort_keys=True).encode("utf-8") + b"\n")
            stream.flush()
            os.fsync(stream.fileno())
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    try:
        os.replace(temporary, path)
        if os.name != "nt":
            descriptor = os.open(path.parent, os.O_RDONLY)
            try:
                os.fsync(descriptor)
            finally:
                os.close(descriptor)
    finally:
        temporary.unlink(missing_ok=True)


def git(root: Path, *args: str) -> str:
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(root), *args], text=True).strip()


def marketing(root: Path, revision: str = "HEAD") -> str:
    source = git(root, "show", revision + ":CMakeLists.txt")
    match = re.search(r"(?ms)^project\(\s*BetterPullReview\s+VERSION\s+(\d+\.\d+\.\d+)\s", source)
    if not match:
        raise ValueError("BPR three-component project version required")
    return match[1]


def native(version: str, number: int, previous: dict | None = None) -> dict:
    if not re.fullmatch(r"\d+\.\d+\.\d+", version) or type(number) is not int or not 1 <= number <= MAXIMUM:
        raise ValueError("release build number must be 1..9999 and version must have three components")
    major, minor, patch = map(int, version.split("."))
    if major > 255 or minor > 255 or patch > 65535:
        raise ValueError("marketing version exceeds MSI/PE numeric bounds")
    msi = (major, minor, number)
    if previous:
        if set(previous) != {"marketingVersion", "msiProductVersion"}:
            raise ValueError("previous Windows identity requires actual marketing and MSI versions")
        if any(not isinstance(item, str) or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", item) for item in previous.values()):
            raise ValueError("invalid previous Windows identity")
        try:
            old_marketing = tuple(map(int, previous["marketingVersion"].split(".")))
            old_msi = tuple(map(int, previous["msiProductVersion"].split(".")))
        except (ValueError, AttributeError) as error:
            raise ValueError("invalid previous Windows identity") from error
        if len(old_marketing) != 3 or len(old_msi) != 3 or (major, minor, patch) < old_marketing or msi <= old_msi:
            raise ValueError("release would downgrade marketing version or fail MSI upgrade ordering")
    return {"displayVersion": f"{version} (build {number})", "assetVersion": f"{version}-build.{number}",
            "macBundleVersion": str(number), "windowsImageVersion": f"{version}.{number}",
            "msiProductVersion": ".".join(map(str, msi)), "debianVersion": f"{version}+build.{number}"}


def source_inputs(root: Path, revision: str) -> dict:
    return {name: hashlib.sha256(subprocess.check_output(["git", "-C", str(root), "show", revision + ":" + name])).hexdigest()
            for name in CONFIG_FILES}


def counter_value(root: Path) -> int:
    value = read(root / COUNTER)
    number = value.get("last")
    if value != {"schema": "bpr.release-counter.v1", "product": PRODUCT, "last": number} or type(number) is not int or not 0 <= number <= MAXIMUM:
        raise ValueError("invalid BPR release counter")
    return number


def configuration(value: dict) -> dict:
    if set(value) != {"schema", "producers", "previousWindows"} or value["schema"] != "bpr.release-inputs.v1":
        raise ValueError("complete bpr.release-inputs.v1 configuration required")
    if not isinstance(value["producers"], dict) or not value["producers"]:
        raise ValueError("declare at least one release producer")
    for name, producer in value["producers"].items():
        if not re.fullmatch(r"[a-z][a-z0-9-]{0,63}", name) or set(producer) != {"platform", "architectures", "buildType", "options", "toolchain", "preset", "settings"}:
            raise ValueError("invalid producer identity or incomplete declaration")
        if producer["platform"] not in {"Darwin", "Windows", "Linux"} or producer["buildType"] not in {"Release", "Debug"}:
            raise ValueError("invalid release producer platform/configuration")
        if not producer["architectures"] or any(item not in {"arm64", "x86_64", "AMD64"} for item in producer["architectures"]):
            raise ValueError("declare native producer architectures")
        if (not set(OPTIONS) <= set(producer["options"])
                or any(not re.fullmatch(r"(?:BUILD_TESTING|BPR_[A-Z0-9_]+)", key) for key in producer["options"])
                or any(item not in {"ON", "OFF"} for item in producer["options"].values())):
            raise ValueError("declare all release feature options as ON/OFF")
        if (not isinstance(producer["settings"], dict) or not producer["settings"]
                or any(not re.fullmatch(r"CMAKE_[A-Z0-9_]+", key) or not isinstance(item, str) for key, item in producer["settings"].items())):
            raise ValueError("declare effective native compiler/linker/SDK settings")
        if set(producer["toolchain"]) != {"cmake", "compilerId", "compilerVersion", "qtVersion"}:
            raise ValueError("declare CMake, compiler and Qt producer versions")
        for item in [producer["preset"], *producer["toolchain"].values()]:
            if not isinstance(item, str) or not re.fullmatch(r"[A-Za-z0-9_.+-]+", item):
                raise ValueError("toolchain/preset facts must be versions/names, not paths or timestamps")
    if any(item["platform"] == "Windows" for item in value["producers"].values()) and not value["previousWindows"]:
        raise ValueError("Windows cohorts require the previous actual MSI identity")
    return value


@contextmanager
def reservation_lock(common: Path):
    """The OS releases this process-exclusive lock after an interrupted allocator."""
    common.mkdir(parents=True, exist_ok=True)
    with (common / "allocation.lock").open("a+b") as stream:
        if stream.tell() == 0:
            stream.write(b"\0")
            stream.flush()
        stream.seek(0)
        try:
            if os.name == "nt":
                import msvcrt
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise ValueError("another release allocator owns the common Git reservation lock") from error
        try:
            yield
        finally:
            if os.name == "nt":
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def receipt_path(number: int) -> Path:
    return DIRECTORY / "allocations" / f"{number:04d}.json"


def changed_paths(root: Path) -> set[str]:
    # -z keeps spaces and non-ASCII file names intact; no shell parses them.
    tracked = git(root, "diff", "--name-only", "-z", "HEAD").split("\0")
    untracked = git(root, "ls-files", "--others", "--exclude-standard", "-z").split("\0")
    return set(tracked + untracked) - {""}


def validate_allocation_commit(root: Path, receipt: dict) -> str:
    revision = git(root, "rev-parse", "HEAD")
    if (receipt["sourceTree"] != git(root, "rev-parse", receipt["sourceBase"] + "^{tree}")
            or receipt["sourceInputs"] != source_inputs(root, receipt["sourceBase"])
            or receipt["version"] != marketing(root, receipt["sourceBase"])):
        raise ValueError("allocation source base/configuration digest mismatch")
    if git(root, "rev-list", "--parents", "-n", "1", revision).split() != [revision, receipt["sourceBase"]]:
        raise ValueError("freeze requires the allocation-only commit directly over its source base")
    expected = {COUNTER.as_posix(), receipt_path(receipt["number"]).as_posix()}
    if set(git(root, "diff", "--name-only", receipt["sourceBase"], revision).splitlines()) != expected:
        raise ValueError("allocation commit contains product/configuration changes; allocate a fresh cohort")
    if read(root / COUNTER) != {"schema": "bpr.release-counter.v1", "product": PRODUCT, "last": receipt["number"]}:
        raise ValueError("allocation counter differs from receipt")
    return revision


def allocation(root: Path, inputs: dict) -> dict:
    if git(root, "branch", "--show-current") != "main":
        raise ValueError("release allocation requires authoritative reconciled main")
    inputs = configuration(inputs)
    version = marketing(root)
    config_digest = digest(inputs)
    head = git(root, "rev-parse", "HEAD")
    # Repeating preparation at a frozen allocation commit consumes nothing.
    if (root / COUNTER).exists() and not changed_paths(root):
        last = counter_value(root)
        path = root / receipt_path(last)
        if last and path.exists():
            prior = read(path)
            if prior["configurationDigest"] == config_digest and prior["version"] == version:
                try:
                    validate_allocation_commit(root, prior)
                    return prior
                except ValueError:
                    pass
    files = source_inputs(root, head)
    identity = {"sourceBase": head, "sourceTree": git(root, "rev-parse", head + "^{tree}"),
                "version": version, "configuration": inputs, "configurationDigest": config_digest, "sourceInputs": files}
    key = digest(identity)
    common = Path(git(root, "rev-parse", "--path-format=absolute", "--git-common-dir")) / "bpr-release-builds"
    with reservation_lock(common):
        journal = common / "reservations"
        journal.mkdir(exist_ok=True)
        reservations = [read(path) for path in journal.glob("*.json")]
        existing = [item for item in reservations if item["inputDigest"] == key]
        if len(existing) > 1:
            raise ValueError("duplicate release reservation identity")
        names = git(root, "log", "--all", "--format=", "--name-only", "--", str(DIRECTORY)).splitlines()
        history = [int(match[1]) for name in names if (match := re.fullmatch(r"packaging/release-builds/allocations/(\d{4})\.json", name))]
        counter = counter_value(root) if (root / COUNTER).exists() else 0
        floor = max([counter, *history, *(item["number"] for item in reservations)])
        number = existing[0]["number"] if existing else floor + 1
        mappings = native(version, number, inputs["previousWindows"])
        receipt = {"schema": "bpr.release-allocation.v1", "product": PRODUCT, "number": number,
                   "inputDigest": key, **identity, "native": mappings}
        expected_paths = {COUNTER.as_posix(), receipt_path(number).as_posix()}
        if changed_paths(root) - expected_paths:
            raise ValueError("commit source/configuration changes before allocating")
        for path in expected_paths & changed_paths(root):
            expected = receipt if path == receipt_path(number).as_posix() else {"schema": "bpr.release-counter.v1", "product": PRODUCT, "last": number}
            if read(root / path) != expected:
                raise ValueError("unfinished allocation files differ from their durable reservation")
        if not existing:
            write(journal / f"{number:04d}.json", receipt)
        elif existing[0] != receipt:
            raise ValueError("reserved release input identity changed")
        target = root / receipt_path(number)
        if target.exists() and read(target) != receipt:
            raise ValueError("immutable allocation receipt already differs")
        write(target, receipt)
        write(root / COUNTER, {"schema": "bpr.release-counter.v1", "product": PRODUCT, "last": number})
        return receipt


def freeze(root: Path, number: int) -> dict:
    if changed_paths(root):
        raise ValueError("commit the allocation slice before freezing")
    receipt = read(root / receipt_path(number))
    revision = validate_allocation_commit(root, receipt)
    value = {"schema": "bpr.release-cohort.v1", "product": PRODUCT, "sourceCommit": revision,
             "sourceTree": git(root, "rev-parse", revision + "^{tree}"),
             "allocation": receipt, "allocationDigest": digest(receipt)}
    return {**value, "cohortDigest": digest(value)}


def check(cohort: dict, root: Path | None = None, version: str | None = None) -> dict:
    value = dict(cohort)
    seal = value.pop("cohortDigest", None)
    if seal != digest(value) or value.get("schema") != "bpr.release-cohort.v1" or value.get("product") != PRODUCT:
        raise ValueError("invalid release cohort seal/product")
    if set(value) != {"schema", "product", "sourceCommit", "sourceTree", "allocation", "allocationDigest"}:
        raise ValueError("unknown or missing release cohort fields")
    receipt = value["allocation"]
    if set(receipt) != {"schema", "product", "number", "inputDigest", "sourceBase", "sourceTree", "version", "configuration", "configurationDigest", "sourceInputs", "native"}:
        raise ValueError("unknown or missing release allocation fields")
    configuration(receipt["configuration"])
    identity = {key: receipt[key] for key in ("sourceBase", "sourceTree", "version", "configuration", "configurationDigest", "sourceInputs")}
    if (value["allocationDigest"] != digest(receipt) or receipt["inputDigest"] != digest(identity)
            or receipt["configurationDigest"] != digest(receipt["configuration"])
            or receipt["native"] != native(receipt["version"], receipt["number"], receipt["configuration"]["previousWindows"])
            or receipt.get("schema") != "bpr.release-allocation.v1" or receipt.get("product") != PRODUCT):
        raise ValueError("invalid allocation/configuration/native identity")
    for item in (value["sourceCommit"], value["sourceTree"], receipt["sourceBase"], receipt["sourceTree"]):
        if not isinstance(item, str) or not re.fullmatch(r"[0-9a-f]{40}", item):
            raise ValueError("full Git source/tree identities required")
    if (set(receipt["sourceInputs"]) != set(CONFIG_FILES)
            or any(not re.fullmatch(r"[0-9a-f]{64}", item) for item in receipt["sourceInputs"].values())):
        raise ValueError("exact source configuration input digests required")
    if version is not None and version != receipt["version"]:
        raise ValueError("release cohort marketing version mismatch")
    if root is not None:
        if changed_paths(root) or freeze(root, receipt["number"]) != cohort:
            raise ValueError("release cohort source/configuration binding mismatch")
    return receipt


def binding(cohort: dict) -> dict:
    receipt = check(cohort)
    return {"number": receipt["number"], "configurationDigest": receipt["configurationDigest"],
            "allocationDigest": cohort["allocationDigest"], "cohortDigest": cohort["cohortDigest"]}


def configure(cohort: dict, root: Path, observed: dict, producer: str) -> dict:
    receipt = check(cohort, root, observed["version"])
    declared = receipt["configuration"]["producers"].get(producer)
    if declared is None:
        raise ValueError("producer is not part of this release cohort")
    actual = {key: observed[key] for key in ("platform", "architectures", "buildType", "options", "toolchain", "settings")}
    expected = {key: declared[key] for key in actual}
    if actual != expected:
        raise ValueError("actual CMake producer options/toolchain/architectures differ from the frozen cohort")
    return {**binding(cohort), "sourceCommit": cohort["sourceCommit"], "version": receipt["version"], "native": receipt["native"]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("allocate", "freeze", "check", "configure"))
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--inputs", type=Path)
    parser.add_argument("--number", type=int)
    parser.add_argument("--cohort", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--observed", type=Path)
    parser.add_argument("--producer")
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    if args.action == "allocate":
        result = allocation(root, read(args.inputs))
    elif args.action == "freeze":
        result = freeze(root, args.number)
    elif args.action == "configure":
        result = configure(read(args.cohort), root, read(args.observed), args.producer)
    else:
        cohort = read(args.cohort)
        check(cohort, root, args.version)
        result = binding(cohort)
    if args.output:
        if args.action != "configure" and args.output.resolve().is_relative_to(root):
            raise ValueError("cohort/result output must be outside source")
        if args.action != "configure" and args.output.exists() and read(args.output) != result:
            raise ValueError("existing external identity differs; use a new output")
        write(args.output, result)
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"release build refused: {error}") from error
