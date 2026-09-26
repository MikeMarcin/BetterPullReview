---
schema: "implement-rfc.card.v1"
id: "BPR-1203"
title: "Compose review opens a page that does not say what is happening or what to do"
status: "Parked"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner rejection of attended candidate BPR-1129-kept, 2026-09-25","session":"06ad5734ed9f7c2fd6d0a62c8328fa0a","candidate":"BPR-1129-kept","decision":"011a2ebb-f211-4096-a88a-e5a820219032","reviewed_revision":"ba9d272339ad235f1d99688c5c1af198b0c9c7ef","reviewed_executable_sha256":"a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83","criteria":["AC-03"],"related_cards":["BPR-1129","BPR-1135"],"reservation":"docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"},"owner_handback49":{"session":"d7cf21b16656c6ec19c8787255aa4006","cursor":49,"source_revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","binary_sha256":"96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b","decisions":[{"candidate":"BPR-1203-page","criterion":"AC3","fingerprint":"f7be43756bd42e13d2da963cade61cfb3f550dbc2845d22bc8e61e06f27cc391","decision_id":"1d7ebc0a-bc56-4253-8c43-2d20c59e3c94","outcome":"reject","note":"I can't reach compose with the instructions provided.","saved_at":"2026-09-26T21:01:32.675373+00:00","actor":"human"},{"candidate":"BPR-1203-kept","criterion":"AC4","fingerprint":"d0ff147b36ac078c16d36516ada370a160a0a67e8f216a4834cf2c2adae7249f","decision_id":"abd4b1dc-91c7-4d78-97a2-84664b30f00a","outcome":"reject","note":"Can't reach compose.","saved_at":"2026-09-26T21:01:49.841776+00:00","actor":"human"}]}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"30748ed64c7bf55f901085ec458891591c1d85ac","evidence":{"kind":"command","detail":"Frozen native closed-fixture reproduction; six editor/composition scale courses and composition pin contract pass. No admission or publication change."}},{"criterion":"AC2","outcome":"pass","revision":"30748ed64c7bf55f901085ec458891591c1d85ac","evidence":{"kind":"command","detail":"Frozen native closed-fixture reproduction; six editor/composition scale courses and composition pin contract pass. No admission or publication change."}},{"criterion":"AC3","outcome":"fail","revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","evidence":{"kind":"observation","detail":"Owner hand-back 49, session d7cf21b16656c6ec19c8787255aa4006; BPR-1203-page reject 1d7ebc0a-bc56-4253-8c43-2d20c59e3c94. Exact fingerprints and notes retained in metadata. Verdict applies only to the frozen candidate."}},{"criterion":"AC4","outcome":"fail","revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","evidence":{"kind":"observation","detail":"Owner hand-back 49, session d7cf21b16656c6ec19c8787255aa4006; BPR-1203-kept reject abd4b1dc-91c7-4d78-97a2-84664b30f00a. Exact fingerprints and notes retained in metadata. Verdict applies only to the frozen candidate."}}]
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

## Engineering evidence — 2026-09-26

The recorded uiforetw-74 fixture is closed. Source tracing establishes the
old `select_observed_review_mutation` refusal followed by generic
`compose_review_submission` invalidation. The real composed course opens
that fixture through ordinary controls and now observes the closed-PR reason,
no draft, no empty fields, and zero additional submissions. This establishes
a real deterministic path; it does not invent an authority race or assert
the owner's exact earlier clicks.

Decision: put purpose, draft state and next step before the inputs so the
page explains itself even when admission yields no draft. Existing publication
admission and commands remain unchanged. Empty target/body fields are hidden
without a draft; every disabled action has an explanatory tooltip.

Editor courses pass at 100/125/200 and composed courses at 100/125/200
(6/6 total), including signed-out, kept, submitted, unknown, changed-account,
changed-head and closed states. Refused and historical drafts retain their
identity and text. First reconfiguration omitted the cache arguments and
failed; corrected offline configuration and scale runs pass.

Composition pin review covers `3161f459..BPR-1203 candidate`: adopted
BPR-0966 only touches Qt and its tests; the pinned app scope changes only
wording on existing refusal branches and the workflow's optional display
reason. No selection, grant, write, effect, recovery or persistence policy
changes. Full digest `82e441d8116f42165092faa28a4e802c2630d8e2870cec25ef87813d8468731b`;
production manifest `c48ab7adf9e9d9c978992e7ff3fe385fbf070e9edea017fbda8a4b59a9da18d8`;
production mutation digest unchanged. AC3 and AC4 remain separate pending
owner judgments; neither the page nor kept mark is accepted by this evidence.

Frozen native observation: launched exact m2 executable `a2e5128a...8a83`
with the uiforetw-74 fixture and a fresh private root; entered
`google/UIforETW#74` in Home, then Compose review. The native screenshot
reproduces the empty target and generic authority-changed sentence exactly.
No publish control was used. Sandboxed GUI attempts could not register an AX
application; the successful native launch used the fixture-only automated
boundary. Its process is task-owned and will be stopped before handoff.

## Owner hand-back 49 — 2026-09-26

The owner reviewed frozen source `8b5bc31c`, executable SHA-256
`96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b`,
in session `d7cf21b16656c6ec19c8787255aa4006`. Full decision identities and
fingerprints are retained in the card metadata.

- **AC3 — Reject**, candidate `BPR-1203-page`, decision `1d7ebc0a-bc56-4253-8c43-2d20c59e3c94`.

  > I can't reach compose with the instructions provided.

- **AC4 — Reject**, candidate `BPR-1203-kept`, decision `abd4b1dc-91c7-4d78-97a2-84664b30f00a`.

  > Can't reach compose.

The rejected course is not accepted by its automated tests. The replacement
preparation must use the open authenticated synthetic fixture, prove the requested
public action and provide the Wizard steps before the next owner judgment. The
owner attachments show public preview, which correctly refuses these actions.
