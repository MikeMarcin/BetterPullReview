"""Run one large-review course, and once more if a loaded machine made it miss a timing limit.

    large_review_course.py REPORT -- COMMAND...

BPR-1598. The courses attribute their timed intervals: each leaves out the time
its threads waited for a processor another process held. One effect of a
loaded machine survives that: a GUI thread blocked on a lock a preempted thread
holds (malloc's fork lock while a worker starts Git) is blocked, not waiting.
So a course that missed a timing limit while its GUI thread was measurably kept
waiting prints LARGE_REVIEW_TIMING_RETRYABLE, and this driver runs it once
more. The first attempt's report is kept beside the second's as
REPORT.first-attempt.json, and its log stays in the row's output. A course
that fails any other way, or misses a timing limit on a quiet machine, is not
run again. The exit code is the last attempt's, so a skip (77) passes through.
"""

from pathlib import Path
import subprocess
import sys

RETRYABLE = "LARGE_REVIEW_TIMING_RETRYABLE"


def attempt(command):
    """Runs the course, echoing its output; returns its exit code and whether it may be retried."""
    retryable = False
    with subprocess.Popen(command, stdout=subprocess.PIPE, text=True) as child:
        for line in child.stdout:
            retryable = retryable or line.rstrip("\n") == RETRYABLE
            sys.stdout.write(line)
            sys.stdout.flush()
        code = child.wait()
    return code, retryable


def main():
    if len(sys.argv) < 4 or sys.argv[2] != "--":
        raise SystemExit("usage: large_review_course.py REPORT -- COMMAND...")
    report = Path(sys.argv[1])
    command = sys.argv[3:]
    kept = report.with_name(report.stem + ".first-attempt" + report.suffix)
    kept.unlink(missing_ok=True)
    code, retryable = attempt(command)
    if code != 0 and retryable:
        if report.exists():
            report.replace(kept)
        print(f"LARGE_REVIEW_COURSE_RETRIED: the first attempt missed a timing limit while the machine "
              f"kept its GUI thread waiting; its report is {kept}", flush=True)
        code, _ = attempt(command)
    return code


if __name__ == "__main__":
    sys.exit(main())
