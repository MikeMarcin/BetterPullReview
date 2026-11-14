---
schema: "implement-rfc.card.v1"
id: "BPR-2166"
title: "A long answer is a report card that opens as a file tab"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2164"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"},"criteria":["AC06"],"origin":"imported 2026-11-13 from the owner's Ask design (docs/design/ask-v1); cards cut at import"}
---

# BPR-2166: A long answer is a report card that opens as a file tab

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md); milestone `reports-in-a-file-tab` of the [plan](../../rfcs/plans/ask-v1/MILESTONES.md).

## Scope

Load `frontend-design:frontend-design` first. The owner's choice (Q8): the report is a tab in the files' tab strip, with the agent glyph and an outline. Core: the tab model's report kind; a report by its shape (Q7). Qt: the report card where asked, the report tab, code references to the file's tab at the line and back, the review bar's file controls off while a report is active (Q9), the drawer's composer asking about the report; What changed and Explain this file answer as reports (Q4). A report comes back through its card while its turn is in the chat (Q10).

## Acceptance

- **AC06:** A long answer appears as a report card where it was asked; the card opens the report as a tab in the files' tab strip with an outline; while it is open the Agent tab, Tour tab and tour chat keep their places and accept input; a code reference opens that file's tab at the line and the report's tab returns to the report.

## Notes

Ends in an owner look (Accept or Reject) on a frozen build serving a pull request with a long What changed.

## Owner looks (2026-11-14)

- ask-v002, **Reject**: "Aren't we supposed to have a mack agent for testing
  now? " (no Fixture agent answer; fixed under [[BPR-2177]]).
- ask-v003, **Reject**: "The footer does not say fixture agent" (the footer
  named Claude Code; fixed under [[BPR-2177]]).
- ask-v004, **Reject**: the Fixture agent's answer to What changed since my
  review was refused as malformed_frame (its init frame listed tools for a
  tool-free run; fixed under [[BPR-2177]], with every ask now pressed by the
  freeze check and `application_fixture_agent_every_ask`). Asked again in
  ask-v005.
- ask-v006: RFC-BPR-0128 Q5 as amended (one ask for many threads) **Reject**:
  "I think this might need to wait until report cards land because the actual
  output in the app is not good enough". Its acceptance now waits on this
  card's report-card redesign; the mechanism stays built.
- ask-v007: the report card **Accept**; the opened report **Accept**, "Yes but
  I think we need a different icon for the tab header, because that one reaads
  as though this report is going to do something with the agent. It should
  read as this is a report not a file."; Q5 **Reject**, "Why is your mockup
  report attachment cutting of text. Why does asking was this addressed on the
  PR in the fixture agaent in the fronzen build not show an attachment
  report?". Fixed in `47dc394c8`: the tab wears a report glyph (a page with a
  folded corner and lines of text); the row's height follows its width (it was
  Fixed, sized from a stale width); a many-thread Was this addressed? is a
  report (it was excluded); the every-ask check fails on a missing report.
  Asked again in ask-v008.

## Progress (2026-11-14, claimed)

Claimed once [[BPR-2164]] was Done.

- **Q4** (`944421875`): Explain this file and What changed since my review
  answer as report cards whatever their length, titled for the question
  ("Explain src/engine/renderer.cpp"), the lead from the answer's first line
  (`core::report_card_of` with a given title; `application_explain_file_public_path`,
  `core_agent_report`).
- **AC06**: with the report showing, the drawer's Tour tab opens and takes
  input and the Agent tab's question field takes input, the report staying
  where it is (`application_ask_report_public_path`). Found: the Pull Request
  menu's Prepare review tour moves to the Tour's Problem step and leaves the
  report (it goes to Review); a person's click on the Tour tab does not. The
  tour chat (a published tour's) is not exercised: the returning-review
  fixture has no published tour.
- **Remaining:** the owner's look, after the coordinator's report-card design
  round (the card's look and RFC-BPR-0128 Q5's acceptance wait on it).
