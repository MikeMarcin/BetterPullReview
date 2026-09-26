---
schema: "implement-rfc.card.v1"
id: "BPR-1203"
title: "Compose review opens a page that does not say what is happening or what to do"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner rejection of attended candidate BPR-1129-kept, 2026-09-25", "session": "06ad5734ed9f7c2fd6d0a62c8328fa0a", "candidate": "BPR-1129-kept", "decision": "011a2ebb-f211-4096-a88a-e5a820219032", "reviewed_revision": "ba9d272339ad235f1d99688c5c1af198b0c9c7ef", "reviewed_executable_sha256": "a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83", "criteria": ["AC-03"], "related_cards": ["BPR-1129", "BPR-1135"], "reservation": "docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json"}
---

# BPR-1203: Compose review opens a page that does not say what is happening or what to do

## Owner decision

The question was whether Compose review's amber kept-draft mark is visible and
clear enough. In attended session `06ad5734` (packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)), candidate
`BPR-1129-kept` on the frozen m2 app (`ba9d2723`), decision
`011a2ebb-f211-4096-a88a-e5a820219032`, sequence 39, `actor: human`: **reject**,
with the note, verbatim:

> When I click compose review I end up on this screen and I have no idea what's happening or what I'm meant to do.

The attached screenshot is kept with the decision in the session's external
state. It shows the Compose review page in Review: Back; "Review pull
request"; an empty one-line field; a "Comment" selector; the "Write a review
summary…" editor; Submit review and Discard; Check status and New review; and
the line "Review authority changed. Open or refresh the pull request to
continue." That line is `review_submission_workflow::invalidate()`, which runs
when the host, account, viewer, repository, pull request or identity
generation behind the draft no longer matches.

The mark was not accepted; the page the owner reached through it explained
nothing. Why the frozen fixture app was in the invalidated state is not
established.

## Acceptance

- AC1 Establish why Compose review on the frozen candidate's fixture showed
  the invalidated-authority state, and whether a real path reaches it,
  keeping observation separate from inference.
- AC2 In every state it can be in (no draft, a kept draft, a draft whose
  authority changed, a read-only or signed-out window), the Compose review page
  says in plain words what it is, what state the draft is in and what to do
  next. Every unavailable action says why. No unexplained empty field remains.
  Renders at 100, 125 and 200 percent; a composed course covers the states.
- AC3 The owner judges the revised page on macOS: one question.
- AC4 The owner judges the kept-draft mark again on a candidate where a draft
  is actually kept: one question, separate from AC3.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]]. RFC-BPR-0037's draft and publication semantics are unchanged.
