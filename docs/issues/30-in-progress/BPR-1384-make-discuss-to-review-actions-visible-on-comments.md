---
schema: "implement-rfc.card.v1"
id: "BPR-1384"
title: "Make Discuss to Review actions visible on comments"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1383"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"Owner steering, 2026-10-10","reservation":"docs/issues/_reservations/BPR-1384--BPR-1384--91dbbeb8d7a1.json","related_cards":["BPR-1383"],"milestone_plan":{"id":"bpr1384-discuss-comment-affordance","path":"docs/rfcs/plans/bpr1384-discuss-comment-affordance/milestone-plan.json"}}
---

# Make Discuss to Review actions visible on comments

## Mandate and observed gap

Owner steering, 2026-10-10: "I saw neither a button, nor a right click menu action for this". Arrived after [[BPR-1383]]'s navigation implementation and deterministic acceptance had been committed. Done is terminal; this follow-up keeps that evidence while completing the amended discoverability scope.

The existing open-thread header button reads filename:line. Right-clicking an individual open comment or reply is handled by the Markdown reader, whose menu contains text/source controls and does not reach the separate thread menu. Expose the destination where the comment is read.

## Acceptance

- AC1: An open inline thread in Discuss has an explicit, visible Open in Review button; its file:line stays visible beside it, with the full path in its tooltip. It remains keyboard accessible.
- AC2: Right-click or keyboard context menu on an inline comment or reply, in rendered or source view, offers Open in Review and opens that exact post beside its source line in Review. The first post and a reply are both covered.
- AC3: General PR comments and unavailable code anchors retain a disabled menu action with a reason; drafts do not offer navigation through their post body. Review's own reader menu does not add a second Review action.
- AC4: A full composed application course reaches the actual reply-body context gesture through Git/cache, with fail-before/pass-after evidence and rendered Discuss/action/Review states; focused reader/card/workbench checks, formatting and documentation gates pass.

## Plan

A bounded follow-up milestone in the same owner-requested feature; BPR-1383 remains accepted. Uses the already loaded frontend-design skill and desktop guidance. Reuses BPR-1383's exact post route, adds a Qt reader menu composition hook, and keeps provider and persistence authority unchanged. Existing broader RFC native/human criteria remain separate.
