---
schema: "implement-rfc.card.v1"
id: "BPR-2309"
title: "Record the retirement of Review Cycles: RFCs, cards, guides and the 0.5.0 release-notes line"
status: "Done"
rfcs: []
depends_on: ["BPR-2308"]
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"tracker","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2308--BPR-2309--30601b62012d.json","origin":"owner, 2026-11-19/20 (see BPR-2308)","related_rfcs":["RFC-BPR-0053","RFC-BPR-0054","RFC-BPR-0055","RFC-BPR-0056","RFC-BPR-0057","RFC-BPR-0058","RFC-BPR-0059","RFC-BPR-0060","RFC-BPR-0061","RFC-BPR-0108"],"related_cards":["BPR-2308","BPR-1194","BPR-2214","BPR-1462","BPR-2230"]}
---

# BPR-2309: Record the retirement of Review Cycles

## What changed

The repository's RFC statuses are Proposed, In Progress, Auto-Verified,
Deferred and Rejected; there is no Superseded or Retired. Following
[[BPR-2313]]'s rule for delivered work a later decision replaces, each cycle
RFC gains a Status section under its title recording the owner's decision,
that [[BPR-2308]] removed the feature, that local PR drafts (RFC-BPR-0108)
replace it, and what of it remains.

| RFC | Was | Now |
|---|---|---|
| RFC-BPR-0053 to 0060 | Auto-Verified | Auto-Verified, Status "retired with Review Cycles" |
| RFC-BPR-0061 | Proposed | Auto-Verified, closed to what shipped and retired; RFC-BPR-0037 leaves `depends_on` |

RFC-BPR-0061 was not made Rejected: its sixteen implementation cards are Done
and delivered, and plan `bpr-review-cycles-v1` names it, so Rejected would
claim they never shipped. It closes on what shipped instead, as BPR-2313 did,
and its closeout [[BPR-1194]] is Done with its checklist withdrawn. Dropping
RFC-BPR-0037 (still Proposed) from its `depends_on` is part of this decision:
the cycle page that took that workflow's chrome no longer exists.

Also recorded: an owner amendment in RFC-BPR-0108 withdrawing AC06's clause
that Review Cycles stay reachable; the RFC index rows; [[BPR-2214]] no longer
blocking BPR-1194; [[BPR-1462]]'s Windows procedure (the capture-based reader
test is gone, the bench is `bench_git_metadata_scale`); [[BPR-2230]]'s
`bpr cycle` mention.

Guides: `docs/development/cycle-cli.md` and `docs/ux/review-cycles-workflow.md`
are removed; README, the docs index, the product contract, system
architecture, domain model, threat model, AI agent boundary and the
development guides now say Review Cycles were retired and what remains.
Historical records (Done cards, verification ledgers, imported packs and
plans) keep their text; links in them to removed files became plain text.

## Release notes

`docs/releases/v0.5.0.md`, under Changed:

> Review Cycles and `bpr cycle` are removed in favour of local PR drafts (**Prepare Pull Request…** and `bpr draft`). Cycles you saved stay on disk, unread.

## Evidence

`python3 scripts/verify_documentation.py --root .` and
`python3 tests/scripts/verify_documentation_tests.py` pass on the landed
revision.
