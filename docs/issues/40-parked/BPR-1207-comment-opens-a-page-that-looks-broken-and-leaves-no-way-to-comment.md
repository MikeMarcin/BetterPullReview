---
schema: "implement-rfc.card.v1"
id: "BPR-1207"
title: "Comment opens a page that looks broken and leaves no way to comment"
status: "Parked"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1133"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner note on attended candidate BPR-1133-arrow, 2026-09-25","session":"06ad5734ed9f7c2fd6d0a62c8328fa0a","candidate":"BPR-1133-arrow","decision":"b3d1d32d-02c6-4951-98bf-d5ecd4d36d0f","reviewed_revision":"ba9d272339ad235f1d99688c5c1af198b0c9c7ef","reviewed_executable_sha256":"a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83","criteria":["AC-08"],"related_cards":["BPR-1133","BPR-1135"],"reservation":"docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"},"owner_handback49":{"session":"d7cf21b16656c6ec19c8787255aa4006","cursor":49,"source_revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","binary_sha256":"96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b","decisions":[{"candidate":"BPR-1207-comment","criterion":"AC3","fingerprint":"feaaefd6e30de336587c5c074e017c49611be7993ed763a8014f21ca5f1bafe5","decision_id":"e1878b04-19ec-40a8-ab7c-ed1b35417f65","outcome":"reject","note":"Meta feedback: I don't know why the link to the PR to open isn't in a copyable code block on the review page. And the instructions are not clear as I would expect from the $wizard skill\n\nI cannot add a comment with the instructions and fixture provided, comment button is not active. \n","saved_at":"2026-09-26T21:00:27.749392+00:00","actor":"human"}]}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"356bbdb22624beabe7b8f611e6c54c8d9abdad3f","evidence":{"kind":"observation","detail":"Exact frozen m2 native UIforETW fixture reproduced unusable page before deliberate change and after current-head change selection; closed PR has no admitted mutation draft. Card records executable digest and route; no publication."}},{"criterion":"AC2","outcome":"pass","revision":"356bbdb22624beabe7b8f611e6c54c8d9abdad3f","evidence":{"kind":"command","detail":"1207-controls-tests.log 6/6; final1207-recovery-final-tests.log4/4; real composed local write zero provider mutation, actual hidden panel/button assertions, forced-enable negative control and retained-conflict reopen. All logs in /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence."}},{"criterion":"AC3","outcome":"fail","revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","evidence":{"kind":"observation","detail":"Owner hand-back 49, session d7cf21b16656c6ec19c8787255aa4006; BPR-1207-comment reject e1878b04-19ec-40a8-ab7c-ed1b35417f65. Exact fingerprints and notes retained in metadata. Verdict applies only to the frozen candidate."}}]
---

# BPR-1207: Comment opens a page that looks broken and leaves no way to comment

## Owner observation

In attended session `06ad5734` (packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)), candidate `BPR-1133-arrow` on the
frozen m2 app (`ba9d2723`), the owner kept the interim arrow and menu (decision
`b3d1d32d-02c6-4951-98bf-d5ecd4d36d0f`, sequence 49, `actor: human`, "Keep the
current arrow and menu") and added, verbatim:

> note: clicking comment takes me to this screen in the screenshot which seems completely broken, Doesn't seem like I can actually leave a comment yet.

The screenshot is kept with the decision in the session's external state. It
shows, in Review, with the bar at "8 changes" and "file 4 of 5": Back; Publish
and Discard; the line "Select a current-head diff line to compose a review
comment."; Reload accepted draft and Reapply my text; and three empty dark
panels. The status bar reads "Actionable file 4 of 5. Private progress:
unread, not accepted. Review history is ready." The line is
`review_draft_editor.cpp`'s empty-target state.

RFC-BPR-0052 AC-08 says Comment starts a comment on the current line.
Whether a current-head line existed in the owner's state is not established.

## Acceptance

- AC1 Reproduce on the frozen candidate's fixture what Comment does from the
  owner's state (a file open, no change selected), and whether selecting a
  current-head line and then Comment gives a usable editor.
- AC2 Comment never opens an unusable page. With no current-head line it is
  either unavailable, with the reason in its tooltip and menu, or it takes a
  defined line. The inline page never shows empty panels or draft-revision
  actions that cannot act. A composed course and a negative control pin it.
- AC3 The owner writes a comment through Comment on the fixture, without
  publishing, and judges it on macOS: one question.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]].

## Engineering evidence — 2026-09-26

AC1: the exact frozen m2 executable (SHA-256
`a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83`)
ran natively with the uiforetw-74 fixture and the explicit automated fixture
boundary. Home opened google/UIforETW#74. On file 4/5,
UIforETW/UIforETWDlg.cpp, Comment reproduced the empty panels and disabled
controls before a deliberate change-selection gesture. After Next change
selected line 216, Comment still showed an unusable page. The recorded PR is
closed, so a current diff line does not imply an admitted mutation draft.
No publication was attempted. The frozen process exited normally; the earlier
failed sandboxed GUI launchers and fixture processes also ended. This records
the observed route without inventing the owner's exact previous clicks.

Decision: enable Comment only for the selected target's admitted editable
draft or actionable recovery of its retained text, with its reason in the tooltip, menu and accessible description. This
preserves existing authority and publication decisions. Activation rechecks
the projection. The inline page hides empty workshop panels and recovery
controls that cannot act, retaining rejected text and its explicit recovery.
An accepted reapply or Reload clears the stale conflict sentence.

The composed fixture course selects a current-head line, opens Comment,
types a comment and observes its accepted local text with zero provider
submissions, comments or replies. Renders cover 100/125/200. The negative
control forces an unavailable Comment action enabled; activation still
refuses to open the unusable page. Editor, conversation and existing Compose
workshop courses pass. Full-window 100/125/200 courses pass after updating
former always-enabled assumptions. A rendered inspection then caught native
tool buttons remaining visible despite their hidden QAction; visibility now
follows the action and the composed assertion checks the buttons themselves.
The final control-specific rerun and exact candidate identity are recorded
in the acceptance evidence. AC3 remains pending: the owner's no-publish
writing judgment must use the open synthetic fixture, not closed UIforETW.

Self-review added a retained-conflict route: Comment may reopen the exact
target's usable Reload/Reapply recovery without enabling Publish. Its new
check exposed hidden QAction enablement being used to decide visibility;
both properties now follow the retained-candidate facts directly. The final
editor/workshop/window/composed rerun passes 4/4, including actual button
visibility, retained-conflict entry and zero mutation. Earlier scale passes
remain in the evidence directory; no failing run is counted as acceptance.

## Owner hand-back 49 — 2026-09-26

The owner reviewed frozen source `8b5bc31c`, executable SHA-256
`96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b`,
in session `d7cf21b16656c6ec19c8787255aa4006`. Full decision identities and
fingerprints are retained in the card metadata.

- **AC3 — Reject**, candidate `BPR-1207-comment`, decision `e1878b04-19ec-40a8-ab7c-ed1b35417f65`.

  > Meta feedback: I don't know why the link to the PR to open isn't in a copyable code block on the review page. And the instructions are not clear as I would expect from the $wizard skill
  >
  > I cannot add a comment with the instructions and fixture provided, comment button is not active.

The rejected course is not accepted by its automated tests. The replacement
preparation must use the open authenticated synthetic fixture, prove the requested
public action and provide the Wizard steps before the next owner judgment. The
owner attachments show public preview, which correctly refuses these actions.
