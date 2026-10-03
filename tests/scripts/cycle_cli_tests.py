"""[[BPR-1167]], [[BPR-1169]]. `bpr cycle` through the built executable, as real processes.

Each command runs in a near-empty environment: no HOME, no desktop, no display
(a platform plugin that cannot exist is named, so any GUI start would fail), no
account and no provider. The course opens a fixture folder as a local Review
Cycle and works it through notes, a round, its export and a response, then
checks the contract's edges: human text versus one JSON document, standard
output versus standard error, refusal without a terminal, typed errors with exit
codes, an unsupported schema version, explicit standard input only, and one
writer per profile.
"""
import base64
import contextlib
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "end_to_end"))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from bpr_narrow_owner_private import make_root_private  # noqa: E402

ENVELOPE_KEYS = {"schema", "command", "operation_id", "outcome", "complete", "data", "error"}


def environment(extra=None):
    """Nothing a desktop session, an account or a provider would supply."""
    value = {"QT_QPA_PLATFORM": "bpr-no-display", "NO_COLOR": "1"}
    if os.name == "nt":
        # PATH too: Windows finds an executable's DLLs through it, and a
        # development build's Qt libraries are on it, not beside bpr.exe.
        # Without it the child cannot load (0xC0000135).
        for name in ("SystemRoot", "SystemDrive", "TEMP", "TMP", "PATH"):
            if name in os.environ:
                value[name] = os.environ[name]
    value.update(extra or {})
    return value


class Course:
    def __init__(self, binary, data, home):
        self.binary = binary
        self.data = data
        self.home = home

    def run(self, *arguments, stdin=None, cwd=None, extra=None, data_root=True):
        command = [self.binary, "cycle", *arguments]
        if data_root:
            command += ["--data-root", str(self.data)]
        result = subprocess.run(command, input=stdin if stdin is not None else b"", cwd=cwd or self.home,
                                env=environment(extra), capture_output=True, timeout=120)
        return result.returncode, result.stdout.decode("utf-8"), result.stderr.decode("utf-8")

    def json(self, *arguments, code=0, **options):
        status, out, err = self.run(*arguments, "--json", **options)
        assert err == "", f"JSON calls keep standard error empty: {arguments} {err!r}"
        assert out.endswith("\n") and out.count("\n") == 1, f"one JSON document: {out!r}"
        value = json.loads(out)
        assert set(value) == ENVELOPE_KEYS, value
        assert value["schema"] == "bpr.cycle.cli-result.v1" and value["command"].startswith("cycle ")
        assert status == code, f"{arguments}: exit {status}, expected {code}: {value}"
        if code == 0:
            assert value["outcome"] == "ok" and value["error"] is None and value["data"] is not None, value
        else:
            assert value["outcome"] == "error" and value["data"] is None and value["complete"] is False, value
            assert set(value["error"]) >= {"code", "message", "next"}, value
        return value

    def refused(self, code, exit_code, *arguments, **options):
        value = self.json(*arguments, code=exit_code, **options)
        assert value["error"]["code"] == code, f"{arguments}: {value['error']}"
        return value["error"]

    def human(self, *arguments, code=0, **options):
        status, out, err = self.run(*arguments, **options)
        assert status == code, f"{arguments}: exit {status}, expected {code}: {out!r} {err!r}"
        assert not out.lstrip().startswith("{"), f"human output is not JSON: {out!r}"
        if code == 0:
            assert err == "", f"success writes nothing to standard error: {err!r}"
        else:
            assert out == "", f"a refusal writes nothing to standard output: {out!r}"
            assert err.strip(), "a refusal says why on standard error"
        return out, err


def contract_edges(course):
    out, _ = course.human("capabilities", data_root=False)
    assert out.startswith("bpr cycle") and "No window, display, account, provider, agent or network" in out
    value = course.json("capabilities", data_root=False)
    data = value["data"]
    assert data["cli_contract"] == "1" and data["result_schema"] == "bpr.cycle.cli-result.v1"
    assert data["gui_required"] is False and data["ai_required"] is False and data["provider_required"] is False and data["network"] is False
    for command in ("open", "show", "capture", "note", "review", "freeze", "export", "compare", "response import"):
        assert command in data["commands"], command
    course.refused("unsupported_version", 2, "capabilities", "--schema-version", "2", data_root=False)
    course.refused("unsupported_option", 2, "capabilities", "--yes", data_root=False)
    course.refused("unsupported_command", 2, "publish", data_root=False)
    _, err = course.human("show", "--force", code=2)
    assert "--force" in err
    course.refused("invalid_input", 2, "show", "RC-01")


def opens_and_notes(course, project):
    first = course.json("open", str(project))["data"]
    assert first["created"] is True and first["cycle"] == "RC-1" and first["latest"] == "R1" and first["latest_entries"] == "2", first
    again = course.json("open", ".", cwd=project)["data"]
    assert again["created"] is False and again["cycle"] == "RC-1", "the same folder opens the same cycle"
    out, _ = course.human("open", str(project))
    assert out.startswith('Opened RC-1 "project"')

    # Without a terminal: no editor, no prompt, and stray standard input is never read.
    error = course.refused("interaction_required", 3, "note", "RC-1", "--file", "src/a.txt", stdin=b"not a note")
    assert "--body-file -" in error["next"]
    assert course.json("show", "RC-1")["data"]["notes"] == [], "nothing was saved"

    noted = course.json("note", "RC-1", "--file", "src/a.txt", "--lines", "2:3", "--body-file", "-", stdin="Cache this lookup.\n".encode())["data"]
    assert noted["item"] == "I1" and noted["target"]["first_line"] == "2" and noted["target"]["last_line"] == "3", noted
    body = project.parent / "note.md"
    body.write_bytes("Whole-file remark é.\n".encode())
    assert course.json("note", "--file", "src/a.txt", "--body-file", str(body), cwd=project)["data"]["item"] == "I2", "the working folder's cycle"
    course.refused("invalid_input", 2, "note", "RC-1", "--file", "src/a.txt", "--lines", "9:12", "--body-file", "-", stdin=b"x")
    course.refused("not_found", 2, "note", "RC-1", "--file", "src/missing.txt", "--body-file", "-", stdin=b"x")
    course.refused("invalid_input", 2, "note", "RC-1", "--body-file", "-", stdin=b"  \n")
    course.refused("not_found", 2, "show", "RC-7")
    unbound = course.refused("not_found", 2, "show", cwd=project.parent)
    assert unbound["next"] == "bpr cycle open ."

    shown = course.json("show", cwd=project)["data"]
    assert [note["item"] for note in shown["notes"]] == ["I1", "I2"]
    assert shown["notes"][0]["body"] == "Cache this lookup.\n" and shown["notes"][1]["target"]["kind"] == "file"
    out, _ = course.human("show", "RC-1")
    assert "I1  src/a.txt:2-3 @R1  open  Cache this lookup." in out


def rounds_and_exports(course, project, exports):
    review = course.json("review", "RC-1")["data"]
    assert review["items"] == ["I1", "I2"] and review["round"] == "F1" and review["frozen"] is False
    assert "Cache this lookup." in review["document"] and review["refusal"] is None
    revision = review["draft_revision"]
    course.refused("interaction_required", 3, "review", "RC-1", "--exclude", "I2")
    stale = course.refused("stale_revision", 4, "review", "RC-1", "--exclude", "I2", "--expected-revision", str(int(revision) + 7))
    assert stale["detail"]["draft_revision"] == revision
    edited = course.json("review", "RC-1", "--exclude", "I2", "--expected-revision", revision)["data"]
    assert edited["changed"] is True and edited["items"] == ["I1"] and edited["left_out"] == ["I2"]
    out, _ = course.human("review", "RC-1")
    assert out.startswith("RC-1: the next round is F1") and "Left out: I2." in out and "## I1" in out

    error = course.refused("interaction_required", 3, "freeze", "RC-1")
    assert error["next"] == f"bpr cycle freeze RC-1 --expected-revision {edited['draft_revision']}"
    assert course.json("show", "RC-1")["data"]["rounds"] == [], "refusing froze nothing"
    course.refused("stale_revision", 4, "freeze", "RC-1", "--expected-revision", revision)
    frozen = course.json("freeze", "RC-1", "--expected-revision", edited["draft_revision"])["data"]
    assert frozen["round"] == "F1" and frozen["items"] == "1" and frozen["reused"] is False
    assert course.json("freeze", "RC-1", "--expected-revision", edited["draft_revision"])["data"]["reused"] is True

    # The terminal prints the exact bytes; a file holds the same bytes.
    status, printed, err = course.run("export", "RC-1", "--round", "F1", "--output", "-")
    assert status == 0 and err == "" and printed.startswith("# Review Cycle RC-1"), (status, err)
    target = exports / "review.md"
    saved = course.json("export", "RC-1", "--round", "F1", "--output", str(target))["data"]
    assert target.read_bytes() == printed.encode() and saved["written"] is True
    assert saved["digest"] == hashlib.sha256(printed.encode()).hexdigest()
    assert course.json("export", "RC-1", "--round", "F1", "--output", str(target))["data"]["written"] is False
    target.write_bytes(b"someone else's file\n")
    conflict = course.refused("conflict", 4, "export", "RC-1", "--round", "F1", "--output", str(target))
    other = hashlib.sha256(b"someone else's file\n").hexdigest()
    assert conflict["detail"]["existing_digest"] == other and target.read_bytes() == b"someone else's file\n"
    course.refused("conflict", 4, "export", "RC-1", "--round", "F1", "--output", str(target), "--replace", "0" * 64)
    course.json("export", "RC-1", "--round", "F1", "--output", str(target), "--replace", other)
    assert target.read_bytes() == printed.encode()
    course.refused("invalid_input", 2, "export", "RC-1", "--round", "F1", "--output", "-")
    course.refused("not_found", 2, "export", "RC-1", "--round", "F2", "--output", str(exports / "f2.md"))
    assert not (exports / "f2.md").exists()
    bundle = exports / "round.bpr-review.zip"
    course.json("export", "RC-1", "--round", "F1", "--output", str(bundle), "--format", "bundle")
    assert bundle.read_bytes().startswith(b"PK")
    deliveries = course.json("show", "RC-1")["data"]["deliveries"]
    assert {(entry["state"], entry["channel"]) for entry in deliveries} == {("retrieved", "terminal_output"), ("saved", "local_file")}

    # A later capture and a comparison of retained bytes.
    (project / "src" / "a.txt").write_bytes(b"one\nTWO\nthree\nfour\nfive\n")
    captured = course.json("capture", "RC-1")["data"]
    assert captured["recorded"] is True and captured["latest"] == "R2"
    assert course.json("capture", "RC-1")["data"]["recorded"] is False
    entries = course.json("compare", "RC-1", "--from", "R1", "--to", "R2")["data"]["entries"]
    assert {"path": "src/a.txt", "change": "modified"} in entries and {"path": "README.md", "change": "unchanged"} in entries
    rows = course.json("compare", "RC-1", "--from", "R1", "--to", "R2", "--path", "src/a.txt")["data"]["rows"]
    assert {"kind": "added", "left_line": None, "right_line": "5", "text": "five"} in rows
    course.refused("not_found", 2, "compare", "RC-1", "--from", "R9")
    return printed


def responses(course, printed):
    digest = hashlib.sha256(printed.encode()).hexdigest()
    shown = course.json("show", "RC-1")["data"]
    round_digest = shown["rounds"][0]["digest"]
    assert round_digest != digest, "the round digest names the canonical payload, not the document"
    response = {
        "schema": "bpr.cycle.response.v1", "response_id": "RESP-1", "cycle_id": "RC-1", "round_id": "F1",
        "round_payload_sha256": round_digest,
        "claimed_author": {"display_name": "Fixture recipient", "kind": "human", "provider": None, "model": None, "effort": None},
        "result_source_revision_id": None,
        "items": [{"item_id": "I1", "item_revision": "1", "disposition": "addressed", "body": "Cached.", "evidence": []}],
    }
    document = json.dumps(response).encode()
    appended = course.json("response", "import", "RC-1", "--round", "F1", "--file", "-", stdin=document)["data"]
    assert appended["appended"] is True and appended["round"] == "F1"
    assert course.json("response", "import", "RC-1", "--round", "F1", "--file", "-", stdin=document)["data"]["appended"] is False
    wrong = dict(response, cycle_id="RC-2", response_id="RESP-2")
    course.refused("conflict", 4, "response", "import", "RC-1", "--round", "F1", "--file", "-", stdin=json.dumps(wrong).encode())
    course.refused("invalid_input", 2, "response", "import", "RC-1", "--round", "F1", "--file", "-", stdin=b"{}")
    course.refused("not_found", 2, "response", "import", "RC-1", "--round", "F4", "--file", "-", stdin=document)
    assert len(course.json("show", "RC-1")["data"]["responses"]) == 1
    notes = course.json("show", "RC-1")["data"]["notes"]
    assert all(note["lifecycle"] == "open" for note in notes), "a response changes no note"


def response_document(round_id, round_digest, response_id="RESP-H1", disposition="addressed", items=("I1",)):
    """A harness's structured response; its author, provider and model are only claims."""
    return json.dumps({
        "schema": "bpr.cycle.response.v1", "response_id": response_id, "cycle_id": "RC-1", "round_id": round_id,
        "round_payload_sha256": round_digest,
        "claimed_author": {"display_name": "Some harness", "kind": "agent", "provider": "an-unknown-provider", "model": "certified-model", "effort": None},
        "result_source_revision_id": None,
        "items": [{"item_id": item, "item_revision": "1", "disposition": disposition, "body": "Done.", "evidence": []} for item in items],
    }).encode()


def round_digest(course, cycle, round_id):
    return next(entry["digest"] for entry in course.json("show", cycle)["data"]["rounds"] if entry["round"] == round_id)


def retrieval(course, project, other):
    """[[BPR-1169]]. A harness reads one frozen round the reviewer granted, and may respond to it.

    Naming a round is not permission; a grant names one round of one cycle and is
    decided like any export of it; a read returns that round's payload and its own
    attachments only, whole items a page at a time bound to its digest; a revoked
    grant reads nothing; a response grant appends a statement and nothing else.
    """
    # Two active cycles: another folder's cycle, whose first round holds 31 short notes.
    assert course.json("open", str(other))["data"]["cycle"] == "RC-2"
    for index in range(31):
        course.json("note", "RC-2", "--body-file", "-", stdin=f"Note {index:02d} of the second cycle; ".encode() + b"w" * 150)
    review = course.json("review", "RC-2")["data"]
    assert len(review["items"]) == 31, review["items"]
    assert course.json("freeze", "RC-2", "--expected-revision", review["draft_revision"])["data"]["round"] == "F1"

    # RC-1 F2: both notes and README.md's bytes as context, nothing else.
    draft = course.json("review", "RC-1")["data"]
    draft = course.json("review", "RC-1", "--include", "I2", "--expected-revision", draft["draft_revision"])["data"]
    course.refused("invalid_input", 2, "review", "RC-1", "--context-lines", "1", "--expected-revision", draft["draft_revision"])
    course.refused("not_found", 2, "review", "RC-1", "--context", "src/missing.txt", "--expected-revision", draft["draft_revision"])
    draft = course.json("review", "RC-1", "--context", "README.md", "--expected-revision", draft["draft_revision"])["data"]
    assert draft["changed"] is True and draft["sources"] == "1" and draft["round"] == "F2", draft
    assert course.json("freeze", "RC-1", "--expected-revision", draft["draft_revision"])["data"]["items"] == "2"

    # Naming a round is not permission, and exposing it is decided like any export of it.
    error = course.refused("policy_denied", 5, "round", "read", "RC-1", "--round", "F2")
    assert error["detail"]["decision"] == "not_granted" and error["next"] == "bpr cycle round grant RC-1 --round F2", error
    error = course.refused("policy_denied", 5, "round", "grant", "RC-1", "--round", "F2")
    assert error["detail"]["decision"] == "channel_unconfigured" and "--channel external_retrieval --permission allowed" in error["next"], error
    assert course.json("show", "RC-1")["data"]["grants"] == [], "nothing was granted"
    course.refused("round_not_ready", 5, "round", "grant", "RC-1", "--round", "F3")
    table = course.json("policy")["data"]["policy"]
    assert len(table) == 15 and {"classification": "unclassified", "channel": "external_retrieval", "configured": "unconfigured", "decision": "channel_unconfigured"} in table
    course.refused("invalid_input", 2, "policy", "--channel", "external_retrieval")
    course.refused("invalid_input", 2, "policy", "RC-1")
    assert course.json("policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", "allowed")["data"]["changed"] is True

    granted = course.json("round", "grant", "RC-1", "--round", "F2", "--read-only")["data"]
    f2 = round_digest(course, "RC-1", "F2")
    assert granted["respond"] is False and granted["payload_sha256"] == f2 and granted["invocation"] == "bpr cycle round read RC-1 --round F2", granted
    value = course.json("round", "read", "RC-1", "--round", "F2")
    read = value["data"]
    assert value["complete"] is True and read["next"] is None and read["payload_sha256"] == f2 and read["items_total"] == "2" and read["sources_total"] == "1", read
    assert [entry["id"] for entry in read["items"]] == ["I1", "I2"] and read["items"][0]["body"] == "Cache this lookup.\n" and (read["first_item"], read["items_through"]) == ("0", "2")
    assert read["assignment"]["action"] == "implement" and read["instruction_set_complete"] is True
    out, _ = course.human("round", "read", "RC-1", "--round", "F2")
    assert out.startswith("RC-1 F2 (sha256 " + f2) and "## I1 (implement)" in out and "That is every instruction: items 1 to 2 of 2." in out, out
    # The grant names one round of one cycle; a working folder finds only its own cycle.
    assert course.refused("policy_denied", 5, "round", "read", "RC-1", "--round", "F1")["detail"]["decision"] == "not_granted"
    assert course.refused("policy_denied", 5, "round", "read", "RC-2", "--round", "F1")["detail"]["decision"] == "not_granted"
    assert course.json("round", "read", "--round", "F2", cwd=project)["data"]["cycle"] == "RC-1"
    course.refused("round_not_ready", 5, "round", "read", "--round", "F2", cwd=other)

    # Only the round's own material: its attachments, not the drafts or notes outside it.
    course.refused("round_not_ready", 5, "round", "read", "RC-1", "--round", "F3")
    listing = course.json("round", "source", "RC-1", "--round", "F2")["data"]
    assert [entry["id"] for entry in listing["sources"]] == ["S1"] and listing["sources"][0]["path"] == "README.md" and listing["next"] is None, listing
    attachment = course.json("round", "source", "RC-1", "--round", "F2", "--source", "S1")["data"]
    assert base64.b64decode(attachment["base64"]) == b"hello\n" and attachment["sha256"] == hashlib.sha256(b"hello\n").hexdigest() and attachment["next_range"] is None
    assert base64.b64decode(course.json("round", "source", "RC-1", "--round", "F2", "--source", "S1", "--range", "1:4")["data"]["base64"]) == b"ell"
    course.refused("not_found", 2, "round", "source", "RC-1", "--round", "F2", "--source", "S2")
    course.refused("not_found", 2, "round", "source", "RC-1", "--round", "F2", "--source", "src/a.txt")
    course.refused("invalid_input", 2, "round", "source", "RC-1", "--round", "F2", "--source", "S1", "--range", "0:99")
    course.json("note", "RC-1", "--body-file", "-", stdin=b"A private note written after F2.\n")
    course.json("review", "RC-1", "--goal-file", "-", "--expected-revision", course.json("review", "RC-1")["data"]["draft_revision"], stdin=b"A draft goal nobody shared.\n")
    again = course.json("round", "read", "RC-1", "--round", "F2")["data"]
    assert again == read, "the round reads exactly as frozen"
    assert "private note" not in json.dumps(again) and "nobody shared" not in json.dumps(again)

    # A read grant cannot respond; a response grant appends a statement and nothing else.
    document = response_document("F2", f2, items=("I1", "I2"))
    assert course.refused("policy_denied", 5, "response", "append", "RC-1", "--round", "F2", "--file", "-", stdin=document)["detail"]["decision"] == "not_granted"
    assert course.json("round", "grant", "RC-1", "--round", "F2")["data"]["respond"] is True
    before = course.json("show", "RC-1")["data"]
    assert course.json("response", "append", "RC-1", "--round", "F2", "--file", "-", stdin=document)["data"]["appended"] is True
    after = course.json("show", "RC-1")["data"]
    statement = after["responses"][-1]
    assert statement["channel"] == "local_interface" and statement["foreign_id"] == "RESP-H1" and statement["round"] == "F2", statement
    assert after["notes"] == before["notes"] and after["rounds"] == before["rounds"], "no note, wording or round changed"
    course.refused("invalid_input", 2, "response", "append", "RC-1", "--round", "F2", "--file", "-", stdin=response_document("F2", f2, "RESP-H2", "verified"))
    course.refused("conflict", 4, "response", "append", "RC-1", "--round", "F2", "--file", "-", stdin=response_document("F2", round_digest(course, "RC-1", "F1"), "RESP-H3"))
    course.refused("policy_denied", 5, "response", "append", "RC-1", "--round", "F1", "--file", "-", stdin=response_document("F1", round_digest(course, "RC-1", "F1"), "RESP-H4"))

    # A harness that declares itself one is held to its grant: nothing else, including any write, export or grant.
    consumer = {"BPR_CYCLE_ROLE": "consumer"}
    assert course.json("capabilities", data_root=False, extra=consumer)["data"]["role"] == "consumer"
    assert course.json("round", "read", "RC-1", "--round", "F2", extra=consumer)["data"]["payload_sha256"] == f2
    for arguments in (["show", "RC-1"], ["list"], ["compare", "RC-1"], ["export", "RC-1", "--round", "F2", "--output", "-"], ["round", "grant", "RC-1", "--round", "F1"],
                      ["round", "revoke", "RC-1", "--round", "F2"], ["policy"], ["review", "RC-1"], ["freeze", "RC-1", "--expected-revision", "1"], ["capture", "RC-1"]):
        course.refused("policy_denied", 5, *arguments, extra=consumer)
    course.refused("policy_denied", 5, "note", "RC-1", "--body-file", "-", stdin=b"from a harness\n", extra=consumer)
    course.refused("policy_denied", 5, "response", "import", "RC-1", "--round", "F2", "--file", "-", stdin=document, extra=consumer)
    for missing in ("verify", "publish", "publication"):
        course.refused("unsupported_command", 2, missing, "RC-1")
    assert "from a harness\n" not in [note["body"] for note in course.json("show", "RC-1")["data"]["notes"]]

    # Every page, in order, bound to one digest; the consumer can check the counts before acting.
    assert course.json("round", "grant", "RC-2", "--round", "F1", "--read-only")["data"]["read"] is True
    rc2 = round_digest(course, "RC-2", "F1")
    first = course.json("round", "read", "RC-2", "--round", "F1", "--page-bytes", "4096")
    assert first["complete"] is False and first["data"]["next"] and first["data"]["items_total"] == "31", first
    items, page, pages = list(first["data"]["items"]), first, 1
    while page["data"]["next"]:
        cursor = page["data"]["next"]
        page = course.json("round", "page", "RC-2", "--round", "F1", "--cursor", cursor)
        assert page["data"]["payload_sha256"] == rc2 and page["data"]["first_item"] == str(len(items)), page["data"]
        items += page["data"]["items"]
        pages += 1
    assert page["complete"] is True and pages >= 3 and len(items) == 31 and [entry["id"] for entry in items] == [f"I{index}" for index in range(1, 32)], (pages, len(items))
    whole = course.json("round", "read", "RC-2", "--round", "F1")["data"]
    assert whole["items"] == items and whole["next"] is None, "the pages are the round"
    out, _ = course.human("round", "read", "RC-2", "--round", "F1", "--page-bytes", "4096")
    assert "Read every page before acting on any:" in out and "bpr cycle round page RC-2 --round F1 --cursor " in out
    cursor = first["data"]["next"]
    course.refused("invalid_input", 2, "round", "page", "RC-1", "--round", "F2", "--cursor", cursor)
    forged = cursor.split(".")
    forged[1] = f2
    course.refused("invalid_input", 2, "round", "page", "RC-2", "--round", "F1", "--cursor", ".".join(forged))
    course.refused("invalid_input", 2, "round", "page", "RC-2", "--round", "F1", "--cursor", "F1." + rc2 + ".31.4096")
    course.refused("invalid_input", 2, "round", "read", "RC-2", "--round", "F1", "--page-bytes", "100")
    # A consumer that cannot hold the whole instruction set is told so, and given none of it.
    partition = course.refused("requires_partition", 5, "round", "read", "RC-2", "--round", "F1", "--budget-bytes", "1000")
    assert int(partition["detail"]["instruction_bytes"]) > 1000 and partition["detail"]["items_total"] == "31", partition
    assert course.json("round", "read", "RC-2", "--round", "F1", "--budget-bytes", whole["instruction_bytes"])["complete"] is True

    # Revoked: nothing more is read, mid-pagination too, and nothing is appended.
    revoked = course.json("round", "revoke", "RC-2", "--round", "F1")["data"]
    assert revoked["revoked"] is True and revoked["changed"] is True
    assert course.refused("policy_denied", 5, "round", "page", "RC-2", "--round", "F1", "--cursor", cursor)["detail"]["decision"] == "revoked"
    assert course.refused("policy_denied", 5, "round", "read", "RC-2", "--round", "F1")["detail"]["decision"] == "revoked"
    course.refused("not_found", 2, "round", "revoke", "RC-2", "--round", "F2")
    assert course.json("show", "RC-2")["data"]["grants"] == [{"round": "F1", "read": True, "respond": False, "revoked": True}]

    # One policy for every interface: a denial holds for the grant's reads and the terminal's exports alike.
    course.json("policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", "denied")
    assert course.refused("policy_denied", 5, "round", "read", "RC-1", "--round", "F2")["detail"]["decision"] == "channel_denied"
    course.json("policy", "--classification", "unclassified", "--channel", "terminal_output", "--permission", "denied")
    status, printed, err = course.run("export", "RC-1", "--round", "F2", "--output", "-")
    assert status == 5 and printed == "" and "does not allow printing to the terminal" in err, (status, err)
    course.json("policy", "--classification", "unclassified", "--channel", "terminal_output", "--permission", "unconfigured")
    course.json("policy", "--classification", "unclassified", "--channel", "external_retrieval", "--permission", "allowed")
    assert course.json("round", "read", "RC-1", "--round", "F2")["data"]["payload_sha256"] == f2


@contextlib.contextmanager
def hold_lease(profile_root):
    """Holds the default profile's runtime lease as another process would: a lock
    on the profile root itself (POSIX), or its lock file opened without the
    exclusive sharing the lease needs (Windows)."""
    if os.name == "nt":
        with open(Path(profile_root) / "profile-runtime.lock", "rb"):
            yield
        return
    import fcntl
    holder = os.open(profile_root, os.O_RDONLY)
    try:
        fcntl.flock(holder, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield
    finally:
        os.close(holder)


def one_writer(course):
    """A profile one process owns is refused, never opened a second time."""
    course.refused("over_limit", 5, "note", "RC-1", "--body-file", "-", stdin=b"y" * 17000)
    review = course.json("review", "RC-1")["data"]
    for index in range(6):
        item = course.json("note", "RC-1", "--body-file", "-", stdin=(f"bulk {index} " + "x" * 15000).encode())["data"]["item"]
        review = course.json("review", "RC-1", "--include", item, "--expected-revision", review["draft_revision"])["data"]
    frozen = course.json("freeze", "RC-1", "--expected-revision", review["draft_revision"])["data"]
    status, printed, _ = course.run("export", "RC-1", "--round", frozen["round"], "--output", "-")
    assert status == 0 and len(printed.encode()) > 90000 and printed.startswith("#"), "the round prints whole"
    # Another process holds the profile's runtime lease and offers no command
    # channel, as a bpr command does: nothing is opened a second time (BPR-1168).
    with hold_lease(course.data):
        course.refused("capability_unavailable", 5, "show", "RC-1")
        course.refused("capability_unavailable", 5, "note", "RC-1", "--body-file", "-", stdin=b"second writer")
    bodies = [note["body"] for note in course.json("show", "RC-1")["data"]["notes"]]
    assert "second writer" not in bodies, "no second writer was opened"


def interactive(course, project, temporary):
    """A terminal may answer: the trusted editor and an exact freeze confirmation."""
    if os.name != "posix":
        print("cycle-cli-interactive:skipped (no pty on this platform)")
        return
    import pty
    import select as readiness

    def in_terminal(arguments, answer=None, extra=None):
        master, slave = pty.openpty()
        child = subprocess.Popen([course.binary, "cycle", *arguments, "--data-root", str(course.data)], stdin=slave, stdout=slave, stderr=slave,
                                 env=environment(extra), cwd=project)
        os.close(slave)
        output = bytearray()
        deadline = time.monotonic() + 60
        answered = answer is None
        try:
            while time.monotonic() < deadline:
                if not answered and b"Type freeze" in output:
                    os.write(master, answer)
                    answered = True
                if readiness.select([master], [], [], 0.05)[0]:
                    try:
                        part = os.read(master, 65536)
                    except OSError:
                        break
                    if not part:
                        break
                    output.extend(part)
                elif child.poll() is not None:
                    break
            status = child.wait(timeout=10)
        finally:
            os.close(master)
            if child.poll() is None:
                child.kill()
                child.wait()
        return status, output.decode("utf-8", "replace")

    editor = Path(temporary) / "editor.py"
    editor.write_text("import pathlib, sys\npathlib.Path(sys.argv[1]).write_text('Typed in the editor.\\n')\n")
    private_temporary = Path(temporary) / "editor-tmp"
    make_root_private(private_temporary)
    item = f"I{len(course.json('show', 'RC-1')['data']['notes']) + 1}"
    status, output = in_terminal(["note", "RC-1", "--file", "src/a.txt", "--lines", "1"],
                                 extra={"BPR_TOUR_EDITOR_ARGV": json.dumps([sys.executable, str(editor)]), "TMPDIR": str(private_temporary)})
    assert status == 0 and f"Noted {item} on src/a.txt:1 @R2" in output, output
    assert course.json("show", "RC-1")["data"]["notes"][-1]["body"] == "Typed in the editor.\n"
    status, output = in_terminal(["note", "RC-1"])
    assert status == 3 and "No trusted editor is configured" in output, output

    revision = course.json("review", "RC-1", "--include", item, "--expected-revision", course.json("review", "RC-1")["data"]["draft_revision"])["data"]
    rounds = len(course.json("show", "RC-1")["data"]["rounds"])
    status, output = in_terminal(["freeze", "RC-1"], answer=b"\n")
    assert status == 130 and "Nothing was frozen." in output, output
    assert len(course.json("show", "RC-1")["data"]["rounds"]) == rounds
    status, output = in_terminal(["freeze", "RC-1"], answer=b"freeze\n")
    assert status == 0 and f"Froze {revision['round']}" in output, output
    assert len(course.json("show", "RC-1")["data"]["rounds"]) == rounds + 1


def links_no_gui(binary):
    """The executable loads no GUI, widget or display library."""
    if sys.platform == "darwin":
        listed = subprocess.run(["otool", "-L", binary], capture_output=True, text=True, check=True).stdout
    elif sys.platform.startswith("linux"):
        listed = subprocess.run(["ldd", binary], capture_output=True, text=True, check=True).stdout
    else:
        print("cycle-cli-libraries:skipped (no dependency lister on this platform)")
        return
    for library in ("QtGui", "Qt6Gui", "QtWidgets", "Qt6Widgets", "libxcb", "libwayland", "AppKit"):
        assert library not in listed, f"{library} is linked"


def main():
    binary = str(Path(sys.argv[1]).resolve())
    links_no_gui(binary)
    with contained_temporary_directory(prefix="bpr-cycle-cli-") as raw:
        temporary = Path(raw).resolve()
        home = temporary / "course"
        make_root_private(home)
        project = temporary / "project"
        (project / "src").mkdir(parents=True)
        (project / "src" / "a.txt").write_bytes(b"one\ntwo\nthree\nfour\n")
        (project / "README.md").write_bytes(b"hello\n")
        exports = temporary / "exports"
        exports.mkdir()
        course = Course(binary, home / "data", home)
        contract_edges(course)
        opens_and_notes(course, project)
        printed = rounds_and_exports(course, project, exports)
        responses(course, printed)
        other = temporary / "other"
        other.mkdir()
        (other / "notes.txt").write_bytes(b"another project\n")
        retrieval(course, project, other)
        one_writer(course)
        interactive(course, project, temporary)
    print("cycle-cli-public-path:pass")


if __name__ == "__main__":
    main()
