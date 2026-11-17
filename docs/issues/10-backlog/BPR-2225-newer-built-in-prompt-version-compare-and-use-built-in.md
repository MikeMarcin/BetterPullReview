---
schema: "implement-rfc.card.v1"
id: "BPR-2225"
title: "A shipped flow prompt says a newer built-in version exists, with Compare and Use built-in"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2224"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json","milestone":"M3","milestone_acceptance":"AC13"}
---

# A shipped flow prompt says a newer built-in version exists, with Compare and Use built-in

RFC-BPR-0129 milestone M3 ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)).

## Scope

- **Extend the existing flag.** Extend RFC-BPR-0125's Outdated flag and
  Compare (`prompt_set_entry::Outdated`) to the shipped flow prompts. It
  reads A newer built-in version exists, with Use built-in.
- **Updates.** Untouched prompts update silently. A contract revision
  raises no flag.
- **Amendment.** Record the RFC-BPR-0125 amendment for this part.
- **Design.** If Compare's presentation is new since [[BPR-2223]]'s look,
  load `frontend-design:frontend-design` and file an owner look.

## Acceptance

RFC-BPR-0129 AC13.
