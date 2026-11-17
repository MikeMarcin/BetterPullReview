---
schema: "implement-rfc.card.v1"
id: "BPR-2233"
title: "Opening the report right after Go to draft can lose the front to the file"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
---

# Opening the report right after Go to draft can lose the front to the file

## Origin

In `application_review_report_public_path`, pressing the report's row moments after Go to draft once in eight runs left the file in front: the file's own open was still settling and took the front back ([[BPR-2219]]). The course presses until it takes; the product race is not fixed.

## Scope and acceptance

- **AC-a.** The last of two front-taking gestures wins, whatever the order their work finishes in.
- **AC-b.** A course presses the report's row once, straight after Go to draft, and the report is in front.
