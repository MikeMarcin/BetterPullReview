---
schema: "implement-rfc.card.v1"
id: "BPR-2224"
title: "Every app flow is a shipped prompt; the writing-hooks and review-skill editors removed, edits saved then dropped, exchange under flow.* IDs"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2222", "BPR-2223", "BPR-2227"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json","milestone":"M2","milestone_acceptance":"AC04, AC10, AC11, AC12"}
---

# Every app flow is a shipped prompt; the writing-hooks and review-skill editors removed, edits saved then dropped, exchange under flow.* IDs

RFC-BPR-0129 milestone M2 ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)).

## Scope

- **Shipped prompts.** Every row of the RFC's per-ID table marked fixed
  becomes a shipped prompt. That includes the Typed question, Draft tour,
  the tour chats, the two workshop prompts chosen by the label helper (O3),
  and the local discussion. The guidance in each prompt's resource or C++
  text moves into its editable text, and its format rules move into
  contracts. `report-templates.md` and `review.suggested-comment` are
  split as the RFC says.
- **Removed.** The writing-hooks panel, its Built-in prompts list,
  `writing_hook_settings.Overrides` and the review skill's panel.
- **Dropped edits.** First save each writing-hook override and any edited
  review skill copy to a file in Downloads, and store their names (O4).
  Then delete them, and show the one-time notice. If saving fails, nothing
  is deleted. Load `frontend-design:frontend-design` for the notice, and
  file an owner look at it.
- **Exchange.** Flow prompts are exchanged under `flow.*` IDs in file
  version 1 (O5). Old purpose IDs are dropped on import and named in the
  summary.
- **Amendments.** Record the RFC-BPR-0092, 0107, 0118 and 0128 amendments.

## Acceptance

RFC-BPR-0129 AC04 (for every flow), AC10, AC11, AC12.
