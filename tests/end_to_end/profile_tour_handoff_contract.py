"""[[BPR-1062]]. The shipped CLI and a real desktop author work one live draft, in both directions."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from profile_activation_contract import private_fixture_parent  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    parser.add_argument("--tour-application", required=True)
    args = parser.parse_args()
    with private_fixture_parent() as parent, contained_temporary_directory(prefix="tour-", parent=parent) as temporary:
        root = Path(temporary).resolve()
        make_root_private(root)
        log_path = root / "desktop.log"
        signal = root / "desktop.quit"
        log = log_path.open("w+", encoding="utf-8")
        desktop = subprocess.Popen([args.application, str(root), "1", str(signal)], stdout=log, stderr=subprocess.STDOUT, text=True)
        seen = 0

        def output():
            log.seek(0)
            return log.read().splitlines()

        def await_line(prefix, timeout=40):
            nonlocal seen
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                lines = output()
                for index in range(seen, len(lines)):
                    if lines[index].startswith(prefix):
                        seen = index + 1
                        return lines[index]
                if desktop.poll() is not None:
                    raise AssertionError(f"desktop exited {desktop.returncode}: {lines}")
                time.sleep(0.02)
            raise AssertionError(f"missing {prefix}: {output()}")

        def ask(command, prefix):
            pending = root / "desktop.quit.command.tmp"
            pending.write_text(command, encoding="utf-8")
            os.replace(pending, root / "desktop.quit.command")
            return await_line(prefix)

        try:
            token = await_line("TOUR_LIVE ").split()[1]
            await_line("OWNER_READY")

            def bpr(*arguments, data=None):
                return subprocess.run([args.tour_application, "tour", *arguments, "--profile", "1", "--draft", token,
                                       "--json", "--data-root", str(root)], input=data, capture_output=True, timeout=60)

            def data(result):
                return json.loads(result.stdout)["data"]

            def code(result):
                return json.loads(result.stdout)["error"]["code"]

            def revision():
                return data(bpr("status"))["revision"]

            def edit(name, text, expected, request_id):
                return bpr("edit", name, "--expected-revision", expected, "--request-id", request_id, "--file", "-", data=text.encode())

            first = bpr("status")
            assert first.returncode == 0 and data(first)["draft_id"] == token, first.stdout
            start = data(first)["revision"]
            # What the desktop author typed is what the CLI reads: nothing waits in a buffer.
            ask("type Hello", "TYPED")
            assert data(bpr("show", "problem"))["text"] == "Hello"
            assert int(revision()) > int(start)
            # A CLI edit reaches the open editor without moving the author's caret or taking focus.
            ask("caret 2", "CARET_SET")
            before = revision()
            written = edit("problem", "Hello world", before, "cli-1")
            assert written.returncode == 0, written.stdout
            assert ask("problem", "PROBLEM ") == "PROBLEM Hello world"
            state = ask("state", "STATE ")
            assert "caret=2 anchor=2 focus=problem" in state, state
            # An input method holding preedit text is neither flushed nor read past.
            assert ask("compose 中", "COMPOSING") == "COMPOSING"
            busy = bpr("status")
            assert busy.returncode == 4 and code(busy) == "editor_busy", busy.stdout
            refused = edit("problem", "must not land", before, "cli-busy")
            assert refused.returncode == 4 and code(refused) == "editor_busy", refused.stdout
            assert ask("problem", "PROBLEM ") == "PROBLEM Hello world"
            assert ask("commit 中", "COMMITTED") == "COMMITTED"
            settled = bpr("show", "problem")
            assert settled.returncode == 0 and "中" in data(settled)["text"], settled.stdout
            committed = data(settled)["text"]
            # A stale CLI edit after the author kept typing keeps both texts and reports a conflict.
            stale = revision()
            ask("type !", "TYPED")
            conflict = edit("problem", "stale candidate", stale, "cli-stale")
            assert conflict.returncode == 4 and code(conflict) == "revision_conflict", conflict.stdout
            assert ask("problem", "PROBLEM ") == "PROBLEM " + committed + "!"
            # Independent fields both proceed, and a duplicate projection does not apply twice.
            current = revision()
            solved = edit("solution", "Retain a snapshot.", current, "cli-2")
            assert solved.returncode == 0, solved.stdout
            landed = data(solved)["revision"]
            assert ask("solution", "SOLUTION ") == "SOLUTION Retain a snapshot."
            assert ask("problem", "PROBLEM ") == "PROBLEM " + committed + "!"
            duplicate = edit("solution", "Retain a snapshot.", current, "cli-2")
            assert duplicate.returncode == 0 and data(duplicate)["revision"] == landed, duplicate.stdout
            assert revision() == landed
            # One shared history: the CLI undoes the latest committed transaction, whoever made it.
            undone = bpr("undo", "--expected-revision", landed, "--request-id", "cli-undo")
            assert undone.returncode == 0 and data(undone)["transaction_label"], undone.stdout
            assert ask("solution", "SOLUTION") == "SOLUTION "
            assert ask("problem", "PROBLEM ") == "PROBLEM " + committed + "!"
            # Structure follows stable stop IDs across presentations: another presentation's reorder,
            # removal and undo change the route but not the author's selected stop or any ID.
            ids = ask("stops 3", "STOPS ").split()[1].split(",")
            assert len(ids) == 3, ids
            assert ask("outline", "OUTLINE ") == f"OUTLINE {','.join(ids)} selected={ids[0]}"
            moved = bpr("move", ids[1], "--before", ids[0], "--expected-revision", revision(), "--request-id", "cli-move")
            assert moved.returncode == 0, moved.stdout
            assert [stop["stop_id"] for stop in data(moved)["stops"]] == [ids[1], ids[0], ids[2]]
            assert ask("outline", "OUTLINE ") == f"OUTLINE {ids[1]},{ids[0]},{ids[2]} selected={ids[0]}"
            removed = bpr("remove", ids[2], "--expected-revision", data(moved)["revision"], "--request-id", "cli-remove")
            assert removed.returncode == 0, removed.stdout
            assert ask("outline", "OUTLINE ") == f"OUTLINE {ids[1]},{ids[0]} selected={ids[0]}"
            restored = bpr("undo", "--expected-revision", data(removed)["revision"], "--request-id", "cli-undo-remove")
            assert restored.returncode == 0, restored.stdout
            assert [stop["stop_id"] for stop in data(restored)["stops"]] == [ids[1], ids[0], ids[2]]
            after_undo = ask("outline", "OUTLINE ")
            assert after_undo.startswith(f"OUTLINE {ids[1]},{ids[0]},{ids[2]} "), after_undo
            # The active agent's request is served where the author is: context and answer both go
            # through the open window's draft, and never past uncommitted input-method text.
            def complete(ticket, replacement):
                answer = json.dumps({"schema": "bpr.review-preparation.result.v1", "kind": "rewrite", "status": "ok",
                                     "replacement_text": replacement, "notes": []})
                return bpr("assist", "complete", "--ticket", ticket, "--result", "-", data=answer.encode())
            hooks = subprocess.run([args.tour_application, "tour", "hooks", "show", "--profile", "1", "--json",
                                    "--data-root", str(root)], capture_output=True, timeout=60)
            assert hooks.returncode == 0 and len(json.loads(hooks.stdout)["data"]["hooks"]) == 4, hooks.stdout
            assert ask("compose \u4e2d", "COMPOSING") == "COMPOSING"
            busy = bpr("assist", "context", "--hook", "problem", "--field", "problem", "--quote", "world")
            assert busy.returncode == 4 and code(busy) == "editor_busy", busy.stdout
            assert ask("commit \u4e2d", "COMMITTED") == "COMMITTED"
            before_text = data(bpr("show", "problem"))["text"]
            assert before_text.count("world") == 1, before_text
            ask("caret 2", "CARET_SET")
            issued = bpr("assist", "context", "--hook", "problem", "--field", "problem", "--quote", "world")
            assert issued.returncode == 0, issued.stdout
            packet = data(issued)["assist"]
            assert packet["selection"]["text"] == "world" and packet["hook"]["name"] == "problem", packet
            applied = complete(packet["ticket"], "there")
            assert applied.returncode == 0 and data(applied)["assist"]["applied"] is True, applied.stdout
            expected_text = before_text.replace("world", "there")
            assert ask("problem", "PROBLEM ") == "PROBLEM " + expected_text
            state = ask("state", "STATE ")
            assert "focus=problem" in state and "saved=yes" in state, state
            # The author keeps typing while a request is outstanding: the answer is stale and both texts stay.
            outstanding = data(bpr("assist", "context", "--hook", "problem", "--field", "problem"))["assist"]["ticket"]
            ask("type ?", "TYPED")
            stale_answer = complete(outstanding, "late")
            assert stale_answer.returncode == 4 and code(stale_answer) == "stale", stale_answer.stdout
            assert ask("problem", "PROBLEM ") == "PROBLEM " + expected_text + "?"
            # A source read is not something a running desktop serves yet.
            sourced = bpr("assist", "context", "--hook", "outline", "--sources", "src/a.cpp:1-3")
            assert sourced.returncode == 3 and code(sourced) == "interaction_required", sourced.stdout
            committed = expected_text
            final = data(bpr("show", "problem"))["text"]
            assert final == committed + "?", final
            saved = ask("state", "STATE ")
            assert "saved=yes" in saved, saved
            signal.touch()
            await_line("OWNER_RELEASED", 60)
            assert desktop.wait(timeout=60) == 0
            # After the desktop drained, the next command runs standalone and reads what it committed.
            standalone = bpr("show", "problem")
            assert standalone.returncode == 0 and data(standalone)["text"] == final, standalone.stdout
        finally:
            if desktop.poll() is None:
                desktop.kill()
                desktop.wait(timeout=10)
            log.close()
    print("PROFILE_TOUR_HANDOFF_OK")


if __name__ == "__main__":
    main()
