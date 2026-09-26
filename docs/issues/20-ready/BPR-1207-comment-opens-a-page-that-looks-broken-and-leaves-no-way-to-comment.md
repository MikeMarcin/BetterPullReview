---
schema: "implement-rfc.card.v1"
id: "BPR-1207"
title: "Comment opens a page that looks broken and leaves no way to comment"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1133"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner note on attended candidate BPR-1133-arrow, 2026-09-25", "session": "06ad5734ed9f7c2fd6d0a62c8328fa0a", "candidate": "BPR-1133-arrow", "decision": "b3d1d32d-02c6-4951-98bf-d5ecd4d36d0f", "reviewed_revision": "ba9d272339ad235f1d99688c5c1af198b0c9c7ef", "reviewed_executable_sha256": "a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83", "criteria": ["AC-08"], "related_cards": ["BPR-1133", "BPR-1135"], "reservation": "docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json", "milestone_plan": {"id": "rfc-bpr-0052-review-workbench-scaffolding", "path": "docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
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
