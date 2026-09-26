"""Content, exact loss accounting, and blocked-pipe public startup isolation."""
import fcntl
import os
from pathlib import Path
import re
import subprocess
import sys
import time


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def run(command, sink, environment=None):
    start = time.monotonic()
    result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=sink, env=environment, timeout=10, check=False)
    return result, time.monotonic() - start


driver, app, logging = (str(Path(item).resolve()) for item in sys.argv[1:])
with contained_temporary_directory(prefix="bpr-diagnostic-") as temporary:
    root = Path(temporary)
    result, elapsed = run([driver], subprocess.PIPE)
    assert result.returncode == 0 and result.stdout == b"terminal-diagnostics:complete\n"
    assert b"BPR_DIAGNOSTIC_PRIVATE_CANARY" not in result.stderr
    assert b"QT_DIAGNOSTIC:warning content=omitted" in result.stderr
    assert b"[omitted:unclassified-exception]" in result.stderr
    assert b"TERMINAL_DIAGNOSTICS:dropped=1" in result.stderr
    assert len(result.stderr) < 1024
    print(f"terminal-content bytes={len(result.stderr)} elapsed={elapsed:.3f}")
    with (root / "stress.log").open("wb+") as sink:
        result, elapsed = run([driver, "--stress"], sink)
        sink.seek(0)
        data = sink.read()
    assert result.returncode == 0 and b"TERMINAL_RECORD:recovered\n" in data
    retained = data.count(b"TERMINAL_RECORD:")
    dropped = sum(int(value) for value in re.findall(rb"TERMINAL_DIAGNOSTICS:dropped=(\d+)", data))
    assert retained + dropped == 80001, (retained, dropped)
    assert dropped > 0 and len(data) <= 80001 * 2048
    print(f"terminal-pressure attempts=80001 retained={retained} dropped={dropped} bytes={len(data)} elapsed={elapsed:.3f}")
    empty_path = root / "empty"
    empty_path.mkdir()
    environment = dict(os.environ, PATH=str(empty_path), QT_QPA_PLATFORM="offscreen",
                       BPR_AUTOMATED_TEST_BOUNDARY="1")
    composed_environment = dict(os.environ, QT_QPA_PLATFORM="offscreen", BPR_AUTOMATED_TEST_BOUNDARY="1")
    result, elapsed = run([logging, "--recovery-log"], subprocess.PIPE, composed_environment)
    assert result.returncode == 0, result.stdout
    assert b"AGENT_WORKSPACE_RECOVERY_REFUSED:path=omitted:" in result.stderr
    assert b"BPR_DIAGNOSTIC_PRIVATE_CANARY" not in result.stdout + result.stderr
    print(f"terminal-recovery classified=1 elapsed={elapsed:.3f}")
    commands = ([driver], [app, "--self-test", "--automated-run-budget-ms", "2000"])
    baseline, _ = run(commands[1], subprocess.PIPE, environment)
    assert baseline.returncode == 1 and baseline.stdout == b"STARTUP_FAILED\n"
    assert b"STARTUP_FAILURE_SOURCE:profile-runtime-construction:" in baseline.stderr
    for closed in (False, True):
        reader, writer = os.pipe()
        try:
            if closed:
                os.close(reader)
                reader = None
            else:
                os.set_blocking(writer, False)
                try:
                    while True:
                        os.write(writer, b"x" * 4096)
                except BlockingIOError:
                    pass
                os.set_blocking(writer, True)
            flags = fcntl.fcntl(writer, fcntl.F_GETFL)
            for command in commands:
                result, elapsed = run(command, writer, environment)
                expected = (0, b"terminal-diagnostics:complete\n") if command[0] == driver else (1, baseline.stdout)
                assert (result.returncode, result.stdout) == expected
                assert fcntl.fcntl(writer, fcntl.F_GETFL) == flags
                print(f"terminal-pipe closed={int(closed)} public_app={int(command[0] == app)} elapsed={elapsed:.3f}")
            result, elapsed = run([logging, "--full-log"], writer, composed_environment)
            assert result.returncode == 0 and result.stdout == b"application-logging-failure-isolation:pass\n"
            assert fcntl.fcntl(writer, fcntl.F_GETFL) == flags
            print(f"terminal-pipe closed={int(closed)} authenticated_workflow=1 full_log=1 elapsed={elapsed:.3f}")
        finally:
            os.close(writer)
            if reader is not None:
                os.close(reader)
print("terminal-diagnostic-contract:pass")
