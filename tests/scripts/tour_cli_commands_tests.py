"""Process-boundary CLI checks with a read-only fixture provider at normal composition."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time


def main():
    binary = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="bpr-cli-process-") as temporary:
        root = Path(temporary).resolve() / "private data"
        def invoke(*args, data=None):
            return subprocess.run([binary, "tour", *args, "--data-root", str(root)],
                                  input=data, capture_output=True, timeout=20)
        opened = invoke("open", "https://github.com/o/r/pull/7", "--json")
        assert opened.returncode == 0, opened.stdout
        target = json.loads(opened.stdout)["data"]
        select = ["--profile", "1", "--draft", target["draft_id"]]
        revision = target["revision"]
        refused = invoke("edit", "problem", *select, "--expected-revision", revision, "--json")
        assert refused.returncode == 3 and json.loads(refused.stdout)["error"]["code"] == "interaction_required"
        raw = "Problem α\r\n\r\n".encode()
        edited = invoke("edit", "problem", *select, "--expected-revision", revision,
                        "--file", "-", "--json", data=raw)
        assert edited.returncode == 0 and not edited.stderr, edited.stdout
        revision = json.loads(edited.stdout)["data"]["revision"]
        shown = invoke("show", "problem", *select, "--json")
        assert json.loads(shown.stdout)["data"]["text"].encode() == raw
        human = invoke("show", "problem", *select)
        assert human.returncode == 0 and not human.stdout.startswith(b"{")
        assert b"revision " + revision.encode() in human.stdout and b"Problem" in human.stdout
        # An explicit empty stream is an empty field, not a prompt or a guessed default.
        cleared = invoke("edit", "solution", *select, "--expected-revision", revision,
                         "--file", "-", "--json", data=b"")
        assert cleared.returncode == 0
        revision = json.loads(cleared.stdout)["data"]["revision"]
        for body, code in [(b"\xff", "invalid_utf8"), (b"a"*65537, "input_too_large")]:
            failed = invoke("edit", "problem", *select, "--expected-revision", revision,
                            "--file", "-", "--json", data=body)
            assert failed.returncode == 2 and json.loads(failed.stdout)["error"]["code"] == code
        if os.name != "nt":
            command = [binary, "tour", "edit", "problem", *select, "--expected-revision", revision,
                       "--file", "-", "--json", "--data-root", str(root)]
            waiting = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                time.sleep(0.5)
                assert waiting.poll() is None
                # Waiting for author text does not retain the profile runtime owner.
                status = invoke("status", *select, "--json")
                assert status.returncode == 0, status.stdout
                waiting.send_signal(signal.SIGINT)
                assert waiting.wait(timeout=10) == 130
                output = waiting.stdout.read()
                assert json.loads(output)["error"]["code"] == "cancelled"
                assert not waiting.stderr.read()
            finally:
                if waiting.poll() is None:
                    waiting.kill()
                    waiting.wait(timeout=10)
                waiting.stdin.close()
            assert json.loads(invoke("show", "problem", *select, "--json").stdout)["data"]["text"].encode() == raw
    print("tour-cli-process-commands:pass")


if __name__ == "__main__":
    main()
