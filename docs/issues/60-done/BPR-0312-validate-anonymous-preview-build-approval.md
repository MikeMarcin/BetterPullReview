---
id: BPR-0312
title: Validate the positive anonymous-preview build decision
status: Done
rfc: RFC-BPR-0017
depends_on: [BPR-0299]
---

# Validate the positive anonymous-preview build decision

## Required final state

- BPR-0299's byte-preserved exact response affirmatively approves building the
  measured public-preview mode; a rejection, question, partial response, or
  `do not build` verdict cannot satisfy this card.
- The validation record binds the subject digest, response bytes, measured
  probe evidence, approved naming/limits/platform scope, result, and commit.
- This card creates the exact
  `scripts/validate_rfc0017_public_preview_decision.py` interface seeded in the
  verification ledger; it accepts only a byte-exact affirmative `build`
  response and emits the bound validation record. Its `--validate-existing`
  mode writes nothing and revalidates the response and record at closeout.
- On rejection this card remains unresolved in backlog, mechanically blocking
  BPR-0300/BPR-0301 and every transitive implementation card.
- This gate grants no additional network, credential, private-read, mutation,
  release, or RFC-closeout authority.

## Lane — 2026-09-01

[[BPR-0299]] is decided: build. The owner's acceptance carried no note, so the
four definitions this card validates — public-preview naming, limit
presentation, sign-in handoff, and supported-platform policy — are still to be
supplied; this card's packet is where they get bound and validated. It moves
to `20-ready/` as the next unattended step of RFC-BPR-0017: write the
decision packet the ledger's "Product build/no-build decision and positive
gate" row names, with the validator script it seeds, and present it.

## Delivered the unattended half — 2026-09-01

The gate now exists and is asked. `docs/verification/reviews/rfc0017-public-preview-decision-v001/`
holds the decision packet, the offline review page built from it, and the
pending-packet source manifest. The packet does not reopen [[BPR-0299]]: it
states the build decision as a fact, binds the nine sanitized probe captures
from `rfc0017-anonymous-probe-v001` and `rfc0017-anonymous-probe-v005` plus the
two `run-boundary-decisions-v001` artifacts that recorded the decision, and
then asks for exactly the four definitions the acceptance left owed. Each is
presented as the question it answers and the exact text proposed to bind —
public-preview naming, limit presentation, sign-in handoff, and
supported-platform policy — so one acceptance resolves all four. Accepting with
an empty note or the note `as proposed` binds the packet's words; a
`<key>: <text>` line replaces one and leaves the other three as proposed.

Only non-markdown artifacts are bound as identity files, because the
documentation gate's link check walks copied markdown under
`offline-package/`; the markdown the definitions were written against is
listed by digest in a detail section instead.

`scripts/validate_rfc0017_public_preview_decision.py` carries the exact
interface the verification ledger seeds. It accepts only a byte-exact
affirmative `bpr.attended_review_verdict` for this packet's batch and subject,
and refuses a rejection, a question, a partial verdict, a stale subject digest,
an unknown or repeated definition key, a named-but-empty definition, a note
carrying `do not build`, packet text that drifted from the page the owner saw,
and bound evidence that no longer matches the tree. Its `--validate-existing`
mode writes nothing and revalidates the response and the record.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The packet's page is built, single-subject, and copies no markdown into `offline-package/` | `python3 .agents/skills/bpr-attended-review/scripts/build_attended_review.py build --input docs/verification/reviews/rfc0017-public-preview-decision-v001/offline-review-input.json --out docs/verification/reviews/rfc0017-public-preview-decision-v001/offline-package` | built `bpr:/attended-review/betterpullreview/479439050b23e8fe4415ce6a`; one subject, 11 identity files, no `.md` under `offline-package/` | this commit |
| The validator accepts only the affirmative build verdict and refuses every other answer | `python3 tests/scripts/validate_rfc0017_public_preview_decision_tests.py` | **44 tests, OK** over the accepting paths, the note grammar, the packet/page binding, and the record | this commit |
| The real packet, page, and validator agree end to end | the ledger command above, rehearsed against a scratch verdict and a scratch `--output`, then rerun with `--validate-existing`; both rehearsal files removed | `RECORDED:build`, `DEFINITIONS_BOUND:4`, `EVIDENCE_BOUND:11`, then `VALID:build` | this commit |
| The tree's documentation, packet, and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| The documentation gate's own contract holds | `python3 tests/scripts/verify_documentation_tests.py` | **54 tests, OK** | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |

## Remaining attended step — the owner's verdict

Nothing else on this card is unattended work. Open
`docs/verification/reviews/rfc0017-public-preview-decision-v001/offline-package/index.html`,
resolve the one subject, and save the exported verdict byte-preserved to
`docs/verification/reviews/rfc0017-public-preview-decision-v001/human-verdict.json`.
That file feeds:

```sh
python3 scripts/validate_rfc0017_public_preview_decision.py \
  --packet docs/verification/reviews/rfc0017-public-preview-decision-v001/operator-packet.md \
  --response docs/verification/reviews/rfc0017-public-preview-decision-v001/human-verdict.json \
  --require build \
  --output docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json
```

Until that verdict arrives this card is not satisfied, the ledger's positive
build approval stays `Pending`, and [[BPR-0300]] onward stay
dependency-blocked. A rejection is a recordable answer, not a failure: it is
recorded as evidence and leaves this gate unresolved.

## Validated — 2026-09-02

The owner's verdict arrived and was saved byte-preserved as
`docs/verification/reviews/rfc0017-public-preview-decision-v001/human-verdict.json`:
the one subject accepted, no subject rejected, an empty note, and every
downstream claim `false`. An empty note binds the packet's proposed text, and
the packet's proposed text was written from the owner's own answers given
before it was presented — the suggested names, "a ring that fills up with a
tooltip ala claude desktop's usage", the suggested handoff, and the suggested
platform policy — so the four bound definitions are the owner's words twice
over, not an agent's defaults accepted by silence.

The validator accepted it on the first run and again with nothing written.
The record binds the subject digest, the response bytes, the packet the owner
saw, the review manifest, the validator itself, the nine probe captures, the
two [[BPR-0299]] decision artifacts, all four definitions with their source,
and the validated commit. `claims.bpr_0312_positive_build_gate_satisfied` is
`true`; every other claim in the record is `false`, as the packet promised: no
credential, private read, mutation, release, platform flip, or closeout was
granted here.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The verdict is byte-exact, affirmative, and bound with the definitions | the ledger command: `python3 scripts/validate_rfc0017_public_preview_decision.py --packet … --response … --require build --output …validation-record.json` | exit 0: `RFC0017_PUBLIC_PREVIEW_DECISION_RECORDED:build`, `RFC0017_PUBLIC_PREVIEW_DEFINITIONS_BOUND:4`, `RFC0017_PUBLIC_PREVIEW_EVIDENCE_BOUND:11` | this commit |
| Revalidation writes nothing and agrees | the same command with `--validate-existing` | exit 0: `RFC0017_PUBLIC_PREVIEW_DECISION_VALID:build`, `DEFINITIONS_BOUND:4`, `EVIDENCE_BOUND:11`; the record's bytes unchanged | this commit |
| The record binds what the card required | `validation-record.json` | `decision: build`, `validated_commit: 623cd2dd`, subject digest `657e5459…a5d5af`, response sha256 `142bf1b1…f3782e` (809 bytes), 4 definitions all `source: packet`, 9 probe and 2 decision evidence files by digest, `downstream_claims` all `false` | this commit |
| The tree's documentation, packet, and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |

Consequences applied in this commit: the ledger's positive-approval row is
Verified and its decision row records the build decision as taken; the RFC
index says implementation may start; [[BPR-0300]] and [[BPR-0301]], whose
only other dependency is the terminal [[BPR-0035]], move to `20-ready/`.
