"""[[BPR-1168]]. The shipped `bpr cycle` and a real desktop owner work one cycle, in both directions.

One process owns the profile at a time. While the desktop owns it, every cycle
command is forwarded over its same-user command channel and run on its store;
when nobody owns it, the command takes the lease itself. The course checks what
each interface shows of the other's work, the reviewer's unsaved and composing
text, repeated and concurrent requests, owner death and replacement, and an
owner that speaks another protocol version.
"""
import argparse
import concurrent.futures
import contextlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from profile_activation_contract import private_fixture_parent  # noqa: E402


POSIX = os.name != "nt"


class Desktop:
    """The desktop reviewer: a real profile owner, driven one command at a time."""

    def __init__(self, application, data, workspace, index):
        self.signal = workspace / f"desktop-{index}.quit"
        self.log = (workspace / f"desktop-{index}.log").open("w+")
        self.process = subprocess.Popen([application, str(data), "1", str(self.signal)], stdout=self.log, stderr=subprocess.STDOUT, text=True)
        self.seen = 0
        self.endpoint = self.await_line("OWNER_ENDPOINT ").split(" ", 1)[1]
        self.await_line("OWNER_READY")

    def lines(self):
        self.log.seek(0)
        return self.log.read().splitlines()

    def await_line(self, prefix, timeout=120):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            lines = self.lines()
            for index in range(self.seen, len(lines)):
                if lines[index].startswith(prefix):
                    self.seen = index + 1
                    return lines[index]
            if self.process.poll() is not None:
                raise AssertionError(f"desktop exited {self.process.returncode}: {lines}")
            time.sleep(0.02)
        raise AssertionError(f"missing {prefix}: {self.lines()}")

    def ask(self, command, prefix):
        pending = Path(str(self.signal) + ".command.tmp")
        pending.write_text(command, encoding="utf-8")
        os.replace(pending, str(self.signal) + ".command")
        return self.await_line(prefix)

    def quit(self):
        self.signal.touch()
        self.await_line("OWNER_RELEASED", 60)
        assert self.process.wait(timeout=60) == 0

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=10)
        self.log.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    parser.add_argument("--cycle-application", required=True)
    args = parser.parse_args()
    with private_fixture_parent() as parent, contained_temporary_directory(prefix="cycle-", parent=parent) as temporary:
        workspace = Path(temporary).resolve()
        make_root_private(workspace)
        data = workspace / "data"
        data.mkdir(mode=0o700)
        project = workspace / "project"
        (project / "src").mkdir(parents=True)
        (project / "src" / "cache.cpp").write_text("int lookup(int key)\n{\n\treturn table[key];\n}\n", encoding="utf-8")
        (project / "README.md").write_text("# Cache\n", encoding="utf-8")

        def bpr(*arguments, stdin=b"", cwd=None):
            return subprocess.run([args.cycle_application, "cycle", *arguments, "--json", "--profile", "1", "--data-root", str(data)],
                                  input=stdin, capture_output=True, timeout=120, cwd=cwd or workspace)

        def envelope(result):
            assert result.stderr == b"", f"JSON calls keep standard error empty: {result.stderr!r}"
            return json.loads(result.stdout)

        def ok(*arguments, **options):
            result = bpr(*arguments, **options)
            value = envelope(result)
            assert result.returncode == 0 and value["outcome"] == "ok", f"{arguments}: {result.returncode} {value}"
            return value["data"]

        def refused(code, exit_code, *arguments, **options):
            result = bpr(*arguments, **options)
            value = envelope(result)
            assert result.returncode == exit_code and value["error"]["code"] == code, f"{arguments}: expected {code}/{exit_code}, got {result.returncode} {value}"
            return value["error"]

        def bodies():
            return [note["body"] for note in ok("show", "RC-1")["notes"]]

        @contextlib.contextmanager
        def lease_held():
            """Another process holds the profile: a lock on the profile root (POSIX)."""
            import fcntl
            holder = os.open(data, os.O_RDONLY)
            try:
                fcntl.flock(holder, fcntl.LOCK_EX | fcntl.LOCK_NB)
                yield
            finally:
                os.close(holder)

        # CLI alone: no desktop, the command takes the lease and opens the cycle itself.
        opened = ok("open", str(project))
        assert opened["cycle"] == "RC-1" and opened["created"] is True
        ok("note", "RC-1", "--file", "src/cache.cpp", "--lines", "3", "--body-file", "-", stdin=b"Bounds-check the key.\n")

        desktop = Desktop(args.application, data, workspace, 1)
        try:
            assert desktop.ask("resume", "RESUMED") == "RESUMED project"
            seen = desktop.ask("wait-notes 1", "NOTES ")
            assert seen == "NOTES 1|I1:Bounds-check the key.", seen
            # The desktop owns the profile now: nothing else can take its lease ...
            if POSIX:
                try:
                    with lease_held():
                        raise AssertionError("the desktop holds the lease")
                except BlockingIOError:
                    pass
            # ... and every command reaches it, on its store, and its window shows the result.
            assert bodies() == ["Bounds-check the key.\n"]
            noted = ok("note", "RC-1", "--body-file", "-", stdin=b"From the terminal.\n")
            assert noted["item"] == "I2"
            assert desktop.ask("wait-notes 2", "NOTES ") == "NOTES 2|I1:Bounds-check the key.|I2:From the terminal.", "the window shows the terminal's note"
            # The working folder still names its cycle, through the owner.
            assert ok("show", cwd=project)["cycle"] == "RC-1"

            # The reviewer's own unsaved note text survives the terminal's writes,
            # and saving it adds a note beside them.
            assert desktop.ask("type-note Typed in the window.", "NOTE_") == "NOTE_TYPED"
            ok("note", "RC-1", "--body-file", "-", stdin=b"While the window types.\n")
            assert desktop.ask("wait-notes 3", "NOTES ").startswith("NOTES 3|")
            assert desktop.ask("note-text", "NOTE_TEXT ") == "NOTE_TEXT Typed in the window. visible=1", "unsaved text kept, still open"
            assert desktop.ask("focus", "FOCUS ") == "FOCUS note", "the caret stays where the reviewer was"
            assert desktop.ask("save-note", "NOTE_SAVED") == "NOTE_SAVED ", "the window's note is saved"
            assert sorted(bodies()) == sorted(["Bounds-check the key.\n", "From the terminal.\n", "While the window types.\n", "Typed in the window."])

            # Compose: both interfaces edit one round draft under expected revisions.
            assert desktop.ask("compose", "COMPOSE") == "COMPOSED"
            draft = ok("review", "RC-1")
            revision = draft["draft_revision"]
            changed = ok("review", "RC-1", "--exclude", "I2", "--expected-revision", revision)
            assert changed["changed"] is True and "I2" not in changed["items"]
            shown = desktop.ask(f"wait-share revision={changed['draft_revision']}", "SHARE ")
            assert "I2:0" in shown, f"the window shows the terminal's change: {shown}"
            stale = refused("stale_revision", 4, "review", "RC-1", "--include", "I2", "--expected-revision", revision)
            assert stale["detail"]["draft_revision"] == changed["draft_revision"], "a stale edit is refused and names the current revision"
            # No undo in Review Cycles: a change is reversed by its counter-change, from either interface.
            assert desktop.ask("toggle I2", "TOGGLED") == "TOGGLED"
            reversed_draft = ok("review", "RC-1")
            assert "I2" in reversed_draft["items"] and int(reversed_draft["draft_revision"]) > int(changed["draft_revision"]), "the window's reversal reaches the terminal"

            # Unsaved goal text in the window is saved before a terminal freeze reads the
            # draft: the freeze can neither omit it nor overwrite it unseen.
            before_goal = ok("review", "RC-1")["draft_revision"]
            assert desktop.ask("goal Ship the cache", "GOAL_") == "GOAL_TYPED"
            frozen_stale = refused("stale_revision", 4, "freeze", "RC-1", "--expected-revision", before_goal)
            after_goal = ok("review", "RC-1")
            assert after_goal["goal"] == "Ship the cache" and int(after_goal["draft_revision"]) > int(before_goal), f"the goal was saved first: {after_goal}"
            assert frozen_stale["detail"]["draft_revision"] == after_goal["draft_revision"]
            assert "modified=0" in desktop.ask("share", "SHARE "), "and the window's field is no longer unsaved"

            # Input-method text is not text yet: the owner neither saves it nor reads past it.
            assert desktop.ask("goal-compose 中", "COMPOSING") == "COMPOSING"
            busy = refused("draft_busy", 4, "review", "RC-1")
            assert "input method" in busy["message"]
            refused("draft_busy", 4, "freeze", "RC-1", "--expected-revision", after_goal["draft_revision"])
            ok("note", "RC-1", "--body-file", "-", stdin=b"A note is not the draft.\n")
            assert desktop.ask("goal-commit 中", "COMMITTED") == "COMMITTED"
            committed = ok("review", "RC-1")
            assert committed["goal"] == "Ship the cache中", f"the committed text is saved and read: {committed['goal']}"
            frozen = ok("freeze", "RC-1", "--expected-revision", committed["draft_revision"])
            assert frozen["round"] == "F1"
            assert "frozen=1" in desktop.ask("wait-share frozen", "SHARE "), "the window shows the round frozen"

            # A repeated request is answered as it was first answered and applied once;
            # the same identity for another request is refused.
            count = len(bodies())
            first = ok("note", "RC-1", "--operation-id", "repeat-1", "--body-file", "-", stdin=b"Once.\n")
            again = bpr("note", "RC-1", "--operation-id", "repeat-1", "--body-file", "-", stdin=b"Once.\n")
            assert again.returncode == 0 and envelope(again)["data"] == first and envelope(again)["operation_id"] == "repeat-1", "the same answer"
            assert len(bodies()) == count + 1 and bodies().count("Once.\n") == 1, "applied once"
            refused("conflict", 4, "note", "RC-1", "--operation-id", "repeat-1", "--body-file", "-", stdin=b"Something else.\n")
            assert len(bodies()) == count + 1

            # Concurrent callers each get their own answer, in whatever order the owner serves them.
            def parallel_note(index):
                result = bpr("note", "RC-1", "--operation-id", f"parallel-{index}", "--body-file", "-", stdin=f"Parallel {index}.\n".encode())
                return index, result
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
                answers = list(pool.map(parallel_note, range(4)))
            items = set()
            for index, result in answers:
                value = envelope(result)
                assert result.returncode == 0 and value["operation_id"] == f"parallel-{index}", value
                items.add(value["data"]["item"])
            assert len(items) == 4, "four notes, four items"
            listed = desktop.ask(f"wait-notes {count + 5}", "NOTES ")
            assert all(f"Parallel {index}." in listed for index in range(4)), listed

            # Owner death: the lease goes with the process; its endpoint is left behind,
            # and the next command runs here, on the same store.
            endpoint = desktop.endpoint
            desktop.process.kill()
            desktop.process.wait(timeout=30)
        finally:
            desktop.close()
        if POSIX:
            assert os.path.exists(endpoint), "the dead owner's socket is left behind, stale"
        alone = ok("note", "RC-1", "--body-file", "-", stdin=b"After the owner died.\n")
        assert "After the owner died.\n" in bodies() and alone["cycle"] == "RC-1"

        # Replacement: a new owner replaces the stale endpoint and is reached in turn.
        desktop = Desktop(args.application, data, workspace, 2)
        try:
            assert desktop.ask("resume", "RESUMED") == "RESUMED project"
            total = len(bodies())
            ok("note", "RC-1", "--body-file", "-", stdin=b"To the new owner.\n")
            assert desktop.ask(f"wait-notes {total + 1}", "NOTES ").endswith("To the new owner."), "the new owner served it and its window shows it"
            desktop.quit()
        finally:
            desktop.close()

        # An owner of another protocol version is named as such and sent nothing; an
        # owner with no channel at all is refused, and nothing is opened a second time.
        # Stood in for with a POSIX lock and socket; a Windows owner is a real one only.
        if POSIX:
            with lease_held():
                refused("capability_unavailable", 5, "show", "RC-1")
                if os.path.exists(endpoint):
                    os.unlink(endpoint)
                listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                listener.bind(endpoint)
                os.chmod(endpoint, 0o600)
                listener.listen(1)
                received = []

                def older_owner():
                    connection, _ = listener.accept()
                    with connection:
                        connection.sendall(bytes([ord("B"), ord("P"), ord("R"), ord("T"), 2]) + bytes(59))
                        connection.settimeout(5)
                        try:
                            while chunk := connection.recv(65536):
                                received.append(chunk)
                        except (socket.timeout, ConnectionResetError):
                            pass
                with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
                    serving = pool.submit(older_owner)
                    version = refused("unsupported_version", 2, "note", "RC-1", "--body-file", "-", stdin=b"To an older owner.\n")
                    assert "another command protocol" in version["message"]
                    serving.result(timeout=30)
                listener.close()
                os.unlink(endpoint)
                assert received == [], "no request bytes reached the other version"
        final = bodies()
        assert "To an older owner.\n" not in final and "To the new owner.\n" in final, "nothing landed beside the refusals"
        exported = subprocess.run([args.cycle_application, "cycle", "export", "RC-1", "--round", "F1", "--output", "-", "--profile", "1", "--data-root", str(data)],
                                  capture_output=True, timeout=120, cwd=workspace)
        assert exported.returncode == 0 and b"Ship the cache\xe4\xb8\xad" in exported.stdout, "the frozen round carries the goal the window held"
    print("PROFILE_CYCLE_HANDOFF_OK")


if __name__ == "__main__":
    main()
