#!/usr/bin/env python3
"""Fixture-only tests for the offline attended review package."""

from __future__ import annotations

import copy
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
import build_attended_review as review  # noqa: E402


PNG_A = b"\x89PNG\r\n\x1a\nfixture-capture-a"
PNG_B = b"\x89PNG\r\n\x1a\nfixture-mockup-b"
WAV_A = b"RIFF\x24\x00\x00\x00WAVEfixture-audio"


class AttendedReviewPackageTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        (self.root / "capture.png").write_bytes(PNG_A)
        (self.root / "mockup.png").write_bytes(PNG_B)
        (self.root / "loop.wav").write_bytes(WAV_A)
        (self.root / "manifest.json").write_bytes(b'{"fixture": true}')
        self.source = {
            "schema_name": review.INPUT_SCHEMA,
            "schema_version": 1,
            "project_id": "fixture-project",
            "title": "Fixture <script>alert(1)</script> Review",
            "review_gate": "outliner_visual_acceptance",
            "review_scope": "human_ui_criterion_isolation_only",
            "downstream_claims": {
                "rfc_closeout_claimed": False,
                "runtime_promotion_approved": False,
            },
            "reference_disclaimer": "Mockups are reference only; RFC text wins.",
            "subjects": [
                {
                    "subject_id": "criterion:/fixture/ac-01",
                    "subject_key": "ac-01-connectors",
                    "display_name": "AC-01 connector distinction",
                    "description": "Connector styles distinguish occurrence kinds.",
                    "acceptance_criteria": ["Distinguishable in every capture"],
                    "known_concerns": ["Hue proximity at 100%"],
                    "audit_verdict": {"status": "pass", "note": "Offscreen course passed."},
                    "media_groups": [
                        {
                            "group_id": "state-captures",
                            "title": "State captures",
                            "items": [{"label": "Dark 100", "path": "capture.png"}],
                        },
                        {
                            "group_id": "mockup-compare",
                            "title": "Mockup vs capture",
                            "required": False,
                            "layout": "compare",
                            "items": [
                                {"label": "Mockup", "path": "mockup.png"},
                                {"label": "Capture", "path": "capture.png"},
                            ],
                        },
                    ],
                    "detail_sections": [
                        {
                            "heading": "Provenance",
                            "kind": "definitions",
                            "items": [{"term": "Command", "definition": "bpr-capture --evidence"}],
                        },
                        {"heading": "States", "kind": "list", "items": ["cycle", "invalid"]},
                        {
                            "heading": "Manifest excerpt",
                            "kind": "code",
                            "items": ["{", "  \"sha256\": \"<script>x</script>\"   ", "}"],
                        },
                    ],
                    "identity_files": [{"label": "Capture manifest", "path": "manifest.json"}],
                },
                {
                    "subject_id": "candidate:/fixture/audio-a",
                    "subject_key": "audio-a",
                    "display_name": "Audio candidate A",
                    "ab_group": "family-1",
                    "media_groups": [
                        {
                            "group_id": "audition",
                            "title": "Audition",
                            "items": [{"label": "Loop", "path": "loop.wav"}],
                        }
                    ],
                },
            ],
        }
        self.input_path = self.root / "batch.json"
        self.write_source()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_source(self) -> None:
        self.input_path.write_text(json.dumps(self.source), encoding="utf-8")

    def reset(self) -> None:
        self.tearDown()
        self.setUp()

    def build(self, name: str):
        return review.build(self.input_path, self.root / name)

    def complete_verdict(self, manifest, accept_indices):
        accepted = []
        rejected = []
        for index, subject in enumerate(manifest["subjects"]):
            identity = {
                "subject_id": subject["subject_id"],
                "subject_digest": subject["subject_digest"],
            }
            (accepted if index in accept_indices else rejected).append(identity)
        return {
            "schema_name": review.VERDICT_SCHEMA,
            "schema_version": 1,
            "batch_id": manifest["batch_id"],
            "review_gate": manifest["review_gate"],
            "review_scope": manifest["review_scope"],
            "accepted_subjects": accepted,
            "rejected_subjects": rejected,
            "notes_by_subject_identity": {},
            "downstream_claims": copy.deepcopy(manifest["downstream_claims"]),
        }

    def test_build_is_deterministic_and_path_local(self) -> None:
        first = self.root / "out-a"
        second = self.root / "out-b"
        manifest = review.build(self.input_path, first)
        review.build(self.input_path, second)

        first_files = sorted(path.relative_to(first) for path in first.rglob("*") if path.is_file())
        second_files = sorted(path.relative_to(second) for path in second.rglob("*") if path.is_file())
        self.assertEqual(first_files, second_files)
        for relative in first_files:
            self.assertEqual((first / relative).read_bytes(), (second / relative).read_bytes())

        page = (first / "index.html").read_text(encoding="utf-8")
        manifest_text = (first / "attended-review-manifest.json").read_text(encoding="utf-8")
        self.assertNotIn(str(self.root), page)
        self.assertNotIn(str(self.root), manifest_text)
        self.assertNotIn("<script>alert(1)</script>", page)
        self.assertIn("Fixture &lt;script&gt;alert(1)&lt;/script&gt; Review", page)
        self.assertIn("Copy the complete verdict", page)
        self.assertIn('class="review-controls" aria-label="Review progress and export"', page)
        self.assertIn('id="review-status" role="status" aria-live="polite" aria-atomic="true"', page)
        self.assertIn("position: sticky", page)
        self.assertIn('<audio controls preload="metadata"', page)
        self.assertIn('class="media-grid compare"', page)
        self.assertIn('data-ab-group="family-1"', page)
        self.assertIn("Mockups are reference only; RFC text wins.", page)
        self.assertIn("rfc_closeout_claimed, runtime_promotion_approved", page)
        self.assertFalse(any(line and not line.strip() for line in page.splitlines()))

        self.assertEqual(manifest["review_scope"], "human_ui_criterion_isolation_only")
        self.assertEqual(
            manifest["downstream_claims"],
            {"rfc_closeout_claimed": False, "runtime_promotion_approved": False},
        )
        self.assertTrue(manifest["batch_id"].startswith("bpr:/attended-review/fixture-project/"))
        subject = manifest["subjects"][0]
        self.assertTrue(subject["subject_digest"].startswith("sha256:"))
        self.assertEqual((first / subject["media_groups"][0]["items"][0]["path"]).read_bytes(), PNG_A)
        self.assertEqual(manifest["subjects"][1]["media_groups"][0]["items"][0]["media_kind"], "audio")

    def test_rejects_structural_errors(self) -> None:
        duplicated = copy.deepcopy(self.source)
        duplicated["subjects"].append(copy.deepcopy(duplicated["subjects"][0]))
        self.source = duplicated
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "subject_id values must be unique"):
            self.build("dup-subject")

        self.reset()
        self.source["subjects"][0]["media_groups"][0]["items"] = []
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "must contain at least one item"):
            self.build("empty-required")

        self.reset()
        self.source["subjects"][0]["media_groups"][1]["group_id"] = "state-captures"
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "group_id must be unique"):
            self.build("dup-group")

        self.reset()
        self.source["subjects"][0]["media_groups"][1]["items"] = [
            {"label": "Only one", "path": "mockup.png"}
        ]
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "compare requires at least two"):
            self.build("compare-one")

        self.reset()
        self.source["subjects"][1]["media_groups"][0]["required"] = False
        self.source["subjects"][1]["media_groups"][0]["items"] = []
        self.write_source()
        with self.assertRaisesRegex(
            review.ReviewError, "retain at least one non-empty media group or identity file"
        ):
            self.build("all-empty")

        self.reset()
        self.source["subjects"][0]["media_groups"][0]["items"] = [
            {"label": "Raw", "path": "manifest.json"}
        ]
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "previewable extension"):
            self.build("bad-ext")

    def test_identity_only_subject_builds_and_validates(self) -> None:
        self.source["subjects"] = [
            {
                "subject_id": "authority:/fixture/document-only",
                "subject_key": "document-only-authority",
                "display_name": "Approve the exact authority subject",
                "description": "One bounded decision over identity-bound text evidence.",
                "acceptance_criteria": ["Every declared fact is accepted as one decision"],
                "known_concerns": ["No visual or release approval is implied"],
                "detail_sections": [
                    {
                        "heading": "Decision facts",
                        "kind": "list",
                        "items": ["Exact scope", "Exact exclusions"],
                    }
                ],
                "identity_files": [
                    {"label": "Authority subject", "path": "manifest.json"}
                ],
            }
        ]
        self.write_source()

        manifest = self.build("identity-only")
        subject = manifest["subjects"][0]
        self.assertEqual(subject["media_groups"], [])
        self.assertEqual(len(subject["identity_files"]), 1)
        page = (self.root / "identity-only" / "index.html").read_text(encoding="utf-8")
        self.assertIn("Approve the exact authority subject", page)
        self.assertIn("Identity-bound files", page)
        self.assertNotIn('<section class="media-group">', page)

        verdict = self.complete_verdict(manifest, accept_indices={0})
        review.validate_verdict(manifest, verdict)

    def test_claims_and_tokens(self) -> None:
        self.source["downstream_claims"]["runtime_promotion_approved"] = True
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "must be false"):
            self.build("true-claim")

        self.reset()
        self.source["downstream_claims"] = {}
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "non-empty object"):
            self.build("no-claims")

        self.reset()
        self.source["review_gate"] = "Bad Gate!"
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "review_gate must match"):
            self.build("bad-gate")

        self.reset()
        self.source["downstream_claims"] = {"Bad Claim": False}
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "downstream_claims key"):
            self.build("bad-claim-key")

    def test_optional_empty_group_digest_equivalence(self) -> None:
        with_empty = copy.deepcopy(self.source)
        with_empty["subjects"][1]["media_groups"].append(
            {"group_id": "extras", "title": "Extras", "required": False, "items": []}
        )
        self.source = with_empty
        self.write_source()
        first = self.build("with-empty")

        self.reset()
        second = self.build("without-empty")
        self.assertEqual(first["subjects"][1]["subject_digest"], second["subjects"][1]["subject_digest"])
        self.assertEqual(first["batch_id"], second["batch_id"])
        self.assertEqual(len(first["subjects"][1]["media_groups"]), 1)

    def test_reference_disclaimer_is_identity_bound(self) -> None:
        first = self.build("disclaimer-a")
        self.reset()
        self.source["reference_disclaimer"] = "Different framing."
        self.write_source()
        second = self.build("disclaimer-b")
        self.assertNotEqual(first["batch_id"], second["batch_id"])

    def test_detail_sections_render_escaped(self) -> None:
        manifest = self.build("details")
        page = (self.root / "details" / "index.html").read_text(encoding="utf-8")
        self.assertIn("<dt>Command</dt>", page)
        self.assertIn("&quot;sha256&quot;: &quot;&lt;script&gt;x&lt;/script&gt;&quot;", page)
        self.assertNotIn("<script>x</script>", page)
        code_section = manifest["subjects"][0]["detail_sections"][2]
        self.assertEqual(code_section["items"][1], '  "sha256": "<script>x</script>"')
        self.assertFalse(any(line and not line.strip() for line in page.splitlines()))

    def test_audit_fail_disables_accept_and_blocks_verdict(self) -> None:
        self.source["subjects"][1]["audit_verdict"] = {"status": "fail", "note": "Course failed."}
        self.write_source()
        manifest = self.build("audit-fail")
        page = (self.root / "audit-fail" / "index.html").read_text(encoding="utf-8")
        self.assertIn('data-audit="fail"', page)
        self.assertIn('data-decision="accept" aria-pressed="false" disabled', page)
        self.assertIn("Accept is unavailable", page)

        accepted_failed = self.complete_verdict(manifest, accept_indices={0, 1})
        with self.assertRaisesRegex(review.ReviewError, "failed its mechanical audit"):
            review.validate_verdict(manifest, accepted_failed)
        rejected_failed = self.complete_verdict(manifest, accept_indices={0})
        review.validate_verdict(manifest, rejected_failed)

    def test_verdict_partition_scalars_order_and_claims(self) -> None:
        manifest = self.build("verdict")
        verdict = self.complete_verdict(manifest, accept_indices={0})
        verdict["notes_by_subject_identity"] = {
            f"{manifest['subjects'][0]['subject_id']}@{manifest['subjects'][0]['subject_digest']}": "note"
        }
        review.validate_verdict(manifest, verdict)

        incomplete = self.complete_verdict(manifest, accept_indices={0})
        incomplete["rejected_subjects"] = []
        with self.assertRaisesRegex(review.ReviewError, "partition is not exact"):
            review.validate_verdict(manifest, incomplete)

        mismatched = self.complete_verdict(manifest, accept_indices={0})
        mismatched["accepted_subjects"][0]["subject_digest"] = "sha256:" + "0" * 64
        with self.assertRaisesRegex(review.ReviewError, "partition is not exact"):
            review.validate_verdict(manifest, mismatched)

        wrong_scope = self.complete_verdict(manifest, accept_indices={0})
        wrong_scope["review_scope"] = "some_other_scope"
        with self.assertRaisesRegex(review.ReviewError, "review_scope must equal"):
            review.validate_verdict(manifest, wrong_scope)

        elevated = self.complete_verdict(manifest, accept_indices={0})
        elevated["downstream_claims"]["runtime_promotion_approved"] = True
        with self.assertRaisesRegex(review.ReviewError, "declared false-only claim set"):
            review.validate_verdict(manifest, elevated)

        reordered = self.complete_verdict(manifest, accept_indices={0, 1})
        reordered["accepted_subjects"].reverse()
        with self.assertRaisesRegex(review.ReviewError, "preserve manifest subject order"):
            review.validate_verdict(manifest, reordered)

    def selection_source(self, min_accept: int = 1, max_accept: int = 1):
        source = copy.deepcopy(self.source)
        source["selection_groups"] = [
            {
                "group_id": "hero",
                "title": "Pick the hero capture",
                "min_accept": min_accept,
                "max_accept": max_accept,
            }
        ]
        for letter in ("a", "b", "c"):
            source["subjects"].append(
                {
                    "subject_id": f"candidate:/fixture/hero-{letter}",
                    "subject_key": f"hero-{letter}",
                    "display_name": f"Hero option {letter.upper()}",
                    "selection_group": "hero",
                    "media_groups": [
                        {
                            "group_id": "shot",
                            "title": "Shot",
                            "items": [{"label": "Shot", "path": "capture.png"}],
                        }
                    ],
                }
            )
        return source

    def selection_verdict(self, manifest, accepted_ids, rejected_ids, not_chosen_ids):
        def entries(wanted):
            return [
                {
                    "subject_id": subject["subject_id"],
                    "subject_digest": subject["subject_digest"],
                }
                for subject in manifest["subjects"]
                if subject["subject_id"] in wanted
            ]

        return {
            "schema_name": review.VERDICT_SCHEMA,
            "schema_version": 1,
            "batch_id": manifest["batch_id"],
            "review_gate": manifest["review_gate"],
            "review_scope": manifest["review_scope"],
            "accepted_subjects": entries(accepted_ids),
            "rejected_subjects": entries(rejected_ids),
            "not_chosen_subjects": entries(not_chosen_ids),
            "notes_by_subject_identity": {},
            "downstream_claims": copy.deepcopy(manifest["downstream_claims"]),
        }

    def test_selection_group_build_renders_and_binds_identity(self) -> None:
        base_manifest = self.build("groupless")
        base_text = (self.root / "groupless/attended-review-manifest.json").read_text(encoding="utf-8")
        self.assertNotIn("selection_group", base_text)

        self.reset()
        self.source = self.selection_source()
        self.write_source()
        manifest = self.build("selection")
        page = (self.root / "selection/index.html").read_text(encoding="utf-8")
        self.assertIn('data-selection-group-header="hero"', page)
        self.assertIn("Choose exactly one alternative.", page)
        self.assertIn('<div class="selection-group-members">', page)
        self.assertIn('class="subject option"', page)
        self.assertIn('>Choose</button>', page)
        self.assertIn('<details class="option-extra">', page)
        self.assertIn('data-selection-group="hero"', page)
        header = page.index('data-selection-group-header="hero"')
        self.assertLess(header, page.index("candidate:/fixture/hero-a"))
        self.assertLess(page.index("candidate:/fixture/hero-c"), page.rindex("</div></section>"))
        self.assertEqual(manifest["selection_groups"][0]["max_accept"], 1)
        self.assertNotEqual(manifest["batch_id"], base_manifest["batch_id"])

    def test_selection_group_structural_errors(self) -> None:
        source = self.selection_source()
        source["subjects"][2]["selection_group"] = "unknown"
        self.source = source
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "undeclared group"):
            self.build("undeclared")

        self.reset()
        source = self.selection_source()
        source["subjects"] = [s for s in source["subjects"] if s["subject_key"] != "hero-b"]
        source["subjects"] = [s for s in source["subjects"] if s["subject_key"] != "hero-c"]
        self.source = source
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "at least two member subjects"):
            self.build("one-member")

        self.reset()
        source = self.selection_source(max_accept=4)
        self.source = source
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "max_accept exceeds"):
            self.build("over-max")

        self.reset()
        source = self.selection_source(min_accept=2, max_accept=1)
        self.source = source
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "must not exceed max_accept"):
            self.build("min-over-max")

        self.reset()
        source = self.selection_source()
        source["subjects"].insert(3, source["subjects"].pop(0))
        self.source = source
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "contiguous in subject order"):
            self.build("split-group")

        self.reset()
        source = self.selection_source()
        source["subjects"][2]["ab_group"] = "family-2"
        self.source = source
        self.write_source()
        with self.assertRaisesRegex(review.ReviewError, "only one of selection_group"):
            self.build("both-groupings")

    def test_selection_group_verdicts(self) -> None:
        self.source = self.selection_source()
        self.write_source()
        manifest = self.build("mc")
        base = {"criterion:/fixture/ac-01"}
        audio = {"candidate:/fixture/audio-a"}
        heroes = {f"candidate:/fixture/hero-{letter}" for letter in ("a", "b", "c")}

        chosen_one = self.selection_verdict(
            manifest, base | {"candidate:/fixture/hero-a"}, audio, heroes - {"candidate:/fixture/hero-a"}
        )
        review.validate_verdict(manifest, chosen_one)

        over_chosen = self.selection_verdict(
            manifest,
            base | {"candidate:/fixture/hero-a", "candidate:/fixture/hero-b"},
            audio,
            {"candidate:/fixture/hero-c"},
        )
        with self.assertRaisesRegex(review.ReviewError, "requires between 1 and 1"):
            review.validate_verdict(manifest, over_chosen)

        under_chosen = self.selection_verdict(
            manifest, base, audio | {"candidate:/fixture/hero-b"},
            heroes - {"candidate:/fixture/hero-b"},
        )
        with self.assertRaisesRegex(review.ReviewError, "requires between 1 and 1"):
            review.validate_verdict(manifest, under_chosen)

        all_defective = self.selection_verdict(manifest, base, audio | heroes, set())
        review.validate_verdict(manifest, all_defective)

        stray = self.selection_verdict(
            manifest, base | {"candidate:/fixture/hero-a"},
            set(), heroes - {"candidate:/fixture/hero-a"} | audio,
        )
        with self.assertRaisesRegex(review.ReviewError, "members of a choice or selection group"):
            review.validate_verdict(manifest, stray)

        missing_field = self.selection_verdict(
            manifest, base | {"candidate:/fixture/hero-a"}, audio, heroes - {"candidate:/fixture/hero-a"}
        )
        del missing_field["not_chosen_subjects"]
        with self.assertRaisesRegex(review.ReviewError, "partition is not exact|needs not_chosen_subjects"):
            review.validate_verdict(manifest, missing_field)

    def test_groupless_batch_tolerates_empty_not_chosen_only(self) -> None:
        manifest = self.build("plain")
        verdict = self.complete_verdict(manifest, accept_indices={0})
        verdict["not_chosen_subjects"] = []
        review.validate_verdict(manifest, verdict)
        moved = self.complete_verdict(manifest, accept_indices={0})
        moved["not_chosen_subjects"] = moved["rejected_subjects"]
        moved["rejected_subjects"] = []
        with self.assertRaisesRegex(review.ReviewError, "members of a choice or selection group"):
            review.validate_verdict(manifest, moved)

    def test_select_n_cardinality(self) -> None:
        self.source = self.selection_source(min_accept=2, max_accept=2)
        self.write_source()
        manifest = self.build("select-n")
        base = {"criterion:/fixture/ac-01"}
        audio = {"candidate:/fixture/audio-a"}
        two_chosen = self.selection_verdict(
            manifest,
            base | {"candidate:/fixture/hero-a", "candidate:/fixture/hero-b"},
            audio,
            {"candidate:/fixture/hero-c"},
        )
        review.validate_verdict(manifest, two_chosen)
        one_chosen = self.selection_verdict(
            manifest,
            base | {"candidate:/fixture/hero-a"},
            audio,
            {"candidate:/fixture/hero-b", "candidate:/fixture/hero-c"},
        )
        with self.assertRaisesRegex(review.ReviewError, "requires between 2 and 2"):
            review.validate_verdict(manifest, one_chosen)

    @unittest.skipUnless(shutil.which("node"), "Node.js is required for the selection runtime test")
    def test_browserless_selection_runtime(self) -> None:
        runtime = SCRIPT_DIR.parent / "assets" / "attended_review_runtime.js"
        runner = self.root / "selection-runtime-test.js"
        runner_source = r"""
const assert = require("assert");
require(__RUNTIME_PATH__);
const api = globalThis.BprAttendedReview;
const heroA = {subject_id: "hero-a", subject_digest: "sha256:aaa", selection_group: "hero"};
const heroB = {subject_id: "hero-b", subject_digest: "sha256:bbb", selection_group: "hero"};
const heroC = {subject_id: "hero-c", subject_digest: "sha256:ccc", selection_group: "hero"};
const lone = {subject_id: "lone", subject_digest: "sha256:ddd"};
const config = {
  batch_id: "batch-sel",
  review_gate: "gate",
  review_scope: "scope",
  downstream_claims: {claimed: false},
  selection_groups: [{group_id: "hero", title: "Hero", min_accept: 1, max_accept: 1}],
  subjects: [heroA, heroB, heroC, lone],
};
const controller = new api.ReviewController(config, api.createMemoryStorage());
assert.throws(() => controller.buildVerdict(), /unresolved/);
controller.setDecision("hero-a", "sha256:aaa", "accept");
controller.setDecision("hero-b", "sha256:bbb", "accept");
assert.strictEqual(controller.state(heroA).decision, null, "radio semantics clear the prior choice");
assert.strictEqual(controller.state(heroB).decision, "accept");
assert.strictEqual(controller.summary().notChosen, 2);
controller.setDecision("lone", "sha256:ddd", "accept");
const verdict = controller.buildVerdict();
assert.deepStrictEqual(verdict.accepted_subjects.map((s) => s.subject_id), ["hero-b", "lone"]);
assert.deepStrictEqual(verdict.not_chosen_subjects.map((s) => s.subject_id), ["hero-a", "hero-c"]);
const wide = new api.ReviewController(
  {...config, selection_groups: [{group_id: "hero", title: "Hero", min_accept: 1, max_accept: 2}]},
  api.createMemoryStorage()
);
wide.setDecision("hero-a", "sha256:aaa", "accept");
wide.setDecision("hero-b", "sha256:bbb", "accept");
assert.throws(() => wide.setDecision("hero-c", "sha256:ccc", "accept"), /at most 2/);
const plain = new api.ReviewController(
  {batch_id: "b", review_gate: "g", review_scope: "s", downstream_claims: {claimed: false}, subjects: [lone]},
  api.createMemoryStorage()
);
plain.setDecision("lone", "sha256:ddd", "reject");
assert.ok(!("not_chosen_subjects" in plain.buildVerdict()));
"""
        runner.write_text(
            runner_source.replace("__RUNTIME_PATH__", json.dumps(str(runtime))),
            encoding="utf-8",
        )
        completed = subprocess.run(
            ["node", str(runner)],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)

    def test_choice_group_shorthand_remains_valid(self) -> None:
        source = copy.deepcopy(self.source)
        for letter in ("a", "b"):
            source["subjects"].append(
                {
                    "subject_id": f"candidate:/fixture/dir-{letter}",
                    "subject_key": f"dir-{letter}",
                    "display_name": f"Direction {letter.upper()}",
                    "choice_group": "direction",
                    "media_groups": [
                        {
                            "group_id": "shot",
                            "title": "Shot",
                            "items": [{"label": "Shot", "path": "capture.png"}],
                        }
                    ],
                }
            )
        self.source = source
        self.write_source()
        manifest = self.build("choice")
        page = (self.root / "choice/index.html").read_text(encoding="utf-8")
        self.assertIn('data-choice-group="direction"', page)
        self.assertIn("Choose exact alternative", page)
        base = {"criterion:/fixture/ac-01"}
        audio = {"candidate:/fixture/audio-a"}
        verdict = self.selection_verdict(
            manifest, base | {"candidate:/fixture/dir-a"}, audio, {"candidate:/fixture/dir-b"}
        )
        review.validate_verdict(manifest, verdict)
        both = self.selection_verdict(
            manifest,
            base | {"candidate:/fixture/dir-a", "candidate:/fixture/dir-b"},
            audio,
            set(),
        )
        with self.assertRaisesRegex(review.ReviewError, "exactly one accepted"):
            review.validate_verdict(manifest, both)

    @unittest.skipUnless(shutil.which("node"), "Node.js is required for the browserless runtime test")
    def test_browserless_runtime(self) -> None:
        runtime = SCRIPT_DIR.parent / "assets" / "attended_review_runtime.js"
        runner = self.root / "runtime-test.js"
        runner_source = r"""
const assert = require("assert");
require(__RUNTIME_PATH__);
const api = globalThis.BprAttendedReview;
const subjectA = {subject_id: "subject-a", subject_digest: "sha256:aaa"};
const subjectB = {subject_id: "subject-b", subject_digest: "sha256:bbb", audit_verdict: {status: "fail", note: "bad"}};
const config = {
  batch_id: "batch-one",
  review_gate: "fixture_gate",
  review_scope: "fixture_scope",
  downstream_claims: {alpha_claimed: false, beta_claimed: false},
  subjects: [subjectA, subjectB],
};
const storage = api.createMemoryStorage();
assert.notStrictEqual(api.storageKey("batch-one", subjectA), api.storageKey("batch-two", subjectA));
assert.notStrictEqual(api.storageKey("batch-one", subjectA), api.storageKey("batch-one", {...subjectA, subject_digest: "sha256:changed"}));
const controller = new api.ReviewController(config, storage);
controller.setDecision("subject-a", "sha256:aaa", "accept");
controller.setNotes("subject-a", "sha256:aaa", "Keep this exact subject");
assert.throws(() => controller.setDecision("subject-b", "sha256:bbb", "accept"), /failed its mechanical audit/);
assert.throws(() => controller.buildVerdict(), /remain pending/);
const reloaded = new api.ReviewController(config, storage);
assert.strictEqual(reloaded.state(subjectA).decision, "accept");
assert.strictEqual(reloaded.state(subjectA).notes, "Keep this exact subject");
const changed = new api.ReviewController({...config, subjects: [{...subjectA, subject_digest: "sha256:changed"}]}, storage);
assert.strictEqual(changed.summary().pending, 1);
reloaded.setDecision("subject-b", "sha256:bbb", "reject");
const verdict = reloaded.buildVerdict();
assert.strictEqual(verdict.schema_name, "bpr.attended_review_verdict");
assert.strictEqual(verdict.review_scope, "fixture_scope");
assert.strictEqual(verdict.review_gate, "fixture_gate");
assert.deepStrictEqual(verdict.downstream_claims, {alpha_claimed: false, beta_claimed: false});
assert.deepStrictEqual(verdict.accepted_subjects, [{subject_id: "subject-a", subject_digest: "sha256:aaa"}]);
assert.deepStrictEqual(verdict.rejected_subjects, [{subject_id: "subject-b", subject_digest: "sha256:bbb"}]);

(async () => {
  let clipboardValue = null;
  assert.strictEqual(await api.copyText("one", {navigator: {clipboard: {writeText: async (value) => { clipboardValue = value; }}}}), "clipboard");
  assert.strictEqual(clipboardValue, "one");
  let selected = false;
  const textarea = {style: {}, setAttribute() {}, select() { selected = true; }};
  const document = {
    createElement: () => textarea,
    body: {appendChild() {}, removeChild() {}},
    execCommand: (command) => command === "copy"
  };
  assert.strictEqual(await api.copyText("two", {document}), "execCommand");
  assert.strictEqual(textarea.value, "two");
  assert.ok(selected);
})().catch((error) => { console.error(error); process.exit(1); });
"""
        runner.write_text(
            runner_source.replace("__RUNTIME_PATH__", json.dumps(str(runtime))),
            encoding="utf-8",
        )
        completed = subprocess.run(
            ["node", str(runner)],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr or completed.stdout)


if __name__ == "__main__":
    unittest.main()
