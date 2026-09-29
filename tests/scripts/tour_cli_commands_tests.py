"""Process-boundary CLI checks with a read-only fixture provider at normal composition."""
import json
import os
import errno
import select as readiness
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time


def editor_checks(binary, root, select, invoke, temporary):
    import pty
    fixture = Path(temporary) / "trusted editor fixture.py"
    fixture.write_text(r'''import json, os, pathlib, signal, subprocess, sys, time
mode, literal, name = sys.argv[1:]
assert literal == "$(touch NOT_EXECUTED); | & `literal`"
path = pathlib.Path(name)
assert path.is_absolute() and path.read_bytes()
assert path.stat().st_mode & 0o077 == 0
assert path.parent.stat().st_mode & 0o077 == 0
pathlib.Path(os.environ["CANDIDATE_RECORD"]).write_text(name)
pathlib.Path(os.environ["CHILD_RECORD"]).write_text(str(os.getpid()))
if mode == "success": path.write_bytes("editor α\r\n\r\n".encode())
if mode == "invalid": path.write_bytes(b"\xff")
if mode == "failure":
    path.write_text("recoverable candidate")
    sys.exit(9)
if mode == "crash": os.kill(os.getpid(), signal.SIGKILL)
if mode == "symlink":
    path.unlink()
    path.symlink_to(os.environ["OUTSIDE_FILE"])
if mode == "conflict":
    response = subprocess.run(json.loads(os.environ["CONCURRENT_EDIT"]), input=b"concurrent winner", capture_output=True)
    assert response.returncode == 0, response.stdout
    path.write_text("uncommitted editor candidate")
if mode == "cancel": time.sleep(30)
''')
    outside = Path(temporary) / "outside file.md"
    outside.write_text("outside must remain unchanged")
    record = Path(temporary) / "candidate path.txt"
    child_record = Path(temporary) / "child pid.txt"
    def revision():
        return json.loads(invoke("status", *select, "--json").stdout)["data"]["revision"]
    def tty_editor(mode, expected):
        record.unlink(missing_ok=True)
        child_record.unlink(missing_ok=True)
        env = dict(os.environ, BPR_TOUR_EDITOR_ARGV=json.dumps([sys.executable, str(fixture), mode,
                   "$(touch NOT_EXECUTED); | & `literal`"]), CANDIDATE_RECORD=str(record),
                   CHILD_RECORD=str(child_record), OUTSIDE_FILE=str(outside),
                   CONCURRENT_EDIT=json.dumps([binary, "tour", "edit", "problem", *select,
                   "--expected-revision", expected, "--file", "-", "--json", "--data-root", str(root)]))
        master, slave = pty.openpty()
        child = subprocess.Popen([binary, "tour", "edit", "problem", *select, "--expected-revision", expected,
                                  "--data-root", str(root)], stdin=slave, stdout=slave, stderr=slave, env=env)
        os.close(slave)
        output = bytearray()
        deadline = time.monotonic() + 15
        cancelled = False
        try:
            while time.monotonic() < deadline:
                if mode == "cancel" and record.exists() and not cancelled:
                    assert invoke("status", *select, "--json").returncode == 0
                    child.send_signal(signal.SIGINT)
                    cancelled = True
                if readiness.select([master], [], [], 0.05)[0]:
                    try:
                        part = os.read(master, 8192)
                    except OSError as error:
                        if error.errno == errno.EIO:
                            break
                        raise
                    if not part: break
                    output.extend(part)
            child.wait(timeout=5)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=5)
            os.close(master)
        return child.returncode, bytes(output), Path(record.read_text())
    for mode, code in [("success", 0), ("failure", 7), ("crash", 7), ("invalid", 2), ("symlink", 7), ("cancel", 130), ("conflict", 4)]:
        before = revision()
        previous = json.loads(invoke("show", "problem", *select, "--json").stdout)["data"]["text"]
        actual, output, candidate = tty_editor(mode, before)
        assert actual == code, (mode, actual, output)
        assert b"\x1b" not in output
        if mode == "success":
            assert not candidate.exists()
            assert json.loads(invoke("show", "problem", *select, "--json").stdout)["data"]["text"] == "editor α\r\n\r\n"
            undone = invoke("undo", *select, "--expected-revision", revision(), "--json")
            assert undone.returncode == 0 and json.loads(undone.stdout)["data"]["transaction_label"] == "Edit Problem"
            assert json.loads(invoke("show", "problem", *select, "--json").stdout)["data"]["text"] == previous
        else:
            assert str(candidate).encode() in output and candidate.parent.exists()
            current = json.loads(invoke("show", "problem", *select, "--json").stdout)["data"]["text"]
            assert current == ("concurrent winner" if mode == "conflict" else previous)
            assert revision() != before if mode == "conflict" else revision() == before
            candidate.unlink()
            candidate.parent.rmdir()
        assert outside.read_text() == "outside must remain unchanged"
        assert not (Path(temporary) / "NOT_EXECUTED").exists()
        if child_record.exists():
            try:
                os.kill(int(child_record.read_text()), 0)
            except ProcessLookupError:
                pass
            else:
                raise AssertionError("Editor process was left running")


def walk_checks(binary, root, select, invoke, temporary):
    import pty
    def terminal_run(args, actions=(), eof=False):
        master, slave = pty.openpty()
        child = subprocess.Popen([binary, "tour", *args, "--data-root", str(root), "--no-color"],
                                 stdin=slave, stdout=slave, stderr=slave,
                                 env=dict(os.environ, COLUMNS="24", NO_COLOR="1"))
        os.close(slave)
        output = bytearray()
        pending = list(actions)
        prompted = 0
        deadline = time.monotonic() + 20
        try:
            while time.monotonic() < deadline:
                if readiness.select([master], [], [], 0.05)[0]:
                    try:
                        part = os.read(master, 8192)
                    except OSError as error:
                        if error.errno == errno.EIO: break
                        raise
                    if not part: break
                    output.extend(part)
                    count = output.count(b"> ")
                    if count > prompted:
                        prompted = count
                        # Every human wait releases the profile owner.
                        assert invoke("status", *select, "--json").returncode == 0
                        if pending:
                            os.write(master, pending.pop(0).encode() + b"\n")
                        elif eof:
                            os.write(master, b"\x04")
                            eof = False
            child.wait(timeout=3)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=5)
            os.close(master)
        assert not pending, (args, output)
        assert b"\x1b" not in output, output
        return child.returncode, bytes(output)
    field = Path(temporary) / "manual field.md"
    for name, content in [("problem", "Manual problem α\n"), ("solution", "Manual solution β\n")]:
        field.write_text(content)
        code, output = terminal_run(["edit", name, *select, "--file", str(field)])
        assert code == 0, output
    field.write_text("Comment with OSC \x1b]52;c;ZXZpbA==\x07 and bidi \u202e literal\n")
    for path in ["資料 α.txt:1-1", "second.cpp:1-1"]:
        code, output = terminal_run(["add", path, *select, "--file", str(field)])
        assert code == 0, output
    status = json.loads(invoke("status", *select, "--json").stdout)["data"]
    assert [x["stop_id"] for x in status["stops"]] == ["1", "2"]
    before_show = status["revision"]
    source = json.loads(invoke("show", "stop:1", *select, "--source", "--json").stdout)["data"]
    assert source["source_availability"] == "available" and source["text"] == field.read_text()
    assert json.loads(invoke("status", *select, "--json").stdout)["data"]["revision"] == before_show
    code, output = terminal_run(["walk", *select], ["m after 2", "n", "p", "r", "r"])
    assert code == 0 and b"1 | fixture source" in output and b"Source unavailable" not in output, output
    assert b"\\u001b" in output and "資料 α.txt".encode() in output, output
    status = json.loads(invoke("status", *select, "--json").stdout)["data"]
    assert [x["stop_id"] for x in status["stops"]] == ["2", "1"]
    assert all(x["ready"] and x["completion_origin"] == "observed" for x in status["stops"])
    for cursor, field_name in [(None, "Problem"), ("1", "Solution"), ("2", "Stop 2"), ("3", "Stop 1")]:
        command = ["preview", *select] + (["--cursor", cursor] if cursor else [])
        page = invoke(*command, "--json")
        assert page.returncode == 0, page.stdout
        data = json.loads(page.stdout)["data"]
        assert data["field"] == field_name and not data["remote_writes"] and not data["publication_authorized"]
        assert len(page.stdout) <= 1048576
        code, output = terminal_run(command)
        assert code == 0, output
    code, output = terminal_run(["walk", *select], ["d", "q"])
    assert code == 0, output
    assert len(json.loads(invoke("status", *select, "--json").stdout)["data"]["stops"]) == 1
    code, output = terminal_run(["walk", *select], eof=True)
    assert code == 130, output
    refused = invoke("walk", *select, "--json")
    assert refused.returncode == 3 and json.loads(refused.stdout)["error"]["code"] == "interaction_required"
    print("tour-cli-raw-terminal-rehearsal:pass")


def owner_checks(binary, root, select, invoke, temporary):
    """A real owner process serves the tour channel; killing it never strands or duplicates work."""
    def revision():
        return json.loads(invoke("status", *select, "--json").stdout)["data"]["revision"]
    def problem():
        return json.loads(invoke("show", "problem", *select, "--json").stdout)["data"]["text"]
    def edit(text, expected, request_id):
        return invoke("edit", "problem", *select, "--expected-revision", expected, "--request-id", request_id,
                      "--file", "-", "--json", data=text.encode())
    def owner(*extra):
        child = subprocess.Popen([binary, "--serve-owner", str(root), "1", *extra], stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, text=True)
        ready = readiness.select([child.stdout], [], [], 15)[0]
        assert ready and child.stdout.readline().strip() == "ready", child.stderr.read() if child.poll() is not None else ""
        return child
    def kill(child):
        child.send_signal(signal.SIGKILL)
        child.wait(timeout=10)
    current = revision()
    before_first = current
    first = owner()
    try:
        forwarded = edit("Forwarded to a real owner α\n", current, "owner-1")
        assert forwarded.returncode == 0, forwarded.stdout
        current = json.loads(forwarded.stdout)["data"]["revision"]
        assert problem() == "Forwarded to a real owner α\n" and revision() == current
        assert first.poll() is None
    finally:
        kill(first)
    # A killed owner leaves its socket and no lease: the next command runs standalone and sees the commit.
    assert problem() == "Forwarded to a real owner α\n" and revision() == current
    recorded = json.loads(invoke("status", *select, "--request-id", "owner-1", "--json").stdout)["data"]
    assert recorded["operation_recorded"] is True and recorded["operation_revision"] == current
    duplicate = edit("Forwarded to a real owner α\n", before_first, "owner-1")
    assert duplicate.returncode == 0 and json.loads(duplicate.stdout)["data"]["revision"] == current
    # A stale endpoint from the dead owner is replaced by the next owner, which serves again.
    second = owner()
    try:
        again = edit("Second owner β\n", current, "owner-2")
        assert again.returncode == 0, again.stdout
        current = json.loads(again.stdout)["data"]["revision"]
    finally:
        kill(second)
    assert problem() == "Second owner β\n"
    # The owner commits and dies before answering: the caller is told the outcome is unknown, the
    # request ID proves the commit, and a retry returns the stored outcome without a second edit.
    stalled = owner("--stall-after-commit")
    caller = subprocess.Popen([binary, "tour", "edit", "problem", *select, "--expected-revision", current,
                               "--request-id", "owner-3", "--file", "-", "--json", "--data-root", str(root)],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        caller.stdin.write("Committed before the owner died γ\n".encode())
        caller.stdin.close()
        ready = readiness.select([stalled.stdout], [], [], 15)[0]
        assert ready and stalled.stdout.readline().strip() == "committed"
    finally:
        kill(stalled)
    output, _ = caller.communicate(timeout=20)
    assert caller.returncode == 6 and json.loads(output)["error"]["code"] == "outcome_unknown", output
    assert problem() == "Committed before the owner died γ\n"
    proven = json.loads(invoke("status", *select, "--request-id", "owner-3", "--json").stdout)["data"]
    assert proven["operation_recorded"] is True and proven["operation_revision"] == revision()
    committed = revision()
    retry = edit("Committed before the owner died γ\n", current, "owner-3")
    assert retry.returncode == 0 and json.loads(retry.stdout)["data"]["revision"] == committed
    assert revision() == committed and problem() == "Committed before the owner died γ\n"
    print("tour-cli-owner-crash:pass")


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
        undone = invoke("undo", *select, "--expected-revision", revision, "--json")
        assert undone.returncode == 0, undone.stdout
        assert json.loads(undone.stdout)["data"]["transaction_label"] == "Edit Problem"
        revision = json.loads(undone.stdout)["data"]["revision"]
        assert json.loads(invoke("show", "problem", *select, "--json").stdout)["data"]["text"] == ""
        redone = invoke("redo", *select, "--expected-revision", revision, "--json")
        assert redone.returncode == 0, redone.stdout
        revision = json.loads(redone.stdout)["data"]["revision"]
        assert json.loads(invoke("show", "problem", *select, "--json").stdout)["data"]["text"].encode() == raw
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
        if os.name != "nt":
            editor_checks(binary, root, select, invoke, temporary)
            walk_checks(binary, root, select, invoke, temporary)
            owner_checks(binary, root, select, invoke, temporary)
    print("tour-cli-process-commands:pass")


if __name__ == "__main__":
    main()
