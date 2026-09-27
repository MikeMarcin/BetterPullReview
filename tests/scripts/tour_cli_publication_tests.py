"""Actual terminal confirmation, lease release and forced publication death."""
import errno
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile
import time


def main():
    if os.name != "posix":
        print("POSIX terminal mechanics only; native Windows terminal remains pending")
        return
    import pty
    binary = str(Path(sys.argv[1]).resolve())
    def run(mode, root):
        return subprocess.run([binary, "terminal", mode, str(root)], capture_output=True, timeout=15)
    def terminal(mode, root, answer=b"publish\n", concurrent=None, kill=False):
        master, slave = pty.openpty()
        child = subprocess.Popen([binary, "terminal", mode, str(root)], stdin=slave, stdout=slave, stderr=slave)
        os.close(slave)
        output = bytearray()
        answered = False
        killed = False
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if kill and (root / "kill-ready").exists():
                    child.kill()
                    killed = True
                    break
                if select.select([master], [], [], .02)[0]:
                    try:
                        data = os.read(master, 65536)
                    except OSError as error:
                        if error.errno == errno.EIO: break
                        raise
                    if not data: break
                    output.extend(data)
                if b"Type publish to authorize" in output and not answered:
                    assert run("read", root).returncode == 0, "profile lease held during prompt"
                    if concurrent:
                        assert run(concurrent, root).returncode == 0
                    os.write(master, answer)
                    answered = True
            child.wait(timeout=3)
            assert not kill or killed, output
            assert b"\x1b" not in output
            return child.returncode, bytes(output)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=3)
            os.close(master)
    with tempfile.TemporaryDirectory(prefix="bpr-tour-publication-") as directory:
        parent = Path(directory)
        parent.chmod(0o700)
        for mode, answer, concurrent, expected in [("publish", b"publish\n", None, 0), ("cancel", b"\n", None, 130), ("yes", b"yes\n", None, 130), ("stale", b"publish\n", "edit", 4), ("unqualified", b"publish\n", None, 5)]:
            root = parent / mode
            root.mkdir(mode=0o700)
            code, output = terminal(mode, root, answer, concurrent)
            assert code == expected, (mode, code, output)
            assert (b"writes=4 creates=1 submits=1" if expected == 0 else b"writes=0") in output, output
        for step in range(4):
            for boundary in ("prepared", "witnessed", "applied"):
                root = parent / f"{step}-{boundary}"
                root.mkdir(mode=0o700)
                code, output = terminal(f"kill-{step}-{boundary}", root, kill=True)
                assert code == -signal.SIGKILL, output
                before = (root / "server").read_bytes() if (root / "server").exists() else None
                first = run("recover", root)
                second = run("recover", root)
                assert first.returncode in (0, 6) and second.returncode in (0, 6), (first.stdout, first.stderr, second.stdout)
                if before is not None:
                    assert (root / "server").read_bytes() == before, "read recovery mutated the fixture server"
                if step == 3 and boundary == "applied":
                    assert b"writes=4 creates=1 submits=1" in second.stdout
                elif boundary != "witnessed":
                    code, output = terminal("resume", root)
                    # Completion before local receipt or the description acknowledgement may require no new write.
                    assert code == 0 and b"writes=4 creates=1 submits=1" in output, (step, boundary, code, output)
                else:
                    code, output = terminal("resume", root)
                    assert code in (4, 6) and b"Type publish to authorize" not in output, output
                print(f"terminal-kill:{step}:{boundary}:pass")
        root = parent / "acknowledged"
        root.mkdir(mode=0o700)
        code, output = terminal("acknowledged", root, kill=True)
        assert code == -signal.SIGKILL
        before = (root / "server").read_bytes()
        assert run("recover", root).returncode == 0
        assert run("recover", root).returncode == 0
        assert (root / "server").read_bytes() == before
    print("tour-cli-terminal-publication:pass")


if __name__ == "__main__":
    main()
