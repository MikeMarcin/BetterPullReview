---
schema: "implement-rfc.card.v1"
id: "BPR-2225"
title: "An edited shipped prompt says a newer built-in version exists, with Compare and Use built-in"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2224"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json","milestone":"M3","milestone_acceptance":"AC12"}
---

# An edited shipped prompt says a newer built-in version exists, with Compare and Use built-in

RFC-BPR-0129 milestone M3 ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)).

## Scope

- Untouched shipped prompts take a new built-in text silently. An edited
  one keeps its text and shows A newer built-in version exists, with
  Compare and Use built-in, decided in core from
  `prompt_builtin_change.WrittenAgainst`.
- A contract revision reaches every prompt and raises no flag.
- Record the RFC-BPR-0125 amendment.
- Compare's presentation is part of the designer's workflow. Load
  `frontend-design:frontend-design`, and file an owner look if it is new
  since [[BPR-2223]]'s look.

## Acceptance

RFC-BPR-0129 AC12.
