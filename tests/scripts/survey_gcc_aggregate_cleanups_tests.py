#!/usr/bin/env python3
"""Which compilers the GCC aggregate-cleanup survey calibrates, with no compiler.

[[BPR-1025]]. The survey calibrated the compiler of the first entry of the first
database. In the tests-disabled `app-release` database that entry is the vendored
SQLite amalgamation, compiled by `cc`, so calibration linked the C++ reproducer
with the C driver and failed. These tests drive the survey's `main` with its
compiler-facing steps replaced: `compiler_family`, `calibrate` and `survey_unit`
record what they are asked and answer as GCC would. So they pin the selection
itself: a database whose first entry is C calibrates the C++ compiler, two C++
compilers are both calibrated before any unit is surveyed, and a unit is surveyed
only after its compiler calibrated.
"""

from __future__ import annotations

import concurrent.futures
import contextlib
import importlib.util
import io
import json
import shlex
import sys
import threading
import unittest
from pathlib import Path
from unittest import mock

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import ContainedTemporaryDirectory, contained_temporary_directory  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "survey_gcc_aggregate_cleanups.py"
SPEC = importlib.util.spec_from_file_location("survey_gcc_aggregate_cleanups", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load the survey: {SCRIPT}")
SURVEY = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = SURVEY
SPEC.loader.exec_module(SURVEY)

CC = "/usr/bin/cc"
CXX = "/usr/bin/c++"
CXX_14 = "/usr/bin/g++-14"
CLANG = "/usr/bin/clang++"


class SurveySelectionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory(prefix="bpr-survey-selection-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.calibrated: list[str] = []
        self.surveyed: list[tuple[str, str]] = []
        self.lock = threading.Lock()
        self.families = {CC: "gcc", CXX: "gcc", CXX_14: "gcc", CLANG: "clang"}
        self.failing: set[str] = set()
        self.unaffected: set[str] = set()

    def database(self, name: str, units: list[tuple[str, str]]) -> Path:
        """A build directory whose compile database lists `(compiler, file)` in order."""
        build = self.root / name
        build.mkdir()
        entries = []
        for compiler, file in units:
            source = self.root / file
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text("\n")
            entries.append({"directory": str(build), "file": str(source),
                            "command": f"{compiler} -O2 -o {source.name}.o -c {source}"})
        (build / "compile_commands.json").write_text(json.dumps(entries))
        return build

    def calibrate(self, compiler: str, work: Path) -> tuple[bool, str, int]:
        self.assertTrue(work.is_dir())
        self.calibrated.append(compiler)
        if compiler in self.failing:
            return False, "calibration build failed:\nundefined reference to `operator new(unsigned long)'", 0
        affected = 0 if compiler in self.unaffected else 16
        return True, f"calibration: 25 shapes; {affected} double-destroy at run time", affected

    def survey_unit(self, entry: dict, work: Path, index: int) -> dict:
        with self.lock:
            self.surveyed.append((shlex.split(entry["command"])[0], Path(entry["file"]).name))
        return {"file": entry["file"], "returncode": 0, "dumps": 2, "findings": [], "stderr": ""}

    def run_survey(self, *builds: Path) -> tuple[int, str]:
        arguments = ["survey_gcc_aggregate_cleanups.py", "--root", str(self.root), "--jobs", "2"]
        for build in builds:
            arguments += ["--build-dir", str(build)]
        output = io.StringIO()
        with contained_temporary_directory(prefix="bpr-survey-work-") as work, \
                mock.patch.object(sys, "argv", arguments), \
                mock.patch.object(SURVEY, "compiler_family", lambda compiler: self.families[compiler]), \
                mock.patch.object(SURVEY, "calibrate", self.calibrate), \
                mock.patch.object(SURVEY, "survey_unit", self.survey_unit), \
                mock.patch.object(concurrent.futures, "ProcessPoolExecutor", concurrent.futures.ThreadPoolExecutor), \
                mock.patch.object(SURVEY.tempfile, "TemporaryDirectory",
                                  lambda prefix: contained_temporary_directory(prefix=prefix, parent=work)), \
                contextlib.redirect_stdout(output):
            status = SURVEY.main()
        return status, output.getvalue()

    def test_a_database_whose_first_entry_is_c_calibrates_the_cxx_compiler(self) -> None:
        release = self.database("app-release", [(CC, "third_party/sqlite3.c"), (CXX, "src/core/policy.cpp")])
        status, output = self.run_survey(release)
        self.assertEqual(status, 0, output)
        self.assertEqual(self.calibrated, [CXX])
        self.assertEqual(set(self.surveyed), {(CXX, "policy.cpp"), (CC, "sqlite3.c")})
        self.assertIn(f"compiler {CXX}, C++ units: 1", output)
        self.assertIn("C units: 1, compiled with their recorded compiler", output)
        self.assertIn("units=2 failures=0 sites=0 gated=0", output)

    def test_the_order_of_databases_does_not_choose_the_compiler(self) -> None:
        release = self.database("app-release", [(CC, "third_party/sqlite3.c"), (CXX, "src/core/policy.cpp")])
        dev = self.database("dev", [(CXX, "tests/sandbox.cpp"), (CC, "third_party/sqlite3.c")])
        for order in ((release, dev), (dev, release)):
            with self.subTest(first=order[0].name):
                self.calibrated.clear()
                self.surveyed.clear()
                status, output = self.run_survey(*order)
                self.assertEqual(status, 0, output)
                self.assertEqual(self.calibrated, [CXX])
                self.assertIn("units=4 failures=0", output)

    def test_two_cxx_compilers_are_each_calibrated(self) -> None:
        build = self.database("dev", [(CXX_14, "src/a.cpp"), (CXX, "src/b.cpp"), (CXX_14, "src/c.cc")])
        status, output = self.run_survey(build)
        self.assertEqual(status, 0, output)
        self.assertEqual(self.calibrated, [CXX_14, CXX])
        self.assertEqual(set(self.surveyed), {(CXX_14, "a.cpp"), (CXX, "b.cpp"), (CXX_14, "c.cc")})
        self.assertIn(f"compiler {CXX_14}, C++ units: 2", output)
        self.assertIn(f"compiler {CXX}, C++ units: 1", output)

    def test_no_unit_is_surveyed_when_any_compiler_fails_calibration(self) -> None:
        self.failing.add(CXX)
        build = self.database("dev", [(CXX_14, "src/a.cpp"), (CXX, "src/b.cpp")])
        status, output = self.run_survey(build)
        self.assertEqual(status, 2, output)
        self.assertEqual(self.calibrated, [CXX_14, CXX])
        self.assertEqual(self.surveyed, [])
        self.assertIn("GCC_AGGREGATE_SURVEY_FAILED:calibration", output)

    def test_units_of_an_unaffected_compiler_are_not_surveyed(self) -> None:
        self.unaffected.add(CXX_14)
        build = self.database("dev", [(CXX_14, "src/a.cpp"), (CXX, "src/b.cpp")])
        status, output = self.run_survey(build)
        self.assertEqual(status, 0, output)
        self.assertEqual(self.surveyed, [(CXX, "b.cpp")])
        self.unaffected.add(CXX)
        self.calibrated.clear()
        self.surveyed.clear()
        status, output = self.run_survey(self.database("dev-2", [(CXX_14, "src/a.cpp"), (CXX, "src/b.cpp")]))
        self.assertEqual((status, self.surveyed), (0, []), output)
        self.assertIn("GCC_AGGREGATE_SURVEY_OK:no compiler here does double-destroy any calibrated shape", output)

    def test_clang_and_other_languages_are_skipped_and_named(self) -> None:
        only_clang = self.database("clang", [(CC, "third_party/sqlite3.c"), (CLANG, "src/a.cpp")])
        status, output = self.run_survey(only_clang)
        self.assertEqual((status, self.calibrated, self.surveyed), (0, [], []), output)
        self.assertIn(f"GCC_AGGREGATE_SURVEY_SKIPPED:{CLANG} is Clang", output)
        mixed = self.database("mixed", [(CLANG, "src/a.cpp"), (CXX, "src/b.cpp"), (CC, "src/start.S")])
        status, output = self.run_survey(mixed)
        self.assertEqual(status, 0, output)
        self.assertEqual((self.calibrated, self.surveyed), ([CXX], [(CXX, "b.cpp")]))
        self.assertIn(f"skipped 1 C++ units of {CLANG}, which is Clang", output)
        self.assertIn("skipped 1 units that are neither C++ nor C", output)

    def test_a_database_with_no_cxx_unit_is_refused(self) -> None:
        status, output = self.run_survey(self.database("c-only", [(CC, "third_party/sqlite3.c")]))
        self.assertEqual((status, self.calibrated), (2, []), output)
        self.assertIn("record no C++ unit", output)

    def test_the_language_is_read_from_x_before_the_suffix(self) -> None:
        def entry(command: str, file: str) -> dict:
            return {"directory": "/build", "file": file, "command": command}
        cases = {
            ("/usr/bin/cc -c /s/a.c", "/s/a.c"): "c",
            ("/usr/bin/c++ -c /s/a.cpp", "/s/a.cpp"): "c++",
            ("/usr/bin/c++ -c /s/a.C", "/s/a.C"): "c++",
            ("/usr/bin/c++ -c /s/a.cxx", "/s/a.cxx"): "c++",
            ("/usr/bin/c++ -x c++ -c /s/a.c", "/s/a.c"): "c++",
            ("/usr/bin/cc -xc -c /s/a.cpp", "/s/a.cpp"): "c",
            ("/usr/bin/cc -c /s/start.S", "/s/start.S"): "other",
        }
        for (command, file), language in cases.items():
            with self.subTest(command=command):
                self.assertEqual(SURVEY.unit_language(entry(command, file)), language)
        arguments = {"directory": "/build", "file": "/s/a.cc", "arguments": ["/usr/bin/g++", "-c", "/s/a.cc"]}
        self.assertEqual(SURVEY.unit_language(arguments), "c++")


if __name__ == "__main__":
    unittest.main()
