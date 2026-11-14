#!/usr/bin/env python3
"""[[BPR-2177]] (look ask-v004, owner 2026-11-14: "still doesnt work"). The
application itself, as an attended look runs it: returning-review-v1 at its
next push, the Fixture agent, offscreen. With BPR_ATTENDED_FIXTURE_AGENT_CHECK
the build presses What changed since my review with the line-6 chip set, then
every quick ask the review bar, the file and a thread offer, a typed question
with the line chip, a second one in the same conversation and two asks queued
at once; every turn must be answered, the footer must name the Fixture agent,
and no `claude` or `codex` first on PATH may start.

    fixture_agent_every_ask.py APPLICATION [SHOT.png]
"""

import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def run(application: Path, data: Path, phase: str, ready: str, environment: dict, wait: int) -> str:
    log = data / f"{phase}.log"
    command = [str(application), "--github-fixture", "returning-review-v1", "--attended-launch",
               "--attended-open-file", "src/engine/renderer.cpp",
               "--epoch-fixture-root", str(data / "data"), "--epoch-fixture-phase", phase]
    with log.open("w", encoding="utf-8") as output:
        process = subprocess.Popen(command, env=environment, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
                if line.startswith(ready) or (line.startswith("ATTENDED_") and "_FAILED" in line):
                    return line
            if process.poll() is not None:
                return f"EXITED {process.returncode}: {log.read_text(encoding='utf-8', errors='replace')[-2000:]}"
            time.sleep(0.25)
        return f"TIMEOUT after {wait}s"
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    application = Path(sys.argv[1])
    shot = sys.argv[2] if len(sys.argv) > 2 else None
    with contained_temporary_directory(prefix="every-ask.") as scratch:
        data = Path(scratch).resolve()
        real = data / "real-agents"
        real.mkdir()
        marked = data / "real-agent-started"
        for name in ("claude", "codex"):
            program = real / name
            program.write_text(f"#!/bin/sh\necho \"$0 $*\" >> '{marked}'\nexit 1\n")
            program.chmod(0o755)
        environment = dict(os.environ)
        environment["PATH"] = f"{real}{os.pathsep}{environment.get('PATH', '')}"
        environment["QT_QPA_PLATFORM"] = "offscreen"
        environment.setdefault("BPR_FIXTURE_AGENT_PAUSE_MS", "30")
        first = run(application, data, "single", "ATTENDED_FILE_SHOWN", environment, 90)
        if not first.startswith("ATTENDED_FILE_SHOWN"):
            print(f"fixture-agent-every-ask: the first head did not open: {first}")
            return 1
        environment["BPR_ATTENDED_FIXTURE_AGENT_CHECK"] = "1"
        if shot:
            environment["BPR_ATTENDED_FOOTER_SHOT"] = shot
        line = run(application, data, "reuse", "ATTENDED_FIXTURE_AGENT_ANSWERED", environment, 360)
        print(line)
        if not line.startswith("ATTENDED_FIXTURE_AGENT_ANSWERED:Fixture agent") or ":footer=Fixture agent" not in line or not line.endswith(":report=1"):
            print("fixture-agent-every-ask: not every ask was answered by the Fixture agent")
            return 1
        if marked.exists():
            print(f"fixture-agent-every-ask: a real agent program started: {marked.read_text().strip()}")
            return 1
    print("fixture-agent-every-ask:pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
