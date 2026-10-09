"""[[BPR-1092]] `bpr tour labels` as real processes, compared with the desktop's services on one fixture.

Arguments: the built `bpr`, the fixture CLI (`test_tour_cli_composition`, which stands in for `bpr`
with a read-only fixture provider and, with `--desktop-services`, prints what the core and
application functions the desktop composition calls decide for the same input), and the pack's
`parser-cases.json`.

The course checks that the CLI and the desktop give the same catalog, annotations, label edits and
counts; that the catalog and parse open no profile and every command keeps no store of its own;
and that a conflict, a missing capability and incomplete coverage are said, in JSON and in text.
"""
import json
import os
from pathlib import Path
import select as readiness
import signal
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

URL = "https://github.com/o/r/pull/7"
ALL_TWELVE = "praise,nitpick,suggestion,issue,todo,question,thought,chore,note,typo,polish,quibble"
ANNOTATION_FIELDS = ("status", "label", "expectation", "guidance", "diagnostics", "body_sha256")


def main():
    bpr = str(Path(sys.argv[1]).resolve())
    fixture = str(Path(sys.argv[2]).resolve())
    cases = json.loads(Path(sys.argv[3]).read_text(encoding="utf-8"))["cases"]
    environment = dict(os.environ, QT_QPA_PLATFORM="bpr-no-display", NO_COLOR="1")
    environment.pop("DISPLAY", None)
    environment.pop("WAYLAND_DISPLAY", None)
    environment.pop("BPR_TOUR_FIXTURE_FEEDBACK", None)
    with contained_temporary_directory(prefix="bpr-cli-labels-") as temporary:
        temp = Path(temporary).resolve()
        root = temp / "data"
        bodies = temp / "bodies"
        bodies.mkdir()

        def run(binary, *args, data=None, feedback=None, profile_root=True):
            env = dict(environment)
            if feedback:
                env["BPR_TOUR_FIXTURE_FEEDBACK"] = feedback
            command = [binary, "tour", *args] + (["--data-root", str(root)] if profile_root else [])
            return subprocess.run(command, input=data, capture_output=True, timeout=30, env=env)

        def document(result):
            assert result.stdout.endswith(b"\n"), result
            value = json.loads(result.stdout)
            assert value["schema"] == "bpr.tour.cli-result.v1", value
            return value

        def desktop(*args):
            result = subprocess.run([fixture, "--desktop-services", *args], capture_output=True, timeout=30, env=environment)
            assert result.returncode == 0, (args, result)
            return json.loads(result.stdout)

        def body_file(name, text):
            path = bodies / name
            path.write_bytes(text.encode("utf-8"))
            return str(path)

        def tree():
            return sorted(str(path.relative_to(root)) for path in root.rglob("*")) if root.exists() else None

        # The catalog: the chip menu's catalog, and no profile is opened or created for it.
        catalog = run(bpr, "labels", "--json")
        assert catalog.returncode == 0 and not catalog.stderr, catalog
        data = document(catalog)["data"]
        assert data["labels"] == desktop("catalog")["labels"] and len(data["labels"]) == 12, data
        assert data["catalog"] == "bpr.feedback-label-catalog.v1" and data["parser_revision"] == "1"
        assert document(run(bpr, "labels", "catalog", "--json"))["data"] == data
        human = run(bpr, "labels")
        assert human.returncode == 0 and human.stdout.startswith(b"Conventional Comments labels") and b"quibble" in human.stdout
        refused = run(bpr, "labels", "--profile", "1", "--json")
        assert refused.returncode == 2 and document(refused)["error"]["code"] == "unsupported_option"
        assert tree() is None, "the catalog created no profile"

        # Parse: every pack parser case, through the CLI, the pack's oracle and the desktop's recognizer.
        for case in cases:
            path = body_file(case["id"] + ".md", case["body"])
            result = run(bpr, "labels", "parse", "--file", path, "--json")
            assert result.returncode == 0 and not result.stderr, (case["id"], result)
            cli = document(result)["data"]
            assert cli["status"] == case["expected_status"], (case["id"], cli)
            assert cli["label"] == case.get("expected_label"), (case["id"], cli)
            assert cli["expectation"] == case["expected_expectation"], (case["id"], cli)
            assert set(case.get("required_diagnostics", [])) <= set(cli["diagnostics"]), (case["id"], cli)
            oracle = desktop("parse", path)
            for field in ANNOTATION_FIELDS:
                assert cli[field] == oracle[field], (case["id"], field, cli[field], oracle[field])
            for span in ("raw_label", "header", "subject"):
                assert (cli[span]["text"] if cli[span] else None) == oracle[span], (case["id"], span)
            assert [entry["text"] for entry in cli["decorations"]] == oracle["decorations"], case["id"]
        piped = run(bpr, "labels", "parse", "--file", "-", "--json", data="todo (if-minor): x\r\n".encode())
        assert document(piped)["data"]["label"] == "todo" and document(piped)["data"]["expectation"] == "if_minor"
        text = run(bpr, "labels", "parse", "--file", "-", data=b"issue (blocking): race")
        assert text.returncode == 0 and text.stdout.startswith(b"recognized: issue (blocking)\n"), text
        missing = run(bpr, "labels", "parse", "--json")
        assert missing.returncode == 2 and document(missing)["error"]["code"] == "invalid_input"
        assert tree() is None, "parse created no profile"

        # A draft with one stop, seeded by the fixture CLI; label edits then run through the real bpr.
        opened = run(fixture, "open", URL, "--json")
        assert opened.returncode == 0, opened
        select = ["--profile", "1", "--draft", document(opened)["data"]["draft_id"]]

        def revision():
            return document(run(bpr, "status", *select, "--json"))["data"]["revision"]

        def stop_body():
            return document(run(bpr, "show", "stop:1", *select, "--json"))["data"]["text"]

        added = run(fixture, "add", "src/a.cpp:1-2", "--file", "-", *select, "--expected-revision", revision(), "--json",
                    data="The cache outlives the view.\r\n".encode())
        assert added.returncode == 0, added
        before = tree()
        steps = [["--label", "question"], ["--expectation", "blocking"], ["--decoration", "security"],
                 ["--remove-decoration", "security"], ["--label", "issue"], ["--expectation", "if-minor"],
                 ["--label", "plain"], ["--label", "todo"]]
        for step in steps:
            shown = stop_body()
            expected = desktop("compose", body_file("shown.md", shown), *step)
            assert expected["refusal"] == "none", (step, expected)
            current = revision()
            edited = run(bpr, "labels", "edit", "stop:1", *step, *select, "--expected-revision", current, "--json")
            assert edited.returncode == 0 and not edited.stderr, (step, edited)
            assert stop_body() == expected["body"], (step, stop_body(), expected["body"])
            annotation = document(edited)["data"]["label_edit"]["annotation"]
            oracle = desktop("parse", body_file("after.md", expected["body"]))
            assert all(annotation[field] == oracle[field] for field in ANNOTATION_FIELDS), (step, annotation, oracle)
            assert document(edited)["data"]["changed"] is True and revision() != current
        assert stop_body() == "todo: The cache outlives the view.\r\n"
        assert tree() == before, "label edits wrote only the profile's existing tour store"
        stale = run(bpr, "labels", "edit", "stop:1", "--label", "issue", *select, "--expected-revision", "1", "--json")
        assert stale.returncode == 4 and document(stale)["status"] == "conflict" and document(stale)["error"]["code"] == "revision_conflict"
        assert stop_body() == "todo: The cache outlives the view.\r\n"
        unchanged = run(bpr, "labels", "edit", "stop:1", "--label", "todo", *select, "--expected-revision", revision(), "--json")
        assert unchanged.returncode == 0 and document(unchanged)["data"]["changed"] is False
        human = run(bpr, "labels", "edit", "stop:1", "--label", "praise", *select, "--expected-revision", revision())
        assert human.returncode == 0 and b"Stop 1 now reads praise:" in human.stdout and not human.stdout.startswith(b"{")
        quoted = run(bpr, "edit", "stop:1:body", "--file", "-", *select, "--expected-revision", revision(), "--json", data=b"> quoted first\n")
        assert quoted.returncode == 0
        expected = desktop("compose", body_file("quoted.md", "> quoted first\n"), "--label", "issue")
        restructure = run(bpr, "labels", "edit", "stop:1", "--label", "issue", *select, "--expected-revision", revision(), "--json")
        assert expected["refusal"] == "restructuring_required" and restructure.returncode == 5, restructure
        assert document(restructure)["error"]["code"] == "restructuring_required" and stop_body() == "> quoted first\n"
        words = run(bpr, "labels", "edit", "stop:1", "--label", "issue", *select, "--expected-revision", revision())
        assert words.returncode == 5 and not words.stdout and b"restructuring" in words.stderr, words
        comment = run(bpr, "labels", "edit", "comment:12", "--label", "issue", *select, "--expected-revision", revision(), "--json")
        assert comment.returncode == 5 and document(comment)["error"]["code"] == "capability_unavailable", comment
        assert b"desktop" in run(bpr, "labels", "edit", "comment:12", "--label", "issue", *select, "--expected-revision", revision()).stderr
        assert tree() == before

        # Counts: the fixture provider's one read, against the desktop window's counting, in every coverage.
        for feedback in ("threads", "rest", "partial"):
            for mode, columns in (("open", "none"), ("open", "issue,question"), ("all", "question,issue,todo"), ("all", ALL_TWELVE)):
                result = run(fixture, "labels", "counts", *select, "--mode", mode, "--columns", columns, "--json", feedback=feedback)
                assert result.returncode == 0, (feedback, mode, columns, result)
                envelope = document(result)
                counts = envelope["data"]["counts"]
                oracle = desktop("counts", feedback, mode, columns)
                assert counts["pull_request"] == oracle["pull_request"], (feedback, mode, columns, counts["pull_request"], oracle["pull_request"])
                assert counts["not_on_a_file"] == oracle["not_on_a_file"], (feedback, mode, columns)
                assert counts["files"] == [row for row in oracle["files"] if row["scope_total"] != "0"], (feedback, mode, columns)
                assert counts["folders"] == [row for row in oracle["folders"] if row["scope_total"] != "0"], (feedback, mode, columns)
                assert counts["quiet_files"]["count"] == str(sum(row["scope_total"] == "0" for row in oracle["files"]))
                assert counts["collection"] == oracle["collection"] and counts["unit"] == "conversation"
                assert counts["mode"] == mode and counts["columns_source"] == "option"
                whole = counts["pull_request"]
                assert sum(int(entry["count"]) for entry in whole["labels"]) + int(whole["other"]["count"]) == int(whole["included"])
                if not counts["exact"]:
                    assert counts["limits"] and any(warning["code"] == "counts_not_exact" for warning in envelope["warnings"])
                if feedback == "partial":
                    assert not counts["exact"] and "collection_partial" in counts["limits"] and counts["collection"] == "partial"
                    assert counts["quiet_files"]["exact"] is False, "a file's zero under a partial read is not exact"
                if feedback == "rest" and mode == "open":
                    assert "resolution_unknown" in counts["limits"] and whole["uncertain"] != "0"
        partial = run(fixture, "labels", "counts", *select, feedback="partial")
        assert partial.returncode == 0 and b"not exact" in partial.stdout and b"not a clean zero" in partial.stderr, partial
        # The defaults come from the profile's own settings store, which the desktop and `hooks show`
        # share; reading it the first time creates it with defaults. Nothing else appears or goes.
        assert set(tree()) - set(before) <= {str(Path("state") / "v1" / "workbench-settings.sqlite3")}, sorted(set(tree()) ^ set(before))
        assert set(before) <= set(tree()), "counting removed nothing"

        if os.name != "nt":
            owner_race(fixture, bpr, root, select, run, document, stop_body, revision)
    print("tour-cli-labels:pass")


def owner_race(fixture, bpr, root, select, run, document, stop_body, revision):
    """A running owner process: the label edit is forwarded, counts are refused, and the edit the
    other interface made first wins over a label chosen against the older revision."""
    owner = subprocess.Popen([fixture, "--serve-owner", str(root), "1"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        ready = readiness.select([owner.stdout], [], [], 15)[0]
        assert ready and owner.stdout.readline().strip() == "ready"
        current = revision()
        written = run(bpr, "edit", "stop:1:body", "--file", "-", *select, "--expected-revision", current, "--json", data=b"Owned text.\n")
        assert written.returncode == 0, written
        current = revision()
        labelled = run(bpr, "labels", "edit", "stop:1", "--label", "question", *select, "--expected-revision", current, "--request-id", "owned-label", "--json")
        assert labelled.returncode == 0 and document(labelled)["data"]["operation_recorded"] is True, labelled
        assert stop_body() == "question: Owned text.\n"
        refused = run(fixture, "labels", "counts", *select, "--json", feedback="threads")
        assert refused.returncode == 3 and document(refused)["error"]["code"] == "interaction_required"
        shown = revision()
        other = run(bpr, "edit", "stop:1:body", "--file", "-", *select, "--expected-revision", shown, "--json", data=b"The author's newer text.\n")
        assert other.returncode == 0
        raced = run(bpr, "labels", "edit", "stop:1", "--label", "issue", *select, "--expected-revision", shown, "--json")
        assert raced.returncode == 4 and document(raced)["error"]["code"] == "revision_conflict"
        assert stop_body() == "The author's newer text.\n"
    finally:
        owner.send_signal(signal.SIGKILL)
        owner.wait(timeout=10)
    print("tour-cli-labels-owner:pass")


if __name__ == "__main__":
    main()
