---
schema: "implement-rfc.card.v1"
id: "BPR-2224"
title: "Every app flow is a shipped prompt; Writing hooks removed and its overrides dropped with a notice"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2222", "BPR-2223"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json","milestone":"M2","milestone_acceptance":"AC04, AC10, AC11, AC14"}
---

# Every app flow is a shipped prompt; Writing hooks removed and its overrides dropped with a notice

RFC-BPR-0129 milestone M2 ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)).

## Scope

- Each `core::writing_hook`, the report guidance and the suggested-comment
  format become shipped prompts in Settings › Prompts, with their Results
  per the RFC's mapping (Q2, default accepted by the coordinator), each
  fixed and shown read-only (owner, Q3). Each keeps its purpose ID. Format rules move
  into contracts, and guidance moves into the text.
- Remove the writing-hooks panel, its Built-in prompts list and
  `writing_hook_settings.Overrides`.
- Drop the existing overrides on the first start, with a one-time notice
  naming each dropped prompt. Exchange import drops writing-hook overrides
  and names them in its summary (the RFC-BPR-0107 amendment).
- The review skill `bpr-pr-review`: its guidance becomes the review
  prompts' text, its panel is removed, and an edited copy is dropped with
  the same one-time notice (owner, Q1).
- A person's prompt with a flow Result is offered in that flow after the
  shipped prompt, for example a second Rewrite text in the Enhance menu
  (owner, Q5).
- Record the RFC-BPR-0092, 0107 and 0128 amendments on those RFCs.

## Acceptance

RFC-BPR-0129 AC04 (for every flow), AC10, AC11, AC14.
