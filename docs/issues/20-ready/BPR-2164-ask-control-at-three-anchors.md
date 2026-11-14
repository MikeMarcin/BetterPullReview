---
schema: "implement-rfc.card.v1"
id: "BPR-2164"
title: "One Ask control at the pull request, the file and the thread"
status: "Ready"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2163"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"},"criteria":["AC01","AC02","AC08"],"origin":"imported 2026-11-13 from the owner's Ask design (docs/design/ask-v1); cards cut at import"}
---

# BPR-2164: One Ask control at the pull request, the file and the thread

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md); milestone `ask-menu` of the [plan](../../rfcs/plans/ask-v1/MILESTONES.md).

## Scope

Load `frontend-design:frontend-design` first; the accepted design is [docs/design/ask-v1](../../design/ask-v1/README.md). The review bar's Ask (1C), the file header's Ask in the place of Explain this file (Q3), the open thread's reply-row Ask (Plain, Ask, Publish), and Ask in a thread's and a file tab's right-click menus replacing their agent items; rows name what a press reads; Custom question… and Edit Quick Asks…; keyboard and accessibility. Runs go through the existing launch paths of RFC-BPR-0101 (fresh or resumed as they are).

## Acceptance

- **AC01:** The review bar, the open file's header and the open thread's reply row each have one Ask control, and a thread's and a file tab's right-click menus each have one Ask item, whose menus list the quick asks of their scope; the agent items they replace are gone and no new row is added to the window.
- **AC02:** An Ask menu lists only macros with Quick ask ticked and a scope that applies there; each row names what a press will read; Custom question… opens the Agent tab on that subject and sends nothing, and Edit Quick Asks… opens Settings at the macros.
- **AC08:** Every Ask control, menu row, claim chip and report tab is reachable and operable by keyboard alone and is named to assistive technology, including a greyed row's reason.

## Notes

Ends in an owner look (Accept or Reject) on a frozen build whose fixture holds a returning pull request with threads by others.
