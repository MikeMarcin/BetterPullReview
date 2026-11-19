---
schema: "implement-rfc.card.v1"
id: "BPR-2271"
title: "Suggested-change diffs in the Comments drawer scroll sideways as code blocks do"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2271--BPR-2273--d45a84d10eb3.json","found_by":"owner, 2026-11-18","related_cards":["BPR-1212","BPR-1564","BPR-1944"]}
---

# BPR-2271: Suggested-change diffs in the Comments drawer scroll sideways as code blocks do

## Observed

Owner, 2026-11-18: "Suggested changes diffs in the comments drawer should
scroll like code blocks there do".

## Acceptance

- AC1. A suggested-change diff in the Comments drawer does not wrap or
  elide a long line: its rows scroll sideways inside their own box, one
  horizontal scroll per diff, in the drawer's code scroll bar style.
- AC2. A sideways wheel or Shift-wheel moves the diff, a vertical wheel
  moves the drawer, and Left and Right scroll it when it has focus, as a
  wide code block in a post does.
- AC3. A Qt test: a suggestion whose line is longer than the drawer gets a
  horizontal scroll range, and the card stays the drawer's width.
- AC4. Focused tests, format and tidy on the changed files, and the
  documentation gate pass.
