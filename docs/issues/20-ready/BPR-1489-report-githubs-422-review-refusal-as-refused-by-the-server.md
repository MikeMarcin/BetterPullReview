---
schema: "implement-rfc.card.v1"
id: "BPR-1489"
title: "Report GitHub's 422 review refusal as refused by the server"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_cards":["BPR-1345","BPR-1429"]}
---

# Report GitHub's 422 review refusal as refused by the server

`application_start_review_contract` fails its `refused` course on main
(`refusal-lost-the-draft`). Since [[BPR-1345]] a review GitHub refuses with
422 was labelled `unsupported`; [[BPR-1429]] (2c4dad35a) began showing that
label's text, so the refusal read "This connection does not support this
operation". Diagnosed in the owner's session "Diagnose four failing GUI
public-path contracts" (ce5e91e08).

## Acceptance

- AC1: The 422 refusal is labelled `refused_by_server`, and
  `live_review_submission_provider` checks the label in both its scripted and
  replayed refusal courses, failing with the old label.
- AC2: `application_start_review_contract` passes on macOS.

## Boundaries

No change to whether the review is withdrawn or restored.
