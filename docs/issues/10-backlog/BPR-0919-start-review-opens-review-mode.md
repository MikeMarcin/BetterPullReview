---
schema: "implement-rfc.card.v1"
id: "BPR-0919"
title: "Make Overview Start review enter Review instead of empty Read"
status: "Backlog"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Make Overview Start review enter Review instead of empty Read

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-02. Earlier RFC references retain their governing contracts.

## Owner feedback

“Start review button on overview takes you to Read with no file open, pretty
useless, just go to review.”

Reported after the first loaded PR, following publication of 0.1.3. The exact
running build, PR and environment have not been independently captured.

## Scope and investigation

Change the Overview Start review destination to the Review modality as the
owner requested. Trace the current overview-entry intent, core mode transition
and file selection rather than merely renaming the button. Specify Continue
review behavior separately so an existing review's place is not accidentally
reset. Keep this local navigation: it does not submit a GitHub review.

Starting points: `src/adapters/qt/src/pull_request_overview_page.cpp` and
`src/adapters/qt/src/workbench_window.cpp`.

## Acceptance

- A composed Overview Start review gesture enters Review, never an empty Read page.
- Define and exercise file selection or an honest empty/error state, including the unavailable-comparison case in [[BPR-0922]]; no blank dead end.
- Continue review preserves the intended existing review position; starting either path publishes nothing.

## Status

Not started. This intake records feedback and investigation/repair scope only;
no reproduction, root cause, runtime fix or live-service verification is claimed.
