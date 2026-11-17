---
schema: "implement-rfc.card.v1"
id: "BPR-2227"
title: "Feature surfaces and flow controls with several prompts"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2222"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2227--BPR-2227--f1dcd66966f6.json","milestone":"M1","milestone_acceptance":"AC07, AC09, AC14"}
---

# Feature surfaces and flow controls with several prompts

RFC-BPR-0129 milestone M1, the output surfaces ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)).

## Scope

- **Suggested comment.** A Draft comment on the block itself, beside the
  answer's own keep action (RFC-BPR-0127). The draft's body is the block's
  text, at the block's place.
- **Suggested change.** A suggestion block, kept as a draft comment's
  suggestion.
- **Proposed fix.** A diff that can be copied, with Apply disabled and
  naming its reason until RFC-BPR-0115 exists (Q4).
- **Missing blocks.** An omitted or malformed declared block is noted on
  its item.
- **Flow controls.** A control whose flow has several prompts (the Enhance
  menu first) offers each by name, the shipped one first (owner, Q5).
  Placements follow the Result: a person's Rewrite text prompt appears in
  the Enhance menu and in no chat.
- **Design.** Load `frontend-design:frontend-design` first and say so in
  the report.
- **The owner's look.** A frozen Fixture-agent build whose fixture answer
  carries each Feature, plus a person's second Rewrite text prompt, in a
  fresh short session with one Accept/Reject for each criterion. The look
  is filed, not waited on.

## Acceptance

RFC-BPR-0129 AC07, AC09 (these surfaces), AC14.
