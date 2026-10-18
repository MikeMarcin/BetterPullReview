"""Observe one sandboxed fixture process from creation through its flushed markers.

The parent clock includes process creation, loader work, pipe delivery and parent
scheduling. These are conservative observed marker-arrival times, not an OS exec
timestamp, compositor presentation, or the packaged application's launch time.
The CTest row invokes this observer inside the existing network-denial boundary.

The first argument, --enforce=1 or --enforce=0, says whether the limits gate
the row. The CTest row passes 1 on macOS and in optimized builds elsewhere: the
limits hold on each platform's optimized measurements (BPR-0967), and a Debug
build off macOS has no justification for them, so it only reports them.

BPR-1598. A limit reads each marker's attributed time, as the course's own
limits do: the observed time less the time the fixture process waited,
runnable, for a processor another process held (the process reports it on the
LARGE_REVIEW_SCHEDULE line after each marker), never below the processor time
its main thread had used by then and never above the observed time. On a quiet
machine the wait is about zero and the two agree. Both stay in the report. A
limit missed while the process measurably waited, or a course timing miss the
fixture marks LARGE_REVIEW_TIMING_RETRYABLE, is observed once more; the first
attempt's report is kept as REPORT.first-attempt.json.
"""

import json
from pathlib import Path
import subprocess
import sys
import time

SCHEDULE_PREFIX = "LARGE_REVIEW_SCHEDULE:"


def schedule(line):
    """Reads `LARGE_REVIEW_SCHEDULE:main wait_ns=N thread_cpu_ns=N wait_measured=1`."""
    name, *fields = line[len(SCHEDULE_PREFIX):].split()
    values = dict(field.split("=", 1) for field in fields)
    return name, {
        "SchedulerWaitMs": int(values["wait_ns"]) / 1_000_000,
        "ThreadCpuMs": int(values["thread_cpu_ns"]) / 1_000_000,
        "WaitMeasured": values["wait_measured"] == "1",
    }


def attributed(observed_ms, reading):
    """The observed time less the scheduler's wait, within [processor time, observed]."""
    floor = min(observed_ms, reading["ThreadCpuMs"])
    return max(floor, observed_ms - reading["SchedulerWaitMs"])


MARKERS = {
    "LARGE_REVIEW_MAIN_ENTERED": "ProcessToMainMarkerMs",
    "LARGE_REVIEW_HOME_RENDERED": "ProcessToHomeMarkerMs",
}
SCHEDULE_MARKERS = {"main": "ProcessToMainMarkerMs", "home": "ProcessToHomeMarkerMs"}
LIMITS_MS = {"ProcessToMainMarkerMs": 100, "ProcessToHomeMarkerMs": 1000}
RETRYABLE = "LARGE_REVIEW_TIMING_RETRYABLE"


def observe(enforced, report_path, command):
    """Runs the course once; writes its launch report and returns its failures and whether a retry is allowed."""
    observation = {"Schema": "bpr.large-review-launch.v1", "Command": command}
    observed = {}
    readings = {}
    course_retryable = False
    began = time.perf_counter_ns()
    with subprocess.Popen(command, stdout=subprocess.PIPE, text=True) as child:
        for line in child.stdout:
            text = line.rstrip("\n")
            name = MARKERS.get(text)
            if name:
                if name in observed:
                    raise RuntimeError("duplicate launch marker")
                observed[name] = (time.perf_counter_ns() - began) / 1_000_000
            elif text.startswith(SCHEDULE_PREFIX):
                marker, reading = schedule(text)
                if marker in SCHEDULE_MARKERS:
                    readings[SCHEDULE_MARKERS[marker]] = reading
            course_retryable = course_retryable or text == RETRYABLE
            sys.stdout.write(line)
            sys.stdout.flush()
        code = child.wait()
    for name, value in observed.items():
        reading = readings.get(name)
        observation[name] = attributed(value, reading) if reading else value
    observation["ObservedMs"] = observed
    observation["Schedule"] = readings
    observation["ExitCode"] = code
    observation["LimitsMs"] = LIMITS_MS
    observation["WithinLimits"] = all(
        0 <= observation.get(name, float("inf")) <= limit
        for name, limit in LIMITS_MS.items()
    )
    observation["Enforced"] = enforced
    Path(report_path).write_text(json.dumps(observation, indent=2) + "\n")
    # [[BPR-1495]]. Every refusal names its check and its measurement, so a
    # gate log says which one failed without the report file.
    failures = []
    if code:
        failures.append(f"fixture exit code {code}")
    missing = [name for name in MARKERS.values() if name not in observation]
    if missing:
        failures.append("missing launch marker " + ", ".join(missing))
    elif observed["ProcessToMainMarkerMs"] >= observed["ProcessToHomeMarkerMs"]:
        failures.append("main marker did not precede home marker")
    # BPR-1598. A launch limit missed while the process measurably waited for
    # a processor (2% of the observed time; none on a quiet machine) may be
    # observed once more, as the course's own timing misses may.
    timing_misses = []
    if enforced:
        for name, limit in LIMITS_MS.items():
            measured = observation.get(name)
            if measured is not None and not 0 <= measured <= limit:
                wait = readings.get(name, {}).get("SchedulerWaitMs", 0.0)
                failures.append(
                    f"{name}={measured:.1f} exceeds limit {limit}"
                    f" (observed {observed.get(name, float('nan')):.1f}, scheduler wait {wait:.1f})")
                timing_misses.append(wait >= 0.02 * observed.get(name, 0.0) > 0)
    only_timing = not missing and (not code or course_retryable)
    retryable = bool(failures) and only_timing and (course_retryable or (bool(timing_misses) and all(timing_misses)))
    return failures, retryable


def main():
    enforce, report_path, *command = sys.argv[1:]
    if enforce not in ("--enforce=1", "--enforce=0"):
        raise SystemExit("usage: large_review_launch.py --enforce=1|0 REPORT COMMAND...")
    enforced = enforce == "--enforce=1"
    report = Path(report_path)
    kept = report.with_name(report.stem + ".first-attempt" + report.suffix)
    kept.unlink(missing_ok=True)
    failures, retryable = observe(enforced, report, command)
    if failures and retryable:
        for failure in failures:
            print(f"LARGE_REVIEW_LAUNCH_FIRST_ATTEMPT: {failure}", flush=True)
        report.replace(kept)
        print(f"LARGE_REVIEW_COURSE_RETRIED: the first attempt missed a timing limit while the machine "
              f"kept the process waiting; its report is {kept}", flush=True)
        failures, _ = observe(enforced, report, command)
    if failures:
        for failure in failures:
            print(f"LARGE_REVIEW_LAUNCH_FAILED: {failure}", flush=True)
        return 1
    print("LARGE_REVIEW_LAUNCH_OBSERVED", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
