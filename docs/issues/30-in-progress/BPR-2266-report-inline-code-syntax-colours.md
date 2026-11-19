---
schema: "implement-rfc.card.v1"
id: "BPR-2266"
title: "A report's inline code is coloured again"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2266--BPR-2266--0872a4edcad1.json","origin":"owner, 2026-11-18","related_cards":["BPR-2166","BPR-1753","BPR-2219"]}
---

# A report's inline code is coloured again

## Origin

The owner, 2026-11-18: "Seems like we lost syntax highlights for inline code
on reports."

## Scope and acceptance

- **AC-a.** Inline code spans on a report's page are coloured as the Agent
  tab colours an answer's inline code: the pull request's language, through
  the same syntax dispatch.
- **AC-b.** A Qt test asserts syntax formats on a rendered report's inline code.
