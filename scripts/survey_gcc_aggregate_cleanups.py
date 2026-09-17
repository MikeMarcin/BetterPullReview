#!/usr/bin/env python3
"""Find aggregate initializations that GCC lowers into a double destroy.

[[BPR-0822]]. GCC 15.2 splits a non-constant aggregate initialization into
member-wise statements, each guarded by a cleanup flag. When a member is
itself a multi-member aggregate initialized from braces (named or positional
elements, empty braces, or simply omitted), GCC arms the member's own cleanup
while the cleanups of the subobjects inside it are still armed. If a later
member's initializer throws, both run, and the inner subobject is destroyed
twice (AddressSanitizer: attempting double-free). Clang is correct.
Initializing the member from a prvalue of its named type
(`.Receipt = receipt{ ... }`) avoids it.

Warnings do not find these sites: `-O3 -Wmaybe-uninitialized` reported two of
them, and the tree then built clean with others still present. Clang tooling
sees Clang's lowering, not GCC's. This survey therefore asks GCC itself:

1. Calibration. It compiles an embedded reproducer with the same compiler,
   runs it to count live objects after a later initializer throws in each
   shape, and requires the detector below to flag exactly the shapes that
   double-destroy at run time. A disagreement stops the survey, so a GCC
   whose dump format or lowering changed cannot report a false clean result.
   If no shape double-destroys, the compiler is not affected and the survey
   stops with success.
2. Survey. It recompiles every translation unit of each supplied CMake
   compilation database with its recorded flags plus
   `-fdump-tree-gimple-lineno`, then reads each dump. Object files and dumps
   go to a temporary directory; the build tree is not modified.
3. Detector. For every destructor call inside a `finally` or `catch` handler,
   it takes the protected `try` body and, for a flag-guarded call, the
   statements at which all its guard flags are 1. Two calls whose object
   paths nest (P and P.S) overlap when a call statement is protected by both;
   one throw there destroys P.S twice. Unguarded handlers are treated as
   active over their whole body, so a lowering the parser does not model
   yields a finding rather than silence.

A finding is reported at the source line GCC attributes to the overlapping
statement. For a default member initializer that can be the declaring header
or the constructing line, and for an initializer list it can be the closing
brace of the enclosing function. A finding means GCC lowered the site into
overlapping cleanups on an exception path; the throwing call it guards may
never throw in practice, so the count is not a count of runtime failures.

The exit status is 1 when a finding lies under a gated path prefix (default
`src/`); 2 on a usage, compiler, calibration or compile failure, or when a
compiled file lies outside `--root`; and 0 otherwise, including for a Clang
build.
"""

from __future__ import annotations

import argparse
import collections
import concurrent.futures
import json
import pathlib
import re
import shlex
import subprocess
import sys
import tempfile
from typing import Any

if sys.version_info < (3, 10):
    sys.exit("survey_gcc_aggregate_cleanups.py requires Python 3.10 or newer")

ANNOTATION = re.compile(r"\[[^\]\s]+:\d+:\d+\] ?")
LOCATION = re.compile(r"\[([^\]\s]+:\d+:\d+)\]")
FLAG_TEST = re.compile(
    r"^if \(([A-Za-z_]\w*\.\d+) != 0\) goto <(D\.\d+)>; else goto <(D\.\d+)>;$"
)
LABEL = re.compile(r"^<(D\.\d+)>:$")
ADDRESS = re.compile(r"^(_\d+) = &(.+);$")
DESTRUCTOR = re.compile(r"^(?:\S+ = )?(\S*::~[^\s(]+|~[^\s(]+) \((&?[^,\)]+)[,\)]")
FLAG_SET = re.compile(r"^([A-Za-z_]\w*\.\d+) = ([01]);$")
CALL = re.compile(r"(?:^|= )[\w:~<>,\. \*&]*[\w>] \(")

# Each `run` line is one shape. `expect_double` states what GCC 15.2 does;
# calibration never trusts it, it compares the detector with the run.
CALIBRATION_SOURCE = r"""
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
static int live = 0;
static bool armed = false;
struct tracked { tracked() { ++live; } tracked(const tracked&) { ++live; } tracked(tracked&&) noexcept { ++live; } tracked& operator=(const tracked&) = default; tracked& operator=(tracked&&) noexcept = default; ~tracked() { --live; } };
struct fragile { fragile() = default; fragile(const fragile&) { if (armed) { throw std::runtime_error("copy"); } } };
static auto boom() -> int { if (armed) { throw std::runtime_error("boom"); } return 1; }
struct receipt { tracked Id; bool Duplicate = false; };
struct outer { receipt Receipt; std::uint64_t Count = 0U; fragile Node; };
struct single { tracked Only; };
struct single_outer { single S; fragile Node; };
struct ctor_member { tracked T; ctor_member() = default; explicit ctor_member(const tracked& a) : T(a) { } };
struct ctor_outer { ctor_member C; fragile Node; };
struct nsdmi_braced { receipt Receipt{ }; fragile Node; };
struct nsdmi_named { receipt Receipt = receipt{ }; fragile Node; };
struct base_outer : receipt { fragile Node; };
struct array_outer { receipt Items[2]; fragile Node; };
struct optional_outer { std::optional<receipt> Maybe; fragile Node; };
struct throwing_default { receipt Receipt; int Value = boom(); };
struct mem_init { receipt Receipt; fragile Node; mem_init(const tracked& a, const fragile& f) : Receipt{ .Id = a }, Node(f) { } };
template <class T> struct box { T Value; fragile Node; };
static auto take(outer) -> void { }
static void run(int line, bool expect_double, auto make)
{
	const auto before = live;
	armed = true;
	try { make(); } catch (const std::runtime_error&) { }
	armed = false;
	std::printf("shape line=%d net=%d expect_double=%d\n", line, live - before, expect_double ? 1 : 0);
}
int main()
{
	const auto a = tracked{ };
	const auto f = fragile{ };
	run(__LINE__, true, [&] { auto v = outer{ .Receipt = { .Id = a }, .Count = 1U, .Node = f }; });
	run(__LINE__, true, [&] { auto v = outer{ .Count = 1U, .Node = f }; });
	run(__LINE__, true, [&] { auto v = outer{ .Receipt = { }, .Node = f }; });
	run(__LINE__, true, [&] { auto v = outer{ { a, false }, 1U, f }; });
	run(__LINE__, false, [&] { auto v = outer{ .Receipt = receipt{ .Id = a }, .Node = f }; });
	run(__LINE__, true, [&]() -> outer { return { .Receipt = { .Id = a }, .Node = f }; });
	run(__LINE__, true, [&] { auto p = std::unique_ptr<outer>(new outer{ .Receipt = { .Id = a }, .Node = f }); });
	run(__LINE__, true, [&] { take({ .Receipt = { .Id = a }, .Node = f }); });
	run(__LINE__, true, [&] { std::vector<outer> v; v.push_back({ .Receipt = { .Id = a }, .Node = f }); });
	run(__LINE__, true, [&] { auto v = std::vector<outer>{ { .Receipt = { a } }, { .Receipt = { a }, .Node = f } }; });
	run(__LINE__, true, [&] { auto l = [v = outer{ .Receipt = { a }, .Node = f }] { }; });
	run(__LINE__, true, [&] { outer v; v = { .Receipt = { a }, .Node = f }; });
	run(__LINE__, true, [&] { auto v = box<receipt>{ { a }, f }; });
	run(__LINE__, true, [&] { auto v = base_outer{ { a }, f }; });
	run(__LINE__, true, [&] { auto v = nsdmi_braced{ .Node = f }; });
	run(__LINE__, true, [&] { auto v = nsdmi_named{ .Node = f }; });
	run(__LINE__, true, [&] { auto v = throwing_default{ .Receipt = { .Id = a } }; });
	run(__LINE__, false, [&] { auto v = single_outer{ .S = { a }, .Node = f }; });
	run(__LINE__, false, [&] { auto v = ctor_outer{ .C = ctor_member{ a }, .Node = f }; });
	run(__LINE__, false, [&] { auto v = ctor_outer{ .Node = f }; });
	run(__LINE__, false, [&] { auto v = array_outer{ .Items = { { a }, { a } }, .Node = f }; });
	run(__LINE__, false, [&] { auto v = optional_outer{ .Maybe = receipt{ .Id = a }, .Node = f }; });
	run(__LINE__, false, [&] { auto v = mem_init{ a, f }; });
	run(__LINE__, false, [&] { auto v = outer(receipt{ a }, 1U, f); });
	run(__LINE__, false, [&] { auto r = receipt{ .Id = a }; auto v = outer{ .Receipt = std::move(r), .Node = f }; });
	return 0;
}
"""


def strip_annotations(line: str) -> str:
    return ANNOTATION.sub("", line).strip()


def gimple_functions(lines: list[str]):
    """Yield (signature, body lines) for each function in a GIMPLE dump."""
    name = None
    body: list[str] = []
    for index, line in enumerate(lines):
        if name is None:
            if (
                line
                and not line[0].isspace()
                and line.rstrip().endswith(")")
                and index + 1 < len(lines)
                and not lines[index + 1][:1].isspace()
                and strip_annotations(lines[index + 1]) == "{"
            ):
                name, body = line.strip(), []
            continue
        body.append(line)
        if line.startswith("}"):
            yield name, body
            name = None


def block_end(statements: list[str], open_index: int) -> int:
    depth = 0
    for index in range(open_index, len(statements)):
        if statements[index] == "{":
            depth += 1
        elif statements[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return len(statements) - 1


def overlapping_cleanups(body: list[str]) -> list[dict[str, Any]]:
    """Return every nested destructor pair that one throw would run twice."""
    statements = [strip_annotations(line) for line in body]
    locations = [(match.group(1) if (match := LOCATION.search(line)) else None) for line in body]
    flags: dict[str, bool] = {}
    flags_at: list[dict[str, bool]] = []
    for statement in statements:
        if match := FLAG_SET.match(statement):
            flags[match.group(1)] = match.group(2) == "1"
        flags_at.append(dict(flags))

    cleanups = []
    for index, statement in enumerate(statements):
        if statement != "try" or index + 1 >= len(statements) or statements[index + 1] != "{":
            continue
        body_start = index + 1
        body_end = block_end(statements, body_start)
        handler = body_end + 1
        if (
            handler + 1 >= len(statements)
            or statements[handler] not in ("finally", "catch")
            or statements[handler + 1] != "{"
        ):
            continue
        handler_end = block_end(statements, handler + 1)
        temporaries: dict[str, str] = {}
        labels: dict[str, tuple[str, bool]] = {}
        guards: set[str] = set()
        depth = 0
        for position in range(handler + 2, handler_end):
            text = statements[position]
            if text in ("{", "}"):
                depth += 1 if text == "{" else -1
                continue
            if depth:
                continue
            if match := FLAG_TEST.match(text):
                labels[match.group(2)] = (match.group(1), True)
                labels[match.group(3)] = (match.group(1), False)
                continue
            if match := LABEL.match(text):
                if entry := labels.get(match.group(1)):
                    if entry[1]:
                        guards.add(entry[0])
                    else:
                        guards.discard(entry[0])
                continue
            if match := ADDRESS.match(text):
                temporaries[match.group(1)] = match.group(2)
                continue
            if match := DESTRUCTOR.match(text):
                argument = match.group(2).strip()
                path = argument[1:] if argument.startswith("&") else temporaries.get(argument)
                if path:
                    cleanups.append({"path": path, "guards": frozenset(guards), "body": (body_start, body_end)})

    findings = []
    for inner in cleanups:
        for outer in cleanups:
            if not (inner["path"].startswith(outer["path"] + ".") or inner["path"].startswith(outer["path"] + "[")):
                continue
            low = max(inner["body"][0], outer["body"][0])
            high = min(inner["body"][1], outer["body"][1])
            for position in range(low + 1, high):
                text = statements[position]
                if not CALL.search(text) or text.startswith("if ") or FLAG_SET.match(text):
                    continue
                state = flags_at[position]
                if all(state.get(flag, False) for flag in inner["guards"] | outer["guards"]):
                    findings.append(
                        {"outer": outer["path"], "inner": inner["path"], "location": locations[position], "statement": text}
                    )
                    break
    return findings


def exception_cleanup_paths(body: list[str]) -> set[str]:
    """Object paths destroyed on exception paths of an EH-lowered function.

    After `lower_eh`, a cleanup that no statement can throw into is folded
    into the normal path, and an exception cleanup ends in `resx`. Destructor
    calls are collected since the previous `return` or `resx`; the run that
    ends in `resx` is exception cleanup. A normal-path destructor left in the
    run can only add paths, so this never hides an exception edge.
    """
    temporaries: dict[str, str] = {}
    pending: set[str] = set()
    paths: set[str] = set()
    for line in body:
        text = strip_annotations(line)
        if match := ADDRESS.match(text):
            temporaries[match.group(1)] = match.group(2)
            continue
        if match := DESTRUCTOR.match(text):
            argument = match.group(2).strip()
            path = argument[1:] if argument.startswith("&") else temporaries.get(argument)
            if path:
                pending.add(path)
            continue
        if text.startswith("resx"):
            paths |= pending
            pending = set()
        elif text.startswith("return"):
            pending = set()
    return paths


def scan_dumps(gimple: pathlib.Path, lowered: pathlib.Path | None) -> list[dict[str, Any]]:
    """Findings from a gimple dump, kept only where the EH dump of the same
    function destroys both objects on an exception path."""
    exceptional: dict[str, set[str]] = {}
    if lowered is not None:
        for name, body in gimple_functions(lowered.read_text(errors="replace").splitlines()):
            exceptional[name] = exception_cleanup_paths(body)
    findings = []
    for name, body in gimple_functions(gimple.read_text(errors="replace").splitlines()):
        for finding in overlapping_cleanups(body):
            paths = exceptional.get(name)
            if lowered is not None and paths is not None and not {finding["outer"], finding["inner"]} <= paths:
                continue
            finding["function"] = name
            findings.append(finding)
    return findings


def dump_pair(directory: pathlib.Path) -> tuple[pathlib.Path | None, pathlib.Path | None]:
    gimple = sorted(directory.glob("*.gimple"))
    lowered = sorted(directory.glob("*.eh"))
    return (gimple[0] if gimple else None, lowered[0] if lowered else None)


def compiler_family(compiler: str) -> str:
    """`gcc`, `clang`, or `unknown` (for example a launcher such as ccache)."""
    result = subprocess.run(
        [compiler, "-dM", "-E", "-x", "c++", "/dev/null"], capture_output=True, text=True, check=False
    )
    macros = result.stdout
    if result.returncode != 0 or "#define __GNUC__ " not in macros:
        return "unknown"
    return "clang" if "#define __clang__ " in macros else "gcc"


def calibrate(compiler: str, work: pathlib.Path) -> tuple[bool, str, int]:
    source = work / "bpr_gcc_aggregate_cleanup_calibration.cpp"
    source.write_text(CALIBRATION_SOURCE)
    program = work / "calibration"
    build = subprocess.run(
        [compiler, "-std=c++23", "-O0", "-o", str(program), str(source)], capture_output=True, text=True, check=False
    )
    if build.returncode != 0:
        return False, f"calibration build failed:\n{build.stderr[-2000:]}", 0
    run = subprocess.run([str(program)], capture_output=True, text=True, check=False)
    runtime = {}
    for line in run.stdout.splitlines():
        if match := re.match(r"shape line=(\d+) net=(-?\d+) expect_double=([01])$", line):
            runtime[int(match.group(1))] = (int(match.group(2)), match.group(3) == "1")
    if run.returncode != 0 or len(runtime) != CALIBRATION_SOURCE.count("run(__LINE__"):
        return False, f"calibration run failed (exit {run.returncode}):\n{run.stdout[-2000:]}{run.stderr[-2000:]}", 0
    dump_dir = work / "calibration-dump"
    dump_dir.mkdir()
    dumped = subprocess.run(
        [compiler, "-std=c++23", "-O0", "-c", "-o", str(dump_dir / "calibration.o"), "-fdump-tree-gimple-lineno",
         "-fdump-tree-eh-lineno", "-dumpdir", f"{dump_dir}/", str(source)],
        capture_output=True, text=True, check=False,
    )
    gimple, lowered = dump_pair(dump_dir)
    if dumped.returncode != 0 or gimple is None or lowered is None:
        return False, f"calibration dump failed:\n{dumped.stderr[-2000:]}", 0
    detected = set()
    for finding in scan_dumps(gimple, lowered):
        if finding["location"]:
            detected.add(int(finding["location"].rsplit(":", 2)[1]))
    doubled = {line for line, (net, _) in runtime.items() if net < 0}
    unexpected_counts = {line: net for line, (net, _) in runtime.items() if net > 0}
    documented = {line for line, (_, expect) in runtime.items() if expect}
    report = (
        f"calibration: {len(runtime)} shapes; {len(doubled)} double-destroy at run time "
        f"(documented GCC 15.2 behaviour: {len(documented)}); detector flags {len(detected)}"
    )
    if unexpected_counts or detected != doubled:
        report += (
            f"\n  detector disagrees with the run: missed lines {sorted(doubled - detected)}, "
            f"extra lines {sorted(detected - doubled)}, leaked {unexpected_counts}"
        )
        return False, report, len(doubled)
    if doubled != documented:
        report += f"\n  note: the compiler's behaviour differs from GCC 15.2 at lines {sorted(doubled ^ documented)}"
    return True, report, len(doubled)


def survey_unit(entry: dict[str, Any], work: pathlib.Path, index: int) -> dict[str, Any]:
    arguments = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
    unit = work / f"tu-{index}"
    unit.mkdir()
    command: list[str] = []
    skip = False
    for argument in arguments:
        if skip:
            skip = False
            continue
        if argument in ("-o", "-MF", "-MT", "-MQ"):
            if argument == "-o":
                command += ["-o", str(unit / "unit.o")]
            skip = True
            continue
        # Dependency and module-scan outputs belong to the build; this compile
        # needs neither, and a missing module map would only stop it.
        if argument in ("-MD", "-MMD", "-fmodules-ts") or argument.startswith(("-fdeps-", "-fmodule-mapper=")):
            continue
        command.append(argument)
    command += ["-fdump-tree-gimple-lineno", "-fdump-tree-eh-lineno", "-dumpdir", f"{unit}/"]
    result = subprocess.run(command, cwd=entry["directory"], capture_output=True, text=True, check=False)
    gimple, lowered = dump_pair(unit)
    findings = scan_dumps(gimple, lowered) if gimple is not None and lowered is not None else []
    for path in unit.iterdir():
        path.unlink()
    unit.rmdir()
    return {
        "file": entry["file"],
        "returncode": result.returncode,
        "dumps": int(gimple is not None) + int(lowered is not None),
        "findings": findings,
        "stderr": result.stderr[-2000:] if result.returncode else "",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=pathlib.Path, action="append", required=True,
                        help="configured build tree with compile_commands.json; repeat for several presets")
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--gate", action="append", default=None,
                        help="repository-relative path prefix whose findings fail the survey (default: src/)")
    parser.add_argument("--json", type=pathlib.Path, help="write the complete result here")
    options = parser.parse_args()
    root = options.root.resolve()
    gates = options.gate or ["src/"]

    databases = []
    for build_dir in options.build_dir:
        database = build_dir.resolve() / "compile_commands.json"
        if not database.is_file():
            print(f"GCC_AGGREGATE_SURVEY_FAILED:no compile database at {database}")
            return 2
        databases.append((build_dir.name, json.loads(database.read_text())))
    entries = [(name, entry) for name, database in databases for entry in database]
    if not entries:
        print("GCC_AGGREGATE_SURVEY_FAILED:empty compile database")
        return 2
    # A database configured from another checkout would compile that
    # checkout's sources, and its findings could not be gated against --root.
    foreign = [
        entry["file"] for _, entry in entries
        if not pathlib.Path(entry["file"]).resolve().is_relative_to(root)
    ]
    if foreign:
        print(f"GCC_AGGREGATE_SURVEY_FAILED:{len(foreign)} compiled files lie outside {root}, e.g. {foreign[0]}")
        return 2
    first = entries[0][1]
    compiler = shlex.split(first["command"])[0] if "command" in first else first["arguments"][0]
    family = compiler_family(compiler)
    if family == "clang":
        print(f"GCC_AGGREGATE_SURVEY_SKIPPED:{compiler} is Clang, which lowers these initializations correctly")
        return 0
    if family != "gcc":
        print(f"GCC_AGGREGATE_SURVEY_FAILED:cannot identify {compiler} as GCC or Clang (a compiler launcher is not supported)")
        return 2

    with tempfile.TemporaryDirectory(prefix="bpr-gcc-aggregate-survey-") as temporary:
        work = pathlib.Path(temporary)
        calibrated, report, affected = calibrate(compiler, work)
        print(report)
        if not calibrated:
            print("GCC_AGGREGATE_SURVEY_FAILED:calibration")
            return 2
        if affected == 0:
            print("GCC_AGGREGATE_SURVEY_OK:this compiler does not double-destroy any calibrated shape")
            return 0
        with concurrent.futures.ProcessPoolExecutor(max(1, options.jobs)) as pool:
            futures = [pool.submit(survey_unit, entry, work, index) for index, (_, entry) in enumerate(entries)]
            results = [(preset, future.result()) for (preset, _), future in zip(entries, futures)]

    failures = [result for _, result in results if result["returncode"] != 0 or result["dumps"] != 2]
    sites: dict[tuple[str, int], dict[str, Any]] = collections.OrderedDict()
    for preset, result in results:
        for finding in result["findings"]:
            if not finding["location"]:
                continue
            file, line, _ = finding["location"].rsplit(":", 2)
            path = pathlib.Path(file)
            try:
                relative = path.resolve().relative_to(root).as_posix()
            except ValueError:
                relative = path.as_posix()
            site = sites.setdefault((relative, int(line)), {"pairs": set(), "presets": set(), "units": set()})
            site["pairs"].add((re.sub(r"D\.\d+", "D", finding["outer"]), re.sub(r"D\.\d+", "D", finding["inner"])))
            site["presets"].add(preset)
            site["units"].add(result["file"])

    gated = []
    for (relative, line), site in sorted(sites.items()):
        is_gated = any(relative.startswith(prefix) for prefix in gates)
        gated += [(relative, line)] if is_gated else []
        pairs = "; ".join(f"{outer} > {inner}" for outer, inner in sorted(site["pairs"])[:3])
        print(f"{'GATED' if is_gated else 'noted'} {relative}:{line} [{','.join(sorted(site['presets']))}] {pairs}")
    if options.json:
        options.json.write_text(json.dumps({
            "calibration": report,
            "units": len(results),
            "failures": [{"file": result["file"], "returncode": result["returncode"], "stderr": result["stderr"]} for result in failures],
            "sites": [{"file": relative, "line": line, "pairs": sorted(site["pairs"]), "presets": sorted(site["presets"]),
                       "units": sorted(site["units"])} for (relative, line), site in sorted(sites.items())],
        }, indent=1) + "\n")
    print(f"units={len(results)} failures={len(failures)} sites={len(sites)} gated={len(gated)}")
    if failures:
        for failure in failures[:10]:
            print(f"GCC_AGGREGATE_SURVEY_FAILED:compile {failure['file']} exited {failure['returncode']}\n{failure['stderr']}")
        return 2
    if gated:
        print("GCC_AGGREGATE_SURVEY_FAILED:" + ", ".join(f"{relative}:{line}" for relative, line in gated))
        return 1
    print("GCC_AGGREGATE_SURVEY_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
