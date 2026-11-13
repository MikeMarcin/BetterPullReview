---
schema: "implement-rfc.card.v1"
id: "BPR-2168"
title: "Verify and close RFC-BPR-0128"
status: "Backlog"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2163", "BPR-2164", "BPR-2165", "BPR-2166", "BPR-2167"]
closeout_for: ["RFC-BPR-0128"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"}}
---

# BPR-2168: Verify and close RFC-BPR-0128

RFC base: `f85b216ecc7d4a7df2c7c5c263a0cf8be1f3d93a`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **AC01:** The review bar, the open file's header and the open thread's reply row each have one Ask control, and a thread's and a file tab's right-click menus each have one Ask item, whose menus list the quick asks of their scope; the agent items they replace are gone and no new row is added to the window. | Not run | No producing revision |
| **AC02:** An Ask menu lists only macros with Quick ask ticked and a scope that applies there; each row names what a press will read; Custom question… opens the Agent tab on that subject and sends nothing, and Edit Quick Asks… opens Settings at the macros. | Not run | No producing revision |
| **AC03:** A quick ask whose subject is absent is not listed, and one whose subject is present but cannot run is listed disabled with its reason as its tooltip and accessible description; building or filtering a menu makes no agent call. | Not run | No producing revision |
| **AC04:** The macro editor's Quick ask checkbox and Thread, File and Pull request toggles are saved with the macro, a scope narrower than its subject cannot be set, and What changed, Was this addressed? and Explain this file each have a built-in row whose sent prompt is unchanged. | Not run | No producing revision |
| **AC05:** Was this addressed? runs on a thread I did not comment in, on each thread of a file and on every thread of the pull request; each claim appears as a chip in its thread's header without changing the drawer's layout height, its press shows the thread's lines at its baseline and now, and nothing is resolved, posted or published. | Not run | No producing revision |
| **AC06:** A long answer appears as a report card where it was asked; the card opens the report as a tab in the files' tab strip with an outline; while it is open the Agent tab, Tour tab and tour chat keep their places and accept input; a code reference opens that file's tab at the line and the report's tab returns to the report. | Not run | No producing revision |
| **AC07:** Wherever commits are listed, a merge of the base branch is one line with its commit count and whether it changed this pull request's files, and the other commits are grouped by area. | Not run | No producing revision |
| **AC08:** Every Ask control, menu row, claim chip and report tab is reachable and operable by keyboard alone and is named to assistive technology, including a greyed row's reason. | Not run | No producing revision |

## Outcome and work

Reconcile the amended RFCs (RFC-BPR-0116, 0117, 0118, 0119, 0125) with what was built; record the owner's answers to Q1 to Q14 (Q8 answered at import); update `docs/ux/ai-experience.md` and the agent-assistance allocation in `docs/architecture/system-architecture.md`; record the owner looks; run the documentation gate.
