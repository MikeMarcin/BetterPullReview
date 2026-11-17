---
schema: "implement-rfc.card.v1"
id: "BPR-2223"
title: "The prompt editor: Result field, read-only contracts and addenda, and /bpr: in the skills picker"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2222"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json","milestone":"M1","milestone_acceptance":"AC01, AC02, AC04, AC05, AC09"}
---

# The prompt editor: Result field, read-only contracts and addenda, and /bpr: in the skills picker

RFC-BPR-0129 milestone M1, the editor half ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)).

## Scope

- **Settings › Prompts.** The Result field, read-only for a shipped flow
  prompt (owner, Q3). Classes 2 to 7 shown read-only:
  - the family contract;
  - the Result's contract and each Feature's contract;
  - the skills directive and each inlined skill;
  - the agent addenda, under each agent kind;
  - the grant addenda, with their conditions.

  Class 8 is listed by field name, and the assembled size is shown against
  the bounds.
- **The picker.** `/bpr:` rows in the completion popup that offers
  `/skills`, in the editor and in a typed chat question's composer (owner,
  Q6). A Feature the Result cannot carry is listed disabled with its
  reason.
- **The frame.** Document the request frame (class 9) once, in
  `docs/architecture/ai-agent-boundary.md`.
- **Design.** The workflow is the designer's call (owner decision 8). Load
  `frontend-design:frontend-design` first and say so in the report.
- **The owner's look.** A frozen Fixture-agent build that opens on a
  fixture pull request with a review prompt, in a fresh short
  attended-review session, with one Accept/Reject for each criterion. The
  look is filed, not waited on.

## Acceptance

RFC-BPR-0129 AC01 (in the editor, picker and Settings), AC02 (as the editor
presents it), AC04 (the editor half), AC05, AC09 (the editor's surfaces).
