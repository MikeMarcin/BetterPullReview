#!/usr/bin/env python3
"""Capture stock vycor-cpp probe results; this is not an effect-policy checker.

The output directory must be new. No fixture is linked or executed, and no
product database or gate is modified. Inspect exit codes AND indexScope in the
saved responses: a successful invocation is not a capability verdict.
"""

import argparse
import json
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", required=True, type=Path)
    parser.add_argument("--clang", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--fixtures", type=Path, default=Path(__file__).resolve().parents[2] / "tests/fixtures/vycor-evaluation")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    fixtures = args.fixtures.resolve()
    sources = sorted(fixtures.glob("*.cpp"))
    if len(sources) != 5:
        parser.error("expected exactly five probe translation units")
    database = [{"directory": str(fixtures), "file": str(source),
                 "arguments": [str(args.clang.resolve()), "-std=c++23", "-c", str(source)]}
                for source in sources]
    (output / "compile_commands.json").write_text(json.dumps(database, indent=2) + "\n")
    records = []

    def run(label, arguments):
        command = [str(args.tool.resolve()), *arguments]
        started = time.monotonic()
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        (output / f"{label}.stdout").write_text(result.stdout)
        (output / f"{label}.stderr").write_text(result.stderr)
        records.append({"label": label, "command": command, "exit": result.returncode,
                        "seconds": round(time.monotonic() - started, 3)})
        (output / "commands.json").write_text(json.dumps(records, indent=2) + "\n")
        print(label, result.returncode, flush=True)
        return result.returncode

    if run("version", ["--version"]) != 0:
        raise SystemExit("tool did not start; see version.stderr")
    common = ["--build-path", str(output)]
    run("index", ["megascope", "index", *common, "--threads", "1"])
    run("info", ["megascope", "info", *common, "--files"])
    if not json.loads((output / "info.stdout").read_text())["coverage"]["complete"]:
        raise SystemExit("incomplete main probe index; see info.stdout and index.stderr")
    if run("dump", ["megascope", "dump", *common, "--format", "json"]) != 0:
        raise SystemExit("no usable dump; see captured index/info/dump results")
    # Follow the tool's resolved identities, which differ between standard libraries.
    dump = json.loads((output / "dump.stdout").read_text())
    for name in ["wrapper", "value_wrapper"]:
        site = next(item for item in dump["callSites"]
                    if item["callerName"] == f"probe::{name}"
                    and item["calleeName"].endswith("::is_directory"))
        run(f"lookup-{name}-callee", ["megascope", "lookup-function", *common,
                                     "--usr", site["calleeUsr"]])
        run(f"chain-{name}-callee", ["megascope", "find-call-chain", *common,
                                    "--from", f"probe::cross_file_{'effect' if name == 'wrapper' else 'value'}",
                                    "--to-usr", site["calleeUsr"]])
        if name == "wrapper":
            path_usr = site["calleeUsr"]
            run("value-to-path-control", ["megascope", "find-call-chain", *common,
                                          "--from", "probe::cross_file_value", "--to-usr", site["calleeUsr"]])
    for query in ["is_directory", "now", "thread"]:
        run(f"search-{query}", ["megascope", "search-functions", *common,
                               "--query", query, "--limit", "100"])
    for name in ["path_effect", "status_value", "aliased_clock", "macro_clock",
                 "supplied_value", "spawn_worker", "thread_reference", "empty_handle",
                 "move_handle", "cross_file_effect", "cross_file_value", "wrapper", "value_wrapper"]:
        run(f"callees-{name}", ["megascope", "get-callees", *common, "--name", f"probe::{name}"])
    run("concurrency", ["megascope", "list-concurrency-entry-points", *common])
    run("chain", ["megascope", "find-call-chain", *common,
                  "--from", "probe::cross_file_effect", "--to", "probe::wrapper"])
    run("ambiguous-overload", ["megascope", "get-callers", *common,
                               "--name", "std::filesystem::is_directory"])
    run("missing-identity", ["megascope", "lookup-function", *common,
                             "--name", "probe::does_not_exist"])
    run("checks", ["anneal", "--list-checks"])
    run("anneal", ["anneal", *common, "--threads", "1", "--format", "json",
                   *[arg for source in sources for arg in ["--source", str(source)]]])
    run("omitted-wrapper-index", ["megascope", "index", *common, "--threads", "1",
                                   "--source", str(fixtures / "entry.cpp"),
                                   "--index", str(output / "omitted.vycs")])
    run("omitted-wrapper-chain", ["megascope", "find-call-chain", "--index", str(output / "omitted.vycs"),
                                   "--from", "probe::cross_file_effect", "--to-usr", path_usr])
    broken = output / "broken"
    broken.mkdir()
    source = broken / "broken.cpp"
    source.write_text('#include "deliberately_missing_bpr_probe.hpp"\n')
    (broken / "compile_commands.json").write_text(json.dumps([
        {"directory": str(broken), "file": str(source),
         "arguments": [str(args.clang.resolve()), "-std=c++23", "-c", str(source)]}]) + "\n")
    run("broken-parse-control", ["megascope", "index", "--build-path", str(broken), "--threads", "1"])
    run("broken-parse-info", ["megascope", "info", "--build-path", str(broken), "--files"])
    (output / "commands.json").write_text(json.dumps(records, indent=2) + "\n")
    if next(record for record in records if record["label"] == "index")["exit"] != 0:
        raise SystemExit("index failed; captured responses are not passing evidence")


if __name__ == "__main__":
    main()
