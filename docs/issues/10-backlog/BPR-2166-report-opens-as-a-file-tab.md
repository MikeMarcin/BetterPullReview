---
schema: "implement-rfc.card.v1"
id: "BPR-2166"
title: "A long answer is a report card that opens as a file tab"
status: "Backlog"
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
