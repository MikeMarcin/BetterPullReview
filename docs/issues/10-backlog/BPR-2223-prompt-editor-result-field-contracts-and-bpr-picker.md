---
schema: "implement-rfc.card.v1"
id: "BPR-2223"
title: "The prompt editor: Result field, read-only contracts and /bpr: in the skills picker"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2222"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json","milestone":"M1","milestone_acceptance":"AC01, AC02, AC05, AC09"}
---

# The prompt editor: Result field, read-only contracts and /bpr: in the skills picker

RFC-BPR-0129 milestone M1, the editor half ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)).

## Scope

- In Settings › Prompts, the Result field (read-only for a shipped flow
  prompt, owner Q3) and the read-only sections: the
  application contract, the Result's and each Feature's contract, the
  inlined skills, and the run's material listed by name.
- `/bpr:` rows in the completion popup that offers `/skills`, in the editor
  and in a typed chat question's composer (owner, Q6).
- The workflow beyond those parts is the designer's call (owner decision 8).
  Load `frontend-design:frontend-design` first and say so in the report.
- The owner's look: a frozen Fixture-agent build that opens on a fixture
  pull request with a review prompt, in a fresh short attended-review
  session with one Accept/Reject per criterion. The look is filed, not
  waited on.

## Acceptance

RFC-BPR-0129 AC01 (in the editor, picker and Settings), AC02 (as the editor
presents it), AC05, AC09.
