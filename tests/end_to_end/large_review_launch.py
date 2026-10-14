"""Observe one sandboxed fixture process from creation through its flushed markers.

The parent clock includes process creation, loader work, pipe delivery and parent
scheduling. These are conservative observed marker-arrival times, not an OS exec
timestamp, compositor presentation, or the packaged application's launch time.
The CTest row invokes this observer inside the existing network-denial boundary.

The first argument, --enforce=1 or --enforce=0, says whether the limits gate
the row. The CTest row passes 1 on macOS and in optimized builds elsewhere: the
limits hold on each platform's optimized measurements (BPR-0967), and a Debug
build off macOS has no justification for them, so it only reports them.
"""

import json
from pathlib import Path
import subprocess
import sys
import time


def main():
    enforce, report_path, *command = sys.argv[1:]
    if enforce not in ("--enforce=1", "--enforce=0"):
        raise SystemExit("usage: large_review_launch.py --enforce=1|0 REPORT COMMAND...")
    markers = {
        "LARGE_REVIEW_MAIN_ENTERED": "ProcessToMainMarkerMs",
        "LARGE_REVIEW_HOME_RENDERED": "ProcessToHomeMarkerMs",
    }
    observation = {"Schema": "bpr.large-review-launch.v1", "Command": command}
    began = time.perf_counter_ns()
    with subprocess.Popen(command, stdout=subprocess.PIPE, text=True) as child:
        for line in child.stdout:
            name = markers.get(line.rstrip("\n"))
            if name:
                if name in observation:
                    raise RuntimeError("duplicate launch marker")
                observation[name] = (time.perf_counter_ns() - began) / 1_000_000
            sys.stdout.write(line)
            sys.stdout.flush()
        code = child.wait()
    observation["ExitCode"] = code
    observation["LimitsMs"] = {"ProcessToMainMarkerMs": 100, "ProcessToHomeMarkerMs": 1000}
    observation["WithinLimits"] = all(
        0 <= observation.get(name, float("inf")) <= limit
        for name, limit in observation["LimitsMs"].items()
    )
    observation["Enforced"] = enforce == "--enforce=1"
    Path(report_path).write_text(json.dumps(observation, indent=2) + "\n")
    # [[BPR-1495]]. Every refusal names its check and its measurement, so a
    # gate log says which one failed without the report file.
    failures = []
    if code:
        failures.append(f"fixture exit code {code}")
    missing = [name for name in markers.values() if name not in observation]
    if missing:
        failures.append("missing launch marker " + ", ".join(missing))
    elif observation["ProcessToMainMarkerMs"] >= observation["ProcessToHomeMarkerMs"]:
        failures.append("main marker did not precede home marker")
    if observation["Enforced"]:
        for name, limit in observation["LimitsMs"].items():
            measured = observation.get(name)
            if measured is not None and not 0 <= measured <= limit:
                failures.append(f"{name}={measured:.1f} exceeds limit {limit}")
    if failures:
        for failure in failures:
            print(f"LARGE_REVIEW_LAUNCH_FAILED: {failure}", flush=True)
        return 1
    print("LARGE_REVIEW_LAUNCH_OBSERVED", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
