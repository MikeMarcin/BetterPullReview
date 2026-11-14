---
schema: "implement-rfc.card.v1"
id: "BPR-2163"
title: "Quick asks: a macro's Quick ask flag, its scopes, and when it is offered"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"},"criteria":["AC03","AC04"],"origin":"imported 2026-11-13 from the owner's Ask design (docs/design/ask-v1); cards cut at import"}
acceptance: [{"criterion":"AC03","outcome":"pass","revision":"7b3fe6eaa","evidence":{"kind":"command","detail":"core_quick_ask; application_ask_menu_public_path (greyed reason as tooltip, status tip and What's This; absent subject not listed; building every menu launched no agent)"}},{"criterion":"AC04","outcome":"pass","revision":"7b3fe6eaa","evidence":{"kind":"command","detail":"core_quick_ask, sqlite_agent_setup_storage, application_ask_menu_public_path (Quick ask and scopes saved through Settings and read back from the store; narrower scope unavailable; built-in rows; What changed sends system=1 as before)"}}]
---

# BPR-2163: Quick asks: a macro's Quick ask flag, its scopes, and when it is offered

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md); milestone `ask-menu` of the [plan](../../rfcs/plans/ask-v1/MILESTONES.md).

## Scope

Core: RFC-BPR-0125's macro gains Quick ask and the Thread, File and Pull request scopes with their legality (a scope narrower than the subject cannot be set; a wider one means once for each); availability of each quick ask at a scope as shown, greyed with a typed reason, or hidden (Q1); built-in rows for What changed, Was this addressed? and Explain this file whose sent prompts stay in Built-in prompts (Q13). Storage of the new macro fields and their sharing under RFC-BPR-0107. Settings: the macro sheet's Quick ask row (load `frontend-design:frontend-design` first). No agent call in any of it.

## Acceptance

- **AC03:** A quick ask whose subject is absent is not listed, and one whose subject is present but cannot run is listed disabled with its reason as its tooltip and accessible description; building or filtering a menu makes no agent call.
- **AC04:** The macro editor's Quick ask checkbox and Thread, File and Pull request toggles are saved with the macro, a scope narrower than its subject cannot be set, and What changed, Was this addressed? and Explain this file each have a built-in row whose sent prompt is unchanged.

## Progress (2026-11-14)

Built and evidenced; see the verification record (`docs/verification/RFC-BPR-0128.md` on lane `claude/v0-5-0-prerelease-targets-52265d`).

- **AC03** passes: `core_quick_ask` (subject absent not listed, present and
  unable greyed with a typed reason, the agent's state greys) and
  `application_ask_menu_public_path` (the greyed row's reason is its tooltip,
  status tip and What's This; a menu with no thread open lists no thread
  section; building every menu launched no agent).
- **AC04** passes: `core_quick_ask` (a narrower scope blocks; the three action
  rows), `sqlite_agent_setup_storage` (Quick ask and scopes stored), and
  `application_ask_menu_public_path` (the sheet's Quick ask off on a built-in
  and on with File and Pull request on an own prompt are saved in the store
  and change the menus; Thread and File unavailable on a pull request's
  prompt; What changed pressed from the review bar sends its own prompt,
  `system=1`, with the change).

Settings needed RFC-BPR-0125's store and sheet, which were not built:
[[BPR-2146]] was claimed and its store and roster built; the sheet is part of
`qt::prompt_macros_panel`.
