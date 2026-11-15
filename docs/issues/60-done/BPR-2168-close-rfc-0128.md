---
schema: "implement-rfc.card.v1"
id: "BPR-2168"
title: "Verify and close RFC-BPR-0128"
status: "Done"
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
| **AC01:** The review bar, the open file's header and the open thread's reply row each have one Ask control, and a thread's and a file tab's right-click menus each have one Ask item, whose menus list the quick asks of their scope; the agent items they replace are gone and no new row is added to the window. | Pass (dev): `application_ask_menu_public_path`, `qt_conversation_cards_view`; owner Accept of the Ask style (look ask-v007) | `bac5aab96` |
| **AC02:** An Ask menu lists only macros with Quick ask ticked and a scope that applies there; each row names what a press will read; Custom question… opens the Agent tab on that subject and sends nothing, and Edit Quick Asks… opens Settings at the macros. | Pass (dev): `application_ask_menu_public_path`, `core_quick_ask` | `bac5aab96` |
| **AC03:** A quick ask whose subject is absent is not listed, and one whose subject is present but cannot run is listed disabled with its reason as its tooltip and accessible description; building or filtering a menu makes no agent call. | Pass (dev): `core_quick_ask`, `application_ask_menu_public_path` | `bac5aab96` |
| **AC04:** The macro editor's Quick ask checkbox and Thread, File and Pull request toggles are saved with the macro, a scope narrower than its subject cannot be set, and What changed, Was this addressed? and Explain this file each have a built-in row whose sent prompt is unchanged. | Pass (dev): `core_quick_ask`, `sqlite_agent_setup_storage`, `application_ask_menu_public_path`; macro editor now [[BPR-2179]]'s Prompts page | `bac5aab96` |
| **AC05:** Was this addressed? runs on a thread I did not comment in, on each thread of a file and on every thread of the pull request; each claim appears as a chip in its thread's header without changing the drawer's layout height, its press shows the thread's lines at its baseline and now, and nothing is resolved, posted or published. | Pass (dev): `application_ask_addressed_public_path`, live installed-agent run (`7b3fe6eaa`); Q5 as amended owner-accepted (look ask-v008) | `bac5aab96` |
| **AC06:** A long answer appears as a report card where it was asked; the card opens the report as a tab in the files' tab strip with an outline; while it is open the Agent tab, Tour tab and tour chat keep their places and accept input; a code reference opens that file's tab at the line and the report's tab returns to the report. | Pass (dev): `application_ask_report_public_path`, `application_fixture_agent_every_ask` (10 reports dressed, every menu section reached); report card, opened report, tab glyph, colours accepted; code blocks accepted in look report-templates-v004 | `bac5aab96` |
| **AC07:** Wherever commits are listed, a merge of the base branch is one line with its commit count and whether it changed this pull request's files, and the other commits are grouped by area. | Pass (dev): `core_agent_report`, `github_provider` (merge count by compare), live compare check on BPR-Test | `38c727f03` |
| **AC08:** Every Ask control, menu row, claim chip and report tab is reachable and operable by keyboard alone and is named to assistive technology, including a greyed row's reason. | Pass (dev): `application_ask_menu_public_path` (keyboard and names), `application_ask_report_public_path` (row and menu by keyboard) | `bac5aab96` |

## Outcome and work

Reconcile the amended RFCs (RFC-BPR-0116, 0117, 0118, 0119, 0125) with what was built; record the owner's answers to Q1 to Q14 (Q8 answered at import); update `docs/ux/ai-experience.md` and the agent-assistance allocation in `docs/architecture/system-architecture.md`; record the owner looks; run the documentation gate.

## Pending (2026-11-14)

- Q5 as amended (one Was this addressed? ask for many threads, look ask-v005)
  is built; its owner acceptance waits on the report-card redesign of
  [[BPR-2166]] (look ask-v006: "I think this might need to wait until report
  cards land because the actual output in the app is not good enough").

## Closeout progress (2026-11-14)

- The checklist above is filled from the lane's evidence. The card stays in
  Backlog until [[BPR-2166]] (look report-templates-v004, the reports' code
  blocks) and [[BPR-2167]] (which depends on it) are Done.
- Docs reconciled: `docs/ux/ai-experience.md` (the Ask entry point, reports,
  and the RFC-BPR-0128 amendments to Explain this file and Was this
  addressed?) and `docs/architecture/ai-agent-boundary.md` (the allocation of
  Ask and reports across core, host and Qt). The amended RFCs (0116, 0117,
  0118, 0119, 0125) each name RFC-BPR-0128 already.
- Owner answers: Q8 at import, Q5 as amended (looks ask-v005 to v008); Q1 to
  Q4, Q6, Q7, Q9 to Q14 built as their defaults and seen in looks ask-v001 to
  ask-v008 and report-templates-v003 without objection.
- Left for the close: the RFC's status and index entry, and the RFC's
  verification summary once v004 is answered.

## Closed (2026-11-14)

[[BPR-2166]] and [[BPR-2167]] are Done; look report-templates-v004 was
accepted. RFC-BPR-0128 is Auto-Verified, its index entry closed and its
[verification summary](../../verification/RFC-BPR-0128.md) written.

