"""Synthetic sealed cohorts for offline artifact/remote-command fixtures.

Real Git source binding and the prepare/freeze CLI are exercised separately in
release_build_tests. Existing publisher courses substitute Git and GitHub; their
checker bridge retains real seal/native/config checks without pretending that
fixture Git objects are real commits.
"""
from pathlib import Path
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "packaging"))
import release_build


def cohort(version: str, revision: str, number: int = 1) -> dict:
    producers = {}
    for name, platform in [("mac", "Darwin"), ("windows", "Windows"), ("linux", "Linux")]:
        producers[name] = {"platform": platform, "architectures": ["x86_64"], "preset": "fixture", "buildType": "Release",
                           "options": {name: "ON" for name in release_build.OPTIONS}, "settings": {"CMAKE_CXX_FLAGS": ""},
                           "toolchain": {"cmake": "4.4.2", "compilerId": "Clang", "compilerVersion": "22.0.0", "qtVersion": "6.12.0"}}
    configuration = {"schema": "bpr.release-inputs.v1", "producers": producers,
                     "previousWindows": {"marketingVersion": "0.4.0", "msiProductVersion": "0.4.0"}}
    identity = {"sourceBase": "b" * 40, "sourceTree": "c" * 40, "version": version, "configuration": configuration,
                "configurationDigest": release_build.digest(configuration), "sourceInputs": {name: hashlib.sha256(b"fixture").hexdigest() for name in release_build.CONFIG_FILES}}
    receipt = {"schema": "bpr.release-allocation.v1", "product": release_build.PRODUCT, "number": number,
               "inputDigest": release_build.digest(identity), **identity, "native": release_build.native(version, number, configuration["previousWindows"])}
    value = {"schema": "bpr.release-cohort.v1", "product": release_build.PRODUCT, "sourceCommit": revision, "sourceTree": "d" * 40,
             "allocation": receipt, "allocationDigest": release_build.digest(receipt)}
    return {**value, "cohortDigest": release_build.digest(value)}


def checker_bridge(repo: Path) -> None:
    target = repo / "packaging/release_build.py"
    target.write_text(f'''# Offline fixture: Git/GitHub are command substitutes; real source binding has its own course.
import importlib.util, json, sys
from pathlib import Path
spec = importlib.util.spec_from_file_location("bpr_real_release_build", {str(ROOT / "packaging/release_build.py")!r})
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
globals().update({{key: value for key, value in vars(module).items() if not key.startswith("__")}})
if __name__ == "__main__":
    args = sys.argv[1:]
    value = module.read(Path(args[args.index("--cohort") + 1]))
    module.check(value, version=args[args.index("--version") + 1])
    print(json.dumps(module.binding(value)))
''')
