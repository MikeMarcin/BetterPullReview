---
schema: "implement-rfc.card.v1"
id: "BPR-2277"
title: "A report's member-access inline code is coloured"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2277--BPR-2279--424d9c5fc0e9.json","found_by":"owner attended review, 2026-11-19, frozen fixture build of 91dedb8a3","related_cards":["BPR-2266","BPR-1753","BPR-2166"]}
---

# BPR-2277: A report's member-access inline code is coloured

## Observed

Owner, 2026-11-19, on the conventional-comments-v1 fixture's agent report
"Review of #21": "it doesn't seem like `Target.resize` is getting
highlighting here." The span is monospace on its grey chip but has no syntax
colour. Reopens [[BPR-2266]].

## Acceptance

- AC1. Inline code on a report's page is coloured as the comments drawer and
  the Agent tab colour it: identifiers, member access, calls, keywords and
  literals, through the same syntax dispatch.
- AC2. A Qt test renders a report holding `Target.resize` (the fixture's own
  text) and asserts syntax formats inside that span.
- AC3. Focused tests, format and the documentation gate pass.
