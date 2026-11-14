---
schema: "implement-rfc.card.v1"
id: "BPR-2163"
title: "Quick asks: a macro's Quick ask flag, its scopes, and when it is offered"
status: "Ready"
rfcs: ["RFC-BPR-0128"]
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"},"criteria":["AC03","AC04"],"origin":"imported 2026-11-13 from the owner's Ask design (docs/design/ask-v1); cards cut at import"}
---

# BPR-2163: Quick asks: a macro's Quick ask flag, its scopes, and when it is offered

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md); milestone `ask-menu` of the [plan](../../rfcs/plans/ask-v1/MILESTONES.md).

## Scope

Core: RFC-BPR-0125's macro gains Quick ask and the Thread, File and Pull request scopes with their legality (a scope narrower than the subject cannot be set; a wider one means once for each); availability of each quick ask at a scope as shown, greyed with a typed reason, or hidden (Q1); built-in rows for What changed, Was this addressed? and Explain this file whose sent prompts stay in Built-in prompts (Q13). Storage of the new macro fields and their sharing under RFC-BPR-0107. Settings: the macro sheet's Quick ask row (load `frontend-design:frontend-design` first). No agent call in any of it.

## Acceptance

- **AC03:** A quick ask whose subject is absent is not listed, and one whose subject is present but cannot run is listed disabled with its reason as its tooltip and accessible description; building or filtering a menu makes no agent call.
- **AC04:** The macro editor's Quick ask checkbox and Thread, File and Pull request toggles are saved with the macro, a scope narrower than its subject cannot be set, and What changed, Was this addressed? and Explain this file each have a built-in row whose sent prompt is unchanged.
