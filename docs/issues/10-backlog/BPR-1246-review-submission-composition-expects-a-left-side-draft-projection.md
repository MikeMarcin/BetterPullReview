---
schema: "implement-rfc.card.v1"
id: "BPR-1246"
title: "The review submission composition course expects a left-side draft projection BPR-0587 no longer offers"
status: "Backlog"
rfcs: []
depends_on: []
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json","related_cards":["BPR-0587","BPR-1207","BPR-1238"]}
---

# BPR-1246: The review submission composition course expects a left-side draft projection BPR-0587 no longer offers

## Observation

At `main` 83edd361b on macOS arm64, `application_review_submission_composition_public_path`
and its `_scale_125` and `_scale_200` rows fail with
`review-submission-composition:fail current line has a local inline draft projection`
(`tests/end_to_end/review_submission_composition_public_path.cpp:140`). After
closing the composition page the course waits for an inline draft on the
current line, then selects `left:45` and waits for a "left-side draft
projection".

Since f55e39e72 (BPR-0587) `present_review_draft` offers a new draft only on a
right-side line, and [[BPR-1207]] keeps that: left replies, no left roots. The
course's left-side expectations predate the rule; whether the current line it
lands on is left-side is not yet confirmed.

## Outcome

The course follows the accepted rule: it expects no new left-side draft, keeps
its negative control that Comment and Publish are refused on the left, and
reaches its local comment on a right-side line. A product defect found on the
way is fixed where it lives.

## Direct acceptance

- The three rows pass on macOS.
