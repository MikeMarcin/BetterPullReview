#!/usr/bin/env python3
"""Measures what adopting candidate T would add to a shipped executable.

For each architecture slice, links two minimal executables that call
`refine_cpp_identifiers`: one with the shipped refinement, one with the
candidate, the tree-sitter runtime and one grammar pin. Both are optimized
(-O3) and dead-stripped, so the difference is the code and data adoption would
add to that slice. Also times the compilation of each grammar's parser.c and
scanner.c and of the runtime, as the build would compile them (-O3, C11).

Usage: python3 measure_size.py --repo-root ROOT --build BUILD --out FILE [--arch arm64,x86_64]
"""

from __future__ import annotations

import argparse
import json
import platform
import subprocess
import sys
import time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "scripts"))
from contained_temporary_support import contained_temporary_directory  # noqa: E402

PROBE = r"""
#include "cpp_identifier_context.hpp"
#include <cstdio>
int main(int count, char** arguments)
{
	auto spans = bpr::application::token_span_list{ };
	const auto text = std::string_view{ count > 1 ? arguments[1] : "namespace A { enum class E { X }; }" };
	const auto status = bpr::adapters::syntax::refine_cpp_identifiers(text, spans, 1000U, { });
	std::printf("%d %zu\n", static_cast<int>(status), spans.size());
	return 0;
}
"""


def run(command: list[str]) -> float:
    started = time.perf_counter()
    subprocess.run(command, check=True)
    return time.perf_counter() - started


def segments(path: Path, arch: str) -> dict:
    output = subprocess.run(["size", "-m", "-arch", arch, str(path)], capture_output=True, text=True, check=True).stdout
    result = {}
    for line in output.splitlines():
        line = line.strip()
        if line.startswith("Segment ") and ":" in line:
            name, value = line[len("Segment "):].split(":", 1)
            result[name.strip()] = int(value.split()[0])
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--arch", default="arm64,x86_64")
    parser.add_argument("--cxx", default="/usr/bin/clang++")
    parser.add_argument("--cc", default="/usr/bin/clang")
    arguments = parser.parse_args()
    root = arguments.repo_root.resolve()
    stage = arguments.build.resolve() / "_bpr" / "tree-sitter"
    master = "c009222808634c1014f82438d4883753516a2c24"
    grammars = {"v0_23_4": stage / "cpp-v0.23.4" / "tree-sitter-cpp-0.23.4" / "src",
                "master": stage / "cpp-master" / f"tree-sitter-cpp-{master}" / "src"}
    runtime = stage / "runtime" / "tree-sitter-0.27.1" / "lib"
    includes = ["-I", str(root / "src/adapters/syntax/src"), "-I", str(root / "src/application/include"),
                "-I", str(root / "src/core/include"), "-isystem", str(runtime / "include")]
    cxx_flags = ["-std=c++23", "-O3", "-DNDEBUG", "-D_LIBCPP_ENABLE_EXPERIMENTAL"]
    c_flags = ["-std=c11", "-O3", "-DNDEBUG", "-D_POSIX_C_SOURCE=200112L", "-D_DEFAULT_SOURCE", "-D_DARWIN_C_SOURCE"]
    report: dict = {"host": platform.platform(), "compiler": subprocess.run([arguments.cxx, "--version"], capture_output=True, text=True).stdout.splitlines()[0],
                    "slices": {}, "compile_seconds": {}}
    with contained_temporary_directory(prefix="bpr-size-") as scratch_name:
        scratch = Path(scratch_name)
        probe = scratch / "probe.cpp"
        probe.write_text(PROBE, encoding="utf-8")
        for arch in arguments.arch.split(","):
            slice_report: dict = {}
            arch_flags = ["-arch", arch]
            base_objects = []
            for source in (probe, root / "src/adapters/syntax/src/cpp_identifier_context.cpp"):
                target = scratch / f"{source.stem}-{arch}.o"
                run([arguments.cxx, *cxx_flags, *arch_flags, *includes, "-c", str(source), "-o", str(target)])
                base_objects.append(target)
            baseline = scratch / f"baseline-{arch}"
            run([arguments.cxx, *arch_flags, *base_objects, "-Wl,-dead_strip", "-o", str(baseline)])
            slice_report["baseline"] = {"bytes": baseline.stat().st_size, "segments": segments(baseline, arch)}
            runtime_object = scratch / f"lib-{arch}.o"
            seconds = run([arguments.cc, *c_flags, *arch_flags, "-I", str(runtime / "include"), "-I", str(runtime / "src"),
                           "-c", str(runtime / "src/lib.c"), "-o", str(runtime_object)])
            report["compile_seconds"].setdefault("runtime lib.c", {})[arch] = round(seconds, 2)
            candidate_object = scratch / f"candidate-{arch}.o"
            run([arguments.cxx, *cxx_flags, *arch_flags, *includes, "-c",
                 str(root / "tests/syntax_evaluation/candidate/cpp_identifier_context_tree_sitter.cpp"), "-o", str(candidate_object)])
            for pin, grammar in grammars.items():
                objects = []
                for name in ("parser", "scanner"):
                    target = scratch / f"{pin}-{name}-{arch}.o"
                    seconds = run([arguments.cc, *c_flags, *arch_flags, "-I", str(grammar), "-c", str(grammar / f"{name}.c"), "-o", str(target)])
                    report["compile_seconds"].setdefault(f"{pin} {name}.c", {})[arch] = round(seconds, 2)
                    objects.append(target)
                executable = scratch / f"candidate-{pin}-{arch}"
                run([arguments.cxx, *arch_flags, base_objects[0], candidate_object, *objects, runtime_object, "-Wl,-dead_strip", "-o", str(executable)])
                measured = {"bytes": executable.stat().st_size, "segments": segments(executable, arch)}
                measured["added_bytes"] = measured["bytes"] - slice_report["baseline"]["bytes"]
                measured["added_mib"] = round(measured["added_bytes"] / (1024 * 1024), 3)
                smoke = subprocess.run([str(executable)], capture_output=True, text=True) if arch == platform.machine() else None
                measured["smoke"] = smoke.stdout.strip() if smoke else "not run on this host"
                slice_report[pin] = measured
            report["slices"][arch] = slice_report
    arguments.out.write_text(json.dumps(report, indent=1) + "\n", encoding="utf-8")
    print(json.dumps({arch: {pin: data.get("added_mib") for pin, data in sizes.items() if pin != "baseline"} for arch, sizes in report["slices"].items()}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
