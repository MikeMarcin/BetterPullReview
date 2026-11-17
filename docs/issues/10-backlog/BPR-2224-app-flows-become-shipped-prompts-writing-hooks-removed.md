---
schema: "implement-rfc.card.v1"
id: "BPR-2224"
title: "Every app flow is a shipped prompt; Writing hooks removed and its overrides dropped with a notice"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2222", "BPR-2223"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json","milestone":"M2","milestone_acceptance":"AC04, AC10, AC11"}
---

# Every app flow is a shipped prompt; Writing hooks removed and its overrides dropped with a notice

RFC-BPR-0129 milestone M2 ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)).

## Scope

- Each `core::writing_hook`, the report guidance and the suggested-comment
  format become shipped prompts in Settings › Prompts, with their Results
  per the RFC's mapping (Q2). Each keeps its purpose ID. Format rules move
  into contracts, and guidance moves into the text.
- Remove the writing-hooks panel, its Built-in prompts list and
  `writing_hook_settings.Overrides`.
- Drop the existing overrides on the first start, with a one-time notice
  naming each dropped prompt. Exchange import drops writing-hook overrides
  and names them in its summary (the RFC-BPR-0107 amendment).
- The review skill `bpr-pr-review` follows the owner's answer to Q1; that
  part is not ready until Q1 is answered.
- Record the RFC-BPR-0092, 0107 and 0128 amendments on those RFCs.

## Acceptance

RFC-BPR-0129 AC04 (for every flow), AC10, AC11.
