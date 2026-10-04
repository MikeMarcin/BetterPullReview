"""[[BPR-1171]]. One Review Cycle through the desktop, the human terminal, a harness and a manual path, and back.

The reviewer works in the application's own profile desktop
(`review_cycle_two_round_public_path_tests.cpp`, the same reviewer the
two-round course drives), one gesture at a time. Everyone else is this course:
the person at a terminal running the shipped `bpr cycle` without `--json`, a
harness that declares itself (`BPR_CYCLE_ROLE=consumer`) and does what the
bpr-cycle skill teaches, and a recipient who cannot reach `bpr` at all and works
from the round's Markdown.

1. Before anything exists the terminal says so in words: no cycles, no cycle for
   this folder, no note without text. Nothing is an empty success.
2. Desktop: File > Open Project [R1], three notes (one an exact suggestion).
   Terminal, while the desktop owns the profile: a note, which the window shows.
   The window writes a note and leaves it unsaved, with the keyboard in it,
   while the terminal writes beside it; the text, the focus and every note
   survive, and the note is then saved.
3. Desktop composes the round; the terminal adds source context and excludes a
   note under expected revisions, and the open sheet shows each change. A goal
   typed in the window and not entered is saved before the terminal's freeze
   reads the draft: that freeze is stale, the next one freezes F1 with the goal.
   The desktop copies and saves the round the terminal froze.
4. The reviewer grants F1 from the terminal and moves the window to another
   cycle. The harness, from the project folder, reads every page, the round's
   source attachment and its anchors; implements in the folder; and appends a
   response, which the window shows. Every interface's round is one golden
   payload: the bundle's canonical `round.json`.
5. Parity: what the terminal and the harness are told with the desktop open
   (forwarded to its owner) is what they are told with it closed (run here):
   the same human text, JSON data, pages, source bytes and refusals.
6. Desktop again: nothing captured on reopening; Capture revision [R2]; F1's
   This round checks the harness's edits; verification; Next round. The goal
   is entered and the desktop quits with the draft open. The terminal changes
   the draft alone; the next launch restores it as the terminal left it, and
   Save to File freezes F2.
7. A recipient who cannot reach `bpr` (not on its PATH) and has no grant for F2
   works from the saved Markdown alone: its digest and every item. It edits
   the folder and replies in prose; the reviewer pastes the reply, Capture
   revision [R3], This round shows the edit, and the note is verified.

Throughout, no agent component is reachable and the desktop starts no process.
"""
import argparse
import base64
import concurrent.futures
import contextlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from profile_activation_contract import private_fixture_parent  # noqa: E402
from review_cycle_two_round_contract import AGENT_NAMES, Desktop, check_comparison, sha256, span  # noqa: E402


POSIX = os.name != "nt"


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def line_bytes(content, first, last):
    """Bytes of lines first..last (1-based, inclusive) as a line range holds them:
    each line's own ending kept between them, none after the last."""
    lines = content.splitlines(keepends=True)
    return b"".join(lines[first - 1:last]).rstrip(b"\r\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    parser.add_argument("--cycle-application", required=True)
    args = parser.parse_args()
    with private_fixture_parent() as parent, contained_temporary_directory(prefix="parity-", parent=parent) as temporary:
        workspace = Path(temporary).resolve()
        make_root_private(workspace)
        data = workspace / "data"
        make_root_private(data)
        handoff = workspace / "handoff"
        handoff.mkdir()

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
            screen = workspace / "offscreen-screen.json"
            screen.write_text(json.dumps({"screens": [{"name": "Course", "x": 0, "y": 0, "width": 1920, "height": 1200, "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1}]}), encoding="utf-8")
            # Qt splits the platform argument at every ':', so a Windows drive
            # letter would end the path at "C". The desktop inherits the course's
            # working directory, so a relative path names the same file.
            spec = os.path.relpath(screen) if ":" in str(screen) else str(screen)
            environment["QT_QPA_PLATFORM"] = f"offscreen:configfile={spec}"

        project = workspace / "config-parser"
        files = {
            "src/parser.cpp": b"// Parses one config value.\r\n#include \"parser.hpp\"\r\n\r\nint parse_value(const char* aText)\r\n{\r\n\tif (aText == nullptr)\r\n\t\treturn -1;\r\n\treturn check(aText);\r\n}\r\n",
            "src/lexer.cpp": b"#include \"parser.hpp\"\n\nint next_token(const char* aCursor)\n{\n\tint count = 0;\n\twhile (*aCursor != '\\0')\n\t\t++count, ++aCursor;\n\treturn count;\n}\n",
            "include/parser.hpp": b"#pragma once\n\nint parse_value(const char* aText);\nint next_token(const char* aCursor);\n",
            "README.md": b"# Config parser\n\nReads one value per line.\n",
        }

        def write(path, content):
            (project / path).parent.mkdir(parents=True, exist_ok=True)
            (project / path).write_bytes(content)

        for path, content in files.items():
            write(path, content)

        def snapshot():
            return {path.relative_to(project).as_posix(): path.read_bytes() for path in sorted(project.rglob("*")) if path.is_file()}

        # ------------------------------------------------------------ the interfaces
        def bpr(*arguments, stdin=b"", consumer=False, cwd=None, machine=True, path=None):
            # capabilities needs no profile and takes none.
            profile = [] if arguments[:1] == ("capabilities",) else ["--profile", "1", "--data-root", str(data)]
            command = [args.cycle_application, "cycle", *arguments, *(["--json"] if machine else []), *profile]
            child = dict(environment, BPR_CYCLE_ROLE="consumer") if consumer else dict(environment)
            if path is not None:
                child["PATH"] = path
            return subprocess.run(command, input=stdin, capture_output=True, timeout=120, cwd=cwd or workspace, env=child)

        def envelope(result, arguments):
            assert result.stderr == b"", f"{arguments}: JSON calls keep standard error empty: {result.stderr!r}"
            return json.loads(result.stdout)

        def ok(*arguments, **options):
            result = bpr(*arguments, **options)
            value = envelope(result, arguments)
            assert result.returncode == 0 and value["outcome"] == "ok", f"{arguments}: {result.returncode} {value}"
            return value["data"]

        def refused(code, exit_code, *arguments, **options):
            result = bpr(*arguments, **options)
            value = envelope(result, arguments)
            assert result.returncode == exit_code and value["outcome"] == "error" and value["data"] is None and value["error"]["code"] == code, f"{arguments}: expected {code}/{exit_code}, got {result.returncode} {value}"
            return value["error"]

        def said(*arguments, **options):
            """The human terminal: words on standard output, nothing on standard error, no JSON."""
            result = bpr(*arguments, machine=False, **options)
            assert result.returncode == 0, f"{arguments}: exit {result.returncode}: {result.stderr!r}"
            assert result.stderr == b"", f"{arguments}: a success says nothing on standard error: {result.stderr!r}"
            text = result.stdout.decode("utf-8")
            assert text.strip(), f"{arguments}: no empty success"
            assert not text.lstrip().startswith(("{", "[")), f"{arguments}: human output is words, not JSON: {text[:80]!r}"
            return text

        def told_no(exit_code, *arguments, **options):
            """A human refusal: nothing on standard output, why on standard error."""
            result = bpr(*arguments, machine=False, **options)
            assert result.returncode == exit_code, f"{arguments}: expected exit {exit_code}, got {result.returncode}: {result.stdout!r} {result.stderr!r}"
            assert result.stdout == b"", f"{arguments}: a refusal writes nothing to standard output: {result.stdout!r}"
            text = result.stderr.decode("utf-8")
            assert text.strip() and not text.lstrip().startswith("{"), f"{arguments}: it says why, in words: {text!r}"
            return text

        def revision_said(text):
            found = re.search(r"\(draft \d+, revision (\d+)\)", text)
            assert found, f"review names the draft revision: {text[:200]!r}"
            return int(found.group(1))

        @contextlib.contextmanager
        def lease_held():
            import fcntl
            holder = os.open(data, os.O_RDONLY)
            try:
                fcntl.flock(holder, fcntl.LOCK_EX | fcntl.LOCK_NB)
                yield
            finally:
                os.close(holder)

        def single_owner():
            """RC58-05: while the desktop owns the profile nothing else can take its lease."""
            if POSIX:
                try:
                    with lease_held():
                        raise AssertionError("the desktop holds the profile's lease")
                except BlockingIOError:
                    pass

        def harness_pages(round_name, cwd=None, page_bytes="4096", name=True):
            """The bpr-cycle skill's reading: capabilities, then every page before acting."""
            capabilities = ok("capabilities", consumer=True, cwd=cwd)
            assert str(capabilities["cli_contract"]) == "1" and "round read" in capabilities["consumer_commands"], capabilities
            target = ["RC-1"] if name else []
            first = ok("round", "read", *target, "--round", round_name, "--page-bytes", page_bytes, consumer=True, cwd=cwd)
            pages = [first]
            while pages[-1]["next"] is not None:
                page = ok("round", "page", *target, "--round", round_name, "--cursor", pages[-1]["next"], consumer=True, cwd=cwd)
                held = sum(len(entry["items"]) for entry in pages)
                assert page["payload_sha256"] == first["payload_sha256"] and int(page["first_item"]) == held, f"each page names the digest and continues where the last ended: {page}"
                pages.append(page)
            items = [item for page in pages for item in page["items"]]
            assert len(items) == int(first["items_total"]) and first["instruction_set_complete"] is True and pages[-1]["next"] is None, "every item, once, of a round said to be complete"
            return pages, items

        def parity_snapshot(label):
            """Everything the terminal and the harness are told about F1, for comparison."""
            snap = {}
            snap["show human"] = said("show", "RC-1")
            snap["show"] = ok("show", "RC-1")
            pages, _ = harness_pages("F1", cwd=project, name=False)
            snap["pages 4096"] = pages
            snap["read"] = ok("round", "read", "RC-1", "--round", "F1", consumer=True)
            snap["read human"] = said("round", "read", "RC-1", "--round", "F1", consumer=True)
            snap["sources"] = ok("round", "source", "RC-1", "--round", "F1", consumer=True)
            snap["source S1"] = ok("round", "source", "RC-1", "--round", "F1", "--source", "S1", consumer=True)
            snap["source S1 range"] = ok("round", "source", "RC-1", "--round", "F1", "--source", "S1", "--range", "2:20", consumer=True)
            snap["source human"] = said("round", "source", "RC-1", "--round", "F1", "--source", "S1", consumer=True)
            snap["partition"] = refused("requires_partition", 5, "round", "read", "RC-1", "--round", "F1", "--budget-bytes", "64", consumer=True)
            snap["not granted F2"] = told_no(5, "round", "read", "RC-1", "--round", "F2", consumer=True)
            snap["reviewer refused"] = refused("policy_denied", 5, "freeze", "RC-1", "--expected-revision", "1", consumer=True)
            print(f"PARITY {label}", len(snap), "observations")
            return snap

        desktop = None
        try:
            # ---------------------------------------- 1. the terminal, before anything exists
            assert said("list") == "No cycles yet. Open a folder: bpr cycle open PATH\n"
            assert ok("list") == {"cycles": [], "next": None}
            nothing = told_no(2, "show", cwd=workspace)
            assert "Next: bpr cycle open ." in nothing, nothing
            refused("not_found", 2, "show", cwd=workspace)

            # ---------------------------------------------------------- 2. desktop, launch 1
            desktop = Desktop(args.application, data, workspace, 1, environment)
            opened = desktop.ask("open-project", path=str(project))
            assert opened["cycle"] == "RC-1" and opened["revisions"] == ["R1"], opened
            r1 = snapshot()
            single_owner()
            assert told_no(2, "note", "RC-1", "--body-file", "-", stdin=b"") == "A note needs some text; nothing was saved.\n", "an empty note is refused, not saved"

            needles = {
                "I1": ("src/parser.cpp", b"check(aText)"),
                "I2": ("src/parser.cpp", b"return -1;"),
                "I3": ("src/lexer.cpp", b"++count, ++aCursor;"),
                "I6": ("include/parser.hpp", b"int next_token(const char* aCursor);"),
            }
            desktop.ask("note", path="src/parser.cpp", body="Rename check to validate; check says nothing.", **span(r1["src/parser.cpp"], needles["I1"][1]))
            desktop.ask("suggest", path="src/parser.cpp", replacement="return kNoValue;", illustrative=False, rationale="Name the sentinel.", **span(r1["src/parser.cpp"], needles["I2"][1]))
            desktop.ask("note", path="src/lexer.cpp", body="Split the comma expression into two statements.", **span(r1["src/lexer.cpp"], needles["I3"][1]))

            # The terminal, forwarded to the desktop's owner; the window shows its note.
            # Long enough that F1 takes more than one 4 KiB page.
            long_note = "Document the return values.\n" + "".join(f"- Case {index}: say what parse_value returns, and why.\n" for index in range(60))
            noted = said("note", "RC-1", "--file", "README.md", "--lines", "3", "--body-file", "-", stdin=long_note.encode())
            assert noted == "Noted I4 on README.md:3 @R1 in RC-1. Saved locally; nothing was sent.\n", noted
            shown = desktop.ask("wait-notes", count=4)
            assert [note["item"] for note in shown["notes"]] == ["I1", "I2", "I3", "I4"] and shown["notes"][3]["body"] == long_note, shown["notes"]

            # The window writes a note and leaves it unsaved; the terminal writes beside it.
            typing = desktop.ask("draft-note", path="include/parser.hpp", body="Declare next_token in a lexer header.", **span(r1["include/parser.hpp"], needles["I6"][1]))
            assert typing["editor"] == {"visible": True, "text": "Declare next_token in a lexer header.", "focused": True}, typing["editor"]
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                writing = pool.submit(said, "note", "RC-1", "--file", "src/lexer.cpp", "--body-file", "-", stdin=b"From the terminal while the window types.\n")
                reading = pool.submit(ok, "show", "RC-1")
                assert writing.result().startswith("Noted I5 on src/lexer.cpp @R1 (whole file) in RC-1."), writing.result()
                assert len(reading.result()["notes"]) in (4, 5)
            kept = desktop.ask("wait-notes", count=5)
            assert kept["editor"] == {"visible": True, "text": "Declare next_token in a lexer header.", "focused": True}, f"the unsaved note, and the keyboard in it, survive the terminal's write: {kept['editor']}"
            saved_note = desktop.ask("save-note")
            assert saved_note["added"] == "I6" and saved_note["editor"]["visible"] is False, saved_note["editor"]
            bodies = {note["item"]: note["body"] for note in ok("show", "RC-1")["notes"]}
            assert sorted(bodies) == [f"I{index}" for index in range(1, 7)] and bodies["I6"] == "Declare next_token in a lexer header." and bodies["I5"] == "From the terminal while the window types.\n", bodies

            # ------------------------------------------ 3. one draft, two interfaces, one round
            share = desktop.ask("compose")
            assert share["round"] == "F1" and not share["frozen"] and [row["item"] for row in share["rows"]] == [f"I{index}" for index in range(1, 7)], share
            drafted = said("review", "RC-1")
            r_shown = revision_said(drafted)
            assert r_shown == share["revision"] == int(ok("review", "RC-1")["draft_revision"]), "the terminal and the window name one draft revision"
            context = said("review", "RC-1", "--context", "src/parser.cpp", "--context-lines", "4:9", "--expected-revision", str(r_shown))
            r_context = revision_said(context)
            assert r_context > r_shown and "1 sources" in context, context
            assert desktop.ask("share-state", revision=r_context)["revision"] == r_context
            excluded = said("review", "RC-1", "--exclude", "I5", "--expected-revision", str(r_context))
            r_excluded = revision_said(excluded)
            sheet = desktop.ask("share-state", revision=r_excluded)
            assert {row["item"]: row["included"] for row in sheet["rows"]}["I5"] is False, f"the open sheet shows the terminal's exclusion: {sheet['rows']}"
            stale = told_no(4, "review", "RC-1", "--include", "I5", "--expected-revision", str(r_context))
            assert f"The draft is at revision {r_excluded}, not {r_context}" in stale, stale

            typed = desktop.ask("goal", text="Tidy the parser before review", enter=False)
            assert typed["modified"] and typed["goal"] == "", typed
            frozen_stale = told_no(4, "freeze", "RC-1", "--expected-revision", str(r_excluded))
            goal_saved = ok("review", "RC-1")
            r_goal = int(goal_saved["draft_revision"])
            assert goal_saved["goal"] == "Tidy the parser before review" and r_goal > r_excluded, f"the typed goal was saved first: {goal_saved}"
            assert f"The draft is at revision {r_goal}, not {r_excluded}" in frozen_stale and "Nothing was frozen." in frozen_stale, frozen_stale
            # The next step a person is given is a person's command, not a JSON one.
            assert frozen_stale.endswith("\nNext: bpr cycle review RC-1\n") and stale.endswith("\nNext: bpr cycle review RC-1\n"), (frozen_stale, stale)
            froze = said("freeze", "RC-1", "--expected-revision", str(r_goal))
            assert froze.startswith("Froze F1 (5 notes, sha256 "), froze
            sheet = desktop.ask("share-state", revision=r_goal, frozen=True)
            assert sheet["frozen"] and sheet["goal"] == "Tidy the parser before review" and not sheet["modified"], sheet
            preview = sheet["document"].encode("utf-8")
            copied = desktop.ask("share", route="copy", evidence=str(workspace / "clipboard-F1.md"))
            assert copied["status"].startswith("Copied F1 to the clipboard"), copied["status"]
            desktop.ask("share", route="save", path=str(handoff / "RC-1-F1.md"))
            desktop.ask("share", route="bundle", path=str(handoff / "RC-1-F1.bpr-review.zip"))
            desktop.ask("close-share")
            clipboard = (workspace / "clipboard-F1.md").read_bytes()
            markdown_f1 = (handoff / "RC-1-F1.md").read_bytes()
            with zipfile.ZipFile(handoff / "RC-1-F1.bpr-review.zip") as bundle:
                bundled_document = bundle.read("review.md")
                golden = json.loads(bundle.read("round.json"))
            digest_f1 = next(entry["digest"] for entry in ok("show", "RC-1")["rounds"] if entry["round"] == "F1")
            assert froze.startswith(f"Froze F1 (5 notes, sha256 {digest_f1[:16]})"), "the terminal named the round the desktop then shared"
            assert clipboard == preview == markdown_f1 == bundled_document, "the desktop's Copy, Save and bundle carry one document, the one its sheet showed"
            assert golden["payload_sha256"] == digest_f1 and sha256(canonical(golden["payload"])) == digest_f1, "the bundle's payload is the canonical round"
            golden_items = golden["payload"]["items"]
            assert [item["id"] for item in golden_items] == ["I1", "I2", "I3", "I4", "I6"], [item["id"] for item in golden_items]

            # ------------------------------------------------- 4. a harness, the desktop open
            said("policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", "allowed")
            granted = said("round", "grant", "RC-1", "--round", "F1")
            assert granted.startswith("RC-1 F1 can now be read by a harness through the local interface"), granted
            second = workspace / "second"
            second.mkdir()
            (second / "notes.txt").write_text("second\n", encoding="utf-8")
            assert said("open", str(second)).startswith('Created RC-2 "second"')
            assert desktop.ask("resume", title="second", notes=0)["cycle"] == "RC-2", "the window now shows another cycle"
            single_owner()

            pages, items = harness_pages("F1", cwd=project, name=False)
            first = pages[0]
            assert len(pages) >= 2, f"4 KiB pages: {len(pages)}"
            assert first["cycle"] == "RC-1" and first["round"] == "F1" and first["payload_sha256"] == digest_f1, "the folder's cycle and round, not the window's"
            assert items == golden_items, "the harness holds the golden items exactly"
            assert first["assignment"] == {key: golden["payload"]["assignment"][key] for key in first["assignment"]}, (first["assignment"], golden["payload"]["assignment"])
            assert first["assignment"]["goal"] == "Tidy the parser before review"
            # Source bindings: each anchor names the bytes the reviewer dragged across, at R1.
            # A suggestion is on whole lines: the line the drag was in, its CRLF kept.
            sentinel_line = b"\t\treturn -1;\r\n"
            for item in items:
                if item["id"] in needles:
                    path, needle = needles[item["id"]]
                    anchor = item["anchors"][0]
                    assert anchor["path"] == path and anchor["source_revision_id"] == "R1" and anchor["blob_sha256"] == sha256(r1[path]), anchor
                    held = r1[path][anchor["start_byte"]:anchor["end_byte"]]
                    assert held == (sentinel_line if item["id"] == "I2" else needle), f"{item['id']}: the anchor holds exactly the bytes noted, not {held!r}"
            suggestion = next(item for item in items if item["id"] == "I2")["suggestions"][0]
            assert suggestion["intent"] == "exact_replacement" and suggestion["anchor"] == next(item for item in items if item["id"] == "I2")["anchors"][0], suggestion
            assert base64.b64decode(suggestion["original"]["base64"]) == sentinel_line and base64.b64decode(suggestion["replacement"]["base64"]) == b"\t\treturn kNoValue;\r\n", suggestion
            sources = ok("round", "source", "RC-1", "--round", "F1", consumer=True)
            assert [entry["id"] for entry in sources["sources"]] == ["S1"] and sources["sources"][0]["path"] == "src/parser.cpp" and sources["sources"][0]["source_revision"] == "R1", sources
            attachment = ok("round", "source", "RC-1", "--round", "F1", "--source", "S1", consumer=True)
            excerpt = base64.b64decode(attachment["base64"])
            assert excerpt == line_bytes(r1["src/parser.cpp"], 4, 9) and attachment["sha256"] == sha256(excerpt), f"the attachment is lines 4 to 9 at R1, CRLF and tabs kept between them: {excerpt!r}"
            # The attachment the harness reads is the golden payload's, byte for byte.
            golden_source = golden["payload"]["sources"][0]
            listed_source = sources["sources"][0]
            assert golden_source["id"] == listed_source["id"] and golden_source["path"] == listed_source["path"] and golden_source["source_revision_id"] == listed_source["source_revision"], (golden_source, listed_source)
            assert int(listed_source["start_byte"]) == golden_source["start_byte"] and int(listed_source["end_byte"]) == golden_source["end_byte"] and listed_source["blob_sha256"] == golden_source["blob_sha256"] == sha256(r1["src/parser.cpp"])
            assert base64.b64decode(golden_source["bytes"]["base64"]) == excerpt and r1["src/parser.cpp"][golden_source["start_byte"]:golden_source["end_byte"]] == excerpt
            assert golden["payload"]["selection"] == {"instruction_set_complete": True, "intentional_omissions": [], "item_count": 5, "source_count": 1}, golden["payload"]["selection"]
            human_read = said("round", "read", "--round", "F1", consumer=True, cwd=project)
            assert human_read.startswith(f"RC-1 F1 (sha256 {digest_f1}): asks to implement 5 items against R1."), human_read[:200]
            assert all(f"## {item['id']} " in human_read for item in items) and "That is every instruction: items 1 to 5 of 5." in human_read, human_read

            # The harness implements in the folder, then responds; the window shows the response.
            files["src/parser.cpp"] = files["src/parser.cpp"].replace(b"check(aText)", b"validate(aText)").replace(b"\t\treturn -1;", b"\t\treturn kNoValue;").replace(b"#include \"parser.hpp\"\r\n", b"#include \"parser.hpp\"\r\nconstexpr int kNoValue = -1;\r\n")
            files["src/lexer.cpp"] = files["src/lexer.cpp"].replace(b"\t\t++count, ++aCursor;\n", b"\t{\n\t\t++count;\n\t\t++aCursor;\n\t}\n")
            for path in ("src/parser.cpp", "src/lexer.cpp"):
                write(path, files[path])
            revision_of = {item["id"]: item["revision"] for item in items}
            response = {
                "schema": "bpr.cycle.response.v1", "response_id": "parity-harness-1", "cycle_id": "RC-1", "round_id": "F1", "round_payload_sha256": digest_f1,
                "claimed_author": {"display_name": "Course harness", "kind": "agent", "provider": None, "model": None, "effort": None}, "result_source_revision_id": None,
                "items": [
                    {"item_id": "I1", "item_revision": revision_of["I1"], "disposition": "addressed", "body": "Renamed check to validate.", "evidence": [{"kind": "path", "value": "src/parser.cpp"}]},
                    {"item_id": "I2", "item_revision": revision_of["I2"], "disposition": "addressed", "body": "Applied the exact suggestion.", "evidence": [{"kind": "path", "value": "src/parser.cpp"}]},
                    {"item_id": "I3", "item_revision": revision_of["I3"], "disposition": "addressed", "body": "Split into two statements. No tests were run.", "evidence": [{"kind": "path", "value": "src/lexer.cpp"}]},
                ],
            }
            appended = ok("response", "append", "RC-1", "--round", "F1", "--file", "-", stdin=json.dumps(response).encode(), consumer=True, cwd=project)
            assert appended["appended"] is True, appended
            # Another interface's write moves no window: the harness's response leaves the
            # window on the second cycle, and a terminal note on the cycle it shows leaves
            # Home in front ([[BPR-1171]]: a projection after another interface's write
            # brought the cycle page forward over Home).
            assert desktop.ask("state")["title"] == "second", "the harness's write leaves the window where the reviewer put it"
            assert desktop.ask("home")["home"] is True
            said("note", "RC-2", "--body-file", "-", stdin=b"A terminal note on the cycle the window holds.\n")
            behind = desktop.ask("wait-notes", count=1)
            assert behind["home"] is True and behind["title"] == "second", f"Home stays in front after another interface writes the cycle behind it: {behind['home']}"
            back = desktop.ask("resume", title="config-parser", notes=6)
            back = desktop.ask("wait-responses", count=1)
            assert back["cycle"] == "RC-1" and back["responses"] == 1, back
            assert any(row.startswith("Response parity-harness-1 to F1 · ") for row in back["listed"]), f"the window lists the response: {back['listed']}"
            i1 = next(note for note in back["notes"] if note["item"] == "I1")
            assert "F1 reply · " in i1["shown"] and "Renamed check to validate." in i1["shown"], f"and shows its words beside the note, as a claim: {i1['shown']}"
            assert back["revisions"] == ["R1"], "a response captures nothing"

            # ------------------------------------- 5. parity: the desktop open, then closed
            export_open = bpr("export", "RC-1", "--round", "F1", "--output", "-", machine=False)
            assert export_open.returncode == 0 and export_open.stdout == markdown_f1, "the terminal's export is the desktop's document, through the owner"
            single_owner()
            snap_open = parity_snapshot("desktop-open")
            desktop.quit()
            print("DESKTOP_1_CHILDREN", sorted(desktop.children) if desktop.sampling else "not sampled on this platform")
            assert not desktop.children, f"the desktop started no process: {desktop.children}"
            desktop.close()
            desktop = None

            snap_closed = parity_snapshot("cli-alone")
            for key in snap_open:
                assert snap_open[key] == snap_closed[key], f"parity: {key} differs with the desktop open and closed:\n{snap_open[key]!r}\n{snap_closed[key]!r}"
            export_closed = bpr("export", "RC-1", "--round", "F1", "--output", "-", machine=False)
            assert export_closed.returncode == 0 and export_closed.stdout == export_open.stdout, "the same export bytes, run here"
            print("PARITY desktop-open == cli-alone:", ", ".join(snap_open))

            # ------------------------------------------- 6. manual capture, and the draft
            desktop = Desktop(args.application, data, workspace, 2, environment)
            resumed = desktop.ask("resume", title="config-parser", notes=6)
            assert resumed["revisions"] == ["R1"] and resumed["responses"] == 1, f"nothing captured on reopening; the response kept: {resumed}"
            captured = desktop.ask("capture")
            assert captured["revisions"] == ["R1", "R2"], captured
            r2 = snapshot()
            for path in ("src/parser.cpp", "src/lexer.cpp"):
                check_comparison(desktop.ask("view", path=path, menu=["This round", "F1 · since R1"]), "R1", "R2", path, r1[path], r2[path])
            desktop.ask("view", path="src/parser.cpp", menu=["This round", "F1 · since R1"])
            for item in ("I1", "I2"):
                assert desktop.ask("decide", item=item, choice="Verified in")["offered"] == "Verified in R2"
            desktop.ask("view", path="src/lexer.cpp", menu=["This round", "F1 · since R1"])
            desktop.ask("decide", item="I3", choice="Verified in")
            nexted = desktop.ask("next-round")
            counts = {key: nexted[key] for key in ("carried", "added", "resolved", "archived")}
            print("NEXT ROUND", json.dumps(counts))
            assert counts == {"carried": 2, "added": 1, "resolved": 3, "archived": 0}, f"I4 and I6 carried, I5 added, three verified: {counts}"
            share = desktop.ask("compose")
            assert share["round"] == "F2" and not share["frozen"], share
            assert share["goal"] == "Tidy the parser before review", "the next round starts from the last goal"
            share = desktop.ask("goal", text="Second pass", replace=True)
            r_window = share["revision"]
            listed = {row["item"]: row["included"] for row in share["rows"]}
            assert listed == {"I4": True, "I6": True, "I5": True, "I1": False, "I2": False, "I3": False}, listed
            desktop.quit()
            print("DESKTOP_2_CHILDREN", sorted(desktop.children) if desktop.sampling else "not sampled on this platform")
            assert not desktop.children, f"the desktop started no process: {desktop.children}"
            desktop.close()
            desktop = None

            # The terminal alone changes the draft the desktop left open.
            alone = said("review", "RC-1")
            assert revision_said(alone) == r_window and "the next round is F2" in alone, alone
            dropped = "I5"
            changed = said("review", "RC-1", "--exclude", dropped, "--expected-revision", str(r_window))
            r_terminal = revision_said(changed)
            draft_alone = ok("review", "RC-1")
            assert draft_alone["goal"] == "Second pass" and dropped not in draft_alone["items"], draft_alone

            desktop = Desktop(args.application, data, workspace, 3, environment)
            desktop.ask("resume", title="config-parser", notes=6)
            restored = desktop.ask("compose")
            restored_rows = {row["item"]: row["included"] for row in restored["rows"]}
            assert restored["round"] == "F2" and restored["revision"] == r_terminal and restored["goal"] == "Second pass" and restored["field"] == "Second pass", f"the draft as the terminal left it: {restored}"
            assert restored_rows[dropped] is False and all(restored_rows[item] == listed[item] for item in listed if item != dropped), (restored_rows, listed)
            saved = desktop.ask("share", route="save", path=str(handoff / "RC-1-F2.md"))
            assert saved["frozen"] and saved["round"] == "F2", saved
            desktop.ask("close-share")

            # ----------------------------- 7. a recipient with no bpr and no grant for F2
            unreachable = workspace / "no-bpr"
            unreachable.mkdir()
            try:
                subprocess.run(["bpr", "cycle", "capabilities", "--json"], capture_output=True, timeout=30, env=dict(environment, PATH=str(unreachable)), cwd=project)
                raise AssertionError("bpr is not reachable from the remote recipient")
            except FileNotFoundError:
                pass
            not_granted = refused("policy_denied", 5, "round", "read", "RC-1", "--round", "F2", consumer=True)
            assert not_granted["detail"]["decision"] == "not_granted", not_granted
            markdown_f2 = (handoff / "RC-1-F2.md").read_bytes()
            text_f2 = markdown_f2.decode("utf-8")
            history = ok("show", "RC-1")
            digest_f2 = next(entry["digest"] for entry in history["rounds"] if entry["round"] == "F2")
            stated = re.search(r"Canonical round SHA-256: `([0-9a-f]{64})`", text_f2)
            assert stated and stated.group(1) == digest_f2, "the Markdown alone names the round's canonical digest"
            headed = re.findall(r"^## (I\d+) — item revision \d+$", text_f2, flags=re.MULTILINE)
            in_round = [item for item, included in restored_rows.items() if included]
            assert headed == in_round and headed, f"the Markdown alone holds every item of F2: {headed} vs {in_round}"
            for item in headed:
                first_line = bodies[item].split("\n")[0]
                assert first_line in text_f2, f"{item}'s words are in the Markdown"
            assert bpr("export", "RC-1", "--round", "F2", "--output", "-", machine=False).stdout == markdown_f2, "and it is the round the terminal exports"
            assert "I4" in headed, headed
            files["README.md"] += b"A missing value returns kNoValue.\n"
            write("README.md", files["README.md"])
            replied = desktop.ask("reply", item="I4", entry="Paste a reply to F2…", **{"from": "Remote recipient (chat)"}, text="I4: documented the return value in README.md. No tests were run.")
            assert replied["revisions"] == ["R1", "R2"], "a reply captures nothing"
            captured = desktop.ask("capture")
            assert captured["revisions"] == ["R1", "R2", "R3"], captured
            r3 = snapshot()
            check_comparison(desktop.ask("view", path="README.md", menu=["This round", "F2 · since R2"]), "R2", "R3", "README.md", r2["README.md"], r3["README.md"])
            verified = desktop.ask("decide", item="I4", choice="Verified in")
            assert verified["offered"] == "Verified in R3", verified["offered"]
            desktop.quit()
            print("DESKTOP_3_CHILDREN", sorted(desktop.children) if desktop.sampling else "not sampled on this platform")
            assert not desktop.children, f"the desktop started no process: {desktop.children}"
            desktop.close()
            desktop = None
        finally:
            if desktop is not None:
                print("\n".join(desktop.lines()[-60:]))
                desktop.close()

        final = ok("show", "RC-1")
        assert [entry["round"] for entry in final["rounds"]] == ["F1", "F2"] and len(final["notes"]) == 6, final["rounds"]
        channels = [entry["channel"] for entry in final["responses"]]
        assert channels == ["local_interface", "manual_entry"], f"the harness's response and the pasted reply, each as what it is: {channels}"
        assert not marks.exists(), f"an agent executable ran: {marks.read_text() if marks.exists() else ''}"
        print("PATH_CANARIES not run:", ", ".join(AGENT_NAMES))
    print("REVIEW_CYCLE_PARITY_OK")


if __name__ == "__main__":
    main()
