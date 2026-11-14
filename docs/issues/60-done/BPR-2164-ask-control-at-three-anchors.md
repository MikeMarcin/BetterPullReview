---
schema: "implement-rfc.card.v1"
id: "BPR-2164"
title: "One Ask control at the pull request, the file and the thread"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2163"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"},"criteria":["AC01","AC02","AC08"],"origin":"imported 2026-11-13 from the owner's Ask design (docs/design/ask-v1); cards cut at import"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"7b3fe6eaa","evidence":{"kind":"command","detail":"application_ask_menu_public_path, qt_conversation_cards_view, qt_workbench_window_scale_{100,125,200}; owner Accept in look ask-v002"}},{"criterion":"AC02","outcome":"pass","revision":"7b3fe6eaa","evidence":{"kind":"command","detail":"application_ask_menu_public_path (only quick asks; rows name what a press reads; Custom question sends nothing; Edit Quick Asks opens Settings at the prompts); owner Accept in look ask-v002"}},{"criterion":"AC08","outcome":"pass","revision":"7b3fe6eaa","evidence":{"kind":"command","detail":"application_ask_menu_public_path (named, keyboard focus, greyed rows reachable with their reason); owner look pending; owner Accept in look ask-v002"}}]
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

## Progress (2026-11-14)

Frontend-design was loaded first. Built and evidenced; see the
verification record (`docs/verification/RFC-BPR-0128.md` on lane `claude/v0-5-0-prerelease-targets-52265d`).

- **AC01** passes (`application_ask_menu_public_path`, `qt_conversation_cards_view`,
  `qt_workbench_window_scale_{100,125,200}`): the review bar's Ask right after
  the comparison; the file header's Ask in the tab strip where Explain this
  file stood (Q3); the open thread's Ask beside Reply; Ask items in a thread's
  and a file tab's menus; Ask agent about this conversation, Was This
  Addressed? and Explain This File are gone from those menus and the header;
  no row was added (the controls sit in existing rows).
- **AC02** passes: only Quick ask macros on a scope that applies; rows name
  what a press reads (threads, files, and commits where the commit list holds
  the reviewed commit); Custom question… attaches the thread and sends nothing;
  Edit Quick Asks… opens Settings at Agents & context with the suggested
  prompts focused.
- **AC08** passes on automation: each Ask is named and takes keyboard focus;
  rows are menu actions with their text; a greyed row is reachable by the
  arrow keys (`SH_Menu_AllowActiveAndDisabled`) and its reason shows beside it
  and is its accessible text. A screen-reader pass by a person is not run.

Ends in the owner's look (filed; see the handoff).

## Owner look ask-v002 (2026-11-14)

`BPR-2164-review-bar-ask`, `BPR-2164-file-ask` and `BPR-2164-thread-ask` all
**Accept** (frozen from `612194781`). On the thread question: "I'm surprised
there's no ask on the threads mode." (recorded; Threads mode has no Ask
control yet, a possible follow-up). Done.
