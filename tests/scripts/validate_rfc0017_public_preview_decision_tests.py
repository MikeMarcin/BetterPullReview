#!/usr/bin/env python3
"""Hermetic tests for the RFC-BPR-0017 public-preview decision validator.

Nothing here touches a network, and `test_module_imports_no_network_client`
proves the module under test cannot: it parses the source and refuses any
import that could open a socket. The rest is the refusal table.

The fixture definitions below are deliberately not the ones the real packet
proposes. A validator that hard-coded the shipped wording — "Public preview",
the usage ring, the device-flow handoff — would pass against the real review
directory and fail here, which is the point.

Every case runs against a scratch repository built once and copied per test,
so a case that mutates the packet, the page, or the bound evidence cannot
reach the repository it is testing.
"""
from __future__ import annotations

import ast
import contextlib
import copy
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

MINIMUM_PYTHON = (3, 10)
if sys.version_info < MINIMUM_PYTHON:
    sys.stderr.write(
        "validate_rfc0017_public_preview_decision_tests.py requires Python "
        "{}.{} or newer; this is {}.{}.{} at {}.\n".format(
            MINIMUM_PYTHON[0], MINIMUM_PYTHON[1], sys.version_info[0],
            sys.version_info[1], sys.version_info[2], sys.executable))
    raise SystemExit(2)

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts import validate_rfc0017_public_preview_decision as VAL  # noqa: E402
from scripts.contained_temporary_support import ContainedTemporaryDirectory  # noqa: E402
from skip_report import SkipReportingTestRunner  # noqa: E402

BUILDER = (
    REPOSITORY_ROOT
    / "scripts/verification/rfc0016_attended_contract/scripts/build_attended_review.py")
GIT_AVAILABLE = shutil.which("git") is not None
BUILDER_AVAILABLE = BUILDER.is_file()

REVIEW_RELATIVE = (
    "docs/verification/reviews/rfc0017-public-preview-decision-v001")
PROBE_RELATIVE = "docs/verification/reviews/fixture-probe-v001/captures"
DECISION_RELATIVE = "docs/verification/reviews/fixture-decisions-v001"

REVIEW_GATE = "rfc0017_public_preview_definitions"
REVIEW_SCOPE = "public_preview_definition_binding_only"
SUBJECT_ID = "decision:/betterpullreview/bpr-0312/public-preview-definitions"
DOWNSTREAM_CLAIMS = {
    "rfc_0017_closeout_claimed": False,
    "implementation_cards_started": False,
    "credential_mutation_or_private_read_authorized": False,
    "platform_enablement_claimed": False,
    "release_gate_satisfied": False,
}

#: Fixture wording. Not the shipped answers, on purpose.
FIXTURE_DEFINITIONS = [
    {
        "key": "public_preview_name",
        "term": "Fixture name",
        "question": "What is the fixture mode called?",
        "proposed": "Fixture preview, with 'sample' stated beside it.",
    },
    {
        "key": "limit_presentation",
        "term": "Fixture limits",
        "question": "How is the fixture budget presented?",
        "proposed": "A fixture gauge that fills across 47 fixture units.",
    },
    {
        "key": "sign_in_handoff",
        "term": "Fixture handoff",
        "question": "How does the fixture hand off?",
        "proposed": "The fixture handoff returns to the fixture request.",
    },
    {
        "key": "supported_platform_policy",
        "term": "Fixture platforms",
        "question": "Which fixture platforms are supported?",
        "proposed": "Fixture platforms A and B; every other profile refuses.",
    },
]

PROBE_FILES = {
    "fixture-rate-limit.json": '{"fixture":"rate-limit","limit":47}\n',
    "fixture-graphql-refusal.json": '{"fixture":"graphql","limit":0}\n',
}
DECISION_FILES = {
    "fixture-human-verdict.json": '{"fixture":"verdict","chosen":"build"}\n',
}

SCRATCH_GIT_ENVIRONMENT = {
    "GIT_CONFIG_GLOBAL": os.devnull,
    "GIT_CONFIG_SYSTEM": os.devnull,
    "GIT_CONFIG_NOSYSTEM": "1",
    "GIT_AUTHOR_NAME": "BPR Fixture",
    "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
    "GIT_COMMITTER_NAME": "BPR Fixture",
    "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
    "GIT_AUTHOR_DATE": "2020-01-01T00:00:00+0000",
    "GIT_COMMITTER_DATE": "2020-01-01T00:00:00+0000",
}

#: A module that cannot import these cannot open a socket by accident.
FORBIDDEN_IMPORT_ROOTS = frozenset({
    "asyncio", "ftplib", "http", "httplib", "httpx", "imaplib", "poplib",
    "requests", "smtplib", "socket", "socketserver", "ssl", "telnetlib",
    "urllib", "urllib3", "xmlrpc",
})


def run_scratch_git(arguments: list[str], *, cwd: Path) -> bytes:
    environment = dict(os.environ)
    environment.update(SCRATCH_GIT_ENVIRONMENT)
    completed = subprocess.run(
        ["git", "-c", "commit.gpgsign=false", "-c", "core.hooksPath=",
         *arguments],
        cwd=str(cwd), capture_output=True, env=environment, check=False)
    if completed.returncode != 0:
        raise AssertionError(
            f"scratch git {arguments} failed: "
            f"{completed.stderr.decode('utf-8', 'replace')}")
    return completed.stdout


def digest_of(path: Path) -> tuple[str, int]:
    payload = path.read_bytes()
    return f"sha256:{hashlib.sha256(payload).hexdigest()}", len(payload)


def review_input(repository: Path) -> dict:
    return {
        "schema_name": "bpr.attended_review_batch_input",
        "schema_version": 1,
        "project_id": "betterpullreview",
        "title": "Fixture public-preview definitions",
        "review_gate": REVIEW_GATE,
        "review_scope": REVIEW_SCOPE,
        "downstream_claims": dict(DOWNSTREAM_CLAIMS),
        "subjects": [
            {
                "subject_id": SUBJECT_ID,
                "subject_key": "bpr-0312-public-preview-definitions",
                "display_name": "Bind the four fixture definitions",
                "description": "The fixture build decision is already taken.",
                "acceptance_criteria": ["The four definitions bind."],
                "known_concerns": ["Fixture only."],
                "audit_verdict": {"status": "pass", "note": "Fixture audit."},
                "media_groups": [],
                "detail_sections": [
                    {
                        "heading": "Proposed definitions to bind",
                        "kind": "definitions",
                        "items": [
                            {"term": entry["term"],
                             "definition": entry["proposed"]}
                            for entry in FIXTURE_DEFINITIONS
                        ],
                    },
                ],
                "identity_files": [
                    {
                        "label": name.removesuffix(".json"),
                        "path": os.path.relpath(
                            repository / PROBE_RELATIVE / name,
                            repository / REVIEW_RELATIVE),
                    }
                    for name in sorted(PROBE_FILES)
                ],
            },
        ],
    }


def packet_definitions_block(manifest: dict, repository: Path) -> dict:
    subject = manifest["subjects"][0]
    probe = []
    for name in sorted(PROBE_FILES):
        sha, size = digest_of(repository / PROBE_RELATIVE / name)
        probe.append({
            "path": f"{PROBE_RELATIVE}/{name}",
            "sha256": sha,
            "byte_length": size,
        })
    decision = []
    for name in sorted(DECISION_FILES):
        sha, size = digest_of(repository / DECISION_RELATIVE / name)
        decision.append({
            "path": f"{DECISION_RELATIVE}/{name}",
            "sha256": sha,
            "byte_length": size,
        })
    return {
        "schema_name": VAL.DEFINITIONS_SCHEMA,
        "schema_version": 1,
        "decision": "build",
        "batch_id": manifest["batch_id"],
        "subject_id": subject["subject_id"],
        "subject_digest": subject["subject_digest"],
        "definitions": copy.deepcopy(FIXTURE_DEFINITIONS),
        "probe_evidence": probe,
        "decision_evidence": decision,
    }


def packet_text(block: dict) -> str:
    return (
        "# Fixture public-preview definitions operator packet\n\n"
        "Status: **pending owner verdict; not RFC closeout**\n\n"
        "## The bound block\n\n"
        "```json\n"
        + json.dumps(block, ensure_ascii=False, indent=2)
        + "\n```\n"
    )


def build_template(destination: Path) -> None:
    """One scratch repository, built once and copied per test."""
    review = destination / REVIEW_RELATIVE
    review.mkdir(parents=True)
    probe = destination / PROBE_RELATIVE
    probe.mkdir(parents=True)
    for name, payload in PROBE_FILES.items():
        (probe / name).write_text(payload, encoding="utf-8")
    decision = destination / DECISION_RELATIVE
    decision.mkdir(parents=True)
    for name, payload in DECISION_FILES.items():
        (decision / name).write_text(payload, encoding="utf-8")

    input_path = review / "offline-review-input.json"
    input_path.write_text(
        json.dumps(review_input(destination), ensure_ascii=False, indent=2)
        + "\n",
        encoding="utf-8")
    completed = subprocess.run(
        [sys.executable, str(BUILDER), "build", "--input", str(input_path),
         "--out", str(review / "offline-package")],
        capture_output=True, text=True, check=False)
    if completed.returncode != 0:
        raise AssertionError(
            f"fixture package build failed: {completed.stderr}")

    manifest = json.loads(
        (review / "offline-package/attended-review-manifest.json").read_text(
            encoding="utf-8"))
    block = packet_definitions_block(manifest, destination)
    (review / "operator-packet.md").write_text(
        packet_text(block), encoding="utf-8")

    run_scratch_git(["init", "--quiet"], cwd=destination)
    run_scratch_git(["add", "-A"], cwd=destination)
    run_scratch_git(
        ["commit", "--quiet", "-m", "fixture review directory"],
        cwd=destination)


_TEMPLATE: ContainedTemporaryDirectory | None = None
_TEMPLATE_ROOT: Path | None = None


def template_root() -> Path:
    global _TEMPLATE, _TEMPLATE_ROOT
    if _TEMPLATE_ROOT is None:
        _TEMPLATE = ContainedTemporaryDirectory()
        _TEMPLATE_ROOT = Path(_TEMPLATE.name) / "template"
        build_template(_TEMPLATE_ROOT)
    return _TEMPLATE_ROOT


def tree_state(root: Path) -> dict[str, str]:
    state: dict[str, str] = {}
    for path in sorted(root.rglob("*")):
        if ".git" in path.parts or not path.is_file():
            continue
        state[str(path.relative_to(root))] = hashlib.sha256(
            path.read_bytes()).hexdigest()
    return state


@unittest.skipUnless(GIT_AVAILABLE, "git is required for commit binding")
@unittest.skipUnless(BUILDER_AVAILABLE, "the attended-review builder is required")
class DecisionValidatorTests(unittest.TestCase):
    """One scratch repository per case, so a mutation cannot leak."""

    def setUp(self) -> None:
        self.temporary = ContainedTemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.repository = Path(self.temporary.name) / "repository"
        shutil.copytree(template_root(), self.repository)
        self.review = self.repository / REVIEW_RELATIVE
        self.packet = self.review / "operator-packet.md"
        self.response = self.review / "human-verdict.json"
        self.output = self.review / "validation-record.json"
        self.manifest_path = (
            self.review / "offline-package/attended-review-manifest.json")
        manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"))
        self.batch_id = manifest["batch_id"]
        self.subject_digest = manifest["subjects"][0]["subject_digest"]
        self.identity_key = f"{SUBJECT_ID}@{self.subject_digest}"

    # -- fixtures ---------------------------------------------------------

    def identity(self) -> dict:
        return {"subject_id": SUBJECT_ID, "subject_digest": self.subject_digest}

    def verdict(self, *, note=None, decision="accept", **overrides) -> dict:
        document = {
            "schema_name": VAL.VERDICT_SCHEMA,
            "schema_version": 1,
            "batch_id": self.batch_id,
            "review_gate": REVIEW_GATE,
            "review_scope": REVIEW_SCOPE,
            "accepted_subjects":
                [self.identity()] if decision == "accept" else [],
            "rejected_subjects":
                [self.identity()] if decision == "reject" else [],
            "notes_by_subject_identity": {},
            "downstream_claims": dict(DOWNSTREAM_CLAIMS),
        }
        if decision == "not_chosen":
            document["not_chosen_subjects"] = [self.identity()]
        if note is not None:
            document["notes_by_subject_identity"][self.identity_key] = note
        document.update(overrides)
        return document

    def write_response(self, document) -> None:
        if isinstance(document, (bytes, str)):
            payload = (
                document if isinstance(document, bytes)
                else document.encode("utf-8"))
        else:
            payload = json.dumps(
                document, ensure_ascii=False, indent=2).encode("utf-8") + b"\n"
        self.response.write_bytes(payload)

    def rewrite_packet(self, mutate) -> None:
        text = self.packet.read_text(encoding="utf-8")
        block = json.loads(
            VAL.FENCED_JSON_BLOCK.search(text).group(1))
        mutate(block)
        self.packet.write_text(packet_text(block), encoding="utf-8")

    def rewrite_manifest(self, mutate) -> None:
        manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"))
        mutate(manifest)
        self.manifest_path.write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8")

    def rewrite_record(self, mutate) -> None:
        record = json.loads(self.output.read_text(encoding="utf-8"))
        mutate(record)
        self.output.write_text(
            json.dumps(record, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8")

    # -- invocation -------------------------------------------------------

    def invoke(self, *extra: str, require: str = "build") -> tuple[int, str, str]:
        argv = [
            "--packet", str(self.packet),
            "--response", str(self.response),
            "--require", require,
            "--output", str(self.output),
            *extra,
        ]
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = VAL.main(argv)
        return code, out.getvalue(), err.getvalue()

    def accept(self, *extra: str, require: str = "build") -> str:
        code, out, err = self.invoke(*extra, require=require)
        self.assertEqual(code, 0, err)
        return out

    def refuse(self, fragment: str, *extra: str, require: str = "build") -> None:
        code, _out, err = self.invoke(*extra, require=require)
        self.assertEqual(code, 3, f"expected a refusal, got {err!r}")
        self.assertIn("RFC0017_PUBLIC_PREVIEW_DECISION_REFUSED:", err)
        self.assertIn(fragment, err)
        self.assertFalse(
            self.output.exists(), "a refusal must leave no record behind")

    def record(self) -> dict:
        return json.loads(self.output.read_text(encoding="utf-8"))

    # -- accepting paths --------------------------------------------------

    def test_empty_note_binds_every_packet_definition(self) -> None:
        self.write_response(self.verdict())
        out = self.accept()
        self.assertIn("RFC0017_PUBLIC_PREVIEW_DECISION_RECORDED:build", out)
        self.assertIn("RFC0017_PUBLIC_PREVIEW_DEFINITIONS_BOUND:4", out)
        self.assertIn("RFC0017_PUBLIC_PREVIEW_EVIDENCE_BOUND:3", out)
        record = self.record()
        self.assertEqual(
            [entry["key"] for entry in record["definitions"]],
            list(VAL.DEFINITION_KEYS))
        for entry, fixture in zip(record["definitions"], FIXTURE_DEFINITIONS):
            self.assertEqual(entry["bound"], fixture["proposed"])
            self.assertEqual(entry["source"], "packet")
        self.assertEqual(record["response_note"], "")

    def test_as_proposed_note_binds_every_packet_definition(self) -> None:
        self.write_response(self.verdict(note="As Proposed\n"))
        self.accept()
        record = self.record()
        self.assertTrue(
            all(entry["source"] == "packet"
                for entry in record["definitions"]))
        self.assertEqual(record["response_note"], "As Proposed\n")

    def test_note_replaces_one_definition_and_leaves_the_rest(self) -> None:
        self.write_response(self.verdict(
            note="limit_presentation: A replacement gauge with a reset time."))
        self.accept()
        bound = {entry["key"]: entry for entry in self.record()["definitions"]}
        self.assertEqual(
            bound["limit_presentation"]["bound"],
            "A replacement gauge with a reset time.")
        self.assertEqual(bound["limit_presentation"]["source"], "response_note")
        for key in ("public_preview_name", "sign_in_handoff",
                    "supported_platform_policy"):
            self.assertEqual(bound[key]["source"], "packet")

    def test_note_may_replace_all_four_definitions(self) -> None:
        note = "\n".join(
            f"{key}: replacement {index}"
            for index, key in enumerate(VAL.DEFINITION_KEYS))
        self.write_response(self.verdict(note=note))
        self.accept()
        for entry in self.record()["definitions"]:
            self.assertEqual(entry["source"], "response_note")
            self.assertTrue(entry["bound"].startswith("replacement "))

    def test_a_replacement_may_wrap_over_several_lines(self) -> None:
        self.write_response(self.verdict(
            note="sign_in_handoff: first line\n  and a wrapped second line\n"))
        self.accept()
        bound = {entry["key"]: entry for entry in self.record()["definitions"]}
        self.assertEqual(
            bound["sign_in_handoff"]["bound"],
            "first line\n  and a wrapped second line")

    def test_the_record_binds_packet_response_evidence_and_commit(self) -> None:
        self.write_response(self.verdict())
        self.accept()
        record = self.record()
        packet_sha, packet_size = digest_of(self.packet)
        response_sha, response_size = digest_of(self.response)
        self.assertEqual(record["reviewed_packet"]["sha256"], packet_sha)
        self.assertEqual(record["reviewed_packet"]["byte_length"], packet_size)
        self.assertEqual(record["response"]["sha256"], response_sha)
        self.assertEqual(record["response"]["byte_length"], response_size)
        self.assertEqual(record["batch_id"], self.batch_id)
        self.assertEqual(record["subject_digest"], self.subject_digest)
        self.assertEqual(
            record["validated_commit"],
            run_scratch_git(
                ["rev-parse", "HEAD"], cwd=self.repository).decode().strip())
        self.assertEqual(len(record["probe_evidence"]), len(PROBE_FILES))
        self.assertEqual(len(record["decision_evidence"]), len(DECISION_FILES))
        self.assertEqual(record["claims"][
            "bpr_0312_positive_build_gate_satisfied"], True)
        self.assertEqual(record["claims"]["rfc_0017_closeout_claimed"], False)
        self.assertIn(
            "--require build", record["validation_command"])
        self.assertIn(
            f"{REVIEW_RELATIVE}/operator-packet.md",
            record["validation_command"])

    def test_validate_existing_revalidates_and_writes_nothing(self) -> None:
        self.write_response(self.verdict(note="as proposed"))
        self.accept()
        before = tree_state(self.repository)
        code, out, err = self.invoke("--validate-existing")
        self.assertEqual(code, 0, err)
        self.assertIn("RFC0017_PUBLIC_PREVIEW_DECISION_VALID:build", out)
        self.assertIn("RFC0017_PUBLIC_PREVIEW_DEFINITIONS_BOUND:4", out)
        self.assertEqual(tree_state(self.repository), before)

    # -- interface refusals ----------------------------------------------

    def test_require_must_name_the_build_branch(self) -> None:
        self.write_response(self.verdict())
        self.refuse("--require must be 'build'", require="do-not-build")

    def test_a_second_write_over_an_existing_record_is_refused(self) -> None:
        self.write_response(self.verdict())
        self.accept()
        code, _out, err = self.invoke()
        self.assertEqual(code, 3)
        self.assertIn("already exists", err)

    def test_validate_existing_without_a_record_is_refused(self) -> None:
        self.write_response(self.verdict())
        self.refuse("validation record is not a regular file",
                    "--validate-existing")

    # -- response refusals ------------------------------------------------

    def test_a_prose_question_is_not_a_verdict(self) -> None:
        self.write_response(
            "Can you say what the ring does when the budget resets?\n")
        self.refuse("response is not JSON")

    def test_another_schema_is_refused(self) -> None:
        document = self.verdict()
        document["schema_name"] = "bpr.attended_review_batch"
        self.write_response(document)
        self.refuse("response is not a bpr.attended_review_verdict")

    def test_a_partial_verdict_is_refused(self) -> None:
        document = self.verdict()
        del document["downstream_claims"]
        self.write_response(document)
        self.refuse("response is a partial verdict")

    def test_an_unsupported_top_level_field_is_refused(self) -> None:
        self.write_response(self.verdict(reviewer_comment="looks fine"))
        self.refuse("unsupported fields")

    def test_a_verdict_for_another_batch_is_refused(self) -> None:
        self.write_response(self.verdict(
            batch_id="bpr:/attended-review/betterpullreview/"
                     "000000000000000000000000"))
        self.refuse("does not match the review batch")

    def test_a_stale_subject_digest_is_refused(self) -> None:
        stale = {"subject_id": SUBJECT_ID, "subject_digest": f"sha256:{'0' * 64}"}
        self.write_response(self.verdict(accepted_subjects=[stale]))
        self.refuse("does not decide exactly this packet's subject")

    def test_a_rejection_is_refused(self) -> None:
        self.write_response(self.verdict(decision="reject"))
        self.refuse("the owner rejected the public-preview definitions")

    def test_an_undecided_subject_is_refused(self) -> None:
        self.write_response(self.verdict(decision="none"))
        self.refuse("does not decide exactly this packet's subject")

    def test_a_not_chosen_subject_is_refused(self) -> None:
        self.write_response(self.verdict(decision="not_chosen"))
        self.refuse("records this subject as not chosen")

    def test_a_duplicated_decision_is_refused(self) -> None:
        document = self.verdict()
        document["rejected_subjects"] = [self.identity()]
        self.write_response(document)
        self.refuse("decides a subject more than once")

    def test_altered_downstream_claims_are_refused(self) -> None:
        claims = dict(DOWNSTREAM_CLAIMS)
        claims["rfc_0017_closeout_claimed"] = True
        self.write_response(self.verdict(downstream_claims=claims))
        self.refuse("downstream_claims differ")

    def test_a_note_against_another_identity_is_refused(self) -> None:
        document = self.verdict()
        document["notes_by_subject_identity"] = {"someone-else@x": "hello"}
        self.write_response(document)
        self.refuse("invalid note entry")

    # -- note refusals ----------------------------------------------------

    def test_a_prose_note_is_refused(self) -> None:
        self.write_response(self.verdict(
            note="Looks good, but what happens at the reset boundary?"))
        self.refuse("note is not a definition assignment")

    def test_an_unknown_definition_key_is_refused(self) -> None:
        self.write_response(self.verdict(
            note="preview_colour: teal"))
        self.refuse("names an unknown definition 'preview_colour'")

    def test_a_repeated_definition_key_is_refused(self) -> None:
        self.write_response(self.verdict(
            note="sign_in_handoff: one\nsign_in_handoff: two"))
        self.refuse("repeats definition 'sign_in_handoff'")

    def test_a_named_but_empty_definition_is_refused(self) -> None:
        self.write_response(self.verdict(
            note="public_preview_name:\n\nlimit_presentation: a gauge"))
        self.refuse("leaves it empty")

    def test_a_do_not_build_note_is_refused(self) -> None:
        self.write_response(self.verdict(
            note="public_preview_name: none, do not build it after all"))
        self.refuse("carries a 'do not build' verdict")

    def test_a_do_not_build_note_is_refused_before_the_grammar(self) -> None:
        self.write_response(self.verdict(note="Do Not Build."))
        self.refuse("carries a 'do not build' verdict")

    # -- packet and page refusals ----------------------------------------

    def test_a_packet_without_the_definitions_block_is_refused(self) -> None:
        self.packet.write_text(
            "# Fixture packet\n\nStatus: **pending**\n", encoding="utf-8")
        self.write_response(self.verdict())
        self.refuse("exactly one public-preview definitions block")

    def test_a_packet_missing_one_definition_is_refused(self) -> None:
        self.rewrite_packet(
            lambda block: block["definitions"].pop())
        self.write_response(self.verdict())
        self.refuse("must propose exactly 4 definitions")

    def test_a_packet_recording_do_not_build_is_refused(self) -> None:
        self.rewrite_packet(
            lambda block: block.update({"decision": "do-not-build"}))
        self.write_response(self.verdict())
        self.refuse("this gate validates only the 'build' branch")

    def test_packet_text_that_drifted_from_the_page_is_refused(self) -> None:
        def mutate(block: dict) -> None:
            block["definitions"][1]["proposed"] = "A quietly different gauge."
        self.rewrite_packet(mutate)
        self.write_response(self.verdict())
        self.refuse("four proposed definitions verbatim")

    def test_a_page_with_two_subjects_is_refused(self) -> None:
        self.rewrite_manifest(
            lambda manifest: manifest["subjects"].append(
                copy.deepcopy(manifest["subjects"][0])))
        self.write_response(self.verdict())
        self.refuse("exactly one subject")

    def test_a_failed_mechanical_audit_cannot_be_accepted(self) -> None:
        def mutate(manifest: dict) -> None:
            manifest["subjects"][0]["audit_verdict"]["status"] = "fail"
        self.rewrite_manifest(mutate)
        self.write_response(self.verdict())
        self.refuse("failed its mechanical audit")

    def test_a_missing_review_page_is_refused(self) -> None:
        self.manifest_path.unlink()
        self.write_response(self.verdict())
        self.refuse("review manifest is not a regular file")

    def test_tampered_probe_evidence_is_refused(self) -> None:
        target = self.repository / PROBE_RELATIVE / "fixture-rate-limit.json"
        target.write_text('{"fixture":"rate-limit","limit":60}\n',
                          encoding="utf-8")
        self.write_response(self.verdict())
        self.refuse("probe evidence")

    def test_tampered_decision_evidence_is_refused(self) -> None:
        target = (
            self.repository / DECISION_RELATIVE / "fixture-human-verdict.json")
        target.write_text('{"fixture":"verdict","chosen":"skip"}\n',
                          encoding="utf-8")
        self.write_response(self.verdict())
        self.refuse("decision evidence")

    def test_missing_bound_evidence_is_refused(self) -> None:
        (self.repository / PROBE_RELATIVE / "fixture-rate-limit.json").unlink()
        self.write_response(self.verdict())
        self.refuse("is not a regular file")

    # -- record refusals --------------------------------------------------

    def test_validate_existing_refuses_a_rewritten_definition(self) -> None:
        self.write_response(self.verdict())
        self.accept()

        def mutate(record: dict) -> None:
            record["definitions"][0]["bound"] = "Something the owner never saw."
        self.rewrite_record(mutate)
        code, _out, err = self.invoke("--validate-existing")
        self.assertEqual(code, 3)
        self.assertIn("no longer matches", err)
        self.assertIn("definitions", err)

    def test_validate_existing_refuses_a_replaced_response(self) -> None:
        self.write_response(self.verdict())
        self.accept()
        self.write_response(self.verdict(note="sign_in_handoff: replaced"))
        code, _out, err = self.invoke("--validate-existing")
        self.assertEqual(code, 3)
        self.assertIn("no longer matches", err)

    def test_validate_existing_refuses_an_unknown_commit(self) -> None:
        self.write_response(self.verdict())
        self.accept()
        self.rewrite_record(
            lambda record: record.update({"validated_commit": "0" * 40}))
        code, _out, err = self.invoke("--validate-existing")
        self.assertEqual(code, 3)
        self.assertIn("validated_commit", err)

    def test_validate_existing_writes_nothing_when_it_refuses(self) -> None:
        self.write_response(self.verdict())
        self.accept()
        before = tree_state(self.repository)
        self.write_response(self.verdict(decision="reject"))
        after_response = tree_state(self.repository)
        code, _out, _err = self.invoke("--validate-existing")
        self.assertEqual(code, 3)
        self.assertEqual(tree_state(self.repository), after_response)
        self.assertNotEqual(after_response, before)


class ModuleShapeTests(unittest.TestCase):
    """The gate that would notice the module growing a network client."""

    def test_module_imports_no_network_client(self) -> None:
        tree = ast.parse(
            Path(VAL.__file__).read_text(encoding="utf-8"),
            filename=VAL.__file__)
        imported: set[str] = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                imported.update(
                    alias.name.split(".")[0] for alias in node.names)
            elif isinstance(node, ast.ImportFrom) and node.module:
                imported.add(node.module.split(".")[0])
        offending = sorted(imported & FORBIDDEN_IMPORT_ROOTS)
        self.assertEqual(
            offending, [],
            "the decision validator must reach no network: "
            f"{offending}")

    def test_the_four_definition_keys_are_the_ones_bpr_0299_owed(self) -> None:
        self.assertEqual(
            VAL.DEFINITION_KEYS,
            (
                "public_preview_name",
                "limit_presentation",
                "sign_in_handoff",
                "supported_platform_policy",
            ))


if __name__ == "__main__":
    unittest.main(verbosity=2, testRunner=SkipReportingTestRunner)
