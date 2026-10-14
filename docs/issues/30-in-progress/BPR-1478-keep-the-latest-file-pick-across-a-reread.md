---
schema: "implement-rfc.card.v1"
id: "BPR-1478"
title: "Keep the reviewer's latest file pick when a reread lands"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1475"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1478--BPR-1478--1180ae81b4b9.json","origin":"BPR-1475 finding, owner request 2026-10-13"}
---

# Keep the reviewer's latest file pick when a reread lands

BPR-1475 made the window reread the pull request by itself after it
publishes. It found that a file the reviewer picks just as a reread of the
shown pull request lands can be lost: the window goes back to the file it
showed before the read. The start-review `pending` course failed 2 runs in 7
until it was changed to wait for the reread before choosing another file. A
manual Refresh has the same exposure.

## Acceptance

- AC1: A fixture course reproduces the loss deterministically: it starts a
  refresh, picks another file before the read lands, then lets the read land.
  On the code before this card the window returns to the earlier file.
- AC2: The reviewer's latest pick wins. A reread or Refresh never replaces a
  file chosen after it started with the view the window showed before it. The
  core makes that decision, and a core test that fails on the old behaviour
  covers it.
- AC3: The start-review `pending` course no longer needs to wait for the
  single comment's reread before it chooses another file, and passes
  repeatedly without that wait.
- AC4: The focused tests, format, changed-line tidy and the documentation
  gate pass, and the composition contract is resealed.

## Evidence
