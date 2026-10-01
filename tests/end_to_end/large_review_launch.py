"""Observe one sandboxed fixture process from creation through its flushed markers.

The parent clock includes process creation, loader work, pipe delivery and parent
scheduling. These are conservative observed marker-arrival times, not an OS exec
timestamp, compositor presentation, or the packaged application's launch time.
The CTest row invokes this observer inside the existing network-denial boundary.

The limits are justified by optimized Mac measurements alone (BPR-0967). On
another platform they are reported with Enforced false and never fail the row.
"""

import json
from pathlib import Path
import subprocess
import sys
import time


def main():
    report_path, *command = sys.argv[1:]
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
    observation["Enforced"] = sys.platform == "darwin"
    Path(report_path).write_text(json.dumps(observation, indent=2) + "\n")
    if code or not all(name in observation for name in markers.values()):
        return 1
    if observation["ProcessToMainMarkerMs"] >= observation["ProcessToHomeMarkerMs"]:
        return 1
    if observation["Enforced"] and not observation["WithinLimits"]:
        return 1
    print("LARGE_REVIEW_LAUNCH_OBSERVED", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
