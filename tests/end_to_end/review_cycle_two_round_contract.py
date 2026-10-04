"""[[BPR-1166]]. The complete manual two-round workflow, through the real desktop.

The reviewer works in the application's own profile desktop
(`review_cycle_two_round_public_path_tests.cpp`), one gesture at a time; this
course is everyone else. It makes the project, edits it with ordinary file
writes and Git as the person given the feedback would, quits and restarts the
desktop on the same profile, and reads what was shared through the shipped
`bpr cycle` as a reviewer's export and as a granted harness's read.

The course, by the card's words (product names in brackets: the first capture
is R1 in the product, so the card's R0/R1/R2 are R1/R2/R3 here):

1. File > Open Project on a Git checkout with uncommitted work, four files, one
   with CRLF lines and tabs: the first capture [R1].
2. Eight notes across the four files, two of them suggestions (one exact
   replacement, one illustrative), one related location, and a ninth scratch
   note left out of the round.
3. Share notes: Copy freezes F1 and copies it; Save to File and Save as Bundle
   write the same round. The shipped `bpr cycle export` and a granted
   harness's `round read` give the same canonical round.
4. The desktop quits. The recipient edits the folder by hand and sends nothing
   back: no response file, no report, no receipt, no callback.
5. Restart: nothing is captured on reopening. Capture revision [R2]; F1's
   This round view and the exact files compare as the edits were made; a reply
   handed over in prose is pasted; the reviewer verifies what was checked,
   asks for more work, withdraws a discussion note and adds a new note.
6. Next round, Share notes: Save to File freezes F2 and writes it; Copy and the
   bundle give the same bytes; `bpr cycle` reaches the open desktop's store
   (forwarded) for the export and the harness read.
7. The desktop quits; the recipient commits a second set of edits with Git.
8. Restart: Capture revision [R3]; This round for F2, All cycle changes and two
   chosen pairs compare R1/R2/R3 exactly; the remaining notes are verified.

Throughout, no agent component is reachable: the share sheet offers no agent
route, executables named like agent CLIs placed first on PATH are never run,
the desktop starts no child process (sampled where the platform lets the
course list children), and nothing but the course's own edits changes the
folder.
"""
import argparse
import difflib
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import time
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from profile_activation_contract import private_fixture_parent  # noqa: E402


POSIX = os.name != "nt"
AGENT_NAMES = ("claude", "codex", "gemini", "aider", "copilot", "cursor-agent", "ollama", "llm", "openai")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class Desktop:
    """One launch of the reviewer's desktop, driven one gesture at a time."""

    # How long the course waits for one reply, and how much of that the desktop
    # keeps back to answer: every wait inside a gesture ends within the rest, so
    # a stalled gesture replies with what the window shows ([[BPR-1184]]).
    ASK_SECONDS = 180
    REPLY_MARGIN_SECONDS = 30

    def __init__(self, application, data, workspace, index, environment):
        self.signal = workspace / f"desktop-{index}.quit"
        self.log_path = workspace / f"desktop-{index}.log"
        # The child appends through its own descriptor; the course reads through
        # another, so reading never moves the offset the child writes at.
        self.log = self.log_path.open("ab")
        budget = (self.ASK_SECONDS - self.REPLY_MARGIN_SECONDS) * 1000
        environment = dict(environment, BPR_CYCLE_COURSE_GESTURE_BUDGET_MS=str(budget))
        self.process = subprocess.Popen([application, str(data), "1", str(self.signal)], stdout=self.log, stderr=subprocess.STDOUT, env=environment)
        self.seen = 0
        self.next_id = 1
        self.children = set()
        self.sampling = POSIX and shutil.which("pgrep") is not None
        self.stop = threading.Event()
        self.sampler = threading.Thread(target=self.sample_children, daemon=True)
        self.sampler.start()
        self.ready = self.await_line("DESKTOP_READY ", 180)
        assert " active=1" in self.ready, f"the window has the keyboard: {self.ready}"
        print(f"DESKTOP_{index}_READY {self.ready.split(' ', 1)[1]} pid={self.process.pid}")

    def sample_children(self):
        """Every process the desktop starts, by name, as often as the course can look."""
        if not self.sampling:
            return
        while not self.stop.is_set() and self.process.poll() is None:
            listed = subprocess.run(["pgrep", "-P", str(self.process.pid)], capture_output=True, text=True)
            for pid in listed.stdout.split():
                named = subprocess.run(["ps", "-o", "comm=", "-p", pid], capture_output=True, text=True)
                self.children.add(named.stdout.strip() or pid)
            time.sleep(0.02)

    def lines(self):
        """Complete lines only: a line still being written is not read yet."""
        text = self.log_path.read_bytes().decode("utf-8", errors="replace")
        return text.split("\n")[:-1]

    def await_line(self, prefix, timeout=120):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            lines = self.lines()
            for index in range(self.seen, len(lines)):
                if lines[index].startswith(prefix):
                    self.seen = index + 1
                    return lines[index]
            if self.process.poll() is not None:
                raise AssertionError(f"desktop exited {self.process.returncode}: {lines[-40:]}")
            time.sleep(0.02)
        raise AssertionError(f"missing {prefix}: {self.lines()[-40:]}")

    def ask(self, verb, **arguments):
        identity = self.next_id
        self.next_id += 1
        pending = Path(str(self.signal) + ".command.tmp")
        pending.write_text(json.dumps(dict(arguments, id=identity, verb=verb)), encoding="utf-8")
        os.replace(pending, str(self.signal) + ".command")
        line = self.await_line(f"REPLY {identity} ", self.ASK_SECONDS)
        reply = json.loads(line.split(" ", 2)[2])
        assert "error" not in reply, f"{verb} {arguments}: {reply['error']} {json.dumps(reply.get('state'))}"
        return reply

    def quit(self):
        self.signal.touch()
        self.await_line("DESKTOP_RELEASED", 120)
        for line in self.lines():
            if line.startswith("NOTE "):
                print(f"DESKTOP_{self.signal.stem.split('-')[1]}_{line}")
        assert self.process.wait(timeout=60) == 0, "the desktop drains in order"
        self.stop.set()
        self.sampler.join(timeout=10)

    def close(self):
        self.stop.set()
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=10)
        self.log.close()


def span(content, needle, occurrence=0):
    """Byte offsets of `needle` in `content`, for a drag across exactly those bytes."""
    start = -1
    for _ in range(occurrence + 1):
        start = content.index(needle, start + 1)
    return {"start": start, "end": start + len(needle)}


def lines_of(data):
    return [line[:-1] if line.endswith("\r") else line for line in data.decode("utf-8").split("\n")[:-1]] if data else []


def expected_changes(left, right):
    """The removed and added rows the shared engine must show for a pair: (sign, line, text)."""
    old, new = lines_of(left), lines_of(right)
    rows = []
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(a=old, b=new, autojunk=False).get_opcodes():
        if tag in ("replace", "delete"):
            rows += [("-", index + 1, old[index]) for index in range(i1, i2)]
        if tag in ("replace", "insert"):
            rows += [("+", index + 1, new[index]) for index in range(j1, j2)]
    return rows


def shown_changes(comparison):
    return [(sign, left if sign == "-" else right, text) for sign, left, right, text in comparison["rows"] if sign != " "]


def check_comparison(comparison, left_name, right_name, path, left, right):
    assert comparison["path"] == path and comparison["left"] == left_name and comparison["right"] == right_name, comparison
    assert comparison["visible"], f"the comparison pane shows {path}"
    want = expected_changes(left, right)
    have = shown_changes(comparison)
    assert sorted(have) == sorted(want), f"{path} {left_name}->{right_name}: shown {have}, expected {want}"
    assert want, f"{path} changed between {left_name} and {right_name}"
    for sign, _, text in want:
        assert f" {sign} {text}" in comparison["drawn"], f"the pane draws {sign} {text!r}"
    return len(want)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    parser.add_argument("--cycle-application", required=True)
    args = parser.parse_args()
    with private_fixture_parent() as parent, contained_temporary_directory(prefix="two-round-", parent=parent) as temporary:
        workspace = Path(temporary).resolve()
        make_root_private(workspace)
        data = workspace / "data"
        make_root_private(data)
        handoff = workspace / "handoff"
        handoff.mkdir()

        # Executables named like agent CLIs, first on PATH: running one leaves a mark.
        canaries = workspace / "agent-canaries"
        canaries.mkdir()
        marks = workspace / "agent-ran"
        for name in AGENT_NAMES:
            if POSIX:
                script = canaries / name
                script.write_text(f"#!/bin/sh\necho {name} >> '{marks}'\nexit 3\n", encoding="utf-8")
                script.chmod(0o755)
            else:
                (canaries / f"{name}.cmd").write_text(f"@echo {name}>> \"{marks}\"\r\n@exit /b 3\r\n", encoding="utf-8")
        environment = dict(os.environ, PATH=str(canaries) + os.pathsep + os.environ.get("PATH", ""))
        if environment.get("QT_QPA_PLATFORM") == "offscreen":
            # A desktop-sized screen: offscreen's own 800-pixel screen holds the
            # restored window at its 900-pixel minimum width.
            screen = workspace / "offscreen-screen.json"
            screen.write_text(json.dumps({"screens": [{"name": "Course", "x": 0, "y": 0, "width": 1920, "height": 1200, "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1}]}), encoding="utf-8")
            # Qt splits the platform argument at every ':', so a Windows drive
            # letter would end the path at "C". The desktop inherits the course's
            # working directory, so a relative path names the same file.
            spec = os.path.relpath(screen) if ":" in str(screen) else str(screen)
            environment["QT_QPA_PLATFORM"] = f"offscreen:configfile={spec}"

        # The project: a Git checkout with work not yet committed, four files, one CRLF with tabs.
        project = workspace / "config-parser"
        (project / "src").mkdir(parents=True)
        (project / "include").mkdir()
        files = {
            "src/parser.cpp": b"// Parses one config value.\r\n#include \"parser.hpp\"\r\n\r\nint parse_value(const char* aText)\r\n{\r\n\tif (aText == nullptr)\r\n\t\treturn -1;\r\n\treturn check(aText);\r\n}\r\n",
            "src/lexer.cpp": b"#include \"parser.hpp\"\n\nint next_token(const char* aCursor)\n{\n\tint count = 0;\n\twhile (*aCursor != '\\0')\n\t\t++count, ++aCursor;\n\treturn count;\n}\n",
            "include/parser.hpp": b"#pragma once\n\nint parse_value(const char* aText);\nint next_token(const char* aCursor);\n",
            "README.md": b"# Config parser\n\nReads one value per line.\n",
        }

        def write(path, content):
            (project / path).parent.mkdir(parents=True, exist_ok=True)
            (project / path).write_bytes(content)

        def git(*arguments):
            subprocess.run(["git", "-c", "core.autocrlf=false", "-c", "user.name=Dana", "-c", "user.email=dana@example.invalid", "-c", "commit.gpgsign=false", *arguments],
                           cwd=project, check=True, capture_output=True, env=dict(os.environ, GIT_CONFIG_NOSYSTEM="1", HOME=str(workspace)))

        for path, content in files.items():
            write(path, content)
        git("init", "-q", "-b", "main")
        git("add", "-A")
        git("commit", "-q", "-m", "Start the parser")
        files["README.md"] += b"Build with make.\n"  # uncommitted work before the review
        write("README.md", files["README.md"])

        def snapshot():
            return {path.relative_to(project).as_posix(): path.read_bytes() for path in sorted(project.rglob("*")) if path.is_file() and ".git" not in path.relative_to(project).parts}

        def hashes(state):
            return {path: sha256(content)[:16] for path, content in state.items()}

        revisions = {}

        def bpr(*arguments, stdin=b"", consumer=False, cwd=None, json_output=True):
            command = [args.cycle_application, "cycle", *arguments, *(["--json"] if json_output else []), "--profile", "1", "--data-root", str(data)]
            child = dict(environment, BPR_CYCLE_ROLE="consumer") if consumer else environment
            return subprocess.run(command, input=stdin, capture_output=True, timeout=120, cwd=cwd or workspace, env=child)

        def ok(*arguments, **options):
            result = bpr(*arguments, **options)
            assert result.stderr == b"", f"{arguments}: JSON calls keep standard error empty: {result.stderr!r}"
            value = json.loads(result.stdout)
            assert result.returncode == 0 and value["outcome"] == "ok", f"{arguments}: {result.returncode} {value}"
            return value["data"]

        def exported(round_name):
            result = bpr("export", "RC-1", "--round", round_name, "--output", "-", json_output=False)
            assert result.returncode == 0, f"export {round_name}: {result.returncode} {result.stderr!r}"
            return result.stdout

        def harness_read(round_name, cwd=None):
            """A harness as the bpr-cycle skill teaches: capabilities, then every page before acting."""
            listed = subprocess.run([args.cycle_application, "cycle", "capabilities", "--json"], capture_output=True, timeout=60, cwd=cwd or workspace, env=dict(environment, BPR_CYCLE_ROLE="consumer"))
            assert listed.returncode == 0 and listed.stderr == b"", listed
            capabilities = json.loads(listed.stdout)["data"]
            assert str(capabilities["cli_contract"]) == "1" and "round read" in capabilities["consumer_commands"], capabilities
            first = ok("round", "read", "RC-1", "--round", round_name, "--page-bytes", "4096", consumer=True, cwd=cwd)
            items = list(first["items"])
            page = first
            while page["next"] is not None:
                page = ok("round", "page", "RC-1", "--round", round_name, "--cursor", page["next"], consumer=True, cwd=cwd)
                assert page["payload_sha256"] == first["payload_sha256"] and int(page["first_item"]) == len(items), page
                items += page["items"]
            assert len(items) == int(first["items_total"]), "every item, once"
            return first, items

        def check_channels(round_name, clipboard, saved, bundle_path, read, items, notes):
            """RC56-03: one canonical round, whatever the channel."""
            assert clipboard == saved, f"{round_name}: the clipboard holds exactly the saved file's bytes"
            with zipfile.ZipFile(bundle_path) as bundle:
                assert sorted(bundle.namelist()) == ["manifest.json", "review.md", "round.json"], bundle.namelist()
                assert bundle.read("review.md") == saved, f"{round_name}: the bundle's document is the same bytes"
                payload = bundle.read("round.json")
            cli = exported(round_name)
            assert cli == saved, f"{round_name}: `bpr cycle export` writes the same bytes"
            digest = next(entry["digest"] for entry in ok("show", "RC-1")["rounds"] if entry["round"] == round_name)
            assert read["payload_sha256"] == digest, "the harness reads the round the reviewer froze"
            envelope = json.loads(payload)
            canonical_bytes = json.dumps(envelope["payload"], sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
            assert envelope["payload_sha256"] == digest and sha256(canonical_bytes) == digest, f"{round_name}: the bundle carries the canonical payload ({sha256(canonical_bytes)} vs {digest})"
            text = saved.decode("utf-8")
            assert digest in text, f"{round_name}: the document names its digest"
            for item in items:
                assert item["id"] in text, f"{round_name}: the document carries {item['id']}"
            canonical_items = envelope["payload"]["items"]
            assert [entry["id"] for entry in canonical_items] == [item["id"] for item in items], "the harness reads the canonical items in order"
            for item in items:
                note = notes[item["id"]]
                first_line = note["body"].split("\n")[0]
                assert first_line in text, f"{round_name}: {item['id']}'s words are in the document"
                assert first_line in json.dumps(item, ensure_ascii=False), f"{round_name}: and in what the harness reads"
            return digest

        renders = os.environ.get("BPR_CYCLE_TWO_ROUND_RENDER_DIR")

        def render(name):
            """The window as the reviewer sees it at this step, when the run keeps renders."""
            if renders:
                Path(renders).mkdir(parents=True, exist_ok=True)
                shown = desktop.ask("render", path=str(Path(renders) / f"{name}.png"))
                print("RENDER", name, f"{shown['width']}x{shown['height']}")

        desktop = None
        try:
            # ------------------------------------------------------------ launch 1
            desktop = Desktop(args.application, data, workspace, 1, environment)
            opened = desktop.ask("open-project", path=str(project))
            assert opened["cycle"] == "RC-1" and opened["revisions"] == ["R1"], opened
            revisions["R1"] = snapshot()
            print("SOURCE R1", json.dumps(hashes(revisions["R1"]), sort_keys=True))

            parser_r1 = revisions["R1"]["src/parser.cpp"]
            lexer_r1 = revisions["R1"]["src/lexer.cpp"]
            header_r1 = revisions["R1"]["include/parser.hpp"]
            readme_r1 = revisions["R1"]["README.md"]
            plan = [
                ("note", "src/parser.cpp", parser_r1, b"check(aText)", "Rename check to validate; check says nothing about what it checks."),
                ("suggest", "src/parser.cpp", parser_r1, b"return -1;", ("return kNoValue;", False, "Name the sentinel.")),
                ("note", "src/parser.cpp", parser_r1, b"// Parses one config value.", "Say what a missing value returns."),
                ("note", "src/lexer.cpp", lexer_r1, b"++count, ++aCursor;", "Split the comma expression into two statements."),
                ("suggest", "src/lexer.cpp", lexer_r1, b"int count = 0;", ("unsigned count = 0;", True, "Something unsigned, as an idea.")),
                ("note", "include/parser.hpp", header_r1, b"int next_token(const char* aCursor);", "Declare next_token in a lexer header."),
                ("note", "include/parser.hpp", header_r1, b"#pragma once", "Keep pragma once; just checking we agree."),
                ("note", "README.md", readme_r1, b"Reads one value per line.", "Document the return values."),
                ("note", "README.md", readme_r1, b"# Config parser", "Scratch: ask about the file format later."),
            ]
            notes_by_item = {}
            for kind, path, content, needle, words in plan:
                if kind == "note":
                    reply = desktop.ask("note", path=path, body=words, **span(content, needle))
                else:
                    replacement, illustrative, rationale = words
                    reply = desktop.ask("suggest", path=path, replacement=replacement, illustrative=illustrative, rationale=rationale, **span(content, needle))
                assert reply["source"] == f"{path} @ R1", reply
            # One discussion across two files: lexer.cpp's definition joins the header note.
            related = desktop.ask("relate", item="I6", path="src/lexer.cpp", **span(lexer_r1, b"next_token"))
            notes_by_item = {note["item"]: note for note in related["notes"]}
            render("01-r1-nine-notes")
            assert sorted(notes_by_item) == [f"I{index}" for index in range(1, 10)], notes_by_item
            assert notes_by_item["I2"]["suggestions"] == 1 and notes_by_item["I5"]["suggestions"] == 1, "two notes carry a suggestion"
            assert notes_by_item["I6"]["related"] == 1 and notes_by_item["I2"]["body"] == "Name the sentinel."
            assert related["revisions"] == ["R1"] and snapshot() == revisions["R1"], "noting read the folder only"

            share = desktop.ask("compose")
            assert share["round"] == "F1" and not share["frozen"], share
            assert [row["item"] for row in share["rows"]] == [f"I{index}" for index in range(1, 10)] and all(row["included"] for row in share["rows"]), share["rows"]
            assert share["routes"] == {"copy": True, "save": True, "bundle": True, "external_agent": False, "send_to_agent": False}, f"Copy and Save, no agent route: {share['routes']}"
            share = desktop.ask("include", item="I9", on=False)
            assert "Scratch" not in share["document"], "the scratch note is left out of the round"
            share = desktop.ask("goal", text="Tidy the parser before review")
            preview = share["document"].encode("utf-8")
            copied = desktop.ask("share", route="copy", evidence=str(workspace / "clipboard-F1.md"))
            assert copied["frozen"] and copied["round"] == "F1" and copied["status"].startswith("Copied F1 to the clipboard"), copied
            clipboard_f1 = (workspace / "clipboard-F1.md").read_bytes()
            assert clipboard_f1 == preview, "Copy froze and copied exactly the preview"
            saved = desktop.ask("share", route="save", path=str(handoff / "RC-1-F1.md"))
            bundled = desktop.ask("share", route="bundle", path=str(handoff / "RC-1-F1.bpr-review.zip"))
            assert len(bundled["deliveries"]) == 3 and bundled["deliveries"][0].startswith("F1 · Copied to the clipboard") and bundled["deliveries"][1].startswith("F1 · Saved as RC-1-F1.md"), bundled["deliveries"]
            assert all(not entry.lower().startswith("f1 · received") for entry in bundled["deliveries"]), "no observation claims more than happened"
            assert saved["status"] and bundled["routes"]["external_agent"] is False
            render("02-f1-shared")
            closed = desktop.ask("close-share")
            assert closed["rounds"] == [{"round": "F1", "start": "R1"}] and closed["revisions"] == ["R1"], closed
            assert snapshot() == revisions["R1"], "sharing wrote nothing into the folder"
            desktop.quit()
            print("DESKTOP_1_CHILDREN", sorted(desktop.children) if desktop.sampling else "not sampled on this platform")
            assert not desktop.children, f"the desktop started no process: {desktop.children}"
            desktop.close()
            desktop = None

            # ------------------------------------------- the channels, desktop closed
            shown = ok("show", "RC-1")
            assert len(shown["notes"]) == 9 and [entry["round"] for entry in shown["rounds"]] == ["F1"], "nothing was lost when the desktop quit"
            refused = bpr("round", "read", "RC-1", "--round", "F1", consumer=True)
            assert refused.returncode == 5 and json.loads(refused.stdout)["error"]["code"] == "policy_denied", "naming a round is not permission"
            ok("policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", "allowed")
            ok("round", "grant", "RC-1", "--round", "F1")
            read_f1, items_f1 = harness_read("F1", cwd=project)
            assert [item["id"] for item in items_f1] == [f"I{index}" for index in range(1, 9)], "eight items, the scratch note not among them"
            digest_f1 = check_channels("F1", clipboard_f1, (handoff / "RC-1-F1.md").read_bytes(), handoff / "RC-1-F1.bpr-review.zip", read_f1, items_f1, notes_by_item)
            assert int(read_f1["suggestions_total"]) == 2, read_f1
            for channel in (clipboard_f1, (handoff / "RC-1-F1.md").read_bytes(), exported("F1")):
                assert b"Scratch" not in channel and str(project).encode() not in channel and str(data).encode() not in channel, "no scratch note and no local path in any channel"
            print("F1", digest_f1, "bytes", len(clipboard_f1), "items", len(items_f1))

            # ------------------------------- the recipient edits by hand, sends nothing
            files["src/parser.cpp"] = (b"// Parses one config value; returns kNoValue when it is missing.\r\n#include \"parser.hpp\"\r\nconstexpr int kNoValue = -1;\r\n\r\n"
                                       b"int parse_value(const char* aText)\r\n{\r\n\tif (aText == nullptr)\r\n\t\treturn kNoValue;\r\n\treturn validate(aText);\r\n}\r\n")
            files["src/lexer.cpp"] = files["src/lexer.cpp"].replace(b"\t\t++count, ++aCursor;\n", b"\t{\n\t\t++count;\n\t\t++aCursor;\n\t}\n")
            files["README.md"] += b"A missing value returns kNoValue.\n"
            for path in ("src/parser.cpp", "src/lexer.cpp", "README.md"):
                write(path, files[path])
            edited_r2 = snapshot()

            # ------------------------------------------------------------ launch 2
            desktop = Desktop(args.application, data, workspace, 2, environment)
            resumed = desktop.ask("resume", title="config-parser", notes=9)
            assert resumed["revisions"] == ["R1"] and resumed["rounds"] == [{"round": "F1", "start": "R1"}], f"nothing captured on reopening: {resumed}"
            assert [note["item"] for note in resumed["notes"]] == [f"I{index}" for index in range(1, 10)], f"every note after the restart: {resumed['notes']}"
            assert all(note["body"] == notes_by_item[note["item"]]["body"] for note in resumed["notes"]), "each note's words unchanged"
            captured = desktop.ask("capture")
            assert captured["revisions"] == ["R1", "R2"], captured
            revisions["R2"] = snapshot()
            assert revisions["R2"] == edited_r2
            print("SOURCE R2", json.dumps(hashes(revisions["R2"]), sort_keys=True))
            rows = 0
            for path in ("src/parser.cpp", "src/lexer.cpp", "README.md"):
                compared = desktop.ask("view", path=path, menu=["This round", "F1 · since R1"])
                rows += check_comparison(compared, "R1", "R2", path, revisions["R1"][path], revisions["R2"][path])
                assert compared["label"].startswith("This round"), compared["label"]
            unchanged = desktop.ask("view", path="include/parser.hpp", menu=["This round", "F1 · since R1"])
            assert unchanged["rows"] == [] or all(row[0] == " " for row in unchanged["rows"]), "an unchanged file shows no change"
            print("COMPARE R1->R2 changed rows", rows)
            render("03-r2-this-round-f1")

            # A reply handed over in prose is pasted by hand, beside its note, as a claim.
            replied = desktop.ask("reply", item="I1", entry="Paste a reply to F1…", **{"from": "Dana (chat)"}, text="Renamed check to validate and named the sentinel.")
            assert replied["responses"] == 1 and "Dana (chat)" in replied["notes"][0]["shown"], replied["notes"][0]
            assert replied["revisions"] == ["R1", "R2"], "a reply captures nothing"

            # Verification is the reviewer's: what was checked in R2, and nothing else.
            desktop.ask("view", path="src/parser.cpp", menu=["This round", "F1 · since R1"])
            for item in ("I1", "I2", "I3"):
                decided = desktop.ask("decide", item=item, choice="Verified in")
                assert decided["offered"] == "Verified in R2", decided["offered"]
            desktop.ask("view", path="src/lexer.cpp", menu=["This round", "F1 · since R1"])
            desktop.ask("decide", item="I4", choice="Verified in")
            desktop.ask("view", path="README.md", menu=["This round", "F1 · since R1"])
            desktop.ask("decide", item="I8", choice="Verified in")
            desktop.ask("decide", item="I6", choice="Needs work")
            decided = desktop.ask("decide", item="I7", choice="Withdraw")
            shown_notes = {note["item"]: note["shown"] for note in decided["notes"]}
            for item in ("I1", "I2", "I3", "I4", "I8"):
                assert "Verified by you in R2" in shown_notes[item], f"{item}: {shown_notes[item]}"
            assert "Verified" not in shown_notes["I5"] and "Verified" not in shown_notes["I6"], "only what was checked is verified"
            render("04-r2-verified")

            # A new note on the captured R2 bytes, then the next round from what is still open.
            latest = desktop.ask("latest", path="include/parser.hpp")
            assert latest["source"] == "include/parser.hpp @ R2", latest
            renoted = desktop.ask("note", path="include/parser.hpp", body="Declare validate here too.", **span(revisions["R2"]["include/parser.hpp"], b"int parse_value(const char* aText);"))
            assert renoted["added"] == "I10", renoted
            nexted = desktop.ask("next-round")
            print("NEXT ROUND", json.dumps({key: nexted[key] for key in ("carried", "added", "resolved", "archived")}))
            counts = {key: nexted[key] for key in ("carried", "added", "resolved", "archived")}
            assert counts == {"carried": 2, "added": 2, "resolved": 6, "archived": 0}, f"I5 and I6 carried; I9 and I10 added; five verified and one withdrawn resolved: {counts}"
            share = desktop.ask("compose")
            assert share["round"] == "F2" and not share["frozen"], share
            listed = {row["item"]: row["included"] for row in share["rows"]}
            assert not any(listed.get(item) for item in ("I1", "I2", "I3", "I4", "I7", "I8")), f"verified and withdrawn notes are listed unchecked, not asked again: {listed}"
            if listed.get("I9"):
                share = desktop.ask("include", item="I9", on=False)
            listed = {row["item"]: row["included"] for row in share["rows"]}
            assert listed.get("I5") and listed.get("I6") and listed.get("I10") and not listed.get("I9"), listed
            assert share["routes"]["external_agent"] is False and share["routes"]["send_to_agent"] is False
            saved = desktop.ask("share", route="save", path=str(handoff / "RC-1-F2.md"))
            of_f2 = [entry for entry in saved["deliveries"] if entry.startswith("F2 · ")]
            assert saved["frozen"] and saved["round"] == "F2" and len(of_f2) == 1 and of_f2[0].startswith("F2 · Saved as RC-1-F2.md"), saved["deliveries"]
            copied = desktop.ask("share", route="copy", evidence=str(workspace / "clipboard-F2.md"))
            desktop.ask("share", route="bundle", path=str(handoff / "RC-1-F2.bpr-review.zip"))
            clipboard_f2 = (workspace / "clipboard-F2.md").read_bytes()
            of_f2 = [entry for entry in copied["deliveries"] if entry.startswith("F2 · ")]
            assert len(of_f2) == 2 and of_f2[1].startswith("F2 · Copied to the clipboard"), copied["deliveries"]

            # `bpr cycle` while the desktop owns the profile: forwarded to its store.
            ok("round", "grant", "RC-1", "--round", "F2")
            read_f2, items_f2 = harness_read("F2")
            assert [item["id"] for item in items_f2] == ["I5", "I6", "I10"], [item["id"] for item in items_f2]
            notes_by_item.update({note["item"]: note for note in renoted["notes"]})
            digest_f2 = check_channels("F2", clipboard_f2, (handoff / "RC-1-F2.md").read_bytes(), handoff / "RC-1-F2.bpr-review.zip", read_f2, items_f2, notes_by_item)
            assert digest_f2 != digest_f1
            print("F2", digest_f2, "bytes", len(clipboard_f2), "items", len(items_f2))
            render("05-f2-shared")
            desktop.ask("close-share")
            assert snapshot() == revisions["R2"], "the second launch wrote nothing into the folder"
            desktop.quit()
            print("DESKTOP_2_CHILDREN", sorted(desktop.children) if desktop.sampling else "not sampled on this platform")
            assert not desktop.children, f"the desktop started no process: {desktop.children}"
            desktop.close()
            desktop = None

            # ------------------------------------- the recipient commits a second set
            files["include/parser.hpp"] = b"#pragma once\n\nint parse_value(const char* aText);\nint validate(const char* aText);\n"
            files["include/lexer.hpp"] = b"#pragma once\n\nint next_token(const char* aCursor);\n"
            files["src/lexer.cpp"] = files["src/lexer.cpp"].replace(b"#include \"parser.hpp\"", b"#include \"lexer.hpp\"").replace(b"\tint count = 0;", b"\tunsigned count = 0;")
            for path in ("include/parser.hpp", "include/lexer.hpp", "src/lexer.cpp"):
                write(path, files[path])
            git("add", "-A")
            git("commit", "-q", "-m", "Second round")
            edited_r3 = snapshot()

            # ------------------------------------------------------------ launch 3
            desktop = Desktop(args.application, data, workspace, 3, environment)
            resumed = desktop.ask("resume", title="config-parser", notes=10)
            assert resumed["revisions"] == ["R1", "R2"] and [entry["round"] for entry in resumed["rounds"]] == ["F1", "F2"], resumed
            assert len(resumed["notes"]) == 10, "ten notes after the second restart"
            shown_notes = {note["item"]: note["shown"] for note in resumed["notes"]}
            assert all("Verified by you in R2" in shown_notes[item] for item in ("I1", "I2", "I3", "I4", "I8")), "verifications kept"
            captured = desktop.ask("capture")
            assert captured["revisions"] == ["R1", "R2", "R3"], captured
            revisions["R3"] = snapshot()
            assert revisions["R3"] == edited_r3
            print("SOURCE R3", json.dumps(hashes(revisions["R3"]), sort_keys=True))
            rows = check_comparison(desktop.ask("view", path="include/parser.hpp", menu=["This round", "F2 · since R2"]), "R2", "R3", "include/parser.hpp", revisions["R2"]["include/parser.hpp"], revisions["R3"]["include/parser.hpp"])
            rows += check_comparison(desktop.ask("view", path="src/lexer.cpp", menu=["This round", "F2 · since R2"]), "R2", "R3", "src/lexer.cpp", revisions["R2"]["src/lexer.cpp"], revisions["R3"]["src/lexer.cpp"])
            whole = desktop.ask("view", path="src/parser.cpp", menu=["All cycle changes"])
            rows += check_comparison(whole, "R1", "R3", "src/parser.cpp", revisions["R1"]["src/parser.cpp"], revisions["R3"]["src/parser.cpp"])
            assert whole["label"] == "All cycle changes · R1 → R3", whole["label"]
            first = desktop.ask("view", path="src/lexer.cpp", menu=["Choose revisions", "From R1", "to R2"])
            rows += check_comparison(first, "R1", "R2", "src/lexer.cpp", revisions["R1"]["src/lexer.cpp"], revisions["R2"]["src/lexer.cpp"])
            assert first["pinned"], "a chosen pair is pinned"
            second = desktop.ask("view", path="src/lexer.cpp", menu=["Choose revisions", "From R2", "to R3"])
            rows += check_comparison(second, "R2", "R3", "src/lexer.cpp", revisions["R2"]["src/lexer.cpp"], revisions["R3"]["src/lexer.cpp"])
            added = desktop.ask("view", path="include/lexer.hpp", menu=["This round", "F2 · since R2"])
            assert added["left"] == "R2" and added["right"] == "R3" and shown_changes(added) == [("+", index + 1, line) for index, line in enumerate(lines_of(revisions["R3"]["include/lexer.hpp"]))], added
            print("COMPARE R2->R3, R1->R3 and chosen pairs changed rows", rows)
            render("06-r3-added-file")
            desktop.ask("view", path="src/lexer.cpp", menu=["This round", "F2 · since R2"])
            for item in ("I5", "I6"):
                decided = desktop.ask("decide", item=item, choice="Verified in")
                assert decided["offered"] == "Verified in R3", decided["offered"]
            desktop.ask("view", path="include/parser.hpp", menu=["This round", "F2 · since R2"])
            final = desktop.ask("decide", item="I10", choice="Verified in")
            shown_notes = {note["item"]: note["shown"] for note in final["notes"]}
            for item in ("I5", "I6", "I10"):
                assert "Verified by you in R3" in shown_notes[item], f"{item}: {shown_notes[item]}"
            assert "Verified" not in shown_notes["I9"], "the scratch note was never shared or verified"
            render("07-r3-verified")
            assert snapshot() == revisions["R3"], "the third launch wrote nothing into the folder"
            desktop.quit()
            print("DESKTOP_3_CHILDREN", sorted(desktop.children) if desktop.sampling else "not sampled on this platform")
            assert not desktop.children, f"the desktop started no process: {desktop.children}"
            desktop.close()
            desktop = None
        finally:
            if desktop is not None:
                print("\n".join(desktop.lines()[-60:]))
                desktop.close()

        history = ok("show", "RC-1")
        assert [entry["round"] for entry in history["rounds"]] == ["F1", "F2"] and len(history["notes"]) == 10, history["rounds"]
        assert len(history["responses"]) == 1 and history["responses"][0]["channel"] == "manual_entry", "the one reply was entered by hand; no report was needed"
        assert not marks.exists(), f"an agent executable ran: {marks.read_text() if marks.exists() else ''}"
        print("PATH_CANARIES not run:", ", ".join(AGENT_NAMES))
    print("REVIEW_CYCLE_TWO_ROUND_OK")


if __name__ == "__main__":
    main()
